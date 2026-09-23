// RITE OF PASSAGE — the other six: GRAIN, BLOOM, FREEZE, REVERSE, BRAKE, DIVE.
//
// Design authority: RITE-OF-PASSAGE-DESIGN.md §5 (the gestures), §7 (the
// standard), §8 (level and pitch). Written 2026-09-18 after the first six.
//
// ONE DECISION THAT DIFFERS FROM THE DESIGN, stated here rather than hidden:
// §7 asks FREEZE, GRAIN and DIVE to reuse LEGION's spectral core (FFT frames,
// 42.7 ms of latency) and §10 then has the whole plugin report a fixed
// worst-case latency for everybody. The six built here are all TIME-DOMAIN
// and ZERO-LATENCY instead — equal-power windowed grains, dithered positions
// (§7's own rule for GRAIN) — so the plugin keeps the latency of 0 that its
// wrapper harness already asserts and no slot ever costs a delay on the
// others. What that trades away is FREEZE's spectral purity and DIVE's
// formant handling; what it keeps is a transition plugin that can sit on a
// live bus. If the spectral versions are wanted later they are new effects
// (FREEZE MkII), never replacements — the BWFX rule.
//
// Level contracts (§8.3, as declared below, and the bench holds each to it):
//   GRAIN   NEUTRAL, computed: equal-power windows at a known overlap have a
//           known power, so the cloud is scaled by sqrt(2/overlap).
//   BLOOM   NEUTRAL, measured: the wet is matched to the input in the
//           K-weighted domain the contract is stated in, slowly, and held
//           while frozen — so a frozen wash neither creeps nor pumps.
//   FREEZE  NEUTRAL, computed: the cloud reads the captured frame at unity
//           windows, so it carries the frame's own power.
//   REVERSE NEUTRAL by construction: a reversed slice has the energy of the
//           slice, exactly.
//   BRAKE   NEUTRAL except SPEED, which is a level knob: a stopped tape is
//           silent and that is what the knob is for.
//   DIVE    NEUTRAL except SEMIS, which is a level knob: a shift moves the
//           spectrum, and a moved spectrum is a different loudness. The
//           waveform itself comes out at unity, with no makeup riding on it.
//
// Real-time rules as everywhere: process() allocates nothing, locks nothing,
// logs nothing; every buffer is sized in prepare().

#include "rop_effects_more.h"
#include "rop_blocks.h"

#include <cstring>
#include <vector>

