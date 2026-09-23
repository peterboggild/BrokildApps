#pragma once

/*  ARTEFACT B2311.22 — the engine.
    ==========================================================================

    No oscillator section, no filter, no ADSR, no LFO. A voice is an additive
    bank of the current specimen's exact vibrational modes (Specimen.h), and
    everything a synth normally does with envelopes and modulators is done
    here by four mechanisms that belong to the object itself:

      APERTURE   a note never STARTS a sound. The modes' phases run on a
                 global clock from the moment the plugin exists; a key press
                 opens a window onto the running process (phases are latched
                 from the clock at note-on — mathematically identical to
                 having run forever, at zero idle cost). Same key twice
                 catches the process elsewhere. Narrow apertures self-close:
                 a glimpse. Wide apertures hold while held.

      MIGRATION  while a note is held its per-mode energies do not decay —
                 they FLOW between modes along the specimen's own edges
                 (the coupling table). Total energy is conserved: the
                 timbral motion of a held note is the object exploring its
                 anatomy, not an envelope.

      THE SECTION the 4D body intersects the interface in a slab; a mode
                 sounds in proportion to how much of its eigenvector energy
                 lives inside the visible slice. Playing displaces the
                 section (mode excitation IS movement along that axis), so
                 what the instrument IS drifts with how it is played.

      REVIVAL    partial frequencies morph between the raw dispersive
                 spectrum and a slow commensurate grid. At full revival any
                 moment of the sound recurs EXACTLY every revivalSeconds —
                 the sound reassembles, with fractional ghosts on the way.

    Determinism is absolute: same prepare + same event sequence = the same
    samples, bit for bit (the bench memcmps it). WAKE at zero is exactly
    inert. Plain C++17, no JUCE — the bench compiles this directly.
*/

#include "Specimen.h"
#include <atomic>

namespace ab
{

constexpr int kVoices   = 6;
constexpr int kCtrl     = 64;      // control tick, samples

//  ------------------------------------------------------------- parameters
/*  KP_KELVIN is APPENDED, so KP_VOL keeps the value 3 that the page tests
    for. Renumbering an enum the panel reads by number is a silent way to
    make every volume control display as a percentage. */
enum ParamKind { KP_PCT, KP_SW, KP_INT, KP_VOL, KP_KELVIN };

struct Params
{
    float specimen  = 0.0f;    // catalog number (int-valued; not automatable)
    float volume    = 0.72f;
    float aperture  = 0.55f;
    float metabolism= 0.35f;
    float revival   = 0.0f;
    float depth     = 0.25f;
    float membrane  = 0.15f;
    float gravity   = 0.5f;
    float warmth    = 0.25f;
    float wake      = 0.15f;
    float sidereal  = 0.0f;    // switch: keyboard walks the specimen's ladder
    float winch     = 0.5f;    // harness: section recall target (0..1 -> -1..1)
    float transit   = 0.5f;    // how strongly playing displaces the section
    float slab      = 0.6f;    // section thickness
    /*  RETENTION — how long the section keeps where playing put it.

        Until 260902.1 a played displacement was recalled to the winch with a
        1.2 s time constant, so the object forgot every phrase before the next
        one arrived and nothing could accumulate. The winch is now a place the
        object is HELD, and playing pushes it away from there; this is how long
        that push survives, from about four seconds to two minutes. */
    float retention = 0.55f;

    /*  TEMPERATURE. The frame has carried a kelvin reading since the first
        build and .22 was the one artefact that did not act on it. Cold, the
        body does not vibrate: a strike puts nothing into it, nothing
        sustains, nothing migrates and it does not patrol. Warm, all four
        rise together.

        The default is EXACTLY room, and the scaling is written so that at
        room every factor is exactly 1.0 — so this build is the previous one
        at every setting anybody has played, and the whole bench stays
        valid. */
    float temp = 0.298755f;    // 0 = 77 K, 1 = 800 K

    //  THE INTERLOCUTOR — the second body (design §13)
    float other     = 61.0f;   // its catalog number (int-valued, not automatable)
    float converse  = 0.45f;   // how strongly it listens and answers
    float commune   = 0.25f;   // the nonlinear membrane between the two voices
    float plastic   = 0.10f;   // how much listening remodels the listener
};

struct PSpec
{
    const char* id;
    const char* name;          // the RESEARCHERS' term — harness language
    float       def;
    ParamKind   kind;
    float       lo, hi;        // display range
    float Params::* member;
};

int numParams();
const PSpec& paramSpec (int i);
int paramIndex (const char* id);
inline float& pvalue (Params& p, const PSpec& s) { return p.*(s.member); }

//  ------------------------------------------------------------------ voice
struct Voice
{
    bool  active   = false;
    bool  releasing= false;
    int   note     = -1;
    float velocity = 0.0f;
    float f0       = 220.0f;

