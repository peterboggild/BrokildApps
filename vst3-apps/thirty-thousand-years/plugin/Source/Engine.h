/*  Thirty Thousand Years — the engine.

    Eight voices, each a MASS + SIGNAL + STRUCTURE; one MEMORY; the
    ENVIRONMENT; LIFE; HISTORY; the macros. Plain C++, no JUCE. The processor
    copies the host parameters into p before each block and reads meters
    after it; everything else happens in process().

    Threading: process(), noteOn/off and the setters marked (audio) are for
    the audio thread. prepare(), the scene/macro/matrix/mseg setters and the
    clip pointers are for the message thread; they hand over by plain stores
    of floats (benign) or atomic pointer swaps (clips).
*/
#pragma once

#include "Dsp.h"
#include "Params.h"
#include "Strata.h"
#include "Memory.h"
#include "Environment.h"
#include "Life.h"

namespace tty
{

static constexpr int SCOPE_N = 1024;
static constexpr int SPEC_N  = 512;
static constexpr int SPEC_BANDS = 48;
static constexpr int MACRO_DESTS = 8;

struct MacroDest { int dst = -1; float depth = 0.0f; };

struct Voice
{
    MassVoice mass; SignalVoice sig; StructVoice st;
    int note = -1, channel = 0, age = 0, id = 0;
    float vel = 0.0f, press = 0.0f, mpeBend = 0.0f, vrnd = 0.0f;
    bool gate = false, drone = false, sustained = false, rootLock = false;
    float hzCur = 110.0f, hzTarget = 110.0f, gateSm = 0.0f;
    Adsr envV[NUM_ENV]; MsegState msegV[4]; Lfo lfoV[NUM_LFO];
    float lfoVal[NUM_LFO] = {}, envVal[NUM_ENV] = {}, msegVal[4] = {};
    float actLevel = 0.0f;
    bool active() const { return mass.active() || sig.active() || st.active(); }
};

class Engine
{
public:
    Params p;                        // host base values (written by the processor)
    Params eff;                      // effective values this tick, SMOOTHED (read by the panel as the "modulated" ring)
    Params effTarget;                // base (or HISTORY) + modulation + macros, before the 8 ms slew
    Params scene[NUM_SCENES]; bool sceneSet[NUM_SCENES] = { false, false, false, false };
    MacroDest macro[NUM_MACROS][MACRO_DESTS];
    Life life; MemoryEngine mem; WaveSet waves;
    Channel chan[4]; Lane laneA, laneB; Space space; FeedLoop loop; OutputStage out;
    Voice voices[MAX_VOICES];
    double sr = 48000.0; int maxBlock = 512;

    // ---- lifecycle
    Engine();
    void defaultMacroMaps();
    void prepare (double sampleRate, int maxBlockSize);
    void reset();                    // everything to silence, state cleared
    void process (float* L, float* R, int n, const float* extL = nullptr, const float* extR = nullptr);

    // ---- performance (audio thread)
    void noteOn (int note, float vel, int channel = 0);
    void noteOff (int note, int channel = 0);
    void setBend (float b, int channel = 0);     // -1..1
    void setWheel (float w) { wheel = w; }
    void setAftertouch (float a, int channel = 0);
    void setPolyAftertouch (int note, float a);
    void setSustain (bool on);
    void allNotesOff();
    void panic();                                // fade, clear, stop until restarted
    void strike (float amt = -1.0f);
    void setTransport (double bpm_, double ppq_, bool playing_) { bpm = bpm_; ppq = ppq_; playing = playing_; }
    void setWorldMod (float det, float pan, float trem, float tremRate, float sag, float filt)
    { wmDet = det; wmPan = pan; wmTrem = trem; wmTremRate = tremRate; wmSag = sag; wmFilt = filt; }
    void setCapturing (bool on) { mem.setCapturing (on); }
    void setScala (const float* cents, int n, float periodCents) { scalaN = std::min (n, 64); for (int i = 0; i < scalaN; ++i) scalaCents[i] = cents[i]; scalaPeriod = periodCents; }
    float noteToHz (float note) const;
    bool isStopped() const { return stopped; }
    int  latency() const { return out.latency(); }

