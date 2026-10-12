/*  A/B LISTENING SET for the material (2026-10-12): the same short bass
    phrase on one specimen of each family, at CHARACTER 0 (the engine as it
    was) and at the default 0.8. Written as 16-bit stereo WAVs.

      ab104listen <outdir>                                                    */
#include "../Source/Engine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

using namespace ab104;
static const int SR = 48000, BLK = 256;

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
        auto q = [] (float x) { x = x < -1 ? -1 : (x > 1 ? 1 : x); return (int16_t) std::lround (x * 32760.0f); };
        const int16_t l = q (L[(size_t) i]), r = q (R[(size_t) i]);
        std::fwrite (&l, 2, 1, f); std::fwrite (&r, 2, 1, f);
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    //  E1 A1 G1 E2, then a held B1 - a bass line, played, then held
    struct N { double on, off; int note; float vel; };
    const N phrase[] = { {0.10, 0.85, 28, 0.85f}, {1.00, 1.75, 33, 0.70f}, {1.90, 2.65, 31, 0.95f},
                         {2.80, 3.55, 40, 0.80f}, {3.80, 6.30, 35, 0.90f} };
    for (int fam = 0; fam < 8; ++fam)
    {
        int spec = 0; while (materialFamilyOf (spec) != fam) ++spec;
        for (float ch : { 0.0f, 0.8f })
        {
            Engine e; e.p = Params(); e.p.character = ch;
            e.prepare (SR, BLK); e.requestSpecimen (spec);
            std::vector<float> L, R, l (BLK), r (BLK);
            const int nb = (int) (7.5 * SR / BLK);
            for (int b = 0; b < nb; ++b)
            {
                const double t0 = (double) b * BLK / SR, t1 = t0 + (double) BLK / SR;
                for (const auto& x : phrase)
                {
                    if (x.on >= t0 && x.on < t1)   e.noteOn (x.note, x.vel);
                    if (x.off >= t0 && x.off < t1) e.noteOff (x.note);
                }
                e.process (l.data(), r.data(), BLK);
                L.insert (L.end(), l.begin(), l.end()); R.insert (R.end(), r.begin(), r.end());
            }
            char name[160];
            std::snprintf (name, sizeof name, "%s/%d-%s-specimen%03d-%s.wav", dir.c_str(), fam + 1,
                           materialName (fam), spec, ch == 0.0f ? "A-old" : "B-new");
            writeWav (name, L, R);
            std::printf ("  %s\n", name);
        }
    }
    return 0;
}
