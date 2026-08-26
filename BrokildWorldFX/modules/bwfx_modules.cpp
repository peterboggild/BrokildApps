// The founding BWFX modules, extracted from Photo-Synth 2's native engine
// (C:\Users\peter\b\PhotoSynth\Source\Engine.cpp) with the UI-side value
// mappings folded in, so each module's knobs mean exactly what the pedal
// said in Photo-Synth. JUCE-free by construction.

#include "../src/bwfx.h"
#include "../src/bwfx_dsp.h"
#include "../src/bwfx_json.h"
#include "bwfx_conv.h"

#include <cstdio>
#include <cstring>

namespace bwfx
{

namespace
{
    inline int nextPow2 (int v) { int p = 1; while (p < v) p <<= 1; return p; }

    // equal-power mix law shared by every pedal (the WebAudio original)
    inline void mixLaw (float mix01, float& dryG, float& wetG)
    {
        const float m = clampf (mix01, 0.0f, 1.0f);
        dryG = std::cos (m * 1.5707963f);
        wetG = std::sin (m * 1.5707963f);
    }
}

//==============================================================================
// TUBE — asymmetric triode saturation, 4x oversampled.
// drive: 0..24 (the loudest knob rule: pre-gain is 10^(v/32), exponential);
// tone: 0..100 -> 800..18000 Hz low-pass on the wet path.
namespace
{
    constexpr double SAT_K = 4.0, SAT_BIAS = 0.11;

    inline double satCurve (double v)
    {
        static const double zero = std::tanh (SAT_K * SAT_BIAS);
        static const double hi   = std::tanh (SAT_K * (1 + SAT_BIAS)) - zero;
        static const double lo   = std::tanh (SAT_K * (-1 + SAT_BIAS)) - zero;
        static const double norm = std::max (std::max (std::abs (hi), std::abs (lo)), 0.001);
        return (std::tanh (SAT_K * (clampd (v, -1, 1) + SAT_BIAS)) - zero) / norm;
    }

    // RMS compensation so the wet path sits at the dry level (browser port)
    float satComp (float driveDb)
    {
        const double pre = std::pow (10.0, driveDb / 32.0);
        double sIn = 0, sOut = 0;
        for (int i = 0; i < 128; ++i)
        {
            const double x = 0.3 * std::sin (6.2831853 * i / 128);
            const double y = satCurve (pre * x);
            sIn += x * x; sOut += y * y;
        }
        return sOut > 1e-9 ? (float) std::sqrt (sIn / sOut) : 1.0f;
    }
}

class TubeSat : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override
    {
        fs = fsr;
        for (auto* h : { &upA_L, &upB_L, &upA_R, &upB_R }) h->design();
        satDC.set (Biquad::highpass, 18, 0.5, 0, fs);
        toneBq.set (Biquad::lowpass, toneHz (getParam (1)), 1.0, 0, fs);
        drive.init (getParam (0));
        tone.init (getParam (1));
        comp = satComp (drive.current);
        lastCompDrive = drive.current;

        // Measure the oversampler round trip once so the dry branch can be
        // delayed to match — otherwise dry+wet comb at mid drive.
        HalfBand a, b;
        a.design(); b.design();
        int peakAt = 0; float peakV = 0;
        for (int i = 0; i < 128; ++i)
        {
            float u0, u1, v0, v1, v2, v3;
            a.up (i == 0 ? 1.0f : 0.0f, u0, u1);
            b.up (u0, v0, v1);
            const float w0 = b.down (v0, v1);
            b.up (u1, v2, v3);
            const float w1 = b.down (v2, v3);
            const float y = a.down (w0, w1);
            if (std::abs (y) > peakV) { peakV = std::abs (y); peakAt = i; }
        }
        osDelay = peakAt;
        reset();
    }

    void reset() override
    {
        for (auto* h : { &upA_L, &upB_L, &upA_R, &upB_R }) h->clear();
        satDC.reset();
        toneBq.reset();
        dryRing.fill (0.0f);
        ringW = 0;
    }

