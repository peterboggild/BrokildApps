/*  Thirty Thousand Years — the per-voice strata: MASS, SIGNAL, STRUCTURE.

    Each voice owns one of each. They render CTRL samples at a time into mono
    scratch buffers (so STRUCTURE can be excited by MASS or SIGNAL of the same
    voice) and the Engine pans them. All parameters arrive as EFFECTIVE values
    (base + modulation) through a const Params&.
*/
#pragma once

#include "Dsp.h"
#include "Params.h"

namespace tty
{

static constexpr int CTRL       = 32;    // control-rate tick, samples
static constexpr int MAX_VOICES = 8;
static constexpr int MAX_MODES  = 48;
static constexpr int MAX_PART   = 64;
static constexpr int WT_LEN     = 2048;  // samples per frame
static constexpr int WT_FRAMES  = 8;     // frames per table
static constexpr int WT_TABLES  = 8;
static constexpr int WT_MIPS    = 9;     // one per octave from 2048 harmonics down to 4

/*  Per-voice expression handed down by the Engine each tick. */
struct VoiceCtl
{
    float hz = 110.0f;        // the note, after glide, tune, bend, scale
    float vel = 0.8f;         // 0..1
    float gate = 0.0f;        // smoothed gate 0..1 (for SPECTRA sag)
    float press = 0.0f;       // aftertouch / MPE pressure
    float fan = 0.0f;         // -1..1, this voice's place in a fan
    float wmDet = 0.0f;       // world-mod detune cents (already fanned)
    float wmPan = 0.0f;       // world-mod pan offset
    float wmTrem = 1.0f;      // world-mod tremolo gain
    float wmSag = 0.0f;       // semitones down
    float wmFilt = 1.0f;      // cutoff multiplier
    bool  retrig = false;     // a fresh strike this tick
};

//==============================================================================
/*  The eight wavetables, built once at prepare. Each table has WT_FRAMES
    frames; each frame has WT_MIPS band-limited copies made by keeping the
    first N harmonics of the frame's FFT (N halves per level). A read picks
    the level whose top harmonic stays under Nyquist and lerps between the
    two nearest levels, so a sweep across pitch has no steps. */
struct WaveSet
{
    // [table][frame][mip][WT_LEN]
    std::vector<float> data;
    Fft fft;
    bool built = false;

    inline const float* frame (int t, int f, int mip) const
    {
        return &data[(size_t) (((t * WT_FRAMES + f) * WT_MIPS + mip) * WT_LEN)];
    }

    static float shape (int table, int f, float x, Rng& r)   // x in 0..1 cycles, f frame 0..7
    {
        const float m = (float) f / (WT_FRAMES - 1);
        const float ph = x * TAU;
        switch (table)
        {
            case 0:   // GEOMETRIC: sine -> triangle -> square -> saw across the frames
            {
                const float s = std::sin (ph), tr = 1.0f - 4.0f * std::abs (x - 0.5f), sq = x < 0.5f ? 1.0f : -1.0f, sw = 2.0f * x - 1.0f;
                if (m < 0.33f) return lerp (s, tr, m / 0.33f);
                if (m < 0.66f) return lerp (tr, sq, (m - 0.33f) / 0.33f);
                return lerp (sq, sw, (m - 0.66f) / 0.34f);
            }
            case 1:   // MACHINE: asymmetric cycles, a duty that hardens and a ramp that steps
            {
                const float duty = 0.5f - 0.42f * m;
                float v = x < duty ? std::pow (x / duty, 0.6f) * 2.0f - 1.0f : 1.0f - 2.0f * std::pow ((x - duty) / (1.0f - duty), 1.0f + 3.0f * m);
                v += 0.25f * m * std::sin (ph * 7.0f) * (x < duty ? 1.0f : 0.2f);
                return v;
            }
            case 2:   // FORMANT: two resonant peaks that move apart with the frame
            {
                const float f1 = 2.0f + 3.0f * m, f2 = 5.0f + 9.0f * m;
                float v = 0.0f;
                for (int k = 1; k <= 24; ++k)
                {
                    const float a = std::exp (-0.5f * std::pow ((k - f1) / 1.2f, 2.0f)) + 0.6f * std::exp (-0.5f * std::pow ((k - f2) / 1.8f, 2.0f));
                    v += a * std::sin (ph * k) / std::sqrt ((float) k);
                }
                return v;
            }
            case 3:   // HOLLOW: odd harmonics only, then every third missing, then a hollow comb
            {
                float v = 0.0f;
                for (int k = 1; k <= 40; ++k)
                {
                    float a = (k & 1) ? 1.0f / k : (0.25f * (1.0f - m)) / k;
                    if (m > 0.5f && (k % 3) == 0) a *= 1.0f - (m - 0.5f) * 2.0f;
                    v += a * std::sin (ph * k);
                }
                return v;
            }
            case 4:   // STEPPED: a quantised ramp whose step count falls across the frames
            {
                const int steps = 32 >> (int) (m * 4.0f);
                const float q = std::floor (x * steps) / std::max (1, steps - 1);
                return 2.0f * q - 1.0f;
            }
            case 5:   // METALLIC: inharmonic partials with a stretch that grows with the frame
            {
                float v = 0.0f;
                for (int k = 1; k <= 16; ++k)
                {
                    const float rk = std::pow ((float) k, 1.0f + 0.35f * m);
                    v += std::sin (ph * rk + (float) k * 0.7f) / ((float) k * 0.8f + 0.2f);
                }
                return v;
            }
            case 6:   // SIREN: a pulse whose width sweeps, and a second pulse a fifth up bleeding in
            {
                const float pw = 0.1f + 0.4f * m;
                const float a = x < pw ? 1.0f : -1.0f;
                const float x2 = std::fmod (x * 1.5f, 1.0f);
                const float b = x2 < pw ? 1.0f : -1.0f;
                return 0.75f * a + 0.35f * m * b + 0.15f * std::sin (ph * 3.0f);
            }
            default:  // ORGANISM: a sum of low harmonics with slowly rotating phases, noise-jittered per frame
            {
                float v = 0.0f;
                for (int k = 1; k <= 12; ++k)
                {
                    const float phk = hash01 ((uint32_t) k, (uint32_t) f, 77u) * TAU;
                    const float a = 1.0f / (1.0f + 0.5f * k) * (0.5f + 0.5f * std::sin (0.9f * k + 4.0f * m));
                    v += a * std::sin (ph * k + phk * m);
                }
                (void) r;
                return v;
            }
        }
    }

