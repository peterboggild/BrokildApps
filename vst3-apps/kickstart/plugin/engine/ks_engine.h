#pragma once

// KICKSTART — the engine. Plain C++17, no JUCE, so the bench can measure it.
//
// Signal path, all of it synthesised (no samples anywhere):
//
//   [ at 4x the host rate ]
//   VOICE  body oscillator (pitch envelope, wave morph, amp curve)
//          + SKIN membrane modes and tension glide
//          + CLICK (tuned burst + band-passed noise)
//   GRIT   sample-and-hold + bit reduction           (bypassed at 0)
//   TRANSIENT  gain from the voice's OWN envelopes   (bypassed at 0/0)
//   ROOM   small 4-line FDN                          (bypassed at 0)
//   DRIVE  one of four Battlestar Overdrive engines  (bypassed at 0)
//   COLOUR 12 dB/oct low-pass                        (bypassed at 100 %)
//   2 x half-band decimation
//   [ at the host rate ]
//   COMP   peak-held detector, soft knee, make-up    (bypassed at 0)
//   LEVEL, then a soft ceiling under 0 dBFS
//
// WHY THE TRANSIENT SHAPER READS THE VOICE'S ENVELOPES, NOT THE AUDIO: a
// transient designer is two envelope followers whose difference is turned
// into gain, and on a 40 Hz kick a follower fast enough to see the attack
// ripples at 80 Hz, which is distortion. A synthesiser KNOWS its envelopes,
// so the same SPL-style difference is computed from the exact envelopes and
// the gain it produces is smooth by construction.
//
// Real-time rules: prepare() allocates, process() never does.

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "ks_params.h"

namespace ks
{

constexpr int kOS = 4;

//==============================================================================
struct HalfBand      // 63-tap windowed-sinc polyphase half-band (Battlestar's)
{
    static constexpr int TAPS = 63, PH = 32, MASK = PH - 1;
    std::array<float, PH> ge {}, go {}, dnE {}, dnO {};
    int dnW = 0;
    void design();
    void clear() { dnE.fill (0.0f); dnO.fill (0.0f); dnW = 0; }
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

struct Svf            // TPT state-variable filter
{
    float g = 0, k = 1.414f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void set (float fc, float fs, float q);
    void clear() { ic1 = ic2 = 0.0f; }
    inline void tick (float x, float& lp, float& bp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1;
    }
};

constexpr int kModes = 7;

struct Voice
{
    bool   on = false;
    double t = 0.0;           // samples since the hit, at the OS rate
    double ph = 0.0;
    double phM[kModes] {};
    double pe = 1.0;          // pitch envelope, exp(-t / bend)
    double me[kModes] {};     // per-mode envelopes
    float  vel = 1.0f, keyMul = 1.0f;
    float  velGain = 1.0f, clickVel = 1.0f, sweepVel = 1.0f;
    // click, fixed at the hit
    float  cfc = 1000.0f, cph0 = 0.0f, cnf = 0.0f, cnorm = 1.0f;
    double cph = 0.0, ce = 1.0, ceK = 1.0;
    Svf    cbp;
    uint32_t rng = 1;
    // choke
    float  choke = 1.0f, chokeStep = 0.0f;
    // what the transient shaper reads: this sample's envelope magnitude
    float  envNow = 0.0f;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    //  a hit, applied at the start of the next sample rendered
    void noteOn (int note, float velocity01);

    //  mono out at the host rate
    void process (const Params& p, float* out, int n);

    //  the decimator (23 host samples) plus the compressor's look-ahead
    //  (1.5 ms), constant whatever the settings so a host compensates once
    static constexpr int kDecimatorLatency = 23;
    int latencySamples() const { return kDecimatorLatency + laN; }

    double sampleRate() const { return fs; }

    // meters (message thread reads)
    float meterPeak() const  { return mPeak.load (std::memory_order_relaxed); }
    float meterGrDb() const  { return mGr.load (std::memory_order_relaxed); }
    int   hitCount() const   { return hits.load (std::memory_order_relaxed); }

    //  offline: render one hit into `out` (n host samples). Used by the
    //  panel's display and by the bench.
    static void renderHit (const Params& p, double sampleRate, int note, float vel,
                           std::vector<float>& out, int n);

    // bench taps
    float lastPitchHz() const { return lastPitch; }
    static float driveTrimFor (int engine, float drive);   // the measured trim

private:
    void renderOS (const Params& p, float* os4);   // four OS samples

    double fs = 48000.0, fo = 192000.0;

    Params P;                    // the params of the current block
    std::array<Voice, 2> voices;
    uint32_t hitSeed = 0;

    // grit
    double gritPh = 1.0;
    float  gritHeld = 0.0f;

    // transient detectors (read the voices' envelopes)
    float tFA = 0, tSA = 0, tFR = 0, tSR = 0;
    float aFA = 0, aSA = 0, aRelA = 0, aFR = 0, aSR = 0;

    // room
    static constexpr int kLines = 4;
    std::array<std::vector<float>, kLines> line;
    std::array<int, kLines> len {}, pos {};
    std::array<float, kLines> dampZ {};
    bool roomLive = false;

    // drive
    float driveSm = 0.0f, colourSm = 1.0f, roomSm = 0.0f;
    double adaaPrev = 0.0;          // first-order antiderivative anti-aliasing
    Svf   colourLp;
    int   coefTick = 0;

    // decimation
    HalfBand hb1, hb2;

    // comp (host rate)
    std::vector<float> dqVal;
    std::vector<long long> dqIdx;
    int dqMask = 0, holdN = 1;
    std::vector<float> laBuf;       // the compressor's look-ahead
    int laN = 0, laPos = 0;
    bool fresh = true;              // snap the smoothed controls on the first block
    long long dqHead = 0, dqTail = 0, cTick = 0;
    float compG = 0.0f;
    float levelSm = 1.0f;

    // pending hit
    int   pendingNote = -1;
    float pendingVel = 0.0f;

    float lastPitch = 0.0f;
    std::atomic<float> mPeak { 0.0f }, mGr { 0.0f };
    std::atomic<int>   hits { 0 };
};

//==============================================================================
//  the drive engines, ported from Battlestar Overdrive (Source/Engine.cpp)
float driveShape (int engine, float x, float k);
float driveADAA  (int engine, float x, double xPrev, float k);   // first-order ADAA
float driveMaxFor (int engine);
const char* driveEngineName (int engine);

//  helpers shared with the panel
const char* noteName (float hz, char* buf, int bufLen);   // "A1 +12c"

} // namespace ks
