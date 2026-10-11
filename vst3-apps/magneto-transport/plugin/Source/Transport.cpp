#include "Transport.h"

#include <algorithm>
#include <cmath>

namespace mt
{
namespace
{
constexpr double PI = 3.14159265358979323846;

// the causes at TRANSPORT CONDITION 100 % with every cause control at 100 %
constexpr double R_CAP   = 0.005;    // capstan radius, m
constexpr double E_CAP   = 5.0e-6;   // its runout, m
constexpr double R_ROLL  = 0.0125;   // pinch roller radius, m
constexpr double E_ROLL  = 20.0e-6;  // its runout, m
constexpr double K_ROLL  = 0.33;     // how much of the roller's runout reaches the tape
constexpr double S_BEAR  = 4.0e-4;   // bearing roughness, rms speed error
constexpr double M_AC    = 1.0e-4;   // AC motor torque ripple at twice the mains, as speed
constexpr double M_DC    = 2.0e-4;   // DC servo tachometer slot ripple, as speed
constexpr int    DC_SLOTS = 24;
constexpr double MAINS   = 50.0;
constexpr double EA      = 1000.0;   // N: 1/4-inch tape, E x cross-section
constexpr double T_ECC   = 0.8;      // reel eccentricity's tension variation, N
constexpr double T_NOISE = 0.3;      // holdback friction, N rms
constexpr double T_LOOP  = 0.1;      // a loop's guide friction, N rms
constexpr double A_SCR   = 0.05;     // stick-slip displacement, um rms (sidebands ~ -45 dB around a tone, worn)
constexpr double F_SCR   = 3200.0;   // the tape span's longitudinal resonance, Hz
} // namespace

void Transport::bandpass (Biquad2& f, double fs, double f0, double q)
{
    const double w0 = 2.0 * PI * f0 / fs, a = std::sin (w0) / (2.0 * q), c = std::cos (w0), n = 1.0 + a;
    f.b0 = a / n; f.b1 = 0.0; f.b2 = -a / n; f.a1 = -2.0 * c / n; f.a2 = (1.0 - a) / n;
}

void Transport::lowpass (Biquad2& f, double fs, double f0, double q)
{
    const double w0 = 2.0 * PI * f0 / fs, a = std::sin (w0) / (2.0 * q), c = std::cos (w0), n = 1.0 + a;
    f.b0 = (1.0 - c) / (2.0 * n); f.b1 = (1.0 - c) / n; f.b2 = f.b0; f.a1 = -2.0 * c / n; f.a2 = (1.0 - a) / n;
}

double Transport::unitGain (Biquad2 f, int n)
{
    f.z1 = f.z2 = 0.0;
    double e = 0.0;
    for (int i = 0; i < n; ++i) { const double y = f.tick (i == 0 ? 1.0 : 0.0); e += y * y; }
    return e > 0.0 ? 1.0 / std::sqrt (e) : 1.0;
}

// white noise of unit variance, uniform: every source is filtered narrower than
// white, so it comes out Gaussian anyway (and costs one xorshift, not a log)
double Transport::uni()
{
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
    return ((double) (rng >> 11) * (1.0 / 9007199254740992.0) - 0.5) * 3.4641016151377544;
}

double Transport::gauss()
{
    if (spare) { spare = false; return spareV; }
    double u, v, s;
    do {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        u = (double) (rng >> 11) * (2.0 / 9007199254740992.0) - 1.0;
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        v = (double) (rng >> 11) * (2.0 / 9007199254740992.0) - 1.0;
        s = u * u + v * v;
    } while (s >= 1.0 || s == 0.0);
    const double m = std::sqrt (-2.0 * std::log (s) / s);
    spare = true; spareV = v * m;
    return u * m;
}

void Transport::prepare (double fsInt)
{
    fs = fsInt; dt = 1.0 / fs;
    rng = 0x9E3779B97F4A7C15ull; spare = false;     // the same machine every time: bounces reproduce
    bandpass (bearing, fs, 9.5, 0.35);               // bearing roughness, 3-30 Hz
    lowpass (tension, fs, 2.0, 0.707);               // tension noise, below 2 Hz
    for (auto& s : scr) bandpass (s, fs, F_SCR, 4.0);
    // normalise each to unit rms for unit white noise (the 2 Hz lowpass needs seconds)
    gBearing = unitGain (bearing, (int) (4.0 * fs));
    gTension = unitGain (tension, (int) (8.0 * fs));
    gScrape  = unitGain (scr[0], (int) (0.1 * fs));
}

void Transport::reset (double vCms)
{
    w = fieldV = vCms; delta = 0.0;
    angCap = angRoll = angSup = angTak = angMains = 0.0;
    bearing.z1 = bearing.z2 = tension.z1 = tension.z2 = 0.0;
    for (auto& s : scr) s.z1 = s.z2 = 0.0;
    for (double& x : xs) x = 0.0;
}

double Transport::tick (const TransportInput& in)
{
    // ---- the motor. Both are J w' = -K delta - D (w - target), delta' = w - target:
    //      an AC synchronous motor's synchronising torque on angle and damper torque
    //      on slip, or a DC servo's PI loop on its tachometer.
    double fh, zeta;
    if (in.motor == 0)
    {
        // a frequency converter feeds the field; it slews (a synchronous motor driven
        // harder would fall out of step)
        const double step = 120.0 * dt;
        fieldV += std::clamp (in.vTargetCms - fieldV, -step, step);
        fh = 1.2; zeta = 0.8;
    }
    else
    {
        const double step = 300.0 * dt;   // torque-limited acceleration
        fieldV += std::clamp (in.vTargetCms - fieldV, -step, step);
        fh = 4.0; zeta = 1.1;
    }
    const double wh = 2.0 * PI * fh;
    w += dt * (-wh * wh * delta - 2.0 * zeta * wh * (w - fieldV));
    delta += dt * (w - fieldV);

    // ---- angles of everything that turns
    const double vm = w * 0.01;                       // m/s
    angCap  += vm / R_CAP * dt;
    angRoll += vm / R_ROLL * dt;
    angSup  += vm / std::max (in.supplyR, 0.02) * dt;
    angTak  += vm / std::max (in.takeupR, 0.02) * dt;
    angMains += 2.0 * PI * 2.0 * MAINS * dt;
    if (angCap > 2.0 * PI) angCap -= 2.0 * PI;
    if (angRoll > 2.0 * PI) angRoll -= 2.0 * PI;
    if (angSup > 2.0 * PI) angSup -= 2.0 * PI;
    if (angTak > 2.0 * PI) angTak -= 2.0 * PI;
    if (angMains > 2.0 * PI) angMains -= 2.0 * PI;

    // ---- the causes, as speed errors at the heads (an ideal transport has none)
    const double c = std::clamp (in.cond, 0.0, 1.0);
    if (c <= 0.0) { for (double& x : xs) x = 0.0; return w; }
    const double kc = c * in.capstan, kr = c * in.reels, ks = c * in.scrape;
    const double nb = bearing.tick (uni()) * gBearing;
    const double nt = tension.tick (uni()) * gTension;
    double m = 0.0;
    m += kc * (E_CAP / R_CAP) * std::sin (angCap);
    m += kc * K_ROLL * (E_ROLL / R_ROLL) * std::sin (angRoll + 1.3);
    m += kc * S_BEAR * nb;
    m += kc * (in.motor == 0 ? M_AC * std::sin (angMains) : M_DC * std::sin (DC_SLOTS * angCap));
    if (in.reelMode)
        m += kr * ((T_ECC / EA) * (std::sin (angSup) + std::sin (angTak + 0.7)) + (T_NOISE / EA) * nt);
    else
        m += kr * (T_LOOP / EA) * nt;

    // ---- stick-slip at each head: a local displacement around the span's resonance
    for (int i = 0; i < 4; ++i) xs[i] = ks * A_SCR * scr[i].tick (uni()) * gScrape;

    return w * (1.0 + m);
}

} // namespace mt
