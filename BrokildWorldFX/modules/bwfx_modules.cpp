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
        agMsIn = agMsWet = 0.0; agGain = 1.0f;

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
        agMsIn = agMsWet = 0.0; agGain = 1.0f;
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
        /*  LINEAR blend, not the equal-power mixLaw: dry and wet here are the
            SAME waveform (one saturated), i.e. fully correlated - cos/sin
            weights sum to +3 dB at mid drive, which was the 'DRIVE raises
            the volume' report. Correlated signals crossfade at equal
            AMPLITUDE, so the level holds across the whole DRIVE range. */
        const float m = clampf (d / 24.0f, 0.0f, 1.0f);
        const float dryG = 1.0f - m;
        const float wetG = m * comp;
        double agAccIn = 0.0, agAccWet = 0.0;

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
            agAccIn  += (double) dl * dl + (double) dr * dr;
            agAccWet += (double) (wl * comp) * (wl * comp) + (double) (wr * comp) * (wr * comp);
            L[i] = dl * dryG + wl * wetG * agGain;
            R[i] = dr * dryG + wr * wetG * agGain;
        }

        // Measured auto-gain, the Mars Wars principle: a slow tracker of
        // dry-in vs (comp-scaled) wet-out RMS corrects what a static comp
        // cannot — saturation's level-dependence. ~400 ms, so no audible
        // pumping; frozen in silence; clamped so it can never scream.
        const double blockK = 1.0 - std::exp (-(double) n / (0.3 * fs));
        agMsIn  += (agAccIn  / (double) n - agMsIn)  * blockK;
        agMsWet += (agAccWet / (double) n - agMsWet) * blockK;
        if (agMsIn > 1.0e-8 && agMsWet > 1.0e-8)
        {
            const float target = clampf ((float) std::sqrt (agMsIn / agMsWet), 0.5f, 2.5f);
            agGain += (target - agGain) * (float) blockK;
        }
    }

private:
    static float toneHz (float v) { return 800.0f * std::pow (18000.0f / 800.0f, clampf (v, 0, 100) / 100.0f); }

    double fs = 48000;
    HalfBand upA_L, upB_L, upA_R, upB_R;
    Biquad satDC, toneBq;
    Smooth drive, tone;
    float comp = 1, lastCompDrive = -1;
    double agMsIn = 0.0, agMsWet = 0.0;
    float agGain = 1.0f;
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
        // 5 s of line: the free-run knob only reaches 900 ms, but a synced
        // 2/1 is 4 s at 120 BPM. Longer divisions at slow tempos clamp to
        // the line rather than wrapping.
        const int len = nextPow2 ((int) (fs * 5.0) + 8);
        bufL.assign ((size_t) len, 0.0f);
        bufR.assign ((size_t) len, 0.0f);
        mask = len - 1;
        maxSec = (float) ((double) (len - 4) / fs);
        mix.init (getParam (0));
        timeMs.init (getParam (1));
        fbAmt.init (getParam (2));
        offMs.init (getParam (3));
        lastSyncMs = 0.0f;
        reset();
    }

    void setTempo (double b) override { bpm = b; }

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
        fbAmt.set (getParam (2));
        offMs.set (getParam (3));
        const bool tape = getParam (4) > 0.5f;

        // SYNC: the division drives the same TIME smoother the knob does, so
        // there is no second code path. Engaging sync (or a big tempo jump)
        // SNAPS the smoother onto the new time — gliding into it lands the
        // first echo early, which is the Black Rider lesson.
        const double syncSec = syncSeconds ((int) std::lround (getParam (5)),
                                            (int) std::lround (getParam (6)), bpm);
        if (syncSec > 0.0)
        {
            const float ms = (float) clampd (syncSec * 1000.0, 20.0, (double) maxSec * 1000.0);
            if (lastSyncMs <= 0.0f || std::abs (ms - lastSyncMs) > lastSyncMs * 0.5f)
                timeMs.init (ms);
            timeMs.set (ms);
            lastSyncMs = ms;
        }
        else
        {
            if (lastSyncMs > 0.0f) timeMs.init (getParam (1));   // back to FREE: snap to the knob
            lastSyncMs = 0.0f;
            timeMs.set (getParam (1));
        }

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

            const float outL = catmullRead (bufL, mask, w, clampd ((double) t0 + wow, 0.0, (double) maxSec) * fs);
            const float outR = catmullRead (bufR, mask, w, clampd ((double) t1 + wow, 0.0, (double) maxSec) * fs);

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
    double bpm = 0;
    float maxSec = 1.5f, lastSyncMs = 0;
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

    void setTempo (double b) override { bpm = b; }

    void process (float* L, float* R, int n) override
    {
        amount.set (getParam (0));
        const float amt = amount.tick (fs, n, 0.03f) / 100.0f;

        // SYNC: the division IS the chop period, so it drives the same RATE
        // smoother the knob does (FREE / no host clock = the knob, exactly
        // as before).
        const double syncSec = syncSeconds ((int) std::lround (getParam (2)),
                                            (int) std::lround (getParam (3)), bpm);
        if (syncSec > 0.0)
        {
            const float dHz = (float) clampd (10.0 / syncSec, 1.0, 2000.0);   // param is v/10 Hz
            if (lastSyncRate <= 0.0f || std::abs (dHz - lastSyncRate) > lastSyncRate * 0.5f)
                rate.init (dHz);
            rate.set (dHz);
            lastSyncRate = dHz;
        }
        else
        {
            if (lastSyncRate > 0.0f) rate.init (getParam (1));
            lastSyncRate = 0.0f;
            rate.set (getParam (1));
        }
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
    double bpm = 0;
    float lastSyncRate = 0;
};

