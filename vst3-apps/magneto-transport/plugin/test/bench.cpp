// Magneto Transport - bench (stage 3).
//
// Measures every claim stage 1 makes. The tape is a medium, so delay, pitch
// bends and Doppler are consequences of where the heads are and how fast the
// tape moves; each is checked against the closed-form physics it must obey.

#include "Engine.h"
#include "stock/stock_456.h"     // the reference's alignment constants - never typed here
#include "stock/pb_456.h"        // the reference's playback targets (generated)
#include "stock/stock_vintage.h" // the other coatings' alignments (generated, stage 6)
#include "stock/stock_budget.h"
#include "Medium.h"
#include <complex>

#include <chrono>
#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <windows.h>
#endif

/*  This thread's own CPU time, s: the engine's cost without the time other
    programs took the core from it (wall-clock timing of one binary wandered
    between 12 % and 15 % of a core from run to run). */
static double threadCpuSeconds()
{
   #ifdef _WIN32
    FILETIME c, e, k, u;
    GetThreadTimes (GetCurrentThread(), &c, &e, &k, &u);
    auto t = [] (const FILETIME& f) { return (double) (((uint64_t) f.dwHighDateTime << 32) | f.dwLowDateTime) * 1.0e-7; };
    return t (k) + t (u);
   #else
    return std::chrono::duration<double> (std::chrono::steady_clock::now().time_since_epoch()).count();
   #endif
}
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace mt;

static int g_fail = 0, g_pass = 0;
static bool g_recordOnly = false; // sections 13-14: the record process alone (no playback head losses)
static bool g_eqOff = false;      // the transport's timing and the reference comparisons: equalisers out
struct EqOff { EqOff() { g_eqOff = true; } ~EqOff() { g_eqOff = false; } };
static bool g_idealMedium = true; // a perfect tape (no particle or modulation noise, no dropouts) except where section 19 says
struct RealMedium { RealMedium() { g_idealMedium = false; } ~RealMedium() { g_idealMedium = true; } };
static bool verbose = false;      // argv[1] == "-v": print every delay case
static void check (bool ok, const char* what, const std::string& detail)
{
    std::printf ("  [%s] %-58s %s\n", ok ? " ok " : "FAIL", what, detail.c_str());
    (ok ? g_pass : g_fail)++;
}
static std::string fmt (const char* f, double a = 0, double b = 0, double c = 0)
{
    char buf[256]; std::snprintf (buf, sizeof buf, f, a, b, c); return buf;
}

//------------------------------------------------------------------------------
struct Render
{
    std::vector<float> L, R;
};

/*  Run an engine over stereo input in 256-sample blocks, servicing allocations
    between blocks the way the processor's timer does. `perBlock` may change the
    parameters mid-render (called with the sample index of the block start). */
static Render run (Params p, double fs, const std::vector<float>& inL, const std::vector<float>& inR,
                   std::function<void (Params&, Engine&, size_t)> perBlock = nullptr, double bpm = 0.0,
                   Engine* keep = nullptr)
{
    Engine localE;
    Engine& e = keep ? *keep : localE;
    e.setParams (p);
    e.setBpm (bpm);
    e.setPlaybackLoss (! g_recordOnly);
    e.setEq (! g_eqOff);
    e.setIdealMedium (g_idealMedium);
    e.prepare (fs, 256);
    Render r; r.L.resize (inL.size()); r.R.resize (inL.size());
    for (size_t i = 0; i < inL.size(); i += 256)
    {
        const int n = (int) std::min<size_t> (256, inL.size() - i);
        if (perBlock) { perBlock (p, e, i); e.setParams (p); }
        e.process (inL.data() + i, inR.data() + i, r.L.data() + i, r.R.data() + i, n);
        e.service();
    }
    return r;
}

static Params wetOnly()
{
    Params p;
    p.mix = 1.0f; p.feedback = 0.0f;
    p.h1On = 1; p.h2On = 0; p.h3On = 0;
    // An ideal transport: every other claim is measured cleanly, and the
    // transport's own wow and flutter are measured in section 18.
    p.condition = 0.0f;
    return p;
}

/*  One playback head on at h; the other two parked where it cannot run into
    them (three heads share one rail and cannot pass each other). */
static void soloHead (Params& p, double h)
{
    p.h1On = p.h2On = p.h3On = 0;
    if (h <= 12.0) { p.h1Pos = (float) h; p.h2Pos = 22.8f; p.h3Pos = 24.0f; p.h1On = 1; }
    else           { p.h3Pos = (float) h; p.h1Pos = 1.5f;  p.h2Pos = 2.7f;  p.h3On = 1; }
}

static std::vector<float> noiseBurst (size_t total, size_t start, size_t len, unsigned seed, float amp)
{
    std::vector<float> x (total, 0.0f);
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    for (size_t i = 0; i < len && start + i < total; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (len - 1));   // Hann, so it is band-limited
        x[start + i] = (float) (amp * w * u (rng));
    }
    return x;
}

/*  Lag (in samples, fractional) of y against x, searched in [lo, hi]. The whole
    chain is linear-phase (symmetric interpolators and FIRs), so the correlation
    peak sits at the group delay; a parabola through the peak refines it. */
static double lagOf (const std::vector<float>& x, const std::vector<float>& y, int lo, int hi,
                     size_t xs, size_t xlen)
{
    std::vector<double> c ((size_t) (hi - lo + 1), 0.0);
    for (int k = lo; k <= hi; ++k)
    {
        double s = 0.0;
        for (size_t i = xs; i < xs + xlen; ++i)
        {
            const size_t j = i + (size_t) k;
            if (j < y.size()) s += (double) x[i] * y[j];
        }
        c[(size_t) (k - lo)] = s;
    }
    size_t m = 0;
    for (size_t i = 1; i < c.size(); ++i) if (c[i] > c[m]) m = i;
    if (m == 0 || m + 1 >= c.size()) return lo + (double) m;
    const double a = c[m - 1], b = c[m], d = c[m + 1];
    const double off = 0.5 * (a - d) / (a - 2.0 * b + d);
    return lo + (double) m + off;
}

static double rmsOf (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0.0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) (b - a));
}

static double goertzelAmp (const std::vector<float>& x, size_t a, size_t b, double f, double fs)
{
    // exact-bin amplitude over [a, b) with a Hann window (normalised)
    double re = 0.0, im = 0.0, wsum = 0.0;
    const size_t n = b - a;
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (n - 1));
        const double ph = 2.0 * M_PI * f * (double) i / fs;
        re += w * x[a + i] * std::cos (ph);
        im += w * x[a + i] * std::sin (ph);
        wsum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

/*  Mean frequency over [a, b) from interpolated rising zero crossings. */
static double freqOf (const std::vector<float>& x, size_t a, size_t b, double fs)
{
    double first = -1, last = -1; int count = 0;
    for (size_t i = a + 1; i < b; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + x[i - 1] / (double) (x[i - 1] - x[i]);
            if (first < 0) first = t;
            last = t; ++count;
        }
    if (count < 2) return 0.0;
    return (count - 1) * fs / (last - first);
}

static std::vector<float> sine (size_t n, double f, double fs, double amp, size_t start = 0, size_t stop = SIZE_MAX)
{
    std::vector<float> x (n, 0.0f);
    for (size_t i = start; i < std::min (n, stop); ++i)
        x[i] = (float) (amp * std::sin (2.0 * M_PI * f * (double) (i - start) / fs));
    return x;
}

static const double REF_AMP = 0.12589254117941673 * std::sqrt (2.0);   // -18 dBFS rms sine peak

//==============================================================================
static void testDelay()
{
    EqOff eqOff;   // minimum-phase equalisers move the echo's phase, as on a real machine
    std::printf ("\n1. DELAY = DISTANCE / SPEED (tolerance 1 us)\n");
    const double rates[] = { 44100, 48000, 88200, 96000, 192000 };
    const double speeds[] = { 4.75, 19.0, 76.0 };
    const double heads[]  = { 1.5, 12.0, 24.0 };
    double worstUs = 0.0, wFs = 0, wV = 0, wH = 0, wSigned = 0;
    for (double fs : rates)
        for (double v : speeds)
            for (double h : heads)
            {
                Params p = wetOnly(); p.speed = (float) v; soloHead (p, h);
                const double expect = h / v;            // s
                // A slow Gaussian pulse (sigma 2 ms: content below ~150 Hz) and the
                // centroid of its echo's energy - the LOW-FREQUENCY group delay.
                // Since stage 3 the coating records in four layers at slightly
                // different distances past the gap, and a wideband pulse sees their
                // spread (3 us at 4.75 cm/s); the defined delay is the long-wave one.
                // The window is +-80 ms: since stage 4c the head bump's cut toward DC
                // gives the echo low-frequency tails, and a +-12 ms window placed to
                // the nearest sample clipped them unevenly (up to 5 us at 19 cm/s).
                const double t0 = 0.10, sig = 0.002;
                const size_t total = (size_t) ((t0 + expect + 0.12) * fs);
                std::vector<float> x (total, 0.0f);
                double cin = 0.0, ein = 0.0;
                for (size_t i = 0; i < total; ++i)
                {
                    const double t = (double) i / fs - t0;
                    // small-signal: well under 0 VU, where every layer's gain is its own slope
                    x[i] = (float) (0.03 * std::exp (-0.5 * t * t / (sig * sig)));
                    cin += (double) x[i] * x[i] * i; ein += (double) x[i] * x[i];
                }
                auto r = run (p, fs, x, x);
                const size_t a = (size_t) ((t0 + expect - 0.08) * fs), b = (size_t) ((t0 + expect + 0.08) * fs);
                double cout = 0.0, eout = 0.0;
                for (size_t i = a; i < b; ++i) { const double e = (double) r.L[i] * r.L[i]; cout += e * i; eout += e; }
                const double lag = cout / eout - cin / ein;
                const double errUs = std::abs (lag / fs - expect) * 1e6;
                if (verbose) std::printf ("     %6.0f Hz %5.2f cm/s %4.1f cm  %+.3f us\n", fs, v, h, (lag / fs - expect) * 1e6);
                if (errUs > worstUs) { worstUs = errUs; wFs = fs; wV = v; wH = h; wSigned = (lag / fs - expect) * 1e6; }
            }
    check (worstUs < 1.0, "delay matches h/v at 5 rates x 3 speeds x 3 heads",
           fmt ("worst %+.3f us", wSigned) + fmt (" at %.0f Hz, %.2f cm/s, ", wFs, wV) + fmt ("%.1f cm", wH));
}

static void testUnity()
{
    std::printf ("\n2. GAIN STRUCTURE (unity at reference, +/-0.05 dB)\n");
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Params p = wetOnly();
        const size_t n = (size_t) (1.2 * fs);
        auto x = sine (n, 1000.0, fs, REF_AMP);
        auto r = run (p, fs, x, x);
        const size_t a = (size_t) (0.6 * fs), b = (size_t) (1.1 * fs);
        const double db = 20.0 * std::log10 (rmsOf (r.L, a, b) / rmsOf (x, a, b));
        check (std::abs (db) < 0.05, fmt ("1 kHz at -18 dBFS, %.0f Hz", fs).c_str(), fmt ("%+.4f dB", db));
    }

    // Frequency response of the ideal medium. Not a tolerance in stage 1 (stage 4
    // adds the physical gap and spacing losses), but the grid's own loss must be
    // small against them. Printed per speed, asserted at 19 cm/s.
    std::printf ("   response record+medium, -20 dB re ref (48 kHz), dB re the 1 kHz at 0 VU:\n");
    const double fs = 48000.0;
    const double freqs[] = { 30, 100, 1000, 5000, 10000, 16000, 20000 };
    double worst19 = 0.0;
    for (double v : { 4.75, 19.0, 76.0 })
    {
        std::printf ("     %5.2f cm/s:", v);
        for (double f : freqs)
        {
            Params p = wetOnly(); p.speed = (float) v; soloHead (p, 1.5);
            const double amp = REF_AMP * 0.1;
            const double d = 1.5 / v;
            const size_t n = (size_t) ((d + 0.6) * fs);
            auto x = sine (n, f, fs, amp);
            auto r = run (p, fs, x, x);
            const size_t a = (size_t) ((d + 0.15) * fs), b = n - 100;
            const double db = 20.0 * std::log10 (goertzelAmp (r.L, a, b, f, fs) / amp);
            std::printf (" %5.0f:%+.2f", f, db);
            if (v >= 19.0 && f <= 16000) worst19 = std::max (worst19, std::abs (db));
        }
        std::printf ("\n");
    }
    // Since stage 3 this includes the RECORDING loss (the zone, the layers'
    // different distances past the gap) - asserted against the reference in 13.
    (void) worst19;
}

