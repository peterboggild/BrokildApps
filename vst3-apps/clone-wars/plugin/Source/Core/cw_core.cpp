#include "cw_core.h"

#include <algorithm>

namespace cw
{

using detail::rng01;
using detail::rngStep;
using detail::fastTanh;

// Black Rider's resonance mappings. GROWL reaches the K = 2 edge of self-
// oscillation only near the top of the dial, so it stays a gnarly resonance
// for most of its travel; SCREAM crosses K = 2 at noon and is pushed a lot
// harder above it, so the top of that dial is a loud, filthy scream.
static inline float k35Kgrowl (float v)
{
    return 2.25f * std::pow (std::clamp (v, 0.0f, 1.0f), 0.9f);
}
static inline float k35Kscream (float v)
{
    v = std::clamp (v, 0.0f, 1.0f);
    return v < 0.62f ? 2.0f * std::pow (v / 0.62f, 0.7f)
                     : 2.0f + 0.9f * ((v - 0.62f) / 0.38f);
}
static inline float ladderK (float v)
{
    return 4.0f * 1.12f * std::pow (std::clamp (v, 0.0f, 1.0f), 0.85f);
}
// A real ladder loses its bass as resonance rises - correct, and half of why
// the models feel different. But TEMPER is a per-army switch here, so an
// uncompensated LADDER simply drops out against GROWL when you flip it.
// Calibrated by measurement in test/ladderprobe.cpp, not from the model.
static constexpr float LADDER_MAKEUP = 0.55f;

//==============================================================================
// id tables (UI + APVTS use these; order must match the enums)
static const char* kVoiceIds[numVoiceFields] =
{
    "wave", "foot", "tune", "cut", "res", "lfowave", "lforate", "lfoamp",
    "lfoflt", "envf", "enva", "loop", "drift", "pan", "level", "mute",
    "solo", "note", "pw"
};
static const char* kGlobalIds[numGlobals] =
{
    "tolerance", "tide", "entraina", "entrainb", "driftmaster", "tempera",
    "temperb", "latcha", "latchb", "kbda", "kbdb", "basea", "baseb", "glide",
    "spread", "war", "warslew", "master", "width", "bussat", "bassmono",
    "hpf", "drone", "hq", "springdwell", "springmix", "springfreeze",
    "tapetime", "tapefdbk", "tapemix", "bbdrate", "bbddepth", "driveamt",
    "ranks", "notemode", "envfilt", "lfosync"
};
const char* voiceFieldId (int f) { return kVoiceIds[f]; }
const char* globalId (int g)     { return kGlobalIds[g]; }

//==============================================================================
void defaultPatch (Patch& p)
{
    float* g = p.global;
    g[gTolerance] = 0.35f; g[gTide] = 0.2f;
    g[gEntrainA] = 0.15f;  g[gEntrainB] = 0.15f;
    g[gDriftMaster] = 0.4f;
    g[gTemperA] = 0; g[gTemperB] = 2;
    g[gLatchA] = 0; g[gLatchB] = 0;   // no hold by default: opened VST3s stay polite
    g[gKbdA] = 0.5f; g[gKbdB] = 0.5f;
    g[gBaseA] = 0.5f; g[gBaseB] = 0.5f;
    g[gGlide] = 0.2f; g[gSpread] = 0.3f;
    g[gWar] = 0.5f; g[gWarSlew] = 0.3f;
    g[gMaster] = 0.75f; g[gWidth] = 0.5f; g[gBusSat] = 0.25f;
    g[gBassMono] = 1; g[gHpf] = 1; g[gHq] = 1;
    g[gDrone] = 0;   // a VST3 must open silent; DRONE is an opt-in power switch
    g[gSpringDwell] = 0.4f; g[gSpringMix] = 0.25f; g[gSpringFreeze] = 0;
    g[gTapeTime] = 0.45f; g[gTapeFdbk] = 0.35f; g[gTapeMix] = 0.2f;
    g[gBbdRate] = 0.3f; g[gBbdDepth] = 0.25f;
    g[gDriveAmt] = 0.15f;
    g[gRanks] = 0.5f;   // dead centre: the panel as set
    g[gNoteMode] = (float) nmTreaty;   // the 16 share the chord out by default
    g[gEnvFilt] = 0.45f;               // what the envelope used to add flat
    g[gLfoSync] = 0;                   // free-run, phases from the seed

    for (int v = 0; v < kVoices; ++v)
    {
        float* f = p.voice[v];
        f[vfWave] = 0;
        f[vfFoot] = (float) (1 + (v + 1) % 4);   // 16' 8' 4' 32' ...
        f[vfTune] = 0;
        f[vfCut] = 0.85f; f[vfRes] = 0.2f;
        f[vfLfoWave] = 0; f[vfLfoRate] = 0.3f;
        f[vfLfoAmp] = 0.15f; f[vfLfoFlt] = 0.25f;
        f[vfEnvF] = 0.5f; f[vfEnvA] = 0.5f; f[vfLoop] = 0;
        f[vfDrift] = 0.3f; f[vfPan] = 0; f[vfLevel] = 0.8f;
        f[vfMute] = 0; f[vfSolo] = 0; f[vfPw] = 0.5f;
        f[vfNote] = (float) (v % kNoteSlots);
    }
}

const char* generatePatch (uint32_t seed, Patch& p)
{
    defaultPatch (p);
    uint32_t s = seed * 2654435761u + 0x9E3779B9u;
    auto r  = [&s]() { return rng01 (s); };
    auto rr = [&] (float lo, float hi) { return lo + (hi - lo) * r(); };
    float* g = p.global;

    static const char* kCats[5] = { "abyss", "swarm", "engines", "cathedral", "rust" };
    const int cat = (int) (seed % 5u);

    g[gTolerance] = rr (0.2f, 0.8f);
    g[gTide]      = rr (0.1f, 0.6f);
    g[gDriftMaster] = rr (0.2f, 0.8f);
    g[gSpread]    = rr (0.1f, 0.7f);
    g[gWar]       = rr (0.3f, 0.7f);

    for (int v = 0; v < kVoices; ++v)
    {
        float* f = p.voice[v];
        f[vfTune]    = rr (-0.5f, 0.5f);
        f[vfPan]     = rr (-0.9f, 0.9f);
        f[vfLfoRate] = rr (0.05f, 0.7f);
        f[vfLfoAmp]  = rr (0.0f, 0.4f);
        f[vfLfoFlt]  = rr (0.0f, 0.5f);
        f[vfDrift]   = rr (0.1f, 0.7f);
        f[vfLevel]   = rr (0.55f, 0.95f);
        f[vfNote]    = (float) (rngStep (s) % kNoteSlots);
    }

    switch (cat)
    {
        case 0: // abyss — vast, dark, slow
            g[gBaseA] = rr (0.05f, 0.3f); g[gBaseB] = g[gBaseA] + rr (-0.05f, 0.1f);
            g[gTemperA] = 2; g[gTemperB] = 2;
            g[gSpringMix] = rr (0.3f, 0.6f); g[gSpringDwell] = rr (0.4f, 0.8f);
            g[gDriveAmt] = rr (0.0f, 0.2f);
            for (int v = 0; v < kVoices; ++v)
            {
                float* f = p.voice[v];
                f[vfWave] = (float) (rngStep (s) % 2 + 2);        // tri / sine
                f[vfFoot] = (float) (1 + (int) (rngStep (s) % 2)); // 32' / 16'
                f[vfCut]  = rr (0.36f, 0.91f);
                f[vfRes]  = rr (0.0f, 0.3f);
                f[vfEnvA] = rr (0.7f, 1.0f); f[vfEnvF] = rr (0.6f, 1.0f);
            }
            break;

        case 1: // swarm — massed detuned saws pulling into lock
            g[gEntrainA] = rr (0.3f, 0.9f); g[gEntrainB] = rr (0.3f, 0.9f);
            g[gSpread] = rr (0.4f, 1.0f);
            g[gTemperA] = 0; g[gTemperB] = (float) (rngStep (s) % 3);
            for (int v = 0; v < kVoices; ++v)
            {
                float* f = p.voice[v];
                f[vfWave] = 0;
                f[vfFoot] = (float) (2 + (int) (rngStep (s) % 2)); // 16' / 8'
                f[vfCut]  = rr (0.64f, 1.0f);
                f[vfRes]  = rr (0.1f, 0.45f);
            }
            break;

        case 2: // engines — looping envelopes, machine-room pulse
            g[gTapeFdbk] = rr (0.4f, 0.7f); g[gTapeMix] = rr (0.2f, 0.5f);
            g[gDriveAmt] = rr (0.2f, 0.5f);
            for (int v = 0; v < kVoices; ++v)
            {
                float* f = p.voice[v];
                f[vfWave] = 1;                                   // pulse
                f[vfLoop] = (rngStep (s) % 3) != 0 ? 1.0f : 0.0f;
                f[vfEnvA] = rr (0.0f, 0.4f); f[vfEnvF] = rr (0.0f, 0.5f);
                f[vfCut]  = rr (0.55f, 1.0f);
                f[vfRes]  = rr (0.2f, 0.6f);
            }
            break;

        case 3: // cathedral — swelling upper partials in a long tank
            g[gSpringMix] = rr (0.4f, 0.7f); g[gSpringDwell] = rr (0.5f, 0.9f);
            g[gBaseA] = rr (0.4f, 0.7f); g[gBaseB] = g[gBaseA] + rr (0.0f, 0.2f);
            for (int v = 0; v < kVoices; ++v)
            {
                float* f = p.voice[v];
                f[vfWave] = (float) (rngStep (s) % 2 + 2);
                f[vfFoot] = (float) (3 + (int) (rngStep (s) % 2)); // 8' / 4'
                f[vfEnvA] = rr (0.6f, 1.0f);
                f[vfCut]  = rr (0.73f, 1.0f);
                f[vfLfoAmp] = rr (0.1f, 0.35f);
            }
            break;

        default: // rust — screaming filters, sample & hold, drive
            g[gTemperA] = 1; g[gTemperB] = (float) (rngStep (s) % 2);
            g[gDriveAmt] = rr (0.4f, 0.85f);
            g[gBusSat] = rr (0.3f, 0.7f);
            for (int v = 0; v < kVoices; ++v)
            {
                float* f = p.voice[v];
                f[vfWave] = (float) (rngStep (s) % 2);
                f[vfLfoWave] = 3;                                // s&h
                f[vfLfoFlt] = rr (0.3f, 0.8f);
                f[vfRes]  = rr (0.4f, 0.85f);
                f[vfCut]  = rr (0.55f, 1.0f);
            }
            break;
    }
    return kCats[cat];
}

//==============================================================================
Engine::Engine()
{
    Patch p;
    defaultPatch (p);
    applyPatch (p);
}

void Engine::applyPatch (const Patch& p)
{
    for (int g = 0; g < numGlobals; ++g) setGlobal (g, p.global[g]);
    for (int v = 0; v < kVoices; ++v)
        for (int f = 0; f < numVoiceFields; ++f) setVoice (v, f, p.voice[v][f]);
}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    maxBlock = maxBlockSize;