//==============================================================================
// GRIT — the lofi stage from Photo-Synth 2: sample-rate crush, bit crush,
// asymmetric dirt, hiss and crackle. Straight port of ps::Lofi. NOISE
// defaults to 0 so a fresh GRIT is still silent in / silent out.
class LofiGrit : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override { fs = fsr; reset(); }

    void reset() override
    {
        crush = noise = dirt = 0;
        holdL = holdR = holdAcc = 0;
        crEnv = crVal = 0;
        rng = 22222;
    }

    void process (float* L, float* R, int n) override
    {
        const double pCrush = getParam (0) / 100.0;
        const double pNoise = getParam (1) / 100.0;
        const double pDirt  = getParam (2) / 100.0;
        const double aS = 1.0 - std::exp (-1.0 / (0.02 * fs));

        for (int i = 0; i < n; ++i)
        {
            crush += (pCrush - crush) * aS;
            noise += (pNoise - noise) * aS;
            dirt  += (pDirt  - dirt)  * aS;
            const double xl = L[i], xr = R[i];

            const double hold = 1.0 + crush * crush * 38.0;
            holdAcc += 1.0;
            if (holdAcc >= hold) { holdAcc -= hold; holdL = xl; holdR = xr; }
            const double wet = std::min (1.0, crush * 3.0);
            double cl = xl + (holdL - xl) * wet;
            double cr = xr + (holdR - xr) * wet;

            if (crush > 0.01)
            {
                const double q = std::pow (2.0, 12.0 - crush * 8.0);
                cl = std::round (cl * q) / q;
                cr = std::round (cr * q) / q;
            }
            if (dirt > 0.01)
            {
                const double dk = 1.0 + dirt * 5.0, dn = 1.0 / std::tanh (dk);
                cl += (std::tanh (cl * dk) * dn - cl) * dirt;
                cr += (std::tanh (cr * dk) * dn - cr) * dirt;
            }
            if (noise > 0.002)
            {
                const double hiss = (rand01() * 2.0 - 1.0) * noise * 0.012;
                if (rand01() < noise * 0.0006) { crEnv = 1.0; crVal = rand01() * 2.0 - 1.0; }
                crEnv *= 0.994;
                const double bed = hiss + crVal * crEnv * noise * 0.35;
                cl += bed; cr += bed;
            }
            L[i] = (float) cl; R[i] = (float) cr;
        }
    }

private:
    double rand01() { rng = 1664525u * rng + 1013904223u; return rng / 4294967296.0; }
    double fs = 48000;
    double crush = 0, noise = 0, dirt = 0;
    double holdL = 0, holdR = 0, holdAcc = 0, crEnv = 0, crVal = 0;
    uint32_t rng = 22222;
};

//==============================================================================
// STRIP — the channel strip: Hairfryer's compressor (soft knee, two-stage
// gain smoothing — a fast stage catches, a slow one breathes, which is what
// reads as analogue) followed by a five-band musical EQ. One AMOUNT macro
// opens both threshold and ratio, the studio move.
class Strip : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override
    {
        fs = fsr;
        for (auto& g : lastGain) g = -999.0f;
        reset();
    }

    void reset() override
    {
        det = 0; g1 = 0; g2 = 0; autoMk = 0;
        for (auto& b : eq) b.reset();
    }

    void process (float* L, float* R, int n) override
    {
        const float amount = clampf (getParam (0) / 100.0f, 0.0f, 1.0f);
        const float thrDb = -2.0f - 30.0f * amount;
        const float ratio = 1.5f + 5.5f * amount;
        constexpr float knee = 6.0f;
        const float aAtt = 1.0f - std::exp ((float) (-1.0 / (0.001 * std::max (0.5, (double) getParam (1)) * fs)));
        const float aRel = 1.0f - std::exp ((float) (-1.0 / (0.001 * std::max (5.0, (double) getParam (2)) * fs)));
        const float aDet = 1.0f - std::exp ((float) (-1.0 / (0.002 * fs)));

        static const double freq[5] = { 80.0, 250.0, 1000.0, 3500.0, 10000.0 };
        static const Biquad::Type kind[5] = { Biquad::lowShelf, Biquad::peaking,
                                              Biquad::peaking, Biquad::peaking, Biquad::highShelf };
        for (int b = 0; b < 5; ++b)
        {
            const float g = getParam (3 + b);
            if (std::abs (g - lastGain[(size_t) b]) > 1e-4f)
            {
                eq[(size_t) b].set (kind[b], freq[b], 0.9, g, fs);
                lastGain[(size_t) b] = g;
            }
        }
        const float outG = std::pow (10.0f, getParam (8) / 20.0f);

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];

            const float mag = std::max (std::abs (l), std::abs (r));
            det += (mag - det) * aDet;
            const float lvl = 20.0f * std::log10 (det + 1.0e-6f);
            float grDb = 0.0f;
            const float over = lvl - thrDb;
            if (over > -knee * 0.5f)
            {
                if (over < knee * 0.5f)
                {
                    const float t = over + knee * 0.5f;
                    grDb = (1.0f / ratio - 1.0f) * t * t / (2.0f * knee);
                }
                else grDb = (1.0f / ratio - 1.0f) * over;
            }
            g1 += (grDb - g1) * (grDb < g1 ? aAtt : aRel);
            g2 += (g1 - g2) * 0.12f;
            autoMk += (-g2 * 0.8f - autoMk) * 0.0005f;
            const float cg = std::pow (10.0f, (g2 + autoMk) / 20.0f);
            l *= cg; r *= cg;

            for (int b = 0; b < 5; ++b) { l = eq[(size_t) b].processL (l); r = eq[(size_t) b].processR (r); }

            L[i] = l * outG; R[i] = r * outG;
        }
    }

private:
    double fs = 48000;
    float det = 0, g1 = 0, g2 = 0, autoMk = 0;
    std::array<Biquad, 5> eq;
    std::array<float, 5> lastGain { -999.0f, -999.0f, -999.0f, -999.0f, -999.0f };
};

//==============================================================================
// SHIMMER — Blade Ruiner's eight-line FDN behind four diffusing allpasses,
// with an octave-up copy of the tail folded back INTO the loop. That is the
// reverb a convolver cannot be: the sheen is generated by the feedback, not
// stored in an impulse. Ported with its runaway lesson — a soft ceiling sits
// in the loop unconditionally.
namespace
{
    struct RvDelay
    {
        std::vector<float> buf;
        int w = 0, mask = 0;
        void init (int nn)
        {
            int p = 8; while (p < nn) p <<= 1;
            buf.assign ((size_t) p, 0.0f); mask = p - 1; w = 0;
        }
        void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
        float read (float d) const
        {
            const float len = (float) (mask + 1);
            float rp = (float) w - std::min (std::max (d, 1.0f), len - 2.0f);
            while (rp < 0.0f) rp += len;
            const int i0 = (int) rp;
            const float f = rp - (float) i0;
            return buf[(size_t) (i0 & mask)] * (1.0f - f) + buf[(size_t) ((i0 + 1) & mask)] * f;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    };

    struct RvOnePole
    {
        float z = 0, a = 0.1f;
        void setHz (float hz, double sr) { a = 1.0f - std::exp (-6.2831853f * clampf (hz, 1.0f, (float) sr * 0.45f) / (float) sr); }
        float lp (float x) { z += (x - z) * a; return z; }
        float hp (float x) { return x - lp (x); }
        void clear() { z = 0; }
    };