    void build()
    {
        data.assign ((size_t) WT_TABLES * WT_FRAMES * WT_MIPS * WT_LEN, 0.0f);
        fft.prepare (WT_LEN);
        std::vector<float> re (WT_LEN), im (WT_LEN), re2 (WT_LEN), im2 (WT_LEN);
        Rng r; r.seed (1234);
        for (int t = 0; t < WT_TABLES; ++t)
            for (int f = 0; f < WT_FRAMES; ++f)
            {
                for (int i = 0; i < WT_LEN; ++i) { re[(size_t) i] = shape (t, f, (float) i / WT_LEN, r); im[(size_t) i] = 0.0f; }
                // remove DC, normalise the frame to unit peak
                float mean = 0.0f; for (float v : re) mean += v; mean /= WT_LEN;
                float pk = 1.0e-6f; for (float& v : re) { v -= mean; pk = std::max (pk, std::abs (v)); }
                for (float& v : re) v /= pk;
                fft.forward (re.data(), im.data());
                for (int mip = 0; mip < WT_MIPS; ++mip)
                {
                    const int keep = std::max (1, (WT_LEN / 2) >> (mip + 1));   // mip 0 keeps 512 harmonics
                    for (int i = 0; i < WT_LEN; ++i)
                    {
                        const int k = i <= WT_LEN / 2 ? i : WT_LEN - i;
                        const bool ok = k >= 1 && k <= keep;
                        re2[(size_t) i] = ok ? re[(size_t) i] : 0.0f;
                        im2[(size_t) i] = ok ? im[(size_t) i] : 0.0f;
                    }
                    fft.inverse (re2.data(), im2.data());
                    float* dst = &data[(size_t) (((t * WT_FRAMES + f) * WT_MIPS + mip) * WT_LEN)];
                    for (int i = 0; i < WT_LEN; ++i) dst[i] = re2[(size_t) i];
                }
            }
        built = true;
    }

    /*  Read at phase ph (cycles) with frame position pos (0..1) at an increment
        inc (cycles/sample). Chooses the mip whose 'keep' harmonics stay under
        Nyquist: harmonics allowed = 0.5/inc. */
    inline float read (int table, float pos, float ph, float inc) const
    {
        if (! built) return 0.0f;
        const float allowed = 0.45f / std::max (1.0e-6f, inc);
        // mip m keeps 512 >> m harmonics; want keep <= allowed
        float mf = std::log2 (512.0f / std::max (1.0f, allowed));
        mf = clampf (mf, 0.0f, (float) (WT_MIPS - 1));
        const int m0 = (int) mf; const int m1 = std::min (WT_MIPS - 1, m0 + 1); const float mt = mf - (float) m0;
        const float fp = clamp01 (pos) * (WT_FRAMES - 1);
        const int f0 = (int) fp; const int f1 = std::min (WT_FRAMES - 1, f0 + 1); const float ft = fp - (float) f0;
        const float x = (ph - std::floor (ph)) * WT_LEN;
        const int i0 = (int) x; const int i1 = (i0 + 1) & (WT_LEN - 1); const float xt = x - (float) i0;
        auto rd = [&] (int f, int m) { const float* d = frame (table, f, m); return lerp (d[i0], d[i1], xt); };
        const float a = lerp (rd (f0, m0), rd (f1, m0), ft);
        const float b = lerp (rd (f0, m1), rd (f1, m1), ft);
        return lerp (a, b, mt);
    }
};

//==============================================================================
struct FilterPair   // ladder or SVF, one selector
{
    /*  THE FILTER'S OPERATING LEVEL. The oscillators feed it at IN_SCALE and
        the makeup comes back after, the way a desk stages gain; since the
        filter is linear at rest this is exactly transparent. Without it, two
        oscillators at full summed to 1.6 and drove the saturator at DRIVE
        ZERO - measured 0.6 % distortion on what should be a clean patch. */
    static constexpr float IN_SCALE = 0.55f;
    static constexpr float MAKEUP = 1.0f / IN_SCALE;
    Ladder ladder; Svf svf; int mode = 0; float sat = 1.0f;
    void set (int m, float hz, float res, float drive, double sr)
    {
        mode = m;
        if (m == 0) ladder.set (hz, res, sr);
        else        svf.set (hz, 0.5f + res * res * 30.0f, sr);
        /*  TRANSPARENT AT REST. ftanh(u*sat)/sat is the identity for small sat;
            the old 0.6 floor meant the filter distorted a plain sine by 1.8 %
            with DRIVE at zero, which is what "clipping although the output is
            below 0 dB" sounded like. 0.14 measures -75 dB on the same sine,
            and the top of the knob is dirtier than it was. */
        sat = 0.02f + 5.5f * drive * drive;
    }
    inline float operator() (float x)
    {
        x *= IN_SCALE;
        return (mode == 0 ? ladder (x, sat) : svf (x, mode - 1, sat)) * MAKEUP;
    }
    void reset() { ladder.reset(); svf.reset(); }
    void clean() { ladder.cleanState(); svf.cleanState(); }
};

//==============================================================================
struct MassVoice
{
    Blep o1[3], o2[3], sub;
    OuWalk drift1, drift2, pwWalk, ampWalk;
    Rng rng;
    FilterPair filt;
    Adsr fenv, aenv;
    DcBlock dc;
    float beatPh = 0.0f;
    double sr = 48000.0;