    for (auto& v : voices)
    {
        v.k35.reset(); v.ladder.reset();
        v.envAmp = {}; v.envFlt = {};
        v.ampSmooth = 0;
    }
    hpfL1 = {}; hpfL2 = {}; hpfR1 = {}; hpfR2 = {}; bassLpL = {}; bassLpR = {};
    tapeL = {}; tapeR = {}; bbdL = {}; bbdR = {};
    tapeToneL = {}; tapeToneR = {}; bbdToneL = {}; bbdToneR = {};

    // spring tank tuned in samples at the working rate
    const double k = fs / 48000.0;
    const int apLens[4]   = { 113, 229, 349, 449 };
    const int combLens[3] = { 1327, 1523, 1747 };
    for (int i = 0; i < 4; ++i)
    {
        springAp[i] = {};
        springAp[i].len = std::min ((int) springAp[i].buf.size() - 1,
                                    std::max (8, (int) (apLens[i] * k)));
        springAp[i].g = 0.62f;
    }
    for (int i = 0; i < 3; ++i)
    {
        springComb[i] = {};
        springComb[i].len = std::min ((int) springComb[i].buf.size() - 1,
                                      std::max (32, (int) (combLens[i] * k)));
    }
    tolSeedApplied = 0;   // force tolerance refresh
    refreshTolerances();
}

