/*  B2311.67 — wind the cut past the fragment and there is nothing to hear.

    Peter, 2026-09-02: "have the slider go 'too far' so that if the mouse wheel
    is scrolled to far, there is no crosssection between the 2D plane and the
    object; i.e. no sound" — and then that the wheel should have no limit at
    all, with "something ... at much bigger values of traverse, as an easter
    egg, undocumented".

    Worth stating what this is testing, because the mathematics does not give
    it: a cut-and-project quasicrystal is infinite and dense, so an ideal
    tiling intersects the plane at every depth and no traverse could ever empty
    it. What ends is the SPECIMEN — a fragment out of a vault has faces. And
    B2311.67 came out of a vault FIELD of sixty-seven, each at the centre of
    its own void, so the field is in the fourth dimension too.

        ab67void
*/
#include "../Source/Engine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

static const int SR = 48000, BLK = 256;

//  peak of a played note with the cut at depth `tau`
static double peakAtTau (double tau)
{
    ax::Engine e;
    e.p.travel = 0.5f;                      // the slider centred
    e.prepare (SR, BLK);
    e.setTravelDelta (tau);                 // the wheel, which has no limit
    /*  service() is what turns the offset into the cut. The plug-in calls it
        every timer tick, so a wheel notch lands on the next one; a bench that
        forgets it reads the SAME peak at every depth, which is exactly what
        this one did first time and it looked like the feature was dead. */
    e.service();
    std::vector<float> L (BLK), R (BLK);
    for (int b = 0; b < 2 * SR / BLK; ++b) e.process (L.data(), R.data(), BLK);
    e.noteOn (52, 0.95f);
    double pk = 0;
    for (int b = 0; b < 4 * SR / BLK; ++b)
    {
        e.process (L.data(), R.data(), BLK);
        for (int i = 0; i < BLK; ++i) pk = std::max (pk, (double) std::fabs (L[(size_t) i]));
    }
    return pk;
}

int main()
{
    std::printf ("ARTEFACT B2311.67 — the fragment has faces, and it is not alone\n\n");
    int fails = 0;

    std::printf ("1. THE FRAGMENT, across the slider's own range\n");
    std::printf ("   %-10s %-10s %12s\n", "TRAVERSE", "tau", "peak");
    double mid = 0, ends = 0;
    for (float v : { 0.50f, 0.20f, 0.14f, 0.10f, 0.05f, 0.00f, 1.00f })
    {
        const double tau = ((double) v - 0.5) * 24.0;
        const double pk = peakAtTau (tau);
        std::printf ("   %-10.2f %-10.1f %12.6f%s\n", v, tau, pk,
                     (v == 0.0f || v == 1.0f) ? "   <- wound fully out" : "");
        if (v == 0.50f) mid = pk;
        if (v == 0.00f || v == 1.00f) ends = std::max (ends, pk);
    }
    if (!(mid > 0.01))  { std::printf ("   FAIL the object is not audible at the centre\n"); ++fails; }
    if (!(ends <= 1e-7)) { std::printf ("   FAIL it is still audible wound fully out\n"); ++fails; }

    std::printf ("\n2. THE VOID, and what is in it. The wheel has no limit, so this\n");
    std::printf ("   goes far past anything the slider can reach.\n");
    std::printf ("   %-10s %12s\n", "tau", "peak");
    double voidMax = 0, home2 = 0, far2 = 0;
    /*  64 and 70 are the NEIGHBOUR SHOULDERS, not void: its face is 4.2 wide,
        so 67 +- 4.2 is all body. Counting them as empty field is what this
        bench did first time, and it reported a failure that was its own
        arithmetic and not the engine's. */
    for (double tau : { 14.0, 24.0, 40.0, 55.0, 64.0, 67.0, 70.0, 80.0,
                        100.0, 120.0, 134.0, 150.0 })
    {
        const double pk = peakAtTau (tau);
        const char* tag = "";
        if (tau == 67.0 || tau == 134.0) tag = "   <- something is here";
        std::printf ("   %-10.1f %12.6f%s\n", tau, pk, tag);
        if (tau == 67.0) home2 = pk;
        else if (tau == 134.0) far2 = pk;
        else if (std::fabs (tau - 67.0) > 4.2 && std::fabs (tau - 134.0) > 2.8)
            voidMax = std::max (voidMax, pk);
    }
    std::printf ("\n   loudest point of the empty field %.6f\n", voidMax);
    std::printf ("   the neighbour at 67  %.6f\n", home2);
    std::printf ("   the neighbour at 134 %.6f\n", far2);

    if (!(voidMax <= 1e-7)) { std::printf ("   FAIL the void is not empty\n"); ++fails; }
    if (!(home2 > 0.005))   { std::printf ("   FAIL nothing at the first neighbour\n"); ++fails; }
    if (!(far2  > 0.002))   { std::printf ("   FAIL nothing at the second\n"); ++fails; }
    if (!(home2 < mid))     { std::printf ("   FAIL the neighbour is not quieter than home\n"); ++fails; }

    std::printf ("\n%s\n", fails == 0
        ? "ALL CLEAR — the object is there, the void is empty, and the field is real"
        : "SEE ABOVE");
    return fails ? 1 : 0;
}