    void process (float* L, float* R, int n) override
    {
        drive.set (getParam (0));
        tone.set (getParam (1));
        const float d = drive.tick (fs, n, 0.03f);
        const float t = tone.tick (fs, n, 0.03f);
        if (std::abs (d - lastCompDrive) > 0.02f) { comp = satComp (d); lastCompDrive = d; }
        toneBq.set (Biquad::lowpass, toneHz (t), 1.0, 0, fs);

        const float pre = (float) std::pow (10.0, d / 32.0);
        float dryG, wetG;
        mixLaw (d / 24.0f, dryG, wetG);
        wetG *= comp;

        for (int i = 0; i < n; ++i)
        {
            dryRing[(size_t) (ringW & 127)] = L[i];
            dryRing[(size_t) ((ringW & 127) + 128)] = R[i];
            const int rd = (ringW - osDelay) & 127;
            const float dl = dryRing[(size_t) rd];
            const float dr = dryRing[(size_t) (rd + 128)];
            ++ringW;

            float u0, u1, v0, v1, v2, v3;
            upA_L.up (L[i] * pre, u0, u1);
            upB_L.up (u0, v0, v1);
            v0 = (float) satCurve (v0); v1 = (float) satCurve (v1);
            const float w0 = upB_L.down (v0, v1);
            upB_L.up (u1, v2, v3);
            v2 = (float) satCurve (v2); v3 = (float) satCurve (v3);
            const float w1 = upB_L.down (v2, v3);
            float wl = upA_L.down (w0, w1);

            upA_R.up (R[i] * pre, u0, u1);
            upB_R.up (u0, v0, v1);
            v0 = (float) satCurve (v0); v1 = (float) satCurve (v1);
            const float x0 = upB_R.down (v0, v1);
            upB_R.up (u1, v2, v3);
            v2 = (float) satCurve (v2); v3 = (float) satCurve (v3);
            const float x1 = upB_R.down (v2, v3);
            float wr = upA_R.down (x0, x1);

            wl = toneBq.processL (satDC.processL (wl));
            wr = toneBq.processR (satDC.processR (wr));
            L[i] = dl * dryG + wl * wetG;
            R[i] = dr * dryG + wr * wetG;
        }
    }

private:
    static float toneHz (float v) { return 800.0f * std::pow (18000.0f / 800.0f, clampf (v, 0, 100) / 100.0f); }

    double fs = 48000;
    HalfBand upA_L, upB_L, upA_R, upB_R;
    Biquad satDC, toneBq;
    Smooth drive, tone;
    float comp = 1, lastCompDrive = -1;
    int osDelay = 0;
    std::array<float, 256> dryRing {};   // L in [0..127], R in [128..255]
    int ringW = 0;
};