    void prepare (double rate, uint32_t seed)
    {
        sr = rate; rng.seed (seed);
        for (int i = 0; i < 3; ++i) { o1[i].reset (rng.uni()); o2[i].reset (rng.uni()); }
        sub.reset(); dc.setHz (8.0f, sr); reset();
    }
    void reset() { filt.reset(); fenv.kill(); aenv.kill(); drift1.reset(); drift2.reset(); pwWalk.reset(); ampWalk.reset(); dc.reset(); }
    void noteOn (float vel, bool legato, const Params& p)
    {
        if (! legato || ! aenv.active())
        {
            fenv.on (vel); aenv.on (vel);
            if (p.li (P_m_o1wave) >= 0) {}
        }
        else { fenv.on (vel); }
    }
    void noteOff() { fenv.off(); aenv.off(); }
    bool active() const { return aenv.active(); }

    /*  Renders n mono samples into out (overwrites). */
    void render (float* out, int n, const VoiceCtl& c, const Params& p, float subMono[])
    {
        fenv.set (lawHz (paramSpec (P_m_f_atk), p[P_m_f_atk]), lawHz (paramSpec (P_m_f_dec), p[P_m_f_dec]), p[P_m_f_sus], lawHz (paramSpec (P_m_f_rel), p[P_m_f_rel]), sr);
        aenv.set (lawHz (paramSpec (P_m_a_atk), p[P_m_a_atk]), lawHz (paramSpec (P_m_a_dec), p[P_m_a_dec]), p[P_m_a_sus], lawHz (paramSpec (P_m_a_rel), p[P_m_a_rel]), sr);

        const float dt = (float) n / (float) sr;
        const float driftC = paramSpec (P_m_drift).lo * p[P_m_drift];
        const float driftT = lawHz (paramSpec (P_m_drifttime), p[P_m_drifttime]);
        const float d1 = drift1.tick (rng, driftT, driftC, dt), d2 = drift2.tick (rng, driftT, driftC, dt);
        const float pwd = pwWalk.tick (rng, driftT, 0.2f * p[P_m_pwdrift], dt);
        const float amd = 1.0f + ampWalk.tick (rng, driftT, 0.3f * p[P_m_ampvar], dt);

        const float base = c.hz * fexp2 ((c.wmDet - c.wmSag * 100.0f * (1.0f - c.gate)) / 1200.0f);
        const float oct1 = (float) (p.li (P_m_o1oct) - 2), oct2 = (float) (p.li (P_m_o2oct) - 2);
        const float f1 = base * fexp2 (oct1 + (lawSemi (paramSpec (P_m_o1semi), p[P_m_o1semi]) + (lawSemi (paramSpec (P_m_o1fine), p[P_m_o1fine]) + d1) / 100.0f) / 12.0f);
        float f2 = base * fexp2 (oct2 + (lawSemi (paramSpec (P_m_o2semi), p[P_m_o2semi]) + (lawSemi (paramSpec (P_m_o2fine), p[P_m_o2fine]) + d2) / 100.0f) / 12.0f);
        const float beat = p[P_m_beat] < 0.005f ? 0.0f : lawHz (paramSpec (P_m_beat), p[P_m_beat]);
        f2 += beat;   // slow beating in HERTZ, whatever the note

        const int uni = p.li (P_m_unison) + 1;
        const float udet = paramSpec (P_m_unidet).lo * p[P_m_unidet];
        const int w1 = p.li (P_m_o1wave), w2 = p.li (P_m_o2wave);
        const float pw1 = clampf (0.5f + 0.45f * p[P_m_o1pw] + pwd, 0.05f, 0.95f);
        const float pw2 = clampf (0.5f + 0.45f * p[P_m_o2pw] + pwd, 0.05f, 0.95f);
        const float l1 = p[P_m_o1lvl], l2 = p[P_m_o2lvl];
        const bool sync = p.sw (P_m_sync);
        const int subMode = p.li (P_m_sub);
        const float subLvl = p[P_m_sublvl];
        const int subWave = p.li (P_m_subwave);
        const float reinf = p[P_m_reinf];

        for (int u = 0; u < uni; ++u)
        {
            const float off = uni == 1 ? 0.0f : (u == 0 ? 0.0f : (u == 1 ? -udet : udet));
            o1[u].setHz (f1 * fexp2 (off / 1200.0f), sr);
            o2[u].setHz (f2 * fexp2 (off / 1200.0f), sr);
        }
        sub.setHz (subMode == 0 ? f1 : f1 * (subMode == 1 ? 0.5f : 0.25f), sr);

        // filter
        const int fm = p.li (P_m_fmode);
        const float ktrack = std::pow (c.hz / 261.6f, p[P_m_ftrack]);
        const float fe = fenv.tick();
        const float fenvAmt = (p[P_m_fenv] - 0.5f) * 2.0f;
        float cut = lawHz (paramSpec (P_m_cut), p[P_m_cut]) * ktrack * fexp2 (fenvAmt * fe * 6.0f) * c.wmFilt;
        cut *= fexp2 (c.press * 2.0f);
        filt.set (fm, clampf (cut, 20.0f, (float) sr * 0.45f), p[P_m_res], p[P_m_fdrive], sr);

        const float uniNorm = uni == 1 ? 1.0f : 0.6f;
        /*  The voice's own level, chosen so a full chord lands near unity on the
            stratum bus and the channel fader works around unity rather than
            undoing the voice. */
        const float aeGain = 0.58f * (0.35f + 0.65f * c.vel) * amd;
        for (int i = 0; i < n; ++i)
        {
            float s1 = 0.0f, s2 = 0.0f;
            for (int u = 0; u < uni; ++u)
            {
                bool wr1 = false, wr2 = false;
                s1 += o1[u].tick (w1, pw1, wr1);
                if (sync && wr1) o2[u].reset (0.0f);
                s2 += o2[u].tick (w2, pw2, wr2);
            }
            bool wrs = false;
            float sb = subMode == 0 ? 0.0f : sub.tick (subWave == 0 ? 0 : 3, 0.5f, wrs);
            if (subMode != 0 && reinf > 0.0f)
            {
                // harmonic reinforcement: the sub's 2nd and 3rd harmonic from its own phase
                const float ph = sub.ph;
                sb += reinf * (0.45f * fsin (ph * 2.0f) + 0.25f * fsin (ph * 3.0f));
            }
            subMono[i] = sb * subLvl;
            const float mix = (s1 * l1 + s2 * l2) * uniNorm;
            const float ae = aenv.tick();
            float y = filt (mix + subMono[i] * 0.4f);
            y = dc (y);
            out[i] = y * ae * aeGain * c.wmTrem;
            subMono[i] *= ae * aeGain * 0.6f;   // the sub is kept apart: it stays centred by the Engine
        }
        if (fenv.st == Adsr::IDLE && aenv.st == Adsr::IDLE) {}
        filt.clean();
    }
};

//==============================================================================
struct SignalVoice
{
    float ph1 = 0.0f, ph2 = 0.0f, phM = 0.0f, fbz = 0.0f;
    float pph[MAX_PART]; OuWalk pmot[MAX_PART];
    FilterPair filt; Adsr fenv, aenv; FreqShifter shifter; DcBlock dc; LevelMatch interfMatch;
    Rng rng; double sr = 48000.0;
    float lastCut = 1000.0f;