    // Reads its own line at twice the rate through two crossfaded windows:
    // an octave up with a soft seam rather than a chirp.
    struct RvOctaveUp
    {
        RvDelay d;
        float ph = 0, win = 2400.0f;
        void prepare (double sr)
        {
            win = (float) sr * 0.05f;
            d.init ((int) win * 2 + 8);
            ph = 0;
        }
        float tick (float x)
        {
            d.push (x);
            ph += 1.0f;
            if (ph >= win) ph -= win;
            float p2 = ph + win * 0.5f;
            if (p2 >= win) p2 -= win;
            const float w1 = 0.5f - 0.5f * std::cos (6.2831853f * ph / win);
            const float w2 = 0.5f - 0.5f * std::cos (6.2831853f * p2 / win);
            return d.read (win - ph) * w1 + d.read (win - p2) * w2;
        }
        void clear() { d.clear(); ph = 0; }
    };

    // transparent below 0.7, asymptotic above — the feedback never runs away
    inline float ceilSoft (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.7f) return x;
        const float y = 0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f);
        return x < 0 ? -y : y;
    }
}

class Shimmer : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override
    {
        fs = fsr;
        srScale = (float) fs / 44100.0f;
        for (int i = 0; i < N; ++i)
        {
            len[i] = baseLen[i] * srScale;
            line[i].init ((int) (len[i] * 3.0f) + 64);
        }
        for (int i = 0; i < 4; ++i) ap[i].init ((int) (apLen[i] * srScale) + 64);
        oct.prepare (fs);
        shHp.setHz (300.0f, fs);
        mix.init (getParam (0));
        reset();
    }

    void reset() override
    {
        for (auto& l : line) l.clear();
        for (auto& a : ap) a.clear();
        for (auto& d : damp) d.clear();
        oct.clear();
        shHp.clear();
        duckCur = 0;
    }

    void inputDuck (float g) override { duckT = g; }

    void process (float* L, float* R, int n) override
    {
        mix.set (getParam (0));
        float dryG, wetG;
        mixLaw (mix.tick (fs, n, 0.05f) / 100.0f, dryG, wetG);

        const float size01  = clampf (getParam (1) / 100.0f, 0.0f, 1.0f);
        const float decay01 = clampf (getParam (2) / 100.0f, 0.0f, 1.0f);
        const float shAmt   = clampf (getParam (3) / 100.0f, 0.0f, 1.0f);
        const float toneHz  = 1500.0f * std::pow (8.0f, clampf (getParam (4) / 100.0f, 0.0f, 1.0f));

        const float sc = (0.55f + 1.30f * size01) * srScale;
        for (int i = 0; i < N; ++i)
        {
            len[i] = baseLen[i] * sc;
            damp[i].setHz (toneHz, fs);
        }
        const float fb = 0.62f + 0.325f * decay01;
        const float dcoef = 1.0f - std::exp ((float) (-1.0 / (0.003 * fs)));

        for (int i = 0; i < n; ++i)
        {
            duckCur += (duckT - duckCur) * dcoef;
            const float in = (L[i] + R[i]) * 0.5f * duckCur;

            float x = in;
            for (int a = 0; a < 4; ++a)
            {
                const float dl = ap[a].read (apLen[a] * srScale);
                const float v  = x - 0.62f * dl;
                ap[a].push (v);
                x = dl + 0.62f * v;
            }

            float y[N];
            for (int k = 0; k < N; ++k) y[k] = damp[k].lp (line[k].read (len[k]));

            // Hadamard-8, three butterfly stages
            float t[N];
            for (int k = 0; k < 4; ++k) { t[k] = y[k] + y[k + 4]; t[k + 4] = y[k] - y[k + 4]; }
            for (int b = 0; b < 8; b += 4)
                for (int k = 0; k < 2; ++k)
                { const float a1 = t[b + k], c = t[b + k + 2]; t[b + k] = a1 + c; t[b + k + 2] = a1 - c; }
            for (int b = 0; b < 8; b += 2)
            { const float a1 = t[b], c = t[b + 1]; t[b] = a1 + c; t[b + 1] = a1 - c; }
            for (int k = 0; k < N; ++k) t[k] *= 0.35355339f;      // 1/sqrt(8)

            float sh = 0.0f;
            if (shAmt > 0.001f)
                sh = shHp.hp (oct.tick ((y[0] + y[3]) * 0.5f)) * shAmt;

            for (int k = 0; k < N; ++k)
                line[k].push (ceilSoft (x + sh + fb * t[k]));

            const float wl = (y[0] + y[2] + y[4] + y[6]) * 0.34f;
            const float wr = (y[1] + y[3] + y[5] + y[7]) * 0.34f;
            L[i] = L[i] * dryG + wl * wetG;
            R[i] = R[i] * dryG + wr * wetG;
        }
    }

private:
    static constexpr int N = 8;
    double fs = 48000;
    float srScale = 1.0f;
    RvDelay line[N], ap[4];
    RvOnePole damp[N], shHp;
    RvOctaveUp oct;
    float apLen[4] { 142.0f, 379.0f, 107.0f, 277.0f };
    float baseLen[N] { 1116.0f, 1188.0f, 1277.0f, 1356.0f, 1422.0f, 1491.0f, 1557.0f, 1617.0f };
    float len[N] {};
    Smooth mix;
    float duckT = 1, duckCur = 1;
};

