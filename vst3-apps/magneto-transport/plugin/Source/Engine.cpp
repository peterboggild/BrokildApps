#include "Engine.h"
#include "stock/stock_456.h"

#include <algorithm>
#include <cstring>
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
    namespace S4 = stock456;
    static_assert (S4::NL == NLAYER, "the record tables and the tape disagree on the layer count");
    static_assert (S4::NM == 11 && S4::NS <= LayerRecord::NSMAX, "LayerRecord's slice size");

    inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }

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
        return std::clamp ((t + 1.0) * 0.5 * (S4::NS - 1), 0.0, (double) (S4::NS - 1));
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
            m[c][l].assign ((size_t) (n0 >> l), 0.0f);
}

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
/*  The record head at a bias and a tape speed, one layer at a time: the G slice
    at the bias amplitude this layer sees and the current speed (both
    log-interpolated between table planes - a slower tape sees more bias cycles
    in the zone and records more completely: +0.6 dB at 19 cm/s against 38.1),
    and the zone kernel - a Gaussian with the measured centroid and rms,
    convolved with the layer's own cell width. */
void Engine::buildRecord (double biasDb, double speedCms)
{
    const double hb = S4::HB_OPERATING * dbToLin (biasDb);
    const double lnA0 = std::log (S4::A_GRID[0]), lnSpan = std::log (S4::A_GRID[S4::NA - 1]) - lnA0;
    const int c = S4::NM / 2;
    // speed planes are a factor of 2 apart
    const double fv = std::clamp (std::log2 (speedCms / S4::SPEED_CMS[0]), 0.0, (double) (S4::NV - 1));
    const int iv = std::min ((int) fv, S4::NV - 2);
    const double tv = fv - iv;
    auto aIndex = [&] (double A, int& ia, double& ta) {
        A = std::clamp (A, S4::A_GRID[0], S4::A_GRID[S4::NA - 1]);
        const double fa = (std::log (A) - lnA0) / lnSpan * (S4::NA - 1);
        ia = std::clamp ((int) std::floor (fa), 0, S4::NA - 2);
        ta = std::clamp (fa - ia, 0.0, 1.0);
    };
    auto zone = [&] (const double (&Z)[S4::NV][S4::NL][S4::NA], int l, int ia, double ta) {
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
            int ia; double ta; aIndex (S4::HB_OPERATING * S4::LAYER_FMAX[l], ia, ta);
            gl[l] = zone (S4::ZONE_GAIN, l, ia, ta);
            xl[l] = zone (S4::ZONE_X_UM, l, ia, ta);
            wl[l] = std::max (0.0, zone (S4::ZONE_W_UM, l, ia, ta));
            gx += gl[l] * xl[l]; gs += gl[l];
        }
        xRefUm = gs > 0.0 ? gx / gs : 0.0;
        const double k1 = 2.0 * M_PI * 1000.0 / (speedCms * 1.0e4);
        double wr = 0.0, wi = 0.0, lr = 0.0, li = 0.0;
        for (int l = 0; l < NLAYER; ++l)
        {
            const double w = gl[l] * std::exp (-0.5 * std::pow (k1 * wl[l], 2.0));
            const double ph = -k1 * (xl[l] - xRefUm);
            const double L = (hk != nullptr) ? headResponse (l, k1, HeadKernel::bucket (speedCms)) : slabResponse (l, k1, speedCms);
            wr += w * std::cos (ph); wi += w * std::sin (ph);
            lr += w * L * std::cos (ph); li += w * L * std::sin (ph);
        }
        const double without = std::hypot (wr, wi), with = std::hypot (lr, li);
        pbTrimSS = with > 0.0 ? without / with : 1.0;
        pbTrim = pbKappa * pbTrimSS;
    }

    double msym[S4::NM];
    for (int i = 0; i < S4::NM; ++i) msym[i] = 0.5 * (S4::M_REM[i] - S4::M_REM[S4::NM - 1 - i]);
    const double dM = (msym[S4::NM - 1] - msym[0]) / (S4::NM - 1);

    for (int l = 0; l < NLAYER; ++l)
    {
        LayerRecord& r = rec[l];
        const double A = std::clamp (hb * S4::LAYER_FMAX[l], S4::A_GRID[0], S4::A_GRID[S4::NA - 1]);
        int ia; double ta; aIndex (A, ia, ta);

        float raw[S4::NM][S4::NS];
        for (int im = 0; im < S4::NM; ++im)
            for (int is = 0; is < S4::NS; ++is)
            {
                const double lo = (1.0 - ta) * S4::G[iv][l][ia][im][is] + ta * S4::G[iv][l][ia + 1][im][is];
                const double hi = (1.0 - ta) * S4::G[iv + 1][l][ia][im][is] + ta * S4::G[iv + 1][l][ia + 1][im][is];
                raw[im][is] = (float) ((1.0 - tv) * lo + tv * hi);
            }
        // G is odd: G(-M, -s) = -G(M, s). Antisymmetrise the slice so silence on
        // erased tape records EXACTLY zero, and so table noise cannot add DC.
        for (int im = 0; im < S4::NM; ++im)
            for (int is = 0; is < S4::NS; ++is)
                r.g[im][is] = 0.5f * (raw[im][is] - raw[S4::NM - 1 - im][S4::NS - 1 - is]);
        // Resample on a uniform s grid, 4.9 mHc apart (0 VU spans 38 steps), so a
        // cell's lookup is one multiply instead of an asinh, and linear between
        // these points is -120 dB from the cubic. Antisymmetry survives: the grid
        // is symmetric about s = 0 and so is the cubic through symmetric data.
        for (int im = 0; im < S4::NM; ++im)
            for (int i = 0; i < LayerRecord::NSF; ++i)
            {
                const double sv = -LayerRecord::SMAX + i * (2.0 * LayerRecord::SMAX / (LayerRecord::NSF - 1));
                const double fs = sIndex (sv);
                const int is = std::min ((int) fs, S4::NS - 2);
                const double ts = fs - is;
                const float* g = r.g[im];
                const int a = std::max (is - 1, 0), d = std::min (is + 2, S4::NS - 1);
                r.gf[im][i] = (float) CR (g[a], g[is], g[is + 1], g[d]).at (ts);
            }
        for (int im = 0; im < S4::NM; ++im) r.gf[im][LayerRecord::NSF / 2] = r.g[im][S4::NS / 2];   // s = 0 exactly
        r.dM = dM;
        r.mLo = -0.5 * (S4::NM - 1) * dM;

        r.amp = A;
        r.xUm = zone (S4::ZONE_X_UM, l, ia, ta);
        r.wUm = std::max (0.0, zone (S4::ZONE_W_UM, l, ia, ta));
        r.gain = (r.g[c][S4::NS / 2 + 1] - r.g[c][S4::NS / 2 - 1]) / (S4::S_GRID[S4::NS / 2 + 1] - S4::S_GRID[S4::NS / 2 - 1]);

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
}

