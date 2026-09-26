#include <JuceHeader.h>
#include "BeetKits.h"

namespace beet
{
    const int C1_ROW[NUM_SLOTS] = { 36, 37, 38, 39, 40, 41, 42, 43 };
    const int GM_ROW[NUM_SLOTS] = { 36, 35, 38, 40, 42, 46, 51, 49 };

    const char* typeName (int t)
    {
        switch (t) { case KICK: return "KICKSTART"; case SNARE: return "SNARE TACTICS";
                     case HATS: return "HATS OFF";  default:    return "EMPTY"; }
    }
    const char* typeShort (int t)
    {
        switch (t) { case KICK: return "KICK"; case SNARE: return "SNARE";
                     case HATS: return "HATS"; default:    return "EMPTY"; }
    }
    int neutralNote (int t, int keys)
    {
        switch (t)
        {
            case KICK:  return 36;                          // Kickstart has no articulations
            case SNARE: return 38;                          // 38 is the plain snare in GM too
            case HATS:  return keys == 1 ? 60 : 42;         // GM: 60 is undefined, i.e. dialled
            default:    return 60;
        }
    }

    juce::String noteName (int n)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (n < 0 || n > 127) return "--";
        return juce::String (names[n % 12]) + juce::String (n / 12 - 2);
    }

    // Bits: slot numbers are 1-based on the panel, 0-based here.
    constexpr unsigned S5 = 1u << 4;     // closed hat
    constexpr unsigned S7 = 1u << 6;     // ride

    //  Every kit follows the convention: kicks in 1-2, snares in 3-4, the metal
    //  in 5-8 as closed hat / open hat / ride / crash (or what suits the kit).
    //  The open hat in slot 6 is choked by the closed hat in slot 5.
    static const Kit KITS[] =
    {
        { "STUDIO",
          { { KICK,  "Studio Kick",       0.0f,  0.00f, 0 },
            { KICK,  "Rock Kick",        -2.0f,  0.00f, 0 },
            { SNARE, "Studio Snare",      0.0f,  0.00f, 0 },
            { SNARE, "Rock Rim Shot",    -3.0f,  0.00f, 0 },
            { HATS,  "Studio Hi-Hat 14", -4.0f, -0.25f, 0 },
            { HATS,  "Loose Hat",        -5.0f, -0.25f, S5 },
            { HATS,  "Jazz Ride 20",     -6.0f,  0.30f, 0 },
            { HATS,  "Crash 16",         -6.0f, -0.35f, 0 } } },

        { "808",
          { { KICK,  "808 Boom",          0.0f,  0.00f, 0 },
            { KICK,  "808 Short",        -2.0f,  0.00f, 0 },
            { SNARE, "808 Snare",        -1.0f,  0.00f, 0 },
            { SNARE, "808 Clap",         -2.0f,  0.10f, 0 },
            { HATS,  "808 Closed Hat",   -5.0f, -0.20f, 0 },
            { HATS,  "808 Open Hat",     -6.0f, -0.20f, S5 },
            { HATS,  "808 Cymbal",       -8.0f,  0.30f, 0 },
            { HATS,  "606 Hats",         -7.0f,  0.25f, 0 } } },

        { "909",
          { { KICK,  "909 Classic",       0.0f,  0.00f, 0 },
            { KICK,  "909 Hard",         -2.0f,  0.00f, 0 },
            { SNARE, "909 Snare",        -1.0f,  0.00f, 0 },
            { SNARE, "909 Tight",        -2.0f,  0.00f, 0 },
            { HATS,  "909 Closed Hat",   -5.0f, -0.20f, 0 },
            { HATS,  "909 Open Hat",     -6.0f, -0.20f, S5 },
            { HATS,  "909 Ride",         -7.0f,  0.30f, 0 },
            { HATS,  "909 Crash",        -7.0f, -0.30f, 0 } } },

        { "EIGHTIES",
          { { KICK,  "LinnDrum",          0.0f,  0.00f, 0 },
            { KICK,  "DMX",              -2.0f,  0.00f, 0 },
            { SNARE, "Big Eighties",     -1.0f,  0.00f, 0 },
            { SNARE, "Simmons SDS-V",    -3.0f,  0.00f, 0 },
            { HATS,  "LinnDrum Hat",     -5.0f, -0.20f, 0 },
            { HATS,  "707 / 727 Hat",    -6.0f, -0.20f, S5 },
            { HATS,  "Ride Bell",        -8.0f,  0.30f, 0 },
            { HATS,  "Crash 18",         -7.0f, -0.30f, 0 } } },

        { "BOOM BAP",
          { { KICK,  "SP-12 Boom Bap",    0.0f,  0.00f, 0 },
            { KICK,  "MPC60 Thump",      -2.0f,  0.00f, 0 },
            { SNARE, "SP-1200 Crack",    -1.0f,  0.00f, 0 },
            { SNARE, "MPC60 Break",      -2.0f,  0.00f, 0 },
            { HATS,  "SP-1200 Hat",      -6.0f, -0.20f, 0 },
            { HATS,  "Loose Hat",        -7.0f, -0.20f, S5 },
            { HATS,  "Sizzle Ride",      -8.0f,  0.30f, 0 },
            { HATS,  "Crash 18",         -8.0f, -0.30f, 0 } } },

        { "TECHNO",
          { { KICK,  "Techno Rumble",     0.0f,  0.00f, 0 },
            { KICK,  "Hard Techno",      -2.0f,  0.00f, 0 },
            { SNARE, "Techno Crush",     -2.0f,  0.00f, 0 },
            { SNARE, "Warehouse Clap",   -3.0f,  0.10f, 0 },
            { HATS,  "Techno Closed",    -5.0f, -0.20f, 0 },
            { HATS,  "Rolling Open Hat", -6.0f, -0.20f, S5 },
            { HATS,  "Detroit Ride",     -8.0f,  0.30f, 0 },
            { HATS,  "Minimal Tick",     -8.0f,  0.35f, 0 } } },

        { "DUB",
          { { KICK,  "Clean Sub",         0.0f,  0.00f, 0 },
            { KICK,  "Felt Beater",      -3.0f,  0.00f, 0 },
            { SNARE, "Steppers",         -1.0f,  0.00f, 0 },
            { SNARE, "Tubby Throw",      -3.0f,  0.00f, 0 },
            { HATS,  "Roots Hi-Hat",     -6.0f, -0.20f, 0 },
            { HATS,  "Dub Hat Echo",     -7.0f, -0.20f, S5 },
            { HATS,  "Skank Hat",        -7.0f,  0.25f, 0 },
            { HATS,  "Space Cymbal",     -8.0f, -0.30f, 0 } } },

        { "INDUSTRIAL",
          { { KICK,  "Industrial Crush",  0.0f,  0.00f, 0 },
            { KICK,  "Gabber",           -3.0f,  0.00f, 0 },
            { SNARE, "Metal Shop",       -1.0f,  0.00f, 0 },
            { SNARE, "EBM Snap",         -2.0f,  0.00f, 0 },
            { HATS,  "Hard Techno Hat",  -6.0f, -0.20f, 0 },
            { HATS,  "Kraftwerk Metal",  -7.0f, -0.20f, S5 },
            { HATS,  "Metal Plate",      -8.0f,  0.30f, 0 },
            { HATS,  "China 18",         -8.0f, -0.30f, 0 } } },

        { "TRAP",
          { { KICK,  "Trap 808",          0.0f,  0.00f, 0 },
            { KICK,  "Dubstep Punch",    -3.0f,  0.00f, 0 },
            { SNARE, "Trap Snare Clap",  -1.0f,  0.00f, 0 },
            { SNARE, "DnB Snare",        -3.0f,  0.00f, 0 },
            { HATS,  "Trap Hat",         -5.0f, -0.20f, 0 },
            { HATS,  "UKG Shuffle Hat",  -7.0f, -0.20f, S5 },
            { HATS,  "Grime Hat",        -7.0f,  0.25f, 0 },
            { HATS,  "Halftime Crash",   -8.0f, -0.30f, 0 } } },

        { "LO-FI",
          { { KICK,  "Lo-Fi Dust",        0.0f,  0.00f, 0 },
            { KICK,  "Towel In The Drum",-2.0f,  0.00f, 0 },
            { SNARE, "Lo-Fi Ghost",      -1.0f,  0.00f, 0 },
            { SNARE, "Seventies Dead",   -2.0f,  0.00f, 0 },
            { HATS,  "Lo-Fi Hat",        -6.0f, -0.20f, 0 },
            { HATS,  "Hats In A Room",   -7.0f, -0.20f, S5 },
            { HATS,  "Dark Ride 22",     -8.0f,  0.30f, 0 },
            { HATS,  "Splash 10",        -8.0f, -0.30f, S7 } } },

        //  --- 260926.7: fourteen more, and these carry their own rack --------
        //  The echoes of a dub kit live on the SNARE and the HATS (each drum's
        //  own tempo-locked tape echo), never on the bus, because an echo on
        //  the bus echoes the kick too and that is mud. The rack brings the room.

        { "DUB SPACE",
          { { KICK,  "Clean Sub",         0.0f,  0.00f, 0,  "decay=700" },
            { KICK,  "Felt Beater",      -4.0f,  0.00f, 0 },
            { SNARE, "Roots Rim Shot",   -1.0f,  0.00f, 0,  "echo=0.35 time=5" },
            { SNARE, "Steppers",         -3.0f,  0.10f, 0,  "echo=0.25 time=4" },
            { HATS,  "Roots Hi-Hat",     -6.0f, -0.20f, 0,  "echo=0.12 time=3" },
            { HATS,  "Dub Hat Echo",     -8.0f, -0.20f, S5 },
            { HATS,  "Skank Hat",        -8.0f,  0.25f, 0 },
            { HATS,  "Space Cymbal",     -9.0f, -0.30f, 0 } },
          R"({"modules":{"reverb":{"on":1,"p":{"mix":14,"character":3,"length":140}}}})" },

        { "DNB ROLLER",
          { { KICK,  "909 Classic",       0.0f,  0.00f, 0,  "pitch=56 decay=260" },
            { KICK,  "Clean Sub",        -3.0f,  0.00f, 0,  "decay=500" },
            { SNARE, "DnB Snare",        -1.0f,  0.00f, 0 },
            { SNARE, "Amen Chop",        -5.0f,  0.00f, 0 },
            { HATS,  "Studio Hi-Hat 14", -6.0f, -0.20f, 0 },
            { HATS,  "Loose Hat",        -7.0f, -0.20f, S5 },
            { HATS,  "Sizzle Ride",      -9.0f,  0.30f, 0 },
            { HATS,  "Crash 18",         -9.0f, -0.30f, 0 } },
          R"({"modules":{"strip":{"on":1,"p":{"amount":40,"attack":10,"release":120,"high":2}}}})" },

        { "DNB NEURO",
          { { KICK,  "Dubstep Punch",     0.0f,  0.00f, 0,  "decay=240" },
            { KICK,  "Hard Techno",      -3.0f,  0.00f, 0,  "decay=300" },
            { SNARE, "DnB Snare",        -1.0f,  0.00f, 0,  "drive=0.5 engine=2 comp=0.7" },
            { SNARE, "Metal Shop",       -4.0f,  0.10f, 0 },
            { HATS,  "Hard Techno Hat",  -6.0f, -0.20f, 0 },
            { HATS,  "Rolling Open Hat", -7.0f, -0.20f, S5 },
            { HATS,  "Metal Plate",      -9.0f,  0.30f, 0 },
            { HATS,  "China 18",         -9.0f, -0.30f, 0 } },
          R"({"modules":{"lofi":{"on":1,"p":{"crush":0,"noise":0,"dirt":30}},"strip":{"on":1,"p":{"amount":55,"low":2,"mid":-2}}}})" },

        { "AMEN BREAKS",
          { { KICK,  "MPC60 Thump",       0.0f,  0.00f, 0 },
            { KICK,  "Kit In A Room",    -3.0f,  0.00f, 0 },
            { SNARE, "Amen Chop",        -1.0f,  0.00f, 0 },
            { SNARE, "MPC60 Break",      -5.0f,  0.00f, 0 },
            { HATS,  "SP-1200 Hat",      -6.0f, -0.20f, 0 },
            { HATS,  "Loose Hat",        -7.0f, -0.20f, S5 },
            { HATS,  "Jazz Ride 20",     -8.0f,  0.30f, 0 },
            { HATS,  "Crash 16",         -8.0f, -0.30f, 0 } },
          R"({"modules":{"lofi":{"on":1,"p":{"crush":25,"noise":0,"dirt":20}},"strip":{"on":1,"p":{"amount":35}}}})" },

        { "BIG BEAT",
          { { KICK,  "Rock Kick",         0.0f,  0.00f, 0,  "room=0.35" },
            { KICK,  "Studio Kick",      -3.0f,  0.00f, 0 },
            { SNARE, "Piccolo Funk",     -1.0f,  0.00f, 0 },
            { SNARE, "Loose Garage",     -4.0f,  0.00f, 0 },
            { HATS,  "Studio Hi-Hat 14", -5.0f, -0.25f, 0 },
            { HATS,  "Loose Hat",        -6.0f, -0.25f, S5 },
            { HATS,  "Sizzle Ride",      -8.0f,  0.30f, 0 },
            { HATS,  "Crash 18",         -8.0f, -0.35f, 0 } },
          R"({"modules":{"strip":{"on":1,"p":{"amount":60,"attack":20,"release":90}},"reverb":{"on":1,"p":{"mix":10,"character":0,"length":70}}}})" },

        //  DOOM is slow and enormous: low tuned drums in a big room
        { "DOOM",
          { { KICK,  "Rock Kick",         0.0f,  0.00f, 0,  "pitch=44 decay=650 room=0.5" },
            { KICK,  "Kit In A Room",    -3.0f,  0.00f, 0,  "pitch=48" },
            { SNARE, "Big Eighties",     -1.0f,  0.00f, 0,  "tune=160" },
            { SNARE, "Rock Rim Shot",    -4.0f,  0.00f, 0,  "tune=170 room=0.5" },
            { HATS,  "Loose Hat",        -7.0f, -0.25f, 0 },
            { HATS,  "Hats In A Room",   -8.0f, -0.25f, S5 },
            { HATS,  "Dark Ride 22",     -8.0f,  0.35f, 0 },
            { HATS,  "China 18",         -8.0f, -0.35f, 0 } },
          R"({"modules":{"reverb":{"on":1,"p":{"mix":16,"character":1,"length":280}}}})" },

        //  BLACK METAL is blast beats and double kick at 200 BPM and more, so
        //  both kicks are short, clicky and dry - every sixteenth has to stay a
        //  separate hit - and they are the two FEET: the same drum, a hair apart
        //  in tune, which is what two beaters on one head sound like. No rack:
        //  this music is cold and dry on purpose.
        { "BLACK METAL",
          { { KICK,  "Studio Kick",       0.0f,  0.00f, 0,  "pitch=62 decay=150 click=0.9 tone=0.85 room=0.05 sustain=-0.6" },
            { KICK,  "Studio Kick",      -0.5f,  0.00f, 0,  "pitch=61 decay=150 click=0.85 tone=0.8 room=0.05 sustain=-0.6" },
            { SNARE, "Piccolo Funk",     -1.0f,  0.00f, 0,  "decay=150 room=0.1" },
            { SNARE, "Rock Rim Shot",    -4.0f,  0.00f, 0 },
            { HATS,  "Studio Hi-Hat 14", -7.0f, -0.25f, 0 },
            { HATS,  "Loose Hat",        -8.0f, -0.25f, S5 },
            { HATS,  "China 18",         -9.0f,  0.35f, 0 },
            { HATS,  "Crash 18",         -8.0f, -0.35f, 0 } } },

        { "FACTORY",
          { { KICK,  "Industrial Crush",  0.0f,  0.00f, 0 },
            { KICK,  "Techno Rumble",    -3.0f,  0.00f, 0 },
            { SNARE, "Factory Floor",    -1.0f,  0.00f, 0 },
            { SNARE, "Power Electronics",-4.0f,  0.10f, 0 },
            { HATS,  "Kraftwerk Metal",  -7.0f, -0.20f, 0 },
            { HATS,  "Metal Plate",      -8.0f, -0.20f, S5 },
            { HATS,  "Warehouse Ride",   -9.0f,  0.30f, 0 },
            { HATS,  "China 18",         -8.0f, -0.30f, 0 } },
          R"({"modules":{"lofi":{"on":1,"p":{"crush":35,"noise":0,"dirt":45}},"reverb":{"on":1,"p":{"mix":18,"character":0,"length":90}}}})" },

        { "EBM",
          { { KICK,  "Drum Computer",     0.0f,  0.00f, 0,  "drive=0.3" },
            { KICK,  "Hard Techno",      -3.0f,  0.00f, 0 },
            { SNARE, "EBM Snap",         -1.0f,  0.00f, 0 },
            { SNARE, "Gated Machine",    -3.0f,  0.00f, 0 },
            { HATS,  "Techno Closed",    -6.0f, -0.20f, 0 },
            { HATS,  "Rolling Open Hat", -7.0f, -0.20f, S5 },
            { HATS,  "Kraftwerk Metal",  -9.0f,  0.30f, 0 },
            { HATS,  "Crash 16",         -9.0f, -0.30f, 0 } },
          R"({"modules":{"strip":{"on":1,"p":{"amount":45}},"reverb":{"on":1,"p":{"mix":12,"character":2,"length":60}}}})" },

        //  JAZZ CLUB keeps time on the RIDE, so the ride is the loud cymbal
        { "JAZZ CLUB",
          { { KICK,  "Jazz Kick",         0.0f,  0.00f, 0 },
            { KICK,  "Felt Beater",      -3.0f,  0.00f, 0 },
            { SNARE, "Jazz Snare",       -1.0f,  0.00f, 0 },
            { SNARE, "Seventies Dead",   -6.0f,  0.00f, 0 },
            { HATS,  "Studio Hi-Hat 14", -8.0f, -0.25f, 0 },
            { HATS,  "Loose Hat",        -9.0f, -0.25f, S5 },
            { HATS,  "Jazz Ride 20",     -4.0f,  0.30f, 0 },
            { HATS,  "Sizzle Ride",      -8.0f, -0.30f, 0 } },
          R"({"modules":{"reverb":{"on":1,"p":{"mix":12,"character":0,"length":110}}}})" },

        { "IDM",
          { { KICK,  "808 Short",         0.0f,  0.00f, 0 },
            { KICK,  "KR-55",            -2.0f,  0.00f, 0 },
            { SNARE, "Simmons SDS-V",    -1.0f,  0.00f, 0 },
            { SNARE, "Minimal Rim",      -3.0f,  0.20f, 0 },
            { HATS,  "Minimal Tick",     -6.0f, -0.20f, 0 },
            { HATS,  "606 Hats",         -7.0f, -0.20f, S5 },
            { HATS,  "CR-78 Hat",        -8.0f,  0.30f, 0 },
            { HATS,  "Splash 10",        -9.0f, -0.30f, 0 } },
          R"({"modules":{"lofi":{"on":1,"p":{"crush":40,"noise":0,"dirt":10}},"delay":{"on":1,"p":{"mix":10,"feedback":25,"sync":5,"feel":2}}}})" },

        { "DOWNTEMPO",
          { { KICK,  "CR-78",             0.0f,  0.00f, 0,  "decay=320" },
            { KICK,  "Felt Beater",      -3.0f,  0.00f, 0 },
            { SNARE, "CR-78",            -1.0f,  0.00f, 0 },
            { SNARE, "Space Echo Clap",  -4.0f,  0.10f, 0 },
            { HATS,  "CR-78 Hat",        -6.0f, -0.20f, 0 },
            { HATS,  "Hats In A Room",   -8.0f, -0.20f, S5 },
            { HATS,  "606 Hats",         -9.0f,  0.30f, 0 },
            { HATS,  "Space Cymbal",    -10.0f, -0.30f, 0 } },
          R"({"modules":{"lofi":{"on":1,"p":{"crush":10,"noise":0,"dirt":25}},"reverb":{"on":1,"p":{"mix":14,"character":2,"length":180}}}})" },

        { "DUB TECHNO",
          { { KICK,  "House Thump",       0.0f,  0.00f, 0,  "decay=450" },
            { KICK,  "Techno Rumble",    -4.0f,  0.00f, 0 },
            { SNARE, "Dub Techno Space", -2.0f,  0.00f, 0 },
            { SNARE, "Minimal Rim",      -4.0f,  0.10f, 0 },
            { HATS,  "Dub Techno Hat",   -6.0f, -0.20f, 0 },
            { HATS,  "Rolling Open Hat", -8.0f, -0.20f, S5 },
            { HATS,  "Detroit Ride",     -9.0f,  0.30f, 0 },
            { HATS,  "Space Cymbal",    -10.0f, -0.30f, 0 } },
          R"({"modules":{"reverb":{"on":1,"p":{"mix":12,"character":1,"length":220}}}})" },

        { "HARD TECHNO",
          { { KICK,  "Hard Techno",       0.0f,  0.00f, 0 },
            { KICK,  "Techno Rumble",    -2.0f,  0.00f, 0 },
            { SNARE, "Techno Crush",     -2.0f,  0.00f, 0 },
            { SNARE, "Warehouse Clap",   -3.0f,  0.10f, 0 },
            { HATS,  "Hard Techno Hat",  -6.0f, -0.20f, 0 },
            { HATS,  "Rolling Open Hat", -7.0f, -0.20f, S5 },
            { HATS,  "Warehouse Ride",   -8.0f,  0.30f, 0 },
            { HATS,  "Crash 18",         -9.0f, -0.30f, 0 } },
          R"({"modules":{"saturation":{"on":1,"p":{"drive":6,"tone":65}},"strip":{"on":1,"p":{"amount":50}}}})" },
    };

    int numKits() { return (int) (sizeof (KITS) / sizeof (KITS[0])); }
    const Kit& kit (int i) { return KITS[juce::jlimit (0, numKits() - 1, i)]; }
}
