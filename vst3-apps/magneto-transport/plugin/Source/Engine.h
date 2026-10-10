#pragma once
// Magneto Transport - engine, stage 1.
//
// The tape is a MEDIUM: an array of magnetisation per 0.5 um, per track. Heads
// read and write at physical positions; delay, pitch bends and Doppler are what
// happens, not formulas. See docs/physics/01-tape-medium.md and 02-transport-stage1.md.
//
// No JUCE in here, so the bench links it directly.

#include "Params.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace mt
{

constexpr double CELL_UM        = 0.5;                  // one tape cell
constexpr double CELLS_PER_CM   = 1.0e4 / CELL_UM;      // 20 000
constexpr double REF_FLUX       = 250.0;                // nWb/m rms at 0 VU
constexpr double REF_DBFS       = -18.0;                // 0 VU in the box
// nWb/m per unit of sample amplitude at the default record level
constexpr double FLUX_PER_UNIT  = 250.0 / 0.12589254117941673;   // 10^(-18/20)

constexpr double ERASE_CM       = 2.5;    // erase head, upstream of record
constexpr double SUPPLY_CM      = 5.0;    // where virgin tape enters (REEL)
constexpr double REEL_WINDOW_CM = 40.0;   // the readable stretch in REEL mode
constexpr double HEAD_MIN_CM    = 1.5;
constexpr double HEAD_MAX_CM    = 24.0;
constexpr double CARRIAGE_MAX_CMS = 12.0; // head carriage top speed
constexpr double HEAD_SPACING_CM = 1.2;  // head bodies: centres can come no closer
constexpr double MOTOR_TAU      = 0.35;   // s, stage 1 stand-in for the motor
constexpr double SPLICE_FADE_S  = 0.015;

constexpr double CAPSTAN_R_M    = 0.005;
constexpr double REEL_HUB_M     = 0.057;
constexpr double REEL_PACK_M    = 730.0;  // m of tape on a full 10.5" reel
constexpr double TAPE_THICK_M   = 50.0e-6;

constexpr int NUM_HEADS = 3;

//==============================================================================
/*  Biquad (RBJ shelves) for the feedback tone stage. */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    void shelf (bool high, double fs, double f0, double gainDb);
    inline double tick (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

/*  2x polyphase half-band, 63-tap windowed sinc - the fleet's, copied from
    Rite of Passage (from bwfx_dsp.h). */
struct HalfBand
{
    static constexpr int TAPS = 63, PH = 32, MASK = PH - 1;
    float ge[PH] {}, go[PH] {};
    float upLine[PH] {}, dnE[PH] {}, dnO[PH] {};
    int upW = 0, dnW = 0;
    void design();
    void clear();
    void up (float x, float& o0, float& o1);
    float down (float a, float b);
};

//==============================================================================
/*  The tape itself. Allocated on the message thread to the size the current
    transport needs and handed to the audio thread at a splice. */
struct TapeStore
{
    explicit TapeStore (int64_t cells) : n (cells), t0 ((size_t) cells, 0.0f), t1 ((size_t) cells, 0.0f) {}
    int64_t n;
    std::vector<float> t0, t1;
};

/*  What the panel (and, later, the mechanical sounds) read. Written by the audio
    thread, read anywhere; individual values are atomic, the set is not. */
struct Telemetry
{
    std::atomic<double> speedCms { 19.0 };
    std::atomic<double> targetCms { 19.0 };
    std::atomic<double> capstanAngle { 0.0 };     // rad
    std::atomic<double> supplyAngle { 0.0 }, takeupAngle { 0.0 };
    std::atomic<double> supplyR { 0.0 }, takeupR { 0.0 };   // m
    std::atomic<double> loopAngle { 0.0 };        // a LOOP guide wheel
    std::atomic<double> headCm[NUM_HEADS] { {4.0}, {8.0}, {12.0} };
    std::atomic<double> headVelCms[NUM_HEADS] { {0.0}, {0.0}, {0.0} };
    std::atomic<int>    mode { 0 };
    std::atomic<double> loopLenM { 2.0 };
    std::atomic<bool>   syncClamped { false };
    std::atomic<bool>   syncActive { false };
    std::atomic<bool>   splicing { false };
    std::atomic<float>  inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<float>  headPeak[NUM_HEADS] { {0.0f}, {0.0f}, {0.0f} };
    std::atomic<double> tapeOdometerM { 0.0 };    // m of tape played (REEL)
};

//==============================================================================
class Engine
{
public:
    Engine();
    ~Engine();

    /*  Message thread. Allocates the tape for the current params. */
    void prepare (double hostSampleRate, int maxBlock);
    void setParams (const Params& p) { params = p; }
    void setBpm (double b) { bpm = b; }

    /*  Audio thread. In-place allowed. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n);

    /*  Message thread (the processor's timer; the bench calls it directly).
        Allocates a fresh tape when a splice is waiting for one, and frees the
        tape the audio thread has retired. Never call from the audio thread. */
    void service();

    int latencySamples() const { return 0; }
    double internalRate() const { return fsInt; }
    int oversampling() const { return os; }

    /*  The magnetisation along the visible path, for the panel: n points from
        `fromCm` to `toCm` (path position, + = downstream of the record head),
        each the peak |flux| in its segment, in units of the reference level.
        Reads the tape without locking - a display may tear, audio never does. */
    void tapeView (float* out, int n, double fromCm, double toCm) const;

    Telemetry tele;

    // bench access
    double odometerCells() const { return T; }
    int64_t ringCells() const { return N; }
    double headPosCm (int i) const { return hPos[i]; }
    double speedCms() const { return v / CELLS_PER_CM; }
    double compensationS() const { return leadS; }
    bool isSplicing() const { return spliceState != 0; }

private:
    void tick (float xL, float xR, float& yL, float& yR);
    void separateHeads (const double* before);
    void adoptStore (TapeStore* s, int newMode, int64_t cells);
    int64_t cellsNeeded (int m, double loopM) const;
    double readAvg (const float* tr, double a, double b) const;
    double readPoint (const float* tr, double u) const;
    void writeSpan (float* tr, int track, double u0, double u1, double p0, double p1, double p2, double p3);
    double targetSpeedCms (bool& clamped, bool& syncOn) const;
    void clearSpan (double& prevPos, double newPos);

    inline int64_t wrapIndex (int64_t i) const
    {
        i %= N; if (i < 0) i += N; return i;
    }

    Params params;
    double bpm = 0.0;

    double fsHost = 48000.0, fsInt = 96000.0, dt = 1.0 / 96000.0;
    int os = 2;
    double leadS = 0.0;            // read-ahead that cancels the OUTPUT-side delays
    int    inLag = 0;              // input-side delay, internal samples (the up half-band)
    // The odometer, unwrapped, for the last 64 internal samples. The record
    // head writes each sample where the tape was when that sample ENTERED the
    // box, so input latency is paid at the speed the tape had then - not at
    // the speed it has when the echo is read (69 us wrong in a glide otherwise).
    static constexpr int HIST = 64;
    double Tlin = 0.0, Thist[HIST] {};
    uint32_t kTick = 0;
    double posAgo (int samples) const { return T - (Tlin - Thist[(kTick - (uint32_t) samples) & (HIST - 1)]); }

    // the tape
    TapeStore* store = nullptr;    // owned by the audio thread while adopted
    std::atomic<TapeStore*> pending { nullptr };   // fresh tape, message -> audio
    std::atomic<TapeStore*> retired { nullptr };   // used tape, audio -> message
    std::atomic<int64_t> wantCells { 0 };          // a splice waiting for a tape of this size
    std::atomic<TapeStore*> viewStore { nullptr }; // what the panel reads (message thread)
    std::atomic<double> viewT { 0.0 };
    float* tr0 = nullptr;
    float* tr1 = nullptr;
    int64_t N = 1;
    int mode = 0;
    double loopM = 2.0;

    // transport
    double T = 0.0;                // odometer, cells, kept in [0, N)
    double v = 0.0, vTarget = 0.0; // cells/s
    double hPos[NUM_HEADS] {}, hVel[NUM_HEADS] {};   // cm, cm/s
    double uPrev[NUM_HEADS] {};    // material each head was over last sample
    // The writer runs ONE sample late, so the record signal between two samples
    // can be reconstructed with a cubic through four of them.
    double wPrevU1 = 0.0;          // where the last write ended
    double sHist[2][3] {};         // record signal (flux) at k-3, k-2, k-1
    double recFade = 1.0;          // ramps the record input in after a splice
    double wAcc[2] { 0.0, 0.0 }, wLen[2] { 0.0, 0.0 };
    double ePos = 0.0;             // erase head
    double vPos = 0.0;             // supply (virgin) point
    double capAng = 0, supAng = 0, takAng = 0, loopAng = 0, playedM = 0;

    // splice
    int spliceState = 0;           // 0 none, 1 fading out, 2 waiting for tape, 3 fading in
    int spliceMode = 0;
    double spliceLoopM = 2.0;
    double pendingLoopM = 2.0, loopSettle = 0.0;
    double wetFade = 1.0;

    // smoothed controls (internal rate)
    double gIn = 1.0, gHead[NUM_HEADS] {}, panL[NUM_HEADS] {}, panR[NUM_HEADS] {};
    double fbEff = 0.0;
    double bassDb = 0.0, trebDb = 0.0;
    int toneTick = 0;
    Biquad lowS[2], highS[2];
    // The read integration is a box in time one internal sample wide, whatever
    // the speed; this 3-tap FIR undoes it (outL, outR, fbL, fbR).
    struct Comp3 { double a = 0, z1 = 0, z2 = 0;
                   double tick (double x) { const double y = -a * x + (1.0 + 2.0 * a) * z1 - a * z2; z2 = z1; z1 = x; return y; }
                   void reset() { z1 = z2 = 0; } };
    Comp3 comp[4];
    double aGain = 0.0, aMotor = 0.0;

    // host-rate smoothed mix
    double gDry = 1.0, gWet = 0.0, gOut = 1.0, aHost = 0.0;

    HalfBand up[2], dn[2];

    float pkIn = 0, pkOut = 0, pkHead[NUM_HEADS] {};
    int pkCount = 0;
};

const char* syncName (int i);

} // namespace mt
