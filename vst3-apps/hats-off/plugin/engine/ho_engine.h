#pragma once

// HATS OFF — the engine. Plain C++17, no JUCE, so the bench can measure it.
//
//   [ at 4x the host rate, stereo ]
//   VOICES (six heard)
//     BRONZE   up to 72 modes as complex one-pole rotations: frequencies from
//              SIZE, PITCH and DENSITY (sparse at the bottom, dense at the top,
//              most above the lowest few in near-degenerate pairs that beat),
//              decays falling with frequency, amplitudes from STRIKE and
//              velocity, a slow RISE per mode for BLOOM (energy flowing up), a
//              few dome partials for the bell, each mode placed across WIDTH
//     CIRCUIT  the 808: six band-limited squares through a band-pass
//     NOISE    a noise layer
//     HAT      OPEN damps everything; SIZZLE is contact rattle; CHICK the foot
//     STICK    the tick
//   EQ (CUT, AIR) -> GRIT -> TRANSIENT -> ROOM -> DRIVE -> COLOUR
//   2 x half-band decimation per channel
//   [ at the host rate ]
//   COMP (linked, look-ahead) -> ECHO (ping-pong) -> LEVEL -> soft ceiling
//
// At WIDTH 0 and ECHO 0 the two channels are the same sample for sample.
// Real-time rules: prepare() allocates, process() never does.

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "ho_params.h"

namespace ho
{

constexpr int kOS = 4;

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

//  its twin: one sample in, two out, the same coefficients (zero-stuff and filter)
struct HalfUp
{
    std::array<float, HalfBand::PH> ge {}, go {}, buf {};
    int w = 0;
    void design (const HalfBand& h) { ge = h.ge; go = h.go; }
    void clear() { buf.fill (0.0f); w = 0; }
    inline void up (float x, float& a, float& b)
    {
        buf[(size_t) w] = x;
        float s0 = 0.0f, s1 = 0.0f;
        for (int k = 0; k < HalfBand::PH; ++k)
        {
            const float v = buf[(size_t) ((w - k) & HalfBand::MASK)];
            s0 += ge[(size_t) k] * v; s1 += go[(size_t) k] * v;
        }
        w = (w + 1) & HalfBand::MASK;
        a = 2.0f * s0; b = 2.0f * s1;
    }
};

struct Svf
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
    inline float hp (float x) { float lp, bp; tick (x, lp, bp); return x - k * bp - lp; }
    inline float lpOnly (float x) { float lp, bp; tick (x, lp, bp); return lp; }
    inline float bpNorm (float x) { float lp, bp; tick (x, lp, bp); return bp * k; }
};

constexpr int kMaxModes = 72;       // up to 64 plate modes + 8 dome partials
constexpr int kSquares = 6;
constexpr int kWash = 8;          // noise bands for the dense top

struct alignas (32) Voice
{
    // the modal bank, structure of arrays so the inner loop vectorises
    alignas (32) float zr[kMaxModes] {}, zi[kMaxModes] {}, cr[kMaxModes] {}, ci[kMaxModes] {};
    alignas (32) float g[kMaxModes] {}, ga[kMaxModes] {}, pl[kMaxModes] {}, pr[kMaxModes] {};
    int   nModes = 0;
    float low0 = 1.0f;              // mode 0's amplitude, for the china's ring
    bool  on = false;
    int   artic = ART_DIALLED;
    double t = 0.0, tBase = 1.0;    // samples since the hit; the longest -60 dB time
    float velGain = 1.0f;
    float bronzeAmt = 1.0f, circuitAmt = 0.0f, noiseAmt = 0.0f, trash = 0.0f, bodyAmt = 1.0f;
    // circuit
    double sqPh[kSquares] {};
    float  sqInc[kSquares] {};
    Svf    cBp;
    double ce = 1.0, ceK = 1.0;     // the circuit / noise / rattle envelope
    // rattle
    float  rattle = 0.0f;
    Svf    rHp;
    // stick
    Svf    sHp;
    double stE = 0.0, stK = 1.0, stPh = 0.0;
    float  stAmt = 0.0f;
    // chick
    Svf    chBp;
    double chE = 0.0, chK = 1.0, chAt2 = 0.0;
    float  chAmt = 0.0f;
    bool   ch2 = false;
    uint32_t rng = 1;
    float  choke = 1.0f, chokeStep = 0.0f;
    bool   isHatClosed = false;
    // the wash: above a few kHz a cymbal's modes are too dense to resolve
    Svf    wbL[kWash], wbR[kWash];
    float  wAmp[kWash] {}, wG[kWash] {}, wGa[kWash] {};
    double wE[kWash] {}, wK[kWash] {};
    int    nWash = 0;
    float  wCos = 1.0f, wSin = 0.0f;
};

