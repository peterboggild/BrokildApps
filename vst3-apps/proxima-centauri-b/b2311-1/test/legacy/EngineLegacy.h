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

namespace ab1legacy
{

//==============================================================================
constexpr int NX = 32, NY = 32, NZ = 3, NW = 3;
constexpr int NUNIT = NX * NY * NZ * NW;          // 9216
constexpr double PI = 3.14159265358979323846;

//  temperature, as on B2311.67: cold, it is silent
constexpr float TEMP_MIN = 77.0f, TEMP_ROOM = 293.0f, TEMP_MAX = 800.0f;

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

    Params p;

    std::atomic<float> outLevel { 0.0f };
    std::atomic<float> pulsePhase { 0.0f };   // where we are between imposed pulses
    std::atomic<int>   lastCascade { 0 };
    std::atomic<long long> totalFired { 0 };   // every firing since reset, for the bench
    std::atomic<int>   biggestCascade { 0 };
    std::atomic<long long> stepsRun { 0 };      // lattice steps taken
    std::atomic<long long> silentSteps { 0 };   // of those, ones in which nothing fired
    std::atomic<float> concentration { 0.0f };// R: how far it has leaned, 0..1

private:
    //  the lattice
    std::vector<float>   ph;              // phase, or negative while dead
    std::vector<float>   rate;            // natural rate, turns per second
    std::vector<int32_t> nb;              // 8 neighbours, -1 off the edge
    std::vector<uint8_t> ux, uy, uz, uw;
    std::vector<uint8_t> inQ;
    std::vector<int>     fireQ;
    std::vector<float>   heat;            // for the panel

    //  one firing leaves a short kernel behind, so the ear is given an event
    //  with a body rather than a sample-wide spike
    struct Tail { float aL = 0, aR = 0, z1 = 0, z2 = 0; };
    Tail tail;

    double sr = 48000.0;
    double stepAcc = 0.0;                 // fractional steps carried between blocks
    double beatPhase = 0.0;               // 0..1 between imposed pulses
    double freeRunPhase = 0.0;
    double hostBpm = 120.0, hostPpq = 0.0;
    bool   hostPlaying = false;

    //  running estimate of how strongly firings gather on the beat
    double concX = 0.0, concY = 0.0, concN = 0.0;

    float  held = 0.0f;                   // gate from played notes
    int    heldNotes = 0;
    float  noteRate = 1.0f;               // the played note scales every rate

    void rebuildRates();
    int  fireCascade (int seed, std::vector<int>& out);
    void advanceStep (double dt, std::vector<int>& fired);

    std::vector<int> firedScratch;
};

//==============================================================================
void applySpecimen (int index, Params& p);
int  specimenCount();

} // namespace ab1legacy
