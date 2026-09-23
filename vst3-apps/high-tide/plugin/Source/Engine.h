#pragma once

/*  HIGH TIDE — the engine.

    THERE IS NO STORED WAVEFORM. A frame is a BOWL — a column of potential
    energy on a heightmap terrain — and the waveform is what a mass does when
    it is dropped into that bowl, integrated by Newton's law at audio rate.
    Position along the wavetable is where the mass IS: a slow, damped second
    coordinate on the same terrain, towed toward a target by a tether (HOLD)
    across a relief that the TIDE floods from below. Morphing is travel.

    Pitch is the ball's own clock: the reference bowl U = x^2/2 swings at one
    radian per unit of ball time, and a note at f Hz runs that clock at 2*pi*f.
    Any bowl whose width at every height matches the parabola's is exactly
    isochronous (Landau & Lifshitz section 12), so a sculptor can shear the
    walls however they like and the pitch does not move with the strike. Bowls
    that break the rule bend the pitch by the amount they break it; SERVO
    measures the period and corrects the clock, one cycle late, if asked.

    ROCK is the one ingredient that lets a mass on a one-dimensional terrain
    become chaotic: the whole landscape tilts like a see-saw at a ratio of the
    note (the Duffing drive), or the bowls steepen and relax (BREATH, the
    Mathieu drive). Period doubling arrives as an exact octave down.

    Design: BrokildApps/HIGH-TIDE-DESIGN.md. Contract: PROTOCOL.md.
    The bench (test/bench.cpp) measures every claim above.
*/

#include <atomic>
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstring>

namespace ht
{

//==============================================================================
constexpr int   NX = 512, NZ = 128;
constexpr float XMIN = -1.5f, XMAX = 1.5f, UMAX = 4.0f;
constexpr int   MAXVOICES = 12;
constexpr int   MAXBALLS  = 4;
constexpr int   TICK      = 32;            // control tick, base-rate samples
constexpr int   SCOPE_N   = 256;
constexpr double PI = 3.14159265358979323846;

//==============================================================================
enum PKind { KP_PCT = 0, KP_INT, KP_LIST, KP_BIPOL, KP_HZ, KP_VOL, KP_SEMI, KP_SEC };

struct Params
{
    float level    = 0.70f;
    float tapPos   = 1.00f, tapVel = 0.00f, tapFrc = 0.00f;
    float tone     = 0.85f;
    float strike   = 0.60f, velSens = 0.70f;
    float friction = 0.12f, release = 0.45f;
    float servo    = 0.00f;
    float position = 0.00f, hold = 0.60f, tide = 0.50f;
    float rock     = 0.00f, rockRatio = 0.0f, rockHz = 0.40f, rockMode = 0.0f;
    float unison   = 0.00f, spread = 0.30f, detune = 0.00f, width = 0.50f;
    float ampA = 0.30f, ampD = 0.64f, ampS = 0.80f, ampR = 0.66f;
    float modA = 0.39f, modD = 0.69f, modS = 0.00f, modR = 0.64f;
    float modTarget = 0.0f, modAmt = 0.50f;
    float lfo1Rate = 0.35f, lfo1Sync = 0.0f, lfo1Shape = 0.0f, lfo1Target = 0.0f, lfo1Amt = 0.50f;
    float lfo2Rate = 0.20f, lfo2Sync = 0.0f, lfo2Shape = 0.0f, lfo2Target = 1.0f, lfo2Amt = 0.50f;
    float tune     = 0.50f, glide = 0.00f, voiceMode = 0.0f;
    float ceiling  = 0.50f;
    float quality  = 1.00f;   // 2x / 4x oversampling — 4x by default, sound is king
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
    const char* const* list;   // KP_LIST names, nullptr otherwise
    int   nlist;
};

int          numParams();
const PSpec& paramSpec (int i);
int          paramIndex (const char* id);
float        secondsOf (float v);            // KP_SEC map
float        hzOf (const PSpec& s, float v); // KP_HZ map
int          listIndex (const PSpec& s, float v);

//==============================================================================
/*  The terrain. U(x, z) on NZ columns of NX samples. The relief R(z) is the
    floor of each column; TIDE is a water line on the relief. */
struct Terrain
{
    std::vector<float> u;          // NZ * NX, row j = column z_j
    /*  THE LEVELS OF DETAIL. A fast ball cannot see a feature narrower than
        its step, and a feature it half-sees is jitter (measured: a box bowl at
        B5 read +8 dB of non-harmonic energy however finely the step was
        subdivided). So a note is given the terrain blurred along x to about
        its own step — the terrain synth's mip-map. Level L is a gaussian of
        sigma 2^L cells; level 0 is u itself. A parabola blurs to the same
        parabola plus a constant, so the reference bowl's pitch is untouched. */
    static constexpr int NLOD = 5;
    std::vector<float> lod[NLOD];  // lod[0] unused (u is level 0)
    float R[NZ] {};                // min over x of U(., z_j)
    float Rslope[NZ] {};           // dR/dz, per unit z
    float floorX[NZ] {};           // x of the floor
    int   floorI[NZ] {};
    float rMin = 0, rMax = 0;

