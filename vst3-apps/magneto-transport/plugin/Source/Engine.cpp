#include "Engine.h"
#include "Stock.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#if defined (_M_X64) || defined (__SSE2__)
 #include <emmintrin.h>
 #define MT_SSE 1
#endif

#ifndef M_PI
 #define M_PI 3.14159265358979323846
#endif

namespace mt
{

namespace
{
    static_assert (STK_NL == NLAYER, "the record tables and the tape disagree on the layer count");
    static_assert (STK_NM == 11 && STK_NS <= LayerRecord::NSMAX, "LayerRecord's slice size");

    inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }

    /*  The playback amplifier: unity to 0 dBFS, a smooth knee into a +2 dBFS rail
        (20 dB over 0 VU). */
    inline double ampRail (double x)
    {
        const double a = std::abs (x);
        if (a <= 1.0) return x;
        constexpr double h = AMP_RAIL - 1.0;
        return std::copysign (1.0 + h * std::tanh ((a - 1.0) / h), x);
    }

    /*  Two tracks against one set of taps, n contiguous floats each - the record
        kernel and the playback head's near taps. Float lanes, four at a time: a
        sum of under a hundred products keeps its error near -130 dB. */
    inline void dot2 (const float* t, const float* p0, const float* p1, int n, double& a0, double& a1)
    {
       #ifdef MT_SSE
        __m128 s0 = _mm_setzero_ps(), s1 = _mm_setzero_ps();
        int j = 0;
        for (; j + 4 <= n; j += 4)
        {
            const __m128 w = _mm_loadu_ps (t + j);
            s0 = _mm_add_ps (s0, _mm_mul_ps (w, _mm_loadu_ps (p0 + j)));
            s1 = _mm_add_ps (s1, _mm_mul_ps (w, _mm_loadu_ps (p1 + j)));
        }
        alignas (16) float r0[4], r1[4];
        _mm_store_ps (r0, s0); _mm_store_ps (r1, s1);
        double x0 = (double) r0[0] + r0[1] + r0[2] + r0[3];
        double x1 = (double) r1[0] + r1[1] + r1[2] + r1[3];
        for (; j < n; ++j) { x0 += (double) t[j] * p0[j]; x1 += (double) t[j] * p1[j]; }
        a0 += x0; a1 += x1;
       #else
        for (int j = 0; j < n; ++j) { const double w = t[j]; a0 += w * p0[j]; a1 += w * p1[j]; }
       #endif
    }
    inline double Phi (double x) { return 0.5 * std::erfc (-x / std::sqrt (2.0)); }

    const char* SYNC_LABELS[NUM_SYNC] =
        { "OFF", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/2D", "1/1", "2/1" };

    // Catmull-Rom segment through p0..p3, evaluated / integrated over t in [0,1].
    struct CR
    {
        double c0, c1, c2, c3;
        CR (double p0, double p1, double p2, double p3)
            : c0 (p1),
              c1 (0.5 * (p2 - p0)),
              c2 (0.5 * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3)),
              c3 (0.5 * (3.0 * (p1 - p2) + p3 - p0)) {}
        double at (double t) const { return c0 + t * (c1 + t * (c2 + t * c3)); }
        double prim (double t) const { return t * (c0 + t * (c1 * 0.5 + t * (c2 / 3.0 + t * c3 * 0.25))); }
    };

    // the s axis of the tables: S_GRID[i] = 2.5 sinh(3 t) / sinh(3), t uniform on [-1, 1]
    const double SINH3 = std::sinh (3.0);
    inline double sIndex (double s)
    {
        const double t = std::asinh (s * SINH3 / 2.5) / 3.0;
        return std::clamp ((t + 1.0) * 0.5 * (STK_NS - 1), 0.0, (double) (STK_NS - 1));
    }
}

const char* syncName (int i) { return (i >= 0 && i < NUM_SYNC) ? SYNC_LABELS[i] : "?"; }

//==============================================================================
void Biquad::shelf (bool high, double fs, double f0, double gainDb)
{
    const double A  = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * M_PI * f0 / fs;
    const double cs = std::cos (w0), sn = std::sin (w0);
    const double alpha = sn * 0.5 * std::sqrt (2.0);
    const double sa = 2.0 * std::sqrt (A) * alpha;
    double a0;
    if (! high)
    {
        b0 = A * ((A + 1) - (A - 1) * cs + sa);
        b1 = 2 * A * ((A - 1) - (A + 1) * cs);
        b2 = A * ((A + 1) - (A - 1) * cs - sa);
        a0 = (A + 1) + (A - 1) * cs + sa;
        a1 = -2 * ((A - 1) + (A + 1) * cs);
        a2 = (A + 1) + (A - 1) * cs - sa;
    }
    else
    {
        b0 = A * ((A + 1) + (A - 1) * cs + sa);
        b1 = -2 * A * ((A - 1) + (A + 1) * cs);
        b2 = A * ((A + 1) + (A - 1) * cs - sa);
        a0 = (A + 1) - (A - 1) * cs + sa;
        a1 = 2 * ((A - 1) - (A + 1) * cs);
        a2 = (A + 1) - (A - 1) * cs - sa;
    }
    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
}

void HalfBand::design()
{
    constexpr int C = TAPS / 2;
    double g[TAPS] {}, sum = 0.0;
    for (int n = 0; n < TAPS; ++n)
    {
        const double t = n - C;
        const double sc = (t == 0.0) ? 0.5 : std::sin (M_PI * 0.5 * t) / (M_PI * t);
        const double wn = 0.42 - 0.5 * std::cos (2.0 * M_PI * n / (TAPS - 1))
                               + 0.08 * std::cos (4.0 * M_PI * n / (TAPS - 1));
        g[n] = sc * wn;
        sum += g[n];
    }
    for (auto& x : g) x /= sum;
    for (int k = 0; k < PH; ++k)
    {
        ge[k] = (2 * k     < TAPS) ? (float) g[2 * k]     : 0.0f;
        go[k] = (2 * k + 1 < TAPS) ? (float) g[2 * k + 1] : 0.0f;
    }
}

void HalfBand::clear()
{
    std::memset (upLine, 0, sizeof (upLine));
    std::memset (dnE, 0, sizeof (dnE));
    std::memset (dnO, 0, sizeof (dnO));
    upW = dnW = 0;
}

void HalfBand::up (float x, float& o0, float& o1)
{
    upLine[upW] = x;
    float s0 = 0.0f, s1 = 0.0f;
    for (int k = 0; k < PH; ++k)
    {
        const float val = upLine[(upW - k) & MASK];
        s0 += ge[k] * val;
        s1 += go[k] * val;
    }
    o0 = 2.0f * s0; o1 = 2.0f * s1;
    upW = (upW + 1) & MASK;
}

float HalfBand::down (float a, float b)
{
    dnE[dnW] = a; dnO[dnW] = b;
    float s = 0.0f;
    for (int k = 0; k < PH; ++k)
        s += ge[k] * dnE[(dnW - k) & MASK] + go[k] * dnO[(dnW - k - 1) & MASK];
    dnW = (dnW + 1) & MASK;
    return s;
}

//==============================================================================
TapeStore::TapeStore (int64_t baseCells) : n0 (baseCells)
{
    for (int c = 0; c < 2; ++c)
        for (int l = 0; l < NLAYER; ++l)
        {
            m[c][l].assign ((size_t) (n0 >> l), 0.0f);
            md[c][l].assign ((size_t) (n0 >> l), 0.0f);
            rp[c][l].assign ((size_t) (n0 >> l), (uint8_t) 0);
            ta[c][l].assign ((size_t) (n0 >> l), (uint8_t) 0);
        }
}

// a retention index (quarter-dB of loss) back to a ratio; 255 is gone
static const struct RqTable { float v[256]; RqTable() { for (int k = 0; k < 255; ++k) v[k] = (float) std::pow (10.0, -0.25 * k / 20.0); v[255] = 0.0f; } } RQ;

Engine::Engine() = default;

Engine::~Engine()
{
    delete store;
    delete pending.exchange (nullptr);
    delete retired.exchange (nullptr);
}

int64_t Engine::cellsNeeded (int m, double loopMeters) const
{
    const double cm = (m == 0) ? REEL_WINDOW_CM : loopMeters * 100.0;
    const int64_t c = (int64_t) std::llround (cm * CELLS_PER_CM);
    return (c + 7) / 8 * 8;      // every layer's ring an exact fraction of the surface ring
}

//==============================================================================
/*  Record and replay equalisation targets (docs/physics/07-playback.md), from
    the machine's own small-signal model at operating bias: each slab weighted by
    its record gain and zone (with the zone's phase), read through its exact slab
    response. Replay = the standard's curve x an HF trim for the head's spacing
    and gap (at most +10 dB); record = whatever makes the short-circuit flux
    follow the standard (at most +20 dB). Both normalised at 1 kHz, held above
    20 kHz. Fitted once per process (warm-started bucket to bucket). */
namespace
{
    struct EqTables
    {
        ShelfCascade rec[2][HeadKernel::NB], rep[2][HeadKernel::NB];
        double worst = 0.0, worstV = 0.0;
    };

    // a minimum with a soft knee (k dB wide), so a limit does not put a kink in a
    // target the shelves then cannot follow
    inline double softMinDb (double a, double b, double k = 1.5)
    {
        const double m = std::min (a, b);
        return m - k * std::log (std::exp (-(a - m) / k) + std::exp (-(b - m) / k));
    }

    void opLayers (const CoatingTables& C, double v, double (&g)[NLAYER], double (&x)[NLAYER], double (&w)[NLAYER])
    {
        const double lnA0 = std::log (C.aGrid[0]), lnSpan = std::log (C.aGrid[STK_NA - 1]) - lnA0;
        const double fv = std::clamp (std::log2 (v / C.speedCms[0]), 0.0, (double) (STK_NV - 1));
        const int iv = std::min ((int) fv, STK_NV - 2);
        const double tv = fv - iv;
        for (int l = 0; l < NLAYER; ++l)
        {
            const double A = std::clamp (C.hbOperating * C.layerFmax[l], C.aGrid[0], C.aGrid[STK_NA - 1]);
            const double fa = (std::log (A) - lnA0) / lnSpan * (STK_NA - 1);
            const int ia = std::clamp ((int) std::floor (fa), 0, STK_NA - 2);
            const double ta = std::clamp (fa - ia, 0.0, 1.0);
            auto z = [&] (const double (*Z)[STK_NL][STK_NA]) {
                const double lo = (1.0 - ta) * Z[iv][l][ia] + ta * Z[iv][l][ia + 1];
                const double hi = (1.0 - ta) * Z[iv + 1][l][ia] + ta * Z[iv + 1][l][ia + 1];
                return (1.0 - tv) * lo + tv * hi;
            };
            g[l] = z (C.zoneGain); x[l] = z (C.zoneX); w[l] = std::max (0.0, z (C.zoneW));
        }
    }

    void eqTargets (int coating, int std_, double v, const double* f, int n, double* recDb, double* repDb)
    {
        const CoatingTables& C = coatingTables (coating);
        double g[NLAYER], x[NLAYER], w[NLAYER];
        opLayers (C, v, g, x, w);
        double gx = 0.0, gs = 0.0;
        for (int l = 0; l < NLAYER; ++l) { gx += g[l] * x[l]; gs += g[l]; }
        const double xRef = gs > 0.0 ? gx / gs : 0.0;
        double tau2, w1;
        eqTimeConstants (std_, v, tau2, w1);
        const double dsp = PB_SPACING_UM + slabSpacingCorrectionUm (v, coating);
        auto at = [&] (double fHz, double& rec, double& rep) {
            const double k = 2.0 * M_PI * fHz / (v * 1.0e4);
            double re = 0.0, im = 0.0;
            for (int l = 0; l < NLAYER; ++l)
            {
                const double a = g[l] * std::exp (-0.5 * std::pow (k * w[l], 2.0)) * slabResponse (l, k, v, coating);
                const double ph = -k * (x[l] - xRef);
                re += a * std::cos (ph); im += a * std::sin (ph);
            }
            const double R = std::hypot (re, im) / gs;
            const double xg = 0.5 * k * PB_GAP_UM;
            const double HL = std::max (1.0e-6, std::exp (-k * dsp) * std::abs (xg < 1.0e-9 ? 1.0 : std::sin (xg) / xg));
            // a fresh stock's surface spacing is trimmed out by the record EQ, as a
            // technician aligns record treble for the tape on the machine
            const double Fsc = R * std::exp (-k * C.alignSpacingUm) / HL;
            const double om = 2.0 * M_PI * fHz;
            const double Hrep = std::hypot (1.0, om * tau2) * (w1 > 0.0 ? om / std::hypot (om, w1) : 1.0);
            const double C = std::pow (10.0, softMinDb (-20.0 * std::log10 (HL), 10.0) / 20.0);
            rep = Hrep * C;
            rec = (1.0 / Hrep) / std::max (Fsc, 1.0e-9);
        };
        double rec1, rep1;
        at (1000.0, rec1, rep1);
        for (int i = 0; i < n; ++i)
        {
            double rc, rp;
            at (std::min (f[i], 20000.0), rc, rp);
            recDb[i] = softMinDb (20.0 * std::log10 (rc / rec1), 20.0);
            repDb[i] = 20.0 * std::log10 (rp / rep1);
        }
    }

    const EqTables& eqTables (int coating)
    {
        static std::mutex mx;
        static std::unique_ptr<EqTables> tabs[NUM_COATINGS];
        std::lock_guard<std::mutex> lock (mx);
        std::unique_ptr<EqTables>& t = tabs[coating];
        if (t) return *t;
        t = std::make_unique<EqTables>();
        constexpr int NF = 48;
        double f[NF], rc[NF], rp[NF];
        for (int i = 0; i < NF; ++i) f[i] = 15.0 * std::pow (22000.0 / 15.0, (double) i / (NF - 1));
        for (int s = 0; s < 2; ++s)
            for (int b = 0; b < HeadKernel::NB; ++b)
            {
                eqTargets (coating, s, HeadKernel::bucketSpeed (b), f, NF, rc, rp);
                if (b > 0) { t->rec[s][b] = t->rec[s][b - 1]; t->rep[s][b] = t->rep[s][b - 1]; }
                // report the error where the head can still be trimmed flat (as section 16 asserts)
                const double v = HeadKernel::bucketSpeed (b);
                const double fTop = v >= 19.0 ? 15000.0 : v >= 9.0 ? 10000.0 : 6000.0;
                const double e = std::max (t->rec[s][b].fit (f, rc, NF, b > 0, fTop), t->rep[s][b].fit (f, rp, NF, b > 0, fTop));
                if (e > t->worst) { t->worst = e; t->worstV = HeadKernel::bucketSpeed (b); }
            }
        return *t;
    }
}

void Engine::eqTargetDb (int coating, int standard, double vCms, double fHz, double& recDb, double& repDb)
{
    eqTargets (coating, standard, vCms, &fHz, 1, &recDb, &repDb);
}

double Engine::eqFitErrorDb (int coating, int standard, int b, double fHz, bool record)
{
    const EqTables& T = eqTables (coating);
    const double f1 = 1000.0;
    double r1, p1, r, p;
    eqTargets (coating, standard, HeadKernel::bucketSpeed (b), &f1, 1, &r1, &p1);
    eqTargets (coating, standard, HeadKernel::bucketSpeed (b), &fHz, 1, &r, &p);
    const ShelfCascade& S = record ? T.rec[standard][b] : T.rep[standard][b];
    return S.analogueDb (fHz) - (record ? r : p);
}

double Engine::eqFitWorstDb (int coating)
{
    if (coating >= 0) return eqTables (coating).worst;
    double w = 0.0;
    for (int k = 0; k < NUM_COATINGS; ++k) w = std::max (w, eqTables (k).worst);
    return w;
}
double Engine::eqFitWorstSpeed (int coating)
{
    if (coating >= 0) return eqTables (coating).worstV;
    int best = 0;
    for (int k = 1; k < NUM_COATINGS; ++k) if (eqTables (k).worst > eqTables (best).worst) best = k;
    return eqTables (best).worstV;
}

/*  The equalisers for the motor's speed: the analogue gains of the two speed
    buckets either side, interpolated, then discretised - so a varispeed glide
    moves the equalisation smoothly (on the slow-speed roll-off a 3 % speed
    error is worth a dB). */
