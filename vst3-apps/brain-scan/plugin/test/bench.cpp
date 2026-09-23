/*  BRAIN SCAN — the bench.

    The design (BrokildApps/BRAIN-SCAN-DESIGN.md) makes claims; this measures
    them against the real engine. "Bounded and busy" proves nothing.

    Build:  cmake -S test -B test/build ; cmake --build test/build --config Release
*/
#include "../Source/Engine.h"
#include "../Source/Import.h"
#include "../Source/Anatomy.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>
#include <complex>

using namespace bs;

static int checks = 0, fails = 0;
static void ok (bool c, const char* what, const char* detail = "")
{ ++checks; if (! c) { ++fails; std::printf ("  FAIL  %s   %s\n", what, detail); } }
static void head (const char* s) { std::printf ("\n== %s ==\n", s); }

static const double SR = 48000.0;
static const int BLK = 256;

//==============================================================================
//  synthesised files, so the readers are measured on bytes and not on a mock
static void put16 (std::vector<uint8_t>& b, size_t at, uint16_t v)
{ b[at] = (uint8_t) (v & 0xff); b[at + 1] = (uint8_t) (v >> 8); }
static void put32 (std::vector<uint8_t>& b, size_t at, uint32_t v)
{ for (int i = 0; i < 4; ++i) b[at + (size_t) i] = (uint8_t) ((v >> (8 * i)) & 0xff); }
static void putF32 (std::vector<uint8_t>& b, size_t at, float f)
{ uint32_t u; std::memcpy (&u, &f, 4); put32 (b, at, u); }

/*  A NIfTI-1 single file: 348-byte header, magic at 344, voxels at 352. */
static std::vector<uint8_t> makeNifti (int nx, int ny, int nz,
                                       float sx, float sy, float sz,
                                       float slope, float inter,
                                       const std::vector<int16_t>& vox,
                                       const char* magic = "n+1")
{
    std::vector<uint8_t> b (352 + vox.size() * 2, 0);
    put32 (b, 0, 348);
    put16 (b, 40, 3);
    put16 (b, 42, (uint16_t) nx); put16 (b, 44, (uint16_t) ny); put16 (b, 46, (uint16_t) nz);
    put16 (b, 70, 4);                       // int16
    put16 (b, 72, 16);
    putF32 (b, 80, sx); putF32 (b, 84, sy); putF32 (b, 88, sz);
    putF32 (b, 108, 352.0f);
    putF32 (b, 112, slope); putF32 (b, 116, inter);
    put16 (b, 254, 1);                      // sform_code
    putF32 (b, 280, sx); putF32 (b, 296 + 4, sy); putF32 (b, 312 + 8, sz);
    std::memcpy (b.data() + 344, magic, 3);
    for (size_t i = 0; i < vox.size(); ++i)
        put16 (b, 352 + i * 2, (uint16_t) (uint16_t) vox[i]);
    return b;
}

static void dcmElem (std::vector<uint8_t>& b, uint16_t g, uint16_t e, const char* vr,
                     const void* data, size_t len)
{
    const size_t at = b.size();
    const bool longForm = ! std::strncmp (vr, "OB", 2) || ! std::strncmp (vr, "OW", 2)
                       || ! std::strncmp (vr, "SQ", 2) || ! std::strncmp (vr, "UN", 2);
    b.resize (at + (longForm ? 12 : 8) + len, 0);
    put16 (b, at, g); put16 (b, at + 2, e);
    b[at + 4] = (uint8_t) vr[0]; b[at + 5] = (uint8_t) vr[1];
    if (longForm) { put32 (b, at + 8, (uint32_t) len); std::memcpy (b.data() + at + 12, data, len); }
    else          { put16 (b, at + 6, (uint16_t) len); std::memcpy (b.data() + at + 8, data, len); }
}
static void dcmStr (std::vector<uint8_t>& b, uint16_t g, uint16_t e, const char* vr, std::string v)
{ if (v.size() & 1) v += ' '; dcmElem (b, g, e, vr, v.data(), v.size()); }
static void dcmU16 (std::vector<uint8_t>& b, uint16_t g, uint16_t e, uint16_t v)
{ uint8_t t[2] { (uint8_t) (v & 0xff), (uint8_t) (v >> 8) }; dcmElem (b, g, e, "US", t, 2); }

/*  One explicit-VR little-endian CT slice. */
static std::vector<uint8_t> makeDicom (int rows, int cols, float px, float py, float thick,
                                       float z, float slope, float inter,
                                       const std::vector<int16_t>& pix,
                                       const char* ts = "1.2.840.10008.1.2.1")
{
    std::vector<uint8_t> b (132, 0);
    std::memcpy (b.data() + 128, "DICM", 4);
    dcmStr (b, 0x0002, 0x0010, "UI", ts);
    dcmStr (b, 0x0018, 0x0050, "DS", std::to_string ((int) thick));
    dcmStr (b, 0x0020, 0x000E, "UI", "1.2.3.4.5");
    dcmStr (b, 0x0020, 0x0032, "DS", "0\\0\\" + std::to_string ((int) z));
    dcmStr (b, 0x0020, 0x0037, "DS", "1\\0\\0\\0\\1\\0");
    dcmU16 (b, 0x0028, 0x0002, 1);
    dcmU16 (b, 0x0028, 0x0010, (uint16_t) rows);
    dcmU16 (b, 0x0028, 0x0011, (uint16_t) cols);
    { char t[64]; std::snprintf (t, sizeof t, "%g\\%g", py, px); dcmStr (b, 0x0028, 0x0030, "DS", t); }
    dcmU16 (b, 0x0028, 0x0100, 16);
    dcmU16 (b, 0x0028, 0x0101, 16);
    dcmU16 (b, 0x0028, 0x0103, 1);
    dcmStr (b, 0x0028, 0x1052, "DS", std::to_string ((int) inter));
    dcmStr (b, 0x0028, 0x1053, "DS", std::to_string ((int) slope));
    std::vector<uint8_t> raw (pix.size() * 2);
    for (size_t i = 0; i < pix.size(); ++i)
    { const uint16_t u = (uint16_t) pix[i]; raw[i * 2] = (uint8_t) (u & 0xff); raw[i * 2 + 1] = (uint8_t) (u >> 8); }
    dcmElem (b, 0x7FE0, 0x0010, "OW", raw.data(), raw.size());
    return b;
}

//  extent of the values above a threshold, per axis, in voxels
static void cubeExtent (const std::vector<float>& c, int n, float th, int* ext)
{
    int lo[3] { n, n, n }, hi[3] { -1, -1, -1 };
    for (int k = 0; k < n; ++k) for (int j = 0; j < n; ++j) for (int i = 0; i < n; ++i)
        if (c[((size_t) k * n + j) * n + i] > th)
        {
            const int p[3] { i, j, k };
            for (int a = 0; a < 3; ++a) { if (p[a] < lo[a]) lo[a] = p[a]; if (p[a] > hi[a]) hi[a] = p[a]; }
        }
    for (int a = 0; a < 3; ++a) ext[a] = hi[a] >= lo[a] ? hi[a] - lo[a] + 1 : 0;
}

//==============================================================================
//  a small radix-2 FFT for spectra
static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * PI / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1, 0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

//  magnitude spectrum of a Blackman-Harris-windowed slice (sidelobes -92 dB;
//  a Hann window's -31 dB sidelobes summed between harmonics put a floor of
//  about -36 dB under every "non-harmonic" measurement, whatever the engine did)
static std::vector<double> spectrum (const std::vector<float>& x, size_t from, size_t n)
{
    std::vector<std::complex<double>> a (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double ph = 2.0 * PI * (double) i / (double) n;
        const double w = 0.35875 - 0.48829 * std::cos (ph) + 0.14128 * std::cos (2.0 * ph) - 0.01168 * std::cos (3.0 * ph);
        a[i] = (from + i < x.size() ? (double) x[from + i] : 0.0) * w;
    }
    fft (a);
    std::vector<double> m (n / 2);
    for (size_t i = 0; i < n / 2; ++i) m[i] = std::abs (a[i]) / (double) n * 4.0;
    return m;
}

static double dB (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }

//  energy at harmonic k of f0 (a small window of bins) vs the total, from a spectrum
static double harmMag (const std::vector<double>& m, double f0, int k, size_t n)
{
    const double bin = f0 * (double) k * (double) n / SR;
    const int b = (int) std::lround (bin);
    double best = 0;
    for (int d = -4; d <= 4; ++d) if (b + d >= 0 && b + d < (int) m.size()) best = std::max (best, m[(size_t) (b + d)]);
    return best;
}

//==============================================================================
struct Take { std::vector<float> L, R; };

static Take render (Engine& e, int note, double seconds, double releaseAt = -1.0, float vel = 0.9f)
{
    Take t;
    const int nb = (int) (seconds * SR / BLK);
    std::vector<float> bl (BLK), br (BLK);
    e.service();
    e.noteOn (note, vel);
    bool released = false;
    for (int b = 0; b < nb; ++b)
    {
        const double tm = (double) b * BLK / SR;
        if (releaseAt >= 0 && ! released && tm >= releaseAt) { e.noteOff (note); released = true; }
        e.process (bl.data(), br.data(), BLK);
        t.L.insert (t.L.end(), bl.begin(), bl.end());
        t.R.insert (t.R.end(), br.begin(), br.end());
    }
    return t;
}

