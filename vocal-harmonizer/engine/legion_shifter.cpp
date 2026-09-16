#include "legion_shifter.h"

#include <algorithm>
#include <cmath>

namespace legion
{

namespace
{
    constexpr int kMatchBins = 2;   // a held partial never moves further than
                                    // this between frames; anything further is
                                    // a new partial and starts from the input's
                                    // own phase
    inline float wrapPi (float x)
    {
        while (x >  (float) M_PI) x -= (float) (2.0 * M_PI);
        while (x < -(float) M_PI) x += (float) (2.0 * M_PI);
        return x;
    }
}

void VoiceShifter::prepare (int maxN)
{
    const size_t h1 = (size_t) (maxN / 2 + 1);
    trkBin.assign (h1, 0);
    trkDelta.assign (h1, 0.0f);
    curDelta.assign (h1, 0.0f);
    reset();
}

void VoiceShifter::reset()
{
    trkCount = 0;
    live = false;
}

void VoiceShifter::render (const Frame& f, int hop, float pitch, float formant, float* outSpec)
{
    const int N = f.N, half = f.half;
    if (N <= 0) return;

    // ---- 1. one phase accumulator per peak --------------------------------
    /*  What is accumulated is the EXCESS over the input's own phase, not the
        output phase itself. Both are the same mathematics — the excess grows
        by hop*(pitch-1)*omega where the absolute phase grows by
        hop*pitch*omega — but the excess is re-based on the input every
        frame, so a peak that wanders a bin between frames cannot leave a
        constant phase offset behind. With the absolute form the bench
        measured unity transparency at -11 dB instead of -80: the wander is
        real, and on a held vowel it happens constantly. At pitch 1 the
        excess is identically zero and the output IS the input. */
    int scan = 0;                                  // both lists ascend
    for (int i = 0; i < f.peakCount; ++i)
    {
        const int b = f.peakBin[i];
        float d = 0.0f;

        if (live && ! f.transient)
        {
            while (scan + 1 < trkCount && std::abs (trkBin[(size_t) (scan + 1)] - b)
                                       <= std::abs (trkBin[(size_t) scan] - b)) ++scan;

            if (trkCount > 0 && std::abs (trkBin[(size_t) scan] - b) <= kMatchBins)
                d = trkDelta[(size_t) scan] + (float) hop * (pitch - 1.0f) * f.peakOmega[i];
            //  else: a partial that was not there last frame starts in phase
            //  with the input, which is also what a transient reset does
        }
        curDelta[(size_t) i] = wrapPi (d);
    }

    // ---- 2. move the peaks ------------------------------------------------
    /*  Each peak, with the whole region of bins it owns, is MOVED to
        pitch * (its true frequency) and keeps its lobe shape. The obvious
        alternative — resample the spectrum, reading bin k/pitch for every
        output bin — widens every main lobe by the pitch ratio, and a widened
        lobe is not what the window makes of a sinusoid any more. Both were
        built and measured on the bench's shifted-tone test: translation's
        worst spur is -39 dB against resampling's -36 dB, and on the case
        they most disagree about (440 Hz up a fifth) it is -45 dB against
        -36 dB. Translation also costs less, because the output region is the
        same width as the input one.

        The move is by a FRACTIONAL number of bins. The whole-bin part is an
        index offset; the remainder is a linear interpolation across the
        complex spectrum, whose lobes the analyser has already made smooth
        for exactly this. Rounding instead leaves each lobe up to half a bin
        from the frequency its own phase accumulator is advancing at, and the
        two disagreeing is worth about 1 to 3 dB of spur on the same test.

        The tract is applied as ONE SCALAR PER PEAK — the envelope where the
        harmonic is going over the envelope where it came from — not bin by
        bin. That is not a shortcut, it is the only correct form: the
        source-filter model says a harmonic's AMPLITUDE changes and its shape
        stays the window's. Doing it bin by bin divides the lobe's tails by
        an envelope that has collapsed away from the peak, and the bench
        measures the result as a spur 14 dB ABOVE the signal on an isolated
        1 kHz tone. */
    for (int k = 0; k <= half; ++k) { outSpec[(size_t) (2 * k)] = 0.0f; outSpec[(size_t) (2 * k + 1)] = 0.0f; }

    const float binsPerRad = (float) N / (float) (2.0 * M_PI);
    const float invFormant = 1.0f / std::max (0.01f, formant);

    auto envAt = [&] (float b) -> float
    {
        if (b <= 0.0f) return f.env[0];
        if (b >= (float) half) return f.env[half];
        const int   j0 = (int) b;
        const float t  = b - (float) j0;
        return f.env[j0] + t * (f.env[std::min (j0 + 1, half)] - f.env[j0]);
    };

    for (int i = 0; i < f.peakCount; ++i)
    {
        const int kp = f.peakBin[i];

        //  the peak's true position in bins — the phase advance knows it to
        //  a fraction of a bin, the bin index only to the nearest one.
        //  Clamped to the peak's own neighbourhood so a nonsense frequency
        //  from a noise bin cannot fling a lobe across the spectrum.
        float bTrue = f.peakOmega[i] * binsPerRad;
        bTrue = std::max ((float) kp - 1.0f, std::min ((float) kp + 1.0f, bTrue));

        const float bOut  = pitch * bTrue;
        const float delta = bOut - bTrue;
        const int   shift = (int) std::lround (delta);
        const float frac  = delta - (float) shift;      // -0.5 .. +0.5

        //  the tract: where it is going over where it came from
        const float gain = envAt (bOut * invFormant) / std::max (envAt (bTrue), 1.0e-12f);

        const float rotRe = gain * std::cos (curDelta[(size_t) i]);
        const float rotIm = gain * std::sin (curDelta[(size_t) i]);

        const int lo = std::max (0,    f.peakLo[i] + shift);
        const int hi = std::min (half, f.peakHi[i] + shift);
        if (lo > hi) continue;

        for (int ko = lo; ko <= hi; ++ko)
        {
            const float src = (float) (ko - shift) - frac;
            const int   i0  = (int) std::floor (src);
            if (i0 < 0 || i0 > half) continue;
            const int   i1  = std::min (i0 + 1, half);
            const float t   = src - (float) i0;

            const float wr = f.cRe[i0] + t * (f.cRe[i1] - f.cRe[i0]);
            const float wi = f.cIm[i0] + t * (f.cIm[i1] - f.cIm[i0]);

            //  undo the analyser's sign flip on the way out
            const float g = (ko & 1) ? -1.0f : 1.0f;

            outSpec[(size_t) (2 * ko)]     += g * (wr * rotRe - wi * rotIm);
            outSpec[(size_t) (2 * ko + 1)] += g * (wr * rotIm + wi * rotRe);
        }
    }

    outSpec[1] = 0.0f;                                  // DC and Nyquist are
    outSpec[(size_t) (2 * half + 1)] = 0.0f;            // real, by definition

    for (int k = 1; k < half; ++k)         // Hermitian mirror
    {
        outSpec[(size_t) (2 * (N - k))]     =  outSpec[(size_t) (2 * k)];
        outSpec[(size_t) (2 * (N - k) + 1)] = -outSpec[(size_t) (2 * k + 1)];
    }

    // ---- 3. hand the tracks to the next frame -----------------------------
    for (int i = 0; i < f.peakCount; ++i)
    {
        trkBin[(size_t) i]   = f.peakBin[i];
        trkDelta[(size_t) i] = curDelta[(size_t) i];
    }
    trkCount = f.peakCount;
    live = true;
}

} // namespace legion
