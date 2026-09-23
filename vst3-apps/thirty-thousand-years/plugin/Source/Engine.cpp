#include "Engine.h"

namespace tty
{

//==============================================================================
const char* Engine::stageName (int i)
{
    static const char* N[NUM_STAGES] = { "MASS", "SIGNAL", "MEMORY", "STRUCT", "mix+strip", "mix+laneA",
                                         "send", "send+laneB", "space wet", "loop send", "loop ret",
                                         "pre-limit", "out" };
    return N[i < 0 ? 0 : (i >= NUM_STAGES ? NUM_STAGES - 1 : i)];
}

/*  A stage's peak over this sub-block, held with a slow decay. */
static inline void stageHit (float& hold, const float* a, const float* b, int n)
{
    float p = 0.0f;
    for (int i = 0; i < n; ++i) { p = std::max (p, std::abs (a[i])); if (b) p = std::max (p, std::abs (b[i])); }
    hold = std::max (p, hold * 0.98f);
}

int chordIntervals (int chord, int* o)
{
    switch (chord)
    {
        case 1:  o[0] = 0; o[1] = 12; return 2;
        case 2:  o[0] = 0; o[1] = 7; return 2;
        case 3:  o[0] = 0; o[1] = 3; o[2] = 7; return 3;
        case 4:  o[0] = 0; o[1] = 4; o[2] = 7; return 3;
        case 5:  o[0] = 0; o[1] = 2; o[2] = 7; return 3;
        case 6:  o[0] = 0; o[1] = 3; o[2] = 7; o[3] = 10; return 4;
        case 7:  o[0] = 0; o[1] = 1; o[2] = 2; return 3;
        case 8:  o[0] = 0; o[1] = 6; return 2;
        case 9:  o[0] = 0; o[1] = 7; o[2] = 12; o[3] = 19; return 4;
        default: o[0] = 0; return 1;
    }
}

static const int SCALES[8][12] = {
    { 0,1,2,3,4,5,6,7,8,9,10,11 },              // chromatic
    { 0,2,3,5,7,8,10, -1 },                      // minor
    { 0,1,3,5,7,8,10, -1 },                      // phrygian
    { 0,1,3,5,6,8,10, -1 },                      // locrian
    { 0,2,3,5,7,8,11, -1 },                      // harmonic minor
    { 0,2,4,6,8,10, -1 },                        // whole tone
    { 0,3,5,7,10, -1 },                          // pentatonic minor
    { 0,2,3,5,7,9,10, -1 }                       // dorian
};

//==============================================================================
/*  The macro maps every instrument starts from; a preset may override any. */
void Engine::defaultMacroMaps()
{
    for (int m = 0; m < NUM_MACROS; ++m) for (int d = 0; d < MACRO_DESTS; ++d) macro[m][d] = MacroDest();
    auto set = [&] (int m, int d, const char* id, float depth) { macro[m][d].dst = paramIndex (id); macro[m][d].depth = depth; };
    set (0, 0, "m_sublvl", 0.45f); set (0, 1, "m_reinf", 0.5f); set (0, 2, "m_gain", 0.12f); set (0, 3, "st_exclvl", 0.2f); set (0, 4, "m_cut", -0.12f); set (0, 5, "e_rv_low", 0.2f);
    set (1, 0, "m_beat", 0.4f); set (1, 1, "m_o2fine", 0.05f); set (1, 2, "s_addspread", 0.15f); set (1, 3, "s_wt1pos", 0.35f); set (1, 4, "m_res", 0.25f); set (1, 5, "e_rv_low", 0.25f); set (1, 6, "mem_stilt", -0.2f);
    set (2, 0, "e_mb_mid", 0.5f); set (2, 1, "e_mb_high", 0.45f); set (2, 2, "e_fold_amt", 0.4f); set (2, 3, "st_exclvl", 0.3f); set (2, 4, "st_stress", 0.5f); set (2, 5, "e_sat_drive", 0.4f); set (2, 6, "m_fdrive", 0.4f);
    set (3, 0, "m_drift", 0.5f); set (3, 1, "m_pwdrift", 0.5f); set (3, 2, "s_addmotion", 0.5f); set (3, 3, "mem_jit", 0.4f); set (3, 4, "st_stiff", 0.2f); set (3, 5, "m_unidet", 0.3f); set (3, 6, "r1_amt", 0.4f); set (3, 7, "v1_prob", 0.3f);
    set (4, 0, "s_ring", 0.5f); set (4, 1, "e_fb_send", 0.35f); set (4, 2, "st_couple", 0.4f); set (4, 3, "s_interf", 0.3f); set (4, 4, "mem_pspread", 0.2f); set (4, 5, "m_loop", 0.3f);
    set (5, 0, "e_distance", 0.8f); set (5, 1, "e_rv_mix", 0.3f); set (5, 2, "m_lp", -0.35f); set (5, 3, "e_scale", 0.25f); set (5, 4, "s_lp", -0.3f);
    set (6, 0, "l_coupling", 0.5f); set (6, 1, "mem_dens", 0.2f); set (6, 2, "v1_prob", 0.4f); set (6, 3, "e_rv_mod", 0.3f); set (6, 4, "m_drift", 0.2f); set (6, 5, "l_autonomy", 0.4f); set (6, 6, "st_sustain", 0.2f);
    set (7, 0, "mem_gain", 0.3f); set (7, 1, "mem_erosion", -0.5f); set (7, 2, "s_addfund", 0.3f); set (7, 3, "e_sat_drive", -0.2f); set (7, 4, "m_res", -0.1f); set (7, 5, "st_stress", -0.3f);
}

Engine::Engine() { defaultMacroMaps(); }

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate; maxBlock = maxBlockSize;
    if (! waves.built) waves.build();
    seedBase = (uint32_t) p.li (P_seed);
    rng.seed (seedBase * 131u + 7u);
    mem.prepare (sr, seedBase * 17u + 3u);
    life.prepare (sr, seedBase * 23u + 5u);
    for (int i = 0; i < 4; ++i) chan[i].prepare (sr);
    const int q = p.li (P_quality); const int factor = q == 0 ? 1 : (q == 1 ? 2 : 4);
    laneA.prepare (sr, factor); laneB.prepare (sr, factor);
    space.prepare (sr); loop.prepare (sr); out.prepare (sr);
    for (int v = 0; v < MAX_VOICES; ++v)
    {
        voices[v].id = v;
        voices[v].mass.prepare (sr, seedBase * 101u + (uint32_t) v * 7u + 1u);
        voices[v].sig.prepare (sr, seedBase * 103u + (uint32_t) v * 11u + 2u);
        voices[v].st.prepare (sr, seedBase * 107u + (uint32_t) v * 13u + 3u);
        voices[v].vrnd = hash01 (seedBase, (uint32_t) v, 99u) * 2.0f - 1.0f;
        for (int k = 0; k < NUM_LFO; ++k) { voices[v].lfoV[k].rng.seed (seedBase * 9u + (uint32_t) (v * 8 + k) + 1u); voices[v].lfoV[k].ph = (float) v / MAX_VOICES; }
        lfoTremPh[v] = std::fmod (v * 0.618f + 0.5f, 1.0f);
    }
    specFft.prepare (SPEC_N); specRe.assign (SPEC_N, 0.0f); specIm.assign (SPEC_N, 0.0f);
    specWin.resize (SPEC_N); for (int i = 0; i < SPEC_N; ++i) specWin[(size_t) i] = 0.5f - 0.5f * std::cos (TAU * i / SPEC_N);
    duckFollow.setTau (0.01f, sr);
    eff = p; effTarget = p; effPrimed = false;
    slewA = 1.0f - std::exp (-(float) CTRL / ((float) sr * 0.008f));   // 8 ms: a cutoff jump lands in a few ticks, a fast LFO is untouched
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) { v.mass.reset(); v.sig.reset(); v.st.reset(); v.note = -1; v.gate = false; v.drone = false; v.sustained = false; v.gateSm = 0.0f; for (auto& e : v.envV) e.kill(); for (auto& m : v.msegV) { m.running = false; m.gate = false; } }
    mem.reset(); life.reset(); space.reset(); loop.reset(); out.lim.reset();
    monoDepth = 0; wasDrone = false; lastDroneChord = -1; lastDroneRoot = -1;
    panicFade = false; panicGain = 1.0f;
    freezeOverride = 0.0f; loopKick = 0.0f; histNudge = 0.0f;
    std::fill (std::begin (loopRetL), std::end (loopRetL), 0.0f); std::fill (std::begin (loopRetR), std::end (loopRetR), 0.0f);
    std::fill (std::begin (loopRetMono), std::end (loopRetMono), 0.0f); std::fill (std::begin (memMono), std::end (memMono), 0.0f);
    std::fill (std::begin (extMono), std::end (extMono), 0.0f);
    for (auto& v : uiNotes) v = -1;
    for (auto& v : uiLevels) v = 0.0f;
}

