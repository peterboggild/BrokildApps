/*  MARMALADE 50 - the springs, as a model

    Everything around the tank is circuit: the ECC81 driver, the reverb
    transformer, the tank's input coil, the recovery valve and the REVERB
    pot. This is the part between the tank's two coils - the springs - and
    it is the one part of the amp that is NOT a physical simulation. A
    measured impulse response of a real tank replaces it (LOAD TANK IR);
    until then this is the best model that is cheap enough to run in real
    time.

    What it models, after Valimaki, Parker and Abel, "Parametric spring
    reverberation effect", J. Audio Eng. Soc. 58 (2010):
      - THREE springs, each a loop: an input transducer band (the coils pass
        roughly 80 Hz - 4.5 kHz), a cascade of STRETCHED ALLPASSES, a bulk
        delay, a little damping and a feedback gain set from the decay time.
      - The stretched allpasses are what make it a spring and not a room. A
        helical spring is dispersive: low frequencies travel slower than high
        ones, so every echo arrives as a descending chirp - the "drip" - and
        the echoes smear into each other the way a spring's do. A plain delay
        loop would sound like a slapback, a plain diffuser like a plate.
      - The bulk delay wanders slowly (springs are not still), which is what
        stops three loops from ringing as three fixed combs.
      - A short sizzle path (plain allpasses, no stretch, fast decay) for the
        top end above the chirps.

    Numbers [est]: an Accutronics type-4 long tank as fitted to Orange's
    valve reverbs - three springs, echo periods 41/47/53 ms, decay 3.0 s at
    mid frequencies. The tank code on the user's amp is not known yet; the
    three periods and the decay are the parameters a measured tank would
    replace first.

    Sensitivity [est]: volts at the output coil per volt across the input
    coil is not on any sheet. It is set so that with the REVERB pot at noon
    the reverb arrives at the phase splitter about 8 dB under the dry signal
    in energy (design doc, "the springs"). That is a calibration to a target,
    and it is the one number in the amp that is - which is why it is a single
    named constant.

    Plain C++, no JUCE, so the bench measures it. process() runs in place on
    a block of drive volts and returns output volts.
*/
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mm
{

class SpringTank
{
public:
    static constexpr double kPi         = 3.14159265358979323846;
    static constexpr int    NSPRING     = 3;
    static constexpr int    NSTRETCH    = 64;     // stretched allpasses per spring
    static constexpr int    NSIZZLE     = 12;
    static constexpr double RT60        = 3.0;    // [est] seconds, mid band
    static constexpr double FC          = 4400.0; // [est] where the chirps stop
    static constexpr double STRETCH_A   = -0.62;  // allpass coefficient: <0 puts LOW frequencies late
    static constexpr double SENSITIVITY = 0.021;  // [est] output volts per input volt, see above

    /*  `advanceSamples`: the engine hands the tank its drive and takes the
        return one chunk later (Engine50::CHUNK), so the model shortens its
        own bulk delay by that much and the first echo still lands where the
        spring puts it. */
    void prepare (double sampleRate, int advanceSamples)
    {
        sr = sampleRate;
        K = std::max (1, (int) std::lround (sr / (2.0 * FC)));
        const double periods[NSPRING] = { 0.041, 0.047, 0.053 };
        const double wobble[NSPRING]  = { 0.00031, 0.00042, 0.00036 };   // [est] seconds of wander
        const double rates[NSPRING]   = { 0.37, 0.53, 0.29 };            // [est] Hz

        // the cascade's group delay at 1.5 kHz, so an echo's middle lands on its period
        const double w = 2.0 * kPi * 1500.0 / sr, a = STRETCH_A;
        const double gdOne = K * (1.0 - a * a) / (1.0 + 2.0 * a * std::cos (K * w) + a * a);
        const double gd = NSTRETCH * gdOne;
        const double w1 = 2.0 * kPi * 1000.0 / sr;
        const double gd1k = NSTRETCH * K * (1.0 - a * a) / (1.0 + 2.0 * a * std::cos (K * w1) + a * a);

        // one-pole damping in each loop: the tail darkens as it goes, like a
        // spring's. The feedback gain is divided by its loss at 1 kHz, so
        // RT60 is the mid-band decay it says
        damp = std::exp (-2.0 * kPi * 3200.0 / sr);
        const double wm = 2.0 * kPi * 1000.0 / sr;
        const double dampMid = (1.0 - damp) / std::sqrt (1.0 - 2.0 * damp * std::cos (wm) + damp * damp);

        /*  The advance is taken from the OUTPUT tap only: reading the line
            `advance` samples nearer its head gives the same response that
            much earlier, while the loop - read at the full delay - keeps
            the spring's own echo period. Shortening the loop delay instead
            would move every later echo too. */
        advance = std::max (0, advanceSamples);

        // the random part of the wander: about 1.5 Hz wide, scaled to roughly unit size
        wanderC = 2.0 * kPi * 1.5 / sr;
        wanderScale = 0.4 / std::sqrt (wanderC / 6.0);

        for (int s = 0; s < NSPRING; ++s)
        {
            auto& sp = springs[(size_t) s];
            sp.ap.assign ((size_t) NSTRETCH * (size_t) K, 0.0);
            sp.apIdx = 0;
            sp.depth = wobble[s] * sr;
            sp.bulk = std::max (4.0 + advance + 2.0 * sp.depth, periods[s] * sr - gd);
            sp.lineLen = 1;
            while (sp.lineLen < (int) (sp.bulk + sp.depth + 8)) sp.lineLen <<= 1;
            sp.line.assign ((size_t) sp.lineLen, 0.0);
            sp.w = 0;
            const double loop1k = (sp.bulk + gd1k) / sr;
            sp.g = std::min (0.98, std::pow (10.0, -3.0 * loop1k / RT60) / dampMid);
            sp.phase = 0.21 * s;
            sp.rate = rates[s] / sr;
            sp.lp = 0.0;
            sp.fb = 0.0;
            sp.noise = sp.noise2 = 0.0;
            sp.rng = 0x9E3779B9u * (uint32_t) (s + 1);
        }

        // the coils' band: 2nd-order high-pass at 80 Hz in, 4th-order low-pass at FC out
        hpIn.highpass (80.0 / sr, 0.707);
        lpOut1.lowpass (FC / sr, 0.541);
        lpOut2.lowpass (FC / sr, 1.307);

        sizzle.assign ((size_t) NSIZZLE, {});
        const int sizLens[NSIZZLE] = { 31, 37, 43, 53, 59, 67, 73, 83, 97, 103, 113, 127 };
        for (int i = 0; i < NSIZZLE; ++i)
        {
            sizzle[(size_t) i].buf.assign ((size_t) std::max (1, (int) std::lround (sizLens[i] * sr / 48000.0)), 0.0);
            sizzle[(size_t) i].i = 0;
        }
        sizHp.highpass (2500.0 / sr, 0.707);
    }

    void reset()
    {
        for (auto& sp : springs) { std::fill (sp.ap.begin(), sp.ap.end(), 0.0); std::fill (sp.line.begin(), sp.line.end(), 0.0); sp.lp = sp.fb = 0.0; }
        for (auto& z : sizzle) std::fill (z.buf.begin(), z.buf.end(), 0.0);
        hpIn.clear(); lpOut1.clear(); lpOut2.clear(); sizHp.clear();
    }

    void process (float* io, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const double x = hpIn.tick ((double) io[i]);
            double out = 0.0;
            for (auto& sp : springs) out += spring (sp, x);
            out *= 1.0 / std::sqrt ((double) NSPRING);

            // sizzle: the top end above the chirps, short and diffuse
            double z = sizHp.tick (x);
            for (auto& ap : sizzle)
            {
                const double d = ap.buf[(size_t) ap.i];
                const double y = d - 0.6 * z;
                ap.buf[(size_t) ap.i] = z + 0.6 * y;
                if (++ap.i >= (int) ap.buf.size()) ap.i = 0;
                z = y;
            }
            out += 0.12 * z;

            io[i] = (float) (SENSITIVITY * lpOut2.tick (lpOut1.tick (out)));
        }
    }

private:
    struct Spring
    {
        std::vector<double> ap, line;
        int apIdx = 0, lineLen = 1, w = 0;
        double bulk = 0, depth = 0, g = 0, phase = 0, rate = 0, lp = 0, fb = 0, noise = 0, noise2 = 0;
        uint32_t rng = 1;
    };

    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (double f, double q, bool hp)
        {
            const double w = 2.0 * kPi * f, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
            if (hp) { b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; }
            else    { b0 = (1 - c) / 2 / a0; b1 =  (1 - c) / a0; b2 = b0; }
            a1 = -2 * c / a0; a2 = (1 - al) / a0;
        }
        void lowpass (double f, double q)  { set (f, q, false); }
        void highpass (double f, double q) { set (f, q, true); }
        void clear() { z1 = z2 = 0; }
        double tick (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    struct AP { std::vector<double> buf; int i = 0; };

    // one spring: input + feedback -> stretched allpass cascade -> damping -> wandering delay
    double spring (Spring& sp, double x)
    {
        double v = x + sp.g * sp.fb;
        const double a = STRETCH_A;
        // stretched first-order allpass: y = a*v + s;  s' (K samples later) = v - a*y
        for (int k = 0; k < NSTRETCH; ++k)
        {
            double& s = sp.ap[(size_t) (k * K + sp.apIdx)];
            const double y = a * v + s;
            s = v - a * y;
            v = y;
        }
        if (++sp.apIdx >= K) sp.apIdx = 0;

        sp.lp = v + damp * (sp.lp - v);
        const double damped = sp.lp;

        // the wander: a slow sine plus a slower random walk, so three loops never lock
        sp.phase += sp.rate; if (sp.phase >= 1.0) sp.phase -= 1.0;
        sp.rng = sp.rng * 1664525u + 1013904223u;
        const double white = (double) (sp.rng >> 8) / 8388608.0 - 1.0;
        sp.noise  += (white - sp.noise) * wanderC;
        sp.noise2 += (sp.noise - sp.noise2) * wanderC;
        const double d = sp.bulk + sp.depth * (0.7 * std::sin (2.0 * kPi * sp.phase) + wanderScale * sp.noise2);

        sp.line[(size_t) sp.w] = damped;
        sp.fb = read (sp, d);                       // what goes round again
        const double out = read (sp, d - advance);  // what the output coil hears, a chunk early
        sp.w = (sp.w + 1) & (sp.lineLen - 1);
        return out;
    }

    // Hermite read: a linear one is a lowpass applied on every trip round the loop
    static double read (const Spring& sp, double d)
    {
        const double rp = (double) sp.w - d;
        const int i1 = (int) std::floor (rp);
        const double f = rp - i1;
        const int m = sp.lineLen - 1;
        const double y0 = sp.line[(size_t) ((i1 - 1) & m)], y1 = sp.line[(size_t) (i1 & m)];
        const double y2 = sp.line[(size_t) ((i1 + 1) & m)], y3 = sp.line[(size_t) ((i1 + 2) & m)];
        const double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    double sr = 48000.0, damp = 0.0;
    int K = 5, advance = 0;
    double wanderC = 0.0, wanderScale = 0.0;
    std::array<Spring, NSPRING> springs;
    std::vector<AP> sizzle;
    Biquad hpIn, lpOut1, lpOut2, sizHp;
};

} // namespace mm
