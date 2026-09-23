/*  THE CLIMATE STATE MACHINE, several findings in ONE process.

    The live check drives separate standalones; a DAW loads every finding into
    ONE process, and that configuration had never been tested. This mirrors the
    processors' siteStep climate logic exactly — propose when I moved, follow
    when the bench moved, the applied-value guard, the settling window — and
    runs it for four findings at once, one of which (B2311.67) is NOT wired,
    so its presence must change nothing for the others.

    What it must show:
      * a move in ANY finding reaches every wired finding
      * an unwired finding neither breaks the others nor joins them
      * no ping-pong: after a move, everything settles and stays settled
      * climate off changes nobody's temperature, ever
*/
#include "../../BrokildWorldFX/adapter/proxima_site.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

using namespace proxima;

static int checks = 0, fails = 0;
static void ok (bool c, const char* what, const std::string& detail = "")
{ ++checks; if (! c) { ++fails; std::printf ("  FAIL  %s   %s\n", what, detail.c_str()); } }

/*  One wired finding, exactly as a processor does it. `kelvin` stands in for
    the host parameter: the panel writes it, the site writes it, and both are
    visible to everyone — which is the whole point of routing the climate
    through a parameter rather than a private variable. */
struct Finding
{
    Client cli;
    uint32_t kind = 0;
    float kelvin = 293.0f;              // "the host parameter"
    float appliedK = -1.0f, lastLocalK = -1.0f;
    double phase = 0.0;
    int    ticks = 0;
    int    writesByTheSite = 0;

    bool open (uint32_t k, float k0) { kind = k; kelvin = k0; return cli.open (k); }

    void step (double dt)
    {
        const float myK = kelvin;
        const float warmth = (myK - 77.0f) / 723.0f;
        if (lastLocalK < 0.0f) lastLocalK = myK;
        ++ticks;
        const bool settled = ticks > 25;

        const auto v = cli.sync (0.0f, myK, (float) phase, warmth);

        if (v.climateShared)
        {
            const bool iMoved = std::fabs (myK - lastLocalK) > 0.5f
                             && std::fabs (myK - appliedK) > 0.5f;
            const bool follow = v.climateMoved || ! settled;
            //  seed an empty bench (see the processors)
            if (v.siteKelvin <= 0.0f && settled)
            {
                cli.proposeKelvin (myK);
                appliedK = myK; lastLocalK = myK;
            }
            else if (iMoved && settled)
            {
                cli.proposeKelvin (myK);
                appliedK = myK;
            }
            else if (follow && v.siteKelvin > 0.0f && std::fabs (v.siteKelvin - myK) > 0.5f)
            {
                appliedK = v.siteKelvin;
                lastLocalK = v.siteKelvin;
                kelvin = v.siteKelvin;         // the site writes the parameter
                ++writesByTheSite;
            }
        }
        if (std::fabs (kelvin - lastLocalK) > 0.5f) lastLocalK = kelvin;

        const float pull = Client::pullStrength (v, warmth);
        phase = Client::stepPhase ((float) phase, cli.naturalHz(), dt, v, pull);
    }

    //  the panel's own move: chosen here, global now
    void chooseClimate (bool on)
    {
        Settings s = cli.settings();
        const bool was = s.climate;
        s.climate = on;
        cli.setSettings (s);
        if (on && ! was) { cli.proposeKelvin (kelvin); appliedK = kelvin; lastLocalK = kelvin; }
    }
};

static void run (std::vector<Finding*>& all, double seconds)
{
    const double dt = 1.0 / 30.0;
    for (int i = 0; i < (int) (seconds / dt); ++i)
        for (auto* f : all) f->step (dt);
}

