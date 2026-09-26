// HATS OFF demo renderer: the trilogy playing together - Kickstart's kick,
// Snare Tactics' snare and Hats Off's cymbals, all three real engines and
// their factory presets - written to 16-bit stereo WAV for the landing page.
//
//   horender <outdir>

#include "../engine/ho_engine.h"
#include "../engine/ho_presets.h"
#include "../../../kickstart/plugin/engine/ks_engine.h"
#include "../../../kickstart/plugin/engine/ks_presets.h"
#include "../../../snare-tactics/plugin/engine/st_engine.h"
#include "../../../snare-tactics/plugin/engine/st_presets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(_M_X64) || defined(__SSE2__)
 #include <xmmintrin.h>
#endif

namespace
{
constexpr double FS = 48000.0;

struct Note { double beat; int note; float vel; };
struct Cym  { const char* preset; std::vector<Note> notes; float gain = 1.0f; };

struct Seg
{
    double bpm, beats;
    const char* kick;  std::vector<Note> kk;
    const char* snare; std::vector<Note> sn;
    std::vector<Cym> cym;
    float tail = 0.0f;      // beats of ring after the pattern (echo, crash)
};

template <typename F> int findIn (int n, F name, const char* want)
{
    for (int i = 0; i < n; ++i) if (std::strcmp (name (i), want) == 0) return i;
    std::printf ("no preset %s\n", want); return 0;
}

void writeWav (const std::string& file, const std::vector<float>& L, const std::vector<float>& R)
{
    FILE* f = std::fopen (file.c_str(), "wb");
    if (! f) return;
    const unsigned n = (unsigned) L.size(), data = n * 4, rate = (unsigned) FS;
    auto w32 = [&] (unsigned v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (unsigned short v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + data); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 (rate); w32 (rate * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (data);
    for (unsigned i = 0; i < n; ++i)
    {
        const short l = (short) std::lround (std::max (-1.0f, std::min (1.0f, L[i])) * 32767.0f);
        const short r = (short) std::lround (std::max (-1.0f, std::min (1.0f, R[i])) * 32767.0f);
        std::fwrite (&l, 2, 1, f); std::fwrite (&r, 2, 1, f);
    }
    std::fclose (f);
}

//  run an engine over a note list; `step` renders m samples into (l, r)
template <typename NoteOn, typename Step>
void drive (const std::vector<Note>& notes, double spb, int n, NoteOn on, Step step)
{
    size_t h = 0;
    for (int i = 0; i < n; )
    {
        int m = std::min (256, n - i);
        if (h < notes.size())
        {
            const int at = (int) std::lround (notes[h].beat * spb * FS);
            if (at <= i) { on (notes[h]); ++h; continue; }
            m = std::min (m, at - i);
        }
        step (i, m);
        i += m;
    }
}

void renderSegs (const std::string& file, const std::vector<Seg>& segs)
{
    std::vector<float> L, R;
    for (const auto& s : segs)
    {
        const double spb = 60.0 / s.bpm;
        const int n = (int) ((s.beats + s.tail) * spb * FS);
        std::vector<float> ml ((size_t) n, 0.0f), mr ((size_t) n, 0.0f), tl ((size_t) n), tr ((size_t) n);

        //  every engine's output lined up to the latest latency, so the groove sits on the grid
        const int hoLat = ho::Engine::kDecimatorLatency + ho::Engine::kDriveLatency + (int) std::lround (0.0015 * FS);
        auto mix = [&] (const std::vector<float>& a, const std::vector<float>& b, int lat, float g)
        {
            const int shift = hoLat - lat;              // earlier engines wait for the later one
            for (int i = 0; i < n; ++i)
            {
                const int j = i - shift;
                if (j < 0 || j >= n) continue;
                ml[(size_t) i] += g * a[(size_t) j]; mr[(size_t) i] += g * b[(size_t) j];
            }
        };

        if (s.kick)
        {
            auto e = std::make_unique<ks::Engine>(); e->prepare (FS, 256);
            const auto p = ks::presetParams (findIn (ks::numPresets(), [] (int i) { return ks::preset (i).name; }, s.kick));
            std::fill (tl.begin(), tl.end(), 0.0f);
            drive (s.kk, spb, n, [&] (const Note& x) { e->noteOn (x.note, x.vel); },
                   [&] (int i, int m) { e->process (p, tl.data() + i, m); });
            mix (tl, tl, e->latencySamples(), 0.75f);
        }
        if (s.snare)
        {
            auto e = std::make_unique<st::Engine>(); e->prepare (FS, 256); e->setTempo (s.bpm);
            const auto p = st::presetParams (st::presetByName (s.snare));
            std::fill (tl.begin(), tl.end(), 0.0f); std::fill (tr.begin(), tr.end(), 0.0f);
            drive (s.sn, spb, n, [&] (const Note& x) { e->noteOn (x.note, x.vel); },
                   [&] (int i, int m) { e->process (p, tl.data() + i, tr.data() + i, m); });
            mix (tl, tr, e->latencySamples(), 0.7f);
        }
        for (const auto& c : s.cym)
        {
            auto e = std::make_unique<ho::Engine>(); e->prepare (FS, 256); e->setTempo (s.bpm);
            const int idx = ho::presetByName (c.preset);
            if (idx < 0) std::printf ("no Hats Off preset %s\n", c.preset);
            const auto p = ho::presetParams (std::max (0, idx));
            std::fill (tl.begin(), tl.end(), 0.0f); std::fill (tr.begin(), tr.end(), 0.0f);
            drive (c.notes, spb, n, [&] (const Note& x) { e->noteOn (x.note, x.vel); },
                   [&] (int i, int m) { e->process (p, tl.data() + i, tr.data() + i, m); });
            mix (tl, tr, e->latencySamples(), 0.65f * c.gain);
        }
        for (int i = hoLat; i < n; ++i) { L.push_back (ml[(size_t) i]); R.push_back (mr[(size_t) i]); }
    }
    float pk = 0.0f;
    for (size_t i = 0; i < L.size(); ++i) pk = std::max ({ pk, std::abs (L[i]), std::abs (R[i]) });
    const float g = pk > 0.0f ? 0.891f / pk : 1.0f;
    for (size_t i = 0; i < L.size(); ++i) { L[i] *= g; R[i] *= g; }
    const int fade = (int) (0.08 * FS);
    for (int i = 0; i < fade && i < (int) L.size(); ++i)
    { L[L.size() - 1 - (size_t) i] *= (float) i / fade; R[R.size() - 1 - (size_t) i] *= (float) i / fade; }
    writeWav (file, L, R);
    std::printf ("  %s  %.1f s\n", file.c_str(), L.size() / FS);
}

// ---- patterns (beats, two bars unless said) ------------------------------------
std::vector<Note> each (double from, double to, double stepB, int note, float v, float accent = 0.0f)
{
    std::vector<Note> x;
    int k = 0;
    for (double b = from; b < to - 1e-9; b += stepB, ++k) x.push_back ({ b, note, (k % 2 == 0) ? std::min (1.0f, v + accent) : v });
    return x;
}
std::vector<Note> cat (std::vector<Note> a, const std::vector<Note>& b)
{ a.insert (a.end(), b.begin(), b.end()); std::sort (a.begin(), a.end(), [] (auto& p, auto& q) { return p.beat < q.beat; }); return a; }
std::vector<Note> without (std::vector<Note> a, std::vector<double> beats)
{
    a.erase (std::remove_if (a.begin(), a.end(), [&] (const Note& n) { for (double b : beats) if (std::abs (n.beat - b) < 1e-6) return true; return false; }), a.end());
    return a;
}
std::vector<Note> fourFloor (int bars = 2) { return each (0, bars * 4.0, 1.0, 36, 1.0f); }
std::vector<Note> backbeat (int bars = 2) { std::vector<Note> v; for (int b = 0; b < bars; ++b) { v.push_back ({ b * 4 + 1.0, 38, 1.0f }); v.push_back ({ b * 4 + 3.0, 38, 1.0f }); } return v; }
std::vector<Note> rockKick (int bars = 2) { std::vector<Note> v; for (int b = 0; b < bars; ++b) { v.push_back ({ b * 4.0, 36, 1.0f }); v.push_back ({ b * 4 + 2.0, 36, 0.95f }); v.push_back ({ b * 4 + 2.5, 36, 0.8f }); } return v; }
//  a swung jazz ride: 1, 2, the "a" of 2, 3, 4, the "a" of 4
std::vector<Note> swingRide (int bars)
{
    std::vector<Note> v;
    for (int b = 0; b < bars; ++b)
        for (double x : { 0.0, 1.0, 1.67, 2.0, 3.0, 3.67 }) v.push_back ({ b * 4 + x, 51, (x == 1.0 || x == 3.0) ? 0.95f : 0.7f });
    return v;
}
} // namespace

int main (int argc, char** argv)
{
   #if defined(_M_X64) || defined(__SSE2__)
    _mm_setcsr (_mm_getcsr() | 0x8040);
   #endif
    const std::string dir = argc > 1 ? argv[1] : ".";

    // 1. ACOUSTIC: a hi-hat groove with an open hat and the foot, a swung jazz ride, a crash
    {
        const auto hats = cat (without (each (0, 8, 0.5, 42, 0.75f, 0.2f), { 3.5, 7.5 }),
                               { { 3.5, 46, 0.9f }, { 7.5, 46, 0.9f } });
        renderSegs (dir + "/hats-off-01-acoustic.wav", {
            { 96, 8, "Studio Kick", rockKick(), "Studio Snare", backbeat(), { { "Studio Hi-Hat 14", hats } } },
            { 96, 8, "Studio Kick", rockKick(), "Studio Snare", backbeat(), { { "Loose Hat", each (0, 8, 0.5, 51, 0.75f, 0.2f) } } },
            { 132, 8, "Jazz Kick", { { 0, 36, 0.5f }, { 4, 36, 0.5f } }, "Jazz Snare", { { 1.67, 38, 0.25f }, { 3.67, 38, 0.3f }, { 5.67, 38, 0.25f }, { 7.0, 38, 0.8f } },
              { { "Jazz Ride 20", swingRide (2) }, { "Studio Hi-Hat 14", { { 1, 44, 0.8f }, { 3, 44, 0.8f }, { 5, 44, 0.8f }, { 7, 44, 0.8f } } } } },
            { 100, 8, "Rock Kick", rockKick(), "Rock Rim Shot", backbeat(),
              { { "Crash 18", { { 0, 49, 1.0f } } }, { "Dark Ride 22", each (0.5, 8, 0.5, 51, 0.7f) }, { "Ride Bell", { { 2, 53, 0.9f }, { 6, 53, 0.9f } } } }, 4 },
            { 100, 4, "Rock Kick", { { 0, 36, 1 } }, nullptr, {}, { { "China 18", { { 0, 49, 1.0f } } } }, 4 } });
    }

    // 2. VINTAGE
    renderSegs (dir + "/hats-off-02-vintage.wav", {
        { 100, 8, "808 Boom", { { 0, 36, 1 }, { 2.5, 36, 0.8f }, { 4, 36, 1 }, { 6.5, 36, 0.8f } }, "808 Snare", backbeat(),
          { { "808 Closed Hat", without (each (0, 8, 0.5, 42, 0.8f, 0.15f), { 3.5, 7.5 }) }, { "808 Open Hat", { { 3.5, 46, 0.9f }, { 7.5, 46, 0.9f } } } } },
        { 124, 8, "909 Classic", fourFloor(), "909 Snare", backbeat(),
          { { "909 Closed Hat", each (0.25, 8, 0.5, 42, 0.7f) }, { "909 Open Hat", each (0.5, 8, 1.0, 46, 0.9f) } } },
        { 104, 8, "LinnDrum", rockKick(), "LinnDrum", backbeat(), { { "LinnDrum Hat", each (0, 8, 0.25, 42, 0.65f, 0.25f) } } },
        { 110, 8, "CR-78", each (0, 8, 2.0, 36, 1.0f), "CR-78", backbeat(), { { "CR-78 Hat", each (0, 8, 0.5, 42, 0.8f) } } } });

    // 3. TECHNO
    renderSegs (dir + "/hats-off-03-techno.wav", {
        { 132, 8, "Hard Techno", fourFloor(), "Techno Crush", backbeat(),
          { { "Techno Closed", each (0, 8, 0.25, 42, 0.55f, 0.3f) }, { "Rolling Open Hat", each (0.5, 8, 1.0, 46, 0.9f) } } },
        { 128, 8, "Techno Rumble", fourFloor(), nullptr, {},
          { { "Detroit Ride", each (0, 8, 0.5, 51, 0.7f, 0.2f) }, { "Minimal Tick", each (0.25, 8, 0.5, 42, 0.8f) } } },
        { 130, 8, "Techno Rumble", fourFloor(), "Warehouse Clap", backbeat(), { { "Warehouse Ride", each (0.5, 8, 1.0, 51, 0.9f) } }, 2 } });

    // 4. DUB: the echo on the song's tempo
    renderSegs (dir + "/hats-off-04-dub.wav", {
        { 76, 8, "Clean Sub", { { 0, 36, 1 }, { 4, 36, 1 } }, "Roots Rim Shot", { { 2, 38, 1 }, { 6, 38, 1 } },
          { { "Roots Hi-Hat", each (0, 8, 0.5, 42, 0.7f, 0.2f) }, { "Dub Hat Echo", { { 3.5, 46, 0.9f } } } } },
        { 76, 8, "Clean Sub", fourFloor(), "Steppers", backbeat(), { { "Skank Hat", each (0.5, 8, 1.0, 42, 0.85f) } } },
        { 120, 8, "House Thump", fourFloor(), "Dub Techno Space", backbeat(), { { "Dub Techno Hat", each (0.5, 8, 1.0, 46, 0.9f) } } },
        { 72, 4, "Clean Sub", { { 0, 36, 1 } }, nullptr, {}, { { "Space Cymbal", { { 0, 49, 1.0f } } } }, 4 } });

    // 5. DUBSTEP: half-time at 140, hat rolls, a crash on the drop
    {
        std::vector<Note> rolls = each (0, 8, 0.5, 42, 0.7f, 0.2f);
        for (int i = 0; i < 8; ++i) rolls.push_back ({ 7.0 + i * 0.125, 42, 0.45f + 0.06f * i });
        std::sort (rolls.begin(), rolls.end(), [] (auto& a, auto& b) { return a.beat < b.beat; });
        renderSegs (dir + "/hats-off-05-dubstep.wav", {
            { 140, 8, "Dubstep Punch", { { 0, 36, 1 }, { 4, 36, 1 }, { 5.5, 36, 0.8f } }, "Dubstep Snare", { { 2, 38, 1 }, { 6, 38, 1 } },
              { { "Halftime Crash", { { 0, 49, 1.0f } } }, { "Dubstep Hat", each (0, 8, 0.5, 42, 0.7f, 0.25f) } } },
            { 140, 8, "Trap 808", { { 0, 36, 1 }, { 2.75, 36, 0.8f }, { 4, 36, 1 } }, "Trap Snare Clap", { { 2, 38, 1 }, { 6, 38, 1 } },
              { { "Trap Hat", rolls } } },
            { 134, 8, "Clean Sub", { { 0, 36, 1 }, { 3.5, 36, 0.9f }, { 4.75, 36, 0.8f } }, "DnB Snare", { { 1, 38, 1 }, { 3, 38, 1 }, { 5, 38, 1 }, { 7, 38, 1 } },
              { { "UKG Shuffle Hat", each (0.5, 8, 1.0, 42, 0.8f) } } } });
    }

    // 6. ONE KNOB: OPEN from closed to open on the same 14-inch hat, eighths, one bar each
    {
        //  OPEN varied on one preset, so the engine is driven directly
        std::vector<float> L, R;
        const double spb = 60.0 / 110.0;
        for (float o : { 0.0f, 0.1f, 0.25f, 0.45f, 0.7f })
        {
            ho::Params p = ho::presetParams (ho::presetByName ("Studio Hi-Hat 14"));
            p.open = o; p.keys = ho::KEYS_FIXED;
            auto e = std::make_unique<ho::Engine>(); e->prepare (FS, 256);
            const int n = (int) (4 * spb * FS);
            std::vector<float> l ((size_t) n), r ((size_t) n);
            drive (each (0, 4, 0.5, 51, 0.8f, 0.1f), spb, n, [&] (const Note& x) { e->noteOn (x.note, x.vel); },
                   [&] (int i, int m) { e->process (p, l.data() + i, r.data() + i, m); });
            L.insert (L.end(), l.begin(), l.end()); R.insert (R.end(), r.begin(), r.end());
        }
        float pk = 0; for (size_t i = 0; i < L.size(); ++i) pk = std::max ({ pk, std::abs (L[i]), std::abs (R[i]) });
        for (size_t i = 0; i < L.size(); ++i) { L[i] *= 0.891f / pk; R[i] *= 0.891f / pk; }
        writeWav (dir + "/hats-off-06-open.wav", L, R);
        std::printf ("  %s/hats-off-06-open.wav  %.1f s\n", dir.c_str(), L.size() / FS);
    }

    // 7. THE STICK ACROSS THE CYMBAL: a 20-inch ride from the bell to the edge, then BLOOM off and on
    {
        std::vector<float> L, R;
        auto take = [&] (ho::Params p, const std::vector<std::pair<double, float>>& hits, double beats, double bpm)
        {
            const double spb = 60.0 / bpm;
            auto e = std::make_unique<ho::Engine>(); e->prepare (FS, 256);
            const int n = (int) (beats * spb * FS);
            std::vector<float> l ((size_t) n), r ((size_t) n);
            size_t h = 0;
            for (int i = 0; i < n; )
            {
                int m = std::min (256, n - i);
                if (h < hits.size())
                {
                    const int at = (int) std::lround (hits[h].first * spb * FS);
                    if (at <= i) { e->noteOnArtic (ho::ART_DIALLED, 0.9f, hits[h].second); ++h; continue; }
                    m = std::min (m, at - i);
                }
                e->process (p, l.data() + i, r.data() + i, m);
                i += m;
            }
            L.insert (L.end(), l.begin(), l.end()); R.insert (R.end(), r.begin(), r.end());
        };
        ho::Params ride = ho::presetParams (ho::presetByName ("Jazz Ride 20"));
        std::vector<std::pair<double, float>> sweep;
        for (int i = 0; i < 12; ++i) sweep.push_back ({ i * 0.5, i / 11.0f });
        take (ride, sweep, 8, 100);
        ho::Params c = ho::presetParams (ho::presetByName ("Crash 18"));
        c.bloom = 0.0f; take (c, { { 0.0, 1.0f } }, 6, 100);
        c.bloom = 1.0f; take (c, { { 0.0, 1.0f } }, 6, 100);
        float pk = 0; for (size_t i = 0; i < L.size(); ++i) pk = std::max ({ pk, std::abs (L[i]), std::abs (R[i]) });
        for (size_t i = 0; i < L.size(); ++i) { L[i] *= 0.891f / pk; R[i] *= 0.891f / pk; }
        writeWav (dir + "/hats-off-07-strike-bloom.wav", L, R);
        std::printf ("  %s/hats-off-07-strike-bloom.wav  %.1f s\n", dir.c_str(), L.size() / FS);
    }

    std::printf ("wrote 7 demos to %s\n", dir.c_str());
    return 0;
}