    void prepare (double rate, uint32_t seed)
    {
        sr = rate; rng.seed (seed);
        for (int k = 0; k < MAX_PART; ++k) { pph[k] = rng.uni(); pmot[k].reset(); }
        dc.setHz (8.0f, sr); interfMatch.prepare (sr, 0.25f); reset();
    }
    void reset() { filt.reset(); fenv.kill(); aenv.kill(); shifter.reset(); dc.reset(); fbz = 0.0f; }
    void noteOn (float vel, bool legato)
    {
        if (! legato || ! aenv.active()) { fenv.on (vel); aenv.on (vel); ph1 = ph2 = phM = 0.0f; }
        else fenv.on (vel);
    }
    void noteOff() { fenv.off(); aenv.off(); }
    bool active() const { return aenv.active(); }

    void render (float* out, int n, const VoiceCtl& c, const Params& p, const WaveSet& ws, float ringIn)
    {
        fenv.set (lawHz (paramSpec (P_s_f_atk), p[P_s_f_atk]), lawHz (paramSpec (P_s_f_dec), p[P_s_f_dec]), p[P_s_f_sus], lawHz (paramSpec (P_s_f_rel), p[P_s_f_rel]), sr);
        aenv.set (lawHz (paramSpec (P_s_a_atk), p[P_s_a_atk]), lawHz (paramSpec (P_s_a_dec), p[P_s_a_dec]), p[P_s_a_sus], lawHz (paramSpec (P_s_a_rel), p[P_s_a_rel]), sr);

        // INTERFERENCE: several tuned interactions at once, compensated afterwards
        const float inter = p[P_s_interf];
        const float pshift = lawSemi (paramSpec (P_s_pshift), p[P_s_pshift]);
        const float base = c.hz * fexp2 ((pshift + (c.wmDet - c.wmSag * 100.0f * (1.0f - c.gate)) / 100.0f) / 12.0f);
        const float f1 = base * fexp2 ((float) (p.li (P_s_wt1oct) - 2) + (lawSemi (paramSpec (P_s_wt1semi), p[P_s_wt1semi]) + lawSemi (paramSpec (P_s_wt1fine), p[P_s_wt1fine]) / 100.0f) / 12.0f);
        const float f2 = base * fexp2 ((float) (p.li (P_s_wt2oct) - 2) + (lawSemi (paramSpec (P_s_wt2semi), p[P_s_wt2semi]) + (lawSemi (paramSpec (P_s_wt2fine), p[P_s_wt2fine]) + 7.0f * inter) / 100.0f) / 12.0f);
        const float inc1 = clampf (f1 / (float) sr, 0.0f, 0.49f), inc2 = clampf (f2 / (float) sr, 0.0f, 0.49f);
        const int t1 = p.li (P_s_wt1tab), t2 = p.li (P_s_wt2tab);
        const float pos1 = p[P_s_wt1pos], pos2 = p[P_s_wt2pos];
        const float l1 = p[P_s_wt1lvl], l2 = p[P_s_wt2lvl];
        const int ratioI = p.li (P_s_pmratio);
        static const float RATIOS[8] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f };
        const float fM = ratioI >= 8 ? lawHz (paramSpec (P_s_pmfixed), p[P_s_pmfixed]) : f1 * RATIOS[ratioI];
        const float incM = clampf (fM / (float) sr, 0.0f, 0.49f);
        const float idx = clamp01 (p[P_s_pmidx] + 0.5f * inter) * 2.5f;
        const float pfb = p[P_s_pmfb] * 0.9f;
        const float ring = clamp01 (p[P_s_ring] + 0.6f * inter);

