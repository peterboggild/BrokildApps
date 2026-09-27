#pragma once

/*  BRAIN SCAN — the engine.

    STORE A VOLUME, NOT A WAVEFORM, AND READ IT ALONG A LINE. A specimen is a
    scalar field V(x, y, z) on the unit cube (64^3, built by code, see
    Specimens.cpp). A scan line is a Catmull-Rom curve through it, open or
    closed. One cycle of the waveform is the field along the curve,
    w(s) = V(g(s)), s the phase. The FILTER's cutoff and a free MODulator are
    the same thing read slower: a second and a third line, traversed once per
    note (an envelope) or looping (an LFO).

    THE SCAN blends the GEOMETRY of two anchor lines, A and B, and only then
    reads the field: g(s) = (1 - scan) A(s) + scan B(s). A wavetable crossfades
    two answers; this moves the question, and the midway waveform is read from
    tissue neither anchor visited. The bench proves the difference (item 5).

    Three traps, each measured: a mip pyramid of the volume per note (the path
    length in texels times the pitch decides the level), a polyBLEP over the
    wrap of an open line (the jump between its ends is known per tick), and a
    DC blocker after the sum (the field's mean along the line moves with every
    scan).

    Design: BrokildApps/BRAIN-SCAN-DESIGN.md. The bench: test/bench.cpp.
*/

#include <atomic>
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <thread>

namespace bs
{

//==============================================================================
/*  128 texels across, not 64. A straight line across the cube at 64 carried
    at most 32 harmonics, and the smoothing read took the top octave of those
    down 4-16 dB — Peter heard "fairly sinusy", and he was right. 128 doubles
    the harmonic budget everywhere below A4 and costs nothing per sample (a
    tricubic read is 64 taps whatever the resolution). */
constexpr int VN        = 128;           // volume side, level 0
constexpr int NLOD      = 5;             // 128, 64, 32, 16, 8
constexpr int MAXVOICES = 8;
constexpr int MAXUNI    = 4;
constexpr int MAXPTS    = 16;
constexpr int NLINES    = 6;             // WAVE A/B, FILTER A/B, MOD A/B
constexpr int TICK      = 32;            // control tick, samples
constexpr int SCOPE_N   = 256;
constexpr double PI     = 3.14159265358979323846;

enum LineId { L_WAVE_A = 0, L_WAVE_B, L_FILT_A, L_FILT_B, L_MOD_A, L_MOD_B };

/*  The volume slot a specimen index lives in also has to name "the imported
    one", and it must not collide with 0..8 or with the -1 that means empty. */
constexpr int SPEC_IMPORTED = -2;

//==============================================================================
enum PKind { KP_PCT = 0, KP_INT, KP_LIST, KP_BIPOL, KP_HZ, KP_VOL, KP_SEMI, KP_SEC, KP_CENT, KP_RATIO };
//  KP_RATIO: 2^((v - 0.5) * 4), i.e. 0.25x .. 4x with exactly 1x in the middle
inline double ratioOf (float v) { return std::pow (2.0, ((double) v - 0.5) * 4.0); }

struct Params
{
    float level    = 0.70f;
    float voiceMode = 0.0f;                 // POLY / MONO
    float unison   = 0.0f, detune = 0.25f, spread = 0.50f, glide = 0.0f, tune = 0.5f;
    float specimen = 1.0f / 14.0f;          // list index (SPINE by default; fifteen slots)
    /*  THE READ. GRAIN is the interpolation kernel — 0 a smoothing B-spline,
        1 an interpolating Catmull-Rom that passes the texels as they are.
        CONTRAST is the CT window applied to the AUDIO: narrow it and the
        read saturates against the window's edges, a sine toward a square.
        FOLD is what the window does to values beyond its edges — clip them,
        or fold them back in. Both are anti-derivative anti-aliased. */
    float grain    = 0.35f, contrast = 0.0f, fold = 0.0f;
    /*  THE SECOND HEAD (260905.1): a second reader on the same blended line,
        at a RATIO of the note's speed (1x in the middle: a fixed-interval
        double when PHASE is offset) or off the integers (a partial that is
        not a harmonic — the one thing a single-cycle read cannot be). */
    float head2    = 0.0f, head2Ratio = 0.5f, head2Phase = 0.0f;
    float uniScan  = 0.0f;                                   // unison readers spread through the scan
    float modContrast = 0.5f, modGrain = 0.5f;               // bipolar: the MOD line drives the read
    float scan     = 0.0f, scanA = 0.30f, scanD = 0.55f, scanAmt = 0.5f;   // amt bipolar
    float filtType = 0.0f;                  // LP / BP / HP / OFF
    float cutoff   = 0.70f, reso = 0.20f, filtDepth = 0.0f;
    float filtMode = 0.0f;                  // ENV / LOOP
    float filtRate = 0.35f, filtSync = 0.0f, filtTrack = 0.5f;
    /*  THE CIRCUIT (260905.2): the state-variable filter, or one of Black
        Rider's three — GROWL (Sallen-Key with a diode clipper in the
        feedback), SCREAM (the same circuit, self-oscillating from two o'clock),
        LADDER (four-pole transistor ladder) — each as a lowpass or a highpass.
        BAND stays the SVF's. Default SVF, so every existing patch is untouched. */
    float filtModel = 0.0f;
    float modRate  = 0.30f, modSync = 0.0f;
    float modScan  = 0.5f, modPitch = 0.5f, modPan = 0.5f;   // bipolar, 0.5 = none
    float ampA = 0.15f, ampD = 0.55f, ampS = 0.75f, ampR = 0.45f, velSens = 0.6f;
};

struct PSpec
{
    const char* id;
    const char* name;
    const char* gloss;
    float def;
    int   kind;
    float lo, hi;
    float& (*get)(Params&);
    const char* const* list;
    int   nlist;
};

int          numParams();
const PSpec& paramSpec (int i);
int          paramIndex (const char* id);
float        secondsOf (float v);            // KP_SEC map
float        hzOf (const PSpec& s, float v); // KP_HZ map
int          listIndex (const PSpec& s, float v);

//==============================================================================
struct Vec3 { float x = 0.5f, y = 0.5f, z = 0.5f; };

/*  A scan line: up to MAXPTS control points, Catmull-Rom through them, open
    or closed, a START mark (where the cycle begins on a loop) and a WARP
    (phase distortion, -1..1: the read runs faster over one half). */
struct Line
{
    Vec3  p[MAXPTS];
    int   n = 2;
    bool  closed = false;
    float start = 0.0f;
    float warp = 0.0f;
    /*  A line that is not one curve: split > 0 makes points [0, split) one
        open segment and [split, n) another, read in the two halves of the
        cycle — one cycle, TWO edges, a different family of timbre. Valid
        when both halves have at least two points. */
    int   split = 0;

