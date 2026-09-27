/*  beetrender - the demos on Beetmachine's landing page.

    Plays real grooves on the factory kits through the REAL processor (eight
    slots, chokes, latency alignment and all) and writes 16-bit stereo WAVs of
    the main mix. tools/wav2mp3.js (Kickstart's) turns them into mp3.

      beetrender <output folder>

    A groove is one string of steps per slot, sixteen to the bar:
      X = full, x = medium, o = ghost, . = rest
    Slots follow the convention: 1-2 kicks, 3-4 snares, 5 closed hat, 6 open
    hat (choked by 5 in every kit), 7 ride, 8 crash.                          */

#include <JuceHeader.h>
#include "../src/BeetProcessor.h"
#include "../src/BeetKits.h"

constexpr double SR = 48000.0;
constexpr int BS = 256;

struct Seg
{
    const char* kit;
    double bpm;
    int bars;
    double swing;                 // 0.5 = straight, 0.6 = a lazy shuffle
    const char* steps[beet::NUM_SLOTS];
    bool crashIn;                 // crash (slot 8) on the first beat
};

struct Hit { int64_t at; int note; float vel; };

static int kitIndex (const char* name)
{
    for (int i = 0; i < beet::numKits(); ++i)
        if (juce::String (beet::kit (i).name) == name) return i;
    std::printf ("  no kit named %s\n", name);
    std::exit (1);
}

static float velOf (char c) { return c == 'X' ? 1.0f : c == 'x' ? 0.72f : c == 'o' ? 0.42f : 0.0f; }

static std::vector<float> renderSegs (const std::vector<Seg>& segs, double tailSeconds = 2.0)
{
    BeetProcessor p;
    p.setRateAndBufferSizeDetails (SR, BS);
    p.prepareToPlay (SR, BS);
    const int chans = p.getTotalNumOutputChannels();
    const int lat = p.getLatencySamples();

    //  the schedule: kit changes and hits, in samples
    std::vector<std::pair<int64_t, int>> kitAt;
    std::vector<Hit> hits;
    double t0 = 0.0;
    for (const auto& s : segs)
    {
        kitAt.push_back ({ (int64_t) (t0 * SR), kitIndex (s.kit) });
        const double step = 60.0 / s.bpm / 4.0;
        const int nSteps = s.bars * 16;
        for (int k = 0; k < beet::NUM_SLOTS; ++k)
        {
            const char* pat = s.steps[k];
            const int len = (int) std::strlen (pat);
            if (len == 0) continue;
            for (int i = 0; i < nSteps; ++i)
            {
                const float v = velOf (pat[i % len]);
                if (v <= 0.0f) continue;
                const double swingOff = (i % 2 == 1) ? (s.swing - 0.5) * 2.0 * step : 0.0;
                hits.push_back ({ (int64_t) ((t0 + i * step + swingOff) * SR) + 64, p.slotNote (k), v });
            }
        }
        if (s.crashIn) hits.push_back ({ (int64_t) (t0 * SR) + 64, p.slotNote (7), 0.9f });
        t0 += nSteps * step;
    }

    const int64_t total = (int64_t) ((t0 + tailSeconds) * SR) + lat;
    std::vector<float> out ((size_t) (2 * total), 0.0f);
    juce::AudioBuffer<float> block (chans, BS);
    size_t nextKit = 0;
    for (int64_t pos = 0; pos < total; pos += BS)
    {
        while (nextKit < kitAt.size() && kitAt[nextKit].first < pos + BS)
            p.loadKit (kitAt[nextKit++].second);             // between blocks, the way a host would
        const int n = (int) juce::jmin<int64_t> (BS, total - pos);
        juce::AudioBuffer<float> b (block.getArrayOfWritePointers(), chans, n);
        b.clear();
        juce::MidiBuffer midi;
        for (const auto& h : hits)
            if (h.at >= pos && h.at < pos + n)
                midi.addEvent (juce::MidiMessage::noteOn (1, h.note, h.vel), (int) (h.at - pos));
        p.processBlock (b, midi);
        for (int i = 0; i < n; ++i)
        {
            const int64_t o = pos + i - lat;                   // the host compensates the latency; so do we
            if (o < 0) continue;
            out[(size_t) (2 * o)]     = b.getSample (0, i);
            out[(size_t) (2 * o + 1)] = b.getSample (1, i);
        }
    }
    out.resize ((size_t) (2 * (total - lat)));
    return out;
}

