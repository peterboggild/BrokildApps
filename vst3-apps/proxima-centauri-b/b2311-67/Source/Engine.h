#pragma once

/*  ARTEFACT B2311.67 — the sounding of the body.

    Two aspects, and they are Fourier transforms of one another.

    FILAMENT (direct space)
        The cut across the tiling is a quasiperiodic chain of masses and bonds.
        We run the actual wave equation on it, at audio rate. Its spectrum is a
        Cantor set — singular continuous, the third kind of spectrum, the one
        no human instrument occupies — and its states are critical, so energy
        spreads anomalously and the sound decays by power laws.

    STAR (reciprocal space)
        The diffraction of that same chain: partials at f0*(p + q*sqrt2), each
        as loud as the window's transform at its Galois conjugate p - q*sqrt2.
        Travelling through the fourth dimension adds the conjugate to the
        frequency:   f = f0*(p + q*sqrt2) + rate*(p - q*sqrt2).
        A hundred partials detuning at a hundred incommensurate rates, and the
        rates are not a modulator's — they are coordinates in w.

    The parameter table is the single source of truth: the APVTS layout, the
    engine copy in processBlock, the habits, the randomiser and the panel all
    walk ax::paramSpec(). The read-order class of bug cannot be expressed.
*/

#include "Lattice.h"

#include <atomic>
#include <array>
#include <vector>
#include <cstring>

namespace ax
{

//==============================================================================
constexpr int MAXN     = 512;    // longest filament (PELL 408 plus headroom)
constexpr int MAXVOICE = 8;
constexpr int MAXPEAK  = 192;
constexpr int GRADE    = 17;     // keys per silver octave

//==============================================================================
struct Params
{
    float level = 0.62f, voicesN = 4.0f, glide = 0.0f, bendRange = 2.0f;

    float habit = 0.0f;
    /*  TEMPERATURE. Stored 0..1; 0 is 77 K and 1 is 800 K, and the default of
        0.2988 is room temperature, which is where the catalogue is meant to be
        heard. See Modulation.h. */
    float temp = 0.298755f;
    float travel = 0.5f, bearing = 0.0f, tilt4 = 0.0f;   // the fourth dimension
    float cutang = 0.0f, cutpos = 0.5f, extent = 3.0f;
    float aperture = 0.5f, rim = 0.22f;

    float contrast = 0.45f, bondlaw = 0.5f, damp = 0.30f, damptilt = 0.30f, nonlin = 0.12f;

    float strike = 0.40f, place = 0.5f, follow = 0.35f, drive = 0.0f, drivecol = 0.5f, spread = 0.55f;

    float blend = 0.45f, peaks = 72.0f, starwid = 0.42f, startilt = 0.30f, drift = 0.0f, stardec = 0.45f;

    float attack = 0.02f, decay = 0.55f, sustain = 0.35f, release = 0.30f;

    float conform = 0.0f, ref = 0.5f, octave = 0.5f;

