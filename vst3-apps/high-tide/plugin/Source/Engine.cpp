#include "Engine.h"
#include <algorithm>
#include <cstdlib>

namespace ht
{

//==============================================================================
//  the parameter table — one place, walked by everything
#define P(m) [] (Params& q) -> float& { return q.m; }

static const char* const L_ROCKRATIO[] = { "1/1", "2/1", "1/2", "1/3", "3/2", "3/1", "FREE" };
static const char* const L_ROCKMODE[]  = { "ROCK", "BREATH" };
static const char* const L_TARGET[]    = { "POSITION", "TIDE", "ROCK", "HOLD", "TONE", "PITCH" };
static const char* const L_SYNC[]      = { "FREE", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/8T", "1/16T" };
static const char* const L_SHAPE[]     = { "SINE", "TRI", "SAW", "SQUARE", "S&H" };
static const char* const L_VOICES[]    = { "POLY", "MONO", "LEGATO" };
static const char* const L_QUALITY[]   = { "2X", "4X" };

static const PSpec SPECS[] =
{
    { "level",     "LEVEL",         "how loud the instrument is allowed to be",                 0.70f, KP_VOL,   0, 0,        P(level),     nullptr, 0 },
    { "tapPos",    "POSITION TAP",  "the ball's position — the round tap",                      1.00f, KP_PCT,   0, 0,        P(tapPos),    nullptr, 0 },
    { "tapVel",    "VELOCITY TAP",  "the ball's velocity — the bright tap",                     0.00f, KP_PCT,   0, 0,        P(tapVel),    nullptr, 0 },
    { "tapFrc",    "FORCE TAP",     "the wall's push on the ball — the hard tap",               0.00f, KP_PCT,   0, 0,        P(tapFrc),    nullptr, 0 },
    { "tone",      "TONE",          "the radiation lowpass, 200 Hz to 20 kHz",                  0.85f, KP_PCT,   0, 0,        P(tone),      nullptr, 0 },
    { "strike",    "STRIKE",        "the energy of a full-velocity hit",                        0.60f, KP_PCT,   0, 0,        P(strike),    nullptr, 0 },
    { "velSens",   "VELOCITY",      "how much velocity scales the strike",                      0.70f, KP_PCT,   0, 0,        P(velSens),   nullptr, 0 },
    { "friction",  "FRICTION",      "energy lost while the note is held — zero is frictionless", 0.12f, KP_PCT,  0, 0,        P(friction),  nullptr, 0 },
    { "release",   "RELEASE",       "energy lost once the note is let go",                      0.45f, KP_PCT,   0, 0,        P(release),   nullptr, 0 },
    { "servo",     "SERVO",         "zero trusts the bowl's own period; one corrects it each cycle", 0.00f, KP_PCT, 0, 0,   P(servo),     nullptr, 0 },
    { "position",  "POSITION",      "where the ball is towed when the pin lane is empty",       0.00f, KP_PCT,   0, 0,        P(position),  nullptr, 0 },
    { "hold",      "HOLD",          "how hard the tether pulls the ball toward its target",     0.60f, KP_PCT,   0, 0,        P(hold),      nullptr, 0 },
    { "tide",      "TIDE",          "the water line on the relief — under water, a ridge is not there", 0.50f, KP_PCT, 0, 0, P(tide),    nullptr, 0 },
    { "rock",      "ROCK",          "how hard the whole terrain is driven at the note",         0.00f, KP_PCT,   0, 0,        P(rock),      nullptr, 0 },
    { "rockRatio", "ROCK RATIO",    "the drive's rate as a ratio of the note",                  0.00f, KP_LIST,  0, 0,        P(rockRatio), L_ROCKRATIO, 7 },
    { "rockHz",    "ROCK RATE",     "the drive's rate when the ratio is FREE",                  0.40f, KP_HZ,    0.1f, 60.0f, P(rockHz),    nullptr, 0 },
    { "rockMode",  "ROCK MODE",     "ROCK tilts the terrain like a see-saw; BREATH steepens and relaxes the bowls", 0.00f, KP_LIST, 0, 0, P(rockMode), L_ROCKMODE, 2 },
    { "unison",    "UNISON",        "how many balls share the bowl",                            0.00f, KP_INT,   1, 4,        P(unison),    nullptr, 0 },
    { "spread",    "SPREAD",        "how differently the balls are struck, and how they repel", 0.30f, KP_PCT,   0, 0,        P(spread),    nullptr, 0 },
    { "detune",    "DETUNE",        "how far apart the balls' clocks run - fifty cents at full", 0.00f, KP_PCT,   0, 0,        P(detune),    nullptr, 0 },
    { "width",     "WIDTH",         "how wide the balls stand in the image",                    0.50f, KP_PCT,   0, 0,        P(width),     nullptr, 0 },
    { "ampA",      "AMP ATTACK",    "the output envelope's rise",                               0.30f, KP_SEC,   0, 0,        P(ampA),      nullptr, 0 },
    { "ampD",      "AMP DECAY",     "its fall to the sustain level",                            0.64f, KP_SEC,   0, 0,        P(ampD),      nullptr, 0 },
    { "ampS",      "AMP SUSTAIN",   "the level held while the key is down",                     0.80f, KP_PCT,   0, 0,        P(ampS),      nullptr, 0 },
    { "ampR",      "AMP RELEASE",   "its fall after the key is let go",                         0.66f, KP_SEC,   0, 0,        P(ampR),      nullptr, 0 },
    { "modA",      "MOD ATTACK",    "the mod envelope's rise",                                  0.39f, KP_SEC,   0, 0,        P(modA),      nullptr, 0 },
    { "modD",      "MOD DECAY",     "its fall to the sustain level",                            0.69f, KP_SEC,   0, 0,        P(modD),      nullptr, 0 },
    { "modS",      "MOD SUSTAIN",   "the level held while the key is down",                     0.00f, KP_PCT,   0, 0,        P(modS),      nullptr, 0 },
    { "modR",      "MOD RELEASE",   "its fall after the key is let go",                         0.64f, KP_SEC,   0, 0,        P(modR),      nullptr, 0 },
    { "modTarget", "MOD TARGET",    "what the mod envelope moves",                              0.00f, KP_LIST,  0, 0,        P(modTarget), L_TARGET, 6 },
    { "modAmt",    "MOD AMOUNT",    "how far, either way",                                      0.50f, KP_BIPOL, 0, 0,        P(modAmt),    nullptr, 0 },
    { "lfo1Rate",  "LFO 1 RATE",    "cycles per second when free",                              0.35f, KP_HZ,    0.02f, 40.0f, P(lfo1Rate), nullptr, 0 },
    { "lfo1Sync",  "LFO 1 SYNC",    "free, or locked to the host clock",                        0.00f, KP_LIST,  0, 0,        P(lfo1Sync),  L_SYNC, 10 },
    { "lfo1Shape", "LFO 1 SHAPE",   "the wave it draws",                                        0.00f, KP_LIST,  0, 0,        P(lfo1Shape), L_SHAPE, 5 },
    { "lfo1Target","LFO 1 TARGET",  "what it moves",                                            0.00f, KP_LIST,  0, 0,        P(lfo1Target),L_TARGET, 6 },
    { "lfo1Amt",   "LFO 1 AMOUNT",  "how far, either way",                                      0.50f, KP_BIPOL, 0, 0,        P(lfo1Amt),   nullptr, 0 },
    { "lfo2Rate",  "LFO 2 RATE",    "cycles per second when free",                              0.20f, KP_HZ,    0.02f, 40.0f, P(lfo2Rate), nullptr, 0 },
    { "lfo2Sync",  "LFO 2 SYNC",    "free, or locked to the host clock",                        0.00f, KP_LIST,  0, 0,        P(lfo2Sync),  L_SYNC, 10 },
    { "lfo2Shape", "LFO 2 SHAPE",   "the wave it draws",                                        0.00f, KP_LIST,  0, 0,        P(lfo2Shape), L_SHAPE, 5 },
    { "lfo2Target","LFO 2 TARGET",  "what it moves",                                            1.00f, KP_LIST,  0, 0,        P(lfo2Target),L_TARGET, 6 },
    { "lfo2Amt",   "LFO 2 AMOUNT",  "how far, either way",                                      0.50f, KP_BIPOL, 0, 0,        P(lfo2Amt),   nullptr, 0 },
    { "tune",      "TUNE",          "twelve semitones either way",                              0.50f, KP_SEMI,  -12, 12,     P(tune),      nullptr, 0 },
    { "glide",     "GLIDE",         "how long a mono or legato note takes to reach its pitch",  0.00f, KP_PCT,   0, 0,        P(glide),     nullptr, 0 },
    { "voiceMode", "VOICES",        "twelve at once, one that restrikes, or one that glides",   0.00f, KP_LIST,  0, 0,        P(voiceMode), L_VOICES, 3 },
    { "ceiling",   "CEILING",       "where the hard-knee ceiling begins",                       0.50f, KP_PCT,   0, 0,        P(ceiling),   nullptr, 0 },
    { "quality",   "OVERSAMPLE",    "how finely the ball's clock is stepped — 4x is the sound, 2x is the economy", 1.00f, KP_LIST, 0, 0, P(quality), L_QUALITY, 2 },
};
#undef P

static const int NSPEC = (int) (sizeof (SPECS) / sizeof (SPECS[0]));

int          numParams()       { return NSPEC; }
const PSpec& paramSpec (int i) { return SPECS[i < 0 ? 0 : (i >= NSPEC ? NSPEC - 1 : i)]; }
int paramIndex (const char* id)
{
    for (int i = 0; i < NSPEC; ++i) if (std::strcmp (SPECS[i].id, id) == 0) return i;
    return -1;
}
float secondsOf (float v) { return 0.002f * std::pow (4000.0f, v); }
float hzOf (const PSpec& s, float v) { return s.lo * std::pow (s.hi / s.lo, v); }
int   listIndex (const PSpec& s, float v)
{
    if (s.nlist <= 1) return 0;
    int i = (int) std::lround (v * (float) (s.nlist - 1));
    return i < 0 ? 0 : (i >= s.nlist ? s.nlist - 1 : i);
}
static inline float clamp01 (float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline double clampd (double v, double a, double b) { return v < a ? a : (v > b ? b : v); }

//  the list parameters the tick and the render consult, resolved once
struct PIdx
{
    int voiceMode, rockMode, rockRatio, rockHz, quality, modTarget, lfo1Target, lfo2Target,
        lfo1Sync, lfo1Shape, lfo1Rate, lfo2Sync, lfo2Shape, lfo2Rate;
    PIdx()
      : voiceMode (paramIndex ("voiceMode")), rockMode (paramIndex ("rockMode")), rockRatio (paramIndex ("rockRatio")),
        rockHz (paramIndex ("rockHz")), quality (paramIndex ("quality")), modTarget (paramIndex ("modTarget")),
        lfo1Target (paramIndex ("lfo1Target")), lfo2Target (paramIndex ("lfo2Target")),
        lfo1Sync (paramIndex ("lfo1Sync")), lfo1Shape (paramIndex ("lfo1Shape")), lfo1Rate (paramIndex ("lfo1Rate")),
        lfo2Sync (paramIndex ("lfo2Sync")), lfo2Shape (paramIndex ("lfo2Shape")), lfo2Rate (paramIndex ("lfo2Rate")) {}
};
static const PIdx& PI_() { static const PIdx p; return p; }

//==============================================================================
//  THE TERRAIN
static const float DX = (XMAX - XMIN) / (float) (NX - 1);
static inline float xOf (int i) { return XMIN + DX * (float) i; }

Terrain::Terrain()
{
    u.assign ((size_t) NX * NZ, 0.0f);
    for (int L = 1; L < NLOD; ++L) lod[L].assign ((size_t) NX * NZ, 0.0f);
    makeReference();
}

void Terrain::makeReference()
{
    for (int j = 0; j < NZ; ++j)
        for (int i = 0; i < NX; ++i)
        {
            const float x = xOf (i);
            at (i, j) = 0.5f * x * x;
        }
    recomputeRelief();
}

void Terrain::setAll (const float* src)
{
    for (size_t k = 0; k < u.size(); ++k) u[k] = src[k] < 0 ? 0 : (src[k] > UMAX * 2 ? UMAX * 2 : src[k]);
    recomputeRelief();
}

int Terrain::levelForStep (double stepCells)
{
    if (stepCells <= 1.0) return 0;
    int L = (int) std::lround (std::log2 (stepCells));
    return L < 0 ? 0 : (L >= NLOD ? NLOD - 1 : L);
}

/*  Blur one column along x into every level. Edge-clamped gaussian, kernel
    to three sigma. Level L: sigma 2^L cells. */
void Terrain::rebuildLodColumn (int j)
{
    const float* src = u.data() + (size_t) j * NX;
    for (int L = 1; L < NLOD; ++L)
    {
        const int   s  = 1 << L;
        const int   hw = 3 * s;
        float* dst = lod[L].data() + (size_t) j * NX;
        //  kernel
        float k[3 * 16 + 1];
        float sum = 0;
        for (int t = 0; t <= hw; ++t) { k[t] = std::exp (-0.5f * (float) (t * t) / (float) (s * s)); sum += t == 0 ? k[t] : 2 * k[t]; }
        for (int t = 0; t <= hw; ++t) k[t] /= sum;
        for (int i = 0; i < NX; ++i)
        {
            float acc = k[0] * src[i];
            for (int t = 1; t <= hw; ++t)
            {
                const int a = i - t < 0 ? 0 : i - t;
                const int b = i + t >= NX ? NX - 1 : i + t;
                acc += k[t] * (src[a] + src[b]);
            }
            dst[i] = acc;
        }
    }
}

void Terrain::writeRegion (int x0, int nx, int z0, int nz, const float* src)
{
    for (int jj = 0; jj < nz; ++jj)
    {
        const int j = z0 + jj;
        if (j < 0 || j >= NZ) continue;
        for (int ii = 0; ii < nx; ++ii)
        {
            const int i = x0 + ii;
            if (i < 0 || i >= NX) continue;
            const float v = src[(size_t) jj * nx + ii];
            at (i, j) = v < 0 ? 0 : (v > UMAX * 2 ? UMAX * 2 : v);
        }
    }
    for (int jj = 0; jj < nz; ++jj) { const int j = z0 + jj; if (j >= 0 && j < NZ) { recomputeColumn (j); rebuildLodColumn (j); } }
    //  the relief slope of the neighbours changes too
    rMin = 1e9f; rMax = -1e9f;
    for (int j = 0; j < NZ; ++j) { rMin = std::min (rMin, R[j]); rMax = std::max (rMax, R[j]); }
    for (int j = 0; j < NZ; ++j)
    {
        const int a = j > 0 ? j - 1 : j, b = j < NZ - 1 ? j + 1 : j;
        Rslope[j] = (R[b] - R[a]) / ((float) (b - a) / (float) (NZ - 1));
    }
}

void Terrain::recomputeColumn (int j)
{
    int   fi = 0; float fm = at (0, j);
    for (int i = 1; i < NX; ++i) if (at (i, j) < fm) { fm = at (i, j); fi = i; }
    floorI[j] = fi; floorX[j] = xOf (fi); R[j] = fm;
    //  refine the floor with the parabola through the three nearest samples
    if (fi > 0 && fi < NX - 1)
    {
        const float a = at (fi - 1, j), b = at (fi, j), c = at (fi + 1, j);
        const float den = a - 2 * b + c;
        if (den > 1e-9f) floorX[j] = xOf (fi) + DX * 0.5f * (a - c) / den;
    }
}

void Terrain::recomputeRelief()
{
    for (int j = 0; j < NZ; ++j) { recomputeColumn (j); rebuildLodColumn (j); }
    rMin = 1e9f; rMax = -1e9f;
    for (int j = 0; j < NZ; ++j) { rMin = std::min (rMin, R[j]); rMax = std::max (rMax, R[j]); }
    for (int j = 0; j < NZ; ++j)
    {
        const int a = j > 0 ? j - 1 : j, b = j < NZ - 1 ? j + 1 : j;
        Rslope[j] = (R[b] - R[a]) / ((float) (b - a) / (float) (NZ - 1));
    }
}

/*  Catmull-Rom along one row: value, d/dt and d2/dt2. */
static inline void cr (float p0, float p1, float p2, float p3, float t, float& v, float& dv, float& ddv)
{
    const float c1 = 0.5f * (p2 - p0);
    const float c2 = 0.5f * (2 * p0 - 5 * p1 + 4 * p2 - p3);
    const float c3 = 0.5f * (-p0 + 3 * p1 - 3 * p2 + p3);
    v   = p1 + t * (c1 + t * (c2 + t * c3));
    dv  = c1 + t * (2 * c2 + 3 * t * c3);
    ddv = 2 * c2 + 6 * t * c3;
}

void Terrain::sample (float x, float z, float& U, float& Ux, float& Uz, float& Uxx, int level) const
{
    float fx = (x - XMIN) / DX;
    if (fx < 1.0f) fx = 1.0f; else if (fx > (float) (NX - 3)) fx = (float) (NX - 3);
    const int   i1 = (int) fx;
    const float tx = fx - (float) i1;
    float fz = z * (float) (NZ - 1);
    if (fz < 0) fz = 0; else if (fz > (float) (NZ - 1)) fz = (float) (NZ - 1);
    int j0 = (int) fz; if (j0 > NZ - 2) j0 = NZ - 2;
    const float tz = fz - (float) j0;

    const float* base = level > 0 ? lod[level].data() : u.data();
    const float* r0 = base + (size_t) j0 * NX + (size_t) (i1 - 1);
    const float* r1 = r0 + NX;
    float v0, d0, dd0, v1, d1, dd1;
    cr (r0[0], r0[1], r0[2], r0[3], tx, v0, d0, dd0);
    cr (r1[0], r1[1], r1[2], r1[3], tx, v1, d1, dd1);
    U   = v0 + (v1 - v0) * tz;
    Ux  = (d0 + (d1 - d0) * tz) / DX;
    Uz  = (v1 - v0) * (float) (NZ - 1);
    Uxx = (dd0 + (dd1 - dd0) * tz) / (DX * DX);
}

/*  d2U/dx2 alone, at the nearest row — the sub-step budget's look-ahead. */
float Terrain::stiffnessAt (float x, float z, int level) const
{
    float fx = (x - XMIN) / DX;
    if (fx < 1.0f) fx = 1.0f; else if (fx > (float) (NX - 3)) fx = (float) (NX - 3);
    const int   i1 = (int) fx;
    const float t = fx - (float) i1;
    int j = (int) (z * (float) (NZ - 1) + 0.5f); if (j < 0) j = 0; else if (j > NZ - 1) j = NZ - 1;
    const float* base = level > 0 ? lod[level].data() : u.data();
    const float* r = base + (size_t) j * NX + (size_t) (i1 - 1);
    const float c2 = 0.5f * (2 * r[0] - 5 * r[1] + 4 * r[2] - r[3]);
    const float c3 = 0.5f * (-r[0] + 3 * r[1] - 3 * r[2] + r[3]);
    float s = (2 * c2 + 6 * t * c3) / (DX * DX);
    //  the edge of the world is stiff too
    if (x > XMAX - 0.06f || x < XMIN + 0.06f) s += 400.0f;
    return s;
}

float Terrain::reliefAt (float z) const
{
    float fz = clamp01 (z) * (float) (NZ - 1);
    int j0 = (int) fz; if (j0 > NZ - 2) j0 = NZ - 2;
    const float t = fz - (float) j0;
    return R[j0] + (R[j0 + 1] - R[j0]) * t;
}
float Terrain::reliefSlopeAt (float z) const
{
    float fz = clamp01 (z) * (float) (NZ - 1);
    int j0 = (int) fz; if (j0 > NZ - 2) j0 = NZ - 2;
    const float t = fz - (float) j0;
    return Rslope[j0] + (Rslope[j0 + 1] - Rslope[j0]) * t;
}
float Terrain::floorAt (float z) const
{
    float fz = clamp01 (z) * (float) (NZ - 1);
    int j0 = (int) fz; if (j0 > NZ - 2) j0 = NZ - 2;
    const float t = fz - (float) j0;
    return floorX[j0] + (floorX[j0 + 1] - floorX[j0]) * t;
}

float Terrain::widthDeparture (int j) const
{
    const int fi = floorI[j];
    const float uf = R[j];
    const float hmax = std::min (at (0, j), at (NX - 1, j)) - uf;
    if (hmax <= 1e-4f) return 0;
    float worst = 0;
    for (int k = 1; k <= 8; ++k)
    {
        const float h = hmax * (float) k / 9.0f;
        //  walk out from the floor to the first crossing of uf + h
        float xl = xOf (0), xr = xOf (NX - 1);
        for (int i = fi; i > 0; --i)
            if (at (i - 1, j) - uf >= h)
            {
                const float a = at (i, j) - uf, b = at (i - 1, j) - uf;
                xl = xOf (i) - DX * (h - a) / std::max (1e-9f, b - a); break;
            }
        for (int i = fi; i < NX - 1; ++i)
            if (at (i + 1, j) - uf >= h)
            {
                const float a = at (i, j) - uf, b = at (i + 1, j) - uf;
                xr = xOf (i) + DX * (h - a) / std::max (1e-9f, b - a); break;
            }
        const float w = xr - xl, ref = 2.0f * std::sqrt (2.0f * h);
        worst = std::max (worst, std::abs (w / ref - 1.0f));
    }
    return worst;
}

//==============================================================================
//  LANES
static inline float easeSeg (float av, float at, float bv, float bt, int e, double t)
{
    if (e == 0) return av;
    double u = (t - at) / std::max (1e-6, (double) (bt - at));
    u = clampd (u, 0.0, 1.0);
    if (e == 2) u = u * u * (3.0 - 2.0 * u);
    return av + (float) ((bv - av) * u);
}

float Lane::evalOn (double t) const
{
    if (on.empty()) return 0;
    if (hasLoop && loopB > loopA + 1e-3f && t >= loopA)
        t = loopA + std::fmod (t - loopA, (double) (loopB - loopA));
    if (t <= on[0].t) return on[0].v;
    for (size_t i = 0; i + 1 < on.size(); ++i)
        if (t < on[i + 1].t) return easeSeg (on[i].v, on[i].t, on[i + 1].v, on[i + 1].t, on[i + 1].e, t);
    return on.back().v;
}

float Lane::evalOff (double t, float vRel) const
{
    if (off.empty()) return vRel;
    if (t <= off[0].t) return easeSeg (vRel, 0.0f, off[0].v, off[0].t, off[0].e, t);
    for (size_t i = 0; i + 1 < off.size(); ++i)
        if (t < off[i + 1].t) return easeSeg (off[i].v, off[i].t, off[i + 1].v, off[i + 1].t, off[i + 1].e, t);
    return off.back().v;
}

//==============================================================================
//  ENVELOPES — RC-shaped, retrigger from the current level
float Engine::Env::step (float a, float d, float s, float r, float dt)
{
    switch (stage)
    {
        case 1: { const float k = 1.0f - std::exp (-dt / std::max (0.0005f, a * 0.35f));
                  y += (1.06f - y) * k; if (y >= 1.0f) { y = 1.0f; stage = 2; } break; }
        case 2: { const float k = 1.0f - std::exp (-dt / std::max (0.0005f, d * 0.4f));
                  y += (s - y) * k; if (std::abs (y - s) < 1e-4f) { y = s; stage = 3; } break; }
        case 3: y = s; break;
        case 4: { const float k = 1.0f - std::exp (-dt / std::max (0.0005f, r * 0.4f));
                  y += (0.0f - y) * k; if (y < 1e-4f) { y = 0; stage = 0; } break; }
        default: y = 0; break;
    }
    return y;
}

//==============================================================================
Engine::Engine()
{
    osL.resize ((size_t) TICK * 4 + 8); osR.resize ((size_t) TICK * 4 + 8);
    scopeRing.assign (4096, 0.0f);
    for (int k = 0; k < 2; ++k) hbInit (hb[k]);
    reset();
}

void Engine::hbInit (HB& s)
{
    //  47-tap halfband, Kaiser beta 9
    const int M = 23;
    s.h.assign ((size_t) (2 * M + 1), 0.0f);
    auto I0 = [] (double x) { double sum = 1, term = 1; for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; if (term < 1e-12 * sum) break; } return sum; };
    const double beta = 9.0, den = I0 (beta);
    for (int n = -M; n <= M; ++n)
    {
        double v;
        if (n == 0) v = 0.5;
        else if ((n & 1) == 0) v = 0.0;
        else v = std::sin (PI * n / 2.0) / (PI * n);
        const double w = I0 (beta * std::sqrt (std::max (0.0, 1.0 - (double) (n * n) / (double) (M * M)))) / den;
        s.h[(size_t) (n + M)] = (float) (v * w);
    }
    double sum = 0; for (float c : s.h) sum += c;
    for (float& c : s.h) c = (float) (c / sum);
    s.zL.assign (s.h.size(), 0.0f); s.zR.assign (s.h.size(), 0.0f); s.idx = 0;
}

/*  Push one input; the dot product is only formed when `out` is true (every
    second push), and only over the taps that are not zero — a halfband's
    even taps are, bar the centre. */
inline float Engine::hbPush (HB& s, float in, bool right, bool out)
{
    std::vector<float>& z = right ? s.zR : s.zL;
    const int n = (int) s.h.size();
    z[(size_t) s.idx] = in;
    float acc = 0;
    if (out)
    {
        const int M = (n - 1) / 2;
        //  centre tap: the sample M pushes ago
        int kc = s.idx - M; if (kc < 0) kc += n;
        acc = s.h[(size_t) M] * z[(size_t) kc];
        for (int t = 1; t <= M; t += 2)
        {
            int ka = kc - t; if (ka < 0) ka += n;
            int kb = kc + t; if (kb >= n) kb -= n;
            acc += s.h[(size_t) (M - t)] * (z[(size_t) ka] + z[(size_t) kb]);
        }
    }
    if (right) { if (++s.idx >= n) s.idx = 0; }
    return acc;
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v = Voice();
    tNow = 0; nextId = 1; bendSemis = 0; sustain = false; monoN = 0;
    for (int k = 0; k < 2; ++k) { std::fill (hb[k].zL.begin(), hb[k].zL.end(), 0.0f); std::fill (hb[k].zR.begin(), hb[k].zR.end(), 0.0f); hb[k].idx = 0; }
    outDcX[0] = outDcX[1] = outDcY[0] = outDcY[1] = 0;
    std::fill (scopeRing.begin(), scopeRing.end(), 0.0f); scopeRingW = 0;
    viewN = 0;
}

void Engine::setTransport (double bpm, double ppq, bool playing)
{
    bpmNow = (bpm > 1 && bpm < 999) ? bpm : 120.0; ppqNow = ppq; playingNow = playing;
}

void Engine::setLanes (const Lanes& l)
{
    Lanes s = l;
    auto sortLane = [] (Lane& ln)
    {
        std::sort (ln.on.begin(),  ln.on.end(),  [] (const LanePoint& a, const LanePoint& b) { return a.t < b.t; });
        std::sort (ln.off.begin(), ln.off.end(), [] (const LanePoint& a, const LanePoint& b) { return a.t < b.t; });
        for (auto& q : ln.on)  { q.v = clamp01 (q.v); if (q.t < 0) q.t = 0; }
        for (auto& q : ln.off) { q.v = clamp01 (q.v); if (q.t < 0) q.t = 0; }
        if (ln.loopB < ln.loopA) std::swap (ln.loopA, ln.loopB);
    };
    sortLane (s.pin); sortLane (s.tide); sortLane (s.rock);
    while (laneLock.exchange (1)) {}
    lanePending = s; lanePendingFlag.store (true);
    laneLock.store (0);
}

void Engine::setWorldMod (float detCents, float sag01, float tremDepth,
                          float tremRate, float filterMul, float panSpread)
{
    wmDet = detCents; wmSag = sag01; wmTremD = tremDepth; wmTremR = tremRate;
    wmFilt = filterMul; wmSpread = panSpread;
    wmActive = ! (detCents == 0.0f && sag01 == 0.0f && tremDepth == 0.0f
                  && std::abs (filterMul - 1.0f) < 1.0e-9f && panSpread == 0.0f);
}

double Engine::noteHz (int note) const { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }

//==============================================================================
//  NOTES
int Engine::allocVoice (int note)
{
    for (int i = 0; i < MAXVOICES; ++i) if (voices[i].active && voices[i].note == note && voices[i].held) return i;
    for (int i = 0; i < MAXVOICES; ++i) if (! voices[i].active) return i;
    int best = -1; double bt = 1e30;
    for (int i = 0; i < MAXVOICES; ++i) if (voices[i].released && voices[i].tOff < bt) { bt = voices[i].tOff; best = i; }
    if (best >= 0) return best;
    bt = 1e30;
    for (int i = 0; i < MAXVOICES; ++i) if (voices[i].tOn < bt) { bt = voices[i].tOn; best = i; }
    return best < 0 ? 0 : best;
}

void Engine::pullLanes()
{
    if (! lanePendingFlag.load()) return;
    while (laneLock.exchange (1)) {}
    laneSet = lanePending; lanePendingFlag.store (false);
    laneLock.store (0);
}

/*  retrigger: keep the timeline running (legato, mono fallback);
    kick: add strike energy (a key was pressed — not a fall-back to a held note);
    glide: slide the pitch from where it was. */
void Engine::startVoice (Voice& v, int note, float vel, bool retrigger, bool kick, bool glide)
{
    const bool wasActive = v.active;
    //  full scale: STRIKE 1 at full velocity puts E = 1/2 into the reference bowl (x reaches 1)
    const float eFull = 0.5f * p.strike * p.strike;
    const float e = eFull * (1.0f - p.velSens * (1.0f - vel * vel));

    if (! retrigger || ! wasActive)
    {
        v.id = nextId++;
        v.tOn = tNow; v.age = 0; v.ageOff = 0;
        v.released = false; v.sustained = false;
        v.amp = Env(); v.mod = Env();
        v.lfoPh[0] = v.lfoPh[1] = 0; v.lfoSH[0] = v.lfoSH[1] = 0;
        v.seed = (uint32_t) v.id * 2654435761u + 12345u;
        v.laneRel[0] = v.laneRel[1] = v.laneRel[2] = 0;
        v.gateSm = wasActive ? v.gateSm : 0.0f;
        v.tideSm = p.tide;
        v.dropped = false;
        //  a fresh ball starts in the valley it is pointed at
        const float z0 = laneSet.pin.empty() ? p.position : laneSet.pin.evalOn (0.0);
        if (! wasActive) { v.z = z0; v.vz = 0; }
        v.zt = z0;
        v.amp.gate (true); v.mod.gate (true);
    }
    else
    {
        v.amp.gate (true); v.mod.gate (true);
        v.released = false; v.sustained = false;
    }

    v.active = true; v.held = true;
    v.note = note; v.vel = vel;
    const double fTarget = noteHz (note);
    if (glide && wasActive && p.glide > 0.001f)
    {
        v.fGlideFrom = v.fHz; v.glideT = 0; v.glideLen = 0.005 * std::pow (600.0, (double) p.glide);
    }
    else { v.fGlideFrom = fTarget; v.glideT = 1; v.glideLen = 0; }
    v.fHz = fTarget;
    //  the level of detail: blur the terrain to about the ball's step at this pitch
    v.lod = Terrain::levelForStep (2.0 * PI * fTarget / (sr * os) * 1.1 / (double) DX);

    const int nb = 1 + (int) std::lround (p.unison * 3.0f);
    if (! wasActive || ! retrigger) v.nBalls = nb < 1 ? 1 : (nb > MAXBALLS ? MAXBALLS : nb);
    const float xf = terr.floorAt ((float) v.z);
    const float uFloor = terr.reliefAt ((float) v.z);
    for (int b = 0; b < v.nBalls; ++b)
    {
        Ball& bl = v.balls[b];
        const float fan = (float) (std::fmod (b * 0.618033988749895 + 0.5, 1.0) * 2.0 - 1.0);
        bl.eScale = 1.0f + p.spread * 0.3f * fan;
        const double eb = std::max (1e-6, (double) e * bl.eScale);
        const double vKick = std::sqrt (2.0 * eb) * ((b & 1) ? -1.0 : 1.0);
        if (! wasActive || ! retrigger)
        {
            bl.x = xf + 0.02 * fan; bl.v = vKick; bl.a = 0; bl.uxx = 0;
            bl.clock = 1.0; bl.tSinceTurn = 0; bl.periodMeas = 2.0 * PI; bl.vPrev = bl.v; bl.turnFrac = 0;
            bl.rockPh = 0; bl.xMin = bl.xMax = bl.x; bl.lp = 0; bl.dcX = bl.dcY = 0;
            bl.tremPh = (float) std::fmod (b * 0.618033988749895, 1.0);
        }
        else if (kick)
        {
            //  a restrike adds momentum to what is there — capped at a hard full-scale hit
            bl.v += (bl.v >= 0 ? 1.0 : -1.0) * std::sqrt (2.0 * eb);
            float U, Ux, Uz; terr.sample ((float) bl.x, (float) v.z, U, Ux, Uz);
            const double E = 0.5 * bl.v * bl.v + (U - uFloor);
            const double cap = 0.5 * 1.2 * bl.eScale;
            if (E > cap)
            {
                const double kin = std::max (0.0, cap - (U - uFloor));
                bl.v = (bl.v >= 0 ? 1.0 : -1.0) * std::sqrt (2.0 * kin);
            }
        }
        bl.pan = p.width * fan;
    }
}

void Engine::noteOn (int note, float vel)
{
    pullLanes();
    const int mode = listIndex (paramSpec (PI_().voiceMode), p.voiceMode);
    if (mode == 0)
    {
        Voice& v = voices[allocVoice (note)];
        startVoice (v, note, vel, false, true, false);
        return;
    }
    //  MONO / LEGATO: one voice, a stack of held notes
    if (monoN < 32) monoStack[monoN++] = note;
    Voice& v = voices[0];
    const bool overlapping = v.active && v.held;
    if (mode == 2)
    {
        //  legato: an overlapping note only moves the pitch; the timeline runs on
        if (overlapping) startVoice (v, note, vel, true, false, true);
        else             startVoice (v, note, vel, false, true, true);
    }
    else
    {
        //  mono: every key restrikes and restarts the timeline; the pitch glides
        startVoice (v, note, vel, false, true, true);
    }
}

void Engine::noteOff (int note)
{
    const int mode = listIndex (paramSpec (PI_().voiceMode), p.voiceMode);
    if (mode != 0)
    {
        for (int i = 0; i < monoN; ++i) if (monoStack[i] == note) { for (int k = i; k + 1 < monoN; ++k) monoStack[k] = monoStack[k + 1]; --monoN; break; }
        Voice& v = voices[0];
        if (! v.active || v.note != note) return;
        if (monoN > 0)
        {
            //  fall back to the note still held: no new strike, just the pitch
            const int back = monoStack[monoN - 1];
            startVoice (v, back, v.vel, true, false, true);
            return;
        }
    }
    for (auto& v : voices)
        if (v.active && v.note == note && v.held)
        {
            if (sustain) { v.sustained = true; v.held = false; continue; }
            v.held = false; v.released = true; v.tOff = tNow; v.ageOff = 0;
            for (int k = 0; k < 3; ++k) v.laneRel[k] = v.laneNow[k];
            v.amp.gate (false); v.mod.gate (false);
        }
}

void Engine::allNotesOff()
{
    monoN = 0;
    for (auto& v : voices) v = Voice();
}

void Engine::setBend (float semis) { bendSemis = semis; }

void Engine::setSustain (bool on)
{
    sustain = on;
    if (! on)
        for (auto& v : voices)
            if (v.active && v.sustained)
            {
                v.sustained = false; v.released = true; v.tOff = tNow; v.ageOff = 0;
                for (int k = 0; k < 3; ++k) v.laneRel[k] = v.laneNow[k];
                v.amp.gate (false); v.mod.gate (false);
            }
}

void Engine::drop (float z, float e, int note, bool on)
{
    if (! on) { noteOff (note); return; }
    pullLanes();
    Voice& v = voices[allocVoice (note)];
    const float vel = clamp01 (std::sqrt (std::max (0.0f, e)));
    startVoice (v, note, vel, false, true, false);
    v.dropped = true; v.dropZ = clamp01 (z);
    v.z = v.dropZ; v.vz = 0; v.zt = v.dropZ;
    const float xf = terr.floorAt ((float) v.z);
    for (int b = 0; b < v.nBalls; ++b) v.balls[b].x = xf + 0.02 * (b - 1);
}

//==============================================================================
//  MODULATORS
float Engine::lfoValue (Voice& v, int which, double dtReal, double bpm, double ppq, bool playing)
{
    const PSpec& sSync  = paramSpec (which ? PI_().lfo2Sync  : PI_().lfo1Sync);
    const PSpec& sShape = paramSpec (which ? PI_().lfo2Shape : PI_().lfo1Shape);
    const PSpec& sRate  = paramSpec (which ? PI_().lfo2Rate  : PI_().lfo1Rate);
    const int sync  = listIndex (sSync,  which ? p.lfo2Sync  : p.lfo1Sync);
    const int shape = listIndex (sShape, which ? p.lfo2Shape : p.lfo1Shape);
    static const double beats[] = { 0, 16, 8, 4, 2, 1, 0.5, 0.25, 1.0 / 3.0, 1.0 / 6.0 };
    double& ph = v.lfoPh[which];
    const double before = ph;
    if (sync > 0)
    {
        const double b = beats[sync];
        if (playing) ph = std::fmod (ppq / b, 1.0);
        else ph = std::fmod (ph + dtReal * (bpm / 60.0) / b, 1.0);
    }
    else
    {
        const double hz = hzOf (sRate, which ? p.lfo2Rate : p.lfo1Rate);
        ph = std::fmod (ph + dtReal * hz, 1.0);
    }
    if (ph < 0) ph += 1.0;
    if (shape == 4 && (ph < before || before == 0.0))
    {
        v.seed = v.seed * 1664525u + 1013904223u;
        v.lfoSH[which] = (float) ((v.seed >> 8) & 0xffff) / 32767.5f - 1.0f;
    }
    switch (shape)
    {
        case 0: return (float) std::sin (2.0 * PI * ph);
        case 1: return (float) (1.0 - 4.0 * std::abs (ph - 0.5));
        case 2: return (float) (2.0 * ph - 1.0);
        case 3: return ph < 0.5 ? 1.0f : -1.0f;
        default: return v.lfoSH[which];
    }
}

void Engine::applyMod (Voice&, int target, float amount, float& zTarget, float& tide,
                       float& rock, float& hold, float& tone, float& pitchSemi)
{
    switch (target)
    {
        case 0: zTarget += amount; break;
        case 1: tide += amount; break;
        case 2: rock += amount; break;
        case 3: hold += amount; break;
        case 4: tone += amount; break;
        default: pitchSemi += amount * 12.0f; break;
    }
}

//==============================================================================
//  THE CONTROL TICK — lanes, envelopes, LFOs, the slow z coordinate
void Engine::tick (Voice& v, double dtReal, double bpm, double ppq, bool playing)
{
    v.age += dtReal;
    if (v.released) v.ageOff += dtReal;

    //  the lanes
    float pinV, tideV, rockV;
    if (! v.released)
    {
        pinV  = laneSet.pin.empty()  ? p.position : laneSet.pin.evalOn (v.age);
        tideV = laneSet.tide.empty() ? p.tide     : laneSet.tide.evalOn (v.age);
        rockV = laneSet.rock.empty() ? p.rock     : laneSet.rock.evalOn (v.age);
    }
    else
    {
        pinV  = laneSet.pin.empty()  ? p.position : laneSet.pin.evalOff (v.ageOff, v.laneRel[0]);
        tideV = laneSet.tide.empty() ? p.tide     : laneSet.tide.evalOff (v.ageOff, v.laneRel[1]);
        rockV = laneSet.rock.empty() ? p.rock     : laneSet.rock.evalOff (v.ageOff, v.laneRel[2]);
    }
    v.laneNow[0] = pinV; v.laneNow[1] = tideV; v.laneNow[2] = rockV;

    float zTarget = v.dropped ? v.dropZ : pinV;
    float tide = tideV, rock = rockV, hold = p.hold, tone = p.tone, pitchSemi = 0;

    //  the mod envelope and the two LFOs
    const float m = v.mod.step (secondsOf (p.modA), secondsOf (p.modD), p.modS, secondsOf (p.modR), (float) dtReal);
    applyMod (v, listIndex (paramSpec (PI_().modTarget), p.modTarget), (p.modAmt * 2 - 1) * m,
              zTarget, tide, rock, hold, tone, pitchSemi);
    const float l1 = lfoValue (v, 0, dtReal, bpm, ppq, playing);
    applyMod (v, listIndex (paramSpec (PI_().lfo1Target), p.lfo1Target), (p.lfo1Amt * 2 - 1) * l1,
              zTarget, tide, rock, hold, tone, pitchSemi);
    const float l2 = lfoValue (v, 1, dtReal, bpm, ppq, playing);
    applyMod (v, listIndex (paramSpec (PI_().lfo2Target), p.lfo2Target), (p.lfo2Amt * 2 - 1) * l2,
              zTarget, tide, rock, hold, tone, pitchSemi);
    tide += modWheel * 0.5f;      // the wheel lifts the tide

    zTarget = clamp01 (zTarget); tide = clamp01 (tide); rock = clamp01 (rock);
    hold = clamp01 (hold); tone = clamp01 (tone);
    v.zt = zTarget;
    v.tideSm += (tide - v.tideSm) * (float) (1.0 - std::exp (-dtReal / 0.02));
    v.tideEff = v.tideSm; v.rockEff = rock;

    //  the gate, smoothed: what the SPECTRA sag is keyed to
    v.gateSm += (((v.held || v.sustained) ? 1.0f : 0.0f) - v.gateSm) * (float) (1.0 - std::exp (-dtReal / 0.05));

    //  the slow coordinate: tether, relief, tide
    const double uzAvg = v.uzN > 0 ? v.uzAcc / v.uzN : 0.0;
    v.uzAcc = 0; v.uzN = 0;
    const float  Tabs = terr.tideAbs (v.tideEff);
    const float  Rz   = terr.reliefAt ((float) v.z);
    const float  Rs   = terr.reliefSlopeAt ((float) v.z);
    const double uzEff = uzAvg - (Rz <= Tabs ? (double) Rs : 0.0);
    const double kt = (double) hold * hold * 500.0;
    const double gam = 2.0 * std::sqrt (kt) + 8.0;
    const double KZ = 14.0;
    double Fz = -KZ * uzEff + kt * (zTarget - v.z) - gam * v.vz;
    v.vz += Fz * dtReal;
    v.z  += v.vz * dtReal;
    if (v.z < 0) { v.z = 0; v.vz = -v.vz * 0.3; }
    if (v.z > 1) { v.z = 1; v.vz = -v.vz * 0.3; }

    //  pitch: glide, tune, bend, modulation
    double fBase = v.fHz;
    if (v.glideT < 1.0)
    {
        v.glideT = v.glideLen > 0 ? std::min (1.0, v.glideT + dtReal / v.glideLen) : 1.0;
        fBase = v.fGlideFrom * std::pow (v.fHz / v.fGlideFrom, v.glideT);
    }
    const double semis = (p.tune * 2.0 - 1.0) * 12.0 + bendSemis + pitchSemi;
    double f = fBase * std::pow (2.0, semis / 12.0);
    if (wmActive && wmSag > 0.0f) f *= std::pow (2.0, -(double) wmSag * (1.0 - v.gateSm) / 12.0);
    if (v.glideT >= 1.0) v.fGlideFrom = v.fHz;

    //  the radiation lowpass, the pans and the clocks, per ball — everything the
    //  render needs that does not change within a tick
    const float fc = 200.0f * std::pow (100.0f, tone) * (wmActive ? wmFilt : 1.0f);
    const float lpA = 1.0f - std::exp (-2.0f * (float) PI * std::min (fc, (float) (sr * os) * 0.45f) / (float) (sr * os));
    const float uFloorZ = terr.reliefAt ((float) v.z);
    for (int b = 0; b < v.nBalls; ++b)
    {
        Ball& bl = v.balls[b];
        const float fan = (float) (std::fmod (b * 0.618033988749895 + 0.5, 1.0) * 2.0 - 1.0);
        bl.pan = clamp01 (0.5f + 0.5f * (p.width * fan + (wmActive ? wmSpread * fan : 0.0f))) * 2.0f - 1.0f;
        bl.gl = std::cos ((bl.pan + 1.0f) * 0.25f * (float) PI);
        bl.gr = std::sin ((bl.pan + 1.0f) * 0.25f * (float) PI);
        /*  DETUNE and the SPECTRA bus are one mechanism: each ball's own clock
            runs a little fast or slow, fanned by the golden angle so ball 0
            (whose fan is exactly 0) always plays the note that was asked for.
            At zero the exponent is exactly 0 and pow(2,0) is exactly 1.0, so an
            un-detuned unison stays bit-identical to what it was. */
        const double detCents = (double) p.detune * 50.0 + (wmActive ? (double) wmDet : 0.0);
        bl.detMul = detCents == 0.0 ? 1.0 : std::pow (2.0, detCents * fan / 1200.0);
        /*  BREATH is a pump with finite power: its grip falls as the ball's
            energy rises, so a parametric drive settles at a level instead of
            growing to the edge of the world (which an isochronous bowl would
            otherwise let it do — the SEICHE patch measured at the ceiling). */
        float U, Ux, Uz; terr.sample ((float) bl.x, (float) v.z, U, Ux, Uz);
        const double E = 0.5 * bl.v * bl.v + std::max (0.0f, U - uFloorZ);
        bl.breathGain = (double) v.rockEff * 0.6 / (1.0 + 5.0 * E);
    }
    //  the amp envelope is stepped here; the render ramps to it
    v.amp.step (secondsOf (p.ampA), secondsOf (p.ampD), p.ampS, secondsOf (p.ampR), (float) dtReal);
    fRender[&v - voices] = f;
    lpRender[&v - voices] = lpA;
}

//==============================================================================
//  THE RENDER — Newton at audio rate
void Engine::renderVoice (Voice& v, float* L, float* R, int nOs, double dtRealOs)
{
    const int vi = (int) (&v - voices);
    const double f = fRender[vi];
    const float  lpA = lpRender[vi];
    const double dtBase = 2.0 * PI * f * dtRealOs;              // ball time per oversampled sample
    const float  gTarget = v.amp.y;
    float  g = ampRender[vi];
    const float gK = 1.0f - std::exp (-(float) dtRealOs / 0.0012f);

    //  damping per oversampled sample
    const float fr = p.friction, rl = p.release;
    const double gamma = (v.held || v.sustained) ? 12.0 * std::pow ((double) fr, 2.2) : 25.0 * std::pow ((double) rl, 2.5) + 0.1;
    const double damp = std::exp (-gamma * dtRealOs);

    //  the drive
    const int   rMode  = listIndex (paramSpec (PI_().rockMode), p.rockMode);
    const int   rRatio = listIndex (paramSpec (PI_().rockRatio), p.rockRatio);
    static const double ratios[] = { 1.0, 2.0, 0.5, 1.0 / 3.0, 1.5, 3.0, 0.0 };
    const double rockAmp = (double) v.rockEff * v.rockEff * 1.3;
    const bool   rocking = v.rockEff > 0.0f;
    double dph;
    if (rRatio == 6) dph = 2.0 * PI * hzOf (paramSpec (PI_().rockHz), p.rockHz) * dtRealOs;
    else dph = ratios[rRatio] * dtBase;

    const float  xf  = terr.floorAt ((float) v.z);
    const int    nb  = v.nBalls;
    const float  bgain = 0.5f / std::sqrt ((float) nb);
    const float  tPos = p.tapPos, tVel = p.tapVel * 0.8f, tFrc = p.tapFrc * 0.7f;
    const float  dcA = 1.0f - 2.0f * (float) PI * 5.0f / (float) (sr * os);
    const float  z = (float) v.z;
    /*  the balls' mutual push: WEAK and LONG-RANGE. A hard short-range
        collision at every crossing (they cross twice a cycle) turned unison
        into a brawl — measured 1254 cents of "detune" in a parabola. */
    /*  Halved once DETUNE existed: a push between the balls shifts each
        one's period a little, and the shift depends on their energy — which
        made a wide-unison patch drift with the strike (6.8 cents at SPREAD
        0.45, measured). Separating the balls is DETUNE's job now; this only
        stops them sitting exactly on top of one another. */
    const double repK = nb > 1 ? (double) p.spread * 0.01 : 0.0;
    const float  tremR = wmTremR, tremD = wmTremD;
    const float  tremInc = (float) (tremR * dtRealOs);
    const bool   trem = wmActive && tremD > 0.0f;
    const bool   servoOn = p.servo > 0.0005f;
    const double servoG = (double) p.servo;
    const double wallHi = XMAX - 0.06, wallLo = XMIN + 0.06;
    const int    lod = v.lod;
    const double dispCap = std::max (0.018, 0.5 * Terrain::sigmaOf (lod) * (double) DX);

    for (int b = 0; b < nb; ++b) { v.balls[b].xMin = 1e9; v.balls[b].xMax = -1e9; }
    double rep[MAXBALLS] = { 0, 0, 0, 0 };

    for (int n = 0; n < nOs; ++n)
    {
        float mono = 0;
        if (repK > 0 && (n & 3) == 0)
        {
            rep[0] = rep[1] = rep[2] = rep[3] = 0;
            for (int a = 0; a < nb; ++a)
                for (int c = a + 1; c < nb; ++c)
                {
                    const double d = v.balls[a].x - v.balls[c].x;
                    const double fr2 = repK * (d >= 0 ? 1.0 : -1.0) * std::exp (-std::abs (d) / 0.25);
                    rep[a] += fr2; rep[c] -= fr2;
                }
        }

        for (int b = 0; b < nb; ++b)
        {
            Ball& bl = v.balls[b];
            const double dt = dtBase * bl.clock * bl.detMul;
            /*  velocity Verlet, one force evaluation per step — and where the
                terrain is STIFF (a hard wall, a narrow pit, the edge of the
                world) the step is subdivided so that omega*dt stays under 0.12.
                Without this the outcome of a wall bounce depends on the phase
                of the sample grid, the period jitters, and a box bowl reads as
                +18 dB of non-harmonic energy: not aliasing, unresolved
                dynamics. The budget is spent only where the stiffness is. */
            int nSub = 1;
            {
                /*  Decide the budget from where the ball is GOING, not where it
                    was: a step decided from the flat floor jumped clean into a
                    wall (or over a pit narrower than the step) before any
                    sub-stepping engaged. Two bounds: omega*dt at the look-ahead
                    point, and the displacement per step — no more than three
                    cells, so a feature at the brush floor (seven cells) is
                    always seen. */
                const double xAhead = bl.x + bl.v * dt;
                const float uxxAhead = terr.stiffnessAt ((float) xAhead, z, lod);
                const float uxx = std::max (bl.uxx, uxxAhead);
                if (uxx > 0.0f)
                {
                    const double wdt = std::sqrt ((double) uxx) * dt;
                    if (wdt > 0.10) nSub = std::min (12, 1 + (int) (wdt / 0.10));
                }
                //  no more than the level's blur per sub-step
                const int nDisp = 1 + (int) (std::abs (bl.v) * dt / dispCap);
                if (nDisp > nSub) nSub = std::min (12, nDisp);
            }
            const double dts = dt / nSub, dphs = dph / nSub;
            float U = 0, Ux = 0, Uz = 0, Uxx = 0;
            for (int s = 0; s < nSub; ++s)
            {
                bl.v += 0.5 * bl.a * dts;
                bl.x += bl.v * dts;
                terr.sample ((float) bl.x, z, U, Ux, Uz, Uxx, lod);
                double force = -(double) Ux + rep[b];
                if (rocking)
                {
                    if (rMode == 0) force += rockAmp * std::sin (bl.rockPh);
                    else            force *= 1.0 + bl.breathGain * std::sin (bl.rockPh);
                    bl.rockPh += dphs; if (bl.rockPh > 2.0 * PI) bl.rockPh -= 2.0 * PI;
                }
                //  the edge of the world, a stiff wall
                if (bl.x > wallHi)      { force -= (bl.x - wallHi) * 400.0; Uxx += 400.0f; }
                else if (bl.x < wallLo) { force -= (bl.x - wallLo) * 400.0; Uxx += 400.0f; }
                bl.a = force;
                bl.v += 0.5 * force * dts;
            }
            bl.uxx = Uxx;
            v.uzAcc += Uz; ++v.uzN;
            bl.v *= damp;

            //  the period, from left turning points, to a fraction of a step
            bl.tSinceTurn += dt;
            if (bl.vPrev < 0 && bl.v >= 0)
            {
                const double frac = bl.vPrev / (bl.vPrev - bl.v);      // where in the step it crossed
                bl.periodMeas = bl.tSinceTurn + dt * (frac - bl.turnFrac);
                bl.turnFrac = frac; bl.tSinceTurn = 0;
                if (servoOn && bl.periodMeas > 0.3 && bl.periodMeas < 60.0)
                {
                    /*  the measured period is in BALL time, so it is the bowl's own
                        and does not move with the clock: the clock must SETTLE on
                        period/2pi, not be multiplied by it every cycle (which ran to
                        the clamp — measured +489 cents). */
                    const double target = bl.periodMeas / (2.0 * PI);
                    bl.clock = clampd (std::pow (target, servoG) * std::pow (bl.clock, 1.0 - servoG), 0.125, 8.0);
                }
            }
            bl.vPrev = bl.v;
            if (bl.x < bl.xMin) bl.xMin = bl.x;
            if (bl.x > bl.xMax) bl.xMax = bl.x;

            //  the three taps
            const float pos = (float) bl.x - xf;
            const float dcOut = pos - bl.dcX + dcA * bl.dcY;
            bl.dcX = pos; bl.dcY = dcOut;
            float s = tPos * dcOut + tVel * (float) bl.v + tFrc * (-Ux);
            bl.lp += (s - bl.lp) * lpA;
            s = bl.lp * bgain;
            if (trem)
            {
                bl.tremPh += tremInc; if (bl.tremPh > 1.0f) bl.tremPh -= 1.0f;
                s *= 1.0f - tremD * 0.5f * (1.0f - std::cos (2.0f * (float) PI * bl.tremPh));
            }
            g += (gTarget - g) * gK;
            s *= g;
            mono += s;
            L[n] += s * bl.gl; R[n] += s * bl.gr;
        }
        v.outMono = mono;
        if (vi == scopeVoice && (n % os) == 0)
        {
            scopeRing[(size_t) scopeRingW] = mono;
            if (++scopeRingW >= (int) scopeRing.size()) scopeRingW = 0;
        }
    }
    ampRender[vi] = g;
}

//==============================================================================
void Engine::process (float* L, float* R, int n)
{
    //  lanes swapped whole at a block edge
    pullLanes();
    const int wantOs = listIndex (paramSpec (PI_().quality), p.quality) == 1 ? 4 : 2;
    if (wantOs != os)
    {
        os = wantOs;
        for (int k = 0; k < 2; ++k) { std::fill (hb[k].zL.begin(), hb[k].zL.end(), 0.0f); std::fill (hb[k].zR.begin(), hb[k].zR.end(), 0.0f); hb[k].idx = 0; }
    }
    const double dtRealOs = 1.0 / (sr * os);

    //  the newest voice feeds the scope
    {
        int best = -1, bid = -1;
        for (int i = 0; i < MAXVOICES; ++i) if (voices[i].active && voices[i].id > bid) { bid = voices[i].id; best = i; }
        if (best >= 0) scopeVoice = best;
    }

    const float thr = 0.5f + 0.45f * p.ceiling;
    const float lvl = 2.4f * p.level * p.level;
    const float dcA = 1.0f - 2.0f * (float) PI * 5.0f / (float) sr;
    float peak = 0;

    int done = 0;
    while (done < n)
    {
        const int nb = std::min (TICK, n - done);
        const int nOs = nb * os;
        std::fill (osL.begin(), osL.begin() + nOs, 0.0f);
        std::fill (osR.begin(), osR.begin() + nOs, 0.0f);

        for (int i = 0; i < MAXVOICES; ++i)
        {
            Voice& v = voices[i];
            if (! v.active) continue;
            if (nb == TICK || v.uzN == 0) tick (v, (double) nb / sr, bpmNow, ppqNow, playingNow);
            renderVoice (v, osL.data(), osR.data(), nOs, dtRealOs);
            //  a voice ends when its envelope has closed
            if (v.released && v.amp.stage == 0) v.active = false;
        }

        //  master: ceiling, decimation, level
        for (int k = 0; k < nOs; ++k)
        {
            float l = osL[(size_t) k], r = osR[(size_t) k];
            auto ceil = [thr] (float x)
            {
                const float ax = std::abs (x);
                if (ax <= thr) return x;
                const float over = (ax - thr) / (1.0f - thr + 1e-6f);
                const float y = thr + (1.0f - thr) * std::tanh (over);
                return x < 0 ? -y : y;
            };
            l = ceil (l); r = ceil (r);
            const bool o1 = (k & 1) == 1;
            if (os == 4)
            {
                const float dl = hbPush (hb[0], l, false, o1), dr = hbPush (hb[0], r, true, o1);
                if (o1)
                {
                    const bool o2 = (k & 3) == 3;
                    const float el = hbPush (hb[1], dl, false, o2), er = hbPush (hb[1], dr, true, o2);
                    if (o2) { const int o = done + k / 4; L[o] = el; R[o] = er; }
                }
            }
            else
            {
                const float dl = hbPush (hb[0], l, false, o1), dr = hbPush (hb[0], r, true, o1);
                if (o1) { const int o = done + k / 2; L[o] = dl; R[o] = dr; }
            }
        }
        for (int k = 0; k < nb; ++k)
        {
            const int o = done + k;
            float l = L[o], r = R[o];
            const float yl = l - outDcX[0] + dcA * outDcY[0]; outDcX[0] = l; outDcY[0] = yl;
            const float yr = r - outDcX[1] + dcA * outDcY[1]; outDcX[1] = r; outDcY[1] = yr;
            L[o] = yl * lvl; R[o] = yr * lvl;
            if (L[o] > 1.0f) L[o] = 1.0f; else if (L[o] < -1.0f) L[o] = -1.0f;
            if (R[o] > 1.0f) R[o] = 1.0f; else if (R[o] < -1.0f) R[o] = -1.0f;
            peak = std::max (peak, std::max (std::abs (L[o]), std::abs (R[o])));
        }
        done += nb;
        tNow += (double) nb / sr;
    }
    outLvl.store (peak, std::memory_order_relaxed);
    captureView();
}

//==============================================================================
void Engine::captureView()
{
    if (viewLock.exchange (1)) return;    // the page is reading: skip this one
    int k = 0;
    const float eFullRef = 0.5f;
    for (int i = 0; i < MAXVOICES && k < MAXVOICES; ++i)
    {
        const Voice& v = voices[i];
        if (! v.active) continue;
        BallView& o = view[k++];
        o.id = v.id; o.note = v.note; o.held = (v.held || v.sustained) ? 1 : 0;
        o.age = (float) (v.released ? v.age : v.age);
        o.z = (float) v.z; o.zt = (float) v.zt;
        const Ball& b0 = v.balls[0];
        o.x = (float) b0.x; o.xa = (float) (b0.xMax > b0.xMin ? 0.5 * (b0.xMax - b0.xMin) : 0.0);
        float U, Ux, Uz; terr.sample ((float) b0.x, (float) v.z, U, Ux, Uz);
        const float E = (float) (0.5 * b0.v * b0.v) + U - terr.reliefAt ((float) v.z);
        o.e = std::min (1.0f, std::max (0.0f, E / eFullRef));
        o.tide = v.tideEff; o.rock = v.rockEff;
        o.nBalls = v.nBalls;
        for (int b = 0; b < v.nBalls; ++b) { o.bx[b] = (float) v.balls[b].x; o.bz[b] = (float) v.z; }
    }
    viewN = k;
    for (int i = 0; i < SCOPE_N; ++i)
    {
        int idx = scopeRingW - SCOPE_N + i; if (idx < 0) idx += (int) scopeRing.size();
        scope[i] = scopeRing[(size_t) idx];
    }
    viewLock.store (0);
}

int Engine::voicesView (BallView* out, int maxOut)
{
    while (viewLock.exchange (1)) {}
    const int n = std::min (maxOut, viewN);
    for (int i = 0; i < n; ++i) out[i] = view[i];
    viewLock.store (0);
    return n;
}

void Engine::scopeView (float* out256)
{
    while (viewLock.exchange (1)) {}
    std::memcpy (out256, scope, sizeof (scope));
    viewLock.store (0);
}

//==============================================================================
//  bench accessors
int Engine::activeVoices() const { int n = 0; for (const auto& v : voices) n += v.active ? 1 : 0; return n; }
double Engine::voiceZ (int v) const       { return v >= 0 && v < MAXVOICES ? voices[v].z : 0; }
double Engine::voiceZTarget (int v) const { return v >= 0 && v < MAXVOICES ? voices[v].zt : 0; }
double Engine::ballX (int v, int b) const { return v >= 0 && v < MAXVOICES && b >= 0 && b < MAXBALLS ? voices[v].balls[b].x : 0; }
double Engine::ballV (int v, int b) const { return v >= 0 && v < MAXVOICES && b >= 0 && b < MAXBALLS ? voices[v].balls[b].v : 0; }
double Engine::ballEnergy (int v, int b) const
{
    if (v < 0 || v >= MAXVOICES || b < 0 || b >= MAXBALLS) return 0;
    const Ball& bl = voices[v].balls[b];
    float U, Ux, Uz; terr.sample ((float) bl.x, (float) voices[v].z, U, Ux, Uz);
    return 0.5 * bl.v * bl.v + U - terr.reliefAt ((float) voices[v].z);
}
double Engine::ballClock (int v, int b) const  { return v >= 0 && v < MAXVOICES && b >= 0 && b < MAXBALLS ? voices[v].balls[b].clock : 1; }
double Engine::ballPeriod (int v, int b) const { return v >= 0 && v < MAXVOICES && b >= 0 && b < MAXBALLS ? voices[v].balls[b].periodMeas : 0; }
int Engine::voiceForNote (int note) const
{
    for (int i = 0; i < MAXVOICES; ++i) if (voices[i].active && voices[i].note == note) return i;
    return -1;
}

} // namespace ht