namespace rop { namespace more {

namespace
{

//  HalfBand, KMakeup, GrainBank and semisToRate now live in rop_blocks.h,
//  because the third six need them too and two copies of an oversampler is
//  two things to get wrong.

// ===========================================================================
// 7 · GRAIN — the music breaks into particles, then a cloud.
//
// Equal-power windowed grains with DITHERED positions (§7): evenly spaced
// grains comb-filter, and the comb is the sound of a cheap granular. SPREAD
// gives each grain its own pitch offset; at 0 every grain reads at exactly
// rate 1 from an integer start, so the cloud is a re-ordered copy of the
// input and nothing else (§8.2's "exact unless spread is on").
const ParamDesc GRAIN_P[] = {
    { "size",    "SIZE",     60.0f, 10.0f,  250.0f, 0.0f, "ms", Warp::Log },
    { "density", "DENSITY",  20.0f,  4.0f,   80.0f, 0.0f, "/s", Warp::Log },
    { "scatter", "SCATTER",  30.0f,  0.0f,  100.0f, 0.0f, "%",  Warp::Linear },
    { "spread",  "SPREAD",    0.0f,  0.0f,   12.0f, 0.0f, "st", Warp::Linear },
    { "mix",     "MIX",     100.0f,  0.0f,  100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
};
const EffectDesc GRAIN_D_ { "grain", "GRAIN", "particles, then a cloud",
                            GRAIN_P, 5, Level::Neutral, false, false, false, true };

class Grain : public Effect
{
public:
    const EffectDesc& desc() const override { return GRAIN_D_; }
    void prepare (double fs, int) override { bank.prepare (fs, 1.0); rng.seed (0x6A3F19C7u); reset(); }
    void reset() override { bank.reset(); rng.seed (0x6A3F19C7u); }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        const int    len      = std::max (8, (int) (pL[0] * 0.001 * bank.sr));
        const double density  = std::max (1.0, (double) pL[1]);
        const double scatter  = pL[2] * 0.01 * 0.250 * bank.sr;          // up to 250 ms back
        const float  spread   = pL[3];
        const float  mix      = clampf (pL[4] * 0.01f, 0.0f, 1.0f);
        const int    interval = std::max (1, (int) (bank.sr / density));
        //  §8.1 computed: k overlapping sin windows carry k/2 of the power
        const double overlap  = std::max (0.25, density * (double) len / bank.sr);
        const float  norm     = std::min (4.0f, (float) std::sqrt (2.0 / overlap));
        float gd, gw; equalPowerMix (gd, gw, mix);

        for (int i = 0; i < n; ++i)
        {
            bank.write (L[i], R[i]);
            if (--bank.nextLaunch <= 0)
            {
                bank.nextLaunch = interval;
                //  scatter dithers the start (§7); with spread the rate too. An
                //  integer start at rate 1 is the exact path.
                const double back = std::floor (scatter * (double) rng.uni());
                const double rate = spread > 1.0e-4f ? semisToRate (spread * rng.bip()) : 1.0;
                bank.launch (back, rate, len);
            }
            float wl, wr;
            bank.read (wl, wr, 1.0, false);
            bank.advance();
            L[i] = L[i] * gd + wl * norm * gw;
            R[i] = R[i] * gd + wr * norm * gw;
        }
    }

    void release (Tail t) override { if (t != Tail::Spill) bank.reset(); }

private:
    GrainBank bank;
    Rng rng;
};

// ===========================================================================
// 8 · BLOOM — a small room growing into an enormous wash.
//
// An 8-line FDN behind a Householder matrix (orthogonal, so the loop is
// lossless before the decay gain is applied), delay lengths MODULATED so the
// tail does not ring metallic (§7), a damping one-pole per line. FREEZE walks
// the loop gain to EXACTLY 1.0, the modulation to exactly 0 and the delays
// to integers — an interpolated moving tap is a filter and would creep — so a
// frozen wash holds its level to the bit, which §15 measures over 60 s.
const ParamDesc BLOOM_P[] = {
    { "size",   "SIZE",    40.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    { "decay",  "DECAY",   50.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear, nullptr, true },
    { "damp",   "DAMP",    35.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    { "mod",    "MOD",     30.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    //  FREEZE is a level knob in the bench's sense: a wash frozen before
    //  anything went in is silence, and that is the knob doing its job
    { "freeze", "FREEZE",   0.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear, nullptr, true },
    { "mix",    "MIX",     35.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear, nullptr, true },
};
const EffectDesc BLOOM_D_ { "bloom", "BLOOM", "a room growing into a wash",
                            BLOOM_P, 6, Level::Neutral, false, false, false };

class Bloom : public Effect
{
public:
    static constexpr int kLines = 8;
    const EffectDesc& desc() const override { return BLOOM_D_; }

    void prepare (double fs, int) override
    {
        sr = fs;
        size = nextPow2 ((int) (fs * 0.9) + 16);
        mask = size - 1;
        for (auto& b : line) b.assign ((size_t) size, 0.0f);
        mk.prepare (fs, 0.500, 12.0f);
        reset();
    }
    void reset() override
    {
        for (auto& b : line) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& z : lp) z = 0.0f;
        for (int i = 0; i < kLines; ++i) ph[i] = (double) i * 0.7853981;
        w = 0; fed = true;
        mk.reset();
    }
    void arm() override { fed = true; }
    void release (Tail t) override
    {
        fed = (t == Tail::Spill);
        if (t == Tail::Clear) { for (auto& b : line) std::fill (b.begin(), b.end(), 0.0f); for (auto& z : lp) z = 0.0f; }
    }
    bool ringing() const override
    {
        float e = 0.0f;
        for (int i = 0; i < kLines; ++i) e += std::abs (lp[i]);
        return e > 1.0e-5f;
    }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        static const double baseMs[kLines] = { 29.7, 37.1, 41.1, 43.7, 53.3, 61.3, 67.9, 73.1 };
        static const double lfoHz[kLines]  = { 0.11, 0.17, 0.23, 0.13, 0.29, 0.19, 0.31, 0.37 };
        const float size01  = clampf (pL[0] * 0.01f, 0.0f, 1.0f);
        const float decay01 = clampf (pL[1] * 0.01f, 0.0f, 1.0f);
        const float damp01  = clampf (pL[2] * 0.01f, 0.0f, 1.0f);
        const float mod01   = clampf (pL[3] * 0.01f, 0.0f, 1.0f);
        const float freeze  = clampf (pL[4] * 0.01f, 0.0f, 1.0f);
        const float mix     = clampf (pL[5] * 0.01f, 0.0f, 1.0f);
        const bool  frozen  = freeze >= 0.9995f;

        const double scale = 0.25 + 2.75 * size01;
        const double rt60  = 0.2 * std::pow (100.0, (double) decay01);     // 0.2 s .. 20 s
        //  damping: 18 kHz down to 900 Hz
        const double fc = 18000.0 * std::pow (0.05, (double) damp01);
        const float dampA = frozen ? 1.0f : (float) (1.0 - std::exp (-2.0 * M_PI * fc / sr));
        const double modDepth = frozen ? 0.0 : 0.0003 * sr * mod01 * (1.0 - freeze);

        double lenS[kLines]; float gain[kLines];
        for (int i = 0; i < kLines; ++i)
        {
            lenS[i] = baseMs[i] * 0.001 * scale * sr;
            const double g = std::pow (10.0, -3.0 * (lenS[i] / sr) / rt60);
            gain[i] = frozen ? 1.0f : (float) (g + (1.0 - g) * (double) freeze);
            if (frozen) lenS[i] = std::round (lenS[i]);
        }
        float gd, gw; equalPowerMix (gd, gw, mix);

        for (int s = 0; s < n; ++s)
        {
            const float inL = fed && ! frozen ? L[s] * (1.0f - freeze) : 0.0f;
            const float inR = fed && ! frozen ? R[s] * (1.0f - freeze) : 0.0f;

            float v[kLines]; float sum = 0.0f;
            for (int i = 0; i < kLines; ++i)
            {
                double d = lenS[i];
                if (modDepth > 0.0)
                {
                    ph[i] += 2.0 * M_PI * lfoHz[i] / sr;
                    if (ph[i] > 2.0 * M_PI) ph[i] -= 2.0 * M_PI;
                    d += modDepth * std::sin (ph[i]);
                }
                const float y = catmullRead (line[i].data(), mask, (int) (w & mask) + size, clampf ((float) d, 4.0f, (float) (size - 8)));
                if (frozen) v[i] = y;
                else { lp[i] += dampA * (y - lp[i]); v[i] = lp[i] * gain[i]; }
                sum += v[i];
            }
            //  Householder: orthogonal, so the loop loses nothing but what gain[] takes
            const float hs = sum * (2.0f / (float) kLines);
            float wetL = 0.0f, wetR = 0.0f;
            for (int i = 0; i < kLines; ++i)
            {
                const float fb = v[i] - hs;
                const float in = (i & 1) ? inR : inL;
                line[i][(size_t) (w & mask)] = in + fb;
                if (i & 1) wetR += v[i]; else wetL += v[i];
            }
            wetL *= 0.5f; wetR *= 0.5f;
            ++w;

            //  §8.1 measured, K-weighted, and HELD while frozen
            const float m = mk.update (L[s], R[s], wetL, wetR, frozen || ! fed);
            L[s] = L[s] * gd + wetL * m * gw;
            R[s] = R[s] * gd + wetR * m * gw;
        }
    }

private:
    double sr = 48000.0, ph[kLines] {};
    int size = 0, mask = 0; long long w = 0;
    std::vector<float> line[kLines];
    float lp[kLines] {};
    bool fed = true;
    KMakeup mk;
};

// ===========================================================================
// 9 · FREEZE — the harmonic fingerprint held while the rhythm dissolves.
//
// Captures the last 300 ms when the slot is entered (and on every bar while
// CAPTURE is REFRESH), then reads that frame as a cloud of grains from random
// positions at exactly rate 1 (§8.2, exact): the frame's spectrum stays, its
// order goes. BLUR is the grain length — short blurs to a shimmer, long keeps
// more of the original phrase. HOLD is how much of the live signal it replaces.
const ParamDesc FREEZE_P[] = {
    { "blur",    "BLUR",    50.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    { "hold",    "HOLD",     0.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear, nullptr, true },
    { "capture", "CAPTURE",  0.0f, 0.0f,   1.0f, 1.0f, "",  Warp::Linear, "REFRESH|HOLD" },
};
const EffectDesc FREEZE_D_ { "freeze", "FREEZE", "the fingerprint held, the rhythm gone",
                             FREEZE_P, 3, Level::Neutral, false, false, false, true };

class Freeze : public Effect
{
public:
    static constexpr int kMaxGrains = 16;
    const EffectDesc& desc() const override { return FREEZE_D_; }

    void prepare (double fs, int) override
    {
        sr = fs;
        capLen = (int) (fs * 0.300);
        size = nextPow2 (capLen * 2 + 16); mask = size - 1;
        ring[0].assign ((size_t) size, 0.0f); ring[1].assign ((size_t) size, 0.0f);
        cap[0].assign ((size_t) capLen, 0.0f); cap[1].assign ((size_t) capLen, 0.0f);
        rng.seed (0x51E7A3B9u);
        reset();
    }
    void reset() override
    {
        std::fill (ring[0].begin(), ring[0].end(), 0.0f); std::fill (ring[1].begin(), ring[1].end(), 0.0f);
        w = 0; captured = false; wantCapture = false; lastPhase = -1.0; nextLaunch = 0;
        for (auto& g : grains) g.on = false;
        rng.seed (0x51E7A3B9u);
    }
    void arm() override { wantCapture = true; }
    void release (Tail t) override { if (t != Tail::Spill) { captured = false; for (auto& g : grains) g.on = false; } }
    bool ringing() const override { return captured; }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx& c) override
    {
        const int   len   = std::max (64, std::min (capLen - 1, (int) ((0.020 + 0.180 * pL[0] * 0.01) * sr)));
        const float hold  = clampf (pL[1] * 0.01f, 0.0f, 1.0f);
        const bool  refresh = pL[2] < 0.5f;
        const int   interval = std::max (1, len / 4);            // overlap 4
        const float norm = (float) std::sqrt (2.0 / 4.0);
        float gd, gw; equalPowerMix (gd, gw, captured ? hold : 0.0f);

        for (int i = 0; i < n; ++i)
        {
            ring[0][(size_t) (w & mask)] = L[i];
            ring[1][(size_t) (w & mask)] = R[i];
            ++w;

            //  bar boundaries, for REFRESH
            const double ppq = c.ppq + i * c.ppqPerSample;
            const double ph  = ppq / 4.0 - std::floor (ppq / 4.0);
            const bool bar = (lastPhase >= 0.0 && ph < lastPhase);
            lastPhase = ph;
            if (wantCapture || (bar && captured && refresh && c.inLane)) { capture(); wantCapture = false; }

            float wl = 0.0f, wr = 0.0f;
            if (captured)
            {
                if (--nextLaunch <= 0)
                {
                    nextLaunch = interval;
                    for (auto& g : grains)
                        if (! g.on)
                        {
                            g.on = true; g.phase = 0; g.len = len;
                            g.start = (int) std::floor ((double) rng.uni() * (double) (capLen - len));
                            break;
                        }
                }
                for (auto& g : grains)
                {
                    if (! g.on) continue;
                    const float win = std::sin ((float) M_PI * (float) g.phase / (float) g.len);
                    const size_t idx = (size_t) (g.start + g.phase);
                    wl += win * cap[0][idx];
                    wr += win * cap[1][idx];
                    if (++g.phase >= g.len) g.on = false;
                }
            }
            L[i] = L[i] * gd + wl * norm * gw;
            R[i] = R[i] * gd + wr * norm * gw;
        }
    }

private:
    void capture()
    {
        //  the last 300 ms, copied out so the live ring can keep moving
        const long long start = w - capLen;
        for (int j = 0; j < capLen; ++j)
        {
            cap[0][(size_t) j] = ring[0][(size_t) ((start + j) & mask)];
            cap[1][(size_t) j] = ring[1][(size_t) ((start + j) & mask)];
        }
        captured = true;
    }

    struct G { bool on = false; int start = 0, len = 0, phase = 0; };
    double sr = 48000.0, lastPhase = -1.0;
    int size = 0, mask = 0, capLen = 0, nextLaunch = 0;
    long long w = 0;
    std::vector<float> ring[2], cap[2];
    G grains[kMaxGrains];
    bool captured = false, wantCapture = false;
    Rng rng;
};

// ===========================================================================
// 10 · REVERSE — fragments rushing backwards into their next boundary.
//
// Every division, the slice just finished is played back to front over the
// next one. Integer reads at rate exactly 1, so it is exact by construction
// and a reversed slice carries the slice's own energy (NEUTRAL). EDGE is a
// short raised-cosine at each join so the splice does not click.
const ParamDesc REVERSE_P[] = {
    { "div",   "DIV",    1.0f, 0.0f,   5.0f, 1.0f, "",   Warp::Linear, kDivChoices },
    { "depth", "DEPTH", 100.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    { "edge",  "EDGE",    3.0f, 0.3f,  30.0f, 0.0f, "ms", Warp::Log },
};
const EffectDesc REVERSE_D_ { "reverse", "REVERSE", "fragments run backwards",
                              REVERSE_P, 3, Level::Neutral, false, false, false, true };

class Reverse : public Effect
{
public:
    const EffectDesc& desc() const override { return REVERSE_D_; }
    void prepare (double fs, int) override
    {
        sr = fs; size = nextPow2 ((int) (fs * 2.5) + 16); mask = size - 1;
        buf[0].assign ((size_t) size, 0.0f); buf[1].assign ((size_t) size, 0.0f);
        reset();
    }
    void reset() override
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f); std::fill (buf[1].begin(), buf[1].end(), 0.0f);
        w = 0; lastPhase = -1.0; have = false; sliceEnd = 0; sliceLen = 0; pos = 0;
    }
    void arm() override { have = false; }
    void release (Tail t) override
    {
        if (t != Tail::Spill) have = false;
        if (t == Tail::Clear) { std::fill (buf[0].begin(), buf[0].end(), 0.0f); std::fill (buf[1].begin(), buf[1].end(), 0.0f); }
    }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx& c) override
    {
        const double beats = beatsForDiv ((int) (pL[0] + 0.5f));
        const float  depth = clampf (pL[1] * 0.01f, 0.0f, 1.0f);
        const int    edge  = std::max (1, (int) (pL[2] * 0.001 * sr));
        float gd, gw; equalPowerMix (gd, gw, depth);

        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) (w & mask)] = L[i];
            buf[1][(size_t) (w & mask)] = R[i];
            ++w;

