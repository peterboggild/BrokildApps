/*  MARMALADE 50 - engine

    Plain C++ with no JUCE in it, so the bench drives it directly. The amp
    is Tv50Netlist.h, solved as the blocks of Tv50Split.h; this file is
    everything around it: the oversampling, the pots and relays turned into
    resistances the way the sheet's parts would set them, the mains, the
    reverb tank's hand-off, and the last step from the 8 ohm tap to the host.

    Nothing here shapes the sound. The output is the voltage across the
    speaker's terminals. The springs are the one part that is not a circuit
    here: they are a measured impulse response of a tank, driven by the
    voltage the reverb transformer puts across the tank's input coil and
    feeding the recovery valve's grid (see TankFn below). Without one the
    reverb circuit runs and the springs are silent.
*/
#pragma once

#include "HalfBand.h"
#include "Partition.h"

#include <atomic>
#include <cmath>
#include <functional>
#include <vector>

namespace mm
{

struct Params50
{
    int   channel = 0;       // 0: preamp 1 (gain, shape, volume), 1: preamp 2 (gain, treble, middle, bass, volume)
    float gain1   = 0.40f;   // RV1, dual A1M0
    float shape   = 0.50f;   // RV2, dual B500K
    float volume1 = 0.50f;   // RV3, A500K
    float gain2   = 0.40f;   // RV4, dual A1M0
    float treble  = 0.50f;   // RV7, B250K
    float middle  = 0.50f;   // RV6, B25K
    float bass    = 0.50f;   // RV5, A500K
    float volume2 = 0.50f;   // RV8, A500K
    float reverb  = 0.30f;   // RV9, 250KA
    bool  reverbOn = true;   // RL2
    float atten   = 0.30f;   // RV10, dual B1M0: clockwise is more attenuation
    bool  attenOn = false;   // RL3
    float output  = 0.50f;   // trim, -24 .. +12 dB
    int   quality = 0;       // 0: >= 88.2 kHz, 1: >= 176.4 kHz

    /*  The bench's alone: the input goes to the FX loop's RETURN jack
        instead of the guitar's, as a technician's generator would. */
    bool  intoReturn = false;

    /*  No DC-mains switch, unlike the Marmalade 120's bench: this bias
        supply is half wave, and a winding held at its peak never charges
        it - the EL34s run away (measured: 446 mA). The mains always run. */
};

class Engine50
{
public:
    Engine50();

    bool ok() const { return good; }
    const std::string& error() const { return err; }

    void prepare (double sampleRate, int maxBlock);
    void setParams (const Params50& p) { pending = p; }
    void process (float* left, float* right, int numSamples);

    /*  The springs. Called once per CHUNK host samples with the tank's
        drive (volts across its input coil, at the host rate) and returns,
        in place, the tank's output volts for the NEXT chunk: so the impulse
        response handed to it must be advanced by CHUNK samples (its first
        CHUNK samples dropped - silence for any spring, whose first arrival
        is tens of milliseconds in). Null: no tank. Audio thread. */
    using TankFn = std::function<void (float* io, int n)>;
    void setTank (TankFn f) { tank = std::move (f); }
    static constexpr int CHUNK = 32;

    int latencySamples() const { return latency; }
    int oversampling() const { return os; }

    float inLevel()   const { return mIn.load(); }
    float outLevel()  const { return mOut.load(); }
    float watts()     const { return mWatts.load(); }
    float sag()       const { return mSag.load(); }
    float plateGlow() const { return mGlow.load(); }

    Partition& circuit() { return amp; }
    int resets() const { return resetCount; }

    static float outputDb (float v) { return -24.0f + 36.0f * v; }

private:
    void applyPots (bool force);
    bool warmUp();

    Partition amp;
    bool good = false;
    std::string err;

    int oSpk = -1, oTank = -1, oB = -1;

    double sr = 48000.0, srOS = 96000.0;
    int os = 2, latency = 0, maxBlock = 512;

    Params50 pending, p;
    // the pots as the circuit has them, smoothed toward the panel; NaN: never applied
    enum { G1, SH, V1, G2, TR, MI, BA, V2, RV, AT, NPOT };
    float s[NPOT] {}, a[NPOT] {};
    int aChannel = -1, aRevOn = -1, aAttOn = -1, aReturn = -1;

    double bIdle = 390.0, bSmooth = 390.0;
    double mainsPhase = 0.0;
    std::vector<double> ampOut;

    HalfBand up1, up2, dn1, dn2;
    std::vector<float> osIn, osSpk;

    TankFn tank;
    float tankBuf[CHUNK] {};
    double tankAcc = 0.0;            // the drive, averaged over one host sample
    int tankFill = 0;
    float tankRet[CHUNK] {};         // this chunk's return, host rate
    double retPrev = 0.0;            // for the interpolation up to the circuit's rate

    double dcIn = 0, dcOut = 0;
    float outGain = 1.0f;
    double wattsAcc = 0.0;
    int resetCount = 0;

    std::atomic<float> mIn { 0 }, mOut { 0 }, mWatts { 0 }, mSag { 0 }, mGlow { 0 };
};

} // namespace mm
