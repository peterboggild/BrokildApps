#pragma once

// LEGION — the host parameter table, in ONE place.
//
// The house lesson (Hairfryer's SPECS[], BWFX's descriptors): a parameter
// that is declared in one file, read in another and labelled in a third is
// a wiring bug waiting to happen. Here the id, the name, the range and the
// unit are one row, the APVTS layout is generated from the rows, and the
// editor builds itself from the same rows. A knob cannot be wired to the
// wrong parameter because nothing names a parameter twice.

#include <JuceHeader.h>

#include "../engine/legion_harmonizer.h"

namespace legion_ids
{
    //  globals
    static constexpr const char* mix      = "mix";
    static constexpr const char* output   = "output";
    static constexpr const char* humanize = "humanize";
    static constexpr const char* detail   = "detail";
    static constexpr const char* rackPos  = "rackpos";

    //  per voice — "v1_pitch" and so on. Built once, never spelled by hand.
    inline juce::String voice (int v, const char* what)
    {
        return "v" + juce::String (v + 1) + "_" + what;
    }
}

struct VoiceSpec
{
    const char* id;        // suffix after "vN_"
    const char* name;      // label on the panel
    float lo, hi, def, step;
    const char* unit;
};

//  the per-voice strip, top to bottom, exactly as the editor draws it
static const VoiceSpec kVoiceSpecs[] =
{
    { "pitch",  "PITCH",   -24.0f,  24.0f,   0.0f, 1.0f,   " st"   },
    { "fine",   "FINE",   -100.0f, 100.0f,   0.0f, 0.0f,   " ct"   },
    { "form",   "FORMANT", -12.0f,  12.0f,   0.0f, 0.0f,   " st"   },
    { "follow", "FOLLOW",    0.0f, 100.0f,   0.0f, 0.0f,   " %"    },
    { "level",  "LEVEL",   -60.0f,   6.0f,  -6.0f, 0.0f,   " dB"   },
    { "pan",    "PAN",    -100.0f, 100.0f,   0.0f, 0.0f,   ""      },
    { "delay",  "DELAY",     0.0f, legion::kMaxDelayMs, 0.0f, 0.0f, " ms" },
};
static constexpr int kNumVoiceSpecs = (int) (sizeof (kVoiceSpecs) / sizeof (kVoiceSpecs[0]));

//  the default interval each voice wakes up on, so a fresh instance with a
//  voice switched on is already a harmony and not four unisons
static constexpr float kDefaultPitch[legion::kVoices] = { -12.0f, 3.0f, 7.0f, 12.0f };
static constexpr float kDefaultPan  [legion::kVoices] = { -40.0f, 25.0f, -25.0f, 40.0f };
