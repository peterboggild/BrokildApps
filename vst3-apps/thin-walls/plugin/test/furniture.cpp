// twfurn: the furniture bench. Every claim the furniture makes, measured.
//   1. an empty layout changes nothing, sample for sample
//   2. absorption: the engine's RT60 is Eyring's with the furniture's area in A,
//      the rug's floor subtraction is exact, and a rendered decay follows it
//   3. scattering: each wall bounce loses exactly 10 log10 (1 - s)
//   4. occlusion: a bookcase between source and listener costs the direct path
//      the Maekawa loss of the shortest way round it, computed independently
//      here, both in the path table and in the rendered audio
//   5. continuity: sliding past its edge, over its top, the loss has no step
//   6. a table top reflects, and the reflection fades off the edge
//   7. cost with four sources and a furnished apartment
#include "Engine.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <memory>
#include <chrono>
#include <cstdlib>
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
static std::unique_ptr<Engine> fresh (double fs) { auto e = std::make_unique<Engine>(); e->prepare (fs, 256); return e; }
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
static double rt60From (const std::vector<float>& h, double fs, int from)
{
    std::vector<double> edc (h.size(), 0.0); double acc = 0;
    for (int i = (int) h.size() - 1; i >= from; --i) { acc += (double) h[(size_t) i] * h[(size_t) i]; edc[(size_t) i] = acc; }
    const double top = db (edc[(size_t) from]); int i5 = -1, i25 = -1;
    for (int i = from; i < (int) h.size(); ++i) { const double d = db (edc[(size_t) i]) - top; if (i5 < 0 && d <= -5) i5 = i; if (i25 < 0 && d <= -25) { i25 = i; break; } }
    if (i5 < 0 || i25 < 0) return -1;
    return 3.0 * (i25 - i5) / fs;
}
static float maekawaDb (float delta, float f)
{
    const float N = 2.0f * delta * f / SPEED_OF_SOUND;
    return Engine::diffractionDb (N);
}
static const PathSpec* findKey (const Engine& e, uint32_t key)
{
    for (int i = 0; i < e.numSpecs(); ++i) if (e.specAt (i).key == key) return &e.specAt (i);
    return nullptr;
}
static void put (Params& p, int type, float x, float y, float yaw)
{
    p.furn[p.nfurn].type = type; p.furn[p.nfurn].x = x; p.furn[p.nfurn].y = y; p.furn[p.nfurn].yaw = yaw; ++p.nfurn;
}

