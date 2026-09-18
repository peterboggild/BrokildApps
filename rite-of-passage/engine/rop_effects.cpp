#include "rop_effect.h"

#include <cstring>
#include <vector>

namespace rop
{

namespace
{

// ===========================================================================
// 1 · CLIMB — the sweep. The spine of every build.
//
// SPECTRAL (§8.3): a lowpass sweeping down is SUPPOSED to thin out — that is
// the music. What must not move the loudness is the resonance, and the SVF
// removes exactly that with a computed integral over its own response. The
// drive stage is the one place a measured makeup is allowed, because
// saturation's loudness gain depends on the material and no formula knows it.
const ParamDesc CLIMB_P[] = {
    { "mode",   "MODE",     0.0f,    0.0f,     2.0f, 1.0f, "",   Warp::Linear, "LP|BP|HP", true },
    { "cutoff", "CUTOFF", 800.0f,   20.0f, 20000.0f, 0.0f, "Hz", Warp::Log, nullptr, true },
    { "reso",   "RESO",    15.0f,    0.0f,   100.0f, 0.0f, "%",  Warp::Linear },
    { "drive",  "DRIVE",    0.0f,    0.0f,   100.0f, 0.0f, "%",  Warp::Linear },
};
const EffectDesc CLIMB_D {
    "climb", "CLIMB", "resonant sweep to self-oscillation",
    CLIMB_P, 4, Level::Spectral, false, false, true
};

class Climb : public Effect
{
public:
    const EffectDesc& desc() const override { return CLIMB_D; }

    void prepare (double fs, int) override
    {
        for (auto& f : svf) f.prepare (fs);
        //  drive at full tilt needs more than the default ceiling: tanh at
        //  g = 25 is close to a square wave and the correction is real
        mk.prepare (fs, 0.120, 24.0f);
        reset();
    }
    void reset() override { for (auto& f : svf) f.reset(); mk.reset(); }

