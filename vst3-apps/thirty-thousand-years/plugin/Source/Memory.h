/*  Thirty Thousand Years — MEMORY: the world remembering itself.

    One global engine (not per voice): a granular reader and an STFT
    resynthesis over a SOURCE, which is one of four procedural factory sounds
    built at prepare, the CAPTURE ring, or an IMPORTED clip handed over by an
    atomic pointer swap. Everything under one EROSION.

    Real-time rules: the grain pool is fixed (96); the capture ring is
    preallocated (30 s mono); the snapshot of a stopped capture is copied on
    the MESSAGE thread, never here; an import is a pointer, never a copy.
*/
#pragma once

#include "Dsp.h"
#include "Params.h"

namespace tty
{

static constexpr int MEM_GRAINS   = 96;
static constexpr int MEM_FFT      = 2048;
static constexpr int MEM_HOP      = 512;
static constexpr float MEM_CAP_S  = 30.0f;

/*  An audio clip: mono floats at the engine's rate. Owned outside the engine
    (the processor keeps imports alive); the engine only ever reads through a
    pointer it was handed. */
struct Clip
{
    std::vector<float> data;
    double rate = 48000.0;
    const char* name = "";
};

struct Grain
{
    bool on = false;
    float pos = 0, inc = 1, len = 1, age = 0, amp = 0, gl = 0.7f, gr = 0.7f;
    int win = 0; bool rev = false;
    const float* buf = nullptr; int start = 0, span = 1;
};

struct MemoryEngine
{
    double sr = 48000.0;
    Rng rng;
    // sources
    std::vector<float> factory[4];
    std::vector<float> capRing; int capW = 0; bool capturing = false; float capLen = 0.0f;
    std::atomic<const Clip*> captured { nullptr };      // the snapshot the message thread made
    std::atomic<const Clip*> imported { nullptr };
    // grains
    Grain grains[MEM_GRAINS];
    float schedLeft = 0.0f; int clusterLeft = 0;
    // spectral
    Fft fft;
    std::vector<float> win, inRing, outRing, re, im, magS, phS, phRun, phPrev, magFrozen;
    int inW = 0, outR = 0, hopCount = 0; bool haveFrozen = false;
    float readHead = 0.0f;                             // the spectral read position, samples
    float dropGate = 1.0f; float dropLeft = 0.0f;
    OnePole eroLpL, eroLpR;
    Adsr aenv;
    DcBlock dcL, dcR;
    float lastMag[MEM_FFT / 2 + 1] = {};
    // status for the panel
    float uiActivity = 0.0f; int uiGrains = 0; int uiSrcLen = 0; int latencySamples = MEM_FFT;
    float uiSpectrum[64] = {};

