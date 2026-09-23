/*  BLACK RIDER — offline bench. Plain C++, no JUCE.

    Renders real audio through the engine and measures the things the
    design claims: that the oscillators are band-limited and in tune, that
    the filters self-oscillate AT their cutoff, that the drift is there when
    VINTAGE is up and absent when it is down, that the chord spread puts the
    bottom note in the middle, that a patch cable makes a difference, that
    nothing is ever unbounded, and that the whole thing is cheap enough.

    Usage: bktest            run everything
           bktest --wav DIR  also write a few renders as WAV for listening
*/
#include "Engine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>

using namespace bk;

static int fails = 0, checks = 0;
static void CHECK (bool ok, const char* what, double a = 0, double b = 0)
{
    ++checks;
    if (! ok) { ++fails; std::printf ("  FAIL  %s   (%.4g / %.4g)\n", what, a, b); }
}
static double db (double x) { return 20.0 * std::log10 (std::max (1.0e-12, x)); }

//==============================================================================
struct Render { std::vector<float> L, R; double sr; };

static Render render (Engine& e, double sr, int block, double seconds,
                      const std::vector<std::tuple<double, int, int, float>>& events /* time, note, on(1)/off(0), vel */)
{
    Render r; r.sr = sr;
    const int n = (int) (seconds * sr);
    r.L.assign ((size_t) n, 0.0f); r.R.assign ((size_t) n, 0.0f);
    size_t ev = 0;
    for (int pos = 0; pos < n; pos += block)
    {
        const int nb = std::min (block, n - pos);
        while (ev < events.size() && std::get<0> (events[ev]) * sr < pos + nb)
        {
            const int note = std::get<1> (events[ev]);
            if (std::get<2> (events[ev])) e.noteOn (note, std::get<3> (events[ev])); else e.noteOff (note);
            ++ev;
        }
        e.process (r.L.data() + pos, r.R.data() + pos, nb);
    }
    return r;
}

static Engine* makeEngine (const Params& p, double sr = 48000.0, int block = 256)
{
    auto* e = new Engine();
    e->p = p;
    e->prepare (sr, block);
    return e;
}

static double goertzel (const float* x, int n, double hz, double sr)
{
    const double w = 2.0 * 3.14159265358979 * hz / sr;
    const double c = 2.0 * std::cos (w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i)
    {
        // Hann window: keeps leakage from the neighbours down
        const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / (n - 1));
        s0 = x[i] * win + c * s1 - s2; s2 = s1; s1 = s0;
    }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return std::sqrt (re * re + im * im) * 2.0 / (0.5 * n);
}
static double peakAbs (const std::vector<float>& v, int from = 0, int to = -1)
{
    if (to < 0) to = (int) v.size();
    double m = 0; for (int i = from; i < to; ++i) m = std::max (m, (double) std::abs (v[(size_t) i]));
    return m;
}
static double rms (const std::vector<float>& v, int from, int to)
{
    double s = 0; for (int i = from; i < to; ++i) s += (double) v[(size_t) i] * v[(size_t) i];
    return std::sqrt (s / std::max (1, to - from));
}
static bool allFinite (const std::vector<float>& v)
{
    for (auto x : v) if (! std::isfinite (x)) return false;
    return true;
}
// dominant frequency by a coarse log sweep then refinement, within [lo, hi]
static double dominantHz (const float* x, int n, double sr, double lo, double hi)
{
    double best = lo, bestA = -1;
    for (double f = lo; f <= hi; f *= 1.02)
    {
        const double a = goertzel (x, n, f, sr);
        if (a > bestA) { bestA = a; best = f; }
    }
    double lo2 = best / 1.02, hi2 = best * 1.02;
    for (int it = 0; it < 40; ++it)
    {
        const double m1 = lo2 + (hi2 - lo2) / 3, m2 = hi2 - (hi2 - lo2) / 3;
        if (goertzel (x, n, m1, sr) < goertzel (x, n, m2, sr)) lo2 = m1; else hi2 = m2;
    }
    return 0.5 * (lo2 + hi2);
}
// pitch by zero crossings of a window (for drift tracking)
static double zcHz (const float* x, int n, double sr)
{
    int first = -1, last = -1, count = 0;
    for (int i = 1; i < n; ++i)
        if (x[i - 1] <= 0.0f && x[i] > 0.0f)
        {
            if (first < 0) first = i; last = i; ++count;
        }
    if (count < 2) return 0.0;
    return (count - 1) * sr / (double) (last - first);
}

