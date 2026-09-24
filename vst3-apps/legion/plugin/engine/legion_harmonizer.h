#pragma once

// LEGION — the harmony bus.
//
// Four voices, one analysis. The expensive half of the work (FFT, true
// envelope, f0, peaks) depends only on the input, so it happens ONCE per
// hop and every voice re-draws from it (legion_shifter.h). Four-part
// harmony therefore costs about twice one voice, not four times.
//
// The engine hands back TWO buses: the harmony, and the dry delayed by
// exactly the analysis latency. Anything else and the second voice arrives
// 43 ms early relative to the singer, which reads as a slap, not a choir.
// The host mixes them, and puts the BWFX rack on whichever it likes.
//
// Real-time rules (house convention): process() never allocates, locks or
// logs. prepare() runs on the message thread and may allocate; every buffer
// for every window size is allocated there, so the DETAIL switch is a
// pointer change on the audio thread.

#include <atomic>
#include <vector>

#include "legion_analysis.h"
#include "legion_fft.h"
#include "legion_shifter.h"

namespace legion
{

constexpr int kVoices    = 4;
constexpr int kDetails   = 3;      // TIGHT / NATURAL / SMOOTH windows
constexpr float kMaxDelayMs = 120.0f;
constexpr int   kTapPad     = 2;   // samples of headroom the cubic delay read
                                   // needs at zero delay; carried by the dry
                                   // path too, so the buses stay aligned

struct VoiceParams
{
    bool  on        = false;
    float semitones = 0.0f;    // -24 .. +24
    float cents     = 0.0f;    // -100 .. +100
    float formant   = 0.0f;    // semitones, -12 .. +12, INDEPENDENT of pitch
    float follow    = 0.0f;    // 0..1, how much the tract rides the pitch
                               // (0 = the same body sings it, 1 = plain
                               //  resampling, the chipmunk)
    float gainDb    = 0.0f;    // -60 .. +6
    float pan       = 0.0f;    // -1 .. +1
    float delayMs   = 0.0f;    // 0 .. kMaxDelayMs
};

struct Params
{
    VoiceParams v[kVoices];
    float humanize = 0.0f;     // 0..1: slow detune drift, level shimmer and
                               // a touch of tract jitter, uncorrelated per
                               // voice. Four identical shifts are one voice
                               // that got louder; this is what makes them
                               // four singers.
};

class Harmonizer
{
public:
    void prepare (double sampleRate, int maxBlock);   // message thread
    void reset();                                      // audio thread safe

    // message thread. 0 TIGHT, 1 NATURAL, 2 SMOOTH. Adopted at the top of
    // the next block; latencySamples() changes with it, so the host must
    // re-report its latency.
    void setDetail (int d);
    int  detail() const     { return pendingDetail.load (std::memory_order_relaxed); }
    int  windowFor (int d) const;
    int  latencyFor (int d) const { return windowFor (d) + kTapPad; }
    int  latencySamples() const   { return latencyFor (detail()); }
    //  the delay the buses carry RIGHT NOW (audio thread; follows a DETAIL
    //  switch the moment process() adopts it, not when it is requested)
    int  activeLatency() const    { return N + kTapPad; }

    // audio thread. harm* is the harmony bus, dry* the input delayed to
    // match it. All four pointers must be distinct from the inputs.
    void process (const Params& p,
                  const float* inL, const float* inR, int n,
                  float* harmL, float* harmR, float* dryL, float* dryR);

    float lastF0() const { return f0Out.load (std::memory_order_relaxed); }

private:
    void runFrame (const Params& p, int writePos);

    double fs = 48000.0;
    int    maxBlock = 512;
    int    baseN = 2048, maxN = 4096;
    int    sizes[kDetails] {};

    std::atomic<int> pendingDetail { 1 };
    int  activeDetail = 1;
    int  N = 2048, hop = 512;

    Fft      fft;
    Analyser analyser;
    VoiceShifter shifter[kVoices];

    std::vector<float> window[kDetails];   // Hann, used for analysis AND
                                           // synthesis; sum of squares at
                                           // the 87.5 % hop is exactly 3
    float olaScale[kDetails] {};

    int ringSize = 0, ringMask = 0;
    std::vector<float> inRing, dryRingL, dryRingR;
    std::vector<float> olaRing[kVoices];

    int dlySize = 0, dlyMask = 0;
    std::vector<float> dlyRing[kVoices];
    int dlyPos = 0;

    std::vector<float> frameBuf, specBuf;

    int pos = 0, hopCount = 0;
    bool idle = true;

    //  per-voice smoothed control state (audio thread only)
    float gL[kVoices] {}, gR[kVoices] {};
    float dlyRead[kVoices] {};
    bool  glideInit = false;

    //  per-voice humanise state, updated once per frame
    float drift[kVoices] {}, shimmer[kVoices] {}, tract[kVoices] {};
    uint32_t rngState[kVoices] {};

    std::atomic<float> f0Out { 0.0f };
};

} // namespace legion
