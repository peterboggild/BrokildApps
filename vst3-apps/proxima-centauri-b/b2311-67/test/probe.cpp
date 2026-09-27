/*  A window into the artefact. Prints what is actually in the sound — the
    engine's internals are otherwise invisible, and guessing at them is what
    made the first three debugging rounds slow. Also writes WAVs to listen to. */

#include "../Source/Engine.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    const int n = (int) L.size();
    FILE* f = std::fopen (path, "wb");
    if (! f) { std::printf ("  (could not write %s)\n", path); return; }
    auto u32 = [&] (unsigned v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (unsigned short v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + n * 4); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); u32 (16); u16 (1); u16 (2); u32 ((unsigned) sr);
    u32 ((unsigned) sr * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 ((unsigned) n * 4);
    for (int i = 0; i < n; ++i)
    {
        short a = (short) std::lround (32000.0f * std::max (-1.0f, std::min (1.0f, L[(size_t) i])));
        short b = (short) std::lround (32000.0f * std::max (-1.0f, std::min (1.0f, R[(size_t) i])));
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
    std::printf ("  wrote %s  (%.2f s)\n", path, (double) n / sr);
}

static double goertzel (const float* x, int n, double f, double sr)
{
    const double w = 2.0 * 3.14159265358979 * f / sr, coeff = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i)
    {
        const double h = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / (n - 1));   // Hann
        const double s0 = x[i] * h + coeff * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / n;
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const int habit = argc > 1 ? std::atoi (argv[1]) : 0;
    const int note  = argc > 2 ? std::atoi (argv[2]) : 48;
    const double sr = 48000.0;

    ax::Engine e;
    e.prepare (sr, 512);
    ax::applyHabit (habit, e.p);
    e.service();
    const double f0 = e.noteHz (note);
    std::printf ("habit %d   note %d   f0 %.2f Hz   chain %d sites   orders %d   w1 %.5f\n",
                 habit, note, f0, e.chainN.load(), e.starN.load(), e.lowHz.load());

    const int N = (int) (sr * 4.0);
    std::vector<float> L ((size_t) N, 0.0f), R ((size_t) N, 0.0f);
    e.noteOn (note, 1.0f);
    for (int off = 0; off < N; off += 256)
    {
        e.service();
        e.process (L.data() + off, R.data() + off, std::min (256, N - off));
        if (off <= (int) (sr * 1.5) && off + 256 > (int) (sr * 1.5)) e.noteOff (note);
    }

    // envelope
    std::printf ("  envelope (rms per 250 ms): ");
    for (int b = 0; b < 16; ++b)
    {
        double s = 0; const int a = b * N / 16, z = (b + 1) * N / 16;
        for (int i = a; i < z; ++i) s += (double) L[(size_t) i] * L[(size_t) i];
        std::printf ("%.3f ", std::sqrt (s / (z - a)));
    }
    std::printf ("\n");

    // spectrum: what is actually in there, as ratios to the note
    const int W = 32768, at = (int) (sr * 0.25);
    struct Pk { double f, a; };
    std::vector<Pk> sp;
    for (int i = 0; i < 900; ++i)
    {
        const double f = 30.0 * std::pow (20000.0 / 30.0, i / 899.0);
        sp.push_back ({ f, goertzel (L.data() + at, W, f, sr) });
    }
    std::vector<Pk> pk;
    for (size_t i = 1; i + 1 < sp.size(); ++i)
        if (sp[i].a > sp[i-1].a && sp[i].a >= sp[i+1].a) pk.push_back (sp[i]);
    std::sort (pk.begin(), pk.end(), [] (const Pk& a, const Pk& b) { return a.a > b.a; });
    std::printf ("  strongest partials (as multiples of f0):\n   ");
    for (int i = 0; i < 14 && i < (int) pk.size(); ++i)
        std::printf (" %.3f", pk[(size_t) i].f / f0);
    std::printf ("\n    (1.000 = the note itself; 1.414 = sqrt2; 2.414 = the silver ratio)\n");
    double tot = 0; for (auto& q : sp) tot += q.a;
    std::printf ("  fundamental share %.1f %% of the strongest peak; centroid ", 100.0 * goertzel (L.data() + at, W, f0, sr) / std::max (1e-12, pk.empty() ? 1.0 : pk[0].a));
    double cs = 0, cw = 0; for (auto& q : sp) { cs += q.f * q.a; cw += q.a; }
    std::printf ("%.0f Hz\n", cs / std::max (1e-12, cw));
    (void) tot;

    writeWav ((std::string ("probe-h") + std::to_string (habit) + "-n" + std::to_string (note) + ".wav").c_str(), L, R, (int) sr);
    return 0;
}
