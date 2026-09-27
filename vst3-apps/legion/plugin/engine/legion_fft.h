#pragma once

// LEGION — in-place iterative radix-2 complex FFT, interleaved re/im.
//
// One twiddle table sized for the LARGEST transform the engine will ever
// run; every smaller power-of-two size reads the same table with a stride.
// That is what lets the window size change on the audio thread without a
// single allocation (the DETAIL switch, see legion_harmonizer.h).
//
// Plain C++17, no JUCE, no allocation outside prepare().

#include <vector>

namespace legion
{

class Fft
{
public:
    // message thread; maxN must be a power of two
    void prepare (int maxN);

    // audio thread: n must be a power of two <= maxN.
    // reim holds n interleaved (re, im) pairs. inverse() scales by 1/n, so
    // inverse(forward(x)) == x.
    void forward (float* reim, int n) const { run (reim, n, false); }
    void inverse (float* reim, int n) const { run (reim, n, true); }

    int maxSize() const { return maxN_; }

private:
    void run (float* reim, int n, bool inverse) const;

    int maxN_ = 0;
    std::vector<float> cosT, sinT;   // maxN/2 entries of cos/sin(2*pi*i/maxN)
};

} // namespace legion
