#pragma once

// SNARE TACTICS — the engine. Plain C++17, no JUCE, so the bench can measure it.
//
// Signal path, all of it synthesised (no samples anywhere):
//
//   [ at 4x the host rate ]
//   VOICES (four)  HEAD   the electronic pair (1 : 1.83) and/or the membrane's
//                         Bessel modes, weighted by where it is struck
//                  WIRES  noise, high-passed by TENSION, low-passed by AIR,
//                         lagging the stick, buzzing at the head's pitch and
//                         ringing along with it when loose
//                  STICK  the click; on a rim shot, the shell's modes too
//                  CLAP   band-passed noise, retriggered, then a tail
//   GRIT       sample-and-hold + bit reduction           (bypassed at 0)
//   TRANSIENT  gain from the voices' OWN envelopes        (bypassed at 0/0)
//   ROOM       8-line FDN behind two diffusers           (bypassed at 0)
//   GATE       keyed by the hit itself, not a threshold   (bypassed at 0)
//   DRIVE      one of four Battlestar Overdrive engines   (bypassed at 0)
//   COLOUR     12 dB/oct low-pass                         (bypassed at 100 %)
//   2 x half-band decimation
//   [ at the host rate ]
//   COMP   peak-held detector, soft knee, make-up, 1.5 ms look-ahead
//   ECHO   ping-pong tape echo, filtered and saturated in the loop, synced
//   LEVEL, then a soft ceiling under 0 dBFS on both channels
//
// Mono up to the echo. The echo is the only thing that makes it stereo, so
// with ECHO at 0 both channels are the same sample for sample.
//
// Real-time rules: prepare() allocates, process() never does.

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "st_params.h"

namespace st
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
    inline float hp (float x)          // high-pass output of one tick
    {
        float lp, bp; tick (x, lp, bp);
        return x - k * bp - lp;
    }
    inline float lpOnly (float x) { float lp, bp; tick (x, lp, bp); return lp; }
};

constexpr int kModes = 10;     // membrane modes, the fundamental included
constexpr int kShell = 3;      // shell / hoop modes (rim shot, cross-stick)
constexpr int kBursts = 4;     // clap

struct Voice
{
    bool   on = false;
    int    artic = ART_SNARE;
    double t = 0.0;            // samples since the hit, at the OS rate
    double ph = 0.0, ph2 = 0.0;
    double phM[kModes] {};
    double pe = 1.0;           // pitch envelope
    double me[kModes] {};      // per-mode envelopes
    double meK[kModes] {};     // per-mode decay per sample, fixed at the hit
    float  mAmp[kModes] {};    // per-mode amplitude, fixed at the hit (STRIKE)
    float  keyMul = 1.0f;
    float  velGain = 1.0f, wireVel = 1.0f, stickVel = 1.0f, dropVel = 1.0f;
    // what each part of the drum gets for this articulation
    float  bodyAmt = 1.0f, bodyDecayMul = 1.0f, wireAmt = 1.0f, clapAmt = 0.0f;
    float  stickAmt = 0.0f, rimAmt = 0.0f, shellMul = 1.0f;
    // wires, fixed at the hit
    Svf    wHp, wLp;
    float  wGain = 1.0f, wLag = 0.0f, wAtt = 1.0f, wBuzz = 0.0f, wSymp = 0.0f;
    bool   wLpOn = true;
    // stick and shell
    Svf    sHp;
    double stE = 1.0, stK = 1.0, shE[kShell] {}, shK[kShell] {}, shPh[kShell] {};
    float  shF[kShell] {}, shA[kShell] {}, stBlipF = 4000.0f;
    double stPh = 0.0;
    // clap
    Svf    cBp;
    float  cNorm = 1.0f;
    double cBurstAt[kBursts] {};
    int    cNumBursts = 1;
    double cBurstK = 1.0, cTailK = 1.0;
    double cEnv = 0.0;
    int    cNext = 0;
    double cTail = 0.0;
    uint32_t rng = 1;
    // steal
    float  choke = 1.0f, chokeStep = 0.0f;
};

//==============================================================================
class Engine
{
public:
    static constexpr int kAudible = 4, kSlots = 6;   // 4 heard; 2 spare so a steal can fade

    void prepare (double sampleRate, int maxBlock);
    void reset();

    //  a hit, applied at the start of the next sample rendered. The note is
    //  read through KEYS (FIXED / GM KIT / CHROMATIC); noteOnArtic forces an
    //  articulation (the panel's rim, cross-stick and clap).
    void noteOn (int note, float velocity01);
    void noteOnArtic (int artic, float velocity01);