    void process (float* L, float* R, int n, const float* pL, const float* pR, const Ctx&) override
    {
        const int   mode = (int) (pL[0] + 0.5f);
        const float driveAmt = pL[3] * 0.01f;
        const float g = 1.0f + driveAmt * 24.0f;

        svf[0].set (pL[1], qFor (pL[2]), mode);
        svf[1].set (pR[1], qFor (pR[2]), mode);

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (driveAmt > 1.0e-4f)
            {
                const float dl = std::tanh (g * l), dr = std::tanh (g * r);
                const float m = mk.update (l, r, dl, dr);
                l = dl * m; r = dr * m;
            }
            L[i] = svf[0].process (l);
            R[i] = svf[1].process (r);
        }
    }

private:
    static float qFor (float pct) { return 0.707f + (pct * 0.01f) * (pct * 0.01f) * 28.0f; }
    SvfTPT svf[2];
    MeasuredMakeup mk;
};

// ===========================================================================
// 2 · GAP — the cut. The only effect whose job is absence, and the only one
// that is allowed to take the level to nothing (§8.3, INTENTIONAL).
//
// It carries no timeline of its own: A at 0 % and B at 100 % with a PEAK
// curve is a hole in the middle of its lane, and that is the whole feature.
const ParamDesc GAP_P[] = {
    { "depth", "DEPTH",  0.0f, 0.0f, 100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
    { "edge",  "EDGE",   4.0f, 0.3f,  80.0f, 0.0f, "ms", Warp::Log },
};
const EffectDesc GAP_D { "gap", "GAP", "the silence before the drop",
                         GAP_P, 2, Level::Intentional, false, false, true };

class Gap : public Effect
{
public:
    const EffectDesc& desc() const override { return GAP_D; }
    void prepare (double fs, int) override { sr = fs; reset(); }
    void reset() override { for (auto& s : sm) { s.snap (1.0f); } }

    void process (float* L, float* R, int n, const float* pL, const float* pR, const Ctx&) override
    {
        sm[0].setTau (sr, pL[1] * 0.001);
        sm[1].setTau (sr, pR[1] * 0.001);
        sm[0].target = 1.0f - clampf (pL[0] * 0.01f, 0.0f, 1.0f);
        sm[1].target = 1.0f - clampf (pR[0] * 0.01f, 0.0f, 1.0f);
        for (int i = 0; i < n; ++i) { L[i] *= sm[0].step(); R[i] *= sm[1].step(); }
    }

    void release (Tail t) override
    {
        if (t != Tail::Spill) { sm[0].snap (1.0f); sm[1].snap (1.0f); }
    }

private:
    double sr = 48000.0;
    Smooth sm[2];
};

// ===========================================================================
// 3 · CHOP — the live-signal gate that accelerates. Time keeps running, which
// is what separates it from STUTTER.
//
// NEUTRAL (§8.3). A gate at 30 % duty is 5 dB quieter than the music it
// chopped, and a build that fades out while it intensifies is the exact fault
// Peter named. The makeup is exact rather than measured, because the duty and
// the depth say precisely how much power was removed:
//     power = duty + (1-duty)*(1-depth)^2
// capped at +9 dB so a 5 % duty does not become a detonation.
const ParamDesc CHOP_P[] = {
    { "div",   "RATE",   1.0f, 0.0f, 5.0f, 1.0f, "",   Warp::Linear, kDivChoices },
    { "depth", "DEPTH", 80.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    //  10..90 rather than 5..95 so the makeup below never reaches its
    //  ceiling: at 5 % duty the correction wanted +10.6 dB, the ceiling
    //  gave +9, and the missing 1.6 dB was exactly the fault this effect
    //  is supposed not to have.
    { "duty",  "DUTY",  50.0f, 10.0f, 90.0f, 0.0f, "%", Warp::Linear },
    { "slew",  "SLEW",   3.0f, 0.1f,  50.0f, 0.0f, "ms", Warp::Log },
};
const EffectDesc CHOP_D { "chop", "CHOP", "rhythmic gate, accelerating",
                          CHOP_P, 4, Level::Neutral, false, false, true };

class Chop : public Effect
{
public:
    const EffectDesc& desc() const override { return CHOP_D; }
    void prepare (double fs, int) override
    {
        sr = fs;
        for (auto& m : mk) m.prepare (fs, 0.5, 10.0f);
        reset();
    }
    void reset() override
    {
        sm[0].snap (1.0f); sm[1].snap (1.0f);
        for (auto& m : mk) m.reset();
    }

    void process (float* L, float* R, int n, const float* pL, const float* pR, const Ctx& c) override
    {
        const float* p[2] = { pL, pR };
        for (int ch = 0; ch < 2; ++ch) sm[ch].setTau (sr, p[ch][3] * 0.001);

        for (int i = 0; i < n; ++i)
        {
            const double ppq = c.ppq + i * c.ppqPerSample;
            float* buf[2] = { L + i, R + i };
            for (int ch = 0; ch < 2; ++ch)
            {
                const double beats = beatsForDiv ((int) (p[ch][0] + 0.5f));
                const double ph = ppq / beats - std::floor (ppq / beats);
                const float duty  = clampf (p[ch][2] * 0.01f, 0.05f, 0.95f);
                const float depth = clampf (p[ch][1] * 0.01f, 0.0f, 1.0f);
                sm[ch].target = (ph < duty) ? 1.0f : (1.0f - depth);
                const float g = sm[ch].step();
                const float in = *buf[ch];
                const float gated = in * g;
                *buf[ch] = gated * mk[ch].update (in, in, gated, gated);
            }
        }
    }

private:
    double sr = 48000.0;
    Smooth sm[2];
    MeasuredMakeup mk[2];
};

// ===========================================================================
// 4 · STUTTER — capture a slice and roll it, tightening into a pitched buzz.
//
// NEUTRAL (§8.3): a stutter repeats whichever slice it happened to catch, and
// that slice can sit well above or below the music it replaced. So the
// captured material is normalised to the running loudness of the input at the
// moment of capture.
//
// PITCH EXACT at PITCH 0 (§8.2), and exactly is meant: at zero the read rate
// is 1.0 and the read index is an integer, so the path through this code is a
// copy. "Nearly one" would be a permanent detune nobody would find.
const ParamDesc STUT_P[] = {
    { "div",     "DIV",     1.0f,  0.0f,   5.0f, 1.0f, "",  Warp::Linear, kDivChoices },
    { "depth",   "DEPTH", 100.0f,  0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    { "decay",   "DECAY",   0.0f,  0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    //  PITCH is a level knob because moving a spectrum moves its K-weighted
//  loudness even at constant energy — that is the metric being honest,
//  not the effect being sloppy.
    { "pitch",   "PITCH",   0.0f,-12.0f,  12.0f, 1.0f, "st", Warp::Linear, nullptr, true },
    { "capture", "CAPTURE", 0.0f,  0.0f,   1.0f, 1.0f, "",  Warp::Linear, "REFRESH|HOLD" },
};
const EffectDesc STUT_D { "stutter", "STUTTER", "capture and roll",
                          STUT_P, 5, Level::Neutral, false, false, false, true };

class Stutter : public Effect
{
public:
    const EffectDesc& desc() const override { return STUT_D; }

    void prepare (double fs, int) override
    {
        sr = fs;
        size = nextPow2 ((int) (fs * 2.0));
        mask = size - 1;
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        inPow.setTau (fs, 0.050);
        reset();
    }

    void reset() override
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f);
        std::fill (buf[1].begin(), buf[1].end(), 0.0f);
        w = 0; captured = false; playPos = 0.0; amp = 1.0f; sliceGain = 1.0f;
        lastPhase = -1.0;
        inPow.reset();
    }

    void arm() override { captured = false; }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx& c) override
    {
        const double beats = beatsForDiv ((int) (pL[0] + 0.5f));
        const float  depth = clampf (pL[1] * 0.01f, 0.0f, 1.0f);
        const float  decay = clampf (pL[2] * 0.01f, 0.0f, 1.0f);
        const int    semis = (int) std::lround (pL[3]);
        const bool   hold  = pL[4] > 0.5f;
        const double rate  = (semis == 0) ? 1.0 : std::pow (2.0, semis / 12.0);

        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) (w & mask)] = L[i];
            buf[1][(size_t) (w & mask)] = R[i];
            inPow.push (L[i], R[i]);

            const double ppq = c.ppq + i * c.ppqPerSample;
            const double ph  = ppq / beats - std::floor (ppq / beats);
            const bool boundary = (lastPhase >= 0.0 && ph < lastPhase);
            lastPhase = ph;

            if (boundary && (! captured || ! hold))
            {
                sliceLen = std::min (size / 2, std::max (32, (int) (beats * 60.0 / std::max (20.0, c.bpm) * sr)));
                sliceStart = w - sliceLen;
                playPos = 0.0;
                amp = 1.0f;
                captured = true;

                //  §8.1: match the slice to what it replaced
                double e = 0.0;
                for (int j = 0; j < sliceLen; ++j)
                {
                    const float a = buf[0][(size_t) ((sliceStart + j) & mask)];
                    const float b = buf[1][(size_t) ((sliceStart + j) & mask)];
                    e += 0.5 * ((double) a * a + (double) b * b);
                }
                const double sliceRms = std::sqrt (e / std::max (1, sliceLen));
                const double inRms = inPow.rms();
                sliceGain = (sliceRms > 1e-9 && inRms > 1e-9)
                          ? (float) clampf ((float) (inRms / sliceRms), 0.25f, 4.0f) : 1.0f;
            }
            else if (boundary)
            {
                playPos = 0.0;
            }

            float wl = L[i], wr = R[i];
            if (captured && sliceLen > 0)
            {
                if (semis == 0)
                {
                    //  the exact path: an integer read, so PITCH 0 cannot detune
                    const int idx = sliceStart + (int) playPos;
                    wl = buf[0][(size_t) (idx & mask)];
                    wr = buf[1][(size_t) (idx & mask)];
                }
                else
                {
                    wl = catmullRead (buf[0].data(), mask, sliceStart + sliceLen,
                                      (float) (sliceLen - playPos));
                    wr = catmullRead (buf[1].data(), mask, sliceStart + sliceLen,
                                      (float) (sliceLen - playPos));
                }
                wl *= amp * sliceGain;
                wr *= amp * sliceGain;

                playPos += rate;
                if (playPos >= (double) sliceLen)
                {
                    playPos -= (double) sliceLen;
                    amp *= (1.0f - decay * 0.35f);
                }
            }

            //  equal power: the repeated slice is decorrelated from the live
            //  signal, and a linear crossfade of those two dips 3 dB at 50 %
            float gd, gw; equalPowerMix (gd, gw, depth);
            L[i] = L[i] * gd + wl * gw;
            R[i] = R[i] * gd + wr * gw;
            ++w;
        }
    }

    void release (Tail t) override
    {
        if (t == Tail::Spill) { captured = captured && true; }
        else                  { captured = false; }
        if (t == Tail::Clear) { std::fill (buf[0].begin(), buf[0].end(), 0.0f);
                                std::fill (buf[1].begin(), buf[1].end(), 0.0f); }
    }