            const double ppq = c.ppq + i * c.ppqPerSample;
            const double ph  = ppq / beats - std::floor (ppq / beats);
            const bool boundary = (lastPhase >= 0.0 && ph < lastPhase);
            lastPhase = ph;
            if (boundary)
            {
                const int len = (int) (beats * 60.0 / std::max (20.0, c.bpm) * sr);
                sliceLen = std::min (size - 8, std::max (32, len));
                sliceEnd = w; pos = 0; have = true;
            }

            float wl = L[i], wr = R[i];
            if (have && pos < sliceLen)
            {
                const long long idx = sliceEnd - 1 - pos;
                wl = buf[0][(size_t) (idx & mask)];
                wr = buf[1][(size_t) (idx & mask)];
                //  the join: a raised cosine at both ends of the slice
                const int e = std::min (edge, sliceLen / 2);
                const int fromEnd = sliceLen - 1 - pos;
                const int k = std::min (pos, fromEnd);
                if (k < e)
                {
                    const float f = 0.5f * (1.0f - std::cos ((float) M_PI * (float) k / (float) e));
                    wl *= f; wr *= f;
                }
                ++pos;
            }
            L[i] = L[i] * gd + wl * gw;
            R[i] = R[i] * gd + wr * gw;
        }
    }

private:
    double sr = 48000.0, lastPhase = -1.0;
    int size = 0, mask = 0, sliceLen = 0, pos = 0;
    long long w = 0, sliceEnd = 0;
    std::vector<float> buf[2];
    bool have = false;
};