    //  per-mode rotor oscillators (phase-continuous complex rotation)
    std::array<float, kMaxModes> oscX {}, oscY {};    // state
    std::array<float, kMaxModes> rotC {}, rotS {};    // per-sample rotation
    std::array<float, kMaxModes> freq {};             // current Hz (post morph)
    std::array<float, kMaxModes> energy {};           // migrating energies
    std::array<float, kMaxModes> amp {}, ampStep {};  // smoothed realised amp
    std::array<float, kMaxModes> exciteW {};          // profile at trigger

    float apEnv = 0.0f;        // aperture envelope 0..1
    float apStep = 0.0f;
    int   gateSamples = -1;    // >0: narrow aperture self-closes
    long long age = 0;
    bool  killFast = false;    // specimen swap: drain quickly
};

/*  ------------------------------------------------------- the interlocutor
    A resident second body. It is NOT in the voice pool — it has no keys, no
    aperture and no player. It has ears (a resonator bank at its own
    eigenfrequencies), a memory of what rang, and a voice of its own. */
struct Interlocutor
{
    //  ears: one 2-pole resonator per mode, at ITS register
    std::array<float, kMaxModes> rz1 {}, rz2 {};
    std::array<float, kMaxModes> rc {}, rr {}, rin {};
    std::array<float, kMaxModes> acc {};       // per-tick absorbed power
    std::array<float, kMaxModes> heardPow {};  // what rang in it, decaying

    //  what it understood: the speaker's profile, rank-translated at reply
    std::array<float, kMaxModes> spoken {};

    //  voice
    std::array<float, kMaxModes> oscX {}, oscY {}, rotC {}, rotS {};
    std::array<float, kMaxModes> freq {}, energy {}, amp {}, ampStep {};
    std::array<float, kMaxModes> sectW {};

    float sentAcc = 0.0f, sentPow = 0.0f;      // what was said TO it
    float spkEnv  = 0.0f;                      // speaker loudness follower
    float quietFor = 0.0f;                     // seconds since it went quiet
    float sinceReply = 0.0f;                   // seconds since it last spoke
    float utter = 0.0f, utterStep = 0.0f;      // the reply's own envelope
    bool  speaking = false;
    float lastKin = 0.0f;                      // measured mutual intelligibility
    float lastReply = 0.0f;                    // amplitude of the last answer

    void silence()
    {
        rz1.fill (0); rz2.fill (0); acc.fill (0); heardPow.fill (0);
        energy.fill (0); amp.fill (0); ampStep.fill (0);
        oscX.fill (1.0f); oscY.fill (0.0f);
        sentAcc = sentPow = spkEnv = quietFor = sinceReply = 0.0f;
        utter = utterStep = 0.0f;
        speaking = false;
    }
};

//  ----------------------------------------------------------------- engine
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    //  message thread: generate + publish a new specimen (voices drain)
    void loadSpecimen (int catalog);
    const Specimen& specimen() const { return spec[(size_t) specIdx.load (std::memory_order_acquire)]; }

    //  the second body (message thread, like loadSpecimen)
    void loadOther (int catalog);
    const Specimen& otherSpecimen() const
        { return oSpec[(size_t) oSpecIdx.load (std::memory_order_acquire)]; }
    int  otherLoadedCatalog() const { return oLoaded; }

    /*  PLASTICITY: the scars are accumulated on the audio thread and applied
        on the message thread, only while the bodies are at rest. */
    bool remodelPending() const { return remodelReq.load (std::memory_order_acquire); }
    void serviceRemodel();
    int  accentSteps() const { return accentCount; }
    void clearAccent();
    /*  a new being arrives unmarked: the scars of the last body mean nothing
        on a different graph. (Restoring a session is not "a new being" — the
        processor only forgets when the catalog number actually changes.) */
    void forgetAccent (bool self, bool other)
    {
        if (self)  { accentA.fill (1.0f); scarA.fill (0.0f); remodelA.store (false); }
        if (other) { accentB.fill (1.0f); scarB.fill (0.0f); remodelB.store (false); }
        accentCount = 0;
    }
    //  state: the accumulated edge multipliers, per body
    const float* accentSelf()  const { return accentA.data(); }
    const float* accentOther() const { return accentB.data(); }
    void setAccent (const float* self, int nSelf, const float* other, int nOther);

