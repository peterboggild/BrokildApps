/*  Thirty Thousand Years — ENVIRONMENT: distortion, feedback and scale.

    Four channel strips, two insert lanes of reorderable processors, a spatial
    send (early reflections + an 8-line FDN), an explicit FEEDBACK LOOP node,
    and the output stage (bass mono, width, a lookahead ceiling limiter).

    Stability strategy, in one place: the FDN is a Householder (lossless)
    matrix with per-line losses, so its loop gain is below one by
    construction; the feedback loop has a tanh bound, a DC blocker and a
    finite-value guard INSIDE it; the limiter is metered and reported, and is
    not part of the stability argument.
*/
#pragma once

#include "Dsp.h"
#include "Params.h"

namespace tty
{

static constexpr int ENV_MAXBLOCK = 64;

//==============================================================================
/*  Selective oversampling around a per-sample nonlinear functor. factor 1, 2
    or 4 (4 = two half-band stages). */
struct Oversampler
{
    HalfBand a[2], b[2], c[2], d[2];   // a,b: stage 1 L/R ; c,d: stage 2 L/R
    int factor = 1;
    void reset() { for (auto* h : { a, b, c, d }) { h[0].reset(); h[1].reset(); } }
    template <typename Fn>
    inline void tick (float& l, float& r, Fn&& fn)
    {
        if (factor == 1) { fn (l, r); return; }
        float l0, l1, r0, r1;
        a[0].up (l, l0, l1); a[1].up (r, r0, r1);
        if (factor == 2)
        {
            fn (l0, r0); fn (l1, r1);
            l = a[0].down (l0, l1); r = a[1].down (r0, r1);
            return;
        }
        float ll[4], rr[4];
        c[0].up (l0, ll[0], ll[1]); c[0].up (l1, ll[2], ll[3]);
        c[1].up (r0, rr[0], rr[1]); c[1].up (r1, rr[2], rr[3]);
        for (int i = 0; i < 4; ++i) fn (ll[i], rr[i]);
        l0 = c[0].down (ll[0], ll[1]); l1 = c[0].down (ll[2], ll[3]);
        r0 = c[1].down (rr[0], rr[1]); r1 = c[1].down (rr[2], rr[3]);
        l = a[0].down (l0, l1); r = a[1].down (r0, r1);
    }
    int latency() const { return factor == 1 ? 0 : (factor == 2 ? HalfBand::latency() : HalfBand::latency() + HalfBand::latency() / 2); }
};

//==============================================================================
struct Channel
{
    Biquad hpL, hpR, lpL, lpR; double sr = 48000.0;
    float lastHp = -1, lastLp = -1;
    void prepare (double rate) { sr = rate; hpL.bypass(); hpR.bypass(); lpL.bypass(); lpR.bypass(); }
    /*  In place: gain, pan, width, cuts. envSend / loopSend get the post-strip signal times the send. */
    void process (float* L, float* R, int n, float gainV, float pan, float width, float hpV, float lpV, bool muted)
    {
        const float g = muted ? 0.0f : lawVol (gainV);
        if (hpV != lastHp) { lastHp = hpV; if (hpV < 0.01f) { hpL.bypass(); hpR.bypass(); } else { hpL.highpass (xmap (hpV, 10.0f, 4000.0f), 0.707f, sr); hpR = hpL; } }
        if (lpV != lastLp) { lastLp = lpV; if (lpV > 0.99f) { lpL.bypass(); lpR.bypass(); } else { lpL.lowpass (xmap (lpV, 200.0f, 20000.0f), 0.707f, sr); lpR = lpL; } }
        const float pl = std::cos ((pan) * PI * 0.5f) * 1.4142f, pr = std::sin ((pan) * PI * 0.5f) * 1.4142f;
        for (int i = 0; i < n; ++i)
        {
            float l = lpL (hpL (L[i])) * g, r = lpR (hpR (R[i])) * g;
            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * width * 2.0f;
            l = (m + s) * pl; r = (m - s) * pr;
            L[i] = l; R[i] = r;
        }
        hpL.cleanState(); hpR.cleanState(); lpL.cleanState(); lpR.cleanState();
    }
};

//==============================================================================
/*  Insert processors. Each is stereo, in place, and owns its state. */
struct Saturator
{
    Oversampler os; LevelMatch lm; Biquad tiltL, tiltR; double sr = 48000.0; float lastTone = -9;
    void prepare (double rate, int factor) { sr = rate; os.factor = factor; os.reset(); lm.prepare (rate * factor, 0.3f); tiltL.bypass(); tiltR.bypass(); }
    void process (float* L, float* R, int n, const Params& p)
    {
        const float drive = 1.0f + 11.0f * p[P_e_sat_drive] * p[P_e_sat_drive];
        const float asym = (p[P_e_sat_asym] - 0.5f) * 0.9f;
        const float tone = (p[P_e_sat_tone] - 0.5f) * 2.0f, mix = p[P_e_sat_mix];
        if (tone != lastTone) { lastTone = tone; tiltL.highShelf (2500.0f, tone * 9.0f, sr); tiltR = tiltL; }
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            const float dl = l, dr = r;
            os.tick (l, r, [&] (float& a, float& b)
            {
                // asymmetric: an even term biased by asym, then tanh
                a = ftanh ((a + asym * 0.3f * a * a) * drive); b = ftanh ((b + asym * 0.3f * b * b) * drive);
                lm.feed (dl, a);
            });
            const float g = lm.gain();
            l = tiltL (l * g); r = tiltR (r * g);
            L[i] = lerp (dl, l, mix); R[i] = lerp (dr, r, mix);   // correlated: linear blend
        }
    }
};

