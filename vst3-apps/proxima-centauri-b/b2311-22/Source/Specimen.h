#pragma once

/*  PROXIMS CENTAURI B — ARTEFACT B2311.22 · the specimen system
    ==========================================================================

    A specimen is a GRAPH, and its sound is the graph's exact vibrational
    spectrum. Every earthly resonator is a low-dimensional shape, and the
    harmonic series — the whole basis of human pitch and timbre — is a
    geometric accident of living in 3D with strings and tubes (Weyl's law:
    mode counts follow dimension). A graph is a resonator with no dimension
    at all; its Laplacian eigenvalues are an overtone series no buildable
    object produces.

    The same eigendecomposition supplies EVERYTHING else, which is the whole
    design (ARTEFACT-B2311-DESIGN.md):

      * eigenvalues        -> partial frequency ratios (via sqrt, membrane law)
      * eigenvectors       -> per-partial amplitude at any excitation node,
                              per-partial L/R weights (two "ear" nodes),
                              AND the artefact's spatial embedding: the first
                              four non-trivial eigenvectors ARE its 4D body
                              coordinates. Body-space and sound-space are the
                              same space — that is why playing it moves it.
      * eigenvector overlap
        along real edges   -> the mode-coupling matrix: energy migrates
                              between partials along the specimen's own
                              anatomy. Timbre IS the shape.

    Alienness is not asserted; it is ENFORCED here: a specimen whose ratio
    set sits within a measured margin of ANY harmonic series is regenerated
    (salted reseed, deterministic) until it passes. The margin is exported so
    the bench can print it.

    Plain C++17, no JUCE — the bench compiles this file directly.
*/

#include <cstdint>
#include <array>

namespace ab
{

constexpr int kCatalog     = 300;   // specimens, deterministic forever
constexpr int kMaxNodes    = 72;    // graph order cap
constexpr int kMinNodes    = 34;
constexpr int kMaxModes    = 71;    // kMaxNodes - 1 (zero mode dropped)
constexpr int kMaxEdges    = 4 * kMaxNodes;
constexpr int kCouplePer   = 4;     // strongest couplings kept per mode
constexpr int kMaxPath     = kMaxNodes;

//  deterministic RNG (splitmix64) — identical on every platform, forever
struct Rng
{
    uint64_t s;
    explicit Rng (uint64_t seed) : s (seed) {}
    uint64_t next()
    {
        s += 0x9E3779B97f4A7C15ull;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float uni()            { return (float) ((next() >> 11) * (1.0 / 9007199254740992.0)); }
    int   irange (int n)   { return n > 0 ? (int) (next() % (uint64_t) n) : 0; }
};

/*  Everything the engine and the panel need, computed once per catalog
    number on the message thread and handed over whole. POD-ish and fixed
    size, so publication is a pointer swap.  */
struct Specimen
{
    int   catalog = -1;
    int   family  = 0;              // topology family, 0..5
    int   nNodes  = 0;
    int   nModes  = 0;              // nNodes - 1, capped at kMaxModes
    int   nEdges  = 0;

    //  the graph itself (the panel draws it; the coupling walks it)
    std::array<int,   kMaxEdges> edgeA {}, edgeB {};
    std::array<float, kMaxEdges> edgeW {};

    //  the body: 4D spectral embedding, per node, each axis normalised to
    //  roughly [-1, 1]. Axis 3 (w) is the section axis — the fourth
    //  dimension the screen can never show directly.
    std::array<float, kMaxNodes> px {}, py {}, pz {}, pw {};

    //  the spectrum: ratios relative to the lowest mode (r[0] == 1), and the
    //  same set snapped to a slow commensurate grid for REVIVAL
    std::array<float, kMaxModes> ratio {};       // dispersive (raw) ratios
    std::array<float, kMaxModes> ratioSnap {};   // commensurate ratios
    float revivalSeconds = 2.0f;                 // exact recurrence at full REVIVAL

    //  per-mode data derived from the eigenvectors
    std::array<float, kMaxModes> ampL {}, ampR {};   // ear-node magnitudes, normalised
    std::array<float, kMaxModes> phaseSkew {};       // small L/R phase offset (radians)
    std::array<float, kMaxModes> decayBias {};       // localisation -> ring character
    //  mode energy share per node is needed for excitation and for the
    //  section weighting; stored as the full eigenvector table (mode-major)
    std::array<float, (size_t) kMaxModes * kMaxNodes> vec {};   // vec[m*nNodes+i]

    //  mode coupling: for each mode, the strongest partners along real edges
    std::array<int,   (size_t) kMaxModes * kCouplePer> coupleTo {};
    std::array<float, (size_t) kMaxModes * kCouplePer> coupleW {};

    //  excitation path: hub -> most remote node, for the DEPTH organ
    std::array<int, kMaxPath> path {};
    int pathLen = 0;

    //  the two ear nodes (extremes of the long axis)
    int earL = 0, earR = 0;

    //  measured non-harmonicity margin (cents RMS to the best harmonic fit).
    //  The generator retries salted seeds until this clears kHarmonicFloor.
    float harmonicMarginCents = 0.0f;
    int   saltUsed = 0;

    //  sidereal ladder: sorted unique ratios folded to one “octave-like”
    //  span of the specimen’s own making (its lowest recurring interval)
    std::array<float, kMaxModes> ladder {};
    int ladderLen = 0;

    float eigenResidual = 0.0f;    // max |A v - lambda v| over checked modes (bench)

    /*  the generator's own constants, kept so a RE-SOLVE of a changed graph
        reproduces the same register and spread: an accent alters anatomy,
        never the creature's scale */
    float genGamma   = 1.0f;       // spectrum stretch exponent
    float genWantTop = 12.0f;      // where the top mode should land

    /*  the specimen's natural speaking register. Fixed per catalog number,
        independent of any note you play — without this, two bodies would
        always share a ladder and "mutual intelligibility" would be a
        tautology instead of a measurable property of the pair. */
    float voiceHz = 96.0f;

    //  how far this body has been remodelled by listening (0 = catalog pure)
    float accentDepth = 0.0f;
};

constexpr float kHarmonicFloor = 20.0f;   // cents RMS; below this = too earthly, reseed

//  Generate catalog entry `num` (0..kCatalog-1). Deterministic: the same
//  number is the same being on any machine, forever.
void generateSpecimen (int num, Specimen& out);

/*  Re-solve a specimen whose edge weights have been altered (plasticity).
    The graph is taken from the specimen itself; everything downstream —
    eigenmodes, ratios, the 4D body, coupling, path, ladder — is recomputed.
    The generator constants and voiceHz are preserved, so the creature keeps
    its scale and register while its anatomy, and therefore its voice,
    genuinely changes. */
void resolveSpecimen (Specimen& io);

/*  Mutual intelligibility of two bodies, in [0,1]: how much of one's
    ladder the other owns a mode for, at their own speaking registers.
    Symmetric, cheap, and the honest basis for "kin converse". */
float spectralKinship (const Specimen& a, const Specimen& b);

//  Field map: a deterministic 2D placement of the whole catalog by real
//  spectral similarity (fixed random projection of a spectral feature
//  vector — cheap, honest, and stable forever).
void fieldPosition (const Specimen& s, float& fx, float& fy);

} // namespace ab
