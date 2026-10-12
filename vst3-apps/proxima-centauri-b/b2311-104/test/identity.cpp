/*  CHARACTER 0 IS THE OLD ENGINE. The material (2026-10-12) is meant to step
    aside completely at CHARACTER 0; this renders the same scenes through the
    engine as it was (test/legacy, frozen from git) and through the engine as
    it is, and compares them sample for sample. Not "close": identical. */
#include "../Source/Engine.h"
#include "legacy/Engine.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

static const int SR = 48000, BLK = 256;

template <class E>
static std::vector<float> scene (E& e, int spec, int kind)
{
    e.prepare (SR, BLK);
    e.requestSpecimen (spec);
    std::vector<float> l (BLK), r (BLK), out;
    const int nb = (int) (4.0 * SR / BLK);
    for (int b = 0; b < nb; ++b)
    {
        const double t = (double) b * BLK / SR;
        if (b == 4)                    { e.noteOn (36 + spec % 12, 0.8f); e.noteOn (43 + spec % 9, 0.5f); }
        if (b == (int) (1.8 * SR / BLK)) e.noteOff (36 + spec % 12);
        if (kind == 1 && b == 40)      e.strike (spec % 20, 0.5f);
        if (kind == 2 && b == 60)      e.pour (spec % 20, 400.0f);
        if (b == (int) (3.0 * SR / BLK)) e.allNotesOff();
        (void) t;
        e.process (l.data(), r.data(), BLK);
        out.insert (out.end(), l.begin(), l.end());
        out.insert (out.end(), r.begin(), r.end());
    }
    return out;
}

int main()
{
    int fails = 0, runs = 0;
    for (int spec : { 0, 7, 42, 63, 101, 127 })
        for (int kind = 0; kind < 3; ++kind)
            for (float amb : { 0.4748f, 0.95f, 0.10f })
            {
                ab104::Engine a;       a.p = ab104::Params();       a.p.character = 0.0f; a.p.ambient = amb;
                ab104legacy::Engine b; b.p = ab104legacy::Params();                          b.p.ambient = amb;
                if (kind == 2) { a.p.turbulence = b.p.turbulence = 0.9f; a.p.traffic = b.p.traffic = 0.6f; }
                const auto x = scene (a, spec, kind), y = scene (b, spec, kind);
                ++runs;
                const bool same = x.size() == y.size() && std::memcmp (x.data(), y.data(), x.size() * sizeof (float)) == 0;
                if (! same)
                {
                    ++fails;
                    size_t at = 0; while (at < x.size() && x[at] == y[at]) ++at;
                    std::printf ("  DIFFERS spec %d kind %d amb %.2f: first at sample %zu (%.6f vs %.6f)\n",
                                 spec, kind, amb, at / 2, at < x.size() ? x[at] : 0.f, at < y.size() ? y[at] : 0.f);
                }
            }
    std::printf ("CHARACTER 0 against the frozen engine: %d of %d scenes bit-identical  %s\n",
                 runs - fails, runs, fails ? "FAILED" : "ALL CLEAR");
    return fails ? 1 : 0;
}
