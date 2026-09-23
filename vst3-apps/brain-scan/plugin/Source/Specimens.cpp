/*  BRAIN SCAN — the specimens (stock volumes) and the factory patches.

    x is the phase axis: a straight line along x reads the specimen's natural
    waveform at that (y, z). y and z are the two timbre axes. Everything here is
    procedural, VN^3 (128), in [0, 1]; the panel gets a 64^3 copy to render.

    Musically normal by construction where it says so — SPINE at (y, z) is a
    harmonic series with a known rolloff and parity, PULSE at y a pulse of a
    known width — and the bench checks both against their formulas. CORTEX is
    the show-piece and makes no such promise.
*/
#include "Engine.h"
#include "Anatomy.h"
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace bs
{

static inline float clamp01f (float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float smooth (float e0, float e1, float x)
{
    const float t = clamp01f ((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

//  a small deterministic value noise, C1 by smoothstep, three octaves
static inline uint32_t hash3 (int x, int y, int z)
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) z * 2147483647u + 0x9E3779B9u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static inline float vnoise (float x, float y, float z)
{
    const int ix = (int) std::floor (x), iy = (int) std::floor (y), iz = (int) std::floor (z);
    const float fx = x - (float) ix, fy = y - (float) iy, fz = z - (float) iz;
    const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy), sz = fz * fz * (3 - 2 * fz);
    auto v = [] (int a, int b, int c) { return (float) (hash3 (a, b, c) & 0xffff) / 65535.0f; };
    const float c00 = v (ix, iy, iz) + (v (ix + 1, iy, iz) - v (ix, iy, iz)) * sx;
    const float c10 = v (ix, iy + 1, iz) + (v (ix + 1, iy + 1, iz) - v (ix, iy + 1, iz)) * sx;
    const float c01 = v (ix, iy, iz + 1) + (v (ix + 1, iy, iz + 1) - v (ix, iy, iz + 1)) * sx;
    const float c11 = v (ix, iy + 1, iz + 1) + (v (ix + 1, iy + 1, iz + 1) - v (ix, iy + 1, iz + 1)) * sx;
    const float c0 = c00 + (c10 - c00) * sy, c1 = c01 + (c11 - c01) * sy;
    return c0 + (c1 - c0) * sz;
}
static inline float fbm (float x, float y, float z, int oct, float lac = 2.0f, float gain = 0.5f)
{
    float a = 0.5f, s = 0, norm = 0;
    for (int o = 0; o < oct; ++o) { s += a * vnoise (x, y, z); norm += a; x *= lac; y *= lac; z *= lac; a *= gain; }
    return s / norm;
}

//==============================================================================
struct SpecDef { const char* name; const char* gloss; float (*f) (float x, float y, float z); bool periodicX; bool hu; const char* window; };

static float fSinus (float x, float y, float z)
{
    const float amp = 0.35f + 0.65f * y;
    //  the second harmonic starts a little way up z, so the bottom face is a
    //  pure sine even through the B-spline's support (measured: -47 dB of
    //  second harmonic at z = 0 without the dead zone)
    const float zz = clamp01f ((z - 0.08f) / 0.92f);
    const float s = (1.0f - zz) * std::sin (2.0f * (float) PI * x) + zz * 0.5f * std::sin (4.0f * (float) PI * x);
    return 0.5f + 0.5f * amp * s;
}

static float fSpine (float x, float y, float z)
{
    //  harmonic stack: y = brightness (rolloff 1/n^3.5 .. 1/n^0.6), z = parity (all .. odd only)
    //  48 harmonics at 128 texels: the top face is brighter than a saw
    const float roll = 0.6f + 2.9f * std::pow (1.0f - y, 1.5f);
    float s = 0, norm = 0;
    for (int n = 1; n <= 48; ++n)
    {
        float a = std::pow ((float) n, -roll);
        if ((n & 1) == 0) a *= (1.0f - z);
        s += a * std::sin (2.0f * (float) PI * (float) n * x);
        norm += a;
    }
    //  normalise to a peak near 0.9 whatever the mix: the sum of the amplitudes bounds it
    return 0.5f + 0.5f * (s / norm) * 1.6f * 0.55f;
}

static float fPulse (float x, float y, float z)
{
    const float w = 0.05f + 0.90f * y;
    //  the bottom face is as hard an edge as a texel can hold (half a texel
    //  at 128); z rounds it off
    const float e = 0.004f + 0.20f * z;
    const float lo = 0.5f - w * 0.5f, hi = 0.5f + w * 0.5f;
    return smooth (lo - e, lo + e, x) * (1.0f - smooth (hi - e, hi + e, x));
}

static float fMarrow (float x, float y, float z)
{
    //  a ramp, bent by y (0.5 = straight), folded toward a triangle by z
    const float bend = std::pow (2.0f, (y - 0.5f) * 4.0f);
    const float ramp = std::pow (x, bend);
    const float tri = 1.0f - std::abs (2.0f * x - 1.0f);
    return (1.0f - z) * ramp + z * tri;
}

static float fNerve (float x, float y, float z)
{
    //  phase modulation: index by y, ratio by z (1 .. 5, continuous)
    const float I = 6.0f * y;
    const float k = 1.0f + 4.0f * z;
    return 0.5f + 0.5f * std::sin (2.0f * (float) PI * x + I * std::sin (2.0f * (float) PI * k * x));
}

static float fRetina (float x, float y, float z)
{
    //  two decaying formants per cycle: vowel-like
    const float F1 = 2.0f + 6.0f * y, F2 = 6.0f + 14.0f * z;
    const float a = std::exp (-5.0f * x) * std::sin (2.0f * (float) PI * F1 * x);
    const float b = 0.6f * std::exp (-3.5f * x) * std::sin (2.0f * (float) PI * F2 * x);
    return 0.5f + 0.5f * (a + b) * 0.9f;
}

/*  THE BODIES (260905.1) are built in Hounsfield units in Anatomy.cpp and
    mapped -1000..2000 HU onto the cube's 0..1 — so a radiographer's window
    means what it says on them, and a bone is a bone. */
static float fSkull    (float x, float y, float z) { return an::toUnit (an::head (x, y, z, false)); }
static float fHead     (float x, float y, float z) { return an::toUnit (an::head (x, y, z, true)); }
static float fThorax   (float x, float y, float z) { return an::toUnit (an::thorax (x, y, z)); }
static float fVertebra (float x, float y, float z) { return an::toUnit (an::vertebra (x, y, z)); }
static float fFemur    (float x, float y, float z) { return an::toUnit (an::femur (x, y, z)); }
static float fJaw      (float x, float y, float z) { return an::toUnit (an::jaw (x, y, z)); }

/*  THE GRITTY THREE (260904.3). Peter: "the waveforms are fairly sinusy … a
    layer of granularity there could be and which isn't quite there yet". The
    original nine are mostly smooth functions; these three are made of edges,
    spikes and beats — the things a filter has something to do with. */
static float fSuture (float x, float y, float z)
{
    /*  A random staircase, periodic in x: y sets the steps per cycle (4 to
        32), z walks through eight patterns, blending between neighbours. A
        step is a hard edge held for a while — every harmonic, then silence
        until the next — and a texel-by-texel read (GRAIN up) keeps the
        edges. Between patterns the steps cross-fade, so a line moving in z
        hears the stairs re-arranging themselves. */
    const int steps = 4 + (int) std::lround (28.0f * y);
    const float zp = z * 7.0f;
    const int p0 = (int) std::floor (zp);
    const float t = zp - (float) p0;
    int k = (int) std::floor (x * (float) steps); if (k >= steps) k = steps - 1; if (k < 0) k = 0;
    auto val = [&] (int pat) { return 0.12f + 0.76f * (float) (hash3 (k, steps, pat) & 0xffff) / 65535.0f; };
    return val (p0) * (1.0f - t) + val (p0 + 1) * t;
}

static float fEnamel (float x, float y, float z)
{
    /*  A comb of spikes: z the number per cycle (1 to 8), y their width. A
        narrow spike is a pulse train with every harmonic in it up to the
        texel rate; wide ones are a soft bell-shaped hump. One spike at y = 0
        is the buzziest thing in the instrument. */
    const int n = 1 + (int) std::lround (7.0f * z);
    const float sig = 0.008f + 0.06f * y * y;                  // a texel wide at the bottom
    float s = 0;
    for (int k = 0; k < n; ++k)
    {
        float d = x - ((float) k + 0.5f) / (float) n;
        d -= std::floor (d + 0.5f);                            // wrap to -0.5 .. 0.5
        s += std::exp (-(d * d) / (2.0f * sig * sig));
    }
    return clamp01f (0.22f + 0.78f * s);
}

static float fTendon (float x, float y, float z)
{
    /*  Four partials whose ratios slide from harmonic (y = 0: 1 2 3 4) to a
        bell's (y = 1: 1 2.76 5.40 8.93); z their brightness. Not periodic in
        x once the ratios leave the integers: the wrap is an edge, the cycle
        is a strike, and the partials beat against the fundamental — the
        anharmonic content Peter asked for, without any noise. */
    static const float bell[4] = { 1.0f, 2.756f, 5.404f, 8.933f };
    static const float harm[4] = { 1.0f, 2.0f,   3.0f,   4.0f   };
    float s = 0, norm = 0;
    for (int k = 0; k < 4; ++k)
    {
        const float r = harm[k] + (bell[k] - harm[k]) * y;
        const float a = std::pow ((float) (k + 1), -(1.7f - 1.4f * z));
        s += a * std::sin (2.0f * (float) PI * r * x);
        norm += a;
    }
    return 0.5f + 0.5f * (s / norm) * 0.9f;
}

static const SpecDef SPECIMENS[] =
{
    //  the phantoms: calibration objects, each a formula, each a promise
    { "SINUS",  "a sine; y its amplitude, z a touch of second harmonic",                fSinus,  true,  false, ""      },
    { "SPINE",  "a harmonic stack; y brightness, z parity (saw to square)",             fSpine,  true,  false, ""      },
    { "PULSE",  "a pulse; y its width, z the softness of its edges",                    fPulse,  true,  false, ""      },
    { "MARROW", "a ramp; y its curvature, z folds it toward a triangle",                fMarrow, false, false, ""      },
    { "NERVE",  "a phase-modulated sine; y the index, z the modulator's ratio",         fNerve,  true,  false, ""      },
    { "RETINA", "two decaying formants per cycle; y and z their positions",             fRetina, false, false, ""      },
    //  the bodies: anatomy in Hounsfield units, a bone is a bone
    { "THORAX", "a chest CT: ribs, spine, sternum, lungs with their vessels, the heart", fThorax, false, true, "LUNG"  },
    { "SKULL",  "the skull alone: vault, orbits, sinuses, jaw and teeth, no soft tissue", fSkull, false, true, "BONE"  },
    { "CORTEX", "a head CT: the same skull with brain, ventricles, eyes and scalp",      fHead,   false, true,  "BRAIN" },
    { "SUTURE", "a random staircase; y the steps per cycle, z walks the patterns",      fSuture, true,  false, ""      },
    { "ENAMEL", "a comb of spikes; z how many per cycle, y their width",                fEnamel, true,  false, ""      },
    { "TENDON", "four partials; y slides them from harmonic to a bell, z brightness",   fTendon, false, false, ""      },
    { "VERTEBRA","three lumbar vertebrae: bodies, discs, canal, processes, the muscles", fVertebra, false, true, "BONE" },
    { "FEMUR",  "a thigh: the femoral shaft, its marrow, muscle compartments, vessels",  fFemur,  false, true,  "BONE"  },
    { "JAW",    "the mandible with both rows of teeth, the tongue and the palate",      fJaw,    false, true,  "BONE"  },
};
static const int NSPECIMENS = (int) (sizeof (SPECIMENS) / sizeof (SPECIMENS[0]));
static_assert (NSPECIMENS == 15, "the SPECIMEN parameter has fifteen slots; change both or neither");

int numSpecimens() { return NSPECIMENS; }
const char* specimenName (int i)  { return SPECIMENS[i < 0 ? 0 : (i >= NSPECIMENS ? NSPECIMENS - 1 : i)].name; }
const char* specimenGloss (int i) { return SPECIMENS[i < 0 ? 0 : (i >= NSPECIMENS ? NSPECIMENS - 1 : i)].gloss; }
bool specimenPeriodicX (int i)    { return SPECIMENS[i < 0 ? 0 : (i >= NSPECIMENS ? NSPECIMENS - 1 : i)].periodicX; }
bool specimenIsHu (int i)         { return SPECIMENS[i < 0 ? 0 : (i >= NSPECIMENS ? NSPECIMENS - 1 : i)].hu; }
const char* specimenWindow (int i) { return SPECIMENS[i < 0 ? 0 : (i >= NSPECIMENS ? NSPECIMENS - 1 : i)].window; }

/*  Two million texels of CORTEX is half a second on one core. The build is
    split across threads (pure per-texel work), and the last four specimens
    built are kept, so dialling back to one is a copy, not a build. The cache
    is per process — every instance in the DAW shares it. */
namespace
{
    struct Cached { int which = -1; unsigned stamp = 0; std::vector<float> v; };
    Cached     CACHE[4];
    unsigned   CACHE_STAMP = 0;
    std::mutex CACHE_LOCK;
}

void buildSpecimen (int which, float* out)
{
    which = which < 0 ? 0 : (which >= NSPECIMENS ? NSPECIMENS - 1 : which);
    const size_t N = (size_t) VN * VN * VN;
    std::lock_guard<std::mutex> guard (CACHE_LOCK);
    for (auto& e : CACHE)
        if (e.which == which && e.v.size() == N)
        {
            std::memcpy (out, e.v.data(), N * sizeof (float));
            e.stamp = ++CACHE_STAMP;
            return;
        }
    const SpecDef& s = SPECIMENS[which];
    auto slab = [&] (int k0, int k1)
    {
        for (int k = k0; k < k1; ++k)
            for (int j = 0; j < VN; ++j)
                for (int i = 0; i < VN; ++i)
                {
                    //  cell-centred, so a coordinate of 0.5 lands on a texel centre
                    const float x = ((float) i + 0.5f) / (float) VN, y = ((float) j + 0.5f) / (float) VN, z = ((float) k + 0.5f) / (float) VN;
                    out[((size_t) k * VN + (size_t) j) * VN + (size_t) i] = clamp01f (s.f (x, y, z));
                }
    };
    int nt = (int) std::thread::hardware_concurrency();
    nt = nt < 1 ? 1 : (nt > 8 ? 8 : nt);
    std::vector<std::thread> pool;
    for (int t = 0; t < nt; ++t)
        pool.emplace_back (slab, VN * t / nt, VN * (t + 1) / nt);
    for (auto& th : pool) th.join();

    Cached* victim = &CACHE[0];
    for (auto& e : CACHE)
    {
        if (e.which < 0) { victim = &e; break; }
        if (e.stamp < victim->stamp) victim = &e;
    }
    victim->which = which;
    victim->stamp = ++CACHE_STAMP;
    victim->v.assign (out, out + N);
}

//==============================================================================
//  factory patches — lines and parameter overrides; the specimen is a parameter
static float listVal (int idx, int n) { return n > 1 ? (float) idx / (float) (n - 1) : 0.0f; }

static void facAdmission (Line* l, Params& p)
{
    p.specimen = listVal (1, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.05f, 0.25f, 0.45f }, { 0.95f, 0.25f, 0.45f });
    l[L_WAVE_B] = Line::straight ({ 0.05f, 0.85f, 0.15f }, { 0.95f, 0.85f, 0.15f });
    l[L_FILT_A] = Line::straight ({ 0.10f, 0.35f, 0.50f }, { 0.45f, 0.35f, 0.50f });
    l[L_FILT_B] = Line::straight ({ 0.10f, 0.80f, 0.50f }, { 0.90f, 0.80f, 0.50f });
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.2f, 1, 8);
    l[L_MOD_B]  = Line::circle ({ 0.5f, 0.6f, 0.5f }, 0.3f, 2, 8);
    p.scan = 0.15f; p.scanAmt = 0.72f; p.scanA = 0.30f; p.scanD = 0.62f;
    p.filtType = 0; p.cutoff = 0.62f; p.reso = 0.25f; p.filtDepth = 0.55f; p.filtMode = 0; p.filtRate = 0.30f;
    p.modScan = 0.5f; p.modPitch = 0.5f; p.modPan = 0.62f; p.modRate = 0.25f;
    p.ampA = 0.10f; p.ampD = 0.55f; p.ampS = 0.7f; p.ampR = 0.45f;
}

static void facSinusRhythm (Line* l, Params& p)
{
    p.specimen = listVal (0, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.05f, 0.80f, 0.10f }, { 0.95f, 0.80f, 0.10f });
    l[L_WAVE_B] = Line::straight ({ 0.05f, 0.95f, 0.85f }, { 0.95f, 0.95f, 0.85f });
    l[L_FILT_A] = Line::straight ({ 0.2f, 0.5f, 0.5f }, { 0.8f, 0.5f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.25f, 2, 6);
    l[L_MOD_B]  = l[L_MOD_A];
    p.scan = 0.0f; p.scanAmt = 0.5f;
    p.filtType = 3; p.modScan = 0.62f; p.modRate = 0.2f; p.modPitch = 0.5f; p.modPan = 0.5f;
    p.unison = listVal (1, 4); p.detune = 0.15f; p.spread = 0.6f;
    p.ampA = 0.35f; p.ampD = 0.5f; p.ampS = 0.85f; p.ampR = 0.55f;
}

static void facCortex (Line* l, Params& p)
{
    p.specimen = listVal (8, NSPECIMENS);
    //  the head is a real head now: the rings must cross the skull to say anything
    l[L_WAVE_A] = Line::circle ({ 0.50f, 0.48f, 0.55f }, 0.34f, 2, 12);
    l[L_WAVE_B] = Line::helix ({ 0.50f, 0.48f, 0.50f }, 0.33f, 0.4f, 1.5f, 12);
    l[L_FILT_A] = Line::straight ({ 0.10f, 0.48f, 0.55f }, { 0.90f, 0.48f, 0.55f });
    l[L_FILT_B] = Line::straight ({ 0.50f, 0.05f, 0.60f }, { 0.50f, 0.95f, 0.60f });
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.4f }, 0.3f, 2, 8);
    l[L_MOD_B]  = Line::circle ({ 0.5f, 0.5f, 0.7f }, 0.15f, 0, 8);
    p.scan = 0.3f; p.scanAmt = 0.65f; p.scanA = 0.45f; p.scanD = 0.7f;
    p.filtType = 0; p.cutoff = 0.7f; p.reso = 0.35f; p.filtDepth = 0.8f; p.filtMode = 1; p.filtRate = 0.22f;
    p.modScan = 0.68f; p.modPan = 0.7f; p.modRate = 0.18f;
    p.unison = listVal (1, 4); p.detune = 0.2f; p.spread = 0.7f;
    p.ampA = 0.45f; p.ampD = 0.6f; p.ampS = 0.8f; p.ampR = 0.62f;
}