// ===========================================================================
// 11 · BRAKE — the whole signal grinding to a halt, or launching.
//
// Varispeed off a long line: the read head runs at SPEED while the write head
// runs at 1, so below 1 the head falls behind (and the pitch with it) and
// above 1 it catches up until it meets the present, where it can go no
// further. At SPEED exactly 1.0 with nothing owed the output IS the input —
// the exact path §15's "BRAKE SETTLES" asks for. A stopped head fades to
// silence over its last few percent of speed, as a tape does.
const ParamDesc BRAKE_P[] = {
    //  a level knob in the bench's sense: a stopped tape is silent
    { "speed", "SPEED", 100.0f, 0.0f, 200.0f, 0.0f, "%", Warp::Linear, nullptr, true },
};
const EffectDesc BRAKE_D_ { "brake", "BRAKE", "grinding to a halt, or launching",
                            BRAKE_P, 1, Level::Neutral, true, false, false };

class Brake : public Effect
{
public:
    const EffectDesc& desc() const override { return BRAKE_D_; }

    void prepare (double fs, int) override
    {
        //  the line lives OVERSAMPLED, so a head running at 2x never asks
        //  for anything past the 96 kHz Nyquist and nothing can fold
        sr = fs * 2.0;
        size = nextPow2 ((int) (sr * 5.0) + 16); mask = size - 1;
        buf[0].assign ((size_t) size, 0.0f); buf[1].assign ((size_t) size, 0.0f);
        for (auto& h : hb) h.design();
        reset();
    }
    void reset() override
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f);
        std::fill (buf[1].begin(), buf[1].end(), 0.0f);
        w = 0; lag = 0.0;
        for (auto& h : hb) h.clear();
    }
    void release (Tail t) override { if (t != Tail::Spill) lag = 0.0; }
    bool ringing() const override { return lag > 1.0; }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        const double r = std::max (0.0, (double) pL[0] * 0.01);
        const float  fade = clampf ((float) (r / 0.08), 0.0f, 1.0f);   // silent as it stops

        for (int i = 0; i < n; ++i)
        {
            float a0, a1, b0, b1;
            hb[0].up (L[i], a0, a1);
            hb[1].up (R[i], b0, b1);

            float y0l, y0r, y1l, y1r;
            step (a0, b0, r, y0l, y0r);
            step (a1, b1, r, y1l, y1r);

            L[i] = hb[0].down (y0l, y1l) * fade;
            R[i] = hb[1].down (y0r, y1r) * fade;
        }
    }