private:
    double sr = 48000.0;
    int size = 0, mask = 0, w = 0;
    std::vector<float> buf[2];
    bool captured = false;
    int sliceStart = 0, sliceLen = 0;
    double playPos = 0.0, lastPhase = -1.0;
    float amp = 1.0f, sliceGain = 1.0f;
    PowerFollower inPow;
};

// ===========================================================================
// 5 · TAPE — the head moves, so the pitch bends. Peter asked for this one by
// name and the bend IS the effect.
//
// REPITCH changes the READ RATE: the smoothed delay is read with a cubic
// interpolator, so moving it doppler-shifts exactly as a tape machine does.
// FADE re-samples the target every 30 ms and equal-power crossfades between
// two taps, which is a different and useful sound and is the mode that must
// NOT repitch (§8.2) — so WOW is disabled there too, because wow is pitch.
//
// MOTOR is the inertia: a one-pole on the read position, so a heavy motor
// arrives late and a light one snaps.
const ParamDesc TAPE_P[] = {
    { "time",  "TIME",    300.0f,  20.0f, 1500.0f, 0.0f, "ms", Warp::Log },
    { "fb",    "FEEDBACK", 35.0f,   0.0f,  120.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
    { "motor", "MOTOR",   120.0f,   1.0f, 2000.0f, 0.0f, "ms", Warp::Log },
    { "mode",  "MODE",      0.0f,   0.0f,    1.0f, 1.0f, "",   Warp::Linear, "REPITCH|FADE" },
    { "drive", "DRIVE",    25.0f,   0.0f,  100.0f, 0.0f, "%",  Warp::Linear },
    { "wow",   "WOW",      15.0f,   0.0f,  100.0f, 0.0f, "%",  Warp::Linear },
    { "mix",   "MIX",      35.0f,   0.0f,  100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
};
const EffectDesc TAPE_D { "tape", "TAPE", "analogue echo, the head moves",
                          TAPE_P, 7, Level::Neutral, true, false, false };

class Tape : public Effect
{
public:
    const EffectDesc& desc() const override { return TAPE_D; }

    void prepare (double fs, int) override
    {
        sr = fs;
        size = nextPow2 ((int) (fs * 2.0) + 8);
        mask = size - 1;
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        reset();
    }

    void reset() override
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f);
        std::fill (buf[1].begin(), buf[1].end(), 0.0f);
        w = 0; readDelay = -1.0f; lp[0] = lp[1] = 0.0f;
        wowPhase = 0.0; fadePos = 1.0f; tapA = tapB = -1.0f; fadeCount = 0;
        fedThisRelease = true;
    }

    void release (Tail t) override
    {
        fedThisRelease = (t == Tail::Spill);   // SPILL keeps ringing, unfed
        if (t == Tail::Clear)
        {
            std::fill (buf[0].begin(), buf[0].end(), 0.0f);
            std::fill (buf[1].begin(), buf[1].end(), 0.0f);
            lp[0] = lp[1] = 0.0f;
        }
    }
    void arm() override { fedThisRelease = true; }
    bool ringing() const override { return std::abs (lp[0]) + std::abs (lp[1]) > 1.0e-5f; }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        const float target  = clampf (pL[0], 1.0f, 1900.0f) * 0.001f * (float) sr;
        const float fb      = clampf (pL[1] * 0.01f, 0.0f, 1.2f);
        const float motorMs = clampf (pL[2], 1.0f, 2000.0f);
        const bool  fade    = pL[3] > 0.5f;
        const float drive   = 1.0f + pL[4] * 0.01f * 8.0f;
        const float wow     = pL[5] * 0.01f;
        const float mix     = clampf (pL[6] * 0.01f, 0.0f, 1.0f);

        if (readDelay < 0.0f) { readDelay = target; tapA = tapB = target; }
        const float a = 1.0f - std::exp (-1.0f / (float) (motorMs * 0.001 * sr));
        const int   fadeLen = (int) (0.030 * sr);

        for (int i = 0; i < n; ++i)
        {
            float d;
            if (! fade)
            {
                //  REPITCH: the read position itself moves, so the pitch bends
                readDelay += a * (target - readDelay);
                wowPhase += 0.37 / sr * 2.0 * M_PI;
                if (wowPhase > 2.0 * M_PI) wowPhase -= 2.0 * M_PI;
                d = readDelay * (1.0f + wow * 0.004f * (float) std::sin (wowPhase));
            }
            else
            {
                //  FADE: two fixed taps, equal-power crossfaded. No repitch.
                if (--fadeCount <= 0)
                {
                    tapA = tapB; tapB = target;
                    fadeCount = fadeLen; fadePos = 0.0f;
                }
                fadePos = clampf (fadePos + 1.0f / (float) fadeLen, 0.0f, 1.0f);
                d = tapA;      // the crossfade is applied to the samples below
            }

            const float dClamped = clampf (d, 2.0f, (float) (size - 4));
            float yl, yr;
            if (! fade)
            {
                yl = catmullRead (buf[0].data(), mask, w, dClamped);
                yr = catmullRead (buf[1].data(), mask, w, dClamped);
            }
            else
            {
                const float dB = clampf (tapB, 2.0f, (float) (size - 4));
                const float ca = std::cos (fadePos * 0.5f * (float) M_PI);
                const float cb = std::sin (fadePos * 0.5f * (float) M_PI);
                yl = catmullRead (buf[0].data(), mask, w, dClamped) * ca
                   + catmullRead (buf[0].data(), mask, w, dB) * cb;
                yr = catmullRead (buf[1].data(), mask, w, dClamped) * ca
                   + catmullRead (buf[1].data(), mask, w, dB) * cb;
            }

            //  head saturation and a band limit INSIDE the loop, so feedback
            //  past unity howls instead of detonating. Never noise in a loop.
            lp[0] += 0.35f * (yl - lp[0]);
            lp[1] += 0.35f * (yr - lp[1]);
            const float fl = std::tanh (drive * lp[0]) / drive;
            const float fr = std::tanh (drive * lp[1]) / drive;

            const float inL = fedThisRelease ? L[i] : 0.0f;
            const float inR = fedThisRelease ? R[i] : 0.0f;
            buf[0][(size_t) (w & mask)] = inL + fl * fb;
            buf[1][(size_t) (w & mask)] = inR + fr * fb;

            L[i] = L[i] + (yl - 0.0f) * mix;
            R[i] = R[i] + (yr - 0.0f) * mix;
            ++w;
        }
    }