//==============================================================================
// HARMONIC — one modulation pedal, three modes. HARMONIC is the brownface
// trick: split the band at 800 Hz and tremolo the halves in OPPOSITE phase,
// which is where the swirl lives (a plain tremolo cannot do it). TREM is the
// straight amplitude wobble; VIBRATO is true pitch through a modulated line.
class HarmTrem : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override
    {
        fs = fsr;
        const int len = nextPow2 ((int) (fs * 0.05) + 8);
        vbL.assign ((size_t) len, 0.0f);
        vbR.assign ((size_t) len, 0.0f);
        mask = len - 1;
        split.set (Biquad::lowpass, 800.0, 0.0, 0, fs);
        rate.init (getParam (1));
        depth.init (getParam (2));
        mix.init (getParam (5));
        lastSyncRate = 0;
        reset();
    }

    void reset() override
    {
        std::fill (vbL.begin(), vbL.end(), 0.0f);
        std::fill (vbR.begin(), vbR.end(), 0.0f);
        w = 0;
        split.reset();
        lfoPhase = 0;
    }

    void setTempo (double b) override { bpm = b; }

    void process (float* L, float* R, int n) override
    {
        const int mode = (int) std::lround (getParam (0));
        depth.set (getParam (2));
        mix.set (getParam (5));

        // SYNC: the division is one full LFO cycle.
        const double syncSec = syncSeconds ((int) std::lround (getParam (3)),
                                            (int) std::lround (getParam (4)), bpm);
        if (syncSec > 0.0)
        {
            const float cHz = (float) clampd (100.0 / syncSec, 5.0, 2000.0);   // param is v/100 Hz
            if (lastSyncRate <= 0.0f || std::abs (cHz - lastSyncRate) > lastSyncRate * 0.5f)
                rate.init (cHz);
            rate.set (cHz);
            lastSyncRate = cHz;
        }
        else
        {
            if (lastSyncRate > 0.0f) rate.init (getParam (1));
            lastSyncRate = 0.0f;
            rate.set (getParam (1));
        }

        const float rateHz = rate.tick (fs, n, 0.05f) / 100.0f;
        const float d = clampf (depth.tick (fs, n, 0.05f) / 100.0f, 0.0f, 1.0f);
        float dryG, wetG;
        mixLaw (mix.tick (fs, n, 0.05f) / 100.0f, dryG, wetG);
        const double step = kTwoPi * rateHz / fs;
        const double vibMax = 0.004 * fs;          // +-4 ms of pitch wobble

        for (int i = 0; i < n; ++i)
        {
            const float s = (float) std::sin (lfoPhase);
            lfoPhase += step; if (lfoPhase > kTwoPi) lfoPhase -= kTwoPi;

            float wl, wr;
            if (mode == 2)                          // VIBRATO
            {
                vbL[(size_t) (w & mask)] = L[i];
                vbR[(size_t) (w & mask)] = R[i];
                const double dl = vibMax * (1.0 + (double) d * s) + 4.0;
                wl = catmullRead (vbL, mask, w, dl);
                wr = catmullRead (vbR, mask, w, dl);
                ++w;
            }
            else if (mode == 1)                     // TREM
            {
                const float g = 1.0f - d * 0.5f * (1.0f - s);
                wl = L[i] * g; wr = R[i] * g;
            }
            else                                    // HARMONIC
            {
                const float gLo = 1.0f - d * 0.5f * (1.0f - s);
                const float gHi = 1.0f - d * 0.5f * (1.0f + s);   // opposite phase
                const float loL = split.processL (L[i]);
                const float loR = split.processR (R[i]);
                wl = loL * gLo + (L[i] - loL) * gHi;
                wr = loR * gLo + (R[i] - loR) * gHi;
            }
            L[i] = L[i] * dryG + wl * wetG;
            R[i] = R[i] * dryG + wr * wetG;
        }
        if (w > (1 << 30)) w -= (1 << 29);
    }

private:
    double fs = 48000;
    std::vector<float> vbL, vbR;
    int mask = 0, w = 0;
    Biquad split;
    double lfoPhase = 0, bpm = 0;
    Smooth rate, depth, mix;
    float lastSyncRate = 0;
};

//==============================================================================
// ROTARY — a Leslie. Mono in (a rotary cabinet has one throat), GROWL drive,
// an 800 Hz LR4 split, then two independent rotors: the HORN carries the top,
// the DRUM the bottom, each with its own doppler (a modulated tap — distance
// to the mic varies as the source circles) and amplitude throw, picked up by
// two virtual mics 90 degrees apart. The point of the whole thing is the
// INERTIA: the horn spins up in about a second, the heavy drum takes four —
// that is what a SLOW/FAST flip sounds like on the real machine, and the
// bench measures both ramp times rather than trusting anyone's ear.
class Rotary : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override
    {
        fs = fsr;
        const int len = nextPow2 ((int) (fs * 0.01) + 16);   // ~10 ms of throw
        hornBuf.assign ((size_t) len, 0.0f);
        drumBuf.assign ((size_t) len, 0.0f);
        mask = len - 1;
        // The real 122's passive crossover is only 12 dB/oct — the mid-band
        // overlap where BOTH rotors carry ~800 Hz (and beat against each
        // other) is part of the famous sound. A steeper split is cleaner and
        // wrong.
        lo1.set (Biquad::lowpass, 800.0, 20.0 * std::log10 (0.7071), 0, fs);
        hi1.set (Biquad::highpass, 800.0, 20.0 * std::log10 (0.7071), 0, fs);
        // the horn's own voice: a mild presence formant before it spins
        hornEq.set (Biquad::peaking, 2500.0, 0.8, 2.0, fs);
        mix.init (getParam (1));
        reset();
    }

    void reset() override
    {
        std::fill (hornBuf.begin(), hornBuf.end(), 0.0f);
        std::fill (drumBuf.begin(), drumBuf.end(), 0.0f);
        w = 0;
        for (auto* b : { &lo1, &hi1, &hornEq }) b->reset();
        hornPh = 0.0;
        drumPh = 0.31;                        // the rotors never start aligned
        wobPh = 0.0;
        const int sp = (int) std::lround (getParam (0));
        hornRate = sp == 1 ? kHornFast : (sp == 2 ? 0.0 : kHornSlow);
        drumRate = sp == 1 ? kDrumFast : (sp == 2 ? 0.0 : kDrumSlow);
    }

    void process (float* L, float* R, int n) override
    {
        const int sp = (int) std::lround (getParam (0));     // SLOW|FAST|BRAKE
        mix.set (getParam (1));
        const float m = clampf (mix.tick (fs, n, 0.05f) / 100.0f, 0.0f, 1.0f);
        const float bal = clampf (getParam (2) / 50.0f, -1.0f, 1.0f);
        const float hornG = bal <= 0 ? 1.0f : 1.0f - 0.6f * bal;
        const float drumG = bal >= 0 ? 1.0f : 1.0f + 0.6f * bal;
        const float growl = clampf (getParam (3) / 100.0f, 0.0f, 1.0f);
        const float drvK = 1.0f + growl * 4.0f;
        const float drvN = 1.0f / drvK;                      // unity small-signal

        const double hornTarget = sp == 1 ? kHornFast : (sp == 2 ? 0.0 : kHornSlow);
        const double drumTarget = sp == 1 ? kDrumFast : (sp == 2 ? 0.0 : kDrumSlow);
        // per-rotor inertia: rising uses the spin-up time, falling the coast
        const double hornTau = hornTarget > hornRate ? 0.9 : 1.1;
        const double drumTau = drumTarget > drumRate ? 3.5 : 4.5;
        const double hornCoef = 1.0 - std::exp (-(double) n / (hornTau * fs));
        const double drumCoef = 1.0 - std::exp (-(double) n / (drumTau * fs));
        hornRate += (hornTarget - hornRate) * hornCoef;
        drumRate += (drumTarget - drumRate) * drumCoef;

        // belt wobble: real rotors are not clockwork — a slow +-0.8% sway
        // (deterministic, so renders stay bit-repeatable)
        wobPh += kTwoPi * 0.11 * n / fs; if (wobPh > kTwoPi) wobPh -= kTwoPi;
        const double wob = 1.0 + 0.008 * std::sin (wobPh);

        const double hornStep = kTwoPi * hornRate * wob / fs;
        const double drumStep = -kTwoPi * drumRate / fs;     // COUNTER-rotation,
                                                             // like the real unit
        // doppler throw: horn ~+-1.35% pitch at fast, drum ~+-0.7% — the
        // physically plausible band for the mouth radius and baffle scoop
        const double hornDep = 0.00032 * fs;
        const double drumDep = 0.00018 * fs;
        const double hornBase = hornDep + 6.0;
        const double drumBase = drumDep + 6.0;
        constexpr double micL = 0.7853981634;                // mics at +-45 degrees
        constexpr double micR = -0.7853981634;

        for (int i = 0; i < n; ++i)
        {
            // mono throat, growl drive
            float x = (L[i] + R[i]) * 0.5f;
            if (growl > 0.001f) x = std::tanh (x * drvK) * drvN * (1.0f + growl * 0.7f);

            const float lo = lo1.processL (x);
            const float hi = hornEq.processL (hi1.processL (x));
            hornBuf[(size_t) (w & mask)] = hi;
            drumBuf[(size_t) (w & mask)] = lo;

            hornPh += hornStep; if (hornPh > kTwoPi) hornPh -= kTwoPi;
            drumPh += drumStep; if (drumPh < 0.0) drumPh += kTwoPi;

            // The horn: a directional beam. Front lobe sharpened (a horn's
            // throw is more peaked than a bare cosine), plus a weak back-lobe
            // reflection off the cabinet — that is what keeps the level from
            // holing out when the horn points away.
            auto horn = [&] (double mic) -> float
            {
                const double c = std::cos (hornPh - mic);
                const double beam = 0.5 + 0.5 * c;
                const float g  = 0.40f + 0.60f * (float) (beam * std::sqrt (beam));
                const float d  = catmullRead (hornBuf, mask, w, hornBase + hornDep * (1.0 - c));
                const float rb = catmullRead (hornBuf, mask, w, hornBase + hornDep * (1.0 + c) + 11.0);
                return d * g + rb * 0.22f * (1.0f - g);
            };
            // The drum: a baffle behind a wooden scoop — broad, gentle throw.
            auto drum = [&] (double mic) -> float
            {
                const double c = std::cos (drumPh - mic);
                const float g = 1.0f - 0.25f * 0.5f * (1.0f - (float) c);
                return catmullRead (drumBuf, mask, w, drumBase + drumDep * (1.0 - c)) * g;
            };
            const float hL = horn (micL), hR = horn (micR);
            const float dL = drum (micL), dR = drum (micR);
            ++w;

            const float wl = hL * hornG + dL * drumG;
            const float wr = hR * hornG + dR * drumG;
            L[i] = L[i] * (1.0f - m) + wl * m;
            R[i] = R[i] * (1.0f - m) + wr * m;
        }
        if (w > (1 << 30)) w -= (1 << 29);
    }

