#pragma once

/*  ARTEFACT B2311.67 — the four-dimensional body.
    ---------------------------------------------------------------------------
    Everything the instrument is made of comes out of one object: the cubic
    lattice Z^4, cut by a plane and projected.

        R^4  =  E_par (the 2 dimensions we can see)  +  E_perp (the 2 we cannot)

    A lattice point n is part of the artefact when its shadow in E_perp falls
    inside a window W — the octagon that the unit hypercube casts there. The
    survivors, drawn in E_par, are the Ammann–Beenker tiling: squares and 45°
    rhombi, eightfold, ordered, never repeating.

    Translating the window (n_perp - gamma in W) is exactly sliding the cut
    plane through the fourth dimension. That is the mouse wheel.

    Basis, with the eightfold rotation acting as k -> k+1:

        e_k   = ( cos(k*45°),  sin(k*45°) )      in E_par
        f_k   = ( cos(3k*45°), sin(3k*45°) )     in E_perp

    Writing  A = n0,  B = n1-n3,  M = n1+n3,  C = n2   (B, M same parity):

        x_par = A + B/sqrt2      x_perp = A - B/sqrt2
        y_par = M/sqrt2 + C      y_perp = M/sqrt2 - C

    — the two planes are Galois conjugates of one another, sqrt2 -> -sqrt2.
    That single fact is the whole instrument: the direct-space chain and the
    reciprocal-space partials are conjugate readings of the same integers.
*/

#include <vector>
#include <cstdint>
#include <cmath>

namespace ab
{

//==============================================================================
constexpr double SQRT2  = 1.41421356237309504880;
constexpr double SILVER = 2.41421356237309504880;   // 1 + sqrt2, the inflation
constexpr double APO    = 1.20710678118654752440;   // (1+sqrt2)/2, window apothem
constexpr double PI     = 3.14159265358979323846;

// Pell numbers: this tiling's own sequence, as Fibonacci is the pentagonal one.
constexpr int PELL[8] = { 5, 12, 29, 70, 169, 408, 985, 2378 };

//==============================================================================
/*  The acceptance window: a regular octagon, |p . u_j| <= APO for the four
    axis directions — held here as eight half-planes so that ARRESTS can bulge
    it asymmetrically. `soft` is the width of the rim over which a site fades
    out of existence rather than blinking: a bubble crossing a plane shrinks to
    a point, it does not disappear. */
struct Window
{
    double bulge[8] { 0,0,0,0,0,0,0,0 };   // outward push per half-plane
    double scale = 1.0;                    // whole-window size (inflation levels)
    double soft  = 0.16;

    static inline void dir (int j, double& ux, double& uy)
    {
        const double a = (double) j * (PI * 0.25);
        ux = std::cos (a); uy = std::sin (a);
    }

    // 1 = solidly inside, 0 = outside, in between = leaving/arriving
    inline double occupancy (double px, double py) const
    {
        double m = 1.0;
        for (int j = 0; j < 8; ++j)
        {
            double ux, uy; dir (j, ux, uy);
            const double lim = APO * scale + bulge[j];
            const double slack = lim - (px * ux + py * uy);
            if (slack <= 0.0) return 0.0;
            const double t = slack / soft;
            if (t < m) m = t;
        }
        return m < 1.0 ? m * m * (3.0 - 2.0 * m) : 1.0;   // smoothstep
    }

    inline double limit (int j) const { return APO * scale + bulge[j]; }
};

//==============================================================================
/*  A HABIT — crystal habit, the characteristic form of a specimen.

    Not a preset in the human sense. The mass of a site is a function of where
    that site stands in the fourth dimension, so a habit is a scalar field over
    the acceptance window: literally a colouring of the dimension we cannot see.
    The same field colours the tiles on screen, so the picture *is* the patch. */
struct Habit
{
    int   index = 0;

    // the field over E_perp: a sum of plane waves with eightfold-related
    // wavevectors, optionally pushed through a threshold to give a two- or
    // three-letter alphabet (the classical quasiperiodic Hamiltonians)
    int    waves = 3;
    double kmag[4]  { 2.0, 3.2, 5.1, 1.3 };
    int    kdir[4]  { 0, 2, 5, 3 };
    double kamp[4]  { 1.0, 0.6, 0.35, 0.2 };
    double kphi[4]  { 0.0, 1.1, 2.3, 0.7 };
    double radial   = 0.0;      // + weight on |p|^2, a bullseye field
    double crisp    = 0.0;      // 0 smooth ... 1 hard threshold (letters)
    int    levels   = 0;        // 0 = continuous, >=2 = quantised alphabet

    // the material
    double contrast = 3.0;      // mass ratio heavy:light
    double bondExp  = 1.6;      // stiffness ~ length^-bondExp
    double damp     = 0.0016;
    double dampTilt = 0.25;     // Kelvin–Voigt: how much more the top is damped
    double nonlin   = 0.10;