private:
    double sr = 48000.0, wowPhase = 0.0;
    int size = 0, mask = 0, w = 0, fadeCount = 0;
    std::vector<float> buf[2];
    float readDelay = -1.0f, lp[2] {}, tapA = -1.0f, tapB = -1.0f, fadePos = 1.0f;
    bool fedThisRelease = true;
};

// ===========================================================================
// 6 · RISER — a generator, and the one effect that still works when the bar
// before the drop is nearly empty. INTENTIONAL level by definition (§8.3):
// adding sound where there was none is the entire job.
const ParamDesc RISE_P[] = {
    { "source", "SOURCE",   0.0f,    0.0f,     2.0f, 1.0f, "",   Warp::Linear, "NOISE|TONE|SHEPARD", true },
    { "freq",   "FREQ",   200.0f,   50.0f, 12000.0f, 0.0f, "Hz", Warp::Log, nullptr, true },
    { "reso",   "RESO",    40.0f,    0.0f,   100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
    { "level",  "LEVEL",  -60.0f,  -60.0f,     6.0f, 0.0f, "dB", Warp::Linear, nullptr, true },
    { "width",  "WIDTH",   70.0f,    0.0f,   100.0f, 0.0f, "%",  Warp::Linear },
};
const EffectDesc RISE_D { "riser", "RISER", "noise, tone or endless climb",
                          RISE_P, 5, Level::Intentional, true, true, true };

class Riser : public Effect
{
public:
    const EffectDesc& desc() const override { return RISE_D; }

    void prepare (double fs, int) override
    {
        sr = fs;
        for (auto& f : bp) f.prepare (fs);
        rng[0].seed (0xA1B2C3D4u); rng[1].seed (0x5E6F7081u);
        lvl[0].setTau (fs, 0.020); lvl[1].setTau (fs, 0.020);
        reset();
    }

    void reset() override
    {
        for (auto& f : bp) f.reset();
        phase = 0.0; shepPos = 0.0;
        lvl[0].snap (0.0f); lvl[1].snap (0.0f);
    }

    void process (float* L, float* R, int n, const float* pL, const float* pR, const Ctx&) override
    {
        const float* p[2] = { pL, pR };
        const int src = (int) (pL[0] + 0.5f);
        const float width = clampf (pL[4] * 0.01f, 0.0f, 1.0f);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float q = 0.707f + (p[ch][2] * 0.01f) * (p[ch][2] * 0.01f) * 40.0f;
            bp[ch].set (p[ch][1], q, SvfTPT::BP);
            lvl[ch].target = (p[ch][3] <= -59.5f) ? 0.0f : std::pow (10.0f, p[ch][3] * 0.05f);
        }

        for (int i = 0; i < n; ++i)
        {
            float a = 0.0f, b = 0.0f;
            if (src == 0)
            {
                //  two independent noises; WIDTH mixes them toward mono
                const float n0 = rng[0].bip(), n1 = rng[1].bip();
                a = bp[0].process (lerpf (n0, n0, 0.0f) * 1.4f);
                b = bp[1].process (lerpf (n0, n1, width) * 1.4f);
            }
            else if (src == 1)
            {
                phase += 2.0 * M_PI * (double) pL[1] / sr;
                if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
                const float h = clampf (pL[2] * 0.01f, 0.0f, 1.0f);
                const float s = (float) (std::sin (phase)
                                       + 0.5 * h * std::sin (2.0 * phase)
                                       + 0.25 * h * std::sin (3.0 * phase));
                a = b = s * 0.5f;
            }
            else
            {
                //  SHEPARD: octave-spaced partials under a fixed raised-cosine
                //  envelope. As the ladder walks one octave it wraps, so the
                //  climb never ends and never jumps.
                shepPos += (double) pL[1] / (sr * 40.0);
                if (shepPos >= 1.0) shepPos -= 1.0;
                float s = 0.0f;
                for (int k = 0; k < kShep; ++k)
                {
                    const double pos = k + shepPos;
                    const double f = 27.5 * std::pow (2.0, pos);
                    if (f > sr * 0.45) continue;
                    shepPhase[k] += 2.0 * M_PI * f / sr;
                    if (shepPhase[k] > 2.0 * M_PI) shepPhase[k] -= 2.0 * M_PI;
                    const double x = (pos - (kShep - 1) * 0.5) / ((kShep - 1) * 0.5);
                    const double env = 0.5 * (1.0 + std::cos (M_PI * clampf ((float) x, -1.0f, 1.0f)));
                    s += (float) (env * std::sin (shepPhase[k]));
                }
                a = b = s * (1.4f / kShep);
            }

            L[i] += a * lvl[0].step();
            R[i] += b * lvl[1].step();
        }
    }

    void release (Tail t) override
    {
        if (t != Tail::Spill) { lvl[0].snap (0.0f); lvl[1].snap (0.0f); }
    }

private:
    static constexpr int kShep = 8;
    double sr = 48000.0, phase = 0.0, shepPos = 0.0, shepPhase[kShep] {};
    SvfTPT bp[2];
    Rng rng[2];
    Smooth lvl[2];
};