/*  New remanence for one layer cell: Catmull-Rom along s (the table is fine
    there but the curve's slope must not kink), linear along M_old. */
float Engine::recordCell (const LayerRecord& r, float mOld, double s) const
{
    const double fm = std::clamp ((mOld - r.mLo) / r.dM, 0.0, (double) (S4::NM - 1));
    const int im = std::min ((int) fm, S4::NM - 2);
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
    os = fsHost < 80000.0 ? 2 : 1;
    fsInt = fsHost * os;
    dt = 1.0 / fsInt;
    hk = &HeadKernel::get (fsInt);      // built once per internal rate, shared by every instance

    for (auto& h : up) { h.design(); h.clear(); }
    for (auto& h : dn) { h.design(); h.clear(); }

    // The half-band pair's delay, MEASURED with an impulse: a symmetric FIR's
    // centroid is its exact group delay. Up half paid at the record head, the
    // down half (plus the read box and its correction FIR) by reading ahead.
    double hbDelay = 0.0;
    if (os == 2)
    {
        HalfBand u, d; u.design(); d.design(); u.clear(); d.clear();
        double num = 0.0, den = 0.0;
        for (int n = 0; n < 256; ++n)
        {
            float a, b;
            u.up (n == 0 ? 1.0f : 0.0f, a, b);
            const double y = d.down (a, b);
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
    }

    aGain  = 1.0 - std::exp (-dt / 0.020);
    aMotor = 1.0 - std::exp (-dt / MOTOR_TAU);
    aHost  = 1.0 - std::exp (-1.0 / (fsHost * 0.020));

    // ---- the record head and its alignment, at OPERATING bias and the machine's
    //      NOMINAL speed (19.05 cm/s), as a technician would align it: record level
    //      so 0 VU = 250 nWb/m, repro level so 250 nWb/m = 0 VU. Other biases and
    //      speeds then change the level and the distortion, as they do on tape.
    sigScale = S4::HS_0VU / REF_PEAK;
    buildRecord (0.0, S4::ALIGN_SPEED_CMS);
    {
        double ss = 0.0; const int K = 720;
        for (int k = 0; k < K; ++k)
        {
            const double s = S4::HS_0VU * std::sin (2.0 * M_PI * (k + 0.5) / K);
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
                const double sv = S4::HS_0VU * std::sin (2.0 * M_PI * (k + 0.5) / K);
                const double m = recordCell (rec[l], 0.0f, sv);
                s2 += m * m;
            }
            ml[l] = std::sqrt (s2 / K);
        }
        const double k1 = 2.0 * M_PI * 1000.0 / (S4::ALIGN_SPEED_CMS * 1.0e4);
        const int b = HeadKernel::bucket (S4::ALIGN_SPEED_CMS);
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
    biasDbSm = params.bias;
    // (the record head for the current speed is built once the motor speed is set, below)

    // ---- a fresh tape for the transport as it stands
    delete store; store = nullptr;
    delete pending.exchange (nullptr);
    delete retired.exchange (nullptr);
    wantCells = 0;

    mode  = params.mode > 0.5f ? 1 : 0;
    loopM = std::clamp (std::round (params.loopLen / 0.05f) * 0.05, 0.5, 6.0);
    pendingLoopM = loopM; loopSettle = 1.0;

    bool clampedNow = false, syncNow = false;
    v = vTarget = targetSpeedCms (clampedNow, syncNow) * CELLS_PER_CM;
    buildRecord (biasDbSm, v / CELLS_PER_CM);
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

    process (nullptr, nullptr, nullptr, nullptr, 0);
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
        uPrev[i] = T - hPos[i] * CELLS_PER_CM + lead;
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
    cNextC = 0;
    recFade = 0.0;

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
            for (int c = 0; c < 2; ++c)
            {
                float& m = store->m[c][l][(size_t) idx];
                m = recordCell (r, m, s[c]);
                recPk = std::max (recPk, std::abs (m));
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
        }
        ++cNextC;
    }
}

