#pragma once

/*  ARTEFACT B2311.1 — the engine.

    THE OBJECT COUNTS. A lattice of units, each holding a phase that rises at
    its own rate. On reaching the top a unit FIRES: it resets, and shoves each
    of its eight neighbours' phases forward. A shove can carry a neighbour over
    its own top, so firings cascade, and a cascade is a percussive event whose
    length nobody chose — measured, the largest runs from 87 units at a coupling
    of 0.06 to 1098 at 0.15.

    THERE IS NO OSCILLATOR. A firing is a moment; the waveform is the list of
    moments. What makes that a timbre rather than a click is where the moment
    falls INSIDE the step: a unit's offset within the step is its position along
    a projection of the four axes. A cascade sweeping the lattice therefore lays
    its firings out in time in the order they stand in space, and the ear hears
    the cascade's geometry. Turn the projection and the sound changes; there is
    no oscillator parameter in existence to turn.

    IT LEANS TOWARDS YOUR CLOCK. Pulse-coupled units with a concave rise
    synchronise — that is Mirollo and Strogatz, and it is why this mechanism was
    chosen over two that were built and measured and thrown away first. So
    entrainment is not engineered here; the OPPOSITE is. Enough spread in the
    natural rates that the object keeps several timings at once and only leans.
    Measured against an imposed beat, first third of a take against the last:
    0.128 -> 0.439 at 3 Hz, 0.280 -> 0.684 at 8 Hz. It eases.

    See ARTEFACT-B2311-1-DESIGN.md for the two rejected mechanisms and the
    numbers that rejected them.
*/

#include <atomic>
#include <vector>
#include <cstdint>
#include <cmath>

namespace ab1
{

//==============================================================================
constexpr int NX = 32, NY = 32, NZ = 3, NW = 3;
constexpr int NUNIT = NX * NY * NZ * NW;          // 9216
constexpr double PI = 3.14159265358979323846;

//  temperature, as on B2311.67: cold, it is silent
constexpr float TEMP_MIN = 77.0f, TEMP_ROOM = 293.0f, TEMP_MAX = 800.0f;

/*  THE BODY IS STRATIFIED. Until 260902.2 every one of the 9216 units excited
    the same pair of one-poles, so every event in a take was the same length
    (1.19 ms to -60 dB at the defaults) and the same colour, and all the variety
    you could hear was the DENSITY of identical clicks.

    Now there are NBAND bodies, and which one a firing reaches is decided by
    that unit's own natural rate — the smooth field that already puts the
    lattice into regions and already draws them as hue on the panel. Slow
    regions answer low and long, fast regions high and short, as any body does.
    Still no oscillator anywhere: a band is a pair of one-poles, Q below one,
    and what differs between them is DAMPING. Raise the Q and spread the
    centres and this would stop being a body and become a bank of tuned bars
    struck by the lattice, which is the thing we are refusing. */
constexpr int NBAND = 8;

//==============================================================================
enum PKind { KP_PCT = 0, KP_INT, KP_LIST, KP_BIPOL, KP_KELVIN, KP_HZ, KP_VOL };

struct Params
{
    float level = 0.62f;

    //  the counting
    float rateLo   = 0.06f;      // slowest natural rate in the lattice
    float rateHi   = 0.34f;      // fastest — the SPREAD is what keeps layers apart
    float couple   = 0.80f;      // how hard a firing shoves its neighbours
    float dead     = 0.55f;      // how long a unit ignores shoves after firing
    float leak     = 0.30f;      // concavity of the rise: 0 linear, 1 strongly concave

    //  the imposed pulse
    float grip     = 0.20f;      // how hard the host's beat shoves the lattice
    float reach    = 0.30f;      // how much of the lattice the beat reaches
    float division = 5.0f;       // beats per imposed pulse (index into a list)
    float freeHz   = 0.35f;      // the pulse when there is no host transport

    //  how the four dimensions reach the ear
    float projx = 0.75f, projy = 0.25f, projz = 0.30f, projw = 0.55f;
    float spanx = 0.55f;         // how much of a step a cascade is spread across
    float tilt  = 0.50f;         // amplitude weighting along the unseen axis

    //  the body
    float temp   = 0.298755f;    // 0 = 77 K, 1 = 800 K; default room
    float shape  = 0.35f;        // the kernel a single firing leaves behind
    float damp   = 0.40f;
    float space  = 0.30f;
    float sat    = 0.35f;

    /*  the three that give an event a life of its own. Each is neutral at 0 —
        and neutral means the arithmetic is exactly x*1 and x+0, so a patch with
        all three shut is the instrument as it stood at 260902.1. */
    float strat    = 0.55f;      // how far apart the bodies of the layers sit
    float persist  = 0.45f;      // how much longer a large event rings
    float traverse = 0.60f;      // how much a cascade's own size sets its spread

