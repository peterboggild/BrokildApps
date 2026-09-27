/*  ESCAPE ROOM — engine

    A room with five cells in it. Each cell makes a noise of a different
    species; none of them is an ordinary oscillator. What they do to one
    another is decided by the SIGIL — a six-bit number that seeds a fixed,
    reproducible wiring loom. Sigil 41 is always sigil 41. That is the whole
    trick: the machine is baffling but not random, so it can be learned.

    Everything an outside caller touches is normalised 0..1. Mapping to hertz,
    seconds and decibels happens here and nowhere else, so the loom can treat
    every destination the same way.
*/
#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace er
{

static constexpr int NUM_CELLS  = 5;
static constexpr int NUM_SLOTS  = 8;    // wires in the loom
static constexpr int NUM_SRC    = 12;   // things a wire can listen to
static constexpr int NUM_DST    = 21;   // things a wire can move
static constexpr int NUM_VOICES = 4;    // playable filter voices
static constexpr int SCOPE_LEN  = 96;

//==============================================================================
inline float clamp01 (float v)                    { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float lerpf   (float a, float b, float t)  { return a + (b - a) * t; }
inline float xmap    (float t, float lo, float hi){ return lo * std::pow (hi / lo, clamp01 (t)); }
inline float softclip (float x)                   { return std::tanh (x); }
inline float flushDenorm (float x)                { return (std::abs (x) < 1.0e-20f) ? 0.0f : x; }

/*  Triangle wavefolder, identity inside |x| <= 1 so the knob does nothing until
    it is actually turned up, and bounded everywhere — which is what keeps the
    maw from being able to blow the room apart. */
inline float wavefold (float x)
{
    float t = std::fmod (x + 1.0f, 4.0f);
    if (t < 0.0f) t += 4.0f;
    t -= 1.0f;
    return (t > 1.0f) ? (2.0f - t) : t;
}

struct Rng
{
    uint32_t s = 0x9e3779b9u;
    inline uint32_t u32()  { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float    uni()  { return (float) (u32() >> 8) * (1.0f / 16777216.0f); }
    inline float    bi()   { return uni() * 2.0f - 1.0f; }
};

//==============================================================================
struct DCBlock
{
    float x1 = 0, y1 = 0;
    inline float operator() (float x)
    {
        const float y = x - x1 + 0.9975f * y1;
        x1 = x; y1 = flushDenorm (y); return y1;
    }
    void clear() { x1 = y1 = 0; }
};

struct OnePole
{
    float z = 0, a = 1;
    inline void setCut (float hz, double sr) { a = 1.0f - std::exp (-6.2831853f * std::max (1.0f, hz) / (float) sr); }
    inline float lp (float x) { z = flushDenorm (z + a * (x - z)); return z; }
    inline float hp (float x) { return x - lp (x); }
    void clear() { z = 0; }
};

/*  Topology-preserving state variable filter. It is the whole personality of
    the door: with k near 0.02 it rings for a very long time, and the tanh on
    the first integrator is what stops that ring taking the plugin with it. */
struct SVF
{
    float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    float lp = 0, bp = 0, hp = 0, nt = 0;

    inline void set (float cutHz, float q, double sr)
    {
        cutHz = std::min (cutHz, (float) sr * 0.45f);
        g  = std::tan (3.14159265f * std::max (8.0f, cutHz) / (float) sr);
        k  = 1.0f / std::max (0.35f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline void process (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenorm (softclip ((2.0f * v1 - ic1) * 0.2f) * 5.0f);
        ic2 = flushDenorm (2.0f * v2 - ic2);
        bp = v1; lp = v2; hp = x - k * v1 - v2; nt = x - k * v1;
    }

    inline float pick (int mode) const
    {
        switch (mode)
        {
            case 1:  return bp * 1.7f;
            case 2:  return hp;
            case 3:  return nt;
            default: return lp;
        }
    }
    void clear() { ic1 = ic2 = lp = bp = hp = nt = 0; }
};

struct Line
{
    std::vector<float> buf;
    int w = 0, mask = 0;

    void setSize (int n)
    {
        int p = 2; while (p < n) p += p;
        buf.assign ((size_t) p, 0.0f); mask = p - 1; w = 0;
    }
    inline void write (float v) { buf[(size_t) w] = v; w = (w + 1) & mask; }
    inline float readF (float d) const                    // cubic, for the drifting taps
    {
        d = std::max (2.0f, std::min (d, (float) mask - 3.0f));
        const float rp = (float) w - d;
        const int i = (int) std::floor (rp);
        const float f = rp - (float) i;
        const float y0 = buf[(size_t) ((i - 1) & mask)], y1 = buf[(size_t) (i & mask)];
        const float y2 = buf[(size_t) ((i + 1) & mask)], y3 = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
    inline float readI (int d) const { return buf[(size_t) ((w - d) & mask)]; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
};

//==============================================================================
/*  One wire of the loom: something listens, something moves. `curve` bends the
    listened-to signal on the way, which is why two sigils sharing a source and
    a destination still do not behave alike. */
struct Slot { int src = 0, dst = 0, curve = 0; float depth = 0.0f; };

void buildWiring (int sigil, Slot* out);      // deterministic; the panel is shown the result

//==============================================================================
struct Params
{
    struct Cell { float on = 1.0f, lvl = 0.5f, a = 0.5f, b = 0.5f; };
    std::array<Cell, NUM_CELLS> cell;

    // the door
    float cut = 0.45f, res = 0.62f, key = 1.0f, glide = 0.06f;
    float atk = 0.08f, rel = 0.35f, floorLvl = 0.45f, envd = 0.5f;
    float twin = 0.0f, twinAmt = 0.4f;
    int   mode = 1;

    // the loom
    float sigil = 23.0f, bind = 0.35f, drift = 0.12f, rate = 0.35f;

    // the traps
    float shAmt = 0.0f, shSize = 0.35f;
    float chTime = 0.32f, chFeed = 0.0f, chTone = 0.5f;
    float vaSize = 0.55f, vaMix = 0.22f, vaShim = 0.0f;

    // the way out
    float drive = 0.2f, gain = 0.7f;
};

//==============================================================================
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int blockSize);
    void reset();
    void process (float* left, float* right, int numSamples);

    void noteOn  (int note, float velocity);
    void noteOff (int note);
    void allNotesOff();
    void panic();                                  // empties every feedback store

    // Brokild World FX world-mod bus (plain stores, any thread). A NEUTRAL
    // bus (0,0,0,0,0,1) is skipped entirely — bit-identical by memcmp.
    void setWorldMod (float detCents, float panSpread, float tremDepth,
                      float tremRateHz, float sagSemis, float filterMul)
    {
        wmIn[0].store (detCents,   std::memory_order_relaxed);
        wmIn[1].store (panSpread,  std::memory_order_relaxed);
        wmIn[2].store (tremDepth,  std::memory_order_relaxed);
        wmIn[3].store (tremRateHz, std::memory_order_relaxed);
        wmIn[4].store (sagSemis,   std::memory_order_relaxed);
        wmIn[5].store (filterMul,  std::memory_order_relaxed);
    }

    void setSigil (int s);
    const Slot* wiring() const { return slots.data(); }

    Params p;                                      // refreshed by the processor each block

    // --- what the panel is shown -------------------------------------------
    std::array<float, SCOPE_LEN> scope {};        // a complete frame, swapped in on the wrap
    std::array<float, NUM_CELLS> cellRms {};
    std::array<float, NUM_DST>   modOut {};        // live offset applied to each destination
    float outRms = 0.0f, doorRing = 0.0f;
    std::array<float, NUM_VOICES> voiceEnv {}, voiceNote {};

private:
    void  controlTick();
    void  dustS  (float& l, float& r);
    void  ironS  (float& l, float& r);
    void  tideS  (float& l, float& r);
    void  glassS (float& l, float& r, float excite);
    float mawS   (float roomIn);

    double sr = 48000.0;
    Rng rng;

    static constexpr int CTRL = 32;
    int ctrlCount = 0;

    // everything expensive (pow, tan, exp) lands here once per control tick
    struct Derived
    {
        float dustDens = 20, dustPitch = 1200, dustDec = 0.01f, dustGain = 1;
        float ironF1 = 200, ironF2 = 330, ironIdx = 0;
        float tideQ = 6; std::array<float, 3> tideHz { { 300, 600, 1000 } };
        float tideWalk = 1;
        std::array<float, 5> glassLen { { 400, 300, 200, 160, 120 } };
        float glassFb = 0.9f, glassSpark = 0.02f;
        int   mawSr = 1; float mawQ = 65536, mawFold = 1, mawFeed = 0;
        float doorFreeHz = 700, doorQ = 8, twinMul = 1, twinAmt = 0, doorOct = 0;
        float atkC = 0.01f, relC = 0.999f, glideC = 0.05f;
        float shSlice = 4800, shAmt = 0;
        float chDelay = 9600, chFb = 0, chWet = 0;
        std::array<int, 4> fdnLen { { 1237, 1583, 1949, 2381 } };
        float fdnG = 0.7f, vaMix = 0, vaShim = 0;
        float drive = 1, driveComp = 1, outGain = 0.7f;
        std::array<float, NUM_CELLS> lvl { { 0, 0, 0, 0, 0 } };
    } d;

    //== cell I — dust
    struct Grain { float amp = 0, ph = 0, inc = 0, env = 0, dec = 0, pan = 0.5f; bool on = false; };
    std::array<Grain, 28> grains {};
    float dustClock = 0.0f, dustLast = 0.0f;

    //== cell II — iron
    float ph1 = 0, ph2 = 0, y1 = 0, y2 = 0;
    OnePole ironLp;
    DCBlock ironDc;

    //== cell III — tide
    std::array<SVF, 3> tide;
    std::array<float, 3> walk {}, walkGoal3 {};
    float pinkA = 0, pinkB = 0, pinkC = 0;
    int   tideTick = 0;

    //== cell IV — glass
    std::array<Line, 5> glass;
    std::array<OnePole, 5> glassLp;
    float glassClock = 0.0f;
    int   sparkLeft = 0;

    //== cell V — maw
    float mawHold = 0.0f;
    int   mawCount = 0;
    DCBlock mawDc;
    float roomTail = 0.0f;                         // what the maw hears, one sample late

    //== the door
    struct Voice
    {
        bool  on = false;
        int   note = -1;
        float vel = 1.0f, env = 0.0f, hz = 220.0f, target = 220.0f;
        int   stage = 0;                           // 0 idle 1 attack 2 hold 3 release
        uint32_t age = 0;
        float wmGate = 0.0f;          // smoothed gate for the world-mod sag
        float wmGL = 1.0f, wmGR = 1.0f;   // per-voice trem + pan gains
        SVF mL, mR, tL, tR;
    };
    std::array<Voice, NUM_VOICES> voices;
    std::array<std::atomic<float>, 6> wmIn { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    float wmDet = 0, wmPan = 0, wmTremD = 0, wmTremR = 0, wmSag = 0, wmFmul = 1;
    bool  wmActive = false;
    double wmT = 0;
    SVF droneL, droneR, droneTL, droneTR;
    uint32_t voiceClock = 0;
    float doorNorm = 1.0f;

    //== traps
    Line  shBuf, shBufR;                           // shatter
    float shClock = 0, shT = 0, shSpeed = 1;
    bool  shOn = false;
    Line  chL, chR;                                // chasm
    OnePole chLpL, chLpR, chHpL, chHpR;
    float chPhase = 0;
    std::array<Line, 4> fdn;                       // vapour
    std::array<OnePole, 4> fdnLp;
    Line  shimBuf;
    float shimPhase = 0;
    static constexpr int SHIMW = 2200;
    DCBlock outDcL, outDcR;
    float limEnv = 0.0f, limRel = 0.0002f;

    //== loom
    std::array<Slot, NUM_SLOTS> slots {};
    int  curSigil = -1;
    std::array<float, NUM_SRC> srcVal {};
    std::array<float, NUM_CELLS> env {}, gate {};
    std::array<float, NUM_DST> driftW {}, driftGoal {};
    std::array<float, 2> lfoW {}, lfoG {};
    float chaosX = 0.1f, chaosY = 0.0f, chaosZ = 0.0f;
    int   walkTick = 0;
    float outEnvF = 0.0f, doorEnvF = 0.0f;

    //== scope
    std::array<float, SCOPE_LEN> scopeFill {};
    int scopeW = 0, scopeCount = 0, scopeSkip = 15;
};

} // namespace er
