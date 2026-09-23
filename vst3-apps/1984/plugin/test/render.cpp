/*  Render the demo passages that ship with 1984. Each is a short piece made
    only by the instrument, its factory patch untouched except where noted,
    written as a 16-bit stereo WAV at 48 kHz. The landing page plays these
    (as mp3, via tools/wav2mp3.js).

        n84render <outdir>
*/
#include "Engine.h"
#include "Patches.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

using namespace n84;
static const int SR = 48000, BLK = 256;

struct Ev { double t; int kind; int a; float v; };   // 0 on, 1 off, 2 wheel, 3 bend, 4 aftertouch

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    const int n = (int) std::min (L.size(), R.size());
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { std::printf ("  cannot write %s\n", path.c_str()); return; }
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const int dataBytes = n * 4;
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); u32 (16); u16 (1); u16 (2); u32 (SR); u32 (SR * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (int i = 0; i < n; ++i)
    {
        auto q = [] (float x) { x = x < -1 ? -1 : (x > 1 ? 1 : x); return (int16_t) std::lround (x * 32000.0f); };
        int16_t l = q (L[(size_t) i]), r = q (R[(size_t) i]);
        std::fwrite (&l, 2, 1, f); std::fwrite (&r, 2, 1, f);
    }
    std::fclose (f);
    std::printf ("  wrote %s  (%.1f s)\n", path.c_str(), (double) n / SR);
}

static int patchByName (const char* name)
{
    for (int i = 0; i < numPatches(); ++i) if (std::strcmp (patchName (i), name) == 0) return i;
    std::printf ("  ?? no patch %s\n", name); return 0;
}

static void render (const std::string& path, double secs, const char* patch, std::vector<Ev> evs,
                    std::vector<std::pair<std::string, float>> setup = {})
{
    Engine e;
    applyPatch (patchByName (patch), e.p);
    e.p.os = 1;    // 2x, the shipped default
    for (auto& s : setup) { const int i = paramIndex (s.first.c_str()); if (i >= 0) paramSpec (i).ref (e.p) = s.second; }
    e.prepare (SR, BLK);
    std::sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.t < b.t; });
    const int nb = (int) (secs * SR / BLK);
    std::vector<float> L, R; L.reserve ((size_t) nb * BLK); R.reserve ((size_t) nb * BLK);
    std::vector<float> bl (BLK), br (BLK);
    size_t ei = 0;
    for (int b = 0; b < nb; ++b)
    {
        const double now = (double) b * BLK / SR;
        while (ei < evs.size() && evs[ei].t <= now)
        {
            const Ev& v = evs[ei++];
            if      (v.kind == 0) e.noteOn (v.a, v.v);
            else if (v.kind == 1) e.noteOff (v.a);
            else if (v.kind == 2) e.setWheel (v.v);
            else if (v.kind == 3) e.setBend (v.v);
            else if (v.kind == 4) e.setAftertouch (v.v);
        }
        e.process (bl.data(), br.data(), BLK);
        L.insert (L.end(), bl.begin(), bl.end()); R.insert (R.end(), br.begin(), br.end());
    }
    // normalise to -1 dBFS peak, fade the ends
    float pk = 0; for (float v : L) pk = std::max (pk, std::abs (v)); for (float v : R) pk = std::max (pk, std::abs (v));
    const float g = pk > 0 ? 0.89f / pk : 1.0f;
    for (auto& v : L) v *= g; for (auto& v : R) v *= g;
    const int fade = SR / 10;
    for (int i = 0; i < fade && i < (int) L.size(); ++i)
    {
        const float w = (float) i / fade;
        L[(size_t) i] *= w; R[(size_t) i] *= w;
        L[L.size() - 1 - (size_t) i] *= w; R[R.size() - 1 - (size_t) i] *= w;
    }
    writeWav (path, L, R);
}

// helpers for writing music
static void chord (std::vector<Ev>& e, double t, double len, std::initializer_list<int> notes, float vel = 0.8f)
{
    for (int n : notes) { e.push_back ({ t, 0, n, vel }); e.push_back ({ t + len, 1, n, 0 }); }
}
static void note (std::vector<Ev>& e, double t, double len, int n, float vel = 0.8f)
{
    e.push_back ({ t, 0, n, vel }); e.push_back ({ t + len, 1, n, 0 });
}