    bool  isSplit() const { return split >= 2 && split <= n - 2; }
    bool  hasWrapEdge() const { return ! closed || isSplit(); }
    Vec3  at (float s) const;                       // s in [0,1)
    float length (int steps = 48) const;            // polyline length in unit-cube units
    static Line straight (Vec3 a, Vec3 b);
    static Line circle (Vec3 c, float r, int axis, int n = 8);   // axis = normal (0 x, 1 y, 2 z)
    static Line helix (Vec3 c, float r, float rise, float turns, int n = 12);
    static Line walk (uint32_t seed, int n, bool closed, float step = 0.18f);
};

//==============================================================================
/*  The volume and its mip pyramid. Level l is a 2x2x2 box average of level
    l-1; reads are tricubic B-spline (C2, smoothing), coordinates in [0,1]. */
struct Volume
{
    std::vector<float> lod[NLOD];
    int side[NLOD] { VN, VN / 2, VN / 4, VN / 8, VN / 16 };
    int specimen = -1;
    /*  The phase axis WRAPS for a specimen that is periodic in x (a sine, a
        harmonic stack, a pulse): a read at x = 1 is a read at x = 0, so a
        straight line across the cube is exactly one cycle with no edge.
        Measured before this: a "sine" with -21 dB THD, all of it the cube's
        edge clamped into the B-spline's support. A specimen that is NOT
        periodic (the ramp, the formants, the brain) clamps instead, and its
        wrap is a real edge the polyBLEP handles. */
    bool periodicX = false;

