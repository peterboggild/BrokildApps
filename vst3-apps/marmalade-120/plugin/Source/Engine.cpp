#include "Engine.h"
#include "Or120Netlist.h"

#include <algorithm>
#include <initializer_list>

namespace mm
{

namespace
{
    constexpr double PI = 3.14159265358979323846;

    /*  The guitar's level: 0 dBFS is 1 V peak at the input jack. A passive
        humbucker strummed hard peaks around there, and a DI recorded with
        sensible headroom (peaks at -12 .. -6 dBFS) lands at 250 - 500 mV,
        which is what the circuit expects to see from a guitar. */
    constexpr double IN_VOLTS = 1.0;

    /*  The 8 ohm tap as a line level: 80 V would be well past full power
        (about 62 V peak, measured), so a flat-out amp sits a couple of dB
        under full scale before the OUTPUT trim. */
    constexpr double SPK_TO_FS = 1.0 / 80.0;

    constexpr int CHUNK = 32;            // host samples between knob updates

    /*  The mains, into the power transformer's 240 V tap: the amp is British.
        The windings themselves are ratios in the netlist. */
    constexpr double MAINS_HZ   = 50.0;
    constexpr double MAINS_PEAK = 240.0 * 1.41421356237;

    // A real pot's ends are not quite 0 ohms, and a 0 would make the matrix singular.
    constexpr double POT_END = 100.0;

    /*  Log ("A") taper: 10% of the track at half rotation, as the common
        carbon pots are. */
    inline double audioTaper (double t)
    {
        t = std::clamp (t, 0.0, 1.0);
        return (std::pow (10.0, 2.0 * t) - 1.0) / 99.0;
    }

