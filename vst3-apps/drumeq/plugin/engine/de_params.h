#pragma once

// DRUMEQ - the parameter table. ONE table drives the host parameters, the
// engine, the presets, the kit link and the panel, so a parameter cannot be
// read in one place under one name and written in another under a different
// one (the Hairfryer pattern).
//
// Values are in REAL UNITS everywhere (Hz, dB, ms). A frequency is a
// frequency whichever drum the instance is on: changing SOURCE re-labels and
// re-ranges the panel's knobs, it never re-interprets a stored number.

namespace de
{
    enum Kind { K_FLOAT, K_CHOICE, K_BOOL };

    struct Params
    {
        float source = 0;                        // which drum (see de_sources.h)

        float in = 0, pol = 0;                   // input trim dB, polarity

        float gate = 0, gthr = -40, grange = 30, gatk = 0.5f, ghold = 60, grel = 150,
              gkeylo = 60, gkeyhi = 8000;

        float hpf = 30, hpfs = 0, lpf = 18000, lpfs = 0;    // slopes: 0 off, 1 12 dB, 2 24 dB

        float eq = 1;
        float b1t = 1, b1f = 100,  b1g = 0, b1q = 1.0f;     // band type: 0 shelf, 1 bell
        float          b2f = 400,  b2g = 0, b2q = 1.0f;
        float          b3f = 3000, b3g = 0, b3q = 1.0f;
        float b4t = 0, b4f = 10000, b4g = 0, b4q = 0.7f;

        float comp = 0, cthr = -20, crat = 4, catk = 10, crel = 120, cknee = 6,
              cmake = 0, cmix = 1, csc = 60, cauto = 1;

        float out = 0;
    };

    struct PSpec
    {
        const char* id;
        const char* name;          // the host's name for it
        Kind kind;
        float lo, hi, def;
        float centre;              // skew centre for a log-ish knob, 0 = linear
        const char* unit;          // "dB" "Hz" "ms" "%" "x" "Q"
        const char* choices;       // "A|B|C" for a choice
        float Params::* member;
        bool automatable;
    };

    constexpr int kNumParams = 41;
    const PSpec* specs();
    int  indexOf (const char* id);                        // -1 if unknown
    bool setById (Params&, const char* id, float v);      // clamps; false if unknown
    float getById (const Params&, const char* id);

    //  the parameters a kit carries: every one except SOURCE (the kit is
    //  indexed BY source), in table order
    constexpr int kKitValues = kNumParams - 1;
    void toKitValues (const Params&, float* out);         // kKitValues floats
    void fromKitValues (Params&, const float* in);        // keeps p.source
}
