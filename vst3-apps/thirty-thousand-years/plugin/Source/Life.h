/*  Thirty Thousand Years — LIFE: modulation with memory and consequence.

    Eight LFOs, four ADSRs, four multi-segment envelopes, four stochastic
    sources, two followers, four event lanes, a 32-slot matrix and the LIFE
    NETWORK — slow control derived from the engine's own audio.

    Everything ticks once per CTRL samples. Modulation is an OFFSET on the
    host base value in normalised units; the base never moves.
*/
#pragma once

#include "Dsp.h"
#include "Params.h"

namespace tty
{

static constexpr int NUM_SLOTS = 32;
static constexpr int MSEG_MAX  = 8;

// ---- modulation sources -----------------------------------------------------
enum ModSource
{
    MS_NONE = 0,
    MS_LFO1, MS_LFO2, MS_LFO3, MS_LFO4, MS_LFO5, MS_LFO6, MS_LFO7, MS_LFO8,
    MS_ENV1, MS_ENV2, MS_ENV3, MS_ENV4,
    MS_MSEG1, MS_MSEG2, MS_MSEG3, MS_MSEG4,
    MS_RND1, MS_RND2, MS_RND3, MS_RND4,
    MS_FOL1, MS_FOL2,
    MS_EVT1, MS_EVT2, MS_EVT3, MS_EVT4,
    MS_NE_MASS, MS_NE_SIGNAL, MS_NE_MEMORY, MS_NE_STRUCT, MS_NE_MIX,
    MS_NB_MASS, MS_NB_SIGNAL, MS_NB_MEMORY, MS_NB_STRUCT, MS_NB_MIX,
    MS_NT_MASS, MS_NT_SIGNAL, MS_NT_MEMORY, MS_NT_STRUCT, MS_NT_MIX,
    MS_VELOCITY, MS_PRESSURE, MS_KEYTRACK, MS_WHEEL, MS_BEND,
    MS_MAC1, MS_MAC2, MS_MAC3, MS_MAC4, MS_MAC5, MS_MAC6, MS_MAC7, MS_MAC8,
    MS_HISTORY, MS_VOICERND, MS_EROSION, MS_CONST,
    NUM_MODSRC
};

inline const char* modSourceName (int s)
{
    static const char* N[NUM_MODSRC] = {
        "—", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "LFO 7", "LFO 8",
        "ENV 1", "ENV 2", "ENV 3", "ENV 4", "SHAPE 1", "SHAPE 2", "SHAPE 3", "SHAPE 4",
        "RANDOM 1", "RANDOM 2", "RANDOM 3", "RANDOM 4", "FOLLOWER 1", "FOLLOWER 2",
        "EVENT 1", "EVENT 2", "EVENT 3", "EVENT 4",
        "ENERGY: MASS", "ENERGY: SIGNAL", "ENERGY: MEMORY", "ENERGY: STRUCTURE", "ENERGY: MIX",
        "BRIGHT: MASS", "BRIGHT: SIGNAL", "BRIGHT: MEMORY", "BRIGHT: STRUCTURE", "BRIGHT: MIX",
        "TRANSIENT: MASS", "TRANSIENT: SIGNAL", "TRANSIENT: MEMORY", "TRANSIENT: STRUCTURE", "TRANSIENT: MIX",
        "VELOCITY", "PRESSURE", "KEY TRACK", "MOD WHEEL", "PITCH BEND",
        "MACRO MASS", "MACRO DREAD", "MACRO VIOLENCE", "MACRO INSTABILITY", "MACRO CONTAMINATION", "MACRO DISTANCE", "MACRO LIFE", "MACRO HUMANITY",
        "HISTORY", "VOICE RANDOM", "NET: EROSION", "CONSTANT" };
    return s >= 0 && s < NUM_MODSRC ? N[s] : "?";
}
/*  Whether a source has a value PER VOICE. LFOs, envelopes and shapes are
    per voice only when their scope / trigger says so, which the caller reads
    from the parameters each tick (perVoiceMask below). */
inline bool modSourceAlwaysPerVoice (int s)
{
    return s == MS_VELOCITY || s == MS_PRESSURE || s == MS_KEYTRACK || s == MS_VOICERND;
}

struct Slot
{
    int src = 0, via = 0, dst = -1, curve = 0;
    float depth = 0.0f, offset = 0.0f, slew = 0.0f, lo = -1.0f, hi = 1.0f;
    bool on = false;
};

struct MsegPoint { float t = 0.5f, level = 0.5f, curve = 0.0f; };
struct Mseg
{
    MsegPoint pts[MSEG_MAX]; int n = 4; bool loop = false; int trig = 0;
    float defaults() { n = 4; pts[0] = { 0.0f, 0.0f, 0.0f }; pts[1] = { 0.5f, 1.0f, 0.0f }; pts[2] = { 2.0f, 0.4f, 0.0f }; pts[3] = { 4.0f, 0.0f, 0.0f }; loop = false; return 0.0f; }
};

/*  Runtime state of one multi-segment envelope (per voice or global). */
struct MsegState
{
    float t = 0.0f; bool gate = false, running = false; float rel = 0.0f;
    void on() { t = 0.0f; gate = true; running = true; rel = 1.0f; }
    void off() { gate = false; }
    float tick (const Mseg& m, float dt)
    {
        if (! running) return 0.0f;
        const float total = m.pts[std::max (0, m.n - 1)].t;
        t += dt;
        if (t >= total)
        {
            if (m.loop && gate) t -= total; else t = total;
        }
        // find the segment
        float v = m.pts[0].level;
        for (int i = 0; i + 1 < m.n; ++i)
        {
            const MsegPoint& a = m.pts[i]; const MsegPoint& b = m.pts[i + 1];
            if (t >= a.t && t <= b.t)
            {
                float u = b.t > a.t ? (t - a.t) / (b.t - a.t) : 1.0f;
                if (b.curve > 0.01f) u = std::pow (u, 1.0f + 3.0f * b.curve); else if (b.curve < -0.01f) u = 1.0f - std::pow (1.0f - u, 1.0f - 3.0f * b.curve);
                v = lerp (a.level, b.level, u); break;
            }
            v = b.level;
        }
        if (! gate) { rel -= dt * 4.0f; if (rel <= 0.0f) { rel = 0.0f; running = false; } }
        return v * rel;
    }
};

//==============================================================================
struct Lfo
{
    float ph = 0.0f, shv = 0.0f, tgt = 0.0f, last = 0.0f; Rng rng;
    static float syncBeats (int s) { static const float B[9] = { 0, 32, 16, 8, 4, 2, 1, 0.5f, 0.25f }; return B[clampi (s, 0, 8)]; }
    float tick (int shape, float rateHz, int sync, float phaseOff, float dt, double bpm, double ppq, bool playing)
    {
        if (sync > 0 && bpm > 1.0)
        {
            const float beats = syncBeats (sync);
            if (playing) ph = (float) std::fmod (ppq / beats, 1.0) + phaseOff;
            else ph += (float) (bpm / 60.0 / beats) * dt;
        }
        else ph += rateHz * dt;
        ph -= std::floor (ph);
        float x = ph + (sync > 0 ? 0.0f : phaseOff); x -= std::floor (x);
        switch (shape)
        {
            case 1: return 1.0f - 4.0f * std::abs (x - 0.5f);
            case 2: return 2.0f * x - 1.0f;
            case 3: return x < 0.5f ? 1.0f : -1.0f;
            case 4: if (x < last) shv = rng.bi(); last = x; return shv;
            case 5: if (x < last) { shv = tgt; tgt = rng.bi(); } last = x; { const float u = x; const float s = u * u * (3.0f - 2.0f * u); return lerp (shv, tgt, s); }
            default: return fsin (x);
        }
    }
};

struct Stochastic
{
    OuWalk ou; float v = 0.0f, hold = 0.0f, ph = 0.0f, pulse = 0.0f; int burst = 0;
    float lx = 0.1f, ly = 0.0f, lz = 0.0f;   // Lorenz
    float tick (int type, float rateHz, float amt, float restore, float smooth, float dt, Rng& rng)
    {
        switch (type)
        {
            case 1:   // bounded random walk with a restoring force
                v += rng.bi() * amt * std::sqrt (rateHz * dt) * 2.0f - v * restore * rateHz * dt * 2.0f;
                v = clampf (v, -1.0f, 1.0f); return v;
            case 2:   // sample and hold
                ph += rateHz * dt; if (ph >= 1.0f) { ph -= std::floor (ph); hold = rng.bi() * amt; }
                v += (hold - v) * (1.0f - smooth * 0.98f); return v;
            case 3:   // probability: a decaying pulse when a coin lands
                ph += rateHz * dt; if (ph >= 1.0f) { ph -= std::floor (ph); if (rng.uni() < 0.35f + 0.6f * amt) pulse = 1.0f; }
                pulse *= std::exp (-dt * rateHz * (1.5f + 6.0f * (1.0f - smooth)));
                v = pulse; return v;
            case 4:   // clustered bursts
                ph += rateHz * dt;
                if (ph >= 1.0f) { ph -= std::floor (ph); if (burst > 0) { --burst; pulse = 0.6f + 0.4f * rng.uni(); } else if (rng.uni() < 0.3f + 0.5f * amt) burst = 2 + (int) (rng.uni() * 6); }
                pulse *= std::exp (-dt * rateHz * 8.0f); v = pulse; return v;
            case 5:   // CHAOS: a bounded Lorenz trajectory, integrated with a clamped step, x/20
            {
                const float h = clampf (rateHz * dt * 0.05f, 0.0f, 0.01f);
                for (int k = 0; k < 4; ++k)
                {
                    const float dx = 10.0f * (ly - lx), dy = lx * (28.0f - lz) - ly, dz = lx * ly - 2.6667f * lz;
                    lx += h * dx; ly += h * dy; lz += h * dz;
                }
                if (bad (lx) || bad (ly) || bad (lz) || std::abs (lx) > 60.0f) { lx = 0.1f; ly = 0.0f; lz = 0.0f; }
                v += (clampf (lx / 20.0f, -1.0f, 1.0f) * amt - v) * (1.0f - smooth * 0.9f);
                return v;
            }
            default:  // smooth correlated noise
                return v = ou.tick (rng, 1.0f / std::max (0.002f, rateHz), amt * 0.8f, dt);
        }
    }
};

struct Follower
{
    float env = 0.0f;
    float tick (float rms, float atkMs, float relMs, float dt)
    {
        const float a = 1.0f - std::exp (-dt / (atkMs * 0.001f)), r = 1.0f - std::exp (-dt / (relMs * 0.001f));
        env += (rms - env) * (rms > env ? a : r);
        return clamp01 (env * 3.0f);
    }
};

struct EventLane
{
    float ph = 0.0f, refractLeft = 0.0f, pulse = 0.0f; bool fired = false; float firedAmt = 0.0f; int fireCount = 0;
    void tick (float prob, float rateHz, float refractS, float amt, float boost, float dt, Rng& rng)
    {
        fired = false;
        if (refractLeft > 0.0f) refractLeft -= dt;
        ph += rateHz * dt;
        if (ph >= 1.0f)
        {
            ph -= std::floor (ph);
            if (refractLeft <= 0.0f && rng.uni() < clamp01 (prob * boost))
            { fired = true; firedAmt = amt; pulse = amt; refractLeft = refractS; ++fireCount; }
        }
        pulse *= std::exp (-dt / std::max (0.02f, refractS * 0.5f));
    }
};

//==============================================================================
/*  Detectors on one stratum's stereo audio: energy, brightness, transient. */
struct Detector
{
    OnePole hp; float e = 0.0f, eh = 0.0f, fast = 0.0f, slow = 0.0f;
    float energy = 0.0f, bright = 0.0f, trans = 0.0f;
    void prepare (double sr) { hp.setHz (2500.0f, sr); }
    void feed (const float* L, const float* R, int n, float smoothMs, float dt)
    {
        float s = 0.0f, sh = 0.0f;
        for (int i = 0; i < n; ++i) { const float m = 0.5f * (L[i] + R[i]); s += m * m; const float h = hp.hp (m); sh += h * h; }
        s /= n; sh /= n;
        const float a = 1.0f - std::exp (-dt / (smoothMs * 0.001f));
        e += (s - e) * a; eh += (sh - eh) * a;
        fast += (s - fast) * (1.0f - std::exp (-dt / 0.005f));
        slow += (s - slow) * (1.0f - std::exp (-dt / 0.25f));
        energy = clamp01 (std::sqrt (e) * 3.0f);
        bright = e > 1.0e-7f ? clamp01 (std::sqrt (eh / e) * 1.5f) : 0.0f;
        trans = clamp01 ((std::sqrt (fast) - std::sqrt (slow)) * 8.0f);
    }
};

/*  The Life Network: a designed dynamical system with observable state.
    Inputs: the five detectors. State: EROSION (an accumulator) and a Schmitt
    trigger on the mix energy that fires "swell" events. Outputs: named
    offsets on a fixed set of parameters, scaled by AUTONOMY (the depth limit)
    and COUPLING (the interaction strength), pulled back by RECOVERY. */
struct LifeNet
{
    float erosion = 0.0f; bool armed = true; float swell = 0.0f; float refract = 0.0f;
    struct Out { int dst; float amt; };
    Out out[8]; int nOut = 0;
    float uiCoupling[6] = {};
    void tick (const Detector* d, float autonomy, float coupling, float recovery, float hyst, float dt, float eventPulse)
    {
        const float c = coupling, a = autonomy;
        const float eM = d[0].energy, eS = d[1].energy, eMem = d[2].energy, eSt = d[3].energy, eMix = d[4].energy, tMix = d[4].trans;
        // the swell: a Schmitt trigger with hysteresis and a refractory period
        if (refract > 0.0f) refract -= dt;
        const float hi = 0.45f, lo = hi * (1.0f - 0.8f * hyst);
        if (armed && eMix > hi && refract <= 0.0f) { armed = false; swell = 1.0f; refract = 0.8f; }
        if (! armed && eMix < lo) armed = true;
        swell *= std::exp (-dt / 1.5f);
        // erosion accumulates with signal and memory activity and with events; recovery drains it
        erosion += dt * (c * (0.12f * eS + 0.12f * eMem + 0.3f * swell) + 0.5f * eventPulse) - dt * (0.05f + 0.6f * recovery) * erosion;
        erosion = clamp01 (erosion);
        nOut = 0;
        auto add = [&] (int dst, float v) { if (nOut < 8) { out[nOut].dst = dst; out[nOut].amt = clampf (v * a, -a, a); ++nOut; } };
        add (P_mem_dens,    c * eSt * 0.35f);          // structure energy raises grain density
        add (P_s_interf,    c * eMem * 0.5f);          // memory energy destabilises signal
        add (P_mem_erosion, erosion * 0.8f);           // accumulated erosion erodes the memory
        add (P_st_exclvl,  -erosion * 0.4f);           // erosion reduces excitation -> the structure falls quiet
        add (P_e_fb_send,   c * tMix * 0.25f);         // transients feed the loop
        add (P_st_stress,   c * eM * 0.3f);            // mass pressure stresses the structure
        add (P_m_cut,      -c * eS * 0.25f);           // signal loudness darkens mass
        add (P_mem_thin,    c * swell * 0.4f);         // a swell thins the memory
        for (int k = 0; k < 6; ++k) uiCoupling[k] = k < nOut ? out[k].amt : 0.0f;
    }
    void reset() { erosion = 0.0f; armed = true; swell = 0.0f; refract = 0.0f; }
};

//==============================================================================
struct Life
{
    double sr = 48000.0; Rng rng;
    Lfo lfo[NUM_LFO]; Adsr env[NUM_ENV]; Mseg mseg[4]; MsegState msegG[4]; Stochastic sto[NUM_STO]; Follower fol[NUM_FOL]; EventLane evt[NUM_EVT];
    Detector det[5]; LifeNet net;
    Slot slots[NUM_SLOTS]; Smooth slew[NUM_SLOTS];
    float src[NUM_MODSRC] = {};          // global source values this tick
    float offs[NUM_PARAMS] = {};         // global offsets this tick
    bool hold = false;
    float lfoVal[NUM_LFO] = {};
    float uiMsegVal[4] = {};
    bool perVoiceSrc[NUM_MODSRC] = {};   // refreshed each tick from the scopes and triggers

