#pragma once

// LEGION — one harmony voice's spectral re-draw.
//
// The whole plugin is this one line:
//
//      MAG_out(k) = WHITE(k / pitch) * ENV(k / formant)
//
// The source (glottis, breath, the grit in a scream) is read at one rate,
// the vocal tract at another, and they are multiplied back together. Set
// both ratios equal and you have plain resampling — the chipmunk. Set the
// formant ratio to 1 and the voice moves while the body stays, which is
// what makes a shifted voice still sound like a body that could have made
// it. Neither knob can affect the other: at pitch=1, formant=1 the product
// is the untouched spectrum, exactly, and the bench proves it.
//
// The re-draw MOVES each spectral peak, region and all, to round(pitch*bin)
// and leaves its lobe shape alone. Resampling the spectrum instead would
// widen every lobe by the pitch ratio, and a widened lobe re-synthesises as
// an amplitude-modulated partial — an audible subharmonic. See the comment
// on step 3 in the .cpp; the bench measured it.
//
// Phase is the other half of "natural". Bins are not rotated independently
// (that is the phase-vocaded chorus-y smear everyone recognises): each
// spectral PEAK carries an accumulator advanced by its own true frequency
// times the pitch ratio, and every bin the peak owns is rotated with it,
// keeping its original phase relationship — Laroche & Dolson identity
// locking. On a transient the accumulators are dropped and the input's own
// phase is used, so a consonant stays a consonant.

#include <vector>

#include "legion_analysis.h"

namespace legion
{

class VoiceShifter
{
public:
    void prepare (int maxN);    // message thread
    void reset();               // audio thread, no alloc

    // audio thread. Writes N interleaved complex pairs into outSpec, ready
    // for the inverse transform. pitch and formant are linear frequency
    // ratios (2.0 = an octave up).
    void render (const Frame& f, int hop, float pitch, float formant, float* outSpec);

private:
    std::vector<int>   trkBin;      // previous frame's peak bins (ascending)
    std::vector<float> trkDelta;    // and each one's accumulated phase EXCESS
                                    // over the input — see the .cpp
    int trkCount = 0;

    std::vector<float> curDelta;
    bool live = false;
};

} // namespace legion