struct Folder
{
    Oversampler os; LevelMatch lm; DcBlock dcL, dcR;
    void prepare (double rate, int factor) { os.factor = factor; os.reset(); lm.prepare (rate * factor, 0.3f); dcL.setHz (10.0f, rate); dcR.setHz (10.0f, rate); }
    static inline float fold (float x, float sym)
    {
        x += sym;
        // a sine fold: soft when the drive is low, tearing when it is high
        float y = std::sin (x * 1.5707963f);
        if (std::abs (x) > 1.0f) y = std::sin (x * 1.5707963f) * (1.0f / (1.0f + 0.15f * (std::abs (x) - 1.0f)));
        return y - std::sin (sym * 1.5707963f);
    }
    void process (float* L, float* R, int n, const Params& p)
    {
        const float amt = 1.0f + 11.0f * p[P_e_fold_amt] * p[P_e_fold_amt], sym = (p[P_e_fold_sym] - 0.5f) * 0.8f, mix = p[P_e_fold_mix];
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i]; const float dl = l, dr = r;
            os.tick (l, r, [&] (float& a, float& b) { a = fold (a * amt, sym); b = fold (b * amt, sym); lm.feed (dl, a); });
            const float g = lm.gain();
            l = dcL (l * g); r = dcR (r * g);
            L[i] = lerp (dl, l, mix); R[i] = lerp (dr, r, mix);
        }
    }
};

