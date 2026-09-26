// twrender: the offline render qualities, measured.
//   1. the default quality IS the live engine, sample for sample
//   2. HIGH: image sources to a higher order - more early reflections, the
//      same reverberant energy (the late field hands over exactly what the
//      extra images now carry), and what it costs
#include "Engine.h"
#include "OfflineRender.h"
#include "Fdtd.h"
#include <functional>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <memory>
#include <chrono>
using namespace tw;

static int checks = 0, failures = 0;
static void check (bool ok, const char* what, double a = 0, double b = 0)
{
    ++checks; if (! ok) ++failures;
    std::printf ("  %s  %s", ok ? "ok  " : "FAIL", what);
    if (a != 0 || b != 0) std::printf ("   [%.5g vs %.5g]", a, b);
    std::printf ("\n");
}
struct Take { std::vector<float> L, R; };
template <typename Gen>
static Take render (Engine& e, const Params& p, double fs, double seconds, Gen gen, int block = 256)
{
    Take t; const int n = (int) (seconds * fs);
    t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
    e.setParams (p);
    for (int i = 0; i < n; i += block)
    {
        const int m = std::min (block, n - i);
        for (int k = 0; k < m; ++k) { const float v = gen (i + k); t.L[(size_t) (i + k)] = v; t.R[(size_t) (i + k)] = v; }
        e.process (&t.L[(size_t) i], &t.R[(size_t) i], m);
    }
    return t;
}
static std::unique_ptr<Engine> fresh (double fs, const RenderQuality& q = RenderQuality()) { auto e = std::make_unique<Engine>(); e->prepare (fs, 256, q); return e; }
static double energy (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = std::max (a, 0); i < std::min (b, (int) v.size()); ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return s; }
static double db (double x) { return 10.0 * std::log10 (std::max (x, 1e-30)); }
static auto impulseAt (int at) { return [at] (int i) { return i == at ? 1.0f : 0.0f; }; }
static std::vector<float> octaveBand (const std::vector<float>& x, double fs, double fc)
{
    std::vector<float> y = x;
    for (int pass = 0; pass < 4; ++pass)
    {
        const double w = 2 * 3.14159265358979 * fc / fs, Q = 1.4142;
        const double alpha = std::sin (w) / (2 * Q), cw = std::cos (w);
        const double b0 = alpha, b2 = -alpha, a0 = 1 + alpha, a1 = -2 * cw, a2 = 1 - alpha;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (auto& v : y) { const double in = v; const double out = (b0 * in + b2 * x2 - a1 * y1 - a2 * y2) / a0; x2 = x1; x1 = in; y2 = y1; y1 = out; v = (float) out; }
    }
    return y;
}
static int reflections (const Engine& e) { int n = 0; for (int i = 0; i < e.numSpecs(); ++i) { const auto k = e.specAt (i).kind; if (k == PathKind::Refl1 || k == PathKind::Refl2) ++n; } return n; }
// peaks per millisecond above a threshold in a window: a count of distinct arrivals
static double arrivals (const std::vector<float>& h, int a, int b, float thr)
{
    int n = 0;
    for (int i = std::max (a, 1); i < std::min (b, (int) h.size() - 1); ++i)
        if (std::abs (h[(size_t) i]) > thr && std::abs (h[(size_t) i]) >= std::abs (h[(size_t) i - 1]) && std::abs (h[(size_t) i]) > std::abs (h[(size_t) i + 1])) ++n;
    return n;
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double fs = 48000.0;
    std::printf ("THIN WALLS - render quality bench\n\n1. the default is the live engine\n");
    {
        Params p; p.material[0] = 3;
        auto gen = [] (int i) { unsigned s = (unsigned) i * 2654435761u; return i < 24000 ? ((s >> 9) / 4194304.0f - 1.0f) * 0.3f : 0.0f; };
        auto e1 = std::make_unique<Engine>(); e1->prepare (fs, 256);
        Take a = render (*e1, p, fs, 1.0, gen);
        auto e2 = fresh (fs, RenderQuality());
        Take b = render (*e2, p, fs, 1.0, gen);
        check (std::memcmp (a.L.data(), b.L.data(), a.L.size() * 4) == 0 && std::memcmp (a.R.data(), b.R.data(), a.R.size() * 4) == 0,
               "prepare() with no quality and with the default quality: byte-identical");
    }

    std::printf ("\n2. HIGH: image sources to a higher order\n");
    {
        // the tiled living room, doors shut: the room where the order shows most
        Params p; p.material[0] = 3; p.door[0] = p.door[1] = p.door[2] = 0;
        p.src[0].x = 1.8f; p.src[0].y = 1.5f; p.src[0].z = 1.4f; p.src[0].type = SRC_PURE;
        p.lisX = 4.2f; p.lisY = 3.6f;
        double eLate[3] = {}, eAll[3] = {}, dens[3] = {};
        int refl[3] = {};
        const int orders[3] = { 2, 4, 6 };
        double cost[3] = {};
        for (int k = 0; k < 3; ++k)
        {
            RenderQuality q; q.order = orders[k]; q.maxPaths = 1200; q.maxSlots = 1400;
            if (k == 0) q = RenderQuality();
            auto e = fresh (fs, q);
            const auto t0 = std::chrono::steady_clock::now();
            Take t = render (*e, p, fs, 2.0, impulseAt (4800));
            cost[k] = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 2.0;
            refl[k] = reflections (*e);
            const auto b = octaveBand (t.L, fs, 1000);
            eAll[k] = energy (b, 4800, (int) t.L.size());
            eLate[k] = energy (b, 4800 + (int) (0.08 * fs), (int) t.L.size());
            // arrivals: the image paths alone (the late field muted), 12-50 ms after the pulse
            Params pe = p; pe.reverbDb = -120;
            auto ee = fresh (fs, q);
            Take te = render (*ee, pe, fs, 0.2, impulseAt (4800));
            dens[k] = arrivals (te.L, 4800 + (int) (0.012 * fs), 4800 + (int) (0.050 * fs), 0.0005f);
            std::printf ("    order %d: %d reflections, %.0f distinct arrivals 12-50 ms, total %.2f dB, after 80 ms %.2f dB, %.2fx real time\n",
                         orders[k], refl[k], dens[k], db (eAll[k]), db (eLate[k]), cost[k]);
        }
        check (refl[1] > 4 * refl[0] && refl[2] > refl[1], "order 4 and 6 add image sources (count)", refl[1], refl[0]);
        check (dens[1] > dens[0] * 1.5, "and more distinct early arrivals (12-50 ms)", dens[1], dens[0]);
        const double d4 = db (eAll[1]) - db (eAll[0]), d6 = db (eAll[2]) - db (eAll[0]);
        char buf[200]; std::snprintf (buf, sizeof buf, "total energy at 1 kHz holds: %+.2f dB at order 4, %+.2f dB at order 6 (the late field hands over)", d4, d6);
        check (std::abs (d4) < 1.0 && std::abs (d6) < 1.0, buf, d4, d6);
        std::snprintf (buf, sizeof buf, "cost: order 4 %.2fx real time, order 6 %.2fx (offline only)", cost[1], cost[2]);
        check (cost[1] < 6.0, buf, cost[1], 6.0);
    }

    std::printf ("\n3. a personal head from a SOFA file\n");
    {
        // libmysofa's own copy of the MIT KEMAR measurement: loaded the new way it
        // must reproduce the built-in set, which came from the same head
        auto sofa = std::make_shared<Hrtf>();
        std::string err;
        const bool ok = sofa->loadSofa (TW_SOFA_KEMAR, fs, err);
        check (ok, ok ? "MIT_KEMAR_normal_pinna.sofa loads" : ("load failed: " + err).c_str());
        auto bogus = std::make_shared<Hrtf>();
        std::string e2;
        const bool refused = ! bogus->loadSofa (__FILE__, fs, e2);
        check (refused && ! e2.empty(), ("a file that is not SOFA is refused with a reason: " + e2).c_str());
        if (ok)
        {
            // the hall, absorbing, direct sound only: a source 2 m to the right, then in front
            auto measure = [&] (bool personal, float azDeg, double& itdUs, double& ild4k, double& ildAll)
            {
                Params p; p.material[2] = 0; p.door[1] = 0; p.door[2] = 0;
                p.lisX = 12.0f; p.lisY = 4.5f; p.lisYaw = 90.0f;
                const float a = (90.0f - azDeg) * 3.14159265f / 180.0f;
                p.src[0].x = p.lisX + 2.0f * std::cos (a); p.src[0].y = p.lisY + 2.0f * std::sin (a); p.src[0].z = EAR_HEIGHT;
                p.src[0].type = SRC_PURE; p.earlyDb = -120; p.reverbDb = -120;
                auto e = fresh (fs);
                if (personal) e->setHrtf (sofa);
                Take t = render (*e, p, fs, 0.25, impulseAt (2400));
                const int a0 = 2400, a1 = 2400 + (int) (0.03 * fs);
                const int maxLag = (int) (0.0012 * fs);
                double best = -1e30; int bl = 0;
                for (int lag = -maxLag; lag <= maxLag; ++lag)
                {
                    double s = 0;
                    for (int i = a0; i < a1; ++i) { const int j = i + lag; if (j >= 0 && j < (int) t.R.size()) s += (double) t.L[(size_t) i] * t.R[(size_t) j]; }
                    if (s > best) { best = s; bl = lag; }
                }
                itdUs = bl / fs * 1e6;
                const auto l4 = octaveBand (t.L, fs, 4000), r4 = octaveBand (t.R, fs, 4000);
                ild4k = db (energy (r4, a0, a1)) - db (energy (l4, a0, a1));
                ildAll = db (energy (t.R, a0, a1)) - db (energy (t.L, a0, a1));
            };
            double i0, g0, a0, i1, g1, a1;
            measure (false, 90.0f, i0, g0, a0);
            measure (true, 90.0f, i1, g1, a1);
            char buf[220];
            std::snprintf (buf, sizeof buf, "source to the right: ITD %.0f us built-in, %.0f us from the SOFA file", i0, i1);
            check (std::abs (i1 - i0) < 60.0 && i1 < -400.0, buf, i1, i0);
            std::snprintf (buf, sizeof buf, "and the level difference at 4 kHz: %.1f dB built-in, %.1f dB from the file", g0, g1);
            check (std::abs (g1 - g0) < 3.0 && g1 > 6.0, buf, g1, g0);
            double f0, h0, b0, f1, h1, b1;
            measure (false, 0.0f, f0, h0, b0);
            measure (true, 0.0f, f1, h1, b1);
            std::snprintf (buf, sizeof buf, "straight ahead the loaded head is balanced: ITD %.0f us, ILD %.2f dB", f1, b1);
            check (std::abs (f1) < 25.0 && std::abs (b1) < 1.5, buf, f1, b1);
            check (sofa->personal() && sofa->name() == "MIT_KEMAR_normal_pinna.sofa", ("it is named after its file: " + sofa->name()).c_str());
        }
    }

    std::printf ("\n4. ULTRA: the late field traced through the rooms\n");
    {
        // a take of an impulse, through renderTake, as the export runs it
        auto take = [&] (const Params& base, double seconds, SoundQuality q, int* keyframes = nullptr,
                         std::function<void (int, Params&)> move = nullptr, double* raySec = nullptr)
        {
            const int n = (int) (seconds * fs);
            std::vector<float> in ((size_t) n, 0.0f); in[4800] = 1.0f;
            TakeView T; T.rate = fs; T.length = n; T.in[0] = in.data(); T.in[1] = in.data();
            T.pblock = 256; T.nblocks = (n + 255) / 256;
            T.paramsAt = [&] (int b, Params& P) { P = base; if (move) move (b, P); };
            RenderOptions o; o.quality = q;
            RenderOutput out;
            renderTake (T, o, out);
            if (keyframes) *keyframes = out.keyframes;
            if (raySec) *raySec = out.raySeconds;
            return out;
        };
        auto bandE = [&] (const RenderOutput& o, int a, int b) { return energy (octaveBand (o.L, fs, 1000), a, b) + energy (octaveBand (o.R, fs, 1000), a, b); };
        // Schroeder decay of the 1 kHz band from t0: T20 (-5 .. -25 dB) x 3
        auto t20 = [&] (const RenderOutput& o, int from) {
            auto bl = octaveBand (o.L, fs, 1000), br = octaveBand (o.R, fs, 1000);
            const int n = (int) bl.size();
            std::vector<double> sch ((size_t) n + 1, 0.0);
            for (int i = n - 1; i >= from; --i) sch[(size_t) i] = sch[(size_t) i + 1] + (double) bl[(size_t) i] * bl[(size_t) i] + (double) br[(size_t) i] * br[(size_t) i];
            const double top = sch[(size_t) from];
            int i5 = -1, i25 = -1;
            for (int i = from; i < n; ++i) { const double d = db (sch[(size_t) i] / top); if (i5 < 0 && d <= -5) i5 = i; if (i25 < 0 && d <= -25) { i25 = i; break; } }
            if (i5 < 0 || i25 < 0) return -1.0;
            return 3.0 * (i25 - i5) / fs;
        };

        // (a) same room, doors shut: the traced field carries the energy the statistical one did
        Params base; base.door[0] = base.door[1] = base.door[2] = 0;
        base.src[0].x = 1.8f; base.src[0].y = 1.5f; base.src[0].z = 1.4f; base.src[0].type = SRC_PURE;
        base.lisX = 4.2f; base.lisY = 3.6f;
        const double r = std::sqrt (2.4 * 2.4 + 2.1 * 2.1 + 0.25 * 0.25);
        const int after = 4800 + (int) (r / 343.0 * fs) + 150;
        const char* mn[] = { "ABSORBING", "FURNISHED", "PLASTER", "TILED", "STUDIO" };
        for (int mat : { 1, 2, 3, 4 })
        {
            Params p = base; p.material[0] = mat;
            const double secs = mat == 3 ? 5.0 : 3.0;
            double rs = 0;
            const auto live = take (p, secs, SoundQuality::Live);
            const auto ult = take (p, secs, SoundQuality::Ultra, nullptr, nullptr, &rs);
            const double dE = db (bandE (ult, after, (int) live.L.size())) - db (bandE (live, after, (int) live.L.size()));
            // the decay: the traced field alone against Eyring
            Params pr = p; pr.directDb = -120; pr.earlyDb = -120;
            const auto rayOnly = take (pr, secs, SoundQuality::Ultra);
            float rt[NBAND]; Engine::eyringRt60 (0, p.material, p.door, rt);
            const double T = t20 (rayOnly, 4800 + (int) (0.02 * fs));
            char buf[240];
            std::snprintf (buf, sizeof buf, "LARGE %-9s reverberant energy at 1 kHz: ULTRA %+.2f dB against LIVE", mn[mat], dE);
            check (std::abs (dE) < 3.0, buf, dE, 3.0);
            std::snprintf (buf, sizeof buf, "LARGE %-9s traced decay T20 %.2f s, Eyring %.2f s (%.1fx real time to trace)", mn[mat], T, rt[3], rs / secs);
            check (T > 0.7 * rt[3] && T < 1.35 * rt[3], buf, T, rt[3]);
        }

        // (b) coupled rooms: a dead room next to a live hall, the door between them
        {
            Params p = base; p.material[0] = 1; p.material[2] = 3;
            const auto shut = take (p, 4.0, SoundQuality::Ultra);
            p.door[1] = 1.0f;
            const auto open = take (p, 4.0, SoundQuality::Ultra);
            const int a = 4800 + (int) (1.0 * fs), b = 4800 + (int) (2.0 * fs);
            const double gain = db (bandE (open, a, b)) - db (bandE (shut, a, b));
            char buf[200]; std::snprintf (buf, sizeof buf, "furnished room, door to a tiled hall: 1-2 s after, %+.1f dB with the door open (the hall's tail)", gain);
            check (gain > 10.0, buf, gain, 10.0);
        }

        // (c) through the wall: the hall's own tail, heard in the next room with every door shut
        {
            Params p; p.door[0] = p.door[1] = p.door[2] = 0; p.material[0] = p.material[2] = 2;
            p.src[0].x = 12.0f; p.src[0].y = 4.5f; p.src[0].z = 1.5f; p.src[0].type = SRC_PURE;
            p.lisX = 3.0f; p.lisY = 2.5f;
            const auto live = take (p, 4.0, SoundQuality::Live);
            const auto ult = take (p, 4.0, SoundQuality::Ultra);
            const int a = 4800 + (int) (0.1 * fs);
            const double dE = db (bandE (ult, a, (int) ult.L.size())) - db (bandE (live, a, (int) live.L.size()));
            char buf[200]; std::snprintf (buf, sizeof buf, "source in the hall, listener next door, doors shut: ULTRA %+.1f dB against LIVE after 100 ms", dE);
            check (std::abs (dE) < 6.0, buf, dE, 6.0);
        }

        // (c2) the standalone's own default room, a loudspeaker, noise: every band against LIVE, at both common rates
        for (const double fs : { 48000.0, 44100.0 })
        {
            Params p;          // defaults: loudspeaker at 3, 2.5; listener at 4.5, 2.5; two doors open
            const int n = (int) (3.0 * fs);
            std::vector<float> in ((size_t) n, 0.0f);
            unsigned sd = 12345u; for (int i = 4800; i < n; ++i) { sd = sd * 1664525u + 1013904223u; in[(size_t) i] = ((sd >> 9) / 4194304.0f - 1.0f) * 0.2f; }
            auto run = [&] (SoundQuality q) {
                TakeView T; T.rate = fs; T.length = n; T.in[0] = in.data(); T.in[1] = in.data();
                T.pblock = 256; T.nblocks = (n + 255) / 256;
                T.paramsAt = [&] (int, Params& P) { P = p; };
                RenderOptions o; o.quality = q; RenderOutput out; renderTake (T, o, out); return out; };
            const auto lv = run (SoundQuality::Live), ul = run (SoundQuality::Ultra);
            double worst = 0; char line[300]; int at = 0;
            at += std::snprintf (line + at, sizeof line - (size_t) at, "default room, loudspeaker, noise, %.1f kHz: ULTRA against LIVE", fs / 1000.0);
            for (double fc : { 125.0, 500.0, 2000.0, 8000.0 })
            {
                const double d = db (energy (octaveBand (ul.L, fs, fc), 9600, n) + energy (octaveBand (ul.R, fs, fc), 9600, n))
                               - db (energy (octaveBand (lv.L, fs, fc), 9600, n) + energy (octaveBand (lv.R, fs, fc), 9600, n));
                worst = std::max (worst, std::abs (d));
                at += std::snprintf (line + at, sizeof line - (size_t) at, "  %.0f Hz %+.1f", fc, d);
            }
            check (worst < 2.0, line, worst, 2.0);
            if (fs == 44100.0)
            {
                // noise has no DC, but its running sum wanders: a source that injects air
                // without taking it back fills the room (0.78 of full scale in 4 s, found live)
                const auto ub = run (SoundQuality::UltraBass);
                double mean = 0, peak = 0;
                for (size_t i = 0; i < ub.L.size(); ++i) { mean += ub.L[i]; peak = std::max (peak, (double) std::abs (ub.L[i])); }
                mean /= (double) ub.L.size();
                const double d125 = db (energy (octaveBand (ub.L, fs, 125), 9600, n)) - db (energy (octaveBand (ul.L, fs, 125), 9600, n));
                char b2[240]; std::snprintf (b2, sizeof b2, "the same at ULTRA+BASS: mean %.5f (no air piling up), peak %.3f, 125 Hz %+.1f dB against ULTRA", mean, peak, d125);
                check (std::abs (mean) < 1e-3 && peak < 0.5 && std::abs (d125) < 5.0, b2, mean, d125);

                // a train of one-sided clicks carries DC: the traced tail must not ring with it
                std::vector<float> clk ((size_t) n, 0.0f);
                for (int i = 4800; i < n; i += 11025) for (int k = 0; k < 60; ++k) clk[(size_t) (i + k)] = 0.5f * std::exp (-k / 12.0f);
                // the traced field ALONE (direct and images trimmed off): the image paths pass DC,
                // every one of them, and that is the image method, not the tail
                auto runIn = [&] (SoundQuality q) {
                    TakeView T; T.rate = fs; T.length = n; T.in[0] = clk.data(); T.in[1] = clk.data();
                    T.pblock = 256; T.nblocks = (n + 255) / 256;
                    T.paramsAt = [&] (int, Params& P) { P = p; P.directDb = -120; P.earlyDb = -120; };
                    RenderOptions o; o.quality = q; RenderOutput out; renderTake (T, o, out); return out; };
                const auto cu = runIn (SoundQuality::Ultra);
                double mu = 0, su = 0;
                for (size_t i = 0; i < cu.L.size(); ++i) { mu += cu.L[i]; su += (double) cu.L[i] * cu.L[i]; }
                mu /= (double) cu.L.size(); su = std::sqrt (su / (double) cu.L.size());
                std::snprintf (b2, sizeof b2, "one-sided clicks, the traced tail alone: mean %.6f against rms %.5f (it does not ring at DC)", mu, su);
                check (std::abs (mu) < 0.02 * su, b2, mu, su);
            }
        }

        // (d) deterministic, and a moving listener retraces as it walks
        {
            Params p = base; p.material[0] = 2;
            const auto a1 = take (p, 1.5, SoundQuality::Ultra), a2 = take (p, 1.5, SoundQuality::Ultra);
            check (std::memcmp (a1.L.data(), a2.L.data(), a1.L.size() * 4) == 0, "the same take renders to the same samples");
            int kf = 0; double rs = 0;
            auto walk = [&] (int b, Params& P) { const float u = std::min (1.0f, b * 256.0f / (float) (3.0 * fs)); P.lisX = 1.5f + 3.0f * u; };
            const auto w = take (p, 3.0, SoundQuality::Ultra, &kf, walk, &rs);
            bool finite = true; for (float v : w.L) if (! std::isfinite (v)) finite = false;
            char buf[200]; std::snprintf (buf, sizeof buf, "a 3 m walk in 3 s: traced again %d times (every 0.25 m), %.1f s to render, finite", kf, rs);
            check (finite && kf >= 10 && kf <= 16, buf, kf, 12);
        }
    }

    std::printf ("\n5. ULTRA+BASS: the low end as a wave\n");
    {
        auto lowPass220 = [&] (std::vector<float> x) {
            for (int pass = 0; pass < 2; ++pass)
            {
                const double w = 2 * 3.14159265358979 * 220.0 / fs, cw = std::cos (w), al = std::sin (w) / (2 * 0.70710678);
                const double a0 = 1 + al, b0 = (1 - cw) / 2 / a0, b1 = (1 - cw) / a0, a1 = -2 * cw / a0, a2 = (1 - al) / a0;
                double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                for (auto& v : x) { const double y = b0 * v + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = v; y2 = y1; y1 = y; v = (float) y; }
            }
            return x;
        };
        auto runWave = [&] (const Params& p, double seconds, FdtdStats& st, std::function<void (int, Params&)> move = nullptr) {
            const int n = (int) (seconds * fs);
            const int nb = (n + 255) / 256;
            std::vector<Params> blocks ((size_t) nb, p);
            if (move) for (int b = 0; b < nb; ++b) move (b, blocks[(size_t) b]);
            std::vector<float> mono[MAX_SOURCES];
            mono[0].assign ((size_t) n, 0.0f); mono[0][4800] = 1.0f;
            Take t;
            fdtdLowBand (blocks, 256, mono, n, fs, t.L, t.R, &st);
            return t;
        };

        // (a) free space: the direct wave has the engine's level, signal over r, and arrives on time
        {
            Params p; p.material[2] = 0; p.floorMat[2] = 0; p.ceilMat[2] = 0; p.door[1] = p.door[2] = 0;
            p.src[0].x = 11.0f; p.src[0].y = 4.5f; p.src[0].z = 2.8f; p.lisX = 13.0f; p.lisY = 4.5f; p.lisYaw = 180.0f;
            FdtdStats st;
            Take t = runWave (p, 0.3, st);
            const auto lo = lowPass220 (t.L);
            std::vector<float> ref ((size_t) t.L.size(), 0.0f);
            const double r = std::sqrt (2.0 * 2.0 + (2.8 - EAR_HEIGHT) * (2.8 - EAR_HEIGHT));
            // the left ear is the far one, a little further than the head centre
            const double rl = std::sqrt ((2.0 + 0.0875) * (2.0 + 0.0875) + (2.8 - EAR_HEIGHT) * (2.8 - EAR_HEIGHT));
            const int at = 4800 + (int) std::lround (rl / SPEED_OF_SOUND * fs) + FracDelay::HALF;
            ref[(size_t) at] = (float) (1.0 / rl);
            // the wave is fed and heard through a 10 Hz high pass, twice (a cone returns to rest): so is the reference
            for (int pass = 0; pass < 2; ++pass)
            {
                const double w = 2 * 3.14159265358979 * 10.0 / fs, cw = std::cos (w), al = std::sin (w) / (2 * 0.70710678);
                const double a0 = 1 + al, b0 = (1 + cw) / 2 / a0, b1 = -(1 + cw) / a0, a1 = -2 * cw / a0, a2 = (1 - al) / a0;
                double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                for (auto& v : ref) { const double y = b0 * v + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = v; y2 = y1; y1 = y; v = (float) y; }
            }
            const auto rlo = lowPass220 (ref);
            float pk = 0, pr = 0; int ipk = 0, ipr = 0;
            for (int i = 4800; i < at + (int) (0.004 * fs); ++i)
            {
                if (std::abs (lo[(size_t) i]) > pk) { pk = std::abs (lo[(size_t) i]); ipk = i; }
                if (std::abs (rlo[(size_t) i]) > pr) { pr = std::abs (rlo[(size_t) i]); ipr = i; }
            }
            char buf[220];
            std::snprintf (buf, sizeof buf, "free field, 2.3 m: direct wave below 220 Hz %+.2f dB against 1/r, %+.2f ms late (grid %.1f cm at %.0f Hz, %lld air cells)",
                           db ((double) pk * pk) - db ((double) pr * pr), (ipk - ipr) * 1000.0 / fs, st.dx * 100, st.rate, st.cells);
            check (std::abs (db ((double) pk * pk) - db ((double) pr * pr)) < 1.0 && std::abs (ipk - ipr) < (int) (0.0006 * fs), buf, (double) pk, (double) pr);
            (void) r;
        }

        // (b) the room's own resonances: a hard room, corner to corner
        {
            Params p; p.material[0] = 3; p.door[0] = p.door[1] = p.door[2] = 0;
            p.src[0].x = 0.2f; p.src[0].y = 0.2f; p.src[0].z = 0.2f; p.lisX = 5.8f; p.lisY = 4.8f;
            FdtdStats st;
            Take t = runWave (p, 3.0, st);
            auto peakIn = [&] (double f0, double f1) {
                double best = 0, bf = 0;
                for (double f = f0; f <= f1; f += 0.1)
                {
                    double re = 0, im = 0;
                    for (size_t i = 4800; i < t.L.size(); ++i) { const double ph = 2 * 3.14159265358979 * f * (double) i / fs; re += t.L[i] * std::cos (ph); im += t.L[i] * std::sin (ph); }
                    const double m = re * re + im * im;
                    if (m > best) { best = m; bf = f; }
                }
                return bf;
            };
            const double fx = peakIn (24, 31.5), fy = peakIn (31.5, 38);
            const double ex = SPEED_OF_SOUND / (2 * 6.0), ey = SPEED_OF_SOUND / (2 * 5.0);
            char buf[220];
            std::snprintf (buf, sizeof buf, "hard living room: axial modes at %.1f and %.1f Hz, c/2L gives %.1f and %.1f (%.1fx real time)", fx, fy, ex, ey, st.seconds / 3.0);
            check (std::abs (fx - ex) / ex < 0.03 && std::abs (fy - ey) / ey < 0.03, buf, fx, ex);
        }

        // the take, through renderTake, at ULTRA and ULTRA+BASS
        auto take = [&] (const Params& base, double seconds, SoundQuality q, RenderOutput* info = nullptr, std::function<void (int, Params&)> move = nullptr) {
            const int n = (int) (seconds * fs);
            std::vector<float> in ((size_t) n, 0.0f); in[4800] = 1.0f;
            TakeView T; T.rate = fs; T.length = n; T.in[0] = in.data(); T.in[1] = in.data();
            T.pblock = 256; T.nblocks = (n + 255) / 256;
            T.paramsAt = [&] (int b, Params& P) { P = base; if (move) move (b, P); };
            RenderOptions o; o.quality = q;
            RenderOutput out;
            renderTake (T, o, out);
            if (info) *info = out;
            return out;
        };
        auto bandE = [&] (const RenderOutput& o, double fc) { return energy (octaveBand (o.L, fs, fc), 4800, (int) o.L.size()) + energy (octaveBand (o.R, fs, fc), 4800, (int) o.R.size()); };

        // (c) above the crossover nothing changes; below it the wave takes over at a like level
        {
            Params p; p.material[0] = 1; p.door[0] = p.door[1] = p.door[2] = 0;
            p.src[0].x = 1.8f; p.src[0].y = 1.5f; p.src[0].z = 1.4f; p.src[0].type = SRC_PURE; p.lisX = 4.2f; p.lisY = 3.6f;
            RenderOutput info;
            const auto u = take (p, 2.0, SoundQuality::Ultra), ub = take (p, 2.0, SoundQuality::UltraBass, &info);
            const double d1k = db (bandE (ub, 1000)) - db (bandE (u, 1000));
            const double d125 = db (bandE (ub, 125)) - db (bandE (u, 125));
            char buf[220];
            std::snprintf (buf, sizeof buf, "furnished room: 1 kHz %+.2f dB (untouched above the crossover), 125 Hz %+.1f dB (the wave against the rays)", d1k, d125);
            check (std::abs (d1k) < 0.5 && std::abs (d125) < 5.0, buf, d1k, d125);
            std::snprintf (buf, sizeof buf, "a 2 s take at ULTRA+BASS: wave %.1f s, rays %.1f s", info.fdtdSeconds, info.raySeconds);
            check (info.fdtdSeconds < 20.0, buf, info.fdtdSeconds, 20.0);
        }

        // (d) the bass through the wall stays: the wave has no walls to pass through, the geometric model does
        {
            Params p; p.door[0] = p.door[1] = p.door[2] = 0; p.material[0] = p.material[2] = 2;
            p.src[0].x = 12.0f; p.src[0].y = 4.5f; p.src[0].z = 1.5f; p.src[0].type = SRC_PURE; p.lisX = 3.0f; p.lisY = 2.5f;
            const auto u = take (p, 3.0, SoundQuality::Ultra), ub = take (p, 3.0, SoundQuality::UltraBass);
            const double d = db (bandE (ub, 125)) - db (bandE (u, 125));
            char buf[200]; std::snprintf (buf, sizeof buf, "hall next door, doors shut: 125 Hz %+.1f dB at ULTRA+BASS against ULTRA", d);
            check (std::abs (d) < 3.0, buf, d, 3.0);
        }

        // (f) a long take stays a long take: one kick, then silence, and the field must
        //     die away. The constant part of the grid's pressure is inaudible (the ears
        //     high-pass it), so it is measured in the grid itself - a float 1/3 that
        //     rounded up once let it grow threefold every three seconds until a real
        //     take detonated 88 s in.
        {
            Params p;
            p.src[0].x = 3.0f; p.src[0].y = 2.5f; p.src[0].z = 1.3f; p.lisX = 4.5f; p.lisY = 3.2f;
            const double secs = 16.0;
            const int n = (int) (secs * fs), nb = (n + 255) / 256;
            std::vector<Params> blocks ((size_t) nb, p);
            std::vector<float> mono[MAX_SOURCES];
            mono[0].assign ((size_t) n, 0.0f);
            for (int i = 0; i < (int) (0.5 * fs); ++i) { const double t = i / fs; mono[0][(size_t) i] = (float) (0.8 * std::sin (2 * 3.14159265358979 * 55.0 * t) * std::exp (-t / 0.15)); }
            FdtdStats st; std::vector<float> l, r;
            fdtdLowBand (blocks, 256, mono, n, fs, l, r, &st);
            const size_t k = st.fieldAbs.size();
            const double early = k > 2 ? st.fieldAbs[2] : 0, late = k > 0 ? st.fieldAbs[k - 1] : 1e30, mid = k > 8 ? st.fieldAbs[8] : 0;
            char buf[220]; std::snprintf (buf, sizeof buf, "one kick, then 15 s of silence: the grid's field falls %.0f dB and keeps falling (nothing grows where the ears cannot hear)", -2.0 * db (late / std::max (early, 1e-30)));
            check (k >= 15 && late < 0.05 * early && late <= mid, buf, late, early);
        }

        // (e) a door swinging open mid-take rebuilds the air, and nothing blows up
        {
            Params p; p.material[0] = 2; p.material[2] = 2; p.door[0] = p.door[1] = p.door[2] = 0;
            p.src[0].x = 12.0f; p.src[0].y = 3.0f; p.src[0].z = 1.5f; p.lisX = 4.0f; p.lisY = 2.5f;
            FdtdStats st;
            auto swing = [&] (int b, Params& P) { P.door[1] = std::min (1.0f, std::max (0.0f, (b * 256.0f / (float) fs - 0.3f) / 1.0f)); };
            Take t = runWave (p, 2.0, st, swing);
            bool finite = true; float mx = 0; for (float v : t.L) { if (! std::isfinite (v)) finite = false; mx = std::max (mx, std::abs (v)); }
            char buf[200]; std::snprintf (buf, sizeof buf, "door to the hall opening over 1 s: the air rebuilt %d times, bounded (peak %.3f)", st.rebuilds, mx);
            check (finite && st.rebuilds >= 10 && mx < 1.0f, buf, st.rebuilds, mx);
        }
    }

    std::printf ("\n%d checks, %d failed  -  %s\n", checks, failures, failures ? "FAILURES" : "ALL CLEAR");
    return failures ? 1 : 0;
}