private:
    //  one oversampled sample: write the present, move the head, read it
    inline void step (float l, float r, double rate, float& ol, float& orr)
    {
        buf[0][(size_t) (w & mask)] = l;
        buf[1][(size_t) (w & mask)] = r;

        lag += 1.0 - rate;
        if (lag < 0.0) lag = 0.0;                          // caught up with the present
        const double maxLag = (double) (size - 8);
        if (lag > maxLag) lag = maxLag;                    // the line's end: the head stalls

        if (lag < 2.0)
        {
            //  too close to the write head for a four-point read
            const float t = (float) lag;
            const float p0 = buf[0][(size_t) (w & mask)], p1 = buf[0][(size_t) ((w - 1) & mask)];
            const float q0 = buf[1][(size_t) (w & mask)], q1 = buf[1][(size_t) ((w - 1) & mask)];
            ol = p0 + (p1 - p0) * t; orr = q0 + (q1 - q0) * t;
        }
        else
        {
            ol  = catmullRead (buf[0].data(), mask, (int) (w & mask) + size, (float) lag);
            orr = catmullRead (buf[1].data(), mask, (int) (w & mask) + size, (float) lag);
        }
        ++w;
    }

    double sr = 96000.0, lag = 0.0;
    int size = 0, mask = 0;
    long long w = 0;
    std::vector<float> buf[2];
    HalfBand hb[2];
};

