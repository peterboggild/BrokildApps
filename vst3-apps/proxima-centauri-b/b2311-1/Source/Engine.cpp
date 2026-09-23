#include "Engine.h"
#include <algorithm>
#include <cstring>

namespace ab1
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

    { "strat",    "STRATIFICATION","how differently each layer of the body answers",      0.55f,  KP_PCT,   0, 0, P(strat) },
    { "persist",  "PERSISTENCE",  "how much longer a large event rings than a small one", 0.45f,  KP_PCT,   0, 0, P(persist) },
    { "traverse", "TRAVERSE",     "how much a cascade's own size decides its spread",     0.60f,  KP_PCT,   0, 0, P(traverse) },
    { "weight",   "WEIGHT",       "how far a heavy blow reaches into the low body",       0.45f,  KP_PCT,   0, 0, P(weight) },
    { "memory",   "MEMORY",       "how long the body keeps the mark of being touched",    0.45f,  KP_PCT,   0, 0, P(memory) },
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
    bandOf.assign (NUNIT, 0);
    mark.assign (NUNIT, 0.0f);
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

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    /*  A firing is written at an OFFSET into the block, and one that lands past
        the end is dropped — that has always been true at block boundaries. So
        the chunk the bank works in is never smaller than the host's block, or
        chunking would start dropping firings the old code kept. */
    chunkCap = std::max (1024, maxBlock > 0 ? maxBlock : 1024);
    bandBuf.assign ((size_t) NBAND * 2 * (size_t) chunkCap, 0.0f);
    //  the crate's lines, sized for the largest box at this rate
    {
        const double scale = sr / 48000.0;
        const int base[4] = { 331, 461, 631, 809 };
        for (int k = 0; k < 4; ++k)
            crate.line[k].assign ((size_t) std::lround (base[k] * 2.25 * scale) + 8, 0.0f);
    }
    reset();
}

void Engine::reset()
{
    uint64_t s = 0x9E3779B97F4A7C15ull;
    auto rnd = [&s] { s = s * 6364136223846793005ull + 1442695040888963407ull;
                      return (float) (((s >> 11) & ((1ull << 53) - 1)) / (double)(1ull << 53)); };
    for (int i = 0; i < NUNIT; ++i) { ph[(size_t)i] = rnd(); heat[(size_t)i] = 0.0f; inQ[(size_t)i] = 0; mark[(size_t)i] = 0.0f; }
    for (Band& b : bands) { b.z1 = b.z2 = b.w1 = b.w2 = 0.0f; b.env = b.slow = 0.0f;
                            b.r1 = b.r2 = b.s1 = b.s2 = 0.0f; }
    maxRingSend.store (0.0f);
    for (int m = 0; m < 3; ++m) { low.y1[m] = low.y2[m] = 0.0f; }
    gEnv = gSlow = 0.0f;
    for (int k = 0; k < 4; ++k)
    {
        std::fill (crate.line[k].begin(), crate.line[k].end(), 0.0f);
        crate.pos[k] = 0; crate.lp[k] = 0.0f;
    }
    lastShape = lastDamp = lastStrat = -1.0f;
    lastWeight = lastSpace = lastBodyDamp = -1.0f;
    lastExtent = 0.0f;
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
        const double u = 0.88 * band + 0.12 * rnd();       // 0..1 within the spread
        rate[(size_t)i] = (float) (lo + (hi - lo) * u);
        /*  and this is the line that ends the one-body instrument: a unit's
            place in the rate field is the body it speaks through. The panel
            already draws that field as hue, so the bands are the regions you
            can see, and a sound tells you which layer made it. */
        const int b = (int) (u * NBAND);
        bandOf[(size_t)i] = (uint8_t) (b < 0 ? 0 : (b >= NBAND ? NBAND - 1 : b));
    }
}

/*  The bank's coefficients. AFTERSOUND and ABSORPTION keep their meaning and
    become the CENTRE of the spread; STRATIFICATION says how far apart the
    bands sit, in octaves, and at 0 every band is the centre — one body again,
    exactly as before.

    Both poles of a band move together, which time-scales its kernel rather
    than reshaping it, so the bands are the same body at different sizes and
    not eight different filters. That also makes the high bands louder, so the
    compensation is MEASURED here — run the impulse through and read the peak —
    rather than modelled. */