//==============================================================================
// ECHO — stereo delay with tape mode. Catmull-Rom taps (a moving tap read
// linearly smears the top octave), damping + high-pass in the loop, wow in
// tape mode, tanh loop limiter for tape / runaway feedback. No noise is ever
// injected inside the loop.
class EchoDelay : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override
    {
        fs = fsr;
        const int len = nextPow2 ((int) (fs * 1.6) + 8);
        bufL.assign ((size_t) len, 0.0f);
        bufR.assign ((size_t) len, 0.0f);
        mask = len - 1;
        mix.init (getParam (0));
        timeMs.init (getParam (1));
        fbAmt.init (getParam (2));
        offMs.init (getParam (3));
        reset();
    }

    void reset() override
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        w = 0;
        dampBq.reset(); dhpBq.reset();
        wowPhase = 0;
        duckCur = 0;                       // fresh start: injection fades in
    }

    void inputDuck (float g) override { duckT = g; }

    void process (float* L, float* R, int n) override
    {
        mix.set (getParam (0));
        timeMs.set (getParam (1));
        fbAmt.set (getParam (2));
        offMs.set (getParam (3));
        const bool tape = getParam (4) > 0.5f;

        float dryG, wetG;
        mixLaw (mix.tick (fs, n, 0.05f) / 100.0f, dryG, wetG);
        const float tSec = timeMs.tick (fs, n, 0.02f) / 1000.0f;
        const float off  = offMs.tick (fs, n, 0.02f) / 1000.0f;
        const float fb   = fbAmt.tick (fs, n, 0.03f) / 100.0f;
        const float t0 = tSec + std::max (0.0f, -off);
        const float t1 = tSec + std::max (0.0f,  off);
        const bool limit = tape || fb > 0.92f;
        const float wowDepth = tape ? 0.0007f : 0.0f;

        dampBq.set (Biquad::lowpass,  tape ? 2600.0 : 4200.0, 1.0, 0, fs);
        dhpBq.set  (Biquad::highpass, tape ? 120.0  : 25.0,  0.5, 0, fs);
        const double wowStep = kTwoPi * 0.55 / fs;
        const double tanhNorm = std::tanh (1.6);
        const float dcoef = 1.0f - std::exp ((float) (-1.0 / (0.003 * fs)));

        for (int i = 0; i < n; ++i)
        {
            duckCur += (duckT - duckCur) * dcoef;
            const float wow = wowDepth * (float) std::sin (wowPhase);
            wowPhase += wowStep; if (wowPhase > kTwoPi) wowPhase -= kTwoPi;

            const float outL = catmullRead (bufL, mask, w, clampd ((double) t0 + wow, 0.0, 1.5) * fs);
            const float outR = catmullRead (bufR, mask, w, clampd ((double) t1 + wow, 0.0, 1.5) * fs);

            float lpL = dampBq.processL (outL), lpR = dampBq.processR (outR);
            lpL = dhpBq.processL (lpL); lpR = dhpBq.processR (lpR);
            if (limit)
            {
                lpL = (float) (std::tanh (1.6 * lpL) / tanhNorm);
                lpR = (float) (std::tanh (1.6 * lpR) / tanhNorm);
            }
            bufL[(size_t) (w & mask)] = L[i] * duckCur + lpL * fb;
            bufR[(size_t) (w & mask)] = R[i] * duckCur + lpR * fb;
            ++w;

            L[i] = L[i] * dryG + outL * wetG;
            R[i] = R[i] * dryG + outR * wetG;
        }
        if (w > (1 << 30)) w -= (1 << 29);
    }

private:
    double fs = 48000;
    std::vector<float> bufL, bufR;
    int mask = 0, w = 0;
    Biquad dampBq, dhpBq;
    double wowPhase = 0;
    Smooth mix, timeMs, fbAmt, offMs;
    float duckT = 1, duckCur = 1;
};

//==============================================================================
// SPACE — stereo convolution reverb on the deterministic Photo-Synth IRs.
// The IR is rebuilt in service() on the message thread (the irLock lesson:
// never under the audio thread) and crossfaded in by the convolver.
class ConvReverb : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int maxBlock) override
    {
        fs = fsr;
        conv.prepare (fs, 6.0f);
        wetL.assign ((size_t) std::max (maxBlock, kSubBlock), 0.0f);
        wetR.assign ((size_t) std::max (maxBlock, kSubBlock), 0.0f);
        mix.init (getParam (0));
        builtType = -1; builtLen = -1;      // service() builds the first IR
        lastSeenType = -1; lastSeenLen = -1;
        reset();
    }

    void reset() override { conv.reset(); }

    void service() override
    {
        // Rebuild only when the knobs have settled for one service tick —
        // dragging LENGTH otherwise queues a dozen full FFT set builds.
        const int   type = (int) std::lround (getParam (1));
        const float len  = clampf (getParam (2) / 100.0f, 0.2f, 6.0f);
        const bool settled = type == lastSeenType && std::abs (len - lastSeenLen) < 1e-4f;
        lastSeenType = type; lastSeenLen = len;
        if (! settled) return;
        if (type == builtType && std::abs (len - builtLen) < 1e-4f) return;
        std::vector<float> ir;
        const int frames = makeReverbImpulse (type, len, fs, ir);
        conv.setImpulse (ir.data(), ir.data() + frames, frames);
        builtType = type; builtLen = len;
    }

    void process (float* L, float* R, int n) override
    {
        mix.set (getParam (0));
        float dryG, wetG;
        mixLaw (mix.tick (fs, n, 0.05f) / 100.0f, dryG, wetG);

        std::memset (wetL.data(), 0, sizeof (float) * (size_t) n);
        std::memset (wetR.data(), 0, sizeof (float) * (size_t) n);
        conv.process (L, R, wetL.data(), wetR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            L[i] = L[i] * dryG + wetL[(size_t) i] * wetG;
            R[i] = R[i] * dryG + wetR[(size_t) i] * wetG;
        }
    }

