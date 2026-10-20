/*  MARMALADE - the oversampling filters, shared by both amps */
#pragma once

#include <array>
#include <cmath>

namespace mm
{

//==============================================================================
/*  Polyphase half-band resampler, 63-tap windowed sinc, as in Battlestar
    Overdrive. One gives 2x, two in cascade 4x. */
struct HalfBand
{
    static constexpr int TAPS = 63;
    static constexpr int PH   = 32;
    static constexpr int MASK = PH - 1;

    std::array<float, PH> ge {}, go {};
    std::array<float, PH> upLine {}, dnE {}, dnO {};
    int upW = 0, dnW = 0;

    void design()
    {
        constexpr double PI = 3.14159265358979323846;
        constexpr int C = TAPS / 2;
        std::array<double, TAPS> g {};
        double sum = 0.0;
        for (int n = 0; n < TAPS; ++n)
        {
            const double t = n - C;
            const double s = (t == 0.0) ? 0.5 : std::sin (PI * 0.5 * t) / (PI * t);
            const double w = 0.42 - 0.5 * std::cos (2.0 * PI * n / (TAPS - 1))
                                  + 0.08 * std::cos (4.0 * PI * n / (TAPS - 1));
            g[(size_t) n] = s * w;
            sum += g[(size_t) n];
        }
        for (auto& v : g) v /= sum;                 // unity at DC

        for (int k = 0; k < PH; ++k)
        {
            ge[(size_t) k] = (2 * k     < TAPS) ? (float) g[(size_t) (2 * k)]     : 0.0f;
            go[(size_t) k] = (2 * k + 1 < TAPS) ? (float) g[(size_t) (2 * k + 1)] : 0.0f;
        }
    }
    void clear() { upLine.fill (0.0f); dnE.fill (0.0f); dnO.fill (0.0f); upW = dnW = 0; }

    inline void up (float x, float& o0, float& o1)
    {
        upLine[(size_t) upW] = x;
        float s0 = 0.0f, s1 = 0.0f;
        for (int k = 0; k < PH; ++k)
        {
            const float v = upLine[(size_t) ((upW - k) & MASK)];
            s0 += ge[(size_t) k] * v;
            s1 += go[(size_t) k] * v;
        }
        o0 = 2.0f * s0;
        o1 = 2.0f * s1;
        upW = (upW + 1) & MASK;
    }

    inline float down (float a, float b)
    {
        dnE[(size_t) dnW] = a;
        dnO[(size_t) dnW] = b;
        float s = 0.0f;
        for (int k = 0; k < PH; ++k)
            s += ge[(size_t) k] * dnE[(size_t) ((dnW - k) & MASK)]
               + go[(size_t) k] * dnO[(size_t) ((dnW - k - 1) & MASK)];
        dnW = (dnW + 1) & MASK;
        return s;
    }
};

} // namespace mm
