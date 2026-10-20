#include "Engine.h"
#include "Or120Netlist.h"

#include <algorithm>

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

    /*  The cone's acceleration (CAB on) brought to the same level as the
        8 ohm tap at 1 kHz, so switching CAB changes the tone, not the volume.
        Measured by the bench (cab-level check), not derived. */
    constexpr double CONE_TO_FS = 1.0 / 80.0 * 7.03;

    constexpr int CHUNK = 32;            // host samples between knob updates

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

const double FAC_CAPS[NUM_FAC] = { 47e-9, 22e-9, 10e-9, 4.7e-9, 2.2e-9, 1e-9 };

//==============================================================================
void HalfBand::design()
{
    constexpr int C = TAPS / 2;
    std::array<double, TAPS> g {};
    double sum = 0.0;
    for (int n = 0; n < TAPS; ++n)
    {
        const double t = n - C;
        const double s = (t == 0.0) ? 0.5 : std::sin (PI * 0.5 * t) / (PI * t);
        const double w = 0.42 - 0.5 * std::cos (2.0 * PI * n / (TAPS - 1))
                              + 0.08 * std::cos (4.0 * PI * n / (TAPS - 1));
        g[(size_t) n] = s * w;
        sum += g[(size_t) n];
    }
    for (auto& v : g) v /= sum;                 // unity at DC

    for (int k = 0; k < PH; ++k)
    {
        ge[(size_t) k] = (2 * k     < TAPS) ? (float) g[(size_t) (2 * k)]     : 0.0f;
        go[(size_t) k] = (2 * k + 1 < TAPS) ? (float) g[(size_t) (2 * k + 1)] : 0.0f;
    }
}

void Biquad::setLP (double f, double sr, double q)
{
    const double w = 2.0 * PI * std::clamp (f, 10.0, sr * 0.49) / sr;
    const double c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = ((1.0 - c) * 0.5) / a0;
    b1 = (1.0 - c) / a0;
    b2 = b0;
    a1 = (-2.0 * c) / a0;
    a2 = (1.0 - al) / a0;
}

void Biquad::setHP (double f, double sr, double q)
{
    const double w = 2.0 * PI * std::clamp (f, 10.0, sr * 0.49) / sr;
    const double c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = ((1.0 + c) * 0.5) / a0;
    b1 = (-(1.0 + c)) / a0;
    b2 = b0;
    a1 = (-2.0 * c) / a0;
    a2 = (1.0 - al) / a0;
}

void Biquad::setPeak (double f, double sr, double q, double dB)
{
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2.0 * PI * std::clamp (f, 10.0, sr * 0.49) / sr;
    const double c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al / A;
    b0 = (1.0 + al * A) / a0;
    b1 = (-2.0 * c) / a0;
    b2 = (1.0 - al * A) / a0;
    a1 = b1;
    a2 = (1.0 - al / A) / a0;
}

