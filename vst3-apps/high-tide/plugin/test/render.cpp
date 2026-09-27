/*  HIGH TIDE — demo passages, one per factory patch, to WAV.
    htrender <outdir>  */
#include "../Source/Engine.h"
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#if defined(_MSC_VER)
#include <immintrin.h>
#endif

using namespace ht;
static const int SR = 48000, BLK = 256;

static void wav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    FILE* f = std::fopen (path.c_str(), "wb"); if (! f) return;
    const uint32_t n = (uint32_t) L.size(), data = n * 4, rate = SR;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + data); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 (rate); w32 (rate * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (data);
    for (uint32_t i = 0; i < n; ++i)
    {
        const float l = std::max (-1.0f, std::min (1.0f, L[i])), r = std::max (-1.0f, std::min (1.0f, R[i]));
        w16 ((uint16_t) (int16_t) std::lround (l * 32767)); w16 ((uint16_t) (int16_t) std::lround (r * 32767));
    }
    std::fclose (f);
}

struct Ev { double t; int n; float v; bool on; };

int main (int argc, char** argv)
{
   #if defined(_MSC_VER)
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_ON);
   #endif
    const std::string out = argc > 1 ? argv[1] : ".";
    //  one passage: a slow phrase in A minor, a chord, a bass note, a held note
    const std::vector<Ev> phrase = {
        { 0.0, 45, 0.8f, true }, { 2.5, 45, 0, false },
        { 2.6, 52, 0.7f, true }, { 4.6, 52, 0, false },
        { 4.7, 48, 0.75f, true }, { 4.7, 55, 0.6f, true }, { 4.7, 60, 0.6f, true }, { 8.5, 48, 0, false }, { 8.5, 55, 0, false }, { 8.5, 60, 0, false },
        { 9.0, 33, 0.95f, true }, { 12.0, 33, 0, false },
        { 12.2, 57, 0.5f, true }, { 16.0, 57, 0, false },
    };
    for (int i = 0; i < numFactory(); ++i)
    {
        Engine e; e.p = Params(); e.prepare (SR, BLK);
        Lanes l; factory (i).build (e.terrain(), l, e.p); e.setLanes (l);
        std::vector<float> L, R, bl (BLK), br (BLK);
        size_t ei = 0;
        const int nb = 19 * SR / BLK;
        for (int b = 0; b < nb; ++b)
        {
            const double now = (double) b * BLK / SR;
            while (ei < phrase.size() && phrase[ei].t <= now) { const auto& v = phrase[ei++]; if (v.on) e.noteOn (v.n, v.v); else e.noteOff (v.n); }
            e.process (bl.data(), br.data(), BLK);
            L.insert (L.end(), bl.begin(), bl.end()); R.insert (R.end(), br.begin(), br.end());
        }
        std::string name = factory (i).name;
        for (auto& c : name) c = (c == ' ') ? '-' : (char) std::tolower (c);
        wav (out + "/High-Tide-" + name + ".wav", L, R);
        std::printf ("  %s\n", name.c_str());
    }
    return 0;
}