void Engine::reseed (uint32_t s)
{
    seedBase = s;
    rng.seed (s * 131u + 7u);
    life.rephase (s * 23u + 5u);
    mem.rng.seed (s * 17u + 3u);
    for (int v = 0; v < MAX_VOICES; ++v)
    {
        voices[v].mass.rng.seed (s * 101u + (uint32_t) v * 7u + 1u);
        voices[v].mass.drift1.reset(); voices[v].mass.drift2.reset(); voices[v].mass.pwWalk.reset(); voices[v].mass.ampWalk.reset();
        voices[v].sig.rng.seed (s * 103u + (uint32_t) v * 11u + 2u);
        voices[v].st.rng.seed (s * 107u + (uint32_t) v * 13u + 3u);
        voices[v].vrnd = hash01 (s, (uint32_t) v, 99u) * 2.0f - 1.0f;
        for (int k = 0; k < NUM_LFO; ++k) { voices[v].lfoV[k].rng.seed (s * 9u + (uint32_t) (v * 8 + k) + 1u); voices[v].lfoV[k].ph = (float) v / MAX_VOICES; }
    }
}

//==============================================================================
float Engine::scaleSnap (float note) const
{
    const int sc = p.li (P_scale);
    if (sc <= 0) return note;
    if (sc == 9) return note;                              // Scala: handled in noteToHz
    const int* s = SCALES[sc - 1];
    const int root = p.li (P_drone_root) % 12;
    const int n = (int) std::lround (note);
    int best = n, bestD = 99;
    for (int d = -6; d <= 6; ++d)
    {
        const int cand = n + d;
        const int deg = ((cand - root) % 12 + 12) % 12;
        bool inScale = false;
        for (int k = 0; k < 12 && s[k] >= 0; ++k) if (s[k] == deg) { inScale = true; break; }
        if (inScale && std::abs (d) < bestD) { bestD = std::abs (d); best = cand; }
    }
    return (float) best + (note - (float) n);
}