    Terrain();
    void  makeReference();                         // the parabola everywhere
    void  setAll (const float* src);               // NZ*NX floats
    void  writeRegion (int x0, int nx, int z0, int nz, const float* src);
    void  recomputeColumn (int j);
    void  recomputeRelief();
    float at (int i, int j) const { return u[(size_t) j * NX + (size_t) i]; }
    float& at (int i, int j)      { return u[(size_t) j * NX + (size_t) i]; }

    /*  U, dU/dx, dU/dz at a continuous point: Catmull-Rom in x (C1, so the
        force is continuous — a kink in the force is a click) and linear in z. */
    void  sample (float x, float z, float& U, float& Ux, float& Uz, float& Uxx, int level) const;
    void  sample (float x, float z, float& U, float& Ux, float& Uz, float& Uxx) const
    { sample (x, z, U, Ux, Uz, Uxx, 0); }
    void  sample (float x, float z, float& U, float& Ux, float& Uz) const
    { float uxx; sample (x, z, U, Ux, Uz, uxx, 0); }
    float stiffnessAt (float x, float z, int level) const;    // d2U/dx2 at the nearest row
    void  rebuildLodColumn (int j);
    static int   levelForStep (double stepCells);
    static float sigmaOf (int level) { return (float) (1 << level); }
    float reliefAt (float z) const;
    float reliefSlopeAt (float z) const;
    float floorAt (float z) const;
    float tideAbs (float tide01) const { return rMin + tide01 * (rMax - rMin + 1.0e-3f); }

    /*  The width test the sculptor's TUNE LOCK enforces: at height h above the
        floor, x+(h) - x-(h) against the parabola's 2*sqrt(2h). Returns the
        worst relative departure over the walls (bench + tension display). */
    float widthDeparture (int j) const;
};

//==============================================================================
/*  The timeline: point lanes timed from note-on and from note-off. */
struct LanePoint { float t = 0, v = 0; int e = 2; };   // e: 0 hold, 1 linear, 2 smooth

struct Lane
{
    std::vector<LanePoint> on, off;
    bool  hasLoop = false;
    float loopA = 0, loopB = 1;
    bool  empty() const { return on.empty() && off.empty(); }
    //  the value at tOn seconds after note-on (held), or tOff after note-off
    float evalOn (double tOn) const;
    float evalOff (double tOff, float valueAtRelease) const;
};

struct Lanes
{
    Lane pin, tide, rock;
};

//==============================================================================
/*  What the panel is shown, thirty times a second. */
struct BallView
{
    int   id = 0, note = 0, held = 0;
    float age = 0, z = 0, zt = 0, x = 0, xa = 0, e = 0, tide = 0, rock = 0;
    int   nBalls = 1;
    float bx[MAXBALLS] {}, bz[MAXBALLS] {};
};

//==============================================================================
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setTransport (double bpm, double ppq, bool playing);
    void process (float* L, float* R, int n);
    void service() {}

    void noteOn (int note, float vel);
    void noteOff (int note);
    void allNotesOff();
    void setBend (float semis);
    void setSustain (bool on);
    void setModWheel (float v01) { modWheel = v01; }

    /*  The sculptor's audition: a ball dropped at z with energy e. While it
        sounds, z is its tether target and the pin lane is ignored for it. */
    void drop (float z, float e, int note, bool on);

    //  SPECTRA world-mod bus (neutral in, bit-identical out)
    void setWorldMod (float detCents, float sag01, float tremDepth,
                      float tremRate, float filterMul, float panSpread);

    //  the terrain and the lanes — message thread writes, audio thread reads
    Terrain&       terrain()       { return terr; }
    const Terrain& terrain() const { return terr; }
    void setLanes (const Lanes& l);
    const Lanes& lanes() const     { return laneSet; }

    //  the view
    int   voicesView (BallView* out, int maxOut);
    void  scopeView (float* out256);
    float outLevel() const { return outLvl.load (std::memory_order_relaxed); }

    Params p;

    //  bench accessors (nothing on the audio path depends on them)
    int    activeVoices() const;
    double voiceZ (int v) const;
    double voiceZTarget (int v) const;
    double ballX (int v, int b) const;
    double ballV (int v, int b) const;
    double ballEnergy (int v, int b) const;       // kinetic + potential, ball units
    double ballClock (int v, int b) const;        // the servo's clock multiplier
    double ballPeriod (int v, int b) const;       // last measured period, ball time
    int    voiceForNote (int note) const;
    double engineTime() const { return tNow; }
    int    oversample() const { return os; }

private:
    //==========================================================================
    struct Ball
    {
        double x = 0, v = 0, a = 0;  // a: last force, for the one-eval Verlet step
        float  uxx = 0;              // local stiffness at the last evaluation (sub-step budget)
        double clock = 1.0;          // servo multiplier on the ball's clock
        double detMul = 1.0;         // SPECTRA detune, fanned per ball (1.0 exact when neutral)
        double breathGain = 0;       // BREATH's grip at this ball's energy, per tick
        double tSinceTurn = 0;       // ball time since the last left turning
        double periodMeas = 2.0 * PI;
        double turnFrac = 0;         // sub-step position of the last crossing
        double vPrev = 0;
        float  gl = 0.7071f, gr = 0.7071f;   // pan gains, per tick
        double xMin = 0, xMax = 0;   // swing over the last tick
        double rockPh = 0;
        float  eScale = 1.0f;        // unison energy scatter
        float  pan = 0;
        float  lp = 0;               // tone one-pole
        float  dcX = 0, dcY = 0;     // position-tap DC blocker
        float  tremPh = 0;
    };