    void  build (const float* level0, bool periodic);   // VN^3 floats in [0,1]
    /*  The read kernel is the Mitchell-Netravali cubic family (B, C): B-spline
        at (1, 0), Catmull-Rom at (0, 1/2). GRAIN walks from the first to the
        second. The default is the smoothing B-spline the formula checks use. */
    float sample (int level, float x, float y, float z, float B = 1.0f, float C = 0.0f) const;
    float sampleBlend (float lodF, float x, float y, float z, float B = 1.0f, float C = 0.0f) const;
    int   levelWithSide (int n) const { for (int l = 0; l < NLOD; ++l) if (side[l] == n) return l; return 0; }
    float at (int i, int j, int k) const { return lod[0][((size_t) k * VN + (size_t) j) * VN + (size_t) i]; }
};

//==============================================================================
/*  What the panel is shown, thirty times a second. */
//==============================================================================
//  the two circuits from Black Rider, per reader
inline float ftanh (float x)
{
    x = x < -4.97f ? -4.97f : (x > 4.97f ? 4.97f : x);
    const float x2 = x * x;
    const float p = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float q = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return p / q;
}

/*  MS-20 style two-pole Sallen-Key: two one-pole TPT stages with feedback of
    K through a one-pole of the opposite kind and a clipper in the feedback
    path, solved as a zero-delay loop for the linear part then refined twice
    for the clipper. Q = 1/(2-K); K = 2 is the edge of self-oscillation and the
    clipper is what keeps it there. Lowpass and highpass are the same circuit
    with the stages swapped. (Black Rider's Korg35, verbatim.) */
struct Korg35
{
    float g = 0.1f, G = 0.1f;
    float s1 = 0, s2 = 0, s3 = 0;
    float sat = 1.0f;
    void reset() { s1 = s2 = s3 = 0.0f; }
    void setG (float gg) { g = gg; G = g / (1.0f + g); }
    inline float clip (float x) const
    {
        const float t = x > 0.0f ? x : x * 1.12f;
        const float y = sat * ftanh (t / sat);
        return x > 0.0f ? y : y / 1.12f;
    }
    inline float lowpass (float x, float K)
    {
        const float v1 = (x - s1) * G; const float a = v1 + s1; s1 = a + v1;
        const float oneG = 1.0f - G;
        const float den = 1.0f / (1.0f - G * K * oneG);
        float y = (G * a + oneG * s2 - G * K * oneG * s3) * den;
        float u = a;
        for (int it = 0; it < 2; ++it)
        {
            const float hp = oneG * (y - s3);
            const float fb = clip (K * hp);
            u = a + fb;
            y = G * u + oneG * s2;
        }
        const float v2 = (u - s2) * G; s2 = y + v2;
        const float vh = (y - s3) * G; const float lp = vh + s3; s3 = lp + vh;
        return y;
    }
    inline float highpass (float x, float K)
    {
        const float oneG = 1.0f - G;
        const float v1 = (x - s1) * G; const float l1 = v1 + s1; s1 = l1 + v1; const float a = x - l1;
        const float den = 1.0f / (1.0f - K * G * oneG);
        float y = oneG * (a - s2 + K * oneG * s3) * den;
        float u = a;
        for (int it = 0; it < 2; ++it)
        {
            const float lp = G * y + oneG * s3;
            const float fb = clip (K * lp);
            u = a + fb;
            y = oneG * (u - s2);
        }
        const float v2 = (u - s2) * G; s2 = s2 + 2.0f * v2;
        const float vl = (y - s3) * G; const float l3 = vl + s3; s3 = l3 + vl;
        return y;
    }
};

/*  Moog style transistor ladder: four trapezoidal one-poles, the loop solved
    exactly for the linear case, then the differential pair at the summing
    node applied as a tanh, a gentle cubic on each stage. The highpass is the
    same loop with highpass stages — h = (1-G)(u - s) — solved the same way:
    h4 = ((1-G)^4 x - S) / (1 + k (1-G)^4). (Black Rider's Ladder, plus HP.) */
struct Ladder
{
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    float G = 0.1f;
    void reset() { s1 = s2 = s3 = s4 = 0.0f; }
    void setG (float g) { G = g / (1.0f + g); }
    inline float lowpass (float x, float k)
    {
        const float G2 = G * G, G4 = G2 * G2, oneG = 1.0f - G;
        const float S = G2 * G * oneG * s1 + G2 * oneG * s2 + G * oneG * s3 + oneG * s4;
        const float y4lin = (G4 * x + S) / (1.0f + k * G4);
        float u = x - k * y4lin;
        u = 1.3f * ftanh (u * (1.0f / 1.3f));
        auto stage = [this] (float in, float& s) { const float v = G * (in - s); const float y = v + s; s = y + v; return y; };
        auto soft = [] (float y) { return y * (1.0f - std::min (0.3f, y * y * 0.025f)); };
        const float y1 = soft (stage (u,  s1));
        const float y2 = soft (stage (y1, s2));
        const float y3 = soft (stage (y2, s3));
        const float y4 = stage (y3, s4);
        return y4;
    }
    inline float highpass (float x, float k)
    {
        const float oneG = 1.0f - G, o2 = oneG * oneG, o3 = o2 * oneG, o4 = o2 * o2;
        const float S = o4 * s1 + o3 * s2 + o2 * s3 + oneG * s4;
        const float h4lin = (o4 * x - S) / (1.0f + k * o4);
        float u = x - k * h4lin;
        u = 1.3f * ftanh (u * (1.0f / 1.3f));
        auto stage = [this] (float in, float& s) { const float v = G * (in - s); const float y = v + s; s = y + v; return in - y; };
        auto soft = [] (float y) { return y * (1.0f - std::min (0.3f, y * y * 0.025f)); };
        const float h1 = soft (stage (u,  s1));
        const float h2 = soft (stage (h1, s2));
        const float h3 = soft (stage (h2, s3));
        const float h4 = stage (h3, s4);
        return h4;
    }
};

struct VoiceView
{
    int   id = 0, note = 0, held = 0;
    float scan = 0, cutoff = 0, fPhase = 0, mPhase = 0, mod = 0, lvl = 0;
};

//==============================================================================
class Engine
{
public:
    Engine();
    ~Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setTransport (double bpm, double ppq, bool playing);
    void process (float* L, float* R, int n);
    /*  Message-thread work: build the specimen the parameter asks for (a
        volume is two million texels of noise — never on the audio thread)
        and publish it. service() builds synchronously (the bench, prepare);
        serviceAsync() builds on a worker and publishes on a later call, so a
        specimen change does not freeze the panel. */
    void service();
    void serviceAsync();
    bool building() const { return buildBusy.load(); }