    float cavity = 0.18f, cavtone = 0.5f, sat = 0.35f, air = 0.35f;
};

enum PKind { KP_PCT = 0, KP_SW, KP_LIST, KP_INT, KP_BIPOL, KP_CENT, KP_VOL, KP_SEMI, KP_HZ,
             KP_KELVIN };

struct PSpec
{
    const char* id;
    const char* name;
    const char* gloss;          // the collector's annotation, for the decoding strip
    float def;
    int   kind;
    float lo, hi;
    float& (*get) (Params&);
};

int          numParams();
const PSpec& paramSpec (int i);
float        paramMax (const PSpec& s);
const char* const* listNames (const char* id, int& n);

//==============================================================================
/*  The chain as the audio thread sees it: masses, bond stiffnesses, damping,
    and the two numbers that keep it in tune and stable — the exact lowest
    eigenfrequency and a Gershgorin bound on the highest. Published by the
    message thread, double buffered, never allocated on the audio thread. */
struct ChainData
{
    int   n = 0;
    float m[MAXN]   {};
    float im[MAXN]  {};       // 1/m, because a divide per site per step is not free
    float k[MAXN + 1] {};     // bond i joins site i-1 and i, so there are n+1 of them
    float z[MAXN]   {};
    float g[MAXN]   {};       // regulariser for the eigen solve, not a force
    float cf[MAXN + 1] {};    // per-bond fade damping: a faint bond is a sticky one
    float mMin = 1.0f;
    float occ[MAXN] {};
    float fld[MAXN] {};
    float s[MAXN]   {};       // position along the cut — the wave lives HERE, not on the index
    unsigned gen = 0;
    double w1 = 0.05, wmax = 2.0;
    double wRef = 0.05;       // the body SIZE: what the note is anchored to
    float  span = 1.0f;       // physical length of the cut, for display
};

struct StarData
{
    int   n = 0;
    float lam[MAXPEAK]  {};
    float conj[MAXPEAK] {};
    float amp[MAXPEAK]  {};
    float rate[MAXPEAK] {};   // lam^0.6, the order-decay exponent, precomputed
};

//==============================================================================
/*  THE OUTPUT STAGE — a musical compressor with a brickwall behind it.

    What was here was a single tanh waveshaper, and it was not protection. The
    bench reported all 256 habits peaking at 0.9349, which is that
    waveshaper's own asymptote: the output was not occasionally clipping, it
    was sitting IN the clipper almost permanently, and what that sounds like is
    distortion.

    Two stages, and the division of labour is the point:

      COMPRESSOR  soft knee, three to one, with a fast and a slow release
                  running together and the lower gain winning — which is what
                  makes a compressor programme-dependent rather than merely
                  quieter. It does the musical work, holding the body of the
                  sound together well below the ceiling.

      BRICKWALL   look-ahead, gain reduction only. Because the compressor has
                  already taken the dynamics out, this stage has almost nothing
                  left to do — which is exactly why it never pumps and never
                  colours. It needs to know about a peak before the peak
                  arrives, so the audio is delayed while the detector reads the
                  input undelayed, giving the gain the whole delay to get out
                  of the way. The latency is reported to the host and therefore
                  compensated.

    A limiter reduces GAIN; a clipper bends the WAVEFORM. Only one of those is
    inaudible when it is not needed. */
struct Dynamics
{
    static constexpr int MAXLA = 1024;

    // ---- 1 · the compressor ------------------------------------------------
    float thr = 0.5f;                     // linear, set from the ceiling
    float invR = 1.0f / 3.0f;             // three to one
    float envC = 0.0f, gFast = 1.0f, gSlow = 1.0f, gC = 1.0f;
    float atkC = 0.01f, relFast = 0.0002f, relSlow = 0.00004f, atkG = 0.02f;

    // ---- 2 · the brickwall -------------------------------------------------
    int   la = 192;
    float dl[2][MAXLA] {};
    int   w = 0;
    float envL = 0.0f, gL = 1.0f, gL2 = 1.0f;
    int   holdL = 0;
    float atkL = 0.02f, relL = 0.0002f, relEL = 0.0001f;

    float gr = 1.0f;                      // worst total reduction this block

    void prepare (double sr)
    {
        la = (int) std::lround (0.004 * sr);                       // 4 ms
        la = la < 32 ? 32 : (la > MAXLA ? MAXLA : la);
        atkC    = 1.0f - std::exp (-1.0f / (float) (0.010 * sr));  // 10 ms, musical
        atkG    = 1.0f - std::exp (-1.0f / (float) (0.008 * sr));
        relFast = 1.0f - std::exp (-1.0f / (float) (0.070 * sr));
        relSlow = 1.0f - std::exp (-1.0f / (float) (0.400 * sr));
        /*  TWO cascaded one-poles, so the coefficient must be set for a
            cascade and not for one. At tau = la/5 a single pole is 99.3 % of
            the way down within the look-ahead but the pair is only 96 %, and
            that missing four per cent is four per cent of overshoot: the bench
            measured 0.9854 against a ceiling of 0.940. At la/9 the pair
            reaches 99.88 %. The compressor ahead of this has already taken the
            dynamics out, so a fast smoother here costs nothing audible. */
        atkL    = 1.0f - std::exp (-9.0f / (float) la);
        relEL   = 1.0f - std::exp (-1.0f / (float) (0.040 * sr));
        relL    = 1.0f - std::exp (-1.0f / (float) (0.130 * sr));
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < MAXLA; ++i) dl[c][i] = 0.0f;
        w = 0; envC = 0.0f; gFast = gSlow = gC = 1.0f;
        envL = 0.0f; gL = gL2 = 1.0f; holdL = 0; gr = 1.0f;
    }