    bool isPerVoice (int s) const { return s > 0 && s < NUM_MODSRC && perVoiceSrc[s]; }
    void refreshScopes (const Params& p)
    {
        for (int s = 0; s < NUM_MODSRC; ++s) perVoiceSrc[s] = modSourceAlwaysPerVoice (s);
        for (int i = 0; i < NUM_LFO; ++i) perVoiceSrc[MS_LFO1 + i] = p.li (P_l1_scope + i * 5) == 1;
        for (int i = 0; i < NUM_ENV; ++i) perVoiceSrc[MS_ENV1 + i] = p.li (P_e1_trig + i * 5) == 0;
        for (int i = 0; i < 4; ++i) perVoiceSrc[MS_MSEG1 + i] = mseg[i].trig == 0;
    }

    void prepare (double rate, uint32_t seed)
    {
        sr = rate; rng.seed (seed);
        for (int i = 0; i < NUM_LFO; ++i) { lfo[i].rng.seed (seed * 7u + (uint32_t) i + 1u); lfo[i].ph = 0.0f; }
        for (int i = 0; i < 5; ++i) det[i].prepare (rate);
        for (int i = 0; i < 4; ++i) mseg[i].defaults();
        for (auto& s : slew) s.set (0.0f);
        reset();
    }
    void reset()
    {
        for (auto& e : env) e.kill();
        for (auto& m : msegG) { m.running = false; m.gate = false; }
        for (auto& s : sto) { s.v = 0.0f; s.pulse = 0.0f; s.burst = 0; s.ou.reset(); }
        for (auto& f : fol) f.env = 0.0f;
        for (auto& e : evt) { e.pulse = 0.0f; e.refractLeft = 0.0f; e.ph = 0.0f; }
        net.reset();
        std::fill (std::begin (offs), std::end (offs), 0.0f);
    }
    void rephase (uint32_t seed)   // deterministic mode: transport start
    {
        rng.seed (seed);
        for (int i = 0; i < NUM_LFO; ++i) { lfo[i].ph = 0.0f; lfo[i].rng.seed (seed * 7u + (uint32_t) i + 1u); lfo[i].shv = lfo[i].tgt = 0.0f; }
        for (auto& s : sto) { s.ou.reset(); s.v = 0.0f; s.ph = 0.0f; s.pulse = 0.0f; s.lx = 0.1f; s.ly = s.lz = 0.0f; }
        for (auto& e : evt) { e.ph = 0.0f; e.pulse = 0.0f; e.refractLeft = 0.0f; }
        net.reset();
    }

