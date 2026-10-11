#pragma once
// Magneto Transport - the playback head (stage 4). docs/physics/07-playback.md.
//
// A head reads each depth slab through that slab's own exact kernel: the
// Lorentzian of spacing loss averaged over the slab's depth,
//     h(x) = [ln(x^2 + bot^2) - ln(x^2 + top^2)] / (2 pi D),
// symmetric (zero-phase, as the physics is), with the gap (2.5 um) and the
// output sample's transit folded in. Near the head: taps on the slab's own grid.
// Beyond: geometric boxes carrying the kernel's exact mass, read as means from
// prefix sums - that 1/x^2 tail IS the thickness loss reaching into the bass.
// No JUCE in here.

#include <cstdint>
#include <vector>

namespace mt
{

constexpr double PB_SPACING_UM = 0.3;     // head to tape: the reference's d_p
constexpr double PB_GAP_UM     = 2.5;     // the reference's playback gap g_p
constexpr double SLAB_UM       = 3.5;     // one depth slab: the 14 um coating / 4
constexpr double COARSE_UM     = 4.0;     // prefix-sum grid of the kernel tails (8 surface cells)
constexpr int    PRE_RING      = 131072;  // coarse cells kept: 52 cm, more than any head ever reads
constexpr int    PRE_MASK      = PRE_RING - 1;

struct HeadKernel
{
    static constexpr int NPH = 64;          // phases per cell
    static constexpr int NB = 49;           // speed buckets, 6 % apart
    static constexpr double B0 = 4.6, BR = 1.06;
    static constexpr int MAXBOX = 20;

    int N[4] {};                             // near taps: cells n - N .. n + N + 1
    int ntap[4] {};                          // 2N + 2
    std::vector<float> taps[4];              // [bucket][phase][tap]
    // the tail. Per slab, exact boxes from the edge of its near region out to a
    // common edge; beyond that every slab's kernel is c_l / x^2, so one combined
    // track C = sum c_l M_l carries all four tails (asymptotic masses c_l beta).
    int nbox[4] {};
    double boxLo[4][MAXBOX] {}, boxHi[4][MAXBOX] {};   // um from the head, one side
    std::vector<float> boxMass[4];           // [bucket][box], one side
    double cSlab[4] {};                      // c_l = (bot^2 - top^2) / (2 pi D)
    int nboxC = 0;
    double boxLoC[MAXBOX] {}, boxHiC[MAXBOX] {}, betaC[MAXBOX] {};
    double fsInt = 0.0;

    const float* tapRow (int l, int b, int ph) const { return taps[l].data() + ((size_t) b * NPH + ph) * (size_t) ntap[l]; }
    const float* masses (int l, int b) const { return boxMass[l].data() + (size_t) b * MAXBOX; }
    static int bucket (double vCms);
    static double bucketSpeed (int b);
    /*  Built once per internal sample rate and shared by every instance. */
    static const HeadKernel& get (double fsInt);
};

/*  Effective extra head spacing that makes four uniform slabs read like the
    reference's sixteen depths (generated: stock/pb_456.h), at a speed. */
double slabSpacingCorrectionUm (double vCms);

/*  The slab's playback response at wavenumber k (1/um), spacing and gap
    included, transit not: for the alignment and the record equaliser. */
double slabResponse (int slab, double kPerUm, double vCms);

} // namespace mt