struct Multiband
{
    // Linkwitz–Riley 4th order = two cascaded 2nd-order Butterworths, per split, per channel
    Biquad loL[2], loR[2], hiL[2], hiR[2], midLoL[2], midLoR[2], midHiL[2], midHiR[2];
    LevelMatch lm; double sr = 48000.0; float lastLo = -1, lastHi = -1;
    void prepare (double rate) { sr = rate; lm.prepare (rate, 0.3f); }
    void design (float lo, float hi)
    {
        for (int k = 0; k < 2; ++k)
        {
            loL[k].lowpass (lo, 0.7071f, sr); loR[k] = loL[k];
            midLoL[k].highpass (lo, 0.7071f, sr); midLoR[k] = midLoL[k];
            midHiL[k].lowpass (hi, 0.7071f, sr); midHiR[k] = midHiL[k];
            hiL[k].highpass (hi, 0.7071f, sr); hiR[k] = hiL[k];
        }
    }
    void process (float* L, float* R, int n, const Params& p)
    {
        const float lo = lawHz (paramSpec (P_e_mb_lo), p[P_e_mb_lo]), hi = std::max (lo * 2.0f, lawHz (paramSpec (P_e_mb_hi), p[P_e_mb_hi]));
        if (lo != lastLo || hi != lastHi) { lastLo = lo; lastHi = hi; design (lo, hi); }
        const float dLow = 1.0f + 3.0f * p[P_e_mb_low], dMid = 1.0f + 30.0f * p[P_e_mb_mid] * p[P_e_mb_mid], dHigh = 1.0f + 20.0f * p[P_e_mb_high] * p[P_e_mb_high];
        const float mix = p[P_e_mb_mix];
        for (int i = 0; i < n; ++i)
        {
            const float dl = L[i], dr = R[i];
            const float ll = loL[1](loL[0](dl)), lr = loR[1](loR[0](dr));
            const float ml = midHiL[1](midHiL[0](midLoL[1](midLoL[0](dl)))), mr = midHiR[1](midHiR[0](midLoR[1](midLoR[0](dr))));
            const float hl = hiL[1](hiL[0](dl)), hr = hiR[1](hiR[0](dr));
            // the low band is PROTECTED: gentle, level-preserving
            float l = ftanh (ll * dLow) / dLow * 1.15f + ftanh (ml * dMid) / std::sqrt (dMid) + ftanh (hl * dHigh) / std::sqrt (dHigh);
            float r = ftanh (lr * dLow) / dLow * 1.15f + ftanh (mr * dMid) / std::sqrt (dMid) + ftanh (hr * dHigh) / std::sqrt (dHigh);
            lm.feed (dl, l);
            const float g = lm.gain();
            L[i] = lerp (dl, l * g, mix); R[i] = lerp (dr, r * g, mix);
        }
        for (auto* b : { loL, loR, hiL, hiR, midLoL, midLoR, midHiL, midHiR }) { b[0].cleanState(); b[1].cleanState(); }
    }
};

struct Shifter
{
    FreqShifter sL, sR; float fbL = 0, fbR = 0; double sr = 48000.0;
    void prepare (double rate) { sr = rate; sL.reset(); sR.reset(); fbL = fbR = 0; }
    void process (float* L, float* R, int n, const Params& p)
    {
        const float hz = lawShz (paramSpec (P_e_sh_hz), p[P_e_sh_hz]);
        sL.setHz (hz, sr); sR.setHz (-hz * 0.0f + hz, sr);
        const float mix = p[P_e_sh_mix], fb = p[P_e_sh_fb] * 0.85f;
        for (int i = 0; i < n; ++i)
        {
            const float dl = L[i], dr = R[i];
            const float l = sL (dl + fbL * fb), r = sR (dr + fbR * fb);
            fbL = ftanh (l); fbR = ftanh (r);
            L[i] = lerp (dl, l, mix); R[i] = lerp (dr, r, mix);
        }
        sL.hb.cleanState(); sR.hb.cleanState();
    }
};

