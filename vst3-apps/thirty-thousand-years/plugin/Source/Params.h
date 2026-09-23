/*  Thirty Thousand Years — the parameter table.

    ONE list drives everything: the engine's value array, the APVTS layout,
    the scene interpolation, the matrix's destination list, the presets and
    the page. A parameter that exists here exists everywhere; one that does
    not, nowhere. That is what makes the read-order class of bug
    inexpressible (the Hairfryer SPECS[] rule).

    Values are normalised 0..1 for continuous kinds and integer-valued for
    LIST / INT / SW. The engine reads EFFECTIVE values (host base, or the
    scene interpolation, plus modulation and macro offsets, clamped).
*/
#pragma once

#include <cstdint>
#include <cmath>

namespace tty
{

enum Kind
{
    KP_PCT = 0,   // 0..100 %
    KP_SW,        // OFF / ON
    KP_HZ,        // lo * (hi/lo)^v Hz
    KP_MS,        // lo * (hi/lo)^v ms
    KP_LIST,      // names[round v]
    KP_SEMI,      // (v-0.5)*lo semitones, signed
    KP_CENT,      // (v-0.5)*lo cents, signed
    KP_CENTU,     // v*lo cents
    KP_BIPOL,     // (v-0.5)*200 %
    KP_GLIDE,     // v<0.01 OFF, else lo*(hi/lo)^v ms
    KP_VOL,       // gain 2v^2 -> dB
    KP_PW,        // 50 + 45v %
    KP_INT,       // round v
    KP_SEC,       // lo * (hi/lo)^v s
    KP_SHZ,       // signed hertz, cubic: sgn(x)*|x|^3*lo with x = 2v-1 (fine near zero)
    KP_DB,        // lo + v*(hi-lo) dB
    KP_NOTE       // round v as a MIDI note, shown as a name
};

enum Flags
{
    F_NONE   = 0,
    F_NS     = 1,   // not part of a HISTORY scene
    F_NA     = 2,   // not automatable (a selector, a seed, a scene position)
    F_NM     = 4    // not a modulation destination even though continuous
};

struct PSpec
{
    const char* id;
    const char* name;
    float def;
    int   kind;
    float lo, hi;
    int   flags;
};

// ---- the list --------------------------------------------------------------
// X(sym, id, name, def, kind, lo, hi, flags)

#define TTY_CHANNEL(X, p, ps, defOn, defDrone) \
    X(p##_on,    ps "_on",    "ON",          defOn, KP_SW,    0, 0,   F_NONE) \
    X(p##_gain,  ps "_gain",  "LEVEL",       0.6f,  KP_VOL,   0, 0,   F_NONE) \
    X(p##_pan,   ps "_pan",   "PAN",         0.5f,  KP_BIPOL, 0, 0,   F_NONE) \
    X(p##_width, ps "_width", "WIDTH",       0.6f,  KP_PCT,   0, 0,   F_NONE) \
    X(p##_hp,    ps "_hp",    "HIGH CUT",    0.0f,  KP_HZ,    10, 4000, F_NONE) \
    X(p##_lp,    ps "_lp",    "LOW CUT",     1.0f,  KP_HZ,    200, 20000, F_NONE) \
    X(p##_send,  ps "_send",  "SPACE SEND",  0.3f,  KP_PCT,   0, 0,   F_NONE) \
    X(p##_loop,  ps "_loop",  "LOOP SEND",   0.0f,  KP_PCT,   0, 0,   F_NONE) \
    X(p##_mute,  ps "_mute",  "MUTE",        0.0f,  KP_SW,    0, 0,   F_NS) \
    X(p##_solo,  ps "_solo",  "SOLO",        0.0f,  KP_SW,    0, 0,   F_NS) \
    X(p##_drone, ps "_drone", "IN DRONE",    defDrone, KP_SW, 0, 0,   F_NONE) \
    X(p##_keys,  ps "_keys",  "ON KEYS",     1.0f,  KP_SW,    0, 0,   F_NONE)

#define TTY_ADSR(X, p, ps, nm, a, d, s, r) \
    X(p##_atk, ps "_atk", nm " ATTACK",  a, KP_MS, 1, 20000, F_NONE) \
    X(p##_dec, ps "_dec", nm " DECAY",   d, KP_MS, 1, 20000, F_NONE) \
    X(p##_sus, ps "_sus", nm " SUSTAIN", s, KP_PCT, 0, 0,    F_NONE) \
    X(p##_rel, ps "_rel", nm " RELEASE", r, KP_MS, 1, 30000, F_NONE)

#define TTY_LFO(X, n) \
    X(l##n##_wave,  "l" #n "_wave",  "LFO " #n " SHAPE", 0.0f, KP_LIST, 0, 5,       F_NONE) \
    X(l##n##_rate,  "l" #n "_rate",  "LFO " #n " RATE",  0.45f, KP_HZ, 0.0008f, 50, F_NONE) \
    X(l##n##_sync,  "l" #n "_sync",  "LFO " #n " SYNC",  0.0f, KP_LIST, 0, 8,       F_NONE) \
    X(l##n##_phase, "l" #n "_phase", "LFO " #n " PHASE", 0.0f, KP_PCT,  0, 0,       F_NONE) \
    X(l##n##_scope, "l" #n "_scope", "LFO " #n " SCOPE", 0.0f, KP_LIST, 0, 1,       F_NONE)

#define TTY_ENV(X, n) \
    X(e##n##_atk,  "e" #n "_atk",  "ENV " #n " ATTACK",  0.3f, KP_MS, 1, 20000, F_NONE) \
    X(e##n##_dec,  "e" #n "_dec",  "ENV " #n " DECAY",   0.5f, KP_MS, 1, 20000, F_NONE) \
    X(e##n##_sus,  "e" #n "_sus",  "ENV " #n " SUSTAIN", 0.6f, KP_PCT, 0, 0,   F_NONE) \
    X(e##n##_rel,  "e" #n "_rel",  "ENV " #n " RELEASE", 0.5f, KP_MS, 1, 30000, F_NONE) \
    X(e##n##_trig, "e" #n "_trig", "ENV " #n " TRIGGER", 0.0f, KP_LIST, 0, 2,  F_NONE)

#define TTY_STO(X, n) \
    X(r##n##_type,    "r" #n "_type",    "RANDOM " #n " KIND",    0.0f, KP_LIST, 0, 5,          F_NONE) \
    X(r##n##_rate,    "r" #n "_rate",    "RANDOM " #n " RATE",    0.4f, KP_HZ,   0.002f, 50,    F_NONE) \
    X(r##n##_amt,     "r" #n "_amt",     "RANDOM " #n " SPREAD",  0.5f, KP_PCT,  0, 0,          F_NONE) \
    X(r##n##_restore, "r" #n "_restore", "RANDOM " #n " RESTORE", 0.3f, KP_PCT,  0, 0,          F_NONE) \
    X(r##n##_smooth,  "r" #n "_smooth",  "RANDOM " #n " SMOOTH",  0.3f, KP_PCT,  0, 0,          F_NONE)

#define TTY_FOL(X, n) \
    X(f##n##_src, "f" #n "_src", "FOLLOWER " #n " SOURCE",  4.0f, KP_LIST, 0, 5,       F_NONE) \
    X(f##n##_atk, "f" #n "_atk", "FOLLOWER " #n " ATTACK",  0.3f, KP_MS,   1, 5000,    F_NONE) \
    X(f##n##_rel, "f" #n "_rel", "FOLLOWER " #n " RELEASE", 0.5f, KP_MS,   1, 20000,   F_NONE)

#define TTY_EVT(X, n) \
    X(v##n##_prob,    "v" #n "_prob",    "EVENT " #n " CHANCE",     0.0f, KP_PCT,  0, 0,        F_NONE) \
    X(v##n##_rate,    "v" #n "_rate",    "EVENT " #n " RATE",       0.4f, KP_HZ,   0.01f, 20,   F_NONE) \
    X(v##n##_target,  "v" #n "_target",  "EVENT " #n " TARGET",     0.0f, KP_LIST, 0, 6,        F_NONE) \
    X(v##n##_refract, "v" #n "_refract", "EVENT " #n " REFRACTORY", 0.4f, KP_MS,   10, 20000,   F_NONE) \
    X(v##n##_amt,     "v" #n "_amt",     "EVENT " #n " AMOUNT",     0.6f, KP_PCT,  0, 0,        F_NONE)

#define TTY_PARAMS(X) \
    /* ---- global ------------------------------------------------------ */ \
    X(volume,     "volume",     "VOLUME",          0.7f,  KP_VOL,  0, 0,     F_NS) \
    /*  PATCH TRIM belongs to the patch; VOLUME belongs to the player. It can
        default is EXACTLY 0 dB - 0.75 and 32 are both exact in binary, so the
        law returns 0.0 and dbGain returns 1.0f - which is what makes a preset
        that never mentions it bit-identical to one written before it existed. */ \
    X(outtrim,    "outtrim",    "PATCH TRIM",      0.75f, KP_DB,   -24, 8,  F_NONE) \
    X(ceiling,    "ceiling",    "CEILING",         0.9f,  KP_DB,   -12, 0,   F_NS) \
    X(quality,    "quality",    "QUALITY",         1.0f,  KP_LIST, 0, 2,     F_NS | F_NA) \
    X(bassmono,   "bassmono",   "BASS MONO BELOW", 0.35f, KP_HZ,   20, 400,  F_NS) \
    X(bassmono_on,"bassmono_on","BASS MONO",       1.0f,  KP_SW,   0, 0,     F_NS) \
    X(outwidth,   "outwidth",   "OUT WIDTH",       0.5f,  KP_PCT,  0, 0,     F_NS) \
    X(vmode,      "vmode",      "VOICE MODE",      0.0f,  KP_LIST, 0, 2,     F_NONE) \
    X(voices,     "voices",     "VOICES",          8.0f,  KP_INT,  1, 8,     F_NONE) \
    X(glide,      "glide",      "GLIDE",           0.0f,  KP_GLIDE,5, 8000,  F_NONE) \
    X(tune,       "tune",       "TUNE",            0.5f,  KP_SEMI, 24, 0,    F_NONE) \
    X(fine,       "fine",       "FINE",            0.5f,  KP_CENT, 100, 0,   F_NONE) \
    X(bend,       "bend",       "BEND RANGE",      2.0f,  KP_INT,  0, 24,    F_NS) \
    X(scale,      "scale",      "SCALE",           0.0f,  KP_LIST, 0, 9,     F_NONE) \
    X(rootlock,   "rootlock",   "ROOT LOCK",       1.0f,  KP_SW,   0, 0,     F_NONE) \
    X(mpe,        "mpe",        "MPE",             0.0f,  KP_SW,   0, 0,     F_NS) \
    X(drone,      "drone",      "DRONE",           0.0f,  KP_SW,   0, 0,     F_NS) \
    X(drone_root, "drone_root", "ROOT",            36.0f, KP_NOTE, 12, 84,   F_NONE) \
    X(drone_chord,"drone_chord","CHORD",           0.0f,  KP_LIST, 0, 9,     F_NONE) \
    X(drone_latch,"drone_latch","LATCH",           1.0f,  KP_SW,   0, 0,     F_NS) \
    X(drone_spread,"drone_spread","DRONE SPREAD",  0.4f,  KP_PCT,  0, 0,     F_NONE) \
    X(drone_vel,  "drone_vel",  "DRONE FORCE",     0.7f,  KP_PCT,  0, 0,     F_NONE) \
    X(ext_mode,   "ext_mode",   "AUX INPUT",       0.0f,  KP_LIST, 0, 3,     F_NS) \
    X(ext_gain,   "ext_gain",   "AUX GAIN",        0.5f,  KP_DB,   -24, 24,  F_NS) \
    X(duck_amt,   "duck_amt",   "DUCK",            0.5f,  KP_PCT,  0, 0,     F_NS) \
    X(duck_rel,   "duck_rel",   "DUCK RELEASE",    0.5f,  KP_MS,   20, 5000, F_NS) \
    X(seed,       "seed",       "SEED",            17.0f, KP_INT,  0, 999,   F_NS | F_NA) \
    X(determin,   "determin",   "DETERMINISTIC",   0.0f,  KP_SW,   0, 0,     F_NS | F_NA) \
    X(patch,      "patch",      "PATCH",           0.0f,  KP_INT,  0, 63,    F_NS | F_NA) \
    /* ---- MASS -------------------------------------------------------- */ \
    TTY_CHANNEL(X, m, "m", 1.0f, 1.0f) \
    X(m_o1wave,   "m_o1wave",   "OSC 1 WAVE",      2.0f,  KP_LIST, 0, 3,     F_NONE) \
    X(m_o1oct,    "m_o1oct",    "OSC 1 OCTAVE",    2.0f,  KP_LIST, 0, 4,     F_NONE) \
    X(m_o1semi,   "m_o1semi",   "OSC 1 SEMI",      0.5f,  KP_SEMI, 24, 0,    F_NONE) \
    X(m_o1fine,   "m_o1fine",   "OSC 1 FINE",      0.5f,  KP_CENT, 100, 0,   F_NONE) \
    X(m_o1pw,     "m_o1pw",     "OSC 1 WIDTH",     0.0f,  KP_PW,   0, 0,     F_NONE) \
    X(m_o1lvl,    "m_o1lvl",    "OSC 1 LEVEL",     0.8f,  KP_PCT,  0, 0,     F_NONE) \
    X(m_o2wave,   "m_o2wave",   "OSC 2 WAVE",      2.0f,  KP_LIST, 0, 3,     F_NONE) \
    X(m_o2oct,    "m_o2oct",    "OSC 2 OCTAVE",    2.0f,  KP_LIST, 0, 4,     F_NONE) \
    X(m_o2semi,   "m_o2semi",   "OSC 2 SEMI",      0.5f,  KP_SEMI, 24, 0,    F_NONE) \
    X(m_o2fine,   "m_o2fine",   "OSC 2 FINE",      0.53f, KP_CENT, 100, 0,   F_NONE) \
    X(m_o2pw,     "m_o2pw",     "OSC 2 WIDTH",     0.0f,  KP_PW,   0, 0,     F_NONE) \
    X(m_o2lvl,    "m_o2lvl",    "OSC 2 LEVEL",     0.6f,  KP_PCT,  0, 0,     F_NONE) \
    X(m_sync,     "m_sync",     "SYNC 2 TO 1",     0.0f,  KP_SW,   0, 0,     F_NONE) \
    X(m_beat,     "m_beat",     "BEAT (HZ)",       0.0f,  KP_HZ,   0.02f, 12, F_NONE) \
    X(m_unison,   "m_unison",   "UNISON",          0.0f,  KP_LIST, 0, 2,     F_NONE) \
    X(m_unidet,   "m_unidet",   "UNISON DETUNE",   0.3f,  KP_CENTU,60, 0,    F_NONE) \
    X(m_sub,      "m_sub",      "SUB",             1.0f,  KP_LIST, 0, 2,     F_NONE) \
    X(m_sublvl,   "m_sublvl",   "SUB LEVEL",       0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(m_subwave,  "m_subwave",  "SUB WAVE",        0.0f,  KP_LIST, 0, 1,     F_NONE) \
    X(m_reinf,    "m_reinf",    "REINFORCE",       0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(m_drift,    "m_drift",    "DRIFT",           0.15f, KP_CENTU,50, 0,    F_NONE) \
    X(m_drifttime,"m_drifttime","DRIFT TIME",      0.4f,  KP_SEC,  0.2f, 60, F_NONE) \
    X(m_pwdrift,  "m_pwdrift",  "WIDTH DRIFT",     0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(m_ampvar,   "m_ampvar",   "LEVEL DRIFT",     0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(m_fmode,    "m_fmode",    "FILTER",          0.0f,  KP_LIST, 0, 4,     F_NONE) \
    X(m_cut,      "m_cut",      "CUTOFF",          0.7f,  KP_HZ,   20, 20000, F_NONE) \
    X(m_res,      "m_res",      "RESONANCE",       0.2f,  KP_PCT,  0, 0,     F_NONE) \
    X(m_fdrive,   "m_fdrive",   "FILTER DRIVE",    0.2f,  KP_PCT,  0, 0,     F_NONE) \
    X(m_fenv,     "m_fenv",     "FILTER ENV",      0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(m_ftrack,   "m_ftrack",   "KEY TRACK",       0.5f,  KP_PCT,  0, 0,     F_NONE) \
    TTY_ADSR(X, m_f, "m_f", "FILTER", 0.3f, 0.5f, 0.5f, 0.5f) \
    TTY_ADSR(X, m_a, "m_a", "AMP",    0.35f, 0.4f, 0.8f, 0.55f) \
    /* ---- SIGNAL ------------------------------------------------------ */ \
    TTY_CHANNEL(X, s, "s", 0.0f, 1.0f) \
    X(s_wt1tab,   "s_wt1tab",   "TABLE 1",         0.0f,  KP_LIST, 0, 7,     F_NONE) \
    X(s_wt1pos,   "s_wt1pos",   "TABLE 1 POSITION",0.2f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_wt1oct,   "s_wt1oct",   "TABLE 1 OCTAVE",  2.0f,  KP_LIST, 0, 4,     F_NONE) \
    X(s_wt1semi,  "s_wt1semi",  "TABLE 1 SEMI",    0.5f,  KP_SEMI, 24, 0,    F_NONE) \
    X(s_wt1fine,  "s_wt1fine",  "TABLE 1 FINE",    0.5f,  KP_CENT, 100, 0,   F_NONE) \
    X(s_wt1lvl,   "s_wt1lvl",   "TABLE 1 LEVEL",   0.8f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_wt2tab,   "s_wt2tab",   "TABLE 2",         1.0f,  KP_LIST, 0, 7,     F_NONE) \
    X(s_wt2pos,   "s_wt2pos",   "TABLE 2 POSITION",0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_wt2oct,   "s_wt2oct",   "TABLE 2 OCTAVE",  2.0f,  KP_LIST, 0, 4,     F_NONE) \
    X(s_wt2semi,  "s_wt2semi",  "TABLE 2 SEMI",    0.5f,  KP_SEMI, 24, 0,    F_NONE) \
    X(s_wt2fine,  "s_wt2fine",  "TABLE 2 FINE",    0.5f,  KP_CENT, 100, 0,   F_NONE) \
    X(s_wt2lvl,   "s_wt2lvl",   "TABLE 2 LEVEL",   0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_pmratio,  "s_pmratio",  "PM RATIO",        1.0f,  KP_LIST, 0, 8,     F_NONE) \
    X(s_pmfixed,  "s_pmfixed",  "PM FIXED HZ",     0.5f,  KP_HZ,   10, 4000, F_NONE) \
    X(s_pmidx,    "s_pmidx",    "PM INDEX",        0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_pmfb,     "s_pmfb",     "PM FEEDBACK",     0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_ring,     "s_ring",     "RING 1x2",        0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_addlvl,   "s_addlvl",   "PARTIALS LEVEL",  0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_addn,     "s_addn",     "PARTIALS",        32.0f, KP_INT,  1, 64,    F_NONE) \
    X(s_addspread,"s_addspread","SPREAD",          0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(s_addodd,   "s_addodd",   "ODD / EVEN",      0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(s_addtilt,  "s_addtilt",  "TILT",            0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(s_addcluster,"s_addcluster","CLUSTER",       0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_addgaps,  "s_addgaps",  "GAPS",            0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_addmotion,"s_addmotion","MOTION",          0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_addfund,  "s_addfund",  "FUNDAMENTAL",     1.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_shift,    "s_shift",    "FREQ SHIFT",      0.5f,  KP_SHZ,  2000, 0,  F_NONE) \
    X(s_pshift,   "s_pshift",   "PITCH SHIFT",     0.5f,  KP_SEMI, 48, 0,    F_NONE) \
    X(s_interf,   "s_interf",   "INTERFERENCE",    0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(s_fmode,    "s_fmode",    "FILTER",          0.0f,  KP_LIST, 0, 5,     F_NONE) \
    X(s_cut,      "s_cut",      "CUTOFF",          0.75f, KP_HZ,   20, 20000, F_NONE) \
    X(s_res,      "s_res",      "RESONANCE",       0.15f, KP_PCT,  0, 0,     F_NONE) \
    X(s_fenv,     "s_fenv",     "FILTER ENV",      0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    TTY_ADSR(X, s_f, "s_f", "FILTER", 0.3f, 0.5f, 0.5f, 0.5f) \
    TTY_ADSR(X, s_a, "s_a", "AMP",    0.4f, 0.4f, 0.8f, 0.55f) \
    /* ---- MEMORY ------------------------------------------------------ */ \
    TTY_CHANNEL(X, mem, "mem", 0.0f, 1.0f) \
    X(mem_src,    "mem_src",    "SOURCE",          0.0f,  KP_LIST, 0, 5,     F_NONE) \
    X(mem_mode,   "mem_mode",   "MODE",            0.0f,  KP_LIST, 0, 2,     F_NONE) \
    X(mem_pos,    "mem_pos",    "POSITION",        0.2f,  KP_PCT,  0, 0,     F_NONE) \
    X(mem_scan,   "mem_scan",   "SCAN",            0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(mem_region, "mem_region", "REGION",          0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(mem_pitch,  "mem_pitch",  "PITCH",           0.5f,  KP_SEMI, 48, 0,    F_NONE) \
    X(mem_pfine,  "mem_pfine",  "PITCH FINE",      0.5f,  KP_CENT, 100, 0,   F_NONE) \
    X(mem_keyfollow,"mem_keyfollow","KEY FOLLOW",  1.0f,  KP_SW,   0, 0,     F_NONE) \
    X(mem_dur,    "mem_dur",    "GRAIN LENGTH",    0.5f,  KP_MS,   5, 2000,  F_NONE) \
    X(mem_dens,   "mem_dens",   "DENSITY",         0.45f, KP_HZ,   0.5f, 200, F_NONE) \
    X(mem_win,    "mem_win",    "WINDOW",          0.0f,  KP_LIST, 0, 5,     F_NONE) \
    X(mem_jit,    "mem_jit",    "JITTER",          0.2f,  KP_PCT,  0, 0,     F_NONE) \
    X(mem_pspread,"mem_pspread","PITCH SPREAD",    0.0f,  KP_CENTU,1200, 0,  F_NONE) \
    X(mem_scatter,"mem_scatter","SCATTER",         0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(mem_rev,    "mem_rev",    "REVERSE",         0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(mem_sched,  "mem_sched",  "SCHEDULE",        1.0f,  KP_LIST, 0, 2,     F_NONE) \
    X(mem_freeze, "mem_freeze", "FREEZE",          0.0f,  KP_SW,   0, 0,     F_NONE) \
    X(mem_smear,  "mem_smear",  "SMEAR",           0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(mem_stilt,  "mem_stilt",  "SPECTRAL TILT",   0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(mem_thin,   "mem_thin",   "THIN",            0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(mem_disp,   "mem_disp",   "DISPLACE",        0.5f,  KP_SHZ,  1000, 0,  F_NONE) \
    X(mem_evolve, "mem_evolve", "EVOLVE",          1.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(mem_erosion,"mem_erosion","EROSION",         0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(mem_ero_bw, "mem_ero_bw", "EROSION: BAND",   0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(mem_ero_drop,"mem_ero_drop","EROSION: DROPOUT",0.5f,KP_PCT,  0, 0,     F_NONE) \
    X(mem_ero_frag,"mem_ero_frag","EROSION: FRAGMENT",0.5f,KP_PCT, 0, 0,     F_NONE) \
    X(mem_ero_simp,"mem_ero_simp","EROSION: SIMPLIFY",0.5f,KP_PCT, 0, 0,     F_NONE) \
    X(mem_capsrc, "mem_capsrc", "CAPTURE FROM",    0.0f,  KP_LIST, 0, 2,     F_NS) \
    X(mem_atk,    "mem_atk",    "ATTACK",          0.4f,  KP_MS,   1, 20000, F_NONE) \
    X(mem_rel,    "mem_rel",    "RELEASE",         0.6f,  KP_MS,   1, 30000, F_NONE) \
    /* ---- STRUCTURE --------------------------------------------------- */ \
    TTY_CHANNEL(X, st, "st", 0.0f, 1.0f) \
    X(st_model,   "st_model",   "BODY",            1.0f,  KP_LIST, 0, 6,     F_NONE) \
    X(st_pitch,   "st_pitch",   "PITCH",           0.5f,  KP_SEMI, 48, 0,    F_NONE) \
    X(st_fine,    "st_fine",    "PITCH FINE",      0.5f,  KP_CENT, 100, 0,   F_NONE) \
    X(st_keyfollow,"st_keyfollow","KEY FOLLOW",    1.0f,  KP_SW,   0, 0,     F_NONE) \
    X(st_stiff,   "st_stiff",   "STIFFNESS",       0.2f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_damp,    "st_damp",    "DAMPING",         0.4f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_dens,    "st_dens",    "MODAL DENSITY",   0.6f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_expos,   "st_expos",   "STRIKE POSITION", 0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_pickup,  "st_pickup",  "PICKUP",          0.7f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_material,"st_material","MATERIAL",        0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_couple,  "st_couple",  "COUPLING",        0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_drive,   "st_drive",   "DRIVE",           0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_stress,  "st_stress",  "STRESS",          0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_exc,     "st_exc",     "EXCITER",         2.0f,  KP_LIST, 0, 7,     F_NONE) \
    X(st_exclvl,  "st_exclvl",  "EXCITER LEVEL",   0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_sustain, "st_sustain", "SUSTAIN ENERGY",  0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_bowforce,"st_bowforce","BOW FORCE",       0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_bowvel,  "st_bowvel",  "BOW SPEED",       0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_strikeon,"st_strikeon","STRIKE ON NOTE",  1.0f,  KP_SW,   0, 0,     F_NONE) \
    X(st_strikelvl,"st_strikelvl","STRIKE",        0.7f,  KP_PCT,  0, 0,     F_NONE) \
    X(st_atk,     "st_atk",     "ATTACK",          0.2f,  KP_MS,   1, 20000, F_NONE) \
    X(st_rel,     "st_rel",     "RELEASE",         0.6f,  KP_MS,   1, 30000, F_NONE) \
    /* ---- ENVIRONMENT ------------------------------------------------- */ \
    X(e_a1,       "e_a1",       "LANE A 1",        0.0f,  KP_LIST, 0, 7,     F_NONE) \
    X(e_a2,       "e_a2",       "LANE A 2",        0.0f,  KP_LIST, 0, 7,     F_NONE) \
    X(e_a3,       "e_a3",       "LANE A 3",        0.0f,  KP_LIST, 0, 7,     F_NONE) \
    X(e_b1,       "e_b1",       "LANE B 1",        0.0f,  KP_LIST, 0, 7,     F_NONE) \
    X(e_b2,       "e_b2",       "LANE B 2",        0.0f,  KP_LIST, 0, 7,     F_NONE) \
    X(e_b3,       "e_b3",       "LANE B 3",        0.0f,  KP_LIST, 0, 7,     F_NONE) \
    X(e_sat_drive,"e_sat_drive","SAT DRIVE",       0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_sat_asym, "e_sat_asym", "SAT ASYMMETRY",   0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(e_sat_tone, "e_sat_tone", "SAT TONE",        0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(e_sat_mix,  "e_sat_mix",  "SAT MIX",         1.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_fold_amt, "e_fold_amt", "FOLD",            0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_fold_sym, "e_fold_sym", "FOLD SYMMETRY",   0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(e_fold_mix, "e_fold_mix", "FOLD MIX",        1.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_mb_lo,    "e_mb_lo",    "MULTI LOW X",     0.4f,  KP_HZ,   40, 400,  F_NONE) \
    X(e_mb_hi,    "e_mb_hi",    "MULTI HIGH X",    0.5f,  KP_HZ,   800, 8000, F_NONE) \
    X(e_mb_low,   "e_mb_low",   "MULTI LOW DRIVE", 0.1f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_mb_mid,   "e_mb_mid",   "MULTI MID DRIVE", 0.4f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_mb_high,  "e_mb_high",  "MULTI HIGH DRIVE",0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_mb_mix,   "e_mb_mix",   "MULTI MIX",       1.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_sh_hz,    "e_sh_hz",    "SHIFT HZ",        0.5f,  KP_SHZ,  2000, 0,  F_NONE) \
    X(e_sh_mix,   "e_sh_mix",   "SHIFT MIX",       0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_sh_fb,    "e_sh_fb",    "SHIFT FEEDBACK",  0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_dl_time,  "e_dl_time",  "DELAY TIME",      0.55f, KP_MS,   1, 2000,  F_NONE) \
    X(e_dl_fb,    "e_dl_fb",    "DELAY FEEDBACK",  0.4f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_dl_mod,   "e_dl_mod",   "DELAY WOW",       0.1f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_dl_rate,  "e_dl_rate",  "DELAY WOW RATE",  0.3f,  KP_HZ,   0.02f, 8, F_NONE) \
    X(e_dl_mode,  "e_dl_mode",  "DELAY MODE",      0.0f,  KP_LIST, 0, 1,     F_NONE) \
    X(e_dl_tone,  "e_dl_tone",  "DELAY TONE",      0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(e_dl_mix,   "e_dl_mix",   "DELAY MIX",       0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_cb_pitch, "e_cb_pitch", "COMB PITCH",      0.5f,  KP_SEMI, 48, 0,    F_NONE) \
    X(e_cb_fb,    "e_cb_fb",    "COMB FEEDBACK",   0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_cb_damp,  "e_cb_damp",  "COMB DAMPING",    0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_cb_diff,  "e_cb_diff",  "COMB DIFFUSION",  0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_cb_mix,   "e_cb_mix",   "COMB MIX",        0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_cr_bits,  "e_cr_bits",  "CRUSH BITS",      0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_cr_rate,  "e_cr_rate",  "CRUSH RATE",      0.8f,  KP_HZ,   500, 48000, F_NONE) \
    X(e_cr_aa,    "e_cr_aa",    "CRUSH ANTI-ALIAS",1.0f,  KP_SW,   0, 0,     F_NONE) \
    X(e_cr_mix,   "e_cr_mix",   "CRUSH MIX",       1.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_rv_mix,   "e_rv_mix",   "SPACE AMOUNT",    0.35f, KP_PCT,  0, 0,     F_NONE) \
    X(e_rv_pre,   "e_rv_pre",   "PRE-DELAY",       0.2f,  KP_MS,   1, 250,   F_NONE) \
    X(e_rv_size,  "e_rv_size",  "ROOM SIZE",       0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_rv_decay, "e_rv_decay", "DECAY",           0.45f, KP_SEC,  0.2f, 60, F_NONE) \
    X(e_rv_diff,  "e_rv_diff",  "DIFFUSION",       0.7f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_rv_damp,  "e_rv_damp",  "HIGH DAMPING",    0.4f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_rv_low,   "e_rv_low",   "LOW DECAY",       0.5f,  KP_BIPOL,0, 0,     F_NONE) \
    X(e_rv_mod,   "e_rv_mod",   "SPACE MOTION",    0.2f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_rv_freeze,"e_rv_freeze","SPACE FREEZE",    0.0f,  KP_SW,   0, 0,     F_NONE) \
    X(e_rv_early, "e_rv_early", "EARLY / LATE",    0.4f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_scale,    "e_scale",    "APPARENT SCALE",  0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_distance, "e_distance", "DISTANCE",        0.2f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_fb_send,  "e_fb_send",  "LOOP SEND",       0.0f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_fb_ret,   "e_fb_ret",   "LOOP RETURN",     0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_fb_delay, "e_fb_delay", "LOOP DELAY",      0.5f,  KP_MS,   1, 2000,  F_NONE) \
    X(e_fb_hp,    "e_fb_hp",    "LOOP HIGH-PASS",  0.25f, KP_HZ,   20, 4000, F_NONE) \
    X(e_fb_lp,    "e_fb_lp",    "LOOP LOW-PASS",   0.75f, KP_HZ,   200, 20000, F_NONE) \
    X(e_fb_shift, "e_fb_shift", "LOOP SHIFT",      0.5f,  KP_SHZ,  500, 0,   F_NONE) \
    X(e_fb_sat,   "e_fb_sat",   "LOOP SATURATION", 0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_fb_damp,  "e_fb_damp",  "LOOP DAMPING",    0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(e_fb_to,    "e_fb_to",    "LOOP RETURNS TO", 0.0f,  KP_LIST, 0, 3,     F_NONE) \
    /* ---- LIFE -------------------------------------------------------- */ \
    TTY_LFO(X, 1) TTY_LFO(X, 2) TTY_LFO(X, 3) TTY_LFO(X, 4) \
    TTY_LFO(X, 5) TTY_LFO(X, 6) TTY_LFO(X, 7) TTY_LFO(X, 8) \
    TTY_ENV(X, 1) TTY_ENV(X, 2) TTY_ENV(X, 3) TTY_ENV(X, 4) \
    TTY_STO(X, 1) TTY_STO(X, 2) TTY_STO(X, 3) TTY_STO(X, 4) \
    TTY_FOL(X, 1) TTY_FOL(X, 2) \
    TTY_EVT(X, 1) TTY_EVT(X, 2) TTY_EVT(X, 3) TTY_EVT(X, 4) \
    X(l_autonomy, "l_autonomy", "AUTONOMY",        0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(l_coupling, "l_coupling", "COUPLING",        0.4f,  KP_PCT,  0, 0,     F_NONE) \
    X(l_recovery, "l_recovery", "RECOVERY",        0.5f,  KP_PCT,  0, 0,     F_NONE) \
    X(l_smooth,   "l_smooth",   "NETWORK SMOOTH",  0.5f,  KP_MS,   50, 20000, F_NONE) \
    X(l_hyst,     "l_hyst",     "HYSTERESIS",      0.3f,  KP_PCT,  0, 0,     F_NONE) \
    X(l_hold,     "l_hold",     "HOLD LIFE",       0.0f,  KP_SW,   0, 0,     F_NS) \
    /* ---- HISTORY ----------------------------------------------------- */ \
    X(h_on,       "h_on",       "HISTORY ON",      0.0f,  KP_SW,   0, 0,     F_NS | F_NM) \
    X(h_pos,      "h_pos",      "HISTORY",         0.0f,  KP_PCT,  0, 0,     F_NS | F_NM) \
    X(h_hold,     "h_hold",     "HOLD HISTORY",    0.0f,  KP_SW,   0, 0,     F_NS) \
    X(h_mode,     "h_mode",     "HISTORY DRIVE",   0.0f,  KP_LIST, 0, 2,     F_NS) \
    X(h_dur,      "h_dur",      "HISTORY DURATION",0.4f,  KP_SEC,  1, 1800,  F_NS) \
    X(h_bars,     "h_bars",     "HISTORY BARS",    3.0f,  KP_LIST, 0, 7,     F_NS) \
    X(h_loop,     "h_loop",     "HISTORY LOOP",    0.0f,  KP_LIST, 0, 2,     F_NS) \
    X(h_sc2pos,   "h_sc2pos",   "SCENE 2 AT",      0.333f,KP_PCT,  0, 0,     F_NS | F_NM) \
    X(h_sc3pos,   "h_sc3pos",   "SCENE 3 AT",      0.667f,KP_PCT,  0, 0,     F_NS | F_NM) \
    X(h_rewind,   "h_rewind",   "NEW CHORD REWINDS",0.0f, KP_SW,   0, 0,     F_NS) \
    /* ---- MACROS ------------------------------------------------------ */ \
    X(mac_mass,   "mac_mass",   "MASS",            0.0f,  KP_PCT,  0, 0,     F_NM) \
    X(mac_dread,  "mac_dread",  "DREAD",           0.0f,  KP_PCT,  0, 0,     F_NM) \
    X(mac_violence,"mac_violence","VIOLENCE",      0.0f,  KP_PCT,  0, 0,     F_NM) \
    X(mac_instab, "mac_instab", "INSTABILITY",     0.0f,  KP_PCT,  0, 0,     F_NM) \
    X(mac_contam, "mac_contam", "CONTAMINATION",   0.0f,  KP_PCT,  0, 0,     F_NM) \
    X(mac_distance,"mac_distance","DISTANCE",      0.0f,  KP_PCT,  0, 0,     F_NM) \
    X(mac_life,   "mac_life",   "LIFE",            0.0f,  KP_PCT,  0, 0,     F_NM) \
    X(mac_humanity,"mac_humanity","HUMANITY",      0.0f,  KP_PCT,  0, 0,     F_NM)

enum ParamIndex
{
   #define TTY_ENUM(sym, id, name, def, kind, lo, hi, flags) P_##sym,
    TTY_PARAMS (TTY_ENUM)
   #undef TTY_ENUM
    NUM_PARAMS
};

static constexpr int NUM_MACROS = 8;
static constexpr int NUM_SCENES = 4;
static constexpr int NUM_LFO = 8, NUM_ENV = 4, NUM_STO = 4, NUM_FOL = 2, NUM_EVT = 4;

int          numParams();
const PSpec& paramSpec (int i);
int          paramIndex (const char* id);          // -1 if unknown
float        paramMax (const PSpec& s);            // 1 for continuous, last index for stepped
bool         paramStepped (const PSpec& s);
bool         paramModulatable (const PSpec& s);
bool         paramInScene (const PSpec& s);
const char* const* listNames (const char* id, int& count);

/*  The engine's value array. Plain floats, copied whole. */
struct Params
{
    float v[NUM_PARAMS];
    Params() { for (int i = 0; i < NUM_PARAMS; ++i) v[i] = paramSpec (i).def; }
    inline float  operator[] (int i) const { return v[i]; }
    inline float& operator[] (int i)       { return v[i]; }
    inline int    li (int i) const { return (int) std::lround (v[i]); }     // list / int
    inline bool   sw (int i) const { return v[i] >= 0.5f; }
};

// ---- value laws, shared with the display -------------------------------------
inline float lawHz    (const PSpec& s, float v) { return s.lo * std::pow (s.hi / s.lo, v < 0 ? 0.0f : (v > 1 ? 1.0f : v)); }
inline float lawSemi  (const PSpec& s, float v) { return (v - 0.5f) * s.lo; }
inline float lawShz   (const PSpec& s, float v) { const float x = 2.0f * v - 1.0f; return (x < 0 ? -1.0f : 1.0f) * x * x * x * s.lo; }
inline float lawDb    (const PSpec& s, float v) { return s.lo + v * (s.hi - s.lo); }
inline float lawVol   (float v)                 { return 2.0f * v * v; }

} // namespace tty