/*  The playback head (stage 4, docs/physics/07-playback.md): every slab through
    its own kernel - near taps on the slab's grid at the head's phase, then the
    1/x^2 tail as box means from the coarse prefix sums. a, b: the tape
    coordinates (surface cells) at the start and end of this sample's transit;
    the transit box is folded into the taps, centred where it always was. */
void Engine::readHead (double a, double b, double& outL, double& outR) const
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
        const double* P0 = pre (0, l);
        const double* P1 = pre (1, l);
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
        const double* P0 = preC (0);
        const double* P1 = preC (1);
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

    // ---- motor and carriages
    v += (vTarget - v) * aMotor;

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
    for (int i = 0; i < NUM_HEADS; ++i)
    {
        const double u1 = T - hPos[i] * CELLS_PER_CM + lead;
        const double g = gHead[i];
        // a head switched off and faded out reads nothing (it would add nothing)
        if (g < 1.0e-9) { uPrev[i] = u1; continue; }
        double hL, hR;
        if (pbOn) readHead (uPrev[i], u1, hL, hR); else readLayers (uPrev[i], u1, hL, hR);
        const double kOut = pbOn ? outK * pbTrim : outK;
        hL *= kOut; hR *= kOut;
        uPrev[i] = u1;
        outL += g * panL[i] * hL;
        outR += g * panR[i] * hR;
        fbL += g * hL;
        fbR += g * hR;
        const float pk = (float) (g * std::max (std::abs (hL), std::abs (hR)));
        if (pk > pkHead[i]) pkHead[i] = pk;
    }
    outL = comp[0].tick (outL); outR = comp[1].tick (outR);
    fbL  = comp[2].tick (fbL);  fbR  = comp[3].tick (fbR);

    // ---- the feedback wire, through the tone stage. No cap: what contains a
    //      loop driven past unity is the tape running out of remanence.
    fbL = highS[0].tick (lowS[0].tick (fbL)) * fbEff;
    fbR = highS[1].tick (lowS[1].tick (fbR)) * fbEff;

    // ---- record: the signal as a deep-gap field onto the field ring...
    if (recFade < 1.0) recFade = std::min (1.0, recFade + dt / SPLICE_FADE_S);
    const double sL = (xL * gIn + fbL) * sigScale * recFade;
    const double sR = (xR * gIn + fbR) * sigScale * recFade;
    {
        const double u0 = wPrevU1;
        const double u1 = posAgo (1 + inLag);
        writeField (0, u0, u1, sHist[0][0], sHist[0][1], sHist[0][2], sL);
        writeField (1, u0, u1, sHist[1][0], sHist[1][1], sHist[1][2], sR);
        wPrevU1 = u1;
    }
    for (int k = 0; k < 2; ++k) { sHist[k][0] = sHist[k][1]; sHist[k][1] = sHist[k][2]; }
    sHist[0][2] = sL; sHist[1][2] = sR;
    // ...and every layer cell whose zone has passed becomes remanence
    finishCells();

    // ---- erase head (full field), and virgin tape onto a REEL
    clearSpan (ePos, T + ERASE_CM * CELLS_PER_CM);
    if (mode == 0) clearSpan (vPos, T + SUPPLY_CM * CELLS_PER_CM);

    yL = (float) (outL * wetFade);
    yR = (float) (outR * wetFade);
}

