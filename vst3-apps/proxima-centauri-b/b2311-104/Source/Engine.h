#pragma once

/*  ARTEFACT B2311.104 — the engine.

    THE OBJECT MOVES ENERGY, AND THE MOVING IS THE SOUND. A web of conduits on
    the 3-sphere, each one an acoustic waveguide with a thermoacoustic cell:
    past a critical disequilibrium the cell pumps, the loop gain crosses unity,
    and the conduit sings at its own resonance — a limit cycle, grown from a
    real transient, saturated on its own nonlinearity. Below the threshold the
    same loop is a resonator that can only ring and die. That is the whole
    instrument: there is no oscillator object anywhere in this file, and where
    oscillation happens it is a CONDITION, not a command.

    A NOTE IS A THERMAL ORDER. The speed of sound goes as sqrt(T) and a duct's
    resonance is c/2L, so pitch is commanded by commanding a temperature. The
    conduit heats toward the order at a rate set by FLUX through an inertia set
    by MASS, and cools back through LEAK — attack, portamento and release are
    not envelopes, they are heating, and every glide is physics.

    DEEP BY CONSTRUCTION. Conduit lengths put the passive resonances between
    roughly 24 and 260 Hz. Brightness is made the way a resonator makes it:
    the wave steepens as it gets loud.

    What this refuses to reuse is stated in ARTEFACT-B2311-104-DESIGN.md:
    no eigenmodes (.22), no counting (.1), no quasiperiodicity (.67), and no
    injected noise inside any loop (the house rule).
*/

#include <atomic>
#include <vector>
#include <cstdint>
#include <cmath>

namespace ab104
{

//==============================================================================
constexpr int MAXNODE = 48;
constexpr int MAXDUCT = 72;
constexpr int MAXCMD  = 10;      // simultaneous thermal orders
constexpr int MAXPKT  = 12;      // wandering heat packets
constexpr int MAXNBD  = 10;      // conduits sharing a junction with this one

constexpr double PI = 3.14159265358979323846;

/*  The design temperature: passive pitches are specified at 234 K, the
    equilibrium temperature of Proxima Centauri b. AMBIENT runs 77-800 K like
    the sibling findings — Peter measured the old 2.7-1216 range MIRRORING
    around ~77 K ("going further down corresponds to making everything more
    noise and alive"), which was the |ln| disequilibrium being symmetric about
    ambient. The law is now a SLOPE, not a bowl: cold is order — obedient,
    in tune, notes stop when released; heat is disorder — self-active, wild.
    That is thermodynamics, and it is also .1's "cold it does not count". */
constexpr double T_REF = 234.0;
constexpr double T_AMB_LO = 77.0, T_AMB_HI = 800.0;
constexpr double T_DUCT_LO = 2.0, T_DUCT_HI = 2600.0;

//==============================================================================
enum PKind { KP_PCT = 0, KP_INT, KP_LIST, KP_BIPOL, KP_KELVIN, KP_HZ, KP_VOL, KP_SEMI };

struct Params
{
    float level     = 0.62f;

    //  the climate
    float ambient   = 0.4748f;   // exponential 77..800 K; default 234 K
    float onset     = 0.50f;     // how much disequilibrium the song requires
    float traffic   = 0.12f;     // how busy the grid is on its own

    //  the section (the 4th coordinate, on the wheel)
    float section   = 0.50f;     // where the three-space cuts the body
    float veil      = 0.42f;     // how thick a slice is present
    float turn      = 0.25f;     // precession: the body turning through itself

    //  the thermal orders
    float flux      = 0.55f;     // how fast a command pours heat
    float mass      = 0.45f;     // how much heat it takes to move a conduit
    float leak      = 0.40f;     // how fast a conduit forgets towards ambient
    float chuff     = 0.45f;     // the stammer before a conduit sings
    float heed      = 0.60f;     // how much a hard command overdrives
    float retain    = 0.00f;     // how much heat a released conduit keeps
    float discipline= 0.75f;     // how exactly an order is obeyed

    //  the web
    float conduction= 0.35f;     // heat travelling between conduits
    float bleed     = 0.30f;     // sound leaking across the junctions
    float steepen   = 0.35f;     // how a loud wave leans forward and breaks

    /*  THE TWO CONTROLS THAT ESCAPE THE HARMONIC SERIES (v2). A single delay
        loop that settles is periodic and therefore harmonic — a "weird synth",
        and no dial on it changes that. These two break the periodicity.

        STIFFNESS threads an allpass cascade through the loop, so the medium is
        DISPERSIVE: the partials land at non-integer multiples of the
        fundamental (the piano/bell inharmonicity law), and the tone stops
        being a distorted saw and becomes a struck bell of alien metal.

        TURBULENCE is the real thermoacoustic physics I only caricatured
        before. A hot wire's heat release obeys King's law and arrives DELAYED;
        past a critical drive the clean limit cycle loses stability through
        period-doubling (a sub-octave appears), then a quasiperiodic torus,
        then broadband chaos — the Ruelle-Takens route a real Rijke tube takes.
        So the same note, driven harder or hotter, does not merely brighten: it
        changes SPECIES. Deep, because the first bifurcation halves the pitch. */
    float stiffness = 0.30f;
    float turbulence= 0.28f;