//==============================================================================
Engine::Engine()
{
    if (! pre.parse (OR120_PREAMP)) { err = "preamp: " + pre.error(); return; }
    if (! pw.parse (OR120_POWER))   { err = "power: " + pw.error();   return; }

    iGainA = pre.elementIndex ("rgaina");  iGainB = pre.elementIndex ("rgainb");
    iBassA = pre.elementIndex ("rbassa");  iBassB = pre.elementIndex ("rbassb");
    iTrebA = pre.elementIndex ("rtreba");  iTrebB = pre.elementIndex ("rtrebb");
    iHf    = pre.elementIndex ("rhfpot");  iFac   = pre.elementIndex ("cfac");
    iMasAA = pw.elementIndex ("rmasaa");   iMasAB = pw.elementIndex ("rmasab");
    iMasBA = pw.elementIndex ("rmasba");   iMasBB = pw.elementIndex ("rmasbb");
    oDrive = pre.outputIndex ("drive");
    oSpk   = pw.outputIndex ("spk");       oCone = pw.outputIndex ("cone");
    oB4    = pw.outputIndex ("b4");        oB1   = pw.outputIndex ("b1");

    const int need[] = { iGainA, iGainB, iBassA, iBassB, iTrebA, iTrebB, iHf, iFac,
                         iMasAA, iMasAB, iMasBA, iMasBB, oDrive, oSpk, oCone, oB4, oB1 };
    for (int v : need)
        if (v < 0) { err = "a part the engine drives is missing from the netlist"; return; }

    preOut.assign ((size_t) pre.numOutputs(), 0.0);
    pwOut.assign ((size_t) pw.numOutputs(), 0.0);
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
    const double target = (p.quality > 0) ? 176400.0 : 88200.0;
    os = 1;
    while (sr * os < target - 1.0 && os < 4) os *= 2;
    srOS = sr * os;

    // each half-band stage is a 63-tap FIR, 31 samples late at its own rate, twice
    latency = (os == 1) ? 0 : (os == 2) ? 31 : 47;

    for (auto* h : { &up1, &up2, &dnA1, &dnA2, &dnB1, &dnB2 }) { h->design(); h->clear(); }
    osIn.assign ((size_t) os, 0.0f);
    osSpk.assign ((size_t) os, 0.0f);
    osCone.assign ((size_t) os, 0.0f);

    // the cabinet: see cabinet()
    cab[0].setHP   (45.0,   sr, 0.70);
    cab[1].setPeak (110.0,  sr, 1.00, 2.0);
    cab[2].setPeak (2400.0, sr, 1.30, 5.0);
    cab[3].setPeak (3700.0, sr, 3.50, -6.0);
    cab[4].setLP   (5000.0, sr, 0.85);
    cab[5].setLP   (6800.0, sr, 0.60);
    for (auto& b : cab) b.clear();
    cabMix = p.cab ? 1.0f : 0.0f;
    dcIn = dcOut = 0.0;

    if (! good) return;

    pre.prepare (srOS);
    pw.prepare (srOS);

    sGain = p.gain; sBass = p.bass; sTreb = p.treble; sHf = p.hfDrive; sMaster = p.master;
    outGain = std::pow (10.0f, outputDb (p.output) / 20.0f);
    applyPots (true);
    if (! warmUp()) good = false;
}

/*  The amp switched on, off stage: both circuits put at their operating
    points, the preamp's rail taken from the power section's, then a moment of
    silence so the coupling capacitors have settled exactly where the
    trapezoidal solver (rather than the DC solver) has them. */
bool Engine::warmUp()
{
    const double zero[1] = { 0.0 };
    if (! pw.solveDC (zero)) { err = "power DC: " + pw.error(); return false; }
    double in[2] = { 0.0, 0.0 };
    pw.process (zero, pwOut.data());
    b4 = pwOut[(size_t) oB4];
    b1Idle = pwOut[(size_t) oB1];
    in[1] = b4;
    if (! pre.solveDC (in)) { err = "preamp DC: " + pre.error(); return false; }

    const int settle = (int) (0.05 * srOS);
    for (int i = 0; i < settle; ++i)
    {
        in[0] = 0.0; in[1] = b4;
        pre.process (in, preOut.data());
        const double d[1] = { preOut[(size_t) oDrive] };
        pw.process (d, pwOut.data());
        b4 = pwOut[(size_t) oB4];
    }
    b1Idle = pwOut[(size_t) oB1];
    coneLast = pwOut[(size_t) oCone];
    pre.resetStats();
    pw.resetStats();
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
        pre.setValue (iGainA, potHalf (1e6, 1.0 - b));
        pre.setValue (iGainB, potHalf (1e6, b));
        aGain = sGain;
    }
    if (force || std::abs (sBass - aBass) > EPS)
    {
        // wiper toward the input end is boost
        pre.setValue (iBassA, potHalf (1e6, 1.0 - sBass));
        pre.setValue (iBassB, potHalf (1e6, sBass));
        aBass = sBass;
    }
    if (force || std::abs (sTreb - aTreb) > EPS)
    {
        pre.setValue (iTrebA, potHalf (1e6, 1.0 - sTreb));
        pre.setValue (iTrebB, potHalf (1e6, sTreb));
        aTreb = sTreb;
    }
    if (force || std::abs (sHf - aHf) > EPS)
    {
        // a linear 1M as a rheostat, wired so its travel is spread usefully
        const double r = 1.0 - (double) sHf;
        pre.setValue (iHf, 1e3 + 1e6 * r * r);
        aHf = sHf;
    }
    const int fac = std::clamp (p.fac, 0, NUM_FAC - 1);
    if (force || fac != aFac)
    {
        pre.setValue (iFac, FAC_CAPS[fac]);
        aFac = fac;
    }
    if (force || std::abs (sMaster - aMaster) > EPS)
    {
        const double b = audioTaper (sMaster);
        pw.setValue (iMasAA, potHalf (220e3, 1.0 - b));
        pw.setValue (iMasAB, potHalf (220e3, b));
        pw.setValue (iMasBA, potHalf (220e3, 1.0 - b));
        pw.setValue (iMasBB, potHalf (220e3, b));
        aMaster = sMaster;
    }
    pre.update();
    pw.update();
}