void Engine::updateEq (double vCms)
{
    const double fb = std::clamp (std::log (vCms / HeadKernel::B0) / std::log (HeadKernel::BR), 0.0, (double) (HeadKernel::NB - 1));
    if (std::abs (fb - eqLastFb) < 0.01 && eqS == eqLastS && coat == eqLastC) return;
    eqLastFb = fb; eqLastS = eqS; eqLastC = coat;
    const int b = std::min ((int) fb, HeadKernel::NB - 2);
    const double t = fb - b;
    auto blend = [&] (const ShelfCascade& a, const ShelfCascade& c, ShelfCascade& out) {
        out = a;
        out.g0Db = (1.0 - t) * a.g0Db + t * c.g0Db;
        for (int i = 0; i < ShelfCascade::NS; ++i) out.gDb[i] = (1.0 - t) * a.gDb[i] + t * c.gDb[i];
        out.discretise (fsInt);
    };
    blend (eqRec (eqS, b), eqRec (eqS, b + 1), eqRecA);
    blend (eqRep (eqS, b), eqRep (eqS, b + 1), eqRepA);
}

/*  The record head at a bias and a tape speed, one layer at a time: the G slice
    at the bias amplitude this layer sees and the current speed (both
    log-interpolated between table planes - a slower tape sees more bias cycles
    in the zone and records more completely: +0.6 dB at 19 cm/s against 38.1),
    and the zone kernel - a Gaussian with the measured centroid and rms,
    convolved with the layer's own cell width. */
void Engine::buildRecord (double biasDb, double speedCms)
{
    const double hb = ST->hbOperating * dbToLin (biasDb);
    const double lnA0 = std::log (ST->aGrid[0]), lnSpan = std::log (ST->aGrid[STK_NA - 1]) - lnA0;
    const int c = STK_NM / 2;
    // speed planes are a factor of 2 apart
    const double fv = std::clamp (std::log2 (speedCms / ST->speedCms[0]), 0.0, (double) (STK_NV - 1));
    const int iv = std::min ((int) fv, STK_NV - 2);
    const double tv = fv - iv;
    auto aIndex = [&] (double A, int& ia, double& ta) {
        A = std::clamp (A, ST->aGrid[0], ST->aGrid[STK_NA - 1]);
        const double fa = (std::log (A) - lnA0) / lnSpan * (STK_NA - 1);
        ia = std::clamp ((int) std::floor (fa), 0, STK_NA - 2);
        ta = std::clamp (fa - ia, 0.0, 1.0);
    };
    auto zone = [&] (const double (*Z)[STK_NL][STK_NA], int l, int ia, double ta) {
        const double lo = (1.0 - ta) * Z[iv][l][ia] + ta * Z[iv][l][ia + 1];
        const double hi = (1.0 - ta) * Z[iv + 1][l][ia] + ta * Z[iv + 1][l][ia + 1];
        return (1.0 - tv) * lo + tv * hi;
    };
    // The rail's zero: the gain-weighted zone centroid at OPERATING bias and the
    // CURRENT speed, so delay = head distance / speed at every speed; moving the
    // bias then shifts the delay slightly, as it does on a real head.
    //
    // The repro level per speed, also at OPERATING bias: what a 1 kHz tone loses
    // through the slab kernels, each slab weighted by its small-signal record gain
    // and its zone (with the zone's own phase); the replay side of a studio machine
    // is aligned per speed. Off the operating bias the playback-weighted level then
    // moves physically (over-bias records deeper, where the head hears less).
    {
        double gx = 0.0, gs = 0.0;
        double gl[NLAYER], xl[NLAYER], wl[NLAYER];
        for (int l = 0; l < NLAYER; ++l)
        {
            int ia; double ta; aIndex (ST->hbOperating * ST->layerFmax[l], ia, ta);
            gl[l] = zone (ST->zoneGain, l, ia, ta);
            xl[l] = zone (ST->zoneX, l, ia, ta);
            wl[l] = std::max (0.0, zone (ST->zoneW, l, ia, ta));
            gx += gl[l] * xl[l]; gs += gl[l];
        }
        xRefUm = gs > 0.0 ? gx / gs : 0.0;
        const double k1 = 2.0 * M_PI * 1000.0 / (speedCms * 1.0e4);
        double wr = 0.0, wi = 0.0, lr = 0.0, li = 0.0;
        for (int l = 0; l < NLAYER; ++l)
        {
            const double w = gl[l] * std::exp (-0.5 * std::pow (k1 * wl[l], 2.0));
            const double ph = -k1 * (xl[l] - xRefUm);
            const double L = (hk != nullptr) ? headResponse (l, k1, HeadKernel::bucket (speedCms)) : slabResponse (l, k1, speedCms, coat);
            wr += w * std::cos (ph); wi += w * std::sin (ph);
            lr += w * L * std::cos (ph); li += w * L * std::sin (ph);
        }
        const double without = std::hypot (wr, wi), with = std::hypot (lr, li);
        pbTrimSS = with > 0.0 ? without / with : 1.0;
        pbTrim = pbKappa * pbTrimSS;
    }

    for (int l = 0; l < NLAYER; ++l)
    {
        LayerRecord& r = rec[l];
        // each slab has its own old-state grid (uniform, symmetric about zero)
        double msym[STK_NM];
        for (int i = 0; i < STK_NM; ++i) msym[i] = 0.5 * (ST->mRem[l][i] - ST->mRem[l][STK_NM - 1 - i]);
        const double dM = (msym[STK_NM - 1] - msym[0]) / (STK_NM - 1);
        const double A = std::clamp (hb * ST->layerFmax[l], ST->aGrid[0], ST->aGrid[STK_NA - 1]);
        int ia; double ta; aIndex (A, ia, ta);

        float raw[STK_NM][STK_NS];
        for (int im = 0; im < STK_NM; ++im)
            for (int is = 0; is < STK_NS; ++is)
            {
                const double lo = (1.0 - ta) * ST->G[iv][l][ia][im][is] + ta * ST->G[iv][l][ia + 1][im][is];
                const double hi = (1.0 - ta) * ST->G[iv + 1][l][ia][im][is] + ta * ST->G[iv + 1][l][ia + 1][im][is];
                raw[im][is] = (float) ((1.0 - tv) * lo + tv * hi);
            }
        // G is odd: G(-M, -s) = -G(M, s). Antisymmetrise the slice so silence on
        // erased tape records EXACTLY zero, and so table noise cannot add DC.
        for (int im = 0; im < STK_NM; ++im)
            for (int is = 0; is < STK_NS; ++is)
                r.g[im][is] = 0.5f * (raw[im][is] - raw[STK_NM - 1 - im][STK_NS - 1 - is]);
        // Resample on a uniform s grid, 4.9 mHc apart (0 VU spans 38 steps), so a
        // cell's lookup is one multiply instead of an asinh, and linear between
        // these points is -120 dB from the cubic. Antisymmetry survives: the grid
        // is symmetric about s = 0 and so is the cubic through symmetric data.
        for (int im = 0; im < STK_NM; ++im)
            for (int i = 0; i < LayerRecord::NSF; ++i)
            {
                const double sv = -LayerRecord::SMAX + i * (2.0 * LayerRecord::SMAX / (LayerRecord::NSF - 1));
                const double fs = sIndex (sv);
                const int is = std::min ((int) fs, STK_NS - 2);
                const double ts = fs - is;
                const float* g = r.g[im];
                const int a = std::max (is - 1, 0), d = std::min (is + 2, STK_NS - 1);
                r.gf[im][i] = (float) CR (g[a], g[is], g[is + 1], g[d]).at (ts);
            }
        for (int im = 0; im < STK_NM; ++im) r.gf[im][LayerRecord::NSF / 2] = r.g[im][STK_NS / 2];   // s = 0 exactly
        r.dM = dM;
        r.mLo = -0.5 * (STK_NM - 1) * dM;

        r.amp = A;
        r.xUm = zone (ST->zoneX, l, ia, ta);
        r.wUm = std::max (0.0, zone (ST->zoneW, l, ia, ta));
        r.gain = (r.g[c][STK_NS / 2 + 1] - r.g[c][STK_NS / 2 - 1]) / (ST->sGrid[STK_NS / 2 + 1] - ST->sGrid[STK_NS / 2 - 1]);

        const double B = (double) (1 << l);
        const double mu = 0.5 * B + r.xUm / CELL_UM;                   // kernel centre, from the cell's first surface cell
        // The kernel is the zone's own. The playback reconstructs each layer with a
        // smooth (Catmull-Rom) curve through its cells, so the cells are POINT
        // samples and take no box of their own: adding the cell's box here as well
        // cost 0.5 dB at 15 kHz at 19 cm/s against the reference.
        // The deep layers' cells are coarse (2 and 4 um). Where a layer's own
        // Nyquist frequency (speed / two cells) falls inside the audio band - only
        // the 2 um layer below 9.5 cm/s and the 4 um layer below 19 cm/s - the
        // kernel is widened to be 60 dB down there, faded in as the Nyquist drops
        // from 24 to 16 kHz. Measured: a 15 kHz tone at 4.75 cm/s folded to -25 dB
        // without it. It costs top end only in those layers at those speeds, where
        // the playback thickness loss (stage 4) removes those wavelengths anyway.
        // 0.3 cells only guards a zero width.
        const double fNyq = speedCms * 1.0e4 / (2.0 * B * CELL_UM);
        const double ua = std::clamp ((24000.0 - fNyq) / 8000.0, 0.0, 1.0);
        const double aa = ua * ua * (3.0 - 2.0 * ua);
        const double sig = std::max ({ r.wUm / CELL_UM, aa * 1.18 * B, 0.3 });
        int j0 = (int) std::floor (mu - 4.0 * sig), j1 = (int) std::ceil (mu + 4.0 * sig);   // 4 sigma: the dropped mass is 6e-5
        if (j1 - j0 + 1 > LayerRecord::MAXTAPS) { const int mid = (int) std::lround (mu); j0 = mid - LayerRecord::MAXTAPS / 2; j1 = j0 + LayerRecord::MAXTAPS - 1; }
        double sum = 0.0;
        r.tapFirst = j0; r.nTaps = j1 - j0 + 1;
        for (int j = j0; j <= j1; ++j)
        {
            const double w = Phi ((j + 1 - mu) / sig) - Phi ((j - mu) / sig);
            r.w[j - j0] = (float) w; sum += w;
        }
        for (int k = 0; k < r.nTaps; ++k) r.w[k] = (float) (r.w[k] / sum);
    }
    recBiasDb = biasDb;
    recSpeed = speedCms;
    ++recBuilds;
    // the particle noise's colour per slab follows the slabs' spacing correction at this speed
    noiseColour (slabSpacingCorrectionUm (speedCms, coat), gCol);
}

/*  New remanence for one layer cell: Catmull-Rom along s (the table is fine
    there but the curve's slope must not kink), linear along M_old. */
float Engine::recordCell (const LayerRecord& r, float mOld, double s) const
{
    const double fm = std::clamp ((mOld - r.mLo) / r.dM, 0.0, (double) (STK_NM - 1));
    const int im = std::min ((int) fm, STK_NM - 2);
    const double tm = fm - im;
    constexpr double K = (LayerRecord::NSF - 1) / (2.0 * LayerRecord::SMAX);
    const double fs = std::clamp ((s + LayerRecord::SMAX) * K, 0.0, (double) (LayerRecord::NSF - 1));
    const int is = std::min ((int) fs, LayerRecord::NSF - 2);
    const double ts = fs - is;
    const float* g0 = r.gf[im];
    const double v0 = g0[is] + ts * (g0[is + 1] - g0[is]);
    if (tm <= 0.0) return (float) v0;
    const float* g1 = r.gf[im + 1];
    const double v1 = g1[is] + ts * (g1[is + 1] - g1[is]);
    return (float) std::clamp (v0 + tm * (v1 - v0), -1.05, 1.05);
}

