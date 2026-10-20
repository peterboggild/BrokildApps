/*  MARMALADE 120 - offline renderer

    Runs a WAV file through the engine, exactly as the plugin would, and
    writes the result: the way to hear the amp on a real DI recording without
    a DAW, and to A/B a netlist change by ear.

        mmrender in.wav out.wav [gain=0.55] [bite=0.25] [bass=0.5] [treble=0.55]
                 [voice=3] [master=0.35] [output=0] [quality=0] [dry=1] [ir=cab.wav]

    Knobs are 0..1 as in the host (gain=0.8 is 8 on the dial), voice is the
    switch position 1..6, output is a trim in dB, ir is a cabinet impulse
    response (WAV, same rate as the input) - without one the output is the
    8 ohm tap. Reads 16/24/32-bit PCM and
    32-bit float WAV, mono or stereo (summed); writes 24-bit mono.

    With no input file but `--riff` in its place, it writes a plucked-string
    riff of its own first (a Karplus-Strong guitar, DI level) and renders that.
*/
#include "Engine.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace mm;

namespace
{
    bool readWav (const std::string& path, std::vector<float>& out, double& sr)
    {
        std::ifstream f (path, std::ios::binary);
        if (! f) return false;
        std::vector<char> d ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
        if (d.size() < 44 || std::memcmp (d.data(), "RIFF", 4) || std::memcmp (d.data() + 8, "WAVE", 4)) return false;

        auto u16 = [&] (size_t o) { uint16_t v; std::memcpy (&v, d.data() + o, 2); return v; };
        auto u32 = [&] (size_t o) { uint32_t v; std::memcpy (&v, d.data() + o, 4); return v; };

        int fmt = 0, ch = 0, bits = 0;
        size_t pos = 12;
        while (pos + 8 <= d.size())
        {
            const uint32_t len = u32 (pos + 4);
            if (! std::memcmp (d.data() + pos, "fmt ", 4))
            {
                fmt = u16 (pos + 8); ch = u16 (pos + 10); sr = u32 (pos + 12); bits = u16 (pos + 22);
                if (fmt == 0xFFFE) fmt = u16 (pos + 32);           // extensible: the real format tag
            }
            else if (! std::memcmp (d.data() + pos, "data", 4))
            {
                const size_t bps = (size_t) bits / 8, frames = std::min ((size_t) len, d.size() - pos - 8) / (bps * (size_t) ch);
                out.assign (frames, 0.0f);
                const char* p = d.data() + pos + 8;
                for (size_t i = 0; i < frames; ++i)
                {
                    float sum = 0.0f;
                    for (int c = 0; c < ch; ++c)
                    {
                        const char* s = p + (i * (size_t) ch + (size_t) c) * bps;
                        float v = 0.0f;
                        if (fmt == 3 && bits == 32) std::memcpy (&v, s, 4);
                        else if (bits == 16) { int16_t x; std::memcpy (&x, s, 2); v = x / 32768.0f; }
                        else if (bits == 24) { int32_t x = (int32_t) ((uint32_t) (uint8_t) s[0] << 8 | (uint32_t) (uint8_t) s[1] << 16 | (uint32_t) (uint8_t) s[2] << 24) >> 8; v = x / 8388608.0f; }
                        else if (bits == 32) { int32_t x; std::memcpy (&x, s, 4); v = x / 2147483648.0f; }
                        sum += v;
                    }
                    out[i] = sum / (float) ch;
                }
                return fmt == 1 || fmt == 3;
            }
            pos += 8 + len + (len & 1);
        }
        return false;
    }

