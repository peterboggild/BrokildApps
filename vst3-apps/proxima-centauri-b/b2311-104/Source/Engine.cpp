#include "Engine.h"
#include <algorithm>
#include <cstring>

namespace ab104
{

//==============================================================================
#define P(x) [] (Params& q) -> float& { return q.x; }

static const PSpec SPECS[] =
{
    { "level",      "LEVEL",        "how loud the grid is allowed to be",                     0.62f, KP_VOL,    0, 0,    P(level) },

    { "ambient",    "AMBIENT",      "the temperature everything returns to",                  0.4748f, KP_KELVIN, 77.0f, 800.0f, P(ambient) },
    { "onset",      "ONSET",        "how much disequilibrium the song requires",              0.50f, KP_PCT,    0, 0,    P(onset) },
    { "traffic",    "TRAFFIC",      "how busy the grid is on its own",                        0.12f, KP_PCT,    0, 0,    P(traffic) },

    { "section",    "SECTION",      "where the three-space cuts the body",                    0.50f, KP_BIPOL,  0, 0,    P(section) },
    { "veil",       "VEIL",         "how thick a slice of the body is present",               0.42f, KP_PCT,    0, 0,    P(veil) },
    { "turn",       "PRECESSION",   "how fast the body turns through itself",                 0.25f, KP_PCT,    0, 0,    P(turn) },

    { "flux",       "FLUX",         "how fast an order pours heat",                           0.55f, KP_PCT,    0, 0,    P(flux) },
    { "mass",       "MASS",         "how much heat it takes to move a conduit",               0.45f, KP_PCT,    0, 0,    P(mass) },
    { "leak",       "LEAK",         "how fast a conduit forgets towards ambient",             0.40f, KP_PCT,    0, 0,    P(leak) },
    { "chuff",      "CHUFF",        "the stammer before a conduit sings",                     0.45f, KP_PCT,    0, 0,    P(chuff) },
    { "heed",       "HEED",         "how much a hard order overdrives",                       0.60f, KP_PCT,    0, 0,    P(heed) },
    { "retain",     "RETENTION",    "how much heat a released conduit keeps",                 0.00f, KP_PCT,    0, 0,    P(retain) },
    { "discipline", "DISCIPLINE",   "how exactly an order is obeyed",                         0.75f, KP_PCT,    0, 0,    P(discipline) },

    { "conduction", "CONDUCTION",   "how heat travels between conduits",                      0.35f, KP_PCT,    0, 0,    P(conduction) },
    { "bleed",      "JUNCTION BLEED","how sound leaks across the joints",                     0.30f, KP_PCT,    0, 0,    P(bleed) },
    { "steepen",    "STEEPENING",   "how a loud wave leans forward and breaks",               0.35f, KP_PCT,    0, 0,    P(steepen) },
    { "stiffness",  "STIFFNESS",    "how far the partials leave the harmonic series",         0.30f, KP_PCT,    0, 0,    P(stiffness) },
    { "turbulence", "TURBULENCE",   "how readily the heat drives the tone into chaos",        0.28f, KP_PCT,    0, 0,    P(turbulence) },
    { "character",  "CHARACTER",    "how strongly the specimen's own material is voiced",     0.80f, KP_PCT,    0, 0,    P(character) },

    { "breadth",    "BREADTH",      "how wide the web stands in the image",                   0.60f, KP_PCT,    0, 0,    P(breadth) },
    { "tune",       "TUNE",         "twelve semitones either way, on every order",            0.50f, KP_SEMI,   -12, 12, P(tune) },
    { "sat",        "CEILING",      "the level it will not exceed",                           0.35f, KP_PCT,    0, 0,    P(sat) },
};
#undef P

static const int NSPEC = (int)(sizeof SPECS / sizeof SPECS[0]);

int          numParams()       { return NSPEC; }
const PSpec& paramSpec (int i) { return SPECS[i < 0 ? 0 : (i >= NSPEC ? NSPEC - 1 : i)]; }
float        paramMax (const PSpec& s)
{
    if (s.kind == KP_LIST || s.kind == KP_INT) return s.hi;
    return 1.0f;
}
const char* const* listNames (const char*, int& n) { n = 0; return nullptr; }

/*  The one map from the AMBIENT parameter to kelvin. Exponential, because
    pitch goes as sqrt(T): equal moves of the control are equal moves in
    cents, which is what a hand expects of a tuning-sized control. */
double ambientKelvin (float v)
{
    const double t = v < 0 ? 0 : (v > 1 ? 1 : v);
    return T_AMB_LO * std::pow (T_AMB_HI / T_AMB_LO, t);
}

//==============================================================================
static inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline double clampd (double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
static inline double xmapd (double v, double lo, double hi)
{ return lo * std::pow (hi / lo, clampd (v, 0.0, 1.0)); }

struct Rng
{
    uint64_t s;
    explicit Rng (uint64_t seed) : s (seed ^ 0x9E3779B97F4A7C15ull) { next(); next(); }
    double next()
    {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return (double) ((s >> 11) & ((1ull << 53) - 1)) / (double) (1ull << 53);
    }
    double range (double a, double b) { return a + (b - a) * next(); }
    int    below (int n) { return (int) (next() * n) % n; }
};

//==============================================================================
/*  THE CATALOGUE. A specimen is a web on the 3-sphere: nodes seeded uniformly
    (normalised 4-gaussians), each joined to its nearest neighbours, the whole
    kept connected by a minimum spanning tree. A conduit's geodesic ANGLE is
    its length, and length is pitch — mapped monotonically into the bass so a
    long conduit is always a deep one.

    The alienness floor is the same law .22 lives under, adapted to a pitch
    set: no specimen may sit near a single harmonic series (>= 18 cents RMS)
    or near the 12-TET grid (>= 15 cents RMS). The generator re-salts,
    deterministically, until the floor clears. */
int specimenCount() { return 128; }

static double tetDistanceRms (const float* f, int n)
{
    double acc = 0;
    for (int i = 0; i < n; ++i)
    {
        double c = 1200.0 * std::log2 ((double) f[i] / 440.0);
        double d = c - 100.0 * std::round (c / 100.0);
        acc += d * d;
    }
    return std::sqrt (acc / std::max (1, n));
}

static double harmonicDistanceRms (const float* f, int n)
{
    double best = 1e9;
    for (int k = 0; k < 260; ++k)
    {
        const double f0 = 6.0 * std::pow (60.0 / 6.0, k / 259.0);
        double acc = 0;
        for (int i = 0; i < n; ++i)
        {
            const double r = (double) f[i] / f0;
            const double m = std::max (1.0, std::round (r));
            const double d = 1200.0 * std::log2 (r / m);
            acc += d * d;
        }
        best = std::min (best, std::sqrt (acc / std::max (1, n)));
    }
    return best;
}

static void buildOnce (uint64_t seed, Geometry& g)
{
    Rng r (seed);
    g = Geometry();
    const int n = 34 + (int) (r.next() * 13.0);   // 34..46
    g.nNode = n;

    for (int i = 0; i < n; ++i)
    {
        //  Box-Muller pairs; normalised, this is uniform on S3
        double v[4];
        for (int k = 0; k < 4; k += 2)
        {
            const double u1 = std::max (1e-12, r.next()), u2 = r.next();
            const double m = std::sqrt (-2.0 * std::log (u1));
            v[k]   = m * std::cos (2.0 * PI * u2);
            v[k+1] = m * std::sin (2.0 * PI * u2);
        }
        double len = std::sqrt (v[0]*v[0] + v[1]*v[1] + v[2]*v[2] + v[3]*v[3]);
        if (len < 1e-9) { v[0] = 1; len = 1; }
        for (int k = 0; k < 4; ++k) g.node[i][k] = (float) (v[k] / len);
    }

    auto angleOf = [&g] (int a, int b)
    {
        double d = 0;
        for (int k = 0; k < 4; ++k) d += (double) g.node[a][k] * g.node[b][k];
        return std::acos (clampd (d, -1.0, 1.0));
    };

    //  candidate edges: every pair, sorted by angle. The spanning tree takes
    //  what connectivity needs; nearest-neighbour edges fill to the cap.
    struct E { int a, b; double ang; };
    std::vector<E> all;
    all.reserve ((size_t) n * (n - 1) / 2);
    for (int a = 0; a < n; ++a)
        for (int b = a + 1; b < n; ++b)
            all.push_back ({ a, b, angleOf (a, b) });
    std::sort (all.begin(), all.end(), [] (const E& x, const E& y) { return x.ang < y.ang; });

    std::vector<int> uf (n);
    for (int i = 0; i < n; ++i) uf[i] = i;
    std::vector<int8_t> used (all.size(), 0);
    auto find = [&uf] (int x) { while (uf[x] != x) x = uf[x] = uf[uf[x]]; return x; };

    int nd = 0;
    for (size_t i = 0; i < all.size() && nd < n - 1; ++i)
    {
        const int ra = find (all[i].a), rb = find (all[i].b);
        if (ra == rb) continue;
        uf[ra] = rb; used[i] = 1; ++nd;
    }
    //  each node's three nearest, shortest first, until the cap
    std::vector<int> degree (n, 0);
    for (size_t i = 0; i < all.size(); ++i) if (used[i]) { ++degree[all[i].a]; ++degree[all[i].b]; }
    for (size_t i = 0; i < all.size() && nd < MAXDUCT; ++i)
    {
        if (used[i]) continue;
        if (degree[all[i].a] >= 3 && degree[all[i].b] >= 3) continue;
        used[i] = 1; ++degree[all[i].a]; ++degree[all[i].b]; ++nd;
    }

    g.nDuct = 0;
    double angLo = 1e9, angHi = -1e9;
    std::vector<double> angs;
    for (size_t i = 0; i < all.size(); ++i)
        if (used[i] && g.nDuct < MAXDUCT)
        {
            g.ductA[g.nDuct] = all[i].a;
            g.ductB[g.nDuct] = all[i].b;
            angs.push_back (all[i].ang);
            angLo = std::min (angLo, all[i].ang);
            angHi = std::max (angHi, all[i].ang);
            ++g.nDuct;
        }

    /*  Length is pitch, monotonically: the specimen's own span of angles maps
        onto 24..260 Hz in log-log, so the longest conduit is always the
        deepest and the full bass range is present by construction. A few
        per-duct cents of seeded jitter keep coincident lengths apart. */
    const double span = std::max (1e-6, std::log (angHi) - std::log (angLo));
    for (int i = 0; i < g.nDuct; ++i)
    {
        const double t = (std::log (angHi) - std::log (angs[(size_t) i])) / span;
        const double jit = std::pow (2.0, r.range (-0.045, 0.045));
        g.fPassive[i] = (float) clampd (24.0 * std::pow (260.0 / 24.0, t) * jit, 22.0, 290.0);
        const double uEnd = r.next();          //  the same draw, kept: the material
        g.closedU[i] = uEnd;                   //  moves the threshold, never the draw
        g.closedEnd[i] = uEnd < 0.4 ? 1 : 0;

        //  geodesic midpoint
        double m[4], len = 0;
        for (int k = 0; k < 4; ++k) { m[k] = (double) g.node[g.ductA[i]][k] + g.node[g.ductB[i]][k]; len += m[k]*m[k]; }
        len = std::sqrt (std::max (1e-12, len));
        for (int k = 0; k < 4; ++k) g.mid[i][k] = (float) (m[k] / len);
    }

    //  which conduits share a junction with this one — the two couplings
    //  (heat, and sound) both travel this table
    for (int i = 0; i < g.nDuct; ++i)
    {
        g.nNbd[i] = 0;
        for (int j = 0; j < g.nDuct && g.nNbd[i] < MAXNBD; ++j)
        {
            if (j == i) continue;
            if (g.ductA[j] == g.ductA[i] || g.ductA[j] == g.ductB[i]
             || g.ductB[j] == g.ductA[i] || g.ductB[j] == g.ductB[i])
                g.nbd[i][g.nNbd[i]++] = j;
        }
    }
}

/*  THE MEDIA. Each row is a region of the same physics, given as ranges; a
    specimen draws inside its family and leans a little towards a second one,
    so no two are alike and every family is reachable. The families are
    INTERLEAVED along the dial (family = (n*5+3) % 8), so neighbouring numbers
    are different materials — a dial that moved one step and changed nothing
    is exactly the complaint this answers.

    Ranges are what the bench measured as musical, not taste: drive is the
    instantaneous nonlinearity (0.4 is nearly linear, 3.5 a square-ish wave),
    fold is the share of it that FOLDS rather than saturates, shock is the
    amplitude-dependent fraction of the period (Burgers), closed the share of
    quarter-wave conduits, rad the radiation corner, skin the even harmonics
    on the tap, f/g/q the vessel's two formants at 234 K, m the wall's two
    inharmonic mode ratios and mb how loudly they ring, pulse the valve
    transient in ms (dbl: a sharp doublet instead of a soft puff), inertia
    scales how slowly heat moves, sym how readily the web answers. */
struct MRange { float lo, hi; };
struct MFamily
{
    const char* name;
    MRange drive, fold, shock, closed, rad, skin, f1, f2, g1, g2, q, m1, m2, mb, pulse;
    float dbl;
    MRange inertia, sym;
    float trimDb;      // level trim per unit CHARACTER: the old engine's mean rms at E2 and E3, less 1.5 dB for brightness (ab104timbre)
};

static const MFamily FAMILIES[8] =
{
    //  name      drive        fold         shock          closed       rad          skin         f1           f2            g1          g2          q          m1           m2           mb           pulse      dbl  inertia      sym          trim
    { "SINEW",  {0.30f,0.60f},{0.00f,0.05f},{0.000f,0.005f},{0.30f,0.50f},{0.8f,1.2f},{0.00f,0.05f},{200,350},{ 700,1100},{ 0, 3},{-3, 0},{1.5f,3},{2.10f,2.30f},{3.50f,3.80f},{0.00f,0.10f},{2.5f,4.0f},0,{0.9f,1.3f},{0.8f,1.2f}, 6.7f },
    { "REED",   {1.40f,2.20f},{0.00f,0.10f},{0.005f,0.015f},{0.85f,1.00f},{2.0f,3.0f},{0.00f,0.10f},{380,620},{1300,1900},{ 4, 8},{ 3, 7},{3.0f,6},{1.65f,1.80f},{4.10f,4.40f},{0.00f,0.15f},{1.5f,3.0f},0,{0.6f,1.0f},{0.7f,1.2f}, 9.0f },
    { "HORN",   {2.00f,3.00f},{0.00f,0.05f},{0.030f,0.060f},{0.00f,0.12f},{2.6f,3.8f},{0.20f,0.50f},{450,750},{1100,1600},{ 5, 9},{ 2, 6},{2.5f,5},{2.30f,2.50f},{3.90f,4.20f},{0.05f,0.20f},{4.0f,8.0f},0,{1.2f,1.8f},{0.8f,1.3f}, 12.2f },
    { "BELL",   {0.50f,1.00f},{0.20f,0.40f},{0.000f,0.010f},{0.20f,0.50f},{1.2f,2.0f},{0.00f,0.10f},{600,1200},{2000,3200},{ 0, 4},{ 2, 6},{4.0f,8},{2.60f,2.90f},{5.20f,5.60f},{0.70f,1.00f},{0.4f,0.9f},1,{0.35f,0.6f},{1.8f,2.8f}, 8.0f },
    { "GLASS",  {1.00f,1.60f},{0.55f,0.90f},{0.000f,0.010f},{0.50f,0.80f},{2.8f,4.0f},{0.00f,0.05f},{900,1500},{2400,3600},{ 3, 7},{ 5,10},{5.0f,9},{1.55f,1.65f},{2.10f,2.35f},{0.30f,0.60f},{0.6f,1.5f},1,{0.5f,0.9f},{1.2f,2.0f}, 5.7f },
    { "GROWL",  {2.60f,3.60f},{0.10f,0.25f},{0.015f,0.040f},{0.35f,0.65f},{1.2f,1.8f},{0.30f,0.70f},{180,320},{ 600, 900},{ 6,11},{ 0, 4},{2.0f,4},{1.40f,1.60f},{2.90f,3.30f},{0.10f,0.30f},{3.0f,6.0f},0,{1.4f,2.4f},{1.2f,1.8f}, 12.8f },
    { "VOICE",  {1.50f,2.40f},{0.25f,0.50f},{0.005f,0.020f},{0.10f,0.40f},{2.2f,3.2f},{0.10f,0.30f},{300,800},{ 900,2400},{ 8,12},{ 7,11},{5.0f,9},{2.00f,2.40f},{3.40f,3.90f},{0.00f,0.15f},{2.0f,4.0f},0,{0.8f,1.3f},{0.8f,1.3f}, 9.7f },
    { "ANOMALY",{0.30f,3.60f},{0.00f,1.00f},{0.000f,0.060f},{0.00f,1.00f},{0.8f,4.0f},{0.00f,0.70f},{150,1500},{ 700,3600},{-6,12},{-6,12},{2.0f,9},{1.30f,3.50f},{3.50f,6.50f},{0.00f,1.00f},{0.4f,8.0f},0.5f,{0.35f,2.4f},{0.6f,3.0f}, 8.4f },
};

const char* materialName (int family) { return FAMILIES[((family % 8) + 8) % 8].name; }
int materialFamilyOf (int index)
{
    index = ((index % specimenCount()) + specimenCount()) % specimenCount();
    return (index * 5 + 3) % 8;
}

static void buildMaterial (int index, Material& m)
{
    Rng r ((uint64_t) index * 0x5DEECE66Dull + 0x104CA7ull);   // its own stream
    m = Material();
    m.family  = materialFamilyOf (index);
    m.family2 = (m.family + 1 + r.below (7)) % 8;
    m.blend   = (float) r.range (0.0, 0.30);
    const MFamily& A = FAMILIES[m.family];
    const MFamily& B = FAMILIES[m.family2];
    //  one draw per trait, used for both families, so the lean is a lean
    auto pick = [&] (const MRange& a, const MRange& b, bool logScale)
    {
        const double u = r.next();
        auto at = [&] (const MRange& x)
        {
            return logScale ? x.lo * std::pow ((double) x.hi / x.lo, u)
                            : x.lo + ((double) x.hi - x.lo) * u;
        };
        return (float) (at (a) + ((double) m.blend) * (at (b) - at (a)));
    };
    m.drive      = pick (A.drive,  B.drive,  true);
    m.fold       = pick (A.fold,   B.fold,   false);
    m.shock      = pick (A.shock,  B.shock,  false);
    m.closedFrac = pick (A.closed, B.closed, false);
    m.radMul     = pick (A.rad,    B.rad,    true);
    m.skin       = pick (A.skin,   B.skin,   false);
    m.f1         = pick (A.f1,     B.f1,     true);
    m.f2         = std::max (m.f1 * 1.6f, pick (A.f2, B.f2, true));
    m.g1db       = pick (A.g1,     B.g1,     false);
    m.g2db       = pick (A.g2,     B.g2,     false);
    m.q1         = pick (A.q,      B.q,      true);
    m.q2         = pick (A.q,      B.q,      true);
    m.modeR[0]   = pick (A.m1,     B.m1,     true);
    m.modeR[1]   = pick (A.m2,     B.m2,     true);
    m.modeBase   = pick (A.mb,     B.mb,     false);
    m.modeQ      = 0.8f + 0.8f * (float) r.next();
    m.pulseMs    = pick (A.pulse,  B.pulse,  true);
    m.doublet    = (float) (r.next() < (A.dbl + m.blend * (B.dbl - A.dbl)) ? 1 : 0);
    m.inertia    = pick (A.inertia,B.inertia,true);
    m.sympathy   = pick (A.sym,    B.sym,    true);
    m.trimDb     = A.trimDb + m.blend * (B.trimDb - A.trimDb);
}

void buildSpecimen (int index, Geometry& g)
{
    index = ((index % specimenCount()) + specimenCount()) % specimenCount();
    for (int salt = 0; salt < 80; ++salt)
    {
        buildOnce ((uint64_t) index * 0xA1041047ull + (uint64_t) salt * 0x9E3779B9ull + 0xB2311104ull, g);
        g.salt = salt;
        if (harmonicDistanceRms (g.fPassive, g.nDuct) >= 18.0
         && tetDistanceRms (g.fPassive, g.nDuct) >= 15.0)
            break;
    }
    buildMaterial (index, g.mat);
}

//==============================================================================
Engine::Engine()
{
    pokes.reserve (64);
    buildSpecimen (0, geo);
    reset();
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    /*  The loop DC blocker sits at 0.25 Hz WHATEVER the rate, and the number
        is a measurement, not a taste. The first draft fixed R = 0.9955 — a
        34 Hz corner at 48k, 68 at 96k, on top of the fundamentals: -66 to
        -92 cents. Moving to 5 Hz still left -24: the blocker's phase LEAD is
        -22 samples at the fundamental and ~0 at the harmonics, and a limit
        cycle is a PERIODIC solution — it compromises between its harmonics'
        phase closures, which linear theory at the fundamental cannot see.
        The corner sweep measured -24 c at 5 Hz, -4 at 1, -1 at 0.5, 0.00 at
        0.25, both end types. Dispersion in a singing loop must be kept
        vanishing; compensation cannot rescue it. */
    dcR = std::exp (-2.0 * PI * 0.25 / sr);
    //  the heater's thermal inertia (see the King's-law block): 6 Hz, i.e.
    //  ~26 ms — quick enough that a DC state loses its gain before it can
    //  latch, and on the ADDITIVE branch, so it barely touches loop phase
    kingHpA = (float) (1.0 - std::exp (-2.0 * PI * 6.0 / sr));
    lineLen = (int) (sr / 11.0) + 8;
    lines.assign ((size_t) MAXDUCT * (size_t) lineLen, 0.0f);
    rebuildOutHp();
    reset();
}

void Engine::rebuildOutHp()
{
    //  16 Hz Butterworth high-pass: the subsonic guard under an instrument
    //  whose business is the bottom octaves
    const double w = 2.0 * PI * 16.0 / sr;
    const double c = std::cos (w), s = std::sin (w);
    const double q = 0.70710678;
    const double alpha = s / (2.0 * q);
    const double a0 = 1.0 + alpha;
    hpB0 = (1.0 + c) * 0.5 / a0;
    hpB1 = -(1.0 + c) / a0;
    hpB2 = (1.0 + c) * 0.5 / a0;
    hpA1 = (-2.0 * c) / a0;
    hpA2 = (1.0 - alpha) / a0;
}

void Engine::reset()
{
    const double tAmb = ambientKelvin (p.ambient);
    for (int i = 0; i < MAXDUCT; ++i)
    {
        duct[i] = Duct();
        radCurL[i] = radCurR[i] = 0; lastOutArr[i] = 0; condAcc[i] = 0;
        T[i] = tAmb;
        fNow[i] = geo.fPassive[i];
        sigma[i] = 0;
        closedNow[i] = geo.closedEnd[i];
        huntPh[i] = (float) (0.61803398875 * i - std::floor (0.61803398875 * i));
    }
    if (! lines.empty())  std::fill (lines.begin(),  lines.end(),  0.0f);
    wr = 0;
    for (auto& c : cmds) c = Cmd();
    cmdClock = 0; bendSemis = 0; sustain = false; gateSm = 0;
    for (auto& k : pkts) k.live = 0;
    nextPktAt = 1.0; pktSeed = 0x104A17ull; carrierStep = -1; carrierAt = 0;
    rotA = 0.31; rotB = 0.12; spinQA = spinQB = 0;
    clockSamples = 0; tickLeft = 0; tremPh = 0;
    outDcX[0] = outDcX[1] = outDcY[0] = outDcY[1] = 0;
    outDcX2[0] = outDcX2[1] = outDcY2[0] = outDcY2[1] = 0;
    hpZ1[0] = hpZ1[1] = hpZ2[0] = hpZ2[1] = 0;
    hp2Z1[0] = hp2Z1[1] = hp2Z2[0] = hp2Z2[1] = 0;
    std::memset (bodyZ1, 0, sizeof bodyZ1); std::memset (bodyZ2, 0, sizeof bodyZ2);
    outLevel.store (0.0f); gridPower.store (0.0f); singing.store (0); chuffFired.store (0);
    updateSection();
}

void Engine::applyGeometry (const Geometry& g)
{
    geo = g;
    ++geomStamp;
    const double tAmb = ambientKelvin (p.ambient);
    for (int i = 0; i < MAXDUCT; ++i)
    {
        duct[i] = Duct();
        radCurL[i] = radCurR[i] = 0; lastOutArr[i] = 0; condAcc[i] = 0;
        T[i] = tAmb;
        fNow[i] = geo.fPassive[i];
        sigma[i] = 0;
        closedNow[i] = geo.closedEnd[i];
    }
    if (! lines.empty())  std::fill (lines.begin(),  lines.end(),  0.0f);
    std::memset (bodyZ1, 0, sizeof bodyZ1); std::memset (bodyZ2, 0, sizeof bodyZ2);
    for (auto& c : cmds) c = Cmd();
    for (auto& k : pkts) k.live = 0;
    updateSection();
}

void Engine::requestSpecimen (int index)
{
    index = ((index % specimenCount()) + specimenCount()) % specimenCount();
    buildSpecimen (index, pending);
    wantSpec.store (index);
    swapReady.store (true);
}

void Engine::service() {}

void Engine::setTransport (double, double, bool) {}

//==============================================================================
void Engine::setBend (float semis) { bendSemis = semis; }
void Engine::setSustain (bool on)  { sustain = on; }

void Engine::noteOn (int note, float vel)
{
    allocCmd (note, clampf (vel, 0.0f, 1.0f));
}

void Engine::noteOff (int note)
{
    for (auto& c : cmds)
        if (c.note == note && c.held) c.held = false;
}

void Engine::allNotesOff()
{
    for (auto& c : cmds) { c.held = false; c.env = 0; c.note = -1; c.duct = -1; }
    sustain = false;
}

int Engine::pickDuct (double fCmd) const
{
    /*  The order goes to the conduit that can most nearly obey it: nearest in
        CURRENT pitch (a warm conduit already close is preferred — legato is
        re-heating, not re-allocating), present in the section, not already
        under another order. */
    int best = 0; double bestCost = 1e18;
    for (int i = 0; i < geo.nDuct; ++i)
    {
        double cost = std::abs (std::log2 (fCmd / std::max (1.0, fNow[i])));
        cost += 0.8 * (1.0 - (double) sigma[i]);
        for (const auto& c : cmds)
            if (c.duct == i && c.env > 1e-4) { cost += 6.0; break; }
        if (cost < bestCost) { bestCost = cost; best = i; }
    }
    return best;
}

void Engine::allocCmd (int note, float vel)
{
    /*  fCmd carries NO tune: TUNE is applied once, on every conduit's
        resonance, so orders and struck rings transpose together. Baking it
        in here as well applied it twice — measured as +24 semitones. */
    const double fCmd = 440.0 * std::pow (2.0, (note - 69) / 12.0);

    //  the same note again is the same order, refreshed
    for (auto& c : cmds)
        if (c.note == note && c.env > 1e-4)
        {
            c.held = true; c.vel = vel; c.age = ++cmdClock;
            c.tgt = 0.22f + 0.78f * vel * clampf (p.heed, 0.0f, 1.0f);
            return;
        }

    //  a free slot, else the oldest released order, else the oldest
    int slot = -1;
    for (int i = 0; i < MAXCMD; ++i) if (cmds[i].env <= 1e-4 && ! cmds[i].held) { slot = i; break; }
    if (slot < 0)
    {
        long long oldest = 1ll << 62;
        for (int i = 0; i < MAXCMD; ++i)
            if (! cmds[i].held && cmds[i].age < oldest) { oldest = cmds[i].age; slot = i; }
        if (slot < 0)
        {
            oldest = 1ll << 62;
            for (int i = 0; i < MAXCMD; ++i)
                if (cmds[i].age < oldest) { oldest = cmds[i].age; slot = i; }
        }
    }

    Cmd& c = cmds[slot];
    c.note = note; c.vel = vel; c.held = true; c.age = ++cmdClock;
    c.fCmd = fCmd;
    c.tgt = 0.22f + 0.78f * vel * clampf (p.heed, 0.0f, 1.0f);
    c.env = 0.0f;
    c.duct = pickDuct (fCmd);

    /*  The valve opens. Every order begins with a real transient — the limit
        cycle must grow FROM something, and the house rule forbids noise in a
        loop. A pump switching on makes a click; that click is the seed. */
    Duct& d = duct[c.duct];
    startPulse (d, 0.06f + 0.20f * vel, 0.003);
    d.active = 1;
}

/*  Every transient that seeds a cycle goes through here. At CHARACTER 0 the
    scale is exactly 1.0 and the arithmetic is the old (int)(sec * sr); the
    material shortens it to a sharp click (BELL, GLASS) or lengthens it to a
    soft breath (HORN). */
void Engine::startPulse (Duct& d, float amp, double baseSec)
{
    d.pulseAmp = amp;
    d.pulseLeft = std::max (32, (int) (baseSec * (double) mPulse * sr));
    d.pulsePh = 0; d.pulseInc = 1.0 / d.pulseLeft;
}

//==============================================================================
void Engine::strike (int ductIdx, float amp)
{
    while (pokeLock.exchange (1)) {}
    if (pokes.size() < 128) pokes.push_back ({ ductIdx, amp, 0.0f });
    pokeLock.store (0);
}

void Engine::pour (int ductIdx, float kelvin)
{
    while (pokeLock.exchange (1)) {}
    if (pokes.size() < 128) pokes.push_back ({ ductIdx, 0.0f, kelvin });
    pokeLock.store (0);
}

void Engine::spin (float dA, float dB)
{
    while (pokeLock.exchange (1)) {}
    spinQA += dA; spinQB += dB;
    pokeLock.store (0);
}

void Engine::setWorldMod (float detCents, float sag01, float tremDepth,
                          float tremRate, float filterMul, float panSpread)
{
    wmDet = detCents; wmSag = sag01; wmTremD = tremDepth; wmTremR = tremRate;
    wmFilt = filterMul; wmSpread = panSpread;
    /*  the neutrality guard: an untouched rack must leave this engine
        byte-identical, so neutral means NOTHING below is evaluated */
    wmActive = ! (detCents == 0.0f && sag01 == 0.0f && tremDepth == 0.0f
                  && std::abs (filterMul - 1.0f) < 1.0e-9f && panSpread == 0.0f);
}

//==============================================================================
void Engine::updateSection()
{
    const double h = ((double) p.section * 2.0 - 1.0) * 0.85;
    const double veilW = 0.10 + 0.55 * (double) p.veil;
    const double ca = std::cos (rotA), sa = std::sin (rotA);
    const double cb = std::cos (rotB), sb = std::sin (rotB);

    //  the same slope as the control tick: cold restrains the dissonance and
    //  the chaos, whatever the knobs say — below ~350 K the object is civil
    const double warmthS = clampd ((double) p.ambient, 0.0, 1.0);
    const float stiffG = (float) (clampf (p.stiffness, 0.0f, 1.0f) * (0.30 + 0.70 * warmthS));
    const float turbG  = (float) (clampf (p.turbulence, 0.0f, 1.0f) * (0.15 + 0.85 * warmthS));
    for (int i = 0; i < geo.nDuct; ++i)
    {
        const float* m = geo.mid[i];
        //  the double rotation. (x,w) is the section + stereo; (y,z) is drawn,
        //  and now also HEARD — the 4D position shapes this conduit's timbre.
        const double x  = m[0] * ca - m[3] * sa;
        const double w  = m[0] * sa + m[3] * ca;
        const double y  = m[1] * cb - m[2] * sb;
        const double z  = m[1] * sb + m[2] * cb;

        const double d = std::abs (w - h);
        sigma[i] = d >= veilW ? 0.0f
                 : (float) (0.5 * (1.0 + std::cos (PI * d / veilW)));

        //  stereo IS the geometry: the projected screen position of the
        //  midpoint under the same rotation the panel draws
        const double s = 1.0 / (1.06 - w);
        const double px = x * s, py = y * s;

        /*  THE 4D BODY REACHES THE EAR. What you SEE is where a conduit sits in
            the projection; that same position now sets what you HEAR, so the
            picture and the timbre are one object and turning the body (the
            PRECESSION you watch) re-voices the web.

              projected RADIUS from the pole  -> BRIGHTNESS (near the void =
                dark and close; far out = bright). As the body turns a conduit
                sweeps in and out, and its brightness sweeps with it.
              DEPTH z of the projection       -> INHARMONICITY (how stiff, how
                bell-like this conduit's metal is).
              the OTHER depth, via py          -> the heat-release DELAY, i.e.
                which conduits sit near their bifurcation and go turbulent
                first. */
        const double pr = std::min (2.5, std::hypot (px, py));   // 0..~2.5
        duct[i].bright   = (float) (0.30 + 2.40 * clampd (pr / 1.5, 0.0, 1.0));
        duct[i].stiffLoc = (float) (stiffG * (0.25 + 0.75 * clampd (0.5 + 0.5 * z, 0.0, 1.0)));
        /*  The heat-release delay tau, as a fraction of the period. This is the
            whole of the regime, and Rayleigh's criterion is why: heat added IN
            PHASE (tau -> 0) is pure negative damping — it sustains the cycle
            without pulling its frequency, so the tone is clean and dead in
            tune. A LAGGING drive both detunes the cycle and, past a point,
            period-doubles it. So tau is zero when TURBULENCE is zero (clean,
            exact), and grows with it — the (y,z) depth deciding WHICH conduits
            tip into chaos first, which is the geometry you can see. */
        const double depth = clampd (0.5 + 0.5 * py / std::max (0.3, s), 0.0, 1.0);
        duct[i].tauFrac  = (float) (turbG * (0.10 + 0.55 * depth));

        double pan = 0.5 + 0.45 * std::tanh (px * 0.85);
        if (wmActive && wmSpread > 0.0f)
        {
            const double fan = std::fmod (i * 0.618033988749895 + 0.5, 1.0) * 2.0 - 1.0;
            pan = clampd (pan + fan * wmSpread * 0.35, 0.0, 1.0);
        }
        //  equal-power, into slewed targets applied per sample
        const double a = pan * PI * 0.5;
        const double radBase = (double) sigma[i];
        double rl = radBase * std::cos (a);
        double rr = radBase * std::sin (a);
        //  keep an ordered conduit audible even if the section has turned away
        for (const auto& c : cmds)
            if (c.duct == i && c.env > 1e-4)
            {
                const double floorRad = 0.75 * (double) c.env;
                if (radBase < floorRad) { rl = floorRad * std::cos (a); rr = floorRad * std::sin (a); }
                break;
            }
        duct[i].radL = (float) rl;   // targets; radCurL/R slew towards them
        duct[i].radR = (float) rr;
    }
}

void Engine::spawnPacket (uint64_t why)
{
    Rng r (pktSeed ^ why * 0x2545F4914F6CDD1Dull);
    pktSeed = pktSeed * 6364136223846793005ull + 1442695040888963407ull;
    for (auto& k : pkts)
        if (! k.live)
        {
            k.live = 1;
            k.len = 2 + r.below (4);
            int at = r.below (std::max (1, geo.nDuct));
            for (int e = 0; e < k.len; ++e)
            {
                k.path[e] = at;
                if (geo.nNbd[at] > 0) at = geo.nbd[at][r.below (geo.nNbd[at])];
            }
            k.t0 = (double) clockSamples / sr;
            k.perEdge = r.range (0.8, 2.6);
            const double tAmb = ambientKelvin (p.ambient);
            //  enough that at a hot site the packet reliably carries a conduit
            //  over onset — the grid self-singing is the whole "energy grid"
            //  hypothesis, and it must actually happen when warm. A warm grid
            //  also simply carries MORE: the multiplier is ~1 at 234 K, 0.4
            //  frozen, 1.66 at 800 K.
            const double warmth = clampd ((double) p.ambient, 0.0, 1.0);
            k.heat = (float) (tAmb * (double) p.traffic * (0.40 + 1.26 * warmth) * r.range (0.60, 2.20));
            return;
        }
}

//==============================================================================
void Engine::controlTick (int samples)
{
    const double dt = samples / sr;
    const double tSec = (double) clockSamples / sr;
    const double tAmb = ambientKelvin (p.ambient);

    //  queued gestures, applied where they cannot interrupt anything
    {
        while (pokeLock.exchange (1)) {}
        for (const auto& pk : pokes)
        {
            if (pk.duct < 0 || pk.duct >= geo.nDuct) continue;
            if (pk.amp != 0.0f)
            {
                Duct& d = duct[pk.duct];
                startPulse (d, clampf (pk.amp, 0.0f, 0.6f), 0.003);
                d.active = 1;
            }
            if (pk.kelvin != 0.0f)
                T[pk.duct] = clampd (T[pk.duct] + pk.kelvin, T_DUCT_LO, T_DUCT_HI);
        }
        pokes.clear();
        rotA += spinQA; rotB += spinQB;
        spinQA = spinQB = 0;
        pokeLock.store (0);
    }

    //  the precession — slow, and slower still when the hand is not on it
    const double turnRate = 0.0012 + 0.030 * (double) p.turn * (double) p.turn;
    rotA += turnRate * dt;
    rotB += turnRate * 0.618 * dt;

    /*  THE SLOPE. Cold is order, heat is disorder — one axis, monotonic.
        `warmth` is the ambient position on the exponential 77-800 K scale;
        the gates below are what it opens:
          wAuto — whether orphaned heat is allowed to SING on its own. Nearly
                  shut below ~350 K (a released note stops; the grid is quiet),
                  wide at 800 (the grid carries unplayed, notes drone on).
          wTurb — how much of TURBULENCE's chaos the site permits. Cold is
                  clean even with the knob up; the knob's full range lives in
                  the heat.
          wHunt — how much the servo is allowed to wander. Cold obeys the
                  MIDI note.
        The leak also quickens when cold — a cold site reclaims heat fast,
        which is what "notes stop when you let go" is, physically. */
    const double warmth = clampd ((double) p.ambient, 0.0, 1.0);
    const double wAuto  = 0.10 + 0.90 * std::pow (clampd ((warmth - 0.45) / 0.55, 0.0, 1.0), 1.5);
    const double wTurb  = 0.15 + 0.85 * warmth;
    const double wHunt  = 0.25 + 0.75 * warmth;
    const float  turbG = (float) (clampf (p.turbulence, 0.0f, 1.0f) * wTurb);

    /*  THE MATERIAL, voiced by CHARACTER. Every trait is a lerp from the old
        engine's value (at ch = 0) to the specimen's own, and the old value is
        always an exact 1.0 or 0.0, so ch = 0 multiplies by one and adds zero.
        STEEPENING finally does what its name says: it scales how hard the
        wave leans (the instantaneous nonlinearity) and how far the crest
        overtakes the trough (the shock), as well as the radiation it set
        before. */
    const Material& mt = geo.mat;
    const float ch = clampf (p.character, 0.0f, 1.0f);
    chOn = ch > 0.0f;
    if (chOn)
    {
        const float st = clampf (p.steepen, 0.0f, 1.0f);
        mDrive   = ch * mt.drive * (0.45f + 1.55f * st);
        /*  ...and it gives way to TURBULENCE: a cycle held hard by its medium
            is a stable cycle, and chaos needs the delayed King's law in
            charge — measured, the material had pinned TURBULENCE 1 to a
            flatness of 0.010 where the route to chaos reaches 0.23. */
        mBoost   = ch * std::max (0.0f, mt.drive - 0.3f) * 1.60f * (0.40f + 1.70f * st)
                   * (1.0f - turbG) * (1.0f - turbG);
        mFold    = ch * mt.fold;
        mShock   = ch * mt.shock * (0.30f + 2.00f * st);
        mSkin    = ch * mt.skin;
        //  the slow envelope loss steps aside as the instantaneous one takes
        //  over the limiting — that hand-over IS the timbre
        mEnvLoss = 1.0f - 0.80f * ch * clampf (mt.drive / 1.6f, 0.0f, 1.0f) * (1.0f - turbG);
        mPulse   = 1.0f + ch * (mt.pulseMs / 3.0f - 1.0f);
        mDoublet = ch * mt.doublet;
        mSym     = 1.0f + ch * (mt.sympathy - 1.0f);
        //  a blow, not a click: a doublet has almost nothing at a bass
        //  conduit's wall modes (a bell's first mode on E2 is 226 Hz), so the
        //  walls take the ONE-SIDED puff, and a narrow wall resonance needs a
        //  strong one (its input gain is sin(w)/2Q, ~0.001 there)
        mStrike  = 40.0f * ch * mt.modeBase;
        mInertia = 1.0f + ch * (mt.inertia - 1.0f);
        //  family trim (measured at E2/E3) plus 3 dB for the bass register,
        //  where the old engine was hottest (it fell ~6 dB an octave)
        mTrim    = std::pow (10.0f, ch * (mt.trimDb + 3.0f) / 20.0f);

        /*  The vessel's two formants. A vessel resonance is a length over the
            speed of sound, so it follows sqrt(ambient) — the site's heat moves
            the vowel, while the ordered notes stay put. */
        const double cs = std::sqrt (tAmb / T_REF);
        const float fq[2] = { mt.f1, mt.f2 }, gq[2] = { mt.g1db, mt.g2db }, qq[2] = { mt.q1, mt.q2 };
        for (int k = 0; k < 2; ++k)
        {
            const double fk = clampd ((double) fq[k] * cs, 60.0, sr * 0.42);
            const double w0 = 2.0 * PI * fk / sr, al = std::sin (w0) / (2.0 * (double) qq[k]);
            const double a0 = 1.0 + al;
            bodyB0[k] = (float) (al / a0);
            bodyA1[k] = (float) (-2.0 * std::cos (w0) / a0);
            bodyA2[k] = (float) ((1.0 - al) / a0);
            bodyG[k]  = (float) (std::pow (10.0, (double) ch * gq[k] / 20.0) - 1.0);
        }
    }
    else
    {
        mDrive = mBoost = mFold = mShock = mSkin = mDoublet = mStrike = 0.0f;
        mEnvLoss = mPulse = mSym = mInertia = mTrim = 1.0f;
    }

    //  the climate's timescales
    const double coldMul = clampd (std::sqrt (tAmb / T_REF), 0.30, 1.7);
    const double massMul = xmapd (p.mass, 0.35, 4.0);
    //  MASS owns the glide; the material's inertia shapes the ATTACK only
    const double tauT    = xmapd (1.0 - (double) p.flux, 0.02, 1.2) * massMul / coldMul;
    const double tauLeak = xmapd (1.0 - (double) p.leak, 0.15, 8.0) * massMul
                           * (0.22 + 0.78 * warmth);   // a cold site reclaims heat fast
    const double tauAtk  = xmapd (1.0 - (double) p.flux, 0.004, 0.9) * massMul / coldMul * (double) mInertia;
    const double tauRel  = xmapd (1.0 - (double) p.leak, 0.03, 2.5) * massMul;
    const double condRate = xmapd (p.conduction, 0.002, 0.8);
    const double thetaOn = 0.22 + 1.85 * (double) p.onset * (double) p.onset;
    const double tuneSemis = ((double) p.tune - 0.5) * 24.0;

    //  traffic: wandering heat, on the object's own clock, deterministic
    if (p.traffic > 0.001f)
    {
        /*  THE SITE. On a shared bench, cold and close, the grid's traffic
            falls into step with the other findings: the local site phase is
            eased onto the negotiated one, and each packet is scheduled on
            the site's nearest wrap instead of on its own clock. At pull 0
            not one line of this runs. */
        const float sPull = sitePull.load();
        if (sPull > 0.0f)
        {
            double diff = (double) sitePhaseIn.load() - sitePh;
            diff -= std::floor (diff + 0.5);
            sitePh += diff * 0.35 + std::max (0.05, (double) siteHz.load()) * dt;
            sitePh -= std::floor (sitePh);
        }
        const double trEff = clampd ((double) p.traffic * (0.40 + 1.26 * warmth), 0.0, 1.0);
        if (tSec >= nextPktAt)
        {
            spawnPacket ((uint64_t) (tSec * 977.0));
            Rng r (pktSeed);
            double nat = tSec + xmapd (1.0 - trEff, 0.8, 14.0) * r.range (0.5, 1.5);
            if (sPull > 0.0f)
            {
                const double hz = std::max (0.05, (double) siteHz.load());
                double cand = tSec + (1.0 - sitePh) / hz;          // the next wrap
                const double n = std::floor ((nat - cand) * hz + 0.5);
                cand += std::max (0.0, n) / hz;                     // ...nearest the natural time
                nat += (cand - nat) * (double) sPull * 0.85;
            }
            nextPktAt = nat;
        }
        /*  the seven strokes. Near 234 K — and only there — the traffic keeps
            a figure. The survey recorded it and declined to speculate. */
        if (std::abs (tAmb - 234.0) < 2.0)
        {
            static const double FIGURE[7] = { 0.0, 0.14, 0.21, 0.43, 0.57, 0.71, 0.86 };
            const double cyc = 1.85;
            const double ph = std::fmod (tSec, cyc) / cyc;
            const int stepNow = (int) (std::fmod (tSec, cyc * 64.0) / cyc);
            for (int q = 0; q < 7; ++q)
                if (ph >= FIGURE[q] && carrierStep < stepNow * 7 + q)
                {
                    carrierStep = stepNow * 7 + q;
                    spawnPacket (0xCA104ull + (uint64_t) carrierStep);
                }
        }
    }
    //  an order holds its valve against the traffic too: a packet crossing a
    //  commanded conduit used to shove its temperature — measured, +8 cents
    //  on a held bass note whenever the grid was busy near it
    float holdOf[MAXDUCT] = {};
    for (const auto& c : cmds)
        if (c.duct >= 0 && c.env > 1e-4)
            holdOf[c.duct] = std::max (holdOf[c.duct], (float) clampd (c.env * 4.0, 0.0, 1.0));

    for (auto& k : pkts)
    {
        if (! k.live) continue;
        const double e = (tSec - k.t0) / k.perEdge;
        const int at = (int) e;
        if (at >= k.len) { k.live = 0; continue; }
        const int d = k.path[at];
        if (d >= 0 && d < geo.nDuct)
            T[d] = clampd (T[d] + (double) k.heat * (dt / k.perEdge) * (1.0 - 0.995 * holdOf[d]),
                           T_DUCT_LO, T_DUCT_HI);
    }

    //  gate for the world-mod sag
    {
        bool any = false;
        for (const auto& c : cmds) if (c.held && c.env > 0.02f) { any = true; break; }
        gateSm += (float) ((any ? 1.0 : 0.0) - gateSm) * (float) (1.0 - std::exp (-dt / 0.05));
    }

    //  the orders: envelopes, thermal servo, chuff
    for (auto& c : cmds)
    {
        if (c.duct < 0) continue;
        const float tgtNow = c.held || sustain ? c.tgt : c.tgt * clampf (p.retain, 0.0f, 1.0f);
        const double tau = (c.held || sustain) || tgtNow > c.env ? tauAtk : tauRel;
        c.env += (float) ((tgtNow - c.env) * (1.0 - std::exp (-dt / std::max (1e-4, tau))));
        if (! c.held && ! sustain && c.env < 1e-4 && p.retain < 0.01f) { c.env = 0; continue; }

        //  the thermal order: the temperature whose resonance is the note
        double fWant = c.fCmd * std::pow (2.0, (double) bendSemis / 12.0);
        if (wmActive && wmSag > 0.0f)
            fWant *= std::pow (2.0, -(double) wmSag * (double) gateSm / 12.0);
        const double fPas = (double) geo.fPassive[c.duct];
        const double tTgt = clampd (T_REF * (fWant / fPas) * (fWant / fPas), T_DUCT_LO, T_DUCT_HI);
        if (c.env > 1e-4)
            T[c.duct] += (tTgt - T[c.duct]) * (1.0 - std::exp (-dt / std::max (1e-4, tauT)))
                         * clampd (c.env * 4.0, 0.0, 1.0);

        /*  The stammer: discrete, quickening, gone once the song holds. A
            countdown, not a modulo — the first draft gated on
            fmod(t, period) < dt with the period SHRINKING, and the phase
            jumped over its own window: two thumps where five were due. The
            window is on the DISEQUILIBRIUM (e), not the amp envelope: a cold
            conduit spends real time below onset earning its heat, and that is
            exactly when a thermoacoustic engine stammers. */
        //  the disequilibrium THIS conduit is at, computed here (the conduit
        //  loop's own `e` is later); the stammer is a pre-song phenomenon, so
        //  it fires while the cell is heating past onset but not yet singing
        const double lnR = std::max (0.0, std::log (T[c.duct] / tAmb));
        const double eAutoC = std::max (0.0, lnR / thetaOn - 1.0) * 0.9 * wAuto;
        const double eCmd = (double) c.env + std::min (1.2, eAutoC);
        /*  ...and GONE ONCE THE SONG HOLDS, which the eCmd window alone never
            achieved: a held order at ordinary velocity sits at eCmd ~0.7, so
            the stammer went on for the whole note (measured, 46 pulses in a
            4 s A1 — a click train between the harmonics, -51 dB, the bench's
            "aliasing"). With the material voiced, the stammer ends when the
            conduit is actually singing; CHARACTER 0 keeps the old behaviour. */
        const bool songHolds = chOn && duct[c.duct].ringEnv > 0.10f && duct[c.duct].heatDrive > 0.010f;
        if (p.chuff > 0.01f && (c.held || sustain) && eCmd > 0.02 && eCmd < 0.92 && ! songHolds)
        {
            if (c.chuffAt <= 0.0) c.chuffAt = tSec + 0.03;
            if (tSec >= c.chuffAt)
            {
                Duct& d = duct[c.duct];
                startPulse (d, clampf (p.chuff, 0.0f, 1.0f) * (0.14f + 0.5f * c.vel), 0.003);
                chuffFired.fetch_add (1);
                const double prog = clampd (eCmd / 0.92, 0.0, 1.0);
                c.chuffAt = tSec + (0.24 - 0.19 * prog) * (0.55 + massMul * 0.30);
            }
        }
        else c.chuffAt = 0.0;
    }

    /*  Section and stereo targets FIRST, because the trem below rides on
        them — writing the targets afterwards would erase it every tick,
        which is exactly the kind of working-but-invisible this house keeps
        finding the hard way. */
    updateSection();

    //  every conduit: leak, conduction, resonance, loop coefficients. The
    //  loop's own loss filter is set PER CONDUIT below (conduit-relative to
    //  its fundamental); the only global here is the DC blocker.
    const double RDC = dcR;
    const double wmFiltMul = wmActive ? (double) wmFilt : 1.0;

    int nSing = 0;
    float power = 0.0f;
    tremPh += dt;

    for (int i = 0; i < geo.nDuct; ++i)
    {
        double acc = 0;
        for (int q = 0; q < geo.nNbd[i]; ++q) acc += T[geo.nbd[i][q]] - T[i];
        condAcc[i] = acc / std::max (1, geo.nNbd[i]);
    }

    for (int i = 0; i < geo.nDuct; ++i)
    {
        //  disequilibrium: what a held order demands, plus what the heat
        //  itself has earned past onset
        double demand = 0;
        for (const auto& c : cmds)
            if (c.duct == i && c.env > demand) demand = c.env;

        /*  Heat forgets towards ambient — but AN ORDER HOLDS THE VALVE. The
            first draft let the servo and the leak fight, and a held note
            settled at an equilibrium BETWEEN its target and ambient: +76
            cents of "wobble" that was really the offset moving, and 79 cents
            of drift when ambient changed under a held order. The servo must
            win while the order stands; the leak is what release means. */
        const double hold = clampd (demand * 4.0, 0.0, 1.0);
        T[i] += (tAmb - T[i]) * (1.0 - std::exp (-dt / std::max (1e-3, tauLeak))) * (1.0 - 0.995 * hold);
        //  an ordered conduit still RADIATES heat into the web (its neighbours
        //  read its temperature through condAcc), but does not drift with it
        T[i] = clampd (T[i] + condRate * condAcc[i] * dt * (1.0 - 0.995 * hold), T_DUCT_LO, T_DUCT_HI);
        /*  DIRECTIONAL, GATED, AND NOT SELF-SUPPLIED. Three fixes in one line,
            each one of Peter's reports:
            - max(0, ln...) not |ln|: only heat ABOVE ambient sings. The old
              absolute value was symmetric about ambient, which is why the
              instrument MIRRORED — cooling past the calm point read as heat.
            - x wAuto: self-song is a hot-site phenomenon. Cold, orphaned heat
              just cools; the drone lives above ~350 K.
            - x (1 - hold): a conduit under order does not count its own servo
              heat as excess — the note that "activated some kind of drone
              mode, a self-supplying state" was doing exactly that. */
        const double lnRatio = std::max (0.0, std::log (T[i] / tAmb));
        const double eAuto = std::max (0.0, lnRatio / thetaOn - 1.0) * 0.9 * wAuto * (1.0 - hold);
        const double e = demand + std::min (1.2, eAuto);

        /*  THE HEAT DRIVE — the prefactor on King's law in the sample loop.
            The loop gain itself is a fixed LOSS (d.g = 0.995); this drive is
            what carries the conduit over unity and sustains the cycle, and how
            hard it pushes past the clean cycle is TURBULENCE. Below onset e is
            ~0, the drive is ~0, and the loss wins — silent, or a struck ring
            dying. That is the same onset the bench already checks. */
        Duct& d = duct[i];
        //  a solid base so the CLEAN cycle (turbulence 0) is a healthy tone
        //  well above the loss, plus TURBULENCE on top to push it past the
        //  clean cycle. In phase (tau ~ 0) even a big drive only grows the
        //  amplitude until King's law saturates — it cannot period-double
        //  without the delay, so a strong clean tone is safe.
        //  capped: past this the drive can only latch the loop, never sing it
        const double driveTgt = std::min (0.30, clampd (e, 0.0, 2.0) * (0.16 + 0.55 * (double) turbG));
        d.heatDrive += (float) ((driveTgt - d.heatDrive) * (1.0 - std::exp (-dt / 0.010)));
        const bool singing = d.heatDrive > 0.010f;
        /*  The loop LOSS. Undriven it is a real damping (Q ~ 30, so a struck
            conduit rings a moment and dies — the onset bench depends on it);
            driven, it opens towards unity so the heat sustains a clean cycle
            rather than fighting an over-damped one. */
        d.g = 0.968f + 0.029f * clampf (d.heatDrive * 6.0f, 0.0f, 1.0f);
        if (singing && d.pulseLeft <= 0 && d.ringEnv < 1e-3f)
        {
            //  the cycle must grow FROM a transient (no noise in a loop): pop
            startPulse (d, 0.05f, 0.002);
        }
        if (singing) ++nSing;

        //  the resonance now — sqrt(T), the whole of the tuning law —
        //  plus the imperfect servo (DISCIPLINE) and the bus detune fan
        double f = (double) geo.fPassive[i] * std::sqrt (T[i] / T_REF);
        //  a passive conduit follows TUNE too, so struck rings and orders agree
        f *= std::pow (2.0, tuneSemis / 12.0);
        const double hunt = (1.0 - (double) p.discipline) * wHunt
                          * 9.0 * std::sin (2.0 * PI * (0.07 + 0.5 * huntPh[i]) * tSec
                                            + huntPh[i] * 17.0);
        double cents = hunt;
        if (wmActive)
        {
            const double fan = std::fmod (i * 0.618033988749895 + 0.5, 1.0) * 2.0 - 1.0;
            cents += fan * (double) wmDet;
            if (wmSag > 0.0f && demand < 1e-6)
                f *= std::pow (2.0, -(double) wmSag * (double) gateSm / 12.0);
        }
        f *= std::pow (2.0, cents / 1200.0);
        fNow[i] = clampd (f, 12.0, 1600.0);

        /*  THE LOOP LOSS is a WIDE one-pole at a fixed multiple of this
            conduit's fundamental — wide, so the harmonics survive in the loop
            and the analytic delay compensation holds the tuning to well under
            a cent. Two darker ideas were tried and thrown out here: a boxcar
            (its phase jumps pi at each null — 80 cents of catalogue-wide
            detune) and a LOW-cornered one-pole (its steep phase near the
            fundamental cost six cents). The TIMBRE is darkened instead on the
            radiation tap below, OUTSIDE the loop, where a filter cannot
            detune anything. */
        const double fc = clampd (fNow[i] * 14.0, fNow[i] * 3.0, sr * 0.45);
        const double aLPd = 1.0 - std::exp (-2.0 * PI * fc / sr);
        d.lpA = (float) aLPd;
        if (chOn)
        {
            //  the pump's resonance, on this conduit's pitch (see the medium)
            const double wp = 2.0 * PI * clampd (fNow[i], 10.0, sr * 0.4) / sr;
            const double al = std::sin (wp) / (2.0 * 1.2), a0 = 1.0 + al;
            d.pB0 = (float) (al / a0);
            d.pA1 = (float) (-2.0 * std::cos (wp) / a0);
            d.pA2 = (float) ((1.0 - al) / a0);
            const double as = std::sin (wp) / (2.0 * 4.0), s0 = 1.0 + as;
            d.sB0 = (float) (as / s0);
            d.sA1 = (float) (-2.0 * std::cos (wp) / s0);
            d.sA2 = (float) ((1.0 - as) / s0);
            d.perExp = sr / fNow[i];
        }
        /*  the radiation lowpass, i.e. the brightness. STEEPENING sets the
            base, and the 4D BRIGHTNESS (d.bright, from the projected radius)
            multiplies it — so a conduit far out in the projection is bright and
            one near the void is dark, and turning the body sweeps the timbre.
            This is the see-hear link on the radiation side. */
        double radMulM = 1.0;
        if (chOn) radMulM = 1.0 + (double) ch * ((double) mt.radMul - 1.0);
        const double fcr = clampd (fNow[i] * xmapd (p.steepen, 1.20, 20.0) * (double) d.bright
                                   * (0.55 + 0.55 * warmth) * wmFiltMul * radMulM,
                                   fNow[i] * 1.05, sr * 0.45);
        d.radA = (float) (1.0 - std::exp (-2.0 * PI * fcr / sr));

        /*  STIFFNESS: two inharmonic modal resonators, tuned to ratios of the
            fundamental that are on no harmonic grid. Per-conduit ratios (from
            the golden-angle phase already used for the servo) spread them so no
            two conduits ring the same metal. Q rises and gain opens with
            STIFFNESS; they colour the radiated tap, never the loop. */
        const double w = 2.0 * PI * fNow[i] / sr;
        {
            float sl = d.stiffLoc;
            double base[2] = { 2.18, 3.63 };
            double jitW = 0.6, qMul = 1.0;
            if (chOn)
            {
                //  the specimen's own metal: its wall ratios, how loudly they
                //  ring even with STIFFNESS low, and how long. The per-conduit
                //  jitter narrows so the family's ratios stay recognisable.
                for (int mmi = 0; mmi < 2; ++mmi)
                    base[mmi] += (double) ch * ((double) mt.modeR[mmi] - base[mmi]);
                //  the material sets how loudly its metal rings and keeps
                //  STIFFNESS's whole travel (a bell already ringing at full
                //  left the knob nothing to do): a little longer Q, and a
                //  multiplier on the ring below
                sl = std::min (1.0f, sl + 0.35f * ch * mt.modeBase * (1.0f - sl));
                jitW = 0.6 - 0.45 * (double) ch;
                /*  and how LONG it rings: at the old Q of 10-20 a wall mode at
                    226 Hz died in ~17 ms — a cushion, not metal. The metal-rich
                    media ring up to twenty times longer (a bell on E2: ~0.2 s),
                    and the strike is scaled to match (ringStrike), since a
                    narrower resonance takes in proportionally less of a blow. */
                const double ringMul = 1.0 + 20.0 * (double) (ch * mt.modeBase);
                qMul = (1.0 + (double) ch * ((double) mt.modeQ - 1.0)) * ringMul;
                d.ringStrike = (float) ringMul;
            }
            for (int mmi = 0; mmi < 2; ++mmi)
            {
                //  an inharmonic ratio, jittered per conduit
                const double jit = jitW * (std::fmod (huntPh[i] * (mmi + 3) * 0.6180339887, 1.0) - 0.5);
                const double fr = clampd (fNow[i] * (base[mmi] + jit), 30.0, sr * 0.45);
                const double wm = 2.0 * PI * fr / sr;
                const double Q  = (8.0 + 24.0 * (double) sl) * qMul;
                const double r  = std::exp (-wm / (2.0 * Q));
                d.mA1[mmi] = (float) (2.0 * r * std::cos (wm));
                d.mA2[mmi] = (float) (r * r);
                //  input scale ~ (1 - r) keeps the resonator peak near unity;
                //  STIFFNESS opens how loud the inharmonic ring is on the tap
                d.mB[mmi]  = (float) ((1.0 - r) * (0.5 + 6.0 * (double) sl));
                if (chOn)
                {
                    const double al = std::sin (wm) / (2.0 * Q), a0 = 1.0 + al;
                    d.nB0[mmi] = (float) (al / a0);
                    d.nA1[mmi] = (float) (2.0 * std::cos (wm) / a0);
                    d.nA2[mmi] = (float) ((1.0 - al) / a0);
                    //  unity peak, times how loudly this metal rings
                    d.nG[mmi]  = (float) ((0.35 + 3.2 * (double) d.stiffLoc) * (1.0 + 2.2 * (double) (ch * mt.modeBase)));
                }
            }
        }

        //  loop delay: the period, less what the loop's own filters delay.
        //  Both filter phase delays are analytic, and the 3 allpass stages'
        //  group delay at f0 is subtracted too, so the FUNDAMENTAL stays exact
        //  while the partials disperse off the harmonic grid.
        const double bLP = 1.0 - aLPd;
        const double dLP = std::atan2 (bLP * std::sin (w), 1.0 - bLP * std::cos (w)) / w;
        const double phDC = (PI - w) * 0.5 - std::atan2 (RDC * std::sin (w), 1.0 - RDC * std::cos (w));
        const double dDC = -phDC / w;
        /*  The end condition in force. The material moves the threshold on
            each conduit's own draw — but only while the conduit is silent: a
            sounding loop that changed its end would halve its delay mid-note. */
        if (d.pulseLeft <= 0 && d.ringEnv < 1.0e-5f && demand < 1e-4)
        {
            const double thr = chOn ? 0.4 + (double) ch * ((double) mt.closedFrac - 0.4) : 0.4;
            closedNow[i] = geo.closedU[i] < thr ? 1 : 0;
        }
        const double period = closedNow[i] ? sr / (2.0 * fNow[i]) : sr / fNow[i];
        double dtTarget = period - dLP - dDC;
        if (chOn && mDrive > 1.0e-4f) dtTarget -= 0.5;   // the ADAA's half sample
        /*  THE PITCH SERVO for a saturated loop. A cycle driven deep into its
            medium settles where its OVERTONES' phase closures compromise, not
            where the fundamental alone would — measured, up to 13 cents off,
            different for every material, so no analytic correction holds. The
            conduit hears its own fundamental (the pump resonance IS the
            fundamental, from its zero crossings) and trims its delay towards
            the order, slowly, only while the tone is clean and singing. A
            chaotic or dying loop keeps the correction it had. */
        if (chOn)
        {
            const bool idle = d.ringEnv < 1.0e-4f && d.pulseLeft <= 0 && demand < 1e-4;
            if (idle) { d.corr = 1.0; d.perSm = 0; d.zcLast = -1.0; }
            else if (d.perSm > 0.0f && d.ringEnv > 0.02f && d.chaos < 0.30f && mBoost > 0.0f)
            {
                const double ratio = clampd (d.perExp / (double) d.perSm, 0.985, 1.015);
                d.corr = clampd (d.corr * std::pow (ratio, 1.0 - std::exp (-dt / 0.10)), 0.90, 1.10);
            }
            dtTarget *= d.corr;
        }

        //  the heat-release delay tau, a fraction of the period (geometry-set),
        //  slewed. This is a SECOND tap, off the tuning path, so it phases the
        //  King's-law drive without moving the pitch.
        const double tauTgt = clampd ((double) d.tauFrac * period, 2.0, (double) lineLen - 5.0);
        d.tauSm = d.tauSm <= 0 ? (float) tauTgt : d.tauSm;
        d.tauSm += (float) ((tauTgt - d.tauSm) * (1.0 - std::exp (-dt / 0.010)));
        d.dtSm = d.dtSm <= 0 ? (float) dtTarget : d.dtSm;
        dtTarget = clampd (dtTarget, 4.0, (double) lineLen - 5.0);
        //  slew at control rate; the sample loop interpolates linearly
        d.dtSm += (float) ((dtTarget - d.dtSm) * (1.0 - std::exp (-dt / 0.010)));

        /*  Is it worth running at audio rate at all? A conduit with no energy
            of its own still wakes when a NEIGHBOUR carries — the junction
            bleed has to have somewhere to arrive — but an idle web runs no
            loops at all, which is both the CPU budget and the exact-silence
            guarantee at the defaults. */
        float nbdRing = 0.0f;
        for (int q = 0; q < geo.nNbd[i]; ++q)
            nbdRing = std::max (nbdRing, duct[geo.nbd[i][q]].ringEnv);
        const bool ordered = demand > 1e-4;
        d.active = (ordered || d.pulseLeft > 0 || d.ringEnv > 1.0e-6f
                    || (sigma[i] > 0.012f && nbdRing > 1.0e-5f)) ? 1 : 0;

        /*  A harder-driven cell radiates more. The loop's own limit-cycle
            amplitude moves only as sqrt of the gain excess — measured, 3 dB
            between a soft and a hard order — so the radiation carries the
            rest of the dynamics, which is where velocity lives. */
        {
            //  quadratic in the disequilibrium, so velocity has real dynamic
            //  range — a King's-law cycle's own amplitude barely moves (it
            //  saturates), so the radiation has to carry the loud/soft
            const double ee = clampd (e, 0.0, 1.0);
            const double radMulE = 0.10 + 0.90 * ee * ee;
            d.radL = (float) (d.radL * radMulE);
            d.radR = (float) (d.radR * radMulE);
        }

        //  trem rides the radiation, fanned per conduit
        if (wmActive && wmTremD > 0.0f)
        {
            const double fanPh = huntPh[i] * 2.0 * PI;
            const double rateFan = 1.0 + 0.13 * (huntPh[i] - 0.5);
            const double lfo = 0.5 * (1.0 + std::sin (2.0 * PI * (double) wmTremR * rateFan * tremPh + fanPh));
            const double m = 1.0 - (double) wmTremD * 0.85 * lfo;
            d.radL = (float) (d.radL * m);
            d.radR = (float) (d.radR * m);
        }
        power += d.ringEnv * sigma[i];
    }
    singing.store (nSing);
    gridPower.store (power);
}

//==============================================================================
static inline float cubicRead (const float* line, int len, double pos)
{
    /*  Catmull-Rom, the real one. The first draft's "optimised" form returned
        the MIDPOINT of the bracketing samples at frac -> 1 — a broken
        interpolator that read as 45 samples of mistuning because the loop
        then oscillated wherever its smeared phase happened to close. */
    while (pos < 0) pos += len;
    int i1 = (int) pos;
    const float f = (float) (pos - i1);
    i1 %= len;
    const int i0 = i1 - 1 < 0 ? len - 1 : i1 - 1;
    const int i2 = i1 + 1 >= len ? 0 : i1 + 1;
    const int i3 = i2 + 1 >= len ? 0 : i2 + 1;
    const float y0 = line[i0], y1 = line[i1], y2 = line[i2], y3 = line[i3];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 1.5f * (y1 - y2) + 0.5f * (y3 - y0);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

static inline float cubicSat (float x)
{
    //  x - (4/27)x^3, clamped at the extremum: s(1.5) = 1, s'(1.5) = 0.
    //  Only low-order harmonics per pass — that is the aliasing budget.
    if (x >  1.5f) return  1.0f;
    if (x < -1.5f) return -1.0f;
    return x - 0.148148148f * x * x * x;
}

/*  THE MEDIUM'S OWN NONLINEARITY, unity slope at zero so a quiet loop is
    untouched and only amplitude meets it: a saturating part (rational tanh,
    bounded, monotonic) and a FOLDING part (sin), whose spectrum turns over as
    the wave grows — metallic, vocal, alive with velocity. Normalised by the
    drive, so the drive sets WHERE the wave starts to lean, not how loud. */
static inline float tanhR (float x)
{
    if (x >  3.0f) return  1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/*  ...and its exact antiderivative, for first-order ADAA: the shaper is
    integrated between successive samples instead of sampled, which is what
    keeps a loop driven deep into it from aliasing (measured, a glass at
    55 Hz had energy -51 dB between its harmonics at 48 kHz and -69 at 96:
    folded-back, not real). d/dx [x^2/18 + (4/3) ln(27 + 9x^2)] is the
    rational tanh above; past +-3 it is |x| plus the value it reached. */
static inline double tanhRInt (double x)
{
    static const double F3 = 0.5 + (4.0 / 3.0) * std::log (108.0);
    const double a = std::abs (x);
    if (a > 3.0) return a - 3.0 + F3;
    return x * x / 18.0 + (4.0 / 3.0) * std::log (27.0 + 9.0 * x * x);
}

void Engine::process (float* L, float* R, int n)
{
    if (lines.empty()) prepare (sr, n);

    //  a newly built web is swapped in only here, at a block edge
    if (swapReady.exchange (false))
    {
        specIndex = wantSpec.load();
        applyGeometry (pending);
    }

    std::memset (L, 0, sizeof (float) * (size_t) n);
    std::memset (R, 0, sizeof (float) * (size_t) n);

    const float RDC = (float) dcR;
    const float radDcA = (float) (1.0 - std::exp (-2.0 * PI * 8.0 / sr));  // tap DC block
    const float ct = 0.055f * clampf (p.bleed, 0.0f, 1.0f);
    const float radSlew = (float) (1.0 - std::exp (-1.0 / (0.005 * sr)));
    const float envSlew = (float) (1.0 - std::exp (-1.0 / (0.030 * sr)));
    const float chaosSlew = (float) (1.0 - std::exp (-1.0 / (0.040 * sr)));
    const float THIRD = 0.3333333f, SQ3 = 0.5773503f;   // King's law: sqrt(1/3)

    const float widthMul = 0.25f + 1.05f * clampf (p.breadth, 0.0f, 1.0f);
    //  make-up gain. Tuned so the default LEVEL sits well below the ceiling —
    //  the ceiling is a safety net for chaotic peaks, not a place the tone
    //  should live (the demos slammed it at 1.55; 0.95 leaves real headroom).
    const float g = 2.4f * p.level * p.level * 0.42f;
    const float ceil = 0.30f + 0.62f * (1.0f - clampf (p.sat, 0.0f, 1.0f));

    for (int s = 0; s < n; ++s)
    {
        if (tickLeft <= 0)
        {
            controlTick (64);
            tickLeft = 64;
        }
        --tickLeft;

        float outL = 0.0f, outR = 0.0f;
        for (int i = 0; i < geo.nDuct; ++i)
        {
            Duct& d = duct[i];
            if (! d.active) continue;
            float* line = lines.data() + (size_t) i * (size_t) lineLen;

            //  what the loop carries, one period ago
            double pos = (double) wr - (double) d.dtSm;
            /*  SHOCK — Burgers steepening. Wave speed rises with amplitude, so
                the loud crest arrives EARLY and the trough late: the wave leans
                forward towards a sawtooth front, the blare of a real horn.
                The loop reads its own last sample to know how loud it is. */
            if (mShock > 0.0f)
                pos += (double) d.dtSm * (double) clampf (mShock * d.lastOut, -0.08f, 0.08f);
            float y = cubicRead (line, lineLen, pos);

            //  the loop: DC block, conduit-relative loss (< 1 — the heat below
            //  is what carries it over unity)
            float v = y - d.dcX1 + RDC * d.dcY1;
            d.dcX1 = y; d.dcY1 = v;
            d.lp += d.lpA * (v - d.lp);
            v = d.lp;
            /*  NONLINEAR LOSS — the limiting mechanism a real Rijke tube has
                and this loop lacked. With the loss near unity, a hard heat
                drive pushed the loop onto the saturator's rail and it LATCHED
                there — an odd King's law pushes in the direction of u, so
                once pinned it stays pinned, flipping only at a slow relaxation
                rate the tap's DC blocker removes. Measured: a heat-flooded
                conduit with envelope 0.67 radiated 0.0009. Acoustic losses
                grow with amplitude; give the loop that and it settles into a
                cycle instead of a latch. Exactly zero at zero amplitude, so a
                quiet loop is untouched. */
            const float rr = d.ringEnv;
            v *= d.g - 0.09f * mEnvLoss * rr * rr;
            if (closedNow[i]) v = -v;

            /*  THE THERMOACOUSTIC HEAT RELEASE — King's law, delayed by tau.
                A hot wire in oscillating flow releases heat as
                  q = sqrt(|1/3 + u(t-tau)|) - sqrt(1/3),
                and that heat, arriving delayed, pumps the acoustic field
                (Rayleigh's criterion). Near zero drive its slope is a delayed
                linear gain that, in phase, carries the loss over unity; as the
                cycle grows the sqrt saturates and the amplitude settles — a
                clean tone. Push the drive (TURBULENCE, heat, velocity) past
                that and the delayed nonlinearity destabilises the cycle:
                period-doubling drops a sub-octave in, then a torus, then
                broadband chaos. Same note, different SPECIES. */
            const float uTau = cubicRead (line, lineLen, (double) wr - (double) d.tauSm);
            /*  King's law made ODD. The literal form sqrt|1/3 + u| rectifies,
                and under a strong drive the loop fills with a DC PEDESTAL: the
                envelope reads high while the tap's DC blocker strips it all,
                and the pedestal pushes the sqrt into its flat region where the
                AC gain dies — measured, a heat-flooded conduit "sang" a 0.4
                offset and radiated nothing. Mirroring the law about zero keeps
                every property that matters (self-limiting amplitude, the
                delayed-feedback period-doubling and chaos) and cannot
                accumulate DC. */
            const float kingMag = std::sqrt (THIRD + std::abs (uTau)) - SQ3;
            const float king = uTau < 0.0f ? -kingMag : kingMag;
            /*  RAYLEIGH, STATED PROPERLY: only the FLUCTUATING heat release
                does work on the acoustic field — the mean heat release warms
                the gas and that is all. Injected raw, the in-phase King term
                amplifies DC exactly as well as the tone, and the loop's own
                blocker (0.25 Hz, for tuning) cannot stop it: read straight
                off the delay line, a flooded conduit sang for 250 ms, then
                LATCHED at +0.62 with no AC at all — a fixed point, not a
                note. So the heater has thermal inertia: its mean is tracked
                over ~26 ms and only the excess enters the loop. A DC state
                now loses its gain within that time and cannot establish. */
            d.kingMean += kingHpA * (king - d.kingMean);
            v += d.heatDrive * (king - d.kingMean);
            //  the safety ceiling (King's law is already sublinear, so this
            //  only catches the loudest chaotic transients)
            if (mDrive > 1.0e-4f)
            {
                /*  The PUMP's excess gain, and it is the whole of the timbre: a
                    self-oscillator's harmonics are set by how much excess the
                    nonlinearity must eat, not by how hard it is driven (more
                    drive alone only settled the cycle lower — measured, the
                    medium was met at 0.5-0.7 of its knee whatever the drive).
                    So the singing conduit's heat release pushes the cycle INTO
                    the medium by the material's measure. Gated on the heat
                    drive: an undriven conduit's loop gain stays below one, so
                    silence and the dying struck ring are untouched. */
                const float hd = clampf (d.heatDrive * 7.0f, 0.0f, 1.0f);
                /*  ...and the pump is a RESONANCE at this conduit's own pitch,
                    as a reed or a lip is. Three faults, one cure, all measured:
                    pumping everything gave a hot loop a stable state at the
                    rail (loop mean +-0.9, no AC: the DC latch again); pumping
                    everything but the slow mean led the fundamental and ran 15
                    cents sharp; and pumping every MODE alike let an open loop
                    leap to its second mode (h2 +20 dB over the fundamental — a
                    note an octave high). A bandpass centred on the pitch has no
                    gain at DC, exactly zero phase at its centre, and ~0.4 at the
                    octave, so the fundamental wins and the overtones are made
                    by its saturation, the way a blown pipe makes them. */
                const float bp = d.pB0 * (v - d.pX2) - d.pA1 * d.pY1 - d.pA2 * d.pY2;
                d.pX2 = d.pX1; d.pX1 = v; d.pY2 = d.pY1; d.pY1 = bp;
                /*  the servo's EAR: a narrow resonance (Q 4) on the ordered
                    pitch, so a strong 2nd or 3rd harmonic cannot add crossings
                    (the pump's own wide one let them — measured, the servo
                    heard octaves and drove the delay to its stop). Only a
                    period near the expected one is believed. */
                const float se = d.sB0 * (v - d.sX2) - d.sA1 * d.sY1 - d.sA2 * d.sY2;
                if (d.sY1 < 0.0f && se >= 0.0f)
                {
                    const double t = (double) clockSamples - 1.0 + (double) (-d.sY1) / (double) (se - d.sY1);
                    if (d.zcLast > 0.0)
                    {
                        const float per = (float) (t - d.zcLast), exp = (float) d.perExp;
                        if (per > 0.80f * exp && per < 1.25f * exp)
                            d.perSm = d.perSm <= 0.0f ? per : d.perSm + 0.15f * (per - d.perSm);
                    }
                    d.zcLast = t;
                }
                d.sX2 = d.sX1; d.sX1 = v; d.sY2 = d.sY1; d.sY1 = se;
                const float x = mDrive * (v + mBoost * hd * bp);
                float sh;
                {
                    const double dx = (double) x - d.adX;
                    const double Fx = tanhRInt ((double) x);
                    sh = std::abs (dx) > 1.0e-4 ? (float) ((Fx - d.adF) / dx)
                                                : tanhR (0.5f * (x + (float) d.adX));
                    d.adX = (double) x; d.adF = Fx;
                }
                //  the fold folds ONCE: its argument is compressed smoothly to
                //  +-2.2 rad, or a pumped wave wrapped round sin several times a
                //  cycle and spread energy far past Nyquist (measured, an
                //  inter-harmonic floor of -47 dB on a glass at 55 Hz)
                if (mFold > 0.0f) sh += mFold * (std::sin (2.2f * tanhR (x / 2.2f)) - sh);
                v = sh / mDrive;
            }
            v = cubicSat (v);

            /*  HOW FAR INTO CHAOS — the period-to-period difference. A clean
                limit cycle repeats (v == y a period ago); period-2 alternates;
                chaos never repeats. Normalised by amplitude, followed slowly,
                radiated to the panel so a turbulent conduit LOOKS turbulent. */
            {
                //  a CLOSED conduit inverts every pass, so its clean cycle
                //  repeats as -y: compared with +y it read as 100 % chaos
                //  forever (measured 0.98 on every quarter-wave conduit) and
                //  shimmered on the panel as if turbulent
                const float rep = std::abs (v - (closedNow[i] ? -y : y)) / (d.ringEnv + 0.02f);
                d.chaos += (std::min (1.0f, rep) - d.chaos) * chaosSlew;
            }

            //  injections: the valve pop, the stammer, a strike — and the
            //  neighbours' sound bleeding across the shared junction
            float inj = 0.0f, hammer = 0.0f;
            if (d.pulseLeft > 0)
            {
                const float puff = 0.5f * (1.0f - (float) std::cos (2.0 * PI * d.pulsePh));
                hammer = d.pulseAmp * puff;   //  one-sided: what the walls are struck with
                if (mDoublet > 0.0f)
                {
                    //  a sharp DOUBLET: a click with no net push (zero DC) —
                    //  the struck metal's onset rather than a valve's puff
                    const float dbl = 1.4f * (float) std::sin (2.0 * PI * d.pulsePh);
                    inj += d.pulseAmp * (puff + mDoublet * (dbl - puff));
                }
                else inj += d.pulseAmp * puff;
                d.pulsePh += d.pulseInc;
                --d.pulseLeft;
            }
            const float pulsePart = hammer;    //  the valve's own snap, one-sided, for the walls below
            if (ct > 0.0f)
            {
                float xn = 0.0f;
                const int nn = geo.nNbd[i];
                for (int k2 = 0; k2 < nn; ++k2) xn += lastOutArr[geo.nbd[i][k2]];
                inj += std::min (0.16f, ct * mSym) * xn / (float) std::max (1, nn);
            }
            v += inj;
            line[wr] = v;

            d.ringEnv += (std::abs (v) - d.ringEnv) * envSlew;
            d.lastOut = v;

            //  the RADIATED tap: first darkened by the timbre lowpass (off the
            //  loop, so tuning is untouched), then DC-blocked in its own right
            //  — the King's-law rectification leaves a per-cycle offset the
            //  loop's own blocker never sees, because we radiate before it
            //  recirculates.
            d.radLp += d.radA * (v - d.radLp);
            float vlp = d.radLp;
            //  the SKIN: an even term on the radiated tap (never in the loop,
            //  where an even term builds DC); the tap's DC block follows
            if (mSkin > 0.0f) vlp += 1.5f * mSkin * vlp * vlp;
            /*  STIFFNESS — the inharmonic modes on the radiated tap. Fed the
                loop's OUTPUT, they ring at their own bell/bar frequencies
                whenever that output carries broadband energy: a strike, the
                chuff, or a TURBULENT (chaotic) tone. So inharmonicity is
                loudest exactly where a struck or driven metal object rings —
                and a clean sustained tone, which is nearly a sine, stays
                nearly harmonic, as a clean tube should. */
            if (d.mB[0] > 1e-6f)
            {
                for (int mmi = 0; mmi < 2; ++mmi)
                {
                    const float my = d.mB[mmi] * v + d.mA1[mmi] * d.mY1[mmi] - d.mA2[mmi] * d.mY2[mmi];
                    d.mY2[mmi] = d.mY1[mmi]; d.mY1[mmi] = my;
                    if (chOn)
                    {
                        /*  THE VALVE STRIKES THE WALL. Fed only the loop, the
                            modes heard the transient diluted by the tone around
                            it, and a bell's onset came out duller than before
                            (measured x0.57 on specimen 0); the old engine's
                            endless stammer clicks had been standing in for the
                            ring. A metal-rich material takes the valve's own
                            snap straight into its walls. */
                        const float mIn = v + mStrike * d.ringStrike * pulsePart;
                        const float ny = d.nB0[mmi] * (mIn - d.nX2[mmi]) + d.nA1[mmi] * d.nY1[mmi] - d.nA2[mmi] * d.nY2[mmi];
                        d.nX2[mmi] = d.nX1[mmi]; d.nX1[mmi] = mIn;
                        d.nY2[mmi] = d.nY1[mmi]; d.nY1[mmi] = ny;
                        const float chf = clampf (p.character, 0.0f, 1.0f);
                        vlp += my + chf * (d.nG[mmi] * ny - my);
                    }
                    else vlp += my;
                }
            }
            float vr = vlp - d.radDcX + (1.0f - radDcA) * d.radDcY;
            d.radDcX = vlp; d.radDcY = vr;

            //  radiation, slewed so the section can turn without a click
            radCurL[i] += (d.radL - radCurL[i]) * radSlew;
            radCurR[i] += (d.radR - radCurR[i]) * radSlew;
            outL += vr * radCurL[i];
            outR += vr * radCurR[i];
        }
        for (int i = 0; i < geo.nDuct; ++i) lastOutArr[i] = duct[i].lastOut;

        if (++wr >= lineLen) wr = 0;
        ++clockSamples;

        //  breadth, then the guards: DC, subsonic, ceiling, bound
        /*  THE VESSEL — the two formants the whole web shares, in parallel
            with the dry radiation (a peak where the gain is positive, a dip
            where it is negative), then the material's level trim. */
        if (chOn)
        {
            float* io[2] = { &outL, &outR };
            for (int chn = 0; chn < 2; ++chn)
            {
                const float x = *io[chn];
                float add = 0.0f;
                for (int k = 0; k < 2; ++k)
                {
                    const float y = bodyB0[k] * x + bodyZ1[chn][k];
                    bodyZ1[chn][k] = -bodyA1[k] * y + bodyZ2[chn][k];
                    bodyZ2[chn][k] = -bodyB0[k] * x - bodyA2[k] * y;
                    add += bodyG[k] * y;
                }
                *io[chn] = (x + add) * mTrim;
            }
        }
        float mid = (outL + outR) * 0.5f;
        float sid = (outL - outR) * 0.5f * widthMul;
        float xl = (mid + sid) * g;
        float xr = (mid - sid) * g;

        for (int chn = 0; chn < 2; ++chn)
        {
            float& x = chn == 0 ? xl : xr;
            float vdc = x - outDcX[chn] + 0.9997f * outDcY[chn];
            outDcX[chn] = x; outDcY[chn] = vdc;
            const float yhp = (float) (hpB0 * vdc + hpZ1[chn]);
            hpZ1[chn] = (float) (hpB1 * vdc - hpA1 * yhp + hpZ2[chn]);
            hpZ2[chn] = (float) (hpB2 * vdc - hpA2 * yhp);
            x = yhp;

            //  the soft ceiling, then a DC blocker, THEN the hard bound. The
            //  King's-law heat release RECTIFIES (its sqrt is even), so the
            //  loop wave is asymmetric, and an ODD saturator on an asymmetric
            //  wave has a non-zero mean — the ceiling puts DC back that the
            //  subsonic guard removed. The block must sit AFTER the ceiling;
            //  the hard bound must sit LAST, or the blocker's transient
            //  overshoot escapes the ceiling (measured a random machine at 1.09).
            const float t = x / ceil;
            x = ceil * (t / (1.0f + std::abs (t) * 0.55f));
            float vfin = x - outDcX2[chn] + 0.9995f * outDcY2[chn];
            outDcX2[chn] = x; outDcY2[chn] = vfin;
            x = vfin;
            const float a = std::abs (x);
            if (a > 0.85f)
            {
                const float o = 0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f);
                x = x < 0.0f ? -o : o;
            }
            //  a SECOND 16 Hz high-pass, last of all: chaos wanders into the
            //  sub-bass and the odd ceiling re-injects it, and neither the
            //  first guard nor a DC blocker (which only kills 0 Hz) catches
            //  4-16 Hz. A bass instrument must still not rumble.
            {
                const float yhp2 = (float) (hpB0 * x + hp2Z1[chn]);
                hp2Z1[chn] = (float) (hpB1 * x - hpA1 * yhp2 + hp2Z2[chn]);
                hp2Z2[chn] = (float) (hpB2 * x - hpA2 * yhp2);
                //  a high-pass differentiates fast edges and can overshoot the
                //  bound it followed, so a final hard clip catches those rare
                //  transient peaks — nothing is allowed to leave above full scale
                x = yhp2 > 1.0f ? 1.0f : (yhp2 < -1.0f ? -1.0f : yhp2);
            }
        }
        L[s] = xl; R[s] = xr;
    }

    float blockPeak = 0.0f;
    for (int i = 0; i < n; ++i) blockPeak = std::max (blockPeak, std::abs (L[i]));
    const float heldLvl = outLevel.load() * (float) std::pow (0.86, (double) n / 256.0);
    outLevel.store (std::max (blockPeak, heldLvl));
}

//==============================================================================
void Engine::visualState (Web& out)
{
    out.nNode = geo.nNode; out.nDuct = geo.nDuct;
    out.specimen = specIndex; out.geomStamp = geomStamp;
    for (int i = 0; i < geo.nNode; ++i)
        for (int k = 0; k < 4; ++k) out.node[i][k] = geo.node[i][k];
    const double tAmb = ambientKelvin (p.ambient);
    for (int i = 0; i < geo.nDuct; ++i)
    {
        out.ductA[i] = (uint8_t) geo.ductA[i];
        out.ductB[i] = (uint8_t) geo.ductB[i];
        out.closedEnd[i] = closedNow[i];
        out.fPassive[i] = geo.fPassive[i];
        const double lnr = std::log (std::max (1e-9, T[i] / tAmb));
        out.heat[i] = (uint8_t) clampd (128.0 + lnr * 96.0, 0.0, 255.0);
        out.amp[i]  = (uint8_t) clampd (std::sqrt ((double) duct[i].ringEnv) * 340.0, 0.0, 255.0);
        out.sig[i]  = (uint8_t) clampd ((double) sigma[i] * 255.0, 0.0, 255.0);
        //  the regime and the timbre, so the panel can draw what it sounds like
        out.chaos[i]  = (uint8_t) clampd ((double) duct[i].chaos * 255.0, 0.0, 255.0);
        out.bright[i] = (uint8_t) clampd (((double) duct[i].bright - 0.30) / 2.40 * 255.0, 0.0, 255.0);
        uint8_t cflag = 0;
        for (const auto& c : cmds) if (c.duct == i && c.env > 1e-3) { cflag = 1; break; }
        out.cmd[i] = cflag;
    }
    out.rotA = (float) rotA; out.rotB = (float) rotB;
    out.sectionH = (float) (((double) p.section * 2.0 - 1.0) * 0.85);
    out.veilW = (float) (0.10 + 0.55 * (double) p.veil);
    out.nPkt = 0;
    const double tSec = (double) clockSamples / sr;
    for (const auto& k : pkts)
    {
        if (! k.live || out.nPkt >= MAXPKT) continue;
        const double e = (tSec - k.t0) / k.perEdge;
        const int at = (int) e;
        if (at < 0 || at >= k.len) continue;
        out.pktDuct[out.nPkt] = (uint8_t) k.path[at];
        out.pktT[out.nPkt] = (uint8_t) clampd ((e - at) * 255.0, 0.0, 255.0);
        ++out.nPkt;
    }
}

} // namespace ab104
