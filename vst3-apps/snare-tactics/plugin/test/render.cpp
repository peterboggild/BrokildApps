// SNARE TACTICS demo renderer: the real engine and the factory presets, over
// a kick from Kickstart's real engine (its sibling), written to 16-bit stereo
// WAV for the landing page (tools/wav2mp3.js turns them into mp3).
//
//   strender <outdir>

#include "../engine/st_engine.h"
#include "../engine/st_presets.h"
#include "../../../kickstart/plugin/engine/ks_engine.h"
#include "../../../kickstart/plugin/engine/ks_presets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(_M_X64) || defined(__SSE2__)
 #include <xmmintrin.h>
#endif

namespace
{
constexpr double FS = 48000.0;

struct Note { double beat; int note; float vel; };

struct Seg
{
    st::Params snare;
    const char* kick;                 // a Kickstart preset, or nullptr
    double bpm, beats;
    std::vector<Note> sn, kk;
};

int ksPreset (const char* n)
{
    for (int i = 0; i < ks::numPresets(); ++i) if (std::strcmp (ks::preset (i).name, n) == 0) return i;
    std::printf ("no Kickstart preset %s\n", n); return 0;
}

st::Params snareOf (const char* n)
{
    const int i = st::presetByName (n);
    if (i < 0) std::printf ("no Snare Tactics preset %s\n", n);
    return st::presetParams (std::max (0, i));
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

void renderSegs (const std::string& file, const std::vector<Seg>& segs)
{
    std::vector<float> L, R;
    for (const auto& s : segs)
    {
        const double spb = 60.0 / s.bpm;
        //  a bar of tail, so an echo or a room is not cut off at the join
        const int n = (int) ((s.beats + (s.snare.echo > 0.0f ? 4.0 : 0.0)) * spb * FS);
        std::vector<float> sl ((size_t) n, 0.0f), sr ((size_t) n, 0.0f), kick ((size_t) n, 0.0f);

        st::Engine e; e.prepare (FS, 256); e.setTempo (s.bpm);
        size_t h = 0;
        for (int i = 0; i < n; )
        {
            int m = std::min (256, n - i);
            if (h < s.sn.size())
            {
                const int at = (int) std::lround (s.sn[h].beat * spb * FS);
                if (at <= i) { e.noteOn (s.sn[h].note, s.sn[h].vel); ++h; continue; }
                m = std::min (m, at - i);
            }
            e.process (s.snare, sl.data() + i, sr.data() + i, m);
            i += m;
        }
        const int lat = e.latencySamples();

        if (s.kick != nullptr)
        {
            ks::Engine k; k.prepare (FS, 256);
            const ks::Params kp = ks::presetParams (ksPreset (s.kick));
            size_t kh = 0;
            for (int i = 0; i < n; )
            {
                int m = std::min (256, n - i);
                if (kh < s.kk.size())
                {
                    const int at = (int) std::lround (s.kk[kh].beat * spb * FS);
                    if (at <= i) { k.noteOn (s.kk[kh].note, s.kk[kh].vel); ++kh; continue; }
                    m = std::min (m, at - i);
                }
                k.process (kp, kick.data() + i, m);
                i += m;
            }
            //  both engines report the same latency; line them up regardless
            const int kl = k.latencySamples() - lat;
            if (kl > 0) kick.erase (kick.begin(), kick.begin() + kl);
            kick.resize ((size_t) n, 0.0f);
        }

        for (int i = lat; i < n; ++i)
        {
            L.push_back (sl[(size_t) i] + 0.8f * kick[(size_t) i]);
            R.push_back (sr[(size_t) i] + 0.8f * kick[(size_t) i]);
        }
    }
    //  normalise the whole passage to -1 dBFS, one gain for all of it
    float pk = 0.0f;
    for (size_t i = 0; i < L.size(); ++i) pk = std::max ({ pk, std::abs (L[i]), std::abs (R[i]) });
    const float g = pk > 0.0f ? 0.891f / pk : 1.0f;
    for (size_t i = 0; i < L.size(); ++i) { L[i] *= g; R[i] *= g; }
    const int fade = (int) (0.05 * FS);
    for (int i = 0; i < fade && i < (int) L.size(); ++i)
    {
        L[L.size() - 1 - (size_t) i] *= (float) i / fade;
        R[R.size() - 1 - (size_t) i] *= (float) i / fade;
    }
    writeWav (file, L, R);
    std::printf ("  %s  %.1f s\n", file.c_str(), L.size() / FS);
}

//  patterns, in beats over two bars
std::vector<Note> fourFloor (int bars = 2) { std::vector<Note> v; for (int b = 0; b < bars * 4; ++b) v.push_back ({ (double) b, 36, 1.0f }); return v; }
std::vector<Note> backbeat (int bars = 2) { std::vector<Note> v; for (int b = 0; b < bars; ++b) { v.push_back ({ b * 4 + 1.0, 38, 1.0f }); v.push_back ({ b * 4 + 3.0, 38, 1.0f }); } return v; }
std::vector<Note> rockKick (int bars = 2) { std::vector<Note> v; for (int b = 0; b < bars; ++b) { v.push_back ({ b * 4 + 0.0, 36, 1.0f }); v.push_back ({ b * 4 + 2.0, 36, 0.95f }); v.push_back ({ b * 4 + 2.5, 36, 0.8f }); } return v; }
//  a backbeat with ghost notes: the thing a snare with a velocity curve is for
std::vector<Note> ghosts (int bars = 2)
{
    std::vector<Note> v;
    for (int b = 0; b < bars; ++b)
    {
        v.push_back ({ b * 4 + 0.75, 38, 0.22f }); v.push_back ({ b * 4 + 1.0, 38, 1.0f });
        v.push_back ({ b * 4 + 1.75, 38, 0.25f }); v.push_back ({ b * 4 + 2.25, 38, 0.2f });
        v.push_back ({ b * 4 + 3.0, 38, 1.0f });   v.push_back ({ b * 4 + 3.5, 38, 0.28f });
    }
    return v;
}
//  the Amen's snare line, near enough: 2, the "and" of 3 and 4 with ghosts
std::vector<Note> amen (int bars = 2)
{
    std::vector<Note> v;
    for (int b = 0; b < bars; ++b)
    {
        v.push_back ({ b * 4 + 1.0, 38, 1.0f }); v.push_back ({ b * 4 + 1.75, 38, 0.45f });
        v.push_back ({ b * 4 + 2.25, 38, 0.5f }); v.push_back ({ b * 4 + 3.0, 38, 1.0f });
        v.push_back ({ b * 4 + 3.75, 38, 0.5f });
    }
    return v;
}
std::vector<Note> amenKick (int bars = 2) { std::vector<Note> v; for (int b = 0; b < bars; ++b) { v.push_back ({ b * 4 + 0.0, 36, 1.0f }); v.push_back ({ b * 4 + 0.5, 36, 0.9f }); v.push_back ({ b * 4 + 2.5, 36, 1.0f }); } return v; }
//  a roll into the next bar
std::vector<Note> rollOut (int steps, double from, double len, float v0, float v1)
{
    std::vector<Note> v;
    for (int i = 0; i < steps; ++i) v.push_back ({ from + len * i / steps, 38, v0 + (v1 - v0) * i / std::max (1, steps - 1) });
    return v;
}
std::vector<Note> cat (std::vector<Note> a, const std::vector<Note>& b) { a.insert (a.end(), b.begin(), b.end()); std::sort (a.begin(), a.end(), [] (auto& x, auto& y) { return x.beat < y.beat; }); return a; }
} // namespace

int main (int argc, char** argv)
{
   #if defined(_M_X64) || defined(__SSE2__)
    _mm_setcsr (_mm_getcsr() | 0x8040);
   #endif
    const std::string dir = argc > 1 ? argv[1] : ".";

    // 1. ACOUSTIC, with the ghost notes a velocity curve is for
    renderSegs (dir + "/snare-tactics-01-acoustic.wav", {
        { snareOf ("Studio Snare"),  "Studio Kick", 92,  8, ghosts(),   rockKick() },
        { snareOf ("Piccolo Funk"),  "Felt Beater", 100, 8, ghosts(),   rockKick() },
        { snareOf ("Jazz Snare"),    "Jazz Kick",   96,  8, ghosts(),   { {0,36,0.7f}, {4,36,0.7f} } },
        { snareOf ("Rock Rim Shot"), "Rock Kick",   96,  8, backbeat(), rockKick() },
        { snareOf ("Big Eighties"),  "Rock Kick",   96,  8, backbeat(), rockKick() } });

    // 2. MACHINES
    renderSegs (dir + "/snare-tactics-02-machines.wav", {
        { snareOf ("808 Snare"),     "808 Boom",    100, 8, backbeat(), { {0,36,1}, {2.5,36,0.8f}, {4,36,1}, {6.5,36,0.8f} } },
        { snareOf ("909 Snare"),     "909 Classic", 124, 8, backbeat(), fourFloor() },
        { snareOf ("LinnDrum"),      "LinnDrum",    104, 8, backbeat(), rockKick() },
        { snareOf ("SP-1200 Crack"), "SP-12 Boom Bap", 92, 8, backbeat(), rockKick() },
        { snareOf ("Simmons SDS-V"), "DMX",         112, 8, backbeat(), rockKick() } });

    // 3. TECHNO
    renderSegs (dir + "/snare-tactics-03-techno.wav", {
        { snareOf ("Techno Crush"),   "Hard Techno",   132, 8, backbeat(), fourFloor() },
        { snareOf ("Warehouse Clap"), "Techno Rumble", 130, 8, backbeat(), fourFloor() },
        { snareOf ("Detroit Stack"),  "909 Classic",   126, 8, backbeat(), fourFloor() },
        { snareOf ("Dark Room"),      "Techno Rumble", 128, 8, backbeat(), fourFloor() } });

    // 4. DUB: the echo on the host tempo
    renderSegs (dir + "/snare-tactics-04-dub.wav", {
        { snareOf ("Roots Rim Shot"),   "Clean Sub", 76, 8, { {2,38,1}, {6,38,1} }, { {0,36,1}, {4,36,1} } },
        { snareOf ("Steppers"),         "Clean Sub", 76, 8, backbeat(), fourFloor() },
        { snareOf ("Tubby Throw"),      "Clean Sub", 72, 8, { {2,38,1}, {6,38,1}, {7.5,38,0.8f} }, { {0,36,1}, {4,36,1} } },
        { snareOf ("Dub Techno Space"), "House Thump", 120, 8, backbeat(), fourFloor() } });

    // 5. INDUSTRIAL
    renderSegs (dir + "/snare-tactics-05-industrial.wav", {
        { snareOf ("Gated Machine"),     "Industrial Crush", 118, 8, backbeat(), fourFloor() },
        { snareOf ("EBM Snap"),          "Industrial Crush", 124, 8, cat (backbeat(), { {3.5,38,0.7f}, {7.5,38,0.7f} }), fourFloor() },
        { snareOf ("Metal Shop"),        "Hard Techno", 120, 8, backbeat(), fourFloor() },
        { snareOf ("Power Electronics"), "Industrial Crush", 110, 8, backbeat(), { {0,36,1}, {1.5,36,0.8f}, {4,36,1}, {5.5,36,0.8f} } } });

    // 6. DIGITAL HARDCORE and the break
    renderSegs (dir + "/snare-tactics-06-hardcore.wav", {
        { snareOf ("Amen Chop"),       "SP-12 Boom Bap", 170, 8, amen(), amenKick() },
        { snareOf ("Atari Riot"),      "Gabber",   180, 8, cat (amen(), rollOut (8, 7.0, 1.0, 0.6f, 1.0f)), amenKick() },
        { snareOf ("Breakcore Blast"), "Hardstyle", 190, 8, cat (amen (1), rollOut (16, 4.0, 4.0, 0.4f, 1.0f)), amenKick() },
        { snareOf ("Gabber Snare"),    "Gabber",   190, 8, backbeat(), fourFloor() },
        { snareOf ("Speedcore Snap"),  "Gabber",   240, 8, cat (backbeat(), rollOut (16, 6.0, 2.0, 0.5f, 1.0f)), fourFloor() } });

    // 7. ONE KNOB: TENSION from loose to tight, the same snare, one bar each
    {
        std::vector<Seg> s;
        for (int i = 0; i < 5; ++i)
        {
            st::Params p = snareOf ("Studio Snare");
            p.tension = i / 4.0f; p.sizzle = 520.0f - 260.0f * i / 4.0f; p.room = 0.1f;
            s.push_back ({ p, nullptr, 100, 4, { {1,38,1}, {3,38,1}, {3.75,38,0.3f} }, {} });
        }
        renderSegs (dir + "/snare-tactics-07-tension.wav", s);
    }

    // 8. THE ARTICULATIONS: snare, rim shot, cross-stick, clap (GM notes 38 40 37 39)
    {
        const std::vector<Note> kit = { {0,38,1}, {1,40,1}, {2,37,1}, {3,39,1}, {4,38,0.3f}, {4.5,38,0.6f}, {5,40,1}, {6,37,0.8f}, {7,39,1} };
        renderSegs (dir + "/snare-tactics-08-articulations.wav", {
            { snareOf ("Studio Snare"), nullptr, 96, 8, kit, {} },
            { snareOf ("909 Snare"),    nullptr, 120, 8, kit, {} } });
    }

    std::printf ("wrote 8 demos to %s\n", dir.c_str());
    return 0;
}