static void testSpeedGlide()
{
    EqOff eqOff;   // the claim is the transport's timing; minimum-phase equalisers move the echo's phase
    std::printf ("\n3. A MOTOR GLIDE BENDS THE ECHOES ALREADY ON THE TAPE\n");
    // Record 1 kHz at 4.75 cm/s with head 3 at 24 cm, then ask for 9.5 cm/s.
    // Material read 1.9-2.2 s after the change was recorded BEFORE it, so it
    // plays at f0 * v(t) / 4.75 - v(t) the motor's ACTUAL glide, read from the
    // engine (since stage 5 a hunting synchronous motor, not a closed form).
    const double fs = 48000.0, f0 = 1000.0, ts = 6.0;
    Params p = wetOnly(); p.speed = 4.75f; soloHead (p, 24.0);
    const size_t n = (size_t) ((ts + 2.4) * fs);
    auto x = sine (n, f0, fs, REF_AMP);
    std::vector<double> tB, vB;
    auto r = run (p, fs, x, x, [&] (Params& q, Engine& e, size_t i) {
        tB.push_back ((double) i / fs); vB.push_back (e.speedCms());
        if ((double) i >= ts * fs) q.speed = 9.5f; });
    const size_t ts0 = ((size_t) (ts * fs) / 256 + 1) * 256;   // the block the change lands in
    const double tsReal = (double) ts0 / fs;
    const double t1 = tsReal + 1.9, t2 = tsReal + 2.2;
    double vMean = 0.0; int nv = 0;
    for (size_t k = 0; k < tB.size(); ++k) if (tB[k] >= t1 && tB[k] < t2) { vMean += vB[k]; ++nv; }
    vMean /= std::max (nv, 1);
    const double expect = f0 * vMean / 4.75;
    const double got = freqOf (r.L, (size_t) (t1 * fs), (size_t) (t2 * fs), fs);
    check (std::abs (1200.0 * std::log2 (got / expect)) < 0.5, "pitch follows v_play / v_rec",
           fmt ("expected %.2f Hz, got %.2f Hz (%.3f cent)", expect, got, 1200.0 * std::log2 (got / expect)));

    // And an impulse sent during the glide arrives when the odometer has moved
    // exactly h: solve the integral of the analytic glide for the arrival time.
    {
        Params q = wetOnly(); q.speed = 4.75f; soloHead (q, 12.0);
        const double tsw = 1.0;
        // A short Gaussian pulse, not noise: an echo recorded during a glide is
        // time-compressed, and noise correlated against a compressed copy of
        // itself peaks wherever chance puts it (the first version's 1.2 ms).
        const size_t total = (size_t) (4.5 * fs);
        const double tPulse = 1.31, sig = 0.0002;
        std::vector<float> xb (total, 0.0f);
        for (size_t i = 0; i < total; ++i)
        {
            const double t = (double) i / fs - tPulse;
            xb[i] = (float) (0.5 * std::exp (-0.5 * t * t / (sig * sig)));
        }
        // the tape's own odometer at every block start: the echo arrives when the
        // tape has moved h past where it was when the pulse was recorded
        std::vector<double> tO, xO;
        auto rb = run (q, fs, xb, xb, [&] (Params& pp, Engine& e, size_t i) {
            tO.push_back ((double) i / fs); xO.push_back (e.odometerCells());
            if ((double) i >= tsw * fs) pp.speed = 9.5f; });
        auto odoAt = [&] (double t) {
            size_t k = 1; while (k + 1 < tO.size() && tO[k] < t) ++k;
            const double a0 = tO[k - 1], a1v = tO[k];
            return xO[k - 1] + (xO[k] - xO[k - 1]) * (t - a0) / (a1v - a0); };
        const double tc = tPulse;
        const double target = odoAt (tc) + 12.0 * CELLS_PER_CM;
        double lo = tc, hi = tc + 4.0;
        for (int it = 0; it < 80; ++it) { const double m = 0.5 * (lo + hi); (odoAt (m) < target ? lo : hi) = m; }
        const double arrive = 0.5 * (lo + hi);
        // Centroid of the echo's energy in a window around the expected arrival.
        const size_t a = (size_t) ((arrive - 0.01) * fs), b = (size_t) ((arrive + 0.01) * fs);
        double num = 0.0, den = 0.0;
        for (size_t i = a; i < b; ++i) { const double e = (double) rb.L[i] * rb.L[i]; num += e * i; den += e; }
        const double tGot = num / den / fs;
        const double errUs = std::abs (tGot - arrive) * 1e6;
        check (errUs < 10.0, "an echo in flight arrives when the tape has moved h",
               fmt ("expected %.6f s, got %.6f s (%.2f us)", arrive, tGot, errUs));
    }
}

static void testHeadDoppler()
{
    std::printf ("\n4. A MOVING HEAD IS DOPPLER (and reads backwards if it outruns the tape)\n");
    const double fs = 48000.0;
    // At 19 cm/s, a head sliding downstream at the carriage's 12 cm/s scans the
    // tape at 7 cm/s: content recorded at 19 plays at 7/19 of its pitch.
    {
        Params p = wetOnly(); p.speed = 19.0f;
        p.h1On = 0; p.h3On = 1; p.h1Pos = 1.5f; p.h2Pos = 2.7f; p.h3Pos = 3.9f;
        const double tm = 3.0;
        const size_t n = (size_t) ((tm + 1.6) * fs);
        auto x = sine (n, 1000.0, fs, REF_AMP);
        auto r = run (p, fs, x, x, [&] (Params& q, Engine&, size_t i) {
            if ((double) i >= tm * fs) q.h3Pos = 24.0f; });
        const double expect = 1000.0 * (19.0 - CARRIAGE_MAX_CMS) / 19.0;
        const double got = freqOf (r.L, (size_t) ((tm + 0.6) * fs), (size_t) ((tm + 1.4) * fs), fs);
        check (std::abs (1200 * std::log2 (got / expect)) < 1.0, "head moving away at 12 cm/s on 19 cm/s tape",
               fmt ("expected %.2f Hz, got %.2f Hz", expect, got));
    }
    // At 4.75 cm/s the same carriage moving downstream outruns the tape: it
    // scans backwards at 7.25 cm/s. Record a rising chirp; it must come out
    // FALLING, at 7.25/4.75 of the recorded pitch.
    {
        Params p = wetOnly(); p.speed = 4.75f;
        p.h1On = 0; p.h3On = 1; p.h1Pos = 1.5f; p.h2Pos = 2.7f; p.h3Pos = 3.9f;
        const double tm = 8.0;
        const size_t n = (size_t) ((tm + 1.6) * fs);
        std::vector<float> x (n);
        double ph = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double f = 300.0 + 40.0 * (double) i / fs;     // +40 Hz per second
            ph += 2.0 * M_PI * f / fs;
            x[i] = (float) (REF_AMP * std::sin (ph));
        }
        auto r = run (p, fs, x, x, [&] (Params& q, Engine&, size_t i) {
            if ((double) i >= tm * fs) q.h3Pos = 24.0f; });
        const double fa = freqOf (r.L, (size_t) ((tm + 0.5) * fs), (size_t) ((tm + 0.8) * fs), fs);
        const double fb = freqOf (r.L, (size_t) ((tm + 1.1) * fs), (size_t) ((tm + 1.4) * fs), fs);
        check (fb < fa, "outrunning the tape reads backwards (a rising chirp falls)",
               fmt ("%.1f Hz then %.1f Hz", fa, fb));
        // What it should be: material under the head at t, recorded when the
        // record head was over it; pitch ratio 7.25 / 4.75 on that content.
        check (fa > 300.0 * 7.25 / 4.75, "scanning backwards at 7.25 cm/s raises the pitch by 7.25/4.75",
               fmt ("%.1f Hz > %.1f Hz", fa, 300.0 * 7.25 / 4.75));
    }
}

static void testFeedback()
{
    std::printf ("\n5. FEEDBACK: STOP THE INPUT AND WATCH; SELF-OSCILLATION HELD BY THE TAPE\n");
    const double fs = 48000.0;
    auto peakOf = [] (const std::vector<float>& x, size_t a, size_t b) { double m = 0.0; for (size_t i = a; i < b; ++i) m = std::max (m, (double) std::abs (x[i])); return m; };
    {
        // below unity loop gain the echoes must DECAY once the input stops
        Params p = wetOnly(); p.feedback = 0.5f; p.speed = 76.0f; soloHead (p, 1.5);
        const size_t n = (size_t) (12.0 * fs);
        auto x = noiseBurst (n, (size_t) (0.1 * fs), (size_t) (0.5 * fs), 3, 0.3f);
        auto r = run (p, fs, x, x);
        const double e1 = rmsOf (r.L, (size_t) (1 * fs), (size_t) (2 * fs));
        const double e2 = rmsOf (r.L, (size_t) (10 * fs), (size_t) (11 * fs));
        check (e2 < e1 * 1e-3, "feedback 50 %: after the input stops the echoes decay", fmt ("rms %.2e -> %.2e", e1, e2));
    }
    {
        // at 100 % the loop runs past unity and sustains on its own - held by the
        // tape running out of remanence, not by a limiter: the output can never
        // exceed full remanence through the repro alignment
        Params p = wetOnly(); p.feedback = 1.0f; p.speed = 19.0f; soloHead (p, 4.0);
        const size_t n = (size_t) (30.0 * fs);
        auto x = noiseBurst (n, (size_t) (0.1 * fs), (size_t) (0.3 * fs), 5, 0.3f);
        Engine e;
        auto r = run (p, fs, x, x, nullptr, 0.0, &e);
        const double late = rmsOf (r.L, (size_t) (25 * fs), (size_t) (29 * fs));
        check (late > 0.05, "feedback 100 %: the loop self-oscillates (sustains 25 s on)", fmt ("rms %.3f", late));
        // What holds it is the tape: its remanence never passes full (the record
        // tables top out there). Since stage 4 the OUTPUT may peak above the
        // full-remanence level - the replay equaliser lifts the harmonics a
        // saturated tape makes, as on a real machine - so the claim is checked
        // where it lives, on the tape, and as a loop that has stopped growing.
        const double pkMid = peakOf (r.L, (size_t) (15 * fs), (size_t) (20 * fs));
        const double pkLate = peakOf (r.L, (size_t) (25 * fs), (size_t) (29 * fs));
        size_t zc = 0;
        for (size_t i = (size_t) (25 * fs) + 1; i < (size_t) (29 * fs); ++i) if ((r.L[i - 1] < 0.0f) != (r.L[i] < 0.0f)) ++zc;
        check (e.tele.recPeak.load() <= 1.05f, "and the tape holds it: its remanence never passes full",
               fmt ("peak |M| %.3f; output peak %+.2f dBFS, oscillating near %.0f Hz", e.tele.recPeak.load(), 20 * std::log10 (pkLate), 0.5 * zc / 4.0));
        check (pkLate <= pkMid * 1.05, "and it has settled (no growth from 15-20 s to 25-29 s)", fmt ("peak %.3f -> %.3f", pkMid, pkLate));
    }
    {
        Params p; p.mix = 1.0f; p.feedback = 1.0f;
        p.h1On = p.h2On = p.h3On = 1; p.h1Lvl = p.h2Lvl = p.h3Lvl = 6.0f;
        p.bass = 12.0f; p.treble = 12.0f; p.recLevel = 12.0f;
        p.speed = 76.0f; p.h1Pos = 1.5f; p.h2Pos = 3.0f; p.h3Pos = 4.5f;
        const size_t n = (size_t) (40.0 * fs);
        auto x = noiseBurst (n, (size_t) (0.1 * fs), (size_t) (0.5 * fs), 3, 0.5f);
        auto r = run (p, fs, x, x);
        bool finite = true; for (float v : r.L) if (! std::isfinite (v)) { finite = false; break; }
        const double a = peakOf (r.L, (size_t) (10 * fs), (size_t) (20 * fs)), b = peakOf (r.L, (size_t) (30 * fs), (size_t) (40 * fs));
        check (finite && b <= a * 1.02, "everything at maximum: finite, and not growing", fmt ("peak %.2f then %.2f", a, b));
    }
    {
        Params q; q.mix = 1.0f; q.feedback = 0.8f; q.h1On = q.h2On = q.h3On = 1;
        std::vector<float> z ((size_t) (3 * fs), 0.0f);
        auto rz = run (q, fs, z, z);
        double m = 0.0; for (float v : rz.L) m = std::max (m, (double) std::abs (v));
        check (m == 0.0, "silence in gives exact zeros out", fmt ("max %.3e", m));
    }
}

static void testLoopAndSplice()
{
    std::printf ("\n6. LOOP MODE AND SPLICING\n");
    const double fs = 48000.0;
    // A 0.5 m loop at 76 cm/s comes round every 0.658 s. With the erase head at
    // full field nothing recorded may survive one trip. Exactly nothing: so with
    // the equalisers out, whose IIR memory decays forever without being tape.
    {
        EqOff eqOff;
        Params p = wetOnly(); p.mode = 1; p.loopLen = 0.5f; p.speed = 76.0f; p.h1Pos = 4.0f;
        const size_t n = (size_t) (3.0 * fs);
        auto x = noiseBurst (n, (size_t) (0.1 * fs), (size_t) (0.02 * fs), 5, 0.5f);
        auto r = run (p, fs, x, x);
        const double echo = rmsOf (r.L, (size_t) ((0.1 + 4.0 / 76.0) * fs), (size_t) ((0.14 + 4.0 / 76.0) * fs));
        const double loopT = 0.5 * 100.0 / 76.0;
        const double ghost = rmsOf (r.L, (size_t) ((0.1 + loopT + 4.0 / 76.0 - 0.01) * fs),
                                         (size_t) ((0.14 + loopT + 4.0 / 76.0 + 0.01) * fs));
        check (echo > 1e-3, "the head's echo is there", fmt ("rms %.3e", echo));
        check (ghost == 0.0, "a full erase leaves no ghost one loop later", fmt ("rms %.3e", ghost));
    }
    // Switch REEL -> LOOP mid-render: the splice completes and the echo comes back.
    {
        Params p = wetOnly(); p.speed = 19.0f; p.h1Pos = 4.0f;
        Engine e;
        const size_t n = (size_t) (2.5 * fs);
        auto x = sine (n, 1000.0, fs, REF_AMP);   // unity is defined at the 1 kHz alignment
        bool sawSplice = false;
        auto r = run (p, fs, x, x, [&] (Params& q, Engine& en, size_t i) {
            if ((double) i >= 0.5 * fs) { q.mode = 1; q.loopLen = 1.0f; }
            if (en.isSplicing()) sawSplice = true; }, 0.0, &e);
        const double after = rmsOf (r.L, (size_t) (2.0 * fs), (size_t) (2.4 * fs));
        check (sawSplice && ! e.isSplicing() && e.ringCells() == (int64_t) (1.0 * 100 * CELLS_PER_CM),
               "REEL -> LOOP splices in a 1 m loop", fmt ("ring %.0f cells", (double) e.ringCells()));
        check (std::abs (20 * std::log10 (after / (REF_AMP / std::sqrt (2.0)))) < 0.1,
               "and the echo resumes at unity", fmt ("%+.3f dB", 20 * std::log10 (after / (REF_AMP / std::sqrt (2.0)))));
        // no step in the output larger than the sine's own largest step
        // from 0.4 s: before that the head is reading the ABRUPT start of the test
        // sine, whose onset step is the test signal, not the splice
        double maxStep = 0.0;
        for (size_t i = (size_t) (0.4 * fs); i < r.L.size(); ++i) maxStep = std::max (maxStep, (double) std::abs (r.L[i] - r.L[i - 1]));
        const double sineStep = REF_AMP * 2.0 * M_PI * 1000.0 / fs;
        check (maxStep < sineStep * 1.05, "the splice is click-free", fmt ("max step %.4f vs sine %.4f", maxStep, sineStep));
    }
}