    static inline float curveOf (int c, float x)
    {
        switch (c)
        {
            case 1: return x >= 0 ? x * x : -x * x;                            // exponential-ish
            case 2: return x >= 0 ? std::sqrt (x) : -std::sqrt (-x);           // logarithmic-ish
            case 3: return x * x * (3.0f - 2.0f * std::abs (x)) ;             // S
            default: return x;
        }
    }

    /*  One control tick. gateAny: something is held. eventGlobal: pulses from
        the lanes this tick are returned through evt[].fired. */
    void tick (const Params& p, float dt, double bpm, double ppq, bool playing, bool gateAny, float wheel, float bend, float history)
    {
        refreshScopes (p);
        if (hold) { /* state frozen; sources keep their last values */ }
        else
        {
            const float netBoost = 1.0f + p[P_l_coupling] * det[4].energy * 2.0f;
            for (int i = 0; i < NUM_LFO; ++i)
            {
                const int b = P_l1_wave + i * 5;
                lfoVal[i] = lfo[i].tick (p.li (b), lawHz (paramSpec (b + 1), p[b + 1]), p.li (b + 2), p[b + 3], dt, bpm, ppq, playing);
                src[MS_LFO1 + i] = lfoVal[i];
            }
            for (int i = 0; i < NUM_ENV; ++i)
            {
                const int b = P_e1_atk + i * 5;
                env[i].set (lawHz (paramSpec (b), p[b]), lawHz (paramSpec (b + 1), p[b + 1]), p[b + 2], lawHz (paramSpec (b + 3), p[b + 3]), 1.0 / dt);
                const int trig = p.li (b + 4);
                if (trig == 1) { if (gateAny && ! env[i].active()) env[i].on(); if (! gateAny && env[i].active() && env[i].st != Adsr::REL) env[i].off(); }
                if (trig == 2 && evt[i % NUM_EVT].fired) env[i].on();
                src[MS_ENV1 + i] = env[i].tick();
            }
            for (int i = 0; i < 4; ++i)
            {
                if (mseg[i].trig == 1) { if (gateAny && ! msegG[i].running) msegG[i].on(); if (! gateAny && msegG[i].gate) msegG[i].off(); }
                if (mseg[i].trig == 2 && evt[i % NUM_EVT].fired) msegG[i].on();
                uiMsegVal[i] = src[MS_MSEG1 + i] = msegG[i].tick (mseg[i], dt);
            }
            for (int i = 0; i < NUM_STO; ++i)
            {
                const int b = P_r1_type + i * 5;
                src[MS_RND1 + i] = sto[i].tick (p.li (b), lawHz (paramSpec (b + 1), p[b + 1]), p[b + 2], p[b + 3], p[b + 4], dt, rng);
            }
            for (int i = 0; i < NUM_FOL; ++i)
            {
                const int b = P_f1_src + i * 3;
                const int s = p.li (b);
                const float rms = s < 5 ? std::sqrt (det[s].e) : det[4].fast;
                src[MS_FOL1 + i] = fol[i].tick (rms, lawHz (paramSpec (b + 1), p[b + 1]), lawHz (paramSpec (b + 2), p[b + 2]), dt);
            }
            float eventPulse = 0.0f;
            for (int i = 0; i < NUM_EVT; ++i)
            {
                const int b = P_v1_prob + i * 5;
                evt[i].tick (p[b], lawHz (paramSpec (b + 1), p[b + 1]), lawHz (paramSpec (b + 3), p[b + 3]) * 0.001f, p[b + 4], netBoost, dt, rng);
                src[MS_EVT1 + i] = evt[i].pulse;
                if (evt[i].fired && p.li (b + 2) == 3) eventPulse += evt[i].firedAmt;   // EROSION PULSE
            }
            net.tick (det, p[P_l_autonomy], p[P_l_coupling], p[P_l_recovery], p[P_l_hyst], dt, eventPulse);
        }
        for (int k = 0; k < 5; ++k) { src[MS_NE_MASS + k] = det[k].energy; src[MS_NB_MASS + k] = det[k].bright; src[MS_NT_MASS + k] = det[k].trans; }
        src[MS_WHEEL] = wheel; src[MS_BEND] = bend;
        for (int k = 0; k < NUM_MACROS; ++k) src[MS_MAC1 + k] = p[P_mac_mass + k];
        src[MS_HISTORY] = history; src[MS_EROSION] = net.erosion; src[MS_CONST] = 1.0f;
        src[MS_VELOCITY] = src[MS_PRESSURE] = src[MS_KEYTRACK] = src[MS_VOICERND] = 0.0f;   // per voice, filled by the engine

        // the matrix: global sources only here; per-voice slots are applied by the engine
        std::fill (std::begin (offs), std::end (offs), 0.0f);
        for (int i = 0; i < NUM_SLOTS; ++i)
        {
            const Slot& s = slots[i];
            if (! s.on || s.dst < 0 || s.dst >= NUM_PARAMS || s.src == 0) continue;
            if (isPerVoice (s.src) || (s.via > 0 && isPerVoice (s.via))) continue;
            float x = curveOf (s.curve, src[s.src]) * s.depth + s.offset;
            if (s.via > 0) x *= src[s.via];
            x = clampf (x, s.lo, s.hi);
            if (s.slew > 0.001f) { slew[i].setTau (s.slew, 1.0 / dt); x = slew[i].to (x); }
            offs[s.dst] += x;
        }
        // the network's own offsets
        for (int k = 0; k < net.nOut; ++k) offs[net.out[k].dst] += net.out[k].amt;
    }

