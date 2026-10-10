// Magneto Transport - stage 1 bench.
//
// Measures every claim stage 1 makes. The tape is a medium, so delay, pitch
// bends and Doppler are consequences of where the heads are and how fast the
// tape moves; each is checked against the closed-form physics it must obey.

#include "Engine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace mt;

static int g_fail = 0, g_pass = 0;
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
                // A Gaussian pulse and the centroid of its echo's energy. Noise
                // correlated with a parabola through the peak scattered +/-0.1
                // sample with random sign (2.4 us at 44.1 kHz) - the probe, not
                // the tape. The chain is linear-phase, so a symmetric pulse's
                // centroid moves by exactly the delay.
                const double t0 = 0.05, sig = 0.0002;
                const size_t total = (size_t) ((t0 + expect + 0.05) * fs);
                std::vector<float> x (total, 0.0f);
                double cin = 0.0, ein = 0.0;
                for (size_t i = 0; i < total; ++i)
                {
                    const double t = (double) i / fs - t0;
                    x[i] = (float) (0.4 * std::exp (-0.5 * t * t / (sig * sig)));
                    cin += (double) x[i] * x[i] * i; ein += (double) x[i] * x[i];
                }
                auto r = run (p, fs, x, x);
                const size_t a = (size_t) ((t0 + expect - 0.004) * fs), b = (size_t) ((t0 + expect + 0.004) * fs);
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
    std::printf ("   response of the medium, -20 dB re ref (48 kHz):\n");
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
    check (worst19 < 0.25, "19 and 76 cm/s flat within 0.25 dB to 16 kHz", fmt ("worst %.3f dB", worst19));
}

static void testSpeedGlide()
{
    std::printf ("\n3. A MOTOR GLIDE BENDS THE ECHOES ALREADY ON THE TAPE\n");
    // Record 1 kHz at 4.75 cm/s with head 3 at 24 cm, then ask for 9.5 cm/s.
    // Material read 1.9-2.2 s after the change was recorded BEFORE it, so it
    // plays at f0 * v(t) / 4.75, with v(t) the motor's first-order glide.
    const double fs = 48000.0, f0 = 1000.0, ts = 6.0;
    Params p = wetOnly(); p.speed = 4.75f; soloHead (p, 24.0);
    const size_t n = (size_t) ((ts + 2.4) * fs);
    auto x = sine (n, f0, fs, REF_AMP);
    auto r = run (p, fs, x, x, [&] (Params& q, Engine&, size_t i) {
        if ((double) i >= ts * fs) q.speed = 9.5f; });
    const size_t ts0 = ((size_t) (ts * fs) / 256 + 1) * 256;   // the block the change lands in
    const double tsReal = (double) ts0 / fs;
    auto vAt = [&] (double t) { return 9.5 + (4.75 - 9.5) * std::exp (-(t - tsReal) / MOTOR_TAU); };
    const double t1 = tsReal + 1.9, t2 = tsReal + 2.2;
    double vMean = 0.0; for (int k = 0; k < 100; ++k) vMean += vAt (t1 + (t2 - t1) * (k + 0.5) / 100); vMean /= 100;
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
        auto rb = run (q, fs, xb, xb, [&] (Params& pp, Engine&, size_t i) {
            if ((double) i >= tsw * fs) pp.speed = 9.5f; });
        const double tsr = (double) ((((size_t) (tsw * fs)) / 256 + 1) * 256) / fs;
        auto dist = [&] (double ta, double tb) {   // cm travelled in [ta, tb] (both after tsr)
            auto F = [&] (double t) { return 9.5 * t + (4.75 - 9.5) * (-MOTOR_TAU) * std::exp (-(t - tsr) / MOTOR_TAU); };
            return F (tb) - F (ta); };
        const double tc = tPulse;
        double lo = tc, hi = tc + 4.0;
        for (int it = 0; it < 80; ++it) { const double m = 0.5 * (lo + hi); (dist (tc, m) < 12.0 ? lo : hi) = m; }
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
    std::printf ("\n5. FEEDBACK: STOP THE INPUT AND WATCH (stage 1 loop gain is capped below 1)\n");
    const double fs = 48000.0;
    Params p; p.mix = 1.0f; p.feedback = 1.0f;
    p.h1On = p.h2On = p.h3On = 1; p.h1Lvl = p.h2Lvl = p.h3Lvl = 6.0f;
    p.bass = 12.0f; p.treble = 12.0f; p.recLevel = 12.0f;
    p.speed = 76.0f; p.h1Pos = 1.5f; p.h2Pos = 3.0f; p.h3Pos = 4.5f;   // short loop, many trips
    const size_t n = (size_t) (40.0 * fs);
    auto x = noiseBurst (n, (size_t) (0.1 * fs), (size_t) (0.5 * fs), 3, 0.5f);
    auto r = run (p, fs, x, x);
    bool finite = true; for (float s : r.L) if (! std::isfinite (s)) { finite = false; break; }
    check (finite, "no NaN or inf in 40 s at maximum everything", finite ? "finite" : "NOT FINITE");
    const double e1 = rmsOf (r.L, (size_t) (2 * fs), (size_t) (4 * fs));
    const double e2 = rmsOf (r.L, (size_t) (20 * fs), (size_t) (22 * fs));
    const double e3 = rmsOf (r.L, (size_t) (37 * fs), (size_t) (39 * fs));
    check (e2 < e1 && e3 < e2, "after the input stops the echoes DECAY",
           fmt ("rms %.2e -> %.2e -> %.2e", e1, e2, e3));

    // Silence in, silence out: exact zeros, not merely small.
    {
        Params q; q.mix = 1.0f; q.feedback = 0.8f; q.h1On = q.h2On = q.h3On = 1;
        std::vector<float> z ((size_t) (3 * fs), 0.0f);
        auto rz = run (q, fs, z, z);
        double m = 0.0; for (float s : rz.L) m = std::max (m, (double) std::abs (s));
        check (m == 0.0, "silence in gives exact zeros out", fmt ("max %.3e", m));
    }
}