    //  the rack is invisible from outside; these are how we READ it
    bool  debugConv() const
        { return p.converse > 0.0f && oLoaded >= 0 && otherSpecimen().nModes > 0; }
    float debugRin (int k) const { return il.rin[(size_t) (k < 0 ? 0 : k)]; }
    float debugSpkEnv() const { return il.spkEnv; }
    float debugSentPow() const { return il.sentPow; }
    float debugQuietFor() const { return il.quietFor; }
    //  how deep the scars are, and whether the bodies are at rest —
    //  the two things that decide when an accent is applied
    float debugScarMax() const
    {
        float mx = 0;
        for (int e = 0; e < specimen().nEdges; ++e) mx = std::max (mx, scarA[(size_t) e]);
        for (int e = 0; e < otherSpecimen().nEdges; ++e) mx = std::max (mx, scarB[(size_t) e]);
        return mx;
    }
    bool debugAtRest() const
    {
        for (const auto& v : voices) if (v.active) return false;
        return true;
    }
    bool debugSurgeryPending() const
    {
        return remodelA.load (std::memory_order_relaxed)
            || remodelB.load (std::memory_order_relaxed);
    }
    float debugKinship()  const { return il.lastKin; }
    float debugReply()    const { return il.lastReply; }
    bool  debugSpeaking() const { return il.speaking; }
    float debugHeard()    const
    {
        float t = 0;
        for (int k = 0; k < otherSpecimen().nModes; ++k) t += il.heardPow[(size_t) k];
        return t;
    }
    void nodeLuminanceOther (float* out) const
    {
        const Specimen& s = otherSpecimen();
        for (int i = 0; i < s.nNodes; ++i) out[i] = 0.0f;
        if (s.nModes <= 0) return;
        for (int k = 0; k < s.nModes; ++k)
        {
            //  a body lit BOTH by what it hears and by what it says
            const float a = il.amp[(size_t) k] * il.utter;
            const float e = a * a + il.heardPow[(size_t) k] * 0.35f;
            if (e <= 1e-9f) continue;
            const float* col = &s.vec[(size_t) k * s.nNodes];
            for (int i = 0; i < s.nNodes; ++i) out[i] += e * col[i] * col[i];
        }
    }

    void noteOn  (int note, float vel);
    void noteOff (int note);

    /*  The probe is an excitation: pressing tissue sounds it, from exactly
        the node touched. Each place on the body has its own stable pitch,
        so playing the creature by hand is real. */
    void touchOn (int node, float vel);
    void touchOff();
    void allNotesOff();
    void setBend (float semis) { bendSemis = semis; }

    void process (float* L, float* R, int n);

    Params p;                  // written per block from the APVTS by the host

    //  section state, for the panel (read any time)
    float sectionW()   const { return w0Pub.load (std::memory_order_relaxed); }
    float sectionVel() const { return wVelPub.load (std::memory_order_relaxed); }

    //  warmth memory, cleared on patch load
    void clearMemory();

    //  world-mod bus (BWFX SPECTRA) — neutral values are exact identity
    /*  THE SITE. The findings share a bench; cooled and close together they
        fall into step. This body leans on the negotiated site phase in two
        ways only, both breathing rather than clocking: its sustain floor
        swells towards the site's wrap, and the interlocutor prefers to
        interject there. REVIVAL is untouched — the recurrence stays exact.
        At pull 0 nothing here runs and the render is bit-identical. */
    void setSite (float phase, float hz, float pull)
    {
        sitePhaseIn.store (phase); siteHz.store (hz); sitePull.store (pull);
    }

    void setWorldMod (float detC, float pan, float tremD, float tremR,
                      float sag, float fmul)
    {
        wmDet = detC; wmPan = pan; wmTremD = tremD; wmTremR = tremR;
        wmSag = sag; wmFMul = fmul;
        wmActive = (detC != 0.0f || pan != 0.0f || tremD != 0.0f
                 || sag != 0.0f || fmul != 1.0f);
    }

    double sampleRate() const { return fs; }

    //  bench-only introspection: the internal economy must be measurable
    /*  the sound's actual place on the body: per-node energy summed over
        active voices and all modes — (amp_k * vec_ki)^2. View-only (message
        thread reads amps relaxed; a torn float lights a pixel wrongly for
        one frame, nothing more). */
    void nodeLuminance (float* out) const
    {
        const Specimen& s = specimen();
        for (int i = 0; i < s.nNodes; ++i) out[i] = 0.0f;
        for (const auto& v : voices)
        {
            if (! v.active) continue;
            for (int k = 0; k < s.nModes; ++k)
            {
                const float a = v.amp[(size_t) k];
                if (a <= 1e-5f) continue;
                const float a2 = a * a;
                const float* col = &s.vec[(size_t) k * s.nNodes];
                for (int i = 0; i < s.nNodes; ++i)
                    out[i] += a2 * col[i] * col[i];
            }
        }
    }