//==============================================================================
void Engine::refreshTolerances()
{
    const uint32_t want = unitSeed.load (std::memory_order_relaxed);
    if (want == tolSeedApplied) return;
    tolSeedApplied = want;

    for (int i = 0; i < kVoices; ++i)
    {
        uint32_t s = want + (uint32_t) i * 0x9E3779B9u;
        Voice& v = voices[(size_t) i];
        v.tolDetune = (rng01 (s) - 0.5f) * 18.0f;        // ± 9 cents at full knob
        v.tolCut    = (rng01 (s) - 0.5f) * 0.14f;
        v.tolEnv    = 0.8f  + rng01 (s) * 0.45f;
        v.tolLevel  = 0.85f + rng01 (s) * 0.3f;
        v.tolLfo    = 0.85f + rng01 (s) * 0.35f;
        v.lfoPhase0 = rng01 (s);
        v.lfoPhase  = v.lfoPhase0;
        v.noiseState = s | 1u;
    }
}

//==============================================================================
void divideClones (int count, int n, int* out)
{
    if (n <= 0) return;
    const int base = count / n;
    int rem = count % n;
    for (int i = 0; i < n; ++i) out[i] = base;
    // an odd remainder over an odd count goes to the exact centre (5-6-5); the
    // rest is handed out in symmetric pairs, working outwards from the middle
    if ((n & 1) && (rem & 1)) { out[n / 2] += 1; --rem; }
    int lo = (n - 1) / 2, hi = n / 2;
    if (n & 1) { --lo; ++hi; }
    while (rem >= 2 && lo >= 0 && hi < n) { out[lo] += 1; out[hi] += 1; rem -= 2; --lo; ++hi; }
    for (int i = 0; rem > 0 && i < n; ++i) { out[i] += 1; --rem; }
}

void Engine::assignNotes (const float* Gp)
{
    // Gather the sounding notes low to high. A note counts as sounding if it
    // is physically held or if EITHER army latches: the division has to agree
    // across the panel, or latching one army would re-shuffle the other.
    const bool latchA = Gp[gLatchA] > 0.5f, latchB = Gp[gLatchB] > 0.5f;
    int  notes[kMaxHeld], onFlag[kMaxHeld], n = 0;
    bool anyHeld = false;
    for (int i = 0; i < kMaxHeld; ++i)
    {
        const int note = heldNote[i].load (std::memory_order_relaxed);
        const int on   = heldOn[i].load (std::memory_order_relaxed);
        if (note < 0) continue;
        if (on == 0 && ! latchA && ! latchB) continue;
        notes[n] = note; onFlag[n] = on; ++n;
        if (on != 0) anyHeld = true;
    }
    for (int i = 1; i < n; ++i)          // insertion sort: the left of the
    {                                    // panel is the bottom of the chord
        const int kn = notes[i], kf = onFlag[i];
        int j = i - 1;
        while (j >= 0 && notes[j] > kn) { notes[j + 1] = notes[j]; onFlag[j + 1] = onFlag[j]; --j; }
        notes[j + 1] = kn; onFlag[j + 1] = kf;
    }

    activeCount  = n;
    armyGated[0] = anyHeld || (n > 0 && latchA);
    armyGated[1] = anyHeld || (n > 0 && latchB);
    for (int v = 0; v < kVoices; ++v) { vAssign[v] = -1; vAssignHeld[v] = 0; }
    if (n == 0) return;                  // nothing sounding: DRONE takes over

    auto place = [&] (int firstVoice, int voiceCount, const int* idx, int idxCount)
    {
        int share[kMaxHeld];
        divideClones (voiceCount, idxCount, share);
        int v = firstVoice;
        for (int k = 0; k < idxCount; ++k)
            for (int c = 0; c < share[k] && v < kVoices; ++c, ++v)
            {
                vAssign[v]     = notes[idx[k]];
                vAssignHeld[v] = onFlag[idx[k]];
            }
    };

    int idx[kMaxHeld];
    const int mode = (int) Gp[gNoteMode];
    if (mode == nmUnison)
    {
        idx[0] = 0;                      // the bottom note: a drone wants a root
        place (0, kVoices, idx, 1);
    }
    else if (mode == nmWar)
    {
        // The armies contest the chord: A takes the lower half, B the upper,
        // and on an odd count they share the middle note. One note and both
        // armies play it; two and it is low on the left, high on the right.
        // THE WAR fader then slides the chord from A's notes to B's.
        const int half = (n + 1) / 2;
        for (int k = 0; k < half; ++k) idx[k] = k;
        place (0, kArmySize, idx, half);
        for (int k = 0; k < half; ++k) idx[k] = n - half + k;
        place (kArmySize, kArmySize, idx, half);
    }
    else
    {
        for (int k = 0; k < n; ++k) idx[k] = k;
        place (0, kVoices, idx, n);      // one body, low on the left
    }
}