private:
    static constexpr double kHornSlow = 0.83, kHornFast = 6.8;
    static constexpr double kDrumSlow = 0.70, kDrumFast = 5.9;
    double fs = 48000;
    std::vector<float> hornBuf, drumBuf;
    int mask = 0, w = 0;
    Biquad lo1, hi1, hornEq;
    double hornPh = 0, drumPh = 0.31, hornRate = kHornSlow, drumRate = kDrumSlow;
    double wobPh = 0;
    Smooth mix;
};

//==============================================================================
// KIERANATOR — step-sequenced effect switching, named in honour of the late
// Kieran Foster (dblue), whose Glitch 2 inspired it. Our own build
// build. A rolling capture buffer holds the last bars; a 16-step pattern
// (drawn on the pedal, stored as the module's opaque extra state) chooses per
// step how that buffer is READ: nothing, retrigger, tape-stop, reverse,
// shuffle, repitch, crush, gate. Everything is time-domain buffer reads —
// glitchy by design, trivially light on CPU. 5 ms crossfades at every step
// boundary keep it rhythmic rather than clicky. Bar-locked to the host
// transport when it rolls; free-runs on an internal clock otherwise, so it
// stays deterministic and playable without a DAW.
class Disruptor : public Module
{
public:
    const Descriptor& desc() const override;

    void prepare (double fsr, int) override
    {
        fs = fsr;
        const int len = nextPow2 ((int) (fs * 10.0) + 8);   // 2 bars at 48 BPM
        bufL.assign ((size_t) len, 0.0f);
        bufR.assign ((size_t) len, 0.0f);
        mask = len - 1;
        mix.init (getParam (1));
        reset();
    }