        // additive bank
        const float addLvl = p[P_s_addlvl];
        const int nPart = clampi (p.li (P_s_addn), 1, MAX_PART);
        const float spread = (p[P_s_addspread] - 0.5f) * 2.0f + 0.35f * inter;
        const float odd = (p[P_s_addodd] - 0.5f) * 2.0f;
        const float tilt = (p[P_s_addtilt] - 0.5f) * 2.0f;
        const float cluster = p[P_s_addcluster];
        const float gaps = p[P_s_addgaps];
        const float motion = p[P_s_addmotion];
        const float fund = p[P_s_addfund];
        const float dt = (float) n / (float) sr;
        float pinc[MAX_PART], pamp[MAX_PART]; float ampSum = 1.0e-6f;
        const float nyq = 0.45f * (float) sr;
        for (int k = 0; k < nPart; ++k)
        {
            const float kk = (float) (k + 1);
            float ratio = std::pow (kk, 1.0f + 0.6f * spread);
            ratio = 1.0f + (ratio - 1.0f) * (1.0f - 0.9f * cluster);
            if (k == 0) ratio = lerp (ratio, 1.0f, fund);                 // the fundamental holds
            const float mot = motion > 0.0f ? pmot[k].tick (rng, 4.0f, 40.0f * motion, dt) : 0.0f;
            const float fk = base * ratio * fexp2 (mot / 1200.0f);
            float a = std::pow (kk, -(1.0f + 0.9f * tilt));
            if (k > 0) a *= ((k + 1) & 1) ? (odd > 0 ? 1.0f - odd : 1.0f) : (odd < 0 ? 1.0f + odd : 1.0f);
            if (k > 0 && gaps > 0.0f && hash01 ((uint32_t) k, 913u, 5u) < gaps) a = 0.0f;
            if (k == 0) a *= 0.7f + 0.6f * fund;
            if (fk > nyq) a = 0.0f;
            pinc[k] = fk / (float) sr; pamp[k] = a; ampSum += a;
        }
        const float addNorm = addLvl * 0.9f / ampSum;

        // filter
        const int fm = p.li (P_s_fmode);
        const float fe = fenv.tick();
        const float fenvAmt = (p[P_s_fenv] - 0.5f) * 2.0f;
        float cut = lawHz (paramSpec (P_s_cut), p[P_s_cut]) * fexp2 (fenvAmt * fe * 6.0f) * c.wmFilt * fexp2 (c.press * 2.0f);
        if (fm > 0) filt.set (fm - 1, clampf (cut, 20.0f, (float) sr * 0.45f), p[P_s_res], 0.2f, sr);

        // frequency shift in hertz, fine near zero
        const float shz = lawShz (paramSpec (P_s_shift), p[P_s_shift]) + 90.0f * inter * inter;
        shifter.setHz (shz, sr);
        const bool useShift = std::abs (shz) > 0.05f;

