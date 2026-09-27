/*  GRAVITY WELL - the click A/B.

    Four proxy metrics in a row disagreed with Peter's ears about the note-on
    click - HF ratio, sample step, per-ms bins, preset scan - each for its own
    reason, and each reason was the window or the denominator moving with the
    thing under test.  So this stops measuring and asks him instead: the SAME
    patch, one thing changed per file, eight short files, plainly named.

    Whichever ones are clean name the cause in a single listening pass.

        gwvariants <dir> [preset]
*/
#include "Engine.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

using namespace gw;

static void writeWav (const std::string& path, const std::vector<float>& L,
                      const std::vector<float>& R, double sr)
{
    const int n = (int) L.size();
    std::vector<unsigned char> b;
    auto u32 = [&] (unsigned v) { for (int i = 0; i < 4; ++i) b.push_back ((unsigned char) ((v >> (8 * i)) & 0xff)); };
    auto u16 = [&] (unsigned v) { for (int i = 0; i < 2; ++i) b.push_back ((unsigned char) ((v >> (8 * i)) & 0xff)); };
    auto tag = [&] (const char* s) { for (int i = 0; i < 4; ++i) b.push_back ((unsigned char) s[i]); };
    tag ("RIFF"); u32 (36 + n * 4); tag ("WAVE");
    tag ("fmt "); u32 (16); u16 (1); u16 (2); u32 ((unsigned) sr);
    u32 ((unsigned) sr * 4); u16 (4); u16 (16);
    tag ("data"); u32 (n * 4);
    for (int i = 0; i < n; ++i) {
        auto s16 = [&] (float x) {
            int v = (int) std::lround (32767.0f * std::max (-1.f, std::min (1.f, x)));
            u16 ((unsigned) (v & 0xffff));
        };
        s16 (L[(size_t) i]); s16 (R[(size_t) i]);
    }
    FILE* f = std::fopen (path.c_str(), "wb");
    if (!f) { std::printf ("  could not write %s\n", path.c_str()); return; }
    std::fwrite (b.data(), 1, b.size(), f);
    std::fclose (f);
}

/*  Four notes with silence between them, so the note-on is heard on its own
    and heard several times.  A click that is only on the first note is a
    different fault from one on every note, and that difference matters.    */
static void renderNotes (Engine& e, double sr, std::vector<float>& L, std::vector<float>& R)
{
    const double noteLen = 0.9, gap = 0.45;
    const int per = (int) (sr * (noteLen + gap));
    const int n = per * 4;
    L.assign ((size_t) n, 0.f); R.assign ((size_t) n, 0.f);
    const int block = 64;
    const int notes[4] = { 28, 28, 33, 28 };
    for (int i = 0; i < n; i += block) {
        const int m = std::min (block, n - i);
        for (int k = 0; k < 4; ++k) {
            const int on  = k * per;
            const int off = k * per + (int) (sr * noteLen);
            if (i <= on  && i + m > on)  e.noteOn  (notes[k], 0.9f);
            if (i <= off && i + m > off) e.noteOff (notes[k]);
        }
        e.process (&L[(size_t) i], &R[(size_t) i], m);
    }
}

int main (int argc, char** argv)
{
    const double sr = 48000.0;
    const std::string dir = argc > 1 ? argv[1] : ".";
    const int which = argc > 2 ? std::atoi (argv[2]) : 0;

    struct V { const char* file; const char* what; void (*set) (Params&); };
    static const V vs[] = {
        { "A-as-it-is",        "the preset exactly as it ships",
          [] (Params&) {} },
        { "B-slow-attack",     "AMP ATTACK 30 ms instead of the preset's own",
          [] (Params& p) { p.v[P_e1_a] = 0.36f; } },
        { "C-no-well",         "MASS TRACK, MASS VEL and the MASS macro all 0",
          [] (Params& p) { p.v[P_mass_track] = 0.f; p.v[P_mass_vel] = 0.f; p.v[P_macro_mass] = 0.f; } },
        { "D-no-singularity",  "SING LEVEL 0 and DEPTH 0 - kills the core entirely",
          [] (Params& p) { p.v[P_sing_level] = 0.f; p.v[P_macro_depth] = 0.f; } },
        { "E-no-ringdown",     "RINGDOWN 0 - no impulse strike at note-on",
          [] (Params& p) { p.v[P_ring_amt] = 0.f; } },
        { "F-no-drive",        "DRIVE AMOUNT 0",
          [] (Params& p) { p.v[P_drv_amt] = 0.f; } },
        { "G-filters-open",    "both filters wide open, no resonance",
          [] (Params& p) { p.v[P_fa_cut] = 1.f; p.v[P_fa_res] = 0.f; p.v[P_fa_env] = 0.f;
                           p.v[P_fb_cut] = 0.f; p.v[P_fb_res] = 0.f; p.v[P_fb_env] = 0.f; } },
        { "H-osc-A-only",      "sub, B and noise off - one oscillator and nothing else",
          [] (Params& p) { p.v[P_sub_level] = 0.f; p.v[P_b_level] = 0.f; p.v[P_noise_level] = 0.f; } },
    };

    std::printf ("Click A/B for preset %d (%s), four notes per file:\n\n", which, factoryName (which));
    for (const V& v : vs) {
        Engine e; e.prepare (sr, 64);
        applyFactory (which, e.p);
        e.p.v[P_out_trim] = 0.f;
        v.set (e.p);
        std::vector<float> L, R;
        renderNotes (e, sr, L, R);
        float pk = 0.f; for (float x : L) pk = std::max (pk, std::fabs (x));
        const std::string path = dir + "/GW-CLICK-" + v.file + ".wav";
        writeWav (path, L, R, sr);
        std::printf ("  %-18s peak %.3f   %s\n", v.file, pk, v.what);
    }
    std::printf ("\nListen to A first, then the others.  Whichever ones are CLEAN\n"
                 "name the cause.  If none of them is clean, the click is in the\n"
                 "part none of these switches off, and that is worth knowing too.\n");
    return 0;
}