    void prepare (double rate, uint32_t seed)
    {
        sr = rate; rng.seed (seed);
        buildFactory();
        capRing.assign ((size_t) (rate * MEM_CAP_S) + 16, 0.0f); capW = 0; capLen = 0.0f;
        fft.prepare (MEM_FFT);
        win.resize (MEM_FFT); for (int i = 0; i < MEM_FFT; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (TAU * i / MEM_FFT);
        inRing.assign (MEM_FFT, 0.0f); outRing.assign (MEM_FFT * 2, 0.0f);
        re.assign (MEM_FFT, 0.0f); im.assign (MEM_FFT, 0.0f);
        magS.assign (MEM_FFT / 2 + 1, 0.0f); phS.assign (MEM_FFT / 2 + 1, 0.0f); phRun.assign (MEM_FFT / 2 + 1, 0.0f);
        phPrev.assign (MEM_FFT / 2 + 1, 0.0f); magFrozen.assign (MEM_FFT / 2 + 1, 0.0f);
        eroLpL.setHz (20000.0f, sr); eroLpR.setHz (20000.0f, sr);
        dcL.setHz (8.0f, sr); dcR.setHz (8.0f, sr);
        reset();
    }

    void reset()
    {
        for (auto& g : grains) g.on = false;
        std::fill (inRing.begin(), inRing.end(), 0.0f); std::fill (outRing.begin(), outRing.end(), 0.0f);
        inW = outR = hopCount = 0; haveFrozen = false; schedLeft = 0.0f; clusterLeft = 0;
        aenv.kill(); dcL.reset(); dcR.reset(); dropGate = 1.0f; dropLeft = 0.0f;
        for (auto& v : phRun) v = 0.0f;
    }

    // ---- factory sources ----------------------------------------------------
    void buildFactory()
    {
        const int L = (int) (sr * 6.0);
        Rng r; r.seed (4242);
        for (int s = 0; s < 4; ++s) factory[s].assign ((size_t) L, 0.0f);
        // VOICE: glottal pulse through three formants sweeping a-o-u-e-i
        {
            Resonator f1, f2, f3; Biquad glot; glot.lowpass (500.0f, 0.7f, sr);
            float ph = 0.0f;
            static const float VOW[5][3] = { { 800, 1200, 2500 }, { 500, 900, 2400 }, { 350, 700, 2300 }, { 500, 1900, 2600 }, { 300, 2200, 3000 } };
            for (int i = 0; i < L; ++i)
            {
                const float t = (float) i / L;
                const float f0 = 105.0f * std::pow (2.0f, 0.12f * std::sin (t * 7.0f) + 0.05f * std::sin (t * 31.0f));
                const float vi = t * 4.0f; const int v0 = std::min (3, (int) vi); const float vt = vi - v0;
                if ((i & 255) == 0)
                {
                    f1.set (lerp (VOW[v0][0], VOW[v0 + 1][0], vt), 0.03f, sr);
                    f2.set (lerp (VOW[v0][1], VOW[v0 + 1][1], vt), 0.02f, sr);
                    f3.set (lerp (VOW[v0][2], VOW[v0 + 1][2], vt), 0.015f, sr);
                }
                ph += f0 / (float) sr; float pulse = 0.0f;
                if (ph >= 1.0f) { ph -= 1.0f; pulse = 1.0f; }
                const float g = glot (pulse * 8.0f) + 0.02f * r.bi();
                const float phrase = 0.5f - 0.5f * std::cos (TAU * t * 1.5f);
                factory[0][(size_t) i] = (f1 (g) * 1.0f + f2 (g) * 0.7f + f3 (g) * 0.35f) * 6.0f * phrase;
            }
        }
        // BROADCAST: a carrier with a repeated message, static, a sweep
        {
            Biquad band; band.bandpass (1800.0f, 1.2f, sr);
            OnePole stat; stat.setHz (3000.0f, sr);
            float ph = 0.0f, ph2 = 0.0f;
            for (int i = 0; i < L; ++i)
            {
                const float t = (float) i / L;
                const float msgT = std::fmod (t * 6.0f, 1.5f);                  // a 1.5 s message
                const int slot = (int) (msgT * 12.0f);
                const bool onBit = ((0b101101110010u >> (slot % 12)) & 1u) != 0;
                const float env = onBit ? 1.0f : 0.15f;
                ph += 1000.0f / (float) sr; ph2 += 1000.0f * 1.5f / (float) sr;
                float tone = env * (0.6f * fsin (ph) + 0.25f * fsin (ph2));
                tone += 0.35f * stat.lp (r.bi());
                if (t > 0.7f) tone += 0.3f * fsin (ph * (1.0f + 3.0f * (t - 0.7f)));
                factory[1][(size_t) i] = band (tone) * 2.0f + 0.05f * r.bi();
            }
        }
        // CHOIR: five formant voices on a minor chord
        {
            struct V { Resonator f1, f2; float ph = 0, f0 = 110; };
            V vs[5]; const float notes[5] = { 0, 3, 7, 12, 15 };
            for (int k = 0; k < 5; ++k) { vs[k].f0 = 110.0f * std::pow (2.0f, notes[k] / 12.0f) * (1.0f + 0.004f * r.bi()); }
            Biquad glot; glot.lowpass (600.0f, 0.7f, sr);
            for (int i = 0; i < L; ++i)
            {
                const float t = (float) i / L;
                if ((i & 511) == 0) for (int k = 0; k < 5; ++k) { vs[k].f1.set (lerp (450.0f, 750.0f, t) * (1 + 0.02f * k), 0.03f, sr); vs[k].f2.set (lerp (800.0f, 1150.0f, t), 0.02f, sr); }
                float acc = 0.0f;
                for (int k = 0; k < 5; ++k)
                {
                    vs[k].ph += vs[k].f0 * (1.0f + 0.003f * std::sin (t * 40.0f + k)) / (float) sr;
                    float pulse = 0.0f; if (vs[k].ph >= 1.0f) { vs[k].ph -= 1.0f; pulse = 1.0f; }
                    const float g = glot (pulse * 5.0f);
                    acc += vs[k].f1 (g) + 0.6f * vs[k].f2 (g);
                }
                const float phrase = std::sin (PI * t);
                factory[2][(size_t) i] = acc * 1.6f * phrase;
            }
        }
        // MACHINE: a motor hum and a 1.2 s cycle of impacts on metallic bodies
        {
            Resonator body[5]; const float rat[5] = { 1.0f, 2.41f, 3.98f, 5.36f, 7.7f };
            for (int k = 0; k < 5; ++k) body[k].set (320.0f * rat[k], 0.5f / (1 + k * 0.5f), sr);
            float ph = 0.0f;
            for (int i = 0; i < L; ++i)
            {
                const float t = (float) i / L;
                const float cyc = std::fmod (t * 5.0f, 1.0f);
                float hit = 0.0f;
                if (cyc < 0.002f || (cyc > 0.5f && cyc < 0.502f) || (cyc > 0.75f && cyc < 0.7505f)) hit = r.bi() * 3.0f;
                float acc = 0.0f; for (int k = 0; k < 5; ++k) acc += body[k](hit) * (1.0f / (1 + k));
                ph += 48.0f / (float) sr;
                const float hum = 0.35f * fsin (ph) + 0.18f * fsin (ph * 2.0f) + 0.1f * fsin (ph * 3.0f) + 0.06f * fsin (ph * 5.0f);
                factory[3][(size_t) i] = acc * 2.5f + hum * (0.9f + 0.1f * std::sin (t * 20.0f)) + 0.03f * r.bi();
            }
        }
        for (int s = 0; s < 4; ++s)
        {
            float pk = 1.0e-6f; for (float v : factory[s]) pk = std::max (pk, std::abs (v));
            for (float& v : factory[s]) v *= 0.7f / pk;
        }
    }

    // ---- capture ------------------------------------------------------------
    inline void captureWrite (float mono)
    {
        if (! capturing) return;
        capRing[(size_t) capW] = mono; capW = (capW + 1) % (int) capRing.size();
        capLen = std::min ((float) capRing.size(), capLen + 1.0f);
    }
    void setCapturing (bool on) { if (on && ! capturing) { capW = 0; capLen = 0.0f; } capturing = on; }

    // ---- the current source -------------------------------------------------
    bool source (int sel, const float*& buf, int& len) const
    {
        if (sel >= 0 && sel < 4) { buf = factory[sel].data(); len = (int) factory[sel].size(); return len > 0; }
        if (sel == 4)
        {
            if (const Clip* c = captured.load (std::memory_order_acquire)) if (! c->data.empty()) { buf = c->data.data(); len = (int) c->data.size(); return true; }
            if (capLen > 1000.0f) { buf = capRing.data(); len = (int) capLen; return true; }   // the live ring while nothing is snapshotted
            return false;
        }
        if (const Clip* c = imported.load (std::memory_order_acquire)) if (! c->data.empty()) { buf = c->data.data(); len = (int) c->data.size(); return true; }
        return false;
    }

    static inline float window (int kind, float t)
    {
        switch (kind)
        {
            case 1:  return t < 0.25f ? 0.5f - 0.5f * std::cos (TAU * t * 2.0f) : (t > 0.75f ? 0.5f - 0.5f * std::cos (TAU * (1.0f - t) * 2.0f) : 1.0f);   // tukey
            case 2:  return 1.0f - std::abs (2.0f * t - 1.0f);
            case 3:  return std::exp (-6.0f * t) * std::min (1.0f, t * 40.0f);
            case 4:  return std::exp (-6.0f * (1.0f - t)) * std::min (1.0f, (1.0f - t) * 40.0f);
            case 5:  return t < 0.1f ? 0.5f - 0.5f * std::cos (PI * t * 10.0f) : (t > 0.9f ? 0.5f - 0.5f * std::cos (PI * (1.0f - t) * 10.0f) : 1.0f);
            default: return 0.5f - 0.5f * std::cos (TAU * t);
        }
    }

    void spawn (const float* buf, int start, int span, float centre, float inc, float lenSamp, const Params& p, float erosion)
    {
        Grain* g = nullptr; float quiet = 1.0e9f;
        for (auto& c : grains) if (! c.on) { g = &c; break; }
        if (g == nullptr) { for (auto& c : grains) if (c.amp < quiet) { quiet = c.amp; g = &c; } }   // overload: steal the quietest
        if (g == nullptr) return;
        const float jit = p[P_mem_jit] * 0.5f * (float) span * rng.bi();
        g->buf = buf; g->start = start; g->span = std::max (1, span);
        g->pos = centre + jit;
        const float cents = paramSpec (P_mem_pspread).lo * p[P_mem_pspread] * rng.bi();
        g->inc = inc * fexp2 (cents / 1200.0f);
        g->rev = rng.uni() < p[P_mem_rev];
        if (g->rev) g->inc = -g->inc;
        g->len = std::max (8.0f, lenSamp * (1.0f - 0.9f * erosion * p[P_mem_ero_frag] * rng.uni()));
        g->age = 0.0f; g->win = p.li (P_mem_win);
        const float pan = 0.5f + (rng.uni() - 0.5f) * p[P_mem_scatter];
        g->gl = std::cos (pan * PI * 0.5f); g->gr = std::sin (pan * PI * 0.5f);
        g->amp = 0.7f + 0.3f * rng.uni();
        g->on = true;
    }

    inline float readSrc (const Grain& g, float pos) const
    {
        // wrap into [start, start+span)
        float rel = pos - (float) g.start;
        rel -= std::floor (rel / (float) g.span) * (float) g.span;
        const float ap = (float) g.start + rel;
        const int i0 = (int) ap; const float f = ap - (float) i0;
        const int i1 = i0 + 1 >= g.start + g.span ? g.start : i0 + 1;
        return lerp (g.buf[i0], g.buf[i1], f);
    }

    /*  Render n stereo samples (ADDS into L/R). keyRatio is the pitch factor
        from the keyboard; gate is whether anything is held. loopIn is the
        feedback-loop return when it is routed here (recorded into the capture
        ring when capturing). */
    void render (float* L, float* R, int n, const Params& p, float keyRatio, bool gate, float extAct, const float* extMono, const float* loopIn)
    {
        aenv.set (lawHz (paramSpec (P_mem_atk), p[P_mem_atk]), 5.0f, 1.0f, lawHz (paramSpec (P_mem_rel), p[P_mem_rel]), sr);
        if (gate && ! aenv.active()) aenv.on (1.0f);
        if (! gate && aenv.active() && aenv.st != Adsr::REL) aenv.off();

        // capture input
        const int capSrc = p.li (P_mem_capsrc);
        if (capturing)
            for (int i = 0; i < n; ++i)
                captureWrite (capSrc == 2 ? (extMono ? extMono[i] : 0.0f) : (loopIn ? loopIn[i] : 0.0f));

        const float* buf = nullptr; int len = 0;
        const bool have = source (p.li (P_mem_src), buf, len) && len > 64;
        uiSrcLen = have ? len : 0;
        if (! have) { uiGrains = 0; uiActivity *= 0.9f; return; }

        const int mode = p.li (P_mem_mode);
        const float erosion = p[P_mem_erosion];
        const float region = clampf (p[P_mem_region], 0.02f, 1.0f);
        const int span = std::max (64, (int) (region * len));
        const int start = clampi ((int) (p[P_mem_pos] * (len - span)), 0, std::max (0, len - span));
        const float scan = (p[P_mem_scan] - 0.5f) * 4.0f;   // -2..2 x
        const float pitch = lawSemi (paramSpec (P_mem_pitch), p[P_mem_pitch]) + lawSemi (paramSpec (P_mem_pfine), p[P_mem_pfine]) / 100.0f;
        const float inc = fexp2 (pitch / 12.0f) * (p.sw (P_mem_keyfollow) ? keyRatio : 1.0f);

        // the read head walks the region at SCAN speed; grains are born around it
        readHead += scan * (float) n;
        readHead -= std::floor (readHead / (float) span) * (float) span;

        // erosion components
        const float eBw = erosion * p[P_mem_ero_bw], eDrop = erosion * p[P_mem_ero_drop], eSimp = erosion * p[P_mem_ero_simp];
        eroLpL.setHz (xmap (1.0f - eBw, 300.0f, 20000.0f), sr); eroLpR.a = eroLpL.a;

        // dropouts: a gate that closes at random for 20-200 ms
        if (dropLeft > 0.0f) { dropLeft -= (float) n; if (dropLeft <= 0.0f) dropGate = 1.0f; }
        else if (eDrop > 0.0f && rng.uni() < eDrop * 0.08f) { dropGate = 0.0f; dropLeft = (0.02f + 0.18f * rng.uni()) * (float) sr; }

        float tmpL[CTRL_MAX], tmpR[CTRL_MAX];
        std::fill (tmpL, tmpL + n, 0.0f); std::fill (tmpR, tmpR + n, 0.0f);

        // ---- grains
        if (mode != 1)
        {
            const float dens = lawHz (paramSpec (P_mem_dens), p[P_mem_dens]);
            const float lenSamp = lawHz (paramSpec (P_mem_dur), p[P_mem_dur]) * 0.001f * (float) sr;
            const int sched = p.li (P_mem_sched);
            for (int i = 0; i < n; ++i)
            {
                schedLeft -= 1.0f;
                if (schedLeft <= 0.0f)
                {
                    spawn (buf, start, span, (float) start + readHead, inc, lenSamp, p, erosion);
                    const float base = (float) sr / std::max (0.5f, dens);
                    if (sched == 0) schedLeft = base;
                    else if (sched == 1) schedLeft = base * std::max (0.1f, -std::log (std::max (1.0e-4f, rng.uni())));
                    else
                    {
                        if (clusterLeft > 0) { --clusterLeft; schedLeft = base * 0.12f * (0.5f + rng.uni()); }
                        else { clusterLeft = 2 + (int) (rng.uni() * 6.0f); schedLeft = base * 4.0f * (0.5f + rng.uni()); }
                    }
                    // fragmentation: gaps in the schedule
                    if (erosion * p[P_mem_ero_frag] > 0.0f && rng.uni() < erosion * p[P_mem_ero_frag] * 0.7f) schedLeft *= 3.0f;
                }
            }
            int live = 0;
            for (auto& g : grains)
            {
                if (! g.on) continue;
                ++live;
                for (int i = 0; i < n; ++i)
                {
                    const float t = g.age / g.len;
                    if (t >= 1.0f) { g.on = false; break; }
                    const float w = window (g.win, t) * g.amp;
                    const float s = readSrc (g, g.pos) * w;
                    tmpL[i] += s * g.gl; tmpR[i] += s * g.gr;
                    g.pos += g.inc; g.age += 1.0f;
                }
            }
            uiGrains = live;
            // overlap normalisation: below one grain on average nothing is taken away; above, by the root of the overlap
            const float overlap = dens * lenSamp / (float) sr;
            const float norm = 1.8f / std::sqrt (std::max (1.0f, overlap));
            for (int i = 0; i < n; ++i) { tmpL[i] *= norm; tmpR[i] *= norm; }
        }
        else uiGrains = 0;

        // ---- spectral
        if (mode != 0)
        {
            float specTmp[CTRL_MAX];
            spectral (specTmp, n, buf, start, span, inc, p, eSimp);
            const float g = mode == 2 ? 0.7f : 1.0f;
            for (int i = 0; i < n; ++i) { tmpL[i] += specTmp[i] * g; tmpR[i] += specTmp[i] * g; }
        }

        float act = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float ae = aenv.tick() * dropGate;
            float l = eroLpL.lp (tmpL[i]) * ae, r = eroLpR.lp (tmpR[i]) * ae;
            l = dcL (l); r = dcR (r);
            L[i] += l; R[i] += r;
            act += l * l + r * r;
        }
        uiActivity = 0.9f * uiActivity + 0.1f * std::sqrt (act / n);
        (void) extAct;
    }

