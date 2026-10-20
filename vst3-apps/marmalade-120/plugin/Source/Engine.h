/*  MARMALADE 120 - engine

    Plain C++ with no JUCE in it, so the offline bench can measure the whole
    signal path without a host. The amplifier itself is Circuit.h solving the
    two netlists in Or120Netlist.h; this file is everything around it: the
    oversampling, the knobs turned into resistances and capacitors the way
    real pots and a real rotary switch would set them, the supply handed from
    the power section back to the preamp, and the last step from the speaker's
    cone to a microphone.

    Nothing here shapes the sound on purpose except that last step (CAB),
    which is the one part that is not a circuit: see cabinet() in Engine.cpp.
*/
#pragma once

#include "Circuit.h"

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

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

    void design();
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

/*  Transposed direct form II, in double: the cabinet's low shelf and peaks
    sit at a few hundred hertz of a 48 kHz rate, where float coefficients
    audibly misplace them. */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void clear() { z1 = z2 = 0; }

    void setLP   (double f, double sr, double q);
    void setHP   (double f, double sr, double q);
    void setPeak (double f, double sr, double q, double dB);
};

//==============================================================================
/*  The six FAC positions, as capacitors: the switch picks which one couples the
    second stage into the tone stack, so it moves the low cut from full to thin. */
constexpr int NUM_FAC = 6;
extern const double FAC_CAPS[NUM_FAC];

struct Params
{
    float gain    = 0.55f;   // 1M log, between the first two stages
    float bass    = 0.50f;   // 1M lin
    float treble  = 0.55f;   // 1M lin
    float hfDrive = 0.25f;   // 1M lin, as a rheostat
    float master  = 0.35f;   // 220k log dual gang, after the phase inverter
    float output  = 0.50f;   // trim, -24 .. +12 dB, centre 0 dB is not the middle
    int   fac     = 2;       // 0 .. 5
    bool  cab     = true;    // speaker-and-mic stage on, or the 8 ohm tap as a line out
    int   quality = 0;       // 0: run the circuit at >= 88.2 kHz, 1: >= 176.4 kHz
};

//==============================================================================
class Engine
{
public:
    Engine();

    /*  False if a netlist failed to parse or the amp failed to find its
        operating point - the plugin then passes audio through untouched
        rather than guess. error() says why. */
    bool ok() const { return good; }
    const std::string& error() const { return err; }

    void prepare (double sampleRate, int maxBlock);
    void setParams (const Params& p) { pending = p; }
    void process (float* left, float* right, int numSamples);

    int latencySamples() const { return latency; }
    int oversampling() const { return os; }

    // read by the panel's timer, so atomics
    float inLevel()   const { return mIn.load(); }
    float outLevel()  const { return mOut.load(); }
    float watts()     const { return mWatts.load(); }      // into the speaker, smoothed
    float sag()       const { return mSag.load(); }        // volts below the idle B+
    float plateGlow() const { return mGlow.load(); }       // 0..1, the output valves' work

    // for the bench
    const Circuit& preamp() const { return pre; }
    const Circuit& power() const { return pw; }
    int resets() const { return resetCount; }

    static float outputDb (float v) { return -24.0f + 36.0f * v; }

private:
    void applyPots (bool force);
    bool warmUp();
    double cabinet (double coneSlope);

    Circuit pre, pw;
    bool good = false;
    std::string err;

    // part indices into the netlists
    int iGainA = -1, iGainB = -1, iBassA = -1, iBassB = -1, iTrebA = -1, iTrebB = -1;
    int iHf = -1, iFac = -1;
    int iMasAA = -1, iMasAB = -1, iMasBA = -1, iMasBB = -1;
    int oDrive = -1, oSpk = -1, oCone = -1, oB4 = -1, oB1 = -1;

    double sr = 48000.0, srOS = 96000.0;
    int os = 2, latency = 0, maxBlock = 512;

    Params pending, p;

    // the knobs as the circuit has them now, smoothed toward the panel
    float sGain = 0, sBass = 0, sTreb = 0, sHf = 0, sMaster = 0;
    float aGain = -1, aBass = -1, aTreb = -1, aHf = -1, aMaster = -1;
    int aFac = -1;
    int sinceApply = 0;

    // power-section values carried across each sample
    double b4 = 330.0, b1Idle = 500.0, coneLast = 0.0;
    std::vector<double> preOut, pwOut;

    HalfBand up1, up2, dnA1, dnA2, dnB1, dnB2;
    std::vector<float> osIn, osSpk, osCone;

    // the cabinet, at the host rate
    std::array<Biquad, 6> cab;
    double cabGain = 1.0;
    float cabMix = 1.0f;         // crossfades CAB in and out instead of clicking
    double dcIn = 0, dcOut = 0;

    float outGain = 1.0f;
    double wattsAcc = 0.0;
    int resetCount = 0;

    std::atomic<float> mIn { 0 }, mOut { 0 }, mWatts { 0 }, mSag { 0 }, mGlow { 0 };
};

} // namespace mm