        const float aeGain = 0.58f * (0.35f + 0.65f * c.vel);
        for (int i = 0; i < n; ++i)
        {
            // PM operator
            const float m = fsin (phM + pfb * fbz * 0.25f);
            fbz = m; phM += incM; phM -= std::floor (phM);
            const float w1 = ws.read (t1, pos1, ph1 + idx * m * 0.5f, inc1);
            const float w2 = ws.read (t2, pos2, ph2, inc2);
            ph1 += inc1; ph1 -= std::floor (ph1);
            ph2 += inc2; ph2 -= std::floor (ph2);
            float y = w1 * l1 + w2 * l2;
            y = lerp (y, w1 * w2 * 1.5f, ring);
            if (ringIn != 0.0f) y = lerp (y, y * ringIn * 2.0f, ring * 0.5f);
            if (addLvl > 0.0f)
            {
                float a = 0.0f;
                for (int k = 0; k < nPart; ++k)
                {
                    if (pamp[k] > 0.0f) a += pamp[k] * fsin (pph[k]);
                    pph[k] += pinc[k]; pph[k] -= std::floor (pph[k]);
                }
                y += a * addNorm;
            }
            if (fm > 0) y = filt (y);
            if (useShift) y = shifter (y);
            const float pre = y;
            if (inter > 0.0f) { y *= lerp (1.0f, interfMatch.gain(), clamp01 (inter * 2.0f)); interfMatch.feed (pre * (1.0f - inter), pre); }
            y = dc (y);
            const float ae = aenv.tick();
            out[i] = y * ae * aeGain * c.wmTrem;
        }
        filt.clean(); shifter.hb.cleanState();
    }
};

//==============================================================================
/*  A modal body, a waveguide or a comb, excited from a buffer. */
struct StructVoice
{
    Resonator modes[MAX_MODES];
    float exW[MAX_MODES], pkW[MAX_MODES], modeHz[MAX_MODES], modeT60[MAX_MODES];
    int nModes = 0, model = -1;
    Delay tube; OnePole tubeDamp; Allpass tubeDiff; float tubeLen = 100.0f;
    float lastOut = 0.0f, bowState = 0.0f, energyEnv = 0.0f, fracRefract = 0.0f, fracBurst = 0.0f, fracDetune = 0.0f;
    float contScale = 0.01f;     // continuous excitation scale: (1 - r) averaged, so the steady-state gain is about one
    int fractures = 0;           // counted, so a fracture shorter than a tick is still seen
    float rawPeak = 0.0f;        // the body BEFORE the output saturator, so its work can be measured
    float strikeLeft = 0.0f, strikeAmp = 0.0f, noiseBurst = 0.0f;
    Adsr aenv; DcBlock dc; Rng rng; double sr = 48000.0;
    float lastBaseHz = -1.0f, lastStiff = -1.0f, lastDamp = -1.0f, lastDens = -1.0f, lastMat = -1.0f, lastEx = -1.0f, lastPk = -1.0f;

    void prepare (double rate, uint32_t seed)
    {
        sr = rate; rng.seed (seed);
        tube.prepare ((int) (rate * 0.1) + 8); tubeDiff.prepare (64); tubeDamp.setHz (4000.0f, sr);
        dc.setHz (10.0f, sr); reset();
        for (int k = 0; k < MAX_MODES; ++k) { modes[k].reset(); exW[k] = pkW[k] = 0.0f; modeHz[k] = 100.0f; modeT60[k] = 1.0f; }
    }
    void reset()
    {
        for (auto& m : modes) m.reset();
        tube.clear(); tubeDamp.reset(); aenv.kill(); dc.reset();
        lastOut = bowState = energyEnv = fracRefract = fracBurst = fracDetune = strikeLeft = strikeAmp = noiseBurst = 0.0f;
        lastBaseHz = -1.0f;
    }
    void noteOn (float vel, bool legato, bool strikeOnNote, float strikeLvl)
    {
        if (! legato || ! aenv.active()) aenv.on (vel);
        if (strikeOnNote) strike (strikeLvl * (0.4f + 0.6f * vel));
    }
    void noteOff() { aenv.off(); }
    bool active() const { return aenv.active(); }
    void strike (float amt) { strikeLeft = 0.003f * (float) sr; strikeAmp = amt; noiseBurst = amt; }