int main()
{
    const double fs = 48000.0;
    std::printf ("THIN WALLS - furniture bench\n\n1. an empty layout changes nothing\n");
    {
        Params p; p.material[0] = 3;
        auto gen = [] (int i) { unsigned s = (unsigned) i * 2654435761u; return i < 48000 ? ((s >> 9) / 4194304.0f - 1.0f) * 0.3f : 0.0f; };
        auto e1 = fresh (fs); Take a = render (*e1, p, fs, 1.5, gen);
        Params q = p; for (int i = 0; i < 4; ++i) q.furn[i].type = -1; q.nfurn = 4;
        auto e2 = fresh (fs); Take b = render (*e2, q, fs, 1.5, gen);
        const bool same = std::memcmp (a.L.data(), b.L.data(), a.L.size() * sizeof (float)) == 0
                       && std::memcmp (a.R.data(), b.R.data(), a.R.size() * sizeof (float)) == 0;
        check (same, "four empty furniture slots: output byte-identical to no layout");
        check (e2->roomScatter (0) == 0.0f, "no furniture, no extra scattering (exactly 0)");
    }

    std::printf ("\n2. absorption\n");
    {
        Params p; p.material[0] = 3; p.door[0] = p.door[1] = p.door[2] = 0;     // a TILED living room, doors shut
        p.src[0].x = 1.8f; p.src[0].y = 1.5f; p.lisX = 4.2f; p.lisY = 3.6f;
        p.directDb = -80; p.earlyDb = -80;
        Params q = p;
        put (q, F_SOFA, 1.5f, 4.3f, 0); put (q, F_RUG, 3.0f, 2.5f, 0); put (q, F_ARMCHAIR, 5.2f, 1.0f, 30);
        put (q, F_BOOKCASE, 0.3f, 2.5f, 90); put (q, F_CURTAIN, 3.0f, 0.15f, 0);
        RoomSurfaces s; s.wall = 3;
        float e0[NBAND], e1[NBAND], a0[NBAND], a1[NBAND];
        Engine::eyringRt60 (0, s, p.door, e0);
        Engine::eyringRt60 (0, s, p.door, q.furn, q.nfurn, e1);
        Engine::absorptionArea (0, s, p.door, a0);
        Engine::absorptionArea (0, s, p.door, q.furn, q.nfurn, a1);
        // the added area is exactly the catalogue's, the rug less the tiles it covers
        double want = 0; for (int i = 0; i < q.nfurn; ++i) { const FurnSpec& F = FURN[q.furn[i].type]; want += F.absorb[3] - (F.coversFloor ? F.w * F.d * MATERIAL_ALPHA[3][3] : 0.0f); }
        check (std::abs ((a1[3] - a0[3]) - want) < 1e-4, "added absorption at 1 kHz = catalogue sum, rug less the floor it covers (m^2)", a1[3] - a0[3], want);
        char buf[200];
        std::snprintf (buf, sizeof buf, "furnishing the tiled room cuts Eyring's RT60 at 1 kHz from %.2f s to %.2f s", e0[3], e1[3]);
        check (e1[3] < 0.6f * e0[3], buf, e1[3], e0[3]);

        auto eng = fresh (fs);
        Take t = render (*eng, q, fs, 0.2, impulseAt (0));
        float mine[NBAND]; eng->roomRt60 (0, mine);
        check (std::abs (mine[3] / e1[3] - 1.0f) < 1e-4f, "the engine's own RT60 is that Eyring figure", mine[3], e1[3]);
        eng = fresh (fs);
        Take tt = render (*eng, q, fs, 2.5, impulseAt (2400));
        const double m1 = rt60From (octaveBand (tt.L, fs, 1000), fs, (int) (0.05 * fs));
        std::snprintf (buf, sizeof buf, "rendered decay at 1 kHz %.2f s against Eyring's %.2f s", m1, e1[3]);
        check (m1 > 0 && std::abs (m1 / e1[3] - 1.0) < 0.25, buf, m1, e1[3]);
    }

    std::printf ("\n3. scattering\n");
    {
        Params p; p.material[0] = 3; p.door[0] = p.door[1] = p.door[2] = 0;
        p.src[0].x = 1.8f; p.src[0].y = 1.5f; p.lisX = 4.2f; p.lisY = 3.6f;
        Params q = p; put (q, F_BOOKCASE, 0.3f, 2.5f, 90); put (q, F_SOFA, 1.5f, 4.3f, 0);
        auto e0 = fresh (fs); render (*e0, p, fs, 0.1, impulseAt (0));
        auto e1 = fresh (fs); render (*e1, q, fs, 0.1, impulseAt (0));
        const float S = ROOMS[0].surface();
        const float sum = FURN[F_BOOKCASE].scatter + FURN[F_SOFA].scatter;
        const float want = 1.0f - std::exp (-2.0f * sum / S);
        check (std::abs (e1->roomScatter (0) - want) < 1e-5f, "room scattering = 1 - exp(-2 sum / S)", e1->roomScatter (0), want);
        // the ceiling image (nz = +1): key (0+2)*25 + (0+2)*5 + (1+2) = 63, source 0
        const PathSpec* a = findKey (*e0, Engine::imageKey (0, 0, 1)), * b = findKey (*e1, Engine::imageKey (0, 0, 1));
        const float lost = (a && b) ? a->bandDb[3] - b->bandDb[3] : -1.0f;
        const float wantDb = -10.0f * std::log10 (1.0f - want);
        check (a && b && std::abs (lost - wantDb) < 1e-3f, "the ceiling reflection loses 10 log10(1 - s) (dB)", lost, wantDb);
    }

    std::printf ("\n4. occlusion\n");
    {
        // the hall, absorbing, doors shut: a source 4 m from the listener, a bookcase broadside between
        Params p; p.material[2] = 0; p.door[0] = p.door[1] = p.door[2] = 0;
        p.src[0].x = 10.0f; p.src[0].y = 4.5f; p.src[0].z = 1.2f; p.src[0].type = SRC_PURE;
        p.lisX = 14.0f; p.lisY = 4.5f; p.lisYaw = 180;
        p.earlyDb = -80; p.reverbDb = -80;
        Params q = p; put (q, F_BOOKCASE, 12.0f, 4.5f, 90);     // width across the line, 0.35 deep along it
        auto eng = fresh (fs); render (*eng, q, fs, 0.2, impulseAt (0));
        // independent: round the side, P(-2,0,1.2) corners (-0.175,+-0.5) (0.175,+-0.5) Q(2,0,1.65)
        const double side2 = 2 * std::sqrt (1.825 * 1.825 + 0.25) + 0.35;
        const double side = std::sqrt (side2 * side2 + 0.45 * 0.45) - std::sqrt (16.0 + 0.45 * 0.45);
        const double top = std::sqrt (1.825 * 1.825 + 0.8 * 0.8) + 0.35 + std::sqrt (1.825 * 1.825 + 0.35 * 0.35) - std::sqrt (16.0 + 0.45 * 0.45);
        const double want = std::min (side, top);
        const Vec3 S (10.0f, 4.5f, 1.2f), L (14.0f, 4.5f, EAR_HEIGHT);
        const float got = eng->furnitureDelta (0, S, L);
        check (std::abs (got - want) < 1e-3, "path difference round the bookcase (m), against a hand calculation", got, want);
        const PathSpec* d = findKey (*eng, Engine::imageKey (0, 0, 0));
        auto e0 = fresh (fs); render (*e0, p, fs, 0.2, impulseAt (0));
        const PathSpec* d0 = findKey (*e0, Engine::imageKey (0, 0, 0));
        for (int band : { 0, 3, 5 })
        {
            const float lost = (d && d0) ? d0->bandDb[band] - d->bandDb[band] : -1.0f;
            const float w = maekawaDb ((float) want, BAND_HZ[band]);
            char buf[160]; std::snprintf (buf, sizeof buf, "direct path at %g Hz loses Maekawa's %.2f dB", BAND_HZ[band], w);
            check (std::abs (lost - w) < 0.01f, buf, lost, w);
        }
        // in the audio: the direct pulse in the 4 kHz octave
        auto ea = fresh (fs); Take ta = render (*ea, p, fs, 0.2, impulseAt (2400));
        auto eb = fresh (fs); Take tb = render (*eb, q, fs, 0.2, impulseAt (2400));
        const auto fa = octaveBand (ta.L, fs, 4000), fb = octaveBand (tb.L, fs, 4000);
        const double la = db (energy (fa, 2400, 9600)), lb = db (energy (fb, 2400, 9600));
        const float w4 = maekawaDb ((float) want, 4000.0f);
        char buf[160]; std::snprintf (buf, sizeof buf, "rendered: the 4 kHz octave falls %.1f dB behind the bookcase (Maekawa %.1f)", la - lb, w4);
        check (std::abs ((la - lb) - w4) < 1.5, buf, la - lb, w4);
    }

    std::printf ("\n5. continuity\n");
    {
        Params q; q.material[2] = 0; q.door[0] = q.door[1] = q.door[2] = 0;
        q.src[0].x = 10.0f; q.src[0].y = 4.5f; q.src[0].z = 1.2f; q.lisX = 14.0f; q.lisY = 4.5f;
        put (q, F_BOOKCASE, 12.0f, 4.5f, 90); put (q, F_TABLE, 12.0f, 7.0f, 0);
        auto eng = fresh (fs); render (*eng, q, fs, 0.1, impulseAt (0));
        // sideways across the bookcase's edge, 1 cm at a time, at 4 kHz
        double worst = 0, prev = 0; bool first = true; double shadow = 0;
        for (int k = 0; k <= 200; ++k)
        {
            const float y = 4.5f + k * 0.01f;
            const float d = eng->furnitureDelta (0, Vec3 (10.0f, 4.5f, 1.2f), Vec3 (14.0f, y, EAR_HEIGHT));
            const double a = d > -1e8f ? maekawaDb (d, 4000.0f) : 0.0;
            if (k == 0) shadow = a;
            if (! first) worst = std::max (worst, std::abs (a - prev));
            prev = a; first = false;
        }
        char buf[160]; std::snprintf (buf, sizeof buf, "past the side edge in 1 cm steps: largest step %.2f dB (from %.1f dB in the shadow to 0)", worst, shadow);
        check (worst < 0.6 && shadow > 10 && prev < 0.01, buf, worst, 0.6);
        // over the top: raise the source through the top edge's shadow line
        worst = 0; first = true;
        for (int k = 0; k <= 300; ++k)
        {
            const float z = 1.5f + k * 0.01f;
            const float d = eng->furnitureDelta (0, Vec3 (10.0f, 4.5f, z), Vec3 (14.0f, 4.5f, EAR_HEIGHT));
            const double a = d > -1e8f ? maekawaDb (d, 4000.0f) : 0.0;
            if (! first) worst = std::max (worst, std::abs (a - prev));
            prev = a; first = false;
        }
        std::snprintf (buf, sizeof buf, "over the top in 1 cm steps: largest step %.2f dB", worst);
        check (worst < 0.6, buf, worst, 0.6);
        // under a table: a leg from low to low passes beneath the top
        const float under = eng->furnitureDelta (1, Vec3 (10.0f, 7.0f, 0.3f), Vec3 (14.0f, 7.0f, 0.3f));
        check (under < 0, "a path beneath the table top is not blocked by it", under, 0);
    }

    std::printf ("\n6. a table top reflects\n");
    {
        Params p; p.material[2] = 0; p.door[0] = p.door[1] = p.door[2] = 0;
        p.src[0].x = 11.0f; p.src[0].y = 4.5f; p.src[0].z = 1.4f; p.src[0].type = SRC_PURE;
        p.lisX = 13.0f; p.lisY = 4.5f; p.lisYaw = 180; p.directDb = -80; p.reverbDb = -80;
        put (p, F_TABLE, 12.0f, 4.5f, 0);
        auto eng = fresh (fs); render (*eng, p, fs, 0.3, impulseAt (0));
        const PathSpec* r = findKey (*eng, 0x60000u);
        const Vec3 I (11.0f, 4.5f, 2 * 0.75f - 1.4f), L (13.0f, 4.5f, EAR_HEIGHT);
        const float want = (L - I).len();
        check (r != nullptr && std::abs (r->length - want) < 1e-3f, "the reflection off the top has the mirror source's length (m)", r ? r->length : 0, want);
        // walk the listener away along x: the mirror point runs off the table's end (x = 12.8)
        double worst = 0, prev = 0; bool first = true, gone = false; int steps = 0;
        for (int k = 0; k <= 60; ++k)
        {
            Params q = p; q.lisX = 13.0f + k * 0.05f;
            render (*eng, q, fs, 0.15, [] (int) { return 0.0f; });
            const PathSpec* s = findKey (*eng, 0x60000u);
            const double g = s ? db ((double) s->gain * s->gain) + s->bandDb[3] : -200.0;
            if (! s) gone = true;
            if (! first && s) worst = std::max (worst, std::abs (g - prev)), ++steps;
            if (getenv ("TWDBG")) std::printf ("    x %.2f  on %d  g %.2f  len %.3f  b0 %.2f b3 %.2f  pt %.3f\n", q.lisX, s ? 1 : 0, g, s ? s->length : 0.0f, s ? s->bandDb[0] : 0.0f, s ? s->bandDb[3] : 0.0f, s ? s->pts[1].x : 0.0f);
            if (s) prev = g;
            first = false;
        }
        char buf[160]; std::snprintf (buf, sizeof buf, "walking 3 m past the table in 5 cm steps: largest step of the reflection %.2f dB (1 kHz)", worst);
        check (worst < 1.5, buf, worst, 1.5);
    }

    std::printf ("\n7. cost\n");
    {
        Params p; p.material[0] = 1; p.material[2] = 2;
        for (int s = 0; s < 4; ++s) p.src[s].input = IN_MAIN_LR;
        p.src[0].x = 2; p.src[0].y = 2; p.src[1].x = 5; p.src[1].y = 1; p.src[2].x = 4.5f; p.src[2].y = 7; p.src[3].x = 12; p.src[3].y = 4.5f;
        p.lisX = 4.5f; p.lisY = 3.5f;
        put (p, F_SOFA, 1.5f, 4.3f, 0); put (p, F_RUG, 3.0f, 2.5f, 0); put (p, F_ARMCHAIR, 5.2f, 1.0f, 30);
        put (p, F_BOOKCASE, 0.3f, 2.5f, 90); put (p, F_TABLE, 3.2f, 2.2f, 0); put (p, F_BED, 4.5f, 7.6f, 0);
        put (p, F_PIANO, 10.0f, 6.0f, 20); put (p, F_PERSON, 13.0f, 3.0f, 0); put (p, F_PERSON, 13.6f, 3.2f, 0); put (p, F_WARDROBE, 3.6f, 5.4f, 0);
        auto eng = fresh (fs);
        auto gen = [] (int i) { unsigned s = (unsigned) i * 2654435761u; return ((s >> 9) / 4194304.0f - 1.0f) * 0.2f; };
        render (*eng, p, fs, 0.5, gen);
        const auto t0 = std::chrono::steady_clock::now();
        render (*eng, p, fs, 5.0, gen);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        char buf[160]; std::snprintf (buf, sizeof buf, "four sources, ten pieces: %d paths, %.1f %% of one core (%d refused)", eng->numActivePaths(), sec / 5.0 * 100.0, eng->numPathsDropped());
        check (sec / 5.0 < 0.6 && eng->numPathsDropped() == 0, buf, sec / 5.0 * 100.0, 60);
    }

    std::printf ("\n8. acoustic art panels\n");
    {
        Params p; p.material[0] = 3; p.door[0] = p.door[1] = p.door[2] = 0;
        p.src[0].x = 1.8f; p.src[0].y = 1.5f; p.lisX = 4.2f; p.lisY = 3.6f;
        Params q = p; q.panelArea[0] = 4.0f;                     // four square metres of printed absorber
        auto e0 = fresh (fs); render (*e0, p, fs, 0.2, impulseAt (0));
        auto e1 = fresh (fs); render (*e1, q, fs, 0.2, impulseAt (0));
        float r0[NBAND], r1[NBAND]; e0->roomRt60 (0, r0); e1->roomRt60 (0, r1);
        const float want = 4.0f * (PANEL_ALPHA[3] - MATERIAL_ALPHA[3][3]);
        check (std::abs (e1->scene().furnA[0] - want) < 1e-3f, "4 m^2 of panel adds exactly its absorber less the tiles behind it at 1 kHz (m^2)", e1->scene().furnA[0], want);
        char buf[160]; std::snprintf (buf, sizeof buf, "and the tiled room's RT60 at 1 kHz falls from %.2f s to %.2f s", r0[3], r1[3]);
        check (r1[3] < 0.85f * r0[3], buf, r1[3], r0[3]);
        check (e0->scene().furnA[0] == 0.0f, "no panels, nothing added (exactly 0)");
    }

    std::printf ("\n9. the light follows the sound in each room\n");
    {
        Params p; p.door[0] = p.door[1] = p.door[2] = 0;       // LARGE sealed off from the hall
        p.src[0].x = 2.0f; p.src[0].y = 2.0f; p.lisX = 4.0f; p.lisY = 3.0f;
        auto eng = fresh (fs);
        render (*eng, p, fs, 0.5, [] (int) { return 0.0f; });
        check (eng->lightLevel (0) == 0.0f && eng->lightLevel (2) == 0.0f, "silence: every lamp exactly at rest");
        // a burst of noise from silence: the onset, then the same noise held
        auto noise = [] (int i) { unsigned s = (unsigned) i * 2654435761u; return ((s >> 9) / 4194304.0f - 1.0f) * 0.25f; };
        render (*eng, p, fs, 0.04, noise);
        const float onset = eng->lightLevel (0);
        render (*eng, p, fs, 1.5, noise);
        const float held = eng->lightLevel (0), hall = eng->lightLevel (2);
        char buf[200]; std::snprintf (buf, sizeof buf, "an onset flashes (%.2f) above the same sound held (%.2f)", onset, held);
        check (onset > held + 0.15f, buf, onset, held);
        std::snprintf (buf, sizeof buf, "a held sound still glows (%.2f), in its own room only: the sealed hall reads %.3f", held, hall);
        check (held > 0.2f && hall < 0.25f * held, buf, held, hall);
        render (*eng, p, fs, 1.0, [] (int) { return 0.0f; });
        check (eng->lightLevel (0) < 0.05f, "and it falls back when the sound stops", eng->lightLevel (0), 0.05);
    }

    std::printf ("\n%d checks, %d failed  -  %s\n", checks, failures, failures ? "FAILURES" : "ALL CLEAR");
    return failures ? 1 : 0;
}