//==============================================================================
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

void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    const Params& p = params;

    const int wantMode = p.mode > 0.5f ? 1 : 0;
    const double q = std::clamp (std::round (p.loopLen / 0.05f) * 0.05, 0.5, 6.0);
    if (std::abs (q - pendingLoopM) > 1.0e-9) { pendingLoopM = q; loopSettle = 0.0; }
    else loopSettle += n / fsHost;
    if (spliceState == 0)
    {
        const bool lenChange = wantMode == 1 && mode == 1 && loopSettle > 0.15
                               && std::abs (pendingLoopM - loopM) > 1.0e-9;
        if (wantMode != mode || lenChange)
        {
            spliceState = 1;
            spliceMode = wantMode;
            spliceLoopM = pendingLoopM;
        }
    }

    bool clamped = false, syncOn = false;
    vTarget = targetSpeedCms (clamped, syncOn) * CELLS_PER_CM;

    // ---- bias: smoothed at block rate, the record tables rebuilt as it moves
    if (n > 0)
    {
        biasDbSm += (p.bias - biasDbSm) * (1.0 - std::exp (-n / (fsHost * 0.05)));
        const double vNow = v / CELLS_PER_CM;
        if (std::abs (biasDbSm - recBiasDb) > 0.005 || std::abs (std::log (vNow / recSpeed)) > 0.002)
            buildRecord (biasDbSm, vNow);
        hkB = HeadKernel::bucket (vNow);
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
    const double tFb = p.feedback * FEEDBACK_MAX * gRec;
    const double tDry = (p.killDry > 0.5f) ? 0.0 : std::cos (p.mix * M_PI * 0.5);
    const double tWet = std::sin (p.mix * M_PI * 0.5);
    const double tOut = dbToLin (p.output);
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
        if (os == 2)
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

        const float ol = (float) ((dl * gDry + wl * gWet) * gOut);
        const float orr = (float) ((dr * gDry + wr * gWet) * gOut);
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

    tele.inPeak = std::max (tele.inPeak.load(), pkIn);
    tele.outPeak = std::max (tele.outPeak.load(), pkOut);
    tele.recPeak = std::max (tele.recPeak.load(), recPk);
    for (int i = 0; i < NUM_HEADS; ++i)
        tele.headPeak[i] = std::max (tele.headPeak[i].load(), pkHead[i]);
    pkIn = pkOut = 0.0f; recPk = 0.0f;
    for (float& x : pkHead) x = 0.0f;
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
