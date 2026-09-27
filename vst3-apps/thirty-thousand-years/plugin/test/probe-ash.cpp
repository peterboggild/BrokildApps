/*  Two reports on THE SUN BEHIND THE ASH, both measured rather than reasoned:
 *
 *    1. "it is muted and the filter cutoff does not open it up"
 *    2. "I can turn SIGNAL, MASS, MEMORY and STRUCTURE all off and it still
 *        plays -- turning them off does not change anything"
 *
 *  The second is the decisive one: if the strata are off and there is still
 *  output, the sound is not coming from the strata, and no filter on a stratum
 *  could ever have brightened it.
 *
 *      cmake --build <dir> --config Release --target ttyash
 */
#include "Engine.h"
#include "Presets.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

using namespace tty;

struct Take { std::vector<float> L, R; double sr; int n() const { return (int) L.size(); } };

static Take render (Engine& e, double seconds, int block = 256)
{
    Take t; t.sr = e.sr; const int n = (int) (seconds * e.sr);
    t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; i += block)
    {
        const int m = std::min (block, n - i);
        e.process (&t.L[(size_t) i], &t.R[(size_t) i], m, nullptr, nullptr);
    }
    return t;
}
static double rmsOf (const Take& t, int from = 0, int to = -1)
{
    if (to < 0) to = t.n(); double s = 0; int c = 0;
    for (int i = from; i < to; ++i) { s += (double) t.L[(size_t) i] * t.L[(size_t) i]; ++c; }
    return c ? std::sqrt (s / c) : 0.0;
}
static double db (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }

/*  Energy above a corner, by a 4-pole high-pass. Crude but monotone, which is
 *  all that "did the top end move" needs. */
static double hiRms (const Take& t, double fc)
{
    const double a = std::exp (-2.0 * 3.14159265358979 * fc / t.sr);
    double s1 = 0, s2 = 0, acc = 0; int c = 0;
    for (int i = 0; i < t.n(); ++i)
    {
        const double x = t.L[(size_t) i];
        s1 = x * (1 - a) + s1 * a;            // lowpass
        const double h1 = x - s1;             // highpass
        s2 = h1 * (1 - a) + s2 * a;
        const double h2 = h1 - s2;
        acc += h2 * h2; ++c;
    }
    return c ? std::sqrt (acc / c) : 0.0;
}

static int findPreset (const char* name)
{
    for (int i = 0; i < numPresets(); ++i) if (std::strcmp (preset (i).name, name) == 0) return i;
    return -1;
}
static Engine* load (int idx, double sr = 48000.0)
{
    Engine* e = new Engine();
    applyPreset (idx, e->p, e->macro, e->scene, e->sceneSet, e->life.slots, e->life.mseg);
    e->prepare (sr, 256);
    return e;
}
/*  a held chord, the way the preset is meant to be played */
static void playChord (Engine& e)
{
    const int root = (int) e.p[P_drone_root];
    e.noteOn (root, 0.8f, 0); e.noteOn (root + 7, 0.75f, 0); e.noteOn (root + 12, 0.7f, 0);
}