void Engine::noteOn (int midiNote)
{
    const uint32_t stamp = heldClock.fetch_add (1);
    for (int i = 0; i < kMaxHeld; ++i)                   // retrigger same note
        if (heldNote[i].load() == midiNote) { heldOn[i].store (1); heldStamp[i].store (stamp); return; }
    for (int i = 0; i < kMaxHeld; ++i)                   // a truly empty seat
        if (heldNote[i].load() < 0) { heldNote[i].store (midiNote); heldOn[i].store (1); heldStamp[i].store (stamp); return; }
    for (int i = 0; i < kMaxHeld; ++i)                   // a released latched remnant
        if (heldOn[i].load() == 0) { heldNote[i].store (midiNote); heldOn[i].store (1); heldStamp[i].store (stamp); return; }
    int oldest = 0; uint32_t best = 0xFFFFFFFFu;         // otherwise steal the oldest
    for (int i = 0; i < kMaxHeld; ++i)
    { const uint32_t st = heldStamp[i].load(); if (st < best) { best = st; oldest = i; } }
    heldNote[oldest].store (midiNote); heldOn[oldest].store (1); heldStamp[oldest].store (stamp);
}

void Engine::noteOff (int midiNote)
{
    for (int i = 0; i < kMaxHeld; ++i)
        if (heldNote[i].load() == midiNote) heldOn[i].store (0);
}

void Engine::allNotesOff()
{
    for (int i = 0; i < kMaxHeld; ++i) { heldNote[i].store (-1); heldOn[i].store (0); }
}

//==============================================================================
float Engine::K35::process (float x, float xPrev, float K, int os)
{
    if (os <= 1) return step (x, K);
    float acc = 0;
    const float inv = 1.0f / (float) os;
    for (int u = 1; u <= os; ++u)
        acc += step (xPrev + (x - xPrev) * ((float) u * inv), K);
    return acc * inv;                    // averaging decimator
}

float Engine::Ladder::process (float x, float xPrev, float k, int os)
{
    if (os <= 1) return step (x, k);
    float acc = 0;
    const float inv = 1.0f / (float) os;
    for (int u = 1; u <= os; ++u)
        acc += step (xPrev + (x - xPrev) * ((float) u * inv), k);
    return acc * inv;
}

float Engine::satOS (float x, float& xPrev, float drive, float norm, int os)
{
    float y;
    if (os <= 1) y = fastTanh (x * drive) * norm;
    else
    {
        float acc = 0;
        const float inv = 1.0f / (float) os;
        for (int u = 1; u <= os; ++u)
            acc += fastTanh ((xPrev + (x - xPrev) * ((float) u * inv)) * drive);
        y = acc * inv * norm;
    }
    xPrev = x;
    return y;
}

float Engine::Env::tick (float atkCoef, float decCoef, float sus, bool loop)
{
    if (stage == 1)                       // attacking
    {
        value += atkCoef * (1.06f - value);
        if (value >= 1.0f) { value = 1.0f; stage = loop ? 3 : 4; }
    }
    else if (stage == 4)                  // held: settle to the sustain level
    {
        value += decCoef * (sus - value);
    }
    else if (stage == 2)                  // released: fall to silence
    {
        value += decCoef * (0.0f - value);
        if (value < 1.0e-4f) { value = 0; stage = 0; }
    }
    else if (stage == 3)                  // loop decay leg
    {
        value += decCoef * (-0.04f - value);
        if (value <= 0.01f) { value = 0.01f; stage = 1; }
    }
    return value;
}

float Engine::DelayLine::read (float delaySamples) const
{
    const int   mask = (int) buf.size() - 1;
    const float rp   = (float) w - delaySamples;
    const int   i0   = (int) std::floor (rp);
    const float fr   = rp - (float) i0;
    const float a = buf[(size_t) (i0 & mask)];
    const float b = buf[(size_t) ((i0 + 1) & mask)];
    return a + fr * (b - a);
}

float Engine::AP::process (float x)
{
    float* d = buf.data();
    const int r = (w - len + (int) buf.size()) & ((int) buf.size() - 1);
    const float z = d[(size_t) r];
    const float y = -g * x + z;
    d[(size_t) w] = x + g * y;
    w = (w + 1) & ((int) buf.size() - 1);
    return y;
}

float Engine::Comb::process (float x)
{
    float* d = buf.data();
    const int r = (w - len + (int) buf.size()) & ((int) buf.size() - 1);
    const float z = d[(size_t) r];
    d[(size_t) w] = x + fb * lp.process (z, 1.0f - damp);
    w = (w + 1) & ((int) buf.size() - 1);
    return z;
}

