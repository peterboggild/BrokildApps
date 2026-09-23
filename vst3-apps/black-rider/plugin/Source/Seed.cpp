/*  BLACK RIDER — the seed library.

    Two hundred deterministic patches. The same number is always the same
    instrument: everything comes from the seed through one seeded RNG, no
    clock, no session state, so seed 137 is seed 137 on anyone's machine.

    The design decision that makes the categories honest: a seed is assigned
    a CATEGORY first, and its parameters are then generated to fit that
    category. We never generate a random patch and guess what it is — so the
    name and the category on the menu always match the sound.

    Names are injective within a category (adjective x noun walked as a grid),
    and the noun lists are disjoint across categories, so no two of the two
    hundred share a name. */
#include "Engine.h"
#include <cstring>

namespace bk
{

//==============================================================================
struct CatDef { const char* name; int count; const char* const* adj; int nAdj; const char* const* noun; int nNoun; };

// adjectives — reused freely; nouns — kept disjoint per category so names are globally unique
static const char* const A_WARM[]  = { "Warm", "Deep", "Round", "Fat", "Smooth", "Velvet", "Amber", "Low" };
static const char* const A_HARD[]  = { "Hard", "Sharp", "Acid", "Bright", "Cold", "Steel", "Neon", "Razor" };
static const char* const A_SOFT[]  = { "Soft", "Slow", "Distant", "Hazy", "Pale", "Dawn", "Drifting", "Quiet" };
static const char* const A_BOLD[]  = { "Bold", "Big", "Wide", "Rising", "Proud", "Bronze", "Open", "Grand" };
static const char* const A_ODD[]   = { "Odd", "Broken", "Ghost", "Hollow", "Alien", "Rusted", "Cracked", "Static" };

static const char* const N_BASS[]  = { "Bass", "Sub", "Cellar", "Root", "Anchor", "Foundation" };
static const char* const N_ACID[]  = { "Acid", "Squelch", "303", "Reptile", "Worm", "Slime" };
static const char* const N_LEAD[]  = { "Lead", "Rider", "Blade", "Solo", "Cutter", "Arrow" };
static const char* const N_PLUCK[] = { "Pluck", "Drop", "Tick", "Spark", "Pip", "Stab" };
static const char* const N_PAD[]   = { "Pad", "Cloud", "Veil", "Field", "Drift", "Aura" };
static const char* const N_STR[]   = { "Strings", "Ensemble", "Bow", "Section", "Choir", "Cello" };
static const char* const N_ORG[]   = { "Organ", "Chapel", "Drawbar", "Reed", "Vox", "Pipe" };
static const char* const N_BRASS[] = { "Brass", "Horn", "Fanfare", "Trumpet", "Section", "Blare" };
static const char* const N_KEYS[]  = { "Keys", "Bell", "Piano", "Chime", "Tine", "Glass" };
static const char* const N_DRONE[] = { "Drone", "Engine", "Hum", "Motor", "Tone", "Nave" };
static const char* const N_SYNC[]  = { "Sync", "Zap", "Sweep", "Tear", "Rip", "Whip" };
static const char* const N_NOISE[] = { "Noise", "Wind", "Wash", "Hiss", "Storm", "Dust" };

enum { C_BASS = 0, C_ACID, C_LEAD, C_PLUCK, C_PAD, C_STR, C_ORG, C_BRASS, C_KEYS, C_DRONE, C_SYNC, C_NOISE, NUM_CATS };

static const CatDef CATS[NUM_CATS] =
{
    { "BASS",    26, A_WARM, 8, N_BASS, 6 },
    { "ACID",    12, A_HARD, 8, N_ACID, 6 },
    { "LEAD",    24, A_HARD, 8, N_LEAD, 6 },
    { "PLUCK",   20, A_HARD, 8, N_PLUCK, 6 },
    { "PAD",     22, A_SOFT, 8, N_PAD, 6 },
    { "STRINGS", 16, A_SOFT, 8, N_STR, 6 },
    { "ORGAN",   16, A_WARM, 8, N_ORG, 6 },
    { "BRASS",   12, A_BOLD, 8, N_BRASS, 6 },
    { "KEYS",    14, A_WARM, 8, N_KEYS, 6 },
    { "DRONE",   16, A_SOFT, 8, N_DRONE, 6 },
    { "SYNC",    10, A_ODD,  8, N_SYNC, 6 },
    { "NOISE",   12, A_ODD,  8, N_NOISE, 6 },
};

int         seedNumCategories()          { return NUM_CATS; }
const char* seedCategoryName (int cat)   { return CATS[cat < 0 ? 0 : (cat >= NUM_CATS ? 0 : cat)].name; }

/*  A bijection over 0..199 spreads the categories across the seed space, so
    stepping the dial by one changes character. Because it is a bijection and
    each category owns a contiguous block of the permuted index, a seed's rank
    within its category is just (index - blockStart) — unique, O(1). */
static inline int perm (int seed) { return (seed * 89 + 41) % NUM_SEEDS; }

void seedInfo (int seed, int& cat, int& rank)
{
    seed = seed < 0 ? 0 : (seed >= NUM_SEEDS ? NUM_SEEDS - 1 : seed);
    const int idx = perm (seed);
    int acc = 0;
    for (int c = 0; c < NUM_CATS; ++c)
    {
        if (idx < acc + CATS[c].count) { cat = c; rank = idx - acc; return; }
        acc += CATS[c].count;
    }
    cat = 0; rank = 0;
}

std::string seedName (int seed)
{
    int c, rank; seedInfo (seed, c, rank);
    const auto& d = CATS[c];
    const char* a = d.adj[rank % d.nAdj];
    const char* n = d.noun[(rank / d.nAdj) % d.nNoun];
    return std::string (a) + " " + n;
}

//==============================================================================
namespace
{
    // note offsets, as the o?semi knob value (0.5 = 0 semitones, +-24 range)
    inline float semiKnob (int st) { return clamp01 (0.5f + (float) st / 24.0f); }
    inline float hzKnob (float hz, float lo, float hi) { return clamp01 (std::log (hz / lo) / std::log (hi / lo)); }
    inline float msKnob (float ms, float lo, float hi) { return clamp01 (std::log (ms / lo) / std::log (hi / lo)); }