struct ModDelay
{
    Delay dL, dR; float timeSm = 1000.0f, lfoPh = 0.0f; Biquad toneL, toneR; DcBlock dcL, dcR; double sr = 48000.0;
    float xfade = 1.0f, headA = 1000.0f, headB = 1000.0f; float lastTone = -9, lastTime = -1;
    void prepare (double rate) { sr = rate; dL.prepare ((int) (rate * 2.2)); dR.prepare ((int) (rate * 2.2)); toneL.bypass(); toneR.bypass(); dcL.setHz (10.0f, rate); dcR.setHz (10.0f, rate); }
    void process (float* L, float* R, int n, const Params& p)
    {
        const float tMs = lawHz (paramSpec (P_e_dl_time), p[P_e_dl_time]);
        const float target = tMs * 0.001f * (float) sr;
        const float fb = p[P_e_dl_fb] * 0.95f, mod = p[P_e_dl_mod], rate = lawHz (paramSpec (P_e_dl_rate), p[P_e_dl_rate]);
        const int mode = p.li (P_e_dl_mode);
        const float tone = (p[P_e_dl_tone] - 0.5f) * 2.0f, mix = p[P_e_dl_mix];
        if (tone != lastTone) { lastTone = tone; if (tone < 0) toneL.lowpass (xmap (1.0f + tone, 800.0f, 18000.0f), 0.6f, sr); else toneL.highpass (xmap (tone, 20.0f, 1500.0f), 0.6f, sr); toneR = toneL; }
        if (mode == 1 && std::abs (target - lastTime) > 2.0f && lastTime > 0)
        {
            // CLEAN: a new head is opened at the new time and crossfaded in (no pitch)
            headA = headB; headB = target; xfade = 0.0f;
        }
        if (lastTime < 0) { timeSm = target; headA = headB = target; }
        lastTime = target;
        const float slew = 1.0f - std::exp (-1.0f / (0.08f * (float) sr));   // TAPE: 80 ms time slew = a pitch sweep
        const float xstep = 1.0f / (0.05f * (float) sr);
        for (int i = 0; i < n; ++i)
        {
            lfoPh += rate / (float) sr; lfoPh -= std::floor (lfoPh);
            const float wow = mod * 0.02f * (float) sr * 0.001f * 40.0f * fsin (lfoPh);   // up to ~0.8 ms at full
            float rl, rr;
            if (mode == 0)
            {
                timeSm += (target - timeSm) * slew;
                rl = dL.read (timeSm + wow); rr = dR.read (timeSm + wow * 0.8f);
            }
            else
            {
                xfade = std::min (1.0f, xfade + xstep);
                const float a = dL.read (headA + wow), b = dL.read (headB + wow);
                rl = lerp (a, b, xfade);
                const float ar = dR.read (headA + wow * 0.8f), br = dR.read (headB + wow * 0.8f);
                rr = lerp (ar, br, xfade);
            }
            rl = toneL (rl); rr = toneR (rr);
            const float dl = L[i], dr = R[i];
            dL.write (dcL (ftanh (dl + rl * fb))); dR.write (dcR (ftanh (dr + rr * fb)));
            L[i] = lerp (dl, rl, mix); R[i] = lerp (dr, rr, mix);
        }
        toneL.cleanState(); toneR.cleanState();
    }
};

struct Comb
{
    Delay dL, dR; OnePole dampL, dampR; Allpass apL[2], apR[2]; DcBlock dcL, dcR; double sr = 48000.0;
    void prepare (double rate)
    {
        sr = rate; dL.prepare ((int) (rate * 0.1)); dR.prepare ((int) (rate * 0.1));
        for (int k = 0; k < 2; ++k) { apL[k].prepare (600); apR[k].prepare (600); }
        apL[0].len = 113; apL[1].len = 271; apR[0].len = 127; apR[1].len = 293;
        dcL.setHz (10.0f, rate); dcR.setHz (10.0f, rate);
    }
    void process (float* L, float* R, int n, const Params& p)
    {
        const float hz = 110.0f * fexp2 (lawSemi (paramSpec (P_e_cb_pitch), p[P_e_cb_pitch]) / 12.0f);
        const float len = clampf ((float) sr / hz, 2.0f, (float) dL.size() - 8.0f);
        const float fb = p[P_e_cb_fb] * 0.985f, diff = p[P_e_cb_diff], mix = p[P_e_cb_mix];
        dampL.setHz (xmap (1.0f - p[P_e_cb_damp], 600.0f, 18000.0f), sr); dampR.a = dampL.a;
        for (int k = 0; k < 2; ++k) { apL[k].g = 0.2f + 0.5f * diff; apR[k].g = apL[k].g; }
        for (int i = 0; i < n; ++i)
        {
            const float dl = L[i], dr = R[i];
            float rl = dampL.lp (dL.read (len)), rr = dampR.lp (dR.read (len));
            if (diff > 0.01f) { rl = apL[1] (apL[0] (rl)); rr = apR[1] (apR[0] (rr)); }
            dL.write (dcL (ftanh (dl + rl * fb))); dR.write (dcR (ftanh (dr + rr * fb)));
            L[i] = lerp (dl, rl, mix); R[i] = lerp (dr, rr, mix);
        }
    }
};

