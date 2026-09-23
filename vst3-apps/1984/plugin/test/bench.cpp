/*  1984 — the bench. Every claim in the design is measured here; a check
    that cannot fail is worse than none, so each one is written against a
    number the physics predicts and a change that would break it.

        n84test            run everything
        n84test --cost     print the cost only
*/
#include "Engine.h"
#include "Patches.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <complex>
#include <vector>
#include <string>

using namespace n84;

static int checks = 0, fails = 0;
static void check (bool ok, const char* what, double a = 0, double b = 0)
{
    ++checks;
    if (! ok) ++fails;
    std::printf ("  [%s] %s", ok ? "ok" : "FAIL", what);
    if (a != 0 || b != 0) std::printf ("   (%.4g vs %.4g)", a, b);
    std::printf ("\n");
}

//==============================================================================
struct Rig
{
    Engine e;
    double fs;
    std::vector<float> L, R;
    Rig (double sampleRate = 48000.0, int os = 1) : fs (sampleRate)
    {
        for (int k = 0; k < numParams(); ++k) paramSpec (k).ref (e.p) = paramSpec (k).def;
        e.p.os = (float) os;
        // a clean bench voice: rank I saw, filter wide open, nothing after
        bare();
        e.prepare (fs, 256);
    }
    void bare()
    {
        Params& p = e.p;
        p.vintage = 0; p.hall_mix = 0; p.tape_mode = 0; p.ens_mode = 0; p.drv_mode = 0; p.choir_mix = 0;
        p.rk[0].lpf = 1.0f; p.rk[0].lpq = 0; p.rk[0].hpf = 0; p.rk[0].hpq = 0; p.rk[0].il = 0.5f; p.rk[0].al = 0.5f;
        p.rk[0].va = 0.0f; p.rk[0].vd = 0.5f; p.rk[0].vs = 1.0f; p.rk[0].vr = 0.0f; p.rk[0].vel = 0; p.rk[0].velb = 0;
        p.rk[1].lvl = 0; p.rk[1].lpf = 1.0f; p.rk[1].lpq = 0; p.rk[1].il = 0.5f; p.rk[1].al = 0.5f; p.rk[1].vel = 0; p.rk[1].velb = 0;
        p.rk[1].va = 0.0f; p.rk[1].vs = 1.0f; p.rk[1].vr = 0.0f;
        p.lfo_pitch = 0; p.lfo_vcf = 0; p.lfo_vca = 0; p.wheel_lfo = 0; p.at_brill = 0; p.at_lfo = 0;
        p.spread = 0; p.ktrack = 0; p.tape_hiss = 0;
    }
    void set (const char* id, float v) { const int i = paramIndex (id); if (i >= 0) paramSpec (i).ref (e.p) = v; else std::printf ("  ?? no param %s\n", id); }
    void hz (const char* id, float f)  { const int i = paramIndex (id); const auto& s = paramSpec (i); set (id, std::log (f / s.lo) / std::log (s.hi / s.lo)); }
    void ms (const char* id, float m)  { hz (id, m); }
    void render (int n)
    {
        L.assign ((size_t) n, 0.0f); R.assign ((size_t) n, 0.0f);
        for (int i = 0; i < n; i += 256) e.process (L.data() + i, R.data() + i, std::min (256, n - i));
    }
    void renderAppend (int n)
    {
        std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
        for (int i = 0; i < n; i += 256) e.process (l.data() + i, r.data() + i, std::min (256, n - i));
        L.insert (L.end(), l.begin(), l.end()); R.insert (R.end(), r.begin(), r.end());
    }
    float peak() const { float m = 0; for (float v : L) m = std::max (m, std::abs (v)); for (float v : R) m = std::max (m, std::abs (v)); return m; }
    float rms (int from = 0, int to = -1) const
    {
        if (to < 0) to = (int) L.size();
        double s = 0; int n = 0;
        for (int i = from; i < to; ++i) { s += L[(size_t) i] * L[(size_t) i]; ++n; }
        return n ? (float) std::sqrt (s / n) : 0.0f;
    }
    bool finite() const { for (float v : L) if (bad (v)) return false; for (float v : R) if (bad (v)) return false; return true; }
};

static double goertzel (const std::vector<float>& x, int from, int n, double f, double fs)
{
    const double w = 2.0 * 3.14159265358979 * f / fs;
    const double c = 2.0 * std::cos (w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / (n - 1));
        s0 = x[(size_t) (from + i)] * win + c * s1 - s2; s2 = s1; s1 = s0;
    }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / n * 4.0;
}

// peak frequency near f0 by a fine Goertzel search (cents resolution)
static double peakNear (const std::vector<float>& x, int from, int n, double f0, double fs, double spanCents = 300, double stepCents = 2)
{
    double best = 0, bf = f0;
    for (double c = -spanCents; c <= spanCents; c += stepCents)
    {
        const double f = f0 * std::pow (2.0, c / 1200.0);
        const double m = goertzel (x, from, n, f, fs);
        if (m > best) { best = m; bf = f; }
    }
    return bf;
}
static double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

static void fft (std::vector<std::complex<double>>& a)
{
    const int n = (int) a.size();
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[(size_t) i], a[(size_t) j]);
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.14159265358979 / len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (int i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (int j = 0; j < len / 2; ++j)
            {
                const auto u = a[(size_t) (i + j)], v = a[(size_t) (i + j + len / 2)] * w;
                a[(size_t) (i + j)] = u + v; a[(size_t) (i + j + len / 2)] = u - v;
                w *= wl;
            }
        }
    }
}
static std::vector<double> spectrum (const std::vector<float>& x, int from, int N)
{
    std::vector<std::complex<double>> a ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / (N - 1));
        a[(size_t) i] = x[(size_t) (from + i)] * win;
    }
    fft (a);
    std::vector<double> m ((size_t) N / 2);
    for (int i = 0; i < N / 2; ++i) m[(size_t) i] = std::abs (a[(size_t) i]) / N * 4.0;
    return m;
}
static double db (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }

// decay time of an envelope, in seconds, from the dB slope between two levels
static double rt60 (const std::vector<float>& x, double fs, int from, double topDb, double botDb)
{
    const int hop = (int) (fs * 0.02);
    std::vector<double> env;
    for (int i = from; i + hop <= (int) x.size(); i += hop)
    {
        double s = 0; for (int j = 0; j < hop; ++j) s += x[(size_t) (i + j)] * x[(size_t) (i + j)];
        env.push_back (10.0 * std::log10 (std::max (1e-20, s / hop)));
    }
    double mx = -1e9; int im = 0;
    for (int i = 0; i < (int) env.size(); ++i) if (env[(size_t) i] > mx) { mx = env[(size_t) i]; im = i; }
    int a = -1, b = -1;
    for (int i = im; i < (int) env.size(); ++i) { if (a < 0 && env[(size_t) i] <= mx + topDb) a = i; if (env[(size_t) i] <= mx + botDb) { b = i; break; } }
    if (a < 0 || b < 0 || b <= a) return -1;
    const double slope = (botDb - topDb) / ((b - a) * hop / fs);   // dB per second (negative)
    return -60.0 / slope;
}

