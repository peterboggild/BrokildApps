/*  Render the demo passages that ship with the finding. Each is a short piece
    made only by the object — no external processing — written as a 16-bit
    stereo WAV. The landing page and the findings report play these. */
#include "../Source/Engine.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

using namespace ab104;
static const int SR = 48000, BLK = 256;

struct Ev { double t; int kind; int a; float v; };  // 0 on,1 off,2 strike,3 pour,4 param,5 bend

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    const int n = (int) std::min (L.size(), R.size());
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { std::printf ("  cannot write %s\n", path.c_str()); return; }
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const int dataBytes = n * 2 * 2;
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); u32 (16); u16 (1); u16 (2);
    u32 (SR); u32 (SR * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (int i = 0; i < n; ++i)
    {
        auto q = [] (float x) { x = x < -1 ? -1 : (x > 1 ? 1 : x); return (int16_t) std::lround (x * 32760.0f); };
        int16_t l = q (L[(size_t) i]), r = q (R[(size_t) i]);
        std::fwrite (&l, 2, 1, f); std::fwrite (&r, 2, 1, f);
    }
    std::fclose (f);
    std::printf ("  wrote %s  (%.1f s)\n", path.c_str(), (double) n / SR);
}

static double clampd01 (double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static int pid (const char* id)
{ for (int i = 0; i < numParams(); ++i) if (std::strcmp (paramSpec (i).id, id) == 0) return i; return -1; }

static void render (const std::string& path, double secs, int specimen,
                    std::vector<Ev> evs, std::vector<std::pair<std::string,float>> setup)
{
    Engine e; e.p = Params();
    for (auto& s : setup) { int i = pid (s.first.c_str()); if (i >= 0) paramSpec (i).get (e.p) = s.second; }
    e.prepare (SR, BLK);
    e.requestSpecimen (specimen);

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
            else if (v.kind == 2) e.strike (v.a, v.v);
            else if (v.kind == 3) e.pour (v.a, v.v);
            else if (v.kind == 4) { int i = pid (nullptr); (void) i; }   // unused
            else if (v.kind == 5) e.setBend (v.v);
        }
        e.process (bl.data(), br.data(), BLK);
        L.insert (L.end(), bl.begin(), bl.end());
        R.insert (R.end(), br.begin(), br.end());
    }
    //  a short fade so a loop is clean
    const int fade = SR / 20;
    for (int i = 0; i < fade && i < (int) L.size(); ++i)
    {
        const float g = (float) i / fade;
        L[i] *= g; R[i] *= g;
        L[L.size()-1-i] *= g; R[R.size()-1-i] *= g;
    }
    writeWav (path, L, R);
}

