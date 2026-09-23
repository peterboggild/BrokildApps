/*  THE SITE — the shared-memory layer, tested in one process.

    Two clients open the same named mapping (exactly what two plugin
    instances do), and this proves: both get slots and see each other; the
    climate propagates from whoever moved it and only flags the OTHER; the
    Kuramoto step pulls phases together when cold and close and leaves them
    alone when hot or far; coupling OFF gives a pull of exactly zero; and a
    closed client's slot goes free. Run it after any change to proxima_site.h. */
#include "BrokildWorldFX/adapter/proxima_site.h"
#include <cstdio>
#include <cmath>

static int checks = 0, fails = 0;
static void ok (bool c, const char* what, const char* detail = "")
{ ++checks; if (! c) { ++fails; std::printf ("  FAIL  %s   %s\n", what, detail); } }

int main()
{
    std::printf ("THE SITE — shared-memory bench\n");
    using namespace proxima;

    //  start from known settings: coupling on, close, for the sync tests
    Settings on; on.climate = true; on.timing = true; on.distance = 0.1f; on.pulseHz = 0.5f;
    const Settings before = loadSettings();     // restore afterwards

    Client a, b;
    ok (a.open (104), "client A opens the site");
    a.setSettings (on);
    ok (b.open (1),   "client B opens the same site");
    ok (a.isOpen() && b.isOpen(), "both mapped");

    //  see each other
    auto va = a.sync (0.2f, 234.0f, 0.10f, 0.4f);
    auto vb = b.sync (0.5f, 293.0f, 0.60f, 0.4f);
    char d[96];
    std::snprintf (d, sizeof d, "A sees %d others, B sees %d", va.others, vb.others);
    ok (va.others == 1 && vb.others == 1, "each sees exactly the other", d);
    ok (vb.climateShared && vb.timingShared, "B reads A's settings from the block");

    //  climate: A speaks, B hears, A is not told it moved
    a.proposeKelvin (512.0f);
    vb = b.sync (0.5f, 293.0f, 0.60f, 0.4f);
    va = a.sync (0.2f, 512.0f, 0.10f, 0.4f);
    std::snprintf (d, sizeof d, "B siteKelvin %.1f moved=%d, A moved=%d", vb.siteKelvin, vb.climateMoved, va.climateMoved);
    ok (std::abs (vb.siteKelvin - 512.0f) < 0.01f && vb.climateMoved, "the climate reaches B", d);
    ok (! va.climateMoved, "the one who moved it is not told it moved", d);

    //  timing: cold + close pulls the phases together
    {
        float pa = 0.10f, pb = 0.60f;
        const float warmthCold = 0.05f;
        for (int t = 0; t < 400; ++t)
        {
            va = a.sync (0.2f, 234.0f, pa, warmthCold);
            vb = b.sync (0.5f, 234.0f, pb, warmthCold);
            const float K = Client::pullStrength (va, warmthCold);
            pa = Client::stepPhase (pa, 0.5f, 1.0 / 30.0, va, K * 4.0f);
            pb = Client::stepPhase (pb, 0.5f, 1.0 / 30.0, vb, Client::pullStrength (vb, warmthCold) * 4.0f);
        }
        float diff = std::abs (pa - pb); if (diff > 0.5f) diff = 1.0f - diff;
        std::snprintf (d, sizeof d, "phase gap after 13 s cold+close: %.3f (coherence %.2f)", diff, va.coherence);
        ok (diff < 0.05f, "cold and close, the two fall into step", d);
        ok (va.coherence > 0.95f, "and the site reads as coherent", d);
    }
    //  hot: the pull is nothing, phases drift with their own rates
    {
        float pa = 0.10f, pb = 0.60f;
        const float warmthHot = 1.0f;
        for (int t = 0; t < 400; ++t)
        {
            va = a.sync (0.2f, 800.0f, pa, warmthHot);
            vb = b.sync (0.5f, 800.0f, pb, warmthHot);
            pa = Client::stepPhase (pa, 0.5f, 1.0 / 30.0, va, Client::pullStrength (va, warmthHot) * 4.0f);
            pb = Client::stepPhase (pb, 0.5f, 1.0 / 30.0, vb, Client::pullStrength (vb, warmthHot) * 4.0f);
        }
        float diff = std::abs (pa - pb); if (diff > 0.5f) diff = 1.0f - diff;
        std::snprintf (d, sizeof d, "phase gap after 13 s hot: %.3f", diff);
        ok (Client::pullStrength (va, warmthHot) == 0.0f, "hot, the pull is exactly zero", d);
        ok (diff > 0.3f, "and the phases keep their own time", d);
    }
    //  coupling OFF: the pull is exactly zero whatever the temperature
    {
        Settings off = on; off.timing = false;
        a.setSettings (off);
        va = a.sync (0.2f, 100.0f, 0.1f, 0.0f);
        vb = b.sync (0.5f, 100.0f, 0.6f, 0.0f);
        ok (Client::pullStrength (va, 0.0f) == 0.0f && Client::pullStrength (vb, 0.0f) == 0.0f,
            "coupling off: the pull is exactly zero even at 77 K");
        ok (! vb.timingShared, "and B saw the setting change");
    }
    //  a closed client frees its slot
    b.close();
    va = a.sync (0.2f, 100.0f, 0.1f, 0.0f);
    ok (va.others == 0, "a closed instance is gone from the site");

    a.setSettings (before);       // leave the machine as it was
    std::printf ("\n%d checks, %d failed  %s\n", checks, fails, fails ? "" : "ALL CLEAR");
    return fails ? 1 : 0;
}