void Engine::rebuildBands()
{
    const float shape = clampf (p.shape, 0.0f, 1.0f);
    const float damp  = clampf (p.damp,  0.0f, 1.0f);
    const float strat = clampf (p.strat, 0.0f, 1.0f);
    if (shape == lastShape && damp == lastDamp && strat == lastStrat) return;
    lastShape = shape; lastDamp = damp; lastStrat = strat;

    const double a1 = xmapd (shape, 0.60, 0.03);
    const double a2 = xmapd (damp,  0.45, 0.015);

    double refEnergy = 0.0;
    auto kernelPeak = [&refEnergy] (double c1, double c2)
    {
        double z1 = 0, z2 = 0, pk = 0;
        for (int i = 0; i < 4096; ++i)
        {
            const double in = (i == 0) ? 1.0 : 0.0;
            z1 += c1 * (in - z1);
            z2 += c2 * (z1 - z2);
            pk = std::max (pk, std::abs (z1 - z2));
            refEnergy += (z1 - z2) * (z1 - z2);
        }
        return pk;
    };
    refEnergy = 0.0;
    const double ref = kernelPeak (a1, a2);
    /*  The ENERGY of one band kernel, not just its peak. A Q-14 resonator
        rings two hundred times longer than a band, so matching their peaks
        leaves the low body carrying almost all of the energy — measured,
        77.5 % of everything below 250 Hz, with the spectral centre dragged
        from 8.6 kHz down to 206 Hz. Levelling has to be done on energy. */
    bandEnergy = (float) refEnergy;
    bandRef = (float) ref;

    for (int b = 0; b < NBAND; ++b)
    {
        const double s = ((b + 0.5) / NBAND) * 2.0 - 1.0;          // -1 slow .. +1 fast
        const double mul = std::pow (2.0, s * (double) strat * 1.9);
        const double c1 = std::min (0.95, std::max (1.0e-5, a1 * mul));
        const double c2 = std::min (0.95, std::max (1.0e-5, a2 * mul));
        bands[b].a1 = (float) c1;
        bands[b].a2 = (float) c2;
        const double pk = kernelPeak (c1, c2);
        bands[b].gain = (float) (pk > 1.0e-12 ? ref / pk : 1.0);

        /*  The long mode: the same body at a tenth the speed. Scaling BOTH
            poles time-stretches the kernel instead of reshaping it, which is
            why this reads as the same object ringing rather than as a second
            filter — and because it is fed rather than crossfaded into, its
            level is set at the moment of the strike and nothing later can
            click it. Its peak is measured and brought to the band's own, so
            PERSISTENCE means the same thing in every band. */
        const double q1 = std::min (0.95, std::max (1.0e-6, c1 * 0.10));
        const double q2 = std::min (0.95, std::max (1.0e-6, c2 * 0.10));
        bands[b].ra1 = (float) q1;
        bands[b].ra2 = (float) q2;
        const double rpk = kernelPeak (q1, q2);
        bands[b].rgain = (float) (rpk > 1.0e-12 ? ref / rpk : 1.0);
    }
}

/*  THE LOW BODY AND THE CRATE.

    Between them they answer the two things the bank could not: the object had
    no energy below about a kilohertz, so every event was light; and the longest
    tail it could produce anywhere was 48 ms, so every event was short.

    The low body is three resonators at 1, 1.47 and 2.09 — deliberately not a
    harmonic series, so what arrives is a struck shell and not a note. Its pitch
    comes from ABSORPTION, i.e. from how dark the object already is, and NOT
    from the played note: a low body that followed the keyboard would be the
    oscillator this instrument exists to avoid.

    The crate is a four-line feedback delay network, Householder mixed, damped
    in the loop so the tail darkens as it goes, as a real box does. */
