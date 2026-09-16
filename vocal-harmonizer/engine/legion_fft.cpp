#include "legion_fft.h"

#include <cmath>

namespace legion
{

void Fft::prepare (int maxN)
{
    if (maxN < 2) maxN = 2;
    maxN_ = maxN;
    cosT.resize ((size_t) (maxN_ / 2));
    sinT.resize ((size_t) (maxN_ / 2));
    for (int i = 0; i < maxN_ / 2; ++i)
    {
        const double a = 2.0 * M_PI * (double) i / (double) maxN_;
        cosT[(size_t) i] = (float) std::cos (a);
        sinT[(size_t) i] = (float) std::sin (a);
    }
}

void Fft::run (float* reim, int n, bool inverse) const
{
    if (n < 2 || maxN_ < n) return;

    //  bit-reversal permutation, table-free (Knuth's counter)
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j)
        {
            std::swap (reim[2 * i],     reim[2 * j]);
            std::swap (reim[2 * i + 1], reim[2 * j + 1]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len >> 1;
        const int step = maxN_ / len;          // k*step < maxN/2, always in table
        for (int base = 0; base < n; base += len)
        {
            for (int k = 0; k < half; ++k)
            {
                const int t  = k * step;
                const float wr =  cosT[(size_t) t];
                const float wi = inverse ? sinT[(size_t) t] : -sinT[(size_t) t];

                float* a = reim + 2 * (base + k);
                float* b = reim + 2 * (base + k + half);

                const float br = b[0] * wr - b[1] * wi;
                const float bi = b[0] * wi + b[1] * wr;

                b[0] = a[0] - br;  b[1] = a[1] - bi;
                a[0] = a[0] + br;  a[1] = a[1] + bi;
            }
        }
    }

    if (inverse)
    {
        const float s = 1.0f / (float) n;
        for (int i = 0; i < 2 * n; ++i) reim[i] *= s;
    }
}

} // namespace legion