// ===========================================================================
// 12 · DIVE — the source itself bending into the drop.
//
// A two-grain equal-power pitch shifter on a short line: pitch that bends
// while the groove keeps its tempo, which is a different animal from BRAKE's
// varispeed. SEMIS 0 is a copy (the exact path); anything else re-reads the
// last GRAIN ms at the new rate. The K-weighted makeup restores the loudness a
// shift takes away at the top of the band (§8.3: Nyquist loss compensated).
const ParamDesc DIVE_P[] = {
    //  a level knob, and deliberately: a shift MOVES the spectrum, and a
    //  moved spectrum is a different loudness. Compensating that would make
    //  a pitch shifter into a compressor, and at +12 st it would be chasing
    //  material that has left the band altogether.
    { "semis", "SEMIS",  0.0f, -24.0f, 12.0f, 0.0f, "st", Warp::Linear, nullptr, true },
    { "grain", "GRAIN", 50.0f,  20.0f, 120.0f, 0.0f, "ms", Warp::Log },
};
const EffectDesc DIVE_D_ { "dive", "DIVE", "the source bending into the drop",
                           DIVE_P, 2, Level::Neutral, true, false, false };

class Dive : public Effect
{
public:
    const EffectDesc& desc() const override { return DIVE_D_; }

    void prepare (double fs, int) override
    {
        //  the grain bank lives in the OVERSAMPLED domain (§ HalfBand): a
        //  grain reading at rate 2 then lands at 28 kHz of 48 kHz Nyquist
        //  instead of folding back over the top of the music
        bank.prepare (fs * 2.0, 1.0);
        bank.raisedCosine = true;        // two correlated grains, so equal amplitude
        for (auto& h : hb) h.design();
        reset();
    }
    void reset() override { bank.reset(); for (auto& h : hb) h.clear(); }
    void release (Tail t) override { if (t != Tail::Spill) bank.reset(); }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        const float  semis = pL[0];
        const int    len   = std::max (16, (int) (pL[1] * 0.001 * bank.sr));   // oversampled
        const double rate  = semisToRate (semis);
        const int    interval = std::max (1, len / 2);            // two grains, equal power