void Engine::rebuildBody()
{
    const float w = clampf (p.weight, 0.0f, 1.0f);
    const float sp = clampf (p.space, 0.0f, 1.0f);
    const float dm = clampf (p.damp, 0.0f, 1.0f);
    if (w == lastWeight && sp == lastSpace && dm == lastBodyDamp) return;
    lastWeight = w; lastSpace = sp; lastBodyDamp = dm;

    //  the shell: darker object, deeper crate
    const double f0 = 120.0 * std::pow (2.0, -1.2 * (double) dm);
    const double ratio[3] = { 1.0, 1.47, 2.09 };
    const double Q[3]     = { 14.0, 11.0, 8.0 };
    const double gain[3]  = { 1.0, 0.52, 0.30 };
    for (int m = 0; m < 3; ++m)
    {
        const double f = std::min (f0 * ratio[m], sr * 0.35);
        const double wn = 2.0 * PI * f / sr;
        const double r = std::exp (-wn / (2.0 * Q[m]));
        low.c[m]  = (float) (2.0 * r * std::cos (wn));
        low.rr[m] = (float) (r * r);
        /*  A resonator's impulse response peaks at about 1/sin(w), so without
            the sin(w) the low body arrives sixty times louder than the object.
            And levelled against ONE BAND's kernel peak, divided by the three
            modes' summed gain — otherwise WEIGHT does not add a low end, it
            replaces the object with one. First calibration measured 73.6 % of
            all energy below 250 Hz and the spectral centre down at 213 Hz from
            9124: audible, and useless. */
        low.g[m]  = (float) (std::sin (wn) * gain[m]);
    }

    /*  Now level the whole low body against one band kernel by ENERGY,
        measured by running its own impulse response, and trim it to the
        share of the sound it should be allowed at WEIGHT 1. */
    {
        double y1[3] = {0,0,0}, y2[3] = {0,0,0}, e = 0.0;
        for (int n = 0; n < (int) sr; ++n)
        {
            const double in = (n == 0) ? 1.0 : 0.0;
            double acc = 0.0;
            for (int m = 0; m < 3; ++m)
            {
                const double yv = in * low.g[m] + low.c[m] * y1[m] - low.rr[m] * y2[m];
                y2[m] = y1[m]; y1[m] = yv; acc += yv;
            }
            e += acc * acc;
        }
        /*  Against the WHOLE BANK, not against one band of it: the low body is
            struck by the drive of all NBAND bands together, while each band
            hears only its own share. Levelling against one left it eight times
            too energetic and it took 77 % of the sound. */
        const double k = e > 1.0e-18 ? std::sqrt ((double) bandEnergy / (e * NBAND)) : 0.0;
        /*  Its energy against the bank's at WEIGHT 1. Solved from the
            measurement rather than guessed: at 0.65 the low body took 72.7 %
            of everything below 250 Hz, which fixes the coupling, and 0.25
            puts it near a quarter — present, and still the same instrument. */
        const double trim = 0.25;
        for (int m = 0; m < 3; ++m) low.g[m] = (float) (low.g[m] * k * trim);
    }

    //  the crate: bigger box, longer lines and a longer tail
    const double scale = sr / 48000.0;
    const int base[4] = { 331, 461, 631, 809 };
    const double size = 0.45 + 1.75 * (double) sp;
    const double t60  = 0.12 + 3.10 * (double) sp * (double) sp;
    for (int k = 0; k < 4; ++k)
    {
        int L = (int) std::lround (base[k] * size * scale);
        L = std::max (16, std::min ((int) crate.line[k].size() - 1, L));
        crate.len[k] = L;
        if (crate.pos[k] >= L) crate.pos[k] = 0;
        crate.fb[k] = (float) std::pow (10.0, -3.0 * L / (sr * t60));
    }
    crate.damp = (float) (0.55 - 0.30 * sp);      // a bigger box swallows more top
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
        /*  The shove, as before — a touch is still felt at once.  */
        if (ph[(size_t)i] >= 0.0f)
            ph[(size_t)i] = (float) std::min (1.0, (double) ph[(size_t)i] + amount);
        /*  And the mark it leaves. Falls off towards the edge of the brush so a
            drag lays a stroke rather than a row of tiles, and accumulates, so
            going over the same place twice presses harder. */
        const float d = std::sqrt ((float)(dx*dx + dy*dy)) / (float) std::max (1, radius);
        const float w = clampf (1.0f - d, 0.0f, 1.0f);
        float& mk = mark[(size_t)i];
        mk = clampf (mk + (amount >= 0.0f ? 1.0f : -1.0f) * w * 0.55f, -1.0f, 1.0f);
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
        /*  A MARKED unit counts faster — up to two octaves either way. Rate is
            a unit's identity here, so a mark moves it into another layer: it
            speaks through a different body and the panel draws it a different
            colour. Drawing on the object really does redraw it. */
        const float mk = mark[(size_t)i];
        const float rm = mk == 0.0f ? 1.0f : std::exp2 (mk * 2.0f);
        if (v < 0.0f)                        // dead: count back up to zero
        {
            v += (float) (rate[(size_t)i] * rm * noteRate * dt);
            ph[(size_t)i] = v > 0.0f ? 0.0f : v;
            continue;
        }
        v += (float) (rate[(size_t)i] * rm * noteRate * dt);
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

    /*  How far this cascade reached. A tap in one corner and a wave that
        crosses the whole object are both "a cascade" to the code above, and
        until 260902.2 they arrived at the ear spread over exactly the same
        slice of the step. */
    if (fired.empty()) { lastExtent = 0.0f; return; }
    int lo[4] = { 255, 255, 255, 255 }, hi[4] = { 0, 0, 0, 0 };
    for (int i : fired)
    {
        const int c[4] = { ux[(size_t)i], uy[(size_t)i], uz[(size_t)i], uw[(size_t)i] };
        for (int k = 0; k < 4; ++k)
        {
            if (c[k] < lo[k]) lo[k] = c[k];
            if (c[k] > hi[k]) hi[k] = c[k];
        }
    }
    const float ext[4] = { NX - 1.0f, NY - 1.0f, NZ - 1.0f, NW - 1.0f };
    float acc = 0.0f;
    for (int k = 0; k < 4; ++k) acc += (float) (hi[k] - lo[k]) / ext[k];
    lastExtent = clampf (acc * 0.25f, 0.0f, 1.0f);
}