float Engine::noteToHz (float note) const
{
    const float tune = lawSemi (paramSpec (P_tune), p[P_tune]) + lawSemi (paramSpec (P_fine), p[P_fine]) / 100.0f;
    if (p.li (P_scale) == 9 && scalaN > 0)
    {
        const int n = (int) std::lround (note);
        const int rel = n - 60;
        const int oct = (int) std::floor ((float) rel / (float) scalaN);
        const int deg = ((rel % scalaN) + scalaN) % scalaN;
        const float cents = (deg == 0 ? 0.0f : scalaCents[deg - 1]) + (float) oct * scalaPeriod;
        return 261.6256f * fexp2 ((cents + (note - (float) n) * 100.0f) / 1200.0f) * fexp2 (tune / 12.0f);
    }
    return midiHz (scaleSnap (note) + tune);
}

//==============================================================================
int Engine::allocVoice (int note, int channel)
{
    const int budget = clampi (p.li (P_voices), 1, MAX_VOICES);
    int droneCount = 0; for (auto& v : voices) if (v.drone && v.active()) ++droneCount;
    const int keyBudget = std::max (1, budget - droneCount);
    // same note already playing on keys: reuse
    for (auto& v : voices) if (! v.drone && v.note == note && v.gate && v.channel == channel) return v.id;
    // a free one
    int used = 0; for (auto& v : voices) if (! v.drone && v.active()) ++used;
    if (used < keyBudget)
        for (auto& v : voices) if (! v.drone && ! v.active()) return v.id;
    // steal: the oldest released, else the oldest
    int best = -1, bestAge = 1 << 30;
    for (auto& v : voices) if (! v.drone && ! v.gate && v.age < bestAge) { bestAge = v.age; best = v.id; }
    if (best >= 0) return best;
    for (auto& v : voices) if (! v.drone && v.age < bestAge) { bestAge = v.age; best = v.id; }
    if (best < 0) for (auto& v : voices) if (! v.active()) return v.id;
    return best < 0 ? 0 : best;
}

void Engine::startVoice (Voice& v, int note, float vel, int channel, bool drone, bool legato)
{
    const bool wasActive = v.active();
    v.note = note; v.vel = vel; v.channel = channel; v.gate = true; v.drone = drone; v.sustained = false; v.age = ++ageCounter;
    v.hzTarget = noteToHz ((float) note);
    const float glide = p[P_glide] < 0.01f ? 0.0f : lawHz (paramSpec (P_glide), p[P_glide]);
    if (! wasActive || glide <= 0.0f) v.hzCur = v.hzTarget;
    if (! wasActive) { v.mpeBend = 0.0f; v.press = 0.0f; }
    v.mass.noteOn (vel, legato && wasActive, p);
    v.sig.noteOn (vel, legato && wasActive);
    v.st.noteOn (vel, legato && wasActive, p.sw (P_st_strikeon), p[P_st_strikelvl]);
    for (int k = 0; k < NUM_ENV; ++k) if (p.li (P_e1_trig + k * 5) == 0 && (! legato || ! wasActive)) v.envV[k].on (vel);
    for (int k = 0; k < 4; ++k) if (life.mseg[k].trig == 0 && (! legato || ! wasActive)) v.msegV[k].on();
    if (! wasActive) for (int k = 0; k < NUM_LFO; ++k) if (p.li (P_l1_scope + k * 5) == 1) v.lfoV[k].ph = p[P_l1_phase + k * 5];
    stopped = false;
}

void Engine::releaseVoice (Voice& v)
{
    v.gate = false;
    v.mass.noteOff(); v.sig.noteOff(); v.st.noteOff();
    for (auto& e : v.envV) e.off();
    for (auto& m : v.msegV) m.off();
}

void Engine::noteOn (int note, float vel, int channel)
{
    note = clampi (note, 0, 127);
    const int mode = p.li (P_vmode);
    if (mode == 0)
    {
        Voice& v = voices[allocVoice (note, channel)];
        startVoice (v, note, vel, channel, false, false);
    }
    else
    {
        // MONO / LEGATO: one key voice (the last slot that is not a drone), a note stack
        if (monoDepth < 16) monoStack[monoDepth++] = note;
        int slot = MAX_VOICES - 1; for (int i = MAX_VOICES - 1; i >= 0; --i) if (! voices[i].drone) { slot = i; break; }
        Voice& v = voices[slot];
        startVoice (v, note, vel, channel, false, mode == 2);
    }
}

void Engine::noteOff (int note, int channel)
{
    const int mode = p.li (P_vmode);
    if (mode == 0)
    {
        for (auto& v : voices)
            if (! v.drone && v.note == note && v.gate && (channel == 0 || v.channel == channel))
            {
                if (sustain) v.sustained = true; else releaseVoice (v);
            }
        return;
    }
    // remove from the stack
    for (int i = 0; i < monoDepth; ++i) if (monoStack[i] == note) { for (int j = i; j + 1 < monoDepth; ++j) monoStack[j] = monoStack[j + 1]; --monoDepth; break; }
    int slot = MAX_VOICES - 1; for (int i = MAX_VOICES - 1; i >= 0; --i) if (! voices[i].drone) { slot = i; break; }
    Voice& v = voices[slot];
    if (v.note != note || ! v.gate) return;
    if (monoDepth > 0) { const int prev = monoStack[monoDepth - 1]; startVoice (v, prev, v.vel, channel, false, true); }
    else if (sustain) v.sustained = true; else releaseVoice (v);
}