    struct Env
    {
        int   stage = 0;             // 0 idle, 1 attack, 2 decay, 3 sustain, 4 release
        float y = 0;
        void  gate (bool on) { stage = on ? 1 : (stage == 0 ? 0 : 4); }
        float step (float a, float d, float s, float r, float dt);
    };

    struct Voice
    {
        bool   active = false, held = false, sustained = false, released = false;
        int    id = 0, note = -1;
        float  vel = 0;
        double tOn = 0, tOff = 0;    // engine seconds
        double age = 0, ageOff = 0;
        double fHz = 55, fGlideFrom = 55, glideT = 0, glideLen = 0;
        int    lod = 0;              // the level of detail this note sees
        Ball   balls[MAXBALLS];
        int    nBalls = 1;
        double z = 0, vz = 0, zt = 0;
        double uzAcc = 0; int uzN = 0;
        Env    amp, mod;
        double lfoPh[2] {};
        float  lfoSH[2] {};
        uint32_t seed = 1;
        float  laneRel[3] {};        // lane values at the moment of release
        float  laneNow[3] {};
        float  tideEff = 0.5f, rockEff = 0;
        float  tideSm = 0.5f;
        float  gateSm = 0;
        bool   dropped = false;
        float  dropZ = 0;
        long long lastPeriodSamples = 0;
        float  outMono = 0;          // for the scope
    };

    void   tick (Voice& v, double dtReal, double bpm, double ppq, bool playing);
    void   renderVoice (Voice& v, float* L, float* R, int nOs, double dtRealOs);
    void   startVoice (Voice& v, int note, float vel, bool retrigger, bool kick, bool glide);
    int    allocVoice (int note);
    void   pullLanes();
    float  lfoValue (Voice& v, int which, double dtReal, double bpm, double ppq, bool playing);
    void   applyMod (Voice& v, int target, float amount, float& zTarget, float& tide,
                     float& rock, float& hold, float& tone, float& pitchSemi);
    double noteHz (int note) const;
    void   captureView();

    //  handed from the tick to the render, per voice
    double fRender[MAXVOICES] {};
    float  lpRender[MAXVOICES] {};
    float  ampRender[MAXVOICES] {};

    //==========================================================================
    Terrain terr;
    Lanes   laneSet;                 // read on the audio thread; swapped whole
    Lanes   lanePending;
    std::atomic<int> laneLock { 0 };
    std::atomic<bool> lanePendingFlag { false };

    Voice  voices[MAXVOICES];
    int    nextId = 1;
    double sr = 48000.0;
    int    os = 2;
    int    osPending = 2;
    double tNow = 0;
    float  bendSemis = 0;
    bool   sustain = false;
    float  modWheel = 0;
    double bpmNow = 120, ppqNow = 0; bool playingNow = false;

    //  oversampled work buffers
    std::vector<float> osL, osR;
    //  halfband decimator: two stages for 4x
    struct HB { std::vector<float> h; std::vector<float> zL, zR; int idx = 0; };
    HB     hb[2];
    void   hbInit (HB& s);
    inline float hbPush (HB& s, float in, bool right, bool out);

    //  master
    float  outDcX[2] {}, outDcY[2] {};
    std::atomic<float> outLvl { 0.0f };

    //  the world-mod bus
    float  wmDet = 0, wmSag = 0, wmTremD = 0, wmTremR = 0, wmFilt = 1, wmSpread = 0;
    bool   wmActive = false;

    //  the view, copied at a block edge under a spin lock
    std::atomic<int> viewLock { 0 };
    BallView view[MAXVOICES]; int viewN = 0;
    float  scope[SCOPE_N] {}; int scopeW = 0;
    int    scopeVoice = 0;
    std::vector<float> scopeRing; int scopeRingW = 0;

    //  mono/legato
    int    monoStack[32]; int monoN = 0;
};

//==============================================================================
/*  Factory patches: terrains built by code, with lanes and parameter overrides,
    so the bench can load them and so the instrument opens on something. */
struct FactoryPatch
{
    const char* name;
    const char* group;        // "STARTERS" (safe ground) or "TERRAINS"
    void (*build) (Terrain& t, Lanes& l, Params& p);
};
int numFactory();
int numStarters();
const FactoryPatch& factory (int i);

} // namespace ht
