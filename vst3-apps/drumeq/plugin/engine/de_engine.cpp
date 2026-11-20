#include "de_engine.h"

#include <algorithm>
#include <cmath>

namespace de
{
    namespace
    {
        constexpr int kSub = 16;                       // control sub-block
        inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
        inline double clamp01 (double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
        //  a crossfade along an S, not a line: a line's corners are kinks
        inline double scurve (double x) { return x * x * (3.0 - 2.0 * x); }
        constexpr double kButterA = 0.54119610014619698;   // 4th-order Butterworth, two sections
        constexpr double kButterB = 1.30656296487637652;
        constexpr double kRoot2   = 0.70710678118654752;
    }

    void Engine::prepare (double sampleRate, int)
    {
        fs = sampleRate > 1000.0 ? sampleRate : 48000.0;
        aSmooth = std::exp (-(double) kSub / (0.020 * fs));        // 20 ms knobs
        fadeStep = 1.0 / (0.010 * fs);                               // 10 ms switches
        polStep  = 2.0 / (0.010 * fs);                               // 10 ms polarity (-1..1)
        pre.assign ((size_t) kRing, 0.0f);
        post.assign ((size_t) kRing, 0.0f);
        ringW.store (0);
        findBuf.assign ((size_t) (8.0 * fs), 0.0f);
        findLen.store (0);
        findOn.store (false);
        reset();
    }

    void Engine::reset()
    {
        for (auto& c : chs) c = Ch {};
        gEnv = 0.0; gGain = 1.0; gHold = 0.0; gOpen = true;
        cY = 0.0; cSlow = 0.0;
        fresh = true;
    }

    void Engine::armFind (bool on)
    {
        if (on) findLen.store (0, std::memory_order_release);
        findOn.store (on);
    }

    void Engine::updateCoefs (bool force)
    {
        //  each design only when its own inputs moved
        auto changed = [&] (int slot, double v)
        {
            if (! force && lastDesigned[slot] == v) return false;
            lastDesigned[slot] = v; return true;
        };
        const double hpf = std::exp2 (sHpf.cur), lpf = std::exp2 (sLpf.cur);
        if (changed (0, hpf) | force)
        {
            hp12 = design (F_HP, hpf, fs, 0, kRoot2);
            hp24a = design (F_HP, hpf, fs, 0, kButterA);
            hp24b = design (F_HP, hpf, fs, 0, kButterB);
        }
        if (changed (1, lpf) | force)
        {
            lp12 = design (F_LP, lpf, fs, 0, kRoot2);
            lp24a = design (F_LP, lpf, fs, 0, kButterA);
            lp24b = design (F_LP, lpf, fs, 0, kButterB);
        }
        for (int b = 0; b < 4; ++b)
        {
            const double f = std::exp2 (sF[b].cur), g = sG[b].cur, q = std::exp2 (sQ[b].cur);
            //  a cheap combined signature of the band's three inputs
            const double sig = f * 1.0e6 + g * 1.0e3 + q;
            if (! changed (2 + b, sig)) continue;
            if (b == 0)      { band[0] = design (F_LSHELF, f, fs, g, q); bandAlt[0] = design (F_PEAK, f, fs, g, q); }
            else if (b == 3) { band[3] = design (F_HSHELF, f, fs, g, q); bandAlt[3] = design (F_PEAK, f, fs, g, q); }
            else               band[b] = design (F_PEAK, f, fs, g, q);
        }
        const double klo = std::exp2 (sKeyLo.cur), khi = std::exp2 (sKeyHi.cur), sc = std::exp2 (sSc.cur);
        if (changed (6, klo) | force) keyHp = design (F_HP, klo, fs, 0, kRoot2);
        if (changed (7, khi) | force) keyLp = design (F_LP, khi, fs, 0, kRoot2);
        if (changed (8, sc) | force)  scHp  = design (F_HP, sc, fs, 0, kRoot2);
    }