    //  the ear
    float breadth   = 0.60f;     // stereo
    float tune      = 0.50f;     // +-12 semitones on every order
    float sat       = 0.35f;     // CEILING — the level it will not exceed
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
};

int          numParams();
const PSpec& paramSpec (int i);
float        paramMax  (const PSpec& s);
const char* const* listNames (const char* id, int& n);

double ambientKelvin (float v);        // the exponential map, one place only

//==============================================================================
/*  A specimen is a WEB: junction nodes seeded on S3, geodesic conduits between
    them, length = pitch. Deterministic, salted until the pitch set clears the
    alienness floor (no harmonic series, no 12-TET — bench numbers). */
struct Geometry
{
    int   nNode = 0, nDuct = 0;
    float node[MAXNODE][4] {};             // on S3
    int   ductA[MAXDUCT] {}, ductB[MAXDUCT] {};
    float fPassive[MAXDUCT] {};           // Hz at T_REF
    uint8_t closedEnd[MAXDUCT] {};        // 1 = quarter-wave, odd harmonics
    float mid[MAXDUCT][4] {};             // geodesic midpoint, unit
    int   nbd[MAXDUCT][MAXNBD] {};        // conduits sharing a junction
    int   nNbd[MAXDUCT] {};
    int   salt = 0;                        // how many draws the floor cost
};

void buildSpecimen (int index, Geometry& g);
int  specimenCount();

//==============================================================================
/*  What the panel is shown. Geometry rides along when it changes (and every
    couple of seconds, the .1 lesson: a single send can be lost); the dynamic
    part goes thirty times a second. */
struct Web
{
    int   nNode = 0, nDuct = 0, specimen = 0, geomStamp = 0;
    float node[MAXNODE][4] {};
    uint8_t ductA[MAXDUCT] {}, ductB[MAXDUCT] {};
    uint8_t closedEnd[MAXDUCT] {};
    float fPassive[MAXDUCT] {};

    uint8_t heat[MAXDUCT] {};     // T against ambient, 128 = at rest
    uint8_t amp [MAXDUCT] {};     // how loudly it carries
    uint8_t sig [MAXDUCT] {};     // how present in the section
    uint8_t cmd [MAXDUCT] {};     // under order
    uint8_t chaos[MAXDUCT] {};    // how far into chaos — turbulent conduits shimmer
    uint8_t bright[MAXDUCT] {};   // the timbre the 4D geometry has given it
    float rotA = 0, rotB = 0;     // the double rotation
    float sectionH = 0, veilW = 0.3f;
    int   nPkt = 0;
    uint8_t pktDuct[MAXPKT] {};
    uint8_t pktT[MAXPKT] {};      // position along the conduit
};

//==============================================================================
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setTransport (double bpm, double ppq, bool playing);
    /*  THE SITE (proxima_site.h). The negotiated site phase, its rate and
        how hard it may pull; the grid's traffic then lands on the site's
        wraps. Pull 0 is an exact no-op — bench-guarded. */
    void setSite (float phase, float hz, float pull)
    {
        sitePhaseIn.store (phase); siteHz.store (hz); sitePull.store (pull);
    }
    double debugNextPacketAt() const { return nextPktAt; }

    void process (float* L, float* R, int n);
    void service();                        // message thread only

    void noteOn (int note, float vel);
    void noteOff (int note);
    void allNotesOff();
    void setBend (float semis);
    void setSustain (bool on);

    /*  The panel's gestures, queued: they must not interrupt the audio thread.
        strike = tap a conduit (it always answers — a silent gesture reads as
        dead); pour = drag heat into it; spin = turn the body. */
    void strike (int duct, float amp);
    void pour (int duct, float kelvin);
    void spin (float dA, float dB);

    /*  SPECTRA world-mod bus, the phase-C mapping. Neutral in, bit-identical
        out: the guard is checked before any of it is touched. */
    void setWorldMod (float detCents, float sag01, float tremDepth,
                      float tremRate, float filterMul, float panSpread);

    /*  The specimen swap. build on the message thread, swap at a block edge —
        the audio thread never sees a half-built web. */
    void requestSpecimen (int index);
    int  currentSpecimen() const { return specIndex; }

    void visualState (Web& out);

    Params p;

    std::atomic<float> outLevel  { 0.0f };
    std::atomic<float> gridPower { 0.0f };   // sum of carried amplitude, the ledger

