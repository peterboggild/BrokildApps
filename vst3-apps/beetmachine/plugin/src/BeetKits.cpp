#include <JuceHeader.h>
#include "BeetKits.h"

namespace beet
{
    const int C3_ROW[NUM_SLOTS] = { 60, 61, 62, 63, 64, 65, 66, 67 };
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
    };

    int numKits() { return (int) (sizeof (KITS) / sizeof (KITS[0])); }
    const Kit& kit (int i) { return KITS[juce::jlimit (0, numKits() - 1, i)]; }
}