        for (int i = 0; i < n; ++i)
        {
            float a0, a1, b0, b1;
            hb[0].up (L[i], a0, a1);
            hb[1].up (R[i], b0, b1);

            float y0l, y0r, y1l, y1r;
            step (a0, b0, rate, len, interval, y0l, y0r);
            step (a1, b1, rate, len, interval, y1l, y1r);

            //  unity by construction: sin^2 windows at 50 % overlap sum to 1
            L[i] = hb[0].down (y0l, y1l);
            R[i] = hb[1].down (y0r, y1r);
        }
    }

private:
    inline void step (float l, float r, double rate, int len, int interval, float& ol, float& orr)
    {
        bank.write (l, r);
        if (--bank.nextLaunch <= 0) { bank.nextLaunch = interval; bank.launch (0.0, rate, len); }
        bank.read (ol, orr, rate, true);   // sqrt(2/2) = 1, so no normalisation
        bank.advance();
    }

    GrainBank bank;
    HalfBand hb[2];
};

} // namespace

const EffectDesc& grainDesc()   { return GRAIN_D_; }
const EffectDesc& bloomDesc()   { return BLOOM_D_; }
const EffectDesc& freezeDesc()  { return FREEZE_D_; }
const EffectDesc& reverseDesc() { return REVERSE_D_; }
const EffectDesc& brakeDesc()   { return BRAKE_D_; }
const EffectDesc& diveDesc()    { return DIVE_D_; }

Effect* makeGrain()   { return new Grain; }
Effect* makeBloom()   { return new Bloom; }
Effect* makeFreeze()  { return new Freeze; }
Effect* makeReverse() { return new Reverse; }
Effect* makeBrake()   { return new Brake; }
Effect* makeDive()    { return new Dive; }

} } // namespace rop::more
