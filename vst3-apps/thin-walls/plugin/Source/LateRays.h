#pragma once
/*  THIN WALLS - the late field traced through the real geometry (ULTRA).

    The live engine's late field is statistical: one Eyring decay per room, a
    level from 16 pi / A, three networks coupled by coefficients. Offline there
    is time to do what that approximates. Rays leave the source, bounce round
    the actual rooms - the folded walls, the furniture as boxes that absorb and
    scatter, the rug, the curtain, the doors at their actual apertures - lose
    what each surface and the air take, and are counted where they cross a
    small sphere round the listener: energy per octave band, per arrival
    direction and per millisecond.

    What the engine already renders is NOT counted again: the direct sound, the
    specular image paths to the order it traced them, and the specular paths it
    sees through a doorway. Everything else - every diffuse bounce, every higher
    order, the other room's field coming round the door - is the rays'.

    Sound through a closed leaf or a party wall is a second stage: the energy
    the first stage throws at each such panel, as a function of time, times its
    transmission, re-radiated from the panel into the next room by a second set
    of rays. So a neighbour's tail really is that room's tail, heard through the
    wall.

    The energy response becomes a binaural impulse response by the standard
    route: per direction, noise split into the seven bands by a power-
    complementary filter tree, each band shaped by its own envelope, then the
    head's response for that direction.

    Plain C++, no JUCE.
*/

#include "Engine.h"
#include <complex>
#include <vector>

namespace tw
{

struct LateRayOptions
{
    int   rays = 16000;
    int   panelRays = 1500;       // per transmitting panel side, second stage
    int   imageOrder = 6;         // the order of the engine's own image paths (not counted here)
    float maxSeconds = 0;         // 0: from the rooms' decay
    uint32_t seed = 1;
};

struct LateEnergy
{
    static constexpr int NDIR = 18;       // 8 azimuths x 2 elevations, and straight up and down
    float binSec = 0.001f;
    int nbins = 0;
    std::vector<float> e;                 // [(band * NDIR + dir) * nbins + bin], energy per bin
    float& at (int b, int d, int t) { return e[((size_t) (b * NDIR + d)) * (size_t) nbins + (size_t) t]; }
    float  at (int b, int d, int t) const { return e[((size_t) (b * NDIR + d)) * (size_t) nbins + (size_t) t]; }
    // what happened, for the bench
    long long crossings = 0, excluded = 0, transmitted = 0;
    float receiverRadius = 0, receiverVolume = 0;
};

// how long the rooms of P can ring, seconds (what the IR must hold)
float lateSeconds (const Params& P);

// the late energy response of source s of P at the listener
// transmitted: when given, what came through leaves and walls goes there instead of into out
void traceLate (const Params& P, int s, const LateRayOptions& o, LateEnergy& out, LateEnergy* transmitted = nullptr);

// the head-relative direction of each bin (degrees), for the synthesis
void lateDirection (int d, float& azDeg, float& elDeg);

/*  A binaural impulse response from an energy response. headSpectra holds, per
    direction, FFT (hL + i hR) at size fftN (see makeHeadSpectra). Deterministic:
    the same energy response always gives the same response. */
void makeHeadSpectra (const Hrtf& head, float earSpan, int fftN, std::vector<std::complex<float>>& out);
void synthLateIr (const LateEnergy& E, const std::vector<std::complex<float>>& headSpectra, int fftN,
                  double fs, std::vector<float>& L, std::vector<float>& R);

namespace fftc
{
    // in place, n a power of two; the inverse is scaled by 1/n
    void fft (std::complex<float>* a, int n, bool inverse);
    int  nextPow2 (int n);
}

} // namespace tw
