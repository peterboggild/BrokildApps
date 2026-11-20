#include "de_params.h"

#include <cstring>
#include <algorithm>

namespace de
{
    namespace
    {
        using P = Params;
        const PSpec kSpecs[kNumParams] =
        {
            { "source", "Source",         K_CHOICE, 0, 10, 0, 0, "",
              "KICK IN|KICK OUT|SNARE TOP|SNARE BOTTOM|TOM 1|TOM 2|TOM 3|HI-HAT|RIDE|ROOM|OVERHEADS", &P::source, false },

            { "in",     "Input",          K_FLOAT, -24, 36, 0, 0, "dB", "", &P::in, true },     // up to +36: quiet drum mics
            { "pol",    "Polarity",       K_BOOL,  0, 1, 0, 0, "", "", &P::pol, true },

            { "gate",   "Gate",           K_BOOL,  0, 1, 0, 0, "", "", &P::gate, true },
            { "gthr",   "Gate threshold", K_FLOAT, -80, 0, -40, 0, "dB", "", &P::gthr, true },
            { "grange", "Gate range",     K_FLOAT, 0, 80, 30, 0, "dB", "", &P::grange, true },
            { "gatk",   "Gate attack",    K_FLOAT, 0.05f, 20, 0.5f, 1.0f, "ms", "", &P::gatk, true },
            { "ghold",  "Gate hold",      K_FLOAT, 0, 500, 60, 80, "ms", "", &P::ghold, true },
            { "grel",   "Gate release",   K_FLOAT, 5, 2000, 150, 200, "ms", "", &P::grel, true },
            { "gkeylo", "Gate key low",   K_FLOAT, 20, 2000, 60, 200, "Hz", "", &P::gkeylo, true },
            { "gkeyhi", "Gate key high",  K_FLOAT, 500, 20000, 8000, 3000, "Hz", "", &P::gkeyhi, true },

            { "hpf",    "High-pass",      K_FLOAT, 10, 1000, 30, 100, "Hz", "", &P::hpf, true },
            { "hpfs",   "High-pass slope",K_CHOICE, 0, 2, 0, 0, "", "OFF|12 dB|24 dB", &P::hpfs, true },
            { "lpf",    "Low-pass",       K_FLOAT, 1000, 22000, 18000, 6000, "Hz", "", &P::lpf, true },
            { "lpfs",   "Low-pass slope", K_CHOICE, 0, 2, 0, 0, "", "OFF|12 dB|24 dB", &P::lpfs, true },

            { "eq",     "EQ in",          K_BOOL,  0, 1, 1, 0, "", "", &P::eq, true },
            { "b1t",    "Band 1 type",    K_CHOICE, 0, 1, 1, 0, "", "SHELF|BELL", &P::b1t, true },
            { "b1f",    "Band 1 freq",    K_FLOAT, 20, 20000, 100, 1000, "Hz", "", &P::b1f, true },
            { "b1g",    "Band 1 gain",    K_FLOAT, -18, 18, 0, 0, "dB", "", &P::b1g, true },
            { "b1q",    "Band 1 Q",       K_FLOAT, 0.3f, 8, 1.0f, 1.2f, "Q", "", &P::b1q, true },
            { "b2f",    "Band 2 freq",    K_FLOAT, 20, 20000, 400, 1000, "Hz", "", &P::b2f, true },
            { "b2g",    "Band 2 gain",    K_FLOAT, -18, 18, 0, 0, "dB", "", &P::b2g, true },
            { "b2q",    "Band 2 Q",       K_FLOAT, 0.3f, 8, 1.0f, 1.2f, "Q", "", &P::b2q, true },
            { "b3f",    "Band 3 freq",    K_FLOAT, 20, 20000, 3000, 1000, "Hz", "", &P::b3f, true },
            { "b3g",    "Band 3 gain",    K_FLOAT, -18, 18, 0, 0, "dB", "", &P::b3g, true },
            { "b3q",    "Band 3 Q",       K_FLOAT, 0.3f, 8, 1.0f, 1.2f, "Q", "", &P::b3q, true },
            { "b4t",    "Band 4 type",    K_CHOICE, 0, 1, 0, 0, "", "SHELF|BELL", &P::b4t, true },
            { "b4f",    "Band 4 freq",    K_FLOAT, 20, 20000, 10000, 1000, "Hz", "", &P::b4f, true },
            { "b4g",    "Band 4 gain",    K_FLOAT, -18, 18, 0, 0, "dB", "", &P::b4g, true },
            { "b4q",    "Band 4 Q",       K_FLOAT, 0.3f, 8, 0.7f, 1.2f, "Q", "", &P::b4q, true },

            { "comp",   "Compressor",     K_BOOL,  0, 1, 0, 0, "", "", &P::comp, true },
            { "cthr",   "Comp threshold", K_FLOAT, -60, 0, -20, 0, "dB", "", &P::cthr, true },
            { "crat",   "Comp ratio",     K_FLOAT, 1, 20, 4, 4, "x", "", &P::crat, true },
            { "catk",   "Comp attack",    K_FLOAT, 0.05f, 100, 10, 5, "ms", "", &P::catk, true },
            { "crel",   "Comp release",   K_FLOAT, 5, 2000, 120, 150, "ms", "", &P::crel, true },
            { "cknee",  "Comp knee",      K_FLOAT, 0, 24, 6, 0, "dB", "", &P::cknee, true },
            { "cmake",  "Comp makeup",    K_FLOAT, -12, 24, 0, 0, "dB", "", &P::cmake, true },
            { "cmix",   "Comp mix",       K_FLOAT, 0, 1, 1, 0, "%", "", &P::cmix, true },
            { "csc",    "Comp key HPF",   K_FLOAT, 20, 500, 60, 100, "Hz", "", &P::csc, true },
            { "cauto",  "Comp auto release", K_BOOL, 0, 1, 1, 0, "", "", &P::cauto, true },

            { "out",    "Output",         K_FLOAT, -36, 24, 0, 0, "dB", "", &P::out, true },    // down to -36: KEEP LEVEL can give back what IN added
        };
    }

    const PSpec* specs() { return kSpecs; }

    int indexOf (const char* id)
    {
        for (int i = 0; i < kNumParams; ++i)
            if (std::strcmp (kSpecs[i].id, id) == 0) return i;
        return -1;
    }

    bool setById (Params& p, const char* id, float v)
    {
        const int i = indexOf (id);
        if (i < 0) return false;
        const auto& s = kSpecs[i];
        v = std::min (s.hi, std::max (s.lo, v));
        if (s.kind != K_FLOAT) v = (float) (int) (v + 0.5f);
        p.*(s.member) = v;
        return true;
    }

    float getById (const Params& p, const char* id)
    {
        const int i = indexOf (id);
        return i < 0 ? 0.0f : p.*(kSpecs[i].member);
    }

    void toKitValues (const Params& p, float* out)
    {
        int k = 0;
        for (int i = 0; i < kNumParams; ++i)
            if (kSpecs[i].member != &Params::source) out[k++] = p.*(kSpecs[i].member);
    }

    void fromKitValues (Params& p, const float* in)
    {
        int k = 0;
        for (int i = 0; i < kNumParams; ++i)
            if (kSpecs[i].member != &Params::source)
            {
                const auto& s = kSpecs[i];
                p.*(s.member) = std::min (s.hi, std::max (s.lo, in[k++]));
            }
    }
}