void Engine::setBend (float b, int channel)
{
    if (p.sw (P_mpe) && channel >= 2 && channel <= 16) { chanBend[channel] = b; for (auto& v : voices) if (v.channel == channel) v.mpeBend = b; }
    else bend = b;
}
void Engine::setAftertouch (float a, int channel)
{
    if (p.sw (P_mpe) && channel >= 2 && channel <= 16) { chanPress[channel] = a; for (auto& v : voices) if (v.channel == channel) v.press = a; }
    else for (auto& v : voices) v.press = a;
}
void Engine::setPolyAftertouch (int note, float a) { for (auto& v : voices) if (v.note == note && v.gate) v.press = a; }
void Engine::setSustain (bool on)
{
    sustain = on;
    if (! on) for (auto& v : voices) if (v.sustained) { v.sustained = false; releaseVoice (v); }
}
void Engine::allNotesOff() { for (auto& v : voices) if (! v.drone) releaseVoice (v); monoDepth = 0; }
void Engine::panic() { panicFade = true; }
void Engine::strike (float amt)
{
    const float a = amt < 0.0f ? p[P_st_strikelvl] : amt;
    bool any = false;
    for (auto& v : voices) if (v.active()) { v.st.strike (a); any = true; }
    if (! any) { Voice& v = voices[0]; startVoice (v, p.li (P_drone_root), 0.8f, 0, false, false); releaseVoice (v); v.st.strike (a); }
    uiFracture = true;
}

//==============================================================================
void Engine::handleDrone()
{
    const bool want = p.sw (P_drone) && ! stopped;
    const int chord = p.li (P_drone_chord), root = p.li (P_drone_root);
    if (want && (! wasDrone || chord != lastDroneChord))
    {
        // (re)start the chord voices
        for (auto& v : voices) if (v.drone) releaseVoice (v), v.drone = false;
        int iv[4]; const int cnt = chordIntervals (chord, iv);
        const int budget = clampi (p.li (P_voices), 1, MAX_VOICES);
        int placed = 0;
        for (int k = 0; k < cnt && placed < budget; ++k)
        {
            // take the lowest free non-key voice
            int slot = -1;
            for (int i = 0; i < MAX_VOICES; ++i) if (! voices[i].active()) { slot = i; break; }
            if (slot < 0) for (int i = 0; i < MAX_VOICES; ++i) if (! voices[i].gate) { slot = i; break; }
            if (slot < 0) break;
            Voice& v = voices[slot];
            startVoice (v, root + iv[k], p[P_drone_vel], 0, true, false);
            v.rootLock = (k == 0);
            ++placed;
        }
        lastDroneChord = chord; lastDroneRoot = root;
    }
    else if (want && root != lastDroneRoot)
    {
        int iv[4]; const int cnt = chordIntervals (chord, iv); int k = 0;
        for (auto& v : voices) if (v.drone && v.gate && k < cnt) { v.note = root + iv[k++]; v.hzTarget = noteToHz ((float) v.note); }
        lastDroneRoot = root;
    }
    else if (! want && wasDrone)
    {
        for (auto& v : voices) if (v.drone) { releaseVoice (v); v.drone = false; }
    }
    if (want && p.sw (P_drone_latch) == false) { /* unlatched: the chord is a held gesture — the drone switch is the gate, so nothing to do */ }
    wasDrone = want;
}

void Engine::handleEvents()
{
    for (int i = 0; i < NUM_EVT; ++i)
    {
        if (! life.evt[i].fired) continue;
        const float amt = life.evt[i].firedAmt;
        switch (p.li (P_v1_target + i * 5))
        {
            case 0: strike (amt); break;
            case 1: mem.clusterLeft = 3 + (int) (amt * 8.0f); mem.schedLeft = 0.0f; break;
            case 2: freezeOverride = 0.5f + 3.0f * amt; break;
            case 3: break;                                                   // EROSION PULSE: consumed by the network
            case 4: loopKick = amt; break;
            case 5: for (auto& v : voices) { v.sig.ph1 = v.sig.ph2 = 0.0f; for (auto& w : v.sig.pmot) w.reset(); } break;
            case 6: histNudge += 0.03f * amt; break;
            default: break;
        }
    }
}