private:
    double fs = 48000;
    PartConv conv;
    std::vector<float> wetL, wetR;
    Smooth mix;
    int builtType = -1;
    float builtLen = -1;
    int lastSeenType = -1;
    float lastSeenLen = -1;
};

//==============================================================================
// SWEEP — vintage 4-stage phaser. The sweep is recomputed every 32 samples
// (per-host-block stepping zippers on deep sweeps) and the all-pass centres
// are kept away from 0 Hz, where the section degenerates and the sound
// drops out at the bottom of the sweep.
class PhaserFx : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override
    {
        fs = fsr;
        mix.init (getParam (0));
        rate.init (getParam (1));
        depth.init (getParam (2));
        reset();
    }

    void reset() override
    {
        for (auto& b : ap) b.reset();
        fbL.fill (0.0f); fbR.fill (0.0f);
        fbPos = 0;
        lfoPhase = 0;
    }

    void process (float* L, float* R, int n) override
    {
        mix.set (getParam (0));
        rate.set (getParam (1));
        depth.set (getParam (2));
        float dryG, wetG;
        mixLaw (mix.tick (fs, n, 0.05f) / 100.0f, dryG, wetG);
        const float rateHz  = rate.tick (fs, n, 0.05f) / 100.0f;
        const float depthHz = 80.0f + 780.0f * depth.tick (fs, n, 0.05f) / 100.0f;

        static const double base[4] = { 300, 650, 1050, 1500 };
        int done = 0;
        while (done < n)
        {
            const int m = std::min (32, n - done);
            const double lfoV = std::sin (lfoPhase);
            for (int s = 0; s < 4; ++s)
                ap[(size_t) s].set (Biquad::allpass,
                                    clampd (base[s] + depthHz * lfoV, 20.0, fs * 0.49), 0.6, 0, fs);
            lfoPhase += kTwoPi * rateHz * m / fs;
            if (lfoPhase > kTwoPi) lfoPhase -= kTwoPi;

            for (int i = done; i < done + m; ++i)
            {
                const int rp = (fbPos - 128) & 255;
                float xl = L[i] + 0.3f * fbL[(size_t) rp];
                float xr = R[i] + 0.3f * fbR[(size_t) rp];
                for (int s = 0; s < 4; ++s) { xl = ap[(size_t) s].processL (xl); xr = ap[(size_t) s].processR (xr); }
                fbL[(size_t) (fbPos & 255)] = xl;
                fbR[(size_t) (fbPos & 255)] = xr;
                fbPos = (fbPos + 1) & 0x3fffffff;
                L[i] = L[i] * dryG + xl * wetG;
                R[i] = R[i] * dryG + xr * wetG;
            }
            done += m;
        }
    }

private:
    double fs = 48000;
    std::array<Biquad, 4> ap;
    std::array<float, 256> fbL {}, fbR {};
    int fbPos = 0;
    double lfoPhase = 0;
    Smooth mix, rate, depth;
};