    // ---- meters (read after process)
    float outRms = 0.0f, outPeak = 0.0f, limReduction = 0.0f, loopEnergy = 0.0f, spaceEnergy = 0.0f;
    float stratumAct[4] = {};                    // rms per stratum bus
    float historyPos = 0.0f;                     // the position actually in use
    float duckEnv = 0.0f;
    std::array<int, MAX_VOICES> uiNotes { -1, -1, -1, -1, -1, -1, -1, -1 };
    std::array<float, MAX_VOICES> uiLevels {};
    float scope[SCOPE_N] = {}; int scopeWrite = 0;
    /*  Peak hold per internal stage, so a stage that saturates while the
        output stays polite can be SEEN. Order is signal flow; names in
        stageName(). Decays 20 %/block so it follows without flickering. */
    enum { ST_MASS, ST_SIGNAL, ST_MEMORY, ST_STRUCT, ST_MIX_STRIP, ST_MIX_LANEA,
           ST_SEND, ST_SEND_LANEB, ST_SPACE_WET, ST_LOOP_SEND, ST_LOOP_RET,
           ST_PRE_LIMIT, ST_OUT, NUM_STAGES };
    float stagePeak[NUM_STAGES] = {};
    static const char* stageName (int i);
    float uiSpectrum[SPEC_BANDS] = {};           // dB-ish 0..1 per band, refreshed every 512 samples
    int   uiGrains = 0; float uiMemAct = 0.0f; float uiErosion = 0.0f;
    int   uiVoicesUsed = 0;
    bool  uiFracture = false;

private:
    double bpm = 120.0, ppq = 0.0; bool playing = false, wasPlaying = false; double lastPpq = -1.0;
    float wheel = 0.0f, bend = 0.0f, chanBend[17] = {}, chanPress[17] = {};
    float wmDet = 0, wmPan = 0, wmTrem = 0, wmTremRate = 0, wmSag = 0, wmFilt = 1;
    bool sustain = false, stopped = false, wasDrone = false, panicFade = false;
    float panicGain = 1.0f;
    int ageCounter = 0, monoStack[16] = {}, monoDepth = 0;
    int lastDroneChord = -1, lastDroneRoot = -1;
    float scalaCents[64] = {}; int scalaN = 0; float scalaPeriod = 1200.0f;
    Rng rng; uint32_t seedBase = 17;
    float hAuto = 0.0f, hDir = 1.0f, histNudge = 0.0f; bool hDone = false;
    /*  NEW CHORD REWINDS: a journey starts again when you play after a silence.
        keysHeld counts only voices you are playing -- a drone is gated for as
        long as it is switched on, so counting it would mean the quiet gap never
        arrives and the feature would be dead. The rewind fires on the RISING
        edge of keysHeld, which is what makes a chord rewind once rather than
        once per note: the second and third keys of a chord land while the edge
        is already up. */
    bool  keysHeld = false;
    float quietFor = 1.0e9f;        // seconds since the last key was let go

    /*  YOUR HAND BEATS THE JOURNEY. HISTORY writes effTarget every control
        tick, so a parameter it is travelling cannot be moved by its own knob --
        the next tick puts it back. Touch one and it is FREED: HISTORY stops
        writing it and it stays where you left it, until the journey is re-armed
        or a patch is loaded. Marked, never removed from the scenes, so the
        stored journey is still intact when it is re-armed.
        The touch is detected by watching the HOST value, which only the player
        and the host ever write -- HISTORY never writes p. */
    float hSeen[NUM_PARAMS] = {};
    bool  hFreed[NUM_PARAMS] = {};
    bool  hSeenValid = false, hWasOn = false;
    float slewA = 0.1f; bool effPrimed = false;
    float freezeOverride = 0.0f, loopKick = 0.0f;
    float specAcc[SPEC_N] = {}; int specW = 0; Fft specFft; std::vector<float> specRe, specIm, specWin;
    float lfoTremPh[MAX_VOICES] = {};
    OnePole duckFollow; float extLevel = 0.0f;

    // buffers (sized at prepare, CTRL wide)
    float busL[4][CTRL], busR[4][CTRL], mixL[CTRL], mixR[CTRL], sendL[CTRL], sendR[CTRL], loopSendL[CTRL], loopSendR[CTRL];
    float loopRetL[CTRL], loopRetR[CTRL], loopRetMono[CTRL], memMono[CTRL], extMono[CTRL], wetL[CTRL], wetR[CTRL];
    float vMass[CTRL], vSig[CTRL], vSt[CTRL], vSub[CTRL];
    Params voiceP;

    void tickControl (int n);
    void renderVoices (int n);
    void handleDrone();
    void handleEvents();
    int  allocVoice (int note, int channel);
    void startVoice (Voice& v, int note, float vel, int channel, bool drone, bool legato);
    void releaseVoice (Voice& v);
    float scaleSnap (float note) const;
    void reseed (uint32_t s);
    void applyHistory();
};

// helpers shared with the processor
int   chordIntervals (int chord, int* out);      // fills up to 4 semitone offsets, returns count
} // namespace tty