//==============================================================================
int main (int argc, char** argv)
{
    const bool costOnly = argc > 1 && std::strcmp (argv[1], "--cost") == 0;
    std::printf ("1984 bench — %d parameters, %d factory patches\n\n", numParams(), numPatches());

    if (! costOnly)
    {
    // ---------------------------------------------------------------- silence
    {
        Rig r; r.render (48000);
        check (r.peak() == 0.0f, "silence at rest is exactly zero", r.peak());
    }
    // ---------------------------------------------------------------- tuning
    {
        std::printf ("tuning\n");
        static const double feet[5] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
        for (int oct = 0; oct < 5; ++oct)
        {
            Rig r; r.set ("a_oct", (float) oct); r.e.noteOn (69, 0.8f); r.render (48000);
            const double f0 = 440.0 * feet[oct];
            const double f = peakNear (r.L, 24000, 16384, f0, r.fs);
            char buf[96]; std::snprintf (buf, sizeof buf, "rank I at A4, %s: within 1 cent", listNames ("a_oct", *new int)[oct]);
            check (std::abs (cents (f, f0)) < 1.0, buf, cents (f, f0), 0);
        }
        {
            Rig r; r.set ("a_lvl", 0); r.set ("b_lvl", 0.8f); r.set ("b_fine", 0.5f + 25.0f / 100.0f); r.e.noteOn (57, 0.8f); r.render (48000);
            const double f = peakNear (r.L, 24000, 16384, 220.0 * std::pow (2.0, 25.0 / 1200.0), r.fs);
            check (std::abs (cents (f, 220.0) - 25.0) < 1.0, "rank II at +25 cents: within 1 cent", cents (f, 220.0), 25.0);
        }
        for (double fs : { 44100.0, 96000.0 })
        {
            Rig r (fs, 1); r.e.noteOn (69, 0.8f); r.render ((int) fs);
            const double f = peakNear (r.L, (int) fs / 2, 16384, 440.0, fs);
            char buf[64]; std::snprintf (buf, sizeof buf, "tuning holds at %.0f Hz", fs);
            check (std::abs (cents (f, 440.0)) < 1.0, buf, cents (f, 440.0), 0);
        }
        {
            Rig r (48000, 2); r.e.noteOn (69, 0.8f); r.render (48000);
            const double f = peakNear (r.L, 24000, 16384, 440.0, r.fs);
            check (std::abs (cents (f, 440.0)) < 1.0, "tuning holds at 2x oversampling", cents (f, 440.0), 0);
        }
    }
    // ---------------------------------------------------------------- aliasing
    {
        std::printf ("aliasing\n");
        for (int os : { 1, 2 })
        {
            Rig r (48000, os); r.e.noteOn (93, 0.8f); r.render (48000);    // A6 = 1760 Hz saw
            const int N = 16384;
            auto sp = spectrum (r.L, 24000, N);
            const double f0 = 1760.0, binHz = r.fs / N;
            double fund = 0, worst = 0; double worstF = 0;
            for (int i = 8; i < N / 2 - 8; ++i)
            {
                const double f = i * binHz;
                const double k = f / f0;
                const bool harmonic = std::abs (k - std::round (k)) * f0 < 6.0 * binHz;
                if (i * binHz > 1700 && i * binHz < 1820) fund = std::max (fund, sp[(size_t) i]);
                if (! harmonic && f > 40.0 && f < 20000.0 && sp[(size_t) i] > worst) { worst = sp[(size_t) i]; worstF = f; }
            }
            char buf[96]; std::snprintf (buf, sizeof buf, "saw at A6, %dx: worst alias below 20 kHz under %s (at %.0f Hz)", os, os == 1 ? "-40 dB" : "-60 dB", worstF);
            check (db (worst / fund) < (os == 1 ? -40.0 : -60.0), buf, db (worst / fund), os == 1 ? -40.0 : -60.0);
        }
        {   // the pulse at 5 % width, the hardest wave
            Rig r (48000, 2); r.set ("a_saw", 0); r.set ("a_pulse", 1.0f); r.set ("a_pw", 1.0f); r.e.noteOn (93, 0.8f); r.render (48000);
            const int N = 16384; auto sp = spectrum (r.L, 24000, N); const double f0 = 1760.0, binHz = r.fs / N;
            double fund = 0, worst = 0;
            for (int i = 8; i < N / 2 - 8; ++i)
            {
                const double f = i * binHz, k = f / f0;
                const bool harmonic = std::abs (k - std::round (k)) * f0 < 6.0 * binHz;
                if (f > 1700 && f < 1820) fund = std::max (fund, sp[(size_t) i]);
                if (! harmonic && f > 40.0 && f < 20000.0) worst = std::max (worst, sp[(size_t) i]);
            }
            check (db (worst / fund) < -50.0, "narrow pulse at A6, 2x: worst alias below -50 dB", db (worst / fund), -50.0);
        }
    }
    // ---------------------------------------------------------------- filters
    {
        std::printf ("filters\n");
        for (int model = 0; model < 2; ++model)
        {
            Rig r; r.set ("a_saw", 0); r.set ("a_noise", 0.02f); r.set ("a_fmode", (float) model); r.set ("a_lpq", 1.0f); r.hz ("a_lpf", 1000.0f);
            r.e.noteOn (60, 0.8f); r.render (144000);
            const double f = peakNear (r.L, 96000, 32768, 1000.0, r.fs, 600, 2);
            char buf[96]; std::snprintf (buf, sizeof buf, "%s self-oscillates at its cutoff (1 kHz) within 2 %%", model ? "LADDER" : "SVF");
            check (std::abs (f / 1000.0 - 1.0) < 0.02, buf, f, 1000.0);
            check (r.peak() < 1.0f && r.rms (120000) > 0.01f, model ? "LADDER self-oscillation is bounded and audible" : "SVF self-oscillation is bounded and audible", r.peak(), r.rms (120000));
        }
        {   // the low-pass attenuates: saw through 300 Hz cutoff vs open
            Rig a; a.e.noteOn (57, 0.8f); a.render (48000);
            Rig b; b.hz ("a_lpf", 300.0f); b.e.noteOn (57, 0.8f); b.render (48000);
            const double ha = goertzel (a.L, 24000, 16384, 220.0 * 8, a.fs), hb = goertzel (b.L, 24000, 16384, 220.0 * 8, b.fs);
            check (db (hb / ha) < -20.0, "LPF at 300 Hz takes the 8th harmonic of A3 down more than 20 dB", db (hb / ha), -20.0);
        }
        {   // the high-pass
            Rig a; a.e.noteOn (57, 0.8f); a.render (48000);
            Rig b; b.hz ("a_hpf", 2000.0f); b.e.noteOn (57, 0.8f); b.render (48000);
            const double ha = goertzel (a.L, 24000, 16384, 220.0, a.fs), hb = goertzel (b.L, 24000, 16384, 220.0, b.fs);
            check (db (hb / ha) < -20.0, "HPF at 2 kHz takes the fundamental of A3 down more than 20 dB", db (hb / ha), -20.0);
        }
        {   // the filter envelope: IL below, AL above, back to the base
            Rig r; r.hz ("a_lpf", 800.0f); r.set ("a_il", 0.0f); r.set ("a_al", 1.0f); r.ms ("a_fa", 200.0f); r.ms ("a_fd", 300.0f);
            r.e.noteOn (57, 0.8f);
            std::vector<double> cut;
            for (int i = 0; i < 100; ++i) { r.renderAppend (480); cut.push_back (r.e.uiCut[0]); }   // every 10 ms
            check (cut[1] < 800.0 * 0.1, "filter envelope starts at IL (below the base cutoff)", cut[1], 80.0);
            double mx = 0; for (double c : cut) mx = std::max (mx, c);
            check (mx > 800.0 * 30.0, "filter envelope reaches AL (six octaves up)", mx, 800.0 * 64.0);
            check (std::abs (cut[99] / 800.0 - 1.0) < 0.05, "filter envelope decays back to the base cutoff", cut[99], 800.0);
        }
        {   // resonance really is Q: a narrow peak at the cutoff
            Rig r; r.set ("a_saw", 0); r.set ("a_noise", 0.5f); r.hz ("a_lpf", 1000.0f); r.set ("a_lpq", 0.8f); r.e.noteOn (60, 0.8f); r.render (96000);
            const double at = goertzel (r.L, 48000, 32768, 1000.0, r.fs), off = goertzel (r.L, 48000, 32768, 2000.0, r.fs);
            check (db (at / off) > 12.0, "SVF resonance 80 %: peak at cutoff 12 dB above one octave up", db (at / off), 12.0);
        }
    }
    // ---------------------------------------------------------------- envelopes and touch
    {
        std::printf ("envelopes and touch\n");
        {
            Rig r; r.ms ("a_va", 100.0f); r.e.noteOn (57, 0.8f); r.render (24000);
            // the envelope of the saw: peak per 5 ms
            const int hop = 240; std::vector<double> env;
            for (int i = 0; i + hop <= 24000; i += hop) { double m = 0; for (int j = 0; j < hop; ++j) m = std::max (m, (double) std::abs (r.L[(size_t) (i + j)])); env.push_back (m); }
            double mx = 0; for (double e : env) mx = std::max (mx, e);
            int t10 = -1, t90 = -1;
            for (int i = 0; i < (int) env.size(); ++i) { if (t10 < 0 && env[(size_t) i] > 0.1 * mx) t10 = i; if (t90 < 0 && env[(size_t) i] > 0.9 * mx) t90 = i; }
            const double rise = (t90 - t10) * 5.0;
            check (rise > 45.0 && rise < 110.0, "VCA attack 100 ms: 10-90 rise between 45 and 110 ms (RC shape)", rise, 100.0);
        }
        {
            Rig r; r.ms ("a_vr", 300.0f); r.e.noteOn (57, 0.8f); r.render (24000); r.e.noteOff (57); r.renderAppend (96000);
            const double t = rt60 (r.L, r.fs, 24000, -1.0, -30.0);
            check (t > 0.2 && t < 0.6, "VCA release 300 ms: 60 dB decay in 0.2-0.6 s (exponential, tau = 300/4)", t, 0.3);
        }
        {
            Rig a; a.set ("a_vel", 1.0f); a.e.noteOn (57, 1.0f); a.render (24000);
            Rig b; b.set ("a_vel", 1.0f); b.e.noteOn (57, 0.25f); b.render (24000);
            const double ratio = b.rms (12000) / a.rms (12000);
            check (std::abs (ratio - 0.25) < 0.05, "VELOCITY 100 %: velocity 0.25 is a quarter of the level", ratio, 0.25);
        }
        {
            Rig a; a.set ("a_velb", 1.0f); a.hz ("a_lpf", 500.0f); a.e.noteOn (57, 0.2f); a.render (24000);
            Rig b; b.set ("a_velb", 1.0f); b.hz ("a_lpf", 500.0f); b.e.noteOn (57, 1.0f); b.render (24000);
            const double ha = goertzel (a.L, 12000, 8192, 220.0 * 6, a.fs), hb = goertzel (b.L, 12000, 8192, 220.0 * 6, b.fs);
            check (db (hb / ha) > 12.0, "VEL>BRILL: a hard strike is at least 12 dB brighter at the 6th harmonic", db (hb / ha), 12.0);
        }
        {
            Rig a; a.set ("at_brill", 1.0f); a.hz ("a_lpf", 500.0f); a.e.noteOn (57, 0.8f); a.render (24000);
            const double before = goertzel (a.L, 12000, 8192, 220.0 * 6, a.fs);
            a.e.setAftertouch (1.0f); a.render (24000);
            const double after = goertzel (a.L, 12000, 8192, 220.0 * 6, a.fs);
            check (db (after / before) > 10.0, "TOUCH>BRILL: aftertouch opens the filter (6th harmonic up 10 dB)", db (after / before), 10.0);
        }
        {
            Rig a; a.set ("at_pitch", 1.0f); a.e.noteOn (69, 0.8f); a.e.setPolyAftertouch (69, 1.0f); a.render (48000);
            const double f = peakNear (a.L, 24000, 16384, 440.0 * std::pow (2.0, 2.0 / 12.0), a.fs);
            check (std::abs (cents (f, 440.0) - 200.0) < 5.0, "TOUCH>PITCH: full poly aftertouch bends two semitones", cents (f, 440.0), 200.0);
        }
        {   // a bend step glides over ~8 ms and never stairs: the pitch per 2 ms window moves by less than 25 cents
            Rig r; r.set ("a_saw", 0); r.set ("a_sine", 1.0f); r.set ("bend", 12.0f); r.e.noteOn (69, 0.8f); r.render (9600);
            r.e.setBend (1.0f); r.renderAppend (9600);
            std::vector<double> track; const int hop = 96;
            for (int w = 9600 - 480; w + hop * 4 <= (int) r.L.size(); w += hop)
            {
                int zc = 0; double first = -1, last = -1;
                for (int i = w + 1; i < w + hop * 4; ++i)
                    if (r.L[(size_t) (i - 1)] <= 0 && r.L[(size_t) i] > 0) { const double t = i - r.L[(size_t) i] / (r.L[(size_t) i] - r.L[(size_t) (i - 1)]); if (first < 0) first = t; last = t; ++zc; }
                track.push_back (zc > 2 ? (zc - 1) / ((last - first) / r.fs) : (track.empty() ? 440.0 : track.back()));
            }
            double worstStep = 0; int t90 = -1;
            for (size_t i = 1; i < track.size(); ++i) worstStep = std::max (worstStep, std::abs (cents (track[i], track[i - 1])));
            for (size_t i = 0; i < track.size(); ++i) if (t90 < 0 && cents (track[i], 440.0) > 0.9 * 1200.0) t90 = (int) i;
            check (worstStep < 250.0 && t90 > 3 && t90 < 40, "bend step of an octave: glides (90 % within 8-80 ms), no stair over 250 cents per 2 ms", worstStep, t90 * 2.0);
        }
        {
            Rig a; a.set ("bend", 2.0f); a.e.noteOn (69, 0.8f); a.e.setBend (1.0f); a.render (48000);
            const double f = peakNear (a.L, 24000, 16384, 440.0 * std::pow (2.0, 2.0 / 12.0), a.fs);
            check (std::abs (cents (f, 440.0) - 200.0) < 3.0, "bend range 2: wheel up is +200 cents", cents (f, 440.0), 200.0);
        }
    }
    // ---------------------------------------------------------------- sync, ring, poly-mod
    {
        std::printf ("sync, ring, poly-mod\n");
        {
            auto run = [] (bool sync)
            {
                Rig r; r.set ("b_lvl", 0.0f); r.set ("a_semi", 0.5f + 7.0f / 24.0f); r.set ("b_sync", sync ? 1.0f : 0.0f);
                r.e.noteOn (57, 0.8f); r.render (48000);
                const double atMaster2 = goertzel (r.L, 24000, 16384, 440.0, r.fs);
                const double atSlave = goertzel (r.L, 24000, 16384, 220.0 * std::pow (2.0, 7.0 / 12.0), r.fs);
                return atSlave / std::max (1e-9, atMaster2);
            };
            const double withSync = run (true), without = run (false);
            check (withSync < 0.15 && without > 1.0, "hard sync (rank I slaved to a silent rank II): the slave's own pitch vanishes, the master's harmonics appear", withSync, without);
        }
        {
            Rig r; r.set ("a_saw", 0); r.set ("a_sine", 1.0f); r.set ("ring_mode", 1); r.set ("ring_depth", 1.0f); r.hz ("ring_speed", 100.0f); r.set ("ring_mod", 0.5f);
            r.e.noteOn (69, 0.8f); r.render (48000);
            const double f0 = goertzel (r.L, 24000, 16384, 440.0, r.fs), up = goertzel (r.L, 24000, 16384, 540.0, r.fs), dn = goertzel (r.L, 24000, 16384, 340.0, r.fs);
            check (up > 5.0 * f0 && dn > 5.0 * f0, "ring CARRIER at 100 Hz on a 440 Hz sine: sum and difference, carrier suppressed", db (up / f0), db (dn / f0));
        }
        {
            Rig r; r.set ("a_saw", 0); r.set ("a_sine", 1.0f); r.set ("b_lvl", 0.8f); r.set ("b_saw", 0); r.set ("b_sine", 1.0f); r.set ("b_semi", 0.5f + 7.0f / 24.0f);
            r.set ("ring_mode", 2); r.set ("ring_depth", 1.0f); r.e.noteOn (57, 0.8f); r.render (48000);
            const double f1 = 220.0, f2 = 220.0 * std::pow (2.0, 7.0 / 12.0);
            const double a = goertzel (r.L, 24000, 16384, f1, r.fs), s = goertzel (r.L, 24000, 16384, f1 + f2, r.fs);
            check (s > 3.0 * a, "ring RANKS: rank I x rank II makes the sum frequency", db (s / a), 10.0);
        }
        {
            Rig a; a.set ("b_lvl", 0.0f); a.set ("b_saw", 0.0f); a.set ("b_tri", 1.0f); a.set ("b_semi", 0.5f - 12.0f / 24.0f); a.e.noteOn (57, 0.8f); a.render (48000);
            Rig b; b.set ("b_lvl", 0.0f); b.set ("b_saw", 0.0f); b.set ("b_tri", 1.0f); b.set ("b_semi", 0.5f - 12.0f / 24.0f); b.set ("pm_o2pitch", 0.5f); b.e.noteOn (57, 0.8f); b.render (48000);
            // FM by a sub-octave triangle: energy at 110 Hz spacing appears between the harmonics
            const double ha = goertzel (a.L, 24000, 16384, 330.0, a.fs), hb = goertzel (b.L, 24000, 16384, 330.0, b.fs);
            check (hb > 8.0 * ha, "poly-mod II>I PITCH: sidebands at half-harmonics appear", db (hb / ha), 18.0);
        }
        {
            Rig a; a.set ("b_lvl", 0.0f); a.set ("pm_envpitch", 1.0f); a.set ("a_il", 1.0f); a.set ("a_al", 1.0f); a.ms ("a_fd", 2000.0f);
            a.e.noteOn (57, 0.8f); a.render (2400);
            const double f = peakNear (a.L, 480, 1024, 220.0 * 4.0, a.fs, 800, 10);
            check (f > 220.0 * 2.5, "poly-mod ENV>I PITCH: the filter envelope starts the note two octaves up", f, 880.0);
        }
    }
    // ---------------------------------------------------------------- voices
    {
        std::printf ("voices\n");
        {
            Rig r; for (int n = 0; n < 8; ++n) r.e.noteOn (48 + n * 2, 0.8f); r.render (4800);
            check (r.e.voicesSounding() == 8, "eight notes: eight voices sounding", r.e.voicesSounding(), 8);
            r.e.noteOn (72, 0.8f); r.render (4800);
            bool oldestGone = true; for (int i = 0; i < 8; ++i) if (r.e.uiNotes[(size_t) i] == 48) oldestGone = false;
            check (r.e.voicesSounding() == 8 && oldestGone, "a ninth note steals the oldest", r.e.voicesSounding(), 8);
        }
        {
            Rig r; r.set ("mode", 3); r.e.noteOn (48, 0.8f); r.render (4800); r.e.noteOn (55, 0.8f); r.render (4800);
            check (r.e.voicesSounding() == 1, "MONO: two keys, one voice", r.e.voicesSounding(), 1);
            check (std::abs (r.e.voicePitch (0) - 55.0f) < 0.01f, "MONO: the newest key sounds", r.e.voicePitch (0), 55);
            r.e.noteOff (55); r.render (4800);
            check (std::abs (r.e.voicePitch (0) - 48.0f) < 0.01f, "MONO: releasing it returns to the held key", r.e.voicePitch (0), 48);
        }
        {
            Rig r; r.set ("mode", 2); r.e.noteOn (48, 0.8f); r.render (4800);
            check (r.e.voicesSounding() == 8, "UNISON: one key, eight voices", r.e.voicesSounding(), 8);
        }
        {
            Rig r; r.set ("mode", 1); r.e.noteOn (48, 0.8f); r.e.noteOn (52, 0.8f); r.render (4800);
            check (r.e.voicesSounding() == 4, "DUO: two keys, four voices", r.e.voicesSounding(), 4);
        }
        {   // unison detune beats
            auto swing = [] (float det)
            {
                Rig r; r.set ("mode", 2); r.set ("unidet", det); r.e.noteOn (57, 0.8f); r.render (96000);
                const int hop = 4800; double mn = 1e9, mx = 0;
                for (int i = 24000; i + hop <= 96000; i += hop) { const double v = r.rms (i, i + hop); mn = std::min (mn, v); mx = std::max (mx, v); }
                return (mx - mn) / mx;
            };
            const double with = swing (0.5f), without = swing (0.0f);
            check (with > 0.15 && without < 0.05, "UNISON DETUNE: eight voices beat (level swing) only when detuned", with, without);
        }
        {
            Rig r; r.set ("glide", 0.5f); r.e.noteOn (48, 0.8f); r.render (4800); r.e.noteOn (60, 0.8f);
            const float gm = glideMs (0.5f);
            r.render ((int) (r.fs * gm * 0.001f * 0.5f));
            const float mid = r.e.voicePitch (r.e.newestVoice());
            check (mid > 50.0f && mid < 59.0f, "GLIDE: half way through the glide time the pitch is between the notes", mid, 54);
            r.render ((int) (r.fs * gm * 0.001f * 3.0f));
            check (std::abs (r.e.voicePitch (r.e.newestVoice()) - 60.0f) < 0.05f, "GLIDE: it arrives", r.e.voicePitch (r.e.newestVoice()), 60);
        }
        {
            Rig r; r.set ("glide", 0.5f); r.set ("gliss", 1.0f); r.set ("mode", 3); r.e.noteOn (48, 0.8f); r.render (4800); r.e.noteOn (60, 0.8f);
            const float gm = glideMs (0.5f); r.render ((int) (r.fs * gm * 0.001f * 0.5f));
            const float mid = r.e.voicePitch (0);
            r.render (4096);
            const double f = peakNear (r.L, 0, 4096, midiHz (std::round (mid)), r.fs, 300, 4);
            const double semi = 12.0 * std::log2 (f / midiHz (48.0f));
            check (mid > 50.0f && mid < 59.0f && std::abs (semi - std::round (semi)) < 0.2, "GLISSANDO: mid-glide the pitch sits on a semitone", semi, std::round (semi));
        }
        {
            auto swing = [] (float vintage)
            {
                Rig r; r.set ("mode", 2); r.set ("unidet", 0.0f); r.set ("vintage", vintage); r.e.noteOn (57, 0.8f); r.render (144000);
                const int hop = 4800; double mn = 1e9, mx = 0;
                for (int i = 48000; i + hop <= 144000; i += hop) { const double v = r.rms (i, i + hop); mn = std::min (mn, v); mx = std::max (mx, v); }
                return (mx - mn) / mx;
            };
            const double with = swing (1.0f), without = swing (0.0f);
            check (with > 0.08 && without < 0.02, "VINTAGE 100 %: eight undetuned unison voices beat (eight machines), at 0 they do not", with, without);
            Rig r; r.set ("vintage", 1.0f); r.e.noteOn (69, 0.8f); r.render (48000);
            const double f = peakNear (r.L, 24000, 16384, 440.0, r.fs, 100, 1);
            check (std::abs (cents (f, 440.0)) < 12.0, "VINTAGE 100 %: never more than 12 cents off", cents (f, 440.0), 12);
        }
        {   // sustain pedal
            Rig r; r.e.setSustain (true); r.e.noteOn (57, 0.8f); r.render (2400); r.e.noteOff (57); r.renderAppend (9600);
            check (r.e.voicesSounding() == 1 && r.rms (9600, 12000) > 0.05f, "sustain pedal holds a released key", r.rms (9600, 12000), 0.1);
            r.e.setSustain (false); r.render (48000);
            check (r.e.voicesSounding() == 0, "releasing the pedal releases the key", r.e.voicesSounding(), 0);
        }
    }
    // ---------------------------------------------------------------- the chain
    {
        std::printf ("the chain\n");
        {   // drive: harmonics appear, level within 2 dB of the clean signal at every amount (measured trim)
            for (int mode = 1; mode <= 4; ++mode)
            {
                double worst = 0;
                for (float amt : { 0.0f, 0.5f, 1.0f })
                {
                    Rig a; a.set ("a_saw", 0); a.set ("a_sine", 1.0f); a.e.noteOn (57, 0.8f); a.render (48000);
                    Rig b; b.set ("a_saw", 0); b.set ("a_sine", 1.0f); b.set ("drv_mode", (float) mode); b.set ("drv_amt", amt); b.e.noteOn (57, 0.8f); b.render (48000);
                    worst = std::max (worst, std::abs (db (b.rms (24000) / a.rms (24000))));
                }
                const double lim = mode == 4 ? 7.0 : 4.0;
                char buf[96]; std::snprintf (buf, sizeof buf, "DRIVE %s: level within %.0f dB of clean across the knob (measured trim)", listNames ("drv_mode", *new int)[mode], lim);
                check (worst < lim, buf, worst, lim);
            }
            {
                Rig a; a.set ("a_saw", 0); a.set ("a_sine", 1.0f); a.e.noteOn (57, 0.8f); a.render (48000);
                Rig b; b.set ("a_saw", 0); b.set ("a_sine", 1.0f); b.set ("drv_mode", 1.0f); b.set ("drv_amt", 0.3f); b.e.noteOn (57, 0.8f); b.render (48000);
                const double h1b = goertzel (b.L, 24000, 16384, 220.0, b.fs), h2b = goertzel (b.L, 24000, 16384, 440.0, b.fs);
                const double h1a = goertzel (a.L, 24000, 16384, 220.0, a.fs), h2a = goertzel (a.L, 24000, 16384, 440.0, a.fs);
                check (db (h2b / h1b) > -24.0 && db (h2b / h1b) > db (h2a / h1a) + 8.0, "DRIVE VALVE at 30 %: a second harmonic above -24 dB (clean sine carries -34)", db (h2b / h1b), db (h2a / h1a));
            }
            {   // aliasing under drive at 2x
                Rig r (48000, 2); r.set ("drv_mode", 3.0f); r.set ("drv_amt", 1.0f); r.e.noteOn (93, 0.8f); r.render (48000);
                const int N = 16384; auto sp = spectrum (r.L, 24000, N); const double f0 = 1760.0, binHz = r.fs / N;
                double fund = 0, worst = 0;
                for (int i = 8; i < N / 2 - 8; ++i)
                {
                    const double f = i * binHz, k = f / f0; const bool harmonic = std::abs (k - std::round (k)) * f0 < 6.0 * binHz;
                    if (f > 1700 && f < 1820) fund = std::max (fund, sp[(size_t) i]);
                    if (! harmonic && f > 40.0 && f < 20000.0) worst = std::max (worst, sp[(size_t) i]);
                }
                check (db (worst / fund) < -36.0, "FUZZ at full on a saw at A6, 2x: aliasing below 20 kHz under -36 dB", db (worst / fund), -36.0);
            }
        }
        {   // ensemble: modulated, stereo, no level jump
            for (int mode = 1; mode <= 4; ++mode)
            {
                Rig a; a.set ("a_saw", 0); a.set ("a_sine", 1.0f); a.e.noteOn (69, 0.8f); a.render (96000);
                Rig b; b.set ("a_saw", 0); b.set ("a_sine", 1.0f); b.set ("ens_mode", (float) mode); b.set ("ens_depth", 1.0f); b.set ("ens_mix", 1.0f); b.e.noteOn (69, 0.8f); b.render (96000);
                // pitch deviation from zero-crossing periods per 20 ms window
                auto devOf = [] (const std::vector<float>& x, double fs)
                {
                    double mn = 1e9, mx = 0; const int hop = (int) (fs * 0.02);
                    for (int w = (int) (fs * 0.5); w + hop < (int) x.size(); w += hop)
                    {
                        int zc = 0; double first = -1, last = -1;
                        for (int i = w + 1; i < w + hop; ++i)
                            if (x[(size_t) (i - 1)] <= 0 && x[(size_t) i] > 0) { const double t = i - x[(size_t) i] / (x[(size_t) i] - x[(size_t) (i - 1)]); if (first < 0) first = t; last = t; ++zc; }
                        if (zc > 3) { const double f = (zc - 1) / ((last - first) / fs); mn = std::min (mn, f); mx = std::max (mx, f); }
                    }
                    return (mx - mn) / 440.0;
                };
                (void) devOf;
                double diffLR = 0, diffWD = 0, dry = 0;
                for (int i = 48000; i < 96000; ++i) { diffLR += std::abs (b.L[(size_t) i] - b.R[(size_t) i]); diffWD += std::abs (b.L[(size_t) i] - a.L[(size_t) i]); dry += std::abs (a.L[(size_t) i]); }
                const double lvl = db (b.rms (48000) / a.rms (48000));
                char buf[128]; std::snprintf (buf, sizeof buf, "ENSEMBLE %s: wet differs from dry (%.0f %%), stereo, level within 4 dB", listNames ("ens_mode", *new int)[mode], 100.0 * diffWD / dry);
                check (diffWD > 0.3 * dry && diffLR > 0.1 * dry && std::abs (lvl) < 4.0, buf, 100.0 * diffWD / dry, lvl);
            }
        }
        {   // choir: vowel A puts a formant at 800 Hz and a hole at 2 kHz
            Rig r; r.set ("a_saw", 1.0f); r.set ("choir_mix", 1.0f); r.set ("choir_vowel", 0.5f); r.set ("choir_reg", 0.5f);
            r.e.noteOn (45, 0.8f); r.render (96000);     // A2 = 110 Hz: harmonics every 110 Hz
            auto band = [&] (double lo, double hi) { double s = 0; for (double f = std::ceil (lo / 110.0) * 110.0; f <= hi; f += 110.0) s += goertzel (r.L, 48000, 32768, f, r.fs); return s; };
            const double f1 = band (700, 900), hole = band (1900, 2300);
            check (db (f1 / hole) > 15.0, "CHOIR vowel A: the first formant (800 Hz) 15 dB over the hole at 2 kHz", db (f1 / hole), 15.0);
            Rig i; i.set ("a_saw", 1.0f); i.set ("choir_mix", 1.0f); i.set ("choir_vowel", 1.0f); i.set ("choir_reg", 0.5f);
            i.e.noteOn (45, 0.8f); i.render (96000);
            auto bandI = [&] (double lo, double hi) { double s = 0; for (double f = std::ceil (lo / 110.0) * 110.0; f <= hi; f += 110.0) s += goertzel (i.L, 48000, 32768, f, i.fs); return s; };
            const double aLow = band (1000, 1300), aHigh = band (1550, 1850), iLow = bandI (1000, 1300), iHigh = bandI (1550, 1850);
            check (aLow > aHigh && iHigh > iLow, "CHOIR: the second formant sits near 1.15 kHz for A and moves to 1.7 kHz for I", db (aLow / aHigh), db (iHigh / iLow));
        }
        {   // tape wow: a 440 Hz sine, the pitch deviation against the formula
            auto devOf = [] (Rig& r)
            {
                double mn = 1e9, mx = 0; const int hop = (int) (r.fs * 0.05);
                for (int w = (int) (r.fs * 1.0); w + hop < (int) r.L.size(); w += hop)
                {
                    int zc = 0; double first = -1, last = -1;
                    for (int i = w + 1; i < w + hop; ++i)
                        if (r.L[(size_t) (i - 1)] <= 0 && r.L[(size_t) i] > 0) { const double t = i - r.L[(size_t) i] / (r.L[(size_t) i] - r.L[(size_t) (i - 1)]); if (first < 0) first = t; last = t; ++zc; }
                    if (zc > 3) { const double f = (zc - 1) / ((last - first) / r.fs); mn = std::min (mn, f); mx = std::max (mx, f); }
                }
                return (mx - mn) / 440.0 * 100.0;    // percent, peak to peak
            };
            {
                Rig r; r.set ("a_saw", 0); r.set ("a_sine", 1.0f); r.set ("tape_mode", 1); r.set ("tape_wow", 1.0f); r.hz ("tape_wowrate", 0.5f); r.set ("tape_flut", 0); r.set ("tape_sat", 0); r.set ("tape_age", 0); r.set ("tape_hiss", 0);
                r.e.noteOn (69, 0.8f); r.render (48000 * 6);
                const double dev = devOf (r);
                // sine part: 2.5 ms * 0.7 * 2*pi*0.5 = 0.55 % peak, 1.1 % p-p; the walk adds
                check (dev > 0.7 && dev < 2.5, "TAPE WOW 100 % at 0.5 Hz: pitch swings 0.7-2.5 % p-p (formula 1.1 + the walk)", dev, 1.1);
            }
            {
                Rig r; r.set ("a_saw", 0); r.set ("a_sine", 1.0f); r.set ("tape_mode", 1); r.set ("tape_wow", 0.0f); r.set ("tape_flut", 1.0f); r.set ("tape_sat", 0); r.set ("tape_age", 0); r.set ("tape_hiss", 0);
                r.e.noteOn (69, 0.8f); r.render (48000 * 3);
                const double dev = devOf (r);
                check (dev > 0.1 && dev < 1.0, "TAPE FLUTTER 100 %: pitch swings 0.1-1 % p-p", dev, 0.4);
            }
            {
                Rig r; r.set ("a_saw", 0); r.set ("a_sine", 1.0f); r.set ("tape_mode", 1); r.set ("tape_wow", 0.0f); r.set ("tape_flut", 0.0f); r.set ("tape_sat", 0); r.set ("tape_age", 0); r.set ("tape_hiss", 0);
                r.e.noteOn (69, 0.8f); r.render (48000 * 2);
                const double dev = devOf (r);
                check (dev < 0.02, "TAPE with wow and flutter at zero: the pitch does not move", dev, 0);
            }
            {   // dropouts: count them
                Rig r; r.set ("a_saw", 0); r.set ("a_sine", 1.0f); r.set ("tape_mode", 1); r.set ("tape_drop", 1.0f); r.set ("tape_wow", 0); r.set ("tape_flut", 0);
                r.e.noteOn (69, 0.8f);
                int edges = 0; bool was = false;
                for (int i = 0; i < 1000; ++i) { r.renderAppend (480); const bool now = r.e.uiDrop; if (now && ! was) ++edges; was = now; }
                check (edges >= 6 && edges <= 40, "DROPOUTS 100 %: 6-40 events in ten seconds (rate 1.5/s)", edges, 15);
            }
            {   // hiss: level, and exactly nothing when off
                Rig r; r.set ("tape_mode", 1); r.set ("tape_hiss", 1.0f); r.set ("tape_age", 1.0f); r.set ("tape_wow", 0); r.set ("tape_flut", 0); r.set ("tape_sat", 0);
                r.set ("a_vr", 0.0f); r.e.noteOn (57, 0.8f); r.render (24000); r.e.noteOff (57); r.renderAppend (48000);
                const double lvl = db (r.rms (36000, 60000));
                check (lvl > -60.0 && lvl < -30.0, "HISS 100 %, AGE 100 %: in the second after a note, between -60 and -30 dBFS", lvl, -45.0);
                r.renderAppend (48000 * 8);
                check (r.rms ((int) r.L.size() - 4800) == 0.0f, "HISS: the tape falls exactly silent once nothing has played for a while (gated)", r.rms ((int) r.L.size() - 4800), 0);
                Rig q; q.set ("tape_mode", 1); q.set ("tape_hiss", 0.0f); q.render (48000);
                check (q.peak() == 0.0f, "HISS 0: the tape adds exactly nothing to silence", q.peak(), 0);
            }
            {   // age darkens
                Rig a; a.set ("tape_mode", 1); a.set ("tape_age", 0.0f); a.set ("tape_wow", 0); a.set ("tape_flut", 0); a.set ("tape_sat", 0); a.e.noteOn (57, 0.8f); a.render (48000);
                Rig b; b.set ("tape_mode", 1); b.set ("tape_age", 1.0f); b.set ("tape_wow", 0); b.set ("tape_flut", 0); b.set ("tape_sat", 0); b.e.noteOn (57, 0.8f); b.render (48000);
                const double ha = goertzel (a.L, 24000, 16384, 220.0 * 40, a.fs), hb = goertzel (b.L, 24000, 16384, 220.0 * 40, b.fs);
                check (db (hb / ha) < -12.0, "TAPE AGE 100 %: the 40th harmonic of A3 (8.8 kHz) down more than 12 dB", db (hb / ha), -12.0);
            }
            {   // VHS: bandwidth capped
                Rig b; b.set ("tape_mode", 2); b.set ("tape_age", 0.0f); b.set ("tape_wow", 0); b.set ("tape_flut", 0); b.set ("tape_sat", 0); b.e.noteOn (57, 0.8f); b.render (48000);
                Rig a; a.e.noteOn (57, 0.8f); a.render (48000);
                const double ha = goertzel (a.L, 24000, 16384, 220.0 * 60, a.fs), hb = goertzel (b.L, 24000, 16384, 220.0 * 60, b.fs);
                check (db (hb / ha) < -10.0, "VHS: 13 kHz is more than 10 dB down even at AGE 0", db (hb / ha), -10.0);
            }
            {   // tape saturation: level trim holds, harmonics appear
                Rig a; a.set ("a_saw", 0); a.set ("a_sine", 1.0f); a.e.noteOn (57, 0.8f); a.render (48000);
                Rig b; b.set ("a_saw", 0); b.set ("a_sine", 1.0f); b.set ("tape_mode", 1); b.set ("tape_sat", 1.0f); b.set ("tape_wow", 0); b.set ("tape_flut", 0); b.set ("tape_age", 0); b.e.noteOn (57, 0.8f); b.render (48000);
                const double lvl = db (b.rms (24000) / a.rms (24000));
                const double h3a = goertzel (a.L, 24000, 16384, 660.0, a.fs), h3b = goertzel (b.L, 24000, 16384, 660.0, b.fs);
                check (std::abs (lvl) < 3.0 && h3b > 10.0 * h3a, "TAPE SAT 100 %: level within 3 dB, third harmonic up 20 dB", lvl, db (h3b / h3a));
            }
        }
        {   // hall
            {
                Rig r; r.set ("hall_mix", 1.0f); r.hz ("hall_damp", 16000.0f); r.set ("hall_size", 0.6f); r.set ("hall_decay", (float) (std::log (2.0 / 0.2) / std::log (100.0))); r.set ("hall_mod", 0); r.set ("hall_shim", 0);
                r.set ("a_vr", 0.0f); r.e.noteOn (57, 0.8f); r.render (4800); r.e.noteOff (57); r.renderAppend (48000 * 4);
                const double t = rt60 (r.L, r.fs, 4800, -5.0, -25.0);
                check (t > 1.6 && t < 2.6, "HALL DECAY 2 s: measured RT60 within 30 %", t, 2.0);
            }
            {   // the runaway test: stop the input and watch, at the worst setting
                Rig r; r.set ("hall_mix", 1.0f); r.set ("hall_decay", 1.0f); r.set ("hall_size", 1.0f); r.set ("hall_shim", 1.0f); r.set ("hall_mod", 1.0f); r.hz ("hall_damp", 16000.0f);
                r.set ("a_vr", 0.0f); r.e.noteOn (57, 0.8f); r.render (24000); r.e.noteOff (57);
                std::vector<double> lvl;
                for (int s = 0; s < 40; ++s) { r.renderAppend (48000); lvl.push_back (db (r.rms ((int) r.L.size() - 48000))); }
                double early = -1e9, late = -1e9;
                for (int s = 2; s < 8; ++s) early = std::max (early, lvl[(size_t) s]);
                for (int s = 34; s < 40; ++s) late = std::max (late, lvl[(size_t) s]);
                check (late < early - 3.0 && r.finite(), "HALL at max decay, size, shimmer and motion: 40 s after the note the tail is still falling (bounded is not stable)", early, late);
            }
            {
                Rig a; a.set ("hall_mix", 0.0f); a.e.noteOn (57, 0.8f); a.render (24000);
                Rig b; b.set ("hall_mix", 0.0f); b.set ("hall_decay", 1.0f); b.e.noteOn (57, 0.8f); b.render (24000);
                bool same = std::memcmp (a.L.data(), b.L.data(), a.L.size() * sizeof (float)) == 0;
                check (same, "HALL MIX 0: byte-identical whatever the hall's own knobs say", 0, 0);
            }
            {
                Rig r; r.set ("hall_mix", 0.5f); r.hz ("hall_pre", 200.0f); r.set ("a_vr", 0.0f); r.set ("a_va", 0.0f);
                r.e.noteOn (57, 0.8f); r.render (480); r.e.noteOff (57); r.renderAppend (48000);
                // the wet part arrives no earlier than 200 ms after the note: energy in 20-150 ms is only the dry tail
                const double early = r.rms (2400, 7200), late = r.rms (10000, 14000);
                check (late > 3.0 * early, "HALL PREDELAY 200 ms: the wet arrives late", db (late / std::max (1e-9, early)), 10.0);
            }
        }
    }
    // ---------------------------------------------------------------- integrity
    {
        std::printf ("integrity\n");
        {   // DC
            Rig r; r.set ("a_pulse", 1.0f); r.set ("a_pw", 1.0f); r.set ("a_tri", 1.0f); r.e.noteOn (40, 0.8f); r.render (96000);
            double m = 0; for (int i = 48000; i < 96000; ++i) m += r.L[(size_t) i]; m /= 48000;
            check (std::abs (m) < 2e-3, "no DC on a narrow pulse plus triangle at E2", m, 0);
        }
        {   // determinism
            Rig a; applyPatch (0, a.e.p); a.e.prepare (48000, 256); a.e.noteOn (57, 0.8f); a.e.noteOn (64, 0.7f); a.render (48000);
            Rig b; applyPatch (0, b.e.p); b.e.prepare (48000, 256); b.e.noteOn (57, 0.8f); b.e.noteOn (64, 0.7f); b.render (48000);
            check (std::memcmp (a.L.data(), b.L.data(), a.L.size() * sizeof (float)) == 0, "determinism: the same patch twice is byte-identical", 0, 0);
        }
        {   // neutral world-mod bus
            Rig a; applyPatch (0, a.e.p); a.e.prepare (48000, 256); a.e.noteOn (57, 0.8f); a.render (24000);
            Rig b; applyPatch (0, b.e.p); b.e.prepare (48000, 256); b.e.setWorldMod (0, 0, 0, 0, 0, 1); b.e.noteOn (57, 0.8f); b.render (24000);
            check (std::memcmp (a.L.data(), b.L.data(), a.L.size() * sizeof (float)) == 0, "a neutral world-mod bus is byte-identical", 0, 0);
            Rig c; applyPatch (0, c.e.p); c.e.prepare (48000, 256); c.e.setWorldMod (0, 0, 0, 0, 0, 0.5f); c.e.noteOn (57, 0.8f); c.render (24000);
            check (std::memcmp (a.L.data(), c.L.data(), a.L.size() * sizeof (float)) != 0, "a world-mod bus with filterMul 0.5 changes the sound", 0, 0);
        }
        {   // every parameter at both extremes
            int badN = 0; float worst = 0;
            for (int k = 0; k < numParams(); ++k)
                for (float v : { 0.0f, paramMax (paramSpec (k)) })
                {
                    Rig r; applyPatch (0, r.e.p); r.e.p.os = 0; paramSpec (k).ref (r.e.p) = v; r.e.prepare (48000, 256);
                    r.e.noteOn (48, 0.9f); r.e.noteOn (55, 0.9f); r.e.noteOn (64, 0.9f); r.render (9600); r.e.noteOff (55); r.renderAppend (9600);
                    if (! r.finite() || r.peak() > 1.001f) { ++badN; std::printf ("     extreme %s = %g: peak %g\n", paramSpec (k).id, v, r.peak()); }
                    worst = std::max (worst, r.peak());
                }
            check (badN == 0, "every parameter at both extremes: finite and inside the ceiling", badN, worst);
        }
        {   // factory patches
            int badN = 0, quiet = 0;
            for (int i = 0; i < numPatches(); ++i)
            {
                Rig r; applyPatch (i, r.e.p); r.e.p.os = 1; r.e.prepare (48000, 256);
                r.e.noteOn (48, 0.85f); r.e.noteOn (55, 0.85f); r.e.noteOn (64, 0.85f); r.render (48000 * 3); r.e.noteOff (48); r.e.noteOff (55); r.e.noteOff (64); r.renderAppend (24000);
                if (! r.finite() || r.peak() > 1.001f) { ++badN; std::printf ("     patch %s: peak %g\n", patchName (i), r.peak()); }
                double loudest = 0; for (int w = 0; w + 12000 <= 48000 * 3; w += 12000) loudest = std::max (loudest, (double) r.rms (w, w + 12000));
                if (db (loudest) < -45.0) { ++quiet; std::printf ("     patch %s: %.1f dBFS\n", patchName (i), db (loudest)); }
            }
            check (badN == 0, "every factory patch: bounded", badN, 0);
            check (quiet == 0, "every factory patch: audible (a quarter second above -45 dBFS within three seconds of a chord)", quiet, 0);
        }
        {   // random machines
            int badN = 0; float worst = 0;
            for (uint32_t s = 1; s <= 200; ++s)
            {
                Rig r; randomPatch (s, r.e.p); r.e.p.os = 0; r.e.prepare (48000, 256);
                r.e.noteOn (48, 0.9f); r.e.noteOn (55, 0.9f); r.e.noteOn (64, 0.9f); r.render (24000); r.e.noteOff (48); r.e.noteOff (55); r.e.noteOff (64); r.renderAppend (12000);
                if (! r.finite() || r.peak() > 1.001f) { ++badN; std::printf ("     random %u: peak %g\n", s, r.peak()); }
                worst = std::max (worst, r.peak());
            }
            check (badN == 0, "200 random machines: finite and inside the ceiling", badN, worst);
        }
        {   // a patch load is a fresh instrument: no click from stale state
            Rig r; applyPatch (0, r.e.p); r.e.prepare (48000, 256); r.render (4800);
            check (r.peak() == 0.0f, "a loaded patch with no key is silent", r.peak(), 0);
        }
    }
    } // ! costOnly

    // ---------------------------------------------------------------- cost
    {
        std::printf ("cost\n");
        for (int os : { 1, 2, 4 })
        {
            Rig r (48000, os == 1 ? 0 : (os == 2 ? 1 : 2)); applyPatch (0, r.e.p); r.e.p.os = os == 1 ? 0 : (os == 2 ? 1 : 2);
            r.set ("drv_mode", 1); r.set ("choir_mix", 0.5f); r.set ("hall_shim", 0.5f); r.e.prepare (48000, 256);
            for (int n = 0; n < 8; ++n) r.e.noteOn (40 + n * 3, 0.8f);
            r.render (4800);
            const auto t0 = std::chrono::high_resolution_clock::now();
            r.render (48000 * 5);
            const double sec = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            std::printf ("  %dx: eight voices, every stage on: %.1f %% of one core\n", os, 100.0 * sec / 5.0);
            if (os == 2) check (sec / 5.0 < 0.5, "cost at 2x with everything on: under 50 % of a core", 100.0 * sec / 5.0, 50.0);
        }
    }

    std::printf ("\n%d checks, %d failed%s\n", checks, fails, fails ? "" : " — ALL CLEAR");
    return fails ? 1 : 0;
}