    /*  WEIGHT, 260902.3. Everything above works on a body whose entire output
        sat around 6.5 kHz, so the object had no low end at all and every event
        was light however long it was made. This is the object's own LOW BODY:
        three low, deliberately unharmonic modes that only a hard blow reaches.

        It is a body and not an oscillator, and the difference is not a matter
        of opinion: it speaks only when struck, it decays, its pitch does not
        follow the keyboard, and nothing in it runs when the object is quiet. */
    float weight = 0.45f;

    /*  MEMORY, 260902.3. Touching the face used to shove a phase and be over
        with — the moment the finger lifted the object was exactly what it had
        been. Now a touch MARKS the units it lands on: they count faster while
        the mark lasts, and it fades on a time constant this control sets, from
        about two seconds up to permanent at the top.

        A mark is a change of RATE, because rate is a unit's identity here: it
        decides which layer the unit belongs to, which body it speaks through,
        and what colour the panel draws it. So drawing on the object redraws its
        regions, and the eye and the ear are shown the same thing. */
    float memory = 0.45f;
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

//==============================================================================
/*  What the panel is shown: one byte of phase per unit in the visible slice,
    plus where the most recent cascades happened. The lattice is 9216 units and
    the slice is NX*NY, so this is 1024 bytes a frame and not 9216. */
struct Slice
{
    static constexpr int W = NX, H = NY;
    uint8_t phase[W * H] {};       // 0..255
    uint8_t heat [W * H] {};       // recent firing, decaying
    /*  Each unit's NATURAL rate, normalised across the slice. Static until the
        spread is changed, and the reason it is sent at all: units near each
        other in rate run together, so this is what makes the layers visible as
        regions instead of leaving them as structure nobody can see. */
    uint8_t rate [W * H] {};
    /*  How strongly this cell is MARKED, 128 = untouched. The panel tints by
        it, so a trace drawn on the face stays visible while it lasts. */
    uint8_t mark [W * H] {};
};

//==============================================================================
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    /*  Host transport. `ppq` is the position in quarter notes and `playing`
        says whether the host is rolling; when it is not, the object keeps its
        own pulse from FREE RATE, because an object that goes silent when the
        transport stops is a plug-in and not an artefact. */
    void setTransport (double bpm, double ppq, bool playing);

    void process (float* L, float* R, int n);
    void service();                       // message thread: nothing on the audio path

    void noteOn (int note, float vel);
    void noteOff (int note);
    void allNotesOff();

    /*  The panel shoves phases directly — this is the whole of the interaction.
        `x`,`y` are in the visible slice, `amount` is signed. */
    void poke (int x, int y, float amount, int radius);

    void visualState (Slice& out) const;

    /*  THE SITE. The findings share a bench, and cooled and close together
        they fall into step. This object's own thesis is entrainment, so the
        site enters the most honest way it can: as a SECOND imposed pulse.
        `phase` is the site phase (0..1) the processor negotiated with the
        other findings, `hz` its rate, and `pull` how hard it may shove —
        cold and close, up to full grip; hot or far, exactly zero, and a
        zero pull is an exact no-op (bit-identical, bench-guarded). */
    void setSite (float phase, float hz, float pull)
    {
        sitePhaseIn.store (phase); siteHz.store (hz); sitePull.store (pull);
    }
    std::atomic<int> siteWraps { 0 };      // how many site shoves have landed (bench)

    Params p;

    std::atomic<float> outLevel { 0.0f };
    std::atomic<float> pulsePhase { 0.0f };   // where we are between imposed pulses
    std::atomic<int>   lastCascade { 0 };
    std::atomic<long long> totalFired { 0 };   // every firing since reset, for the bench
    std::atomic<int>   biggestCascade { 0 };
    std::atomic<long long> stepsRun { 0 };      // lattice steps taken
    std::atomic<long long> silentSteps { 0 };   // of those, ones in which nothing fired
    std::atomic<float> concentration { 0.0f };// R: how far it has leaned, 0..1
    /*  How far into its long mode the body was driven, 0..1. Exposed because
        when a feature does not fire, the thing to instrument is the CONDITION,
        not the effect — this is what found the first PERSISTENCE wrong. */
    std::atomic<float> maxRingSend { 0.0f };

private:
    //  the lattice
    std::vector<float>   ph;              // phase, or negative while dead
    std::vector<float>   rate;            // natural rate, turns per second
    std::vector<int32_t> nb;              // 8 neighbours, -1 off the edge
    std::vector<uint8_t> ux, uy, uz, uw;
    std::vector<uint8_t> inQ;
    std::vector<int>     fireQ;
    std::vector<float>   heat;            // for the panel
    std::vector<uint8_t> bandOf;          // which body this unit speaks through
    /*  What a touch left behind: a signed multiplier on this unit's rate,
        decaying at the rate MEMORY sets. 0 = untouched. */
    std::vector<float>   mark;