    bool writeWav (const std::string& path, const std::vector<float>& x, double sr)
    {
        std::ofstream f (path, std::ios::binary);
        if (! f) return false;
        auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
        auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
        const uint32_t bytes = (uint32_t) x.size() * 3;
        f.write ("RIFF", 4); w32 (36 + bytes); f.write ("WAVE", 4);
        f.write ("fmt ", 4); w32 (16); w16 (1); w16 (1); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 3); w16 (3); w16 (24);
        f.write ("data", 4); w32 (bytes);
        for (float s : x)
        {
            const int32_t v = (int32_t) std::lround (std::max (-1.0f, std::min (1.0f, s)) * 8388607.0f);
            const char b[3] = { (char) (v & 0xff), (char) ((v >> 8) & 0xff), (char) ((v >> 16) & 0xff) };
            f.write (b, 3);
        }
        return true;
    }

    /*  A plucked string per note (Karplus-Strong with a pick-position comb and
        a pickup's gentle low-pass), so the amp has something guitar-shaped to
        chew on: real attacks, real decays, chords that intermodulate. */
    std::vector<float> riff (double sr)
    {
        struct Note { double t, f, len, vel; };
        std::vector<Note> notes;
        const double E2 = 82.41, A2 = 110.0, G3 = 196.0, B3 = 246.94, E4 = 329.63;
        auto power = [&] (double t, double root, double len)                         // root + fifth + octave
        { notes.push_back ({ t, root, len, 1.0 }); notes.push_back ({ t + 0.004, root * 1.4983, len, 0.9 }); notes.push_back ({ t + 0.008, root * 2.0, len, 0.8 }); };
        double t = 0.1;
        for (int bar = 0; bar < 2; ++bar)
        {
            for (int k = 0; k < 4; ++k) { notes.push_back ({ t, E2, 0.22, 1.0 }); t += 0.25; }   // chugs
            power (t, G3 / 2.0, 0.45); t += 0.5;
            power (t, A2, 0.45); t += 0.5;
        }
        // an open E chord, let ring, then a little lead line
        const double chord[] = { E2, B3 / 2.0, E4 / 2.0, G3 * 1.0595 , B3, E4 };
        for (int i = 0; i < 6; ++i) notes.push_back ({ t + i * 0.018, chord[i], 2.0, 0.85 });
        t += 2.2;
        const double lead[] = { E4, E4 * 1.1225, E4 * 1.2599, E4 * 1.4983, E4 * 1.3348, E4 * 1.4983 };
        for (double f : lead) { notes.push_back ({ t, f, 0.3, 0.9 }); t += 0.3; }
        notes.push_back ({ t, E4 * 1.4983, 1.4, 1.0 });
        t += 1.8;

        std::vector<float> out ((size_t) (t * sr), 0.0f);
        std::mt19937 rng (3);
        std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        for (const auto& n : notes)
        {
            const int period = (int) std::lround (sr / n.f);
            std::vector<float> line ((size_t) period);
            for (auto& s : line) s = u (rng);
            // pick at about 1/7 of the string
            const int pick = std::max (1, period / 7);
            std::vector<float> l2 = line;
            for (int i = 0; i < period; ++i) line[(size_t) i] = l2[(size_t) i] - l2[(size_t) ((i + pick) % period)];
            const size_t start = (size_t) (n.t * sr), len = (size_t) (n.len * sr);
            float lp = 0.0f, prev = 0.0f;
            const float damp = 0.996f - 0.0000035f * (float) n.f;
            for (size_t i = 0; i < len && start + i < out.size(); ++i)
            {
                const size_t k = i % (size_t) period;
                const float s = line[k];
                line[k] = damp * 0.5f * (s + prev);
                prev = s;
                const float env = (i + 400 > len) ? (float) (len - i) / 400.0f : 1.0f;   // a palm, not a click
                lp += 0.35f * (s - lp);
                out[start + i] += 0.30f * (float) n.vel * lp * env;
            }
        }
        return out;
    }
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("usage: mmrender in.wav|--riff out.wav [gain=..] [bite=..] [bass=..] [treble=..]\n"
                     "                [voice=1..6] [master=..] [output=dB] [quality=0|1] [ir=cab.wav]\n");
        return 2;
    }

    std::vector<float> in;
    double sr = 48000.0;
    if (std::string (argv[1]) == "--riff") in = riff (sr);
    else if (! readWav (argv[1], in, sr)) { std::printf ("could not read %s (PCM or float WAV only)\n", argv[1]); return 1; }

    Params p;
    bool dry = false;
    std::string irPath;
    for (int i = 3; i < argc; ++i)
    {
        const std::string a (argv[i]);
        const auto eq = a.find ('=');
        if (eq == std::string::npos) continue;
        const std::string k = a.substr (0, eq);
        const double v = std::atof (a.c_str() + eq + 1);
        if      (k == "gain")    p.gain = (float) v;
        else if (k == "bite")    p.hfDrive = (float) v;
        else if (k == "bass")    p.bass = (float) v;
        else if (k == "treble")  p.treble = (float) v;
        else if (k == "voice")   p.fac = (int) v - 1;
        else if (k == "master")  p.master = (float) v;
        else if (k == "output")  p.output = (float) ((v + 24.0) / 36.0);
        else if (k == "ir")      irPath = a.substr (eq + 1);
        else if (k == "quality") p.quality = (int) v;
        else if (k == "dry")     dry = v > 0.5;        // write the input untouched, to compare
        else std::printf ("ignoring %s\n", a.c_str());
    }

    if (dry) return writeWav (argv[2], in, sr) ? 0 : 1;

    Engine e;
    if (! e.ok()) { std::printf ("engine: %s\n", e.error().c_str()); return 1; }
    e.setParams (p);
    e.prepare (sr, 512);

    // the plugin's latency is trimmed off, so the output lines up with the input
    const int lat = e.latencySamples();
    std::vector<float> out (in.size() + (size_t) lat, 0.0f), L (512), R (512);
    std::vector<float> src = in;
    src.resize (in.size() + (size_t) lat, 0.0f);
    for (size_t s = 0; s < src.size(); s += 512)
    {
        const int n = (int) std::min ((size_t) 512, src.size() - s);
        for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = src[s + (size_t) i];
        e.process (L.data(), nullptr, n);
        for (int i = 0; i < n; ++i) out[s + (size_t) i] = L[(size_t) i];
    }
    out.erase (out.begin(), out.begin() + lat);

    /*  A cabinet, if one is given: a measured impulse response, convolved
        directly (exact, and offline there is time). Scaled to unit energy,
        as the plugin's loader normalises it, so a change of cabinet is a
        change of tone more than of level. Its rate must match the input's. */
    if (! irPath.empty())
    {
        std::vector<float> ir;
        double irRate = 0.0;
        if (! readWav (irPath, ir, irRate)) { std::printf ("could not read IR %s\n", irPath.c_str()); return 1; }
        if (std::abs (irRate - sr) > 0.5) { std::printf ("IR is %.0f Hz, input %.0f Hz: resample one first\n", irRate, sr); return 1; }
        double e2 = 0.0;
        for (float v : ir) e2 += (double) v * v;
        const double g = e2 > 0.0 ? 1.0 / std::sqrt (e2) : 1.0;
        std::vector<float> wet (out.size(), 0.0f);
        for (size_t i = 0; i < out.size(); ++i)
        {
            double acc = 0.0;
            const size_t kmax = std::min (ir.size(), i + 1);
            for (size_t k = 0; k < kmax; ++k) acc += (double) ir[k] * out[i - k];
            wet[i] = (float) (acc * g);
        }
        out.swap (wet);
    }

    float peak = 0.0f;
    for (float s : out) peak = std::max (peak, std::abs (s));
    if (! writeWav (argv[2], out, sr)) { std::printf ("could not write %s\n", argv[2]); return 1; }
    std::printf ("%s: %.1f s at %.0f Hz, peak %.1f dBFS%s\n", argv[2], (double) out.size() / sr, sr,
                 20.0 * std::log10 (std::max (peak, 1e-9f)), peak >= 1.0f ? "  (CLIPPED - lower OUTPUT)" : "");
    return 0;
}