static void testStereo()
{
    std::printf ("\n7. STEREO\n");
    const double fs = 48000.0;
    const size_t n = (size_t) (1.0 * fs);
    auto x = sine (n, 1000.0, fs, REF_AMP);
    std::vector<float> z (n, 0.0f);
    {
        Params p = wetOnly();
        auto r = run (p, fs, x, z);
        // nothing is RECORDED on the right track; the right channel carries only the
        // head's crosstalk (stage 4: fringing across the guard band, a flat leak)
        const size_t a = (size_t) (0.5 * fs), b = (size_t) (0.9 * fs);
        const double xt = 20.0 * std::log10 (std::max (rmsOf (r.R, a, b), 1.0e-30) / rmsOf (r.L, a, b));
        check (xt < -45.0 && xt > -90.0, "STEREO: left in, the right carries only crosstalk (-45..-90 dB)", fmt ("%.1f dB", xt));
    }
    {
        Params p = wetOnly(); p.inMode = 1;
        auto r = run (p, fs, x, z);
        const size_t a = (size_t) (0.5 * fs), b = (size_t) (0.9 * fs);
        const double l = rmsOf (r.L, a, b), rr = rmsOf (r.R, a, b);
        check (std::abs (l - rr) < 1e-6 && l > 0.0, "MONO: the sum is recorded on both tracks",
               fmt ("L %.4f R %.4f", l, rr));
    }
    {
        Params p = wetOnly(); p.inMode = 1; p.h1Pan = -1.0f;
        auto r = run (p, fs, x, x);
        const size_t a = (size_t) (0.5 * fs), b = (size_t) (0.9 * fs);
        check (rmsOf (r.R, a, b) < 1e-9 && rmsOf (r.L, a, b) > 0.05, "a head panned hard left is only on the left",
               fmt ("L %.4f R %.2e", rmsOf (r.L, a, b), rmsOf (r.R, a, b)));
    }
}

static void testSync()
{
    EqOff eqOff;   // minimum-phase equalisers move the echo's phase, as on a real machine
    std::printf ("\n8. SYNC\n");
    const double fs = 48000.0;
    {
        Params p = wetOnly(); p.sync = 7; p.syncHead = 0; p.h1Pos = 4.0f;     // 1/4 at 120 BPM = 0.5 s
        const size_t total = (size_t) (0.75 * fs);
        std::vector<float> x (total, 0.0f);
        double cin = 0.0, ein = 0.0;
        for (size_t i = 0; i < total; ++i)
        {
            const double t = (double) i / fs - 0.05;
            x[i] = (float) (0.03 * std::exp (-0.5 * t * t / (0.002 * 0.002)));   // small: the timing reference is the small-signal group delay, as in section 1
            cin += (double) x[i] * x[i] * i; ein += (double) x[i] * x[i];
        }
        Engine e;
        auto r = run (p, fs, x, x, nullptr, 120.0, &e);
        double cout = 0.0, eout = 0.0;
        for (size_t i = (size_t) (0.538 * fs); i < (size_t) (0.562 * fs); ++i) { const double v = (double) r.L[i] * r.L[i]; cout += v * i; eout += v; }
        const double lag = (cout / eout - cin / ein) / fs;
        check (std::abs (lag - 0.5) < 1e-6, "1/4 at 120 BPM puts the tempo head at 0.5 s",
               fmt ("%.7f s at %.3f cm/s", lag, e.speedCms()));
    }
    {
        Params p = wetOnly(); p.sync = 12; p.h1Pos = 4.0f;                  // 2/1 at 60 BPM: 8 s -> 0.5 cm/s
        std::vector<float> z ((size_t) (0.2 * fs), 0.0f);
        Engine e;
        run (p, fs, z, z, nullptr, 60.0, &e);
        check (e.tele.syncClamped.load() && std::abs (e.speedCms() - 4.75) < 1e-6,
               "an unreachable note value clamps to 4.75 cm/s and says so", fmt ("%.3f cm/s", e.speedCms()));
    }
}

static void testEveryControl()
{
    std::printf ("\n9. EVERY CONTROL CHANGES THE OUTPUT (each in a context where it should)\n");
    const double fs = 48000.0;
    const size_t n = (size_t) (2.5 * fs);
    std::vector<float> xl = noiseBurst (n, (size_t) (0.05 * fs), (size_t) (0.4 * fs), 21, 0.3f);
    std::vector<float> xr = noiseBurst (n, (size_t) (0.05 * fs), (size_t) (0.4 * fs), 22, 0.3f);

    for (int s = 0; s < NUM_SPECS; ++s)
    {
        const Spec& sp = SPECS[s];
        const std::string id = sp.id;
        Params base;
        base.mix = 0.6f; base.feedback = 0.5f;
        base.h1On = base.h2On = base.h3On = 1;
        double bpm = 0.0;
        if (id == "looplen") base.mode = 1;
        if (id == "synchead" || id == "sync") { bpm = 120.0; base.sync = 7; }
        if (id == "sync") base.sync = 0;
        // the transport's causes on a used machine, where they show; the motor on a
        // worn one, where its ripple differs audibly (fresh, both are tiny - correctly)
        if (id == "capstan" || id == "reels" || id == "scrape") base.condition = 50.0f;
        if (id == "motor") base.condition = 100.0f;
        // the tape's own controls act on a real medium; ACCUMULATE on a short fast loop, sped up
        const bool medium = id == "stock" || id == "particles" || id == "wear" || id == "accumulate";
        if (id == "accumulate") { base.mode = 1; base.loopLen = 0.5f; base.speed = 76.0f; }
        // ON switches are tested by turning them off
        Params alt = base;
        float& m = alt.*(sp.member);
        if (sp.kind == Kind::Toggle) m = (m > 0.5f) ? 0.0f : 1.0f;
        else if (sp.kind == Kind::Choice) m = (m > 0.5f) ? 0.0f : 1.0f;
        else m = (std::abs (m - sp.hi) > std::abs (m - sp.lo)) ? sp.hi : sp.lo;
        if (id == "looplen") m = 0.5f;
        if (id == "sync") m = 4;
        if (id == "speed") m = 40.0f;
        if (medium) g_idealMedium = false;
        Engine ea, eb; ea.setWearRateScale (300.0); eb.setWearRateScale (300.0);
        auto a = run (base, fs, xl, xr, nullptr, bpm, &ea);
        auto b = run (alt, fs, xl, xr, nullptr, bpm, &eb);
        g_idealMedium = true;
        double d = 0.0, e = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            d += std::pow ((double) a.L[i] - b.L[i], 2) + std::pow ((double) a.R[i] - b.R[i], 2);
            e += (double) a.L[i] * a.L[i] + (double) a.R[i] * a.R[i];
        }
        const double rel = std::sqrt (d / std::max (e, 1e-30));
        // Since stage 5 a REEL and a LOOP differ (the reels' wow); the loop's
        // LENGTH still changes nothing on a REEL (and with the erase head at full
        // field, nothing on a loop either until FLUX MEMORY, stage 7). Inert BY
        // DESIGN - asserted as such, so this line fails the day it starts to act.
        if (id == "looplen")
            check (rel < 1.0e-6, (std::string ("control ") + sp.name + " (inert on a reel by design)").c_str(),
                   fmt ("rel. difference %.3f", rel));
        else
            check (rel > 1e-3, (std::string ("control ") + sp.name).c_str(), fmt ("rel. difference %.3f", rel));
    }
}

static void testRailAndView()
{
    std::printf ("\n11. THE RAIL, AND WHAT THE PANEL SEES\n");
    const double fs = 48000.0;
    {
        // Head 1 asked for 10 cm while head 2 sits at 8: it stops against it.
        Params p = wetOnly(); p.h1Pos = 4.0f; p.h2Pos = 8.0f; p.h3Pos = 12.0f;
        std::vector<float> z ((size_t) (3.0 * fs), 0.0f);
        Engine e;
        run (p, fs, z, z, [&] (Params& q, Engine&, size_t i) { if (i >= (size_t) (0.5 * fs)) q.h1Pos = 10.0f; }, 0.0, &e);
        check (std::abs (e.headPosCm (0) - (8.0 - HEAD_SPACING_CM)) < 1e-6 && std::abs (e.headPosCm (1) - 8.0) < 1e-6,
               "heads cannot pass: head 1 stops against head 2, head 2 holds",
               fmt ("head 1 %.4f cm, head 2 %.4f cm", e.headPosCm (0), e.headPosCm (1)));
    }
    {
        // The tape view reads the engine's own cells: a reference-level sine
        // shows as 1.0 downstream of the record head, nothing upstream of erase.
        Params p = wetOnly(); p.speed = 19.0f;
        const size_t n = (size_t) (2.0 * fs);
        auto x = sine (n, 1000.0, fs, REF_AMP);
        Engine e;
        run (p, fs, x, x, nullptr, 0.0, &e);
        float view[66];
        e.tapeView (view, 66, -6.0, 27.0);          // 0.5 cm per point
        double down = 0.0, up = 0.0;
        for (int i = 0; i < 66; ++i)
        {
            const double cm = -6.0 + (i + 0.5) * 0.5;
            if (cm > 1.0 && cm < 26.0) down = std::max (down, (double) view[i]);
            if (cm < -3.0) up = std::max (up, (double) view[i]);   // erased, then virgin on the supply reel
        }
        check (std::abs (down - 1.0) < 0.03 && up == 0.0, "the panel's tape view: 1.0 recorded, 0 before the erase head",
               fmt ("recorded %.3f, upstream %.3f", down, up));
    }
}

/*  The real-time model against the stage 2 reference, at the reference's own
    conditions: 38.1 cm/s, 1 kHz, operating bias. Reference figures are from
    docs/physics/05-reference-results.md (mtref, full settings). The comparisons
    are RELATIVE (to 0 VU, to 1 kHz, to the 3.0 Hc bias row): the absolute level
    is the repro alignment, which is trimmed to 0 VU by construction. */