    // how it is read
    int    extent   = 3;        // index into PELL
    int    cutIndex = 0;        // 0..7, the cut line's bearing in E_par
    double blend    = 0.45;     // 0 = all FILAMENT, 1 = all STAR
    double starWid  = 1.30;     // width of the conjugate window (partial count)
    double starTilt = 0.55;     // how fast intensity falls with |conjugate|
    double strike   = 0.35;     // 0 soft/broad ... 1 hard/point
    double drive    = 0.0;      // continuous excitation under the gate

    // how it looks (structural colour is physical; these set the film)
    double filmBase = 420.0;    // nm at field 0
    double filmSpan = 520.0;    // nm added at field 1
    double glint    = 0.5;

    double eval (double px, double py) const;      // the field, 0..1
};

Habit habitOf (int index);
int    habitCount();

/*  A per-specimen level trim, so that no patch lives inside the limiter.
    MEASURED by rendering every habit and reading its pre-limiter peak -- see
    test/gaincal.cpp, which regenerates the table. It only ever reduces, never
    boosts, so a quiet specimen keeps its quiet. Regenerate it after any change
    that moves the level, and the bench will tell you if you forgot. */
float  habitTrim (int index);

//==============================================================================
/*  A site of the artefact: one accepted lattice point. */
struct Site
{
    float  x = 0, y = 0;        // E_par  — where it is
    float  px = 0, py = 0;      // E_perp — where it is in the fourth dimension
    float  occ = 0;             // 1 present, 0 gone, between = at the rim
    float  field = 0;           // the habit's field at this site
    int    n[4] { 0,0,0,0 };
    int    level = 0;           // 0 = the tiling, 1 = its own inflation, 2 = ...
};

struct Edge  { int a = 0, b = 0; };
struct Tile  { int v[4] { 0,0,0,0 }; int kind = 0; };   // kind 0 = rhombus, 1 = square

struct Patch
{
    std::vector<Site> sites;
    std::vector<Edge> edges;
    std::vector<Tile> tiles;
};

/*  Build what is inside the aperture: the tiling out to `radius` unit edges,
    plus (levels>0) its own inflations, which are the same construction with
    the window shrunk by the silver ratio — the artefact contains itself. */
void buildPatch (Patch& out, const Window& w, const Habit& h,
                 double gx, double gy, double radius, int levels);

//==============================================================================
/*  THE CUT — the sounding chain.

    Draw a line across the tiling; the vertices it passes through, in order,
    are a quasiperiodic chain. Its spacings take two values in the ratio
    1+sqrt2, in the silver-mean order, and its masses come from the field. */
struct ChainSite
{
    double s = 0;               // position along the cut
    double px = 0, py = 0;      // its shadow in the fourth dimension
    double occ = 0;             // 1 present ... 0 gone
    double field = 0;
    double x = 0, y = 0;        // E_par, for drawing
};

/*  `strain` is OBLIQUITY, and it is a LINEAR PHASON STRAIN.

    The cut is a plane through four dimensions, and until now it lay flat in
    the two we can see. Lean it, and the window it carries slides through the
    unseen plane as you travel along it -- the acceptance at the far end of the
    filament is not the acceptance at the near end. That is the slope of the
    cut against the lattice, and it is the classical way a quasicrystal is
    turned into a crystal: at the strains where the leaned plane becomes
    commensurate with the lattice the structure closes into a RATIONAL
    APPROXIMANT, periodic, with an ordinary harmonic spectrum. Between any two
    such strains lies another, so the knob climbs a devil's staircase.

    Note what does NOT move: a site's position along the chain. Only whether it
    is there at all. An earlier attempt tilted the positions instead and the
    measurements condemned it -- one bearing went completely dead, the mean
    spacing jumped at the first step and then sat flat, and the smallest gap
    fell to 0.0186, which at bondExp 1.6 is a bond eight hundred times stiffer
    than its neighbours. A strain has none of those problems because it moves
    the window, not the body. */
void buildChain (std::vector<ChainSite>& out, const Window& w, const Habit& h,
                 double gx, double gy, double bearingRad, double offset,
                 int wantSites, double halfWidth, double strain = 0.0);

//==============================================================================
/*  THE STAR — reciprocal space.

    The diffraction of that same chain: peaks at f0 * |p + q*sqrt2|, with
    intensity set by the Fourier transform of the window evaluated at the
    Galois conjugate p - q*sqrt2. A partial is loud when its shadow in the
    fourth dimension is small. */
struct Peak
{
    double lam  = 0;            // p + q*sqrt2   — the frequency ratio
    double conj = 0;            // p - q*sqrt2   — its shadow, and its drift rate
    double amp  = 0;
};

/*  The diffraction of the cut. Given the chain, the intensities are the
    structure factor of that actual chain — the transform of the DECORATED
    lattice — rather than the window's envelope, which is the same for every
    specimen and made every held note fade into the same background. */
void buildStar (std::vector<Peak>& out, const Habit& h, int wantPeaks,
                const std::vector<ChainSite>* chain = nullptr, double contrast = 3.0);

} // namespace ab