    //  the host tempo, for the echo; 120 when nobody says otherwise
    void setTempo (double bpm);

    //  stereo out at the host rate
    void process (const Params& p, float* left, float* right, int n);

    //  the decimator (23 host samples) plus the compressor's look-ahead
    //  (1.5 ms), constant whatever the settings so a host compensates once
    static constexpr int kDecimatorLatency = 23;
    int latencySamples() const { return kDecimatorLatency + laN; }
    double sampleRate() const { return fs; }

    // meters (message thread reads)
    float meterPeak() const  { return mPeak.load (std::memory_order_relaxed); }
    float meterGrDb() const  { return mGr.load (std::memory_order_relaxed); }
    int   hitCount() const   { return hits.load (std::memory_order_relaxed); }

    //  offline: render one hit (left channel) into `out` (n host samples).
    //  Used by the panel's display and by the bench.
    static void renderHit (const Params& p, double sampleRate, int artic, float vel,
                           std::vector<float>& out, int n);

    // bench taps
    float lastPitchHz() const { return lastPitch; }
    double echoSeconds() const { return echoT; }
    static float driveTrimFor (int engine, float drive);
    //  bench only: freeze the head (no tension glide, no decay), so the
    //  processing after it can be measured on a steady tone - a hard clipper's
    //  upper harmonics move with level, and a decaying tone smears them
    static inline bool benchSteady = false;

    //  what a note means under a KEYS setting (the bench and the panel ask)
    static int articFor (int keys, int note);

private:
    void startVoice (int artic, int note, float vel);
    void renderOS (const Params& p, float* os4);

    double fs = 48000.0, fo = 192000.0;

    Params P;
    std::array<Voice, kSlots> voices;
    uint32_t hitSeed = 0;

    // grit
    double gritPh = 1.0;
    float  gritHeld = 0.0f;

    // transient detectors (read the voices' envelopes)
    float tFA = 0, tSA = 0, tFR = 0, tSR = 0;
    float aFA = 0, aSA = 0, aRelA = 0, aFR = 0, aSR = 0;

    // room: two diffusers, then an 8-line FDN
    static constexpr int kLines = 8;
    std::array<std::vector<float>, kLines> line;
    std::array<int, kLines> len {}, pos {};
    std::array<float, kLines> dampZ {};
    std::array<std::vector<float>, 2> ap;
    std::array<int, 2> apLen {}, apPos {};
    bool roomLive = false;

    // gate: keyed by the hit
    double sinceHit = 1.0e12;
    float  gateEnv = 0.0f;

    // drive
    float driveSm = 0.0f, colourSm = 1.0f, roomSm = 0.0f;
    double adaaPrev = 0.0;
    Svf   colourLp;
    int   coefTick = 0;

    // decimation
    HalfBand hb1, hb2;

    // comp (host rate)
    std::vector<float> dqVal;
    std::vector<long long> dqIdx;
    int dqMask = 0, holdN = 1;
    std::vector<float> laBuf;
    int laN = 0, laPos = 0;
    long long dqHead = 0, dqTail = 0, cTick = 0;
    float compG = 0.0f;

    // echo (host rate, stereo)
    std::array<std::vector<float>, 2> eBuf;
    int eLen = 0, eW = 0;
    double bpm = 120.0, echoT = 0.375, echoTSm = 0.375, wowPh = 0.0;
    float eHpZ[2] {}, eLpZ[2] {}, echoSm = 0.0f;
    bool echoLive = false;

    bool  fresh = true;
    float levelSm = 1.0f;

    // pending hits (several can land on one sample: a flam, a chord of articulations)
    struct Pending { int artic; int note; float vel; };
    std::array<Pending, 8> pend {};
    int numPend = 0;

    float lastPitch = 0.0f;
    std::atomic<float> mPeak { 0.0f }, mGr { 0.0f };
    std::atomic<int>   hits { 0 };
};

//==============================================================================
//  the drive engines, ported from Battlestar Overdrive (Source/Engine.cpp)
float driveShape (int engine, float x, float k);
float driveADAA  (int engine, float x, double xPrev, float k);
float driveMaxFor (int engine);
const char* driveEngineName (int engine);

//  helpers shared with the panel
const char* noteName (float hz, char* buf, int bufLen);   // "F#3 +12c"

//  the head's modes: ratio to the fundamental, and whether a centre strike
//  excites it (axisymmetric, the (0,n) modes)
extern const float kModeRatio[kModes];
extern const bool  kModeAxis[kModes];

} // namespace st