    static float templateRatio (int model, int k, float stiff, int n)
    {
        const float kk = (float) (k + 1);
        switch (model)
        {
            case 0:   // PLATE: (m^2/a^2 + n^2/b^2), a:b = 1:1.37, sorted by construction below
            {
                // enumerate a small grid and take the k-th smallest; cheap enough at prepare-like rates
                float vals[64]; int c = 0;
                for (int m = 1; m <= 8 && c < 64; ++m) for (int q = 1; q <= 8 && c < 64; ++q) vals[c++] = m * m + q * q / 1.8769f;
                std::sort (vals, vals + c);
                return vals[std::min (c - 1, k)] / vals[0];
            }
            case 1:   // CABLE: stiff string
            {
                const float B = 0.0005f + 0.02f * stiff;
                return kk * std::sqrt (1.0f + B * kk * kk);
            }
            case 2:   // BEAM: clamped-free
            {
                static const float bl[4] = { 1.875f, 4.694f, 7.855f, 10.996f };
                const float b = k < 4 ? bl[k] : (2.0f * kk - 1.0f) * PI * 0.5f;
                return (b * b) / (1.875f * 1.875f);
            }
            case 3:   // CAVITY: rigid box 1 : 0.8 : 0.62
            {
                float vals[64]; int c = 0;
                for (int l = 0; l <= 3 && c < 64; ++l) for (int m = 0; m <= 3 && c < 64; ++m) for (int q = 0; q <= 3 && c < 64; ++q)
                    if (l + m + q > 0) vals[c++] = std::sqrt ((float) (l * l) + (float) (m * m) / 0.64f + (float) (q * q) / 0.3844f);
                std::sort (vals, vals + c);
                return vals[std::min (c - 1, k)] / vals[0];
            }
            default:  // GLASS: a bell-like set with a designed irregularity
            {
                static const float g[12] = { 1.0f, 1.99f, 2.41f, 3.02f, 4.18f, 5.36f, 6.79f, 8.11f, 9.63f, 11.3f, 13.1f, 15.2f };
                const float base = k < 12 ? g[k] : 15.2f + (kk - 12.0f) * 2.3f;
                return base * (1.0f + 0.03f * (hash01 ((uint32_t) k, 31u, 8u) - 0.5f));
            }
        }
        (void) n;
    }

    void design (float baseHz, const Params& p)
    {
        model = p.li (P_st_model);
        const float stiff = p[P_st_stiff], damp = p[P_st_damp], dens = p[P_st_dens], mat = p[P_st_material];
        const float ex = p[P_st_expos], pk = p[P_st_pickup];
        const bool same = std::abs (baseHz - lastBaseHz) < 0.01f * baseHz && stiff == lastStiff && damp == lastDamp && dens == lastDens && mat == lastMat && ex == lastEx && pk == lastPk;
        if (same) return;
        lastBaseHz = baseHz; lastStiff = stiff; lastDamp = damp; lastDens = dens; lastMat = mat; lastEx = ex; lastPk = pk;
        if (model >= 5)
        {
            tubeLen = clampf ((float) sr / std::max (20.0f, baseHz), 2.0f, (float) tube.size() - 8.0f);
            tubeDamp.setHz (lerp (12000.0f, 800.0f, damp) * lerp (0.6f, 1.6f, mat), sr);
            tubeDiff.g = 0.3f + 0.4f * stiff; tubeDiff.len = 1 + (int) (stiff * 40.0f);
            nModes = 0;
            return;
        }
        nModes = clampi (4 + (int) (dens * (MAX_MODES - 4)), 4, MAX_MODES);
        const float t60 = xmap (1.0f - damp, 0.08f, 25.0f);
        const float nyq = 0.45f * (float) sr;
        float lossSum = 0.0f;
        for (int k = 0; k < nModes; ++k)
        {
            float r = templateRatio (model, k, stiff, nModes);
            r = std::pow (r, 1.0f + 0.25f * stiff);              // stiffness stretches every template
            const float hz = baseHz * r;
            // material: 0 = soft (highs die fast), 1 = metallic (highs ring)
            const float rel = (float) k / (float) std::max (1, nModes - 1);
            float t = t60 * lerp (std::pow (0.12f, rel), std::pow (1.4f, rel), mat);
            modeHz[k] = hz; modeT60[k] = t;
            modes[k].set (hz > nyq ? nyq : hz, t, sr);
            exW[k] = hz > nyq ? 0.0f : std::sin (PI * (k + 1) * (0.05f + 0.9f * ex));
            pkW[k] = hz > nyq ? 0.0f : std::sin (PI * (k + 1) * (0.05f + 0.9f * pk)) / std::sqrt (1.0f + 0.2f * k);
            lossSum += 1.0f - modes[k].r;
        }
        // a continuous force meets a resonant gain of g/(1-r): scale it by the mean loss so it lands near unity
        contScale = clampf (lossSum / (float) nModes * 40.0f, 0.003f, 1.0f);
    }