//==============================================================================
void Engine::prepare (double hostSampleRate, int)
{
    fsHost = hostSampleRate;
    if (quality == 0)      os = 1;
    else if (quality == 2) os = fsHost < 80000.0 ? 4 : fsHost < 160000.0 ? 2 : 1;
    else                   os = fsHost < 80000.0 ? 2 : 1;
    tele.quality = quality;
    fsInt = fsHost * os;
    dt = 1.0 / fsInt;
    // built once per internal rate and coating, shared by every instance
    for (int k = 0; k < NUM_COATINGS; ++k) hkC[k] = &HeadKernel::get (fsInt, k);
    HeadShaper::prepareTables();
    om20 = 2.0 * M_PI * std::min (20000.0, 0.45 * fsInt) / fsInt;
    selectStock ((int) std::lround (params.stock));
    {
        for (int k = 0; k < NUM_COATINGS; ++k)
        {
            const EqTables& T = eqTables (k);
            for (int s = 0; s < 2; ++s)
                for (int b = 0; b < HeadKernel::NB; ++b)
                {
                    eqBuf[(size_t) ((k * 4 + s) * HeadKernel::NB + b)] = T.rec[s][b];
                    eqBuf[(size_t) ((k * 4 + 2 + s) * HeadKernel::NB + b)] = T.rep[s][b];
                }
        }
        for (auto& st : recEqS) st.reset();
        for (auto& st : repEqS) st.reset();
        eqLastFb = -1.0; eqLastS = -1;
        eqS = params.eqStd > 0.5f ? 1 : 0;
        updateEq (params.speed);
    }

    for (auto& h : up) { h.design(); h.clear(); }
    for (auto& h : dn) { h.design(); h.clear(); }
    for (auto& h : up2) { h.design(); h.clear(); }
    for (auto& h : dn2) { h.design(); h.clear(); }

    // The half-band pair's delay, MEASURED with an impulse: a symmetric FIR's
    // centroid is its exact group delay. Up half paid at the record head, the
    // down half (plus the read box and its correction FIR) by reading ahead.
    double hbDelay = 0.0;
    if (os >= 2)
    {
        HalfBand u, d, u2, d2; u.design(); d.design(); u2.design(); d2.design(); u.clear(); d.clear(); u2.clear(); d2.clear();
        double num = 0.0, den = 0.0;
        for (int n = 0; n < 512; ++n)
        {
            float a, b;
            u.up (n == 0 ? 1.0f : 0.0f, a, b);
            double y;
            if (os == 4)
            {
                float a0, a1, b0, b1;
                u2.up (a, a0, a1); u2.up (b, b0, b1);
                const float m0 = d2.down (a0, a1);   // sequenced: argument order is unspecified
                const float m1 = d2.down (b0, b1);
                y = d.down (m0, m1);
            }
            else y = d.down (a, b);
            num += n * y; den += y;
        }
        hbDelay = num / den;
    }
    inLag = (int) std::lround (0.5 * hbDelay * os);
    leadS = 0.5 * hbDelay / fsHost + 0.5 * dt + dt;
    {
        const double x = 16000.0 * dt;
        const double inv = (M_PI * x) / std::sin (M_PI * x);
        const double a = (inv - 1.0) / (2.0 * (1.0 - std::cos (2.0 * M_PI * x)));
        for (auto& f : comp) { f.a = a; f.reset(); }
        for (auto& f : compS) { f.a = a; f.reset(); }
    }

    aGain  = 1.0 - std::exp (-dt / 0.020);
    aRec   = 1.0 - std::exp (-dt / 0.005);           // the record relay: bias and signal in a few ms
    lcA    = std::exp (-2.0 * M_PI * 5.0 / fsInt);  // the replay amplifier's integrator stops near DC
    for (int i = 0; i < 4; ++i) lcX[i] = lcY[i] = 0.0;
    for (int i = 0; i < 2; ++i) lcSX[i] = lcSY[i] = 0.0;
    aHost  = 1.0 - std::exp (-1.0 / (fsHost * 0.020));
    aVu    = 1.0 - std::exp (-1.0 / (fsHost * 0.050));
    gByp = params.bypass > 0.5f ? 1.0 : 0.0; msIn = msOut = 0.0;
    mech.prepare (fsHost, 0x4D454348ull); mechOn = false;

    // ---- the record head and its alignment, at OPERATING bias and the machine's
    //      NOMINAL speed (19.05 cm/s), as a technician would align it: record level
    //      so 0 VU = 250 nWb/m, repro level so 250 nWb/m = 0 VU. Other biases and
    //      speeds then change the level and the distortion, as they do on tape.
    //      Every stock is aligned the same way, to its own operating level (a fresh
    //      stock's tables and level; the shedding stock is the studio tape on a machine
    //      aligned for it fresh), and the result kept per stock.
    for (int st = 0; st < NUM_STOCKS; ++st)
    {
    selectStock (st);
    pbKappa = 1.0;
    const double hs0 = ST->hs0vu * (SM->opLevelNwbm / REF_FLUX);
    buildRecord (0.0, ST->alignSpeedCms);
    {
        double ss = 0.0; const int K = 720;
        for (int k = 0; k < K; ++k)
        {
            const double s = hs0 * std::sin (2.0 * M_PI * (k + 0.5) / K);
            double m = 0.0;
            for (int l = 0; l < NLAYER; ++l) m += recordCell (rec[l], 0.0f, s);
            m /= NLAYER;
            ss += m * m;
        }
        outK = 0.12589254117941673 / std::sqrt (ss / K);
        // The repro trim weights the slabs by their small-signal gains; at 0 VU the
        // slabs' shares differ a little. Measure the ratio once, here, at the
        // alignment (operating bias, nominal speed), and keep it at every speed.
        pbKappa = 1.0;
        double ml[NLAYER] {};
        for (int l = 0; l < NLAYER; ++l)
        {
            double s2 = 0.0;
            for (int k = 0; k < K; ++k)
            {
                const double sv = hs0 * std::sin (2.0 * M_PI * (k + 0.5) / K);
                const double m = recordCell (rec[l], 0.0f, sv);
                s2 += m * m;
            }
            ml[l] = std::sqrt (s2 / K);
        }
        const double k1 = 2.0 * M_PI * 1000.0 / (ST->alignSpeedCms * 1.0e4);
        const int b = HeadKernel::bucket (ST->alignSpeedCms);
        double wr = 0.0, wi = 0.0, lr = 0.0, li = 0.0;
        for (int l = 0; l < NLAYER; ++l)
        {
            const double w = ml[l] * std::exp (-0.5 * std::pow (k1 * rec[l].wUm, 2.0));
            const double ph = -k1 * (rec[l].xUm - xRefUm);
            const double L = headResponse (l, k1, b);
            wr += w * std::cos (ph); wi += w * std::sin (ph);
            lr += w * L * std::cos (ph); li += w * L * std::sin (ph);
        }
        const double exact = std::hypot (wr, wi) / std::hypot (lr, li);
        pbKappa = exact / pbTrimSS;
    }
    outKs[st] = outK; pbKappaS[st] = pbKappa;
    }
    selectStock ((int) std::lround (params.stock));
    computeEff();
    biasDbSm = eff.bias;
    // (the record head for the current speed is built once the motor speed is set, below)

    // ---- a fresh tape for the transport as it stands
    delete store; store = nullptr;
    delete pending.exchange (nullptr);
    delete retired.exchange (nullptr);
    wantCells = 0;

    mode  = params.mode > 0.5f ? 1 : 0;
    for (int i = 0; i < NUM_HEADS; ++i) { clog[i] = Clog {}; clog[i].rng = 0xC10600ull + 0x9E3779B97F4A7C15ull * (uint64_t) (i + 1); creaseNow[i] = 0.0; }
    loopM = std::clamp (std::round (params.loopLen / 0.05f) * 0.05, 0.5, 6.0);
    pendingLoopM = loopM; loopSettle = 1.0;

    bool clampedNow = false, syncNow = false;
    v = vTarget = targetSpeedCms (clampedNow, syncNow) * CELLS_PER_CM;
    tr.prepare (fsInt);
    tr.reset (v / CELLS_PER_CM);
    {
        // the W&F meter at 1 kHz: an approximation of the IEC 60386 weighting (a
        // band-pass at 4 Hz, unity at its peak) and a 2-second mean square
        wfDiv = std::max (1, (int) std::lround (fsInt / 1000.0));
        const double f0 = 4.0, q = 0.6, fsm = fsInt / wfDiv;
        const double w0 = 2.0 * M_PI * f0 / fsm, al = std::sin (w0) / (2.0 * q), cs = std::cos (w0), nn = 1.0 + al;
        wfB0 = al / nn; wfB2 = -al / nn; wfA1 = -2.0 * cs / nn; wfA2 = (1.0 - al) / nn;
        wfZ1 = wfZ2 = 0.0; wfMs = 0.0; wfPct = -1.0; wfCnt = 0; wfW = 0; wfSettle = 0; wfN = 0.0;
        wfA = 1.0 - std::exp (-1.0 / (fsm * 2.0));
        for (int i = 0; i < WF_HIST; ++i) wfHist[i] = v / CELLS_PER_CM;
    }
    buildRecord (biasDbSm, v / CELLS_PER_CM);
    updateMedium (eff, 0);
    packSm = packTgt;
    for (int i = 0; i < NUM_HEADS; ++i)
    {
        const float* pos[NUM_HEADS] = { &params.h1Pos, &params.h2Pos, &params.h3Pos };
        hPos[i] = std::clamp ((double) *pos[i], HEAD_MIN_CM, HEAD_MAX_CM);
        hVel[i] = 0.0;
    }
    {
        double start[NUM_HEADS];
        for (int i = 0; i < NUM_HEADS; ++i) start[i] = hPos[i] - 1.0e-9 * (NUM_HEADS - i);
        separateHeads (start);
        for (int i = 0; i < NUM_HEADS; ++i) hPos[i] = std::clamp (hPos[i], HEAD_MIN_CM, HEAD_MAX_CM);
    }

    const int64_t need = cellsNeeded (mode, loopM);
    adoptStore (new TapeStore (need), mode, need);
    spliceState = 0;
    wetFade = 1.0;
    recFade = 1.0;

    for (auto& b : lowS)  b.reset();
    for (auto& b : highS) b.reset();
    bassDb = params.bass; trebDb = params.treble;
    for (int c = 0; c < 2; ++c)
    {
        lowS[c].shelf (false, fsInt, 200.0, bassDb);
        highS[c].shelf (true, fsInt, 3000.0, trebDb);
    }

    {
        const double maxS = HEAD_MAX_CM / 4.6 + 0.05;
        int cap = 1;
        while (cap < (int) std::ceil (maxS * fsHost) + 8) cap <<= 1;
        for (auto& r : dryRing) r.assign ((size_t) cap, 0.0f);
        dryMask = cap - 1; dryW = 0;
        reproLat = 0;
        dryD = dryDOld = reproTarget (params); dryXf = 0;
        dryXfLen = std::max (1, (int) std::lround (0.020 * fsHost));
        reproLatency.store (dryD);
    }
    process (nullptr, nullptr, nullptr, nullptr, 0);
    recEnv = recTgt; heSm = heTgt;                   // the heads start where the controls put them
    buildDub (heSm, biasDbSm, std::max (v / CELLS_PER_CM, 4.6), recTgt > 0.5);
    gDry = (params.killDry > 0.5f) ? 0.0 : std::cos (params.mix * M_PI * 0.5);
    gWet = std::sin (params.mix * M_PI * 0.5);
    gOut = dbToLin (params.output);
}

void Engine::adoptStore (TapeStore* s, int newMode, int64_t cells)
{
    if (store != nullptr)
    {
        TapeStore* prev = retired.exchange (store);
        delete prev;   // only reachable from prepare(), where nothing else runs
    }
    store = s;
    viewStore.store (s, std::memory_order_release);
    N = cells;
    mode = newMode;

    T = 0.0; kTick = 0;
    for (double& x : Thist) x = 0.0;
    const double lead = v * leadS - xRefUm / CELL_UM;
    for (int i = 0; i < NUM_HEADS; ++i)
    {
        uPrev[i] = T - hPos[i] * CELLS_PER_CM + lead + HeadShaper::R * (v - hVel[i] * CELLS_PER_CM) * dt;
        shaper[i].reset(); shOff[i] = false;
        for (auto& d : drop[i]) d.seg = INT64_MIN;
    }
    for (int l = 0; l < NLAYER; ++l) { nqK[l] = INT64_MIN; epsJ[l] = INT64_MIN; }
    lastPass = 0;
    wPrevU1 = T;
    ePos = T + ERASE_CM * CELLS_PER_CM;
    vPos = T + SUPPLY_CM * CELLS_PER_CM;
    for (int c = 0; c < 2; ++c) { sHist[c][0] = sHist[c][1] = sHist[c][2] = 0.0; wAcc[c] = 0.0; wLen[c] = 0.0; }
    std::fill (fieldBuf.get(), fieldBuf.get() + 2 * FIELD, 0.0f);
    for (auto& k : nextCell) k = 0;
    std::fill (preBuf.get(), preBuf.get() + (size_t) 2 * NLAYER * PRE_RING, 0.0);
    for (auto& r : pRun) for (double& x : r) x = 0.0;
    for (auto& c : cNext) c = 0;
    std::fill (preCBuf.get(), preCBuf.get() + (size_t) 2 * PRE_RING, 0.0);
    pRunC[0] = pRunC[1] = 0.0;
    std::fill (preSBuf.get(), preSBuf.get() + (size_t) 2 * PRE_RING, 0.0);
    pRunS[0] = pRunS[1] = 0.0;
    for (int i = 0; i < NUM_HEADS; ++i) for (int k = 0; k < 4; ++k) edgePrev[i][k] = edgeNext[i][k] = 0.0;

    cNextC = 0;
    recFade = 0.0;
    resetAhead (T + ERASE_CM * CELLS_PER_CM);
    syncFrom = syncHi = (int64_t) std::floor (T + SYNC_CM * CELLS_PER_CM);
    sPrevC = (double) syncHi; sPrevV[0] = sPrevV[1] = 0.0;
    uPrevS = T + SYNC_CM * CELLS_PER_CM - xRefUm / CELL_UM;
    syncRun = false; syncLive = false; gSync = 0.0;
    for (int c = 0; c < 2; ++c) { syncEqS[c] = ShelfState {}; compS[c].reset(); lcSX[c] = lcSY[c] = 0.0; }

    tele.mode = mode;
    tele.loopLenM = loopM;
}

void Engine::service()
{
    if (TapeStore* r = retired.exchange (nullptr, std::memory_order_acq_rel))
        delete r;
    const int64_t want = wantCells.load (std::memory_order_acquire);
    if (want > 0 && pending.load (std::memory_order_acquire) == nullptr)
    {
        pending.store (new TapeStore (want), std::memory_order_release);
        wantCells.store (0, std::memory_order_release);
    }
}

//==============================================================================
double Engine::readPoint (const float* tr, int64_t ring, double u) const
{
    const double x = u - 0.5;
    const double fl = std::floor (x);
    const int64_t i = (int64_t) fl;
    const double t = x - fl;
    CR seg (tr[wrapTo (i - 1, ring)], tr[wrapTo (i, ring)], tr[wrapTo (i + 1, ring)], tr[wrapTo (i + 2, ring)]);
    return seg.at (t);
}

/*  Mean of the Catmull-Rom interpolant over [a, b] in this layer's own cells,
    both tracks in one pass (they share every index). */
void Engine::readAvg2 (const float* t0, const float* t1, int64_t ring, double a, double b, double& r0, double& r1) const
{
    if (b < a) std::swap (a, b);
    const double w = b - a;
    if (w < 1.0e-3) { r0 = readPoint (t0, ring, 0.5 * (a + b)); r1 = readPoint (t1, ring, 0.5 * (a + b)); return; }

    const double xa = a - 0.5, xb = b - 0.5;
    const double fa = std::floor (xa), fb = std::floor (xb);
    const int64_t ia = (int64_t) fa, ib = (int64_t) fb;
    const double ta = xa - fa, tb = xb - fb;
    int64_t k = wrapTo (ia - 1, ring);
    double p[4], q[4];
    for (int i = 0; i < 4; ++i) { p[i] = t0[k]; q[i] = t1[k]; if (++k == ring) k = 0; }
    auto shift = [&] { p[0] = p[1]; p[1] = p[2]; p[2] = p[3]; q[0] = q[1]; q[1] = q[2]; q[2] = q[3];
                       p[3] = t0[k]; q[3] = t1[k]; if (++k == ring) k = 0; };
    if (ia == ib)
    {
        CR s (p[0], p[1], p[2], p[3]), u (q[0], q[1], q[2], q[3]);
        r0 = (s.prim (tb) - s.prim (ta)) / w; r1 = (u.prim (tb) - u.prim (ta)) / w;
        return;
    }
    double s0, s1;
    { CR s (p[0], p[1], p[2], p[3]), u (q[0], q[1], q[2], q[3]); s0 = s.prim (1.0) - s.prim (ta); s1 = u.prim (1.0) - u.prim (ta); }
    for (int64_t i = ia + 1; i < ib; ++i)
    {
        shift();
        s0 += (-p[0] + 13.0 * p[1] + 13.0 * p[2] - p[3]) * (1.0 / 24.0);
        s1 += (-q[0] + 13.0 * q[1] + 13.0 * q[2] - q[3]) * (1.0 / 24.0);
    }
    shift();
    { CR s (p[0], p[1], p[2], p[3]), u (q[0], q[1], q[2], q[3]); s0 += s.prim (tb); s1 += u.prim (tb); }
    r0 = s0 / w; r1 = s1 / w;
}

/*  The playback head sees the whole coating: here, the mean of the four layers
    (stage 4 weights them by their distance from the head - thickness loss). */
void Engine::readLayers (double a, double b, double& outL, double& outR) const
{
    double sL = 0.0, sR = 0.0;
    for (int l = 0; l < NLAYER; ++l)
    {
        const double sc = 1.0 / (double) (1 << l);
        double r0, r1;
        readAvg2 (store->m[0][l].data(), store->m[1][l].data(), N >> l, a * sc, b * sc, r0, r1);
        sL += r0; sR += r1;
    }
    outL = sL * (1.0 / NLAYER); outR = sR * (1.0 / NLAYER);
}

/*  The record signal (deep-gap field) onto the field ring at the gap: each
    surface cell gets the signal's mean over its own transit, from the
    Catmull-Rom cubic through the samples around it. */
void Engine::writeField (int c, double u0, double u1, double p0, double p1, double p2, double p3)
{
    const double du = u1 - u0;
    if (du <= 0.0) return;
    const CR seg (p0, p1, p2, p3);
    double pos = u0, Pa = 0.0;
    for (;;)
    {
        const double boundary = std::floor (pos) + 1.0;
        if (boundary > u1) break;
        const double tb = (boundary - u0) / du;
        const double Pb = seg.prim (tb);
        wAcc[c] += du * (Pb - Pa);
        wLen[c] += boundary - pos;
        if (wLen[c] > 0.0)
            field[c][(int64_t) std::floor (pos) & FMASK] = (float) (wAcc[c] / wLen[c]);
        wAcc[c] = 0.0; wLen[c] = 0.0;
        pos = boundary; Pa = Pb;
    }
    wAcc[c] += du * (seg.prim (1.0) - Pa);
    wLen[c] += u1 - pos;
}

/*  Finish every layer cell whose recording zone the field ring has fully
    passed: average the field over the zone, then look the new remanence up in
    the reference's table with the cell's old remanence. */
