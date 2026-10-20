#include "Engine50.h"
#include "Tv50Netlist.h"
#include "Tv50Split.h"

#include <algorithm>

namespace mm
{

namespace
{
    constexpr double PI = 3.14159265358979323846;

    // 0 dBFS is 1 V peak at the jack, as in the Marmalade 120
    constexpr double IN_VOLTS = 1.0;

    /*  The 8 ohm tap as a line level: a 50 W pair clips near 30 V peak, so
        flat out sits a few dB under full scale before the OUTPUT trim. */
    constexpr double SPK_TO_FS = 1.0 / 40.0;

    constexpr double MAINS_HZ   = 50.0;
    constexpr double MAINS_PEAK = 240.0 * 1.41421356237;

    constexpr double POT_END = 100.0;          // a pot's track never quite reaches 0 ohms
    constexpr double MADE = 0.1, OPEN = 1.0e9; // a relay contact

    inline double audioTaper (double t)
    {
        t = std::clamp (t, 0.0, 1.0);
        return (std::pow (10.0, 2.0 * t) - 1.0) / 99.0;
    }
    inline double part (double total, double fraction) { return std::max (POT_END, total * fraction); }
}

//==============================================================================
Engine50::Engine50()
{
    if (! amp.build (TV50_AMP, TV50_CUTS, TV50_ORDER)) { err = "netlist: " + amp.error(); return; }
    oSpk = amp.outputIndex ("spk");
    oTank = amp.outputIndex ("tank");
    oB = amp.outputIndex ("b");
    if (oSpk < 0 || oTank < 0 || oB < 0) { err = "an output the engine reads is missing"; return; }

    const char* parts[] = { "rg1a", "rg1b", "rg2a", "rg2b", "rsha", "rshb", "rshc", "rshd", "rv1a", "rv1b",
                            "rg3a", "rg3b", "rg4a", "rg4b", "rtra", "rtrb", "rbas", "rmia", "rmib", "rv2a", "rv2b",
                            "rrva", "rrvb", "rata", "ratb", "ratc", "ratd",
                            "rrl4x", "rrl4y", "rrl1x", "rrl1y", "rrl2", "rrl3a", "rrl3b", "rrl3c", "rrl3d",
                            "rjn", "rjr" };
    for (const char* n : parts)
        if (amp.getValue (n) <= 0.0) { err = std::string ("a part the engine drives is missing: ") + n; return; }

    ampOut.assign ((size_t) amp.numOutputs(), 0.0);
    good = true;
}

//==============================================================================
void Engine50::prepare (double sampleRate, int block)
{
    sr = sampleRate;
    maxBlock = std::max (1, block);
    p = pending;

    const double target = (p.quality >= 1) ? 176400.0 : 88200.0;
    os = 1;
    while (sr * os < target - 1.0 && os < 4) os *= 2;
    srOS = sr * os;
    latency = (os == 1) ? 0 : (os == 2) ? 31 : 47;

    for (auto* h : { &up1, &up2, &dn1, &dn2 }) { h->design(); h->clear(); }
    osIn.assign ((size_t) os, 0.0f);
    osSpk.assign ((size_t) os, 0.0f);
    dcIn = dcOut = 0.0;
    std::fill (std::begin (tankBuf), std::end (tankBuf), 0.0f);
    std::fill (std::begin (tankRet), std::end (tankRet), 0.0f);
    tankAcc = 0.0; tankFill = 0; retPrev = 0.0;

    if (! good) return;

    const float knobs[NPOT] = { p.gain1, p.shape, p.volume1, p.gain2, p.treble, p.middle, p.bass, p.volume2, p.reverb, p.atten };
    for (int k = 0; k < NPOT; ++k) s[k] = knobs[k];
    outGain = std::pow (10.0f, outputDb (p.output) / 20.0f);
    applyPots (true);
    if (! warmUp()) good = false;
}

/*  Switched on and left to warm up: 10 s of mains at 16 kHz, enough for the
    slowest filter - the bias supply's 220k into 10u, settled to 10 mV by
    then (measured) - then the real rate, with every state carried across. */
bool Engine50::warmUp()
{
    constexpr double WARM_RATE = 16000.0, WARM_SECONDS = 10.0;
    amp.prepare (WARM_RATE);
    double in[4] = { 0.0, MAINS_PEAK, 0.0, 0.0 };
    if (! amp.solveDC (in)) { err = "DC: " + amp.error(); return false; }

    mainsPhase = 0.0;
    const auto run = [&] (double rate, double seconds)
    {
        const int n = (int) (seconds * rate);
        for (int i = 0; i < n; ++i)
        {
            const double m = std::sin (2.0 * PI * mainsPhase);
            mainsPhase += MAINS_HZ / rate; if (mainsPhase >= 1.0) mainsPhase -= 1.0;
            in[1] = MAINS_PEAK * m;
            amp.process (in, ampOut.data());
        }
    };
    run (WARM_RATE, WARM_SECONDS);
    amp.changeRate (srOS);
    run (srOS, 0.1);

    bIdle = bSmooth = ampOut[(size_t) oB];
    amp.resetStats();
    return true;
}

//==============================================================================
/*  The pots and relays become parts, by the sheet's pin numbers: clockwise
    takes a wiper toward pin 3 (pin 6 on a second gang), and an A track is
    log. A part is only rebuilt when it really moved. */
void Engine50::applyPots (bool force)
{
    constexpr float EPS = 2.0e-4f;
    const auto moved = [&] (int k) { return force || std::abs (s[k] - a[k]) > EPS; };

    if (moved (G1))
    {
        // RV1, dual A1M0: pin 3 (6) the top of the track, toward the next grid
        const double b = audioTaper (s[G1]);
        amp.setValue ("rg1a", part (1e6, 1.0 - b)); amp.setValue ("rg1b", part (1e6, b));
        amp.setValue ("rg2a", part (1e6, 1.0 - b)); amp.setValue ("rg2b", part (1e6, b));
        a[G1] = s[G1];
    }
    if (moved (SH))
    {
        // RV2, dual B500K: section A from C35 (pin 3) to C34 (pin 1), section B from there to 0 V
        const double t = std::clamp ((double) s[SH], 0.0, 1.0);
        amp.setValue ("rsha", part (5e5, 1.0 - t)); amp.setValue ("rshb", part (5e5, t));
        amp.setValue ("rshc", part (5e5, t));       amp.setValue ("rshd", part (5e5, 1.0 - t));
        a[SH] = s[SH];
    }
    if (moved (V1))
    {
        const double b = audioTaper (s[V1]);
        amp.setValue ("rv1a", part (5e5, 1.0 - b)); amp.setValue ("rv1b", part (5e5, b));
        a[V1] = s[V1];
    }
    if (moved (G2))
    {
        const double b = audioTaper (s[G2]);
        amp.setValue ("rg3a", part (1e6, 1.0 - b)); amp.setValue ("rg3b", part (1e6, b));
        amp.setValue ("rg4a", part (1e6, 1.0 - b)); amp.setValue ("rg4b", part (1e6, b));
        a[G2] = s[G2];
    }
    if (moved (TR))
    {
        const double t = std::clamp ((double) s[TR], 0.0, 1.0);
        amp.setValue ("rtra", part (2.5e5, 1.0 - t)); amp.setValue ("rtrb", part (2.5e5, t));
        a[TR] = s[TR];
    }
    if (moved (MI))
    {
        const double t = std::clamp ((double) s[MI], 0.0, 1.0);
        amp.setValue ("rmia", part (2.5e4, 1.0 - t)); amp.setValue ("rmib", part (2.5e4, t));
        a[MI] = s[MI];
    }
    if (moved (BA))
    {
        // RV5, A500K with its wiper on pin 3: a rheostat, pin 1 to the wiper
        amp.setValue ("rbas", part (5e5, audioTaper (s[BA])));
        a[BA] = s[BA];
    }
    if (moved (V2))
    {
        const double b = audioTaper (s[V2]);
        amp.setValue ("rv2a", part (5e5, 1.0 - b)); amp.setValue ("rv2b", part (5e5, b));
        a[V2] = s[V2];
    }
    if (moved (RV))
    {
        // RV9, 250KA: the recovery's signal on the wiper, pin 3 out to the mix, pin 1 to 0 V
        const double b = audioTaper (s[RV]);
        amp.setValue ("rrva", part (2.5e5, 1.0 - b)); amp.setValue ("rrvb", part (2.5e5, b));
        a[RV] = s[RV];
    }
    if (moved (AT))
    {
        // RV10, dual B1M0: pin 1 at the splitter's output, pin 3 at 0 V - clockwise attenuates
        const double t = std::clamp ((double) s[AT], 0.0, 1.0);
        amp.setValue ("rata", part (1e6, t)); amp.setValue ("ratb", part (1e6, 1.0 - t));
        amp.setValue ("ratc", part (1e6, t)); amp.setValue ("ratd", part (1e6, 1.0 - t));
        a[AT] = s[AT];
    }

    // the relays: the idle channel grounded at its gain pot's top and its volume's wiper
    const int ch = std::clamp (p.channel, 0, 1);
    if (force || ch != aChannel)
    {
        amp.setValue ("rrl4x", ch == 0 ? OPEN : MADE); amp.setValue ("rrl4y", ch == 0 ? MADE : OPEN);
        amp.setValue ("rrl1x", ch == 0 ? OPEN : MADE); amp.setValue ("rrl1y", ch == 0 ? MADE : OPEN);
        aChannel = ch;
    }
    const int rv = p.reverbOn ? 1 : 0;
    if (force || rv != aRevOn)
    {
        amp.setValue ("rrl2", rv ? OPEN : MADE);    // RL2 grounds the recovery's grid when off
        aRevOn = rv;
    }
    const int at = p.attenOn ? 1 : 0;
    if (force || at != aAttOn)
    {
        amp.setValue ("rrl3a", at ? OPEN : MADE); amp.setValue ("rrl3b", at ? MADE : OPEN);
        amp.setValue ("rrl3c", at ? OPEN : MADE); amp.setValue ("rrl3d", at ? MADE : OPEN);
        aAttOn = at;
    }
    const int rt = p.intoReturn ? 1 : 0;
    if (force || rt != aReturn)
    {
        amp.setValue ("rjn", rt ? OPEN : MADE); amp.setValue ("rjr", rt ? MADE : OPEN);
        aReturn = rt;
    }
    amp.update();
}

//==============================================================================
void Engine50::process (float* left, float* right, int numSamples)
{
    p = pending;
    if (! good) { mIn.store (0.0f); mOut.store (0.0f); return; }

    const float targetOut = std::pow (10.0f, outputDb (p.output) / 20.0f);
    float peakIn = 0.0f, peakOut = 0.0f;
    const float knobs[NPOT] = { p.gain1, p.shape, p.volume1, p.gain2, p.treble, p.middle, p.bass, p.volume2, p.reverb, p.atten };

    for (int start = 0; start < numSamples; start += CHUNK)
    {
        const int n = std::min (CHUNK, numSamples - start);

        const float k = 1.0f - std::exp (-(float) n / (0.025f * (float) sr));
        for (int j = 0; j < NPOT; ++j) s[j] += (knobs[j] - s[j]) * k;
        applyPots (false);

        const float ko = 1.0f - std::exp (-1.0f / (0.020f * (float) sr));

        bool blewUp = false;
        for (int i = start; i < start + n; ++i)
        {
            const float l = left[i], r = right ? right[i] : left[i];
            float x = 0.5f * (l + r);
            peakIn = std::max (peakIn, std::abs (x));
            dcIn += (x - dcIn) * (2.0 * PI * 5.0 / sr);
            x -= (float) dcIn;

            if (os == 1)       osIn[0] = x;
            else if (os == 2)  up1.up (x, osIn[0], osIn[1]);
            else
            {
                float a0, b0;
                up1.up (x, a0, b0);
                up2.up (a0, osIn[0], osIn[1]);
                up2.up (b0, osIn[2], osIn[3]);
            }

            // the springs' output this host sample, interpolated up to the circuit's rate
            const double ret = tankRet[tankFill];
            for (int j = 0; j < os; ++j)
            {
                const double m = std::sin (2.0 * PI * mainsPhase);
                mainsPhase += MAINS_HZ / srOS; if (mainsPhase >= 1.0) mainsPhase -= 1.0;
                const double fr = (double) (j + 1) / os;
                const double x = (double) osIn[(size_t) j] * IN_VOLTS;
                const double in[4] = { p.intoReturn ? 0.0 : x,
                                       MAINS_PEAK * m,
                                       retPrev + (ret - retPrev) * fr,
                                       p.intoReturn ? x : 0.0 };
                amp.process (in, ampOut.data());

                const double spk = ampOut[(size_t) oSpk];
                bSmooth += (ampOut[(size_t) oB] - bSmooth) * (1.0 / (0.05 * srOS));
                if (! std::isfinite (spk) || std::abs (spk) > 1000.0) { blewUp = true; break; }
                osSpk[(size_t) j] = (float) spk;
                tankAcc += ampOut[(size_t) oTank];
                wattsAcc += (spk * spk / 8.0 - wattsAcc) * (1.0 / (0.15 * srOS));
            }
            if (blewUp) break;
            retPrev = ret;

            // the drive, one host sample's average, toward the next chunk's return
            tankBuf[tankFill] = (float) (tankAcc / os);
            tankAcc = 0.0;
            if (++tankFill == CHUNK)
            {
                tankFill = 0;
                if (tank) { std::copy (std::begin (tankBuf), std::end (tankBuf), tankRet); tank (tankRet, CHUNK); }
                else        std::fill (std::begin (tankRet), std::end (tankRet), 0.0f);
            }

            double spk;
            if (os == 1)      spk = osSpk[0];
            else if (os == 2) spk = dn1.down (osSpk[0], osSpk[1]);
            else
            {
                // two statements: the calls share state, and argument order is unspecified
                const float s0 = dn2.down (osSpk[0], osSpk[1]);
                const float s1 = dn2.down (osSpk[2], osSpk[3]);
                spk = dn1.down (s0, s1);
            }

            double y = spk * SPK_TO_FS;
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
            ++resetCount;
            warmUp();
            for (auto* h : { &up1, &up2, &dn1, &dn2 }) h->clear();
            for (int i = start; i < numSamples; ++i) { left[i] = 0.0f; if (right) right[i] = 0.0f; }
            break;
        }
    }

    const auto mix = [] (std::atomic<float>& m, float v, float up, float down)
    {
        const float o = m.load();
        m.store (o + (v - o) * (v > o ? up : down));
    };
    mix (mIn, peakIn, 0.6f, 0.08f);
    mix (mOut, peakOut, 0.6f, 0.08f);
    mWatts.store ((float) wattsAcc);
    mix (mSag, (float) std::max (0.0, bIdle - bSmooth), 0.3f, 0.05f);
    mix (mGlow, (float) std::clamp (wattsAcc / 50.0, 0.0, 1.0), 0.3f, 0.03f);
}

} // namespace mm