int main()
{
    const int idx = findPreset ("THE SUN BEHIND THE ASH");
    if (idx < 0) { std::printf ("preset not found\n"); return 2; }
    std::printf ("\nTHE SUN BEHIND THE ASH  (preset %d)\n", idx);

    /* ---- 1. what the preset sounds like as shipped -------------------- */
    {
        Engine* e = load (idx);
        playChord (*e);
        Take t = render (*e, 6.0);
        std::printf ("\n[1] as shipped, chord held 6 s\n");
        std::printf ("      rms %.5f (%.1f dB)   above 2 kHz %.1f dB   above 6 kHz %.1f dB\n",
                     rmsOf (t), db (rmsOf (t)), db (hiRms (t, 2000)), db (hiRms (t, 6000)));
        std::printf ("      m_cut %.3f -> %.0f Hz      s_cut %.3f -> %.0f Hz\n",
                     e->p[P_m_cut], lawHz (paramSpec (P_m_cut), e->p[P_m_cut]),
                     e->p[P_s_cut], lawHz (paramSpec (P_s_cut), e->p[P_s_cut]));
        delete e;
    }

    /* ---- 2. THE DECISIVE ONE: every stratum off ----------------------- */
    {
        std::printf ("\n[2] with SIGNAL, MASS, MEMORY and STRUCTURE all OFF\n");
        Engine* e = load (idx);
        playChord (*e);
        Take warm = render (*e, 4.0);                 // let it get going
        e->p[P_m_on] = 0; e->p[P_s_on] = 0; e->p[P_mem_on] = 0; e->p[P_st_on] = 0;
        for (int s = 0; s < 6; ++s)
        {
            Take t = render (*e, 2.0);
            std::printf ("      %2d-%2d s after switching off:  rms %.6f (%.1f dB)\n",
                         s * 2, s * 2 + 2, rmsOf (t), db (rmsOf (t)));
        }
        std::printf ("      (for comparison, with them ON it was %.5f)\n", rmsOf (warm));
        delete e;
    }

    /* ---- 3. and with the environment taken out as well ---------------- */
    {
        std::printf ("\n[3] all strata OFF and the ENVIRONMENT neutral (reverb + loop out)\n");
        Engine* e = load (idx);
        playChord (*e);
        render (*e, 4.0);
        e->p[P_m_on] = 0; e->p[P_s_on] = 0; e->p[P_mem_on] = 0; e->p[P_st_on] = 0;
        e->p[P_e_rv_mix] = 0; e->p[P_e_fb_send] = 0; e->p[P_e_fb_ret] = 0;
        for (int s = 0; s < 3; ++s)
        {
            Take t = render (*e, 2.0);
            std::printf ("      %2d-%2d s:  rms %.6f (%.1f dB)\n", s * 2, s * 2 + 2, rmsOf (t), db (rmsOf (t)));
        }
        delete e;
    }

    /* ---- 4. does opening a cutoff move the top end? ------------------- */
    {
        std::printf ("\n[4] opening the cutoffs (chord held, 6 s each)\n");
        struct Case { const char* what; float mcut, scut; };
        const Case cases[] = {
            { "as shipped",                    -1.0f, -1.0f },
            { "MASS cutoff wide open",          1.0f, -1.0f },
            { "SIGNAL cutoff wide open",       -1.0f,  1.0f },
            { "both wide open",                 1.0f,  1.0f },
        };
        for (const Case& c : cases)
        {
            Engine* e = load (idx);
            if (c.mcut >= 0) e->p[P_m_cut] = c.mcut;
            if (c.scut >= 0) e->p[P_s_cut] = c.scut;
            playChord (*e);
            Take t = render (*e, 6.0);
            std::printf ("      %-26s rms %.5f   >2k %.1f dB   >6k %.1f dB\n",
                         c.what, rmsOf (t), db (hiRms (t, 2000)), db (hiRms (t, 6000)));
            delete e;
        }
    }

    /* ---- 5. what else is darkening it -------------------------------- */
    {
        std::printf ("\n[5] the other things holding the top end down\n");
        struct Case { const char* what; int id; float v; };
        const Case cases[] = {
            { "as shipped",                -1,              0.0f },
            { "reverb HIGH DAMPING to 0",   P_e_rv_damp,    0.0f },
            { "DISTANCE to 0",              P_e_distance,   0.0f },
            { "reverb MIX to 0",            P_e_rv_mix,     0.0f },
            { "HUMANITY macro to full",     P_mac_mass + 7, 1.0f },
        };
        for (const Case& c : cases)
        {
            Engine* e = load (idx);
            if (c.id >= 0) e->p[c.id] = c.v;
            playChord (*e);
            Take t = render (*e, 6.0);
            std::printf ("      %-26s rms %.5f   >2k %.1f dB   >6k %.1f dB\n",
                         c.what, rmsOf (t), db (hiRms (t, 2000)), db (hiRms (t, 6000)));
            delete e;
        }
    }

    /* ---- 6. against a preset that DOES respond ----------------------- */
    {
        std::printf ("\n[6] how fast the sound goes once the strata are off, two presets\n");
        const char* names[] = { "THE SUN BEHIND THE ASH", "A WEAPON WAITING FOR A WAR" };
        for (const char* nm : names)
        {
            const int i = findPreset (nm);
            if (i < 0) { std::printf ("      %-28s NOT FOUND\n", nm); continue; }
            Engine* e = load (i);
            playChord (*e);
            Take warm = render (*e, 4.0);
            const double on = rmsOf (warm);
            e->p[P_m_on] = 0; e->p[P_s_on] = 0; e->p[P_mem_on] = 0; e->p[P_st_on] = 0;
            /*  how long until it is 40 dB below what it was -- i.e. gone */
            double t40 = -1; const double target = on * 0.01;
            for (int s = 0; s < 30 && t40 < 0; ++s)
            {
                Take t = render (*e, 0.5);
                if (rmsOf (t) < target) t40 = (s + 1) * 0.5;
            }
            std::printf ("      %-28s sounding at %.5f with the strata on\n", nm, on);
            {
                Engine* f = load (i); playChord (*f); render (*f, 4.0);
                f->p[P_m_on] = 0; f->p[P_s_on] = 0; f->p[P_mem_on] = 0; f->p[P_st_on] = 0;
                Take h = render (*f, 0.5);
                char when[32];
                if (t40 < 0) std::snprintf (when, sizeof when, "more than 15 s");
                else         std::snprintf (when, sizeof when, "%.1f s", t40);
                std::printf ("        drops to %.5f at once (%.1f dB down), and is 40 dB down after %s\n",
                             rmsOf (h), db (rmsOf (h) / on), when);
                delete f;
            }
            delete e;
        }
    }

    std::printf ("\n");
    return 0;
}