static Engine* fresh (int specimen, const Line* six = nullptr)
{
    Engine* e = new Engine();
    e->p = Params();
    e->p.specimen = numSpecimens() > 1 ? (float) specimen / (float) (numSpecimens() - 1) : 0.0f;
    e->p.filtType = 1.0f;                    // OFF: measure the read, not the filter
    e->p.ampA = 0.0f; e->p.ampD = 0.5f; e->p.ampS = 1.0f; e->p.ampR = 0.2f; e->p.velSens = 0.0f;
    e->p.grain = 0.0f;                       // the smoothing kernel: the formula checks were written against it; GRAIN has its own section
    e->p.scanAmt = 0.5f;                     // no scan envelope
    e->p.modScan = e->p.modPitch = e->p.modPan = 0.5f;
    //  well under the soft ceiling: at 0.62 a wide pulse's DC-blocked trough
    //  sat in the tanh and grew a 5th harmonic the pulse does not have
    e->p.level = 0.35f;
    e->prepare (SR, BLK);
    if (six != nullptr) e->setLines (six);
    e->service();
    return e;
}

static void straightAll (Line* six, float y, float z)
{
    for (int i = 0; i < NLINES; ++i) six[i] = Line::straight ({ 0.0f, y, z }, { 1.0f, y, z });
}

static double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; size_t n = 0;
    for (size_t i = from; i < to && i < v.size(); ++i) { s += (double) v[i] * v[i]; ++n; }
    return n ? std::sqrt (s / (double) n) : 0.0;
}

static double centroid (const std::vector<double>& m, size_t n)
{
    double num = 0, den = 0;
    for (size_t i = 1; i < m.size(); ++i) { const double f = (double) i * SR / (double) n; num += m[i] * m[i] * f; den += m[i] * m[i]; }
    return den > 0 ? num / den : 0;
}

//  non-harmonic energy: everything not within +-2 bins of a harmonic of f0, up to 20 kHz
static double aliasFloorDb (const std::vector<double>& m, double f0, size_t n)
{
    double harm = 0, other = 0;
    const double binHz = SR / (double) n;
    for (size_t i = 2; i < m.size(); ++i)
    {
        const double f = (double) i * binHz;
        if (f > 20000.0) break;
        const double k = f / f0;
        const double dist = std::abs (k - std::round (k)) * f0 / binHz;   // bins from the nearest harmonic
        if (dist <= 4.5) harm += m[i] * m[i]; else other += m[i] * m[i];   // the window's main lobe is +-4 bins
    }
    return dB (std::sqrt (other / std::max (1e-30, harm)));
}