    static constexpr int CTRL_MAX = 64;

    /*  STFT path: the read head advances at pitch through the region; each hop
        a frame is analysed, transformed and overlap-added. Output is delayed by
        one FFT length; the panel shows it as latencySamples. */
    void spectral (float* out, int n, const float* buf, int start, int span, float inc, const Params& p, float eSimp)
    {
        const bool freeze = p.sw (P_mem_freeze);
        const float smear = p[P_mem_smear], tilt = (p[P_mem_stilt] - 0.5f) * 2.0f, thin = clamp01 (p[P_mem_thin] + eSimp), evolve = p[P_mem_evolve];
        const float disp = lawShz (paramSpec (P_mem_disp), p[P_mem_disp]);
        const float binHz = (float) sr / MEM_FFT;
        const int shiftBins = (int) std::lround (disp / binHz);
        const int NB = MEM_FFT / 2;
        for (int i = 0; i < n; ++i)
        {
            // feed
            float rel = readHead * 0.0f;   // the spectral head is its own: pos below
            (void) rel;
            specPos += inc; specPos -= std::floor (specPos / (float) span) * (float) span;
            const float ap = (float) start + specPos; const int i0 = (int) ap; const float f = ap - i0;
            const int i1 = i0 + 1 >= start + span ? start : i0 + 1;
            inRing[(size_t) inW] = lerp (buf[i0], buf[i1], f); inW = (inW + 1) % MEM_FFT;
            out[i] = outRing[(size_t) outR] / 1.5f;         // WOLA gain of Hann^2 at 75 % overlap
            outRing[(size_t) outR] = 0.0f; outR = (outR + 1) % (MEM_FFT * 2);
            if (++hopCount < MEM_HOP) continue;
            hopCount = 0;
            // analyse the last MEM_FFT samples
            for (int k = 0; k < MEM_FFT; ++k) { re[(size_t) k] = inRing[(size_t) ((inW + k) % MEM_FFT)] * win[(size_t) k]; im[(size_t) k] = 0.0f; }
            fft.forward (re.data(), im.data());
            float pk = 1.0e-6f;
            for (int k = 0; k <= NB; ++k)
            {
                float mag = std::sqrt (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]);
                const float ph = std::atan2 (im[(size_t) k], re[(size_t) k]);
                if (freeze && haveFrozen) mag = magFrozen[(size_t) k];
                else if (freeze) magFrozen[(size_t) k] = mag;      // the frame being frozen; taken only if it carries energy (below)
                const float sm = smear * 0.97f;
                magS[(size_t) k] = magS[(size_t) k] * sm + mag * (1.0f - sm);
                // phase: analysed advance (evolve 1) vs running synthetic phase (evolve 0)
                float dph = ph - phPrev[(size_t) k]; phPrev[(size_t) k] = ph;
                const float expected = TAU * (float) k * MEM_HOP / MEM_FFT;
                dph -= expected; dph -= TAU * std::floor ((dph + PI) / TAU); dph += expected;
                phRun[(size_t) k] += lerp (expected, dph, freeze ? 0.0f : evolve);
                phRun[(size_t) k] -= TAU * std::floor (phRun[(size_t) k] / TAU);
                pk = std::max (pk, magS[(size_t) k]);
            }
            // freeze the FIRST frame that carries energy: an empty ring at start would otherwise freeze silence
            if (freeze) { if (! haveFrozen && pk > 2.0f) haveFrozen = true; } else haveFrozen = false;
            // shape and re-place
            std::fill (re.begin(), re.end(), 0.0f); std::fill (im.begin(), im.end(), 0.0f);
            const float thr = thin * thin * pk * 0.6f;
            for (int k = 1; k < NB; ++k)
            {
                float mag = magS[(size_t) k];
                if (mag < thr) continue;
                mag *= std::pow ((float) k / 32.0f, tilt * 0.8f);
                const int kd = k + shiftBins;
                if (kd < 1 || kd >= NB) continue;
                const float phv = phRun[(size_t) k];
                re[(size_t) kd] += mag * std::cos (phv); im[(size_t) kd] += mag * std::sin (phv);
                re[(size_t) (MEM_FFT - kd)] += mag * std::cos (phv); im[(size_t) (MEM_FFT - kd)] -= mag * std::sin (phv);
                if ((k & 31) == 0) uiSpectrum[std::min (63, k / 32)] = mag / pk;
            }
            fft.inverse (re.data(), im.data());
            for (int k = 0; k < MEM_FFT; ++k)
                outRing[(size_t) ((outR + k) % (MEM_FFT * 2))] += re[(size_t) k] * win[(size_t) k];
        }
    }
    float specPos = 0.0f;
};

} // namespace tty