    //  the site: negotiated phase in, local phase eased onto it (see setSite)
    std::atomic<float> sitePhaseIn { 0.0f }, siteHz { 0.5f }, sitePull { 0.0f };
    double sitePh = 0.0;
    std::atomic<int>   singing   { 0 };      // conduits above onset right now
    std::atomic<int>   chuffFired { 0 };     // stammer pulses since reset (bench)

    //  the bench reads these; nothing on the audio path depends on them
    double ductT (int i) const        { return i >= 0 && i < geo.nDuct ? T[i] : 0.0; }
    double ductF (int i) const        { return i >= 0 && i < geo.nDuct ? fNow[i] : 0.0; }
    float  ductSigma (int i) const    { return i >= 0 && i < geo.nDuct ? sigma[i] : 0.0f; }
    float  ductRing (int i) const     { return i >= 0 && i < geo.nDuct ? duct[i].ringEnv : 0.0f; }
    float  ductChaos (int i) const    { return i >= 0 && i < geo.nDuct ? duct[i].chaos : 0.0f; }
    float  ductRadTgt (int i) const   { return i >= 0 && i < geo.nDuct ? duct[i].radL : 0.0f; }
    float  ductRadCur (int i) const   { return i >= 0 && i < geo.nDuct ? radCurL[i] : 0.0f; }
    float  ductHeatDrive (int i) const{ return i >= 0 && i < geo.nDuct ? duct[i].heatDrive : 0.0f; }
    //  the loop's own recent history, read straight off its delay line: what
    //  is it actually DOING in there (bench/probe only)
    void   ductLoopStats (int i, int n, float& mean, float& acRms, int& zeroCrossings) const
    {
        mean = 0; acRms = 0; zeroCrossings = 0;
        if (i < 0 || i >= geo.nDuct || lineLen <= 0) return;
        const float* line = lines.data() + (size_t) i * (size_t) lineLen;
        n = n < 8 ? 8 : (n > lineLen ? lineLen : n);
        double s = 0;
        for (int k = 0; k < n; ++k) s += line[(wr - 1 - k + 2 * lineLen) % lineLen];
        mean = (float) (s / n);
        double q = 0; float prev = 0;
        for (int k = n - 1; k >= 0; --k)
        {
            const float v = line[(wr - 1 - k + 2 * lineLen) % lineLen] - mean;
            q += (double) v * v;
            if (k < n - 1 && ((v >= 0) != (prev >= 0))) ++zeroCrossings;
            prev = v;
        }
        acRms = (float) std::sqrt (q / n);
    }
    float  ductBright (int i) const   { return i >= 0 && i < geo.nDuct ? duct[i].bright : 0.0f; }
    double ductPassive (int i) const  { return i >= 0 && i < geo.nDuct ? geo.fPassive[i] : 0.0; }
    int    ductNeighbour (int i, int q) const
    { return i >= 0 && i < geo.nDuct && q >= 0 && q < geo.nNbd[i] ? geo.nbd[i][q] : -1; }
    int    ductCount() const          { return geo.nDuct; }
    int    commandDuct (int c) const  { return c >= 0 && c < MAXCMD ? cmds[c].duct : -1; }

private:
    //==========================================================================
    Geometry geo;                 // the live web
    Geometry pending;             // built off-thread, swapped at a block edge
    std::atomic<bool> swapReady { false };
    std::atomic<int>  wantSpec  { -1 };
    int specIndex = 0;
    int geomStamp = 0;

    //  one delay loop per conduit
    std::vector<float> lines;     // MAXDUCT * lineLen
    int    lineLen = 0;
    int    wr = 0;
    struct Duct
    {
        float dcX1 = 0, dcY1 = 0;         // loop DC blocker
        float lp = 0;                      // loop loss one-pole
        float lpA = 0.2f;                  // its coefficient, conduit-relative
        float radDcX = 0, radDcY = 0;      // DC block on the RADIATED tap
        float radA = 0.5f;                 // radiation lowpass: the timbre, off the loop
        float radLp = 0;
        float dtSm = 0;                    // smoothed loop delay, samples
        float g = 0.995f;                  // loop LOSS (< 1); the heat release, not
                                           // this, is what carries it over unity
        float radL = 0, radR = 0;          // radiation TARGETS, set each tick
        float inj = 0;                     // this sample's injection
        float pulseAmp = 0; int pulseLeft = 0; double pulsePh = 0, pulseInc = 0;
        float lastOut = 0;
        float ringEnv = 0;                 // loop amplitude follower
        int   active = 0;