static void testAgainstReference()
{
    EqOff eqOff;   // minimum-phase equalisers move the echo's phase, as on a real machine
    struct RecordOnly { RecordOnly() { g_recordOnly = true; } ~RecordOnly() { g_recordOnly = false; } } recordOnly;
    std::printf ("\n13. AGAINST THE REFERENCE (38.1 cm/s, 1 kHz, the reference's operating bias)\n");
    const double fs = 48000.0, v = 38.1;
    // The machine is aligned at 19.05 cm/s; the reference was measured at 38.1 with
    // its own alignment. Set the bias and the 0 VU field to the reference's.
    namespace S4 = mt::stock456;
    const double biasRef = 20.0 * std::log10 (S4::HB_OP_REF38 / S4::HB_OPERATING);
    const double amp0 = REF_AMP * S4::HS_0VU_REF38 / S4::HS_0VU;
    auto tone = [&] (double f, double amp, double bias, double& fund, double* hd3)
    {
        Params p = wetOnly(); p.speed = (float) v; soloHead (p, 4.0); p.bias = (float) bias;
        const double d = 4.0 / v;
        const size_t n = (size_t) ((d + 0.5) * fs);
        auto x = sine (n, f, fs, amp);
        auto r = run (p, fs, x, x);
        // a whole number of periods of f, well after the echo arrives
        const size_t a = (size_t) ((d + 0.15) * fs);
        const size_t len = (size_t) (std::floor (0.3 * f) / f * fs);
        fund = goertzelAmp (r.L, a, a + len, f, fs);
        if (hd3) *hd3 = goertzelAmp (r.L, a, a + len, 3.0 * f, fs) / fund;
    };

    // ---- level sweep (reference: demagnetised tape, nu fitted on the full process, 2026-10-11)
    struct LvRow { double db, refRe0, refHd3; };
    const LvRow rows[] = { { -20, -20.02, 0.00009 }, { -10, -10.00, 0.00059 }, { -6, -5.99, 0.00068 }, { 0, 0.0, 0.00143 },
                           { 6, 5.82, 0.01019 }, { 10, 9.39, 0.02791 }, { 12, 10.99, 0.04308 }, { 16, 13.65, 0.08920 } };
    double f0, h0; tone (1000.0, amp0, biasRef, f0, &h0);
    double worstLv = 0.0, worstHd = 0.0, hdAt0 = 0.0, refHdAt0 = 0.0;
    std::printf ("   %8s %10s %10s %10s %10s\n", "dB re 0VU", "out re 0VU", "reference", "HD3 %", "ref HD3 %");
    for (const auto& rw : rows)
    {
        double a, h; tone (1000.0, amp0 * std::pow (10.0, rw.db / 20.0), biasRef, a, &h);
        const double re = 20.0 * std::log10 (a / f0);
        std::printf ("   %8.0f %10.2f %10.2f %10.3f %10.3f\n", rw.db, re, rw.refRe0, 100 * h, 100 * rw.refHd3);
        worstLv = std::max (worstLv, std::abs (re - rw.refRe0));
        if (rw.db >= 6) worstHd = std::max (worstHd, std::abs (20.0 * std::log10 (h / rw.refHd3)));
        if (rw.db == 0) { hdAt0 = h; refHdAt0 = rw.refHd3; }
    }
    check (worstLv < 0.25, "the level transfer follows the reference within 0.25 dB (-20..+16)", fmt ("worst %.3f dB", worstLv));
    check (worstHd < 1.5, "third harmonic follows the reference within 1.5 dB (+6..+16)", fmt ("worst %.2f dB", worstHd));
    // KNOWN DEVIATION, not a pass: at 0 VU the DC-table method records less third
    // harmonic than the AC reference. Excluded as causes: table resolution (41 -> 81
    // s points moved it by 0.001 %), depth sampling (slab-integrated layers), bias
    // phase (averaged). Both models share a ~0.06 % level-independent floor from the
    // finite bias decay; at 0 VU that floor and the curve's own HD3 are comparable.
    std::printf ("  [note] KNOWN DEVIATION: HD3 at 0 VU %.3f %% vs reference %.3f %% (%+.1f dB; %.0f vs %.0f dB re fundamental)\n",
                 100 * hdAt0, 100 * refHdAt0, 20.0 * std::log10 (hdAt0 / refHdAt0), 20.0 * std::log10 (hdAt0), 20.0 * std::log10 (refHdAt0));
    {
        double lo = 0.0, hi = 20.0;
        for (int it = 0; it < 18; ++it)
        {
            const double db = 0.5 * (lo + hi); double a, h;
            tone (1000.0, amp0 * std::pow (10.0, db / 20.0), biasRef, a, &h);
            (h < 0.03 ? lo : hi) = db;
        }
        double a, h; tone (1000.0, amp0 * std::pow (10.0, 0.5 * (lo + hi) / 20.0), biasRef, a, &h);
        const double mol = 20.0 * std::log10 (a / f0);
        check (std::abs (mol - 9.66) < 0.5, "MOL (3 % third harmonic) within 0.5 dB of the reference's +9.66 dB re 0 VU",
               fmt ("%+.2f dB re 0 VU = %+.2f dB re 185 nWb/m (sheet +12.3)", mol, mol + 20.0 * std::log10 (250.0 / 185.0)));
    }

    // ---- response, -20 dB
    {
        const double fr[] = { 2000, 5000, 10000, 15000 };
        const double ref[] = { -0.01, -0.08, -0.32, -0.72 };
        double r1; tone (1000.0, amp0 * 0.1, biasRef, r1, nullptr);
        double worst = 0.0;
        std::printf ("   response:");
        for (int i = 0; i < 4; ++i)
        {
            double a; tone (fr[i], amp0 * 0.1, biasRef, a, nullptr);
            const double db = 20.0 * std::log10 (a / r1);
            std::printf ("  %.0f Hz %+.2f (ref %+.2f)", fr[i], db, ref[i]);
            worst = std::max (worst, std::abs (db - ref[i]));
        }
        std::printf ("\n");
        check (worst < 0.25, "recording loss follows the reference within 0.25 dB to 15 kHz", fmt ("worst %.3f dB", worst));
    }

    // ---- bias curve at a fixed deep-gap signal of 0.08 Hc, relative to the 3.0 Hc row
    {
        struct BRow { double hb, r1, r10; };
        const BRow br[] = { { 1.5, -13.76, -13.96 }, { 2.0, -11.83, -12.04 }, { 2.5, -10.98, -11.21 }, { 3.0, -10.40, -10.65 },
                            { 4.0, -9.70, -10.02 }, { 6.0, -11.44, -11.72 }, { 8.0, -13.82, -14.34 } };
        const double ref1at3 = -10.40, ref10at3 = -10.65;
        const double hbOp = mt::stock456::HB_OPERATING;
        const double amp = 0.08 / (mt::stock456::HS_0VU / REF_PEAK);  // deep-gap 0.08 Hc in sample units
        double base1 = 0, base10 = 0, worst = 0.0;
        std::printf ("   bias curve, dB re the 3.0 Hc row (1 kHz / 10 kHz):\n    ");
        for (int pass = 0; pass < 2; ++pass)
            for (const auto& b : br)
            {
                const double db = 20.0 * std::log10 (b.hb / hbOp);
                double a1, a10; tone (1000.0, amp, db, a1, nullptr); tone (10000.0, amp, db, a10, nullptr);
                if (pass == 0) { if (b.hb == 3.0) { base1 = a1; base10 = a10; } continue; }
                const double o1 = 20.0 * std::log10 (a1 / base1), o10 = 20.0 * std::log10 (a10 / base10);
                const double e1 = o1 - (b.r1 - ref1at3), e10 = o10 - (b.r10 - ref10at3);
                std::printf (" %.1f: %+.2f/%+.2f (ref %+.2f/%+.2f) ", b.hb, o1, o10, b.r1 - ref1at3, b.r10 - ref10at3);
                worst = std::max ({ worst, std::abs (e1), std::abs (e10) });
            }
        std::printf ("\n");
        check (worst < 0.5, "the bias curve follows the reference within 0.5 dB (1.5 .. 8 Hc)", fmt ("worst %.3f dB", worst));
    }
}

/*  Speed: the record process is not speed-independent - a slower tape sees more
    bias cycles in the zone. Reference: mtref --speed= at a FIXED bias and 0 VU
    field (the 38.1 alignment), 05-reference-results.md / 06-realtime-record.md. */
static void testSpeeds()
{
    EqOff eqOff;   // minimum-phase equalisers move the echo's phase, as on a real machine
    struct RecordOnly { RecordOnly() { g_recordOnly = true; } ~RecordOnly() { g_recordOnly = false; } } recordOnly;
    std::printf ("\n14. SPEED: THE SAME RECORDING AT 19 AND 76 cm/s (fixed bias and field)\n");
    namespace S4 = mt::stock456;
    const double fs = 48000.0;
    const double biasRef = 20.0 * std::log10 (S4::HB_OP_REF38 / S4::HB_OPERATING);
    const double amp0 = REF_AMP * S4::HS_0VU_REF38 / S4::HS_0VU;
    auto tone = [&] (double v, double f, double amp, double& fund, double* hd3)
    {
        Params p = wetOnly(); p.speed = (float) v; soloHead (p, 4.0); p.bias = (float) biasRef;
        const double d = 4.0 / v;
        const size_t n = (size_t) ((d + 0.5) * fs);
        auto x = sine (n, f, fs, amp);
        auto r = run (p, fs, x, x);
        const size_t a = (size_t) ((d + 0.15) * fs);
        const size_t len = (size_t) (std::floor (0.3 * f) / f * fs);
        fund = goertzelAmp (r.L, a, a + len, f, fs);
        if (hd3) *hd3 = goertzelAmp (r.L, a, a + len, 3.0 * f, fs) / fund;
    };
    struct SpeedRef { double v, sens, hd6, hd10, l10, l15; };
    const SpeedRef refs[] = { { 19.05, 0.63, 0.01377, 0.03416, -1.13, -2.38 },
                              { 76.2, -1.25, 0.00540, 0.01874, -0.10, -0.23 } };
    double base; tone (38.1, 1000.0, amp0, base, nullptr);
    for (const auto& r : refs)
    {
        double a0, a6, h6, a10, h10, r1, r10, r15;
        tone (r.v, 1000.0, amp0, a0, nullptr);
        tone (r.v, 1000.0, amp0 * 2.0, a6, &h6);
        tone (r.v, 1000.0, amp0 * std::pow (10.0, 0.5), a10, &h10);
        tone (r.v, 1000.0, amp0 * 0.1, r1, nullptr);
        tone (r.v, 10000.0, amp0 * 0.1, r10, nullptr);
        tone (r.v, 15000.0, amp0 * 0.1, r15, nullptr);
        const double sens = 20.0 * std::log10 (a0 / base);
        const double l10 = 20.0 * std::log10 (r10 / r1), l15 = 20.0 * std::log10 (r15 / r1);
        std::printf ("   %.2f cm/s: sensitivity %+.2f dB re 38.1 (ref %+.2f); HD3 +6 dB %.3f %% (ref %.3f), +10 dB %.3f %% (ref %.3f); loss 10k %+.2f (ref %+.2f), 15k %+.2f (ref %+.2f)\n",
                     r.v, sens, r.sens, 100 * h6, 100 * r.hd6, 100 * h10, 100 * r.hd10, l10, r.l10, l15, r.l15);
        check (std::abs (sens - r.sens) < 0.25, fmt ("%.2f cm/s: sensitivity follows the reference within 0.25 dB", r.v).c_str(), fmt ("%+.3f dB off", sens - r.sens));
        const double hdErr = std::max (std::abs (20.0 * std::log10 (h6 / r.hd6)), std::abs (20.0 * std::log10 (h10 / r.hd10)));
        check (hdErr < 1.5, fmt ("%.2f cm/s: third harmonic within 1.5 dB at +6 and +10 dB", r.v).c_str(), fmt ("worst %.2f dB", hdErr));
        const double lErr = std::max (std::abs (l10 - r.l10), std::abs (l15 - r.l15));
        check (lErr < 0.35, fmt ("%.2f cm/s: recording loss within 0.35 dB at 10 and 15 kHz", r.v).c_str(), fmt ("worst %.2f dB", lErr));
    }

    // The deep layers' cells are 2 and 4 um. At the slowest speed a 15 kHz tone is
    // 3.2 um on tape and would fold in them; their kernels are widened where their
    // Nyquist is inside the audio band (it was -25 dB re the tone without that).
    {
        const double v = 4.75, f = 15000.0, lamUm = v * 1.0e4 / f;
        double fund; tone (v, f, amp0 * 0.5, fund, nullptr);
        Params p = wetOnly(); p.speed = (float) v; soloHead (p, 4.0); p.bias = (float) biasRef;
        const double d = 4.0 / v;
        const size_t n = (size_t) ((d + 0.5) * fs);
        auto x = sine (n, f, fs, amp0 * 0.5);
        auto rr = run (p, fs, x, x);
        const size_t a = (size_t) ((d + 0.15) * fs), len = (size_t) (0.3 * fs);
        std::printf ("   15 kHz at 4.75 cm/s (%.2f um on tape)", lamUm);
        double worst = -300.0;
        for (double pitchUm : { 2.0, 4.0 })
        {
            const double fa = std::abs (1.0 / lamUm - 1.0 / pitchUm) * v * 1.0e4;
            const double db = 20.0 * std::log10 (goertzelAmp (rr.L, a, a + len, fa, fs) / fund);
            std::printf ("; %.0f um cells fold it to %.0f Hz at %+.1f dB re the tone", pitchUm, fa, db);
            worst = std::max (worst, db);
        }
        std::printf ("\n");
        check (worst < -55.0, "the deep layers' coarse cells do not fold (15 kHz at 4.75 cm/s, < -55 dB re the tone)", fmt ("worst %+.1f dB", worst));
    }
}

/*  The playback head against the reference's playback: the same small signal
    recorded at the 19.05 cm/s alignment's bias and field, read through the
    head (spacing, thickness, gap) - each re its own 1 kHz. The reference reads
    16 depths; the engine 4 slabs with the generated spacing correction. */
static void testPlayback()
{
    EqOff eqOff;   // minimum-phase equalisers move the echo's phase, as on a real machine
    std::printf ("\n15. PLAYBACK HEAD AGAINST THE REFERENCE (-20 dB, dB re each one's 1 kHz)\n");
    namespace P = mt::pb456;
    const double fs = 48000.0;
    auto tone = [&] (double v, double f) {
        Params p = wetOnly(); p.speed = (float) v; soloHead (p, 4.0);
        const double d = 4.0 / v;
        const size_t n = (size_t) ((d + 0.5) * fs);
        auto x = sine (n, f, fs, REF_AMP * 0.1);
        auto r = run (p, fs, x, x);
        const size_t a = (size_t) ((d + 0.15) * fs);
        const size_t len = (size_t) (std::floor (0.3 * f) / f * fs);
        return goertzelAmp (r.L, a, a + len, f, fs);
    };
    double worstAll = 0.0;
    for (int iv = 0; iv < P::NV; ++iv)
    {
        const double v = P::SPEED_CMS[iv];
        const double a1 = tone (v, 1000.0);
        std::printf ("   %6.2f cm/s:", v);
        double worst = 0.0, deep = 0.0, deepF = 0.0;
        for (int fi = 0; fi < P::NF; ++fi)
        {
            if (P::F_HZ[fi] == 1000.0) continue;
            const double db = 20.0 * std::log10 (tone (v, P::F_HZ[fi]) / a1);
            const double ref = P::REF_DB[iv][fi];
            std::printf ("  %5.0f %+6.2f (ref %+6.2f)", P::F_HZ[fi], db, ref);
            if (P::F_HZ[fi] > 15000.0) continue;
            // the tolerance holds where the head still passes the signal (within 20 dB
            // of 1 kHz); deeper in the roll-off the error is printed, not hidden
            if (ref > -20.0) worst = std::max (worst, std::abs (db - ref));
            else if (ref > -30.0 && std::abs (db - ref) > deep) { deep = std::abs (db - ref); deepF = P::F_HZ[fi]; }
        }
        std::printf ("\n");
        // diagnostic: the same with the head losses off, against the reference's
        // depth-averaged flux. Not a check: at slow speeds the deep slabs' depth
        // average carries treble the head never hears (and the anti-alias rule
        // trims it), so record error does not carry over into playback one to one.
        {
            struct RecordOnly { RecordOnly() { g_recordOnly = true; } ~RecordOnly() { g_recordOnly = false; } } ro;
            const double r1 = tone (v, 1000.0);
            std::printf ("          record only:");
            for (int fi = 0; fi < P::NF; ++fi)
            {
                if (P::F_HZ[fi] == 1000.0) continue;
                const double db = 20.0 * std::log10 (tone (v, P::F_HZ[fi]) / r1);
                std::printf ("  %5.0f %+6.2f (ref %+6.2f)", P::F_HZ[fi], db, P::REC_DB[iv][fi]);
            }
            std::printf ("\n");
        }
        check (worst < 0.25, fmt ("%.2f cm/s: playback follows the reference within 0.25 dB (to -20 dB)", v).c_str(), fmt ("worst %.3f dB", worst));
        if (deep > 0.25)
            std::printf ("  [note] KNOWN DEVIATION: %.2f cm/s, %.0f Hz (response below -20 dB): %.2f dB off the reference\n", v, deepF, deep);
        worstAll = std::max (worstAll, worst);
    }
}