//==============================================================================
void Engine::process (float* L, float* R, int n)
{
    if (chunkCap <= 0) prepare (sr, n);
    for (int off = 0; off < n; )
    {
        const int m = std::min (n - off, chunkCap);
        processChunk (L + off, R + off, m);
        off += m;
    }

    /*  PEAK, HELD. This object is silent for ninety-nine per cent of its
        steps, so a per-block mean reads as nothing at all and the meter never
        moves while the thing is plainly audible. What is wanted is the loudest
        moment, released slowly. */
    float blockPeak = 0.0f;
    for (int i = 0; i < n; ++i) blockPeak = std::max (blockPeak, std::abs (L[i]));
    const float heldLvl = outLevel.load() * (float) std::pow (0.86, (double) n / 256.0);
    outLevel.store (std::max (blockPeak, heldLvl));
    pulsePhase.store ((float) beatPhase);
}

void Engine::processChunk (float* L, float* R, int n)
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

    /*  IT SLOWS TO A STOP; IT IS NOT SWITCHED OFF. (Peter, 260902.3: "it is
        unrealistic that the patterns stop dead at 77 K… there is a big change
        from 79 to 77 K, not realistic or meaningful.")

        He is right, and it was two separate cliffs. The rate law was
        `0.25 + 3.2 w²`, which still counts at a QUARTER SPEED at 77.1 K and
        then meets a hard `return` at 77.0 — so the object went from busy to
        muted across a tenth of a kelvin. Now the rate goes smoothly to zero as
        `3.45 w^1.55`, chosen so the two curves agree where it matters: at 800 K
        both give 3.45, and at room both give 0.53, so nothing about the object
        at any ordinary temperature has changed. What changed is the approach to
        the floor, which is now an object gradually running down.

        And the early return is GONE, so cooling it no longer cuts the body off
        mid-ring. The counting stops; whatever the body was holding rings out
        and the crate decays, which is what freezing something actually sounds
        like — and it is also the only way the crate's own tail could ever be
        measured, since the old return meant the room was never rendered. */
    /*  THE RATE LAW IS UNTOUCHED; A FADE IS APPLIED UNDER 99 K.

        The first attempt replaced 0.25 + 3.2w^2 with a smooth 3.45 w^1.55,
        matched at 800 K and at room to within one per cent. One per cent was
        far too much: at the default CONDUCTION the lattice sits near global
        synchrony and is exquisitely sensitive to how its own rate stands
        against the imposed pulse, and BOTH entrainment checks inverted — R
        fell across a take where it had risen. The counting must not be moved
        at any temperature anyone actually plays at.

        So the old law stands exactly everywhere above 99 K, and the last
        twenty-two kelvin carry a fade to zero. Above the fade this build is
        the same object it was; below it, it runs down instead of being
        switched off. The same gate holds the imposed pulse, or a frozen
        object would still be shoved into firing at 77 K. */
    const double coldGate = std::min (1.0, warmth / 0.03);
    const double heatRate = (0.25 + 3.2 * warmth * warmth) * coldGate;

    //  the imposed pulse
    const int dIdx = (int) clampf (std::round (p.division), 0.0f, 6.0f);
    const double beatsPer = DIVISION_BEATS[dIdx];
    double pulseHz;
    if (hostPlaying) pulseHz = (hostBpm / 60.0) / beatsPer;
    else             pulseHz = xmapd (p.freeHz, 0.15, 12.0);

    const double grip  = xmapd (p.grip, 0.05, 1.50);
    const int    reach = (int) std::lround (1.0 + clampf (p.reach, 0.0f, 1.0f) * (NX * 0.42));

    /*  THE SITE PULSE. A second imposed pulse, from the other findings on the
        bench. The local phase free-runs at the site's rate and is pulled
        towards the negotiated phase once per chunk; on wrap it shoves the
        lattice exactly as the host's pulse does, scaled by the pull. At pull
        0 nothing here runs — not even the phase — so an uncoupled render is
        the render it always was. */
    const double sPull = (double) sitePull.load();
    const double sHz   = (double) siteHz.load();
    if (sPull > 0.0)
    {
        double target = (double) sitePhaseIn.load();
        double diff = target - sitePhase;
        diff -= std::floor (diff + 0.5);                    // shortest way round
        sitePhase += diff * 0.35;                            // ease onto it
        sitePhase -= std::floor (sitePhase);
    }

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

    //  the bodies a firing can leave behind. Filters, not oscillators — the
    //  medium cannot respond instantly, and it does not respond the same
    //  everywhere.
    rebuildBands();
    rebuildBody();
    const float traverse = clampf (p.traverse, 0.0f, 1.0f);
    //  the planes are strided by chunkCap, so clear each one's first n samples
    for (int q = 0; q < NBAND * 2; ++q)
        std::memset (bandBuf.data() + (size_t) q * (size_t) chunkCap, 0,
                     sizeof (float) * (size_t) n);

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
                    ph[(size_t)i] = (float) gRise.shove (ph[(size_t)i], grip * coldGate);
                }
            }

            /*  the site pulse, on wrap. NOT the host's shove again: that one
                is a hand on the centre of the face, and measured against the
                site it was swallowed whole — eighteen shoves at grip and reach
                and the counting did not move a firing. The bench is not a
                hand, it is the ENVIRONMENT the object sits in, so it reaches
                the whole body, and hard enough that a cold body fires on the
                wraps. The pull — cold times close — is what fades it to
                nothing as the site warms. */
            if (sPull > 0.0)
            {
                sitePhase += dt * sHz;
                if (sitePhase >= 1.0)
                {
                    sitePhase -= std::floor (sitePhase);
                    siteWraps.fetch_add (1);
                    const double sg = sPull * coldGate * (0.55 + 0.45 * grip);
                    for (int i = 0; i < NUNIT; ++i)
                    {
                        if (ph[(size_t)i] < 0.0f) continue;
                        ph[(size_t)i] = (float) gRise.shove (ph[(size_t)i], sg);
                    }
                }
            }

            /*  THE MARK FADES. MEMORY is a time constant from two seconds to
                permanent; at the top the multiplier is exactly 1 and a mark
                stays until something else moves it. */
            {
                const float mem = clampf (p.memory, 0.0f, 1.0f);
                if (mem < 0.999f)
                {
                    const double tau = 2.0 * std::pow (140.0, (double) mem);
                    const float k = (float) std::exp (-dt / tau);
                    for (int i = 0; i < NUNIT; ++i) mark[(size_t)i] *= k;
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
            /*  TRAVERSE: how long this cascade takes to arrive is how far it
                reached. At 0 the multiplier is exactly 1 and EXTENSION means
                what it always meant. */
            const double spreadNow = spread
                * (double) ((1.0f - traverse) + traverse * (0.25f + 0.75f * lastExtent));
            for (int i : firedScratch)
            {
                const double proj = (pj[0] * ux[(size_t)i] + pj[1] * uy[(size_t)i]
                                   + pj[2] * uz[(size_t)i] + pj[3] * uw[(size_t)i] - pmin) / pspan;
                const int off = (int) (proj * spreadNow * stepSamples);
                const int at = sample + off;
                if (at < 0 || at >= n) continue;
                const float sgn = ((ux[(size_t)i] + uy[(size_t)i] + uz[(size_t)i] + uw[(size_t)i]) & 1) ? -1.0f : 1.0f;
                const float dep = (float) uw[(size_t)i] / std::max (1, NW - 1);
                const float amp = sgn * (0.35f + 0.65f * (tilt * dep + (1.0f - tilt) * (1.0f - dep)));
                const float pan = (float) ux[(size_t)i] / (NX - 1);
                /*  AND THE MARK MOVES THE BAND. Without this the mark changed only
                    how fast a unit counted, and at any real CONDUCTION the coupling
                    pins the collective rate and swallows it: measured, a second
                    after a drag the object differed by one per cent and MEMORY
                    made no difference at all. Rate is a unit's identity here, so a
                    marked unit must speak through the body its NEW rate belongs to
                    — which is what the comment already claimed and the code did
                    not do, because bandOf is built from the natural rate. */
                int bi = bandOf[(size_t)i];
                const float mki = mark[(size_t)i];
                if (mki != 0.0f) bi += (int) std::lround (mki * (NBAND - 1) * 0.5f);
                bi = bi < 0 ? 0 : (bi >= NBAND ? NBAND - 1 : bi);
                float* bl = bandBuf.data() + (size_t) (bi * 2) * (size_t) chunkCap;
                bl[at] += amp * (1.0f - pan);
                bl[(size_t) chunkCap + (size_t) at] += amp * pan;
            }

            /*  The panel samples this at thirty a second. Decaying a flare in
                thirty milliseconds means almost every firing happens between two
                frames and is never seen: the object was sounding and the picture
                was still. Two hundred milliseconds is long enough to be caught. */
            for (int i = 0; i < NUNIT; ++i) heat[(size_t)i] *= 0.9938f;
        }
        ++sample;
    }

    /*  Give the events their bodies, and keep the object inside its ceiling.

        PERSISTENCE, and why it needs no constant tuned per specimen: each band
        watches its own drive at two speeds — `env` is what is arriving (2 ms
        up, 300 ms down, so a convulsion holds the body open after it has
        passed) and `slow` is what usually arrives over a couple of seconds.
        Their RATIO is the excess, and the excess opens the slow pole. At the
        object's own ordinary busyness the ratio is one and nothing happens; a
        cascade thirty times the usual size rings, whether this specimen's
        ordinary is six thousand firings a second or forty thousand. */
    const float ceil = 0.30f + 0.62f * (1.0f - clampf (p.sat, 0.0f, 1.0f));
    const float persist = clampf (p.persist, 0.0f, 1.0f);
    const float aUp   = (float) (1.0 - std::exp (-1.0 / (0.0008 * sr)));
    const float aDown = (float) (1.0 - std::exp (-1.0 / (0.3000 * sr)));
    const float aSlow = (float) (1.0 - std::exp (-1.0 / (2.0000 * sr)));
    const float halfG = g * 0.5f;
    float sendSeen = 0.0f;

    const float weight = clampf (p.weight, 0.0f, 1.0f);
    const float room   = clampf (p.space,  0.0f, 1.0f);

    for (int i = 0; i < n; ++i)
    {
        float x = 0.0f, y = 0.0f, hit = 0.0f;
        for (int b = 0; b < NBAND; ++b)
        {
            Band& B = bands[b];
            const float* pl = bandBuf.data() + (size_t) (b * 2) * (size_t) chunkCap;
            const float xl = pl[i], xr = pl[(size_t) chunkCap + (size_t) i];
            hit  += std::abs (xl) + std::abs (xr);
            /*  The SIGNED sum was tried here and is wrong, though it took a
                measurement to see why. Sign comes from coordinate parity, so a
                smooth cascade cancels — but that rule governs what the object
                RADIATES, not what it is struck by. A blow is compressive:
                nine thousand impacts push the same way whatever the parity of
                their coordinates. Striking the low body with the signed train
                made WEIGHT do nothing at all at the defaults (4.5 % of energy
                below 250 Hz against 4.6 %), because at that density the
                cancellation is very nearly total. Magnitude it is. */


            B.z1 += B.a1 * (xl - B.z1);
            B.z2 += B.a2 * (B.z1 - B.z2);
            x += (B.z1 - B.z2) * B.gain;
            B.w1 += B.a1 * (xr - B.w1);
            B.w2 += B.a2 * (B.w1 - B.w2);
            y += (B.w1 - B.w2) * B.gain;

            if (persist <= 0.0f) continue;      // adds nothing, and adds it exactly

            const float drv = std::abs (xl) + std::abs (xr);
            B.env  += (drv > B.env ? aUp : aDown) * (drv - B.env);
            /*  `slow` follows `env`, NOT the raw drive. Following the drive
                measures the average SAMPLE, and in an object that is silent
                between events every event is enormous against that — measured,
                the send pinned at its cap for all of them and discriminated
                nothing. Following the envelope measures the average EVENT,
                which is the comparison that was wanted: big for THIS object. */
            B.slow += aSlow * (B.env - B.slow);
            const float excess = B.env / (B.slow + 1.0e-9f) - 1.0f;
            const float send = persist * clampf (excess * (1.0f / 3.0f), 0.0f, 1.0f);
            if (send > sendSeen) sendSeen = send;

            B.r1 += B.ra1 * (xl * send - B.r1);
            B.r2 += B.ra2 * (B.r1 - B.r2);
            x += (B.r1 - B.r2) * B.rgain;
            B.s1 += B.ra1 * (xr * send - B.s1);
            B.s2 += B.ra2 * (B.s1 - B.s2);
            y += (B.s1 - B.s2) * B.rgain;
        }
        /*  THE LOW BODY. Struck by the EXCESS — how hard the object is being
            hit against how hard it is usually hit — so it answers a convulsion
            and ignores the ordinary chatter. Feeding it the raw drive instead
            would hold three low resonators open continuously and the object
            would hum, which is the opposite of weight. */
        if (weight > 0.0f)
        {
            gEnv  += (hit > gEnv ? aUp : aDown) * (hit - gEnv);
            gSlow += aSlow * (gEnv - gSlow);
            const float ex = gEnv / (gSlow + 1.0e-9f) - 1.0f;
            const float exc = hit * weight * clampf (ex * (1.0f / 3.0f), 0.0f, 1.0f);
            float lo = 0.0f;
            for (int m = 0; m < 3; ++m)
            {
                const float yv = exc * low.g[m] + low.c[m] * low.y1[m] - low.rr[m] * low.y2[m];
                low.y2[m] = low.y1[m]; low.y1[m] = yv;
                lo += yv;
            }
            //  the low body is one object, so it arrives in the middle
            x += lo; y += lo;
        }

        x *= halfG; y *= halfG;

        /*  THE CRATE. Fed post-gain so ENCLOSURE is a property of the room and
            not of how hard the object is driven into it. */
        if (room > 0.0f)
        {
            const float in = (x + y) * 0.5f;
            float t[4];
            for (int k = 0; k < 4; ++k) t[k] = crate.line[k][(size_t) crate.pos[k]];
            const float sum = 0.5f * (t[0] + t[1] + t[2] + t[3]);   // Householder, N=4
            for (int k = 0; k < 4; ++k)
            {
                float v = (t[k] - sum) * crate.fb[k] + in;
                crate.lp[k] += crate.damp * (v - crate.lp[k]);      // the box swallows the top
                crate.line[k][(size_t) crate.pos[k]] = crate.lp[k];
                if (++crate.pos[k] >= crate.len[k]) crate.pos[k] = 0;
            }
            const float wet = room * 0.9f;
            x += wet * (t[0] - t[3]);
            y += wet * (t[1] - t[2]);
        }
        //  a soft ceiling, applied always: transparent below it, asymptotic above
        auto soft = [ceil] (float v) {
            const float t = v / ceil;
            return ceil * (t / (1.0f + std::abs (t) * 0.55f));
        };
        /*  AND A BOUND ON TOP OF IT, because the ceiling above asymptotes to
            ceil/0.55 — which is 1.67 at a wide-open CEILING, so the control
            named for the level the object will not exceed could be exceeded by
            two thirds. Nothing had ever driven it that hard; the long mode
            does, and the worst peak across the catalogue went to 1.29.

            This is deliberately a HARD KNEE and not a gentler curve everywhere.
            Any smooth saturator that binds at full scale also bends the quiet
            signal — the first attempt raised the ceiling's own knee coefficient
            and moved every sample in the take by about a per cent, measured as
            a residual 40 dB down where there should have been nothing at all.
            Identity below 0.85 and a tanh above it is exactly transparent for
            everything the object used to make, and touches only what would
            have left the building above full scale. */
        auto bound = [] (float v) {
            const float a = std::abs (v);
            if (a <= 0.85f) return v;
            const float o = 0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f);
            return v < 0.0f ? -o : o;
        };
        L[i] = bound (soft (x)); R[i] = bound (soft (y));
    }
    if (sendSeen > maxRingSend.load()) maxRingSend.store (sendSeen);
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
            out.mark [y * NX + x] = (uint8_t) clampf (128.0f + mark[(size_t)i] * 127.0f, 0.0f, 255.0f);
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

    /*  APPENDED, AND THAT IS THE WHOLE OF WHY IT IS SAFE. The draws above are
        taken in order from one sequence, so adding to the END leaves every
        earlier draw bit-identical: specimen 90's rates, coupling, projection
        and dead time are exactly what they were at 260902.1, and a number is
        still a specimen for good. What it gains is a body with layers — which
        is the point, since the catalogue is where the variety is heard. */
    p.strat    = rng (0.15f, 1.00f);
    p.persist  = rng (0.00f, 0.85f);
    p.traverse = rng (0.20f, 1.00f);
    p.weight   = rng (0.10f, 0.75f);
    p.memory   = rng (0.20f, 0.90f);

    /*  HALF THE CATALOGUE IS DRAWN A DIFFERENT WAY (260902.3).

        The generator above makes BUSY objects — several thousand firings a
        second — and at that density events overlap, so a long tail and a heavy
        blow are heard as texture rather than as events. Everything built in
        260902.2 and .3 is therefore nearly inaudible across the old catalogue,
        which is not a fault in the mechanisms but in what the specimens ask of
        them. So the odd half is re-drawn SPARSE and RESONANT: slow counts, so
        events stand apart; enough conduction to make real cascades; and a body
        that is given time and room to answer.

        The split is by the SPECIMEN NUMBER and not by a draw, so it is legible
        from the dial: even numbers are the objects as they were, odd numbers
        are the ones that show what the body can now do. */
    if (index & 1)
    {
        //  slow enough that one event finishes before the next begins
        p.rateLo = rng (0.00f, 0.10f);
        p.rateHi = p.rateLo + rng (0.04f, 0.30f);
        //  in the regime where cascades are large but not continuous
        p.couple = rng (0.35f, 0.70f);
        p.dead   = rng (0.20f, 0.60f);
        p.leak   = rng (0.25f, 0.85f);
        //  and a body with somewhere to put it
        p.strat    = rng (0.45f, 1.00f);
        p.persist  = rng (0.40f, 1.00f);
        p.traverse = rng (0.35f, 1.00f);
        p.weight   = rng (0.35f, 1.00f);
        p.space    = rng (0.25f, 0.85f);
        p.shape    = rng (0.30f, 0.90f);
        p.damp     = rng (0.35f, 0.90f);
        p.grip     = rng (0.10f, 0.55f);
    }
}

} // namespace ab1