    /*  A last resort only. Transparent below 0.97 and asymptotic to 1, so it
        cannot engage while the brickwall is doing its job — it is there to
        catch the smoother's residual on the sharpest possible transient. */
    static inline float ceilSoft (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.97f) return x;
        const float e = a - 0.97f;
        return (x < 0.0f ? -1.0f : 1.0f) * (0.97f + 0.03f * e / (0.03f + e));
    }

    inline void process (float& l, float& r, float ceiling)
    {
        // ---- compressor ----------------------------------------------------
        const float pk = std::max (std::abs (l), std::abs (r));
        if (pk > envC) envC += (pk - envC) * atkC; else envC += (pk - envC) * relFast;

        float tgt = 1.0f;
        if (envC > thr)
        {
            // soft knee in the linear domain: g = (x/thr)^(1/r - 1)
            tgt = std::pow (envC / thr, invR - 1.0f);
        }
        /*  Fast and slow releases run together and the LOWER gain wins. A
            single release either chatters on transients or drags through a
            phrase; two, taken as a minimum, recover quickly from a stab and
            hold through a sustained passage. That is the whole of what makes a
            compressor sound musical rather than merely quieter. */
        if (tgt < gFast) gFast += (tgt - gFast) * atkG; else gFast += (tgt - gFast) * relFast;
        if (tgt < gSlow) gSlow += (tgt - gSlow) * atkG; else gSlow += (tgt - gSlow) * relSlow;
        gC = gFast < gSlow ? gFast : gSlow;

        l *= gC; r *= gC;

        // ---- brickwall -----------------------------------------------------
        /*  The detector must HOLD across the whole look-ahead. Letting it
            release during those samples is the difference between a brickwall
            and a suggestion: at a forty-millisecond release the envelope falls
            by exp(-192/1920) = 0.905 before the peak it saw actually emerges,
            so the gain is ten per cent too high exactly when it matters. The
            bench measured 0.969 against a ceiling of 0.940 on that alone. */
        const float pk2 = std::max (std::abs (l), std::abs (r));
        if (pk2 >= envL) { envL = pk2; holdL = la; }
        else if (holdL > 0) --holdL;
        else envL += (pk2 - envL) * relEL;

        /*  ONE pole of smoothing, over an EXACT target.

            Two poles chasing a target that is itself still falling lag it, and
            the lag is overshoot: driven flat out with three notes the bench
            measured 0.959 against a ceiling of 0.935. Tracking the target
            instantly and smoothing once is both simpler and tighter — a single
            pole at tau = la/9 is 99.99 % of the way down within the look-ahead
            — and the compressor ahead of this has already taken the dynamics
            out, so there is very little for the ramp to be smooth about.

            The margin is engineering, not physics: aim a half per cent under
            the ceiling so that whatever is left lands beneath it rather than
            on it. */
        const float aim = ceiling * 0.995f;
        const float t2 = (envL > aim) ? aim / std::max (1.0e-9f, envL) : 1.0f;
        if (t2 < gL) gL = t2; else gL += (t2 - gL) * relL;
        gL2 += (gL - gL2) * atkL;

        dl[0][w] = l; dl[1][w] = r;
        const int rd = (w + 1) % la;
        l = ceilSoft (dl[0][rd] * gL2);
        r = ceilSoft (dl[1][rd] * gL2);
        w = rd;

        const float tot = gC * gL2;
        if (tot < gr) gr = tot;
    }
};

