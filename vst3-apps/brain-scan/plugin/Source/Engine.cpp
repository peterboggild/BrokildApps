#include "Engine.h"
#include <algorithm>

namespace bs
{

//==============================================================================
//  the parameter table — the single source of truth
static const char* const L_VOICES[]  = { "POLY", "MONO" };
static const char* const L_UNISON[]  = { "1", "2", "3", "4" };
static const char* const L_FTYPE[]   = { "LOW", "BAND", "HIGH", "OFF" };
static const char* const L_FMODE[]   = { "SWEEP", "LOOP" };
static const char* const L_FMODEL[]  = { "SVF", "GROWL", "SCREAM", "LADDER" };
static const char* const L_SYNC[]    = { "FREE", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/8T", "1/16T" };
static const char* L_SPEC[16];
static int nSpecNames = 0;

#define P(m) [] (Params& q) -> float& { return q.m; }
static const PSpec SPECS[] =
{
    { "level",     "LEVEL",      "output level",                                           0.70f, KP_VOL,  0, 0,  P(level),     nullptr, 0 },
    { "voices",    "VOICES",     "polyphonic, or one voice with legato glide",             0.00f, KP_LIST, 0, 1,  P(voiceMode), L_VOICES, 2 },
    { "unison",    "UNISON",     "how many readers walk the same line",                    0.00f, KP_LIST, 0, 3,  P(unison),    L_UNISON, 4 },
    { "detune",    "DETUNE",     "how far the unison readers are spread in pitch",         0.25f, KP_CENT, 0, 40, P(detune),    nullptr, 0 },
    { "spread",    "SPREAD",     "how wide the unison readers sit in the stereo field",    0.50f, KP_PCT,  0, 0,  P(spread),    nullptr, 0 },
    { "glide",     "GLIDE",      "portamento between notes",                               0.00f, KP_SEC,  0, 0,  P(glide),     nullptr, 0 },
    { "tune",      "TUNE",       "master tuning",                                          0.50f, KP_SEMI, 0, 0,  P(tune),      nullptr, 0 },
    { "specimen",  "SPECIMEN",   "the volume the lines read",                              1.0f / 14.0f, KP_LIST, 0, 14, P(specimen), L_SPEC, 15 },
    { "grain",     "GRAIN",      "how sharply the tissue is read: smoothed, or texel by texel", 0.35f, KP_PCT, 0, 0, P(grain),    nullptr, 0 },
    { "contrast",  "CONTRAST",   "the audio window: narrow it and the read saturates, a sine toward a square", 0.00f, KP_PCT, 0, 0, P(contrast), nullptr, 0 },
    { "fold",      "FOLD",       "what the window does past its edges: clip, or fold back in",  0.00f, KP_PCT, 0, 0, P(fold),     nullptr, 0 },
    { "head2",     "2ND HEAD",   "a second reader on the same line, mixed in",                  0.00f, KP_PCT, 0, 0, P(head2),    nullptr, 0 },
    { "head2Ratio","HEAD RATIO", "the second head's speed: an interval, or a partial that is not a harmonic", 0.5f, KP_RATIO, 0, 0, P(head2Ratio), nullptr, 0 },
    { "head2Phase","HEAD PHASE", "where along the cycle the second head starts",                0.00f, KP_PCT, 0, 0, P(head2Phase), nullptr, 0 },
    { "uniScan",   "SCAN SPREAD","unison readers spread through the scan, each a little off the others", 0.00f, KP_PCT, 0, 0, P(uniScan), nullptr, 0 },
    { "modContrast","MOD>WINDOW","how much the MOD line narrows the window",                    0.50f, KP_BIPOL, 0, 0, P(modContrast), nullptr, 0 },
    { "modGrain",  "MOD>GRAIN",  "how much the MOD line sharpens the read",                     0.50f, KP_BIPOL, 0, 0, P(modGrain), nullptr, 0 },
    { "scan",      "SCAN",       "where between A and B every line is read",               0.00f, KP_PCT,  0, 0,  P(scan),      nullptr, 0 },
    { "scanA",     "SCAN ATK",   "how fast the scan envelope rises",                       0.30f, KP_SEC,  0, 0,  P(scanA),     nullptr, 0 },
    { "scanD",     "SCAN DEC",   "how fast it falls back",                                 0.55f, KP_SEC,  0, 0,  P(scanD),     nullptr, 0 },
    { "scanAmt",   "SCAN ENV",   "how far the envelope moves the scan, either way",        0.50f, KP_BIPOL,0, 0,  P(scanAmt),   nullptr, 0 },
    { "ftype",     "FILTER",     "the filter's response",                                  0.00f, KP_LIST, 0, 3,  P(filtType),  L_FTYPE, 4 },
    { "fmodel",    "CIRCUIT",    "the filter circuit: the state-variable, or Black Rider's GROWL, SCREAM and LADDER", 0.00f, KP_LIST, 0, 3, P(filtModel), L_FMODEL, 4 },
    { "cutoff",    "CUTOFF",     "the cutoff when the FILTER line has no say",             0.70f, KP_PCT,  0, 0,  P(cutoff),    nullptr, 0 },
    { "reso",      "RESONANCE",  "the filter's resonance",                                 0.20f, KP_PCT,  0, 0,  P(reso),      nullptr, 0 },
    { "fdepth",    "F DEPTH",    "how much the FILTER line decides the cutoff",            0.00f, KP_PCT,  0, 0,  P(filtDepth), nullptr, 0 },
    { "fmode",     "F MODE",     "read the FILTER line once per note, or keep looping it", 0.00f, KP_LIST, 0, 1,  P(filtMode),  L_FMODE, 2 },
    { "frate",     "F RATE",     "the sweep time, or the loop rate",                       0.35f, KP_HZ,   0.05f, 40, P(filtRate), nullptr, 0 },
    { "fsync",     "F SYNC",     "lock the loop to the host clock",                        0.00f, KP_LIST, 0, 9,  P(filtSync),  L_SYNC, 10 },
    { "ftrack",    "KEY TRACK",  "how much the cutoff follows the note",                   0.50f, KP_PCT,  0, 0,  P(filtTrack), nullptr, 0 },
    { "mrate",     "MOD RATE",   "how fast the MOD line is read",                          0.30f, KP_HZ,   0.02f, 30, P(modRate),  nullptr, 0 },
    { "msync",     "MOD SYNC",   "lock the MOD line to the host clock",                    0.00f, KP_LIST, 0, 9,  P(modSync),   L_SYNC, 10 },
    { "mscan",     "MOD>SCAN",   "how much the MOD line moves the scan",                   0.50f, KP_BIPOL,0, 0,  P(modScan),   nullptr, 0 },
    { "mpitch",    "MOD>PITCH",  "how much the MOD line bends the pitch",                  0.50f, KP_BIPOL,0, 0,  P(modPitch),  nullptr, 0 },
    { "mpan",      "MOD>PAN",    "how much the MOD line moves the voice in the field",     0.50f, KP_BIPOL,0, 0,  P(modPan),    nullptr, 0 },
    { "ampA",      "ATTACK",     "amplitude attack",                                       0.15f, KP_SEC,  0, 0,  P(ampA),      nullptr, 0 },
    { "ampD",      "DECAY",      "amplitude decay",                                        0.55f, KP_SEC,  0, 0,  P(ampD),      nullptr, 0 },
    { "ampS",      "SUSTAIN",    "amplitude sustain",                                      0.75f, KP_PCT,  0, 0,  P(ampS),      nullptr, 0 },
    { "ampR",      "RELEASE",    "amplitude release",                                      0.45f, KP_SEC,  0, 0,  P(ampR),      nullptr, 0 },
    { "velsens",   "VELOCITY",   "how much velocity decides the level",                    0.60f, KP_PCT,  0, 0,  P(velSens),   nullptr, 0 },
};
#undef P
static const int NSPEC = (int) (sizeof (SPECS) / sizeof (SPECS[0]));

int numParams() { return NSPEC; }
const PSpec& paramSpec (int i)
{
    if (nSpecNames == 0)
    {
        nSpecNames = numSpecimens();
        for (int k = 0; k < nSpecNames && k < 16; ++k) L_SPEC[k] = specimenName (k);
    }
    return SPECS[i < 0 ? 0 : (i >= NSPEC ? NSPEC - 1 : i)];
}
int paramIndex (const char* id)
{
    for (int i = 0; i < NSPEC; ++i) if (std::strcmp (SPECS[i].id, id) == 0) return i;
    return -1;
}
float secondsOf (float v) { return 0.001f * std::pow (10000.0f, v < 0 ? 0 : (v > 1 ? 1 : v)); }   // 1 ms .. 10 s
float hzOf (const PSpec& s, float v)
{
    const float lo = s.lo > 0 ? s.lo : 0.01f, hi = s.hi > lo ? s.hi : 100.0f;
    return lo * std::pow (hi / lo, v < 0 ? 0 : (v > 1 ? 1 : v));
}
int listIndex (const PSpec& s, float v)
{
    const int n = s.nlist > 0 ? s.nlist : 1;
    int i = (int) std::lround (v * (float) (n - 1));
    return i < 0 ? 0 : (i >= n ? n - 1 : i);
}

static inline float clamp01 (float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline double clampd (double v, double a, double b) { return v < a ? a : (v > b ? b : v); }

//==============================================================================
//  lines
static inline Vec3 mix3 (const Vec3& a, const Vec3& b, float t)
{
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
}

static inline float warpPhase (float s, float w)
{
    //  monotone, endpoints fixed: w > 0 slow start / fast end, w < 0 the mirror
    if (w > 0.0f) return (1.0f - w) * s + w * s * s;
    if (w < 0.0f) { const float u = 1.0f - s; return 1.0f - ((1.0f + w) * u - w * u * u); }
    return s;
}

//  Catmull-Rom through n points, open (phantom ends) or closed, at s in [0,1)
static Vec3 crEval (const Vec3* p, int n, bool closed, float s)
{
    const int nseg = closed ? n : n - 1;
    float f = s * (float) nseg;
    int i = (int) f;
    if (i >= nseg) { i = nseg - 1; f = (float) nseg; }
    const float t = f - (float) i;
    /*  Open lines get PHANTOM end points, extrapolated: with the end point
        merely duplicated the end segments run at half speed and a straight
        line of four collinear points is phase-distorted at both ends —
        measured as a "sine" with -21 dB THD and a pulse 5 % narrower than
        its formula. Extrapolated, four collinear points are exactly linear. */
    auto idx = [&] (int k) -> Vec3
    {
        if (closed) { k %= n; if (k < 0) k += n; return p[k]; }
        if (k < 0)  return { 2.0f * p[0].x - p[1].x, 2.0f * p[0].y - p[1].y, 2.0f * p[0].z - p[1].z };
        if (k >= n) return { 2.0f * p[n - 1].x - p[n - 2].x, 2.0f * p[n - 1].y - p[n - 2].y, 2.0f * p[n - 1].z - p[n - 2].z };
        return p[k];
    };
    const Vec3 p0 = idx (i - 1), p1 = idx (i), p2 = idx (i + 1), p3 = idx (i + 2);
    const float t2 = t * t, t3 = t2 * t;
    auto cr = [&] (float a, float b, float c, float d)
    {
        return 0.5f * ((2.0f * b) + (-a + c) * t + (2.0f * a - 5.0f * b + 4.0f * c - d) * t2 + (-a + 3.0f * b - 3.0f * c + d) * t3);
    };
    Vec3 r { cr (p0.x, p1.x, p2.x, p3.x), cr (p0.y, p1.y, p2.y, p3.y), cr (p0.z, p1.z, p2.z, p3.z) };
    r.x = clamp01 (r.x); r.y = clamp01 (r.y); r.z = clamp01 (r.z);
    return r;
}

Vec3 Line::at (float sIn) const
{
    if (n <= 1) return p[0];
    float s = sIn - std::floor (sIn);
    s = warpPhase (s, clampf (warp, -0.95f, 0.95f));
    if (isSplit())
    {
        //  two open segments: the first half of the cycle on one, the second on the other
        const bool second = s >= 0.5f;
        const float ss = second ? (s - 0.5f) * 2.0f : s * 2.0f;
        return second ? crEval (p + split, n - split, false, ss) : crEval (p, split, false, ss);
    }
    if (closed) { s += start; s -= std::floor (s); }
    return crEval (p, n, closed, s);
}

float Line::length (int steps) const
{
    float len = 0; Vec3 prev = at (0.0f);
    for (int i = 1; i <= steps; ++i)
    {
        const Vec3 q = at ((float) i / (float) steps * 0.99999f);
        len += std::sqrt ((q.x - prev.x) * (q.x - prev.x) + (q.y - prev.y) * (q.y - prev.y) + (q.z - prev.z) * (q.z - prev.z));
        prev = q;
    }
    if (closed)
    {
        const Vec3 q = at (0.0f);
        len += std::sqrt ((q.x - prev.x) * (q.x - prev.x) + (q.y - prev.y) * (q.y - prev.y) + (q.z - prev.z) * (q.z - prev.z));
    }
    return len;
}

Line Line::straight (Vec3 a, Vec3 b)
{
    Line l; l.n = 4; l.closed = false;
    l.p[0] = a; l.p[1] = mix3 (a, b, 1.0f / 3.0f); l.p[2] = mix3 (a, b, 2.0f / 3.0f); l.p[3] = b;
    return l;
}

Line Line::circle (Vec3 c, float r, int axis, int n)
{
    Line l; l.n = n < 3 ? 3 : (n > MAXPTS ? MAXPTS : n); l.closed = true;
    for (int i = 0; i < l.n; ++i)
    {
        const float a = (float) i / (float) l.n * 2.0f * (float) PI;
        const float u = c.x, v = c.y, w = c.z;
        Vec3 q;
        if (axis == 0)      q = { u, v + r * std::cos (a), w + r * std::sin (a) };
        else if (axis == 1) q = { u + r * std::cos (a), v, w + r * std::sin (a) };
        else                q = { u + r * std::cos (a), v + r * std::sin (a), w };
        q.x = clamp01 (q.x); q.y = clamp01 (q.y); q.z = clamp01 (q.z);
        l.p[i] = q;
    }
    return l;
}

Line Line::helix (Vec3 c, float r, float rise, float turns, int n)
{
    Line l; l.n = n < 3 ? 3 : (n > MAXPTS ? MAXPTS : n); l.closed = false;
    for (int i = 0; i < l.n; ++i)
    {
        const float t = (float) i / (float) (l.n - 1);
        const float a = t * turns * 2.0f * (float) PI;
        Vec3 q { c.x + (t - 0.5f) * rise, c.y + r * std::cos (a), c.z + r * std::sin (a) };
        q.x = clamp01 (q.x); q.y = clamp01 (q.y); q.z = clamp01 (q.z);
        l.p[i] = q;
    }
    return l;
}

Line Line::walk (uint32_t seed, int n, bool closed, float step)
{
    Line l; l.n = n < 2 ? 2 : (n > MAXPTS ? MAXPTS : n); l.closed = closed;
    uint32_t s = seed * 2654435761u + 12345u;
    auto rnd = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; };
    Vec3 q { 0.2f + 0.6f * rnd(), 0.2f + 0.6f * rnd(), 0.2f + 0.6f * rnd() };
    for (int i = 0; i < l.n; ++i)
    {
        l.p[i] = q;
        Vec3 d { rnd() - 0.5f, rnd() - 0.5f, rnd() - 0.5f };
        const float m = std::sqrt (d.x * d.x + d.y * d.y + d.z * d.z) + 1e-6f;
        q.x = clampf (q.x + d.x / m * step, 0.05f, 0.95f);
        q.y = clampf (q.y + d.y / m * step, 0.05f, 0.95f);
        q.z = clampf (q.z + d.z / m * step, 0.05f, 0.95f);
    }
    return l;
}

//==============================================================================
//  the volume
void Volume::build (const float* level0, bool periodic)
{
    periodicX = periodic;
    lod[0].assign (level0, level0 + (size_t) VN * VN * VN);
    /*  Each level is the one above blurred with a separable [1 4 6 4 1]/16
        binomial (a 2x2x2 box alone leaves 60 % of the content at half the
        texel rate, which is exactly what has to go) and then decimated. The
        phase axis wraps when the specimen does; the others clamp. */
    for (int l = 1; l < NLOD; ++l)
    {
        const int n = side[l], m = side[l - 1];
        const std::vector<float>& src = lod[l - 1];
        std::vector<float> tmp ((size_t) m * m * m), tmp2 ((size_t) m * m * m);
        static const float w5[5] = { 1.0f / 16, 4.0f / 16, 6.0f / 16, 4.0f / 16, 1.0f / 16 };
        auto ix = [&] (int c, bool wrap) { if (wrap) { c %= m; if (c < 0) c += m; return c; } return c < 0 ? 0 : (c >= m ? m - 1 : c); };
        //  x
        for (int k = 0; k < m; ++k) for (int j = 0; j < m; ++j) for (int i = 0; i < m; ++i)
        {
            float s = 0; for (int q = -2; q <= 2; ++q) s += w5[q + 2] * src[((size_t) k * m + (size_t) j) * m + (size_t) ix (i + q, periodicX)];
            tmp[((size_t) k * m + (size_t) j) * m + (size_t) i] = s;
        }
        //  y
        for (int k = 0; k < m; ++k) for (int j = 0; j < m; ++j) for (int i = 0; i < m; ++i)
        {
            float s = 0; for (int q = -2; q <= 2; ++q) s += w5[q + 2] * tmp[((size_t) k * m + (size_t) ix (j + q, false)) * m + (size_t) i];
            tmp2[((size_t) k * m + (size_t) j) * m + (size_t) i] = s;
        }
        //  z, then decimate
        lod[l].assign ((size_t) n * n * n, 0.0f);
        for (int k = 0; k < n; ++k) for (int j = 0; j < n; ++j) for (int i = 0; i < n; ++i)
        {
            const int kk = 2 * k, jj = 2 * j, ii = 2 * i;
            float s = 0; for (int q = -2; q <= 2; ++q) s += w5[q + 2] * tmp2[((size_t) ix (kk + q, false) * m + (size_t) jj) * m + (size_t) ii];
            lod[l][((size_t) k * n + (size_t) j) * n + (size_t) i] = s;
        }
    }
}

/*  The Mitchell-Netravali cubic family. (B, C) = (1, 0) is the smoothing
    B-spline: C2, and -16 dB at the texel Nyquist. (0, 1/2) is Catmull-Rom:
    interpolating, C1, flat to about 0.4 of the texel rate and a little bright
    above — the texels are read as they are, kinks and all, which is the grain
    Peter asked for. GRAIN walks B from 1 to 0 with C = (1 - B)/2. */
static inline float mnKernel (float x, float B, float C)
{
    x = std::abs (x);
    if (x < 1.0f)
        return ((12.0f - 9.0f * B - 6.0f * C) * x * x * x + (-18.0f + 12.0f * B + 6.0f * C) * x * x + (6.0f - 2.0f * B)) / 6.0f;
    if (x < 2.0f)
        return ((-B - 6.0f * C) * x * x * x + (6.0f * B + 30.0f * C) * x * x + (-12.0f * B - 48.0f * C) * x + (8.0f * B + 24.0f * C)) / 6.0f;
    return 0.0f;
}
static inline void cubicW (float t, float B, float C, float w[4])
{
    w[0] = mnKernel (1.0f + t, B, C);
    w[1] = mnKernel (t, B, C);
    w[2] = mnKernel (1.0f - t, B, C);
    w[3] = mnKernel (2.0f - t, B, C);
}

float Volume::sample (int level, float x, float y, float z, float B, float C) const
{
    level = level < 0 ? 0 : (level >= NLOD ? NLOD - 1 : level);
    const int n = side[level];
    const float* d = lod[level].data();
    if (d == nullptr) return 0.5f;
    const float gx = clamp01 (x) * (float) n - 0.5f, gy = clamp01 (y) * (float) n - 0.5f, gz = clamp01 (z) * (float) n - 0.5f;
    const int ix = (int) std::floor (gx), iy = (int) std::floor (gy), iz = (int) std::floor (gz);
    float wx[4], wy[4], wz[4];
    cubicW (gx - (float) ix, B, C, wx); cubicW (gy - (float) iy, B, C, wy); cubicW (gz - (float) iz, B, C, wz);
    int cx[4], cy[4], cz[4];
    for (int q = 0; q < 4; ++q)
    {
        cx[q] = ix - 1 + q;
        if (periodicX) { cx[q] %= n; if (cx[q] < 0) cx[q] += n; }
        else cx[q] = cx[q] < 0 ? 0 : (cx[q] >= n ? n - 1 : cx[q]);
        cy[q] = iy - 1 + q; cy[q] = cy[q] < 0 ? 0 : (cy[q] >= n ? n - 1 : cy[q]);
        cz[q] = iz - 1 + q; cz[q] = cz[q] < 0 ? 0 : (cz[q] >= n ? n - 1 : cz[q]);
    }
    float acc = 0;
    for (int k = 0; k < 4; ++k)
    {
        const float* pk = d + (size_t) cz[k] * n * n;
        float ak = 0;
        for (int j = 0; j < 4; ++j)
        {
            const float* pj = pk + (size_t) cy[j] * n;
            ak += wy[j] * (wx[0] * pj[cx[0]] + wx[1] * pj[cx[1]] + wx[2] * pj[cx[2]] + wx[3] * pj[cx[3]]);
        }
        acc += wz[k] * ak;
    }
    return acc;
}

float Volume::sampleBlend (float lodF, float x, float y, float z, float B, float C) const
{
    lodF = lodF < 0 ? 0 : (lodF > (float) (NLOD - 1) ? (float) (NLOD - 1) : lodF);
    const int l0 = (int) lodF;
    const float t = lodF - (float) l0;
    if (t < 1e-3f || l0 >= NLOD - 1) return sample (l0, x, y, z, B, C);
    return sample (l0, x, y, z, B, C) * (1.0f - t) + sample (l0 + 1, x, y, z, B, C) * t;
}

//==============================================================================
float Engine::readLine (const Volume& v, const Line& a, const Line& b, float scan, float s, int level)
{
    const Vec3 pa = a.at (s), pb = b.at (s);
    const Vec3 q = mix3 (pa, pb, clamp01 (scan));
    return v.sample (level, q.x, q.y, q.z);
}

//==============================================================================
float Engine::Env::step (float a, float d, float s, float r, float dt)
{
    switch (stage)
    {
        case 1: y += dt / std::max (0.0005f, a); if (y >= 1.0f) { y = 1.0f; stage = 2; } break;
        case 2: { const float k = 1.0f - std::exp (-dt / std::max (0.001f, d) * 4.0f); y += (s - y) * k; if (std::abs (y - s) < 1e-4f) stage = 3; } break;
        case 3: y = s; break;
        case 4: { const float k = 1.0f - std::exp (-dt / std::max (0.001f, r) * 4.0f); y += (0.0f - y) * k; if (y < 1e-4f) { y = 0; stage = 0; } } break;
        default: y = 0; break;
    }
    return y;
}

void Engine::Svf::set (float fcHz, float res01, double srate)
{
    const float fc = clampf (fcHz, 10.0f, (float) (srate * 0.47));
    g = std::tan ((float) PI * fc / (float) srate);
    k = 2.0f - 1.96f * clamp01 (res01);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

inline float Engine::Svf::run (float in, int type)
{
    const float v3 = in - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    switch (type)
    {
        case 0: return v2;                       // low
        case 1: return v1;                       // band
        case 2: return in - k * v1 - v2;         // high
        default: return in;                      // off
    }
}

//==============================================================================
Engine::Engine()
{
    for (int i = 0; i < NLINES; ++i)
    {
        lineSet[i] = Line::straight ({ 0.05f, 0.5f, 0.5f }, { 0.95f, 0.5f, 0.5f });
        linePending[i] = lineSet[i];
    }
    for (auto& v : voices) v.seed = (uint32_t) (0x2311u + 977u * (uint32_t) (&v - voices));
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v = Voice();
    for (int i = 0; i < MAXVOICES; ++i) voices[i].seed = (uint32_t) (0x2311u + 977u * (uint32_t) i);
    tNow = 0; monoN = 0;
    outDcX[0] = outDcX[1] = outDcY[0] = outDcY[1] = 0;
    for (int i = 0; i < SCOPE_N; ++i) scopeW[i] = scopeF[i] = scopeM[i] = 0;
    if (vols[0].specimen < 0 && vols[1].specimen < 0) service();
}

void Engine::setTransport (double bpm, double ppq, bool playing)
{
    bpmNow = bpm > 1 ? bpm : 120.0; ppqNow = ppq; playingNow = playing;
}

Engine::~Engine() { joinBuilder(); }

void Engine::joinBuilder()
{
    if (builder.joinable()) builder.join();
    buildBusy.store (false);
}

void Engine::service()
{
    joinBuilder();                          // never two builders, never a torn publish
    buildDone.store (false);
    const int want = importOn ? SPEC_IMPORTED
                              : listIndex (paramSpec (paramIndex ("specimen")), p.specimen);
    if (vols[volCur.load()].specimen == want && ! importDirty) return;
    const int spare = 1 - volCur.load();
    if (importOn)
    {
        if (importCube.size() != (size_t) VN * VN * VN) { importOn = false; importDirty = false; return; }
        /*  A real body is not periodic in x: its wrap is a genuine edge, and
            the polyBLEP is what deals with it — the same path CORTEX takes. */
        vols[spare].build (importCube.data(), false);
    }
    else
    {
        std::vector<float> tmp ((size_t) VN * VN * VN);
        buildSpecimen (want, tmp.data());
        vols[spare].build (tmp.data(), specimenPeriodicX (want));
    }
    vols[spare].specimen = want;
    volCur.store (spare);
    importDirty = false;
}

/*  The same, off the message thread. At 128^3 a specimen is two million
    texels of noise, up to half a second for CORTEX — long enough to freeze
    the panel and the DAW's window if it ran in the timer. The worker builds
    into buildVol; the tick that finds it done swaps the vectors in and
    publishes. A synchronous service() joins first, so the two never race. */
void Engine::serviceAsync()
{
    if (buildBusy.load())
    {
        if (! buildDone.load()) return;
        joinBuilder();
        buildDone.store (false);
        const int spare = 1 - volCur.load();
        for (int l = 0; l < NLOD; ++l) vols[spare].lod[l].swap (buildVol.lod[l]);
        vols[spare].periodicX = buildVol.periodicX;
        vols[spare].specimen = buildWant;
        volCur.store (spare);
        if (buildWant == SPEC_IMPORTED) importDirty = false;
        return;
    }
    const int want = importOn ? SPEC_IMPORTED
                              : listIndex (paramSpec (paramIndex ("specimen")), p.specimen);
    if (vols[volCur.load()].specimen == want && ! importDirty) return;
    if (importOn && importCube.size() != (size_t) VN * VN * VN) { importOn = false; importDirty = false; return; }
    buildWant = want;
    buildPeriodic = importOn ? false : specimenPeriodicX (want);
    buildBusy.store (true);
    buildDone.store (false);
    std::vector<float> cube = importOn ? importCube : std::vector<float>();
    builder = std::thread ([this, want, cube = std::move (cube)] () mutable
    {
        if (want == SPEC_IMPORTED) buildVol.build (cube.data(), false);
        else
        {
            std::vector<float> tmp ((size_t) VN * VN * VN);
            buildSpecimen (want, tmp.data());
            buildVol.build (tmp.data(), buildPeriodic);
        }
        buildDone.store (true);
    });
}

void Engine::setImported (const float* cube)
{
    importCube.assign (cube, cube + (size_t) VN * VN * VN);
    importOn = true;
    importDirty = true;
    service();
}

void Engine::clearImported()
{
    if (! importOn) return;
    importOn = false;
    importDirty = true;
    importCube.clear();
    importCube.shrink_to_fit();
    service();
}

void Engine::setLine (int which, const Line& l)
{
    if (which < 0 || which >= NLINES) return;
    while (lineLock.exchange (1)) {}
    linePending[which] = l;
    linePendingFlag.store (true);
    lineLock.store (0);
}

void Engine::setLines (const Line* six)
{
    while (lineLock.exchange (1)) {}
    for (int i = 0; i < NLINES; ++i) linePending[i] = six[i];
    linePendingFlag.store (true);
    lineLock.store (0);
}

void Engine::pullLines()
{
    if (! linePendingFlag.load()) return;
    if (lineLock.exchange (1)) return;           // the writer has it; next block
    for (int i = 0; i < NLINES; ++i) lineSet[i] = linePending[i];
    linePendingFlag.store (false);
    lineLock.store (0);
}

void Engine::setWorldMod (float detCents, float sag01, float tremDepth, float tremRate, float filterMul, float panSpread)
{
    wmDet = detCents; wmSag = sag01; wmTremD = tremDepth; wmTremR = tremRate; wmFilt = filterMul; wmSpread = panSpread;
    wmActive = (detCents != 0.0f) || (sag01 != 0.0f) || (tremDepth != 0.0f) || (filterMul != 1.0f) || (panSpread != 0.0f);
}

//==============================================================================
double Engine::noteHz (int note) const
{
    const double semis = (double) note - 69.0 + ((double) p.tune - 0.5) * 24.0;
    return 440.0 * std::pow (2.0, semis / 12.0);
}

int Engine::allocVoice (int note)
{
    for (int i = 0; i < MAXVOICES; ++i) if (voices[i].active && voices[i].note == note && voices[i].held) return i;
    for (int i = 0; i < MAXVOICES; ++i) if (! voices[i].active) return i;
    int best = 0; double bestAge = -1;
    for (int i = 0; i < MAXVOICES; ++i)
    {
        const double a = voices[i].ageOff > 0 ? voices[i].ageOff + 1000.0 : voices[i].age;
        if (a > bestAge) { bestAge = a; best = i; }
    }
    return best;
}

void Engine::startVoice (Voice& v, int note, float vel, bool retrigger, bool glide)
{
    const double target = noteHz (note);
    if (! v.active || ! retrigger)
    {
        v = Voice();
        v.id = nextId++;
        v.fHz = target; v.fGlideFrom = target;
    }
    else
    {
        v.fGlideFrom = v.fHz;
    }
    v.active = true; v.held = true; v.sustained = false; v.released = false;
    v.note = note; v.vel = vel;
    v.age = 0; v.ageOff = 0;
    if (glide && p.glide > 0.001f) { v.glideLen = secondsOf (p.glide); v.glideT = 0; }
    else { v.glideLen = 0; v.fHz = target; }
    v.nUni = 1 + listIndex (paramSpec (paramIndex ("unison")), p.unison);
    for (int k = 0; k < MAXUNI; ++k)
    {
        Uni& u = v.u[k];
        if (! retrigger || ! v.active) { u.ph = 0; u.ph2 = 0; u.shX = 0; u.svf.reset(); u.k35.reset(); u.lad.reset(); }
        //  unison fan, symmetric about zero; the SPECTRA detune is fanned the same way
        const float fan = v.nUni > 1 ? ((float) k / (float) (v.nUni - 1)) * 2.0f - 1.0f : 0.0f;
        const float cents = fan * p.detune * 40.0f;
        u.detMul = std::pow (2.0, (double) cents / 1200.0);
        const float pan = fan * p.spread;
        u.gl = std::cos ((pan + 1.0f) * 0.25f * (float) PI);
        u.gr = std::sin ((pan + 1.0f) * 0.25f * (float) PI);
    }
    v.amp.gate (true);
    v.scanEnvT = 0; v.fPhase = 0; v.mPhase = 0;
    v.scanNow = v.scanPrev = clamp01 (p.scan);
    v.cutHz = v.cutTarget = 1000;
    v.lastTickAbs = -1;
}

void Engine::noteOn (int note, float vel)
{
    const bool mono = listIndex (paramSpec (paramIndex ("voices")), p.voiceMode) == 1;
    if (mono)
    {
        if (monoN < 32) monoStack[monoN++] = note;
        Voice& v = voices[0];
        const bool legato = v.active && v.held;
        startVoice (v, note, vel, legato, legato);
        return;
    }
    const int i = allocVoice (note);
    startVoice (voices[i], note, vel, false, false);
}

void Engine::noteOff (int note)
{
    const bool mono = listIndex (paramSpec (paramIndex ("voices")), p.voiceMode) == 1;
    if (mono)
    {
        for (int i = 0; i < monoN; ++i) if (monoStack[i] == note) { for (int j = i; j < monoN - 1; ++j) monoStack[j] = monoStack[j + 1]; --monoN; break; }
        Voice& v = voices[0];
        if (! v.active) return;
        if (monoN > 0)
        {
            const int back = monoStack[monoN - 1];
            if (back != v.note) { v.fGlideFrom = v.fHz; v.note = back; v.glideLen = p.glide > 0.001f ? secondsOf (p.glide) : 0; v.glideT = 0; if (v.glideLen <= 0) v.fHz = noteHz (back); }
            return;
        }
        if (sustain) { v.sustained = true; return; }
        v.held = false; v.released = true; v.amp.gate (false);
        return;
    }
    for (auto& v : voices)
        if (v.active && v.note == note && v.held)
        {
            if (sustain) { v.sustained = true; v.held = false; continue; }
            v.held = false; v.released = true; v.amp.gate (false);
        }
}

void Engine::allNotesOff()
{
    for (auto& v : voices) v = Voice();
    monoN = 0;
}

void Engine::setBend (float semis) { bendSemis = semis; }

void Engine::setSustain (bool on)
{
    sustain = on;
    if (! on)
        for (auto& v : voices)
            if (v.active && v.sustained) { v.sustained = false; v.held = false; v.released = true; v.amp.gate (false); }
}

//==============================================================================
static double syncHz (int syncIdx, double bpm)
{
    //  L_SYNC: FREE, 4/1, 2/1, 1/1, 1/2, 1/4, 1/8, 1/16, 1/8T, 1/16T  -> cycles per second
    static const double beats[] = { 0, 16, 8, 4, 2, 1, 0.5, 0.25, 1.0 / 3.0, 1.0 / 6.0 };
    if (syncIdx <= 0 || syncIdx > 9) return 0;
    return (bpm / 60.0) / beats[syncIdx];
}

float Engine::scanValue (Voice& v) const
{
    float s = clamp01 (p.scan);
    //  the scan envelope: a rise then a fall, either way
    const float a = secondsOf (p.scanA), d = secondsOf (p.scanD);
    float env = 0;
    if (v.scanEnvT < a) env = v.scanEnvT / std::max (0.001f, a);
    else env = std::exp (-(v.scanEnvT - a) / std::max (0.001f, d));
    s += env * (p.scanAmt * 2.0f - 1.0f);
    s += v.modV * (p.modScan * 2.0f - 1.0f);
    s += modWheel * 0.0f;
    return clamp01 (s);
}

void Engine::tick (Voice& v, double dt, double bpm, double, bool)
{
    const Volume& vol = vols[volCur.load()];
    v.age += dt;
    if (v.released) v.ageOff += dt;
    v.scanEnvT += (float) dt;

    //  glide
    if (v.glideLen > 0)
    {
        v.glideT += dt;
        const double t = clampd (v.glideT / v.glideLen, 0.0, 1.0);
        v.fHz = v.fGlideFrom * std::pow (noteHz (v.note) / v.fGlideFrom, t);
        if (t >= 1.0) v.glideLen = 0;
    }
    else v.fHz = noteHz (v.note);

    //  MOD line: a looping reader, free or synced
    {
        const int sync = listIndex (paramSpec (paramIndex ("msync")), p.modSync);
        double hz = sync > 0 ? syncHz (sync, bpm) : hzOf (paramSpec (paramIndex ("mrate")), p.modRate);
        v.mPhase += (float) (hz * dt);
        v.mPhase -= std::floor (v.mPhase);
        const float r = readLine (vol, lineSet[L_MOD_A], lineSet[L_MOD_B], v.scanNow, v.mPhase, 1);
        v.modV = r * 2.0f - 1.0f;
    }

    //  the scan, and the level of detail it implies
    v.scanPrev = v.scanNow;
    v.scanNow = scanValue (v);
    {
        //  path length in texels at level 0 decides the top harmonic the read can carry
        const Line& A = lineSet[L_WAVE_A]; const Line& B = lineSet[L_WAVE_B];
        float len = 0; Vec3 prev = mix3 (A.at (0), B.at (0), v.scanNow);
        for (int i = 1; i <= 24; ++i)
        {
            const float s = (float) i / 24.0f * 0.99999f;
            const Vec3 q = mix3 (A.at (s), B.at (s), v.scanNow);
            len += std::sqrt ((q.x - prev.x) * (q.x - prev.x) + (q.y - prev.y) * (q.y - prev.y) + (q.z - prev.z) * (q.z - prev.z));
            prev = q;
        }
        const double texels = std::max (1.0, (double) len * VN);
        /*  The harmonic BUDGET. The path carries up to texels/2 harmonics at
            level 0; the window shaper multiplies what it is given by roughly
            its gain, and the interpolating kernel passes the texel Nyquist
            where the B-spline muffled it. So the level is chosen for the
            spectrum the whole read will produce, not the field alone: a hard
            window reads a coarser field, and the harmonics come from the
            window instead. */
        /*  Measured (bench 9): with no coarsening for the window at all, the
            hardest window at C5 on the brightest field aliases at -51 dB —
            the ADAA carries it. So the window does NOT cost detail: only the
            interpolating kernel, which passes the texel Nyquist, asks for a
            little headroom. */
        const double budget = 1.0 + 0.35 * (double) clamp01 (p.grain);
        const double top = v.fHz * texels * 0.5 * budget;   // Hz of the highest harmonic the read carries
        const double lodF = std::log2 (std::max (1.0, top / (0.45 * sr)));
        /*  The level that does not alias is the CEILING of that, whole. A
            fractional blend between the fine level and the coarse one leaks
            the fine level's aliasing at its weight — measured: LUNG at C6 read
            -34.6 dB with the blend and -34.8 with level 0 forced, i.e. the
            blend had done nothing. So the level is an integer, and the voice
            slews onto it over ~20 ms so a glide across a boundary is a fade,
            not a step. */
        v.lodTarget = mipLock >= 0 ? (float) mipLock : (float) clampd (std::ceil (lodF - 1e-6), 0.0, (double) (NLOD - 1));
        if (v.lastTickAbs < 0) v.lodF = v.lodTarget;
        else v.lodF += (v.lodTarget - v.lodF) * (1.0f - std::exp (-(float) dt / 0.02f));
        v.lastTickAbs = 0;
        /*  The read, for this voice: the kernel and the window, which the MOD
            line may move. At MOD>WINDOW and MOD>GRAIN centred the offset is
            an exact +0.0, so the knob values pass through unchanged. */
        {
            const float g = clamp01 (p.grain + v.modV * (p.modGrain * 2.0f - 1.0f));
            v.kB = 1.0f - g; v.kC = 0.5f * g;
            const float c = clamp01 (p.contrast + v.modV * (p.modContrast * 2.0f - 1.0f));
            v.shGain = std::pow (2.0f, 4.0f * c);
            v.shOn = c > 0.0005f || shFold > 0.0005f;
        }
        //  the wrap jump, for the polyBLEP — and the jump at half a cycle of a SPLIT line
        float edge = 0.0f, edge2 = 0.0f;
        if (A.hasWrapEdge() || B.hasWrapEdge())
        {
            const Vec3 q0 = mix3 (A.at (0.0f), B.at (0.0f), v.scanNow);
            const Vec3 qe = mix3 (A.at (0.99999f), B.at (0.99999f), v.scanNow);
            const float w0 = vol.sampleBlend (v.lodF, q0.x, q0.y, q0.z, v.kB, v.kC);
            const float w1 = vol.sampleBlend (v.lodF, qe.x, qe.y, qe.z, v.kB, v.kC);
            edge = (w0 - w1) * 2.0f;
        }
        if (A.isSplit() || B.isSplit())
        {
            const Vec3 q0 = mix3 (A.at (0.5f), B.at (0.5f), v.scanNow);
            const Vec3 qe = mix3 (A.at (0.49999f), B.at (0.49999f), v.scanNow);
            const float w0 = vol.sampleBlend (v.lodF, q0.x, q0.y, q0.z, v.kB, v.kC);
            const float w1 = vol.sampleBlend (v.lodF, qe.x, qe.y, qe.z, v.kB, v.kC);
            edge2 = (w0 - w1) * 2.0f;
        }
        for (int k = 0; k < v.nUni; ++k) { v.u[k].edge = edge; v.u[k].edge2 = edge2; }
    }

    //  FILTER line: a sweep once per note, or a loop
    {
        const int mode = listIndex (paramSpec (paramIndex ("fmode")), p.filtMode);
        const int sync = listIndex (paramSpec (paramIndex ("fsync")), p.filtSync);
        const double rate = sync > 0 ? syncHz (sync, bpm) : hzOf (paramSpec (paramIndex ("frate")), p.filtRate);
        if (mode == 0)
        {
            //  SWEEP: the rate is 1/sweep time; hold at the end
            v.fPhase = (float) std::min (0.99999, (double) v.fPhase + rate * dt);
        }
        else { v.fPhase += (float) (rate * dt); v.fPhase -= std::floor (v.fPhase); }
        const float r = readLine (vol, lineSet[L_FILT_A], lineSet[L_FILT_B], v.scanNow, v.fPhase, 1);
        const float depth = clamp01 (p.filtDepth);
        const float pos = (1.0f - depth) * clamp01 (p.cutoff) + depth * r;
        double fc = 20.0 * std::pow (1000.0, (double) pos);
        //  key tracking about middle C
        fc *= std::pow (2.0, ((double) v.note - 60.0) / 12.0 * (double) clamp01 (p.filtTrack));
        if (wmActive) fc *= (double) wmFilt;
        v.cutTarget = (float) clampd (fc, 20.0, 20000.0);
        /*  Smoothed in octaves. Smoothed in hertz, a sweep from 20 kHz down to
            20 Hz spent its first time constant crossing 7 kHz — 8.5 octaves
            off its target — and the bench read the cutoff 0.73 octaves from
            the line it was meant to follow. */
        const float k = 1.0f - std::exp (-(float) dt / 0.008f);
        const float target = std::log2 (v.cutTarget);
        if (v.lastTickAbs < 0) v.cutLog = target;
        v.cutLog += (target - v.cutLog) * k;
        v.cutHz = std::pow (2.0f, v.cutLog);
        const float res = clamp01 (p.reso);
        for (int q = 0; q < v.nUni; ++q) v.u[q].svf.set (v.cutHz, res, sr);
        /*  The circuits: K from the same RESONANCE knob by each model's own law
            (Black Rider's), and the prewarped coefficient lifted to correct the
            clipper's measured tuning drag on the Sallen-Key models — the
            clipper takes a share of the loop and its share grows with K. */
        const int model = listIndex (paramSpec (paramIndex ("fmodel")), p.filtModel);
        if (model > 0)
        {
            float K, pw = 1.0f;
            if (model == 1)      K = 2.25f * std::pow (res, 0.9f);                                       // GROWL: screams only at the top
            else if (model == 2) K = res < 0.62f ? 2.0f * std::pow (res / 0.62f, 0.7f) : 2.0f + 0.9f * ((res - 0.62f) / 0.38f);   // SCREAM: past two o'clock
            else                 K = 4.0f * 1.12f * std::pow (res, 0.85f);                               // LADDER: self-oscillates at 4
            if (model <= 2)
            {
                float cents = 0.0f;
                if      (K > 2.45f) cents = 17.0f + 78.0f * (K - 2.45f);
                else if (K > 2.25f) cents = 6.0f + 55.0f * (K - 2.25f);
                else if (K > 2.0f)  cents = 24.0f * (K - 2.0f);
                pw = std::pow (2.0f, cents / 1200.0f);
                K = std::min (K, model == 2 ? 2.95f : 2.45f);
            }
            else K = std::min (K, 4.6f);
            const float fc = clampf (v.cutHz * pw, 10.0f, (float) (sr * 0.45));
            v.fG = std::tan ((float) PI * fc / (float) sr);
            v.fK = K;
            for (int q = 0; q < v.nUni; ++q) { v.u[q].k35.setG (v.fG); v.u[q].lad.setG (v.fG); }
        }
    }

    //  pitch modulation, pan modulation
    v.pitchMod = v.modV * (p.modPitch * 2.0f - 1.0f) * 12.0f;
    v.panMod = v.modV * (p.modPan * 2.0f - 1.0f);

    //  gate smoothing for the SPECTRA sag
    v.gateSm += ((v.held || v.sustained ? 1.0f : 0.0f) - v.gateSm) * (1.0f - std::exp (-(float) dt / 0.05f));
}

//==============================================================================
/*  The residual of a cubic-B-spline step: the integral of the four-sample
    kernel minus the unit step, for t = the signed distance from the
    discontinuity in samples, -2..2. Band-limited step = naive step + jump *
    blep(t). The two-sample (linear-kernel) version was worth 14.6 dB on a saw
    at C5; four samples of cubic is worth ~15 dB more. */
//  the window shaper and its antiderivative: clip to [-1, 1], or a triangle
//  fold of period 4 that is the identity on [-1, 1], blended by f
static inline float shape (float x, float f)
{
    const float c = x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
    float u = x - 4.0f * std::floor ((x + 2.0f) * 0.25f);      // -2 .. 2
    const float t = u > 1.0f ? 2.0f - u : (u < -1.0f ? -2.0f - u : u);
    return c + (t - c) * f;
}
static inline float shapeF (float x, float f)
{
    const float a = std::abs (x);
    const float Fc = a <= 1.0f ? 0.5f * x * x : a - 0.5f;
    const float u = x - 4.0f * std::floor ((x + 2.0f) * 0.25f);
    float Ft;
    if (u < -1.0f)     Ft = -0.5f * u * u - 2.0f * u - 2.0f;
    else if (u <= 1.0f) Ft = 0.5f * u * u - 1.0f;
    else               Ft = -0.5f * u * u + 2.0f * u - 2.0f;
    return Fc + (Ft - Fc) * f;
}

static inline float blep (float t)
{
    if (t <= -2.0f || t >= 2.0f) return 0.0f;
    if (t < -1.0f) { const float u = t + 2.0f; return u * u * u * u / 24.0f; }
    if (t < 0.0f)  return 1.0f / 24.0f + (4.0f * t - 2.0f * t * t * t - 0.75f * t * t * t * t + 2.75f) / 6.0f;
    if (t < 1.0f)  return 0.5f + (4.0f * t - 2.0f * t * t * t + 0.75f * t * t * t * t) / 6.0f - 1.0f;
    { const float u = 2.0f - t; return -u * u * u * u / 24.0f; }
}

void Engine::renderVoice (Voice& v, float* L, float* R, int n, double dt)
{
    const Volume& vol = vols[volCur.load()];
    const Line& A = lineSet[L_WAVE_A]; const Line& B = lineSet[L_WAVE_B];
    const int ftype = listIndex (paramSpec (paramIndex ("ftype")), p.filtType);
    const int fmodel = listIndex (paramSpec (paramIndex ("fmodel")), p.filtModel);
    const float velGain = 1.0f - clamp01 (p.velSens) * (1.0f - v.vel);
    float ampA = secondsOf (p.ampA), ampD = secondsOf (p.ampD), ampR = secondsOf (p.ampR);

    double semis = bendSemis + v.pitchMod;
    if (wmActive) semis -= (double) wmSag * (1.0 - (double) v.gateSm) * 12.0;
    const double fBase = v.fHz * std::pow (2.0, semis / 12.0);

    const float uniGain = 1.0f / std::sqrt ((float) v.nUni);
    //  the second head: its mix, its speed, its offset; and how far unison readers spread through the scan
    const float h2Mix = clamp01 (p.head2);
    const bool  h2On = h2Mix > 0.0005f;
    const double h2Ratio = ratioOf (p.head2Ratio);
    const float h2Phase = clamp01 (p.head2Phase);
    const float h2Norm = 1.0f / (1.0f + 0.5f * h2Mix);
    const float lod2 = v.lodF + (float) std::log2 (std::max (1.0, h2Ratio));   // the faster head reads coarser
    const float uniSc = clamp01 (p.uniScan) * 0.3f;
    //  SCAN SPREAD: reader 0 sits on the scan exactly, the others fan around it (an exact 0 at SPREAD 0)
    float scanOff[MAXUNI];
    for (int k = 0; k < MAXUNI; ++k) scanOff[k] = uniSc * (std::fmod ((float) k * 0.618f + 0.5f, 1.0f) * 2.0f - 1.0f);
    float panOff = v.panMod;
    if (wmActive) panOff += wmSpread * ((v.id % 2) ? 0.5f : -0.5f);
    //  the voice's own pan: constant power, exactly 1.0 on both sides at centre
    const float pa = (clampf (panOff, -1.0f, 1.0f) + 1.0f) * 0.25f * (float) PI;
    const float pl = std::cos (pa) * 1.41421356f, pr = std::sin (pa) * 1.41421356f;

    for (int i = 0; i < n; ++i)
    {
        const float env = v.amp.step (ampA, ampD, clamp01 (p.ampS), ampR, (float) dt);
        if (v.amp.stage == 0 && v.released) { v.active = false; break; }
        const float scan = v.scanPrev + (v.scanNow - v.scanPrev) * ((float) i / (float) n);
        float sumL = 0, sumR = 0;
        for (int k = 0; k < v.nUni; ++k)
        {
            Uni& u = v.u[k];
            double det = u.detMul;
            if (wmActive)
            {
                const float fan = std::fmod ((float) k * 0.618f + 0.5f, 1.0f) * 2.0f - 1.0f;
                det *= std::pow (2.0, (double) (wmDet * fan) / 1200.0);
            }
            const double inc = fBase * det / sr;
            const double ph = u.ph;
            const float scanK = clamp01 (scan + scanOff[k]);
            const Vec3 q = mix3 (A.at ((float) ph), B.at ((float) ph), scanK);
            float w = vol.sampleBlend (v.lodF, q.x, q.y, q.z, v.kB, v.kC) * 2.0f - 1.0f;
            if (blepOn)
            {
                //  the wrap is at phase 0: after it (ph small) and before it (ph near 1)
                const float dts = (float) inc;
                if (u.edge != 0.0f)
                {
                    if (ph < 2.0 * dts)             w += u.edge * blep ((float) ph / dts);
                    else if (ph > 1.0 - 2.0 * dts)  w += u.edge * blep (((float) ph - 1.0f) / dts);
                }
                if (u.edge2 != 0.0f)
                {
                    const float d = (float) ph - 0.5f;
                    if (std::abs (d) < 2.0f * dts) w += u.edge2 * blep (d / dts);
                }
            }
            if (h2On)
            {
                //  the second head: the same line, its own phase, mixed in before the window
                double ph2 = u.ph2 + (double) h2Phase; ph2 -= std::floor (ph2);
                const Vec3 q2 = mix3 (A.at ((float) ph2), B.at ((float) ph2), scanK);
                float w2 = vol.sampleBlend (lod2, q2.x, q2.y, q2.z, v.kB, v.kC) * 2.0f - 1.0f;
                if (blepOn)
                {
                    const float dts = (float) (inc * h2Ratio);
                    if (u.edge != 0.0f)
                    {
                        if (ph2 < 2.0 * dts)            w2 += u.edge * blep ((float) ph2 / dts);
                        else if (ph2 > 1.0 - 2.0 * dts) w2 += u.edge * blep (((float) ph2 - 1.0f) / dts);
                    }
                    if (u.edge2 != 0.0f)
                    {
                        const float d = (float) ph2 - 0.5f;
                        if (std::abs (d) < 2.0f * dts) w2 += u.edge2 * blep (d / dts);
                    }
                }
                w = (w + h2Mix * w2) * h2Norm;
                u.ph2 += inc * h2Ratio; u.ph2 -= std::floor (u.ph2);
            }
            if (v.shOn)
            {
                /*  THE AUDIO WINDOW, first-order ADAA. x = the read scaled by
                    the window's gain; the shaper clips it to the window or
                    folds it back, blended by FOLD. Both pieces are piecewise
                    linear, so their antiderivatives are exact quadratics and
                    (F(x) - F(xp)) / (x - xp) is the band-limited output. The
                    shaper is skipped entirely at gain 1 with no fold, so the
                    plain read stays the plain read — the ADAA's own half-sample
                    average would otherwise take the top octave down 3 dB. */
                const float x = w * v.shGain, xp = u.shX;
                u.shX = x;
                const float dx = x - xp;
                if (std::abs (dx) > 1e-3f) w = (shapeF (x, shFold) - shapeF (xp, shFold)) / dx;
                else                       w = shape (0.5f * (x + xp), shFold);
            }
            //  the filter: the SVF, or one of the circuits as a lowpass or a highpass (BAND is the SVF's)
            if (fmodel == 0 || ftype == 1 || ftype == 3) w = u.svf.run (w, ftype);
            else if (fmodel == 3) w = ftype == 0 ? u.lad.lowpass (w, v.fK) : u.lad.highpass (w, v.fK);
            else                  w = ftype == 0 ? u.k35.lowpass (w, v.fK) : u.k35.highpass (w, v.fK);
            sumL += w * u.gl; sumR += w * u.gr;
            u.ph = ph + inc; if (u.ph >= 1.0) u.ph -= 1.0;
            if (k == 0) v.lastWave = w;
        }
        float g = env * velGain * uniGain;
        if (wmActive && wmTremD > 0.0f)
        {
            v.tremPh += wmTremR * (float) dt; v.tremPh -= std::floor (v.tremPh);
            g *= 1.0f - wmTremD * 0.5f * (1.0f - std::cos (2.0f * (float) PI * v.tremPh));
        }
        L[i] += sumL * pl * g;
        R[i] += sumR * pr * g;
        //  the scope: one cycle of reader 0, phase-indexed
        if (&v == &voices[scopeVoice])
        {
            const int idx = (int) (v.u[0].ph * SCOPE_N);
            scopeW[idx < 0 ? 0 : (idx >= SCOPE_N ? SCOPE_N - 1 : idx)] = v.lastWave;
        }
    }
}

void Engine::process (float* L, float* R, int n)
{
    std::memset (L, 0, sizeof (float) * (size_t) n);
    std::memset (R, 0, sizeof (float) * (size_t) n);
    pullLines();
    const double dt = 1.0 / sr;

    //  the read, for this block: the fold (the kernel and the gain are per voice, in tick)
    shFold = clamp01 (p.fold);

    //  the scope follows the newest sounding voice
    {
        int best = -1, bestId = -1;
        for (int i = 0; i < MAXVOICES; ++i) if (voices[i].active && voices[i].id > bestId) { bestId = voices[i].id; best = i; }
        if (best >= 0) scopeVoice = best;
    }

    for (int i0 = 0; i0 < n; i0 += TICK)
    {
        const int m = std::min (TICK, n - i0);
        for (auto& v : voices)
        {
            if (! v.active) continue;
            tick (v, dt * m, bpmNow, ppqNow, playingNow);
            renderVoice (v, L + i0, R + i0, m, dt);
        }
        tNow += dt * m;
    }

    //  master: soft ceiling, DC block, level
    const float lvl = 2.4f * p.level * p.level;
    const float dcR = 1.0f - (float) (2.0 * PI * 5.0 / sr);
    float peak = 0;
    for (int i = 0; i < n; ++i)
    {
        float x[2] = { L[i] * lvl, R[i] * lvl };
        for (int c = 0; c < 2; ++c)
        {
            const float y = x[c] - outDcX[c] + dcR * outDcY[c];
            outDcX[c] = x[c]; outDcY[c] = y;
            //  soft ceiling: identity below 0.85, tanh above (transparent where it matters)
            const float a = std::abs (y);
            const float out = a <= 0.85f ? y : (y < 0 ? -1.0f : 1.0f) * (0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f));
            x[c] = out;
            peak = std::max (peak, std::abs (out));
        }
        L[i] = x[0]; R[i] = x[1];
    }
    outLvl.store (peak);

    //  mod-scanner and filter-scanner scopes, written at block rate from the scope voice
    {
        const Voice& v = voices[scopeVoice];
        if (v.active)
        {
            const Volume& vol = vols[volCur.load()];
            for (int i = 0; i < SCOPE_N; i += 4)
            {
                const float s = (float) i / (float) SCOPE_N;
                scopeF[i] = readLine (vol, lineSet[L_FILT_A], lineSet[L_FILT_B], v.scanNow, s, 1);
                scopeM[i] = readLine (vol, lineSet[L_MOD_A], lineSet[L_MOD_B], v.scanNow, s, 1);
            }
        }
    }
    captureView();
}

//==============================================================================
void Engine::captureView()
{
    if (viewLock.exchange (1)) return;
    int n = 0;
    for (const auto& v : voices)
    {
        if (! v.active || n >= MAXVOICES) continue;
        VoiceView& o = view[n++];
        o.id = v.id; o.note = v.note; o.held = v.held ? 1 : 0;
        o.scan = v.scanNow; o.cutoff = v.cutHz; o.fPhase = v.fPhase; o.mPhase = v.mPhase; o.mod = v.modV; o.lvl = v.amp.y;
    }
    viewN = n;
    viewLock.store (0);
}

int Engine::voicesView (VoiceView* out, int maxOut)
{
    while (viewLock.exchange (1)) {}
    const int n = std::min (viewN, maxOut);
    for (int i = 0; i < n; ++i) out[i] = view[i];
    viewLock.store (0);
    return n;
}

void Engine::scopeView (float* w, float* f, float* m)
{
    for (int i = 0; i < SCOPE_N; ++i)
    {
        w[i] = scopeW[i];
        const int j = (i / 4) * 4;
        f[i] = scopeF[j]; m[i] = scopeM[j];
    }
}

int Engine::activeVoices() const { int n = 0; for (const auto& v : voices) n += v.active ? 1 : 0; return n; }
float Engine::voiceCutoff (int v) const { return (v >= 0 && v < MAXVOICES) ? voices[v].cutHz : 0.0f; }
float Engine::voiceCutTarget (int v) const { return (v >= 0 && v < MAXVOICES) ? voices[v].cutTarget : 0.0f; }
float Engine::voiceScan (int v) const { return (v >= 0 && v < MAXVOICES) ? voices[v].scanNow : 0.0f; }
int   Engine::voiceLod (int v) const { return (v >= 0 && v < MAXVOICES) ? (int) std::lround (voices[v].lodTarget) : 0; }

} // namespace bs