void Engine::applyHistory()
{
    // position
    float pos = p[P_h_pos];
    const int mode = p.li (P_h_mode);
    if (p.sw (P_h_on) && mode > 0)
    {
        if (! p.sw (P_h_hold))
        {
            float dur = lawHz (paramSpec (P_h_dur), p[P_h_dur]);
            if (mode == 2 && bpm > 1.0) { static const float BARS[8] = { 1, 2, 4, 8, 16, 32, 64, 128 }; dur = BARS[p.li (P_h_bars)] * 4.0f * 60.0f / (float) bpm; }
            const float step = (float) CTRL / (float) sr / std::max (0.1f, dur);
            const int lp = p.li (P_h_loop);
            if (! hDone)
            {
                hAuto += step * hDir;
                if (hAuto >= 1.0f) { if (lp == 0) { hAuto = 1.0f; hDone = true; } else if (lp == 1) hAuto -= 1.0f; else { hAuto = 2.0f - hAuto; hDir = -1.0f; } }
                if (hAuto <= 0.0f && hDir < 0.0f) { hAuto = -hAuto; hDir = 1.0f; }
            }
        }
        pos = hAuto;
    }
    else { hAuto = pos; hDone = false; hDir = 1.0f; }
    pos = clamp01 (pos + histNudge); histNudge *= 0.98f;
    historyPos = pos;

    if (! p.sw (P_h_on)) { effTarget = p; return; }
    // scenes along the path: 0 -> sc2 -> sc3 -> 1, skipping scenes that were never stored
    float sp[4] = { 0.0f, clamp01 (p[P_h_sc2pos]), clamp01 (p[P_h_sc3pos]), 1.0f };
    int order[4]; int cnt = 0;
    for (int i = 0; i < 4; ++i) if (sceneSet[i]) order[cnt++] = i;
    effTarget = p;
    if (cnt == 0) return;
    if (cnt == 1) { const Params& a = scene[order[0]]; for (int i = 0; i < NUM_PARAMS; ++i) if (paramInScene (paramSpec (i))) effTarget.v[i] = a.v[i]; return; }
    int ia = order[0], ib = order[cnt - 1]; float ta = sp[order[0]], tb = sp[order[cnt - 1]];
    for (int k = 0; k + 1 < cnt; ++k)
        if (pos >= sp[order[k]] && pos <= sp[order[k + 1]]) { ia = order[k]; ib = order[k + 1]; ta = sp[ia]; tb = sp[ib]; break; }
    const float t = tb > ta ? clamp01 ((pos - ta) / (tb - ta)) : 0.0f;
    const Params& A = scene[ia]; const Params& B = scene[ib];
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        const PSpec& s = paramSpec (i);
        if (! paramInScene (s)) continue;
        if (paramStepped (s)) effTarget.v[i] = t < 0.5f ? A.v[i] : B.v[i];
        else effTarget.v[i] = lerp (A.v[i], B.v[i], t);      // normalised space is already log for Hz/ms, linear for semitones
    }
}

//==============================================================================
void Engine::tickControl (int n)
{
    const float dt = (float) n / (float) sr;
    // transport edges: deterministic mode re-seeds at a start or a jump back
    if (p.sw (P_determin))
    {
        const bool start = playing && ! wasPlaying;
        const bool jump = playing && lastPpq >= 0.0 && ppq < lastPpq - 0.01;
        if (start || jump) reseed ((uint32_t) p.li (P_seed));
    }
    wasPlaying = playing; lastPpq = ppq;

    applyHistory();

    bool gateAny = false; for (auto& v : voices) if (v.gate) gateAny = true;
    life.hold = p.sw (P_l_hold);
    life.tick (effTarget, dt, bpm, ppq, playing, gateAny, wheel, bend, historyPos);
    handleEvents();

    // offsets: matrix + network + macros, onto the effective values, clamped to each range
    for (int m = 0; m < NUM_MACROS; ++m)
    {
        const float mv = effTarget[P_mac_mass + m];
        if (mv <= 0.0f) continue;
        for (int d = 0; d < MACRO_DESTS; ++d)
        {
            const MacroDest& md = macro[m][d];
            if (md.dst < 0 || md.dst >= NUM_PARAMS || md.depth == 0.0f) continue;
            life.offs[md.dst] += mv * md.depth * (paramStepped (paramSpec (md.dst)) ? paramMax (paramSpec (md.dst)) : 1.0f);
        }
    }
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        if (life.offs[i] == 0.0f) continue;
        const PSpec& s = paramSpec (i);
        effTarget.v[i] = clampf (effTarget.v[i] + life.offs[i], 0.0f, paramMax (s));
    }
    if (freezeOverride > 0.0f) { freezeOverride -= dt; effTarget.v[P_mem_freeze] = 1.0f; }
    // the slew: every continuous value approaches its target over ~8 ms, so a knob or a
    // HISTORY jump cannot step the audio; stepped values switch at once (measured: test [10b])
    if (! effPrimed) { eff = effTarget; effPrimed = true; }
    else for (int i = 0; i < NUM_PARAMS; ++i)
    {
        if (paramStepped (paramSpec (i))) eff.v[i] = effTarget.v[i];
        else eff.v[i] += (effTarget.v[i] - eff.v[i]) * slewA;
    }
    uiErosion = life.net.erosion;
}

