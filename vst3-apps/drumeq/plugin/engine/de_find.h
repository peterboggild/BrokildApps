#pragma once

// DRUMEQ - FIND DRUM and the spectrum. Plain C++, no JUCE.
//
// FIND DRUM listens to a few hits of the drum and returns its fundamental:
// every hit is located (an energy onset), a window is taken just after the
// stick (the strike itself is broadband and says nothing about the pitch), the
// magnitude spectra of all the hits are averaged, and the strongest peak in
// the drum's own range is refined by a parabola through its log magnitude. A
// tom drops in pitch as it decays and every hit differs; averaging the hits is
// what makes the answer the drum's rather than one stroke's.

#include <vector>

namespace de
{
    //  in-place complex FFT, n a power of two
    void fft (std::vector<double>& re, std::vector<double>& im, bool inverse = false);

    struct FindResult
    {
        double hz = 0.0;          // 0 = nothing found
        int hits = 0;             // how many strokes were heard
        double confidence = 0.0;  // peak over the median of the search range, dB
    };

    FindResult findFundamental (const float* x, int n, double fs, double lo, double hi);

    //  THE LEVELS of the same capture, for setting the input trim and the gate.
    //  Every hit is located; a hit is the drum's OWN when its peak in the gate's
    //  key band is within 12 dB of the loudest - everything quieter is spill
    //  from the rest of the kit. Levels are read the way the engine reads them:
    //  the broadband peak (what the trim normalises) and the gate detector's
    //  envelope (key band-pass, 10 ms peak hold).
    struct LevelResult
    {
        int hits = 0;              // the drum's own strokes
        int spill = 0;             // other events heard
        double peakDb = -200.0;    // median own-stroke peak, broadband, dBFS
        double keyMinDb = -200.0;  // the quietest own stroke at the gate detector
        double spillDb = -200.0;   // the loudest spill at the gate detector (-200 = none)
        bool ok() const { return hits > 0; }
    };
    LevelResult measureLevels (const float* x, int n, double fs, double keyLo, double keyHi);

    //  magnitude spectrum in dB of the last `size` samples of a ring buffer
    //  (Hann window), `size` a power of two; out gets size/2 bins
    void spectrumDb (const float* ring, int ringSize, int writePos, int size, std::vector<float>& out);
}