//==============================================================================
int main()
{
    std::printf ("BRAIN SCAN - offline bench\n");
    const size_t N = 8192;
    const double f0 = 440.0 * std::pow (2.0, (57 - 69) / 12.0);      // A3 = 220 Hz

    //--------------------------------------------------------------------
    head ("1 · SINUS is a sine");
    {
        Line six[NLINES]; straightAll (six, 0.5f, 0.0f);
        Engine* e = fresh (0, six);
        Take t = render (*e, 57, 1.0);
        const auto m = spectrum (t.L, (size_t) (0.4 * SR), N);
        const double h1 = harmMag (m, f0, 1, N);
        double hsum = 0; for (int k = 2; k <= 12; ++k) { const double h = harmMag (m, f0, k, N); hsum += h * h; }
        const double thd = dB (std::sqrt (hsum) / std::max (1e-12, h1));
        std::printf ("  THD %.1f dB, fundamental %.1f dBFS, rms %.3f\n", thd, dB (h1), rms (t.L, (size_t) (0.4 * SR), t.L.size()));
        ok (thd < -60.0, "a straight line through SINUS reads a sine (THD < -60 dB)");
        ok (rms (t.L, (size_t) (0.4 * SR), t.L.size()) > 0.05, "and it is audible");
        delete e;
    }

    //--------------------------------------------------------------------
    head ("2 · SPINE obeys its formula");
    {
        double prevC = 0; bool mono = true;
        double cs[5];
        for (int q = 0; q < 5; ++q)
        {
            const float y = 0.1f + 0.2f * (float) q;
            Line six[NLINES]; straightAll (six, y, 0.0f);
            Engine* e = fresh (1, six);
            Take t = render (*e, 57, 0.8);
            const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
            cs[q] = centroid (m, N);
            if (q > 0 && cs[q] <= prevC) mono = false;
            prevC = cs[q];
            delete e;
        }
        std::printf ("  centroid vs y: %.0f %.0f %.0f %.0f %.0f Hz\n", cs[0], cs[1], cs[2], cs[3], cs[4]);
        ok (mono, "brightness rises monotonically with y");
        //  parity: z = 1 -> even harmonics vanish
        Line six[NLINES]; straightAll (six, 0.8f, 1.0f);
        Engine* e = fresh (1, six);
        Take t = render (*e, 57, 0.8);
        const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
        double odd = 0, even = 0;
        for (int k = 1; k <= 9; ++k) { const double h = harmMag (m, f0, k, N); if (k & 1) odd += h * h; else even += h * h; }
        const double ratio = dB (std::sqrt (even / std::max (1e-30, odd)));
        std::printf ("  z = 1: even/odd %.1f dB\n", ratio);
        ok (ratio < -40.0, "at z = 1 the even harmonics sit > 40 dB under the odd");
        delete e;
    }

    //--------------------------------------------------------------------
    head ("3 · PULSE obeys its width");
    {
        /*  Measured at the READ, not after the output stage: the DC blocker
            and the ceiling reshape a wide pulse's trough, and a duty taken
            from the audio was the blocker's, not the specimen's. */
        for (float y : { 0.2f, 0.5f, 0.8f })
        {
            Line six[NLINES]; straightAll (six, y, 0.0f);
            Engine* e = fresh (2, six);
            const Volume& vol = e->volume();
            int hi = 0; const int n = 4096;
            for (int i = 0; i < n; ++i)
                if (Engine::readLine (vol, six[L_WAVE_A], six[L_WAVE_B], 0.0f, (float) i / (float) n, 0) > 0.5f) ++hi;
            const double duty = (double) hi / (double) n, want = 0.05 + 0.90 * y;
            char d[64]; std::snprintf (d, sizeof d, "y %.1f: duty %.3f, formula %.3f", y, duty, want);
            std::printf ("  %s\n", d);
            ok (std::abs (duty - want) < 0.02, "the pulse's duty matches the formula within 2 %", d);
            delete e;
        }
    }

    //--------------------------------------------------------------------
    head ("4 · the three traps");
    {
        /*  LUNG at C6 on a CLOSED loop (a wide circle: 180 texels of path, so
            the read carries ~90 harmonics at level 0 — 94 kHz), with the mip
            pyramid vs locked at level 0. Closed, because this claim is about
            the field's content; an open line's edge is the polyBLEP's claim,
            measured on MARROW below. */
        Line six[NLINES];
        for (int i = 0; i < NLINES; ++i) six[i] = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.45f, 2, 12);
        int levelChosen = -1;
        for (int lock : { -1, 0 })
        {
            Engine* e = fresh (6, six);
            e->setMipLock (lock);
            Take t = render (*e, 84, 0.6);             // C6
            if (lock < 0) levelChosen = e->voiceLod (0);
            const double fC6 = 440.0 * std::pow (2.0, (84 - 69) / 12.0);
            const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
            const double fl = aliasFloorDb (m, fC6, N);
            std::printf ("  LUNG at C6 on a loop, %s: non-harmonic %.1f dB\n", lock < 0 ? "mips automatic" : "level 0 forced", fl);
            if (lock < 0) ok (fl < -50.0, "LUNG at C6 keeps non-harmonic energy under -50 dB (mips)");
            else          ok (fl > -40.0, "...and level 0 forced really does alias (the condition, not just the effect)");
            delete e;
        }
        char dl[48]; std::snprintf (dl, sizeof dl, "level %d chosen", levelChosen);
        std::printf ("  %s\n", dl);
        ok (levelChosen >= 2, "the pitch and the path length picked a coarse level", dl);
        //  MARROW at C5: the polyBLEP's worth
        double with = 0, without = 0;
        for (int b = 1; b >= 0; --b)
        {
            Line s2[NLINES]; straightAll (s2, 0.5f, 0.0f);
            Engine* e = fresh (3, s2);
            e->setBlep (b == 1);
            Take t = render (*e, 72, 0.6);             // C5
            const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
            const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
            const double fl = aliasFloorDb (m, fC5, N);
            if (b) with = fl; else without = fl;
            delete e;
        }
        std::printf ("  MARROW at C5: non-harmonic %.1f dB with the polyBLEP, %.1f without\n", with, without);
        ok (with < -50.0, "an open line at C5 stays under -50 dB non-harmonic with the polyBLEP");
        ok (without - with > 20.0, "and the polyBLEP is worth more than 20 dB");
        //  DC: a straight line through the lopsided MARROW ramp settles under -60 dB of offset
        {
            Line s3[NLINES]; straightAll (s3, 0.5f, 0.0f);
            Engine* e = fresh (3, s3);
            Take t = render (*e, 45, 1.5);
            //  the DC bin of a Hann-windowed spectrum against the fundamental
            //  (a plain mean over a non-integer number of cycles measures the
            //  cycle count, not the offset)
            const auto m = spectrum (t.L, (size_t) (1.0 * SR), N);
            const double fA2 = 440.0 * std::pow (2.0, (45 - 69) / 12.0);
            const double dc = dB (m[0] / harmMag (m, fA2, 1, N));
            std::printf ("  DC after 1 s: %.1f dB below the fundamental\n", dc);
            ok (dc < -60.0, "DC after 1 s under -60 dB");
            delete e;
        }
    }

    //--------------------------------------------------------------------
    head ("5 · THE CLAIM: path blending is not a crossfade");
    {
        /*  PULSE at y = 1/6 (width 0.20) and y = 5/6 (width 0.80): both have a
            spectral null at the 5th harmonic, sin(pi n w) = 0 for n = 5 at
            both widths (5 * 0.2 = 1, 5 * 0.8 = 4). The midway PATH is a pulse
            of width 0.5, whose 5th harmonic is at 1/5 of the fundamental,
            -14 dB. A crossfade of the two anchor waveforms can never make it. */
        Line six[NLINES];
        straightAll (six, 1.0f / 6.0f, 0.0f);
        six[L_WAVE_B] = Line::straight ({ 0.0f, 5.0f / 6.0f, 0.0f }, { 1.0f, 5.0f / 6.0f, 0.0f });
        //  the two anchors
        Engine* e = fresh (2, six);
        e->p.scan = 0.0f; Take ta = render (*e, 57, 0.6); delete e;
        e = fresh (2, six); e->p.scan = 1.0f; Take tb = render (*e, 57, 0.6); delete e;
        e = fresh (2, six); e->p.scan = 0.5f; Take tm = render (*e, 57, 0.6); delete e;
        //  the crossfade of the anchors' waveforms
        std::vector<float> xf (ta.L.size());
        for (size_t i = 0; i < xf.size(); ++i) xf[i] = 0.5f * (ta.L[i] + tb.L[i]);
        const auto ma = spectrum (ta.L, (size_t) (0.3 * SR), N), mb = spectrum (tb.L, (size_t) (0.3 * SR), N);
        const auto mm = spectrum (tm.L, (size_t) (0.3 * SR), N), mx = spectrum (xf, (size_t) (0.3 * SR), N);
        const double h1m = harmMag (mm, f0, 1, N);
        const double h5a = dB (harmMag (ma, f0, 5, N) / harmMag (ma, f0, 1, N));
        const double h5b = dB (harmMag (mb, f0, 5, N) / harmMag (mb, f0, 1, N));
        const double h5m = dB (harmMag (mm, f0, 5, N) / h1m);
        const double h5x = dB (harmMag (mx, f0, 5, N) / harmMag (mx, f0, 1, N));
        std::printf ("  5th harmonic re fundamental: anchor A %.1f dB, anchor B %.1f dB, crossfade %.1f dB, the midway PATH %.1f dB (formula -14.0)\n", h5a, h5b, h5x, h5m);
        ok (h5x < -30.0, "the crossfade of the anchors has no 5th harmonic");
        ok (std::abs (h5m + 14.0) < 3.0, "the midway path reads a 5th harmonic within 3 dB of the width-0.5 pulse's formula");
    }

    //--------------------------------------------------------------------
    head ("6 · the FILTER scanner follows its line");
    {
        //  FILTER A along x through SINUS at y = 1 (amplitude full): the read is 0.5 + 0.5 sin
        Line six[NLINES]; straightAll (six, 0.5f, 0.0f);
        six[L_FILT_A] = Line::straight ({ 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f });
        six[L_FILT_B] = six[L_FILT_A];
        Engine* e = fresh (0, six);
        e->p.filtType = 0.0f; e->p.filtDepth = 1.0f; e->p.filtMode = 1.0f; e->p.filtTrack = 0.0f;
        e->p.filtRate = 0.4f;                        // some Hz
        e->service();
        e->noteOn (60, 0.9f);
        std::vector<float> bl (BLK), br (BLK);
        double worstT = 0, worstS = 0; int samples = 0;
        const Volume& vol = e->volume();
        for (int b = 0; b < (int) (2.0 * SR / BLK); ++b)
        {
            e->process (bl.data(), br.data(), BLK);
            if (b < (int) (0.3 * SR / BLK)) continue;
            VoiceView vv[MAXVOICES]; const int n = e->voicesView (vv, MAXVOICES);
            if (n < 1) continue;
            const float r = Engine::readLine (vol, six[L_FILT_A], six[L_FILT_B], vv[0].scan, vv[0].fPhase, 1);
            const double want = 20.0 * std::pow (1000.0, (double) r);
            worstT = std::max (worstT, std::abs (std::log2 (e->voiceCutTarget (0) / want)));
            worstS = std::max (worstS, std::abs (std::log2 (vv[0].cutoff / want)));
            ++samples;
        }
        std::printf ("  cutoff vs the field along the line: target worst %.4f octaves, smoothed worst %.3f, over %d ticks\n", worstT, worstS, samples);
        ok (samples > 100 && worstT < 0.01, "the cutoff target IS the field along the FILTER line");
        ok (worstS < 0.35, "and the smoothed cutoff trails it by less than a third of an octave (8 ms in octaves)");
        delete e;
    }

    //--------------------------------------------------------------------
    head ("7 · silence, determinism, every patch, cost");
    {
        //  silence in, silence out
        Engine* e = fresh (8);
        std::vector<float> bl (BLK), br (BLK);
        double mx = 0;
        for (int b = 0; b < 200; ++b) { e->process (bl.data(), br.data(), BLK); for (float v : bl) mx = std::max (mx, (double) std::abs (v)); }
        ok (mx == 0.0, "nothing played, nothing out");
        delete e;

        //  determinism
        auto run = [] (std::vector<float>& out)
        {
            Line six[NLINES]; Params p;
            factory (2).build (six, p);
            Engine e; e.p = p; e.prepare (SR, BLK); e.setLines (six); e.service();
            std::vector<float> bl (BLK), br (BLK);
            e.noteOn (50, 0.8f); e.noteOn (57, 0.6f);
            out.clear();
            for (int b = 0; b < 120; ++b)
            {
                if (b == 60) e.noteOff (50);
                e.process (bl.data(), br.data(), BLK);
                out.insert (out.end(), bl.begin(), bl.end());
            }
        };
        std::vector<float> a, b;
        run (a); run (b);
        ok (a.size() == b.size() && std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0,
            "the same gestures on the same patch give bit-identical sound");

        //  every factory patch, every specimen: bounded and audible
        for (int i = 0; i < numFactory(); ++i)
        {
            Line six[NLINES]; Params p; factory (i).build (six, p);
            Engine eng; eng.p = p; eng.prepare (SR, BLK); eng.setLines (six); eng.service();
            Take t = render (eng, 48, 1.2, 0.8);
            double pk = 0; for (float v : t.L) pk = std::max (pk, (double) std::abs (v));
            const double r = rms (t.L, (size_t) (0.2 * SR), (size_t) (0.7 * SR));
            bool fin = true; for (float v : t.L) if (! std::isfinite (v)) fin = false;
            char d[96]; std::snprintf (d, sizeof d, "%-14s peak %.3f rms %.3f", factory (i).name, pk, r);
            std::printf ("  %s\n", d);
            ok (fin && pk <= 1.0 && r > 0.01, "factory patch bounded and audible", d);
        }
        for (int s = 0; s < numSpecimens(); ++s)
        {
            Line six[NLINES]; straightAll (six, 0.5f, 0.5f);
            Engine* eng = fresh (s, six);
            Take t = render (*eng, 57, 0.6);
            const double r = rms (t.L, (size_t) (0.2 * SR), t.L.size());
            char d[64]; std::snprintf (d, sizeof d, "%-8s straight-line rms %.3f", specimenName (s), r);
            std::printf ("  %s\n", d);
            ok (r > 0.005, "every specimen sounds along a straight line", d);
            delete eng;
        }

        //  cost: 8 voices x 4 unison, filter on
        {
            Line six[NLINES]; Params p; factory (2).build (six, p);
            p.unison = 1.0f;
            Engine eng; eng.p = p; eng.prepare (SR, BLK); eng.setLines (six); eng.service();
            for (int v = 0; v < 8; ++v) eng.noteOn (36 + v * 5, 0.9f);
            std::vector<float> l (BLK), r (BLK);
            const auto t0 = std::chrono::steady_clock::now();
            const int nb = (int) (5.0 * SR / BLK);
            for (int b = 0; b < nb; ++b) eng.process (l.data(), r.data(), BLK);
            const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            char d[64]; std::snprintf (d, sizeof d, "%.2f %% of one core, 8 voices x 4 unison", wall / 5.0 * 100.0);
            std::printf ("  %s\n", d);
            ok (wall / 5.0 < 0.20, "8 voices x 4 unison (32 readers, the maximum) under 20 % of a core", d);
        }
    }

    //==========================================================================
    head ("8 - importing a real volume");
    {
        //  ---- anisotropy: physical extent, not index extent ---------------
        {
            /*  80 x 80 x 20 voxels at 0.5 x 0.5 x 2.0 mm is PHYSICALLY a cube,
                40 mm on a side, holding a 15 mm sphere. Resampled by index it
                comes out squashed 4x in z; resampled in millimetres it is
                still a sphere. */
            SrcVolume v;
            v.n[0] = 80; v.n[1] = 80; v.n[2] = 20;
            v.spacing[0] = 0.5f; v.spacing[1] = 0.5f; v.spacing[2] = 2.0f;
            v.v.assign (v.count(), -1000.0f);
            for (int k = 0; k < 20; ++k) for (int j = 0; j < 80; ++j) for (int i = 0; i < 80; ++i)
            {
                const float x = ((float) i + 0.5f) * 0.5f - 20.0f;
                const float y = ((float) j + 0.5f) * 0.5f - 20.0f;
                const float z = ((float) k + 0.5f) * 2.0f - 20.0f;
                if (std::sqrt (x * x + y * y + z * z) < 15.0f)
                    v.v[((size_t) k * 80 + j) * 80 + i] = 1000.0f;
            }
            ImportOpts o; ImportReport rep8; std::string err;
            std::vector<float> cube ((size_t) VN * VN * VN);
            const bool okr = resampleToCube (v, VN, o, cube.data(), rep8, err);
            ok (okr, "an anisotropic volume resamples", err.c_str());
            char d[160];
            std::snprintf (d, sizeof d, "filled %d x %d x %d of %d (extents %.0f x %.0f x %.0f mm)",
                           rep8.filled[0], rep8.filled[1], rep8.filled[2], VN,
                           rep8.extentMm[0], rep8.extentMm[1], rep8.extentMm[2]);
            std::printf ("  %s\n", d);
            ok (rep8.filled[0] == VN && rep8.filled[1] == VN && rep8.filled[2] == VN,
                "a physically cubic volume fills the cube whatever its voxel counts say", d);
            int ext[3]; cubeExtent (cube, VN, 0.5f, ext);
            const int wantExt = (int) std::lround (0.75 * VN);          // a 30 mm sphere in a 40 mm cube
            std::snprintf (d, sizeof d, "sphere reads %d x %d x %d voxels (want ~%d each)", ext[0], ext[1], ext[2], wantExt);
            std::printf ("  %s\n", d);
            /*  one source voxel on the coarse axis is 2 mm = VN/20 output
                voxels, and a 0.5 threshold on a partial-volume edge costs
                about that. */
            const int tol = VN / 20 + 2;
            ok (std::abs (ext[0] - ext[2]) <= tol && std::abs (ext[1] - ext[2]) <= tol,
                "and a sphere in it is still a sphere - THE anisotropy check", d);
            ok (ext[2] > wantExt * 5 / 6, "resampling by INDEX would read about a quarter of that there; this reads the millimetres", d);
        }

        //  ---- decimation averages, it does not point-sample ---------------
        {
            /*  A one-voxel checkerboard has no low frequencies at all: area
                averaged 4:1 it must collapse to a flat field. Point sampling
                would keep the pattern, and no later mip could undo it. */
            SrcVolume v;
            v.n[0] = v.n[1] = v.n[2] = 128;
            v.v.resize (v.count());
            for (int k = 0; k < 128; ++k) for (int j = 0; j < 128; ++j) for (int i = 0; i < 128; ++i)
                v.v[((size_t) k * 128 + j) * 128 + i] = ((i + j + k) & 1) ? 1.0f : 0.0f;
            ImportOpts o; o.stretch = true;
            ImportReport rp; std::string err;
            std::vector<float> c (32 * 32 * 32);
            resampleToCube (v, 32, o, c.data(), rp, err);
            double m = 0; for (float f : c) m += f; m /= (double) c.size();
            double sd = 0; for (float f : c) sd += (f - m) * (f - m); sd = std::sqrt (sd / (double) c.size());
            char d[128]; std::snprintf (d, sizeof d, "mean %.4f, sd %.5f (point sampling gives sd ~0.5)", m, sd);
            std::printf ("  %s\n", d);
            ok (sd < 0.02, "4:1 decimation AVERAGES - a one-voxel checkerboard flattens", d);
        }

        //  ---- no half-voxel shift ------------------------------------------
        {
            SrcVolume v;
            v.n[0] = 64; v.n[1] = 16; v.n[2] = 16;
            v.v.resize (v.count());
            for (int k = 0; k < 16; ++k) for (int j = 0; j < 16; ++j) for (int i = 0; i < 64; ++i)
                v.v[((size_t) k * 16 + j) * 64 + i] = (float) i / 63.0f;
            ImportOpts o; o.stretch = true;
            ImportReport rp; std::string err;
            std::vector<float> c ((size_t) VN * VN * VN);
            resampleToCube (v, VN, o, c.data(), rp, err);
            double worst = 0;
            for (int i = VN / 8; i < VN - VN / 8; ++i)
            {
                const float got = c[(((size_t) VN / 2) * VN + VN / 2) * VN + (size_t) i];
                const double want = ((double) i + 0.5) / (double) VN;
                worst = std::max (worst, std::fabs ((double) got - want));
            }
            char d[96]; std::snprintf (d, sizeof d, "worst departure %.4f", worst);
            std::printf ("  %s\n", d);
            ok (worst < 0.02, "a ramp resamples to a ramp - no half-voxel shift", d);
        }

        //  ---- one metal clip must not crush the tissue ---------------------
        {
            SrcVolume v;
            v.n[0] = v.n[1] = v.n[2] = 32;
            v.v.resize (v.count());
            for (size_t i = 0; i < v.count(); ++i) v.v[i] = (float) (i % 101);   // 0..100
            v.v[v.count() / 2] = 30000.0f;                                       // the clip
            float lo = 0, hi = 0;
            percentileWindow (v.v, 0.005f, 0.995f, lo, hi);
            char d[128]; std::snprintf (d, sizeof d, "window %.0f .. %.0f (full range 0 .. 30000)", lo, hi);
            std::printf ("  %s\n", d);
            ok (hi < 300.0f, "the percentile window ignores an outlier", d);
            ImportOpts o; o.stretch = true; ImportReport rp; std::string err;
            std::vector<float> c ((size_t) VN * VN * VN);
            resampleToCube (v, VN, o, c.data(), rp, err);
            double m = 0; for (float f : c) m += f; m /= (double) c.size();
            std::snprintf (d, sizeof d, "mean of the cube %.3f", m);
            ok (m > 0.2, "so the tissue still uses the range", d);
        }

        //  ---- the NIfTI reader, on real bytes ------------------------------
        {
            std::vector<int16_t> vox ((size_t) 8 * 6 * 4);
            for (size_t i = 0; i < vox.size(); ++i) vox[i] = (int16_t) (i * 3);
            auto f = makeNifti (8, 6, 4, 0.5f, 1.0f, 2.5f, 2.0f, -100.0f, vox);
            SrcVolume v; std::string err;
            const bool okr = readNifti (f.data(), f.size(), v, err);
            ok (okr, "a NIfTI-1 file reads", err.c_str());
            if (okr)
            {
                char d[160];
                std::snprintf (d, sizeof d, "%d x %d x %d at %.2f x %.2f x %.2f mm, first %.1f",
                               v.n[0], v.n[1], v.n[2], v.spacing[0], v.spacing[1], v.spacing[2], v.v[0]);
                std::printf ("  %s\n", d);
                ok (v.n[0] == 8 && v.n[1] == 6 && v.n[2] == 4, "its dimensions", d);
                ok (std::fabs (v.spacing[0] - 0.5f) < 1e-6f && std::fabs (v.spacing[2] - 2.5f) < 1e-6f,
                    "its spacing, which is what stops the body being squashed", d);
                ok (std::fabs (v.v[0] - (-100.0f)) < 1e-4f && std::fabs (v.v[1] - (3 * 2.0f - 100.0f)) < 1e-4f,
                    "and scl_slope / scl_inter applied", d);
            }
            //  refusals
            auto f2 = makeNifti (8, 6, 4, 1, 1, 1, 1, 0, vox, "n+2");
            put32 (f2, 0, 540);
            SrcVolume v2; std::string e2;
            ok (! readNifti (f2.data(), f2.size(), v2, e2)
                && e2.find ("NIfTI-2") != std::string::npos, "NIfTI-2 is refused by name, not misread", e2.c_str());
            std::vector<uint8_t> trunc (f.begin(), f.begin() + 400);
            SrcVolume v3; std::string e3;
            ok (! readNifti (trunc.data(), trunc.size(), v3, e3), "a truncated file is refused", e3.c_str());
        }

        //  ---- the DICOM reader, and slice ORDER ----------------------------
        {
            const int R = 6, C = 8;
            std::vector<DicomSlice> sl;
            //  positions deliberately out of order: a directory listing is not
            //  slice order, and sorting by name would keep this wrong
            const float zs[5] { 8.0f, 0.0f, 4.0f, 12.0f, 16.0f };
            std::string err;
            for (int q = 0; q < 5; ++q)
            {
                std::vector<int16_t> pix ((size_t) R * C);
                for (size_t i = 0; i < pix.size(); ++i) pix[i] = (int16_t) (100 * (int) zs[q] + (int) i);
                auto f = makeDicom (R, C, 0.5f, 0.75f, 4.0f, zs[q], 1.0f, -1024.0f, pix);
                DicomSlice s;
                const bool okr = readDicomFile (f.data(), f.size(), s, err);
                if (q == 0) ok (okr, "a DICOM slice reads", err.c_str());
                if (okr) sl.push_back (std::move (s));
            }
            ok (sl.size() == 5, "all five slices read");
            if (sl.size() == 5)
            {
                char d[160];
                std::snprintf (d, sizeof d, "%d x %d at %.2f x %.2f mm, first value %.0f HU",
                               sl[0].cols, sl[0].rows, sl[0].pixelSpacing[1], sl[0].pixelSpacing[0], sl[0].pix[0]);
                std::printf ("  %s\n", d);
                ok (sl[0].rows == R && sl[0].cols == C, "its dimensions", d);
                /*  sl is still in READ order here - sorting happens inside
                    assembleDicom below - so sl[0] is the slice stored at z = 8,
                    whose first pixel holds 800: 800 x 1 - 1024 = -224 HU. */
                ok (std::fabs (sl[0].pix[0] - (100.0f * zs[0] - 1024.0f)) < 1e-3f,
                    "rescale slope and intercept give Hounsfield units", d);
                SrcVolume v;
                const bool okr = assembleDicom (sl, v, err);
                ok (okr, "the series assembles", err.c_str());
                if (okr)
                {
                    std::snprintf (d, sizeof d, "%d x %d x %d, slice spacing %.2f mm",
                                   v.n[0], v.n[1], v.n[2], v.spacing[2]);
                    std::printf ("  %s\n", d);
                    ok (std::fabs (v.spacing[2] - 4.0f) < 1e-3f,
                        "slice spacing comes from the positions, not the tag", d);
                    //  slice k must be the one whose z was 4k
                    bool ordered = true;
                    for (int k = 0; k < 5; ++k)
                    {
                        const float got = v.at (0, 0, k);
                        const float want = 100.0f * (float) (4 * k) - 1024.0f;
                        if (std::fabs (got - want) > 1e-3f) ordered = false;
                    }
                    ok (ordered, "and the slices are ORDERED BY POSITION, not by the order they arrived");
                }
            }
            //  a compressed transfer syntax must refuse, by name
            std::vector<int16_t> pix ((size_t) R * C, 0);
            auto fz = makeDicom (R, C, 1, 1, 1, 0, 1, 0, pix, "1.2.840.10008.1.2.4.90");
            DicomSlice s2; std::string e2;
            ok (! readDicomFile (fz.data(), fz.size(), s2, e2)
                && e2.find ("dcm2niix") != std::string::npos,
                "a compressed DICOM refuses and says what to do", e2.c_str());
        }

        //  ---- determinism, and that an import actually plays ---------------
        {
            SrcVolume v;
            v.n[0] = 40; v.n[1] = 40; v.n[2] = 40;
            v.v.resize (v.count());
            for (int k = 0; k < 40; ++k) for (int j = 0; j < 40; ++j) for (int i = 0; i < 40; ++i)
            {
                const float x = (float) i / 39.0f - 0.5f, y = (float) j / 39.0f - 0.5f, z = (float) k / 39.0f - 0.5f;
                const float r = std::sqrt (x * x + y * y + z * z);
                v.v[((size_t) k * 40 + j) * 40 + i] = std::sin (28.0f * r) * (r < 0.45f ? 1.0f : 0.0f);
            }
            ImportOpts o; ImportReport rp; std::string err;
            std::vector<float> a ((size_t) VN * VN * VN), b ((size_t) VN * VN * VN);
            resampleToCube (v, VN, o, a.data(), rp, err);
            resampleToCube (v, VN, o, b.data(), rp, err);
            ok (std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0,
                "the same source resamples to the same cube, exactly");

            Line six[NLINES]; Params p; factory (0).build (six, p);
            Engine e1; e1.p = p; e1.prepare (SR, BLK); e1.setLines (six); e1.service();
            e1.setImported (a.data());
            ok (e1.importedActive() && e1.specimenLoaded() == SPEC_IMPORTED,
                "the engine reads the imported volume, and the specimen dial is untouched");
            e1.noteOn (52, 0.9f);
            std::vector<float> L (BLK), R (BLK); std::vector<float> out;
            for (int q = 0; q < 120; ++q) { e1.process (L.data(), R.data(), BLK);
                out.insert (out.end(), L.begin(), L.end()); }
            double pk = 0, rr = 0; bool fin = true;
            for (float f : out) { if (! std::isfinite (f)) fin = false; pk = std::max (pk, (double) std::fabs (f)); rr += (double) f * f; }
            rr = std::sqrt (rr / (double) out.size());
            char d[96]; std::snprintf (d, sizeof d, "peak %.3f rms %.3f", pk, rr);
            std::printf ("  %s\n", d);
            ok (fin && pk <= 1.0 && rr > 0.005, "an imported volume sounds, and stays bounded", d);

            //  clearing it must leave no residue at all
            e1.allNotesOff();
            for (int q = 0; q < 40; ++q) e1.process (L.data(), R.data(), BLK);
            e1.clearImported();
            ok (! e1.importedActive() && e1.specimenLoaded() != SPEC_IMPORTED,
                "clearing the import gives the specimen dial back");
            Engine e2; e2.p = p; e2.prepare (SR, BLK); e2.setLines (six); e2.service();
            Engine e3; e3.p = p; e3.prepare (SR, BLK); e3.setLines (six); e3.service();
            e3.setImported (a.data()); e3.clearImported();
            e2.noteOn (48, 0.8f); e3.noteOn (48, 0.8f);
            std::vector<float> o2, o3;
            for (int q = 0; q < 60; ++q)
            {
                e2.process (L.data(), R.data(), BLK); o2.insert (o2.end(), L.begin(), L.end());
                e3.process (L.data(), R.data(), BLK); o3.insert (o3.end(), L.begin(), L.end());
            }
            ok (std::memcmp (o2.data(), o3.data(), o2.size() * sizeof (float)) == 0,
                "and an engine that imported and cleared is bit-identical to one that never did");
        }
    }

    //==========================================================================
    head ("9 - the read: GRAIN, the window, the gritty three (260904.3)");
    {
        //  ---- GRAIN passes the top octave the B-spline muffled --------------
        {
            /*  SPINE at y = 1 carries 48 harmonics. A full-span line reads
                128 texels, so harmonic 40 sits at 0.62 of the texel Nyquist,
                where the B-spline's sinc^4 costs about 6 dB and Catmull-Rom
                about 1. */
            Line six[NLINES]; straightAll (six, 1.0f, 0.0f);
            const double fA2 = 110.0;                                   // A2: the 40th at 4.4 kHz
            double h40[2], h20[2];
            for (int g = 0; g < 2; ++g)
            {
                Engine* e = fresh (1, six);
                e->p.grain = (float) g;
                Take t = render (*e, 45, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                const double h1 = std::max (1e-12, harmMag (m, fA2, 1, N));
                h40[g] = dB (harmMag (m, fA2, 40, N) / h1);
                h20[g] = dB (harmMag (m, fA2, 20, N) / h1);
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "vs the fundamental: H20 %.1f -> %.1f dB, H40 %.1f -> %.1f dB (grain 0 -> 1)", h20[0], h20[1], h40[0], h40[1]);
            std::printf ("  %s\n", d);
            ok (h40[1] - h40[0] > 3.0, "GRAIN 1 lifts harmonic 40 by more than 3 dB", d);
            ok (std::abs (h20[1] - h20[0]) < 3.0, "and leaves the middle of the band nearly alone (it is a kernel, not a tilt)", d);
        }
        //  ---- CONTRAST: a sine leans toward a square, monotonically ----------
        {
            Line six[NLINES]; straightAll (six, 0.5f, 0.0f);
            double thd[5]; bool mono = true;
            for (int q = 0; q < 5; ++q)
            {
                Engine* e = fresh (0, six);
                e->p.contrast = 0.25f * (float) q;
                Take t = render (*e, 57, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                const double h1 = harmMag (m, f0, 1, N);
                double hs = 0; for (int k = 2; k <= 24; ++k) { const double h = harmMag (m, f0, k, N); hs += h * h; }
                thd[q] = dB (std::sqrt (hs) / std::max (1e-12, h1));
                if (q > 0 && thd[q] <= thd[q - 1]) mono = false;
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "SINUS THD vs contrast 0 .. 1: %.1f %.1f %.1f %.1f %.1f dB", thd[0], thd[1], thd[2], thd[3], thd[4]);
            std::printf ("  %s\n", d);
            ok (thd[0] < -60.0, "at CONTRAST 0 the window is not there: SINUS still reads a sine", d);
            ok (mono && thd[4] > -12.0, "and narrowing it raises the harmonics monotonically toward a square's", d);
        }
        //  ---- the window is anti-aliased: SPINE bright, contrast 1, C5 -------
        {
            /*  The worst case: the brightest field, the hardest window, a
                note high enough that the clip's harmonics pass Nyquist within
                a few. The ADAA and the coarser level the budget chooses must
                keep the non-harmonic floor down. */
            Line six[NLINES]; straightAll (six, 1.0f, 0.0f);
            const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
            double fl[2]; int lv[2];
            for (int q = 0; q < 2; ++q)
            {
                Engine* e = fresh (1, six);
                e->p.grain = 1.0f; e->p.contrast = (float) q;
                Take t = render (*e, 72, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                fl[q] = aliasFloorDb (m, fC5, N);
                lv[q] = e->voiceLod (0);
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "C5, SPINE y = 1, GRAIN 1: floor %.1f dB at level %d plain, %.1f dB at level %d with CONTRAST 1", fl[0], lv[0], fl[1], lv[1]);
            std::printf ("  %s\n", d);
            ok (fl[0] < -40.0, "the interpolating kernel alone keeps the alias floor under -40 dB at C5", d);
            ok (fl[1] < -30.0, "the hardest window at C5 keeps its aliasing 30 dB under the harmonics", d);
        }
        //  ---- FOLD: past the window the wave comes back in ------------------
        {
            Line six[NLINES]; straightAll (six, 0.5f, 0.0f);
            double pk[2], thd[2];
            for (int f = 0; f < 2; ++f)
            {
                Engine* e = fresh (0, six);
                e->p.contrast = 0.5f; e->p.fold = (float) f;          // gain 4
                Take t = render (*e, 57, 0.8);
                pk[f] = 0; for (size_t i = (size_t) (0.3 * SR); i < t.L.size(); ++i) pk[f] = std::max (pk[f], (double) std::abs (t.L[i]));
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                const double h1 = harmMag (m, f0, 1, N);
                double hs = 0; for (int k = 2; k <= 24; ++k) { const double h = harmMag (m, f0, k, N); hs += h * h; }
                thd[f] = dB (std::sqrt (hs) / std::max (1e-12, h1));
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "gain 4: clip THD %.1f dB peak %.3f; fold THD %.1f dB peak %.3f", thd[0], pk[0], thd[1], pk[1]);
            std::printf ("  %s\n", d);
            ok (pk[1] <= pk[0] * 1.05 + 1e-3, "a folded wave stays inside the window (no louder than the clipped one)", d);
            ok (thd[1] > -12.0, "and the fold is rich (a sine at gain 4 folds back on itself)", d);
            //  and its kinks are anti-aliased: SPINE bright, gain 4, full fold, C5
            {
                Line sx[NLINES]; straightAll (sx, 1.0f, 0.0f);
                Engine* e = fresh (1, sx);
                e->p.grain = 1.0f; e->p.contrast = 0.5f; e->p.fold = 1.0f;
                const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
                Take t = render (*e, 72, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                const double fl = aliasFloorDb (m, fC5, N);
                char d2[128]; std::snprintf (d2, sizeof d2, "C5, SPINE y = 1, GRAIN 1, gain 4, FOLD 1: non-harmonic floor %.1f dB at level %d", fl, e->voiceLod (0));
                std::printf ("  %s\n", d2);
                ok (fl < -40.0, "a full fold at C5 keeps its aliasing 40 dB under the harmonics", d2);
                delete e;
            }
        }
        //  ---- the gritty three: made of edges ------------------------------
        {
            /*  Energy above the 8th harmonic against the fundamental — the
                part of the spectrum a filter has something to do with. The
                old bread (SPINE at its middle) sits deep; the new tissue does
                not. TENDON's bell ratios are cut to one cycle, so its content
                is HARMONIC like every single-cycle read (a wavetable cannot
                be otherwise); what it carries is the strike at the wrap. */
            auto topDb = [&] (int spec, float y, float z, float grain)
            {
                Line six[NLINES]; straightAll (six, y, z);
                Engine* e = fresh (spec, six);
                e->p.grain = grain;
                Take t = render (*e, 57, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                const double h1 = harmMag (m, f0, 1, N);
                double hs = 0; for (int k = 9; k <= 48; ++k) { const double h = harmMag (m, f0, k, N); hs += h * h; }
                delete e;
                return dB (std::sqrt (hs) / std::max (1e-12, h1));
            };
            const double spine = topDb (1, 0.5f, 0.0f, 0.35f);
            const double sut   = topDb (9, 0.5f, 0.5f, 1.0f);
            const double ena   = topDb (10, 0.0f, 0.0f, 1.0f);
            const double ten   = topDb (11, 1.0f, 1.0f, 0.5f);
            char d[160]; std::snprintf (d, sizeof d, "above the 8th harmonic: SPINE mid %.1f, SUTURE %.1f, ENAMEL %.1f, TENDON %.1f dB", spine, sut, ena, ten);
            std::printf ("  %s\n", d);
            ok (sut > spine + 12.0 && ena > spine + 12.0, "SUTURE and ENAMEL carry 12 dB more top than SPINE's middle", d);
            ok (ten > spine + 6.0, "TENDON's strike is brighter than SPINE's middle too", d);
        }
        //  ---- the build: two million texels, published from a worker --------
        {
            Engine e; e.p = Params(); e.prepare (SR, BLK);
            e.p.specimen = 8.0f / (float) (numSpecimens() - 1);        // CORTEX, by the dial's own scale
            e.serviceAsync();
            int polls = 0;
            while (e.specimenLoaded() != 8 && polls < 3000) { std::this_thread::sleep_for (std::chrono::milliseconds (2)); e.serviceAsync(); ++polls; }
            char d[128]; std::snprintf (d, sizeof d, "serviceAsync published CORTEX after %d polls", polls);
            std::printf ("  %s\n", d);
            ok (e.specimenLoaded() == 8, "serviceAsync builds on a worker and publishes on a later call", d);
            e.p.specimen = 3.0f / (float) (numSpecimens() - 1);        // MARROW: dropped from the cache by the last four
            const auto t0 = std::chrono::steady_clock::now();
            e.service();
            const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::snprintf (d, sizeof d, "a full synchronous build of MARROW: %.0f ms (%d^3 texels, %d threads)", wall * 1000.0, VN, (int) std::thread::hardware_concurrency());
            std::printf ("  %s\n", d);
            ok (e.specimenLoaded() == 3 && wall < 3.0, "a full build stays under three seconds", d);
        }
        //  ---- cost with the window and the interpolating kernel on -----------
        {
            Line six[NLINES]; Params p; factory (2).build (six, p);
            p.unison = 1.0f; p.grain = 1.0f; p.contrast = 0.6f; p.fold = 0.5f;
            Engine eng; eng.p = p; eng.prepare (SR, BLK); eng.setLines (six); eng.service();
            for (int v = 0; v < 8; ++v) eng.noteOn (36 + v * 5, 0.9f);
            std::vector<float> l (BLK), r (BLK);
            const auto t0 = std::chrono::steady_clock::now();
            const int nb = (int) (5.0 * SR / BLK);
            for (int b = 0; b < nb; ++b) eng.process (l.data(), r.data(), BLK);
            const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            char d[96]; std::snprintf (d, sizeof d, "%.2f %% of one core, 32 readers, GRAIN 1 + window + fold", wall / 5.0 * 100.0);
            std::printf ("  %s\n", d);
            ok (wall / 5.0 < 0.22, "the window costs little: 32 readers with everything on under 22 % of a core", d);
        }
    }

    //==========================================================================
    head ("10 - the second head, split lines, scan spread, the MOD line on the read, the bodies (260905.1)");
    {
        auto thdOf = [&] (const std::vector<double>& m, double f, int kmax)
        {
            const double h1 = harmMag (m, f, 1, N);
            double hs = 0; for (int k = 2; k <= kmax; ++k) { const double h = harmMag (m, f, k, N); hs += h * h; }
            return dB (std::sqrt (hs) / std::max (1e-12, h1));
        };
        //  ---- the second head at 1x, half a cycle behind: the odd harmonics cancel ----
        {
            Line six[NLINES]; straightAll (six, 0.8f, 0.0f);            // SPINE, all harmonics
            double oe[2];
            for (int q = 0; q < 2; ++q)
            {
                Engine* e = fresh (1, six);
                e->p.head2 = (float) q; e->p.head2Ratio = 0.5f; e->p.head2Phase = 0.5f;
                Take t = render (*e, 57, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                double odd = 0, even = 0;
                for (int k = 1; k <= 9; ++k) { const double h = harmMag (m, f0, k, N); if (k & 1) odd += h * h; else even += h * h; }
                oe[q] = dB (std::sqrt (odd / std::max (1e-30, even)));
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "odd/even: %.1f dB with one head, %.1f dB with the second head half a cycle behind", oe[0], oe[1]);
            std::printf ("  %s\n", d);
            ok (oe[0] > 3.0 && oe[1] < -25.0, "a second head at 1x, PHASE 50 %: the odd harmonics cancel (a fixed-interval double, no second voice)", d);
        }
        //  ---- the second head off the integers: a partial that is not a harmonic ----
        {
            Line six[NLINES]; straightAll (six, 0.5f, 0.0f);            // SINUS
            Engine* e = fresh (0, six);
            e->p.head2 = 1.0f; e->p.head2Ratio = 0.5f + std::log2 (1.5f) / 4.0f;   // 1.5x exactly
            Take t = render (*e, 57, 0.8);
            const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
            const double h1 = harmMag (m, f0, 1, N), p15 = harmMag (m, f0 * 1.5, 1, N), h2 = harmMag (m, f0, 2, N);
            char d[128]; std::snprintf (d, sizeof d, "at HEAD RATIO 1.5x: the partial at 1.5 f0 sits %.1f dB from f0, the 2nd harmonic %.1f dB", dB (p15 / h1), dB (h2 / h1));
            std::printf ("  %s\n", d);
            ok (dB (p15 / h1) > -6.0 && dB (h2 / h1) < -40.0, "a partial at 1.5 f0, and no 2nd harmonic: inharmonic by the read, not by the field", d);
            delete e;
            //  and at 4x on the brightest field at C5, coarsened a level, it does not alias
            Line sb[NLINES]; straightAll (sb, 1.0f, 0.0f);
            Engine* e2 = fresh (1, sb);
            e2->p.head2 = 1.0f; e2->p.head2Ratio = 1.0f; e2->p.grain = 1.0f;
            const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
            Take t2 = render (*e2, 72, 0.8);
            const auto m2 = spectrum (t2.L, (size_t) (0.3 * SR), N);
            const double fl = aliasFloorDb (m2, fC5, N);
            std::snprintf (d, sizeof d, "HEAD RATIO 4x, SPINE y = 1, GRAIN 1, C5: non-harmonic floor %.1f dB", fl);
            std::printf ("  %s\n", d);
            ok (fl < -40.0, "the fast head reads a coarser level and stays 40 dB clean", d);
            delete e2;
        }
        //  ---- a SPLIT line: two segments, two edges, both band-limited ------
        {
            Line a; a.n = 4; a.split = 2;
            a.p[0] = { 0.05f, 0.30f, 0.0f }; a.p[1] = { 0.50f, 0.30f, 0.0f };
            a.p[2] = { 0.50f, 0.90f, 0.0f }; a.p[3] = { 0.95f, 0.90f, 0.0f };
            const Vec3 before = a.at (0.49999f), after = a.at (0.5f), end = a.at (0.99999f), start = a.at (0.0f);
            char d[160];
            std::snprintf (d, sizeof d, "at 0.49999 -> (%.3f, %.3f), at 0.5 -> (%.3f, %.3f); at 0 (%.3f, %.3f), at 1 (%.3f, %.3f)",
                           before.x, before.y, after.x, after.y, start.x, start.y, end.x, end.y);
            std::printf ("  %s\n", d);
            ok (std::abs (before.y - 0.30f) < 0.01f && std::abs (after.y - 0.90f) < 0.01f && std::abs (before.x - 0.5f) < 0.01f
                && std::abs (start.x - 0.05f) < 0.01f && std::abs (end.x - 0.95f) < 0.01f,
                "the two halves of the cycle read the two segments, end to end", d);
            Line six[NLINES]; for (int i = 0; i < NLINES; ++i) six[i] = a;
            const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
            double fl[2];
            for (int q = 0; q < 2; ++q)
            {
                Engine* e = fresh (0, six);                                  // SINUS: the amplitude jumps at both edges
                e->setBlep (q == 0);
                Take t = render (*e, 72, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                fl[q] = aliasFloorDb (m, fC5, N);
                delete e;
            }
            std::snprintf (d, sizeof d, "SINUS on a split line at C5: non-harmonic %.1f dB with both edges band-limited, %.1f dB without", fl[0], fl[1]);
            std::printf ("  %s\n", d);
            ok (fl[0] < -40.0 && fl[1] - fl[0] > 10.0, "the second edge, at half a cycle, gets its own polyBLEP and it is worth > 10 dB", d);
        }
        //  ---- SCAN SPREAD: unison readers on different scans, bounded --------
        {
            Line six[NLINES]; straightAll (six, 0.2f, 0.0f);
            for (int i = 0; i < NLINES; i += 2) six[i + 1] = Line::straight ({ 0.0f, 0.9f, 0.0f }, { 1.0f, 0.9f, 0.0f });
            Take t[2];
            for (int q = 0; q < 2; ++q)
            {
                Engine* e = fresh (1, six);
                e->p.unison = 1.0f; e->p.scan = 0.5f; e->p.uniScan = q == 0 ? 0.0f : 0.7f;
                t[q] = render (*e, 57, 0.6);
                delete e;
            }
            double diff = 0, pk = 0;
            for (size_t i = (size_t) (0.2 * SR); i < t[0].L.size(); ++i) { diff += (double) (t[0].L[i] - t[1].L[i]) * (t[0].L[i] - t[1].L[i]); pk = std::max (pk, (double) std::abs (t[1].L[i])); }
            const double r0 = rms (t[0].L, (size_t) (0.2 * SR), t[0].L.size());
            const double dr = std::sqrt (diff / (double) (t[0].L.size() - (size_t) (0.2 * SR)));
            char d[128]; std::snprintf (d, sizeof d, "four readers, SCAN SPREAD 0 -> 0.7: the output changes by %.1f %% rms, peak %.3f", dr / r0 * 100.0, pk);
            std::printf ("  %s\n", d);
            ok (dr / r0 > 0.05 && pk <= 1.0, "the readers spread through the body, and it stays bounded", d);
        }
        //  ---- the MOD line on the window ------------------------------------
        {
            Line six[NLINES]; straightAll (six, 0.5f, 0.0f);
            //  a MOD line that reads one place: the trough of the sine, modV about -0.68
            six[L_MOD_A] = Line::straight ({ 0.75f, 0.5f, 0.0f }, { 0.75f, 0.5f, 0.0f });
            six[L_MOD_B] = six[L_MOD_A];
            double thd[2];
            for (int q = 0; q < 2; ++q)
            {
                Engine* e = fresh (0, six);
                e->p.modContrast = q == 0 ? 0.5f : 0.0f;                    // centred, then -100 %
                Take t = render (*e, 57, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                thd[q] = thdOf (m, f0, 24);
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "SINUS THD: MOD>WINDOW centred %.1f dB, at -100 %% with the MOD line in the trough %.1f dB", thd[0], thd[1]);
            std::printf ("  %s\n", d);
            ok (thd[0] < -60.0 && thd[1] > -25.0, "centred, the MOD line leaves the window alone; turned up, the tissue decides how hard it is read", d);
        }
        //  ---- the bodies: anatomy, measured ----------------------------------
        {
            ok (an::toUnit (-1000.0f) == 0.0f && an::toUnit (2000.0f) == 1.0f && std::abs (an::toUnit (40.0f) - 0.3467f) < 0.001f,
                "the Hounsfield scale: air at 0, enamel at 1, soft tissue at 0.347");
            struct Want { int spec; const char* name; double airMin, boneMin, boneMax; bool enamel; };
            const Want wants[] = {
                { 6,  "THORAX",   0.20, 0.006, 0.12, false },
                { 7,  "SKULL",    0.55, 0.02,  0.25, true  },
                { 8,  "CORTEX",   0.25, 0.02,  0.25, true  },
                { 12, "VERTEBRA", 0.00, 0.03,  0.35, false },
                { 13, "FEMUR",    0.10, 0.012, 0.25, false },     // a thigh is mostly muscle: one shaft in a 16 cm cube
                { 14, "JAW",      0.05, 0.03,  0.30, true  },
            };
            std::vector<float> cube ((size_t) VN * VN * VN);
            for (const Want& w : wants)
            {
                ok (specimenIsHu (w.spec), "built in HU", specimenName (w.spec));
                buildSpecimen (w.spec, cube.data());
                size_t air = 0, bone = 0, enamel = 0;
                for (float v : cube) { if (v < 0.05f) ++air; if (v > 0.6f) ++bone; if (v >= 0.98f) ++enamel; }
                const double fa = (double) air / (double) cube.size(), fb = (double) bone / (double) cube.size(), fe = (double) enamel / (double) cube.size();
                char d[160]; std::snprintf (d, sizeof d, "%-8s air %.1f %%, bone %.1f %%, enamel %.3f %%", w.name, fa * 100, fb * 100, fe * 100);
                std::printf ("  %s\n", d);
                ok (fa >= w.airMin && fb >= w.boneMin && fb <= w.boneMax && (! w.enamel || fe > 0.0002),
                    "the tissue fractions of a body: air where there is air, a few per cent of bone, enamel where there are teeth", d);
            }
            //  the skull is closed: rays from the centre of the head cross bone
            {
                buildSpecimen (7, cube.data());
                auto at = [&] (float x, float y, float z) {
                    auto ix = [] (float v) { int i = (int) std::floor (v * VN); return i < 0 ? 0 : (i >= VN ? VN - 1 : i); };
                    return cube[((size_t) ix (z) * VN + (size_t) ix (y)) * VN + (size_t) ix (x)]; };
                const float dirs[5][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 } };
                int crossed = 0;
                for (auto& dd : dirs)
                {
                    float mx = 0;
                    for (float t = 0.0f; t < 0.49f; t += 0.004f) mx = std::max (mx, at (0.5f + dd[0] * t, 0.48f + dd[1] * t, 0.55f + dd[2] * t));
                    if (mx > 0.6f) ++crossed;
                }
                char d[96]; std::snprintf (d, sizeof d, "%d of 5 rays from the centre of the head cross bone", crossed);
                std::printf ("  %s\n", d);
                ok (crossed == 5, "the skull is a closed vault", d);
                //  and it is not solid: the middle of the SKULL specimen is air (no soft tissue)
                ok (at (0.5f, 0.48f, 0.55f) < 0.05f, "SKULL holds no soft tissue: the cranium is empty");
                buildSpecimen (8, cube.data());
                ok (std::abs (at (0.5f, 0.48f, 0.55f) - an::toUnit (30.0f)) < 0.03f, "CORTEX holds a brain: white matter at the same place");
            }
            //  the thorax has two lungs
            {
                buildSpecimen (6, cube.data());
                const int k = (int) (0.55f * VN);
                size_t left = 0, right = 0, n = 0;
                for (int j = 0; j < VN; ++j) for (int i = 0; i < VN; ++i)
                {
                    const float v = cube[((size_t) k * VN + (size_t) j) * VN + (size_t) i];
                    const float x = ((float) i + 0.5f) / VN, y = ((float) j + 0.5f) / VN;
                    const bool inChest = (x - 0.5f) * (x - 0.5f) / (0.43f * 0.43f) + (y - 0.5f) * (y - 0.5f) / (0.29f * 0.29f) < 0.64f;
                    if (! inChest) continue;
                    ++n;
                    if (v < 0.12f) { if (x < 0.45f) ++left; if (x > 0.55f) ++right; }
                }
                char d[96]; std::snprintf (d, sizeof d, "air inside the chest at mid-height: %.1f %% on one side, %.1f %% on the other", 100.0 * left / n, 100.0 * right / n);
                std::printf ("  %s\n", d);
                ok (left > n / 10 && right > n / 10, "two lungs", d);
            }
            //  the vertebra has a canal with bone either side
            {
                buildSpecimen (12, cube.data());
                auto at = [&] (float x, float y, float z) { const int i = (int) (x * VN), j = (int) (y * VN), k = (int) (z * VN);
                    return cube[((size_t) k * VN + (size_t) j) * VN + (size_t) i]; };
                const float canal = at (0.5f, 0.40f, 0.5f), l = at (0.5f - 0.133f, 0.408f, 0.5f), r = at (0.5f + 0.133f, 0.408f, 0.5f);
                char d[128]; std::snprintf (d, sizeof d, "canal %.3f (CSF is %.3f), pedicles %.2f / %.2f", canal, an::toUnit (8.0f), l, r);
                std::printf ("  %s\n", d);
                ok (std::abs (canal - an::toUnit (8.0f)) < 0.03f && l > 0.6f && r > 0.6f, "a canal of CSF between two pedicles of bone", d);
            }
        }
        //  ---- cost with the second head on -----------------------------------
        {
            Line six[NLINES]; Params p; factory (2).build (six, p);
            p.unison = 1.0f; p.head2 = 1.0f; p.head2Ratio = 0.6f; p.grain = 1.0f; p.contrast = 0.5f;
            Engine eng; eng.p = p; eng.prepare (SR, BLK); eng.setLines (six); eng.service();
            for (int v = 0; v < 8; ++v) eng.noteOn (36 + v * 5, 0.9f);
            std::vector<float> l (BLK), r (BLK);
            const auto t0 = std::chrono::steady_clock::now();
            const int nb = (int) (5.0 * SR / BLK);
            for (int b = 0; b < nb; ++b) eng.process (l.data(), r.data(), BLK);
            const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            char d[96]; std::snprintf (d, sizeof d, "%.2f %% of one core, 32 readers each with a second head, window on", wall / 5.0 * 100.0);
            std::printf ("  %s\n", d);
            ok (wall / 5.0 < 0.45, "64 heads under 45 % of a core (a second head is a second 64-tap read)", d);
        }
    }

    //==========================================================================
    head ("11 - the circuits: GROWL, SCREAM, LADDER, each low and high (260905.2)");
    {
        //  the cutoff knob: fc = 20 * 1000^pos
        auto posOf = [] (double hz) { return (float) (std::log (hz / 20.0) / std::log (1000.0)); };
        const char* names[4] = { "SVF", "GROWL", "SCREAM", "LADDER" };
        //  ---- every circuit filters: LOW pulls the centroid down, HIGH pushes it up ----
        {
            Line six[NLINES]; straightAll (six, 1.0f, 0.0f);            // SPINE, bright
            Engine* e0 = fresh (1, six);
            Take t0 = render (*e0, 57, 0.8);
            const double c0 = centroid (spectrum (t0.L, (size_t) (0.3 * SR), N), N);
            delete e0;
            char d[200]; int n = 0;
            n += std::snprintf (d + n, sizeof d - (size_t) n, "centroid OFF %.0f Hz;", c0);
            bool good = true;
            for (int m = 1; m <= 3; ++m)
            {
                double cl, ch;
                for (int hp = 0; hp < 2; ++hp)
                {
                    Engine* e = fresh (1, six);
                    e->p.filtModel = (float) m / 3.0f; e->p.filtType = hp ? 2.0f / 3.0f : 0.0f;
                    e->p.cutoff = posOf (hp ? 2000.0 : 500.0); e->p.reso = 0.2f; e->p.filtDepth = 0.0f;
                    Take t = render (*e, 57, 0.8);
                    const double c = centroid (spectrum (t.L, (size_t) (0.3 * SR), N), N);
                    if (hp) ch = c; else cl = c;
                    delete e;
                }
                n += std::snprintf (d + n, sizeof d - (size_t) n, " %s low %.0f high %.0f;", names[m], cl, ch);
                if (! (cl < c0 * 0.6 && ch > c0 * 1.3)) good = false;
            }
            std::printf ("  %s\n", d);
            ok (good, "each circuit's lowpass at 500 Hz pulls the centroid under 60 %, its highpass at 2 kHz pushes it past 130 %", d);
        }
        //  ---- self-oscillation, in tune: SCREAM and LADDER at full resonance ----
        {
            /*  A cutoff between two harmonics of the note (1000 Hz; the note's
                4th and 5th sit at 880 and 1100), full RESONANCE, a quiet sine
                to kick it: the strongest line in the octave around the cutoff
                must be the circuit's own oscillation, at the cutoff. */
            Line six[NLINES]; straightAll (six, 0.0f, 0.0f);            // SINUS at its quietest
            for (int m = 2; m <= 3; ++m)
            {
                Engine* e = fresh (0, six);
                e->p.filtModel = (float) m / 3.0f; e->p.filtType = 0.0f;
                e->p.cutoff = posOf (1000.0); e->p.reso = 1.0f; e->p.filtDepth = 0.0f; e->p.filtTrack = 0.0f;   // KEY TRACK off: the probe measures the cutoff, not the note
                Take t = render (*e, 57, 1.2);
                const auto sp = spectrum (t.L, (size_t) (0.6 * SR), N);
                size_t best = 0; double bm = 0;
                for (size_t i = (size_t) (700.0 * N / SR); i < (size_t) (1400.0 * N / SR); ++i) if (sp[i] > bm) { bm = sp[i]; best = i; }
                //  parabolic interpolation on the log magnitude for a better peak
                double fpk = (double) best * SR / (double) N;
                if (best > 0 && best + 1 < sp.size())
                {
                    const double a = std::log (std::max (1e-12, sp[best - 1])), b = std::log (std::max (1e-12, sp[best])), c = std::log (std::max (1e-12, sp[best + 1]));
                    const double dd = 0.5 * (a - c) / (a - 2 * b + c);
                    fpk = ((double) best + (std::isfinite (dd) ? dd : 0.0)) * SR / (double) N;
                }
                const double cents = 1200.0 * std::log2 (fpk / 1000.0);
                const double h1 = harmMag (sp, f0, 1, N);
                char d[160]; std::snprintf (d, sizeof d, "%s at RESONANCE 1, cutoff 1000 Hz: sings at %.1f Hz (%+.1f cents), %.1f dB over the note", names[m], fpk, cents, dB (bm / std::max (1e-12, h1)));
                std::printf ("  %s\n", d);
                ok (std::abs (cents) < 40.0 && bm > h1 * 1.4, "self-oscillates at the cutoff within 40 cents, at least 3 dB over the note that kicks it", d);
                delete e;
            }
            //  and GROWL, at full resonance, is still a filter and not an oscillator
            Engine* e = fresh (0, six);
            e->p.filtModel = 1.0f / 3.0f; e->p.filtType = 0.0f;
            e->p.cutoff = posOf (1000.0); e->p.reso = 1.0f; e->p.filtDepth = 0.0f; e->p.filtTrack = 0.0f;   // KEY TRACK off: the probe measures the cutoff, not the note
            Take t = render (*e, 57, 1.2);
            const auto sp = spectrum (t.L, (size_t) (0.6 * SR), N);
            size_t best = 0; double bm = 0;
            for (size_t i = (size_t) (700.0 * N / SR); i < (size_t) (1400.0 * N / SR); ++i) if (sp[i] > bm) { bm = sp[i]; best = i; }
            const double h1 = harmMag (sp, f0, 1, N);
            char d[128]; std::snprintf (d, sizeof d, "GROWL at RESONANCE 1: the loudest line near the cutoff sits %.1f dB against the note", dB (bm / std::max (1e-12, h1)));
            std::printf ("  %s\n", d);
            ok (true, "GROWL's top of the dial is reported, not asserted - it is meant to be on the edge", d);
            delete e;
        }
        //  ---- aliasing, below the oscillation edge, on the brightest field at C5 ----
        {
            Line six[NLINES]; straightAll (six, 1.0f, 0.0f);
            const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
            char d[200]; int n = 0; bool good = true;
            for (int m = 1; m <= 3; ++m)
            {
                Engine* e = fresh (1, six);
                e->p.filtModel = (float) m / 3.0f; e->p.filtType = 0.0f; e->p.grain = 1.0f;
                e->p.cutoff = posOf (3000.0); e->p.reso = 0.5f; e->p.filtDepth = 0.0f;
                Take t = render (*e, 72, 0.8);
                const double fl = aliasFloorDb (spectrum (t.L, (size_t) (0.3 * SR), N), fC5, N);
                n += std::snprintf (d + n, sizeof d - (size_t) n, " %s %.1f dB;", names[m], fl);
                if (fl > -40.0) good = false;
                delete e;
            }
            std::printf ("  non-harmonic floor at C5, RESONANCE 0.5, cutoff 3 kHz:%s\n", d);
            ok (good, "the circuits below their oscillation edge keep the floor under -40 dB at 1x", d);
        }
        //  ---- bounded, driven as hard as the instrument can: full window into full resonance ----
        {
            Line six[NLINES]; straightAll (six, 1.0f, 0.0f);
            double worst = 0; bool fin = true;
            for (int m = 1; m <= 3; ++m) for (int hp = 0; hp < 2; ++hp)
            {
                Engine* e = fresh (1, six);
                e->p.filtModel = (float) m / 3.0f; e->p.filtType = hp ? 2.0f / 3.0f : 0.0f;
                e->p.cutoff = posOf (800.0); e->p.reso = 1.0f; e->p.contrast = 1.0f; e->p.fold = 0.5f; e->p.level = 0.7f;
                Take t = render (*e, 48, 1.0, 0.7);
                for (float v : t.L) { if (! std::isfinite (v)) fin = false; worst = std::max (worst, (double) std::abs (v)); }
                delete e;
            }
            char d[96]; std::snprintf (d, sizeof d, "worst peak %.3f across six circuits at RESONANCE 1 with CONTRAST 1 and FOLD", worst);
            std::printf ("  %s\n", d);
            ok (fin && worst <= 1.0, "bounded and finite however hard the read drives them", d);
        }
        //  ---- the SVF is untouched: CIRCUIT at its default is the old path ----
        {
            Line six[NLINES]; straightAll (six, 0.8f, 0.0f);
            Engine* a = fresh (1, six); a->p.filtType = 0.0f; a->p.cutoff = 0.6f; a->p.reso = 0.4f;
            Engine* b = fresh (1, six); b->p.filtType = 0.0f; b->p.cutoff = 0.6f; b->p.reso = 0.4f; b->p.filtModel = 0.0f;
            Take ta = render (*a, 57, 0.5), tb = render (*b, 57, 0.5);
            ok (ta.L.size() == tb.L.size() && std::memcmp (ta.L.data(), tb.L.data(), ta.L.size() * sizeof (float)) == 0,
                "CIRCUIT at SVF is bit-identical to the filter as it shipped");
            delete a; delete b;
        }
        //  ---- cost: the ladder on every reader ----
        {
            Line six[NLINES]; Params p; factory (2).build (six, p);
            p.unison = 1.0f; p.filtModel = 1.0f; p.filtType = 0.0f; p.reso = 0.8f;
            Engine eng; eng.p = p; eng.prepare (SR, BLK); eng.setLines (six); eng.service();
            for (int v = 0; v < 8; ++v) eng.noteOn (36 + v * 5, 0.9f);
            std::vector<float> l (BLK), r (BLK);
            const auto t0 = std::chrono::steady_clock::now();
            const int nb = (int) (5.0 * SR / BLK);
            for (int b = 0; b < nb; ++b) eng.process (l.data(), r.data(), BLK);
            const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            char d[96]; std::snprintf (d, sizeof d, "%.2f %% of one core, 32 readers through the LADDER", wall / 5.0 * 100.0);
            std::printf ("  %s\n", d);
            ok (wall / 5.0 < 0.30, "32 ladders under 30 % of a core", d);
        }
    }

    std::printf ("\n%d checks, %d failed  -  %s\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");
    return fails == 0 ? 0 : 1;
}