void Engine::renderVoices (int n)
{
    for (int b = 0; b < 4; ++b) { std::fill (busL[b], busL[b] + n, 0.0f); std::fill (busR[b], busR[b] + n, 0.0f); }
    const bool perVoice = life.anyPerVoice();
    const float dt = (float) n / (float) sr;
    const int mode = p.li (P_vmode);
    const float bendRange = (float) p.li (P_bend);
    const bool rootLock = p.sw (P_rootlock);
    const int excKind = eff.li (P_st_exc);
    const int loopTo = eff.li (P_e_fb_to);
    const float spread = eff[P_drone_spread];
    int used = 0;
    // find the lowest sounding voice for ROOT LOCK
    int lowest = -1; float lowHz = 1.0e9f;
    for (auto& v : voices) if (v.active() && v.hzCur < lowHz) { lowHz = v.hzCur; lowest = v.id; }

    for (auto& v : voices)
    {
        uiNotes[(size_t) v.id] = v.active() ? v.note : -1;
        if (! v.active()) { uiLevels[(size_t) v.id] = 0.0f; continue; }
        ++used;
        // glide
        const float glide = p[P_glide] < 0.01f ? 0.0f : lawHz (paramSpec (P_glide), p[P_glide]) * 0.001f;
        if (glide > 0.0f && std::abs (v.hzCur - v.hzTarget) > 1.0e-3f)
        {
            const float a = 1.0f - std::exp (-dt / glide);
            v.hzCur = v.hzCur * std::pow (v.hzTarget / v.hzCur, a);
        }
        else v.hzCur = v.hzTarget;
        v.gateSm += ((v.gate ? 1.0f : 0.0f) - v.gateSm) * (1.0f - std::exp (-dt / 0.05f));

        VoiceCtl c;
        const float bendSemi = (p.sw (P_mpe) && v.channel >= 2 ? v.mpeBend * 48.0f : bend * bendRange);
        c.hz = v.hzCur * fexp2 (bendSemi / 12.0f);
        c.vel = v.vel; c.gate = v.gateSm; c.press = v.press;
        c.fan = std::fmod ((float) v.id * 0.618f + 0.5f, 1.0f) * 2.0f - 1.0f;
        const bool locked = rootLock && (v.rootLock || v.id == lowest);
        c.wmDet = locked ? 0.0f : wmDet * c.fan;
        c.wmPan = wmPan * c.fan * 0.5f;
        c.wmSag = wmSag;
        c.wmFilt = wmFilt;
        if (wmTrem > 0.0f)
        {
            lfoTremPh[v.id] += wmTremRate * (1.0f + 0.1f * c.fan) * dt; lfoTremPh[v.id] -= std::floor (lfoTremPh[v.id]);
            c.wmTrem = 1.0f - wmTrem * 0.5f * (1.0f + fsin (lfoTremPh[v.id]));
        }
        // per-voice modulators
        for (int k = 0; k < NUM_LFO; ++k)
            if (p.li (P_l1_scope + k * 5) == 1)
            {
                const int bb = P_l1_wave + k * 5;
                v.lfoVal[k] = v.lfoV[k].tick (p.li (bb), lawHz (paramSpec (bb + 1), p[bb + 1]), p.li (bb + 2), 0.0f, dt, bpm, ppq, playing);
            }
            else v.lfoVal[k] = life.lfoVal[k];
        for (int k = 0; k < NUM_ENV; ++k)
        {
            const int bb = P_e1_atk + k * 5;
            if (p.li (bb + 4) == 0) { v.envV[k].set (lawHz (paramSpec (bb), p[bb]), lawHz (paramSpec (bb + 1), p[bb + 1]), p[bb + 2], lawHz (paramSpec (bb + 3), p[bb + 3]), sr / n); v.envVal[k] = v.envV[k].tick(); }
            else v.envVal[k] = life.src[MS_ENV1 + k];
        }
        for (int k = 0; k < 4; ++k) v.msegVal[k] = life.mseg[k].trig == 0 ? v.msegV[k].tick (life.mseg[k], dt) : life.src[MS_MSEG1 + k];

        const Params* pp = &eff;
        if (perVoice)
        {
            voiceP = eff;
            float vo[NUM_PARAMS]; std::fill (std::begin (vo), std::end (vo), 0.0f);
            life.voiceOffsets (vo, v.vel, v.press, ((float) v.note - 60.0f) / 48.0f, v.vrnd, v.envVal, v.lfoVal, v.msegVal);
            for (int i = 0; i < NUM_PARAMS; ++i) if (vo[i] != 0.0f) voiceP.v[i] = clampf (voiceP.v[i] + vo[i], 0.0f, paramMax (paramSpec (i)));
            pp = &voiceP;
        }
        const Params& q = *pp;
        if (locked) { /* root lock: no drift on this voice */ }

        // ---- MASS
        const bool massOn = q.sw (P_m_on) && ((v.drone && q.sw (P_m_drone)) || (! v.drone && q.sw (P_m_keys)));
        if (massOn) v.mass.render (vMass, n, c, q, vSub); else { std::fill (vMass, vMass + n, 0.0f); std::fill (vSub, vSub + n, 0.0f); if (v.mass.active()) v.mass.aenv.tick(); }
        // ---- SIGNAL
        const bool sigOn = q.sw (P_s_on) && ((v.drone && q.sw (P_s_drone)) || (! v.drone && q.sw (P_s_keys)));
        if (sigOn) v.sig.render (vSig, n, c, q, waves, loopTo == 3 ? loopRetMono[0] : 0.0f); else { std::fill (vSig, vSig + n, 0.0f); if (v.sig.active()) v.sig.aenv.tick(); }
        // ---- STRUCTURE
        const bool stOn = q.sw (P_st_on) && ((v.drone && q.sw (P_st_drone)) || (! v.drone && q.sw (P_st_keys)));
        if (stOn)
        {
            const float* exc = vMass;
            switch (excKind) { case 4: exc = vSig; break; case 5: exc = memMono; break; case 6: exc = loopRetMono; break; case 7: exc = extMono; break; default: exc = vMass; break; }
            v.st.render (vSt, exc, n, c, q);
        }
        else { std::fill (vSt, vSt + n, 0.0f); if (v.st.active()) v.st.aenv.tick(); }

        // ---- pan and sum: MASS / SIGNAL / STRUCTURE buses; the sub stays centred
        const float vp = clampf (0.5f + c.fan * spread * 0.5f + c.wmPan, 0.0f, 1.0f);
        const float gl = std::cos (vp * PI * 0.5f), gr = std::sin (vp * PI * 0.5f);
        float lvl = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            busL[0][i] += vMass[i] * gl + vSub[i] * 0.707f; busR[0][i] += vMass[i] * gr + vSub[i] * 0.707f;
            busL[1][i] += vSig[i] * gl;  busR[1][i] += vSig[i] * gr;
            busL[3][i] += vSt[i] * gl;   busR[3][i] += vSt[i] * gr;
            lvl += vMass[i] * vMass[i] + vSig[i] * vSig[i] + vSt[i] * vSt[i];
        }
        v.actLevel = 0.8f * v.actLevel + 0.2f * std::sqrt (lvl / n);
        uiLevels[(size_t) v.id] = clamp01 (v.actLevel * 4.0f);
        if (v.st.fractures > 0) { uiFracture = true; v.st.fractures = 0; }
    }
    uiVoicesUsed = used;
}