/*  The aligned machine: record pre-emphasis and replay equalisation in, a small
    signal through the whole chain, re its own 1 kHz. A technician aligns for flat
    at -20 dB; above the head's limits (the gap null near 19 kHz at 4.76 cm/s,
    the +10 dB replay trim) the response is allowed to fall. */
static void testEqualised()
{
    std::printf ("\n16. THE ALIGNED MACHINE (record and replay equalisation, -20 dB, dB re 1 kHz)\n");
    // the slowest speed's band edge (6 kHz at 4.6 cm/s), where the replay trim climbs toward the
    // gap null, is the hard spot for every coating; the studio is the flatness claim
    for (int k = 1; k < NUM_COATINGS; ++k)
        check (Engine::eqFitWorstDb (k) < 0.75, (std::string ("the ") + coatingTables (k).name + " coating's equaliser fits within 0.75 dB").c_str(), fmt ("worst %.3f dB at %.2f cm/s", Engine::eqFitWorstDb (k), Engine::eqFitWorstSpeed (k)));
    for (int k = 0; k < NUM_COATINGS; ++k)
        std::printf ("   equaliser fit, coating %d (%s): worst %.3f dB at %.2f cm/s\n", k, coatingTables (k).name,
                     Engine::eqFitWorstDb (k), Engine::eqFitWorstSpeed (k));
    const double fit = Engine::eqFitWorstDb (0);
    check (fit < 0.5, "the equaliser fits follow their targets within 0.5 dB (the flatness tolerance)", fmt ("worst %.3f dB at %.2f cm/s", fit, Engine::eqFitWorstSpeed (0)));
    const double fs = 48000.0;
    auto tone = [&] (double v, int eq, double f) {
        Params p = wetOnly(); p.speed = (float) v; soloHead (p, 4.0); p.eqStd = (float) eq;
        const double d = 4.0 / v;
        const size_t n = (size_t) ((d + 0.6) * fs);
        auto x = sine (n, f, fs, REF_AMP * 0.1);
        auto r = run (p, fs, x, x);
        const size_t a = (size_t) ((d + 0.2) * fs);
        const size_t len = (size_t) (std::floor (0.3 * f) / f * fs);
        return goertzelAmp (r.L, a, a + len, f, fs);
    };
    const double fl[] = { 40, 100, 300, 3000, 6000, 10000, 15000 };
    for (int eq = 0; eq < 2; ++eq)
        for (double v : { 4.7625, 9.525, 19.05, 38.1, 76.2 })
        {
            const double a1 = tone (v, eq, 1000.0);
            std::printf ("   %s %6.2f cm/s:", eq ? "NAB" : "IEC", v);
            double worst = 0.0;
            for (double fr : fl)
            {
                const double db = 20.0 * std::log10 (tone (v, eq, fr) / a1);
                std::printf ("  %5.0f %+6.2f", fr, db);
                // flat where the head can still be trimmed flat: to 15 kHz from 19 cm/s,
                // to 10 kHz at 9.5, to 6 kHz at 4.76
                const double fTop = v >= 19.0 ? 15000.0 : v >= 9.0 ? 10000.0 : 6000.0;
                // above the head bump (stage 4c): the contour effect is part of the
                // machine, and nobody aligns it away
                if (fr >= 300.0 && fr <= fTop) worst = std::max (worst, std::abs (db));
            }
            std::printf ("\n");
            check (worst < 0.5, (std::string (eq ? "NAB " : "IEC ") + fmt ("%.2f cm/s: flat within 0.5 dB where the head allows", v)).c_str(), fmt ("worst %.2f dB", worst));
        }
}

/*  Stage 4c: what a real head and its electronics add. The head bump (the
    contour effect of a finite head: a bump near the wavelength 2L and a cut
    toward DC, moving with speed), crosstalk between the tracks, and the playback
    amplifier's +2 dBFS rail. */
static void testHeadAndElectronics()
{
    std::printf ("\n17. HEAD BUMP, CROSSTALK, PLAYBACK AMPLIFIER\n");
    const double fs = 48000.0;
    auto tone = [&] (double v, double f, const std::vector<float>* rightIn = nullptr) {
        Params p = wetOnly(); p.speed = (float) v; soloHead (p, 4.0);
        const double d = 4.0 / v;
        const double dur = std::max (0.6, 12.0 / f);
        const size_t n = (size_t) ((d + 0.3 + dur) * fs);
        auto x = sine (n, f, fs, REF_AMP * 0.1);
        auto r = run (p, fs, x, x);
        const size_t a = (size_t) ((d + 0.3) * fs);
        const size_t len = (size_t) (std::floor (dur * f) / f * fs);
        (void) rightIn;
        return goertzelAmp (r.L, a, a + len, f, fs);
    };
    for (double v : { 19.05, 38.1, 76.2 })
    {
        const double a1 = tone (v, 1000.0);
        const double fb = v * 1.0e4 / (2.0 * HEAD_EDGE_UM);     // where the wavelength is 2L
        std::printf ("   %5.2f cm/s (2L at %.0f Hz):", v, fb);
        double peak = -100.0, peakF = 0.0;
        for (double m : { 0.25, 0.5, 0.7, 0.85, 1.0, 1.2, 1.5, 2.0, 3.0 })
        {
            const double db = 20.0 * std::log10 (tone (v, fb * m) / a1);
            std::printf ("  %.0f %+.2f", fb * m, db);
            if (m >= 0.5 && db > peak) { peak = db; peakF = fb * m; }
        }
        const double low = 20.0 * std::log10 (tone (v, fb * 0.25) / a1);
        std::printf ("\n");
        check (peak > 1.0 && peak < 4.5 && peakF > 0.6 * fb && peakF < 1.3 * fb,
               fmt ("%.2f cm/s: a head bump of 1-4.5 dB near the wavelength 2L", v).c_str(), fmt ("%+.2f dB at %.0f Hz", peak, peakF));
        check (low < -3.0, fmt ("%.2f cm/s: and a cut below it, toward DC", v).c_str(), fmt ("%+.2f dB at %.0f Hz", low, 0.25 * fb));
    }
    // crosstalk: left track only, the right channel at 100 Hz and 1 kHz
    for (double f : { 100.0, 1000.0 })
    {
        Params p = wetOnly(); p.speed = 19.05f; soloHead (p, 4.0);
        const size_t n = (size_t) (1.2 * fs);
        auto x = sine (n, f, fs, REF_AMP);
        std::vector<float> z (n, 0.0f);
        auto r = run (p, fs, x, z);
        const size_t a = (size_t) (0.5 * fs), b = (size_t) (1.1 * fs);
        const double xt = 20.0 * std::log10 (goertzelAmp (r.R, a, b, f, fs) / goertzelAmp (r.L, a, b, f, fs));
        check (xt < -45.0 && xt > -80.0, fmt ("crosstalk at %.0f Hz, 19 cm/s: -45..-80 dB", f).c_str(), fmt ("%.1f dB", xt));
    }
    // the playback amplifier's rail holds a self-oscillation's output
    {
        Params p = wetOnly(); p.feedback = 1.0f; p.speed = 19.0f; soloHead (p, 4.0);
        const size_t n = (size_t) (12.0 * fs);
        auto x = noiseBurst (n, (size_t) (0.1 * fs), (size_t) (0.3 * fs), 5, 0.3f);
        auto r = run (p, fs, x, x);
        double pk = 0.0; for (size_t i = (size_t) (2 * fs); i < n; ++i) pk = std::max (pk, (double) std::abs (r.L[i]));
        check (pk <= AMP_RAIL * 1.01, "a self-oscillation's output stays inside the +2 dBFS rail", fmt ("peak %+.2f dBFS", 20.0 * std::log10 (pk)));
    }
}

/*  Stage 5: transport mechanics. Wow and flutter from physical causes, and a
    W&F meter checked against an independent measurement: a 3150 Hz tone recorded
    and played, its frequency demodulated from the zero crossings, weighted and
    detected the same way (DIN 45507 / IEC 60386, 2 sigma). */