class Engine
{
public:
    static constexpr int kAudible = 6, kSlots = 8;

    void prepare (double sampleRate, int maxBlock);
    void reset();

    void noteOn (int note, float velocity01);
    //  a forced articulation; strike < 0 keeps the dialled STRIKE (the pad
    //  passes where on the cymbal it was clicked)
    void noteOnArtic (int artic, float velocity01, float strike = -1.0f);
    void setTempo (double bpm);

    void process (const Params& p, float* left, float* right, int n);

    //  the decimators (23 host samples), the drive stage at 8x (8), and the
    //  compressor look-ahead (1.5 ms): constant whatever the settings
    static constexpr int kDecimatorLatency = 23, kDriveLatency = 8;
    int latencySamples() const { return kDecimatorLatency + kDriveLatency + laN; }
    double sampleRate() const { return fs; }

    float meterPeak() const  { return mPeak.load (std::memory_order_relaxed); }
    float meterGrDb() const  { return mGr.load (std::memory_order_relaxed); }
    int   hitCount() const   { return hits.load (std::memory_order_relaxed); }
    int   voicesOn() const;

    static void renderHit (const Params& p, double sampleRate, int artic, float vel,
                           std::vector<float>& left, std::vector<float>& right, int n);

    static float driveTrimFor (int engine, float drive);
    static int articFor (int keys, int note);

    //  what the bank would be for these settings: the lowest plate mode, and
    //  how many modes ring (the display and the bench ask)
    static float lowestModeHz (const Params& p);
    static int   modeCount (const Params& p);

    //  bench only: every voice renders a steady sine at this frequency instead
    //  of a cymbal, so the processing after the voice can be measured on a
    //  tone that does not move (latency, aliasing, grit, drive level)
    static inline float benchToneHz = 0.0f;
    //  bench only: no per-hit variation (random phases, amplitude jitter)
    static inline bool benchSteady = false;

private:
    void startVoice (int artic, int note, float vel, float strikeOverride);
    void renderOS (const Params& p, float* oL, float* oR);

    double fs = 48000.0, fo = 192000.0;
    Params P;
    std::array<Voice, kSlots> voices;
    uint32_t hitSeed = 0;

    // EQ
    Svf cutL, cutR, airL, airR;
    int eqTick = 0;

    double gritPh = 1.0;
    float  gritL = 0.0f, gritR = 0.0f;

    float tFA = 0, tSA = 0, tFR = 0, tSR = 0;
    float aFA = 0, aSA = 0, aRelA = 0, aFR = 0, aSR = 0;

    static constexpr int kLines = 8;
    std::array<std::vector<float>, kLines> line;
    std::array<int, kLines> len {}, pos {};
    std::array<float, kLines> dampZ {};
    std::array<std::vector<float>, 2> ap;
    std::array<int, 2> apLen {}, apPos {};
    bool roomLive = false;

    float driveSm = 0.0f, colourSm = 1.0f, roomSm = 0.0f;
    double adaaL = 0.0, adaaR = 0.0;
    Svf   colL, colR;
    int   coefTick = 0;

    HalfBand hb1L, hb2L, hb1R, hb2R, dnL, dnR;
    HalfUp   upL, upR;
    float    padL = 0.0f, padR = 0.0f;

    std::vector<float> dqVal;
    std::vector<long long> dqIdx;
    int dqMask = 0, holdN = 1;
    std::vector<float> laL, laR;
    int laN = 0, laPos = 0;
    long long dqHead = 0, dqTail = 0, cTick = 0;
    float compG = 0.0f;

    std::array<std::vector<float>, 2> eBuf;
    int eLen = 0, eW = 0;
    double bpm = 120.0, echoT = 0.375, echoTSm = 0.375, wowPh = 0.0;
    float eHpZ[2] {}, eLpZ[2] {}, echoSm = 0.0f;
    bool echoLive = false;

    bool  fresh = true;
    float levelSm = 1.0f;

    struct Pending { int artic; int note; float vel; float strike; };
    std::array<Pending, 8> pend {};
    int numPend = 0;

    std::atomic<float> mPeak { 0.0f }, mGr { 0.0f };
    std::atomic<int>   hits { 0 };
};

float driveShape (int engine, float x, float k);
float driveADAA  (int engine, float x, double xPrev, float k);
const char* driveEngineName (int engine);

//  the 808's six square-wave pitches, Hz
extern const float k808Hz[kSquares];

} // namespace ho