//==============================================================================
void Engine::controlTick()
{
    for (int g = 0; g < numGlobals; ++g)
        G[g] = globalsIn[(size_t) g].load (std::memory_order_relaxed);
    for (int v = 0; v < kVoices; ++v)
        for (int f = 0; f < numVoiceFields; ++f)
            V[v][f] = voiceIn[(size_t) v][(size_t) f].load (std::memory_order_relaxed);

    // THE RANKS: contrast the continuous panel values around each army's mean.
    // 0.5 leaves the panel as set; toward 0 the clones fall into line, toward 1
    // their differences grow (up to 2.5x). The stored values never move - this
    // reshapes only the working copy, so pulling the slider back to centre
    // always returns the exact patch. Discretes and the faders are untouched.
    {
        const float ranks = std::clamp (G[gRanks], 0.0f, 1.0f);
        if (std::fabs (ranks - 0.5f) > 1.0e-4f)
        {
            const float k = ranks <= 0.5f ? ranks * 2.0f
                                          : 1.0f + (ranks - 0.5f) * 3.0f;
            static constexpr int fields[] = { vfTune, vfCut, vfRes, vfLfoRate,
                                              vfLfoAmp, vfLfoFlt, vfEnvF, vfEnvA,
                                              vfDrift, vfPan };
            for (int army = 0; army < 2; ++army)
            {
                const int v0 = army * kArmySize;
                for (int f : fields)
                {
                    float mean = 0.0f;
                    for (int v = v0; v < v0 + kArmySize; ++v) mean += V[v][f];
                    mean /= (float) kArmySize;
                    const float lo = (f == vfTune || f == vfPan) ? -1.0f : 0.0f;
                    for (int v = v0; v < v0 + kArmySize; ++v)
                        V[v][f] = std::clamp (mean + k * (V[v][f] - mean), lo, 1.0f);
                }
            }
        }
    }

    refreshTolerances();

    // TIDE: two independent minutes-scale random walks (filter, pitch)
    const float dtc = (float) kSubBlock / (float) fs;
    const float leak = dtc / 45.0f;
    tideState  += -tideState  * leak + (rng01 (tideRng) - 0.5f) * 0.09f * std::sqrt (dtc);
    tideState2 += -tideState2 * leak + (rng01 (tideRng) - 0.5f) * 0.09f * std::sqrt (dtc);
    tideState  = std::clamp (tideState,  -1.0f, 1.0f);
    tideState2 = std::clamp (tideState2, -1.0f, 1.0f);
}

//==============================================================================
void Engine::process (float* L, float* R, int n)
{
    int done = 0;
    while (done < n)
    {
        const int m = std::min (kSubBlock, n - done);
        controlTick();

        float* bLA = busLA.data(); float* bRA = busRA.data();
        float* bLB = busLB.data(); float* bRB = busRB.data();
        std::memset (bLA, 0, sizeof (float) * (size_t) m);
        std::memset (bRA, 0, sizeof (float) * (size_t) m);
        std::memset (bLB, 0, sizeof (float) * (size_t) m);
        std::memset (bRB, 0, sizeof (float) * (size_t) m);

        renderVoices (bLA, bRA, bLB, bRB, m);

        // WAR: equal-power A/B with a slew that can take minutes
        const float slewTau  = 0.05f * std::pow (6000.0f, G[gWarSlew]);
        const float slewCoef = 1.0f - std::exp (-1.0f / (slewTau * (float) fs));
        float* oL = L + done; float* oR = R + done;
        for (int i = 0; i < m; ++i)
        {
            warCurrent += slewCoef * (G[gWar] - warCurrent);
            const float a = std::cos (warCurrent * 1.5707963f);
            const float b = std::sin (warCurrent * 1.5707963f);
            oL[i] = bLA[i] * a + bLB[i] * b;
            oR[i] = bRA[i] * a + bRB[i] * b;
        }

        masterChain (oL, oR, m);
        done += m;
    }
}

