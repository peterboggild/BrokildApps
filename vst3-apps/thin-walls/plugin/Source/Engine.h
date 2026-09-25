#pragma once
/*  THIN WALLS - the apartment.

    Up to four sound sources and a binaural listener placed anywhere in three
    rooms joined by three doors. What the listener hears is built from:

      * discrete paths - the line of sight, wall reflections up to second order
        (image-source method in each shoebox room), paths bent through open
        doorways (Maekawa edge diffraction), sound transmitted through closed door
        leaves and through the party walls (mass-law transmission loss) - each with
        its own delay, 1/r spreading, ISO 9613-1 air absorption, per-band wall
        absorption, the source's directivity, and a measured HRTF pair (MIT KEMAR,
        minimum phase + ITD) for its direction of arrival;

      * a late field per room - a 16-line feedback delay network whose per-band
        decay follows Eyring's formula for that room's volume, surfaces, material
        and open door area, calibrated so the reverberant level is 16 pi / A
        relative to the direct sound at 1 m (the direct-to-reverberant ratio is
        physics, not a knob). The three networks are coupled one way through the
        doors and walls, from the rooms with the most source power outward, with
        the energy-balance coefficient of coupled-room theory. The listener's own
        room is rendered as a diffuse field (eight decorrelated HRTF directions);
        a neighbouring room's field is heard THROUGH its door, localised there
        and as wide as the doorway.

    A source is either PURE - a point with an optional cardioid tendency, no
    box - or a LOUDSPEAKER: a two-way box whose beaming follows the piston
    formula for its woofer and tweeter and whose rear is what a finite baffle
    leaks by diffraction.

    Plain C++, no JUCE, so the bench measures the same code the plug-in runs.
*/

#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <atomic>
#include <memory>
#include <string>