    /*  One band of the body. Two one-poles per channel, and two followers on
        its own drive: `env` is what is arriving now, `slow` is what usually
        arrives. The RATIO of those is what opens the decay, which is why
        PERSISTENCE needs no calibration per specimen — a convulsion is loud
        against THIS object's own background, whatever that background is. */
    struct Band
    {
        float z1 = 0, z2 = 0, w1 = 0, w2 = 0;
        float env = 0, slow = 0;
        float a1 = 0.2f, a2 = 0.1f;       // this band's poles
        float gain = 1.0f;                // measured, so a band is not louder
        /*  THE LONG MODE. A separate, ten-times-slower copy of the same body,
            reached only in proportion to how hard this band is being driven.

            The first attempt opened the band's own slow pole instead, which is
            the obvious thing and is wrong: measured, shrinking that pole moves
            the -20 dB point from 0.375 ms to 0.125 ms — it does lengthen the
            tail, but it lowers it faster than it lengthens it, so the audible
            event gets SHORTER. Scaling both poles works but costs 20 dB of
            level on exactly the loudest events, which is a compressor. A real
            body does neither: it has a long mode, and only a hard strike
            reaches it. */
        float r1 = 0, r2 = 0, s1 = 0, s2 = 0;
        float ra1 = 0.02f, ra2 = 0.01f;
        float rgain = 1.0f;
    };
    Band bands[NBAND];
    std::vector<float> bandBuf;           // NBAND * 2 * chunkCap
    int   chunkCap = 0;
    float lastShape = -1, lastDamp = -1, lastStrat = -1;

    /*  THE LOW BODY. Three two-pole resonators at unharmonic ratios, struck in
        proportion to how hard the object is being hit — the same excess that
        opens the long mode, because it is the same physical fact: a hard blow
        reaches the low modes and a tap does not. */
    struct Low
    {
        float y1[3] {}, y2[3] {};
        float c[3] {}, rr[3] {}, g[3] {};   // 2r·cos w, r², input scale
    };
    Low  low;
    float gEnv = 0.0f, gSlow = 0.0f;        // the whole object's drive, two speeds

    /*  THE CRATE. It sat in one for two years, ENCLOSURE has always been on the
        panel calling itself "the room the crate makes around it", and until
        260902.3 the engine never read it — declared, glossed, randomised into
        all 256 specimens, and wired to nothing. Four delay lines through a
        Householder mix, damped in the loop. This is where LENGTH comes from:
        the bank alone tops out at 48 ms. */
    struct Crate
    {
        std::vector<float> line[4];
        int   len[4] {}, pos[4] {};
        float lp[4] {}, fb[4] {};
        float damp = 0.35f;
    };
    Crate crate;
    /*  its OWN cached damp: rebuildBands runs first and updates lastDamp, so
        sharing it would leave the crate deaf to ABSORPTION for ever after. */
    float lastWeight = -1, lastSpace = -1, lastBodyDamp = -1;
    float bandRef = 0.186f;               // one band kernel peak
    float bandEnergy = 1.0f;              // and its ENERGY, which is what the low body must match

    void  rebuildBands();
    void  rebuildBody();                    // low modes + crate, on a change
    void  processChunk (float* L, float* R, int n);

    double sr = 48000.0;
    double stepAcc = 0.0;                 // fractional steps carried between blocks
    double beatPhase = 0.0;               // 0..1 between imposed pulses
    double freeRunPhase = 0.0;
    double hostBpm = 120.0, hostPpq = 0.0;
    bool   hostPlaying = false;

    //  the site pulse: a local phase free-running at siteHz, corrected
    //  towards the negotiated phase each block, shoving the lattice on wrap
    std::atomic<float> sitePhaseIn { 0.0f }, siteHz { 0.5f }, sitePull { 0.0f };
    double sitePhase = 0.0;

    //  running estimate of how strongly firings gather on the beat
    double concX = 0.0, concY = 0.0, concN = 0.0;

    float  held = 0.0f;                   // gate from played notes
    int    heldNotes = 0;
    float  noteRate = 1.0f;               // the played note scales every rate

    void rebuildRates();
    int  fireCascade (int seed, std::vector<int>& out);
    void advanceStep (double dt, std::vector<int>& fired);

    std::vector<int> firedScratch;
    /*  How far the last cascade reached across the four axes, 0..1. A tap and
        a wave crossing the whole object take different times to arrive, and
        until now they took the same. */
    float lastExtent = 0.0f;
};

//==============================================================================
void applySpecimen (int index, Params& p);
int  specimenCount();

} // namespace ab1