int main (int argc, char** argv)
{
    const std::string out = argc > 1 ? std::string (argv[1]) + "/" : "";
    std::printf ("1984 demo passages\n");

    {   // 1. BLADE BRASS: the opening of a film, four held chords, a swell on the wheel
        std::vector<Ev> e;
        chord (e, 0.2, 3.6, { 50, 57, 62, 65 }, 0.7f);          // Dm
        chord (e, 4.0, 3.6, { 46, 53, 58, 62 }, 0.75f);         // Bb
        chord (e, 8.0, 3.6, { 48, 55, 60, 64 }, 0.8f);          // C
        chord (e, 12.0, 5.5, { 50, 57, 62, 69 }, 0.9f);         // Dm, high
        for (int i = 0; i <= 20; ++i) e.push_back ({ 12.0 + i * 0.25, 4, (int) 0, std::min (1.0f, i / 14.0f) });
        for (int i = 0; i <= 20; ++i) e.push_back ({ 17.6 + i * 0.05, 4, (int) 0, std::max (0.0f, 1.0f - i / 20.0f) });
        render (out + "1984-01-blade-brass.wav", 21.0, "BLADE BRASS", e);
    }
    {   // 2. CS STRINGS: a slow progression, the ensemble doing the work
        std::vector<Ev> e;
        chord (e, 0.2, 5.0, { 45, 52, 57, 60, 64 }, 0.6f);      // Am
        chord (e, 5.0, 5.0, { 41, 48, 53, 57, 60 }, 0.6f);      // F
        chord (e, 10.0, 5.0, { 43, 50, 55, 59, 62 }, 0.65f);    // G
        chord (e, 15.0, 6.5, { 45, 52, 57, 60, 64, 69 }, 0.7f); // Am
        render (out + "1984-02-cs-strings.wav", 23.0, "CS STRINGS", e);
    }
    {   // 3. VANGELIS CHOIR: two chords and a suspension
        std::vector<Ev> e;
        chord (e, 0.3, 7.0, { 50, 57, 62, 65, 69 }, 0.6f);      // Dm
        chord (e, 7.5, 7.0, { 48, 55, 60, 64, 67 }, 0.6f);      // C
        chord (e, 15.0, 7.5, { 46, 53, 58, 62, 65, 70 }, 0.65f);// Bb add
        render (out + "1984-03-vangelis-choir.wav", 24.0, "VANGELIS CHOIR", e);
    }
    {   // 4. SYNC LEAD: a mono line with glide and the wheel
        std::vector<Ev> e;
        const int line[] = { 62, 65, 69, 67, 65, 62, 60, 62, 65, 69, 72, 70, 69, 65, 62, 57 };
        double t = 0.2;
        for (int i = 0; i < 16; ++i) { const double len = (i % 4 == 3) ? 0.9 : 0.45; note (e, t, len - 0.03, line[i], 0.75f + 0.2f * (i % 3 == 0)); t += len; }
        note (e, t + 0.1, 3.5, 62, 0.9f);
        for (int i = 0; i <= 30; ++i) e.push_back ({ t + 0.8 + i * 0.08, 2, 0, std::min (1.0f, i / 20.0f) });
        render (out + "1984-04-sync-lead.wav", t + 4.5, "SYNC LEAD", e);
    }
    {   // 5. VHS MEMORY: a pad on a worn tape
        std::vector<Ev> e;
        chord (e, 0.3, 9.0, { 45, 52, 57, 61, 64 }, 0.6f);      // A
        chord (e, 9.5, 9.0, { 42, 49, 54, 57, 61 }, 0.6f);      // F#m
        render (out + "1984-05-vhs-memory.wav", 20.0, "VHS MEMORY", e);
    }
    {   // 6. VINTAGE BASS + POLY-MOD SWEEP: a bass figure, then the strange thing over it
        std::vector<Ev> e;
        const int bass[] = { 38, 38, 45, 38, 41, 41, 48, 41, 43, 43, 50, 43, 38, 38, 45, 50 };
        for (int i = 0; i < 16; ++i) note (e, 0.2 + i * 0.5, 0.42, bass[i], 0.85f);
        render (out + "1984-06-vintage-bass.wav", 9.0, "VINTAGE BASS", e);
        std::vector<Ev> f;
        chord (f, 0.2, 3.8, { 50, 57 }, 0.8f); chord (f, 4.0, 3.8, { 53, 60 }, 0.8f); chord (f, 8.0, 5.0, { 48, 55, 62 }, 0.9f);
        render (out + "1984-07-poly-mod-sweep.wav", 14.5, "POLY-MOD SWEEP", f);
    }
    {   // 8. HEAVEN AND HELL: the big one, tape and hall and shimmer
        std::vector<Ev> e;
        chord (e, 0.3, 10.0, { 38, 50, 57, 62, 65, 69 }, 0.7f);
        chord (e, 10.5, 11.0, { 36, 48, 55, 60, 64, 67, 72 }, 0.75f);
        render (out + "1984-08-heaven-and-hell.wav", 28.0, "HEAVEN AND HELL", e);
    }
    return 0;
}