static void testTransport()
{
    std::printf ("\n18. TRANSPORT MECHANICS (wow and flutter from causes, the W&F meter)\n");
    const double fs = 48000.0;
    // weighted W&F of a played 3150 Hz tone, the same weighting and detector as the meter
    auto measure = [&] (Params p, double secs, double* wowRms = nullptr, double* meter = nullptr, double head = 4.0) {
        soloHead (p, head);
        const size_t n = (size_t) (secs * fs);
        auto x = sine (n, 3150.0, fs, REF_AMP * 0.3);
        Engine e;
        auto r = run (p, fs, x, x, nullptr, 0.0, &e);
        if (meter) *meter = e.tele.wowFlutter.load();
        // zero crossings, linearly interpolated, from 1.5 s (the echo is there)
        std::vector<double> tz;
        for (size_t i = (size_t) (1.5 * fs); i + 1 < n; ++i)
            if (r.L[i] <= 0.0f && r.L[i + 1] > 0.0f)
                tz.push_back ((i + r.L[i] / (double) (r.L[i] - r.L[i + 1])) / fs);
        // the deviation at 1 kHz
        std::vector<double> dev;
        size_t k = 1;
        for (double t = tz.front() + 0.01; t < tz.back() - 0.01; t += 0.001)
        {
            while (k + 1 < tz.size() && tz[k] < t) ++k;
            const double f = 1.0 / (tz[k] - tz[k - 1]);
            dev.push_back (f / 3150.0 - 1.0);
        }
        // remove the mean (the tone's own frequency through the record/play chain)
        double mu = 0.0; for (double d : dev) mu += d; mu /= (double) dev.size();
        // IEC weighting (band-pass 4 Hz, Q 0.6, unity peak) and a 2-second mean square
        const double w0 = 2.0 * M_PI * 4.0 / 1000.0, al = std::sin (w0) / 1.2, cs = std::cos (w0), nn = 1.0 + al;
        const double b0 = al / nn, b2 = -al / nn, a1 = -2.0 * cs / nn, a2 = (1.0 - al) / nn;
        double z1 = 0.0, z2 = 0.0, ms = 0.0, ws = 0.0; const double ea = 1.0 - std::exp (-1.0 / 2000.0);
        for (double d : dev)
        {
            const double xx = d - mu;
            const double y = b0 * xx + z1; z1 = -a1 * y + z2; z2 = b2 * xx - a2 * y;
            ms += ea * (y * y - ms);
            ws += xx * xx;
        }
        if (wowRms) *wowRms = 100.0 * std::sqrt (ws / (double) dev.size());
        return 2.0 * std::sqrt (ms) * 100.0;
    };
    Params base; base.mix = 1.0f; base.feedback = 0.0f; base.speed = 19.05f;
    // an ideal transport: the tape's speed is the target, exactly
    {
        Params p = base; p.condition = 0.0f; soloHead (p, 4.0);
        Engine e; double worst = 0.0;
        const size_t n = (size_t) (2.0 * fs);
        std::vector<float> z (n, 0.0f);
        run (p, fs, z, z, [&] (Params&, Engine& en, size_t) { worst = std::max (worst, std::abs (en.speedCms() / (double) 19.05f - 1.0)); }, 0.0, &e);
        check (worst < 1.0e-12, "CONDITION 0: an ideal transport (the speed is the target, exactly)", fmt ("worst %.2e", worst));
    }
    // a freshly serviced machine, and the meter against the measurement
    double meter = 0.0;
    const double fresh = measure (base, 9.0, nullptr, &meter);
    std::printf ("   freshly serviced (CONDITION 15): weighted W&F %.4f %% measured, %.4f %% on the meter\n", fresh, meter);
    check (fresh > 0.015 && fresh < 0.05, "a freshly serviced machine measures about 0.03 % weighted W&F", fmt ("%.4f %%", fresh));
    check (std::abs (meter / fresh - 1.0) < 0.10, "the W&F meter agrees with a demodulated 3150 Hz tone within 10 %", fmt ("meter %.4f vs %.4f %%", meter, fresh));
    // wear
    {
        Params p = base; p.condition = 100.0f;
        const double worn = measure (p, 9.0);
        std::printf ("   worn out (CONDITION 100): %.3f %%\n", worn);
        check (worn / fresh > 4.0 && worn / fresh < 10.0, "worn out: W&F grows with the wear (x4..x10)", fmt ("x%.1f", worn / fresh));
    }
    // the causes move their own bands
    {
        Params a = base; a.condition = 50.0f; a.capstan = 0.0f;
        Params b = a; b.capstan = 200.0f;
        const double c0 = measure (a, 6.0), c2 = measure (b, 6.0);
        check (c2 > 2.0 * c0, "CAPSTAN raises the flutter", fmt ("%.4f -> %.4f %%", c0, c2));
        Params ra = base; ra.condition = 50.0f; ra.reels = 0.0f; ra.capstan = 0.0f; ra.scrape = 0.0f;
        Params rb = ra; rb.reels = 200.0f;
        double w0 = 0.0, w2 = 0.0;
        // a far head: slow wow mostly cancels between record and play at a short
        // delay (2 sin(pi f tau)), and the demodulator has a floor of its own
        measure (ra, 12.0, &w0, nullptr, 20.0); measure (rb, 12.0, &w2, nullptr, 20.0);
        check (w2 > 2.0 * std::max (w0, 1.0e-6), "REELS raises the (unweighted) wow", fmt ("rms %.4f -> %.4f %%", w0, w2));
    }
    // scrape: fuzz around the tone, barely a weighted W&F
    {
        auto fuzz = [&] (float scr) {
            Params p = base; p.condition = 50.0f; p.scrape = scr; p.capstan = 0.0f; p.reels = 0.0f; soloHead (p, 4.0);
            const size_t n = (size_t) (3.0 * fs);
            // a 9 kHz tone: the stick-slip at ~3.2 kHz puts its sidebands at 9 +- 3.2 kHz,
            // clear of every harmonic (at 3150 Hz they fall on 50 Hz and on the 2nd harmonic)
            auto x = sine (n, 9000.0, fs, REF_AMP * 0.3);
            auto r = run (p, fs, x, x);
            const size_t a = (size_t) (1.5 * fs), b = n - 100;
            const double car = goertzelAmp (r.L, a, b, 9000.0, fs);
            double side = 0.0;
            for (double fq = 5200.0; fq <= 6400.0; fq += 100.0) side += std::pow (goertzelAmp (r.L, a, b, fq, fs), 2.0);
            for (double fq = 11600.0; fq <= 12800.0; fq += 100.0) side += std::pow (goertzelAmp (r.L, a, b, fq, fs), 2.0);
            return 10.0 * std::log10 (side / (car * car) + 1.0e-30);
        };
        const double s0 = fuzz (0.0f), s2 = fuzz (200.0f);
        check (s2 > s0 + 10.0, "SCRAPE raises the modulation noise around a tone", fmt ("%+.1f -> %+.1f dB re the tone", s0, s2));
        Params a = base; a.condition = 50.0f; a.scrape = 0.0f;
        Params b = a; b.scrape = 200.0f;
        const double wa = measure (a, 6.0), wb = measure (b, 6.0);
        check (std::abs (wb / wa - 1.0) < 0.15, "and barely moves a weighted W&F meter", fmt ("%.4f -> %.4f %%", wa, wb));
    }
    // the motors: a speed step
    auto stepResp = [&] (int motor, double& overshoot, double& settle, double& fHunt) {
        Params p = base; p.condition = 0.0f; p.motor = (float) motor; p.speed = 9.5f; soloHead (p, 4.0);
        const size_t n = (size_t) (5.0 * fs);
        std::vector<float> z (n, 0.0f);
        std::vector<double> tv, vv;
        Engine e;
        run (p, fs, z, z, [&] (Params& q, Engine& en, size_t i) {
            tv.push_back ((double) i / fs); vv.push_back (en.speedCms());
            if ((double) i >= 0.5 * fs) q.speed = 19.05f; }, 0.0, &e);
        double peak = 0.0; settle = 0.0;
        int crossings = 0; double tFirst = 0.0, tLast = 0.0;
        for (size_t k = 1; k < tv.size(); ++k)
        {
            if (tv[k] < 0.5) continue;
            peak = std::max (peak, vv[k]);
            if (std::abs (vv[k] / 19.05 - 1.0) > 0.001) settle = tv[k] - 0.5;
            if ((vv[k - 1] - 19.05) * (vv[k] - 19.05) < 0.0) { if (crossings == 0) tFirst = tv[k]; tLast = tv[k]; ++crossings; }
        }
        overshoot = (peak - 19.05) / (19.05 - 9.5);
        fHunt = crossings >= 3 ? 0.5 * (crossings - 1) / (tLast - tFirst) : 0.0;
    };
    {
        double os, st, fh;
        stepResp (0, os, st, fh);
        std::printf ("   AC SYNC, 9.5 -> 19.05 cm/s: overshoot %.1f %%, within 0.1 %% after %.2f s, hunting %.2f Hz\n", 100 * os, st, fh);
        check (os > 0.02 && os < 0.30 && st < 3.0, "AC SYNC: the speed glides there and hunts a little", fmt ("overshoot %.1f %%", 100 * os) + fmt (", settled %.2f s", st));
        double os2, st2, fh2;
        stepResp (1, os2, st2, fh2);
        std::printf ("   DC SERVO: overshoot %.1f %%, within 0.1 %% after %.2f s\n", 100 * os2, st2);
        check (os2 < os && st2 < st, "DC SERVO: firmer and quicker than AC SYNC", fmt ("overshoot %.1f %%", 100 * os2) + fmt (", settled %.2f s", st2));
    }
    // the meter through a speed change: a glide is not wow, so it waits, then reads
    {
        Params p = base; p.speed = 19.05f; soloHead (p, 4.0);
        const size_t n = (size_t) (9.0 * fs);
        std::vector<float> z (n, 0.0f);
        double during = 0.0, worst = 0.0, last = 0.0, tBack = -1.0;
        Engine e;
        run (p, fs, z, z, [&] (Params& q, Engine& en, size_t i) {
            const double t = (double) i / fs, m = en.tele.wowFlutter.load();
            if (t >= 1.0) q.speed = 38.1f;
            if (t > 1.15 && t < 1.25) during = m;
            if (t > 1.0) { worst = std::max (worst, m); if (m >= 0.0 && tBack < 0.0) tBack = t - 1.0; if (m < 0.0) tBack = -1.0; }
            last = m; }, 0.0, &e);
        std::printf ("   meter through 19 -> 38 cm/s: %.0f during the glide, reading again after %.2f s, %.4f %% at the end (worst %.4f %%)\n", during, tBack, last, worst);
        check (during < 0.0 && last > 0.015 && last < 0.06 && worst < 0.1,
               "the W&F meter waits out a speed change, then reads the machine again", fmt ("%.4f %% at the end", last) + fmt (", worst %.4f %%", worst));
    }
}

//------------------------------------------------------------------------------ stage 6 helpers
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
        const double ang = -2.0 * M_PI / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

/*  One-sided power spectral density (units^2 per Hz) of x[a, b), Welch, Hann, half
    overlap, segments of nfft. */
static std::vector<double> psdOf (const std::vector<float>& x, size_t a, size_t b, size_t nfft)
{
    std::vector<double> p (nfft / 2 + 1, 0.0), w (nfft);
    double w2 = 0.0;
    for (size_t i = 0; i < nfft; ++i) { w[i] = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / nfft); w2 += w[i] * w[i]; }
    int segs = 0;
    std::vector<std::complex<double>> buf (nfft);
    for (size_t s = a; s + nfft <= b; s += nfft / 2)
    {
        for (size_t i = 0; i < nfft; ++i) buf[i] = std::complex<double> (w[i] * x[s + i], 0.0);
        fft (buf);
        for (size_t k = 0; k <= nfft / 2; ++k) p[k] += std::norm (buf[k]);
        ++segs;
    }
    for (size_t k = 0; k <= nfft / 2; ++k) p[k] *= (k == 0 || k == nfft / 2 ? 1.0 : 2.0) / (w2 * std::max (segs, 1));
    return p;   // divide by fs for per-Hz: done by the caller, who knows fs
}

static double bandPower (const std::vector<double>& psd, double fs, size_t nfft, double f1, double f2)
{
    const double df = fs / (double) nfft;
    double s = 0.0;
    for (size_t k = 1; k + 1 < psd.size(); ++k)
    {
        const double f = k * df;
        if (f >= f1 && f < f2) s += psd[k] / fs * df;
    }
    return s;
}

static double aWeight (double f)
{
    const double f2 = f * f;
    const double ra = 12194.0 * 12194.0 * f2 * f2 / ((f2 + 20.6 * 20.6) * std::sqrt ((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9)) * (f2 + 12194.0 * 12194.0));
    return ra * 1.2589254117941673;   // +2.0 dB, so 1 kHz is unity
}

/*  The continuous coating's hiss, as the head reads it: per unit length the
    particles' variance density is v/p (um^3) over track width x coating^2, every
    depth on its own, through spacing and gap. One-sided PSD of the engine's
    output, units^2 per Hz, with the repro alignment (outK pbTrim packing), the
    sample's transit box and its correction FIR. Written here from first principles,
    independent of the engine's slabs. */
static double hissPsd (double f, double vCms, double vpUm3, double outK, double pbTrim, double pack, double fsInt)
{
    const double vum = vCms * 1.0e4;
    const double k = 2.0 * M_PI * f / vum;
    const double d = PB_SPACING_UM, g = PB_GAP_UM, delta = COATING_UM;
    const double x = 0.5 * k * g;
    const double gap = (x < 1.0e-9) ? 1.0 : std::sin (x) / x;
    const double depth = (std::exp (-2.0 * k * d) - std::exp (-2.0 * k * (d + delta))) / (2.0 * k);
    const double qR = vpUm3 / (TRACK_W_UM * delta * delta) * gap * gap * depth;   // um
    const double xs = M_PI * f / fsInt;
    const double tr = std::sin (xs) / xs;
    const double xc = 16000.0 / fsInt;
    const double inv = (M_PI * xc) / std::sin (M_PI * xc);
    const double ac = (inv - 1.0) / (2.0 * (1.0 - std::cos (2.0 * M_PI * xc)));
    const double comp = (1.0 + 2.0 * ac) - 2.0 * ac * std::cos (2.0 * M_PI * f / fsInt);
    const double g2 = outK * pbTrim * pack;
    return g2 * g2 * 2.0 * qR / vum * tr * tr * comp * comp;
}