//==============================================================================
/*  The one stage that is not a circuit: from the cone to a close microphone.

    The physics is still real up to the cone: the speaker's electrical
    impedance (voice coil, its inductance, the mechanical resonance reflected
    as a parallel RLC) is part of the power-section netlist, so the output
    stage feels its load and the motional voltage across that RLC is the
    cone's velocity. Its time derivative - the acceleration, which is what a
    cone radiates - is taken at the oversampled rate and handed here. Below
    resonance that already falls at 12 dB/octave, as a sealed 4x12 does.

    What the cone's breakup, the box and a close dynamic mic add on top is
    modelled as a fixed response: the usual 2 - 3 kHz presence of a 12" guitar
    cone, a narrow cancellation notch above it, the box's thump, and the steep
    fall past 5 kHz every guitar speaker has. This is the part tuned by ear,
    and the part to switch off (CAB) when using impulse responses instead. */
double Engine::cabinet (double coneSlope)
{
    double y = coneSlope;
    for (auto& b : cab) y = b.process (y);
    return y;
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
        sHf     += (p.hfDrive - sHf)     * k;
        sMaster += (p.master  - sMaster) * k;
        applyPots (false);

        const float cabTarget = p.cab ? 1.0f : 0.0f;
        const float kc = 1.0f - std::exp (-1.0f / (0.010f * (float) sr));
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
                const double in[2] = { (double) osIn[(size_t) j] * IN_VOLTS, b4 };
                pre.process (in, preOut.data());
                const double d[1] = { preOut[(size_t) oDrive] };
                pw.process (d, pwOut.data());

                const double spk  = pwOut[(size_t) oSpk];
                const double cone = pwOut[(size_t) oCone];
                b4 = pwOut[(size_t) oB4];

                if (! std::isfinite (spk) || std::abs (spk) > 2000.0) { blewUp = true; break; }

                // acceleration, scaled so 1 kHz reads 1:1 with velocity
                const double slope = (cone - coneLast) * srOS / (2.0 * PI * 1000.0);
                coneLast = cone;

                osSpk[(size_t) j]  = (float) spk;
                osCone[(size_t) j] = (float) slope;
                wattsAcc += (spk * spk / 8.0 - wattsAcc) * (1.0 / (0.15 * srOS));
            }
            if (blewUp) break;

            double spk, slope;
            if (os == 1)      { spk = osSpk[0]; slope = osCone[0]; }
            else if (os == 2) { spk = dnA1.down (osSpk[0], osSpk[1]); slope = dnB1.down (osCone[0], osCone[1]); }
            else
            {
                const float s0 = dnA2.down (osSpk[0], osSpk[1]),   s1 = dnA2.down (osSpk[2], osSpk[3]);
                const float c0 = dnB2.down (osCone[0], osCone[1]), c1 = dnB2.down (osCone[2], osCone[3]);
                spk = dnA1.down (s0, s1);
                slope = dnB1.down (c0, c1);
            }

            const double line = spk * SPK_TO_FS;
            const double miked = cabinet (slope) * CONE_TO_FS;
            cabMix += (cabTarget - cabMix) * kc;
            double y = miked * cabMix + line * (1.0f - cabMix);

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
            for (auto* h : { &up1, &up2, &dnA1, &dnA2, &dnB1, &dnB2 }) h->clear();
            for (auto& b : cab) b.clear();
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
    mix (mSag, (float) std::max (0.0, b1Idle - pwOut[(size_t) oB1]), 0.3f, 0.05f);
    mix (mGlow, (float) std::clamp (wattsAcc / 120.0, 0.0, 1.0), 0.3f, 0.03f);
}

} // namespace mm
