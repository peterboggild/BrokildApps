#pragma once

// RITE OF PASSAGE — ITU-R BS.1770 K-weighted loudness.
//
// §8's level contract is stated in LUFS, so the plugin and the bench both need
// a real loudness meter rather than an RMS. Two biquads (a high shelf and an
// RLB high-pass) derived at the actual sample rate, then mean square.
//
// The plugin uses it for the measured makeups; the bench uses it to hold every
// effect to its declared class. Same code, so the thing being measured and the
// thing being compensated cannot disagree.

namespace rop
{

/*  The K-weighting curve's power response at one frequency, from the same
    coefficients the meter uses. Exposed because a compensation that is
    computed in one domain and measured in another will always leave a
    residue: CLIMB's resonance makeup integrates its own response against
    THIS, so the thing being corrected and the thing being measured are the
    same curve. */
double kWeightMagSq (double hz, double fs);

class KWeight
{
public:
    void prepare (double sampleRate);
    void reset();

    // one sample per channel in, K-weighted samples out (for squaring)
    void process (float l, float r, float& outL, float& outR);

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;
        inline double tick (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() { z1 = z2 = 0; }
    };

    Biquad shelfL, shelfR, hpL, hpR;
};

// Short-term loudness over a sliding window, in LUFS.
class LoudnessMeter
{
public:
    void prepare (double sampleRate, double windowSeconds = 0.4);
    void reset();
    void push (float l, float r);

    // -inf when the window holds silence
    double lufs() const;
    double meanSquare() const { return acc; }

private:
    KWeight k;
    double acc = 0.0, a = 0.0;
};

} // namespace rop