namespace tw
{

//------------------------------------------------------------------------------
// the apartment
constexpr int NUM_ROOMS = 3;
constexpr int NUM_DOORS = 3;
constexpr int NUM_MATERIALS = 5;   // the fifth is STUDIO
constexpr int MAX_SOURCES = 4;
constexpr int NBAND = 7;                    // octave bands 125 .. 8000 Hz
constexpr float SPEED_OF_SOUND = 343.0f;
constexpr float EAR_HEIGHT = 1.65f;
constexpr float HEAD_RADIUS = 0.0875f;
constexpr float APARTMENT_W = 18.0f;        // normalised x runs over this
constexpr float APARTMENT_D = 9.0f;         // normalised y runs over this

struct Vec3
{
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3 (float X, float Y, float Z) : x (X), y (Y), z (Z) {}
    Vec3 operator+ (const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator- (const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator* (float s) const { return { x * s, y * s, z * s }; }
    float dot (const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    float len() const { return std::sqrt (x * x + y * y + z * z); }
};

struct Room
{
    const char* name;
    float x0, x1, y0, y1, h;
    float volume() const { return (x1 - x0) * (y1 - y0) * h; }
    float surface() const { return 2 * ((x1 - x0) * (y1 - y0) + (x1 - x0) * h + (y1 - y0) * h); }
    bool contains (float x, float y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
};

struct Door
{
    const char* name;
    int roomA, roomB;      // the two rooms it joins
    int axis;              // 0: the door plane is x = pos, 1: it is y = pos
    float pos;             // the plane
    float s0, s1;          // span along the other axis (width 0.9)
    bool hingeAtS0;        // the leaf is hinged at s0 (else at s1)
    float height;          // 2.05
    float width() const { return s1 - s0; }
    float area() const { return width() * height; }
};

extern const Room  ROOMS[NUM_ROOMS];
extern const Door  DOORS[NUM_DOORS];
extern const char* MATERIAL_NAMES[NUM_MATERIALS];
extern const float MATERIAL_ALPHA[NUM_MATERIALS][NBAND];   // octave-band absorption
extern const float MATERIAL_SCATTER[NUM_MATERIALS][NBAND]; // ISO 17497 scattering coefficient
extern const float MATERIAL_SPLAY[NUM_MATERIALS];          // metres of image perturbation per order
extern const float BAND_HZ[NBAND];

/*  Which material each of a room's six surfaces wears. A floor or ceiling of -1
    means "the same as the walls", which is what every room did before there was
    a choice - so a default Params is bit-identical to the old one. */
struct WallBreak;
struct RoomBreaks;
struct RoomGeom;

struct RoomSurfaces
{
    int wall = 1, floor = -1, ceil = -1;
    int floorMat() const { return floor < 0 ? wall : floor; }
    int ceilMat()  const { return ceil  < 0 ? wall : ceil;  }
    // imagePath numbers the walls 0 x0, 1 x1, 2 y0, 3 y1, 4 floor, 5 ceiling
    int of (int wallId) const { return wallId == 4 ? floorMat() : (wallId == 5 ? ceilMat() : wall); }
};

int   roomOf (float x, float y);                       // -1 outside every room
Vec3  clampIntoRooms (Vec3 p, float margin);           // nearest point inside a room

//------------------------------------------------------------------------------
// source inputs: which of the four incoming channels feeds a source
enum SourceInput { IN_OFF = 0, IN_MAIN_L, IN_MAIN_R, IN_MAIN_LR, IN_AUX_L, IN_AUX_R, IN_AUX_LR };
enum SourceType  { SRC_PURE = 0, SRC_LOUDSPEAKER = 1 };

struct SourceParams
{
    float x = 3.0f, y = 2.5f, z = 1.2f, yaw = 0.0f;   // metres, degrees (0 = facing +x)
    int   type = SRC_LOUDSPEAKER;
    float directivity = 0.0f;                        // PURE only: 0 omni .. 1 cardioid
    int   input = IN_MAIN_LR;
    float levelDb = 0.0f;
    bool  active() const { return input != IN_OFF; }
};

//------------------------------------------------------------------------------
/*  FURNITURE. Each piece is a catalogue entry placed at (x, y) and turned by yaw.
    What it does to the sound, in order of how much you hear it:
      * ABSORPTION - an equivalent absorption area per octave band, the way the
        acoustics tables give it per object (an upholstered sofa, a person
        standing). It adds straight into the room's A, so RT60, the late field and
        the direct-to-reverberant ratio all follow. A rug REPLACES the floor under
        it, so the floor's own absorption over its area is taken back out.
      * SCATTERING - a room full of furniture breaks specular reflections up. Each
        piece carries an equivalent scattering area; the room's extra scattering
        coefficient is 1 - exp(-2 sum / S), and every wall bounce keeps (1 - s)
        of its specular energy - the scattered part the late field picks up.
      * OCCLUSION - a path whose leg passes through a piece's box loses the
        Maekawa attenuation of the shortest way round it (over, under or beside),
        and one that passes NEAR an edge gets the lit-side part of the same curve,
        so walking behind a bookcase darkens the sound without a step.
      * REFLECTION - a hard top (a table, a closed piano lid) gives a first-order
        reflection, bent at its edges the same way when the mirror point leaves it.
    An empty layout adds nothing anywhere: every one of these is a loop over zero
    pieces, and the scattering term is an exact +0 dB. */
constexpr int MAX_FURN = 24;
enum FurnType { F_SOFA = 0, F_ARMCHAIR, F_BED, F_RUG, F_CURTAIN, F_BOOKCASE, F_TABLE,
                F_PIANO, F_WARDROBE, F_PERSON, NUM_FURN_TYPES };

struct FurnSpec
{
    const char* id;          // stable id, used in saved layouts
    const char* name;
    float w, d, h;           // footprint along its own x (width) and y (depth), height, metres
    float zb, zt;            // the part that blocks sound, bottom and top (zb > 0: a gap under it)
    bool  occludes;          // rugs and curtains let sound through
    bool  reflectTop;        // a hard horizontal top that reflects
    float topAlpha;          // what that top absorbs (broadband)
    bool  coversFloor;       // a rug: replaces the floor under it
    float absorb[NBAND];     // equivalent absorption area, m^2, 125 .. 8000 Hz
    float scatter;           // equivalent scattering area, m^2
};
extern const FurnSpec FURN[NUM_FURN_TYPES];
/*  An ACOUSTIC PANEL: artwork printed on a 5 cm fabric-wrapped absorber, the
    real product. Per unit area, in place of the wall it hangs on. A plain PRINT
    is visual only and never reaches the engine. */
extern const float PANEL_ALPHA[NBAND];

struct FurnItem
{
    int   type = -1;         // -1: an empty slot
    float x = 0, y = 0;      // centre, metres
    float yaw = 0;           // degrees, 0 = its width along +x
};

struct Params
{
    // real units, already mapped from the host's 0..1 (see PluginProcessor / PROTOCOL.md)
    SourceParams src[MAX_SOURCES];
    float lisX = 4.5f, lisY = 2.5f, lisYaw = 180.0f;
    float door[NUM_DOORS] = { 1.0f, 1.0f, 0.0f };                // aperture 0..1
    int   material[NUM_ROOMS] = { 1, 1, 2 };        // the walls
    int   floorMat[NUM_ROOMS] = { -1, -1, -1 };     // -1: the same as the walls
    int   ceilMat[NUM_ROOMS]  = { -1, -1, -1 };
    // two breakable walls per room: where along each, and how far out of the room
    float breakAlong[NUM_ROOMS][2] = { { 0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, 0.5f } };
    float breakPush [NUM_ROOMS][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
    float directDb = 0, earlyDb = 0, reverbDb = 0, outputDb = 0;
    float mix = 1.0f;
    float earSpan = 0.175f;                                       // metres between the ears, 0.15 .. 1.0
    /*  Render-only: the offline ULTRA+BASS render splits what comes through the
        solid parts (door leaves, party walls) from what comes through the air,
        because the wave simulation below the crossover only does the air. The
        live plug-in never touches these, and 0 dB is an exact 1.0. */
    float transmitDb = 0, airborneDb = 0;
    // the furniture layout: project state, not host parameters
    FurnItem furn[MAX_FURN];
    int nfurn = 0;
    // acoustic art panels on the walls: their total face area per room, m^2
    float panelArea[NUM_ROOMS] = { 0, 0, 0 };

    Params()
    {
        // sources 2..4 start silent, parked at their own spots
        src[1].x = 1.5f;  src[1].y = 1.0f; src[1].input = IN_OFF;
        src[2].x = 4.5f;  src[2].y = 7.0f; src[2].input = IN_OFF;
        src[3].x = 12.0f; src[3].y = 4.5f; src[3].input = IN_OFF;
    }
};

//------------------------------------------------------------------------------
/*  One "gain + three first-order high shelves" filter. Every frequency-dependent
    loss in the model (wall absorption, air, diffraction, transmission, decay per
    pass in the FDN) is a smooth monotone curve over four octave-spaced anchors,
    which three shelves reproduce EXACTLY at the anchors (the fit is iterated on
    the shelves' true digital response) at a tiny fraction of the cost of a real
    per-band split. Anchors: 250 Hz (the gain), then 1 k, 4 k and 8 k. */
struct BandFilter
{
    float g = 1.0f;                 // linear gain at the bottom
    float k1 = 0, k2 = 0, k3 = 0;   // shelf (G - 1) terms
    float a1 = 0, a2 = 0, a3 = 0;   // one-pole coefficients
    float s1 = 0, s2 = 0, s3 = 0;   // states
    float fsHz = 48000.0f;

    void setCoeffs (float fs);
    // target in dB at the 7 octave bands (125 .. 8k); the fit uses 250, 1k, 4k, 8k
    void setBandsDb (const float* db7);
    void reset() { s1 = s2 = s3 = 0; }
    inline float process (float x)
    {
        // each shelf: y = x + k * hp(x), hp = x - lp, lp one-pole
        s1 += a1 * (x - s1);  x = x + k1 * (x - s1);
        s2 += a2 * (x - s2);  x = x + k2 * (x - s2);
        s3 += a3 * (x - s3);  x = x + k3 * (x - s3);
        return g * x;
    }
    // interpolate towards another filter's coefficients (per block smoothing)
    void approach (const BandFilter& t, float f)
    {
        g += f * (t.g - g); k1 += f * (t.k1 - k1); k2 += f * (t.k2 - k2); k3 += f * (t.k3 - k3);
    }
    void copyCoeffs (const BandFilter& t) { g = t.g; k1 = t.k1; k2 = t.k2; k3 = t.k3; }
};

//------------------------------------------------------------------------------
/*  The measured head. KEMAR compact set, diffuse-field equalised, minimum phase,
    resampled to the host rate at prepare. lookup() blends the four grid
    neighbours (bilinear in elevation ring and azimuth) - legitimate for
    minimum-phase responses - and interpolates the ITD the same way. */
class Hrtf
{
public:
    void prepare (double fs);                   // the built-in MIT KEMAR set
    /*  A PERSONAL set from a SOFA file (AES69), resampled to fs, sampled onto the
        same elevation rings as the built-in set but around the FULL circle (a
        real head is not symmetric, so nothing is mirrored), and diffuse-field
        equalised as the built-in set is, so the two can be compared by ear.
        Needs libmysofa (HrtfSofa.cpp); false with a reason when it fails. */
    bool loadSofa (const std::string& file, double fs, std::string& error);
    int  numTaps() const { return ntap; }
    // az: degrees, 0 front, 90 right; el: degrees, up positive.
    // Writes ntap taps for each ear and the ITD in samples (positive = right ear later).
    void lookup (float azDeg, float elDeg, float* left, float* right, float& itdSamples) const;
    const std::string& name() const { return label; }
    const std::string& file() const { return path; }
    bool personal() const { return full; }
private:
    int ntap = 0;
    double fs = 48000.0;
    std::vector<float> taps;   // [dir][ear][ntap]
    std::vector<float> itd;    // [dir] in samples at fs
    bool full = false;         // a loaded set: whole circles per ring
    std::vector<int> ringN, ringFirst;
    std::string label = "MIT KEMAR", path;
    void blendDir (int dir, float w, float* l, float* r, float& it) const;
};

//------------------------------------------------------------------------------
/*  Fractional delay by a windowed sinc, 16 taps at 512 phases.

    MEASURED, which is why it is here: 4-point Hermite loses 5.0 dB at 20 kHz at
    a half-sample fraction, and two-point linear (which the ITD used) loses 11.7.
    Both swing with sub-millimetre movements of a source, so the top octave
    breathes as it moves and the two ears can lose different amounts - a roll-off
    no room has. This is flat to a tenth of a dB across the audio band at every
    fraction. The window is Kaiser (beta 8.6) and every phase is normalised to
    unity at DC, so moving a source cannot change its level either. */
struct FracDelay
{
    static constexpr int TAPS = 16;
    static constexpr int HALF = 8;          // taps run j = -(HALF-1) .. HALF
    static constexpr int PHASES = 512;
    static constexpr int MIN_DELAY = HALF;  // the oldest tap a read touches

    float h[PHASES][TAPS];
    FracDelay();
    static const FracDelay& table();
};

//------------------------------------------------------------------------------
// a mono ring buffer read at a fractional delay
class DelayLine
{
public:
    void prepare (int maxSamples);
    void clear();
    inline void write (float v) { buf[(size_t) w] = v; w = (w + 1) & mask; }
    // delay in samples, measured from the sample just written. Must be at least
    // FracDelay::MIN_DELAY: the kernel reaches HALF-1 samples NEWER than the
    // integer part, and beyond the write cursor there is nothing written yet.
    inline float read (float delay) const
    {
        const int   di = (int) delay;
        const float f  = delay - (float) di;
        const float* h = FracDelay::table().h[(int) (f * (float) FracDelay::PHASES) & (FracDelay::PHASES - 1)];
        int idx = (w - 1 - (di - (FracDelay::HALF - 1))) & mask;   // j = -(HALF-1)
        float acc = 0;
        for (int k = 0; k < FracDelay::TAPS; ++k) { acc += h[k] * buf[(size_t) idx]; idx = (idx - 1) & mask; }
        return acc;
    }
    // the same kernel over a contiguous array: v[at] interpolated f forward
    static inline float interp (const float* v, int at, float f)
    {
        const float* h = FracDelay::table().h[(int) (f * (float) FracDelay::PHASES) & (FracDelay::PHASES - 1)];
        const float* p = v + at + (FracDelay::HALF - 1);           // j = -(HALF-1) is the NEWEST here
        float acc = 0;
        for (int k = 0; k < FracDelay::TAPS; ++k) acc += h[k] * p[-k];
        return acc;
    }
    inline float readInt (int delay) const { return buf[(size_t) ((w - 1 - delay) & mask)]; }
    int capacity() const { return mask; }
private:
    std::vector<float> buf;
    int w = 0, mask = 0;
};

//------------------------------------------------------------------------------
// what a discrete path IS, as the geometry sees it (rebuilt every sub-block)
enum class PathKind : uint8_t { Direct, Refl1, Refl2, Portal, Leaf, Wall, DoorField };

struct PathSpec
{
    uint32_t key = 0;            // identity across sub-blocks (source index in the top byte)
    PathKind kind = PathKind::Direct;
    int   src = 0;               // which source (or, for DoorField, unused)
    int   feed = 0;              // 0..3 = source lines, 4 + r = room r's late field
    float length = 1;            // metres of travel (delay)
    float gain = 1;              // broadband amplitude at the listener (head centre), before the trims
    float gainL = 1, gainR = 1;  // near-field per-ear factor (1 except for the direct path)
    float bandDb[NBAND] = {};    // frequency-dependent part, dB (0 = flat)
    float az = 0, el = 0;        // arrival direction, head-relative, degrees
    int   npts = 0;              // for the picture
    Vec3  pts[10];              // source, up to 7 bounces (or a doorway), listener
    int   selfItem = -1;         // a reflection OFF this furniture piece: it cannot block itself
};

/*  96 was already too few for four sources in the listener's room (25 image paths
    each) and the doorway transition zone roughly doubles a source's paths near a
    door. A refused path is COUNTED (Scene::pathsDropped) - it used to vanish in
    silence. There are more slots than paths because a retiring path keeps its
    slot for its fade-out while its replacement fades in. */
constexpr int MAX_PATHS = 192;
constexpr int MAX_SLOTS = 256;
constexpr int MAX_TAPS  = 384;
constexpr int SUB_BLOCK = 128;
constexpr float PATH_FADE_S = 0.025f;

/*  How hard the engine may work. The DEFAULT is the live plug-in, exactly: the
    offline render of a take (the video export, the bounce) may ask for more -
    image sources to a higher order, and path and slot budgets to carry them.
    Broken rooms stay at second order (their general search grows as surfaces
    to the power of the order). */
struct RenderQuality
{
    int order = 2;              // image-source order in the listener's room
    int maxPaths = 192;         // = MAX_PATHS
    int maxSlots = 256;         // = MAX_SLOTS
};   // a path that appears or disappears fades over this
constexpr float DOOR_ZONE_M = 0.4f;     // the doorway transition zone, either side of the plane

//------------------------------------------------------------------------------
// a rendering slot that follows a PathSpec with smoothing and its own FIR state
struct PathSlot
{
    uint32_t key = 0;
    bool active = false;
    PathKind kind = PathKind::Direct;
    int feed = 0;

    float delay = 0, delayTarget = 0;     // samples
    float gain = 0, gainTarget = 0;       // broadband, trims included
    float gL = 1, gR = 1, gLTarget = 1, gRTarget = 1;
    float itd = 0, itdTarget = 0;         // samples
    BandFilter filt, filtTarget;
    std::array<float, MAX_TAPS> hL {}, hR {}, hLTarget {}, hRTarget {};
    bool fresh = true;                    // just (re)assigned: snap instead of slew
    float env = 0, envTarget = 0;         // appear / disappear fade, 0..1 over PATH_FADE_S

    // crossfading delay jumps: reader B fades in while A fades out
    float delayB = 0; float xfade = 0; bool crossing = false;

    // post-filter mono history for the two ear FIRs
    std::vector<float> hist; int hw = 0; int hmask = 0;
    std::vector<float> zL, zR;            // scratch convolution outputs (block + spare)
};

//------------------------------------------------------------------------------
// one room's late field
struct RoomField
{
    static constexpr int N = 16;
    static constexpr int NAP = 3;         // allpasses inside each line
    static constexpr int NIN = 4;         // allpasses on the way in
    std::array<DelayLine, N> line;
    std::array<int, N> len {};
    std::array<BandFilter, N> loss;
    std::array<float, N> out {};          // this sample's line outputs
    float y = 0;                          // the observed sum, this sample

    /*  The tank. Three allpasses in series inside every line: unity gain, so the
        decay is untouched, but they carry the loop's modal density and multiply
        its echo density. apTotal is what each line's loop grew by, which the
        decay per pass and the level calibration both have to know. */
    static constexpr int AP_CAP = 1536;   // allocated once; the active length is set per material
    std::array<std::vector<float>, N * NAP> ap;
    std::array<int, N * NAP> apLen {}, apW {};
    std::array<float, N> apTotal {};      // energy-weighted samples the allpasses add to line k
    float apG = 0.62f;
    /*  A Schroeder allpass holds the signal for exactly its own length on
        average - the energy-weighted mean delay works out to La with the (1-g^2)
        cancelling, independent of g. So its raw length is what the loop grows by,
        and charging anything else makes the decay wrong (measured: 27 % short at
        1/(1-g^2)). Kept as a named function because it is the sort of factor one
        is tempted to put back. */
    static constexpr float apEff() { return 1.0f; }
    void sizeDiffusers (double fs, float rt60At1k);
    // and the modulation that keeps the remaining modes from standing still
    float modPhase[N] = {}, modRate[N] = {}, modDepth = 1.5f;

    // input side
    int preDelay = 0;
    std::array<std::vector<float>, NIN> inAp;
    std::array<int, NIN> inApLen {}, inApW {};
    float inGain = 0;                     // for a source in this room, full field
    /*  Measured at prepare, at four design decays, with the diffuser chain sized
        for each as it would be in use. calEff is the rendered energy against the
        steady-state formula; calTrim is how far the network's own decay misses
        the one it was designed for. Both interpolated in log decay at runtime. */
    static constexpr int NCAL = 4;
    float calRt[NCAL]   = { 0.12f, 0.45f, 1.5f, 5.0f };
    float calEff[NCAL]  = { 1, 1, 1, 1 };
    float calTrim[NCAL] = { 1, 1, 1, 1 };
    float calAt (const float* tbl, float rt) const
    {
        if (rt <= calRt[0]) return tbl[0];
        if (rt >= calRt[NCAL - 1]) return tbl[NCAL - 1];
        for (int i = 1; i < NCAL; ++i)
            if (rt <= calRt[i])
            {
                const float u = (std::log (rt) - std::log (calRt[i - 1])) / (std::log (calRt[i]) - std::log (calRt[i - 1]));
                return tbl[i - 1] + u * (tbl[i] - tbl[i - 1]);
            }
        return tbl[NCAL - 1];
    }
    float efficiency = 1;                 // the value in use, for the probes to read
    float rt60[NBAND] = {};
    float absorptionArea[NBAND] = {};
    float tEarly = 0;                     // seconds of early part covered by the images
    DelayLine feed;                       // y, for door rendering (read at the door's transit time)
    // diffuse render: 8 fixed HRTF directions, each fed by two lines
    std::array<std::array<float, MAX_TAPS>, 8> dL {}, dR {};
    std::array<std::vector<float>, 8> dhist; std::array<int, 8> dw {}; int dmask = 0;
    float weight = 0, weightTarget = 0;   // 1 when this is the listener's room
    float sourcePower = 0;                // sum of level^2 of the active sources in here (ranks the coupling)
};

struct Coupling
{
    int from = 0, to = 0;
    BandFilter filt;                      // per-band coupling gain
    float gain = 0;                       // broadband part
};

//------------------------------------------------------------------------------
// the picture the panel draws
struct ScenePath
{
    PathKind kind; int src; int npts; Vec3 pts[10]; float db; float ms;
};
struct SceneSource
{
    Vec3 pos; float yaw = 0; int type = 0; int room = 0; bool active = false; float directivity = 0;
};
struct Scene
{
    SceneSource sources[MAX_SOURCES];
    Vec3 lis; int lisRoom = 0; float lisYaw = 0;
    int npaths = 0; ScenePath paths[MAX_PATHS];
    int pathsDropped = 0;                 // paths refused at the MAX_PATHS cap in the last build (0 = none)
    float rt[NUM_ROOMS][4] = {};          // seconds at 250, 1k, 4k, 8k
    // each room's plan polygon, following the folds
    int   planN[NUM_ROOMS] = {};
    float planX[NUM_ROOMS][10] = {}, planY[NUM_ROOMS][10] = {};
    float inDb = -120, outDb = -120, drrDb = 0;
    float furnA[NUM_ROOMS] = {};           // absorption the furniture adds at 1 kHz, m^2 (net: a rug less the floor it covers)
    float light[NUM_ROOMS] = {};           // 0..1: how much sound is in each room right now (the lamps follow it)
};

//------------------------------------------------------------------------------
class Engine
{
public:
    void prepare (double sampleRate, int maxBlock, const RenderQuality& q = RenderQuality());
    // the identity of the image (nx, ny, nz) - unique to order 7 either side
    static uint32_t imageKey (int nx, int ny, int nz) { return (uint32_t) ((nx + 8) * 289 + (ny + 8) * 17 + (nz + 8)); }
    const RenderQuality& renderQuality() const { return quality; }
    /*  Swap the head. Called on the audio thread between blocks (the processor
        hands the set over with a try-lock), or before rendering; the caller keeps
        the old set alive so nothing is freed here. prepare() keeps a personal set
        (the caller reloads it at a new rate) and otherwise builds MIT KEMAR. */
    void setHrtf (std::shared_ptr<const Hrtf> h);
    const Hrtf& hrtf() const { return *hrtfSet; }
    void reset();
    void setParams (const Params& p);
    // four input channels (main L/R, aux L/R; aux may be null), binaural out.
    // The dry side of MIX is the main pair.
    void process (const float* mainL, const float* mainR, const float* auxL, const float* auxR,
                  float* outL, float* outR, int n);
    // convenience for a single stereo pair in place (the bench, a host without the aux bus)
    void process (float* left, float* right, int n);

    // for the panel / bench
    const Scene& scene() const { return sceneBuf[sceneIdx.load()]; }
    float sampleRate() const { return (float) fs; }
    int   numActivePaths() const { return activePaths; }
    int   numPathsDropped() const { return pathsDropped; }
    const RoomField& field (int r) const { return rooms[(size_t) r]; }
    const PathSlot&  slot (int i) const { return slots[(size_t) i]; }
    void  roomRt60 (int r, float* out7) const { for (int b = 0; b < NBAND; ++b) out7[b] = rooms[(size_t) r].rt60[b]; }
    static void  eyringRt60 (int room, const int* materials, const float* doorAperture, float* out7);
    static void  eyringRt60 (int room, const RoomSurfaces& surf, const float* doorAperture, float* out7);
    // the same with a furniture layout in the rooms
    static void  eyringRt60 (int room, const RoomSurfaces& surf, const float* doorAperture, const FurnItem* furn, int nfurn, float* out7);
    // the room's equivalent absorption area per band, m^2: the material over every
    // surface, each door counted at its own aperture, the party walls at what they pass
    static void  absorptionArea (int room, const int* materials, const float* doorAperture, float* out7);
    static void  absorptionArea (int room, const RoomSurfaces& surf, const float* doorAperture, float* out7);
    static void  absorptionArea (int room, const RoomSurfaces& surf, const float* doorAperture, const FurnItem* furn, int nfurn, float* out7);
    // furniture, for the bench and the panel: the Maekawa path difference of a leg P->Q
    // round piece i (positive: blocked; negative: how close it passes; very negative: nowhere near)
    float furnitureDelta (int i, const Vec3& P, const Vec3& Q) const;
    int   numSpecs() const { return nspecs; }
    const PathSpec& specAt (int i) const { return specs[(size_t) i]; }
    float roomScatter (int r) const { return furnScatter[r]; }
    // the light follower's output for room r, 0..1 (what the scene carries as light)
    float lightLevel (int r) const { return lightOut[r]; }
    static float diffractionDb (float fresnelN);
    static float airDbPerMetre (int band);
    // the apartment's fixed facts, for the offline ray tracer (LateRays.cpp)
    static float leafTransmission (int band);   // a closed door leaf, energy fraction
    static float wallTransmission (int band);   // a party wall, energy fraction
    static void  doorOpenStrip (int door, float aperture, float& lo, float& hi);
    static int   doorWallOf (int room, int door);          // 0 x0, 1 x1, 2 y0, 3 y1, -1 none
    static bool  partyWall (int ra, int rb, int& axis, float& pos, float& s0, float& s1, float& h);
    // directivity of a source (dB, <= 0) at cos(theta) off its axis, per band
    static float directivityDb (const SourceParams& s, float cosTheta, int band);
    // the radiated-power correction of that directivity (dB, <= 0): what the field gets
    static float radiatedPowerDb (const SourceParams& s, int band);

    // debug: running sums of y^2 and of the injected signal^2 per room
    double dbgFieldEnergy[NUM_ROOMS] = { 0, 0, 0 }, dbgInjEnergy[NUM_ROOMS] = { 0, 0, 0 };
    // debug: what the late field's share was computed from, for source 0
    double dbgEarly = 0, dbgAll = 0, dbgFraction = 1;
    bool   dbgFlatLoss = false;     // every line loss = its 1 kHz value (a probe's switch)

private:
    double fs = 48000.0;
    int maxBlock = 512;
    int ntap = 128;
    std::shared_ptr<const Hrtf> hrtfSet;       // swapped whole, never edited in place
    void rebuildDiffuseHrtf();

    Params target, cur;
    // smoothed geometry
    Vec3 srcPos[MAX_SOURCES]; float srcYaw[MAX_SOURCES] = {};
    Vec3 lisPos; float lisYaw = 0;
    bool paramsFresh = true;
    float doorNow[NUM_DOORS] = {};
    RoomSurfaces surfNow[NUM_ROOMS];
    std::vector<char> geomStore;          // one RoomGeom per room, built when a break moves
    float breakNow[NUM_ROOMS][4] = {};    // along0, push0, along1, push1
    int   matNow[NUM_ROOMS] = { -1, -1, -1 };
    float lastDoorAc[NUM_DOORS] = { -1, -1, -1 };
    int   lastSrcRoomAc[MAX_SOURCES] = { -1, -1, -1, -1 };
    int   lastTypeAc[MAX_SOURCES] = { -1, -1, -1, -1 };
    float lastDirAc[MAX_SOURCES] = { -1, -1, -1, -1 };
    bool  lastActiveAc[MAX_SOURCES] = { false, false, false, false };
    float lastLevelAc[MAX_SOURCES] = { -1, -1, -1, -1 };
    float trimDirect = 1, trimEarly = 1, trimReverb = 1, trimOut = 1, mixNow = 1;

    // per source
    DelayLine source[MAX_SOURCES];
    DelayLine srcPre[MAX_SOURCES];        // pre-delay before its room's field
    BandFilter srcInFilt[MAX_SOURCES];    // radiated power vs on-axis (directivity)
    float srcInGain[MAX_SOURCES] = {};    // room inGain x sqrt (fraction the images do not carry)
    float srcInGainNow[MAX_SOURCES] = {};
    float srcLevel[MAX_SOURCES] = {};     // level trim, linear, smoothed
    float srcW[MAX_SOURCES][NUM_ROOMS] = {};   // 1 where the source injects
    std::vector<float> monoIn[MAX_SOURCES];
    int curSrc = 0;

    RenderQuality quality;
    std::vector<PathSlot> slots;           // quality.maxSlots, sized at prepare
    int pathsDropped = 0;                 // this build's refusals, see MAX_PATHS
    bool pathsFull (int margin) { if (nspecs < quality.maxPaths - margin) return false; ++pathsDropped; return true; }
    bool snapFades = true;                // the first block after reset: no fade-in
    bool throughOpenDoor (const Vec3& a, const Vec3& b) const;
    float doorZone (int d, const Vec3& P, float& past) const;
    int activePaths = 0;
    std::array<RoomField, NUM_ROOMS> rooms;
    std::vector<Coupling> couplings;
    float doorTau[NUM_DOORS][NBAND] = {};
    // furniture as the engine sees it this block
    struct FurnPose { float cx = 0, cy = 0, c = 1, s = 0, hw = 0, hd = 0, zb = 0, zt = 0; int type = -1, room = -1; };
    FurnPose furnPose[MAX_FURN]; int nfurnNow = 0;
    FurnItem lastFurnAc[MAX_FURN]; int lastNfurnAc = -1;
    int furnKeyAc[MAX_FURN] = {}; int furnKeyN = -1;   // type and room per piece: what the Eyring sums depend on
    float furnScatter[NUM_ROOMS] = {};     // the extra scattering coefficient furniture gives each room
    float furnAbs1k[NUM_ROOMS] = {};
    float panelAreaAc[NUM_ROOMS] = { -1, -1, -1 };
    // the light follower: fast and slow envelopes per room, field energy gathered by tickFields
    float lightFast[NUM_ROOMS] = {}, lightSlow[NUM_ROOMS] = {}, lightOut[NUM_ROOMS] = {};
    double fieldAcc[NUM_ROOMS] = {};
    void updateLight (int n);       // what the furniture adds to A at 1 kHz, measured the engine's own way
    float furnScatterDb[NUM_ROOMS] = {};   // 10 log10 (1 - that), added per wall bounce (exactly 0 with none)
    float wallTau[3][NBAND] = {};          // per room pair (0-1, 0-2, 1-2)

    // sub-block scratch
    std::vector<float> wetL, wetR, tmpA, tmpB;
    std::vector<PathSpec> specs; int nspecs = 0;   // quality.maxPaths, sized at prepare

    // metering
    float inSq = 0, outSq = 0, directSq = 0, revSq = 0;
    Scene sceneBuf[2]; std::atomic<int> sceneIdx { 0 };
    int sceneTick = 0;

    // geometry
    void buildPaths();
    void addImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase);
    // the same job for a room that is no longer a box
    void addPlanImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase);
    bool roomIsBroken (int room) const;
    void addPortalPaths (const Vec3& S, int rs, int rl);
    void addTransmissionPaths (const Vec3& S, int rs, int rl);
    void addDoorFieldPaths (int rl, float scale);
    void addFurnitureReflections (const Vec3& S, int rs, int rl);
    void applyOcclusion();
    void updateFurniture();
    bool imagePath (int room, const Vec3& S, const Vec3& L, int nx, int ny, int nz,
                    Vec3* bounces, int& nb, int* wallIds, float& length);
    bool bounceHitsOpening (int room, int wall, const Vec3& p) const;
    void finishSpec (PathSpec& s, const Vec3& arriveFrom, const Vec3& departTo, float length);
    void headRelative (const Vec3& p, float& az, float& el) const;
    void applyDirectivity (PathSpec& s, const Vec3& departDir) const;
    void assignSlots();
    void updateRoomAcoustics (bool force);
    void rebuildCouplings();
    void measureEfficiency (int r);
    void measureEfficiencyOld (int r);
    void publishScene();

    // audio
    void renderSubBlock (int n);
    void renderPath (PathSlot& p, int n);
    void tickFields (int n);
    void renderDiffuse (int n);
};

} // namespace tw