class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    void noteOn (int note, float vel);
    void noteOff (int note);
    void allNotesOff();
    void setSustainPedal (bool on);
    void setBend (float b)  { bend = b; }
    void setWheel (float w) { wheel = w; }

    void process (float* L, float* R, int n);

    /*  Message-thread work: rebuild the cut and the star from the parameters.
        Called from a processor-side timer so it runs with the editor closed
        (a project restore or an automation lane can move the body). */
    void service();

    // what the panel is shown (read from the message thread)
    const ab::Patch& patch() const { return vis; }

    /*  The star the instrument is actually sounding, so the panel can draw
        THAT rather than an idealisation of its own. Message thread only --
        rebuildStar() and this are both service()'s neighbours. `starGeneration`
        counts rebuilds, so the bridge can send the star when it changes
        instead of thirty times a second. */
    const std::vector<ab::Peak>& star() const { return peaks; }
    int  starGeneration() const { return starGen; }

    /*  How much of the body in view is SQUARE. Ammann-Beenker is eightfold by
        bond angle wherever you stand -- measured psi8 = 1.000, psi4 = 0.000 at
        every depth, aperture and specimen -- so nothing about the bond angles
        will ever tell you the body looks cubic. What does vary, and strongly,
        is the mixture of tiles: at a small aperture the patch is nothing but
        squares (measured 1.000 at window scale 0.663) and falls to 0.352 as
        the window opens. That is the number the eye is reading. */
    float squareFraction() const
    {
        int sq = 0, n = 0;
        for (const auto& t : vis.tiles) { if (t.kind == 1) ++sq; ++n; }
        return n ? (float) sq / (float) n : 0.0f;
    }
    void  visualState (float* siteEnergy, int maxSites, int& nOut) const;
    double travelDepth() const { return tau; }
    void   setTravelDelta (double d) { tauUi.store (tauUi.load() + d); }
    void   setTravel (double t)      { tauUi.store (t); }

    /*  THE SITE (proxima_site.h). The findings share a bench; cooled and
        close together they fall into step. This lattice has one motion that
        is its own — the cut sliding through the fourth dimension, the tiling
        reorganising by phason flips as it goes — so that is what leans: the
        section ROCKS through w in the bench's time, `pull` deep. At pull 0
        the rock is exactly 0.0 and the depth is what it always was; the
        bench checks that by memcmp. */
    void   setSite (float phase, float hz, float pull)
    {
        sitePhaseIn.store (phase); siteHz.store (hz); sitePull.store (pull);
    }
    double siteRock() const { return siteTau; }
    void   arrest (double px, double py, bool on);
    void   clearArrests();
    int    arrestCount() const { return (int) arrests.size(); }
    float  arrestStrainAt (int i) const { return i >= 0 && i < (int) arrestStrain.size() ? arrestStrain[(size_t) i] : 0.0f; }
    bool   takeTorn() { const bool t = torn; torn = false; return t; }
    const std::vector<std::pair<double,double>>& arrestList() const { return arrests; }

    float noteHz (int note) const;

    Params p;

    // live readouts for the panel
    std::atomic<float> outLevel { 0.0f };
    std::atomic<int>   chainN   { 0 };
    std::atomic<int>   starN    { 0 };

    /*  The loudest sample the instrument produced BEFORE the compressor and
        the brickwall saw it. This is what the level calibration is measured
        from: the peak AFTER the output stage tells you only what the ceiling
        is, which is the same number for every patch and says nothing. Reset it
        yourself before a measurement. */
    std::atomic<float> preLimitPeak { 0.0f };
    std::atomic<float> lowHz    { 0.0f };
    std::atomic<int>   runaways { 0 };   // times the safety net had to catch the field
    std::atomic<float> limitGR { 1.0f }; // worst gain reduction, so the panel can show it
    /*  How much of the fragment the cut is passing through, 1 inside and 0
        once the traverse has taken the plane clear of the body. */
    std::atomic<float> presencePub { 1.0f };
    int latencySamples() const { return dyn.la; }
    // instrumentation: when something goes wrong it is the CONDITION that must
    // be visible, not just the effect
    std::atomic<float> dbgH { 0 }, dbgWmax { 0 }, dbgVmax { 0 }, dbgUmax { 0 }, dbgKmax { 0 };
    std::atomic<int>   dbgSub { 0 };