//==============================================================================
// ENSEMBLE — dual-line chorus, Catmull-Rom taps, 5.5 kHz wet low-pass.
class ChorusFx : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override
    {
        fs = fsr;
        const int len = nextPow2 ((int) (fs * 0.07) + 8);
        bufL.assign ((size_t) len, 0.0f);
        bufR.assign ((size_t) len, 0.0f);
        mask = len - 1;
        lp.set (Biquad::lowpass, 5500, 1.0, 0, fs);
        mix.init (getParam (0));
        rate.init (getParam (1));
        depth.init (getParam (2));
        reset();
    }

    void reset() override
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        w = 0;
        lp.reset();
        lfoPhase = 0;
        duckCur = 0;
    }

    void inputDuck (float g) override { duckT = g; }

    void process (float* L, float* R, int n) override
    {
        mix.set (getParam (0));
        rate.set (getParam (1));
        depth.set (getParam (2));
        float dryG, wetG;
        mixLaw (mix.tick (fs, n, 0.05f) / 100.0f, dryG, wetG);
        const float rateHz = rate.tick (fs, n, 0.05f) / 100.0f;
        const double depthS = 0.0008 + 0.0042 * (double) depth.tick (fs, n, 0.05f) / 100.0;
        const double step = kTwoPi * rateHz / fs;
        const float dcoef = 1.0f - std::exp ((float) (-1.0 / (0.003 * fs)));

        for (int i = 0; i < n; ++i)
        {
            const double lfoS = std::sin (lfoPhase) * depthS;
            lfoPhase += step; if (lfoPhase > kTwoPi) lfoPhase -= kTwoPi;

            duckCur += (duckT - duckCur) * dcoef;
            bufL[(size_t) (w & mask)] = L[i] * duckCur;
            bufR[(size_t) (w & mask)] = R[i] * duckCur;

            float wl = catmullRead (bufL, mask, w, clampd (0.016 + lfoS, 0.0, 0.06) * fs);
            float wr = catmullRead (bufR, mask, w, clampd (0.021 - lfoS, 0.0, 0.06) * fs);
            ++w;
            wl = lp.processL (wl);
            wr = lp.processR (wr);
            L[i] = L[i] * dryG + wl * wetG;
            R[i] = R[i] * dryG + wr * wetG;
        }
        if (w > (1 << 30)) w -= (1 << 29);
    }

private:
    double fs = 48000;
    std::vector<float> bufL, bufR;
    int mask = 0, w = 0;
    Biquad lp;
    double lfoPhase = 0;
    Smooth mix, rate, depth;
    float duckT = 1, duckCur = 1;
};

//==============================================================================
// GATE — the stutter: a smoothstepped LFO gate.
class StutterGate : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override
    {
        fs = fsr;
        amount.init (getParam (0));
        rate.init (getParam (1));
        reset();
    }

    void reset() override { lfoPhase = 0; }

    void process (float* L, float* R, int n) override
    {
        amount.set (getParam (0));
        rate.set (getParam (1));
        const float amt = amount.tick (fs, n, 0.03f) / 100.0f;
        const float rateHz = rate.tick (fs, n, 0.05f) / 10.0f;
        const float base = 1.0f - amt;
        const double step = kTwoPi * rateHz / fs;

        for (int i = 0; i < n; ++i)
        {
            const double x = std::sin (lfoPhase);
            lfoPhase += step; if (lfoPhase > kTwoPi) lfoPhase -= kTwoPi;
            const double u = clampd ((x + 0.4) / 0.5, 0, 1);
            const float gate = base + amt * (float) (u * u * (3 - 2 * u));
            L[i] *= gate; R[i] *= gate;
        }
    }

private:
    double fs = 48000;
    double lfoPhase = 0;
    Smooth amount, rate;
};

//==============================================================================
// The registry. Type indices are the default chain order (Photo-Synth 2's:
// saturation, phaser, chorus, stutter, delay, reverb).
namespace
{
    const ParamDesc TUBE_PARAMS[] = {
        { "drive", "DRIVE", 8,   0,   24,  0, "dB", nullptr },
        { "tone",  "TONE",  72,  0,   100, 0, "%",  nullptr },
    };
    const ParamDesc PHASER_PARAMS[] = {
        { "mix",   "MIX",   35,  0,   100, 0, "%",   nullptr },
        { "rate",  "RATE",  40,  5,   200, 0, "cHz", nullptr },
        { "depth", "DEPTH", 55,  0,   100, 0, "%",   nullptr },
    };
    const ParamDesc CHORUS_PARAMS[] = {
        { "mix",   "MIX",   40,  0,   100, 0, "%",   nullptr },
        { "rate",  "RATE",  45,  5,   300, 0, "cHz", nullptr },
        { "depth", "DEPTH", 55,  0,   100, 0, "%",   nullptr },
    };
    const ParamDesc STUTTER_PARAMS[] = {
        { "amount", "AMOUNT", 50, 0,  100, 0, "%",   nullptr },
        { "rate",   "RATE",   80, 10, 160, 0, "dHz", nullptr },
    };
    const ParamDesc DELAY_PARAMS[] = {
        { "mix",       "MIX",       25,  0,    100, 0, "%",  nullptr },
        { "time",      "TIME",      260, 40,   900, 0, "ms", nullptr },
        { "feedback",  "FEEDBACK",  34,  0,    112, 0, "%",  nullptr },
        { "offset",    "OFFSET",    0,   -250, 250, 5, "ms", nullptr },
        { "character", "CHARACTER", 0,   0,    1,   1, "",   "CLEAN|TAPE" },
    };
    const ParamDesc REVERB_PARAMS[] = {
        { "mix",       "MIX",       25,  0,  100, 0, "%",  nullptr },
        { "character", "CHARACTER", 0,   0,  4,   1, "",   "ROOM|HALL|PLATE|SPRING|REVERSE" },
        { "length",    "LENGTH",    180, 20, 600, 0, "cs", nullptr },
    };