    float debugEnergySum (int vi) const
    {
        if (vi < 0 || vi >= kVoices || ! voices[vi].active) return 0.0f;
        float e = 0;
        for (int k = 0; k < specimen().nModes; ++k) e += voices[vi].energy[(size_t) k];
        return e;
    }
    /*  Where a voice's energy actually IS, mode by mode — the thing
        MIGRATION moves. The audible centroid is energy seen THROUGH the
        section, and once the section is a thin selector it pins that
        centroid whatever the energies do; measuring the mechanism needs
        to look at the mechanism. */
    float debugModeEnergy (int vi, int k) const
    {
        if (vi < 0 || vi >= kVoices || ! voices[vi].active) return 0.0f;
        if (k < 0 || k >= specimen().nModes) return 0.0f;
        return voices[vi].energy[(size_t) k];
    }
    int debugActiveVoices() const
    {
        int c2 = 0;
        for (const auto& v : voices) if (v.active) ++c2;
        return c2;
    }

private:
    double fs = 48000.0;
    long long tGlobal = 0;                 // samples since prepare — the clock
                                           // every aperture opens onto

    //  double-buffered specimen (message thread writes spare, publishes)
    Specimen spec[2];
    std::atomic<int> specIdx { 0 };
    int specLoaded = -1;

    //  the second body, same double-buffer discipline
    Specimen oSpec[2];
    std::atomic<int> oSpecIdx { 0 };
    int oLoaded = -1;
    Interlocutor il;

    /*  PLASTICITY. scar[] accumulates on the audio thread; accent[] is the
        cumulative multiplier actually applied to the catalog graph, and is
        what the session saves. */
    std::array<float, kMaxEdges> scarA {}, scarB {};
    std::array<float, kMaxEdges> accentA {}, accentB {};
    //  two surgeries, two gates: a body is remodelled at ITS own first
    //  moment of quiet, not at some moment of shared silence that a talkative
    //  interlocutor may never allow
    std::atomic<bool> remodelReq { false };      // either body is ready
    std::atomic<bool> remodelA { false }, remodelB { false };
    int accentCount = 0;
    int plastPhase = 0;
    std::array<float, kMaxModes> modeE {};     // per-tick scratch
    Specimen scratch;                          // message-thread rebuild space

    Voice voices[kVoices];

    //  the section
    float w0 = 0.0f, wVel = 0.0f;
    std::atomic<float> w0Pub { 0.0f }, wVelPub { 0.0f };
    std::array<float, kMaxModes> sectW {};     // per-mode slice weight (smoothed)
    std::array<float, kMaxModes> wCent {};     // per-mode w centroid (per specimen)
    /*  What playing has put into the section and the object has not yet let
        go of. Separate from w0 on purpose: the winch must stay instant under
        the hand while this stays slow. */
    float wDrift = 0.0f;

    /*  How cold it is, as a multiplier that is exactly 1 at room. */
    float thermal() const
    {
        const float w = p.temp < 0.0f ? 0.0f : (p.temp > 1.0f ? 1.0f : p.temp);
        const float t = w / 0.298755f;
        return t > 2.6f ? 2.6f : t;
    }

    //  warmth: long-term per-mode memory (global, deterministic from events)
    std::array<float, kMaxModes> warmMem {};
    std::array<float, kMaxModes> tiltW {};     // per-tick gravity scratch
    std::array<float, kMaxModes> decayMul {};  // per-tick ringdown scratch
    std::array<float, kMaxModes> breathW {};   // per-tick breath scratch

    //  substrate randomness (seeded, deterministic)
    Rng subRng { 0xB2311u };
    float subPhase = 0.0f;

    float bendSemis = 0.0f;

    //  world-mod
    float wmDet = 0, wmPan = 0, wmTremD = 0, wmTremR = 0, wmSag = 0, wmFMul = 1;
    bool  wmActive = false;
    float wmPhase = 0.0f;

    //  the site (see setSite): a local phase eased onto the negotiated one
    std::atomic<float> sitePhaseIn { 0.0f }, siteHz { 0.5f }, sitePull { 0.0f };
    double sitePhase = 0.0;
    int    siteCycle = 0;         // which of the four wraps the breath is on
    float  siteSwell = 1.0f;      // exactly 1 when uncoupled
    float  siteRock = 0.0f;       // the section rocked through the body; exactly 0 uncoupled
    bool   siteNearWrap = true;   // exactly true when uncoupled

    int ctrlPhase = 0;

    void controlTick();
    void interlocutorTick (float dt);
    void updateOtherRotors();
    void accumulateScar (const Specimen& s, const float* modeEnergy,
                         std::array<float, kMaxEdges>& scar, float rate);
    void remodelBody (int catalog, std::array<float, kMaxEdges>& scar,
                      std::array<float, kMaxEdges>& accent,
                      Specimen* buf, std::atomic<int>& idx, bool isSelf);
    void triggerVoice (Voice& v, int note, float vel, int forceNode = -1);
    int touchNote = -1;             // the sounding touch, if any
    void updateVoiceRotors (Voice& v, bool force);
    void updateSectionWeights();
    float noteToFreq (int note) const;
};

} // namespace ab