    void noteOn (int note, float vel);
    void noteOff (int note);
    void allNotesOff();
    void setBend (float semis);
    void setSustain (bool on);
    void setModWheel (float v01) { modWheel = v01; }

    //  SPECTRA world-mod bus (neutral in, bit-identical out)
    void setWorldMod (float detCents, float sag01, float tremDepth,
                      float tremRate, float filterMul, float panSpread);

    /*  An imported volume overrides the specimen dial until it is cleared.
        VN^3 floats in [0,1]; message thread only. */
    void setImported (const float* cube);
    void clearImported();
    bool importedActive() const { return importOn; }

    //  the lines — message thread writes, audio thread reads at a block edge
    void setLine (int which, const Line& l);
    const Line& line (int which) const { return lineSet[which]; }
    void setLines (const Line* six);

    //  the volume the audio thread is reading (message thread may look)
    const Volume& volume() const { return vols[volCur.load()]; }
    int   specimenLoaded() const { return vols[volCur.load()].specimen; }

    //  the view
    int   voicesView (VoiceView* out, int maxOut);
    void  scopeView (float* wave256, float* filt256, float* mod256);
    float outLevel() const { return outLvl.load (std::memory_order_relaxed); }

    Params p;

    //  bench accessors (nothing on the audio path depends on them)
    int    activeVoices() const;
    float  voiceCutoff (int v) const;
    float  voiceCutTarget (int v) const;
    float  voiceScan (int v) const;
    int    voiceLod (int v) const;
    double engineTime() const { return tNow; }
    void   setBlep (bool on) { blepOn = on; }            // bench: the polyBLEP's worth
    void   setMipLock (int level) { mipLock = level; }   // bench: -1 = automatic
    static float readLine (const Volume& v, const Line& a, const Line& b, float scan, float s, int level);

private:
    //==========================================================================
    struct Env
    {
        int   stage = 0;             // 0 idle, 1 attack, 2 decay, 3 sustain, 4 release
        float y = 0;
        void  gate (bool on) { stage = on ? 1 : (stage == 0 ? 0 : 4); }
        float step (float a, float d, float s, float r, float dt);
    };

    struct Svf
    {
        float ic1 = 0, ic2 = 0;
        float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
        void  set (float fcHz, float res01, double sr);
        inline float run (float in, int type);
        void  reset() { ic1 = ic2 = 0; }
    };

    struct Uni
    {
        double ph = 0;
        double detMul = 1.0;
        float  gl = 0.7071f, gr = 0.7071f;
        Svf    svf;
        float  edge = 0;             // the wrap jump for the polyBLEP, per tick
        float  edge2 = 0;            // the jump at phase 0.5 of a SPLIT line
        float  shX = 0;              // the window shaper's previous input (ADAA)
        double ph2 = 0;              // the second head's own phase
        Korg35 k35;                  // the Sallen-Key circuit (GROWL, SCREAM)
        Ladder lad;                  // the transistor ladder
    };

