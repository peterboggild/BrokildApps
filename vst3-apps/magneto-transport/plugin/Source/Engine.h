#pragma once
// Magneto Transport - engine, stage 3.
//
// The tape is a MEDIUM: remanent magnetisation stored along it, in FOUR DEPTH
// LAYERS per track (1.75 / 5.25 / 8.75 / 12.25 um below the surface) at 0.5 /
// 1 / 2 / 4 um resolution - a deep layer cannot hold a short wavelength. Heads
// read and write at physical positions; delay, pitch bends and Doppler are what
// happens, not formulas.
//
// The record head is the stage 2 reference model in table form
// (Source/stock/stock_456.h, written by reference/mtref --tables): per layer,
// the signal field is averaged over the layer's recording zone (a Gaussian
// kernel downstream of the gap, measured), and the new remanence is
// G[A][M_old][s_zone] - so saturation, bias behaviour and what survives an
// overwrite are the reference's physics, not shapes chosen here.
//
// See docs/physics/01..05 and docs/DECISIONS.md. No JUCE in here.

#include "Params.h"
#include "Playback.h"
#include "Equaliser.h"
#include "Transport.h"
#include "Stock.h"
#include "Medium.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace mt
{

constexpr double CELL_UM        = 0.5;                  // one surface-layer cell
constexpr double CELLS_PER_CM   = 1.0e4 / CELL_UM;      // 20 000
constexpr int    NLAYER         = 4;                    // depth layers per track
constexpr double REF_FLUX       = 250.0;                // nWb/m rms at 0 VU
constexpr double REF_DBFS       = -18.0;                // 0 VU in the box
// nWb/m per unit of sample amplitude: -18 dBFS rms = 250 nWb/m rms
constexpr double FLUX_PER_UNIT  = 250.0 / 0.12589254117941673;
constexpr double REF_PEAK       = 0.12589254117941673 * 1.4142135623730951;   // 0 VU sine peak, sample units

constexpr double ERASE_CM       = 2.5;    // erase head, upstream of record
constexpr double SUPPLY_CM      = 5.0;    // where virgin tape enters (REEL)
constexpr double REEL_WINDOW_CM = 40.0;   // the readable stretch in REEL mode
constexpr double HEAD_MIN_CM    = 1.5;
constexpr double HEAD_MAX_CM    = 24.0;
constexpr double CARRIAGE_MAX_CMS = 12.0; // head carriage top speed
constexpr double HEAD_SPACING_CM = 1.2;   // head bodies: centres can come no closer
constexpr double SPLICE_FADE_S  = 0.015;
constexpr double FEEDBACK_MAX   = 1.5;    // small-signal loop gain at FEEDBACK 100 %, one head at 0 dB

constexpr double CAPSTAN_R_M    = 0.005;
constexpr double REEL_HUB_M     = 0.057;
constexpr double REEL_PACK_M    = 730.0;  // m of tape on a full 10.5" reel
constexpr double TAPE_THICK_M   = 50.0e-6;

constexpr int NUM_HEADS = 3;

//==============================================================================
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

/*  2x polyphase half-band, 63-tap windowed sinc - the fleet's. */
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
/*  The tape. Per track, per layer, a ring of remanence (M/Ms, 1 = Mr). Layer l
    has n0 >> l cells. Allocated on the message thread to the size the current
    transport needs and handed to the audio thread at a splice. */
struct TapeStore
{
    explicit TapeStore (int64_t baseCells);
    int64_t n0;                               // surface-layer cells (a multiple of 8)
    std::vector<float> m[2][NLAYER];
    int64_t nl (int l) const { return n0 >> l; }
};

/*  The record head for one layer, at the current bias: the G slice at this
    layer's bias amplitude, and the zone kernel in surface-cell units. */
struct LayerRecord
{
    static constexpr int MAXTAPS = 96;
    static constexpr int NSF = 1025;        // the slice resampled on a uniform s grid
    static constexpr double SMAX = 2.5;     // = the table's s range
    static constexpr int NSMAX = 81;
    float g[11][NSMAX] {};   // [M_old][s], antisymmetrised so G(0,0) = 0 exactly
    float gf[11][NSF] {};    // the same, Catmull-Rom resampled: s = -SMAX + i * 2 SMAX / (NSF - 1)
    double mLo = -0.95, dM = 0.19;
    int tapFirst = 0, nTaps = 1;            // taps relative to the cell's first surface cell
    float w[MAXTAPS] {};
    double gain = 0.0;                      // dG/ds at M_old = 0, s = 0
    double xUm = 0.0, wUm = 0.0, amp = 0.0; // zone centroid and rms, bias amplitude (Hc)
};

/*  What the panel (and, later, the mechanical sounds) read. */
struct Telemetry
{
    std::atomic<double> speedCms { 19.0 };
    std::atomic<double> targetCms { 19.0 };
    std::atomic<double> capstanAngle { 0.0 };
    std::atomic<double> supplyAngle { 0.0 }, takeupAngle { 0.0 };
    std::atomic<double> supplyR { 0.0 }, takeupR { 0.0 };
    std::atomic<double> loopAngle { 0.0 };
    std::atomic<double> headCm[NUM_HEADS] { {4.0}, {8.0}, {12.0} };
    std::atomic<double> headVelCms[NUM_HEADS] { {0.0}, {0.0}, {0.0} };
    std::atomic<int>    mode { 0 };
    std::atomic<double> loopLenM { 2.0 };
    std::atomic<bool>   syncClamped { false };
    std::atomic<bool>   syncActive { false };
    std::atomic<bool>   splicing { false };
    std::atomic<float>  inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<float>  headPeak[NUM_HEADS] { {0.0f}, {0.0f}, {0.0f} };
    std::atomic<float>  recPeak { 0.0f };      // peak |M| written, 1 = full remanence (tape saturation meter)
    std::atomic<double> tapeOdometerM { 0.0 };
    std::atomic<double> wowFlutter { 0.0 };    // weighted W&F, 2 sigma, % (DIN 45507 / IEC 60386); -1 while the motor settles
    std::atomic<double> wear { 0.0 };          // the tape's wear now, 0..1 (WEAR plus what a loop has accumulated)
    std::atomic<int>    stock { 0 };
};

//==============================================================================
class Engine
{
public:
    Engine();
    ~Engine();

    void prepare (double hostSampleRate, int maxBlock);   // message thread
    void setParams (const Params& p) { params = p; }
    void setBpm (double b) { bpm = b; }

    /*  Audio thread. In-place allowed. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n);

    /*  Message thread: allocates a tape for a waiting splice, frees retired ones. */
    void service();

    int latencySamples() const { return 0; }
    double internalRate() const { return fsInt; }
    int oversampling() const { return os; }

    void tapeView (float* out, int n, double fromCm, double toCm) const;

    Telemetry tele;

    // bench access
    double odometerCells() const { return T; }
    int64_t ringCells() const { return N; }
    double headPosCm (int i) const { return hPos[i]; }
    double speedCms() const { return v / CELLS_PER_CM; }
    double compensationS() const { return leadS; }
    int recordBuilds() const { return recBuilds; }
    /*  Bench: read the slabs with (true, the default) or without the playback
        head's spacing, thickness and gap losses - without, sections 13-14 test
        the record process alone against the reference's depth-averaged flux. */
    void setPlaybackLoss (bool on) { pbOn = on; }
    /*  Bench: the record and replay equalisers in (default) or out. Out, the
        transport's own timing and the head against the reference are tested;
        the equalisers are minimum-phase like the circuits, so they move the
        echo's phase as a real machine's do. */
    void setEq (bool on) { eqOn = on; }
    /*  The worst error of the equaliser fits, dB, over every speed and both standards. */
    static double eqFitWorstDb (int coating = -1);      // -1: every coating
    static double eqFitWorstSpeed (int coating = -1);   // where it is (cm/s)
    /*  Bench: the record and replay equaliser targets (dB re 1 kHz) at a frequency. */
    static void eqTargetDb (int coating, int standard, double vCms, double fHz, double& recDb, double& repDb);
    /*  Bench: the fitted cascade minus its target (dB), bucket b, record (true) or replay. */
    static double eqFitErrorDb (int coating, int standard, int bucket, double fHz, bool record);
    double playbackTrim() const { return pbTrim; }
    bool isSplicing() const { return spliceState != 0; }
    const LayerRecord& layerRecord (int l) const { return rec[l]; }
    double outputTrim() const { return outK; }
    double zoneRefUm() const { return xRefUm; }
    /*  Mean remanence over the four layers at path position cm (bench). */
    double layerM (int track, int layer, double pathCm) const;

    /*  ACCUMULATE's wear and the loop it belongs to (stock, length in m), saved with
        the project: the processor reads it for its state and hands it back on load. */
    void wearState (double& w, int& stockId, double& loopLen) const { w = wearAcc.load(); stockId = wearIdStock.load(); loopLen = wearIdLen.load(); }
    void setWearState (double w, int stockId, double loopLen) { wearAcc.store (w < 0.0 ? 0.0 : (w > 1.0 ? 1.0 : w)); wearIdStock.store (stockId); wearIdLen.store (loopLen); }
    /*  Bench: a perfect medium (no particle or modulation noise, a smooth surface, no
        dropouts) so every other claim is measured cleanly; section 19 turns it off. */
    void setIdealMedium (bool on) { idealMedium = on; }
    /*  Bench: ACCUMULATE's wear per pass scaled, so it shows within seconds. */
    void setWearRateScale (double s) { wearScale = s; }
    int stockIndex() const { return stockIdx; }
    double noiseSigma (int layer) const { return noiseSig[layer]; }
    const double* noiseColourOf (int layer) const { return gCol[layer]; }

private:
    void tick (float xL, float xR, float& yL, float& yR);
    void separateHeads (const double* before);
    void adoptStore (TapeStore* s, int newMode, int64_t cells);
    int64_t cellsNeeded (int m, double loopM) const;
    double targetSpeedCms (bool& clamped, bool& syncOn) const;
    void buildRecord (double biasDb, double speedCms);
    void readLayers (double a, double b, double& outL, double& outR) const;
    void readHead (double a, double b, double& outL, double& outR) const;
    void edgeTerms (double xum, double* e) const;   // bump L/R, fringing L/R (depth sums)
    void finishCoarse();
    double headResponse (int slab, double kPerUm, int bucket) const;
    void readAvg2 (const float* t0, const float* t1, int64_t ring, double a, double b, double& r0, double& r1) const;
    double readPoint (const float* tr, int64_t ring, double u) const;
    void writeField (int c, double u0, double u1, double p0, double p1, double p2, double p3);
    void finishCells();
    void clearSpan (double& prevPos, double newPos);
    float recordCell (const LayerRecord& r, float mOld, double s) const;
    void selectStock (int s);
    void applyStock (int s);
    void updateMedium (const Params& p, int n);
    void updateShaper (int i, double x, double vus);
    double epsAt (int l, double X);
    double epsGrid (int64_t j) const;
    // both tracks' draws for cell k of layer l from one hash: two 32-bit uniforms, unit variance
    inline void unitNoise2 (int64_t k, int l, double& a, double& b) const
    {
        const uint64_t h = mix64 ((uint64_t) k ^ salt[l]);
        constexpr double S = 3.4641016151377544 / 4294967296.0;
        a = ((double) (uint32_t) h + 0.5) * S - 1.7320508075688772;
        b = ((double) (uint32_t) (h >> 32) + 0.5) * S - 1.7320508075688772;
    }

    static inline int64_t wrapTo (int64_t i, int64_t ring) { i %= ring; if (i < 0) i += ring; return i; }

    Params params;
    double bpm = 0.0;

    double fsHost = 48000.0, fsInt = 96000.0, dt = 1.0 / 96000.0;
    int os = 2;
    double leadS = 0.0;            // read-ahead that cancels the OUTPUT-side delays
    int    inLag = 0;              // input-side delay, internal samples (the up half-band)

    // The odometer for the last 64 internal samples: the record head writes each
    // sample where the tape was when that sample ENTERED the box.
    static constexpr int HIST = 64;
    double Thist[HIST] {};
    uint32_t kTick = 0;
    double posAgo (int samples) const { return Thist[(kTick - (uint32_t) samples) & (HIST - 1)]; }

    // the tape. T is UNWRAPPED (cells of tape passed since the splice); every ring
    // index is taken modulo its own ring, and because n0 is a multiple of 8, a
    // layer's ring is an exact fraction of the surface ring - one coordinate for all.
    TapeStore* store = nullptr;
    std::atomic<TapeStore*> pending { nullptr };
    std::atomic<TapeStore*> retired { nullptr };
    std::atomic<int64_t> wantCells { 0 };
    std::atomic<TapeStore*> viewStore { nullptr };
    std::atomic<double> viewT { 0.0 };
    int64_t N = 8;
    int mode = 0;
    double loopM = 2.0;

    // the record head: the field ring (surface-cell resolution, the last few mm)
    static constexpr int FIELD = 8192, FMASK = FIELD - 1;
    // On the heap: the engine lives on a stack in the bench and wherever a host
    // puts it; 250 KB of tables in the object overflowed a 1 MB stack (exit 127).
    std::unique_ptr<float[]> fieldBuf { new float[2 * FIELD]() };
    float* field[2] { fieldBuf.get(), fieldBuf.get() + FIELD };
    int64_t nextCell[NLAYER] {};   // next unfinished cell of each layer (unwrapped)
    std::unique_ptr<LayerRecord[]> recBuf { new LayerRecord[NLAYER]() };
    LayerRecord* rec = recBuf.get();
    double recBiasDb = 1.0e9;      // bias the record tables are built for
    double recSpeed = 0.0;         // and the tape speed (cm/s): the tables have a speed axis
    int recBuilds = 0;             // how often the record head has been rebuilt (a rebuild storm costs CPU)
    double biasDbSm = 0.0;
    double sigScale = 0.0;         // deep-gap field (Hc) per sample unit at the record head
    double outK = 0.0;             // sample units per unit of mean remanence (the repro alignment)
    double xRefUm = 0.0;           // gain-weighted zone centroid at operating bias
    float recPk = 0.0f;

    // the playback head (stage 4): kernel tables shared per internal rate, the
    // speed bucket in use, and coarse prefix sums of every slab for the tails -
    // a 52 cm ring indexed by the UNWRAPPED coarse coordinate, so it covers what
    // any head can read whatever the loop length
    const HeadKernel* hk = nullptr;
    int hkB = 0;
    bool pbOn = true;
    double pbTrim = 1.0;           // repro level per speed: 1 kHz unity through the head losses
    double pbTrimSS = 1.0;         // the same from small-signal layer weights
    double pbKappa = 1.0;          // 0 VU weights / small-signal weights, measured at the alignment

    // record and replay equalisation: per standard and speed bucket, discretised
    // for this rate (on the heap - the engine may live on a small stack)
    bool eqOn = true;
    int eqS = 0;
    std::unique_ptr<ShelfCascade[]> eqBuf { new ShelfCascade[(size_t) NUM_COATINGS * 4 * HeadKernel::NB] };
    const ShelfCascade& eqRec (int s, int b) const { return eqBuf[(size_t) ((coat * 4 + s) * HeadKernel::NB + b)]; }
    const ShelfCascade& eqRep (int s, int b) const { return eqBuf[(size_t) ((coat * 4 + 2 + s) * HeadKernel::NB + b)]; }
    ShelfState recEqS[2], repEqS[4];

    // the transport (stage 5): motor, wow and flutter from physical causes
    Transport tr;
    TransportInput trIn;
    // the W&F meter: tape speed at 1 kHz, the weighting filter, the 2-sigma detector
    static constexpr int WF_HIST = 8192, WF_MASK = WF_HIST - 1;
    std::unique_ptr<double[]> wfHist { new double[WF_HIST]() };
    int wfW = 0, wfDiv = 96, wfCnt = 0;
    double wfB0 = 0, wfB2 = 0, wfA1 = 0, wfA2 = 0, wfZ1 = 0, wfZ2 = 0;
    double wfMs = 0.0, wfA = 0.0, wfPct = 0.0;
    int wfSettle = 0; double wfN = 0.0;   // a meter reads only once the motor is at speed
    // the cascades in use: analogue gains interpolated between the two speed
    // buckets either side of the motor speed, rediscretised as it moves
    ShelfCascade eqRecA, eqRepA;
    double eqLastFb = -1.0;
    int eqLastS = -1, eqLastC = -1;
    void updateEq (double vCms);
    std::unique_ptr<double[]> preBuf { new double[(size_t) 2 * NLAYER * PRE_RING]() };
    double* pre (int c, int l) const { return preBuf.get() + (size_t) (c * NLAYER + l) * PRE_RING; }
    double pRun[2][NLAYER] {};
    int64_t cNext[NLAYER] {};
    std::unique_ptr<double[]> preCBuf { new double[(size_t) 2 * PRE_RING]() };   // the combined tail track
    double* preC (int c) const { return preCBuf.get() + (size_t) c * PRE_RING; }
    double pRunC[2] {};
    int64_t cNextC = 0;
    std::unique_ptr<double[]> preSBuf { new double[(size_t) 2 * PRE_RING]() };   // the depth sum, for the head's edges
    double* preS (int c) const { return preSBuf.get() + (size_t) c * PRE_RING; }
    double pRunS[2] {};
    // the head bump and the fringing carry nothing above a few hundred hertz, so
    // they are evaluated every EDGE_EVERY internal samples at the position the
    // head will then have reached, and interpolated in between (no delay)
    static constexpr int EDGE_EVERY = 8;
    double edgePrev[NUM_HEADS][4] {}, edgeNext[NUM_HEADS][4] {};


    // transport
    double T = 0.0;
    double v = 0.0, vTarget = 0.0; // cells/s
    double hPos[NUM_HEADS] {}, hVel[NUM_HEADS] {};
    double uPrev[NUM_HEADS] {};
    double wPrevU1 = 0.0;
    double sHist[2][3] {};
    double recFade = 1.0;
    double wAcc[2] { 0.0, 0.0 }, wLen[2] { 0.0, 0.0 };
    double ePos = 0.0, vPos = 0.0;
    double capAng = 0, supAng = 0, takAng = 0, loopAng = 0, playedM = 0;

    // splice
    int spliceState = 0;
    int spliceMode = 0;
    double spliceLoopM = 2.0;
    double pendingLoopM = 2.0, loopSettle = 0.0;
    double wetFade = 1.0;

    // smoothed controls
    double gIn = 1.0, gHead[NUM_HEADS] {}, panL[NUM_HEADS] {}, panR[NUM_HEADS] {};
    double fbEff = 0.0;
    double bassDb = 0.0, trebDb = 0.0;
    int toneTick = 0;
    Biquad lowS[2], highS[2];
    struct Comp3 { double a = 0, z1 = 0, z2 = 0;
                   double tick (double x) { const double y = -a * x + (1.0 + 2.0 * a) * z1 - a * z2; z2 = z1; z1 = x; return y; }
                   void reset() { z1 = z2 = 0; } };
    Comp3 comp[4];
    double aGain = 0.0;
    double gDry = 1.0, gWet = 0.0, gOut = 1.0, aHost = 0.0;

    HalfBand up[2], dn[2];

    float pkIn = 0, pkOut = 0, pkHead[NUM_HEADS] {};

    // the stock (stage 6): the coating's tables and alignment, the medium
    int stockIdx = 0, coat = 0, spliceStock = 0;
    const CoatingTables* ST = &coatingTables (0);
    const StockMedium* SM = &stockMedium (0);
    const HeadKernel* hkC[NUM_COATINGS] {};
    double outKs[NUM_STOCKS] {}, pbKappaS[NUM_STOCKS] {};
    uint64_t seed = 0, salt[NLAYER] {};
    bool idealMedium = false;
    double om20 = 1.0;           // 20 kHz (or 0.45 fs) in radians per internal sample: what the shaper must get right
    double wearScale = 1.0;
    std::atomic<double> wearAcc { 0.0 };
    std::atomic<int> wearIdStock { -1 };
    std::atomic<double> wearIdLen { 0.0 };
    int64_t lastPass = 0;
    double wearEff = 0.0, packSm = 1.0, packTgt = 1.0;
    // particle noise per layer: sigma, colour, and the last five draws per track
    double noiseSig[NLAYER] {};
    double gCol[NLAYER][3] { { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 } };
    double nq[2][NLAYER][5] {};
    int64_t nqK[NLAYER] { INT64_MIN, INT64_MIN, INT64_MIN, INT64_MIN };
    // modulation noise per layer: a cached stretch of the density variation
    double modEff = 0.0;
    int64_t epsJ[NLAYER] { INT64_MIN, INT64_MIN, INT64_MIN, INT64_MIN };
    double epsA[NLAYER] {}, epsB[NLAYER] {};
    // at the heads
    HeadShaper shaper[NUM_HEADS];
    bool shOff[NUM_HEADS] {};
    DropoutCache drop[NUM_HEADS][2];
    double spacingStatic = 0.0, roughEff = 0.0, dropRate = 0.0, dropDepth = 0.0, holeEff = 0.0;
    double azim[NUM_HEADS] {};   // net azimuth error per playback head (rad)
    double weave = 0.0;          // the tape's weave amplitude (rad)
};

const char* syncName (int i);

} // namespace mt