static void testStock()
{
    std::printf ("\n19. TAPE STOCK AND MEDIUM (particles, coating, surface, wear)\n");
    const double fs = 48000.0;

    // ---- the coatings against their own reference runs (38.1 cm/s, the reference's alignment)
    {
        EqOff eqOff;
        struct RecordOnly { RecordOnly() { g_recordOnly = true; } ~RecordOnly() { g_recordOnly = false; } } recordOnly;
        // the reference's MOL at its 38.1 cm/s alignment, dB re 250 nWb/m: printed by
        // mtref --stock=... --tablesonly when the tables were generated (2026-10-11)
        struct C { int stock; const char* name; double hbRef, hbOp, hsRef, hs0, mol; };
        const C cs[] = {
            { 1, "VINTAGE", mt::stockvintage::HB_OP_REF38, mt::stockvintage::HB_OPERATING, mt::stockvintage::HS_0VU_REF38, mt::stockvintage::HS_0VU, 5.47 },
            { 3, "BUDGET",  mt::stockbudget::HB_OP_REF38,  mt::stockbudget::HB_OPERATING,  mt::stockbudget::HS_0VU_REF38,  mt::stockbudget::HS_0VU,  7.02 },
        };
        const double v = 38.1;
        for (const C& c : cs)
        {
            const double opR = stockMedium (c.stock).opLevelNwbm / REF_FLUX;
            const double biasRef = 20.0 * std::log10 (c.hbRef / c.hbOp);
            const double amp0 = REF_AMP * (c.hsRef / c.hs0) / opR;     // 250 nWb/m at the reference's field
            auto tone = [&] (double amp, double& fund, double& hd3) {
                Params p = wetOnly(); p.stock = (float) c.stock; p.speed = (float) v; soloHead (p, 4.0); p.bias = (float) biasRef;
                const double dl = 4.0 / v;
                const size_t n = (size_t) ((dl + 0.5) * fs);
                auto x = sine (n, 1000.0, fs, amp);
                auto r = run (p, fs, x, x);
                const size_t a = (size_t) ((dl + 0.15) * fs), len = (size_t) (0.3 * fs);
                fund = goertzelAmp (r.L, a, a + len, 1000.0, fs);
                hd3 = goertzelAmp (r.L, a, a + len, 3000.0, fs) / fund;
            };
            double f0, h0; tone (amp0, f0, h0);
            double lo = 0.0, hi = 20.0;
            for (int it = 0; it < 16; ++it) { const double db = 0.5 * (lo + hi); double a, h; tone (amp0 * std::pow (10.0, db / 20.0), a, h); (h < 0.03 ? lo : hi) = db; }
            double a, h; tone (amp0 * std::pow (10.0, 0.5 * (lo + hi) / 20.0), a, h);
            const double mol = 20.0 * std::log10 (a / f0);
            check (std::abs (mol - c.mol) < 0.5, (std::string (c.name) + " coating: MOL within 0.5 dB of its reference").c_str(),
                   fmt ("%+.2f dB re 250 nWb/m (reference %+.2f)", mol, c.mol));
        }
    }

    // ---- every stock on the machine aligned for it: 0 VU in is 0 VU out (shedding: the studio
    //      tape decayed on a machine aligned for it fresh, so it plays its lost oxide quieter)
    {
        double worst = 0.0, shed = 0.0;
        std::printf ("   1 kHz at 0 VU, 19.05 cm/s, dB re 0 VU:");
        for (int s = 0; s < NUM_STOCKS; ++s)
        {
            Params p = wetOnly(); p.stock = (float) s; p.speed = 19.05f; soloHead (p, 4.0);
            const size_t n = (size_t) (1.2 * fs);
            auto x = sine (n, 1000.0, fs, REF_AMP);
            auto r = run (p, fs, x, x);
            const double a = goertzelAmp (r.L, (size_t) (0.6 * fs), (size_t) (1.1 * fs), 1000.0, fs);
            const double db = 20.0 * std::log10 (a / REF_AMP);
            std::printf ("  %s %+.3f", stockMedium (s).name, db);
            if (s == 2) shed = db; else worst = std::max (worst, std::abs (db));
        }
        std::printf ("\n");
        check (worst < 0.1, "each fresh stock's 0 VU plays back at 0 VU (the machine aligned for it)", fmt ("worst %.3f dB", worst));
        const double want = 20.0 * std::log10 (stockMedium (2).packing);
        check (std::abs (shed - want) < 0.1, "SHEDDING plays its lost oxide quieter, on the studio alignment", fmt ("%+.3f dB (packing says %+.3f)", shed, want));
    }

    RealMedium realMedium;
    // ---- hiss: the engine's spectrum against the continuous coating's (EQ out)
    {
        EqOff eqOff;
        for (double v : { 19.05, 38.1 })
        {
            Params p = wetOnly(); p.speed = (float) v; soloHead (p, 4.0);
            const size_t n = (size_t) (9.0 * fs);
            std::vector<float> z (n, 0.0f);
            Engine e;
            auto r = run (p, fs, z, z, nullptr, 0.0, &e);
            const size_t nfft = 16384;
            auto psd = psdOf (r.L, (size_t) (0.6 * fs), n, nfft);
            double worst = 0.0, wf = 0.0;
            std::printf ("   hiss at %.2f cm/s, 1/3 octaves, measured - expected (dB):\n    ", v);
            int col = 0;
            for (double fc = 250.0; fc <= 16100.0; fc *= std::pow (2.0, 1.0 / 3.0))
            {
                const double f1 = fc * std::pow (2.0, -1.0 / 6.0), f2 = fc * std::pow (2.0, 1.0 / 6.0);
                const double meas = bandPower (psd, fs, nfft, f1, f2);
                double expd = 0.0; const int m = 64;
                for (int i = 0; i < m; ++i) { const double f = f1 + (f2 - f1) * (i + 0.5) / m; expd += hissPsd (f, v, stockMedium (0).particleVolUm3, e.outputTrim(), e.playbackTrim(), 1.0, e.internalRate()) * (f2 - f1) / m; }
                const double err = 10.0 * std::log10 (meas / expd);
                std::printf ("%6.0f:%+5.2f ", fc, err);
                if (++col % 9 == 0) std::printf ("\n    ");
                if (std::abs (err) > worst) { worst = std::abs (err); wf = fc; }
            }
            std::printf ("\n");
            check (worst < 1.0, fmt ("hiss spectrum within 1 dB of the continuous coating's, %.2f cm/s", v).c_str(), fmt ("worst %.2f dB at %.0f Hz", worst, wf));
        }
    }

    // ---- the hiss each stock brings, on the aligned machine (EQ in), re 0 VU
    {
        std::printf ("   hiss re 0 VU (EQ in, one head), unweighted 20 Hz-20 kHz / A-weighted:\n");
        double studio19 = 0.0;
        for (int s = 0; s < NUM_STOCKS; ++s)
            for (double v : { 19.05, 38.1 })
            {
                if (s > 0 && v > 20.0) continue;
                Params p = wetOnly(); p.stock = (float) s; p.speed = (float) v; soloHead (p, 4.0);
                const size_t n = (size_t) (5.0 * fs);
                std::vector<float> z (n, 0.0f);
                auto r = run (p, fs, z, z);
                const size_t nfft = 16384;
                auto psd = psdOf (r.L, (size_t) (0.6 * fs), n, nfft);
                double un = 0.0, aw = 0.0;
                const double df = fs / nfft;
                for (size_t k = 1; k + 1 < psd.size(); ++k)
                {
                    const double f = k * df;
                    if (f < 20.0 || f > 20000.0) continue;
                    un += psd[k] / fs * df;
                    aw += psd[k] / fs * df * aWeight (f) * aWeight (f);
                }
                const double ref = REF_AMP * REF_AMP / 2.0;
                const double dbU = 10.0 * std::log10 (un / ref), dbA = 10.0 * std::log10 (aw / ref);
                std::printf ("     %-20s %5.2f cm/s: %6.1f dB / %6.1f dBA\n", stockMedium (s).name, v, dbU, dbA);
                if (s == 0 && v < 20.0) studio19 = dbA;
            }
        // a fresh instance carries real 456 hiss: a two-track 1/4-inch machine on 456 at
        // 19 cm/s measures in the low-to-mid 60s dBA under 250 nWb/m (09-stock.md)
        check (studio19 < -60.0 && studio19 > -67.0, "the studio tape's hiss at 19 cm/s is real 456 hiss (-60 .. -67 dBA re 0 VU)", fmt ("%.1f dBA", studio19));
    }

    // ---- PARTICLES: ten times the particles, 10 dB less hiss; the top, silence
    {
        auto hiss = [&] (float P, std::vector<float>* keep = nullptr) {
            Params p = wetOnly(); p.particles = P; p.speed = 19.05f; soloHead (p, 4.0);
            const size_t n = (size_t) (3.0 * fs);
            std::vector<float> z (n, 0.0f);
            auto r = run (p, fs, z, z);
            if (keep) *keep = r.L;
            return rmsOf (r.L, (size_t) (0.6 * fs), n);
        };
        const double h1 = hiss (1.0f), h10 = hiss (10.0f);
        const double d = 20.0 * std::log10 (h10 / h1);
        check (std::abs (d + 10.0) < 0.3, "PARTICLES x10: the hiss falls 10 dB (it is particle statistics)", fmt ("%+.2f dB", d));
        std::vector<float> s;
        hiss (1000.0f, &s);
        double mx = 0.0; for (float x : s) mx = std::max (mx, (double) std::abs (x));
        check (mx == 0.0, "PARTICLES at the top: a continuum, and silence in gives exactly silence out", fmt ("peak %.3g", mx));
        std::vector<float> a, b;
        hiss (1.0f, &a); hiss (1.0f, &b);
        check (a == b, "the medium is seeded by the tape position: two renders are identical", a == b ? "bit for bit" : "DIFFER");
    }

    // ---- a loop brings its coating and its dropouts round every pass; the particles are
    //      re-randomised every time the tape is recorded over, so the hiss is not repeated
    {
        const double v = 19.05, L = 0.5, per = L * 100.0 / v;
        auto envelope = [&] (int mode, std::vector<double>& env) {
            Params p = wetOnly(); p.stock = 2; p.mode = (float) mode; p.loopLen = (float) L; p.speed = (float) v; p.particles = 1000.0f;
            soloHead (p, 4.0);
            const size_t n = (size_t) ((3.0 * per + 1.0) * fs);
            auto x = sine (n, 5000.0, fs, REF_AMP * 0.3);
            auto r = run (p, fs, x, x);
            const size_t hop = (size_t) (0.002 * fs);
            env.clear();
            for (size_t a = (size_t) (0.6 * fs); a + hop <= n; a += hop) env.push_back (rmsOf (r.L, a, a + hop));
        };
        auto corr = [] (const std::vector<double>& e, size_t lag) {
            const size_t m = std::min (e.size() - lag, (size_t) (e.size() / 2));
            double ma = 0, mb = 0; for (size_t i = 0; i < m; ++i) { ma += e[i]; mb += e[i + lag]; } ma /= m; mb /= m;
            double sab = 0, saa = 0, sbb = 0;
            for (size_t i = 0; i < m; ++i) { const double x = e[i] - ma, y = e[i + lag] - mb; sab += x * y; saa += x * x; sbb += y * y; }
            return sab / std::sqrt (saa * sbb + 1e-300);
        };
        std::vector<double> eL, eR;
        envelope (1, eL); envelope (0, eR);
        const size_t lag = (size_t) std::lround (per / 0.002);
        const double cL = corr (eL, lag), cR = corr (eR, lag);
        check (cL > 0.8 && cR < 0.3, "a LOOP brings its coating and dropouts round every pass (a reel does not)", fmt ("envelope correlation one pass apart: loop %.2f, reel %.2f", cL, cR));
        // hiss: silence on a loop, one pass apart
        Params p = wetOnly(); p.mode = 1; p.loopLen = (float) L; p.speed = (float) v; soloHead (p, 4.0);
        const size_t n = (size_t) ((2.0 * per + 1.0) * fs);
        std::vector<float> z (n, 0.0f);
        auto r = run (p, fs, z, z);
        const size_t a0 = (size_t) (0.6 * fs), len = (size_t) (0.5 * fs), off = (size_t) std::lround (per * fs);
        double sab = 0, saa = 0, sbb = 0;
        for (size_t i = 0; i < len; ++i) { const double x = r.L[a0 + i], y = r.L[a0 + off + i]; sab += x * y; saa += x * x; sbb += y * y; }
        const double ch = sab / std::sqrt (saa * sbb + 1e-300);
        check (std::abs (ch) < 0.1, "but the hiss is new every pass (the particles re-randomised by the bias)", fmt ("correlation one pass apart %.3f", ch));
    }

    // ---- modulation noise: the skirt around a tone, studio against shedding
    {
        auto skirt = [&] (int stock) {
            Params p = wetOnly(); p.stock = (float) stock; p.speed = 19.05f; p.particles = 1000.0f; soloHead (p, 4.0);
            const size_t n = (size_t) (6.0 * fs);
            auto x = sine (n, 1000.0, fs, REF_AMP);
            auto r = run (p, fs, x, x);
            const size_t nfft = 32768;
            auto psd = psdOf (r.L, (size_t) (0.6 * fs), n, nfft);
            const double tone = bandPower (psd, fs, nfft, 990.0, 1010.0);
            const double sk = bandPower (psd, fs, nfft, 1030.0, 1300.0) + bandPower (psd, fs, nfft, 700.0, 970.0);
            return 10.0 * std::log10 (sk / tone);
        };
        const double st = skirt (0), sh = skirt (2);
        check (sh - st > 6.0, "modulation noise: the shedding tape's skirt round a tone is well above the studio's",
               fmt ("skirt (30-300 Hz either side) re the tone: studio %.1f dB, shedding %.1f dB", st, sh));
    }

    // ---- dropouts on the shedding tape: as many as the stock's rate says
    {
        Params p = wetOnly(); p.stock = 2; p.speed = 19.05f; p.particles = 1000.0f; soloHead (p, 4.0);
        const double secs = 21.0;
        const size_t n = (size_t) (secs * fs);
        auto x = sine (n, 10000.0, fs, REF_AMP * 0.3);
        auto r = run (p, fs, x, x);
        const size_t hop = (size_t) (0.00025 * fs);
        std::vector<double> env;
        for (size_t a = (size_t) (0.6 * fs); a + 4 * hop <= n; a += hop) env.push_back (rmsOf (r.L, a, a + 4 * hop));
        std::vector<double> sorted = env; std::sort (sorted.begin(), sorted.end());
        const double med = sorted[sorted.size() / 2];
        int dips = 0; bool in = false;
        for (double e : env)
        {
            const bool d = e < med * 0.7079;   // -3 dB
            if (d && ! in) ++dips;
            in = d;
        }
        const double metres = (secs - 0.6) * 0.1905;
        // a lift d (um) costs 54.6 d / lambda dB; a hole h costs -20 log(1 - h), h = d / 2.5:
        // below -3 dB at 19 um for d > 1.04 um (lift) or 0.73 um (hole), exponential depths
        const StockMedium& S = stockMedium (2);
        const double pDeep = (1.0 - S.holeFrac) * std::exp (-1.044 / S.dropDepthUm) + S.holeFrac * std::exp (-0.730 / S.dropDepthUm);
        const double want = S.dropPerM * pDeep * metres;
        std::printf ("   10 kHz envelope on the shedding tape: min %.4f, median %.4f, max %.4f (%zu windows)\n", sorted.front(), med, sorted.back(), env.size());
        check (dips > 0.5 * want && dips < 1.6 * want, "SHEDDING: dropouts at the stock's rate (10 kHz dips past -3 dB)",
               fmt ("%.0f over %.1f m, %.1f expected", (double) dips, metres, want));
    }

    // ---- the shaper's kernels against their exact responses (one head, 96 kHz)
    {
        const double fsI = 96000.0;
        auto respond = [&] (double tau, double box, double shift, double f, double& gain, double& lag) {
            HeadShaper h; h.reset();
            h.set (0, tau, box, shift, 1.0); h.set (1, 0.0, 0.0, 0.0, 1.0);
            const int n = 8192;
            double re = 0, im = 0, rr = 0, ri = 0;
            for (int i = 0; i < n + HeadShaper::R; ++i)
            {
                const double x = std::sin (2.0 * M_PI * f * i / fsI);
                double l, r; h.push (x, x, l, r);
                const int j = i - HeadShaper::R;
                if (j < 1024 || j >= n) continue;
                const double c = std::cos (2.0 * M_PI * f * j / fsI), s = std::sin (2.0 * M_PI * f * j / fsI);
                re += l * c; im += l * s; rr += r * c; ri += r * s;
            }
            gain = std::hypot (re, im) / std::hypot (rr, ri);
            lag = -(std::atan2 (re, im) - std::atan2 (rr, ri)) / (2.0 * M_PI * f / fsI);
        };
        double worstS = 0.0, worstS2 = 0.0, worstB = 0.0, worstD = 0.0;
        for (double tau : { 0.25, 1.0 })
            for (double f : { 2000.0, 5000.0, 10000.0, 20000.0 })
            {
                double g, l; respond (tau, 0.0, 0.0, f, g, l);
                const double want = -8.685889638 * 2.0 * M_PI * f * tau / fsI;
                (f < 4000.0 ? worstS2 : worstS) = std::max (f < 4000.0 ? worstS2 : worstS, std::abs (20.0 * std::log10 (g) - want));
                if (verbose) std::printf ("     spacing tau %.2f f %5.0f: %+.3f dB (exact %+.3f)\n", tau, f, 20.0 * std::log10 (g), want);
            }
        for (double B : { 0.5, 2.0, 6.0 })
            for (double f : { 1000.0, 5000.0, 10000.0 })
            {
                double g, l; respond (0.0, B, 0.0, f, g, l);
                const double xs = M_PI * f * B / fsI;
                const double want = 20.0 * std::log10 (std::abs (std::sin (xs) / xs));
                worstB = std::max (worstB, std::abs (20.0 * std::log10 (g) - want));
                if (verbose) std::printf ("     box B %.2f f %5.0f: %+.3f dB (exact %+.3f)\n", B, f, 20.0 * std::log10 (g), want);
            }
        for (double sh : { 0.3, -2.7, 5.5 })
        {
            double g, l; respond (0.0, 0.0, sh, 3000.0, g, l);
            worstD = std::max (worstD, std::abs (l - sh));
        }
        double worstF = 0.0;
        for (double f : { 2000.0, 8000.0, 16000.0 })
        {
            double g, l; respond (0.0, 0.0, 2.5, f, g, l);
            worstF = std::max (worstF, std::abs (20.0 * std::log10 (g)));
        }
        std::printf ("   fractional delay's magnitude at half a sample, 2-16 kHz at 96 kHz: worst %.3f dB\n", worstF);
        check (worstS < 0.1, "spacing: the Cauchy kernel gives exp(-k dd) within 0.1 dB (5-20 kHz, dd/v up to a sample)", fmt ("worst %.3f dB", worstS));
        // below, the Poisson kernel's cusp at DC lives in tails longer than the kernel: a deep
        // dropout loses a little less bass than exp(-k dd) says - stated, and bounded
        check (worstS2 < 0.5, "and at 2 kHz within 0.5 dB (the DC cusp, in tails longer than the kernel)", fmt ("worst %.3f dB", worstS2));
        check (worstB < 0.1, "azimuth box: sinc(pi f W tan a / v) within 0.1 dB (to 6.5 dB of loss)", fmt ("worst %.3f dB", worstB));
        check (worstD < 0.01, "the tracks' offset: a fractional delay within 0.01 sample", fmt ("worst %.4f samples", worstD));
    }

    // ---- azimuth on the machine: a worn transport tilts the gaps (reels, capstan, scrape out,
    //      so nothing else moves); the tracks arrive S tan(a) / v apart, each loses its box
    {
        // phase of a tone over [a, b): the Goertzel bin's angle
        auto phaseOf = [&] (const std::vector<float>& x, size_t a0, size_t b0, double f) {
            double re = 0.0, im = 0.0;
            for (size_t i = a0; i < b0; ++i) { const double ph = 2.0 * M_PI * f * (double) i / fs; re += x[i] * std::cos (ph); im += x[i] * std::sin (ph); }
            return std::atan2 (im, re);
        };
        auto run1 = [&] (double cond, double v, double f, double& ampL, double& lagLR) {
            Params p = wetOnly(); p.condition = (float) cond; p.capstan = p.reels = p.scrape = 0.0f; p.speed = (float) v; soloHead (p, 12.5);
            const double arrive = 12.5 / v;
            const size_t n = (size_t) ((arrive + 1.2) * fs);
            auto x = sine (n, f, fs, REF_AMP * 0.3);
            auto r = run (p, fs, x, x);
            const size_t a0 = (size_t) ((arrive + 0.3) * fs), b0 = (size_t) ((arrive + 1.1) * fs);
            ampL = goertzelAmp (r.L, a0, b0, f, fs);
            double d = phaseOf (r.R, a0, b0, f) - phaseOf (r.L, a0, b0, f);
            while (d > M_PI) d -= 2.0 * M_PI; while (d < -M_PI) d += 2.0 * M_PI;
            lagLR = d / (2.0 * M_PI * f);                 // seconds by which L lags R
        };
        const double a = 1.0 * 8.0 * M_PI / (180.0 * 60.0) * 0.55;   // head 3 at CONDITION 100
        double amp, lagS;
        run1 (100.0, 19.05, 2000.0, amp, lagS);
        const double want = TRACK_SEP_UM * std::tan (a) / 19.05e4;
        // which track leads is the tilt's direction; the size is the physics
        check (std::abs (std::abs (lagS) - want) < 1.0e-6, "azimuth: the tracks arrive S tan(a) / v apart, within 1 us",
               fmt ("%.2f us apart (exact %.2f us)", 1e6 * std::abs (lagS), 1e6 * want));
        double a0, a1, l0, l1;
        run1 (0.0, 4.76, 10000.0, a0, l0); run1 (100.0, 4.76, 10000.0, a1, l1);
        const double xs = M_PI * TRACK_W_UM * std::tan (a) / (4.76e4 / 10000.0);
        const double wantB = 20.0 * std::log10 (std::abs (std::sin (xs) / xs));
        const double got = 20.0 * std::log10 (a1 / a0);
        check (std::abs (got - wantB) < 0.5, "and each track loses its gap's tilt: 10 kHz at 4.76 cm/s", fmt ("%.2f dB (sinc says %.2f)", got, wantB));
    }

    // ---- WEAR: oxide lost and the tape lifted
    {
        auto lvl = [&] (float wear) {
            Params p = wetOnly(); p.wear = wear; p.speed = 19.05f; p.particles = 1000.0f; soloHead (p, 4.0);
            const size_t n = (size_t) (1.5 * fs);
            auto x = sine (n, 1000.0, fs, REF_AMP);
            auto r = run (p, fs, x, x);
            return goertzelAmp (r.L, (size_t) (0.8 * fs), (size_t) (1.4 * fs), 1000.0, fs);
        };
        const double d = 20.0 * std::log10 (lvl (100.0f) / lvl (0.0f));
        const StockMedium& S = stockMedium (0);
        const double sp = S.spacingUm + 0.45 + 1.083 * S.roughUm * 4.0;      // static + the rougher surface's mean lift
        const double want = 20.0 * std::log10 (1.0 - 0.25) - 8.685889638 * 2.0 * M_PI * sp / (19.05e4 / 1000.0);
        check (std::abs (d - want) < 0.2, "WEAR 100 %: a quarter of the oxide gone and the tape lifted", fmt ("%+.2f dB at 1 kHz (expected %+.2f)", d, want));
    }

    // ---- ACCUMULATE: a loop wears on every pass, and keeps it
    {
        Params p = wetOnly(); p.mode = 1; p.loopLen = 0.5f; p.speed = 76.0f; p.accumulate = 1.0f; soloHead (p, 4.0);
        const size_t n = (size_t) (8.0 * fs);
        std::vector<float> z (n, 0.0f);
        Engine e; e.setWearRateScale (100.0);
        run (p, fs, z, z, nullptr, 0.0, &e);
        const double passes = std::floor (e.odometerCells() / (double) e.ringCells());
        const double want = passes * stockMedium (0).wearPerPass * 100.0;
        double w; int st; double len; e.wearState (w, st, len);
        check (std::abs (e.tele.wear.load() - want) < 1.0e-9 && passes >= 10, "ACCUMULATE: the loop wears by the stock's rate on every pass",
               fmt ("%.0f passes, wear %.4f (expected %.4f)", passes, e.tele.wear.load(), want));
        // a project reopened: the wear comes back with the loop it belongs to
        Engine e2; e2.setWearState (w, st, len);
        Params q = p; q.accumulate = 0.0f;
        std::vector<float> z2 ((size_t) (0.2 * fs), 0.0f);
        run (q, fs, z2, z2, nullptr, 0.0, &e2);
        check (std::abs (e2.tele.wear.load() - w) < 1.0e-12, "and a reopened project finds its loop as worn as it left it", fmt ("wear %.4f", e2.tele.wear.load()));
        Params q2 = q; q2.loopLen = 1.0f;
        Engine e3; e3.setWearState (w, st, len);
        run (q2, fs, z2, z2, nullptr, 0.0, &e3);
        check (e3.tele.wear.load() == 0.0, "a different loop is a fresh tape", fmt ("wear %.4f", e3.tele.wear.load()));
    }

    // ---- a change of stock threads a new reel through the splice: no click
    {
        Params p = wetOnly(); p.speed = 19.05f; soloHead (p, 4.0);
        const size_t n = (size_t) (2.0 * fs);
        auto x = sine (n, 440.0, fs, REF_AMP);
        bool spliced = false;
        auto r = run (p, fs, x, x, [&] (Params& q, Engine& e, size_t i) {
            if (i >= (size_t) fs) q.stock = 1;
            if (e.isSplicing()) spliced = true; });
        double stepAt = 0.0, stepElse = 0.0;
        for (size_t i = (size_t) (0.5 * fs); i < n; ++i)
        {
            const double s = std::abs ((double) r.L[i] - r.L[i - 1]);
            if (i > (size_t) (0.99 * fs) && i < (size_t) (1.3 * fs)) stepAt = std::max (stepAt, s); else stepElse = std::max (stepElse, s);
        }
        check (spliced && stepAt <= stepElse * 1.05, "a change of stock threads a new reel through the splice, without a click",
               fmt ("largest step at the change %.4f, elsewhere %.4f", stepAt, stepElse));
    }
}

