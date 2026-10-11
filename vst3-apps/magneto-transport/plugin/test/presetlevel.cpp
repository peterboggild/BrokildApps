// Magneto Transport - the factory presets' loudness levelling (stage 8).
//
//   mtlevel            print each preset's loudness against the default machine
//   mtlevel --write=<Source/PresetTrims.h>   and write the trims that match them
//
// The programme: two bars of a synthetic band at 120 BPM (kick, snare, hats, a bass
// line and a chord), looped for 12 s, at about 0 VU. Loudness: ITU-R BS.1770 K-weighting
// (the 48 kHz pre-filter and RLB high-pass), the mean over 2..12 s of both channels, no
// gating (the echoes' tails ARE the preset). OUTPUT is the last gain stage and linear,
// so one pass converges; the trim is clamped to the OUTPUT knob's range.
#include "Engine.h"
#include "Presets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace mt;

static const double FS = 48000.0;

static std::vector<float> programme (size_t n)
{
    std::vector<float> x (n, 0.0f);
    const double beat = 0.5;      // 120 BPM
    unsigned rng = 12345u;
    auto noise = [&rng] { rng = rng * 1664525u + 1013904223u; return (double) (int) (rng >> 8) / 8388608.0 - 1.0; };
    const double bassNotes[8] = { 55.0, 55.0, 65.41, 49.0, 55.0, 55.0, 73.42, 49.0 };
    const double chord[3] = { 220.0, 261.63, 329.63 };
    for (size_t i = 0; i < n; ++i)
    {
        const double t = i / FS;
        const double tb = std::fmod (t, 8.0 * beat);          // two bars of 4/4
        const int b = (int) (tb / beat);
        const double tin = tb - b * beat;
        double s = 0.0;
        if (b % 2 == 0) s += 0.9 * std::exp (-tin / 0.12) * std::sin (2.0 * M_PI * (50.0 + 60.0 * std::exp (-tin / 0.02)) * tin);
        else            s += 0.5 * std::exp (-tin / 0.09) * noise();
        const double th = std::fmod (t, 0.25 * beat);
        s += 0.12 * std::exp (-th / 0.015) * noise();
        const double f0 = bassNotes[b];
        s += 0.35 * std::exp (-tin / 0.4) * (std::sin (2.0 * M_PI * f0 * t) + 0.3 * std::sin (4.0 * M_PI * f0 * t));
        if (b == 0 || b == 4) for (double fc : chord) s += 0.08 * std::exp (-tin / 0.8) * std::sin (2.0 * M_PI * fc * t);
        x[i] = (float) (0.25 * s);
    }
    return x;
}

// BS.1770 K-weighting at 48 kHz
struct Biq { double b0, b1, b2, a1, a2, z1 = 0, z2 = 0; double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
static double loudness (const std::vector<float>& L, const std::vector<float>& R, size_t a, size_t b)
{
    double acc = 0.0;
    for (int c = 0; c < 2; ++c)
    {
        Biq s1 { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
        Biq s2 { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
        const std::vector<float>& x = c == 0 ? L : R;
        for (size_t i = 0; i < b; ++i)
        {
            const double y = s2.run (s1.run (x[i]));
            if (i >= a) acc += y * y;
        }
    }
    return -0.691 + 10.0 * std::log10 (acc / (double) (b - a) + 1e-30);
}

static double renderLoudness (const Params& p, const std::vector<float>& x)
{
    Engine e; e.setParams (p); e.prepare (FS, 256);
    std::vector<float> L (x.size()), R (x.size());
    for (size_t i = 0; i < x.size(); i += 256)
    {
        const int n = (int) std::min<size_t> (256, x.size() - i);
        e.process (x.data() + i, x.data() + i, L.data() + i, R.data() + i, n);
        e.service();
    }
    return loudness (L, R, (size_t) (2.0 * FS), x.size());
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const char* out = nullptr;
    for (int i = 1; i < argc; ++i) if (std::strncmp (argv[i], "--write=", 8) == 0) out = argv[i] + 8;
    const auto x = programme ((size_t) (12.0 * FS));
    // the target is the programme itself: every preset as loud as what goes in, so a
    // bypass A/B is a level match (and a tape-only preset sits at unity)
    const double ref = loudness (x, x, (size_t) (2.0 * FS), x.size());
    { Params def; std::printf ("the programme: %.2f LUFS; the default machine plays it at %.2f\n", ref, renderLoudness (def, x)); }
    std::vector<float> trim (NUM_FACTORY, 0.0f);
    double worst = 0.0;
    for (int i = 0; i < NUM_FACTORY; ++i)
    {
        Params p; std::string err;
        if (! applyFactoryPreset (i, p, &err)) { std::printf ("preset %d (%s): %s\n", i, FACTORY[i].name, err.c_str()); return 1; }
        const double was = p.output;
        p.output = 0.0f;
        const double l = renderLoudness (p, x);
        const double t = std::clamp (ref - l, -24.0, 12.0);
        trim[(size_t) i] = (float) (std::round (t * 10.0) / 10.0);
        worst = std::max (worst, std::abs ((l + was) - ref));
        std::printf ("  %2d %-22s %-13s %7.2f LUFS at OUTPUT 0 -> trim %+5.1f dB (was %+5.1f)\n", i, FACTORY[i].name, FACTORY[i].group, l, trim[(size_t) i], was);
    }
    std::printf ("worst level error with the CURRENT trims: %.2f dB\n", worst);
    if (out)
    {
        FILE* o = std::fopen (out, "w");
        if (! o) { std::printf ("cannot write %s\n", out); return 1; }
        std::fprintf (o, "#pragma once\n// GENERATED by test/presetlevel.cpp (mtlevel --write): each factory preset's OUTPUT, dB,\n"
                         "// so that it plays as loud (K-weighted, the same programme) as the programme itself.\n"
                         "// Re-run after changing a preset or anything that moves levels. Do not edit by hand.\nnamespace mt\n{\ninline constexpr float PRESET_TRIM_DB[] = {\n   ");
        for (int i = 0; i < NUM_FACTORY; ++i) std::fprintf (o, " %.1ff,%s", trim[(size_t) i], (i % 8 == 7) ? "\n   " : "");
        std::fprintf (o, "\n};\n}\n");
        std::fclose (o);
        std::printf ("written %s\n", out);
    }
    return 0;
}
