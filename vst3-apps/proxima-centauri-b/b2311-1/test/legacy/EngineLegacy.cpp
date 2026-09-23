#include "EngineLegacy.h"
#include <algorithm>
#include <cstring>

namespace ab1legacy
{

//==============================================================================
#define P(x) [] (Params& q) -> float& { return q.x; }

static const PSpec SPECS[] =
{
    { "level",    "LEVEL",        "how loud the object is allowed to be",                 0.62f,  KP_VOL,   0, 0, P(level) },

    { "ratelo",   "SLOWEST",      "the slowest count anywhere in the body",               0.06f,  KP_PCT,   0, 0, P(rateLo) },
    { "ratehi",   "FASTEST",      "the fastest count anywhere in the body",               0.34f,  KP_PCT,   0, 0, P(rateHi) },
    { "couple",   "CONDUCTION",   "how hard one count shoves the counts beside it",       0.80f,  KP_PCT,   0, 0, P(couple) },
    { "dead",     "DEAD TIME",    "how long a unit ignores its neighbours after firing",  0.55f,  KP_PCT,   0, 0, P(dead) },
    { "leak",     "CONCAVITY",    "how much a shove matters more near the threshold",     0.30f,  KP_PCT,   0, 0, P(leak) },

    { "grip",     "GRIP",         "how hard the imposed pulse shoves the body",           0.20f,  KP_PCT,   0, 0, P(grip) },
    { "reach",    "REACH",        "how much of the body the imposed pulse touches",       0.30f,  KP_PCT,   0, 0, P(reach) },
    { "division", "DIVISION",     "how the imposed pulse divides the host's beat",        5.0f,   KP_LIST,  0, 6, P(division) },
    { "freehz",   "FREE RATE",    "the pulse it keeps when no transport is running",      0.35f,  KP_PCT,   0, 0, P(freeHz) },

    { "projx",    "SECTION X",    "how far the visible width reaches into the sound",     0.75f,  KP_PCT,   0, 0, P(projx) },
    { "projy",    "SECTION Y",    "how far the visible height reaches into the sound",    0.25f,  KP_PCT,   0, 0, P(projy) },
    { "projz",    "DEPTH Z",      "how far the first unseen axis reaches into the sound", 0.30f,  KP_PCT,   0, 0, P(projz) },
    { "projw",    "DEPTH W",      "how far the second unseen axis reaches",               0.55f,  KP_PCT,   0, 0, P(projw) },
    { "spanx",    "EXTENSION",    "how much of a step a cascade is spread across",        0.55f,  KP_PCT,   0, 0, P(spanx) },
    { "tilt",     "WEIGHTING",    "which depth is loudest",                               0.50f,  KP_PCT,   0, 0, P(tilt) },

    { "temp",     "TEMPERATURE",  "cold it does not count; warm it counts faster",        0.298755f, KP_KELVIN, 77, 800, P(temp) },
    { "shape",    "AFTERSOUND",   "what a single count leaves behind it",                 0.35f,  KP_PCT,   0, 0, P(shape) },
    { "damp",     "ABSORPTION",   "how quickly the body swallows what it has made",       0.40f,  KP_PCT,   0, 0, P(damp) },
    { "space",    "ENCLOSURE",    "the room the crate makes around it",                   0.30f,  KP_PCT,   0, 0, P(space) },
    { "sat",      "CEILING",      "the level it will not exceed",                         0.35f,  KP_PCT,   0, 0, P(sat) },
};
#undef P

static const int NSPEC = (int)(sizeof SPECS / sizeof SPECS[0]);

int          numParams()            { return NSPEC; }
const PSpec& paramSpec (int i)      { return SPECS[i < 0 ? 0 : (i >= NSPEC ? NSPEC-1 : i)]; }
float        paramMax (const PSpec& s)
{
    if (s.kind == KP_LIST || s.kind == KP_INT) return s.hi;
    return 1.0f;
}

static const char* DIVISION_NAMES[7] = { "4 beats", "2 beats", "1 beat", "1/2", "1/3", "1/4", "1/8" };
static const double DIVISION_BEATS[7] = { 4.0, 2.0, 1.0, 0.5, 1.0/3.0, 0.25, 0.125 };

const char* const* listNames (const char* id, int& n)
{
    if (std::strcmp (id, "division") == 0) { n = 7; return DIVISION_NAMES; }
    n = 0; return nullptr;
}

//==============================================================================
static inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline double xmapd (double v, double lo, double hi)
{ return lo * std::pow (hi / lo, clampf ((float) v, 0.0f, 1.0f)); }

//==============================================================================
/*  THE CONCAVE RISE, which is the whole reason this mechanism entrains.

    A unit's phase runs 0..1 at a steady rate, but what a shove adds to is not
    the phase — it is the STATE, x = f(phase), with f concave. Mirollo and
    Strogatz proved that pulse-coupled units with a concave rise synchronise;
    with a straight rise they do not, and the object would ignore the host
    entirely. CONCAVITY is therefore not a colour control, it is the dial
    between an object that can be led and one that cannot.

        f(u)     = (1 - e^(-b u)) / (1 - e^(-b))
        f^-1(x)  = -ln(1 - x (1 - e^(-b))) / b
*/
struct Rise
{
    double b = 1.0, denom = 1.0;
    void set (double concavity)
    {
        b = std::max (0.02, concavity * 5.0);
        denom = 1.0 - std::exp (-b);
    }
    inline double f (double u)  const { return (1.0 - std::exp (-b * u)) / denom; }
    inline double inv (double x) const
    {
        const double a = 1.0 - x * denom;
        return a <= 1.0e-9 ? 1.0 : -std::log (a) / b;
    }
    /*  Shove the phase by adding `eps` to the state. Returns the new phase,
        which may be at or past 1 — the caller decides what firing means. */
    inline double shove (double u, double eps) const
    {
        const double x = f (u) + eps;
        return x >= 1.0 ? 1.0 : inv (x);
    }
};

static Rise gRise;

//==============================================================================
Engine::Engine()
{
    ph.assign (NUNIT, 0.0f);
    rate.assign (NUNIT, 1.0f);
    heat.assign (NUNIT, 0.0f);
    inQ.assign (NUNIT, 0);
    ux.resize (NUNIT); uy.resize (NUNIT); uz.resize (NUNIT); uw.resize (NUNIT);
    nb.assign ((size_t) NUNIT * 8, -1);
    fireQ.reserve (NUNIT);
    firedScratch.reserve (NUNIT);

    auto idx = [] (int x, int y, int z, int w) -> int
    {
        if (x < 0 || x >= NX || y < 0 || y >= NY || z < 0 || z >= NZ || w < 0 || w >= NW) return -1;
        return ((w * NZ + z) * NY + y) * NX + x;
    };
    const int d[8][4] = {{1,0,0,0},{-1,0,0,0},{0,1,0,0},{0,-1,0,0},
                         {0,0,1,0},{0,0,-1,0},{0,0,0,1},{0,0,0,-1}};
    for (int w = 0; w < NW; ++w) for (int z = 0; z < NZ; ++z)
    for (int y = 0; y < NY; ++y) for (int x = 0; x < NX; ++x)
    {
        const int i = idx (x, y, z, w);
        ux[(size_t)i] = (uint8_t) x; uy[(size_t)i] = (uint8_t) y;
        uz[(size_t)i] = (uint8_t) z; uw[(size_t)i] = (uint8_t) w;
        for (int k = 0; k < 8; ++k)
            nb[(size_t) i * 8 + k] = idx (x + d[k][0], y + d[k][1], z + d[k][2], w + d[k][3]);
    }
    reset();
    rebuildRates();
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    reset();
}

void Engine::reset()
{
    uint64_t s = 0x9E3779B97F4A7C15ull;
    auto rnd = [&s] { s = s * 6364136223846793005ull + 1442695040888963407ull;
                      return (float) (((s >> 11) & ((1ull << 53) - 1)) / (double)(1ull << 53)); };
    for (int i = 0; i < NUNIT; ++i) { ph[(size_t)i] = rnd(); heat[(size_t)i] = 0.0f; inQ[(size_t)i] = 0; }
    tail = Tail();
    stepAcc = 0; beatPhase = 0; freeRunPhase = 0;
    concX = concY = concN = 0;
    held = 0; heldNotes = 0; noteRate = 1.0f;
    outLevel.store (0.0f); lastCascade.store (0); concentration.store (0.0f);
    totalFired.store (0); biggestCascade.store (0);
    stepsRun.store (0); silentSteps.store (0);
}

/*  The spread of natural rates is the thing that keeps this from being one
    clock. Units near each other in space are near each other in rate, so the
    lattice falls into REGIONS that run together — which is what "several layers
    following different timings" means when you have to build it. */
void Engine::rebuildRates()
{
    const double lo = xmapd (p.rateLo, 0.25, 12.0);
    const double hi = xmapd (std::max (p.rateHi, p.rateLo), 0.25, 40.0);
    uint64_t s = 0x2545F4914F6CDD1Dull;
    auto rnd = [&s] { s = s * 6364136223846793005ull + 1442695040888963407ull;
                      return ((s >> 11) & ((1ull << 53) - 1)) / (double)(1ull << 53); };
    for (int i = 0; i < NUNIT; ++i)
    {
        const double bx = (double) ux[(size_t)i] / (NX - 1);
        const double by = (double) uy[(size_t)i] / (NY - 1);
        const double bw = (double) uw[(size_t)i] / std::max (1, NW - 1);
        //  a smooth field across the lattice, plus a little grit so no two
        //  neighbours are exactly equal (exactly equal units never separate)
        const double band = 0.45 * bx + 0.35 * by + 0.20 * bw;
        rate[(size_t)i] = (float) (lo + (hi - lo) * (0.88 * band + 0.12 * rnd()));
    }
}

//==============================================================================
void Engine::setTransport (double bpm, double ppq, bool playing)
{
    hostBpm = bpm > 1.0 ? bpm : 120.0;
    hostPpq = ppq;
    hostPlaying = playing;
}

void Engine::noteOn (int note, float vel)
{
    ++heldNotes;
    held = clampf (0.35f + 0.65f * vel, 0.0f, 1.0f);
    //  a played note transposes the COUNTING. Higher note, faster count, and
    //  since pitch here is nothing but event rate, that is the whole of tuning.
    noteRate = (float) std::pow (2.0, (note - 48) / 12.0);
}

void Engine::noteOff (int)
{
    if (--heldNotes <= 0) { heldNotes = 0; held = 0.0f; }
}

void Engine::allNotesOff() { heldNotes = 0; held = 0.0f; }

void Engine::poke (int x, int y, float amount, int radius)
{
    for (int w = 0; w < NW; ++w) for (int z = 0; z < NZ; ++z)
    for (int dy = -radius; dy <= radius; ++dy) for (int dx = -radius; dx <= radius; ++dx)
    {
        const int xx = x + dx, yy = y + dy;
        if (xx < 0 || xx >= NX || yy < 0 || yy >= NY) continue;
        const int i = ((w * NZ + z) * NY + yy) * NX + xx;
        if (ph[(size_t)i] < 0.0f) continue;
        ph[(size_t)i] = (float) std::min (1.0, (double) ph[(size_t)i] + amount);
    }
}

//==============================================================================
/*  One step of the lattice. Everything that fires — whether because its own
    count came round or because a neighbour shoved it over — lands in `fired`,
    in the order it happened. */
void Engine::advanceStep (double dt, std::vector<int>& fired)
{
    fired.clear();
    fireQ.clear();

    const float deadFor = -clampf (p.dead, 0.0f, 1.0f) * 0.9f;
    const double eps = xmapd (p.couple, 0.02, 0.60);

    for (int i = 0; i < NUNIT; ++i)
    {
        float v = ph[(size_t)i];
        if (v < 0.0f)                        // dead: count back up to zero
        {
            v += (float) (rate[(size_t)i] * noteRate * dt);
            ph[(size_t)i] = v > 0.0f ? 0.0f : v;
            continue;
        }
        v += (float) (rate[(size_t)i] * noteRate * dt);
        ph[(size_t)i] = v;
        if (v >= 1.0f && ! inQ[(size_t)i]) { inQ[(size_t)i] = 1; fireQ.push_back (i); }
    }

    for (size_t q = 0; q < fireQ.size(); ++q)
    {
        const int i = fireQ[q];
        ph[(size_t)i] = deadFor;
        heat[(size_t)i] = 1.0f;
        fired.push_back (i);
        for (int k = 0; k < 8; ++k)
        {
            const int j = nb[(size_t) i * 8 + k];
            if (j < 0) continue;
            float& pj = ph[(size_t)j];
            if (pj < 0.0f) continue;                     // still dead: ignores the shove
            pj = (float) gRise.shove (pj, eps);
            if (pj >= 1.0f && ! inQ[(size_t)j]) { inQ[(size_t)j] = 1; fireQ.push_back (j); }
        }
    }
    for (int i : fireQ) inQ[(size_t)i] = 0;
}

//==============================================================================
void Engine::process (float* L, float* R, int n)
{
    std::memset (L, 0, sizeof (float) * (size_t) n);
    std::memset (R, 0, sizeof (float) * (size_t) n);

    gRise.set (clampf (p.leak, 0.0f, 1.0f));

    /*  TEMPERATURE drives the counting itself. At 77 K the object does not
        count and there is nothing to hear — not "almost nothing": the loop is
        skipped. Warmed, it counts faster, which is what the brief means by
        "upon heating, emits a rhythmic pulse". */
    const double kelvin = TEMP_MIN + (TEMP_MAX - TEMP_MIN) * clampf (p.temp, 0.0f, 1.0f);
    const double warmth = (kelvin - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
    if (warmth <= 0.0008) { outLevel.store (0.0f); return; }
    const double heatRate = 0.25 + 3.2 * warmth * warmth;

    //  the imposed pulse
    const int dIdx = (int) clampf (std::round (p.division), 0.0f, 6.0f);
    const double beatsPer = DIVISION_BEATS[dIdx];
    double pulseHz;
    if (hostPlaying) pulseHz = (hostBpm / 60.0) / beatsPer;
    else             pulseHz = xmapd (p.freeHz, 0.15, 12.0);

    const double grip  = xmapd (p.grip, 0.05, 1.50);
    const int    reach = (int) std::lround (1.0 + clampf (p.reach, 0.0f, 1.0f) * (NX * 0.42));

    //  how a firing's position becomes its moment
    const double pj[4] = { (double) p.projx * 2.0 - 1.0, (double) p.projy * 2.0 - 1.0,
                           (double) p.projz * 2.0 - 1.0, (double) p.projw * 2.0 - 1.0 };
    double pmin = 0, pmax = 0;
    const double ext[4] = { NX - 1.0, NY - 1.0, NZ - 1.0, NW - 1.0 };
    for (int k = 0; k < 4; ++k) { const double e = pj[k] * ext[k]; if (e > 0) pmax += e; else pmin += e; }
    const double pspan = std::max (1e-9, pmax - pmin);
    const double spread = clampf (p.spanx, 0.0f, 1.0f);

    const double stepHz = 900.0;                 // the lattice's own clock
    const double dt = 1.0 / stepHz;
    const double stepsPerSample = stepHz / sr;

    const float tilt = clampf (p.tilt, 0.0f, 1.0f);
    const float g = (float) (2.4 * (double) p.level * (double) p.level);

    //  the kernel a firing leaves behind: two one-poles, so an event has a
    //  body. A filter, not an oscillator — the medium cannot respond instantly.
    const float a1 = (float) xmapd (p.shape, 0.60, 0.03);
    const float a2 = (float) xmapd (p.damp,  0.45, 0.015);

    int sample = 0;
    while (sample < n)
    {
        stepAcc += stepsPerSample;
        if (stepAcc >= 1.0)
        {
            stepAcc -= 1.0;

            //  the imposed pulse, and where we are between them
            const double advance = dt * pulseHz;
            beatPhase += advance;
            if (beatPhase >= 1.0)
            {
                beatPhase -= std::floor (beatPhase);
                const int cx = NX / 2, cy = NY / 2;
                for (int w = 0; w < NW; ++w) for (int z = 0; z < NZ; ++z)
                for (int dy = -reach; dy <= reach; ++dy) for (int dx = -reach; dx <= reach; ++dx)
                {
                    const int xx = cx + dx, yy = cy + dy;
                    if (xx < 0 || xx >= NX || yy < 0 || yy >= NY) continue;
                    if (dx*dx + dy*dy > reach*reach) continue;
                    const int i = ((w * NZ + z) * NY + yy) * NX + xx;
                    if (ph[(size_t)i] < 0.0f) continue;
                    ph[(size_t)i] = (float) gRise.shove (ph[(size_t)i], grip);
                }
            }

            advanceStep (dt * heatRate, firedScratch);
            stepsRun.fetch_add (1);
            if (firedScratch.empty()) silentSteps.fetch_add (1);

            if (! firedScratch.empty())
            {
                lastCascade.store ((int) firedScratch.size());
                totalFired.fetch_add ((long long) firedScratch.size());
                { int b = biggestCascade.load();
                  if ((int) firedScratch.size() > b) biggestCascade.store ((int) firedScratch.size()); }
                //  how far it has leaned: the firings' phase against the beat
                const double a = 2.0 * PI * beatPhase;
                concX = concX * 0.999 + std::cos (a) * firedScratch.size();
                concY = concY * 0.999 + std::sin (a) * firedScratch.size();
                concN = concN * 0.999 + firedScratch.size();
                if (concN > 1.0)
                    concentration.store ((float) (std::hypot (concX, concY) / concN));
            }

            /*  Each firing is a moment. Where the moment falls inside the step
                is where the unit stands along the projection — this is the only
                route from four dimensions to the ear, and the whole timbre. */
            const int stepSamples = std::max (1, (int) (sr / stepHz));
            for (int i : firedScratch)
            {
                const double proj = (pj[0] * ux[(size_t)i] + pj[1] * uy[(size_t)i]
                                   + pj[2] * uz[(size_t)i] + pj[3] * uw[(size_t)i] - pmin) / pspan;
                const int off = (int) (proj * spread * stepSamples);
                const int at = sample + off;
                if (at < 0 || at >= n) continue;
                const float sgn = ((ux[(size_t)i] + uy[(size_t)i] + uz[(size_t)i] + uw[(size_t)i]) & 1) ? -1.0f : 1.0f;
                const float dep = (float) uw[(size_t)i] / std::max (1, NW - 1);
                const float amp = sgn * (0.35f + 0.65f * (tilt * dep + (1.0f - tilt) * (1.0f - dep)));
                const float pan = (float) ux[(size_t)i] / (NX - 1);
                L[at] += amp * (1.0f - pan);
                R[at] += amp * pan;
            }

            /*  The panel samples this at thirty a second. Decaying a flare in
                thirty milliseconds means almost every firing happens between two
                frames and is never seen: the object was sounding and the picture
                was still. Two hundred milliseconds is long enough to be caught. */
            for (int i = 0; i < NUNIT; ++i) heat[(size_t)i] *= 0.9938f;
        }
        ++sample;
    }

    //  give the events a body, and keep the object inside its ceiling
    const float ceil = 0.30f + 0.62f * (1.0f - clampf (p.sat, 0.0f, 1.0f));
    float meter = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        tail.z1 += a1 * (L[i] - tail.z1);
        tail.z2 += a2 * (tail.z1 - tail.z2);
        float x = (tail.z1 - tail.z2) * g * 0.5f;
        tail.aL += a1 * (R[i] - tail.aL);
        tail.aR += a2 * (tail.aL - tail.aR);
        float y = (tail.aL - tail.aR) * g * 0.5f;
        //  a soft ceiling, applied always: transparent below it, asymptotic above
        auto soft = [ceil] (float v) {
            const float t = v / ceil;
            return ceil * (t / (1.0f + std::abs (t) * 0.55f));
        };
        x = soft (x); y = soft (y);
        L[i] = x; R[i] = y;
        meter += std::abs (x) + std::abs (y);
    }
    /*  PEAK, HELD. This object is silent for ninety-nine per cent of its
        steps, so a per-block mean reads as nothing at all and the meter never
        moves while the thing is plainly audible. What is wanted is the loudest
        moment, released slowly. */
    float blockPeak = 0.0f;
    for (int i = 0; i < n; ++i) blockPeak = std::max (blockPeak, std::abs (L[i]));
    const float heldLvl = outLevel.load() * (float) std::pow (0.86, (double) n / 256.0);
    outLevel.store (std::max (blockPeak, heldLvl));
    (void) meter;
    pulsePhase.store ((float) beatPhase);
}

//==============================================================================
void Engine::service()
{
    rebuildRates();
}

void Engine::visualState (Slice& out) const
{
    /*  The panel is shown the cross-section: one slice through the two unseen
        axes, phase as brightness and recent firing as heat. */
    const int z = NZ / 2, w = NW / 2;
    float rlo = 1e9f, rhi = -1e9f;
    for (int i = 0; i < NUNIT; ++i) { rlo = std::min (rlo, rate[(size_t)i]); rhi = std::max (rhi, rate[(size_t)i]); }
    const float rspan = std::max (1e-6f, rhi - rlo);
    for (int y = 0; y < NY; ++y)
        for (int x = 0; x < NX; ++x)
        {
            const int i = ((w * NZ + z) * NY + y) * NX + x;
            const float v = ph[(size_t)i];
            const float u = v < 0.0f ? 0.0f : v;
            out.phase[y * NX + x] = (uint8_t) std::min (255, (int) (u * 255.0f));
            out.heat [y * NX + x] = (uint8_t) std::min (255, (int) (heat[(size_t)i] * 255.0f));
            out.rate [y * NX + x] = (uint8_t) std::min (255, (int) ((rate[(size_t)i] - rlo) / rspan * 255.0f));
        }
}

//==============================================================================
/*  THE CATALOGUE. Two hundred and fifty six specimens, each a different set of
    counts. The generator is deterministic, so a number is a specimen for good. */
int specimenCount() { return 256; }

void applySpecimen (int index, Params& p)
{
    index = ((index % 256) + 256) % 256;
    uint64_t s = (uint64_t) index * 0x9E3779B97F4A7C15ull + 0xB2311001ull;
    auto rnd = [&s] { s = s * 6364136223846793005ull + 1442695040888963407ull;
                      return (float) (((s >> 11) & ((1ull << 53) - 1)) / (double)(1ull << 53)); };
    auto rng = [&] (float a, float b) { return a + (b - a) * rnd(); };

    p.rateLo = rng (0.05f, 0.45f);
    p.rateHi = std::min (1.0f, p.rateLo + rng (0.10f, 0.55f));
    p.couple = rng (0.10f, 0.72f);
    p.dead   = rng (0.04f, 0.42f);
    p.leak   = rng (0.10f, 0.85f);
    p.grip   = rng (0.15f, 0.75f);
    p.reach  = rng (0.08f, 0.60f);
    p.projx  = rng (0.0f, 1.0f);
    p.projy  = rng (0.0f, 1.0f);
    p.projz  = rng (0.0f, 1.0f);
    p.projw  = rng (0.0f, 1.0f);
    p.spanx  = rng (0.15f, 1.0f);
    p.tilt   = rng (0.0f, 1.0f);
    p.shape  = rng (0.10f, 0.85f);
    p.damp   = rng (0.15f, 0.80f);
    p.space  = rng (0.05f, 0.65f);
}

} // namespace ab1legacy