    void reset() override
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        w = 0;
        beats = 0.0;
        lastStep = -1;
        rng = 0xD15Bu;
        cur = Voice();
        old = Voice();
        fade = 1.0f;
    }

    void setTempo (double b) override { bpm = b; }
    void setClock (double ppq, bool playing) override { hostPpq = ppq; hostRolls = playing; }

    /*  The pattern: up to 32 characters '0'..'8', one per step, where 8 is
        the RANDOM brush. 32 x 4 bits does not fit a uint64 and the pattern
        is read once per SAMPLE, so it lives in two slots with an atomic
        index rather than in a packed word — the writer fills the spare slot
        and publishes it, and the audio thread never sees a half-written
        pattern. */
    void setExtra (const std::string& x) override
    {
        const int idx = patIdx.load (std::memory_order_relaxed);
        auto& dst = pat[(size_t) (idx ^ 1)];
        for (int i = 0; i < kSteps; ++i)
        {
            const int c = i < (int) x.size() ? x[(size_t) i] - '0' : 0;
            dst[(size_t) i] = (uint8_t) (c >= 0 && c <= 8 ? c : 0);
        }
        patIdx.store (idx ^ 1, std::memory_order_release);
    }
    /*  SIXTEEN characters whenever the second page is empty — the identical
        string the module returned before page B existed, not merely one
        that sounds the same. Otherwise every rack blob in every saved
        project would change the day this shipped. */
    std::string getExtra() const override
    {
        const auto& p = pat[(size_t) patIdx.load (std::memory_order_acquire)];
        int len = 16;
        for (int i = 16; i < kSteps; ++i) if (p[(size_t) i] != 0) { len = kSteps; break; }
        std::string s ((size_t) len, '0');
        bool any = false;
        for (int i = 0; i < len; ++i)
        {
            s[(size_t) i] = (char) ('0' + p[(size_t) i]);
            if (p[(size_t) i] != 0) any = true;
        }
        return any ? s : std::string();                       // default = empty blob
    }

    void process (float* L, float* R, int n) override
    {
        mix.set (getParam (1));
        const float m = clampf (mix.tick (fs, n, 0.05f) / 100.0f, 0.0f, 1.0f);
        const double useBpm = bpm > 0.0 ? bpm : 120.0;
        const int bars = (int) std::lround (getParam (0)) == 1 ? 2 : 1;
        const double stepBeats = bars * 4.0 / 16.0;
        const double stepLen = stepBeats * 60.0 / useBpm * fs;      // samples

        const double repFrac[3] = { 0.5, 0.25, 0.125 };
        const double repLen = std::max (64.0, stepLen * repFrac[(int) std::lround (getParam (2)) & 3]);
        const float decay = clampf (getParam (3) / 100.0f, 0.0f, 1.0f);
        const float stopDepth = clampf (getParam (4) / 100.0f, 0.0f, 1.0f);
        static const double ratios[4] = { 0.5, 2.0 / 3.0, 1.5, 2.0 };
        const double ratio = ratios[(int) std::lround (getParam (5)) & 3];
        const float crushAmt = clampf (getParam (6) / 100.0f, 0.0f, 1.0f);
        const float duty = clampf (getParam (7) / 100.0f, 0.05f, 1.0f);
        const int   last = (int) std::lround (getParam (8));
        const int   nSteps = last < 2 ? 2 : (last > kSteps ? kSteps : last);
        const float chaos = clampf (getParam (9) / 100.0f, 0.0f, 1.0f);
        const auto& pgm = pat[(size_t) patIdx.load (std::memory_order_acquire)];
        const double beatsPerSample = useBpm / (60.0 * fs);
        const float fadeStep = 1.0f / (0.005f * (float) fs);        // 5 ms

        for (int i = 0; i < n; ++i)
        {
            // clock: bar-locked to the host when it rolls, else internal
            double b;
            if (hostRolls && hostPpq >= 0.0)
            {
                b = hostPpq + (double) i * beatsPerSample;
                beats = b;                                          // stay continuous on stop
            }
            else
            {
                beats += beatsPerSample;
                b = beats;
            }
            const double stepPos = b / stepBeats;
            /*  A host can hand out a negative ppq during a count-in, and a
                negative index would read outside the pattern. */
            const int64_t rawStep = (int64_t) std::floor (stepPos);
            int64_t cycle = rawStep / nSteps;
            int step = (int) (rawStep - cycle * nSteps);
            if (step < 0) { step += nSteps; --cycle; }
            const double phase = stepPos - std::floor (stepPos);

            // capture never stops
            bufL[(size_t) (w & mask)] = L[i];
            bufR[(size_t) (w & mask)] = R[i];

            if (step != lastStep)
            {
                lastStep = step;
                old = cur;
                fade = 0.0f;
                cur.type = pgm[(size_t) step];
                cur.start = w;
                cur.t = 0;
                cur.jRep = 1.0; cur.jRatio = 1.0;
                cur.jDecay = 1.0f; cur.jStop = 1.0f; cur.jCrush = 1.0f; cur.jDuty = 1.0f;

                /*  RANDOM resolves to one of the SEVEN real effects, never
                    to NONE — a brush that sometimes does nothing reads as a
                    dropout rather than as a choice. Seeded from the loop
                    cycle and the step, so the bar is reproducible and a
                    re-render gives the same audio. */
                if (cur.type == 8)
                    cur.type = 1 + (int) (stepHash (cycle, step, 0x5EEDu) % 7u);

                if (chaos > 0.0f)
                {
                    //  a step that is OFF fires anyway, more often as the
                    //  knob comes up. This is the half that makes CHAOS feel
                    //  alive rather than merely loose.
                    if (cur.type == 0)
                    {
                        const uint32_t h = stepHash (cycle, step, 0xC0FFEEu);
                        const uint32_t thresh = (uint32_t) (chaos * chaos * 0.45f * 65536.0f);
                        if ((h & 0xFFFFu) < thresh) cur.type = 1 + (int) ((h >> 16) % 7u);
                    }
                    //  ...and whatever fires is knocked off its knob setting
                    //  by up to +/- half the CHAOS reading. Applied AT the
                    //  step, so it is a different glitch each time rather
                    //  than a wobble across the bar.
                    const float k = chaos * 0.5f;
                    cur.jDecay = jitter (cycle, step, 0x11u, k);
                    cur.jStop  = jitter (cycle, step, 0x22u, k);
                    cur.jCrush = jitter (cycle, step, 0x33u, k);
                    cur.jDuty  = jitter (cycle, step, 0x44u, k);
                    cur.jRep   = (double) jitter (cycle, step, 0x55u, k);
                    cur.jRatio = (double) jitter (cycle, step, 0x66u, k * 0.5f);
                }
                if (cur.type == 4)                                 // SHUFFLE
                {
                    rng = 1664525u * rng + 1013904223u;
                    cur.shuffleBack = (1.0 + (double) (rng % 15u)) * stepLen;
                }
            }

            float fl, fr, ol, orr;
            render (cur, L[i], R[i], stepLen, repLen, decay, stopDepth, ratio,
                    crushAmt, duty, phase, fl, fr);
            if (fade < 1.0f)
            {
                render (old, L[i], R[i], stepLen, repLen, decay, stopDepth, ratio,
                        crushAmt, duty, 1.0, ol, orr);
                const float g = 0.5f - 0.5f * std::cos (3.14159265f * fade);   // equal-ish power
                fl = ol + (fl - ol) * g;
                fr = orr + (fr - orr) * g;
                fade = std::min (1.0f, fade + fadeStep);
            }
            ++cur.t;
            ++old.t;
            ++w;

            L[i] = L[i] * (1.0f - m) + fl * m;
            R[i] = R[i] * (1.0f - m) + fr * m;
        }
        if (w > (1 << 30)) { w -= (1 << 29); cur.start -= (1 << 29); old.start -= (1 << 29); }
    }