    void Engine::process (const Params& p, float* const* ch, int numCh, int n)
    {
        numCh = std::max (1, std::min (kMaxCh, numCh));
        cur = p;

        //  ---- targets --------------------------------------------------------
        sIn.tgt = p.in; sOut.tgt = p.out; sMix.tgt = p.cmix; sMake.tgt = p.cmake;
        sHpf.tgt = std::log2 (std::max (1.0f, p.hpf)); sLpf.tgt = std::log2 (std::max (1.0f, p.lpf));
        const float bf[4] = { p.b1f, p.b2f, p.b3f, p.b4f }, bg[4] = { p.b1g, p.b2g, p.b3g, p.b4g },
                    bq[4] = { p.b1q, p.b2q, p.b3q, p.b4q };
        for (int b = 0; b < 4; ++b)
        {
            sF[b].tgt = std::log2 (std::max (1.0f, bf[b]));
            sG[b].tgt = bg[b];
            sQ[b].tgt = std::log2 (std::max (0.05f, bq[b]));
        }
        sKeyLo.tgt = std::log2 (std::max (1.0f, p.gkeylo));
        sKeyHi.tgt = std::log2 (std::max (1.0f, p.gkeyhi));
        sSc.tgt = std::log2 (std::max (1.0f, p.csc));
        hpFade.target = std::max (0, std::min (2, (int) std::lround (p.hpfs)));
        lpFade.target = std::max (0, std::min (2, (int) std::lround (p.lpfs)));
        t1Fade.target = p.b1t > 0.5f ? 1 : 0;
        t4Fade.target = p.b4t > 0.5f ? 1 : 0;
        eqT = p.eq > 0.5f ? 1.0 : 0.0;
        gateT = p.gate > 0.5f ? 1.0 : 0.0;
        compT = p.comp > 0.5f ? 1.0 : 0.0;
        polT = p.pol > 0.5f ? -1.0 : 1.0;

        if (fresh)
        {
            //  a new instance or a reset starts ON its settings, not gliding to them
            for (Smooth* s : { &sIn, &sOut, &sMix, &sMake, &sHpf, &sLpf, &sKeyLo, &sKeyHi, &sSc }) s->snap (s->tgt);
            for (int b = 0; b < 4; ++b) { sF[b].snap (sF[b].tgt); sG[b].snap (sG[b].tgt); sQ[b].snap (sQ[b].tgt); }
            hpFade.snap (hpFade.target); lpFade.snap (lpFade.target);
            t1Fade.snap (t1Fade.target); t4Fade.snap (t4Fade.target);
            eqW = eqT; gateW = gateT; compW = compT; polS = polT;
            updateCoefs (true);
            fresh = false;
        }

        //  ---- the compressor's constants for this block ---------------------
        const double T = p.cthr, R = std::max (1.0f, p.crat), Wk = std::max (0.0f, p.cknee);
        const double aA = std::exp (-1.0 / (std::max (0.02f, p.catk) * 0.001 * fs));
        const double aS = std::exp (-1.0 / (0.5 * fs));
        const double aGateEnv = std::exp (-1.0 / (0.010 * fs));
        const double gRange = std::max (0.0f, p.grange);
        const double gFloor = dbToLin (-gRange);
        const double gAtkStep = (1.0 - gFloor) / std::max (1.0, std::max (0.02f, p.gatk) * 0.001 * fs);
        const double gRelStepDb = gRange / std::max (1.0, std::max (1.0f, p.grel) * 0.001 * fs);
        const double holdN = p.ghold * 0.001 * fs;
        const bool keyHpOn = p.csc > 20.5f;            // the bottom of KEY HPF is OFF

        float pkIn = 0, pkOut = 0;
        double grMin = 0.0;
        int rw = ringW.load (std::memory_order_relaxed);
        const bool capture = findOn.load();
        int fl = findLen.load (std::memory_order_relaxed);
        const int fcap = (int) findBuf.size();

        for (int s0 = 0; s0 < n; s0 += kSub)
        {
            const int m = std::min (kSub, n - s0);

            //  ---- smoothed controls: step once per sub-block, ramp within ----
            const double inA = dbToLin (sIn.cur), outA = dbToLin (sOut.cur);
            const double mixA = sMix.cur, makeA = sMake.cur;
            bool moved = false;
            sIn.step (aSmooth); sOut.step (aSmooth); sMix.step (aSmooth); sMake.step (aSmooth);
            moved |= sHpf.step (aSmooth); moved |= sLpf.step (aSmooth);
            for (int b = 0; b < 4; ++b) { moved |= sF[b].step (aSmooth); moved |= sG[b].step (aSmooth); moved |= sQ[b].step (aSmooth); }
            moved |= sKeyLo.step (aSmooth); moved |= sKeyHi.step (aSmooth); moved |= sSc.step (aSmooth);
            if (moved) updateCoefs (false);
            const double inB = dbToLin (sIn.cur), outB = dbToLin (sOut.cur);
            const double mixB = sMix.cur, makeB = sMake.cur;

            //  the release this sub-block (program dependent when AUTO)
            double relMs = std::max (1.0f, p.crel);
            if (p.cauto > 0.5f) relMs *= 0.35 + 1.65 * clamp01 (-cSlow / 10.0);
            const double aR = std::exp (-1.0 / (relMs * 0.001 * fs));

            const bool hpIdle = hpFade.settled() && hpFade.target == 0;
            const bool lpIdle = lpFade.settled() && lpFade.target == 0;

            for (int i = 0; i < m; ++i)
            {
                const int idx = s0 + i;
                const double t = (double) (i + 1) / (double) m;
                const double inG = inA + (inB - inA) * t;
                const double outG = outA + (outB - outA) * t;

                //  structure fades
                if (! hpFade.settled()) hpFade.tick (fadeStep);
                if (! lpFade.settled()) lpFade.tick (fadeStep);
                if (! t1Fade.settled()) t1Fade.tick (fadeStep);
                if (! t4Fade.settled()) t4Fade.tick (fadeStep);
                if (eqW != eqT)     eqW   = eqW < eqT ? std::min (eqT, eqW + fadeStep) : std::max (eqT, eqW - fadeStep);
                if (gateW != gateT) gateW = gateW < gateT ? std::min (gateT, gateW + fadeStep) : std::max (gateT, gateW - fadeStep);
                if (compW != compT) compW = compW < compT ? std::min (compT, compW + fadeStep) : std::max (compT, compW - fadeStep);
                if (polS != polT)   polS  = polS < polT ? std::min (polT, polS + polStep) : std::max (polT, polS - polStep);

                const double polE = polS == 1.0 ? 1.0 : (polS == -1.0 ? -1.0 : 2.0 * scurve (0.5 * (polS + 1.0)) - 1.0);
                double x[kMaxCh] = { 0, 0 };
                double inPk = 0;
                for (int c = 0; c < numCh; ++c)
                {
                    const double v = (double) ch[c][idx];
                    inPk = std::max (inPk, std::fabs (v));
                    x[c] = v * inG * polE;
                }
                pkIn = std::max (pkIn, (float) inPk);
                const double monoPre = numCh == 2 ? 0.5 * (x[0] + x[1]) : x[0];
                if (capture && fl < fcap) findBuf[(size_t) fl++] = (float) monoPre;

                //  ---- GATE ------------------------------------------------
                if (gateW > 0.0)
                {
                    double k = 0.0;
                    for (int c = 0; c < numCh; ++c)
                        k = std::max (k, std::fabs (chs[c].keyLp.tick (keyLp, chs[c].keyHp.tick (keyHp, x[c]))));
                    gEnv = std::max (k, gEnv * aGateEnv);
                    const double eDb = 20.0 * std::log10 (std::max (1.0e-9, gEnv));
                    if (eDb > p.gthr)            { gOpen = true; gHold = holdN; }
                    else if (eDb > p.gthr - 4.0) { if (gOpen) gHold = holdN; }     // hysteresis
                    else if (gHold > 0.0)        gHold -= 1.0;
                    else                         gOpen = false;
                    //  ATTACK is the time to open fully, RELEASE the time to close
                    //  fully - the way a gate's times are read. Opening is a
                    //  straight rise in gain (fast and clean on a stick); closing
                    //  is a straight fall in dB, so the tail fades like a decay.
                    if (gOpen) gGain = std::min (1.0, gGain + gAtkStep);
                    else if (gGain > gFloor)
                    {
                        const double gdb = 20.0 * std::log10 (std::max (1.0e-9, gGain));
                        gGain = std::max (gFloor, dbToLin (gdb - gRelStepDb));
                    }
                    const double ge = 1.0 + (gGain - 1.0) * scurve (gateW);
                    for (int c = 0; c < numCh; ++c) x[c] *= ge;
                }
                else if (gateT == 0.0)
                {
                    gGain = 1.0; gOpen = true; gHold = 0.0; gEnv = 0.0;
                    for (int c = 0; c < numCh; ++c) { chs[c].keyHp.reset(); chs[c].keyLp.reset(); }
                }

                //  ---- FILTERS ----------------------------------------------
                for (int c = 0; c < numCh; ++c)
                {
                    auto& S = chs[c];
                    double y = x[c];
                    if (! hpIdle)
                    {
                        double w[3]; hpFade.shaped (w);
                        double o = w[0] * y;
                        if (w[1] > 0.0) o += w[1] * S.hp12.tick (hp12, y); else S.hp12.reset();
                        if (w[2] > 0.0) o += w[2] * S.hp24b.tick (hp24b, S.hp24a.tick (hp24a, y)); else { S.hp24a.reset(); S.hp24b.reset(); }
                        y = o;
                    }
                    if (! lpIdle)
                    {
                        double w[3]; lpFade.shaped (w);
                        double o = w[0] * y;
                        if (w[1] > 0.0) o += w[1] * S.lp12.tick (lp12, y); else S.lp12.reset();
                        if (w[2] > 0.0) o += w[2] * S.lp24b.tick (lp24b, S.lp24a.tick (lp24a, y)); else { S.lp24a.reset(); S.lp24b.reset(); }
                        y = o;
                    }

                    //  ---- EQ ------------------------------------------------
                    if (eqW > 0.0)
                    {
                        double e = y;
                        //  band 1 and band 4: a shelf and a bell, crossfaded on a change
                        auto twoShape = [&] (int b, const Fade<2>& f)
                        {
                            double o = 0.0, w[2];
                            f.shaped (w);
                            if (w[0] > 0.0) o += w[0] * (band[b].identity() ? (S.band[b].reset(), e) : S.band[b].tick (band[b], e));
                            else S.band[b].reset();
                            if (w[1] > 0.0) o += w[1] * (bandAlt[b].identity() ? (S.bandAlt[b].reset(), e) : S.bandAlt[b].tick (bandAlt[b], e));
                            else S.bandAlt[b].reset();
                            e = o;
                        };
                        twoShape (0, t1Fade);
                        for (int b = 1; b <= 2; ++b)
                        {
                            if (band[b].identity()) S.band[b].reset();
                            else e = S.band[b].tick (band[b], e);
                        }
                        twoShape (3, t4Fade);
                        y = eqW == 1.0 ? e : y + (e - y) * scurve (eqW);
                    }
                    x[c] = y;
                }

                //  ---- COMPRESSOR (stereo-linked) -----------------------------
                if (compW > 0.0)
                {
                    double sc = 0.0;
                    for (int c = 0; c < numCh; ++c)
                        sc = std::max (sc, std::fabs (keyHpOn ? chs[c].scHp.tick (scHp, x[c]) : x[c]));
                    const double L = 20.0 * std::log10 (std::max (1.0e-9, sc));
                    const double over = L - T;
                    double G;
                    if (2.0 * over < -Wk)                       G = 0.0;
                    else if (Wk > 1e-6 && 2.0 * std::fabs (over) <= Wk)
                                                                G = (1.0 / R - 1.0) * (over + 0.5 * Wk) * (over + 0.5 * Wk) / (2.0 * Wk);
                    else                                        G = (1.0 / R - 1.0) * over;
                    cY = G < cY ? G + (cY - G) * aA : G + (cY - G) * aR;
                    cSlow = cY + (cSlow - cY) * aS;
                    grMin = std::min (grMin, cY * compW);
                    const double mix = mixA + (mixB - mixA) * t;
                    const double mk = makeA + (makeB - makeA) * t;
                    const double g = dbToLin (cY + mk);
                    const double wet = (1.0 - mix) + g * mix;          // parallel: dry + compressed
                    const double k = 1.0 + (wet - 1.0) * scurve (compW);
                    for (int c = 0; c < numCh; ++c) x[c] *= k;
                }
                else if (compT == 0.0)
                {
                    cY = 0.0; cSlow = 0.0;
                    for (int c = 0; c < numCh; ++c) chs[c].scHp.reset();
                }

                //  ---- OUTPUT --------------------------------------------------
                double outPk = 0;
                for (int c = 0; c < numCh; ++c)
                {
                    const double v = x[c] * outG;
                    ch[c][idx] = (float) v;
                    outPk = std::max (outPk, std::fabs (v));
                }
                pkOut = std::max (pkOut, (float) outPk);

                const double monoPost = numCh == 2 ? 0.5 * ((double) ch[0][idx] + (double) ch[1][idx]) : (double) ch[0][idx];
                pre[(size_t) rw] = (float) monoPre;
                post[(size_t) rw] = (float) monoPost;
                rw = (rw + 1) & (kRing - 1);
            }
        }

        ringW.store (rw, std::memory_order_release);
        if (capture)
        {
            findLen.store (fl, std::memory_order_release);
            if (fl >= fcap) findOn.store (false);
        }
        mIn.store (pkIn);
        mOut.store (pkOut);
        mGr.store ((float) grMin);
        mGate.store (gateW > 0.0 ? (float) (20.0 * std::log10 (std::max (1.0e-9, 1.0 + (gGain - 1.0) * gateW))) : 0.0f);
    }
}