    const Descriptor DESCS[] = {
        { "saturation", "TUBE",     "asymmetric valve saturation", 1, TUBE_PARAMS,    2 },
        { "phaser",     "SWEEP",    "vintage 4-stage phaser",      1, PHASER_PARAMS,  3 },
        { "chorus",     "ENSEMBLE", "dual-line chorus",            1, CHORUS_PARAMS,  3 },
        { "stutter",    "GATE",     "rhythmic stutter gate",       1, STUTTER_PARAMS, 2 },
        { "delay",      "ECHO",     "stereo tape echo",            1, DELAY_PARAMS,   5 },
        { "reverb",     "SPACE",    "stereo convolution space",    1, REVERB_PARAMS,  3 },
    };
    constexpr int kNumTypes = (int) (sizeof (DESCS) / sizeof (DESCS[0]));
}

const Descriptor& TubeSat::desc() const     { return DESCS[0]; }
const Descriptor& PhaserFx::desc() const    { return DESCS[1]; }
const Descriptor& ChorusFx::desc() const    { return DESCS[2]; }
const Descriptor& StutterGate::desc() const { return DESCS[3]; }
const Descriptor& EchoDelay::desc() const   { return DESCS[4]; }
const Descriptor& ConvReverb::desc() const  { return DESCS[5]; }

int numModuleTypes() { return kNumTypes; }

const Descriptor& moduleDescriptor (int type)
{
    return DESCS[type < 0 || type >= kNumTypes ? 0 : type];
}

Module* createModule (int type)
{
    Module* m = nullptr;
    switch (type)
    {
        case 0: m = new TubeSat(); break;
        case 1: m = new PhaserFx(); break;
        case 2: m = new ChorusFx(); break;
        case 3: m = new StutterGate(); break;
        case 4: m = new EchoDelay(); break;
        case 5: m = new ConvReverb(); break;
        default: return nullptr;
    }
    const Descriptor& d = m->desc();
    for (int p = 0; p < d.numParams; ++p)
        m->setParam (p, d.params[p].def);
    return m;
}

std::string descriptorJson()
{
    std::string s = "[";
    for (int t = 0; t < kNumTypes; ++t)
    {
        const Descriptor& d = DESCS[t];
        if (t > 0) s += ",";
        s += "{\"id\":\"" + json::escape (d.id) + "\",\"name\":\"" + json::escape (d.name)
           + "\",\"sub\":\"" + json::escape (d.sub) + "\",\"ver\":" + std::to_string (d.version)
           + ",\"params\":[";
        for (int p = 0; p < d.numParams; ++p)
        {
            const ParamDesc& pd = d.params[p];
            if (p > 0) s += ",";
            char buf[256];
            std::snprintf (buf, sizeof (buf),
                "{\"id\":\"%s\",\"name\":\"%s\",\"def\":%g,\"lo\":%g,\"hi\":%g,\"step\":%g,\"unit\":\"%s\"",
                pd.id, pd.name, (double) pd.def, (double) pd.lo, (double) pd.hi, (double) pd.step, pd.unit);
            s += buf;
            if (pd.choices != nullptr)
                s += std::string (",\"choices\":\"") + json::escape (pd.choices) + "\"";
            s += "}";
        }
        s += "]}";
    }
    s += "]";
    return s;
}

} // namespace bwfx