void Engine::finishCells()
{
    const int64_t frontier = (int64_t) std::floor (wPrevU1);     // field cells below this are written
    for (int l = 0; l < NLAYER; ++l)
    {
        const LayerRecord& r = rec[l];
        const int64_t B = (int64_t) 1 << l;
        const int64_t ring = N >> l;
        const double sig = noiseSig[l];
        const double* gc = gCol[l];
        const bool loop = mode == 1;
        int64_t k = nextCell[l];
        int64_t idx = wrapTo (k, ring);
        for (;; ++k, idx = (idx + 1 == ring) ? 0 : idx + 1)
        {
            const int64_t base = k * B + r.tapFirst;
            if (base + r.nTaps - 1 >= frontier) break;
            // both tracks in one pass over the taps; a direct pointer unless the
            // kernel straddles the end of the field ring
            double s[2] = { 0.0, 0.0 };
            const int64_t b0 = base & FMASK;
            if (b0 + r.nTaps <= FIELD)
            {
                const float* p0 = field[0] + b0;
                const float* p1 = field[1] + b0;
                dot2 (r.w, p0, p1, r.nTaps, s[0], s[1]);
            }
            else
                for (int j = 0; j < r.nTaps; ++j)
                {
                    const int64_t q = (base + j) & FMASK;
                    s[0] += r.w[j] * field[0][q]; s[1] += r.w[j] * field[1][q];
                }
            // the coating's density here (modulation noise: a property of the tape, so a
            // loop brings it round every pass) and the particles' orientations (a fresh
            // draw every time a cell is recorded: k is unwrapped, a loop's next pass is
            // a new k), coloured along the slab to the continuous coating's spectrum
            const double eps = (modEff > 0.0) ? modEff * epsAt (l, (double) ((loop ? idx : k) * B)) : 0.0;
            double nz0 = 0.0, nz1 = 0.0;
            if (sig > 0.0)
            {
                double* qa = nq[0][l]; double* qb = nq[1][l];
                if (nqK[l] == k - 1)
                {
                    qa[0] = qa[1]; qa[1] = qa[2]; qa[2] = qa[3]; qa[3] = qa[4];
                    qb[0] = qb[1]; qb[1] = qb[2]; qb[2] = qb[3]; qb[3] = qb[4];
                    unitNoise2 (k + 2, l, qa[4], qb[4]);
                }
                else if (nqK[l] != k)
                    for (int i = 0; i < 5; ++i) unitNoise2 (k - 2 + i, l, qa[i], qb[i]);
                nqK[l] = k;
                const double* q0 = nq[0][l]; const double* q1 = nq[1][l];
                nz0 = sig * (gc[0] * q0[2] + gc[1] * (q0[1] + q0[3]) + gc[2] * (q0[0] + q0[4]));
                nz1 = sig * (gc[0] * q1[2] + gc[1] * (q1[1] + q1[3]) + gc[2] * (q1[0] + q1[4]));
            }
            // record and bias off, and no erase field: the cell keeps what it had (the LOOP
            // STATE PLAY); the relay's ramp blends, the bias fading in or out over a few ms of tape
            if (recEnv <= 0.0 && ! dubPass) continue;
            const DubSlice& D = dub[l];
            for (int c = 0; c < 2; ++c)
            {
                float& m = store->m[c][l][(size_t) idx];
                if (! loop)
                {
                    // a REEL: virgin tape at the record head
                    const double mr = recordCell (r, m, s[c]);
                    // binomial statistics: a saturated cell has every particle aligned, so no noise
                    const double nz = c == 0 ? nz0 : nz1;
                    const double mw = mr * (1.0 + eps) + nz * std::sqrt (std::max (0.0, 1.0 - mr * mr));
                    m = (float) (recEnv >= 1.0 ? mw : m + recEnv * (mw - m));
                    recPk = std::max (recPk, (float) std::abs (mr));
                    continue;
                }
                // A LOOP's cell holds the COMMITTED take - what survives beyond the reach the
                // heads have already paid for - and what the last pass recorded on TOP of it.
                // A sweep takes from the committed take only what it has not taken before
                // (one shot: the reference's E1) and replaces the top (E3), keeping its
                // fringe as a ghost. Material recorded at a higher reach than this pass is
                // committed material now. 10-flux-memory.md.
                float& md = store->md[c][l][(size_t) idx];
                uint8_t& rp = store->rp[c][l][(size_t) idx];
                uint8_t& ta = store->ta[c][l][(size_t) idx];
                double mdn = md, topn = 0.0;
                const double top = (double) m - md;
                bool touched = false;
                if (md != 0.0f)
                {
                    const uint8_t k = D.paidIndex (std::abs ((double) md));
                    if (k > rp) { mdn = (k == 255) ? 0.0 : md * (RQ.v[k] / RQ.v[rp]); touched = true; }
                }
                if (top != 0.0)
                {
                    // either way the top's survivors lie beyond this pass's reach, so they join
                    // the committed material (a later pass at the same reach takes nothing
                    // more: the reference's E1)
                    if ((int) ta >= D.ia + 2) mdn += top * RQ.v[D.paidIndex (std::abs (top))];   // recorded at a higher reach: committed material now
                    else                      mdn += top * RQ.v[D.topIndex (std::abs (top))];    // recorded at this reach: replaced, its fringe left as a ghost
                    touched = true;
                }
                // what has been paid is recorded at the level the next pass will read it at (the
                // ratio moves with level), and never lowered: a pass with a reach below what was
                // paid takes nothing, and a return to the higher reach must not take it twice
                if (touched) rp = std::max (rp, D.paidIndex (std::abs (mdn)));
                double mw = 0.0;
                if (recEnv > 0.0)
                {
                    // the fresh recording, into the hysterons this bias reaches
                    const double mr = recordCell (r, 0.0f, s[c]);
                    const double nz = c == 0 ? nz0 : nz1;
                    mw = mr * (1.0 + eps) + nz * std::sqrt (std::max (0.0, 1.0 - mr * mr));
                    recPk = std::max (recPk, (float) std::abs (mr));
                    ta = (uint8_t) D.ia;
                }
                const double mNew = mdn + topn + mw;
                if (recEnv >= 1.0 || recEnv <= 0.0) { m = (float) mNew; md = (float) mdn; }
                else { m = (float) (m + recEnv * (mNew - m)); md = (float) (md + recEnv * (mdn - md)); }
            }
        }
        nextCell[l] = k;
    }
    finishCoarse();
}

/*  What the read does to a sinusoid of wavenumber k on one slab: its near taps
    (at phase 0), its own boxes and its share of the combined tail - the same
    numbers readHead applies, so the repro trim matches the read exactly. */
double Engine::headResponse (int l, double k, int b) const
{
    const HeadKernel& K = *hk;
    const double p = CELL_UM * (double) (1 << l);
    const float* t = K.tapRow (l, b, 0);
    double r = 0.0;
    for (int j = -K.N[l]; j <= K.N[l] + 1; ++j) r += t[j + K.N[l]] * std::cos (k * j * p);
    auto box = [k] (double lo, double hi) { return (k < 1.0e-12) ? 1.0 : (std::sin (k * hi) - std::sin (k * lo)) / (k * (hi - lo)); };
    const float* bm = K.masses (l, b);
    for (int i = 0; i < K.nbox[l]; ++i) r += 2.0 * bm[i] * box (K.boxLo[l][i], K.boxHi[l][i]);
    for (int i = 0; i < K.nboxC; ++i) r += 2.0 * K.cSlab[l] * K.betaC[i] * box (K.boxLoC[i], K.boxHiC[i]);
    return r;
}

/*  The coarse prefix sums of every slab, extended as its cells are finished: a
    4 um coarse cell holds the integral (M x um) of its slab cells. The ring is
    indexed by the unwrapped coarse coordinate, so the chain is continuous over
    everything a head can read. */
void Engine::finishCoarse()
{
    for (int l = 0; l < NLAYER; ++l)
    {
        const int r = 8 >> l;
        const double p = CELL_UM * (double) (1 << l);
        const int64_t ring = N >> l;
        while ((cNext[l] + 1) * r <= nextCell[l])
        {
            const int64_t q = cNext[l];
            int64_t k = wrapTo (q * r, ring);
            double s0 = 0.0, s1 = 0.0;
            for (int i = 0; i < r; ++i)
            {
                s0 += store->m[0][l][(size_t) k]; s1 += store->m[1][l][(size_t) k];
                if (++k == ring) k = 0;
            }
            pRun[0][l] += s0 * p; pRun[1][l] += s1 * p;
            pre (0, l)[(size_t) (q & PRE_MASK)] = pRun[0][l];
            pre (1, l)[(size_t) (q & PRE_MASK)] = pRun[1][l];
            ++cNext[l];
        }
    }
    // the combined tail track, once every slab has finished a coarse cell
    int64_t ready = cNext[0];
    for (int l = 1; l < NLAYER; ++l) ready = std::min (ready, cNext[l]);
    while (cNextC < ready)
    {
        const size_t iq = (size_t) (cNextC & PRE_MASK), ip = (size_t) ((cNextC - 1) & PRE_MASK);
        for (int c = 0; c < 2; ++c)
        {
            double s = 0.0;
            for (int l = 0; l < NLAYER; ++l) s += hk->cSlab[l] * (pre (c, l)[iq] - (cNextC > 0 ? pre (c, l)[ip] : 0.0));
            pRunC[c] += s;
            preC (c)[iq] = pRunC[c];
            double sd = 0.0;
            for (int l = 0; l < NLAYER; ++l) sd += pre (c, l)[iq] - (cNextC > 0 ? pre (c, l)[ip] : 0.0);
            pRunS[c] += sd;
            preS (c)[iq] = pRunS[c];
        }
        ++cNextC;
    }
}

/*  DUB's sync head reads tape the record head has not reached yet: the last pass. Its
    kernel tails need prefix sums there, built straight from the store as the tape comes
    up - before the erase head (2.5 cm) reaches it, since the chain runs SYNC_AHEAD_CM
    past the sync head (3.2 cm). Same layout as the playback chain (finishCoarse): a 4 um
    coarse cell holds the integral (M x um) of its slab cells; the ring is indexed by the
    unwrapped coarse coordinate. BUGLIST 9. */
void Engine::resetAhead (double fromCells)
{
    const int64_t q0 = (int64_t) std::floor (fromCells / 8.0);
    for (int l = 0; l < NLAYER; ++l) { cNextA[l] = q0; pRunA[0][l] = pRunA[1][l] = 0.0; }
    cNextAC = q0; pRunAC[0] = pRunAC[1] = 0.0;
    std::fill (preABuf.get(), preABuf.get() + (size_t) 2 * NLAYER * PRE_RING, 0.0);
    std::fill (preACBuf.get(), preACBuf.get() + (size_t) 2 * PRE_RING, 0.0);
}

void Engine::extendAhead (double toCells)
{
    const int64_t qT = (int64_t) std::floor (toCells / 8.0);
    // a jump (a splice, a stopped tape restarting far on) restarts the chain rather than
    // grinding through centimetres of coarse cells in one sample
    if (qT - cNextAC > PRE_RING / 4) resetAhead (toCells - (SYNC_AHEAD_CM + SYNC_CM - ERASE_CM) * CELLS_PER_CM);
    for (int l = 0; l < NLAYER; ++l)
    {
        const int r = 8 >> l;
        const double p = CELL_UM * (double) (1 << l);
        const int64_t ring = N >> l;
        while (cNextA[l] < qT)
        {
            const int64_t q = cNextA[l];
            int64_t k = wrapTo (q * r, ring);
            double s0 = 0.0, s1 = 0.0;
            for (int i = 0; i < r; ++i)
            {
                s0 += store->m[0][l][(size_t) k]; s1 += store->m[1][l][(size_t) k];
                if (++k == ring) k = 0;
            }
            pRunA[0][l] += s0 * p; pRunA[1][l] += s1 * p;
            preA (0, l)[(size_t) (q & PRE_MASK)] = pRunA[0][l];
            preA (1, l)[(size_t) (q & PRE_MASK)] = pRunA[1][l];
            ++cNextA[l];
        }
    }
    int64_t ready = cNextA[0];
    for (int l = 1; l < NLAYER; ++l) ready = std::min (ready, cNextA[l]);
    while (cNextAC < ready)
    {
        const size_t iq = (size_t) (cNextAC & PRE_MASK), ip = (size_t) ((cNextAC - 1) & PRE_MASK);
        for (int c = 0; c < 2; ++c)
        {
            double s = 0.0;
            for (int l = 0; l < NLAYER; ++l) s += hk->cSlab[l] * (preA (c, l)[iq] - preA (c, l)[ip]);
            pRunAC[c] += s;
            preAC (c)[iq] = pRunAC[c];
        }
        ++cNextAC;
    }
}

/*  What the sync head read at tape position u (surface cells, unwrapped), track c: linear
    between the cells either side; 0 where it has not read (the first SYNC_CM after a splice). */
double Engine::syncAt (int c, double u) const
{
    const double x = u - 0.5;
    const int64_t i = (int64_t) std::floor (x);
    if (i < syncFrom || i + 1 >= syncHi) return 0.0;
    const double t = x - (double) i;
    const float* b = syncBuf.get() + (size_t) c * SYNC_RING;
    return b[(size_t) (i & SYNC_MASK)] + t * (b[(size_t) ((i + 1) & SYNC_MASK)] - b[(size_t) (i & SYNC_MASK)]);
}

/*  The loop's small-signal gain at fHz, record amplifier to replay amplifier, as the
    tables, the head and the equalisers stand now: each slab's record slope, its zone's
    spread and offset, the head's response to it, the repro trim and both equalisers. */
double Engine::syncLoopGain (double fHz) const
{
    const double vC = std::max (recSpeed, 1.0);
    const double k1 = 2.0 * M_PI * fHz / (vC * 1.0e4);
    double re = 0.0, im = 0.0;
    for (int l = 0; l < NLAYER; ++l)
    {
        const LayerRecord& r = rec[l];
        const double w = r.gain * std::exp (-0.5 * std::pow (k1 * r.wUm, 2.0)) * headResponse (l, k1, hkB);
        const double ph = -k1 * (r.xUm - xRefUm);
        re += w * std::cos (ph); im += w * std::sin (ph);
    }
    double g = std::hypot (re, im) / NLAYER * sigScale * outK * pbTrim * packSm;
    if (eqOn) g *= dbToLin (eqRecA.analogueDb (fHz) + eqRepA.analogueDb (fHz));
    return g;
}

/*  The playback head (stage 4, docs/physics/07-playback.md): every slab through
    its own kernel - near taps on the slab's grid at the head's phase, then the
    1/x^2 tail as box means from the coarse prefix sums. a, b: the tape
    coordinates (surface cells) at the start and end of this sample's transit;
    the transit box is folded into the taps, centred where it always was. */
void Engine::readHead (double a, double b, double& outL, double& outR, bool ahead) const
{
    const HeadKernel& K = *hk;
    const double x = 0.5 * (a + b);
    const double xum = x * CELL_UM;
    double sL = 0.0, sR = 0.0;
    for (int l = 0; l < NLAYER; ++l)
    {
        const int64_t ring = N >> l;
        const double y = x / (double) (1 << l) - 0.5;      // slab cell i sits at i + 0.5
        const double fl = std::floor (y);
        int64_t n = (int64_t) fl;
        int ph = (int) std::lround ((y - fl) * HeadKernel::NPH);
        if (ph == HeadKernel::NPH) { ph = 0; ++n; }
        const float* t = K.tapRow (l, hkB, ph);
        const int nt = K.ntap[l];
        const float* m0 = store->m[0][l].data();
        const float* m1 = store->m[1][l].data();
        const int64_t i0 = wrapTo (n - K.N[l], ring);
        double a0 = 0.0, a1 = 0.0;
        if (i0 + nt <= ring)
        {
            const float* p0 = m0 + i0;
            const float* p1 = m1 + i0;
            dot2 (t, p0, p1, nt, a0, a1);
        }
        else
        {
            int64_t k = i0;
            for (int j = 0; j < nt; ++j) { const double w = t[j]; a0 += w * m0[k]; a1 += w * m1[k]; if (++k == ring) k = 0; }
        }
        // the tail, both sides: a box mean is the prefix difference at its edges over
        // its width; edges are shared (one box's hi is the next one's lo)
        const double* P0 = ahead ? preA (0, l) : pre (0, l);
        const double* P1 = ahead ? preA (1, l) : pre (1, l);
        const float* bm = K.masses (l, hkB);
        auto at = [&] (double um, double& v0, double& v1)
        {
            const double yy = um * (1.0 / COARSE_UM);
            const double fq = std::floor (yy);
            const int64_t q = (int64_t) fq;
            const double fr = yy - fq;
            const size_t iq = (size_t) (q & PRE_MASK), ip = (size_t) ((q - 1) & PRE_MASK);
            v0 = P0[ip] + fr * (P0[iq] - P0[ip]);
            v1 = P1[ip] + fr * (P1[iq] - P1[ip]);
        };
        double rp0, rp1, lp0, lp1;
        at (xum + K.boxLo[l][0], rp0, rp1);
        at (xum - K.boxLo[l][0], lp0, lp1);
        for (int i = 0; i < K.nbox[l]; ++i)
        {
            const double hi = K.boxHi[l][i];
            double r0, r1, q0, q1;
            at (xum + hi, r0, r1);
            at (xum - hi, q0, q1);
            const double w = (double) bm[i] / (hi - K.boxLo[l][i]);
            a0 += w * ((r0 - rp0) + (lp0 - q0));
            a1 += w * ((r1 - rp1) + (lp1 - q1));
            rp0 = r0; rp1 = r1; lp0 = q0; lp1 = q1;
        }
        sL += a0; sR += a1;
    }
    // the combined tail of all four slabs
    {
        const double* P0 = ahead ? preAC (0) : preC (0);
        const double* P1 = ahead ? preAC (1) : preC (1);
        auto at = [&] (double um, double& v0, double& v1)
        {
            const double yy = um * (1.0 / COARSE_UM);
            const double fq = std::floor (yy);
            const int64_t q = (int64_t) fq;
            const double fr = yy - fq;
            const size_t iq = (size_t) (q & PRE_MASK), ip = (size_t) ((q - 1) & PRE_MASK);
            v0 = P0[ip] + fr * (P0[iq] - P0[ip]);
            v1 = P1[ip] + fr * (P1[iq] - P1[ip]);
        };
        double rp0, rp1, lp0, lp1;
        at (xum + K.boxLoC[0], rp0, rp1);
        at (xum - K.boxLoC[0], lp0, lp1);
        for (int i = 0; i < K.nboxC; ++i)
        {
            const double hi = K.boxHiC[i];
            double r0, r1, q0, q1;
            at (xum + hi, r0, r1);
            at (xum - hi, q0, q1);
            const double w = K.betaC[i] / (hi - K.boxLoC[i]);
            sL += w * ((r0 - rp0) + (lp0 - q0));
            sR += w * ((r1 - rp1) + (lp1 - q1));
            rp0 = r0; rp1 = r1; lp0 = q0; lp1 = q1;
        }
    }
    outL = sL * (1.0 / NLAYER); outR = sR * (1.0 / NLAYER);
}

