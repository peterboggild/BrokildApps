#include "rop_loudness.h"

#include <cmath>

namespace rop
{

namespace
{
    //  BS.1770 stage 1: high shelf, +4 dB above ~1.7 kHz ("the head")
    constexpr double kShelfF0 = 1681.974450955533;
    constexpr double kShelfG  = 3.999843853973347;   // dB
    constexpr double kShelfQ  = 0.7071752369554196;
    //  stage 2: RLB high-pass
    constexpr double kHpF0 = 38.13547087602444;
    constexpr double kHpQ  = 0.5003270373238773;
}

void KWeight::prepare (double fs)
{
    //  Derived at the real sample rate rather than using the published 48 k
    //  coefficients: the spec's numbers are a 48 k special case and a plugin
    //  that measures itself wrongly at 96 k would compensate wrongly too.
    {
        const double K  = std::tan (M_PI * kShelfF0 / fs);
        const double Vh = std::pow (10.0, kShelfG / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / kShelfQ + K * K;
        Biquad b;
        b.b0 = (Vh + Vb * K / kShelfQ + K * K) / a0;
        b.b1 = 2.0 * (K * K - Vh) / a0;
        b.b2 = (Vh - Vb * K / kShelfQ + K * K) / a0;
        b.a1 = 2.0 * (K * K - 1.0) / a0;
        b.a2 = (1.0 - K / kShelfQ + K * K) / a0;
        shelfL = shelfR = b;
    }
    {
        const double K  = std::tan (M_PI * kHpF0 / fs);
        const double a0 = 1.0 + K / kHpQ + K * K;
        Biquad b;
        b.b0 = 1.0;
        b.b1 = -2.0;
        b.b2 = 1.0;
        b.a1 = 2.0 * (K * K - 1.0) / a0;
        b.a2 = (1.0 - K / kHpQ + K * K) / a0;
        //  normalise the numerator by a0 as well
        b.b0 /= a0; b.b1 /= a0; b.b2 /= a0;
        hpL = hpR = b;
    }
    reset();
}

void KWeight::reset()
{
    shelfL.clear(); shelfR.clear(); hpL.clear(); hpR.clear();
}

void KWeight::process (float l, float r, float& outL, float& outR)
{
    outL = (float) hpL.tick (shelfL.tick ((double) l));
    outR = (float) hpR.tick (shelfR.tick ((double) r));
}

// ---------------------------------------------------------------------------
double kWeightMagSq (double hz, double fs)
{
    if (hz <= 0.0 || hz >= fs * 0.5) return 0.0;
    const double w = 2.0 * M_PI * hz / fs;
    const double cw = std::cos (w), sw = std::sin (w);
    const double c2w = std::cos (2.0 * w), s2w = std::sin (2.0 * w);

    auto mag2 = [&] (double b0, double b1, double b2, double a1, double a2)
    {
        const double nr = b0 + b1 * cw + b2 * c2w, ni = -(b1 * sw + b2 * s2w);
        const double dr = 1.0 + a1 * cw + a2 * c2w, di = -(a1 * sw + a2 * s2w);
        return (nr * nr + ni * ni) / (dr * dr + di * di + 1e-30);
    };

    double out = 1.0;
    {
        const double K  = std::tan (M_PI * kShelfF0 / fs);
        const double Vh = std::pow (10.0, kShelfG / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / kShelfQ + K * K;
        out *= mag2 ((Vh + Vb * K / kShelfQ + K * K) / a0,
                     2.0 * (K * K - Vh) / a0,
                     (Vh - Vb * K / kShelfQ + K * K) / a0,
                     2.0 * (K * K - 1.0) / a0,
                     (1.0 - K / kShelfQ + K * K) / a0);
    }
    {
        const double K  = std::tan (M_PI * kHpF0 / fs);
        const double a0 = 1.0 + K / kHpQ + K * K;
        out *= mag2 (1.0 / a0, -2.0 / a0, 1.0 / a0,
                     2.0 * (K * K - 1.0) / a0,
                     (1.0 - K / kHpQ + K * K) / a0);
    }
    return out;
}

// ---------------------------------------------------------------------------
void LoudnessMeter::prepare (double fs, double windowSeconds)
{
    k.prepare (fs);
    a = 1.0 - std::exp (-1.0 / (windowSeconds * fs));
    reset();
}

void LoudnessMeter::reset() { k.reset(); acc = 0.0; }

void LoudnessMeter::push (float l, float r)
{
    float kl, kr;
    k.process (l, r, kl, kr);
    const double ms = (double) kl * kl + (double) kr * kr;   // both channels weight 1.0
    acc += a * (ms - acc);
}

double LoudnessMeter::lufs() const
{
    if (acc <= 1e-20) return -200.0;
    return -0.691 + 10.0 * std::log10 (acc);
}

} // namespace rop