struct Crusher
{
    Biquad preL, preR, postL, postR; float holdL = 0, holdR = 0, acc = 0; double sr = 48000.0; float lastRate = -1;
    void prepare (double rate) { sr = rate; preL.bypass(); preR.bypass(); postL.bypass(); postR.bypass(); }
    void process (float* L, float* R, int n, const Params& p)
    {
        const float bits = 16.0f - 14.0f * p[P_e_cr_bits];
        const float rate = std::min ((float) sr, lawHz (paramSpec (P_e_cr_rate), p[P_e_cr_rate]));
        const bool aa = p.sw (P_e_cr_aa); const float mix = p[P_e_cr_mix];
        if (rate != lastRate) { lastRate = rate; const float fc = std::min (0.45f * (float) sr, rate * 0.45f); preL.lowpass (fc, 0.7f, sr); preR = preL; postL.lowpass (fc, 0.7f, sr); postR = postL; }
        const float q = std::pow (2.0f, bits - 1.0f);
        const float step = rate / (float) sr;
        for (int i = 0; i < n; ++i)
        {
            const float dl = L[i], dr = R[i];
            float l = aa ? preL (dl) : dl, r = aa ? preR (dr) : dr;
            acc += step;
            if (acc >= 1.0f) { acc -= 1.0f; holdL = std::round (l * q) / q; holdR = std::round (r * q) / q; }
            l = aa ? postL (holdL) : holdL; r = aa ? postR (holdR) : holdR;
            L[i] = lerp (dl, l, mix); R[i] = lerp (dr, r, mix);
        }
    }
};

//==============================================================================
/*  Early reflections + an 8-line FDN. APPARENT SCALE moves the early spacing,
    the line lengths, the modulation rate and the low/high decay balance; it is
    a different control from the amount. FREEZE holds the tail with no input. */
struct Space
{
    static constexpr int NL = 8;
    Delay pre[2], early[2], line[NL];
    OnePole dampF[NL]; Biquad lowShelf[NL];
    Allpass diffIn[4];
    float lenBase[NL] = { 1033, 1213, 1427, 1637, 1801, 1949, 2131, 2311 };   // samples at 48 k, size 0.5, scale 0.5
    float modPh[NL]; float g[NL]; float lastDecay = -1, lastSize = -1, lastScale = -1, lastDamp = -1, lastLow = -1;
    DcBlock dc[NL];
    double sr = 48000.0; float energy = 0.0f;
    float eTap[8] = { 0.011f, 0.019f, 0.027f, 0.037f, 0.049f, 0.062f, 0.079f, 0.097f };   // seconds at scale 0.5
    float eGain[8] = { 0.8f, 0.7f, 0.62f, 0.55f, 0.48f, 0.4f, 0.33f, 0.27f };

    void prepare (double rate)
    {
        sr = rate;
        for (int c = 0; c < 2; ++c) { pre[c].prepare ((int) (rate * 0.3)); early[c].prepare ((int) (rate * 0.5)); }
        for (int k = 0; k < NL; ++k) { line[k].prepare ((int) (rate * 0.6)); modPh[k] = (float) k / NL; g[k] = 0.5f; dc[k].setHz (5.0f, rate); dampF[k].setHz (5000.0f, rate); lowShelf[k].bypass(); }
        for (int k = 0; k < 4; ++k) { diffIn[k].prepare (2000); }
        diffIn[0].len = (int) (rate * 0.0047); diffIn[1].len = (int) (rate * 0.0071); diffIn[2].len = (int) (rate * 0.0103); diffIn[3].len = (int) (rate * 0.0149);
        lastDecay = -1;
    }
    void reset() { for (auto& l : line) l.clear(); for (auto& c : pre) c.clear(); for (auto& c : early) c.clear(); energy = 0.0f; }