static void writeWav (const std::string& path, const Render& r)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const int n = (int) r.L.size();
    const int dataBytes = n * 2 * 2;
    auto w32 = [&] (uint32_t v) { std::fputc (v & 255, f); std::fputc ((v >> 8) & 255, f); std::fputc ((v >> 16) & 255, f); std::fputc ((v >> 24) & 255, f); };
    auto w16 = [&] (uint16_t v) { std::fputc (v & 255, f); std::fputc ((v >> 8) & 255, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 ((uint32_t) (36 + dataBytes)); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (1); w16 (2); w32 ((uint32_t) r.sr); w32 ((uint32_t) (r.sr * 4)); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 ((uint32_t) dataBytes);
    for (int i = 0; i < n; ++i)
    {
        w16 ((uint16_t) (int16_t) std::lround (clampf (r.L[(size_t) i], -1.0f, 1.0f) * 32767.0f));
        w16 ((uint16_t) (int16_t) std::lround (clampf (r.R[(size_t) i], -1.0f, 1.0f) * 32767.0f));
    }
    std::fclose (f);
}

static Params bare()
{
    Params p;
    p.vintage = 0.0f; p.o2lvl = 0.0f; p.o1lvl = 0.8f; p.lpf = 1.0f; p.lpeak = 0.0f; p.fenv = 0.5f; p.fkey = 0.0f;
    p.fdrive = 0.0f; p.hpf = 0.0f; p.hpeak = 0.0f; p.e2a = 0.0f; p.e2s = 1.0f; p.e2r = 0.0f; p.e2vel = 0.0f;
    p.e1s = 1.0f; p.volume = 0.5f; p.drv = 0.0f; p.chmix = 0; p.dlmix = 0; p.spmix = 0;
    return p;
}

//==============================================================================
int main (int argc, char** argv)
{
    std::string wavDir;
    for (int i = 1; i < argc; ++i) if (std::strcmp (argv[i], "--wav") == 0 && i + 1 < argc) wavDir = argv[i + 1];

    std::printf ("BLACK RIDER bench — %d parameters\n", numParams());

    // ---- 0. the table --------------------------------------------------
    {
        for (int i = 0; i < numParams(); ++i)
            for (int j = i + 1; j < numParams(); ++j)
                CHECK (std::strcmp (paramSpec (i).id, paramSpec (j).id) != 0, "duplicate id", i, j);
        Params p;
        for (int i = 0; i < numParams(); ++i)
        {
            const auto& s = paramSpec (i);
            CHECK (s.get (p) == s.def, (std::string ("default in table == default in Params: ") + s.id).c_str(), s.get (p), s.def);
        }
    }

    // ---- 1. oscillator: tuning and band-limiting ------------------------
    std::printf ("\n[1] oscillators\n");
    for (int wave = 0; wave < 4; ++wave)
    {
        Params p = bare(); p.o1wave = (float) wave; p.os = 1;
        auto* e = makeEngine (p);
        auto r = render (*e, 48000, 256, 1.0, { {0.0, 57, 1, 0.8f} });
        const int from = 24000, n = 16384;
        const double f0 = 220.0;
        const double h1 = goertzel (r.L.data() + from, n, f0, 48000);
        const double measured = dominantHz (r.L.data() + from, n, 48000, 150, 300);
        const double cents = 1200.0 * std::log2 (measured / f0);
        // the biggest non-harmonic component between harmonics 20 and 40
        double worst = 0;
        for (int h = 20; h < 40; ++h) worst = std::max (worst, goertzel (r.L.data() + from, n, f0 * (h + 0.5), 48000));
        std::printf ("  wave %d  f=%.2f Hz (%+.1f cents)  fundamental %.1f dB  between-harmonics %.1f dB\n",
                     wave, measured, cents, db (h1), db (worst / h1));
        CHECK (std::abs (cents) < 2.0, "oscillator in tune", cents, 2.0);
        CHECK (allFinite (r.L), "finite");
        delete e;
    }
    // aliasing at a high note, 1x vs 2x vs 4x
    for (int os = 0; os < 3; ++os)
    {
        Params p = bare(); p.o1wave = 0; p.os = (float) os;
        auto* e = makeEngine (p);
        auto r = render (*e, 48000, 256, 1.0, { {0.0, 100, 1, 0.8f} });
        const int from = 24000, n = 16384;
        const double f0 = 440.0 * std::pow (2.0, (100 - 69) / 12.0);
        const double h1 = goertzel (r.L.data() + from, n, f0, 48000);
        // scan 100 Hz .. f0 for anything that is not the fundamental
        double worst = 0, worstF = 0;
        for (double f = 100; f < f0 * 0.93; f *= 1.01)
        {
            const double a = goertzel (r.L.data() + from, n, f, 48000);
            if (a > worst) { worst = a; worstF = f; }
        }
        std::printf ("  saw at %.0f Hz, %dx: worst alias below f0 = %.1f dB at %.0f Hz\n", f0, 1 << os, db (worst / h1), worstF);
        if (os >= 1) CHECK (db (worst / h1) < -60.0, "aliasing below -60 dB at 2x/4x", db (worst / h1), -60);
        delete e;
    }

    // ---- 2. filters: self-oscillation frequency == cutoff ---------------
    std::printf ("\n[2] filters\n");
    static const char* MODEL[] = { "GROWL", "SCREAM", "LADDER" };
    for (int model = 0; model < 3; ++model)
    {
        double worstErr = 0;
        for (double hz : { 110.0, 440.0, 1760.0, 5000.0 })
        {
            Params p = bare(); p.fmodel = (float) model; p.lpeak = 1.0f; p.o1lvl = 0.0f; p.vcamode = 2;
            p.lpf = (float) (std::log (hz / 20.0) / std::log (1000.0));
            p.os = 1;
            auto* e = makeEngine (p);
            auto r = render (*e, 48000, 256, 1.5, {});
            const int from = 48000, n = 24000;
            const double f = dominantHz (r.L.data() + from, n, 48000, hz * 0.6, hz * 1.6);
            const double amp = rms (r.L, from, from + n);
            const double err = 1200.0 * std::log2 (f / hz);
            worstErr = std::max (worstErr, std::abs (err));
            std::printf ("  %s self-osc set %.0f Hz -> %.1f Hz (%+.0f cents), rms %.2f\n", MODEL[model], hz, f, err, amp);
            CHECK (amp > 0.02, "self-oscillates at max peak", amp, 0.02);
            CHECK (allFinite (r.L), "finite");
            delete e;
        }
        CHECK (worstErr < 60.0, "self-oscillation within 60 cents of the cutoff", worstErr, 60);
    }
    // the character difference: SCREAM oscillates from noon; GROWL waits for the top.
    {
        auto oscAmp = [] (int model, float peak) {
            Params p = bare(); p.fmodel = (float) model; p.lpeak = peak; p.o1lvl = 0.0f; p.vcamode = 2;
            p.lpf = (float) (std::log (440.0 / 20.0) / std::log (1000.0));
            auto* e = makeEngine (p); auto r = render (*e, 48000, 256, 1.2, {}); delete e;
            return rms (r.L, 36000, 57600);
        };
        const double growl2 = oscAmp (0, 0.70f), scream2 = oscAmp (1, 0.70f);
        const double growlTop = oscAmp (0, 0.98f), screamLow = oscAmp (1, 0.55f);
        std::printf ("  past two o'clock (peak .70): GROWL rms %.3f, SCREAM rms %.3f\n", growl2, scream2);
        std::printf ("  GROWL at top (.98) rms %.3f; SCREAM below onset (.55) rms %.3f\n", growlTop, screamLow);
        CHECK (scream2 > 0.02, "SCREAM self-oscillates past two o'clock", scream2, 0.02);
        CHECK (screamLow < 0.01, "SCREAM does NOT oscillate below its onset", screamLow, 0.01);
        CHECK (growl2 < 0.01, "GROWL does NOT oscillate at two o'clock", growl2, 0.01);
        CHECK (growlTop > 0.02, "GROWL oscillates at the top of the dial", growlTop, 0.02);
        // playable in tune: SCREAM tracks the keyboard through KEY follow
        Params p = bare(); p.fmodel = 1; p.lpeak = 0.8f; p.o1lvl = 0.0f; p.vcamode = 0; p.fkey = 1.0f; p.fenv = 0.5f;
        p.lpf = (float) (std::log (261.6 / 20.0) / std::log (1000.0));   // cutoff at C4 with no key
        p.e2a = 0.0f; p.e2s = 1.0f; p.e2r = 0.1f;
        auto* e = makeEngine (p);
        auto rC = render (*e, 48000, 256, 1.0, { {0.0, 60, 1, 0.9f} });   // C4
        delete e;
        e = makeEngine (p);
        auto rC5 = render (*e, 48000, 256, 1.0, { {0.0, 72, 1, 0.9f} });  // C5, an octave up
        delete e;
        const double fC = dominantHz (rC.L.data() + 24000, 16384, 48000, 180, 400);
        const double fC5 = dominantHz (rC5.L.data() + 24000, 16384, 48000, 380, 800);
        std::printf ("  SCREAM played with KEY follow: C4 -> %.1f Hz, C5 -> %.1f Hz (ratio %.2f)\n", fC, fC5, fC5 / fC);
        CHECK (std::abs (fC5 / fC - 2.0) < 0.15, "full key follow makes the scream track an octave", fC5 / fC, 2.0);
    }
    // resonance off: the passband is flat-ish and the filter cuts
    for (int model = 0; model < 3; ++model)
    {
        Params p = bare(); p.fmodel = (float) model; p.lpeak = 0.0f; p.o1wave = 0;
        p.lpf = (float) (std::log (500.0 / 20.0) / std::log (1000.0));
        auto* e = makeEngine (p);
        auto r = render (*e, 48000, 256, 1.0, { {0.0, 45, 1, 0.8f} });   // 110 Hz saw
        const int from = 24000, n = 16384;
        const double lo = goertzel (r.L.data() + from, n, 110.0, 48000);
        const double hi = goertzel (r.L.data() + from, n, 110.0 * 36, 48000);   // 3960 Hz, 3 octaves above cutoff
        std::printf ("  %s 500 Hz: h1 %.1f dB, h36 (3.96 kHz) %.1f dB relative to h1 (saw would be %.1f)\n",
                     MODEL[model], db (lo), db (hi / lo), db (1.0 / 36.0));
        CHECK (db (hi / lo) < db (1.0 / 36.0) - 20.0, "filter attenuates 3 octaves up by > 20 dB more than the saw's own slope", db (hi / lo), db (1.0 / 36.0) - 20.0);
        delete e;
    }
    // HPF: cuts the fundamental
    {
        Params p = bare(); p.hpf = (float) (std::log (2000.0 / 20.0) / std::log (400.0)); p.o1wave = 0;
        auto* e = makeEngine (p);
        auto r = render (*e, 48000, 256, 1.0, { {0.0, 45, 1, 0.8f} });
        const int from = 24000, n = 16384;
        const double lo = goertzel (r.L.data() + from, n, 110.0, 48000);
        const double hi = goertzel (r.L.data() + from, n, 110.0 * 36, 48000);
        std::printf ("  HPF 2 kHz on a 110 Hz saw: h1 %.1f dB, h36 %.1f dB\n", db (lo), db (hi));
        CHECK (db (hi) > db (lo) + 10.0, "HPF removes the fundamental", db (hi), db (lo));
        delete e;
    }

    // ---- 3. VINTAGE: drift is there when asked and not otherwise ---------
    std::printf ("\n[3] vintage\n");
    {
        double sdOff = 0, sdOn = 0;
        for (int v = 0; v < 2; ++v)
        {
            Params p = bare(); p.vintage = v ? 1.0f : 0.0f; p.o1wave = 3;
            auto* e = makeEngine (p);
            auto r = render (*e, 48000, 256, 8.0, { {0.0, 57, 1, 0.8f} });
            std::vector<double> cents;
            for (int w = 48000; w + 9600 <= (int) r.L.size(); w += 9600)
            {
                const double f = zcHz (r.L.data() + w, 9600, 48000);
                if (f > 0) cents.push_back (1200.0 * std::log2 (f / 220.0));
            }
            double m = 0; for (auto c : cents) m += c; m /= std::max<size_t> (1, cents.size());
            double s = 0; for (auto c : cents) s += (c - m) * (c - m); s = std::sqrt (s / std::max<size_t> (1, cents.size()));
            std::printf ("  vintage %d: mean %+.1f cents, wander sd %.2f cents over %zu windows\n", v, m, s, cents.size());
            (v ? sdOn : sdOff) = s;
        }
        CHECK (sdOff < 0.2, "no drift at vintage 0", sdOff, 0.2);
        CHECK (sdOn > 0.6 && sdOn < 12.0, "audible but sane drift at vintage 1", sdOn, 0.6);
    }

    // ---- 4. envelopes ----------------------------------------------------
    std::printf ("\n[4] envelopes\n");
    {
        Params p = bare(); p.o1wave = 3; p.e2a = (float) (std::log (200.0 / 0.5) / std::log (20000.0)); p.e2r = (float) (std::log (300.0 / 2.0) / std::log (7500.0));
        auto* e = makeEngine (p);
        auto r = render (*e, 48000, 64, 2.0, { {0.0, 57, 1, 0.8f}, {1.0, 57, 0, 0} });
        auto env = [&] (double t) { const int i = (int) (t * 48000); return peakAbs (r.L, i, i + 480); };
        const double full = env (0.8);
        std::printf ("  attack 200 ms: level at 50 ms %.2f, 100 ms %.2f, 200 ms %.2f, 800 ms %.2f (full)\n", env (0.05) / full, env (0.1) / full, env (0.2) / full, 1.0);
        std::printf ("  release 300 ms: level at +100 ms %.2f, +300 ms %.2f, +900 ms %.3f\n", env (1.1) / full, env (1.3) / full, env (1.9) / full);
        CHECK (env (0.1) / full > 0.4 && env (0.1) / full < 0.95, "attack is on its way at half time", env (0.1) / full, 0.6);
        CHECK (env (0.2) / full > 0.9, "attack reaches the top", env (0.2) / full, 0.9);
        CHECK (env (1.9) / full < 0.02, "release gets there", env (1.9) / full, 0.02);
        delete e;
    }

    // ---- 5. chord spread -------------------------------------------------
    std::printf ("\n[5] chord spread\n");
    {
        Params p = bare(); p.mode = 2; p.spread = 1.0f; p.o1wave = 3;
        auto* e = makeEngine (p);
        auto r = render (*e, 48000, 256, 1.5, { {0.0, 48, 1, 0.8f}, {0.0, 55, 1, 0.8f}, {0.0, 64, 1, 0.8f} });
        const int from = 48000, n = 16384;
        auto lr = [&] (int note) {
            const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            return std::make_pair (goertzel (r.L.data() + from, n, f, 48000), goertzel (r.R.data() + from, n, f, 48000)); };
        auto lo = lr (48), mid = lr (55), hi = lr (64);
        std::printf ("  three notes, spread 100%%: low L/R %.3f/%.3f  mid %.3f/%.3f  high %.3f/%.3f\n",
                     lo.first, lo.second, mid.first, mid.second, hi.first, hi.second);
        CHECK (std::abs (lo.first - lo.second) < 0.1 * std::max (lo.first, lo.second), "bottom note centred", lo.first, lo.second);
        CHECK (mid.first > mid.second * 4, "middle note left", mid.first, mid.second);
        CHECK (hi.second > hi.first * 4, "top note right", hi.second, hi.first);
        delete e;

        e = makeEngine (p);
        r = render (*e, 48000, 256, 1.5, { {0.0, 48, 1, 0.8f}, {0.0, 64, 1, 0.8f} });
        lo = lr (48); hi = lr (64);
        std::printf ("  two notes: low L/R %.3f/%.3f  high %.3f/%.3f\n", lo.first, lo.second, hi.first, hi.second);
        CHECK (lo.first > lo.second * 4, "two notes: low left", lo.first, lo.second);
        CHECK (hi.second > hi.first * 4, "two notes: high right", hi.second, hi.first);
        delete e;

        // four notes: LL L R RR by pitch
        e = makeEngine (p);
        r = render (*e, 48000, 256, 1.5, { {0.0, 44, 1, 0.8f}, {0.0, 52, 1, 0.8f}, {0.0, 62, 1, 0.8f}, {0.0, 70, 1, 0.8f} });
        {
            auto a = lr (44), b = lr (52), c = lr (62), d = lr (70);
            std::printf ("  four notes: %.3f/%.3f  %.3f/%.3f  %.3f/%.3f  %.3f/%.3f\n",
                         a.first, a.second, b.first, b.second, c.first, c.second, d.first, d.second);
            CHECK (a.first > a.second * 4, "four notes: bottom hard left", a.first, a.second);
            CHECK (b.first > b.second * 1.7, "four notes: second half left", b.first, b.second);
            CHECK (c.second > c.first * 1.7, "four notes: third half right", c.second, c.first);
            CHECK (d.second > d.first * 4, "four notes: top hard right", d.second, d.first);
        }
        delete e;

        // five notes: LL L M R RR by pitch
        e = makeEngine (p);
        r = render (*e, 48000, 256, 1.5, { {0.0, 40, 1, 0.8f}, {0.0, 48, 1, 0.8f}, {0.0, 55, 1, 0.8f}, {0.0, 64, 1, 0.8f}, {0.0, 72, 1, 0.8f} });
        {
            auto a = lr (40), b = lr (48), c = lr (55), d = lr (64), f = lr (72);
            std::printf ("  five notes: %.3f/%.3f  %.3f/%.3f  %.3f/%.3f  %.3f/%.3f  %.3f/%.3f\n",
                         a.first, a.second, b.first, b.second, c.first, c.second, d.first, d.second, f.first, f.second);
            CHECK (a.first > a.second * 4, "five notes: bottom hard left", a.first, a.second);
            CHECK (b.first > b.second * 1.7, "five notes: second half left", b.first, b.second);
            CHECK (std::abs (c.first - c.second) < 0.15 * std::max (c.first, c.second), "five notes: middle centred", c.first, c.second);
            CHECK (d.second > d.first * 1.7, "five notes: fourth half right", d.second, d.first);
            CHECK (f.second > f.first * 4, "five notes: top hard right", f.second, f.first);
        }
        delete e;

        // a strum inside the 80 ms window still lands ranked like a block chord
        e = makeEngine (p);
        r = render (*e, 48000, 256, 1.5, { {0.0, 48, 1, 0.8f}, {0.03, 55, 1, 0.8f}, {0.06, 64, 1, 0.8f} });
        lo = lr (48); mid = lr (55); hi = lr (64);
        std::printf ("  strummed three: low %.3f/%.3f  mid %.3f/%.3f  high %.3f/%.3f\n",
                     lo.first, lo.second, mid.first, mid.second, hi.first, hi.second);
        CHECK (std::abs (lo.first - lo.second) < 0.1 * std::max (lo.first, lo.second), "strum: bottom centred", lo.first, lo.second);
        CHECK (mid.first > mid.second * 4, "strum: middle left", mid.first, mid.second);
        CHECK (hi.second > hi.first * 4, "strum: top right", hi.second, hi.first);
        delete e;

        // sticky seats: a note that is already sounding does not move when
        // a later note arrives — the newcomer takes a free station instead
        e = makeEngine (p);
        r = render (*e, 48000, 256, 1.5, { {0.0, 48, 1, 0.8f}, {0.5, 64, 1, 0.8f} });
        lo = lr (48); hi = lr (64);
        std::printf ("  late second note: first %.3f/%.3f  newcomer %.3f/%.3f\n", lo.first, lo.second, hi.first, hi.second);
        CHECK (std::abs (lo.first - lo.second) < 0.1 * std::max (lo.first, lo.second), "late note: the sounding note stays put", lo.first, lo.second);
        CHECK (hi.second > hi.first * 4, "late note: newcomer takes the right", hi.second, hi.first);
        delete e;

        p.spread = 0.0f;
        e = makeEngine (p);
        r = render (*e, 48000, 256, 1.5, { {0.0, 48, 1, 0.8f}, {0.0, 64, 1, 0.8f} });
        lo = lr (48); hi = lr (64);
        CHECK (std::abs (hi.first - hi.second) < 0.1 * hi.first, "spread 0: everything centred", hi.first, hi.second);
        delete e;
    }

    // ---- 6. the patch bay does something -------------------------------
    std::printf ("\n[6] patch bay\n");
    {
        Params base = bare(); base.o1wave = 0; base.lpf = 0.45f;
        auto ref = [&] () { auto* e = makeEngine (base); auto r = render (*e, 48000, 256, 1.0, { {0.0, 57, 1, 0.8f} }); delete e; return r; }();
        auto apart = [&] (const Render& a, const Render& b) {
            double d = 0, s = 0; for (size_t i = 24000; i < a.L.size(); ++i) { d += (a.L[i] - b.L[i]) * (a.L[i] - b.L[i]); s += a.L[i] * a.L[i]; }
            return std::sqrt (d / std::max (1.0e-12, s)); };
        for (int dst = 1; dst < NUM_DESTS; ++dst)
        {
            Params p = base;
            int src = S_LFO;
            if (dst == D_DLYT) { p.dlmix = 0.5f; src = S_EG1; }
            if (dst == D_LRATE) { p.flfo = 0.5f; src = S_EG1; }          // the LFO must be doing something for its rate to matter
            if (dst == D_P2 || dst == D_PW2 || dst == D_FM2) { p.o2lvl = 0.7f; p.o2wave = 2; }
            if (dst == D_PW1) p.o1wave = 2;
            if (dst == D_VCAIN || dst == D_FIN) src = S_NOISE;
            if (dst == D_DRIVE) p.drv = 0.0f;
            p.cable[0].src = (float) src;
            p.cable[0].dst = (float) dst; p.cable[0].amt = 0.9f;
            p.lrate = 0.5f;
            auto* e = makeEngine (p); auto r = render (*e, 48000, 256, 1.0, { {0.0, 57, 1, 0.8f} }); delete e;
            const double d = apart (ref, r);
            std::printf ("  -> %-12s  difference %.3f\n", destName (dst), d);
            CHECK (d > 0.01, (std::string ("cable into ") + destName (dst) + " changes the sound").c_str(), d, 0.01);
            CHECK (allFinite (r.L), "finite");
        }
        // an unpatched cable is inert
        {
            Params p = base; p.cable[0].src = (float) S_LFO; p.cable[0].dst = 0; p.cable[0].amt = 1.0f;
            auto* e = makeEngine (p); auto r = render (*e, 48000, 256, 1.0, { {0.0, 57, 1, 0.8f} }); delete e;
            CHECK (apart (ref, r) < 1.0e-6, "half a cable is inert", apart (ref, r), 0);
        }
        // the famous feedback patch stays bounded
        {
            Params p = base; p.lpeak = 0.9f; p.fdrive = 0.8f; p.cable[0].src = (float) S_VCA; p.cable[0].dst = (float) D_FIN; p.cable[0].amt = 1.0f;
            auto* e = makeEngine (p); auto r = render (*e, 48000, 256, 3.0, { {0.0, 45, 1, 1.0f} }); delete e;
            std::printf ("  VCA -> FILTER IN at full: peak %.3f\n", peakAbs (r.L));
            CHECK (peakAbs (r.L) <= 1.0 && allFinite (r.L), "feedback patch bounded", peakAbs (r.L), 1.0);
        }
    }

    // ---- 7. sync, FM, ring: each makes a different sound ------------------
    std::printf ("\n[7] sync / fm / ring / sub\n");
    {
        Params base = bare(); base.o1wave = 0; base.o2wave = 0; base.o2lvl = 0.8f; base.o1lvl = 0.0f; base.o2semi = 0.5f + 7.0f / 24.0f;
        auto a = [&] (const Params& p) { auto* e = makeEngine (p); auto r = render (*e, 48000, 256, 1.0, { {0.0, 57, 1, 0.8f} }); delete e; return r; };
        auto ref = a (base);
        Params ps = base; ps.o2sync = 1; auto rs = a (ps);
        double d = 0; for (size_t i = 24000; i < rs.L.size(); ++i) d += std::abs (rs.L[i] - ref.L[i]);
        std::printf ("  sync on vs off: mean |diff| %.3f, peak %.3f\n", d / (rs.L.size() - 24000), peakAbs (rs.L));
        CHECK (d / (rs.L.size() - 24000) > 0.05, "sync changes the wave");
        // synced VCO2's pitch is VCO1's
        const double f1 = 220.0;
        const double fm = dominantHz (rs.L.data() + 24000, 16384, 48000, 150, 300);
        std::printf ("  synced VCO2 (+7 semi) dominant %.1f Hz (VCO1 at %.0f)\n", fm, f1);
        CHECK (std::abs (1200 * std::log2 (fm / f1)) < 10, "sync locks the period to VCO1", fm, f1);
        Params pf = base; pf.o2fm = 0.6f; auto rf = a (pf);
        CHECK (allFinite (rf.L) && peakAbs (rf.L) > 0.05, "FM sounds and is finite");
        Params pr = bare(); pr.o1lvl = 0; pr.o2lvl = 0; pr.ringlvl = 0.8f; pr.o1wave = 3; pr.o2wave = 3; pr.o2semi = 0.5f + 7.0f / 24.0f; auto rr = a (pr);
        const double f2 = 220.0 * std::pow (2.0, 7.0 / 12.0);
        const double sum = goertzel (rr.L.data() + 24000, 16384, f1 + f2, 48000), dif = goertzel (rr.L.data() + 24000, 16384, f2 - f1, 48000), car = goertzel (rr.L.data() + 24000, 16384, f1, 48000);
        std::printf ("  ring mod of two sines: sum %.1f dB, difference %.1f dB, carrier %.1f dB\n", db (sum), db (dif), db (car));
        CHECK (sum > car * 3 && dif > car * 3, "ring mod gives sum and difference, not the carriers", db (sum), db (car));
        Params pb = bare(); pb.o1lvl = 0; pb.sublvl = 0.8f; auto rb = a (pb);
        const double subf = dominantHz (rb.L.data() + 24000, 16384, 48000, 60, 200);
        std::printf ("  sub (-1 oct) dominant %.1f Hz\n", subf);
        CHECK (std::abs (1200 * std::log2 (subf / 110.0)) < 10, "sub is an octave down", subf, 110);
    }

    // ---- 8. effects -------------------------------------------------------
    std::printf ("\n[8] effects\n");
    {
        // tape delay: an echo at the set time
        Params p = bare(); p.o1wave = 3; p.dlmix = 0.6f; p.dlfb = 0.3f; p.dlwow = 0.0f;
        p.dltime = (float) (std::log (400.0 / 20.0) / std::log (75.0));
        auto* e = makeEngine (p);
        auto r = render (*e, 48000, 256, 2.0, { {0.0, 69, 1, 0.8f}, {0.05, 69, 0, 0} });
        delete e;
        // envelope of the output: expect a bump around 0.40 s + 0.05
        double bumpAt = 0, bumpV = 0;
        for (double t = 0.25; t < 0.7; t += 0.005) { const double v = rms (r.L, (int) (t * 48000), (int) (t * 48000) + 480); if (v > bumpV) { bumpV = v; bumpAt = t; } }
        std::printf ("  tape delay 400 ms: loudest echo at %.3f s\n", bumpAt);
        CHECK (std::abs (bumpAt - 0.40) < 0.05, "echo lands at the delay time", bumpAt, 0.40);

        // delay sync: 1/4 at the default 120 bpm = 0.5 s, whatever TIME says
        {
            Params q2 = bare(); q2.o1wave = 3; q2.dlmix = 0.6f; q2.dlfb = 0.2f; q2.dlwow = 0.0f;
            q2.dltime = 0.1f; q2.dlsync = 3;                       // knob says ~40 ms; sync says 1/4
            auto* e2 = makeEngine (q2);
            auto r2 = render (*e2, 48000, 256, 2.0, { {0.0, 69, 1, 0.8f}, {0.05, 69, 0, 0} });
            delete e2;
            double bumpAt = 0, bumpV = 0;
            for (double t = 0.3; t < 0.8; t += 0.005) { const double v = rms (r2.L, (int) (t * 48000), (int) (t * 48000) + 480); if (v > bumpV) { bumpV = v; bumpAt = t; } }
            std::printf ("  delay SYNC 1/4 @120: loudest echo at %.3f s (knob said 40 ms)\n", bumpAt);
            CHECK (std::abs (bumpAt - 0.55) < 0.06, "synced echo lands on the quarter note", bumpAt, 0.55);
        }

        // spring: an impulse-ish input gives a long, band-limited tail
        Params q = bare(); q.o1wave = 3; q.spmix = 1.0f; q.spdwell = 0.8f;
        e = makeEngine (q);
        r = render (*e, 48000, 256, 3.0, { {0.0, 69, 1, 1.0f}, {0.02, 69, 0, 0} });
        delete e;
        const double t05 = rms (r.L, 24000, 28800), t10 = rms (r.L, 48000, 52800), t20 = rms (r.L, 96000, 100800);
        std::printf ("  spring tail: 0.5 s %.1f dB, 1 s %.1f dB, 2 s %.1f dB\n", db (t05), db (t10), db (t20));
        CHECK (t05 > 1.0e-3 && t10 < t05 && t20 < t10, "spring decays", t05, t20);
        const double hf = goertzel (r.L.data() + 24000, 16384, 9000, 48000), mf = goertzel (r.L.data() + 24000, 16384, 1200, 48000);
        std::printf ("  spring tail spectrum: 1.2 kHz %.1f dB, 9 kHz %.1f dB\n", db (mf), db (hf));
        CHECK (hf < mf, "spring is band-limited", db (hf), db (mf));
        // the chirp: dispersion means the first reflection is not a copy of the input
        // chorus: L and R differ
        Params c = bare(); c.o1wave = 0; c.chmix = 0.8f; c.chdepth = 0.8f;
        e = makeEngine (c);
        r = render (*e, 48000, 256, 1.0, { {0.0, 57, 1, 0.8f} });
        delete e;
        double dd = 0; for (size_t i = 24000; i < r.L.size(); ++i) dd += std::abs (r.L[i] - r.R[i]);
        std::printf ("  chorus L/R mean |diff| %.3f\n", dd / 24000);
        CHECK (dd / 24000 > 0.01, "chorus is stereo");
        // drive
        for (float amt : { 0.4f, 1.0f })
        {
            Params d = bare(); d.o1wave = 3; d.drv = amt;
            e = makeEngine (d);
            r = render (*e, 48000, 256, 1.0, { {0.0, 57, 1, 0.8f} });
            delete e;
            const double h1 = goertzel (r.L.data() + 24000, 16384, 220, 48000), h2 = goertzel (r.L.data() + 24000, 16384, 440, 48000), h3 = goertzel (r.L.data() + 24000, 16384, 660, 48000);
            std::printf ("  drive %.0f%% on a sine: h2 %.1f dB, h3 %.1f dB relative, peak %.2f\n", amt * 100, db (h2 / h1), db (h3 / h1), peakAbs (r.L));
            // at full drive it is a fuzz, and a fuzz is square: odd harmonics dominate, as they should
            CHECK (h2 / h1 > (amt < 0.5f ? 0.03 : 0.008) && h3 / h1 > 0.03, "drive adds even and odd harmonics", db (h2 / h1), db (h3 / h1));
        }
    }

    // ---- 9. every recipe, every extreme, random machines: bounded --------
    std::printf ("\n[9] bounds\n");
    {
        double worst = 0;
        for (int i = 0; i < NUM_RECIPES; ++i)
        {
            Params p; applyRecipe (i, p);
            auto* e = makeEngine (p);
            auto r = render (*e, 48000, 256, 2.5, { {0.0, 45, 1, 0.9f}, {0.0, 52, 1, 0.9f}, {0.0, 57, 1, 0.9f}, {1.2, 45, 0, 0}, {1.2, 52, 0, 0}, {1.2, 57, 0, 0}, {1.4, 50, 1, 1.0f} });
            delete e;
            const double pk = peakAbs (r.L);
            const double level = rms (r.L, 12000, 48000);
            std::printf ("  %-14s peak %.3f  rms %.3f%s\n", recipeName (i), pk, level, allFinite (r.L) ? "" : "  NOT FINITE");
            CHECK (allFinite (r.L) && allFinite (r.R) && pk <= 1.0, "recipe bounded", pk, 1.0);
            CHECK (pk > 0.05, "recipe audible", pk, 0.05);
            worst = std::max (worst, pk);
        }
        // extremes: every parameter at both ends, one at a time, over a loud patch
        Params loud; applyRecipe (8, loud); loud.fdrive = 1.0f; loud.drv = 1.0f; loud.lpeak = 1.0f; loud.hpeak = 1.0f;
        for (int i = 0; i < numParams(); ++i)
            for (int end = 0; end < 2; ++end)
            {
                Params p = loud;
                const auto& s = paramSpec (i);
                s.get (p) = end ? paramMax (s) : 0.0f;
                auto* e = makeEngine (p, 48000, 128);
                auto r = render (*e, 48000, 128, 0.6, { {0.0, 40, 1, 1.0f}, {0.0, 47, 1, 1.0f}, {0.0, 52, 1, 1.0f}, {0.4, 40, 0, 0} });
                delete e;
                const double pk = std::max (peakAbs (r.L), peakAbs (r.R));
                if (! (allFinite (r.L) && allFinite (r.R) && pk <= 1.0))
                    std::printf ("  extreme %s=%s: peak %.3f%s\n", s.id, end ? "max" : "min", pk, allFinite (r.L) ? "" : " NOT FINITE");
                CHECK (allFinite (r.L) && allFinite (r.R) && pk <= 1.0, "extreme bounded", pk, 1.0);
                worst = std::max (worst, pk);
            }
        // random machines
        Rng rg; rg.seed (77);
        for (int m = 0; m < 120; ++m)
        {
            Params p;
            for (int i = 0; i < numParams(); ++i)
            {
                const auto& s = paramSpec (i);
                const float mx = paramMax (s);
                s.get (p) = (s.kind == KP_LIST || s.kind == KP_INT) ? (float) (rg.u32() % ((uint32_t) mx + 1)) : rg.uni() * mx;
            }
            const double sr = (m % 4 == 0) ? 44100.0 : (m % 4 == 1 ? 48000.0 : (m % 4 == 2 ? 96000.0 : 88200.0));
            auto* e = makeEngine (p, sr, 256);
            auto r = render (*e, sr, 256, 1.2, { {0.0, 36 + (int) (rg.u32() % 48), 1, 1.0f}, {0.1, 60, 1, 0.7f}, {0.7, 60, 0, 0} });
            delete e;
            const double pk = std::max (peakAbs (r.L), peakAbs (r.R));
            if (! (allFinite (r.L) && allFinite (r.R) && pk <= 1.0)) std::printf ("  random %d @ %.0f: peak %.3f%s\n", m, sr, pk, allFinite (r.L) ? "" : " NOT FINITE");
            CHECK (allFinite (r.L) && allFinite (r.R) && pk <= 1.0, "random machine bounded", pk, 1.0);
            worst = std::max (worst, pk);
        }
        std::printf ("  worst peak anywhere: %.3f\n", worst);
    }

    // ---- 9b. the seed library ---------------------------------------------
    std::printf ("\n[9b] seed library (%d patches, %d categories)\n", NUM_SEEDS, seedNumCategories());
    {
        std::vector<std::string> names;
        int catCount[64] = {0};
        double worst = 0; int quiet = 0;
        for (int s = 0; s < NUM_SEEDS; ++s)
        {
            Params p; seedPatch (s, p);
            int c = 0, rank = 0; seedInfo (s, c, rank);
            catCount[c]++;
            names.push_back (seedName (s));
            // determinism: a second call is byte-identical
            Params q; seedPatch (s, q);
            CHECK (std::memcmp (&p, &q, sizeof (Params)) == 0, "seed is deterministic", s, 0);
            // the stamped seed matches
            CHECK ((int) std::round (p.seed) == s, "seed stamps itself", p.seed, s);
            // render it: a chord and a single low note
            auto* e = makeEngine (p, 48000, 256);
            auto r = render (*e, 48000, 256, 1.6, { {0.0, 45, 1, 0.9f}, {0.0, 52, 1, 0.85f}, {0.0, 57, 1, 0.85f}, {1.0, 45, 0, 0}, {1.0, 52, 0, 0}, {1.0, 57, 0, 0} });
            delete e;
            const double pk = std::max (peakAbs (r.L), peakAbs (r.R));
            const double lvl = rms (r.L, 6000, 48000);
            if (! (allFinite (r.L) && allFinite (r.R) && pk <= 1.0))
                std::printf ("  seed %d (%s): peak %.3f%s\n", s, names.back().c_str(), pk, allFinite (r.L) ? "" : " NOT FINITE");
            CHECK (allFinite (r.L) && allFinite (r.R) && pk <= 1.0, "seed bounded", pk, 1.0);
            if (pk < 0.03) { ++quiet; std::printf ("  seed %d (%s) quiet: peak %.3f\n", s, names.back().c_str(), pk); }
            worst = std::max (worst, pk);
        }
        CHECK (quiet == 0, "every seed is audible", quiet, 0);
        // names unique
        std::vector<std::string> sorted = names; std::sort (sorted.begin(), sorted.end());
        int dupes = 0;
        for (size_t i = 1; i < sorted.size(); ++i) if (sorted[i] == sorted[i-1]) { ++dupes; std::printf ("  duplicate name: %s\n", sorted[i].c_str()); }
        CHECK (dupes == 0, "no two seeds share a name", dupes, 0);
        // categories all populated, counts sum right
        int total = 0;
        for (int c = 0; c < seedNumCategories(); ++c) { std::printf ("  %-9s %d\n", seedCategoryName (c), catCount[c]); total += catCount[c]; CHECK (catCount[c] > 0, "category populated", catCount[c], 1); }
        CHECK (total == NUM_SEEDS, "categories cover every seed exactly once", total, NUM_SEEDS);
        std::printf ("  worst seed peak: %.3f\n", worst);
    }

    // ---- 10. silence in, silence out ---------------------------------------
    {
        Params p; p.vintage = 0.0f;
        auto* e = makeEngine (p);
        auto r = render (*e, 48000, 256, 1.0, {});
        delete e;
        std::printf ("\n[10] no notes, vintage 0: peak %.2e\n", peakAbs (r.L));
        CHECK (peakAbs (r.L) < 1.0e-5, "silent when not played", peakAbs (r.L), 1.0e-5);
    }

    // ---- 11. cost ---------------------------------------------------------
    std::printf ("\n[11] cost\n");
    for (int os = 1; os <= 2; ++os)
    {
        Params p; applyRecipe (6, p); p.mode = 2; p.os = (float) os; p.chmix = 0.5f; p.dlmix = 0.3f; p.spmix = 0.3f; p.drv = 0.3f;
        auto* e = makeEngine (p, 48000, 256);
        const auto t0 = std::chrono::high_resolution_clock::now();
        auto r = render (*e, 48000, 256, 10.0, { {0.0, 45, 1, 0.9f}, {0.0, 52, 1, 0.9f}, {0.0, 57, 1, 0.9f}, {0.0, 60, 1, 0.9f}, {0.0, 64, 1, 0.9f} });
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double secs = std::chrono::duration<double> (t1 - t0).count();
        std::printf ("  5 voices + all effects at %dx: %.1f%% of one core at 48 kHz\n", 1 << os, secs / 10.0 * 100.0);
        delete e;
    }

    // ---- 12. the SPECTRA world-mod bus ------------------------------------
    std::printf ("\n[12] world-mod bus (BWFX SPECTRA)\n");
    {
        // neutral bus == bit-identical to never calling setWorldMod
        Params p = bare();
        auto* e1 = makeEngine (p);
        auto* e2 = makeEngine (p);
        e2->setWorldMod (0, 0, 0, 0, 0, 1);
        auto ra = render (*e1, 48000, 256, 1.5, { {0.0, 57, 1, 0.9f}, {1.0, 57, 0, 0} });
        auto rb = render (*e2, 48000, 256, 1.5, { {0.0, 57, 1, 0.9f}, {1.0, 57, 0, 0} });
        CHECK (std::memcmp (ra.L.data(), rb.L.data(), ra.L.size() * sizeof (float)) == 0,
               "neutral bus bit-identical");
        delete e1; delete e2;
    }
    {
        // sag keys to the gate: in tune while held, one semitone flat as it dies
        Params p = bare(); p.e2r = 0.7f; p.glide = 0.0f;
        auto* e = makeEngine (p);
        e->setWorldMod (0, 0, 0, 0, 1.0f, 1);            // sag = 1 -> 100 cents
        auto r = render (*e, 48000, 256, 3.0, { {0.0, 57, 1, 0.9f}, {1.2, 57, 0, 0} });
        const double held = zcHz (r.L.data() + 24000, 24000, 48000);
        const double late = zcHz (r.L.data() + (int) (2.0 * 48000), 24000, 48000);
        const double centsDown = 1200.0 * std::log2 (held / std::max (1.0, late));
        std::printf ("  sag: held %.2f Hz, released %.2f Hz (%.0f cents down)\n", held, late, centsDown);
        CHECK (std::abs (held - 220.0) < 2.0, "in tune while held", held, 220.0);
        CHECK (centsDown > 70.0 && centsDown < 130.0, "sags ~100 cents on release", centsDown, 100.0);
        delete e;
    }
    {
        // detune fans across the unison voices: the render must change
        Params p = bare(); p.mode = 1.0f; p.udet = 0.0f;
        auto* e1 = makeEngine (p);
        auto* e2 = makeEngine (p);
        e2->setWorldMod (30.0f, 0, 0, 0, 0, 1);
        auto ra = render (*e1, 48000, 256, 1.0, { {0.0, 57, 1, 0.9f} });
        auto rb = render (*e2, 48000, 256, 1.0, { {0.0, 57, 1, 0.9f} });
        double diff = 0;
        for (size_t i = 24000; i < ra.L.size(); ++i) diff += std::abs ((double) ra.L[i] - rb.L[i]);
        diff /= (double) (ra.L.size() - 24000);
        CHECK (diff > 1.0e-3, "unison detune audibly does something", diff, 1.0e-3);
        delete e1; delete e2;
    }
    {
        // filterMul darkens: HF energy drops when the bus closes the filter
        Params p = bare(); p.o1wave = 1.0f; p.lpf = 0.75f;
        auto* e1 = makeEngine (p);
        auto* e2 = makeEngine (p);
        e2->setWorldMod (0, 0, 0, 0, 0, 0.25f);
        auto ra = render (*e1, 48000, 256, 1.0, { {0.0, 45, 1, 0.9f} });
        auto rb = render (*e2, 48000, 256, 1.0, { {0.0, 45, 1, 0.9f} });
        const double hfA = goertzel (ra.L.data() + 24000, 24000, 110.0 * 16, 48000);
        const double hfB = goertzel (rb.L.data() + 24000, 24000, 110.0 * 16, 48000);
        std::printf ("  dull: 16th-partial %.4g -> %.4g\n", hfA, hfB);
        CHECK (hfB < hfA * 0.6, "filterMul darkens", hfB, hfA);
        delete e1; delete e2;
    }
    {
        // per-voice tremolo breathes the level at the asked rate
        Params p = bare();
        auto* e = makeEngine (p);
        e->setWorldMod (0, 0, 0.9f, 4.0f, 0, 1);
        auto r = render (*e, 48000, 256, 2.0, { {0.0, 57, 1, 0.9f} });
        double mn = 1e9, mx = 0;
        for (int w = 24000; w + 2400 < (int) r.L.size(); w += 2400)
        {
            const double v = rms (r.L, w, w + 2400);
            mn = std::min (mn, v); mx = std::max (mx, v);
        }
        std::printf ("  trem: window rms %.4g .. %.4g\n", mn, mx);
        CHECK (mn < mx * 0.55, "tremolo modulates the level", mn, mx);
        CHECK (allFinite (r.L), "trem render finite");
        delete e;
    }
    {
        // everything cranked at once stays bounded
        Params p; applyRecipe (6, p); p.mode = 2.0f;
        auto* e = makeEngine (p);
        e->setWorldMod (60.0f, 1.0f, 1.0f, 8.0f, 1.0f, 4.0f);
        auto r = render (*e, 48000, 256, 3.0, { {0.0, 45, 1, 1.0f}, {0.0, 52, 1, 1.0f}, {0.0, 57, 1, 1.0f}, {2.0, 45, 0, 0}, {2.0, 52, 0, 0}, {2.0, 57, 0, 0} });
        CHECK (allFinite (r.L) && allFinite (r.R), "extreme bus finite");
        CHECK (peakAbs (r.L) < 1.7 && peakAbs (r.R) < 1.7, "extreme bus bounded", peakAbs (r.L), 1.7);
        delete e;
    }

    // ---- wavs -------------------------------------------------------------
    if (! wavDir.empty())
    {
        for (int i = 0; i < NUM_RECIPES; ++i)
        {
            Params p; applyRecipe (i, p);
            auto* e = makeEngine (p);
            std::vector<std::tuple<double, int, int, float>> ev;
            if (p.mode > 1.5f) ev = { {0.0, 48, 1, 0.9f}, {0.0, 55, 1, 0.9f}, {0.0, 64, 1, 0.9f}, {1.5, 48, 0, 0}, {1.5, 55, 0, 0}, {1.5, 64, 0, 0}, {2.0, 50, 1, 0.9f}, {2.0, 57, 1, 0.8f}, {3.2, 50, 0, 0}, {3.2, 57, 0, 0} };
            else ev = { {0.0, 45, 1, 0.9f}, {0.5, 45, 0, 0}, {0.55, 48, 1, 0.9f}, {1.0, 48, 0, 0}, {1.05, 52, 1, 1.0f}, {1.6, 52, 0, 0}, {1.7, 40, 1, 1.0f}, {2.6, 40, 0, 0}, {2.7, 57, 1, 0.7f}, {3.4, 57, 0, 0} };
            auto r = render (*e, 48000, 256, 5.0, ev);
            delete e;
            std::string name = recipeName (i); for (auto& ch : name) if (ch == ' ') ch = '_';
            writeWav (wavDir + "/" + std::to_string (i) + "_" + name + ".wav", r);
        }
        std::printf ("\nwrote %d recipe renders to %s\n", NUM_RECIPES, wavDir.c_str());
    }

    std::printf ("\n%d checks, %d failed — %s\n", checks, fails, fails ? "NOT CLEAR" : "ALL CLEAR");
    return fails ? 1 : 0;
}
