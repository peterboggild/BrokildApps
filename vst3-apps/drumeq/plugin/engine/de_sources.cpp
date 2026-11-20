#include "de_sources.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace de
{
    namespace
    {
        //  ---- the drums ----------------------------------------------------
        const SourceDef kSources[kNumSources] =
        {
            { "KICK IN", "KICK IN",
              { { "THUMP", 35, 160 }, { "BOX", 150, 900 }, { "BEATER", 1500, 9000 }, { "CLICK", 4000, 16000 } },
              35, 120, 0,
              "hpf=30 hpfs=1 b1t=1 b1f=60 b1g=2 b1q=1.2 b2f=380 b2g=-6 b2q=1.6 b3f=4000 b3g=4 b3q=1.3 "
              "b4t=0 b4f=9000 b4g=1 b4q=0.7 comp=1 cthr=-18 crat=4 catk=6 crel=80 cknee=6 cmake=3 csc=30 cauto=1 "
              "gthr=-30 grange=20 gatk=0.3 ghold=60 grel=120 gkeylo=40 gkeyhi=5000" },

            { "KICK OUT", "KICK OUT",
              { { "SUB", 25, 120 }, { "MUD", 120, 700 }, { "PRESENCE", 800, 6000 }, { "TOP", 2500, 14000 } },
              30, 100, 0,
              "hpf=25 hpfs=1 lpf=9000 lpfs=1 b1t=1 b1f=55 b1g=3 b1q=1.0 b2f=250 b2g=-5 b2q=1.4 b3f=2500 b3g=-2 b3q=1.0 "
              "b4t=0 b4f=6000 b4g=-4 b4q=0.7 comp=1 cthr=-18 crat=3 catk=15 crel=120 cknee=6 cmake=2 csc=20 cauto=1 "
              "gthr=-30 grange=15 gatk=0.5 ghold=80 grel=180 gkeylo=30 gkeyhi=2000" },

            { "SNARE TOP", "SNR TOP",
              { { "BODY", 120, 350 }, { "BOX", 300, 1500 }, { "CRACK", 1800, 9000 }, { "AIR", 5000, 16000 } },
              140, 330, 0,
              "hpf=80 hpfs=1 b1t=1 b1f=200 b1g=2 b1q=1.2 b2f=500 b2g=-3 b2q=1.6 b3f=4500 b3g=3 b3q=1.2 "
              "b4t=0 b4f=10000 b4g=2 b4q=0.7 comp=1 cthr=-18 crat=4 catk=12 crel=100 cknee=6 cmake=3 csc=100 cauto=1 "
              "gthr=-30 grange=15 gatk=0.3 ghold=80 grel=150 gkeylo=150 gkeyhi=8000" },

            { "SNARE BOTTOM", "SNR BOT",
              { { "BODY", 120, 400 }, { "HONK", 300, 1500 }, { "WIRES", 2500, 11000 }, { "AIR", 6000, 16000 } },
              140, 330, 0,
              "pol=1 hpf=180 hpfs=2 b1t=1 b1f=220 b1g=-2 b1q=1.0 b2f=600 b2g=-4 b2q=1.4 b3f=6000 b3g=3 b3q=1.0 "
              "b4t=0 b4f=11000 b4g=1 b4q=0.7 comp=1 cthr=-20 crat=4 catk=5 crel=80 cknee=6 cmake=2 csc=150 cauto=1 "
              "gthr=-34 grange=20 gatk=0.3 ghold=70 grel=140 gkeylo=150 gkeyhi=1500" },

            { "TOM 1", "TOM 1",
              { { "BODY", 80, 320 }, { "BOX", 250, 1000 }, { "ATTACK", 1500, 8000 }, { "BLEED", 5000, 16000 } },
              85, 320, 0,
              "hpf=60 hpfs=1 b1t=1 b1f=180 b1g=3 b1q=1.2 b2f=450 b2g=-5 b2q=1.4 b3f=4000 b3g=4 b3q=1.2 "
              "b4t=0 b4f=9000 b4g=-3 b4q=0.7 comp=1 cthr=-20 crat=4 catk=12 crel=150 cknee=6 cmake=3 csc=60 cauto=1 "
              "gate=1 gthr=-30 grange=24 gatk=0.5 ghold=120 grel=350 gkeylo=80 gkeyhi=1500" },

            { "TOM 2", "TOM 2",
              { { "BODY", 60, 250 }, { "BOX", 200, 900 }, { "ATTACK", 1500, 7000 }, { "BLEED", 5000, 16000 } },
              65, 250, 0,
              "hpf=50 hpfs=1 b1t=1 b1f=130 b1g=3 b1q=1.2 b2f=380 b2g=-5 b2q=1.4 b3f=3500 b3g=4 b3q=1.2 "
              "b4t=0 b4f=9000 b4g=-3 b4q=0.7 comp=1 cthr=-20 crat=4 catk=12 crel=170 cknee=6 cmake=3 csc=50 cauto=1 "
              "gate=1 gthr=-30 grange=24 gatk=0.5 ghold=140 grel=420 gkeylo=60 gkeyhi=1500" },

            { "TOM 3", "TOM 3",
              { { "BODY", 40, 180 }, { "BOX", 150, 700 }, { "ATTACK", 1200, 6000 }, { "BLEED", 5000, 16000 } },
              40, 170, 0,
              "hpf=35 hpfs=1 b1t=1 b1f=85 b1g=3 b1q=1.2 b2f=300 b2g=-5 b2q=1.4 b3f=3000 b3g=4 b3q=1.2 "
              "b4t=0 b4f=9000 b4g=-3 b4q=0.7 comp=1 cthr=-20 crat=4 catk=15 crel=200 cknee=6 cmake=3 csc=40 cauto=1 "
              "gate=1 gthr=-30 grange=24 gatk=0.5 ghold=160 grel=500 gkeylo=45 gkeyhi=1200" },

            { "HI-HAT", "HI-HAT",
              { { "BLEED", 100, 800 }, { "CLANG", 400, 2500 }, { "HARSH", 2000, 9000 }, { "AIR", 7000, 16000 } },
              0, 0, 0,
              "hpf=250 hpfs=2 b1t=0 b1f=400 b1g=-3 b1q=0.7 b2f=900 b2g=-2 b2q=1.4 b3f=4500 b3g=-2 b3q=1.6 "
              "b4t=0 b4f=12000 b4g=2 b4q=0.7 comp=0 cthr=-18 crat=2 catk=5 crel=80 cknee=6 cmake=0 csc=200 cauto=1 "
              "gthr=-40 grange=12 gatk=0.2 ghold=40 grel=100 gkeylo=2000 gkeyhi=16000" },

            { "RIDE", "RIDE",
              { { "BLEED", 100, 800 }, { "WASH", 300, 2500 }, { "PING", 2000, 9000 }, { "AIR", 7000, 16000 } },
              0, 0, 0,
              "hpf=200 hpfs=2 b1t=0 b1f=300 b1g=-3 b1q=0.7 b2f=600 b2g=-2 b2q=1.2 b3f=3500 b3g=2 b3q=1.4 "
              "b4t=0 b4f=11000 b4g=1 b4q=0.7 comp=0 cthr=-18 crat=2 catk=5 crel=100 cknee=6 cmake=0 csc=200 cauto=1 "
              "gthr=-40 grange=12 gatk=0.2 ghold=60 grel=200 gkeylo=1500 gkeyhi=16000" },

            { "ROOM", "ROOM",
              { { "LOW", 40, 250 }, { "MUD", 200, 900 }, { "HARSH", 1500, 7000 }, { "AIR", 5000, 16000 } },
              0, 0, 0,
              "hpf=40 hpfs=1 b1t=0 b1f=100 b1g=1 b1q=0.7 b2f=400 b2g=-4 b2q=1.0 b3f=3000 b3g=-3 b3q=1.2 "
              "b4t=0 b4f=10000 b4g=1 b4q=0.7 comp=1 cthr=-24 crat=6 catk=10 crel=150 cknee=6 cmake=6 cmix=1 csc=80 cauto=1 "
              "gthr=-50 grange=10 gatk=1 ghold=100 grel=300 gkeylo=60 gkeyhi=8000" },

            { "OVERHEADS", "OH",
              { { "LOW", 60, 400 }, { "MUD", 200, 900 }, { "HARSH", 1500, 7000 }, { "AIR", 7000, 16000 } },
              0, 0, 0,
              "hpf=100 hpfs=1 b1t=0 b1f=200 b1g=-2 b1q=0.7 b2f=400 b2g=-3 b2q=1.0 b3f=3000 b3g=-2 b3q=1.4 "
              "b4t=0 b4f=12000 b4g=2 b4q=0.7 comp=1 cthr=-18 crat=2 catk=25 crel=200 cknee=8 cmake=1 csc=120 cauto=1 "
              "gthr=-50 grange=10 gatk=1 ghold=100 grel=300 gkeylo=200 gkeyhi=16000" },
        };

        //  ---- the presets, five per drum; index 0 is always STANDARD -------
        const Preset kKickIn[] = {
            { "STANDARD", "" },
            { "ROCK PUNCH", "b1f=70 b1g=3 b2f=350 b2g=-5 b3f=3200 b3g=4 b3q=1.2 b4g=0 crat=4 catk=10 crel=90" },
            { "METAL CLICK", "b1f=55 b1g=1 b2f=420 b2g=-9 b2q=1.4 b3f=4500 b3g=7 b3q=1.0 b4t=1 b4f=9500 b4g=3 b4q=1.2 "
                             "cthr=-20 crat=6 catk=3 crel=60 cmake=4 gate=1 gthr=-28 grange=30 grel=80" },
            { "EXTREME CLICK", "hpf=40 hpfs=2 b1f=60 b1g=0 b2f=450 b2g=-12 b2q=1.2 b3f=5000 b3g=10 b3q=0.9 b4t=1 b4f=10500 b4g=5 b4q=1.4 "
                               "cthr=-24 crat=8 catk=2 crel=50 cmake=6 gate=1 gthr=-24 grange=40 ghold=40 grel=60" },
            { "VINTAGE THUD", "hpf=25 b1f=80 b1g=4 b1q=0.9 b2f=300 b2g=-3 b2q=1.2 b3f=2500 b3g=1 b4t=0 b4f=6000 b4g=-4 b4q=0.7 "
                              "crat=3 catk=20 crel=150" },
        };
        const Preset kKickOut[] = {
            { "STANDARD", "" },
            { "ROCK BOOM", "b1f=60 b1g=4 b1q=0.9 b2f=300 b2g=-4 lpf=7000" },
            { "METAL SUB", "hpf=30 hpfs=2 b1f=50 b1g=5 b1q=1.4 b2f=220 b2g=-7 b2q=1.2 b3f=3000 b3g=-4 lpf=6000 lpfs=2 crat=4 catk=10 crel=90" },
            { "TIGHT", "hpf=40 hpfs=2 b1f=65 b1g=2 b1q=1.6 b2f=200 b2g=-8 b2q=1.0 lpf=5000 lpfs=2 crat=6 catk=5 crel=60 cmake=4" },
            { "VINTAGE", "hpf=20 b1t=0 b1f=80 b1g=3 b1q=0.7 b2f=350 b2g=-2 lpf=10000 crat=2.5 catk=30 crel=200" },
        };
        const Preset kSnareTop[] = {
            { "STANDARD", "" },
            { "ROCK FAT", "b1f=180 b1g=4 b1q=1.0 b2f=600 b2g=-2 b3f=3500 b3g=3 b4g=1 crat=4 catk=20 crel=140 cmix=0.8" },
            { "METAL CRACK", "hpf=100 hpfs=2 b1f=220 b1g=3 b2f=550 b2g=-5 b2q=1.8 b3f=5000 b3g=6 b3q=1.0 b4f=11000 b4g=3 "
                             "cthr=-22 crat=6 catk=8 crel=70 cmake=5" },
            { "TIGHT & DRY", "hpf=110 b1f=240 b1g=1 b2f=900 b2g=-4 b2q=3 b3f=5500 b3g=4 gate=1 gthr=-26 grange=20 grel=90 "
                             "crat=5 catk=6 crel=60" },
            { "VINTAGE", "hpf=70 b1f=190 b1g=3 b1q=0.8 b2f=1000 b2g=1 b2q=1 b3f=3000 b3g=2 b4f=8000 b4g=-3 crat=3 catk=25 crel=180" },
        };
        const Preset kSnareBot[] = {
            { "STANDARD", "" },
            { "ROCK SIZZLE", "b3f=5000 b3g=4 b4g=2 crat=3 catk=10" },
            { "METAL WIRES", "hpf=250 b2g=-6 b3f=7000 b3g=5 b3q=0.9 b4g=3 crat=6 catk=3 crel=60 gate=1 gthr=-30 grange=30 grel=80" },
            { "SUBTLE", "hpf=200 b3g=1 b4g=0 crat=2.5 catk=15 crel=120" },
            { "VINTAGE", "hpf=150 hpfs=1 b3f=4000 b3g=2 b4g=-3 crat=2.5 catk=20 crel=150" },
        };
        //  the three toms share their presets: each keeps its own BODY and BOX
        //  frequencies (they follow the drum's size), the presets move the rest
        const Preset kTom[] = {
            { "STANDARD", "" },
            { "ROCK BOOM", "b1g=5 b1q=0.9 b2g=-4 b3g=3 b4g=-2 crat=4 catk=20 crel=200 grel=450" },
            { "METAL ATTACK", "b1g=3 b1q=1.4 b2g=-8 b2q=1.2 b3g=7 b3q=1.0 b4g=-4 cthr=-24 crat=6 catk=6 crel=110 cmake=5 "
                              "grange=40 grel=250" },
            { "NATURAL", "gate=0 b1g=2 b2g=-3 b3g=2 b4g=0 crat=2.5 catk=25 crel=200 cmake=1" },
            { "VINTAGE", "b1g=4 b1q=0.8 b2g=-2 b3g=1 b4g=-5 crat=3 catk=30 crel=250 grange=15" },
        };
        const Preset kHat[] = {
            { "STANDARD", "" },
            { "ROCK", "hpf=200 b2g=-1 b3g=-1 b4g=2" },
            { "METAL TIGHT", "hpf=350 b1g=-4 b2f=1000 b2g=-4 b3f=3500 b3g=-3 b4f=11000 b4g=3 comp=1 cthr=-16 crat=3 catk=2 crel=60" },
            { "DARK", "hpf=220 b3g=-4 b4t=0 b4f=9000 b4g=-3 lpf=14000 lpfs=1" },
            { "BRIGHT", "hpf=300 b2g=-3 b3g=0 b4f=10000 b4g=4" },
        };
        const Preset kRide[] = {
            { "STANDARD", "" },
            { "ROCK", "b2g=-1 b3g=1" },
            { "METAL PING", "hpf=300 b2f=700 b2g=-5 b3f=3200 b3g=5 b3q=1.6 b4g=2 comp=1 cthr=-18 crat=3 catk=1 crel=80" },
            { "DARK WASH", "b3g=-1 b4t=0 b4f=8000 b4g=-4 lpf=14000 lpfs=1" },
            { "BRIGHT", "b3g=3 b4f=10000 b4g=4" },
        };
        const Preset kRoom[] = {
            { "STANDARD", "" },
            { "ROCK SMASH", "cthr=-30 crat=12 catk=3 crel=120 cmake=12 cmix=0.6 b2g=-5 b3g=-2 b4g=2" },
            { "METAL TIGHT", "hpf=80 hpfs=2 b1g=-2 b2f=350 b2g=-6 b3f=2500 b3g=-5 lpf=9000 lpfs=1 crat=8 catk=5 crel=80 cmake=8" },
            { "NATURAL AIR", "cthr=-18 crat=2.5 catk=25 crel=250 cmake=2 b2g=-2 b3g=-1 b4g=3" },
            { "CRUSHED", "cthr=-36 crat=20 catk=0.5 crel=60 cknee=0 cmake=16 cmix=0.5 b1g=3 b3g=-4" },
        };
        const Preset kOh[] = {
            { "STANDARD", "" },
            { "ROCK FULL KIT", "hpf=50 hpfs=1 b1g=0 b2g=-2 b3g=-1 b4g=2 cthr=-16 crat=2" },
            { "METAL CYMBALS ONLY", "hpf=350 hpfs=2 b1g=0 b2f=800 b2g=-3 b3f=3500 b3g=-3 b4f=11000 b4g=3 crat=3 catk=15 crel=150" },
            { "DARK", "hpf=80 b3g=-4 b4t=0 b4f=9000 b4g=-3 lpf=15000 lpfs=1" },
            { "BRIGHT", "hpf=120 b3g=-1 b4f=10000 b4g=4" },
        };

        struct List { const Preset* p; int n; };
        #define DE_LIST(a) { a, (int) (sizeof (a) / sizeof (a[0])) }
        const List kLists[kNumSources] = {
            DE_LIST (kKickIn), DE_LIST (kKickOut), DE_LIST (kSnareTop), DE_LIST (kSnareBot),
            DE_LIST (kTom), DE_LIST (kTom), DE_LIST (kTom),
            DE_LIST (kHat), DE_LIST (kRide), DE_LIST (kRoom), DE_LIST (kOh),
        };
        #undef DE_LIST

        //  ---- the factory kits: one preset per drum ------------------------
        //  order: KICK IN, KICK OUT, SNARE TOP, SNARE BOTTOM, TOM 1-3, HI-HAT, RIDE, ROOM, OH
        const Kit kKits[] = {
            { "STANDARD",      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
            { "CLASSIC ROCK",  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } },
            { "MODERN METAL",  { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
            { "EXTREME METAL", { 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2 } },
            { "VINTAGE ROCK",  { 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3 } },
            { "NATURAL",       { 0, 0, 0, 3, 3, 3, 3, 0, 0, 3, 0 } },
        };
    }

    const SourceDef& source (int s) { return kSources[s < 0 ? 0 : (s >= kNumSources ? kNumSources - 1 : s)]; }

    int numPresets (int s) { return kLists[s < 0 || s >= kNumSources ? 0 : s].n; }
    const Preset& preset (int s, int i)
    {
        const auto& l = kLists[s < 0 || s >= kNumSources ? 0 : s];
        return l.p[i < 0 ? 0 : (i >= l.n ? l.n - 1 : i)];
    }

    bool applyText (Params& p, const char* text, std::string* err)
    {
        const char* c = text;
        while (*c)
        {
            while (*c == ' ') ++c;
            if (! *c) break;
            const char* eq = std::strchr (c, '=');
            if (eq == nullptr) { if (err) *err = std::string ("no '=' in: ") + c; return false; }
            const std::string id (c, (size_t) (eq - c));
            char* end = nullptr;
            const float v = std::strtof (eq + 1, &end);
            if (end == eq + 1) { if (err) *err = "no value for " + id; return false; }
            if (! setById (p, id.c_str(), v)) { if (err) *err = "unknown parameter " + id; return false; }
            c = end;
        }
        return true;
    }

    Params presetParams (int s, int i)
    {
        Params p;
        p.source = (float) (s < 0 ? 0 : (s >= kNumSources ? kNumSources - 1 : s));
        applyText (p, source (s).standard);
        applyText (p, preset (s, i).values);
        return p;
    }

    int numKits() { return (int) (sizeof (kKits) / sizeof (kKits[0])); }
    const Kit& kit (int i) { return kKits[i < 0 ? 0 : (i >= numKits() ? numKits() - 1 : i)]; }
}
