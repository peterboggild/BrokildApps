#pragma once

// BEETMACHINE - slot types, note rows and the themed factory kits.
//
// A kit names each drum's preset by its NAME, never its index: a preset list
// that is reordered or grows must not quietly turn a kit's crash into a ride.
// An unknown name falls back to that drum's first preset and is reported by
// the bench.

#include <JuceHeader.h>

namespace beet
{
    enum SlotType { EMPTY = 0, KICK, SNARE, HATS, NUM_TYPES };
    constexpr int NUM_SLOTS = 8;

    enum OutMode { OUT_MIX = 0, OUT_OWN, OUT_BOTH, NUM_OUT_MODES };

    struct KitSlot
    {
        int         type;
        const char* preset;
        float       levelDb;
        float       pan;        // -1 .. +1
        unsigned    chokedBy;   // bit j set: a hit on slot j (0-based) chokes this slot
    };

    struct Kit
    {
        const char* name;
        KitSlot     slots[NUM_SLOTS];
    };

    int         numKits();
    const Kit&  kit (int index);

    const char* typeName  (int type);   // "EMPTY", "KICKSTART", "SNARE TACTICS", "HATS OFF"
    const char* typeShort (int type);   // "EMPTY", "KICK", "SNARE", "HATS"

    //  The note each drum is sent so that it plays its DIALLED sound, given
    //  that drum's own KEYS switch (0 FIXED, 1 GM KIT, 2 CHROMATIC). On
    //  CHROMATIC it is the drum's reference note, so it is exactly in tune.
    //  On GM KIT the reference note is not neutral: for Hats Off 42 IS the
    //  closed hi-hat, which would turn every crash and ride into a short
    //  closed hit - so a note GM leaves undefined is sent instead.
    int neutralNote (int type, int keys = 2);

    //  The two note rows. C3 is Ableton's C3 = MIDI 60. The GM row follows the
    //  slot convention: kicks, snares, closed hat, open hat, ride, crash.
    extern const int C3_ROW[NUM_SLOTS];
    extern const int GM_ROW[NUM_SLOTS];

    juce::String noteName (int midiNote);   // Ableton naming: 60 = C3
}