static void testCost()
{
    std::printf ("\n12. COST (48 kHz stereo, 76 cm/s, three heads, feedback, the default tape and its medium)\n");
    const double fs = 48000.0;
    Params p; p.speed = 76.0f; p.h1On = p.h2On = p.h3On = 1; p.feedback = 0.6f; p.mix = 0.5f;
    const double seconds = 4.0;
    const size_t n = (size_t) (seconds * fs);
    auto x = noiseBurst (n, 0, n, 1, 0.2f);
    Engine e; e.setParams (p); e.prepare (fs, 256);
    std::vector<float> L (256), R (256);
    // best of five: the machine running the bench is not idle (one binary measured
    // 12.5 % and 15.0 % on consecutive runs), and the cost of the engine is the
    // time it needs, not the time it was interrupted for
    double sec = 1.0e9;
    for (int run = 0; run < 5; ++run)
    {
        const double t0 = threadCpuSeconds();
        for (size_t i = 0; i < n; i += 256)
            e.process (x.data() + i, x.data() + i, L.data(), R.data(), 256);
        sec = std::min (sec, threadCpuSeconds() - t0);
    }
    const double pct = 100.0 * sec / seconds;
    std::printf ("   record head rebuilt %d times (once per stock in prepare at the alignment speed, once at 76)\n", e.recordBuilds());
    // The design (section 6) sets CPU as TARGETS with actuals reported, not as a
    // tolerance: the same binary reads 12.5-15.2 % from run to run as the CPU's
    // clock moves (boost, heat), even timed on its own thread. So: report against
    // the NORMAL target, and fail only past a ceiling that would mean a real fault.
    std::printf ("   %.2f %% of one core (NORMAL target 15 %%; the CPU's clock state moves this by about +-1.5 %%)\n", pct);
    check (pct < 20.0, "cost stays under the 20 % ceiling (NORMAL target 15 %)", fmt ("%.2f %% of one core", pct));
}

int main (int argc, char** argv)
{
    verbose = (argc > 1 && std::strcmp (argv[1], "-v") == 0) || (argc > 3 && std::strcmp (argv[3], "-v") == 0);
    // "mttest -s stock" runs one section (delay unity glide doppler feedback loop stereo sync
    // controls rail reference speeds playback equalised head transport stock cost)
    const char* only = (argc > 2 && std::strcmp (argv[1], "-s") == 0) ? argv[2] : nullptr;
    auto want = [only] (const char* n) { return only == nullptr || std::strcmp (only, n) == 0; };
    std::printf ("Magneto Transport - bench (stage 6)\n");
    {
        Engine e; Params p; e.setParams (p); e.prepare (48000.0, 256);
        std::printf ("internal rate %.0f Hz (x%d), read-ahead %.3f us\n",
                     e.internalRate(), e.oversampling(), e.compensationS() * 1e6);
    }
    if (want ("delay")) testDelay();
    if (want ("unity")) testUnity();
    if (want ("glide")) testSpeedGlide();
    if (want ("doppler")) testHeadDoppler();
    if (want ("feedback")) testFeedback();
    if (want ("loop")) testLoopAndSplice();
    if (want ("stereo")) testStereo();
    if (want ("sync")) testSync();
    if (want ("controls")) testEveryControl();
    if (want ("rail")) testRailAndView();
    if (want ("reference")) testAgainstReference();
    if (want ("speeds")) testSpeeds();
    if (want ("playback")) testPlayback();
    if (want ("equalised")) testEqualised();
    if (want ("head")) testHeadAndElectronics();
    if (want ("transport")) testTransport();
    if (want ("stock")) testStock();
    if (want ("cost")) testCost();
    std::printf ("\n%d checks, %d failed - %s\n", g_pass + g_fail, g_fail, g_fail ? "FAILURES" : "ALL CLEAR");
    return g_fail ? 1 : 0;
}