static void testLoopAndSplice()
{
    std::printf ("\n6. LOOP MODE AND SPLICING\n");
    const double fs = 48000.0;
    // A 0.5 m loop at 76 cm/s comes round every 0.658 s. With the erase head at
    // full field nothing recorded may survive one trip.
    {
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
        auto x = sine (n, 500.0, fs, REF_AMP);
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
        const double sineStep = REF_AMP * 2.0 * M_PI * 500.0 / fs;
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
        double mr = 0.0; for (float s : r.R) mr = std::max (mr, (double) std::abs (s));
        check (mr == 0.0, "STEREO: left in, nothing on the right track", fmt ("right max %.2e", mr));
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
    std::printf ("\n8. SYNC\n");
    const double fs = 48000.0;
    {
        Params p = wetOnly(); p.sync = 7; p.syncHead = 0; p.h1Pos = 4.0f;     // 1/4 at 120 BPM = 0.5 s
        const size_t start = (size_t) (0.1 * fs), len = (size_t) (0.03 * fs);
        auto x = noiseBurst ((size_t) (1.0 * fs), start, len, 9, 0.3f);
        Engine e;
        auto r = run (p, fs, x, x, nullptr, 120.0, &e);
        const double lag = lagOf (x, r.L, (int) (0.5 * fs) - 40, (int) (0.5 * fs) + 40, start, len) / fs;
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
        // ON switches are tested by turning them off
        Params alt = base;
        float& m = alt.*(sp.member);
        if (sp.kind == Kind::Toggle) m = (m > 0.5f) ? 0.0f : 1.0f;
        else if (sp.kind == Kind::Choice) m = (m > 0.5f) ? 0.0f : 1.0f;
        else m = (std::abs (m - sp.hi) > std::abs (m - sp.lo)) ? sp.hi : sp.lo;
        if (id == "looplen") m = 0.5f;
        if (id == "sync") m = 4;
        if (id == "speed") m = 40.0f;
        auto a = run (base, fs, xl, xr, nullptr, bpm);
        auto b = run (alt, fs, xl, xr, nullptr, bpm);
        double d = 0.0, e = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            d += std::pow ((double) a.L[i] - b.L[i], 2) + std::pow ((double) a.R[i] - b.R[i], 2);
            e += (double) a.L[i] * a.L[i] + (double) a.R[i] * a.R[i];
        }
        const double rel = std::sqrt (d / std::max (e, 1e-30));
        // With the erase head at full field a loop sounds exactly like a reel:
        // the difference arrives with FLUX MEMORY (stage 7) and the reel
        // mechanics (stage 5). Inert BY DESIGN in stage 1 - asserted as such,
        // so this line fails the day they start doing something.
        if (id == "mode" || id == "looplen")
            check (rel < 1.0e-6, (std::string ("control ") + sp.name + " (inert in stage 1 by design)").c_str(),
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

static void testCost()
{
    std::printf ("\n12. COST (48 kHz stereo, 76 cm/s, three heads, feedback)\n");
    const double fs = 48000.0;
    Params p; p.speed = 76.0f; p.h1On = p.h2On = p.h3On = 1; p.feedback = 0.6f; p.mix = 0.5f;
    const size_t n = (size_t) (10.0 * fs);
    auto x = noiseBurst (n, 0, n, 1, 0.2f);
    Engine e; e.setParams (p); e.prepare (fs, 256);
    std::vector<float> L (256), R (256);
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < n; i += 256)
        e.process (x.data() + i, x.data() + i, L.data(), R.data(), 256);
    const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double pct = 100.0 * sec / 10.0;
    check (pct < 15.0, "stage 1 fits the NORMAL budget (< 15 % of a core)", fmt ("%.2f %% of one core", pct));
}

int main (int argc, char** argv)
{
    verbose = argc > 1 && std::strcmp (argv[1], "-v") == 0;
    std::printf ("Magneto Transport - stage 1 bench\n");
    {
        Engine e; Params p; e.setParams (p); e.prepare (48000.0, 256);
        std::printf ("internal rate %.0f Hz (x%d), read-ahead %.3f us\n",
                     e.internalRate(), e.oversampling(), e.compensationS() * 1e6);
    }
    testDelay();
    testUnity();
    testSpeedGlide();
    testHeadDoppler();
    testFeedback();
    testLoopAndSplice();
    testStereo();
    testSync();
    testEveryControl();
    testRailAndView();
    testCost();
    std::printf ("\n%d checks, %d failed - %s\n", g_pass + g_fail, g_fail, g_fail ? "FAILURES" : "ALL CLEAR");
    return g_fail ? 1 : 0;
}