    void design (float decayS, float size, float scale, float damp, float low)
    {
        const float lenMul = (0.4f + 1.2f * size) * (0.5f + 1.5f * scale) * (float) sr / 48000.0f;
        for (int k = 0; k < NL; ++k)
        {
            const float len = clampf (lenBase[k] * lenMul, 32.0f, (float) line[k].size() - 64.0f);
            lineLen[k] = len;
            // per-line loss for the wanted T60
            g[k] = std::pow (10.0f, -3.0f * len / (decayS * (float) sr));
            // high damping: a one-pole in the loop; low decay: a shelf (+ = longer lows)
            dampF[k].setHz (xmap (1.0f - damp, 900.0f, 18000.0f), sr);
            lowShelf[k].lowShelf (250.0f * (0.5f + scale), (low - 0.5f) * 6.0f, sr);
        }
    }
    float lineLen[NL] = {};

    /*  in: the send (stereo). outL/outR: ADDED. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Params& p, float scale)
    {
        const float decay = lawHz (paramSpec (P_e_rv_decay), p[P_e_rv_decay]);
        const float size = p[P_e_rv_size], damp = p[P_e_rv_damp], low = p[P_e_rv_low];
        if (decay != lastDecay || size != lastSize || scale != lastScale || damp != lastDamp || low != lastLow)
        { lastDecay = decay; lastSize = size; lastScale = scale; lastDamp = damp; lastLow = low; design (decay, size, scale, damp, low); }
        const bool freeze = p.sw (P_e_rv_freeze);
        const float diff = p[P_e_rv_diff], mod = p[P_e_rv_mod] * (0.5f + scale), earlyMix = p[P_e_rv_early];
        const float preS = lawHz (paramSpec (P_e_rv_pre), p[P_e_rv_pre]) * 0.001f * (float) sr;
        for (int k = 0; k < 4; ++k) diffIn[k].g = 0.25f + 0.5f * diff;
        const float modRate = (0.08f + 0.5f * (1.0f - scale)) / (float) sr;
        const float eScale = (0.35f + 1.3f * scale) * (float) sr;
        float e = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            pre[0].write (inL[i]); pre[1].write (inR[i]);
            const float l = pre[0].read (preS + 1.0f), r = pre[1].read (preS + 1.0f);
            // early reflections
            early[0].write (l); early[1].write (r);
            float el = 0.0f, er = 0.0f;
            for (int t = 0; t < 8; ++t)
            {
                const float d = eTap[t] * eScale;
                el += eGain[t] * early[t & 1].read (d);
                er += eGain[t] * early[(t + 1) & 1].read (d * 1.07f);
            }
            // diffuse the input, then the FDN
            float x = 0.5f * (l + r);
            x = diffIn[3] (diffIn[2] (diffIn[1] (diffIn[0] (x))));
            float v[NL], sum = 0.0f;
            for (int k = 0; k < NL; ++k)
            {
                modPh[k] += modRate * (1.0f + 0.13f * k); modPh[k] -= std::floor (modPh[k]);
                const float dl = lineLen[k] + mod * 12.0f * fsin (modPh[k]);
                v[k] = line[k].read (dl);
                sum += v[k];
            }
            // Householder: y = v - (2/N) * sum
            const float h = sum * (2.0f / NL);
            float lo = 0.0f, ro = 0.0f;
            for (int k = 0; k < NL; ++k)
            {
                float y = v[k] - h;
                if (! freeze) { y = dampF[k].lp (y); y = lowShelf[k](y); y *= g[k]; y += x * (k & 1 ? 0.5f : 0.5f); }
                y = dc[k](y);
                y = ftanh (y);                    // energy containment
                line[k].write (clean (y));
                if (k & 1) ro += v[k]; else lo += v[k];
                e += v[k] * v[k];
            }
            lo *= 0.35f; ro *= 0.35f;
            outL[i] += lerp (lo, el, earlyMix); outR[i] += lerp (ro, er, earlyMix);
        }
        energy = 0.95f * energy + 0.05f * std::sqrt (e / (n * NL));
        for (auto& b : lowShelf) b.cleanState();
    }
};

//==============================================================================
/*  The FEEDBACK LOOP node: send -> delay -> hp/lp -> shift -> saturation ->
    damping -> return. Every guard is inside it. */
struct FeedLoop
{
    Delay dL, dR; Biquad hpL, hpR, lpL, lpR; FreqShifter shL, shR; OnePole dampL, dampR; DcBlock dcL, dcR;
    double sr = 48000.0; float energy = 0.0f, lastHp = -1, lastLp = -1;
    void prepare (double rate)
    {
        sr = rate; dL.prepare ((int) (rate * 2.1)); dR.prepare ((int) (rate * 2.1));
        hpL.bypass(); hpR.bypass(); lpL.bypass(); lpR.bypass(); shL.reset(); shR.reset();
        dampL.setHz (8000.0f, rate); dampR.a = dampL.a; dcL.setHz (12.0f, rate); dcR.setHz (12.0f, rate);
    }
    void reset() { dL.clear(); dR.clear(); shL.reset(); shR.reset(); energy = 0.0f; }
    /*  sendL/R: this block's send. retL/R: OVERWRITTEN with the return. */
    void process (const float* sendL, const float* sendR, float* retL, float* retR, int n, const Params& p)
    {
        const float delay = lawHz (paramSpec (P_e_fb_delay), p[P_e_fb_delay]) * 0.001f * (float) sr;
        const float hp = p[P_e_fb_hp], lp = p[P_e_fb_lp];
        if (hp != lastHp) { lastHp = hp; hpL.highpass (lawHz (paramSpec (P_e_fb_hp), hp), 0.707f, sr); hpR = hpL; }
        if (lp != lastLp) { lastLp = lp; lpL.lowpass (lawHz (paramSpec (P_e_fb_lp), lp), 0.707f, sr); lpR = lpL; }
        const float shz = lawShz (paramSpec (P_e_fb_shift), p[P_e_fb_shift]);
        shL.setHz (shz, sr); shR.setHz (shz, sr);
        const bool useShift = std::abs (shz) > 0.05f;
        const float sat = 1.0f + 6.0f * p[P_e_fb_sat];
        dampL.setHz (xmap (1.0f - p[P_e_fb_damp], 500.0f, 16000.0f), sr); dampR.a = dampL.a;
        const float ret = p[P_e_fb_ret] * 1.2f;
        float e = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = dL.read (delay), r = dR.read (delay);
            l = lpL (hpL (l)); r = lpR (hpR (r));
            if (useShift) { l = shL (l); r = shR (r); }
            l = ftanh (l * sat) / std::sqrt (sat); r = ftanh (r * sat) / std::sqrt (sat);
            l = dampL.lp (l); r = dampR.lp (r);
            l = dcL (l); r = dcR (r);
            retL[i] = l * ret; retR[i] = r * ret;
            // the loop closes here: the send plus the return, bounded
            dL.write (clean (ftanh (sendL[i] + retL[i] * 0.9f)));
            dR.write (clean (ftanh (sendR[i] + retR[i] * 0.9f)));
            e += l * l + r * r;
        }
        energy = 0.9f * energy + 0.1f * std::sqrt (e / (2 * n));
        hpL.cleanState(); hpR.cleanState(); lpL.cleanState(); lpR.cleanState(); shL.hb.cleanState(); shR.hb.cleanState();
    }
};