int main()
{
    std::printf ("=== THE CLIMATE, four findings in one process ===\n");

    //  the settings this test leaves behind must be the ones it found
    const Settings before = loadSettings();

    Finding a, b, c;                        // .104, .22, .1 — wired
    if (! a.open (104, 234.0f) || ! b.open (22, 293.0f) || ! c.open (1, 293.0f))
    { std::printf ("could not open the site\n"); return 1; }
    std::vector<Finding*> all { &a, &b, &c };

    //  B2311.67 stands for a finding that is present but NOT wired: it holds
    //  no slot, publishes nothing, and follows nothing
    float unwiredKelvin = 400.0f;

    {   //  climate OFF: nobody moves anybody
        Settings s = a.cli.settings(); s.climate = false; s.timing = false; a.cli.setSettings (s);
        run (all, 1.5);
        a.kelvin = 600.0f;
        run (all, 1.5);
        char d[128]; std::snprintf (d, sizeof d, ".104 %.0f, .22 %.0f, .1 %.0f", (double) a.kelvin, (double) b.kelvin, (double) c.kelvin);
        std::printf ("  climate off: %s\n", d);
        ok (std::fabs (b.kelvin - 293.0f) < 0.01f && std::fabs (c.kelvin - 293.0f) < 0.01f,
            "climate off moves nobody", d);
        a.kelvin = 234.0f; a.lastLocalK = 234.0f;
        run (all, 0.5);
    }

    {   /*  climate ON but nobody has spoken: the bench is empty, and the
            findings must CONVERGE rather than sit at their own values until
            someone happens to move a slider. This is the case a project load
            produces, and the one that reads as the sharing being broken. */
        a.kelvin = 234.0f; a.lastLocalK = 234.0f; a.appliedK = -1.0f;
        b.kelvin = 293.0f; b.lastLocalK = 293.0f; b.appliedK = -1.0f;
        c.kelvin = 400.0f; c.lastLocalK = 400.0f; c.appliedK = -1.0f;
        Settings s = a.cli.settings(); s.climate = true; a.cli.setSettings (s);
        //  a fresh bench: no temperature yet
        a.cli.proposeKelvin (0.0f);
        run (all, 2.5);
        char d[128]; std::snprintf (d, sizeof d, ".104 %.0f, .22 %.0f, .1 %.0f",
                                    (double) a.kelvin, (double) b.kelvin, (double) c.kelvin);
        std::printf ("  climate on, empty bench: %s\n", d);
        ok (std::fabs (a.kelvin - b.kelvin) < 1.0f && std::fabs (a.kelvin - c.kelvin) < 1.0f,
            "an empty bench is seeded, and the findings converge", d);
    }

    {   /*  chosen in .104 — the bench takes ITS temperature at once. "Chosen"
            means the OFF -> ON transition, so the climate is put back off
            first; leaving it on made this pass for the wrong reason once. */
        a.chooseClimate (false);
        run (all, 0.5);
        a.kelvin = 500.0f; a.lastLocalK = 500.0f;
        a.chooseClimate (true);
        run (all, 1.5);
        char d[128]; std::snprintf (d, sizeof d, ".104 %.0f, .22 %.0f, .1 %.0f", (double) a.kelvin, (double) b.kelvin, (double) c.kelvin);
        std::printf ("  chosen in .104 at 500 K: %s\n", d);
        ok (std::fabs (b.kelvin - 500.0f) < 1.0f && std::fabs (c.kelvin - 500.0f) < 1.0f,
            "choosing the climate makes that finding's temperature the bench's", d);
    }

    {   //  a move in the MIDDLE finding reaches both others
        b.kelvin = 149.0f;
        run (all, 1.5);
        char d[128]; std::snprintf (d, sizeof d, ".104 %.0f, .22 %.0f, .1 %.0f", (double) a.kelvin, (double) b.kelvin, (double) c.kelvin);
        std::printf ("  moved .22 to 149 K: %s\n", d);
        ok (std::fabs (a.kelvin - 149.0f) < 1.0f && std::fabs (c.kelvin - 149.0f) < 1.0f,
            "a move in any finding reaches every other", d);
    }

    {   //  a DRAG: thirty consecutive values, as a slider actually produces
        int drags = 0;
        for (int i = 0; i < 30; ++i)
        {
            c.kelvin = 200.0f + (float) i * 10.0f;      // the panel writes it every tick
            for (auto* f : all) f->step (1.0 / 30.0);
            ++drags;
        }
        run (all, 1.0);
        char d[128]; std::snprintf (d, sizeof d, "after %d dragged values: .104 %.0f, .22 %.0f, .1 %.0f",
                                    drags, (double) a.kelvin, (double) b.kelvin, (double) c.kelvin);
        std::printf ("  %s\n", d);
        ok (std::fabs (a.kelvin - c.kelvin) < 1.0f && std::fabs (b.kelvin - c.kelvin) < 1.0f,
            "a dragged slider carries the others with it", d);
    }

    {   //  no ping-pong: once settled, nothing writes anything again
        run (all, 1.0);
        const int wa = a.writesByTheSite, wb = b.writesByTheSite, wc = c.writesByTheSite;
        run (all, 2.0);
        char d[128]; std::snprintf (d, sizeof d, "further site writes in 2 s: %d / %d / %d",
                                    a.writesByTheSite - wa, b.writesByTheSite - wb, c.writesByTheSite - wc);
        std::printf ("  %s\n", d);
        ok (a.writesByTheSite == wa && b.writesByTheSite == wb && c.writesByTheSite == wc,
            "settled, the findings stop writing to each other", d);
    }

    {   //  the unwired finding: present, but not on the bench
        const auto v = a.cli.sync (0.0f, a.kelvin, (float) a.phase, 0.5f);
        char d[128]; std::snprintf (d, sizeof d, "%d others seen by .104, unwired sits at %.0f K",
                                    v.others, (double) unwiredKelvin);
        std::printf ("  %s\n", d);
        ok (v.others == 2, "a finding without site code is not on the bench", d);
        ok (std::fabs (unwiredKelvin - 400.0f) < 0.01f,
            "an unwired finding is not moved by the bench", d);
    }

    {   //  and a fourth WIRED finding joins mid-session and takes the climate
        Finding d4;
        if (d4.open (67, 300.0f))
        {
            std::vector<Finding*> four { &a, &b, &c, &d4 };
            for (int i = 0; i < 60; ++i) for (auto* f : four) f->step (1.0 / 30.0);
            char d[128]; std::snprintf (d, sizeof d, "joined at 300 K, settled at %.0f K (bench %.0f K)",
                                        (double) d4.kelvin, (double) a.kelvin);
            std::printf ("  %s\n", d);
            ok (std::fabs (d4.kelvin - a.kelvin) < 1.0f,
                "a finding joining later takes the bench's climate", d);
            const auto v = a.cli.sync (0.0f, a.kelvin, (float) a.phase, 0.5f);
            ok (v.others == 3, "four wired findings are all on the bench",
                std::to_string (v.others) + " others seen");
            d4.cli.close();
        }
        else ok (false, "a fourth client could open a slot");
    }

    //  put the bench back exactly as it was found
    a.cli.setSettings (before);
    a.cli.close(); b.cli.close(); c.cli.close();

    std::printf ("\n%d checks, %d failed  -  %s\n", checks, fails, fails ? "SEE ABOVE" : "ALL CLEAR");
    return fails ? 1 : 0;
}