    struct Gen
    {
        Params& p; Rng& r;
        float u()            { return r.uni(); }
        float rr (float a, float b) { return a + r.uni() * (b - a); }
        bool  ch (float pr)  { return r.uni() < pr; }
        int   pick (int n)   { return (int) (r.u32() % (uint32_t) n); }
        float pickf (std::initializer_list<float> v) { const float* a = v.begin(); return a[pick ((int) v.size())]; }

        void lp (float hz)   { p.lpf = hzKnob (hz, 20.0f, 20000.0f); }
        void hp (float hz)   { p.hpf = hzKnob (hz, 20.0f, 8000.0f); }
        void e1 (float a, float d, float s, float rl) { p.e1a = msKnob (a, 0.5f, 10000.0f); p.e1d = msKnob (d, 2.0f, 15000.0f); p.e1s = s; p.e1r = msKnob (rl, 2.0f, 15000.0f); }
        void e2 (float a, float d, float s, float rl) { p.e2a = msKnob (a, 0.5f, 10000.0f); p.e2d = msKnob (d, 2.0f, 15000.0f); p.e2s = s; p.e2r = msKnob (rl, 2.0f, 15000.0f); }
        void lrate (float hz){ p.lrate = clamp01 (std::log (hz / 0.02f) / std::log (2000.0f / 0.02f)); }
        void cable (int slot, int src, int dst, float amt) { p.cable[slot].src = (float) src; p.cable[slot].dst = (float) dst; p.cable[slot].amt = 0.5f + 0.5f * amt; }
    };
}

//==============================================================================
void seedPatch (int seed, Params& p)
{
    seed = seed < 0 ? 0 : (seed >= NUM_SEEDS ? NUM_SEEDS - 1 : seed);
    p = Params();
    p.seed = (float) seed;
    p.volume = 0.5f; p.os = 1;

    Rng r; r.seed (0x9e3779b9u ^ ((uint32_t) seed * 2654435761u + 1013904223u));
    int cat, rank; seedInfo (seed, cat, rank);
    Gen g { p, r };

    // a little vintage on almost everything, more on the old-fashioned voices
    p.vintage = g.rr (0.2f, 0.5f);

    switch (cat)
    {
        case C_BASS:
            p.mode = 0; p.o1wave = (float) g.pickf ({0, 0, 2}); p.o1oct = (float) g.pickf ({1, 1, 0});
            p.o1lvl = g.rr (0.7f, 0.9f);
            p.o2wave = (float) g.pickf ({0, 2}); p.o2oct = p.o1oct; p.o2fine = g.rr (0.47f, 0.53f); p.o2lvl = g.ch (0.6f) ? g.rr (0.4f, 0.7f) : 0.0f;
            p.suboct = 0; p.sublvl = g.ch (0.5f) ? g.rr (0.15f, 0.5f) : 0.0f;
            p.fmodel = (float) g.pickf ({2, 2, 0}); g.lp (g.rr (180.0f, 700.0f)); p.lpeak = g.rr (0.1f, 0.45f);
            p.fenv = g.rr (0.62f, 0.78f); p.fkey = g.rr (0.2f, 0.5f); p.fdrive = g.rr (0.2f, 0.5f);
            g.e1 (g.rr (1.0f, 8.0f), g.rr (120.0f, 500.0f), g.rr (0.0f, 0.25f), g.rr (80.0f, 300.0f));
            g.e2 (g.rr (1.0f, 6.0f), g.rr (200.0f, 700.0f), g.rr (0.6f, 0.9f), g.rr (60.0f, 250.0f));
            p.glide = g.ch (0.3f) ? g.rr (0.15f, 0.35f) : 0.0f; p.drv = g.ch (0.5f) ? g.rr (0.1f, 0.4f) : 0.0f;
            break;

        case C_ACID:
            p.mode = 0; p.o1wave = (float) g.pickf ({0, 2}); p.o1oct = 1; p.o1lvl = 0.85f; p.o2lvl = 0.0f;
            p.fmodel = 2; g.lp (g.rr (120.0f, 400.0f)); p.lpeak = g.rr (0.6f, 0.88f);
            p.fenv = g.rr (0.72f, 0.9f); p.fkey = g.rr (0.1f, 0.4f); p.fdrive = g.rr (0.4f, 0.7f);
            g.e1 (0.5f, g.rr (80.0f, 260.0f), 0.0f, g.rr (60.0f, 200.0f));
            g.e2 (0.5f, g.rr (120.0f, 320.0f), g.rr (0.4f, 0.7f), g.rr (60.0f, 160.0f));
            p.glide = g.rr (0.2f, 0.4f); p.legato = 1; p.drv = g.rr (0.25f, 0.5f); p.drvtone = g.rr (0.4f, 0.7f);
            if (g.ch (0.4f)) { p.dlmix = g.rr (0.12f, 0.3f); p.dlfb = g.rr (0.25f, 0.5f); p.dltime = g.rr (0.45f, 0.7f); }
            break;

        case C_LEAD:
            p.mode = 0; p.o1wave = (float) g.pickf ({0, 2}); p.o1oct = (float) g.pickf ({2, 2, 1});
            p.o2wave = (float) g.pickf ({0, 2}); p.o2fine = g.rr (0.53f, 0.6f); p.o2lvl = g.rr (0.4f, 0.7f);
            if (g.ch (0.3f)) p.o2semi = semiKnob (g.pick (2) ? 7 : -12);
            p.fmodel = (float) g.pickf ({0, 1}); g.lp (g.rr (600.0f, 2600.0f)); p.lpeak = g.rr (0.3f, 0.6f);
            p.fenv = g.rr (0.55f, 0.72f); p.fkey = g.rr (0.4f, 0.8f); p.fdrive = g.rr (0.2f, 0.45f);
            g.e1 (g.rr (2.0f, 30.0f), g.rr (250.0f, 800.0f), g.rr (0.25f, 0.6f), g.rr (150.0f, 500.0f));
            g.e2 (g.rr (2.0f, 20.0f), g.rr (300.0f, 700.0f), g.rr (0.75f, 0.95f), g.rr (150.0f, 400.0f));
            p.glide = g.ch (0.6f) ? g.rr (0.2f, 0.4f) : 0.0f; p.legato = 1;
            p.lrate = g.rr (0.45f, 0.6f); p.lpitch = g.ch (0.4f) ? g.rr (0.03f, 0.1f) : 0.0f; p.lwheel = g.rr (0.4f, 0.7f);
            if (g.ch (0.6f)) { p.dlmix = g.rr (0.12f, 0.3f); p.dlfb = g.rr (0.2f, 0.4f); }
            if (g.ch (0.3f)) p.chmix = g.rr (0.2f, 0.4f);
            break;

        case C_PLUCK:
            p.mode = (float) g.pickf ({0, 2}); p.spread = g.rr (0.5f, 0.9f);
            p.o1wave = (float) g.pickf ({0, 2, 1}); p.o1oct = (float) g.pickf ({2, 2, 3});
            p.o2wave = (float) g.pickf ({0, 2}); p.o2fine = g.rr (0.5f, 0.56f); p.o2lvl = g.ch (0.6f) ? g.rr (0.3f, 0.6f) : 0.0f;
            p.fmodel = (float) g.pickf ({0, 2}); g.lp (g.rr (400.0f, 1600.0f)); p.lpeak = g.rr (0.2f, 0.55f);
            p.fenv = g.rr (0.65f, 0.82f); p.fkey = g.rr (0.3f, 0.7f);
            g.e1 (0.5f, g.rr (60.0f, 260.0f), 0.0f, g.rr (60.0f, 200.0f));
            g.e2 (0.5f, g.rr (90.0f, 320.0f), g.rr (0.0f, 0.2f), g.rr (60.0f, 220.0f));
            if (g.ch (0.6f)) { p.dlmix = g.rr (0.15f, 0.35f); p.dlfb = g.rr (0.25f, 0.5f); p.dltime = g.rr (0.4f, 0.7f); }
            if (g.ch (0.3f)) p.spmix = g.rr (0.12f, 0.3f);
            break;

        case C_PAD:
            p.mode = (float) g.pickf ({2, 1}); p.spread = g.rr (0.7f, 1.0f); p.udet = g.rr (0.2f, 0.5f);
            p.o1wave = (float) g.pickf ({0, 1}); p.o2wave = (float) g.pickf ({0, 1}); p.o2oct = (float) g.pickf ({2, 1});
            p.o2fine = g.rr (0.44f, 0.56f); p.o2lvl = g.rr (0.4f, 0.7f); p.sublvl = g.ch (0.5f) ? g.rr (0.15f, 0.4f) : 0.0f;
            p.fmodel = (float) g.pickf ({2, 0}); g.lp (g.rr (400.0f, 1400.0f)); p.lpeak = g.rr (0.05f, 0.4f);
            p.fenv = g.rr (0.45f, 0.6f); p.fkey = g.rr (0.2f, 0.5f); p.flfo = g.ch (0.6f) ? g.rr (0.1f, 0.35f) : 0.0f;
            g.e1 (g.rr (300.0f, 2000.0f), g.rr (800.0f, 4000.0f), g.rr (0.4f, 0.8f), g.rr (600.0f, 3000.0f));
            g.e2 (g.rr (300.0f, 2500.0f), g.rr (1000.0f, 5000.0f), g.rr (0.8f, 1.0f), g.rr (800.0f, 4000.0f));
            g.lrate (g.rr (0.1f, 0.6f)); p.chmix = g.rr (0.3f, 0.6f); p.chdepth = g.rr (0.4f, 0.8f);
            p.spmix = g.rr (0.2f, 0.5f); p.spdwell = g.rr (0.5f, 0.85f); p.vintage = g.rr (0.3f, 0.6f);
            break;

        case C_STR:
            p.mode = 1; p.udet = g.rr (0.15f, 0.4f); p.spread = g.rr (0.6f, 0.9f);
            p.o1wave = 0; p.o2wave = 0; p.o2oct = (float) g.pickf ({2, 1}); p.o2fine = g.rr (0.46f, 0.54f); p.o2lvl = g.rr (0.4f, 0.6f);
            p.fmodel = (float) g.pickf ({2, 0}); g.lp (g.rr (700.0f, 2000.0f)); p.lpeak = g.rr (0.05f, 0.3f);
            p.fenv = g.rr (0.5f, 0.6f); p.fkey = g.rr (0.3f, 0.6f);
            g.e1 (g.rr (150.0f, 900.0f), g.rr (600.0f, 2500.0f), g.rr (0.5f, 0.8f), g.rr (400.0f, 1500.0f));
            g.e2 (g.rr (120.0f, 700.0f), g.rr (800.0f, 2500.0f), g.rr (0.8f, 1.0f), g.rr (400.0f, 1500.0f));
            g.lrate (g.rr (4.0f, 6.5f)); p.lpitch = g.rr (0.03f, 0.09f);       // ensemble shimmer
            p.chmix = g.rr (0.4f, 0.65f); p.chdepth = g.rr (0.5f, 0.85f); p.spmix = g.ch (0.6f) ? g.rr (0.15f, 0.35f) : 0.0f;
            break;

        case C_ORG:
            p.mode = (float) g.pickf ({2, 1}); p.spread = g.rr (0.4f, 0.8f);
            p.o1wave = (float) g.pickf ({3, 1}); p.o1oct = (float) g.pickf ({2, 1});
            p.o2wave = (float) g.pickf ({3, 1}); p.o2oct = (float) g.pickf ({3, 2}); p.o2lvl = g.rr (0.4f, 0.7f);
            p.suboct = 0; p.sublvl = g.ch (0.6f) ? g.rr (0.2f, 0.5f) : 0.0f;
            p.fmodel = 2; g.lp (g.rr (1500.0f, 6000.0f)); p.lpeak = g.rr (0.0f, 0.2f);
            p.fenv = 0.5f; p.fkey = g.rr (0.3f, 0.6f);                          // no filter sweep: drawbar tone
            g.e1 (2.0f, 400.0f, 1.0f, 100.0f);
            g.e2 (g.rr (1.0f, 6.0f), 400.0f, 1.0f, g.rr (30.0f, 120.0f));       // fast on/off
            p.vcamode = g.ch (0.4f) ? 1.0f : 0.0f;                              // GATE or EG2
            p.chmix = g.rr (0.25f, 0.55f); p.chrate = g.rr (0.3f, 0.6f);
            if (g.ch (0.4f)) p.spmix = g.rr (0.12f, 0.3f);
            break;

        case C_BRASS:
            p.mode = (float) g.pickf ({0, 1}); p.udet = g.rr (0.1f, 0.3f); p.spread = g.rr (0.4f, 0.7f);
            p.o1wave = 0; p.o1oct = 2; p.o2wave = 0; p.o2fine = g.rr (0.52f, 0.58f); p.o2lvl = g.rr (0.5f, 0.75f);
            p.o1pw = g.rr (0.0f, 0.4f); p.o1pwm = g.ch (0.4f) ? g.rr (0.2f, 0.5f) : 0.0f;
            p.fmodel = (float) g.pickf ({0, 2}); g.lp (g.rr (500.0f, 1400.0f)); p.lpeak = g.rr (0.2f, 0.5f);
            p.fenv = g.rr (0.62f, 0.75f); p.fkey = g.rr (0.4f, 0.7f); p.fdrive = g.rr (0.2f, 0.4f);
            g.e1 (g.rr (40.0f, 160.0f), g.rr (300.0f, 800.0f), g.rr (0.5f, 0.75f), g.rr (150.0f, 400.0f));   // the swell
            g.e2 (g.rr (40.0f, 140.0f), g.rr (300.0f, 700.0f), g.rr (0.8f, 0.95f), g.rr (150.0f, 400.0f));
            if (g.ch (0.5f)) p.chmix = g.rr (0.2f, 0.4f);
            break;

        case C_KEYS:
            p.mode = 2; p.spread = g.rr (0.4f, 0.8f);
            if (g.ch (0.5f)) {   // FM bell
                p.o1wave = 3; p.o1lvl = 0.0f; p.o2wave = 3; p.o2lvl = 0.9f;
                p.o1semi = semiKnob (g.pickf ({7, 12, 5} ) != 0 ? (int) g.pickf ({7, 12, 5}) : 7);
                p.o2fm = g.rr (0.3f, 0.6f);
            } else {             // electric piano: two detuned sines/tri
                p.o1wave = (float) g.pickf ({3, 1}); p.o2wave = p.o1wave; p.o2fine = g.rr (0.53f, 0.58f); p.o2lvl = g.rr (0.5f, 0.7f);
            }
            p.fmodel = 2; g.lp (g.rr (1200.0f, 5000.0f)); p.lpeak = g.rr (0.0f, 0.2f);
            p.fenv = g.rr (0.5f, 0.62f); p.fkey = g.rr (0.3f, 0.6f);
            g.e1 (0.5f, g.rr (300.0f, 1200.0f), 0.0f, g.rr (200.0f, 600.0f));
            g.e2 (g.rr (0.5f, 4.0f), g.rr (500.0f, 2000.0f), g.rr (0.0f, 0.25f), g.rr (200.0f, 700.0f));
            p.chmix = g.rr (0.2f, 0.45f); if (g.ch (0.4f)) p.spmix = g.rr (0.12f, 0.3f);
            break;

        case C_DRONE:
            p.mode = (float) g.pickf ({1, 2}); p.udet = g.rr (0.2f, 0.5f); p.spread = g.rr (0.6f, 1.0f);
            p.vcamode = 2;                                                       // DRONE: sounds with no key
            p.o1wave = (float) g.pickf ({0, 1}); p.o2wave = (float) g.pickf ({0, 1}); p.o2oct = (float) g.pickf ({1, 0}); p.o2fine = g.rr (0.44f, 0.56f); p.o2lvl = g.rr (0.4f, 0.7f);
            p.suboct = (float) g.pick (2); p.sublvl = g.rr (0.2f, 0.5f);
            p.fmodel = (float) g.pickf ({2, 0}); g.lp (g.rr (300.0f, 1200.0f)); p.lpeak = g.rr (0.15f, 0.5f);
            p.fenv = 0.5f; p.flfo = g.rr (0.15f, 0.45f); g.lrate (g.rr (0.05f, 0.4f)); p.lwave = (float) g.pickf ({0, 3, 5});
            p.spmix = g.rr (0.3f, 0.55f); p.spdwell = g.rr (0.6f, 0.9f); p.chmix = g.ch (0.5f) ? g.rr (0.2f, 0.45f) : 0.0f;
            p.vintage = g.rr (0.4f, 0.7f);
            break;

        case C_SYNC:
            p.mode = 0; p.o1lvl = 0.0f; p.o2lvl = 0.85f; p.o2sync = 1; p.o2wave = 0; p.o2oct = 2;
            p.o2semi = semiKnob (g.pickf ({0, 7, 5}) == 0 ? 0 : (int) g.pickf ({7, 5, 12}));
            p.fmodel = (float) g.pickf ({2, 1}); g.lp (g.rr (900.0f, 4000.0f)); p.lpeak = g.rr (0.05f, 0.3f);
            p.fenv = g.rr (0.5f, 0.6f);
            g.e1 (g.rr (1.0f, 20.0f), g.rr (300.0f, 1200.0f), g.rr (0.0f, 0.3f), g.rr (150.0f, 500.0f));
            g.e2 (g.rr (1.0f, 15.0f), g.rr (400.0f, 900.0f), g.rr (0.6f, 0.9f), g.rr (150.0f, 400.0f));
            g.cable (0, S_EG1, D_P2, g.rr (0.4f, 0.85f));                        // the sync sweep
            if (g.ch (0.5f)) p.drv = g.rr (0.15f, 0.35f);
            if (g.ch (0.5f)) { p.dlmix = g.rr (0.15f, 0.35f); p.dlfb = g.rr (0.25f, 0.5f); }
            break;

        default:  // C_NOISE — percussion and fx
            p.mode = 0; p.o1lvl = g.ch (0.5f) ? g.rr (0.2f, 0.5f) : 0.0f; p.o1wave = (float) g.pick (4);
            p.o2lvl = g.ch (0.4f) ? g.rr (0.2f, 0.5f) : 0.0f; p.o2semi = semiKnob (g.pick (24) - 12);
            p.nzlvl = g.rr (0.4f, 0.9f); p.nzcol = (float) g.pick (2); p.ringlvl = g.ch (0.5f) ? g.rr (0.3f, 0.7f) : 0.0f;
            p.fmodel = (float) g.pick (3); g.lp (g.rr (300.0f, 6000.0f)); p.lpeak = g.rr (0.3f, 0.85f); g.hp (g.ch (0.5f) ? g.rr (100.0f, 1500.0f) : 20.0f);
            p.fenv = g.rr (0.3f, 0.85f); p.fkey = g.rr (0.0f, 0.5f);
            g.e1 (0.5f, g.rr (40.0f, 400.0f), g.rr (0.0f, 0.4f), g.rr (60.0f, 400.0f));
            g.e2 (0.5f, g.rr (60.0f, 500.0f), g.rr (0.0f, 0.4f), g.rr (60.0f, 500.0f));
            p.lwave = 4; g.lrate (g.rr (2.0f, 40.0f)); if (g.ch (0.6f)) g.cable (0, S_SH, D_LPF, g.rr (0.3f, 0.8f));
            if (g.ch (0.5f)) { p.dlmix = g.rr (0.15f, 0.4f); p.dlfb = g.rr (0.3f, 0.6f); }
            if (g.ch (0.4f)) p.spmix = g.rr (0.2f, 0.45f);
            break;
    }
}

} // namespace bk