/*  The head's edges and the fringing, from the depth-sum prefix, at a position
    xum (um). e: bump L, bump R, fringing into L, fringing into R (depth sums). */
void Engine::edgeTerms (double xum, double* e) const
{
    // Gaussian-smoothed depth sums, from the depth-sum prefix: nine boxes of s/2
    // over +-2.25 s (five boxes of s aliased the Gaussian's transform back and
    // drew a spurious +0.9 dB ripple at three times the bump's frequency)
    {
        static const double W[9] = { 0.028533, 0.067235, 0.124009, 0.179044, 0.202358, 0.179044, 0.124009, 0.067235, 0.028533 };
        const double* S0 = preS (0);
        const double* S1 = preS (1);
        auto at = [&] (double um, double& v0, double& v1)
        {
            const double yy = um * (1.0 / COARSE_UM);
            const double fq = std::floor (yy);
            const int64_t q = (int64_t) fq;
            const double fr = yy - fq;
            const size_t iq = (size_t) (q & PRE_MASK), ip = (size_t) ((q - 1) & PRE_MASK);
            v0 = S0[ip] + fr * (S0[iq] - S0[ip]);
            v1 = S1[ip] + fr * (S1[iq] - S1[ip]);
        };
        // the mean of the depth sum under a Gaussian of width sg centred at um
        auto gauss = [&] (double um, double sg, double& g0, double& g1)
        {
            const double h = 0.5 * sg;
            double p0, p1;
            at (um - 4.5 * h, p0, p1);
            g0 = g1 = 0.0;
            for (int i = 0; i < 9; ++i)
            {
                double r0, r1;
                at (um + (i - 3.5) * h, r0, r1);
                g0 += W[i] * (r0 - p0); g1 += W[i] * (r1 - p1);
                p0 = r0; p1 = r1;
            }
            g0 /= h; g1 /= h;
        };
        // the head bump: its outer pole edges at +-L read the depth sum with the
        // opposite sign and half the weight each (at millimetre wavelengths every
        // slab weighs the same). Crosstalk fringing: each track also hears the
        // other's depth sum smoothed over the guard band (long wavelengths).
        // Both symmetric like the head's field, so they move no echo in time.
        double eR0, eR1, eL0, eL1, c0, c1;
        gauss (xum + HEAD_EDGE_UM, HEAD_EDGE_SIG_UM, eR0, eR1);
        gauss (xum - HEAD_EDGE_UM, HEAD_EDGE_SIG_UM, eL0, eL1);
        gauss (xum, XT_GUARD_UM, c0, c1);
        e[0] = 0.5 * (eR0 + eL0); e[1] = 0.5 * (eR1 + eL1);
        e[2] = c1; e[3] = c0;        // fringing INTO left comes from the right track
    }
}

/*  Return every cell that has passed `newPos` to the demagnetised state, in
    every layer (the erase head at full field; virgin tape onto a REEL). */
void Engine::clearSpan (double& prevPos, double newPos)
{
    for (int l = 0; l < NLAYER; ++l)
    {
        const double sc = 1.0 / (double) (1 << l);
        const int64_t a = (int64_t) std::floor (prevPos * sc), b = (int64_t) std::floor (newPos * sc);
        const int64_t ring = N >> l;
        if (b <= a) continue;
        int64_t k = wrapTo (a, ring);
        for (int64_t i = a; i < b; ++i)
        {
            store->m[0][l][(size_t) k] = 0.0f; store->m[1][l][(size_t) k] = 0.0f;
            if (mode == 1)
            {
                // the one-shot bookkeeping is a loop's: a reel never reads it (and clears every
                // cell twice, at the erase head and as virgin tape at the supply)
                store->md[0][l][(size_t) k] = 0.0f; store->md[1][l][(size_t) k] = 0.0f;
                store->rp[0][l][(size_t) k] = 0; store->rp[1][l][(size_t) k] = 0;
                store->ta[0][l][(size_t) k] = 0; store->ta[1][l][(size_t) k] = 0;
            }
            if (++k == ring) k = 0;
        }
    }
    prevPos = newPos;
}

double Engine::layerM (int track, int layer, double pathCm) const
{
    const double u = (T - pathCm * CELLS_PER_CM) / (double) (1 << layer);
    return store->m[track][layer][(size_t) wrapTo ((int64_t) std::floor (u), N >> layer)];
}

double Engine::layerMd (int track, int layer, double pathCm) const
{
    const double u = (T - pathCm * CELLS_PER_CM) / (double) (1 << layer);
    return store->md[track][layer][(size_t) wrapTo ((int64_t) std::floor (u), N >> layer)];
}

//==============================================================================
void Engine::separateHeads (const double* before)
{
    int ord[NUM_HEADS] = { 0, 1, 2 };
    std::sort (ord, ord + NUM_HEADS, [&] (int a, int b) { return before[a] < before[b]; });
    for (int k = 0; k + 1 < NUM_HEADS; ++k)
    {
        const int a = ord[k], b = ord[k + 1];
        if (hPos[b] - hPos[a] >= HEAD_SPACING_CM) continue;
        const double ma = hPos[a] - before[a], mb = hPos[b] - before[b];
        if (ma > mb) { hPos[a] = hPos[b] - HEAD_SPACING_CM; hVel[a] = 0.0; }
        else         { hPos[b] = hPos[a] + HEAD_SPACING_CM; hVel[b] = 0.0; }
    }
}

//==============================================================================
void Engine::tick (float xL, float xR, float& yL, float& yR)
{
    // ---- splice
    if (spliceState == 1)
    {
        wetFade -= dt / SPLICE_FADE_S;
        if (wetFade <= 0.0)
        {
            wetFade = 0.0;
            spliceState = 2;
            wantCells.store (cellsNeeded (spliceMode, spliceLoopM), std::memory_order_release);
        }
    }
    else if (spliceState == 2)
    {
        const int64_t need = cellsNeeded (spliceMode, spliceLoopM);
        TapeStore* s = pending.load (std::memory_order_acquire);
        if (s != nullptr && retired.load (std::memory_order_acquire) == nullptr)
        {
            pending.store (nullptr, std::memory_order_release);
            if (s->n0 == need)
            {
                viewStore.store (s, std::memory_order_release);
                TapeStore* old = store;
                store = nullptr;
                retired.store (old, std::memory_order_release);
                loopM = spliceLoopM;
                adoptStore (s, spliceMode, need);
                if (spliceStock != stockIdx) applyStock (spliceStock);   // a new reel of another stock
                clearLeft = 0.0;
                spliceState = 3;
            }
            else
            {
                retired.store (s, std::memory_order_release);
                wantCells.store (need, std::memory_order_release);
            }
        }
    }
    else if (spliceState == 3)
    {
        wetFade += dt / SPLICE_FADE_S;
        if (wetFade >= 1.0) { wetFade = 1.0; spliceState = 0; }
    }

    // ---- the capstan and everything that turns with it (stage 5)
    v = tr.tick (trIn) * CELLS_PER_CM;

    // ---- the W&F meter: record-to-play speed ratio under the first active head
    if (++wfCnt >= wfDiv)
    {
        wfCnt = 0;
        const double vc = v / CELLS_PER_CM;
        wfHist[wfW] = vc;
        int hh = 0;
        for (int i = 0; i < NUM_HEADS; ++i) if (gHead[i] > 1.0e-6) { hh = i; break; }
        const double vm = std::max (tr.motorSpeedCms(), 1.0);
        const int lag = std::min ((int) std::lround (hPos[hh] / vm * (fsInt / wfDiv)), WF_HIST - 1);
        // A speed change is not wow: while the motor is off its target, and until the
        // record-to-play lag and the weighting filter have caught up, the meter waits
        // (and then averages from scratch, so its first reading is not a slow climb).
        const double tgt = std::max (trIn.vTargetCms, 1.0);
        if (std::abs (tr.motorSpeedCms() - tgt) > 0.003 * tgt)
        {
            wfSettle = 0; wfN = 0.0; wfZ1 = wfZ2 = 0.0; wfMs = 0.0; wfPct = -1.0;
        }
        else
        {
            const double wdev = vc / wfHist[(wfW - lag) & WF_MASK] - 1.0;
            const double y = wfB0 * wdev + wfZ1;
            wfZ1 = -wfA1 * y + wfZ2;
            wfZ2 = wfB2 * wdev - wfA2 * y;
            if (wfSettle < lag + 1000) ++wfSettle;        // 1 kHz: the lag, then a second
            else
            {
                wfN += 1.0;
                wfMs += std::max (wfA, 1.0 / wfN) * (y * y - wfMs);
                wfPct = 2.0 * std::sqrt (wfMs) * 100.0;
            }
        }
        wfW = (wfW + 1) & WF_MASK;
    }

    const float* tgtP[NUM_HEADS] = { &params.h1Pos, &params.h2Pos, &params.h3Pos };
    const double headMax = (mode == 1)
        ? std::min (HEAD_MAX_CM, 0.85 * (loopM * 100.0 - ERASE_CM)) : HEAD_MAX_CM;
    constexpr double W = 25.0;
    double before[NUM_HEADS];
    for (int i = 0; i < NUM_HEADS; ++i)
    {
        before[i] = hPos[i];
        const double tgt = std::clamp ((double) *tgtP[i], HEAD_MIN_CM, headMax);
        hVel[i] += (W * W * (tgt - hPos[i]) - 2.0 * W * hVel[i]) * dt;
        hVel[i] = std::clamp (hVel[i], -CARRIAGE_MAX_CMS, CARRIAGE_MAX_CMS);
        hPos[i] += hVel[i] * dt;
    }
    separateHeads (before);

    // ---- the tape moves (T unwrapped; rings index modulo themselves)
    T += v * dt;
    ++kTick;
    Thist[kTick & (HIST - 1)] = T;
    const double lead = v * leadS - xRefUm / CELL_UM;

    // ---- playback
    double outL = 0.0, outR = 0.0, fbL = 0.0, fbR = 0.0;
    const int ph = (int) (kTick % EDGE_EVERY);
    for (int i = 0; i < NUM_HEADS; ++i)
    {
        // the head reads HeadShaper::R samples ahead (where it will be), and the shaper
        // hands the sample back R samples later - its delay paid, as the half-band's is
        const double rel = v - hVel[i] * CELLS_PER_CM;          // tape past the head, cells/s
        const double ahead = HeadShaper::R * rel * dt;
        const double u1 = T - hPos[i] * CELLS_PER_CM + lead + ahead + tr.scrapeUm (i) * (1.0 / CELL_UM);   // stick-slip moves the tape under the head
        const double g = gHead[i];
        // a head switched off and faded out reads nothing (it would add nothing)
        if (g < 1.0e-9)
        {
            uPrev[i] = u1;
            if (! shOff[i]) { shaper[i].reset(); shOff[i] = true; }
            continue;
        }
        shOff[i] = false;
        double hL, hR;
        if (pbOn)
        {
            readHead (uPrev[i], u1, hL, hR);
            if (ph == 0)
            {
                for (int k = 0; k < 4; ++k) edgePrev[i][k] = edgeNext[i][k];
                const double xNext = 0.5 * (uPrev[i] + u1) + EDGE_EVERY * (u1 - uPrev[i]);
                edgeTerms (xNext * CELL_UM, edgeNext[i]);
            }
            const double t = (double) ph / EDGE_EVERY;
            double e[4];
            for (int k = 0; k < 4; ++k) e[k] = (edgePrev[i][k] + t * (edgeNext[i][k] - edgePrev[i][k])) * (1.0 / NLAYER);
            constexpr double fr = 0.0031622776601683794, fl = 0.00056234132519034907;   // -50 dB, -65 dB
            const double l0 = hL - e[0], r0 = hR - e[1];
            hL = l0 + fr * e[2] + fl * r0;
            hR = r0 + fr * e[3] + fl * l0;
        }
        else readLayers (uPrev[i], u1, hL, hR);
        const double kOut = (pbOn ? outK * pbTrim : outK) * packSm;
        hL *= kOut; hR *= kOut;
        // the medium under the head: spacing (roughness, dropouts, wear), holes in the
        // coating, the azimuth's box and the tracks' offset - exact kernels, per track
        if (ph == 0) updateShaper (i, 0.5 * (uPrev[i] + u1) - ahead, std::max (rel * dt * CELL_UM, 1.0e-3));
        uPrev[i] = u1;
        shaper[i].push (hL, hR, hL, hR);
        outL += g * panL[i] * hL;
        outR += g * panR[i] * hR;
        fbL += g * hL;
        fbR += g * hR;
        const float pk = (float) (g * std::max (std::abs (hL), std::abs (hR)));
        if (pk > pkHead[i]) pkHead[i] = pk;
    }
    outL = comp[0].tick (outL); outR = comp[1].tick (outR);
    fbL  = comp[2].tick (fbL);  fbR  = comp[3].tick (fbR);

    // ---- the replay amplifiers' equalisation (the echo send is taken after it)
    if (eqOn)
    {
        const ShelfCascade& R = eqRepA;
        outL = repEqS[0].tick (R, outL); outR = repEqS[1].tick (R, outR);
        fbL  = repEqS[2].tick (R, fbL);  fbR  = repEqS[3].tick (R, fbR);
        // a head is a differentiator and the replay amplifier integrates; its integrator
        // stops near DC, so a stopped tape (no flux change) gives no output
        double* io[4] = { &outL, &outR, &fbL, &fbR };
        for (int i = 0; i < 4; ++i)
        {
            const double x = *io[i];
            lcY[i] = lcA * (lcY[i] + x - lcX[i]); lcX[i] = x;
            *io[i] = lcY[i];
        }
    }
    // ---- the playback amplifier's headroom on the echo send (the output's rail is
    //      applied after decimation, where a clipped waveform cannot overshoot it)
    if (pbOn) { fbL = ampRail (fbL); fbR = ampRail (fbR); }

    // ---- the feedback wire, through the tone stage. No cap: what contains a
    //      loop driven past unity is the tape running out of remanence.
    fbL = highS[0].tick (lowS[0].tick (fbL)) * fbEff;
    fbR = highS[1].tick (lowS[1].tick (fbR)) * fbEff;

    // ---- DUB's sync head (BUGLIST 9): it reads the loop SYNC_CM upstream of the record
    //      head, where the tape still holds the last pass, through the same head, trim,
    //      replay equalisation and amplifier as a playback head. It reads whenever a loop is
    //      recording, so a switch to DUB finds the whole pass already read. What it reads is
    //      kept by TAPE POSITION and goes back into the record amplifier where it was read:
    //      the loop re-recorded in place, and every pass is a generation of the medium.
    if (mode == 1)
    {
        extendAhead (T + (SYNC_CM + SYNC_AHEAD_CM) * CELLS_PER_CM);
        const double u1s = T + SYNC_CM * CELLS_PER_CM - xRefUm / CELL_UM;
        if (syncRun && u1s > uPrevS)
        {
            double sL, sR;
            readHead (uPrevS, u1s, sL, sR, true);
            const double kOut = outK * pbTrim * packSm;
            sL = compS[0].tick (sL * kOut); sR = compS[1].tick (sR * kOut);
            if (eqOn)
            {
                sL = syncEqS[0].tick (eqRepA, sL); sR = syncEqS[1].tick (eqRepA, sR);
                lcSY[0] = lcA * (lcSY[0] + sL - lcSX[0]); lcSX[0] = sL; sL = lcSY[0];
                lcSY[1] = lcA * (lcSY[1] + sR - lcSX[1]); lcSX[1] = sR; sR = lcSY[1];
            }
            sL = ampRail (sL); sR = ampRail (sR);
            // onto the position ring: every cell centre this sample's read passed, linear
            // between this sample and the last
            const double cNow = 0.5 * (uPrevS + u1s);
            if (! syncLive)
            {
                // reading again after a stretch with record off: nothing in between is known
                syncFrom = syncHi = (int64_t) std::ceil (cNow - 0.5);
                sPrevC = cNow; sPrevV[0] = sL; sPrevV[1] = sR;
                syncLive = true;
            }
            const double span = cNow - sPrevC;
            int64_t i = std::max (syncHi, (int64_t) std::ceil (sPrevC - 0.5));
            for (; (double) i + 0.5 <= cNow; ++i)
            {
                const double t = span > 1.0e-12 ? ((double) i + 0.5 - sPrevC) / span : 1.0;
                syncBuf[(size_t) (i & SYNC_MASK)] = (float) (sPrevV[0] + t * (sL - sPrevV[0]));
                syncBuf[(size_t) SYNC_RING + (size_t) (i & SYNC_MASK)] = (float) (sPrevV[1] + t * (sR - sPrevV[1]));
            }
            if (i > syncHi) { syncHi = i; syncFrom = std::max (syncFrom, syncHi - (int64_t) SYNC_RING + 8); }
            sPrevC = cNow; sPrevV[0] = sL; sPrevV[1] = sR;
        }
        if (! syncRun) syncLive = false;
        uPrevS = u1s;
    }
    gSync += (gSyncTgt - gSync) * aRec;
    if (gSyncTgt <= 0.0 && gSync < 1.0e-6) gSync = 0.0;
    double reL = 0.0, reR = 0.0;
    if (gSync > 0.0)
    {
        // what goes onto the record head now lands where the tape was 1 + inLag samples ago
        // (the record path's own odometer), and the remanence it leaves sits xRef behind the gap
        const double uw = posAgo (1 + inLag) - xRefUm / CELL_UM + (syncLagNow + syncLag) * v * dt;
        reL = gSync * syncAt (0, uw);
        reR = gSync * syncAt (1, uw);
    }

    // ---- record: the signal as a deep-gap field onto the field ring...
    if (recFade < 1.0) recFade = std::min (1.0, recFade + dt / SPLICE_FADE_S);
    recEnv += (recTgt - recEnv) * aRec;
    if (recTgt <= 0.0 && recEnv < 1.0e-4) recEnv = 0.0;
    if (recTgt >= 1.0 && recEnv > 1.0 - 1.0e-6) recEnv = 1.0;
    double rl = xL * gIn + fbL + reL, rr = xR * gIn + fbR + reR;
    if (eqOn)
    {
        // the record amplifier's pre-emphasis: boosted treble meets the tape's
        // saturation like everything else
        const ShelfCascade& P = eqRecA;
        rl = recEqS[0].tick (P, rl); rr = recEqS[1].tick (P, rr);
    }
    const double sL = rl * sigScale * recFade;
    const double sR = rr * sigScale * recFade;
    {
        const double u0 = wPrevU1;
        const double u1 = posAgo (1 + inLag) + tr.scrapeUm (3) * (1.0 / CELL_UM);
        writeField (0, u0, u1, sHist[0][0], sHist[0][1], sHist[0][2], sL);
        writeField (1, u0, u1, sHist[1][0], sHist[1][1], sHist[1][2], sR);
        wPrevU1 = u1;
    }
    for (int k = 0; k < 2; ++k) { sHist[k][0] = sHist[k][1]; sHist[k][1] = sHist[k][2]; }
    sHist[0][2] = sL; sHist[1][2] = sR;
    // ...and every layer cell whose zone has passed becomes remanence
    finishCells();

    // ---- the erase head (FLUX MEMORY: its field from full to nothing), and virgin tape onto a REEL
    eraseSpan (ePos, T + ERASE_CM * CELLS_PER_CM);
    if (mode == 0) clearSpan (vPos, T + SUPPLY_CM * CELLS_PER_CM);

    yL = (float) (outL * wetFade);
    yR = (float) (outR * wetFade);
}

