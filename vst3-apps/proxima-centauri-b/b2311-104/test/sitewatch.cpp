/*  THE SITE, watched from outside.

    Opens the shared block read-only and prints who is on the bench and what
    each of them is publishing — kind, its own kelvin, activity, phase, and how
    long ago it last spoke. Run it while a DAW is open: if a finding is loaded
    and wired, it appears here within a second; if it does not appear, it is
    not on the bench, and no amount of staring at panels will say why.

    This is the answer to "do they really share data" — the block is the only
    shared state there is, so reading it is the whole truth.

      sitewatch            one snapshot
      sitewatch 30         watch for 30 seconds, printing every change
*/
#include "../../BrokildWorldFX/adapter/proxima_site.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace proxima;

static const char* kindName (uint32_t k)
{
    switch (k) { case 1: return "B2311.1"; case 22: return "B2311.22";
                 case 67: return "B2311.67"; case 104: return "B2311.104";
                 default: return "unknown"; }
}

int main (int argc, char** argv)
{
    const double seconds = argc > 1 ? std::atof (argv[1]) : 0.0;

    HANDLE map = OpenFileMappingW (FILE_MAP_READ, FALSE, L"Local\\BrokildProximaSite");
    if (map == nullptr)
    {
        std::printf ("no bench: nothing has opened the site in this login session.\n"
                     "(no finding with site code is running, or none has reached its constructor)\n");
        return 1;
    }
    const Block* blk = (const Block*) MapViewOfFile (map, FILE_MAP_READ, 0, 0, sizeof (Block));
    if (blk == nullptr) { std::printf ("could not map the block\n"); CloseHandle (map); return 1; }

    if (blk->magic.load() != kMagic)
    { std::printf ("the block is present but not initialised (magic %08x)\n", blk->magic.load()); return 1; }

    std::string last;
    const uint64_t until = GetTickCount64() + (uint64_t) (seconds * 1000.0);
    do
    {
        const uint64_t now = GetTickCount64();
        char buf[4096]; int n = 0;
        n += std::snprintf (buf + n, sizeof buf - n,
            "version %u   climate %s   timing %s   distance %.2f   pulse %.2f Hz   bench %.1f K (owner %llx, seq %u)\n",
            blk->version.load(),
            blk->climateShared.load() ? "SHARED" : "own",
            blk->timingShared.load() ? "SHARED" : "own",
            (double) blk->distance.load(), (double) blk->pulseHz.load(),
            (double) blk->siteKelvin.load(),
            (unsigned long long) blk->climateOwner.load(), blk->climateSeq.load());

        int live = 0;
        for (int i = 0; i < kSlots; ++i)
        {
            const Slot& s = blk->slots[i];
            const uint64_t id = s.id.load();
            if (id == 0) continue;
            const uint64_t age = now - s.beatMs.load();
            const bool stale = age > kStaleMs;
            if (! stale) ++live;
            n += std::snprintf (buf + n, sizeof buf - n,
                "  slot %2d  %-11s  %7.1f K   activity %.2f   phase %.2f   %s\n",
                i, kindName (s.kind.load()), (double) s.localKelvin.load(),
                (double) s.activity.load(), (double) s.phase.load(),
                stale ? "GONE (stale)" : "live");
        }
        n += std::snprintf (buf + n, sizeof buf - n, "  %d finding%s on the bench\n",
                            live, live == 1 ? "" : "s");

        if (buf != last) { std::printf ("%s\n", buf); std::fflush (stdout); last = buf; }
        if (seconds <= 0.0) break;
        Sleep (250);
    } while (GetTickCount64() < until);

    UnmapViewOfFile (blk);
    CloseHandle (map);
    return 0;
}