    /*  exc: the exciter buffer (mono, n samples). out: mono. */
    void render (float* out, const float* exc, int n, const VoiceCtl& c, const Params& p)
    {
        aenv.set (lawHz (paramSpec (P_st_atk), p[P_st_atk]), 5.0f, 1.0f, lawHz (paramSpec (P_st_rel), p[P_st_rel]), sr);
        const float pitch = lawSemi (paramSpec (P_st_pitch), p[P_st_pitch]) + lawSemi (paramSpec (P_st_fine), p[P_st_fine]) / 100.0f;
        const float baseHz = (p.sw (P_st_keyfollow) ? c.hz : 110.0f) * fexp2 ((pitch + (c.wmDet - c.wmSag * 100.0f * (1.0f - c.gate)) / 100.0f) / 12.0f + fracDetune);
        design (baseHz, p);

        const int excKind = p.li (P_st_exc);
        const float excLvl = p[P_st_exclvl] * 2.0f;
        const float sustain = p[P_st_sustain];
        const float bowF = p[P_st_bowforce], bowV = 0.02f + 0.3f * p[P_st_bowvel];
        const float couple = p[P_st_couple], drive = p[P_st_drive], stress = p[P_st_stress];
        /*  A modal bank sums many more contributors than a two-oscillator
            voice, so it needs the lowest per-voice level of the three: it was
            reaching 2.5 on its own bus and the limiter was taking 7.5 dB off
            the presets built on it. */
        const float aeGain = 0.52f * (0.4f + 0.6f * c.vel);
        const float strikeLvl = p[P_st_strikelvl];
        const float dtS = 1.0f / (float) sr;
        const float thr = 0.5f * (1.0f - 0.9f * stress);     // fracture threshold (output level) falls with stress

        for (int i = 0; i < n; ++i)
        {
            // ---- excitation
            float e = 0.0f;
            const float noise = rng.bi();
            if (strikeLeft > 0.0f) { e += strikeAmp * (0.6f * noise + 0.4f) * (strikeLeft / (0.003f * (float) sr)); strikeLeft -= 1.0f; }
            if (noiseBurst > 1.0e-4f) { e += 0.4f * noiseBurst * noise; noiseBurst *= 0.9995f; }
            const float cs = model >= 5 ? 0.5f : contScale;
            switch (excKind)
            {
                case 0: break;                                         // IMPULSE: strikes only
                case 1: e += cs * excLvl * 1.5f * noise * sustain; break;   // NOISE BURST: continuous at SUSTAIN
                case 2:
                {
                    // FRICTION: stick–slip against the body's own velocity at the bow point.
                    // A simplified hyperbolic friction curve (documented as such): static
                    // friction inside |vRel| < vth, velocity-weakening beyond it, which is
                    // what lets the body's own motion pump energy in.
                    const float vBody = (lastOut - bowState) * (float) sr * 0.001f; bowState = lastOut;   // per ms
                    const float vRel = bowV - vBody;
                    const float vth = 0.02f;
                    float fr;
                    if (std::abs (vRel) < vth) fr = vRel / vth;
                    else fr = (vRel > 0 ? 1.0f : -1.0f) * (0.3f + 0.7f / (1.0f + 6.0f * std::abs (vRel)));
                    e += cs * excLvl * 2.0f * bowF * sustain * fr * (0.9f + 0.1f * noise);
                    break;
                }
                default: e += cs * excLvl * 3.0f * exc[i] * (0.4f + 0.6f * sustain); break;   // another stratum / loop / aux
            }
            if (fracBurst > 1.0e-4f) { e += fracBurst * noise * 0.8f; fracBurst *= 0.999f; }
            e = ftanh (e * (1.0f + 3.0f * drive)) / (1.0f + 3.0f * drive);

            float y = 0.0f, energy = 0.0f;
            if (model >= 5)
            {
                // TUBE / COMB
                const float fb = 0.75f + 0.245f * (1.0f - p[P_st_damp]) + 0.1f * stress;
                float v = tube.read (tubeLen - 1.0f);
                v = tubeDamp.lp (v);
                if (model == 6) v = tubeDiff (v);
                v = ftanh (v * fb * (1.0f + couple) + e * 0.5f);
                tube.write (v);
                y = v; energy = v * v;
            }
            else
            {
                const float coupleIn = couple > 0.0f ? ftanh (lastOut * couple * 3.0f) * 0.3f : 0.0f;
                for (int k = 0; k < nModes; ++k)
                {
                    const float mk = modes[k](e * exW[k] + coupleIn * exW[k]);
                    y += mk * pkW[k];
                    energy += mk * mk;
                }
                y *= 1.5f / std::sqrt ((float) nModes);
            }
            // STRESS: past a threshold the body fractures — a burst, a chirp, a refractory period.
            // The threshold is on the body's OUTPUT level (what is heard), tracked over ~20 ms.
            energyEnv += (y * y - energyEnv) * 0.001f;
            if (fracRefract > 0.0f) fracRefract -= dtS;
            else if (stress > 0.02f && energyEnv > thr * thr * 0.25f)
            {
                fracBurst = 0.4f * stress; fracDetune = -0.08f * stress; fracRefract = 0.15f + 0.6f * (1.0f - stress); ++fractures;
                for (int k = 0; k < nModes; k += 3) modes[k].y1 *= 0.5f;   // a crack
            }
            fracDetune *= 0.9997f;
            lastOut = y;
            y = dc (y);
            rawPeak = std::max (std::abs (y), rawPeak * 0.999999f);
            /*  A soft ceiling, not a saturator: fracture bursts still cannot
                leave the body unbounded, but an ordinary note is untouched
                below 70 % of full scale (ceilSoft is transparent there). */
            out[i] = ceilSoft (y, 1.35f) * aenv.tick() * aeGain * c.wmTrem;
        }
        for (int k = 0; k < nModes; ++k) modes[k].cleanState();
        if (bad (lastOut)) { lastOut = 0.0f; tube.clear(); }
        (void) strikeLvl;
    }
};

} // namespace tty
