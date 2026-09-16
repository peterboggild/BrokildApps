#pragma once

// LEGION — the shared per-frame analysis.
//
// ONE analysis serves every harmony voice. That is the whole reason the
// engine is arranged this way: the expensive half (FFT, true envelope,
// f0, peak picking) depends only on the INPUT, so four voices cost four
// cheap spectral re-draws, not four analyses.
//
// What a frame carries, and why:
//
//   mag/phase      the plain polar spectrum.
//   omega          the TRUE frequency of each bin in rad/sample, from the
//                  phase advance since the last frame. A pitch shift is a
//                  scaling of these, not of the bin numbers.
//   env            the spectral envelope — the VOCAL TRACT. Estimated with
//                  the true-envelope method (Roebel & Rodet): cepstrally
//                  smooth, take the max against the log spectrum, repeat.
//                  A plain cepstral lifter sits in the middle of the
//                  harmonics and drags the formants down with the pitch;
//                  the max-iteration makes it hug the peaks instead, which
//                  is what makes formant control independent at all.
//   cRe/cIm        the complex spectrum, with the sign of every odd bin
//                  flipped. That flip un-does the half-window time offset
//                  and makes each main lobe a SMOOTH complex function —
//                  without it, adjacent bins of one lobe alternate in sign
//                  and the sub-bin interpolation in legion_shifter would be
//                  interpolating across a zero crossing.
//   peaks/region   sinusoidal peaks and the bins each one owns, for the
//                  identity phase locking in legion_shifter.
//   transient      spectral flux said an attack landed: the shifter resets
//                  phase to the input's, which is what keeps a consonant a
//                  consonant instead of a smear.
//
// Plain C++17, no JUCE. prepare() allocates; analyse() does not.

#include <vector>

#include "legion_fft.h"

namespace legion
{

struct Frame
{
    int   N = 0;
    int   half = 0;              // N/2; arrays hold half+1 entries

    const float* mag   = nullptr;
    const float* env   = nullptr;   // linear magnitude envelope
    const float* cRe   = nullptr;   // complex spectrum, odd bins sign-flipped
    const float* cIm   = nullptr;   // (see above)

    const int*   peakBin   = nullptr;   // ascending
    const int*   peakLo    = nullptr;   // first and last bin each peak owns;
    const int*   peakHi    = nullptr;   // the regions tile [0, half] exactly
    const float* peakOmega = nullptr;
    int   peakCount = 0;

    bool  transient = false;
    float f0 = 0.0f;             // Hz, 0 = no periodicity found
    float voiced = 0.0f;         // 0..1 strength of the cepstral peak
};

class Analyser
{
public:
    void prepare (int maxN, const Fft* fft);   // message thread
    void reset();                              // audio thread, no alloc

    /*  audio thread. `windowed` holds N already-windowed samples.

        The true envelope is six pairs of transforms and about 40 % of the
        engine's cost, and re-estimating it only every other hop was tried:
        it saves 4 % of a core and costs 5 dB of shifted-tone purity, because
        a tract gain that alternates between fresh and stale is amplitude
        modulation at half the frame rate. Measured, rejected, recorded here
        so it is not tried again. */
    void analyse (const float* windowed, int N, int hop, double fs);

    const Frame& frame() const { return fr; }

private:
    void trueEnvelope (int N, int order, double fs);
    void findPeaks (int N);

    const Fft* fft = nullptr;
    int maxN = 0;

    Frame fr;

    std::vector<float> spec;        // 2*maxN interleaved
    std::vector<float> cep;         // 2*maxN interleaved
    std::vector<float> magB, phaseB, omegaB, envB, cReB, cImB;
    std::vector<float> logMag, cur, smoothed;
    std::vector<float> prevPhase, prevMag;
    std::vector<int>   peakBinB, peakLoB, peakHiB;
    std::vector<float> peakOmegaB;
    bool  havePrev = false;
};

} // namespace legion
