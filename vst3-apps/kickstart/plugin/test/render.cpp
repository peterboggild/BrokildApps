// KICKSTART demo renderer: the real engine, the factory presets, written to
// 16-bit stereo WAV for the landing page (tools/wav2mp3.js turns them into mp3).
//
//   ksrender <outdir>

#include "../engine/ks_engine.h"
#include "../engine/ks_presets.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(_M_X64) || defined(__SSE2__)
 #include <xmmintrin.h>
#endif

using namespace ks;

namespace
{
constexpr double FS = 48000.0;

int presetByName (const char* n)
{
    for (int i = 0; i < numPresets(); ++i) if (std::strcmp (preset (i).name, n) == 0) return i;
    std::printf ("no preset %s\n", n); return 0;
}

struct Hit { double beat; int note; float vel; };

//  one segment: a preset (optionally changed), played over a pattern
struct Seg
{
    Params p;
    double bpm;
    std::vector<Hit> hits;
    double beats;
};

void writeWav (const std::string& file, const std::vector<float>& x)
{
    FILE* f = std::fopen (file.c_str(), "wb");
    if (! f) return;
    const unsigned n = (unsigned) x.size(), data = n * 4, rate = (unsigned) FS;
    auto w32 = [&] (unsigned v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (unsigned short v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + data); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 (rate); w32 (rate * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (data);
    for (float v : x)
    {
        const short s = (short) std::lround (std::max (-1.0f, std::min (1.0f, v)) * 32767.0f);
        std::fwrite (&s, 2, 1, f); std::fwrite (&s, 2, 1, f);
    }
    std::fclose (f);
}

std::vector<float> renderSegs (const std::vector<Seg>& segs)
{
    std::vector<float> out;
    for (const auto& s : segs)
    {
        Engine e; e.prepare (FS, 256);
        const double spb = 60.0 / s.bpm;
        const int n = (int) (s.beats * spb * FS);
        std::vector<float> buf ((size_t) n, 0.0f);
        size_t h = 0;
        for (int i = 0; i < n; )
        {
            int m = std::min (256, n - i);
            if (h < s.hits.size())
            {
                const int at = (int) std::lround (s.hits[h].beat * spb * FS);
                if (at <= i) { e.noteOn (s.hits[h].note, s.hits[h].vel); ++h; continue; }
                m = std::min (m, at - i);
            }
            e.process (s.p, buf.data() + i, m);
            i += m;
        }
        //  the engine's latency is taken off, so the pattern sits on the grid
        const int lat = e.latencySamples();
        out.insert (out.end(), buf.begin() + lat, buf.end());
        out.insert (out.end(), (size_t) lat, 0.0f);
    }
    //  a short fade at the very end
    const int fade = (int) (0.05 * FS);
    for (int i = 0; i < fade && i < (int) out.size(); ++i) out[out.size() - 1 - (size_t) i] *= (float) i / fade;
    return out;
}

std::vector<Hit> fourOnFloor (int bars, float accent = 1.0f)
{
    std::vector<Hit> h;
    for (int b = 0; b < bars * 4; ++b) h.push_back ({ (double) b, 36, b % 4 == 0 ? accent : 0.9f });
    return h;
}

Seg seg (const char* name, double bpm, std::vector<Hit> hits, double beats)
{
    return { presetParams (presetByName (name)), bpm, std::move (hits), beats };
}
} // namespace

int main (int argc, char** argv)
{
   #if defined(_M_X64) || defined(__SSE2__)
    _mm_setcsr (_mm_getcsr() | 0x8040);
   #endif
    const std::string dir = argc > 1 ? argv[1] : ".";

    //  a rock/boom-bap pattern: 1, the and of 2, 3 (and a ghost)
    const std::vector<Hit> groove = { {0, 36, 1.0f}, {1.5, 36, 0.75f}, {2, 36, 0.95f}, {3.75, 36, 0.5f},
                                      {4, 36, 1.0f}, {5.5, 36, 0.75f}, {6, 36, 0.95f} };

    // 1. ACOUSTIC
    writeWav (dir + "/kickstart-01-acoustic.wav", renderSegs ({
        seg ("Studio Kick",       96, groove, 8), seg ("Jazz Kick",         96, groove, 8),
        seg ("Rock Kick",         96, groove, 8), seg ("Kit In A Room",     96, groove, 8) }));

    // 2. CLASSIC
    writeWav (dir + "/kickstart-02-classic.wav", renderSegs ({
        seg ("808 Boom",  100, { {0,36,1}, {2.5,36,0.8f}, {4,36,1}, {6.5,36,0.8f} }, 8),
        seg ("909 Classic", 124, fourOnFloor (2), 8),
        seg ("LinnDrum",   104, groove, 8),
        seg ("SP-12 Boom Bap", 92, groove, 8),
        seg ("MPC60 Thump", 92, groove, 8) }));

    // 3. MODERN
    writeWav (dir + "/kickstart-03-modern.wav", renderSegs ({
        seg ("Techno Rumble", 132, fourOnFloor (2), 8),
        seg ("Hardstyle",     150, fourOnFloor (2), 8),
        seg ("Gabber",        180, fourOnFloor (3), 12),
        seg ("Dubstep Punch", 140, { {0,36,1}, {3,36,0.9f}, {4,36,1}, {7,36,0.9f} }, 8),
        seg ("Psytrance Tick",145, fourOnFloor (2), 8) }));

    // 4. TRAP 808, played as a bass line (KEY TRACK on)
    {
        const std::vector<Hit> line = { {0,36,1}, {1.5,36,0.9f}, {2.5,39,0.9f}, {3.25,34,0.8f},
                                        {4,36,1}, {5.5,43,0.9f}, {6.5,41,0.9f}, {7.25,39,0.8f} };
        writeWav (dir + "/kickstart-04-trap-808.wav", renderSegs ({ seg ("Trap 808", 140, line, 8), seg ("Trap 808", 140, line, 8) }));
    }

    // 5. ONE KNOB: SKIN from 0 to 100 % - the same kick, electronic to acoustic
    {
        std::vector<Seg> s;
        for (int i = 0; i < 6; ++i)
        {
            Params p = presetParams (presetByName ("Init"));
            p.skin = i / 5.0f; p.sweep = 18.0f - 12.0f * i / 5.0f; p.click = 0.45f; p.tone = 0.55f - 0.2f * i / 5.0f;
            p.room = 0.25f * i / 5.0f;
            s.push_back ({ p, 110, { {0,36,1}, {1,36,0.9f} }, 2 });
        }
        writeWav (dir + "/kickstart-05-skin.wav", renderSegs (s));
    }

    // 6. THE FOUR DRIVES on one kick
    {
        std::vector<Seg> s;
        for (int e = 0; e < NUM_DRIVE_ENGINES; ++e)
        {
            Params p = presetParams (presetByName ("909 Classic"));
            p.drive = 0.7f; p.engine = (float) e; p.curve = 0.35f; p.decay = 520;
            s.push_back ({ p, 128, fourOnFloor (1), 4 });
        }
        writeWav (dir + "/kickstart-06-drives.wav", renderSegs (s));
    }

    std::printf ("wrote 6 demos to %s\n", dir.c_str());
    return 0;
}