private:
    struct Voice
    {
        bool  on = false, held = false;
        int   note = 0;
        float vel = 0.0f;
        float gate = 0.0f, env = 0.0f;
        double f0 = 110.0, f0Target = 110.0;
        double h = 0.02;                 // filament timestep
        int    sub = 1;
        int    site = 0;                 // where this note strikes the body
        int    lp0 = 0, lp1 = 0;         // the two listen points
        double sStation = 0, sL = 0, sR = 0;   // ... held as POSITIONS, not indices
        float  sOld[MAXN] {};
        int    nOld = 0;
        unsigned gen = 0;
        double energyPrev = 0.0;         // the body may LOSE energy to the cut moving, never gain
        float  u[MAXN] {}, v[MAXN] {};
        float  ph[MAXPEAK] {}, pa[MAXPEAK] {};
        float  age = 0.0f;
        unsigned envTick = 0;
        float  drivez = 0.0f;
        float  hpx = 0.0f, hpy = 0.0f;   // output dc block
        float  own = 0.0f;               // this voice's own last output, for the cavity coupling
        uint32_t rng = 0x2311u;
    };

    void  rebuildChain();
    void  rebuildStar();
    void  spectralAnchor (ChainData& c);
    static void resampleVoice (Voice& vc, const ChainData& c);
    static int  nearestIndex (const ChainData& c, double s);
    void  startVoice (Voice& vc, int note, float vel);
    int   allocVoice (int note);
    struct BlockConst;
    void  renderVoiceBlock (Voice& vc, const BlockConst& b, float* aL, float* aR, int n, float cav);

    double sr = 48000.0;
    double tau = 0.0;                       // depth in the fourth dimension
    double tauPrev = 0.0, travelRate = 0.0; // how fast the body is being dragged through
    float  starRate = 0.0f;                 // turns/sample added per unit of conjugate
    float  airL = 0.0f, airR = 0.0f;
    std::atomic<double> tauUi { 0.0 };      // the wheel's contribution
    std::atomic<float>  sitePhaseIn { 0.0f }, siteHz { 0.5f }, sitePull { 0.0f };
    double siteTau = 0.0;                   // the bench's rock on the depth (message thread)
    double gx = 0.0, gy = 0.0;

    ab::Window win;
    ab::Habit  hab;
    ab::Patch  vis;
    std::vector<ab::ChainSite> cut;
    std::vector<ab::Peak>      peaks;
    std::vector<std::pair<double,double>> arrests;
    std::vector<float> arrestStrain;      // 0 slack ... 1 about to tear loose
    bool torn = false;

    ChainData chains[3];   // three, because the writer publishes to (cur+1) % 3
    StarData  stars[3];
    std::atomic<int> chainCur { 0 }, starCur { 0 };
    unsigned chainGen = 0;
    int lastHabit = -1, lastExtent = -1, lastPeaks = -1;
    int    starHabit = -2;
    double starTau = 1.0e9, starPeaks = -1, starWid = -1, starTilt = -1, starExtent = -1;
    double starTilt4 = -1;
    int    starGen = 0;
    float  habTrim = 1.0f;         // this specimen's level trim, from HabitGain.h
    double starChainKey = 1.0e300;   // the chain the star was made from

    std::array<Voice, MAXVOICE> voices;
    bool  sustainPedal = false;
    float bend = 0.0f, wheel = 0.0f;

    // the cavity: every voice sounds into one body and hears the others
    float cavZ = 0.0f, cavSum = 0.0f;
    Dynamics dyn;

    // the grade — the artefact's own scale
    double gradeCents[GRADE + 1] {};

    // display feedback
    float siteE[MAXN] {};
    float meter = 0.0f;

    friend struct EngineProbe;
};

//==============================================================================
// the catalogue, as the panel needs it
int   habitCount();
void  habitSignature (int index, int& gapA, int& gapB, float& hueA, float& hueB);
void  applyHabit (int index, Params& p);

} // namespace ax