static void writeWav (const std::string& file, std::vector<float> x)
{
    float peak = 0.0f;
    double e = 0.0;
    for (float v : x) { peak = juce::jmax (peak, std::abs (v)); e += (double) v * v; }
    const float gain = peak > 0.0f ? 0.891f / peak : 1.0f;           // every demo peaks at -1 dBFS
    for (float& v : x) v *= gain;
    const double rms = std::sqrt (e / (double) juce::jmax<size_t> (1, x.size())) * gain;

    FILE* f = std::fopen (file.c_str(), "wb");
    if (! f) { std::printf ("  cannot write %s\n", file.c_str()); std::exit (1); }
    auto w32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const uint32_t data = (uint32_t) (x.size() * 2);
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + data); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) SR); w32 ((uint32_t) SR * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (data);
    for (float v : x) { const int s = juce::jlimit (-32767, 32767, (int) std::lround (v * 32767.0f)); w16 ((uint16_t) (int16_t) s); }
    std::fclose (f);
    std::printf ("  %-40s %5.1f s  gain %+5.1f dB  rms %5.1f dBFS\n", file.c_str(), x.size() / 2.0 / SR,
                 20.0 * std::log10 (gain), 20.0 * std::log10 (juce::jmax (1e-9, rms)));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const std::string dir = argc > 1 ? argv[1] : ".";
    juce::File (dir).createDirectory();

    //                 1 kick              2 kick              3 snare             4 snare             5 closed hat        6 open hat          7 ride              8 crash
    writeWav (dir + "/beetmachine-01-studio.wav", renderSegs ({
        { "STUDIO", 96, 8, 0.5, { "X.......X.X.....", "", "....X.......X...", "..........o.....", "x.x.x.x.x.x.x.x.", "", "", "" }, true },
    }));
    writeWav (dir + "/beetmachine-02-808.wav", renderSegs ({
        { "808", 112, 8, 0.5, { "X......X..X.....", "..........X..x..", "....X.......X...", "............X...", "x.xxx.x.x.xxx...", "..............X.", "", "" }, true },
    }));
    writeWav (dir + "/beetmachine-03-909.wav", renderSegs ({
        { "909", 124, 8, 0.5, { "X...X...X...X...", "", "....X.......X...", "", "o...o...o...o...", "..X...X...X...X.", "", "" }, true },
    }));
    writeWav (dir + "/beetmachine-04-boom-bap.wav", renderSegs ({
        { "BOOM BAP", 88, 8, 0.6, { "X......X..X.....", "", "....X.......X...", ".......o......o.", "x.x.x.x.x.x.x.x.", "", "", "" }, false },
    }));
    writeWav (dir + "/beetmachine-05-techno.wav", renderSegs ({
        { "TECHNO", 132, 8, 0.5, { "X...X...X...X...", "", "..............o.", "....X.......X...", "xo.oxo.oxo.oxo.o", "..x...x...x...x.", "x.x.x.x.x.x.x.x.", "" }, true },
    }));
    writeWav (dir + "/beetmachine-06-tour.wav", renderSegs ({
        { "DUB",        75, 4, 0.55, { "........X.......", "", "........X.......", "", "x.x.x...x.x.x.x.", "......x.........", "", "" }, false },
        { "INDUSTRIAL", 120, 4, 0.5, { "X..X..X.X..X..X.", "", "....X.......X...", "", "x.x.x.x.x.x.x.x.", "", "X.......X.......", "" }, true },
        { "TRAP",       140, 4, 0.5, { "X.........X.....", "", "........X.......", "", "x.x.x.xxx.x.xxxx", "", "", "" }, false },
        { "LO-FI",      82, 4, 0.58, { "X.......X.X.....", "", "....X.......X...", "", "x.x.x.x.x.x.x.x.", "", "", "" }, false },
    }));
    return 0;
}
