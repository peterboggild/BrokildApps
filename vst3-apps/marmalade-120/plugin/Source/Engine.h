/*  MARMALADE 120 - engine

    Plain C++ with no JUCE in it, so the offline bench can measure the whole
    signal path without a host. The amplifier itself is Circuit.h solving the
    two netlists in Or120Netlist.h; this file is everything around it: the
    oversampling, the knobs turned into resistances and capacitors the way
    real pots and a real rotary switch would set them, the supply handed from
    the power section back to the preamp, and the last step from the speaker's
    8 ohm tap to the host.

    Nothing here shapes the sound. The output is the voltage across the
    speaker's terminals, with the speaker's electrical impedance loading the
    output stage as part of the circuit; what the cabinet and a microphone do
    to it is a measured impulse response's job (the plugin's CAB), because a
    response made up here would be the one part of the amp that is not true.
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

//==============================================================================
/*  The six FAC positions, as capacitors: what couples the second stage to the
    driver, from full (68n alone) to thin (the whole chain, 203p). */
constexpr int NUM_FAC = 6;
extern const double FAC_CAPS[NUM_FAC];

struct Params
{
    float gain    = 0.40f;   // 1M log, between the tone stack and the second stage
    float bass    = 0.60f;   // 1M log
    float treble  = 0.60f;   // 1M log
    float boost   = 0.0f;    // 1k lin rheostat into the driver's resonant cathode network
    float output  = 0.50f;   // trim, -24 .. +12 dB, centre 0 dB is not the middle
    int   fac     = 0;       // 0 .. 5, full to thin
    int   quality = 0;       // 0: run the circuit at >= 88.2 kHz, 1: >= 176.4 kHz

    /*  The bench's alone, never a host parameter: false holds the transformer
        windings at their peak instead of running the mains, so the supply is
        DC and hum-free. Aliasing cannot be told apart from the rectifier's
        own 100 Hz harmonics mixing with a note otherwise. */
    bool  mains   = true;
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
    const Circuit& circuit() const { return amp; }
    Circuit& circuit() { return amp; }
    int resets() const { return resetCount; }

    static float outputDb (float v) { return -24.0f + 36.0f * v; }

private:
    void applyPots (bool force);
    bool warmUp();

    Circuit amp;
    bool good = false;
    std::string err;

    // part indices into the netlists
    int iGainA = -1, iGainB = -1, iBassA = -1, iBassB = -1, iTrebA = -1, iTrebB = -1;
    int iBoost = -1, iFac = -1;
    int oSpk = -1, oB4 = -1, oB1 = -1;

    double sr = 48000.0, srOS = 96000.0;
    int os = 2, latency = 0, maxBlock = 512;

    Params pending, p;

    // the knobs as the circuit has them now, smoothed toward the panel
    float sGain = 0, sBass = 0, sTreb = 0, sBoost = 0;
    float aGain = -1, aBass = -1, aTreb = -1, aBoost = -1;
    int aFac = -1;
    int sinceApply = 0;

    double b1Idle = 500.0, b1Smooth = 500.0;
    double mainsPhase = 0.0;
    std::vector<double> ampOut;

    HalfBand up1, up2, dn1, dn2;
    std::vector<float> osIn, osSpk;

    double dcIn = 0, dcOut = 0;

    float outGain = 1.0f;
    double wattsAcc = 0.0;
    int resetCount = 0;

    std::atomic<float> mIn { 0 }, mOut { 0 }, mWatts { 0 }, mSag { 0 }, mGlow { 0 };
};

} // namespace mm