//==============================================================================
/*  REPRO's latency: head 1's delay at the set speed, in host samples. Bench section 1
    holds the echo to distance / speed within 1 us. It is the carriage's ACTUAL position,
    not the knob's: the heads keep 1.2 cm apart and push each other, so where head 1
    ends up depends on the others. Taken up once the carriage has settled; while it
    travels the host keeps the last value. */
int Engine::reproTarget (const Params& p)
{
    if (p.repro <= 0.5f) { reproLat = 0; return 0; }
    // separating the heads zeroes a pushed carriage's velocity, so settled also means
    // its position has stopped changing (2e-5 cm a block: under a tenth of a sample)
    const bool still = std::abs (hVel[0]) < 1.0e-3 && std::abs (hPos[0] - reproPos) < 2.0e-5;
    reproPos = hPos[0];
    if (reproLat <= 0 || still)
    {
        bool cl = false, sy = false;
        const double v = targetSpeedCms (cl, sy);
        reproLat = std::clamp ((int) std::lround (hPos[0] / v * fsHost), 1, dryMask - 8);
    }
    return reproLat;
}

double Engine::targetSpeedCms (bool& clamped, bool& syncOn) const
{
    const Params& p = params;
    double vt = std::clamp ((double) p.speed, 4.75, 76.0);
    const int si = std::clamp ((int) std::lround (p.sync), 0, NUM_SYNC - 1);
    clamped = false; syncOn = false;
    if (si > 0 && bpm > 0.0)
    {
        const float* pos[NUM_HEADS] = { &p.h1Pos, &p.h2Pos, &p.h3Pos };
        const int th = std::clamp ((int) std::lround (p.syncHead), 0, NUM_HEADS - 1);
        const double h = std::clamp ((double) *pos[th], HEAD_MIN_CM, HEAD_MAX_CM);
        const double dur = SYNC_BEATS[si] * 60.0 / bpm;
        const double want = h / dur;
        vt = std::clamp (want, 4.75, 76.0);
        clamped = std::abs (vt - want) > 1.0e-6;
        syncOn = true;
    }
    return vt;
}

// ERASE's knob -> the erase head's field (deep gap, Hc). The field is the cause and the
// knob keeps moving it; the taper follows what it does. 0 switches the head off. 0..90 %
// walks the reference's one-pass erasure (mtref --erasure at 19.05 cm/s: the erase head
// alone over a 0 VU 1 kHz recording) geometrically from 0.6 dB to 60 dB lost a pass, i.e.
// from a hundred passes to one; the last 10 % take the field on to a clean wipe. The old
// linear knob put the whole audible transition between 40 and 62 % (BUGLIST 7).
// If the reference's erase transition moves (BUGLIST 8), re-derive the table.
double Engine::eraseFieldHc (double knobPct)
{
    const double k = std::clamp (knobPct, 0.0, 100.0) * 0.01;
    if (k <= 0.0) return 0.0;
    static constexpr double H[5] = { 0.75, 1.0, 1.25, 1.5, 2.0 };        // field, Hc
    static constexpr double D[5] = { 0.1, 3.0, 18.3, 51.5, 148.0 };      // lost in one pass, dB
    auto heFor = [] (double dB) {
        const double l = std::log (dB);
        int i = 0;
        while (i < 3 && l > std::log (D[i + 1])) ++i;
        return H[i] + (H[i + 1] - H[i]) * (l - std::log (D[i])) / (std::log (D[i + 1]) - std::log (D[i]));
    };
    if (k <= 0.9) return heFor (0.6 * std::pow (100.0, k / 0.9));
    const double h9 = heFor (60.0);
    return h9 + (HE_FULL - h9) * (k - 0.9) / 0.1;
}

// OK -> BROKEN (BUGLIST 5): the machine's causes moved from the knobs towards a wreck,
// in a straight line, so the knobs can show where they act. At OK, the knobs exactly.
void Engine::computeEff()
{
    brokenB = std::clamp ((double) params.broken, 0.0, 100.0) * 0.01;
    faultG = std::clamp (2.0 * brokenB - 1.0, 0.0, 1.0);
    eff = params;
    if (brokenB > 0.0)
    {
        const float b = (float) brokenB;
        eff.condition += (100.0f - eff.condition) * b;
        eff.capstan   += (200.0f - eff.capstan) * b;
        eff.reels     += (200.0f - eff.reels) * b;
        eff.scrape    += (200.0f - eff.scrape) * b;
        eff.wear      += (100.0f - eff.wear) * b;
        eff.bias       = std::max (-10.0f, eff.bias - 6.0f * b);   // the bias oscillator sags: under-bias
    }
    // DUB (BUGLIST 9): sound-on-sound by re-recording the loop in place (the sync head, in
    // tick). The tape side is REC's: operating bias, erase head full.
    dubOn = params.mode > 0.5f && std::lround (params.looper) == 1;
    tele.brokenEff[0] = eff.condition; tele.brokenEff[1] = eff.capstan; tele.brokenEff[2] = eff.reels;
    tele.brokenEff[3] = eff.scrape;    tele.brokenEff[4] = eff.wear;    tele.brokenEff[5] = eff.bias;
}

