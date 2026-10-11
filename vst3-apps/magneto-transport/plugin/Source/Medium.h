#pragma once
// Magneto Transport - what the medium does at the heads (stage 6).
// docs/physics/09-stock.md.
//
// 1. The particle noise's colour per slab. A slab holds one value per cell
//    column, so the noise in it is coherent through the slab's depth; a real
//    coating's particles are independent at every depth, and the head hears the
//    shallow ones better, so the real coating carries more short-wavelength noise.
//    A 5-tap symmetric FIR along each slab's cells restores the continuous
//    medium's spectrum (and takes out the slabs' effective spacing, which is a
//    correction for recorded SIGNALS, not for noise).
// 2. The head shaper. A dropout or the surface's roughness lifts the tape from
//    the head; a field at height d + dd is the field at d through a Poisson
//    kernel, which along a moving tape is a Cauchy kernel in time, dd / v wide -
//    exact, not a filter chosen to sound like it. An azimuth error tilts the gap
//    across the track: each track reads a box of tape W tan(a) long, and the two
//    tracks are S tan(a) apart along the tape. Per head, per track, one FIR
//    applied to the head's output; the head reads ahead to pay its delay.
// No JUCE in here.

#include "Stock.h"
#include <cstdint>

namespace mt
{

/*  g[l] = { g0, g1, g2 }: n_c = g0 u_c + g1 (u_c-1 + u_c+1) + g2 (u_c-2 + u_c+2),
    G(0) = 1, so the long-wavelength noise is untouched. pbSpacingUm: the slabs'
    effective extra spacing at the speed (it reads noise too). */
void noiseColour (double pbSpacingUm, double (&g)[4][3]);

/*  The noise power of the continuous coating over the four slabs' at wavenumber k
    (1/um), with the slabs' spacing correction - what the colour has to supply. */
double noiseColourTarget (int slab, double kPerUm, double pbSpacingUm);

/*  A track's dropouts: up to two candidate events per centimetre of tape, from a
    hash of the centimetre; an event is present when its draw falls under the
    stock's rate, so more wear only ever adds events. */
struct DropoutCache
{
    static constexpr double SEG = 20000.0;   // surface cells: 1 cm
    int64_t seg = INT64_MIN;
    struct Ev { double x0, len, depth, hole; };
    Ev ev[6] {};
    int n = 0;
    double rate = -1.0, depthMean = 0.0, holeFrac = 0.0;
    /*  Extra spacing (um) and the coating left (0..1, a hole) at material x (surface cells). */
    void at (double x, uint64_t seed, double perM, double meanUm, double hole, double& dd, double& keep);
};

class HeadShaper
{
public:
    static constexpr int K = 16;              // spacing / azimuth kernel half length (samples)
    static constexpr int FD = 4;              // fractional delay: 8 taps
    static constexpr int DMAX = 6;            // largest shift between the tracks (samples, each way)
    static constexpr int R = K + FD + DMAX;   // the read-ahead this costs: 26 samples
    static constexpr int NT = 2 * R + 1;
    static constexpr int RING = 64;

    /*  Message thread: builds the shared tables (a fraction of a second, once). */
    static void prepareTables();
    void reset();
    /*  For track c: spacing tau (samples), azimuth box (samples), shift (samples,
        + = later), gain (the coating left under the head). */
    void set (int c, double tau, double box, double shift, double gain);
    /*  One sample in, one out, R samples behind. */
    void push (double inL, double inR, double& outL, double& outR);
    bool active() const { return ! identA[0] || ! identA[1] || ! identB[0] || ! identB[1]; }

private:
    static constexpr int D0 = FD + DMAX;   // stage B's nominal delay
    // rings written twice, so every window is contiguous
    // stage A: the spacing / azimuth-box kernel - symmetric, K each side, delay K
    float ringA[2][2 * RING] {};
    float tapA[2][2 * K + 1] {};
    int a0[2] { 0, 0 }, a1[2] { 0, 0 };    // its nonzero span
    bool identA[2] { true, true };
    // stage B: the shift between the tracks, whole samples and a windowed-sinc fraction
    float ringB[2][2 * RING] {};
    float rtapB[2][8] {};                  // reversed, against the contiguous window
    int offB[2] { D0, D0 };                // how far back the window starts
    bool identB[2] { true, true };
    int w = 0;
    double gn[2] { 1.0, 1.0 };
};

} // namespace mt