//==============================================================================
void Engine::process (float* L, float* R, int n, const float* extL, const float* extR)
{
    int done = 0;
    while (done < n)
    {
        const int m = std::min (CTRL, n - done);
        float* oL = L + done; float* oR = R + done;

        // external input for this sub-block
        float extAct = 0.0f;
        if (extL != nullptr)
        {
            const float g = dbGain (lawDb (paramSpec (P_ext_gain), p[P_ext_gain]));
            for (int i = 0; i < m; ++i) { extMono[i] = 0.5f * ((extL[done + i]) + (extR ? extR[done + i] : extL[done + i])) * g; extAct += extMono[i] * extMono[i]; }
        }
        else std::fill (extMono, extMono + m, 0.0f);
        extLevel = 0.9f * extLevel + 0.1f * std::sqrt (extAct / m);
        const int extMode = p.li (P_ext_mode);
        if (extMode == 0 || extMode == 2) std::fill (extMono, extMono + m, 0.0f);   // not an exciter

        handleDrone();
        tickControl (m);
        renderVoices (m);

        // ---- MEMORY (global)
        std::fill (busL[2], busL[2] + m, 0.0f); std::fill (busR[2], busR[2] + m, 0.0f);
        {
            bool gate = false; int lastNote = -1, lastAge = -1;
            for (auto& v : voices) if (v.gate) { gate = true; if (v.age > lastAge) { lastAge = v.age; lastNote = v.note; } }
            if (! gate && p.sw (P_drone)) lastNote = p.li (P_drone_root);
            const float keyRatio = lastNote >= 0 ? fexp2 (((float) lastNote - 60.0f) / 12.0f) : 1.0f;
            const float* capIn = eff.li (P_mem_capsrc) == 1 ? mixL : loopRetMono;   // POST-SPACE is the previous sub-block's mix
            const bool memOn = eff.sw (P_mem_on);
            if (memOn) mem.render (busL[2], busR[2], m, eff, keyRatio, gate || p.sw (P_drone), extLevel, extMono, capIn);
            for (int i = 0; i < m; ++i) memMono[i] = 0.5f * (busL[2][i] + busR[2][i]);
            uiGrains = mem.uiGrains; uiMemAct = mem.uiActivity;
        }
        for (int b = 0; b < 4; ++b) stageHit (stagePeak[ST_MASS + b], busL[b], busR[b], m);
        // detectors (before the strips, so a muted channel still informs the network)
        {
            const float dt = (float) m / (float) sr; const float sm = lawHz (paramSpec (P_l_smooth), eff[P_l_smooth]);
            for (int b = 0; b < 4; ++b) { life.det[b].feed (busL[b], busR[b], m, sm, dt); stratumAct[b] = life.det[b].energy; }
        }

        // ---- channel strips -> mix, space send, loop send
        std::fill (mixL, mixL + m, 0.0f); std::fill (mixR, mixR + m, 0.0f);
        std::fill (sendL, sendL + m, 0.0f); std::fill (sendR, sendR + m, 0.0f);
        std::fill (loopSendL, loopSendL + m, 0.0f); std::fill (loopSendR, loopSendR + m, 0.0f);
        const bool anySolo = eff.sw (P_m_solo) || eff.sw (P_s_solo) || eff.sw (P_mem_solo) || eff.sw (P_st_solo);
        static const int CH[4] = { P_m_on, P_s_on, P_mem_on, P_st_on };
        for (int b = 0; b < 4; ++b)
        {
            const int base = CH[b];
            const bool muted = eff.sw (base + 8) || (anySolo && ! eff.sw (base + 9)) || ! eff.sw (base);
            chan[b].process (busL[b], busR[b], m, eff[base + 1], eff[base + 2], eff[base + 3], eff[base + 4], eff[base + 5], muted);
            const float es = eff[base + 6], ls = eff[base + 7];
            for (int i = 0; i < m; ++i)
            {
                mixL[i] += busL[b][i]; mixR[i] += busR[b][i];
                sendL[i] += busL[b][i] * es; sendR[i] += busR[b][i] * es;
                loopSendL[i] += busL[b][i] * ls; loopSendR[i] += busR[b][i] * ls;
            }
        }
        stageHit (stagePeak[ST_MIX_STRIP], mixL, mixR, m);
        stageHit (stagePeak[ST_SEND], sendL, sendR, m);
        // ---- the feedback loop node
        {
            const float gs = eff[P_e_fb_send];
            for (int i = 0; i < m; ++i) { loopSendL[i] += mixL[i] * gs; loopSendR[i] += mixR[i] * gs; }
            if (loopKick > 0.0f) { loopSendL[0] += loopKick; loopSendR[0] += loopKick; loopKick = 0.0f; }
            stageHit (stagePeak[ST_LOOP_SEND], loopSendL, loopSendR, m);
            loop.process (loopSendL, loopSendR, loopRetL, loopRetR, m, eff);
            stageHit (stagePeak[ST_LOOP_RET], loopRetL, loopRetR, m);
            for (int i = 0; i < m; ++i) loopRetMono[i] = 0.5f * (loopRetL[i] + loopRetR[i]);
            loopEnergy = loop.energy;
            if (eff.li (P_e_fb_to) == 0) for (int i = 0; i < m; ++i) { mixL[i] += loopRetL[i]; mixR[i] += loopRetR[i]; }
        }
        // ---- lane A on the mix, lane B on the space send, then the space
        laneA.process (mixL, mixR, m, eff, P_e_a1, P_e_a2, P_e_a3);
        stageHit (stagePeak[ST_MIX_LANEA], mixL, mixR, m);
        laneB.process (sendL, sendR, m, eff, P_e_b1, P_e_b2, P_e_b3);
        stageHit (stagePeak[ST_SEND_LANEB], sendL, sendR, m);
        std::fill (wetL, wetL + m, 0.0f); std::fill (wetR, wetR + m, 0.0f);
        space.process (sendL, sendR, wetL, wetR, m, eff, eff[P_e_scale]);
        stageHit (stagePeak[ST_SPACE_WET], wetL, wetR, m);
        spaceEnergy = space.energy;
        {
            const float dist = eff[P_e_distance], amount = eff[P_e_rv_mix];
            const float dryG = 1.0f - 0.75f * dist, wetG = amount * (1.0f + 0.8f * dist);
            for (int i = 0; i < m; ++i)
            {
                mixL[i] = mixL[i] * dryG + wetL[i] * wetG;
                mixR[i] = mixR[i] * dryG + wetR[i] * wetG;
            }
        }
        // ---- ducking from the AUX input
        float duckGain = 1.0f;
        if (extMode >= 2 && extL != nullptr)
        {
            const float rel = lawHz (paramSpec (P_duck_rel), p[P_duck_rel]) * 0.001f;
            duckEnv += (extLevel * 6.0f - duckEnv) * (extLevel * 6.0f > duckEnv ? 0.6f : (1.0f - std::exp (-(float) m / (float) sr / rel)));
            duckGain = 1.0f - p[P_duck_amt] * clamp01 (duckEnv);
        }
        else duckEnv = 0.0f;
        // ---- panic fade and stop
        if (panicFade)
        {
            for (int i = 0; i < m; ++i) { panicGain = std::max (0.0f, panicGain - 1.0f / (0.02f * (float) sr)); mixL[i] *= panicGain; mixR[i] *= panicGain; }
            if (panicGain <= 0.0f) { reset(); stopped = true; panicFade = false; panicGain = 1.0f; std::fill (mixL, mixL + m, 0.0f); std::fill (mixR, mixR + m, 0.0f); }
        }
        if (stopped) { std::fill (mixL, mixL + m, 0.0f); std::fill (mixR, mixR + m, 0.0f); }
        out.process (mixL, mixR, m, p, duckGain);
        stagePeak[ST_PRE_LIMIT] = std::max (stagePeak[ST_PRE_LIMIT], out.preLimPeak);
        stageHit (stagePeak[ST_OUT], mixL, mixR, m);
        if (stopped) { std::fill (mixL, mixL + m, 0.0f); std::fill (mixR, mixR + m, 0.0f); }   // the output filters' own tails do not count as "stopped"
        for (int i = 0; i < m; ++i) { oL[i] = mixL[i]; oR[i] = mixR[i]; }

        // ---- meters
        outRms = out.rms; outPeak = std::max (out.peakL, out.peakR); limReduction = out.lim.reduction;
        for (int i = 0; i < m; ++i)
        {
            const float mono = 0.5f * (mixL[i] + mixR[i]);
            scope[(size_t) scopeWrite] = mono; scopeWrite = (scopeWrite + 1) & (SCOPE_N - 1);
            specAcc[specW++] = mono;
            if (specW >= SPEC_N)
            {
                specW = 0;
                for (int k = 0; k < SPEC_N; ++k) { specRe[(size_t) k] = specAcc[k] * specWin[(size_t) k]; specIm[(size_t) k] = 0.0f; }
                specFft.forward (specRe.data(), specIm.data());
                for (int b = 0; b < SPEC_BANDS; ++b)
                {
                    const float f0 = 30.0f * std::pow (600.0f, (float) b / SPEC_BANDS), f1 = 30.0f * std::pow (600.0f, (float) (b + 1) / SPEC_BANDS);
                    int k0 = std::max (1, (int) (f0 * SPEC_N / sr)), k1 = std::max (k0 + 1, (int) (f1 * SPEC_N / sr));
                    k1 = std::min (k1, SPEC_N / 2);
                    float e = 0.0f; for (int k = k0; k < k1; ++k) e += specRe[(size_t) k] * specRe[(size_t) k] + specIm[(size_t) k] * specIm[(size_t) k];
                    e /= (k1 - k0);
                    const float db = 10.0f * std::log10 (e + 1.0e-12f);
                    uiSpectrum[b] = clamp01 ((db + 90.0f) / 90.0f);
                }
            }
        }
        done += m;
    }
}

} // namespace tty