static void facPulseOx (Line* l, Params& p)
{
    p.specimen = listVal (2, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.02f, 0.20f, 0.20f }, { 0.98f, 0.20f, 0.20f });
    l[L_WAVE_B] = Line::straight ({ 0.02f, 0.80f, 0.40f }, { 0.98f, 0.80f, 0.40f });
    l[L_FILT_A] = Line::straight ({ 0.1f, 0.9f, 0.9f }, { 0.9f, 0.1f, 0.9f });
    l[L_FILT_B] = Line::straight ({ 0.1f, 0.5f, 0.9f }, { 0.9f, 0.5f, 0.9f });
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.35f, 2, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.scan = 0.2f; p.scanAmt = 0.5f;
    p.filtType = 0; p.cutoff = 0.55f; p.reso = 0.45f; p.filtDepth = 0.7f; p.filtMode = 0; p.filtRate = 0.42f;
    p.modScan = 0.75f; p.modRate = 0.35f;
    p.ampA = 0.05f; p.ampD = 0.45f; p.ampS = 0.6f; p.ampR = 0.35f;
}

static void facMarrow (Line* l, Params& p)
{
    p.specimen = listVal (3, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.02f, 0.50f, 0.05f }, { 0.98f, 0.50f, 0.05f });
    l[L_WAVE_B] = Line::straight ({ 0.02f, 0.85f, 0.95f }, { 0.98f, 0.85f, 0.95f });
    l[L_FILT_A] = Line::straight ({ 0.1f, 0.2f, 0.5f }, { 0.9f, 0.9f, 0.5f });
    l[L_FILT_B] = Line::straight ({ 0.1f, 0.2f, 0.5f }, { 0.3f, 0.3f, 0.5f });
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.25f, 1, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.scan = 0.0f; p.scanAmt = 0.6f; p.scanA = 0.05f; p.scanD = 0.5f;
    p.filtType = 0; p.cutoff = 0.5f; p.reso = 0.3f; p.filtDepth = 0.9f; p.filtMode = 0; p.filtRate = 0.36f;
    p.unison = listVal (2, 4); p.detune = 0.3f; p.spread = 0.8f;
    p.ampA = 0.02f; p.ampD = 0.5f; p.ampS = 0.7f; p.ampR = 0.4f;
}