//==============================================================================
struct Lane
{
    Saturator sat; Folder fold; Multiband multi; Shifter shift; ModDelay delay; Comb comb; Crusher crush;
    void prepare (double rate, int factor) { sat.prepare (rate, factor); fold.prepare (rate, factor); multi.prepare (rate); shift.prepare (rate); delay.prepare (rate); comb.prepare (rate); crush.prepare (rate); }
    void run (int kind, float* L, float* R, int n, const Params& p)
    {
        switch (kind)
        {
            case 1: sat.process (L, R, n, p); break;
            case 2: fold.process (L, R, n, p); break;
            case 3: multi.process (L, R, n, p); break;
            case 4: shift.process (L, R, n, p); break;
            case 5: delay.process (L, R, n, p); break;
            case 6: comb.process (L, R, n, p); break;
            case 7: crush.process (L, R, n, p); break;
            default: break;
        }
    }
    void process (float* L, float* R, int n, const Params& p, int s1, int s2, int s3)
    {
        const int k[3] = { p.li (s1), p.li (s2), p.li (s3) };
        for (int j = 0; j < 3; ++j) if (k[j] > 0) run (k[j], L, R, n, p);
    }
};

//==============================================================================
struct OutputStage
{
    Biquad bmLoL[2], bmLoR[2], bmHiL[2], bmHiR[2]; OnePole distLpL, distLpR; Limiter lim; double sr = 48000.0; float lastBm = -1;
    float peakL = 0.0f, peakR = 0.0f, rms = 0.0f;
    /*  Measured where its name says: after volume and trim, immediately before
        the limiter. The engine's own tap used to be upstream of the master
        gain, which let a preset report a pre-limiter peak below the ceiling
        while the limiter was pulling 5 dB. */
    float preLimPeak = 0.0f;
    bool noLimit = false;           // probe only: measure the patch, not the limiter
    void prepare (double rate) { sr = rate; lim.prepare (rate, 2.0f); distLpL.setHz (20000.0f, rate); distLpR.a = distLpL.a; }
    int latency() const { return lim.latency(); }
    void process (float* L, float* R, int n, const Params& p, float duckGain)
    {
        const float vol = lawVol (p[P_volume]) * dbGain (lawDb (paramSpec (P_outtrim), p[P_outtrim])) * duckGain;
        const float width = p[P_outwidth] * 2.0f;
        lim.ceiling = dbGain (lawDb (paramSpec (P_ceiling), p[P_ceiling]));
        const bool bm = p.sw (P_bassmono_on);
        const float bmHz = lawHz (paramSpec (P_bassmono), p[P_bassmono]);
        if (bm && bmHz != lastBm) { lastBm = bmHz; for (int k = 0; k < 2; ++k) { bmLoL[k].lowpass (bmHz, 0.7071f, sr); bmLoR[k] = bmLoL[k]; bmHiL[k].highpass (bmHz, 0.7071f, sr); bmHiR[k] = bmHiL[k]; } }
        float pl = 0.0f, pr = 0.0f, e = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i] * vol, r = R[i] * vol;
            if (bm)
            {
                const float lo = 0.5f * (bmLoL[1](bmLoL[0](l)) + bmLoR[1](bmLoR[0](r)));
                const float hl = bmHiL[1](bmHiL[0](l)), hr = bmHiR[1](bmHiR[0](r));
                l = lo + hl; r = lo + hr;
            }
            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * width;
            l = m + s; r = m - s;
            preLimPeak = std::max (preLimPeak, std::max (std::abs (l), std::abs (r)));
            if (! noLimit) { lim.tick (l, r); l = clampf (l, -1.5f, 1.5f); r = clampf (r, -1.5f, 1.5f); }
            L[i] = l; R[i] = r;
            pl = std::max (pl, std::abs (l)); pr = std::max (pr, std::abs (r)); e += l * l + r * r;
        }
        peakL = std::max (pl, peakL * 0.9f); peakR = std::max (pr, peakR * 0.9f);
        rms = 0.9f * rms + 0.1f * std::sqrt (e / (2 * n));
    }
};

} // namespace tty