private:
    struct Voice
    {
        int type = 0;            // 0 NONE 1 RETRIG 2 TAPESTOP 3 REVERSE
                                 // 4 SHUFFLE 5 PITCH 6 CRUSH 7 GATE
        int start = 0;           // write position at the step boundary
        int64_t t = 0;           // samples into the step
        double shuffleBack = 0;
        //  CHAOS: per-step multipliers on this effect's own settings. All
        //  exactly 1 when the knob is shut, and x1.0f is IEEE-exact, so a
        //  rack at CHAOS 0 renders the identical samples it always did.
        double jRep = 1.0, jRatio = 1.0;
        float jDecay = 1.0f, jStop = 1.0f, jCrush = 1.0f, jDuty = 1.0f;
        // crush state
        float holdL = 0, holdR = 0;
        double holdAcc = 0;
    };

    void render (Voice& v, float inL, float inR, double stepLen, double repLen,
                 float decay, float stopDepth, double ratio, float crushAmt,
                 float duty, double phase, float& outL, float& outR)
    {
        switch (v.type)
        {
            default:
            case 0:                                                // NONE: dry
                outL = inL; outR = inR;
                return;
            case 1:                                                // RETRIG
            {
                const double rl = std::max (64.0, repLen * v.jRep);
                const int64_t k = v.t / (int64_t) rl;
                const double pos = (double) (v.t % (int64_t) rl);
                const float g = std::pow (clampf (decay * v.jDecay, 0.0f, 1.0f), (float) k);
                outL = catmullRead (bufL, mask, w, (double) (w - v.start) - pos + 2.0) * g;
                outR = catmullRead (bufR, mask, w, (double) (w - v.start) - pos + 2.0) * g;
                return;
            }
            case 2:                                                // TAPESTOP
            {
                const double u = std::min (1.0, (double) v.t / std::max (1.0, stepLen));
                const double sd = (double) clampf (stopDepth * v.jStop, 0.0f, 1.0f);
                const double rate = std::max (0.0, 1.0 - sd * std::pow (u, 1.4));
                // integrated read position (closed form of the power ramp)
                const double travel = (double) v.t - sd * stepLen * std::pow (u, 2.4) / 2.4;
                const double back = (double) (w - v.start) - travel + 2.0;
                const float g = 0.25f + 0.75f * (float) std::sqrt (rate);   // dies as it stops
                outL = catmullRead (bufL, mask, w, back) * g;
                outR = catmullRead (bufR, mask, w, back) * g;
                return;
            }
            case 3:                                                // REVERSE
            {
                const double back = (double) (w - v.start) + (double) v.t + 2.0;
                outL = catmullRead (bufL, mask, w, back);
                outR = catmullRead (bufR, mask, w, back);
                return;
            }
            case 4:                                                // SHUFFLE
            {
                const double pos = (double) (v.t % (int64_t) std::max (64.0, stepLen));
                const double back = (double) (w - v.start) + v.shuffleBack - pos + 2.0;
                outL = catmullRead (bufL, mask, w, back);
                outR = catmullRead (bufR, mask, w, back);
                return;
            }
            case 5:                                                // PITCH
            {
                // loop the PREVIOUS step's slice at the ratio: the read head
                // never has to cross the write head, whatever the interval
                const double span = std::max (64.0, stepLen);
                const double pos = std::fmod ((double) v.t * ratio * v.jRatio, span);
                const double back = (double) (w - v.start) + span - pos + 2.0;
                outL = catmullRead (bufL, mask, w, back);
                outR = catmullRead (bufR, mask, w, back);
                return;
            }
            case 6:                                                // CRUSH
            {
                const float ca = clampf (crushAmt * v.jCrush, 0.0f, 1.0f);
                const double hold = 1.0 + ca * ca * 60.0;
                v.holdAcc += 1.0;
                if (v.holdAcc >= hold) { v.holdAcc -= hold; v.holdL = inL; v.holdR = inR; }
                const double q = std::pow (2.0, 11.0 - ca * 8.0);
                outL = (float) (std::round (v.holdL * q) / q);
                outR = (float) (std::round (v.holdR * q) / q);
                return;
            }
            case 7:                                                // GATE
            {
                // open for `duty` of the step, 1% edges so the chop is a
                // chop, not a click
                const double edge = 0.01;
                const double dy = (double) clampf (duty * v.jDuty, 0.05f, 1.0f);
                float g = 0.0f;
                if (phase < dy)
                {
                    g = 1.0f;
                    if (phase < edge)             g = (float) (phase / edge);
                    else if (phase > dy - edge)   g = (float) ((dy - phase) / edge);
                }
                outL = inL * g; outR = inR * g;
                return;
            }
        }
    }

    double fs = 48000;
    std::vector<float> bufL, bufR;
    int mask = 0, w = 0;
    double bpm = 0, hostPpq = -1.0, beats = 0;
    bool hostRolls = false;
    int lastStep = -1;
    uint32_t rng = 0xD15Bu;

    /*  Deterministic per (loop cycle, step, purpose). Everything random in
        this module is drawn from here, so a bar always sounds the same way
        twice, a bounce matches the playback, and the bench can assert on
        it. A free-running rand() would give none of those. */
    static uint32_t stepHash (int64_t cycle, int step, uint32_t salt)
    {
        uint64_t h = (uint64_t) cycle * 0x9E3779B97F4A7C15ull;
        h ^= (uint64_t) (uint32_t) step * 0xBF58476D1CE4E5B9ull;
        h ^= (uint64_t) salt * 0x94D049BB133111EBull;
        h ^= h >> 30; h *= 0xBF58476D1CE4E5B9ull;
        h ^= h >> 27; h *= 0x94D049BB133111EBull;
        h ^= h >> 31;
        return (uint32_t) h;
    }
    static float jitter (int64_t cycle, int step, uint32_t salt, float amount)
    {
        const float u = (float) (stepHash (cycle, step, salt) & 0xFFFFFFu) / 16777215.0f;
        return 1.0f + amount * (2.0f * u - 1.0f);
    }

    static constexpr int kSteps = 32;
    std::array<uint8_t, kSteps> pat[2] { };
    std::atomic<int> patIdx { 0 };
    Voice cur, old;
    float fade = 1.0f;
    Smooth mix;
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
    // Shared sync vocabulary (item 1). FREE is index 0 and the default
    // everywhere, so adding these changed nobody's sound.
    constexpr const char* SYNC_CHOICES = "FREE|2/1|1/1|1/2|1/4|1/8|1/16|1/32";
    constexpr const char* FEEL_CHOICES = "STRAIGHT|TRIPLET|DOTTED";

    const ParamDesc STUTTER_PARAMS[] = {
        { "amount", "AMOUNT", 50, 0,  100, 0, "%",   nullptr },
        { "rate",   "RATE",   80, 10, 160, 0, "dHz", nullptr },
        { "sync",   "SYNC",   0,  0,  7,   1, "",    SYNC_CHOICES },
        { "feel",   "FEEL",   0,  0,  2,   1, "",    FEEL_CHOICES },
    };
    const ParamDesc DELAY_PARAMS[] = {
        { "mix",       "MIX",       25,  0,    100, 0, "%",  nullptr },
        { "time",      "TIME",      260, 40,   900, 0, "ms", nullptr },
        { "feedback",  "FEEDBACK",  34,  0,    112, 0, "%",  nullptr },
        { "offset",    "OFFSET",    0,   -250, 250, 5, "ms", nullptr },
        { "character", "CHARACTER", 0,   0,    1,   1, "",   "CLEAN|TAPE" },
        { "sync",      "SYNC",      0,   0,    7,   1, "",   SYNC_CHOICES },
        { "feel",      "FEEL",      0,   0,    2,   1, "",   FEEL_CHOICES },
    };
    const ParamDesc LOFI_PARAMS[] = {
        { "crush", "CRUSH", 25, 0, 100, 0, "%", nullptr },
        { "noise", "NOISE", 0,  0, 100, 0, "%", nullptr },
        { "dirt",  "DIRT",  30, 0, 100, 0, "%", nullptr },
    };
    const ParamDesc STRIP_PARAMS[] = {
        { "amount",  "COMP",    35,  0,   100, 0, "%",  nullptr },
        { "attack",  "ATTACK",  12,  1,   100, 0, "ms", nullptr },
        { "release", "RELEASE", 180, 20,  800, 0, "ms", nullptr },
        { "low",     "80 HZ",   0,   -12, 12,  0, "dB", nullptr },
        { "lomid",   "250 HZ",  0,   -12, 12,  0, "dB", nullptr },
        { "mid",     "1 KHZ",   0,   -12, 12,  0, "dB", nullptr },
        { "himid",   "3.5 KHZ", 0,   -12, 12,  0, "dB", nullptr },
        { "high",    "10 KHZ",  0,   -12, 12,  0, "dB", nullptr },
        { "output",  "OUTPUT",  0,   -12, 12,  0, "dB", nullptr },
    };
    const ParamDesc DISRUPTOR_PARAMS[] = {
        { "length", "LENGTH", 0,   0, 1,   1, "",  "1 BAR|2 BARS" },
        { "mix",    "MIX",    100, 0, 100, 0, "%", nullptr },
        { "repeat", "REPEAT", 1,   0, 2,   1, "",  "1/2 STEP|1/4 STEP|1/8 STEP" },
        { "decay",  "DECAY",  70,  0, 100, 0, "%", nullptr },
        { "stop",   "STOP",   100, 0, 100, 0, "%", nullptr },
        { "pitch",  "PITCH",  0,   0, 3,   1, "",  "OCT DOWN|5TH DOWN|5TH UP|OCT UP" },
        { "crush",  "CRUSH",  60,  0, 100, 0, "%", nullptr },
        { "duty",   "DUTY",   50,  0, 100, 0, "%", nullptr },
        { "last",   "LAST",   16,  2, 32,  1, "",  nullptr },
        { "chaos",  "CHAOS",  0,   0, 100, 0, "%", nullptr },
    };
    const ParamDesc ROTARY_PARAMS[] = {
        { "speed",   "SPEED",   0,   0,   2,   1, "",   "SLOW|FAST|BRAKE" },
        { "mix",     "MIX",     100, 0,   100, 0, "%",  nullptr },
        { "balance", "BALANCE", 0,   -50, 50,  0, "",   nullptr },
        { "growl",   "GROWL",   20,  0,   100, 0, "%",  nullptr },
    };
    const ParamDesc SHIMMER_PARAMS[] = {
        { "mix",     "MIX",     30, 0, 100, 0, "%", nullptr },
        { "size",    "SIZE",    55, 0, 100, 0, "%", nullptr },
        { "decay",   "DECAY",   60, 0, 100, 0, "%", nullptr },
        { "shimmer", "SHIMMER", 45, 0, 100, 0, "%", nullptr },
        { "tone",    "TONE",    55, 0, 100, 0, "%", nullptr },
    };
    const ParamDesc TREM_PARAMS[] = {
        { "mode",  "MODE",  0,   0, 2,    1, "",    "HARMONIC|TREM|VIBRATO" },
        { "rate",  "RATE",  400, 5, 2000, 0, "cHz", nullptr },
        { "depth", "DEPTH", 55,  0, 100,  0, "%",   nullptr },
        { "sync",  "SYNC",  0,   0, 7,    1, "",    SYNC_CHOICES },
        { "feel",  "FEEL",  0,   0, 2,    1, "",    FEEL_CHOICES },
        { "mix",   "MIX",   100, 0, 100,  0, "%",   nullptr },
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
        { "trem",       "HARMONIC", "harmonic tremolo & vibrato",  1, TREM_PARAMS,    6 },
        { "stutter",    "GATE",     "rhythmic stutter gate",       1, STUTTER_PARAMS, 4 },
        { "lofi",       "GRIT",     "sample crusher and dirt",     1, LOFI_PARAMS,    3 },
        { "strip",      "STRIP",    "compressor and 5-band EQ",    1, STRIP_PARAMS,   9 },
        { "delay",      "ECHO",     "stereo tape echo",            1, DELAY_PARAMS,   7 },
        { "reverb",     "SPACE",    "stereo convolution space",    1, REVERB_PARAMS,  3 },
        { "shimmer",    "SHIMMER",  "octave-up cathedral",         1, SHIMMER_PARAMS, 5 },
        { "rotary",     "ROTARY",   "leslie cabinet, real inertia", 1, ROTARY_PARAMS, 4 },
        { "kieranator", "KIERANATOR", "step-sequenced havoc",       1, DISRUPTOR_PARAMS, 10, "steps" },
    };
    constexpr int kNumTypes = (int) (sizeof (DESCS) / sizeof (DESCS[0]));
}