void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    computeEff();
    const Params& p = eff;

    const int wantMode = p.mode > 0.5f ? 1 : 0;
    // LOOP with SYNC: the loop cut to N bars at the tempo head's note value. The
    // length is latched when the bars, the note or the tempo head change - not when
    // a head is dragged (re-cutting is a new tape). The loop then lasts N bars at any
    // tempo (the speed follows the tempo); a dragged tempo head moves the speed and
    // the loop with it, which the telemetry reports as the bars it lasts now
    bool barsOk = true;
    {
        const int si = std::clamp ((int) std::lround (p.sync), 0, NUM_SYNC - 1);
        const int th = std::clamp ((int) std::lround (p.syncHead), 0, NUM_HEADS - 1);
        const int nb = std::clamp ((int) std::lround (p.bars), 0, 16);
        const int key = (wantMode == 1 && si > 0 && nb > 0) ? (nb * 64 + si) * 4 + th : -1;
        const float* pos[NUM_HEADS] = { &p.h1Pos, &p.h2Pos, &p.h3Pos };
        const double headCm = std::clamp ((double) *pos[th], HEAD_MIN_CM, HEAD_MAX_CM);
        if (key != barsKey)
        {
            barsKey = key;
            if (key >= 0)
            {
                const double want = nb * 4.0 / SYNC_BEATS[si] * headCm * 0.01;
                looperLoopM = std::clamp (want, 0.5, 6.0);         // exact: N bars last N bars
                barsHeadCm = headCm;
                barsOk = std::abs (looperLoopM - want) < 1.0e-9;
                tele.barsFit = barsOk;
            }
            else tele.barsFit = true;
        }
        // the loop's period over one bar at the tempo head's note: L / (4 / note * head)
        tele.barsNow = barsKey >= 0 ? looperLoopM * 100.0 * SYNC_BEATS[si] / (4.0 * headCm) : 0.0;
    }
    const double q = (barsKey >= 0) ? looperLoopM : std::clamp (std::round (p.loopLen / 0.05f) * 0.05, 0.5, 6.0);
    if (std::abs (q - pendingLoopM) > 1.0e-9) { pendingLoopM = q; loopSettle = 0.0; }
    else loopSettle += n / fsHost;
    // a different stock is a different reel: threaded through the same splice
    const int wantStock = std::clamp ((int) std::lround (p.stock), 0, NUM_STOCKS - 1);
    if (spliceState == 0)
    {
        const bool lenChange = wantMode == 1 && mode == 1 && (loopSettle > 0.15 || barsKey >= 0)
                               && std::abs (pendingLoopM - loopM) > 1.0e-9;
        if (wantMode != mode || lenChange || wantStock != stockIdx)
        {
            spliceState = 1;
            spliceMode = wantMode;
            spliceLoopM = (wantMode == 1) ? pendingLoopM : loopM;
            spliceStock = wantStock;
        }
    }

    bool clamped = false, syncOn = false;
    vTarget = targetSpeedCms (clamped, syncOn) * CELLS_PER_CM;
    trIn.vTargetCms = vTarget / CELLS_PER_CM;
    trIn.motor = p.motor > 0.5f ? 1 : 0;
    trIn.cond = std::clamp ((double) p.condition, 0.0, 100.0) * 0.01;
    trIn.capstan = std::clamp ((double) p.capstan, 0.0, 200.0) * 0.01;
    trIn.reels = std::clamp ((double) p.reels, 0.0, 200.0) * 0.01;
    trIn.scrape = std::clamp ((double) p.scrape, 0.0, 200.0) * 0.01;
    trIn.reelMode = mode == 0;
    trIn.running = p.run > 0.5f;
    trIn.hunt = (faultMask & 2) ? faultG : 0.0;
    {
        double cr = 0.0;
        for (int i = 0; i < NUM_HEADS; ++i) cr = std::max (cr, creaseNow[i]);
        trIn.crease = cr;
        tele.crease = cr;
    }
    tele.huntPct = 1.5 * trIn.hunt;

    // BROKEN, head clog: oxide shed by the tape builds up on a playback head over
    // seconds, lifting the tape off the gap (the treble goes first), until it breaks
    // loose. Each head its own clog, at its own time.
    {
        const double dtB = n / fsHost;
        double worst = 0.0;
        for (int i = 0; i < NUM_HEADS; ++i)
        {
            Clog& k = clog[i];
            if (faultG <= 0.0 || ! (faultMask & 1)) { k.phase = 0; k.t = 0.0; k.um = 0.0; continue; }
            auto rnd = [&k] { k.rng = mix64 (k.rng); return hashUni (k.rng); };
            k.t -= dtB;
            if (k.phase == 0 && k.t <= 0.0)
            {
                k.phase = 1;
                k.target = faultG * (1.5 + 4.5 * rnd());          // up to 6 um of spacing
                k.rate = k.target / (2.0 + 6.0 * rnd());          // built over 2-8 s
            }
            else if (k.phase == 1)
            {
                k.um += k.rate * dtB;
                if (k.um >= k.target) { k.um = k.target; k.phase = 2; k.t = 0.5 + 3.0 * rnd(); }
            }
            else if (k.phase == 2 && k.t <= 0.0) k.phase = 3;
            else if (k.phase == 3)
            {
                k.um *= std::exp (-dtB / 0.08);                   // it breaks loose
                if (k.um < 0.01) { k.um = 0.0; k.phase = 0; k.t = (1.0 + 6.0 * rnd()) * (1.5 - faultG); }
            }
            worst = std::max (worst, k.um);
        }
        tele.clogUm = worst;
    }

    // ---- FLUX MEMORY and the LOOP panel: what the record and erase heads do
    {
        double he = eraseFieldHc (p.erase), rt = 1.0;                       // DUB: the ERASE knob
        const bool clr = p.clear > 0.5f;
        if (wantMode == 1 && clr && ! clearPrev) clearLeft = (double) N;    // one pass of the loop
        clearPrev = clr;
        loopState = std::clamp ((int) std::lround (p.looper), 0, 2);
        if (mode == 0) { he = HE_FULL; rt = 1.0; clearLeft = 0.0; }         // a reel: always a clean wipe
        else if (loopState == 0) { he = HE_FULL; rt = 1.0; }                // REC: fresh tape every pass
        else if (loopState == 1) { he = HE_FULL; rt = 1.0; }                // DUB: wiped, and re-recorded in place
        else if (loopState == 2) { he = 0.0;     rt = 0.0; }                // PLAY: record and bias off
        if (clearLeft > 0.0) { he = HE_FULL; rt = 0.0; clearLeft -= v * n * os * dt; }
        // BROKEN: the erase head weakens (worn, mis-aligned, its oscillator low): on a loop
        // its wipe stops being clean, down to about 1.2 Hc. A reel brings blank tape.
        // (not in DUB: a re-record over a part-erased loop adds to what survived and builds up)
        if (mode == 1 && loopState != 1 && brokenB > 0.0) he *= 1.0 - 0.45 * brokenB;
        if (forceHe >= 0.0) he = forceHe;
        if (forceRec >= 0.0) rt = forceRec;
        heTgt = he; recTgt = rt;
        // DUB: the sync head's signal into the record amplifier. ERASE sets how much of the loop
        // each pass loses: 0 keeps it at unity (the medium's own generation loss only), 100 %
        // keeps nothing. The sync head reads whenever a loop records, so DUB starts seamless.
        syncRun = mode == 1 && rt > 0.0;
        const double keep = 1.0 - std::clamp ((double) p.erase, 0.0, 100.0) * 0.01;
        double lg = 1.0, kHf = 1.0;
        if (mode == 1 && loopState == 1)
        {
            lg = syncLoopGain (1000.0);
            // and the treble: the worst computed gain from 2 to 16 kHz, relative to 1 kHz
            double hfMax = 0.0;
            for (double f : { 2000.0, 3000.0, 4000.0, 6000.0, 8000.0, 10000.0, 12000.0, 14000.0, 16000.0, 18000.0 }) hfMax = std::max (hfMax, syncLoopGain (f));
            const double hfDb = 20.0 * std::log10 (std::max (hfMax, 1.0e-12) / std::max (lg, 1.0e-12));
            double lfMax = 0.0;
            for (double f : { 60.0, 100.0, 160.0, 250.0, 400.0 }) lfMax = std::max (lfMax, syncLoopGain (f));
            const double lfDb = 20.0 * std::log10 (std::max (lfMax, 1.0e-12) / std::max (lg, 1.0e-12));
            kHf = dbToLin (-SYNC_HF_K * (std::max (0.0, hfDb - syncHfRef - SYNC_HF_TOL_DB) + std::max (0.0, lfDb - syncLfRef - SYNC_LF_TOL_DB) * (SYNC_LF_K / SYNC_HF_K)));
            tele.syncHfDb = hfDb; tele.syncLfDb = lfDb;
        }
        const double kLoop = (lg > 1.0e-6) ? syncPeakK / lg * kHf : 0.0;
        gSyncTgt = (mode == 1 && loopState == 1 && clearLeft <= 0.0 && forceRec < 0.0) ? SYNC_K * syncTrim * keep * kLoop : 0.0;
        tele.syncLoopDb = 20.0 * std::log10 (std::max (lg, 1.0e-9));
        tele.reRecDb = gSyncTgt > 0.0 ? 20.0 * std::log10 (keep) : -200.0;
        tele.looper = mode == 1 ? (clearLeft > 0.0 ? 3 : loopState) : -1;
        tele.recording = recEnv > 0.5;
    }
    trIn.supplyR = tele.supplyR.load(); trIn.takeupR = tele.takeupR.load();
    updateMedium (p, n);

    // ---- bias: smoothed at block rate, the record tables rebuilt as it moves
    if (n > 0)
    {
        biasDbSm += (p.bias - biasDbSm) * (1.0 - std::exp (-n / (fsHost * 0.05)));
        // the motor's speed: tables and equalisers do not chase every flutter cycle (and
        // a stopping tape keeps the slowest speed's, where nothing moves past a head)
        const double vNow = std::max (tr.motorSpeedCms(), 4.6);
        // the erase head's field, ramped, its table rebuilt as it moves
        heSm = (heSm > 1.0e8) ? heTgt : heSm + (heTgt - heSm) * (1.0 - std::exp (-n / (fsHost * 0.02)));
        // an exponential never arrives: the head off is OFF (a hair of field left on would
        // nudge every cell of a frozen loop on every pass), and full is full
        if (heTgt <= 0.0 && heSm < 0.02) heSm = 0.0;
        if (heTgt >= 2.2 && heSm > 2.2) heSm = heTgt;
        tele.eraseHc = heSm;
        if (std::abs (biasDbSm - recBiasDb) > 0.005 || std::abs (std::log (vNow / recSpeed)) > 0.002)
            buildRecord (biasDbSm, vNow);
        // the pass's one-shot tables, rebuilt as the erase field, the bias, the speed or the record relay move
        const bool recOn = recTgt > 0.5;
        if (std::abs (heSm - dubHe) > 0.004 || std::abs (biasDbSm - dubBiasDb) > 0.005
            || std::abs (std::log (vNow / std::max (dubSpeed, 1.0))) > 0.002 || recOn != dubRecOn)
            buildDub (heSm, biasDbSm, vNow, recOn);
        hkB = HeadKernel::bucket (vNow);
        {
            // the re-record placement for this speed (SYNC_LAG_US, interpolated in log speed)
            const double fk = std::clamp (std::log2 (vNow / 4.75), 0.0, 4.0);
            const int ik = std::min ((int) fk, 3);
            const double us = SYNC_LAG_US[ik] + (fk - ik) * (SYNC_LAG_US[ik + 1] - SYNC_LAG_US[ik]);
            syncLagNow = SYNC_LAG_SAMPLES + us * 1.0e-6 / dt;
            const double pk = SYNC_PEAK_DB[ik] + (fk - ik) * (SYNC_PEAK_DB[ik + 1] - SYNC_PEAK_DB[ik]);
            syncPeakK = dbToLin (-(pk + SYNC_MARGIN_DB));
            syncHfRef = SYNC_HF_REF_DB[ik] + (fk - ik) * (SYNC_HF_REF_DB[ik + 1] - SYNC_HF_REF_DB[ik]);
            syncLfRef = SYNC_LF_REF_DB[ik] + (fk - ik) * (SYNC_LF_REF_DB[ik + 1] - SYNC_LF_REF_DB[ik]);
        }
        eqS = p.eqStd > 0.5f ? 1 : 0;
        updateEq (vNow);

    }

    const float* on[NUM_HEADS]  = { &p.h1On, &p.h2On, &p.h3On };
    const float* lvl[NUM_HEADS] = { &p.h1Lvl, &p.h2Lvl, &p.h3Lvl };
    const float* pan[NUM_HEADS] = { &p.h1Pan, &p.h2Pan, &p.h3Pan };
    double tHead[NUM_HEADS], tPL[NUM_HEADS], tPR[NUM_HEADS];
    for (int i = 0; i < NUM_HEADS; ++i)
    {
        tHead[i] = (*on[i] > 0.5f) ? dbToLin (*lvl[i]) : 0.0;
        const double pn = std::clamp ((double) *pan[i], -1.0, 1.0);
        tPL[i] = std::min (1.0, 1.0 - pn);
        tPR[i] = std::min (1.0, 1.0 + pn);
    }
    const double gRec = dbToLin (p.recLevel);
    const double tIn = dbToLin (p.drive) * gRec;
    const bool reproOn = p.repro > 0.5f;
    if (reproOn) tHead[1] = tHead[2] = 0.0;       // a tape machine replays from one head
    // The heads' outputs are summed onto the feedback wire, so echoes arriving in phase
    // add: two heads at 0 dB would halve the knob's runaway point and three take it to a
    // third. The wire is divided by the number of heads that are on, so FEEDBACK means
    // the same with one, two or three (BUGLIST 3). A head turned down feeds back less.
    int nOn = 0;
    for (int i = 0; i < NUM_HEADS; ++i) if (tHead[i] > 0.0) ++nOn;
    const double tFb = reproOn ? 0.0 : p.feedback * FEEDBACK_MAX * gRec / std::max (1, nOn);
    {
        // the delay the host is told about, and the dry path's (crossfaded over 20 ms)
        const int want = reproTarget (p);
        // one crossfade at a time: a change arriving mid-fade is taken up when it ends
        if (want != dryD && dryXf == 0) { dryDOld = dryD; dryD = want; dryXf = dryXfLen; }
        reproLatency.store (want, std::memory_order_relaxed);
        tele.repro = reproOn ? 1 : 0;
        tele.reproMs = 1000.0 * want / fsHost;
    }
    const double tDry = (p.killDry > 0.5f) ? 0.0 : std::cos (p.mix * M_PI * 0.5);
    const double tWet = std::sin (p.mix * M_PI * 0.5);
    const double tOut = dbToLin (p.output);
    const double tByp = p.bypass > 0.5f ? 1.0 : 0.0;
    // MECH SOUNDS: fed from the transport as it is now; at 0 (and faded out) not run at all
    if (p.mech > 0.0f || mechOn)
    {
        Mech::In mi;
        mi.level = std::clamp ((double) p.mech, 0.0, 100.0) * 0.01;
        mi.vCms = v / CELLS_PER_CM;
        mi.powered = p.run > 0.5f;
        mi.dcServo = p.motor > 0.5f;
        mi.mode = mode;
        mi.looper = loopState;
        mi.supplyR = tele.supplyR.load(); mi.takeupR = tele.takeupR.load();
        mi.loopM = loopM;
        for (int i = 0; i < NUM_HEADS; ++i) { mi.headCm[i] = hPos[i]; mi.headVelCms[i] = hVel[i]; }
        mi.cond = trIn.cond; mi.capstan = trIn.capstan; mi.reels = trIn.reels; mi.scrape = trIn.scrape;
        mi.splice = spliceState != 0;
        mech.block (mi);
        mechOn = mech.active();
    }
    const bool mono = p.inMode > 0.5f;

    if (n == 0)
    {
        for (int i = 0; i < NUM_HEADS; ++i) { gHead[i] = tHead[i]; panL[i] = tPL[i]; panR[i] = tPR[i]; }
        fbEff = tFb; gIn = tIn;
        return;
    }

    for (int k = 0; k < n; ++k)
    {
        for (int i = 0; i < NUM_HEADS; ++i)
        {
            gHead[i] += (tHead[i] - gHead[i]) * aHost;
            panL[i]  += (tPL[i] - panL[i]) * aHost;
            panR[i]  += (tPR[i] - panR[i]) * aHost;
        }
        fbEff += (tFb - fbEff) * aHost;
        gIn   += (tIn - gIn) * aHost;
        gDry  += (tDry - gDry) * aHost;
        gWet  += (tWet - gWet) * aHost;
        gOut  += (tOut - gOut) * aHost;

        if ((toneTick++ & 63) == 0)
        {
            bassDb += (p.bass - bassDb) * 0.1;
            trebDb += (p.treble - trebDb) * 0.1;
            for (int c = 0; c < 2; ++c)
            {
                Biquad nl = lowS[c], nh = highS[c];
                nl.shelf (false, fsInt, 200.0, bassDb);
                nh.shelf (true, fsInt, 3000.0, trebDb);
                nl.z1 = lowS[c].z1; nl.z2 = lowS[c].z2;
                nh.z1 = highS[c].z1; nh.z2 = highS[c].z2;
                lowS[c] = nl; highS[c] = nh;
            }
        }

        const float dl = inL ? inL[k] : 0.0f;
        const float dr = inR ? inR[k] : dl;
        float xl = dl, xr = dr;
        if (mono) xl = xr = 0.5f * (dl + dr);
        pkIn = std::max (pkIn, std::max (std::abs (xl), std::abs (xr)));

        float wl, wr;
        if (os == 4)
        {
            // HIGH: two half-band stages each way
            float a0, a1, b0, b1, q[4], r[4], yl[4], yr[4];
            up[0].up (xl, a0, a1);
            up[1].up (xr, b0, b1);
            up2[0].up (a0, q[0], q[1]); up2[0].up (a1, q[2], q[3]);
            up2[1].up (b0, r[0], r[1]); up2[1].up (b1, r[2], r[3]);
            for (int j = 0; j < 4; ++j) tick (q[j], r[j], yl[j], yr[j]);
            // sequenced, never nested: a function's arguments are evaluated in no defined order
            const float l0 = dn2[0].down (yl[0], yl[1]);
            const float l1 = dn2[0].down (yl[2], yl[3]);
            const float r0 = dn2[1].down (yr[0], yr[1]);
            const float r1 = dn2[1].down (yr[2], yr[3]);
            wl = dn[0].down (l0, l1);
            wr = dn[1].down (r0, r1);
        }
        else if (os == 2)
        {
            float a0, a1, b0, b1, y0l, y0r, y1l, y1r;
            up[0].up (xl, a0, a1);
            up[1].up (xr, b0, b1);
            tick (a0, b0, y0l, y0r);
            tick (a1, b1, y1l, y1r);
            wl = dn[0].down (y0l, y1l);
            wr = dn[1].down (y0r, y1r);
        }
        else
            tick (xl, xr, wl, wr);
        if (pbOn) { wl = (float) ampRail (wl); wr = (float) ampRail (wr); }

        // the dry path, delayed by what the host is told (0 unless REPRO): written then
        // read, so at zero delay it is the input sample itself
        dryRing[0][(size_t) dryW] = dl; dryRing[1][(size_t) dryW] = dr;
        float ddl = dryRing[0][(size_t) ((dryW - dryD) & dryMask)];
        float ddr = dryRing[1][(size_t) ((dryW - dryD) & dryMask)];
        if (dryXf > 0)
        {
            const float a = (float) dryXf / (float) dryXfLen;   // 1 -> 0: the old tap fades out
            ddl = a * dryRing[0][(size_t) ((dryW - dryDOld) & dryMask)] + (1.0f - a) * ddl;
            ddr = a * dryRing[1][(size_t) ((dryW - dryDOld) & dryMask)] + (1.0f - a) * ddr;
            --dryXf;
        }
        dryW = (dryW + 1) & dryMask;
        float ol = (float) ((ddl * gDry + wl * gWet) * gOut);
        float orr = (float) ((ddr * gDry + wr * gWet) * gOut);
        if (mechOn) { float ml, mr; mech.tick (ml, mr); ol += ml; orr += mr; }
        // the host's bypass: a 20 ms crossfade to the dry path at unity, delayed by what
        // the host is told; the tape runs on underneath. Exactly the machine when off.
        if (tByp != gByp || gByp > 0.0)
        {
            gByp += (tByp - gByp) * aHost;
            if (std::abs (tByp - gByp) < 1.0e-5) gByp = tByp;
            ol  = (float) ((1.0 - gByp) * ol  + gByp * ddl);
            orr = (float) ((1.0 - gByp) * orr + gByp * ddr);
        }
        msIn  += ((double) xl * xl * 0.5 + (double) xr * xr * 0.5 - msIn) * aVu;
        msOut += ((double) ol * ol * 0.5 + (double) orr * orr * 0.5 - msOut) * aVu;
        pkOut = std::max (pkOut, std::max (std::abs (ol), std::abs (orr)));
        if (outL) outL[k] = ol;
        if (outR) outR[k] = orr;
    }

    // ---- telemetry
    const double distM = v * n * os * dt * CELL_UM * 1.0e-6;
    capAng += distM / CAPSTAN_R_M;
    if (mode == 0)
    {
        playedM += distM;
        if (playedM >= REEL_PACK_M) playedM -= REEL_PACK_M;
        const double rs = std::sqrt (REEL_HUB_M * REEL_HUB_M + (REEL_PACK_M - playedM) * TAPE_THICK_M / M_PI);
        const double rt = std::sqrt (REEL_HUB_M * REEL_HUB_M + playedM * TAPE_THICK_M / M_PI);
        supAng += distM / rs;
        takAng += distM / rt;
        tele.supplyR = rs; tele.takeupR = rt;
    }
    else
        loopAng += distM / 0.03;
    viewT.store (T, std::memory_order_relaxed);
    tele.speedCms = v / CELLS_PER_CM;
    tele.wowFlutter = wfPct;
    tele.wear = wearEff;
    tele.stock = stockIdx;
    tele.targetCms = vTarget / CELLS_PER_CM;
    tele.capstanAngle = std::fmod (capAng, 2.0 * M_PI);
    tele.supplyAngle = std::fmod (supAng, 2.0 * M_PI);
    tele.takeupAngle = std::fmod (takAng, 2.0 * M_PI);
    tele.loopAngle = std::fmod (loopAng, 2.0 * M_PI);
    tele.tapeOdometerM = playedM;
    for (int i = 0; i < NUM_HEADS; ++i) { tele.headCm[i] = hPos[i]; tele.headVelCms[i] = hVel[i]; }
    tele.syncClamped = clamped;
    tele.syncActive = syncOn;
    tele.splicing = spliceState != 0;

    tele.inMs = msIn; tele.outMs = msOut;
    tele.inPeak = std::max (tele.inPeak.load(), pkIn);
    tele.outPeak = std::max (tele.outPeak.load(), pkOut);
    tele.recPeak = std::max (tele.recPeak.load(), recPk);
    for (int i = 0; i < NUM_HEADS; ++i)
        tele.headPeak[i] = std::max (tele.headPeak[i].load(), pkHead[i]);
    pkIn = pkOut = 0.0f; recPk = 0.0f;
    for (float& x : pkHead) x = 0.0f;
}

//==============================================================================
/*  The stock (stage 6). selectStock only points at a stock's tables and alignment;
    applyStock also rebuilds what depends on them. */
