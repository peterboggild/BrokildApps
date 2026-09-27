#pragma once

// LEGION — the LEVELLER. A vocal compressor that works from both ends: the
// loud parts come down (ratio above TOP), the quiet parts come UP (a gentle
// 2:1 lift toward TOP, capped at LIFT), and anything below FLOOR is left
// alone, so breaths, room and bleed between phrases are never pumped up.
//
// WHY IT CAN BE CLEAN. Legion already delays the singer by its analysis
// latency (N + kTapPad samples, 21-85 ms) to line up with the choir. The
// leveller listens to the UNDELAYED input and applies its gain to the
// DELAYED buses, so it gets a look-ahead of that latency for free — no added
// latency, no overshoot to catch, no clipper. And it is a gain multiply and
// nothing else: no saturation, no filter in the audio path (the 80 Hz high-
// pass lives in the detector only), stereo-linked so the image cannot move.
//
// The gain never wobbles inside a cycle. The detector is an RMS, and its
// residual ripple (twice the fundamental) is removed by a sliding MINIMUM of
// the gain over a 10 ms window — longer than the ripple period of any voice
// above 50 Hz, so a steady note gets a gain that is exactly constant. The
// minimum is then averaged over the same window, which turns every drop in
// gain into a straight ramp that finishes on time (the look-ahead limiter
// construction), and rises go through the release.
//
// OFF means off: the audio is not touched at all, not even multiplied by 1.
// Switching on or off fades the gain over ~15 ms.
//
// Real-time rules as the rest of the engine: prepare() allocates, process()
// never does.

#include <atomic>
#include <vector>

namespace legion
{

struct LevellerParams
{
    bool  on      = false;
    float topDb   = -20.0f;   // RMS dBFS where the downward compression starts
    float ratio   = 3.0f;     // above TOP (soft knee, 6 dB wide)
    float liftDb  = 6.0f;     // the most a quiet part is lifted
    float floorDb = -50.0f;   // below this nothing is lifted (12 dB fade in)
    float speed   = 0.5f;     // 0 slow (800 ms release) .. 1 fast (60 ms)
};

class Leveller
{
public:
    static constexpr float kWindowMs  = 10.0f;  // min-hold + ramp length
    static constexpr float kDetTauMs  = 2.5f;   // each of two detector poles
    static constexpr float kKneeDb    = 6.0f;
    static constexpr float kFloorFade = 12.0f;
    static constexpr float kUpRatio   = 2.0f;   // the lift's own ratio

    //  maxLatency: the largest bus delay process() will ever be told about
    void prepare (double sampleRate, int maxLatency);
    void reset();

    //  the look-ahead the gain pipeline itself needs; the bus latency should
    //  be at least this, and process() degrades gracefully if it is not
    int lookaheadNeeded() const { return W - 1 + lag; }

    //  inL/inR: the input as it arrives NOW. bufs: the buses to be levelled,
    //  already delayed by `latency` samples relative to that input. Gains
    //  are applied in place. Distinct pointers only.
    void process (const LevellerParams& p, const float* inL, const float* inR, int n,
                  int latency, float* const* bufs, int numBufs);

    //  the static curve: gain in dB for a steady RMS level in dBFS
    static float curveDb (const LevellerParams& p, float levelDb);

    //  what is being applied right now, dB (+ = lift, - = reduction)
    float gainDb() const { return meter.load (std::memory_order_relaxed); }

private:
    double fs = 48000.0;
    int W = 480, lag = 240;

    //  detector
    double hpB0 = 1, hpB1 = -2, hpB2 = 1, hpA1 = 0, hpA2 = 0;
    double hz1[2] {}, hz2[2] {};
    double e1 = 0, e2 = 0, aDet = 0;

    //  sliding minimum (monotonic deque)
    std::vector<float> dqVal;
    std::vector<long long> dqIdx;
    int dqMask = 0;
    long long dqHead = 0, dqTail = 0, tick = 0;

    //  release, then a boxcar of W
    float rel = 0.0f;
    std::vector<float> box;
    int boxPos = 0;
    double boxSum = 0.0;
    int boxRecalc = 0;

    //  the delay that lines the gain up with the buses
    std::vector<float> dly;
    int dlyMask = 0, dlyPos = 0;

    float amount = 0.0f;       // 0 off .. 1 on, faded
    float aAmt = 0.0f;
    std::atomic<float> meter { 0.0f };
};

} // namespace legion