const Descriptor& TubeSat::desc() const     { return DESCS[0]; }
const Descriptor& PhaserFx::desc() const    { return DESCS[1]; }
const Descriptor& ChorusFx::desc() const    { return DESCS[2]; }
const Descriptor& HarmTrem::desc() const    { return DESCS[3]; }
const Descriptor& StutterGate::desc() const { return DESCS[4]; }
const Descriptor& LofiGrit::desc() const    { return DESCS[5]; }
const Descriptor& Strip::desc() const       { return DESCS[6]; }
const Descriptor& EchoDelay::desc() const   { return DESCS[7]; }
const Descriptor& ConvReverb::desc() const  { return DESCS[8]; }
const Descriptor& Shimmer::desc() const     { return DESCS[9]; }
const Descriptor& Rotary::desc() const      { return DESCS[10]; }
const Descriptor& Disruptor::desc() const   { return DESCS[11]; }

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
        case 3: m = new HarmTrem(); break;
        case 4: m = new StutterGate(); break;
        case 5: m = new LofiGrit(); break;
        case 6: m = new Strip(); break;
        case 7: m = new EchoDelay(); break;
        case 8: m = new ConvReverb(); break;
        case 9: m = new Shimmer(); break;
        case 10: m = new Rotary(); break;
        case 11: m = new Disruptor(); break;
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
        s += "]";
        if (d.custom != nullptr)
            s += std::string (",\"custom\":\"") + json::escape (d.custom) + "\"";
        s += "}";
    }
    s += "]";
    return s;
}

} // namespace bwfx