    /*  Per-voice slots: adds offsets for one voice on top of the global ones. */
    void voiceOffsets (float* vo, float vel, float press, float key, float vrnd, const float* envV, const float* lfoV, const float* msegV) const
    {
        for (int i = 0; i < NUM_SLOTS; ++i)
        {
            const Slot& s = slots[i];
            if (! s.on || s.dst < 0 || s.dst >= NUM_PARAMS || s.src == 0) continue;
            const bool pv = isPerVoice (s.src) || (s.via > 0 && isPerVoice (s.via));
            if (! pv) continue;
            auto val = [&] (int m) -> float
            {
                if (m == MS_VELOCITY) return vel;
                if (m == MS_PRESSURE) return press;
                if (m == MS_KEYTRACK) return key;
                if (m == MS_VOICERND) return vrnd;
                if (m >= MS_ENV1 && m <= MS_ENV4) return envV[m - MS_ENV1];
                if (m >= MS_LFO1 && m <= MS_LFO8) return lfoV[m - MS_LFO1];
                if (m >= MS_MSEG1 && m <= MS_MSEG4) return msegV[m - MS_MSEG1];
                return src[m];
            };
            float x = curveOf (s.curve, val (s.src)) * s.depth + s.offset;
            if (s.via > 0) x *= val (s.via);
            vo[s.dst] += clampf (x, s.lo, s.hi);
        }
    }
    bool anyPerVoice() const
    {
        for (const auto& s : slots) if (s.on && s.src && (isPerVoice (s.src) || (s.via && isPerVoice (s.via)))) return true;
        return false;
    }
};

} // namespace tty
