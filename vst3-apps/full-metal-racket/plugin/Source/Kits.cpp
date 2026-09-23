/*  FULL METAL RACKET — the kit library.

    Two hundred kits on a dial, generated the way Black Rider generates its
    patches and Blade Ruiner its moods: the CATEGORY is decided first, from a
    bijective permutation of the seed into contiguous blocks, and only then
    are the parameters generated to fit it. Doing it the other way round —
    roll parameters, then label what came out — is how you end up with a kit
    called JAZZ BRUSHES that is a wall of industrial noise.

    Everything here is a pure function of the seed, so a kit is reproducible
    from one integer, on any machine, forever.                                */

#include "Engine.h"
#include <cstring>
#include <string>
#include <vector>

namespace fmr
{

//==============================================================================
/*  A category is a set of biases, not a set of values. Each channel then
    derives its own parameters from the bias plus its family's needs plus a
    little seeded scatter — which is why two house kits are recognisably both
    house and audibly not the same kit. */
struct CatDef
{
    const char* name;
    int   count;
    float tune, decay, tone, snap, bend, drive;   // -1..+1 biases
    float sag, bleed, body, age;                  // the machine's own character, 0..1
    float scatter;                                // how far the kits in this family roam
    int   kick, snare, tom, perc, metal;          // model, -1 = let the seed choose
};

static const CatDef CATS[] =
{
    //  name            n   tune  decay tone  snap  bend  drive  sag   bleed body  age   scat  k  s  t  p  m
    { "HOUSE",         18,  0.05f,-0.10f, 0.15f, 0.25f, 0.00f, 0.05f, 0.22f,0.20f,0.16f,0.22f, 0.16f, 1, 0, 1, 1, 0 },
    { "TECHNO",        20, -0.20f, 0.10f,-0.10f, 0.30f, 0.10f, 0.35f, 0.42f,0.30f,0.22f,0.34f, 0.20f, 1, 1,-1, 2, 2 },
    { "EIGHT OH EIGHT",18, -0.15f, 0.35f,-0.20f,-0.15f,-0.10f,-0.05f, 0.30f,0.22f,0.16f,0.34f, 0.14f, 0, 0, 0, 0, 0 },
    { "BREAKS",        16,  0.10f,-0.20f, 0.20f, 0.35f, 0.15f, 0.20f, 0.34f,0.44f,0.34f,0.46f, 0.22f,-1, 1, 0,-1,-1 },
    { "INDUSTRIAL",    16, -0.25f, 0.15f,-0.30f, 0.20f, 0.35f, 0.70f, 0.62f,0.58f,0.46f,0.66f, 0.24f, 2, 1, 0, 1, 2 },
    { "JAZZ",          12,  0.15f, 0.05f, 0.30f,-0.20f, 0.25f,-0.30f, 0.14f,0.52f,0.42f,0.30f, 0.18f, 2, 0, 0, 1, 1 },
    { "LO-FI",         14, -0.05f,-0.15f,-0.35f,-0.10f, 0.05f, 0.30f, 0.46f,0.40f,0.36f,0.70f, 0.20f, 0, 0, 1, 1, 2 },
    { "TRAP",          16, -0.35f, 0.30f,-0.05f, 0.30f, 0.45f, 0.15f, 0.30f,0.16f,0.12f,0.20f, 0.18f, 1, 2, 1, 3, 0 },
    { "DUB",           14, -0.15f, 0.35f,-0.15f,-0.25f, 0.20f, 0.10f, 0.50f,0.50f,0.52f,0.44f, 0.20f, 0, 0, 0, 0, 1 },
    { "ELECTRO",       16,  0.00f,-0.05f, 0.25f, 0.40f,-0.05f, 0.10f, 0.24f,0.18f,0.14f,0.22f, 0.18f, 1, 0, 2, 2, 0 },
    { "ACOUSTIC",      20,  0.10f, 0.10f, 0.10f, 0.05f, 0.55f,-0.25f, 0.20f,0.62f,0.56f,0.34f, 0.20f, 2, 1, 0, 1, 1 },
    { "EXPERIMENTAL",  20,  0.00f, 0.20f, 0.00f, 0.15f, 0.40f, 0.35f, 0.44f,0.54f,0.44f,0.52f, 0.42f,-1,-1,-1,-1,-1 }
};
static const int NUM_CATS = (int) (sizeof (CATS) / sizeof (CATS[0]));
static const int NUM_SEEDS = 200;

//==============================================================================
/*  Names. Each category owns its own adjectives AND its own nouns — a shared
    noun list eventually produces something absurd in a category it does not
    belong to, and there is no prompt-level fix for that. The walk uses a
    stride coprime with the grid so neighbouring seeds land far apart, which
    is also what makes every name in a category distinct. */
struct WordSet { const char* const* adj; int na; const char* const* noun; int nn; };

static const char* A_HOUSE[]  = { "WARM", "LATE", "SUNDAY", "PAPER", "VELVET", "SOFT" };
static const char* N_HOUSE[]  = { "LOFT", "GARAGE", "BASEMENT", "CHICAGO", "PIANO", "SHUFFLE" };
static const char* A_TECHNO[] = { "COLD", "IRON", "TUNNEL", "BLIND", "GREY", "ENDLESS" };
static const char* N_TECHNO[] = { "HANGAR", "CONVEYOR", "DETROIT", "TRANSIT", "PISTON", "GRID" };
static const char* A_808[]    = { "CLASSIC", "LONG", "PURPLE", "SLOW", "DEEP", "SMOOTH" };
static const char* N_808[]    = { "BOOM", "CADILLAC", "SUBWOOFER", "TAPE", "CASSETTE", "BOULEVARD" };
static const char* A_BREAK[]  = { "CHOPPED", "DUSTY", "AMEN", "BROKEN", "TIGHT", "CRISP" };
static const char* N_BREAK[]  = { "LOOP", "CRATE", "BREAKBEAT", "SAMPLER", "JUNGLE", "SHUFFLE" };
static const char* A_IND[]    = { "RUSTED", "BURNT", "SHEET", "FURNACE", "SCRAP", "BROKEN" };
static const char* N_IND[]    = { "FOUNDRY", "MILL", "HAMMER", "BOILER", "YARD", "PRESS" };
static const char* A_JAZZ[]   = { "BRUSHED", "QUIET", "CLUB", "LATE", "SUPPER", "SOFT" };
static const char* N_JAZZ[]   = { "KIT", "RIDE", "BALLAD", "BRUSHES", "TRIO", "SESSION" };
static const char* A_LOFI[]   = { "WORN", "MUDDY", "FOURTH", "CHEAP", "FADED", "SLEEPY" };
static const char* N_LOFI[]   = { "CARTRIDGE", "DUB", "RADIO", "BEDROOM", "WALKMAN", "GENERATION" };
static const char* A_TRAP[]   = { "SUB", "SOUTHERN", "ROLLING", "SLOW", "HEAVY", "NIGHT" };
static const char* N_TRAP[]   = { "808", "TRIPLET", "ATLANTA", "ROLL", "BASSLINE", "HIGHWAY" };
static const char* A_DUB[]    = { "DEEP", "ECHO", "KINGSTON", "HEAVY", "STEPPING", "MIDNIGHT" };
static const char* N_DUB[]    = { "CHAMBER", "SPRING", "RIDDIM", "SOUNDSYSTEM", "SKANK", "DELAY" };
static const char* A_ELEC[]   = { "CHROME", "VECTOR", "NEON", "SHARP", "PLASTIC", "MIAMI" };
static const char* N_ELEC[]   = { "BODY", "ARCADE", "FREESTYLE", "CIRCUIT", "BOOGIE", "DRIVE" };
static const char* A_ACOU[]   = { "MAPLE", "BIRCH", "OPEN", "STUDIO", "LIVE", "OAK" };
static const char* N_ACOU[]   = { "ROOM", "SHELL", "STAGE", "FLOOR", "HALL", "TIMBER" };
static const char* A_EXP[]    = { "GLASS", "BONE", "STRANGE", "HOLLOW", "INVERTED", "PAPER" };
static const char* N_EXP[]    = { "ORGANISM", "APPARATUS", "SPECIMEN", "ARTEFACT", "MECHANISM", "GEOMETRY" };

static const WordSet WORDS[] =
{
    { A_HOUSE, 6, N_HOUSE, 6 }, { A_TECHNO, 6, N_TECHNO, 6 }, { A_808, 6, N_808, 6 },
    { A_BREAK, 6, N_BREAK, 6 }, { A_IND, 6, N_IND, 6 },       { A_JAZZ, 6, N_JAZZ, 6 },
    { A_LOFI, 6, N_LOFI, 6 },   { A_TRAP, 6, N_TRAP, 6 },     { A_DUB, 6, N_DUB, 6 },
    { A_ELEC, 6, N_ELEC, 6 },   { A_ACOU, 6, N_ACOU, 6 },     { A_EXP, 6, N_EXP, 6 }
};

//==============================================================================
/*  (seed * 89 + 41) mod 200 is a bijection, because 89 is coprime with 200 —
    so every seed lands in exactly one category slot, no slot is used twice,
    and neighbouring seeds are thrown far apart. */
static int permute (int seed) { return ((seed * 89) + 41) % NUM_SEEDS; }

static void seedInfo (int seed, int& cat, int& rank)
{
    const int q = permute (seed < 0 ? 0 : (seed >= NUM_SEEDS ? NUM_SEEDS - 1 : seed));
    int at = 0;
    for (int c = 0; c < NUM_CATS; ++c)
    {
        if (q < at + CATS[c].count) { cat = c; rank = q - at; return; }
        at += CATS[c].count;
    }
    cat = NUM_CATS - 1; rank = 0;
}

int         numSeeds()               { return NUM_SEEDS; }
int         numSeedCategories()      { return NUM_CATS; }
const char* seedCategoryName (int c) { return CATS[c < 0 ? 0 : (c >= NUM_CATS ? NUM_CATS - 1 : c)].name; }

int seedCategory (int seed) { int c = 0, r = 0; seedInfo (seed, c, r); return c; }

const char* seedName (int seed)
{
    static std::vector<std::string> cache;
    if (cache.empty())
    {
        cache.resize ((size_t) NUM_SEEDS);
        for (int s = 0; s < NUM_SEEDS; ++s)
        {
            int c = 0, r = 0; seedInfo (s, c, r);
            const WordSet& w = WORDS[c];
            const int cap = w.na * w.nn;
            //  stride 7 is coprime with 36, so a category's ranks walk the
            //  whole grid without repeating before it is exhausted
            const int at = (r * 7) % cap;
            cache[(size_t) s] = std::string (w.adj[at / w.nn]) + " " + w.noun[at % w.nn];
        }
    }
    return cache[(size_t) (seed < 0 ? 0 : (seed >= NUM_SEEDS ? NUM_SEEDS - 1 : seed))].c_str();
}

//==============================================================================
namespace
{
    inline float bias (float base, float b, float jitter)
    {
        return clamp01 (base + b * 0.34f + jitter);
    }
    inline int pickModel (int want, Rng& r, int nModels)
    {
        return want >= 0 ? (want % nModels) : (int) (r.uni() * (float) nModels) % nModels;
    }
}

void applySeed (int seed, Params& p)
{
    p = Params();                                    // always from the defaults
    seed = seed < 0 ? 0 : (seed >= NUM_SEEDS ? NUM_SEEDS - 1 : seed);
    int cat = 0, rank = 0;
    seedInfo (seed, cat, rank);
    const CatDef& C = CATS[cat];

    Rng r; r.seed ((uint32_t) (seed * 2654435761u + 0x9E37u));
    const float sc = C.scatter;
    auto j = [&r, sc] { return (r.uni() * 2.0f - 1.0f) * sc; };

    for (int c = 0; c < NCH; ++c)
    {
        const int fam = channelFamily (c);
        float* P = p.ch[c];

        int nm = 0;
        listNames ((std::string (channelId (c)) + "_model").c_str(), nm);
        if (nm < 1) nm = 1;
        const int want = fam == FAM_KICK ? C.kick : fam == FAM_SNARE ? C.snare
                       : fam == FAM_TOM  ? C.tom  : fam == FAM_PERC  ? C.perc : C.metal;
        P[CP_MODEL] = (float) pickModel (want, r, nm);

        /*  The defaults are already a good kit, so a category BIASES them
            rather than replacing them — which is why even EXPERIMENTAL, with
            the widest scatter, still comes back as a drum kit. */
        P[CP_TUNE]  = bias (P[CP_TUNE],  C.tune,  j());
        P[CP_DECAY] = bias (P[CP_DECAY], C.decay, j());
        P[CP_TONE]  = bias (P[CP_TONE],  C.tone,  j());
        P[CP_SNAP]  = bias (P[CP_SNAP],  C.snap,  j());
        P[CP_BEND]  = bias (P[CP_BEND],  C.bend,  j());
        P[CP_DRIVE] = clamp01 (P[CP_DRIVE] + C.drive * 0.28f + j() * 0.5f);

        //  A little width, never so much that the kit falls apart in mono
        if (fam == FAM_TOM || fam == FAM_METAL || fam == FAM_PERC)
            P[CP_PAN] = clampf (0.5f + (r.uni() * 2.0f - 1.0f) * 0.22f, 0.15f, 0.85f);

        /*  REBOUND is a garnish and the generator should barely reach for it.
            At 0.45 it is five bounces closing to forty milliseconds — on a hat
            lane already playing sixteenths that is not a flam, it is a blast
            beat, and the kit stops sounding like a kit. One channel in twenty,
            and never past a flam. */
        P[CP_REBOUND] = r.uni() < 0.05f ? 0.06f + r.uni() * 0.12f : 0.0f;
    }

    p.g[GP_SAG]   = clamp01 (C.sag   + j() * 0.5f);
    p.g[GP_BLEED] = clamp01 (C.bleed + j() * 0.5f);
    p.g[GP_BODY]  = clamp01 (C.body  + j() * 0.5f);
    p.g[GP_AGE]   = clamp01 (C.age   + j() * 0.5f);
    p.g[GP_KITTUNE] = clampf (0.5f + (r.uni() * 2.0f - 1.0f) * 0.18f, 0.2f, 0.8f);
}

//==============================================================================
//  The kit dial the panel drives is the seed library.
int         numKits()       { return NUM_SEEDS; }
const char* kitName (int i) { return seedName (i); }
void        applyKit (int i, Params& p) { applySeed (i, p); }

} // namespace fmr