void Engine::selectStock (int s)
{
    stockIdx = std::clamp (s, 0, NUM_STOCKS - 1);
    SM = &stockMedium (stockIdx);
    coat = SM->coating;
    ST = &coatingTables (coat);
    hk = hkC[coat];
    // 0 VU is this stock's operating level: the record level a technician sets for it
    sigScale = ST->hs0vu * (SM->opLevelNwbm / REF_FLUX) / REF_PEAK;
    outK = outKs[stockIdx];
    pbKappa = pbKappaS[stockIdx];
    seed = 0x4D54000000ull + (uint64_t) stockIdx * 0x1000193ull;
    for (int l = 0; l < NLAYER; ++l) salt[l] = mix64 (seed * 0x9E3779B97F4A7C15ull + (uint64_t) l);
}

void Engine::applyStock (int s)
{
    selectStock (s);
    const double vNow = std::max (tr.motorSpeedCms(), 1.0);
    buildRecord (biasDbSm, vNow);
    eqLastFb = -1.0;
    updateEq (vNow);
    for (auto& st : recEqS) st.reset();
    for (auto& st : repEqS) st.reset();
}

/*  Per block: wear, ACCUMULATE, and what the stock's medium does at this wear. */
void Engine::updateMedium (const Params& p, int n)
{
    // ACCUMULATE: a loop wears on every pass, the wear belonging to that loop (stock
    // and length), so a project reopened finds its loop as it left it
    const bool sameLoop = wearIdStock.load() == stockIdx && std::abs (wearIdLen.load() - loopM) < 1.0e-6;
    const int64_t passes = (int64_t) std::floor (T / (double) std::max<int64_t> (N, 1));
    if (mode == 1 && p.accumulate > 0.5f && passes > lastPass)
    {
        double w = sameLoop ? wearAcc.load() : 0.0;
        w = std::min (1.0, w + (double) (passes - lastPass) * SM->wearPerPass * wearScale);
        wearIdStock.store (stockIdx); wearIdLen.store (loopM); wearAcc.store (w);
    }
    lastPass = passes;
    const bool own = wearIdStock.load() == stockIdx && std::abs (wearIdLen.load() - loopM) < 1.0e-6;
    const double acc = (mode == 1 && own) ? wearAcc.load() : 0.0;
    wearEff = std::clamp (p.wear * 0.01 + acc, 0.0, 1.0);

    // the particles: sigma per cell = 1 / sqrt(particles in the cell), relative to Mr
    const double P = (double) p.particles;
    packTgt = SM->packing * (1.0 - 0.25 * wearEff);
    const double vp = SM->particleVolUm3 * SM->packing / packTgt;
    for (int l = 0; l < NLAYER; ++l)
    {
        const double cellVol = CELL_UM * (double) (1 << l) * SLAB_UM * TRACK_W_UM;
        noiseSig[l] = (idealMedium || P >= 999.0) ? 0.0 : std::sqrt (vp / (cellVol * std::max (P, 0.01)));
    }
    modEff = idealMedium ? 0.0 : SM->modNoise * (1.0 + 2.0 * wearEff);
    spacingStatic = SM->spacingUm + 0.45 * wearEff;
    roughEff = SM->roughUm * (1.0 + 3.0 * wearEff);
    dropRate = SM->dropPerM * (1.0 + 12.0 * wearEff);
    dropDepth = SM->dropDepthUm * (1.0 + wearEff);
    holeEff = std::min (0.9, SM->holeFrac + 0.3 * wearEff);
    if (idealMedium) packTgt = SM->packing;
    if (n > 0) packSm += (packTgt - packSm) * (1.0 - std::exp (-n / (fsHost * 0.05)));

    // azimuth: each playback head's gap off the record head's, by the transport's
    // condition (a freshly serviced machine within a minute of arc), and the tape's
    // weave along the guides
    constexpr double ARCMIN = M_PI / (180.0 * 60.0);
    static constexpr double AZ_R[NUM_HEADS] = { 0.30, -0.60, 0.55 };
    for (int i = 0; i < NUM_HEADS; ++i) azim[i] = trIn.cond * 8.0 * ARCMIN * AZ_R[i];
    weave = trIn.cond * trIn.reels * (mode == 0 ? 1.5 : 0.5) * ARCMIN;
    // a sticky or worn tape drags at the heads: more stick-slip
    trIn.scrape *= SM->friction * (1.0 + wearEff);
}

// the loop's splice joint (stage 7): a 20 um butt gap with no oxide, the splicing
// tape on the back lifting the tape 2 um over +-0.4 mm
constexpr double SPLICE_GAP_UM = 20.0, SPLICE_LIFT_UM = 400.0, SPLICE_DD_UM = 2.0;

/*  The shaper's settings for head i, from the tape under it now (x: surface cells,
    unwrapped) and the tape's speed past it (um per internal sample). */
void Engine::updateShaper (int i, double x, double vus)
{
    double X = x;
    if (mode == 1) { const double fl = std::floor (x); X = (double) wrapTo ((int64_t) fl, N) + (x - fl); }
    double a = azim[i] + (weave > 0.0 ? weave * valueNoise (X, 100000.0, seed ^ 0x77) : 0.0);
    // BROKEN: a crinkled stretch lifts and twists the tape at the head
    double cdd = 0.0, ckeep = 1.0;
    creaseNow[i] = 0.0;
    if (! idealMedium && faultG > 0.0 && (faultMask & 4)) creaseNow[i] = faultG * crinkleAt (X, cdd, ckeep, a);
    const double ta = std::tan (a);
    const double box = std::abs (TRACK_W_UM * ta) / vus;
    const double sh = 0.5 * TRACK_SEP_UM * ta / vus;
    for (int c = 0; c < 2; ++c)
    {
        double dd = 0.0, keep = 1.0;
        if (! idealMedium)
        {
            const double r = roughEff * (1.0 + 0.8 * valueNoise (X, 120.0, seed ^ (0x100u + c)) + 0.6 * valueNoise (X, 480.0, seed ^ (0x200u + c)));
            double ddrop = 0.0;
            drop[i][c].at (X, seed ^ (0x300u + c), dropRate, dropDepth, holeEff, ddrop, keep);
            dd = spacingStatic + std::max (0.0, r) + ddrop;
            if (mode == 1)
            {
                // the loop's splice: the two ends butt (a sliver with no oxide) under a strip
                // of splicing tape on the back, which lifts the tape from the head
                const double ds = std::min (X, (double) N - X) * CELL_UM;     // um from the joint
                if (ds < SPLICE_LIFT_UM) dd += SPLICE_DD_UM * 0.5 * (1.0 + std::cos (M_PI * ds / SPLICE_LIFT_UM));
                if (ds < SPLICE_GAP_UM) keep *= 1.0 - 0.7 * (0.5 + 0.5 * std::cos (M_PI * ds / SPLICE_GAP_UM));
            }
            dd += cdd; keep *= ckeep;
        }
        dd += clog[i].um;                       // BROKEN: oxide clogging the head
        // what changes nothing below 20 kHz by 0.05 dB (or 0.01 sample) is left out:
        // the studio tape at 76 cm/s barely lifts at all, and its kernel then costs nothing
        double tau = dd / vus;
        if (tau * om20 < 0.006) tau = 0.0;
        const double bx = (box * om20 < 0.37) ? 0.0 : box;
        const double s = (std::abs (sh) < 0.01) ? 0.0 : (c == 0 ? sh : -sh);
        shaper[i].set (c, tau, bx, s, keep);
    }
}

/*  BROKEN past the middle: a crinkled stretch of tape. One on a loop, where it was
    creased; on a reel, one in most 2.5 m of tape as faults grow. Over 5 cm, sharp folds
    about 2 mm apart lift the tape off the head and twist it across the gap. */
double Engine::crinkleAt (double X, double& dd, double& keep, double& az) const
{
    const double Lc = 50000.0 / CELL_UM;                       // 5 cm, in surface cells
    double d;
    if (mode == 1)
    {
        const double c = hashUni (mix64 (seed ^ 0xC71ull)) * (double) N;
        d = X - c;
        d -= (double) N * std::round (d / (double) N);
    }
    else
    {
        const double S = 2.5e6 / CELL_UM;                      // 2.5 m
        const double js = std::floor (X / S);
        const uint64_t j = (uint64_t) (int64_t) js;
        if (hashUni (mix64 (seed ^ 0xC72ull ^ (j * 0x9E3779B97F4A7C15ull))) > 0.25 + 0.75 * faultG) return 0.0;
        d = X - (js + 0.2 + 0.6 * hashUni (mix64 (seed ^ 0xC73ull ^ (j * 0xBF58476D1CE4E5B9ull)))) * S;
    }
    const double u = d / Lc;
    if (std::abs (u) >= 0.5) return 0.0;
    const double e = std::cos (M_PI * u) * std::cos (M_PI * u);
    const double mm = d * CELL_UM * 0.001;
    const double fold = std::pow (std::abs (std::sin (M_PI * mm / 2.2 + 0.7 * std::sin (0.9 * mm))), 6.0);
    dd += faultG * e * (1.5 + 6.0 * fold);
    keep *= 1.0 - 0.5 * faultG * e * fold;
    az += faultG * e * 6.0 * (M_PI / (180.0 * 60.0)) * std::sin (2.1 * mm);
    return e;
}

/*  The coating's density variation at surface cell X, per layer (each layer writes
    at its own frontier): three octaves of a smooth random function, 4, 1 and
    0.25 mm, sampled every 32 um and interpolated. */
double Engine::epsAt (int l, double X)
{
    constexpr double STEP = 64.0;
    const double y = X / STEP;
    const int64_t j = (int64_t) std::floor (y);
    if (j != epsJ[l])
    {
        if (epsJ[l] != INT64_MIN && j == epsJ[l] + 1) { epsA[l] = epsB[l]; epsB[l] = epsGrid (j + 1); }
        else { epsA[l] = epsGrid (j); epsB[l] = epsGrid (j + 1); }
        epsJ[l] = j;
    }
    return epsA[l] + (y - (double) j) * (epsB[l] - epsA[l]);
}

double Engine::epsGrid (int64_t j) const
{
    const double x = (double) j * 64.0;
    return (valueNoise (x, 8000.0, seed ^ 0x11) + 0.6 * valueNoise (x, 2000.0, seed ^ 0x22)
            + 0.35 * valueNoise (x, 500.0, seed ^ 0x33)) * (1.0 / 1.2176);
}

//==============================================================================
/*  The erase head (stage 7): the record process with no signal, at the erase head's
    field - the reference's G at s = 0, at the amplitude each layer sees under a 60 um
    gap (Karlqvist: (2/pi) atan(g / 2y)). What a cell keeps is a function of what it
    had, tabulated per layer for the current field and speed. */
/*  The pass's one-shot tables (BUGLIST 8): for the erase head's field he and the bias
    (off when the record relay is open), per slab, the loss the pair takes from the
    committed take by |M| and from the top material by its level, as retention indices.
    Interpolated in dB across speed, he and A: the transition is steep (15 dB per grid
    step of he), and linear in the ratio would be far too gentle between the points. */
void Engine::buildDub (double he, double biasDb, double speedCms, bool recOn)
{
    dubHe = he; dubBiasDb = biasDb; dubSpeed = speedCms; dubRecOn = recOn;
    dubPass = he > 0.0;      // an erase field acts even with the record relay open (the bench's erase-alone case)
    const double hb = ST->hbOperating * dbToLin (biasDb);
    const double lnA0 = std::log (ST->aGrid[0]), lnSpan = std::log (ST->aGrid[STK_NA - 1]) - lnA0;
    const double fv = std::clamp (std::log2 (speedCms / ST->speedCms[0]), 0.0, (double) (STK_NV - 1));
    const int iv = std::min ((int) fv, STK_NV - 2);
    const double tv = fv - iv;
    const double heC = std::clamp (he, 0.0, ST->heGrid[STK_NHE - 1]);
    int ih = 0; while (ih < STK_NHE - 2 && heC >= ST->heGrid[ih + 1]) ++ih;
    const double th = std::clamp ((heC - ST->heGrid[ih]) / (ST->heGrid[ih + 1] - ST->heGrid[ih]), 0.0, 1.0);
    auto q = [] (double r) { return r <= 1.0e-9 ? 255.0 : std::min (255.0, -80.0 * std::log10 (r)); };
    for (int l = 0; l < NLAYER; ++l)
    {
        DubSlice& D = dub[l];
        const double A = recOn ? std::clamp (hb * ST->layerFmax[l], ST->aGrid[0], ST->aGrid[STK_NA - 1]) : ST->aGrid[0];
        const double fa = (std::log (A) - lnA0) / lnSpan * (STK_NA - 1);
        const int ia = std::clamp ((int) std::floor (fa), 0, STK_NA - 2);
        const double ta = std::clamp (fa - ia, 0.0, 1.0);
        D.ia = recOn ? (int) std::lround (fa) : 0;
        auto tri = [&] (auto&& at) {
            double acc = 0.0;
            for (int dv = 0; dv < 2; ++dv)
                for (int dh = 0; dh < 2; ++dh)
                    for (int da = 0; da < 2; ++da)
                        acc += (dv ? tv : 1.0 - tv) * (dh ? th : 1.0 - th) * (da ? ta : 1.0 - ta) * q (at (iv + dv, ih + dh, ia + da));
            return acc;
        };
        const int c = STK_NM / 2;
        for (int k = 0; k <= STK_NM / 2; ++k)
            D.rdQ[k] = (float) tri ([&] (int v, int h, int a) { return (double) ST->rDub[v][l][h][a][c + k]; });
        for (int it = 0; it < STK_NT; ++it)
        {
            D.rtQ[it] = (float) tri ([&] (int v, int h, int a) { return (double) ST->rTop[v][l][h][a][it]; });
            double acc = 0.0;
            for (int dv = 0; dv < 2; ++dv)
                for (int da = 0; da < 2; ++da)
                    acc += (dv ? tv : 1.0 - tv) * (da ? ta : 1.0 - ta) * std::abs ((double) ST->topM[iv + dv][l][ia + da][it]);
            D.tm[it] = (float) acc;
        }
        D.mStep = (ST->mRem[l][STK_NM - 1] - ST->mRem[l][0]) / (STK_NM - 1);
    }
}

void Engine::eraseSpan (double& prevPos, double newPos)
{
    // full: a clean wipe here at the head. A partial field acts at the record head together
    // with the bias, through the pair table (buildDub): the two are not separable.
    if (heSm >= 2.2) { clearSpan (prevPos, newPos); return; }
    prevPos = newPos;
}

uint64_t Engine::tapeChecksum() const
{
    uint64_t h = 0x9E3779B97F4A7C15ull;
    for (int c = 0; c < 2; ++c)
        for (int l = 0; l < NLAYER; ++l)
            for (float x : store->m[c][l]) { uint32_t u; std::memcpy (&u, &x, 4); h = mix64 (h ^ u); }
    return h;
}

//==============================================================================
void Engine::tapeView (float* out, int n, double fromCm, double toCm) const
{
    const TapeStore* vs = viewStore.load (std::memory_order_acquire);
    if (vs == nullptr || n <= 0) { for (int i = 0; i < n; ++i) out[i] = 0.0f; return; }
    const double t = viewT.load (std::memory_order_relaxed);
    // the reference peak in units of mean remanence, through the repro alignment
    // the audio uses - so the view and the output agree on what 0 VU is
    const double peakRef = REF_PEAK / outK;
    const bool reel = tele.mode.load() == 0;
    for (int j = 0; j < n; ++j)
    {
        const double x0 = fromCm + (toCm - fromCm) * j / n;
        const double x1 = fromCm + (toCm - fromCm) * (j + 1) / n;
        if (reel && x1 <= -SUPPLY_CM) { out[j] = 0.0f; continue; }
        double m = 0.0;
        constexpr int SMP = 24;
        for (int s = 0; s < SMP; ++s)
        {
            const double x = x0 + (x1 - x0) * (s + 0.5) / SMP;
            const double u = t - x * CELLS_PER_CM;
            for (int c = 0; c < 2; ++c)
            {
                double sum = 0.0;
                for (int l = 0; l < NLAYER; ++l)
                {
                    const int64_t ring = vs->nl (l);
                    sum += vs->m[c][l][(size_t) wrapTo ((int64_t) std::floor (u / (double) (1 << l)), ring)];
                }
                m = std::max (m, std::abs (sum / NLAYER));
            }
        }
        out[j] = (float) (m / peakRef);
    }
}

} // namespace mt
