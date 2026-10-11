#pragma once
// Magneto Transport - HEAD PATTERN (stage 8, BUGLIST 1).
//
// A one-shot snap, not a lock: SET writes the three head positions once, from a
// pattern of ratios to the tempo head's distance, and lets go - so a nudge or a
// drag on the rail just works afterwards (a lock would be two owners for one
// value). The tempo head keeps its own position; the unit is its distance over
// its own ratio; the pattern's positions go to the heads in rail order (heads
// cannot pass each other, and they are identical). The carriages then travel
// there with their real inertia, so every echo in flight glides. No JUCE here.

namespace mt
{

constexpr int NUM_PATTERNS = 7;
#define MT_PATTERN_NAMES "1:2:3|1:1.5:2|1:1.33:2|1:2:4|1:1.5:3|1:3:4|GOLDEN"
inline constexpr double PATTERNS[NUM_PATTERNS][3] = {
    { 1.0, 2.0, 3.0 },                       // the Space Echo's
    { 1.0, 1.5, 2.0 },                       // dotted
    { 1.0, 4.0 / 3.0, 2.0 },                 // a triplet feel
    { 1.0, 2.0, 4.0 },                       // doubling
    { 1.0, 1.5, 3.0 },
    { 1.0, 3.0, 4.0 },
    { 1.0, 1.6180339887, 2.6180339887 },     // golden
};

/*  The positions a pattern asks for (cm), given the tempo head (0..2) and the
    current positions. Returns the number of heads that would fall off the rail
    (1.5 .. 24 cm); their positions are clamped and their bit is set in `outMask`. */
inline int headPattern (int pattern, int tempoHead, const double cur[3], double out[3], int& outMask,
                        double lo = 1.5, double hi = 24.0)
{
    const double* r = PATTERNS[(pattern >= 0 && pattern < NUM_PATTERNS) ? pattern : 0];
    const int t = (tempoHead >= 0 && tempoHead < 3) ? tempoHead : 0;
    const double unit = cur[t] / r[t];
    int bad = 0; outMask = 0;
    for (int i = 0; i < 3; ++i)
    {
        out[i] = (i == t) ? cur[t] : unit * r[i];
        if (out[i] < lo - 1.0e-9 || out[i] > hi + 1.0e-9) { ++bad; outMask |= 1 << i; out[i] = out[i] < lo ? lo : hi; }
    }
    return bad;
}

/*  The same in RAIL order: heads cannot pass each other, so the pattern's ascending
    ratios go to the heads sorted by where they actually are (railCm), the tempo head
    keeping its position. Returns the number clamped; bit i of outMask is head i. */
inline int headPatternRail (int pattern, int tempoHead, const double railCm[3], double out[3], int& outMask,
                            double lo = 1.5, double hi = 24.0)
{
    const double* r = PATTERNS[(pattern >= 0 && pattern < NUM_PATTERNS) ? pattern : 0];
    const int t = (tempoHead >= 0 && tempoHead < 3) ? tempoHead : 0;
    int ord[3] = { 0, 1, 2 };
    for (int a = 0; a < 2; ++a)
        for (int b = 0; b < 2 - a; ++b)
            if (railCm[ord[b]] > railCm[ord[b + 1]] || (railCm[ord[b]] == railCm[ord[b + 1]] && ord[b] > ord[b + 1]))
                { const int x = ord[b]; ord[b] = ord[b + 1]; ord[b + 1] = x; }
    int rank[3] {};
    for (int k = 0; k < 3; ++k) rank[ord[k]] = k;
    const double unit = railCm[t] / r[rank[t]];
    int bad = 0; outMask = 0;
    for (int i = 0; i < 3; ++i)
    {
        out[i] = (i == t) ? railCm[t] : unit * r[rank[i]];
        if (out[i] < lo - 1.0e-9 || out[i] > hi + 1.0e-9) { ++bad; outMask |= 1 << i; out[i] = out[i] < lo ? lo : hi; }
    }
    return bad;
}

} // namespace mt