int main (int argc, char** argv)
{
    std::string dir = argc > 1 ? argv[1] : ".";
    auto p = [&] (const char* n) { return dir + "/" + n; };

    //  1 — the grid, unplayed: hot ambient, onset low enough that traffic
    //  crosses it, and the grid carries on its own — the survey's "energy
    //  distribution" hypothesis made audible. A couple of faint held orders
    //  seed the web so it does not take the whole take to warm.
    render (p ("B2311-104-01_the-grid-unplayed.wav"), 20.0, 3,
            { {0.5,0,24,0.4f}, {2.5,1,24,0}, {1.0,0,31,0.35f}, {3.0,1,31,0} },
            { {"onset", 0.04f}, {"traffic", 0.9f}, {"ambient", 0.93f}, {"conduction", 0.7f},
              {"leak", 0.2f}, {"turn", 0.5f} });

    //  2 — a bassline: thermal orders, one conduit heating to each note, with
    //  a little turbulence and stiffness for the metal to speak
    {
        std::vector<Ev> b;
        const int seq[] = { 26, 26, 33, 26, 31, 26, 33, 36 };
        double t = 0.3;
        for (int k = 0; k < 24; ++k)
        {
            const int nt = seq[k % 8] - (k / 8) * 0;
            b.push_back ({ t, 0, nt, 0.85f });
            b.push_back ({ t + 0.42, 1, nt, 0 });
            t += 0.5;
        }
        render (p ("B2311-104-02_a-bassline.wav"), t + 1.5, 9, b,
                { {"flux", 0.7f}, {"mass", 0.25f}, {"leak", 0.6f}, {"steepen", 0.5f},
                  {"turbulence", 0.22f}, {"stiffness", 0.45f} });
    }

    //  3 — the thermal glide: heavy MASS, so every note arrives slowly, the
    //  conduit audibly heating toward the ordered pitch
    render (p ("B2311-104-03_the-thermal-glide.wav"), 14.0, 21,
            { {0.3,0,24,0.9f}, {3.5,1,24,0}, {4.0,0,36,0.9f}, {8.0,1,36,0}, {8.5,0,29,0.9f}, {13.0,1,29,0} },
            { {"mass", 0.9f}, {"flux", 0.3f}, {"leak", 0.3f}, {"steepen", 0.4f}, {"level", 0.5f} });

    //  4 — cold to warm: the same order struck again and again while AMBIENT
    //  rises from near-frozen to white heat. Cold, the conduit is sluggish and
    //  dark and barely obeys; warmed, it comes alive — the temperature-as-life
    //  of the whole finding, in one gesture. (Ordered notes stay audible
    //  whatever the section does; a bare STRIKE on an out-of-section conduit
    //  radiates nothing, which is the panel's own rule.)
    {
        Engine e; e.p = Params(); e.p.onset = 0.30f; e.p.traffic = 0.25f;
        e.p.conduction = 0.5f; e.p.flux = 0.55f; e.p.leak = 0.5f; e.p.mass = 0.35f;
        e.p.level = 0.5f;
        e.prepare (SR, BLK); e.requestSpecimen (41);
        const int aIdx = pid ("ambient");
        std::vector<float> L, R; std::vector<float> bl (BLK), br (BLK);
        const int nb = (int) (22.0 * SR / BLK);
        const int notes[] = { 26, 33, 29, 36 };
        for (int b = 0; b < nb; ++b)
        {
            const double f = (double) b / nb;
            paramSpec (aIdx).get (e.p) = (float) (0.0 + 0.97 * f);    // 77 K -> ~750 K
            const int per = (int) (1.1 * SR / BLK);
            if (b % per == 0)      e.noteOn (notes[(b/per) % 4], 0.9f);
            else if (b % per == (int)(0.75*per)) e.noteOff (notes[(b/per) % 4]);
            e.process (bl.data(), br.data(), BLK);
            L.insert (L.end(), bl.begin(), bl.end()); R.insert (R.end(), br.begin(), br.end());
        }
        const int fade = SR / 20;
        for (int i = 0; i < fade; ++i){ float g=(float)i/fade; L[i]*=g;R[i]*=g;L[L.size()-1-i]*=g;R[R.size()-1-i]*=g; }
        writeWav (p ("B2311-104-04_cold-to-warm.wav"), L, R);
    }

    //  5 — sympathetic: play one conduit, its neighbours warm through the
    //  junctions and begin to carry on their own
    render (p ("B2311-104-05_sympathetic.wav"), 16.0, 67,
            { {0.5,0,28,1.0f}, {6.0,1,28,0}, {6.5,0,28,1.0f}, {12.0,1,28,0} },
            { {"conduction", 0.85f}, {"bleed", 0.6f}, {"onset", 0.28f}, {"leak", 0.35f},
              {"flux", 0.6f}, {"stiffness", 0.5f} });

    //  6 — the tone comes apart: a single held note while TURBULENCE is raised
    //  from nothing to full, so the clean tube tone period-doubles and then
    //  goes to chaos — the thermoacoustic route the survey did not expect
    {
        Engine e; e.p = Params(); e.p.stiffness = 0.35f; e.p.discipline = 0.85f;
        e.p.onset = 0.35f; e.prepare (SR, BLK); e.requestSpecimen (55);
        const int tIdx = pid ("turbulence");
        std::vector<float> L, R; std::vector<float> bl (BLK), br (BLK);
        const int nb = (int) (18.0 * SR / BLK);
        const int notes[] = { 24, 24, 31, 24 };
        for (int b = 0; b < nb; ++b)
        {
            const double f = (double) b / nb;
            paramSpec (tIdx).get (e.p) = (float) clampd01 (f * 1.05);
            const int per = (int) (4.3 * SR / BLK);
            if (b % per == 0)      e.noteOn (notes[(b/per) % 4], 0.92f);
            else if (b % per == (int)(0.9*per)) e.noteOff (notes[(b/per) % 4]);
            e.process (bl.data(), br.data(), BLK);
            L.insert (L.end(), bl.begin(), bl.end()); R.insert (R.end(), br.begin(), br.end());
        }
        const int fade = SR / 20;
        for (int i = 0; i < fade; ++i){ float g=(float)i/fade; L[i]*=g;R[i]*=g;L[L.size()-1-i]*=g;R[R.size()-1-i]*=g; }
        writeWav (p ("B2311-104-06_the-tone-comes-apart.wav"), L, R);
    }

    std::printf ("done\n");
    return 0;
}