    struct Voice
    {
        bool   active = false, held = false, sustained = false, released = false;
        int    id = 0, note = -1;
        float  vel = 0;
        double age = 0, ageOff = 0;
        double fHz = 55, fGlideFrom = 55, glideT = 0, glideLen = 0;
        Uni    u[MAXUNI];
        int    nUni = 1;
        float  lodF = 0;             // level of detail this note reads at (slewed)
        float  lodTarget = 0;        // the level the pitch and the path length ask for
        float  cutLog = 10.0f;       // the cutoff smoothed in OCTAVES, not hertz
        float  scanNow = 0, scanPrev = 0;      // per tick, ramped per sample
        float  scanEnvT = 0;
        float  fPhase = 0, mPhase = 0;
        float  cutHz = 1000, cutTarget = 1000;
        float  modV = 0;
        float  panMod = 0;
        Env    amp;
        float  gateSm = 0;
        float  pitchMod = 0;         // semis from MOD
        uint32_t seed = 1;
        float  lastWave = 0;         // for the scope
        float  tremPh = 0;
        int    lastTickAbs = -1;
        //  the read, per voice: the MOD line may move the kernel and the window
        float  kB = 1.0f, kC = 0.0f;
        float  shGain = 1.0f;
        bool   shOn = false;
        //  the circuit, per voice: its prewarped coefficient and its feedback K
        float  fG = 0.1f, fK = 0.0f;
    };

    void   tick (Voice& v, double dt, double bpm, double ppq, bool playing);
    void   renderVoice (Voice& v, float* L, float* R, int n, double dt);
    void   startVoice (Voice& v, int note, float vel, bool retrigger, bool glide);
    int    allocVoice (int note);
    void   pullLines();
    double noteHz (int note) const;
    void   captureView();
    float  scanValue (Voice& v) const;

    //==========================================================================
    Volume vols[2];
    std::atomic<int> volCur { 0 };
    int    volBuilding = -1;
    std::vector<float> importCube;          // VN^3, message thread owns it
    bool   importOn = false, importDirty = false;

    //  the worker that builds a volume for serviceAsync()
    std::thread builder;
    std::atomic<bool> buildBusy { false }, buildDone { false };
    int    buildWant = -1;
    bool   buildPeriodic = false;
    Volume buildVol;
    void   joinBuilder();

    //  the read, per block: the fold (the kernel and the gain are per voice)
    float  shFold = 0.0f;

    Line   lineSet[NLINES];          // audio thread reads
    Line   linePending[NLINES];
    std::atomic<int> lineLock { 0 };
    std::atomic<bool> linePendingFlag { false };

    Voice  voices[MAXVOICES];
    int    nextId = 1;
    double sr = 48000.0;
    double tNow = 0;
    float  bendSemis = 0;
    bool   sustain = false;
    float  modWheel = 0;
    double bpmNow = 120, ppqNow = 0; bool playingNow = false;
    bool   blepOn = true;
    int    mipLock = -1;

    //  master
    float  outDcX[2] {}, outDcY[2] {};
    std::atomic<float> outLvl { 0.0f };

    //  the world-mod bus
    float  wmDet = 0, wmSag = 0, wmTremD = 0, wmTremR = 0, wmFilt = 1, wmSpread = 0;
    bool   wmActive = false;

    //  the view, copied at a block edge under a spin lock
    std::atomic<int> viewLock { 0 };
    VoiceView view[MAXVOICES]; int viewN = 0;
    float  scopeW[SCOPE_N] {}, scopeF[SCOPE_N] {}, scopeM[SCOPE_N] {};
    int    scopeVoice = 0;

    //  mono
    int    monoStack[32]; int monoN = 0;
};

//==============================================================================
//  Specimens (Specimens.cpp): the stock volumes, and the factory patches.
int         numSpecimens();
const char* specimenName (int i);
bool        specimenIsHu (int i);        // built in Hounsfield units (-1000 .. 2000 across the cube's 0 .. 1)
const char* specimenWindow (int i);      // the radiographer's window it opens on: BRAIN, SOFT, LUNG, BONE, or ""
const char* specimenGloss (int i);
bool        specimenPeriodicX (int i);
void        buildSpecimen (int i, float* out);      // VN^3 floats in [0,1]

struct FactoryPatch
{
    const char* name;
    void (*build) (Line* six, Params& p);
};
int numFactory();
const FactoryPatch& factory (int i);

} // namespace bs