//==============================================================================
void Engine::renderVoices (float* mixLA, float* mixRA, float* mixLB, float* mixRB, int n)
{
    const int quality = std::clamp ((int) std::lround (G[gHq]), 0, 2);
    const int os = quality == 2 ? 4 : (quality == 1 ? 2 : 1);
    bool soloAny = false;
    for (int v = 0; v < kVoices; ++v) soloAny |= V[v][vfSolo] > 0.5f;
    assignNotes (G);
    if (scatterReq.exchange (0, std::memory_order_relaxed))
        for (int v = 0; v < kVoices; ++v)     // fresh phases, a new draw each press
        {
            scatterState = scatterState * 1664525u + 1013904223u;
            voices[(size_t) v].lfoPhase = (double) rng01 (scatterState);
        }

    const float glideTau  = 0.001f + G[gGlide] * G[gGlide] * 2.0f;
    const float glideCoef = 1.0f - std::exp ((float) (-(double) kSubBlock / (glideTau * fs)));
    const float dtc = (float) kSubBlock / (float) fs;
    const float T   = G[gTolerance];

    for (int vi = 0; vi < kVoices; ++vi)
    {
        Voice& vc = voices[(size_t) vi];
        const float* F = V[vi];
        const bool armyA = vi < kArmySize;
        const int  temper = (int) (armyA ? G[gTemperA] : G[gTemperB]);
        const bool latched = (armyA ? G[gLatchA] : G[gLatchB]) > 0.5f;
        const float entrain = armyA ? G[gEntrainA] : G[gEntrainB];
        const float kbd = armyA ? G[gKbdA] : G[gKbdB];

        // ---- gate & pitch: assignNotes() has already decided which note this
        //      clone follows under the current NOTE MODE.
        const int note = vAssign[vi];
        const int held = vAssignHeld[vi];
        const float base = armyA ? G[gBaseA] : G[gBaseB];
        float targetNote = 24.0f + 36.0f * base;
        bool gate;
        if (note >= 0 && (held != 0 || latched)) { gate = true; targetNote = (float) note; }
        else if (G[gDrone] > 0.5f && ! armyGated[armyA ? 0 : 1]) { gate = true; }
        else                                       gate = false;

        if (gate && ! vc.gated) { vc.envAmp.gateOn(); vc.envFlt.gateOn(); }
        if (! gate && vc.gated) { vc.envAmp.gateOff(); vc.envFlt.gateOff(); }
        vc.gated = gate;

        // ---- drift (per-voice OU) and pitch assembly
        vc.driftState += -vc.driftState * (dtc / 8.0f)
                         + (rng01 (vc.noiseState) - 0.5f) * 0.12f * std::sqrt (dtc);
        const float fan = ((float) vi - 7.5f) / 7.5f;
        const float cents = F[vfTune] * 50.0f
                          + G[gSpread] * 30.0f * fan
                          + vc.tolDetune * T
                          + vc.driftState * 14.0f * F[vfDrift] * G[gDriftMaster]
                          + tideState2 * 7.0f * G[gTide];
        const float noteHz = 440.0f * std::pow (2.0f, (targetNote - 69.0f) / 12.0f);
        const float targetHz = noteHz * footMult ((int) F[vfFoot])
                             * std::pow (2.0f, cents / 1200.0f);
        vc.freqCurrent += glideCoef * (targetHz - vc.freqCurrent);
        const double dt = (double) vc.freqCurrent / fs;

        // ---- filter control values
        const float kTrack = kbd * (targetNote - 48.0f) / 60.0f;
        // Clamped here so the envelope below opens what is LEFT of the range
        // rather than pushing past the ceiling and clamping the knob flat.
        const float cut01base = std::clamp (F[vfCut] + vc.tolCut * T + kTrack
                                            + tideState * 0.12f * G[gTide], 0.0f, 1.0f);
        const float envFiltD = G[gEnvFilt];
        const float res = F[vfRes];
        // Resonance, and for the Sallen-Key models the prewarp that keeps the
        // self-oscillation in tune: the clipper takes a share of the loop and
        // its share GROWS with K (measured -6 cents at K 2.25, -17 at 2.45,
        // -52 at 2.9). Below the oscillation edge the lift shrinks to nothing.
        float filtK, prewarp = 1.0f, ladMakeup = 1.0f;
        if (temper == 2)
        {
            filtK = std::min (4.6f, ladderK (res));
            ladMakeup = 1.0f + LADDER_MAKEUP * filtK;
        }
        else
        {
            filtK = std::min (temper == 1 ? k35Kscream (res) : k35Kgrowl (res),
                              temper == 1 ? 2.95f : 2.45f);
            float warpCents = 0.0f;
            if      (filtK > 2.45f) warpCents = 17.0f + 78.0f * (filtK - 2.45f);
            else if (filtK > 2.25f) warpCents = 6.0f + 55.0f * (filtK - 2.25f);
            else if (filtK > 2.0f)  warpCents = 24.0f * (filtK - 2.0f);
            if (warpCents > 0.0f) prewarp = std::pow (2.0f, warpCents / 1200.0f);
        }

        // ---- LFO / env control values
        float lfoHz;
        if (G[gLfoSync] > 0.5f)
        {
            // Beats per LFO cycle, longest first, so the knob still reads
            // "slow on the left" exactly as the free-running sweep does.
            // Tolerance is deliberately NOT applied: synced means synced.
            static constexpr float kBeats[8] = { 16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f };
            const double bpm = hostBpm.load (std::memory_order_relaxed);
            const int idx = std::clamp ((int) (F[vfLfoRate] * 7.999f), 0, 7);
            lfoHz = (float) (std::max (20.0, bpm) / 60.0) / kBeats[idx];
        }
        else lfoHz = 0.02f * std::pow (400.0f, F[vfLfoRate]) * vc.tolLfo;
        const double lfoDt = (double) lfoHz / fs;
        auto envTimes = [&] (float k, float& atkC, float& decC)
        {
            auto sstep = [] (float a, float b, float x)
            { x = std::clamp ((x - a) / (b - a), 0.0f, 1.0f); return x * x * (3 - 2 * x); };
            const float atkS = 0.004f * std::pow (1250.0f, sstep (0.45f, 1.0f, k)) * vc.tolEnv;
            const float decS = 0.06f  * std::pow (133.0f,  sstep (0.0f, 0.55f, k)) * vc.tolEnv;
            atkC = 1.0f - std::exp ((float) (-1.0 / (atkS * fs)));
            decC = 1.0f - std::exp ((float) (-1.0 / (decS * fs)));
        };
        float aAtk, aDec, fAtk, fDec;
        envTimes (F[vfEnvA], aAtk, aDec);
        envTimes (F[vfEnvF], fAtk, fDec);
        // One knob, so the sustain rides with it: shut = plucky, open = pad.
        const float fSus = 0.15f + 0.8f * F[vfEnvF];
        const bool loop = F[vfLoop] > 0.5f;

        // ---- output gains
        const bool audible = F[vfMute] < 0.5f && (! soloAny || F[vfSolo] > 0.5f);
        const float lvl = audible ? F[vfLevel] * F[vfLevel] * vc.tolLevel : 0.0f;
        const float panv = std::clamp (F[vfPan], -1.0f, 1.0f);
        const float tgtL = lvl * std::cos ((panv + 1.0f) * 0.7853981f);
        const float tgtR = lvl * std::sin ((panv + 1.0f) * 0.7853981f);

        float* mixL = armyA ? mixLA : mixLB;
        float* mixR = armyA ? mixRA : mixRB;
        const int lfoWave = (int) F[vfLfoWave];
        const int wave = (int) F[vfWave];
        const float lfoAmpD = F[vfLfoAmp];
        const float pw = 0.05f + 0.9f * std::clamp (F[vfPw], 0.0f, 1.0f);
        const float lfoFltD = F[vfLfoFlt];
        const double neighborPhase = vi % kArmySize == 0 ? -1.0
                                    : voices[(size_t) vi - 1].phase;
        const float pull = entrain * entrain * 0.004f;

        float peak = vc.meterAcc;

        for (int i = 0; i < n; ++i)
        {
            // -- lfo
            vc.lfoPhase += lfoDt;
            if (vc.lfoPhase >= 1.0)
            {
                vc.lfoPhase -= 1.0;
                vc.shValue = rng01 (vc.noiseState) * 2.0f - 1.0f;
            }
            const float lt = (float) vc.lfoPhase;
            float lfo;
            switch (lfoWave)
            {
                case 1:  lfo = 1.0f - 4.0f * std::fabs (lt - 0.5f); break;
                case 2:  lfo = lt < 0.5f ? 1.0f : -1.0f; break;
                case 3:  lfo = vc.shValue; break;
                default: lfo = std::sin (6.2831853f * lt); break;
            }

            // -- envelopes
            const float ea = vc.envAmp.tick (aAtk, aDec, 1.0f, loop);
            const float ef = vc.envFlt.tick (fAtk, fDec, fSus, loop);

            // -- filter coefficient for this output sample. The envelope opens
            //    what headroom is LEFT above the knob: added flat it pushed
            //    past the ceiling and clamped, killing CUT's whole top half.
            float cut01 = cut01base + ef * envFiltD * (1.0f - cut01base)
                        + lfo * lfoFltD * 0.35f;
            cut01 = std::clamp (cut01, 0.0f, 1.0f);
            vc.cutSmooth += 0.02f * (cut01 - vc.cutSmooth);
            const float fc = 30.0f * std::pow (533.0f, vc.cutSmooth);   // 30 Hz .. 16 kHz
            const float fcC = std::min (fc * prewarp, (float) fs * 0.45f);
            const float g = std::tan (3.14159265f * fcC / ((float) fs * (float) os));
            if (temper == 2) vc.ladder.setG (g); else vc.k35.setG (g);

            // -- oscillator AND filter, both stepped at the oversampled rate.
            //    This used to generate the oscillator once per output sample
            //    and interpolate it up into the filter, which left the polyBLEP
            //    residual exactly where it was: the quality tiers only cleaned
            //    up the filter, and measured saw aliasing stopped improving
            //    past -47 dB however high you set them.
            const float  invOs = 1.0f / (float) os;
            const double dtOs  = dt * (double) invOs;
            auto blep = [dtOs] (double tt)
            {
                if (tt < dtOs)       { const double x = tt / dtOs;         return (float) (x + x - x * x - 1.0); }
                if (tt > 1.0 - dtOs) { const double x = (tt - 1.0) / dtOs; return (float) (x * x + x + x + 1.0); }
                return 0.0f;
            };
            float y = 0.0f;
            for (int u = 0; u < os; ++u)
            {
                if (neighborPhase >= 0.0 && pull > 0.0f)
                {
                    const double np = voices[(size_t) vi - 1].phase;
                    vc.phase += (double) invOs * pull * std::sin (6.2831853 * (np - vc.phase));
                }
                vc.phase += dtOs;
                if (vc.phase >= 1.0) vc.phase -= 1.0;
                if (vc.phase < 0.0)  vc.phase += 1.0;
                const double t = vc.phase;
                float osc;
                if (wave == 3)      osc = std::sin (6.2831853f * (float) t);
                else if (wave == 0) osc = (float) (2.0 * t - 1.0) - blep (t);
                else
                {
                    // PW sets the pulse duty. The triangle keeps a symmetric
                    // square: a pulse of width d carries DC of 2d-1, and the
                    // leaky integrator below would accumulate it.
                    const double d = (wave == 1) ? (double) pw : 0.5;
                    const float sq = (t < d ? 1.0f : -1.0f) + blep (t)
                                   - blep (t >= d ? t - d : t + 1.0 - d);
                    if (wave == 1) osc = sq * 0.85f;
                    else
                    {
                        // triangle: leaky-integrated blepped square. The leak is
                        // proportional to frequency so the amplitude is pitch-
                        // independent (~0.95) and DC drains in ~10 cycles.
                        const float k4dt = (float) (4.0 * dtOs);
                        vc.triState = vc.triState * (1.0f - 0.05f * k4dt) + k4dt * sq;
                        osc = vc.triState;
                    }
                }
                y += (temper == 2) ? vc.ladder.step (osc, filtK) : vc.k35.step (osc, filtK);
            }
            y *= invOs;                              // averaging decimator
            if (temper == 2) y *= ladMakeup;

            // -- amplitude: env * tremolo, smoothed pan/level
            const float trem = 1.0f - lfoAmpD * (0.5f - 0.5f * lfo) * 0.9f;
            const float a = ea * trem;
            vc.ampSmooth += 0.003f * (tgtL - vc.ampSmooth);
            vc.panSmooth += 0.003f * (tgtR - vc.panSmooth);
            const float s = y * a * 0.35f;
            mixL[i] += s * vc.ampSmooth;
            mixR[i] += s * vc.panSmooth;

            const float av = std::fabs (s * (vc.ampSmooth + vc.panSmooth) * 0.7f);
            if (av > peak) peak = av;
        }

        vc.meterAcc = peak * 0.86f;   // decays between sub-blocks
        meters[(size_t) vi].store (std::min (1.0f, vc.meterAcc * 1.6f),
                                   std::memory_order_relaxed);
    }
}