// ===========================================================================
struct Entry { const EffectDesc* d; Effect* (*make)(); };

const Entry REGISTRY[] = {
    { &CLIMB_D, [] () -> Effect* { return new Climb; } },
    { &TAPE_D,  [] () -> Effect* { return new Tape; } },
    { &STUT_D,  [] () -> Effect* { return new Stutter; } },
    { &CHOP_D,  [] () -> Effect* { return new Chop; } },
    { &RISE_D,  [] () -> Effect* { return new Riser; } },
    { &GAP_D,   [] () -> Effect* { return new Gap; } },
};
constexpr int kNumEffects = (int) (sizeof (REGISTRY) / sizeof (REGISTRY[0]));

} // namespace

int numEffects() { return kNumEffects; }

const EffectDesc& effectDescriptor (int t)
{
    return *REGISTRY[t < 0 ? 0 : (t >= kNumEffects ? kNumEffects - 1 : t)].d;
}

int effectTypeByName (const char* id)
{
    if (id == nullptr) return -1;
    for (int t = 0; t < kNumEffects; ++t)
        if (std::strcmp (id, REGISTRY[t].d->id) == 0) return t;
    return -1;
}

Effect* createEffect (int t)
{
    if (t < 0 || t >= kNumEffects) return nullptr;
    return REGISTRY[t].make();
}

} // namespace rop