        /*  STIFFNESS — inharmonic MODES, not dispersion. At bass frequencies
            every partial sits so near DC that no allpass can disperse them
            (its phase is near-linear there — measured, zero shift at 55 Hz).
            So the stiff metal is given two high-Q resonators at INHARMONIC
            ratios of the fundamental; struck or spiked, they ring at bell/bar
            frequencies that are on no harmonic grid, and the conduit stops
            sounding like a saw and starts sounding like struck alien metal.
            They sit on the RADIATED tap, never in the loop, so they cannot
            destabilise it. */
        float mY1[2] {}, mY2[2] {};
        float mA1[2] {}, mA2[2] {}, mB[2] {};   // 2 bandpass biquads

        //  THE THERMOACOUSTIC DDE — King's law heat release, delayed by tau
        float heatDrive = 0;               // slewed disequilibrium into the heat law
        float tauSm = 0;                   // the heat-release delay, samples (slewed)
        float kingMean = 0;                // the heater's tracked MEAN heat release —
                                           // subtracted so only the fluctuation drives
                                           // the loop (Rayleigh); kills the DC latch

        //  what the 4D geometry has made of this conduit's voice — set in
        //  updateSection so the picture and the timbre are one object
        float bright = 1.0f;               // radiation-corner multiplier (see-hear)
        float stiffLoc = 0;                // this conduit's share of STIFFNESS
        float tauFrac = 0.42f;             // heat delay as a fraction of the period

        //  how far into chaos it is now — the second-difference roughness of the
        //  loop, followed slowly. Radiated to the panel, so a turbulent conduit
        //  LOOKS turbulent; read by the bench.
        float chaos = 0, prevV = 0, prevV2 = 0;
    };
    Duct  duct[MAXDUCT];
    float radCurL[MAXDUCT] {};             // radiation, slewed towards the targets
    float radCurR[MAXDUCT] {};
    float lastOutArr[MAXDUCT] {};          // one-sample delay for junction bleed
    double condAcc[MAXDUCT] {};            // conduction scratch, per tick
    double T[MAXDUCT] {};                  // conduit temperature
    double fNow[MAXDUCT] {};               // resonance now
    float  sigma[MAXDUCT] {};              // presence in the section
    float  huntPh[MAXDUCT] {};             // the imperfect servo, DISCIPLINE low

    //  thermal orders
    struct Cmd
    {
        int   note = -1, duct = -1;
        float vel = 0, env = 0, tgt = 0;
        bool  held = false;
        double fCmd = 0;                   // absolute pitch ordered
        double chuffAt = 0;                // when the next stammer falls due
        long long age = 0;
    };
    Cmd  cmds[MAXCMD];
    long long cmdClock = 0;
    float bendSemis = 0;
    bool  sustain = false;
    float gateSm = 0;                      // smoothed "anything held", for sag

    //  wandering heat
    struct Pkt
    {
        int   path[6] {}; int len = 0;
        double t0 = 0, perEdge = 1;        // seconds
        float heat = 0;
        int   live = 0;
    };
    Pkt  pkts[MAXPKT];
    double nextPktAt = 0;                  // seconds of engine time
    uint64_t pktSeed = 0;
    int   carrierStep = -1;
    double carrierAt = 0;

    //  the section and the turning
    double rotA = 0, rotB = 0;
    double spinQA = 0, spinQB = 0;         // queued from the panel

    //  queued gestures
    struct Poke { int duct; float amp; float kelvin; };
    std::vector<Poke> pokes;               // guarded by pokeFlag spin
    std::atomic<int> pokeLock { 0 };

    //  world-mod
    float wmDet = 0, wmSag = 0, wmTremD = 0, wmTremR = 0, wmFilt = 1, wmSpread = 0;
    bool  wmActive = false;
    double tremPh = 0;

    double sr = 48000.0;
    double dcR = 0.99934;                  // loop DC blocker: 0.25 Hz at any rate (set in prepare)
    float  kingHpA = 0.0008f;              // the heater's thermal inertia: 6 Hz one-pole on the
                                           // King branch, so only FLUCTUATING heat enters the loop
    long long clockSamples = 0;
    int   tickLeft = 0;                    // samples until the next control tick
    float outDcX[2] {}, outDcY[2] {};
    float outDcX2[2] {}, outDcY2[2] {};    // final DC block, after the ceiling
    float hpZ1[2] {}, hpZ2[2] {};          // 16 Hz subsonic guard, before the ceiling
    float hp2Z1[2] {}, hp2Z2[2] {};        // and again after it — chaos and the odd
                                           // ceiling both re-inject sub-bass
    double hpB0 = 1, hpB1 = 0, hpB2 = 0, hpA1 = 0, hpA2 = 0;

    void controlTick (int samples);
    void allocCmd (int note, float vel);
    int  pickDuct (double fCmd) const;
    void applyGeometry (const Geometry& g);
    void updateSection();
    void spawnPacket (uint64_t why);
    void rebuildOutHp();
};

} // namespace ab104