//==============================================================================
void Engine::masterChain (float* L, float* R, int n)
{
    const int quality = std::clamp ((int) std::lround (G[gHq]), 0, 2);
    const int os = quality == 2 ? 4 : (quality == 1 ? 2 : 1);

    const float sat  = G[gBusSat];
    const float satD = 1.0f + 6.0f * sat;
    const float satN = 1.0f / satD;                 // unity small-signal gain
    const float driveAmt = G[gDriveAmt];
    const float driveIn  = 1.0f + 9.0f * driveAmt;
    const float driveOut = 1.0f / (1.0f + 2.0f * driveAmt);

    const float bbdRate  = 0.05f * std::pow (120.0f, G[gBbdRate]);
    const float bbdDepth = G[gBbdDepth];
    const double bbdDt   = (double) bbdRate / fs;
    const float bbdMix   = 0.55f * std::min (1.0f, bbdDepth * 2.5f);
    const float bbdBase  = (float) (0.006 * fs);
    const float bbdSwing = (float) (0.0035 * fs) * bbdDepth;

    const float tapeTime = 0.06f * std::pow (20.0f, G[gTapeTime]);
    const float tapeFb   = 0.05f + G[gTapeFdbk] * 0.72f;
    const float tapeMix  = G[gTapeMix];
    const double wowDt   = 0.5 / fs;

    const bool  freeze   = G[gSpringFreeze] > 0.5f;
    const float dwell    = G[gSpringDwell];
    const float springIn = freeze ? 0.12f : (0.25f + dwell * 0.9f);
    const float springMix = G[gSpringMix];
    for (auto& c : springComb)
    {
        c.fb   = freeze ? 0.997f : (0.72f + dwell * 0.16f);
        c.damp = freeze ? 0.08f : 0.30f;
    }

    const float hpA   = 1.0f - std::exp ((float) (-2.0 * 3.14159265 * 20.0 / fs));
    const float loA   = 1.0f - std::exp ((float) (-2.0 * 3.14159265 * 150.0 / fs));
    const bool  doHpf = G[gHpf] > 0.5f;
    const bool  doBassMono = G[gBassMono] > 0.5f;
    const float width = G[gWidth] * 2.0f;
    const float masterTgt = G[gMaster] * G[gMaster] * 1.4f;

    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];

        // bus soft clip (unity small-signal gain), oversampled per quality
        l = satOS (l, satPrevL, satD, satN, os);
        r = satOS (r, satPrevR, satD, satN, os);

        // drive
        l = satOS (l, drvPrevL, driveIn, driveOut, os);
        r = satOS (r, drvPrevR, driveIn, driveOut, os);

        // BBD chorus
        bbdPhase += bbdDt; if (bbdPhase >= 1.0) bbdPhase -= 1.0;
        const float m1 = std::sin (6.2831853f * (float) bbdPhase);
        const float m2 = std::sin (6.2831853f * ((float) bbdPhase + 0.25f));
        bbdL.push (l);
        bbdR.push (r);
        const float cl = bbdToneL.process (bbdL.read (bbdBase + bbdSwing * m1), 0.28f);
        const float cr = bbdToneR.process (bbdR.read (bbdBase + bbdSwing * m2), 0.28f);
        l = l * (1.0f - bbdMix * 0.5f) + cl * bbdMix * 0.65f;   // honest dry/wet
        r = r * (1.0f - bbdMix * 0.5f) + cr * bbdMix * 0.65f;

        // tape delay
        wowPhase += wowDt; if (wowPhase >= 1.0) wowPhase -= 1.0;
        const float wow = 1.0f + 0.004f * std::sin (6.2831853f * (float) wowPhase);
        const float dSamp = std::min ((float) tapeL.buf.size() - 4.0f,
                                      tapeTime * wow * (float) fs);
        const float tl = tapeToneL.process (tapeL.read (dSamp), 0.20f);
        const float tr = tapeToneR.process (tapeR.read (dSamp), 0.20f);
        tapeL.push (l + tr * tapeFb);              // ping-pong-ish cross feedback
        tapeR.push (r + tl * tapeFb);
        l += tl * tapeMix;
        r += tr * tapeMix;

        // spring tank (mono in, decorrelated comb taps out)
        float tank = (l + r) * 0.5f * springIn;
        for (auto& ap : springAp) tank = ap.process (tank);
        const float c0 = springComb[0].process (tank);
        const float c1 = springComb[1].process (tank);
        const float c2 = springComb[2].process (tank);
        const float sl = (c0 + 0.8f * c1 + 0.55f * c2) * 0.33f;
        const float sr = (0.55f * c0 + 0.8f * c1 + c2) * 0.33f;
        l += sl * springMix;
        r += sr * springMix;

        // bass mono
        if (doBassMono)
        {
            const float lowL = bassLpL.process (l, loA);
            const float lowR = bassLpR.process (r, loA);
            const float lowM = (lowL + lowR) * 0.5f;
            l += lowM - lowL;
            r += lowM - lowR;
        }

        // infrasonic high-pass (2-pole)
        if (doHpf)
        {
            l -= hpfL2.process (hpfL1.process (l, hpA), hpA);
            r -= hpfR2.process (hpfR1.process (r, hpA), hpA);
        }

        // width
        {
            const float mid  = (l + r) * 0.5f;
            const float side = (l - r) * 0.5f * width;
            l = mid + side;
            r = mid - side;
        }

        // master gain, then a transparent safety clip: unity below ~0.7,
        // tanh knee only above that (was a full tanh, which colored
        // everything all the time — measured ~1% THD on clean material).
        masterSmooth += 0.002f * (masterTgt - masterSmooth);
        l *= masterSmooth;
        r *= masterSmooth;
        auto knee = [] (float x)
        {
            const float a = std::fabs (x);
            if (a <= 0.7f) return x;                       // bit-transparent
            const float y = 0.7f + 0.3f * fastTanh ((a - 0.7f) / 0.3f);
            return x < 0 ? -y : y;                         // slope-continuous
        };
        l = knee (l);
        r = knee (r);

        L[i] = l; R[i] = r;
    }

    // master meters
    float pl = meterL.load (std::memory_order_relaxed) * 0.9f;
    float pr = meterR.load (std::memory_order_relaxed) * 0.9f;
    for (int i = 0; i < n; ++i)
    {
        pl = std::max (pl, std::fabs (L[i]));
        pr = std::max (pr, std::fabs (R[i]));
    }
    meterL.store (std::min (1.5f, pl), std::memory_order_relaxed);
    meterR.store (std::min (1.5f, pr), std::memory_order_relaxed);
}

} // namespace cw