    inline double potHalf (double total, double fraction)
    {
        return std::max (POT_END, total * fraction);
    }
}

/*  FAC, from the 1972 sheet: the 68n coupling capacitor, then a series chain
    4n7 - 4n7 - 2n2 - 1n - 330p that the switch taps. Each position is the
    series value of everything up to its tap. */
namespace
{
    constexpr double series (std::initializer_list<double> c)
    {
        double inv = 0.0;
        for (double v : c) inv += 1.0 / v;
        return 1.0 / inv;
    }
}
const double FAC_CAPS[NUM_FAC] =
{
    68e-9,
    series ({ 68e-9, 4.7e-9 }),
    series ({ 68e-9, 4.7e-9, 4.7e-9 }),
    series ({ 68e-9, 4.7e-9, 4.7e-9, 2.2e-9 }),
    series ({ 68e-9, 4.7e-9, 4.7e-9, 2.2e-9, 1e-9 }),
    series ({ 68e-9, 4.7e-9, 4.7e-9, 2.2e-9, 1e-9, 330e-12 }),
};

//==============================================================================
Engine::Engine()
{
    if (! amp.parse (OR120_AMP)) { err = "netlist: " + amp.error(); return; }

    iGainA = amp.elementIndex ("rgaina");  iGainB = amp.elementIndex ("rgainb");
    iBassA = amp.elementIndex ("rbassa");  iBassB = amp.elementIndex ("rbassb");
    iTrebA = amp.elementIndex ("rtreba");  iTrebB = amp.elementIndex ("rtrebb");
    iBoost = amp.elementIndex ("rbst");    iFac   = amp.elementIndex ("cfac");
    oSpk   = amp.outputIndex ("spk");
    oB4    = amp.outputIndex ("b4");       oB1   = amp.outputIndex ("b1");

    const int need[] = { iGainA, iGainB, iBassA, iBassB, iTrebA, iTrebB, iBoost, iFac, oSpk, oB4, oB1 };
    for (int v : need)
        if (v < 0) { err = "a part the engine drives is missing from the netlist"; return; }

    ampOut.assign ((size_t) amp.numOutputs(), 0.0);
    good = true;
}

//==============================================================================
void Engine::prepare (double sampleRate, int block)
{
    sr = sampleRate;
    maxBlock = std::max (1, block);
    p = pending;

    /*  The circuit runs at no less than 88.2 kHz (176.4 kHz on HIGH): the
        solver was checked against ngspice at both, and below them the
        trapezoidal rule's warping and the aliasing of a clipped EL34 both
        start to be measurable in the audio band. */
    const double target = (p.quality >= 2) ? 352800.0 : (p.quality == 1) ? 176400.0 : 88200.0;
    os = 1;
    while (sr * os < target - 1.0 && os < 4) os *= 2;
    srOS = sr * os;

    // each half-band stage is a 63-tap FIR, 31 samples late at its own rate, twice
    latency = (os == 1) ? 0 : (os == 2) ? 31 : 47;

    for (auto* h : { &up1, &up2, &dn1, &dn2 }) { h->design(); h->clear(); }
    osIn.assign ((size_t) os, 0.0f);
    osSpk.assign ((size_t) os, 0.0f);
    dcIn = dcOut = 0.0;

    if (! good) return;


    sGain = p.gain; sBass = p.bass; sTreb = p.treble; sBoost = p.boost;
    outGain = std::pow (10.0f, outputDb (p.output) / 20.0f);
    applyPots (true);
    if (! warmUp()) good = false;
}

/*  The amp switched on, off stage, and left to warm up: the mains run into
    the transformer and the rectifier until every rail has settled at its own
    average under its own ripple - which takes seconds of circuit time, the
    slowest filter being about a second. That much is run at 16 kHz, where it
    costs a fraction of a second; the solver then changes rate with every
    capacitor's charge and every inductor's current carried across, and a
    last stretch at the real rate settles what only shows up there. */
bool Engine::warmUp()
{
    constexpr double WARM_RATE = 16000.0, WARM_SECONDS = 5.0;
    amp.prepare (WARM_RATE);
    double in[2] = { 0.0, MAINS_PEAK };      // the peak of the cycle, as the DC guess
    if (! amp.solveDC (in)) { err = "DC: " + amp.error(); return false; }

    mainsPhase = 0.0;
    const auto run = [&] (double rate, double seconds)
    {
        const double dph = MAINS_HZ / rate;
        const int n = (int) (seconds * rate);
        for (int i = 0; i < n; ++i)
        {
            const double m = std::sin (2.0 * PI * mainsPhase);
            mainsPhase += dph; if (mainsPhase >= 1.0) mainsPhase -= 1.0;
            in[0] = 0.0;
            in[1] = p.mains ? MAINS_PEAK * m : MAINS_PEAK;
            amp.process (in, ampOut.data());
        }
    };
    run (WARM_RATE, WARM_SECONDS);
    amp.changeRate (srOS);
    run (srOS, 0.1);

    b1Idle = ampOut[(size_t) oB1];
    b1Smooth = b1Idle;
    amp.resetStats();
    return true;
}

//==============================================================================
/*  The knobs become parts. Pots are two resistors either side of the wiper;
    the circuit is rebuilt only when one has really moved, because a rebuild is
    a factorisation of the whole MNA system. */
void Engine::applyPots (bool force)
{
    constexpr float EPS = 2.0e-4f;

    if (force || std::abs (sGain - aGain) > EPS)
    {
        const double b = audioTaper (sGain);
        amp.setValue (iGainA, potHalf (1e6, 1.0 - b));
        amp.setValue (iGainB, potHalf (1e6, b));
        aGain = sGain;
    }
    /*  BASS and TREBLE are log pots on the sheet. Turning up moves the wiper
        toward the boost end (bass: the 100k; treble: the 1n5), so the log
        track's slow end, the counter-clockwise terminal, is the cut end: at
        noon the wiper is a tenth of the way from it. That is how a
        Baxandall built with log pots behaves - it is not flat at noon. */
    if (force || std::abs (sBass - aBass) > EPS)
    {
        const double b = audioTaper (sBass);
        amp.setValue (iBassA, potHalf (1e6, 1.0 - b));
        amp.setValue (iBassB, potHalf (1e6, b));
        aBass = sBass;
    }
    if (force || std::abs (sTreb - aTreb) > EPS)
    {
        const double b = audioTaper (sTreb);
        amp.setValue (iTrebA, potHalf (1e6, 1.0 - b));
        amp.setValue (iTrebB, potHalf (1e6, b));
        aTreb = sTreb;
    }
    if (force || std::abs (sBoost - aBoost) > EPS)
    {
        // a 1k lin rheostat: up is less resistance in series with the choke
        amp.setValue (iBoost, std::max (2.0, 1e3 * (1.0 - (double) sBoost)));
        aBoost = sBoost;
    }
    const int fac = std::clamp (p.fac, 0, NUM_FAC - 1);
    if (force || fac != aFac)
    {
        amp.setValue (iFac, FAC_CAPS[fac]);
        aFac = fac;
    }
    amp.update();
}

//==============================================================================
void Engine::process (float* left, float* right, int numSamples)
{
    p = pending;

    if (! good)
    {
        // nothing to stand behind: pass the guitar through, untouched
        mIn.store (0.0f); mOut.store (0.0f);
        return;
    }

    const float targetOut = std::pow (10.0f, outputDb (p.output) / 20.0f);
    float peakIn = 0.0f, peakOut = 0.0f;

    for (int start = 0; start < numSamples; start += CHUNK)
    {
        const int n = std::min (CHUNK, numSamples - start);

        // knobs: about 25 ms to arrive, so a fast turn is a sweep, not a step
        const float k = 1.0f - std::exp (-(float) n / (0.025f * (float) sr));
        sGain   += (p.gain    - sGain)   * k;
        sBass   += (p.bass    - sBass)   * k;
        sTreb   += (p.treble  - sTreb)   * k;
        sBoost  += (p.boost   - sBoost)  * k;
        applyPots (false);

        const float ko = 1.0f - std::exp (-1.0f / (0.020f * (float) sr));

        bool blewUp = false;
        for (int i = start; i < start + n; ++i)
        {
            const float l = left[i], r = right ? right[i] : left[i];
            float x = 0.5f * (l + r);
            peakIn = std::max (peakIn, std::abs (x));

            // the jack is DC coupled to the grid in the iron; a DAW's DC is not a guitar
            dcIn += (x - dcIn) * (2.0 * PI * 5.0 / sr);
            x -= (float) dcIn;

            if (os == 1)       osIn[0] = x;
            else if (os == 2)  up1.up (x, osIn[0], osIn[1]);
            else
            {
                float a, b;
                up1.up (x, a, b);
                up2.up (a, osIn[0], osIn[1]);
                up2.up (b, osIn[2], osIn[3]);
            }

            for (int j = 0; j < os; ++j)
            {
                const double m = std::sin (2.0 * PI * mainsPhase);
                mainsPhase += MAINS_HZ / srOS; if (mainsPhase >= 1.0) mainsPhase -= 1.0;
                const double in[2] = { (double) osIn[(size_t) j] * IN_VOLTS, p.mains ? MAINS_PEAK * m : MAINS_PEAK };
                amp.process (in, ampOut.data());

                const double spk  = ampOut[(size_t) oSpk];
                b1Smooth += (ampOut[(size_t) oB1] - b1Smooth) * (1.0 / (0.05 * srOS));

                if (! std::isfinite (spk) || std::abs (spk) > 2000.0) { blewUp = true; break; }

                osSpk[(size_t) j]  = (float) spk;
                wattsAcc += (spk * spk / 8.0 - wattsAcc) * (1.0 / (0.15 * srOS));
            }
            if (blewUp) break;

            double spk;
            if (os == 1)      spk = osSpk[0];
            else if (os == 2) spk = dn1.down (osSpk[0], osSpk[1]);
            else
            {
                // two statements, not two arguments: the order of a call's
                // arguments is unspecified, and these two calls share state -
                // nested, GCC ran the second first and folded every 4x block
                // back on itself (the bench caught it as HIGH aliasing worse
                // than NORMAL)
                const float s0 = dn2.down (osSpk[0], osSpk[1]);
                const float s1 = dn2.down (osSpk[2], osSpk[3]);
                spk = dn1.down (s0, s1);
            }

            double y = spk * SPK_TO_FS;

            // the output transformer passes no DC, but a lopsided clip has some for a moment
            dcOut += (y - dcOut) * (2.0 * PI * 8.0 / sr);
            y -= dcOut;

            outGain += (targetOut - outGain) * ko;
            const float o = (float) (y * outGain);
            peakOut = std::max (peakOut, std::abs (o));
            left[i] = o;
            if (right) right[i] = o;
        }

        if (blewUp)
        {
            // Never seen in the bench's sweeps; if it ever happens, the amp is
            // switched off and on again rather than allowed to scream.
            ++resetCount;
            warmUp();
            for (auto* h : { &up1, &up2, &dn1, &dn2 }) h->clear();
            for (int i = start; i < numSamples; ++i) { left[i] = 0.0f; if (right) right[i] = 0.0f; }
            break;
        }
    }

    // meters, smoothed the way a needle would be
    const auto mix = [] (std::atomic<float>& m, float v, float up, float down)
    {
        const float o = m.load();
        m.store (o + (v - o) * (v > o ? up : down));
    };
    mix (mIn, peakIn, 0.6f, 0.08f);
    mix (mOut, peakOut, 0.6f, 0.08f);
    mWatts.store ((float) wattsAcc);
    mix (mSag, (float) std::max (0.0, b1Idle - b1Smooth), 0.3f, 0.05f);
    mix (mGlow, (float) std::clamp (wattsAcc / 120.0, 0.0, 1.0), 0.3f, 0.03f);
}

} // namespace mm
