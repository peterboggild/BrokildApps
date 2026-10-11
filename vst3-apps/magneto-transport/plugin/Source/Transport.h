#pragma once
// Magneto Transport - transport mechanics (stage 5). docs/physics/08-transport.md.
//
// The capstan motor (AC synchronous or DC servo) and every physical cause of wow
// and flutter: capstan and pinch-roller runout, bearing roughness, motor ripple,
// reel eccentricity and tension (as strain), and stick-slip at each head. The
// controls set causes; the modulation depth is what they do. No JUCE in here.

#include <cstdint>

namespace mt
{

struct TransportInput
{
    double vTargetCms = 19.0;      // what the motor is asked for
    int    motor = 0;              // 0 AC synchronous, 1 DC servo
    double cond = 0.15;            // TRANSPORT CONDITION, 0..1 (0 = an ideal transport)
    double capstan = 1.0, reels = 1.0, scrape = 1.0;   // the causes' balance, 0..2
    bool   reelMode = true;        // reels present (REEL) or a closed loop
    double supplyR = 0.1, takeupR = 0.1;               // reel radii, m
};

class Transport
{
public:
    void prepare (double fsInt);
    void reset (double vCms);

    /*  One internal sample: the tape's speed at the heads (cm/s). */
    double tick (const TransportInput& in);

    /*  The motor's own speed, without wow and flutter (cm/s): what the rest of the
        machine (record tables, equalisers) should follow, not every flutter cycle. */
    double motorSpeedCms() const { return w; }

    /*  Stick-slip displacement of the tape at a head (um): 0..2 the playbacks,
        3 the record head. */
    double scrapeUm (int head) const { return xs[head]; }

private:
    struct Biquad2 { double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
                     double tick (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    static void bandpass (Biquad2& f, double fs, double f0, double q);
    static void lowpass (Biquad2& f, double fs, double f0, double q);
    static double unitGain (Biquad2 f, int n);   // 1 / rms of the response to unit white noise
    double gauss();
    double uni();

    double fs = 96000.0, dt = 1.0 / 96000.0;
    double w = 19.0, delta = 0.0, fieldV = 19.0;
    double angCap = 0.0, angRoll = 0.0, angSup = 0.0, angTak = 0.0, angMains = 0.0;
    uint64_t rng = 0x9E3779B97F4A7C15ull;
    bool spare = false; double spareV = 0.0;
    Biquad2 bearing, tension, scr[4];
    double gBearing = 1.0, gTension = 1.0, gScrape = 1.0;
    double xs[4] {};
};

} // namespace mt