static void facNerve (Line* l, Params& p)
{
    p.specimen = listVal (4, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.02f, 0.15f, 0.10f }, { 0.98f, 0.15f, 0.10f });
    l[L_WAVE_B] = Line::straight ({ 0.02f, 0.70f, 0.55f }, { 0.98f, 0.70f, 0.55f });
    l[L_FILT_A] = Line::straight ({ 0.2f, 0.5f, 0.5f }, { 0.8f, 0.5f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::helix ({ 0.5f, 0.5f, 0.5f }, 0.2f, 0.6f, 2.0f, 12);
    l[L_MOD_B]  = l[L_MOD_A];
    p.scan = 0.0f; p.scanAmt = 0.85f; p.scanA = 0.02f; p.scanD = 0.45f;
    p.filtType = 0; p.cutoff = 0.75f; p.reso = 0.2f; p.filtDepth = 0.0f;
    p.modScan = 0.5f; p.modPitch = 0.53f; p.modRate = 0.5f;
    p.ampA = 0.01f; p.ampD = 0.4f; p.ampS = 0.5f; p.ampR = 0.35f;
}

static void facRetinal (Line* l, Params& p)
{
    p.specimen = listVal (5, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.02f, 0.30f, 0.20f }, { 0.98f, 0.30f, 0.20f });
    l[L_WAVE_B] = Line::straight ({ 0.02f, 0.75f, 0.65f }, { 0.98f, 0.75f, 0.65f });
    l[L_FILT_A] = Line::straight ({ 0.5f, 0.2f, 0.5f }, { 0.5f, 0.8f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.3f, 0, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.scan = 0.25f; p.scanAmt = 0.5f;
    p.filtType = 1; p.cutoff = 0.6f; p.reso = 0.5f; p.filtDepth = 0.6f; p.filtMode = 1; p.filtRate = 0.3f;
    p.modScan = 0.7f; p.modRate = 0.15f;
    p.ampA = 0.2f; p.ampD = 0.5f; p.ampS = 0.7f; p.ampR = 0.5f;
}

static void facVentilator (Line* l, Params& p)
{
    p.specimen = listVal (6, NSPECIMENS);
    l[L_WAVE_A] = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.28f, 2, 10);
    l[L_WAVE_B] = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.12f, 0, 10);
    l[L_FILT_A] = Line::straight ({ 0.1f, 0.5f, 0.1f }, { 0.9f, 0.5f, 0.9f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::walk (7u, 8, true, 0.2f);
    l[L_MOD_B]  = Line::walk (11u, 8, true, 0.2f);
    p.scan = 0.0f; p.scanAmt = 0.5f;
    p.filtType = 0; p.cutoff = 0.5f; p.reso = 0.3f; p.filtDepth = 0.9f; p.filtMode = 1; p.filtRate = 0.12f;
    p.modScan = 0.8f; p.modPan = 0.75f; p.modRate = 0.1f;
    p.unison = listVal (3, 4); p.detune = 0.35f; p.spread = 0.9f;
    p.ampA = 0.5f; p.ampD = 0.6f; p.ampS = 0.8f; p.ampR = 0.7f;
}

static void facArrhythmia (Line* l, Params& p)
{
    p.specimen = listVal (8, NSPECIMENS);
    l[L_WAVE_A] = Line::walk (3u, 9, false, 0.22f);
    l[L_WAVE_B] = Line::walk (5u, 9, true, 0.22f);
    l[L_FILT_A] = Line::walk (13u, 6, false, 0.3f);
    l[L_FILT_B] = Line::walk (17u, 6, true, 0.3f);
    l[L_MOD_A]  = Line::walk (19u, 7, true, 0.25f);
    l[L_MOD_B]  = Line::walk (23u, 7, true, 0.25f);
    p.scan = 0.4f; p.scanAmt = 0.3f; p.scanA = 0.1f; p.scanD = 0.7f;
    p.filtType = 0; p.cutoff = 0.65f; p.reso = 0.4f; p.filtDepth = 0.7f; p.filtMode = 1; p.filtRate = 0.4f;
    p.modScan = 0.8f; p.modPitch = 0.52f; p.modPan = 0.8f; p.modRate = 0.45f;
    p.ampA = 0.02f; p.ampD = 0.5f; p.ampS = 0.65f; p.ampR = 0.45f;
}

//  the three that show the 260904.3 read off: grain, the window, the fold
static void facStaples (Line* l, Params& p)
{
    p.specimen = listVal (9, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.02f, 0.30f, 0.15f }, { 0.98f, 0.30f, 0.15f });
    l[L_WAVE_B] = Line::straight ({ 0.02f, 0.75f, 0.80f }, { 0.98f, 0.75f, 0.80f });
    l[L_FILT_A] = Line::straight ({ 0.1f, 0.5f, 0.2f }, { 0.9f, 0.5f, 0.2f });
    l[L_FILT_B] = Line::straight ({ 0.1f, 0.5f, 0.9f }, { 0.9f, 0.5f, 0.9f });
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.3f, 2, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.grain = 1.0f; p.contrast = 0.0f; p.fold = 0.0f;
    p.scan = 0.1f; p.scanAmt = 0.6f; p.scanA = 0.02f; p.scanD = 0.5f;
    p.filtType = 0; p.cutoff = 0.55f; p.reso = 0.4f; p.filtDepth = 0.7f; p.filtMode = 0; p.filtRate = 0.38f;
    p.modScan = 0.6f; p.modRate = 0.22f;
    p.ampA = 0.01f; p.ampD = 0.45f; p.ampS = 0.6f; p.ampR = 0.35f;
}

static void facEnamel (Line* l, Params& p)
{
    p.specimen = listVal (10, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.02f, 0.05f, 0.05f }, { 0.98f, 0.05f, 0.05f });
    l[L_WAVE_B] = Line::straight ({ 0.02f, 0.45f, 0.60f }, { 0.98f, 0.45f, 0.60f });
    l[L_FILT_A] = Line::straight ({ 0.1f, 0.9f, 0.5f }, { 0.9f, 0.1f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.25f, 1, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.grain = 0.7f; p.contrast = 0.35f; p.fold = 0.0f;
    p.scan = 0.0f; p.scanAmt = 0.8f; p.scanA = 0.05f; p.scanD = 0.6f;
    p.filtType = 0; p.cutoff = 0.5f; p.reso = 0.55f; p.filtDepth = 0.85f; p.filtMode = 0; p.filtRate = 0.3f;
    p.modScan = 0.55f; p.modPan = 0.6f; p.modRate = 0.3f;
    p.unison = listVal (1, 4); p.detune = 0.12f; p.spread = 0.5f;
    p.ampA = 0.02f; p.ampD = 0.5f; p.ampS = 0.55f; p.ampR = 0.4f;
}

static void facTendon (Line* l, Params& p)
{
    p.specimen = listVal (11, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.02f, 0.10f, 0.40f }, { 0.98f, 0.10f, 0.40f });
    l[L_WAVE_B] = Line::straight ({ 0.02f, 0.90f, 0.85f }, { 0.98f, 0.90f, 0.85f });
    l[L_FILT_A] = Line::straight ({ 0.2f, 0.5f, 0.5f }, { 0.8f, 0.5f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::helix ({ 0.5f, 0.5f, 0.5f }, 0.2f, 0.5f, 1.5f, 12);
    l[L_MOD_B]  = l[L_MOD_A];
    p.grain = 0.5f; p.contrast = 0.45f; p.fold = 0.8f;
    p.scan = 0.0f; p.scanAmt = 0.9f; p.scanA = 0.01f; p.scanD = 0.7f;
    p.filtType = 0; p.cutoff = 0.8f; p.reso = 0.15f; p.filtDepth = 0.3f; p.filtMode = 0; p.filtRate = 0.5f;
    p.modScan = 0.5f; p.modPan = 0.7f; p.modRate = 0.12f;
    p.unison = listVal (2, 4); p.detune = 0.18f; p.spread = 0.8f;
    p.ampA = 0.01f; p.ampD = 0.6f; p.ampS = 0.4f; p.ampR = 0.6f;
}

//  the bodies, played: each patch shows one of the 260905.1 controls off
static void facSkull (Line* l, Params& p)
{
    p.specimen = listVal (7, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.02f, 0.62f, 0.56f }, { 0.98f, 0.62f, 0.56f });   // through the orbits
    l[L_WAVE_B] = Line::straight ({ 0.02f, 0.40f, 0.14f }, { 0.98f, 0.40f, 0.14f });   // through the teeth
    l[L_FILT_A] = Line::straight ({ 0.5f, 0.2f, 0.5f }, { 0.5f, 0.9f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.55f }, 0.3f, 2, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.grain = 0.8f; p.contrast = 0.25f;
    p.scan = 0.0f; p.scanAmt = 0.8f; p.scanA = 0.15f; p.scanD = 0.65f;
    p.filtType = 0; p.cutoff = 0.6f; p.reso = 0.35f; p.filtDepth = 0.6f; p.filtMode = 1; p.filtRate = 0.25f;
    p.modScan = 0.55f; p.modPan = 0.65f; p.modRate = 0.2f;
    p.ampA = 0.02f; p.ampD = 0.5f; p.ampS = 0.6f; p.ampR = 0.45f;
}

static void facVertebra (Line* l, Params& p)
{
    p.specimen = listVal (12, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.05f, 0.60f, 0.50f }, { 0.95f, 0.60f, 0.50f });   // across a body
    l[L_WAVE_B] = Line::straight ({ 0.05f, 0.40f, 0.62f }, { 0.95f, 0.40f, 0.62f });   // across the canal
    l[L_FILT_A] = Line::straight ({ 0.5f, 0.1f, 0.5f }, { 0.5f, 0.9f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::helix ({ 0.5f, 0.5f, 0.5f }, 0.2f, 0.5f, 1.5f, 12);
    l[L_MOD_B]  = l[L_MOD_A];
    p.head2 = 0.6f; p.head2Ratio = 0.5f + std::log2 (1.5f) / 4.0f; p.head2Phase = 0.0f;   // a second head at 1.5x: a beating partial
    p.grain = 0.6f;
    p.scan = 0.0f; p.scanAmt = 0.7f; p.scanA = 0.05f; p.scanD = 0.6f;
    p.filtType = 0; p.cutoff = 0.65f; p.reso = 0.3f; p.filtDepth = 0.5f; p.filtMode = 0; p.filtRate = 0.4f;
    p.modScan = 0.6f; p.modRate = 0.18f;
    p.ampA = 0.01f; p.ampD = 0.55f; p.ampS = 0.5f; p.ampR = 0.5f;
}

static void facFemur (Line* l, Params& p)
{
    p.specimen = listVal (13, NSPECIMENS);
    //  a SPLIT line: two segments, one through the bone, one through the muscle — two edges a cycle
    Line a; a.n = 4; a.split = 2;
    a.p[0] = { 0.25f, 0.62f, 0.5f }; a.p[1] = { 0.55f, 0.62f, 0.5f };
    a.p[2] = { 0.40f, 0.30f, 0.5f }; a.p[3] = { 0.90f, 0.30f, 0.5f };
    l[L_WAVE_A] = a;
    Line b = a; b.p[0] = { 0.20f, 0.60f, 0.85f }; b.p[1] = { 0.60f, 0.60f, 0.85f };
    b.p[2] = { 0.30f, 0.25f, 0.85f }; b.p[3] = { 0.85f, 0.25f, 0.85f };
    l[L_WAVE_B] = b;
    l[L_FILT_A] = Line::straight ({ 0.1f, 0.5f, 0.5f }, { 0.9f, 0.5f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.25f, 1, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.grain = 0.5f; p.fold = 0.35f; p.contrast = 0.3f;
    p.scan = 0.0f; p.scanAmt = 0.75f; p.scanA = 0.02f; p.scanD = 0.55f;
    p.filtType = 0; p.cutoff = 0.55f; p.reso = 0.4f; p.filtDepth = 0.7f; p.filtMode = 0; p.filtRate = 0.35f;
    p.unison = listVal (2, 4); p.detune = 0.15f; p.spread = 0.7f; p.uniScan = 0.5f;
    p.modScan = 0.5f; p.modPan = 0.6f; p.modRate = 0.22f;
    p.ampA = 0.01f; p.ampD = 0.45f; p.ampS = 0.55f; p.ampR = 0.4f;
}

static void facJaw (Line* l, Params& p)
{
    p.specimen = listVal (14, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.05f, 0.30f, 0.44f }, { 0.95f, 0.30f, 0.44f });   // along the lower teeth
    l[L_WAVE_B] = Line::circle ({ 0.5f, 0.42f, 0.42f }, 0.32f, 2, 12);                 // round the arch
    l[L_FILT_A] = Line::straight ({ 0.5f, 0.1f, 0.5f }, { 0.5f, 0.9f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.3f, 2, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.grain = 1.0f; p.contrast = 0.2f;
    p.modContrast = 0.85f;                                                            // the MOD line narrows the window
    p.scan = 0.3f; p.scanAmt = 0.5f;
    p.filtType = 0; p.cutoff = 0.7f; p.reso = 0.3f; p.filtDepth = 0.4f; p.filtMode = 1; p.filtRate = 0.3f;
    p.modScan = 0.5f; p.modRate = 0.2f;
    p.ampA = 0.01f; p.ampD = 0.5f; p.ampS = 0.55f; p.ampR = 0.4f;
}

static const FactoryPatch FACTORY[] =
{
    { "ADMISSION",     facAdmission   },
    { "SINUS RHYTHM",  facSinusRhythm },
    { "CORTEX",        facCortex      },
    { "PULSE OX",      facPulseOx     },
    { "MARROW",        facMarrow      },
    { "NERVE",         facNerve       },
    { "RETINAL",       facRetinal     },
    { "VENTILATOR",    facVentilator  },
    { "ARRHYTHMIA",    facArrhythmia  },
    { "STAPLES",       facStaples     },
    { "ENAMEL",        facEnamel      },
    { "TENDON",        facTendon      },
    { "SKULL",         facSkull       },
    { "VERTEBRA",      facVertebra    },
    { "FEMUR",         facFemur       },
    { "JAW",           facJaw         },
};
int numFactory() { return (int) (sizeof (FACTORY) / sizeof (FACTORY[0])); }
const FactoryPatch& factory (int i) { const int n = numFactory(); return FACTORY[i < 0 ? 0 : (i >= n ? n - 1 : i)]; }

} // namespace bs
