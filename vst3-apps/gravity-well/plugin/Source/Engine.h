#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <string>

/*  GRAVITY WELL - the bassline synth.

    Plain C++: no JUCE anywhere in this file or Engine.cpp, so the offline
    bench compiles the same code the plug-in runs.

    THE ONE IDEA.  A single scalar - MASS - sets a Schwarzschild radius rs.
    Everything else is a consequence of rs through the real formula:

      * every modulator sits at a RADIUS r and runs at sqrt(1 - rs/r) of its
        nominal rate; at r <= rs it FREEZES and holds its last value;
      * the FORMANT bank redshifts by the same factor, so the spectrum sinks
        while f0 does not move (pitch and spectrum are separable, which is
        the whole reason the idea is usable on a bass instrument);
      * the RINGDOWN bank rings lower and longer, because a black hole's
        quasi-normal modes go as omega ~ 1/M and tau ~ M.

    At MASS 0 the well is flat and none of it is reachable: the instrument is
    an ordinary (excellent) bass synth and a patch that never mentions the
    well is bit-identical to one built without it.  The bench asserts that.
*/

namespace gw {

// ---------------------------------------------------------------- parameters
/*  ONE table.  The enum, the specs, the APVTS layout, the engine read, the
    presets and the panel all walk it, so a parameter cannot be read in a
    different order from the one it was declared in.  (Blade Ruiner needed a
    script to catch that class of bug; this makes it inexpressible.)

    X(id, label, default, lo, hi, kind, choices)                                */

#define GW_KIND_LIN    0
#define GW_KIND_CHOICE 1
#define GW_KIND_HZ     2   // 0..1 mapped logarithmically by the reader
#define GW_KIND_DB     3
#define GW_KIND_PCT    4
#define GW_KIND_SEC    5
#define GW_KIND_INT    6
#define GW_KIND_BIPOL  7

#define GW_PARAMS(X)                                                                                  \
  /* ---- the six macros: Peter's own vocabulary, each a curated gesture ---- */                      \
  X(macro_mass,  "MASS",        0.0f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(macro_depth, "DEPTH",       0.35f, 0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(macro_energy,"ENERGY",      0.25f, 0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(macro_horiz, "HORIZON",     0.5f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(macro_time,  "TIME",        0.5f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(macro_space, "SPACE",       0.25f, 0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  /* ---- source: oscillator A ---- */                                                                \
  X(a_engine,    "A ENGINE",    0.f,   0.f, 1.f, GW_KIND_CHOICE, "ANALOGUE|WAVETABLE")                 \
  X(a_shape,     "A SHAPE",     0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(a_table,     "A TABLE",     0.f,   0.f, 7.f, GW_KIND_CHOICE, "BASIC|OCTAVE|FORMANT|METAL|GLASS|GROWL|FOLD|SWARM") \
  X(a_width,     "A WIDTH",     0.5f,  0.02f,0.98f,GW_KIND_PCT,  nullptr)                             \
  X(a_oct,       "A OCTAVE",    0.f,  -3.f, 3.f, GW_KIND_INT,    nullptr)                             \
  X(a_semi,      "A SEMI",      0.f, -12.f,12.f, GW_KIND_INT,    nullptr)                             \
  X(a_fine,      "A FINE",      0.f, -50.f,50.f, GW_KIND_BIPOL,  nullptr)                             \
  X(a_level,     "A LEVEL",     1.0f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  /* ---- source: oscillator B ---- */                                                                \
  X(b_engine,    "B ENGINE",    0.f,   0.f, 1.f, GW_KIND_CHOICE, "ANALOGUE|WAVETABLE")                 \
  X(b_shape,     "B SHAPE",     0.25f, 0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(b_table,     "B TABLE",     0.f,   0.f, 7.f, GW_KIND_CHOICE, "BASIC|OCTAVE|FORMANT|METAL|GLASS|GROWL|FOLD|SWARM") \
  X(b_width,     "B WIDTH",     0.5f,  0.02f,0.98f,GW_KIND_PCT,  nullptr)                             \
  X(b_oct,       "B OCTAVE",    0.f,  -3.f, 3.f, GW_KIND_INT,    nullptr)                             \
  X(b_semi,      "B SEMI",      0.f, -12.f,12.f, GW_KIND_INT,    nullptr)                             \
  X(b_fine,      "B FINE",      0.f, -50.f,50.f, GW_KIND_BIPOL,  nullptr)                             \
  X(b_level,     "B LEVEL",     0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(sync,        "SYNC",        0.f,   0.f, 1.f, GW_KIND_CHOICE, "OFF|ON")                            \
  X(pm,          "PHASE MOD",   0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(ringmod,     "RING MOD",    0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  /* ---- source: sub and noise ---- */                                                               \
  X(sub_level,   "SUB LEVEL",   0.5f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(sub_oct,     "SUB OCTAVE",  1.f,   1.f, 2.f, GW_KIND_INT,    nullptr)                             \
  X(sub_shape,   "SUB SHAPE",   0.f,   0.f, 1.f, GW_KIND_CHOICE, "SINE|SQUARE")                       \
  X(noise_level, "NOISE",       0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(noise_col,   "NOISE COLOUR",0.5f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  /* ---- the accretion disk: unison on Keplerian radii ---- */                                       \
  X(disk_n,      "DISK VOICES", 1.f,   1.f, 8.f, GW_KIND_INT,    nullptr)                             \
  X(disk_spread, "DISK SPREAD", 0.2f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(disk_rot,    "DISK ORBIT",  0.15f, 0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(disk_width,  "DISK WIDTH",  0.5f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  /* ---- the well ---- */                                                                            \
  X(mass_track,  "MASS TRACK",  0.5f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(mass_vel,    "MASS VEL",    0.3f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(redshift,    "REDSHIFT",    0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(ring_amt,    "RINGDOWN",    0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(ring_decay,  "RING DECAY",  0.5f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(ring_tone,   "RING TONE",   0.5f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  /* ---- horizon A (filter section A) ---- */                                                        \
  X(fa_circ,     "A CIRCUIT",   0.f,   0.f, 6.f, GW_KIND_CHOICE, "LADDER|GROWL|SCREAM|SVF|DIODE|COMB|FORMANT") \
  X(fa_mode,     "A MODE",      0.f,   0.f, 3.f, GW_KIND_CHOICE, "LP|BP|HP|NOTCH")                    \
  X(fa_cut,      "A CUTOFF",    0.6f,  0.f, 1.f, GW_KIND_HZ,     nullptr)                             \
  X(fa_res,      "A RESONANCE", 0.2f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(fa_drive,    "A DRIVE",     0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(fa_key,      "A KEY FOLLOW",0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(fa_env,      "A ENV",       0.f,  -1.f, 1.f, GW_KIND_BIPOL,  nullptr)                             \
  /* ---- horizon B ---- */                                                                           \
  X(fb_circ,     "B CIRCUIT",   3.f,   0.f, 6.f, GW_KIND_CHOICE, "LADDER|GROWL|SCREAM|SVF|DIODE|COMB|FORMANT") \
  X(fb_mode,     "B MODE",      2.f,   0.f, 3.f, GW_KIND_CHOICE, "LP|BP|HP|NOTCH")                    \
  X(fb_cut,      "B CUTOFF",    0.15f, 0.f, 1.f, GW_KIND_HZ,     nullptr)                             \
  X(fb_res,      "B RESONANCE", 0.1f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(fb_drive,    "B DRIVE",     0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(fb_key,      "B KEY FOLLOW",0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(fb_env,      "B ENV",       0.f,  -1.f, 1.f, GW_KIND_BIPOL,  nullptr)                             \
  /* ---- routing ---- */                                                                             \
  X(route,       "ROUTING",     0.f,   0.f, 2.f, GW_KIND_CHOICE, "SERIES|PARALLEL|SPLIT")             \
  X(split,       "SPLIT",       0.35f, 0.f, 1.f, GW_KIND_HZ,     nullptr)                             \
  X(blend,       "A/B BLEND",   0.5f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  /* ---- the singularity: the protected core ---- */                                                 \
  X(sing_freq,   "SING FREQ",   0.25f, 0.f, 1.f, GW_KIND_HZ,     nullptr)                             \
  X(sing_level,  "SING LEVEL",  0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(sing_src,    "SING SOURCE", 0.f,   0.f, 1.f, GW_KIND_CHOICE, "FILTERED LOW|PURE SINE")            \
  /* ---- envelopes ---- */                                                                           \
  X(e1_a,        "AMP ATTACK",  0.1476f,0.f, 1.f, GW_KIND_SEC,    nullptr)                             \
  X(e1_d,        "AMP DECAY",   0.6616f, 0.f, 1.f, GW_KIND_SEC,    nullptr)                             \
  X(e1_s,        "AMP SUSTAIN", 0.8f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(e1_r,        "AMP RELEASE", 0.5835f, 0.f, 1.f, GW_KIND_SEC,    nullptr)                             \
  X(e1_curve,    "AMP CURVE",   0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                          \
  X(e2_a,        "ENV2 ATTACK", 0.0738f,0.f, 1.f, GW_KIND_SEC,    nullptr)                             \
  X(e2_d,        "ENV2 DECAY",  0.6379f,  0.f, 1.f, GW_KIND_SEC,    nullptr)                             \
  X(e2_s,        "ENV2 SUSTAIN",0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(e2_r,        "ENV2 RELEASE",0.6073f, 0.f, 1.f, GW_KIND_SEC,    nullptr)                             \
  X(e2_curve,    "ENV2 CURVE",  0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                          \
  X(e3_a,        "ENV3 ATTACK", 0.3189f, 0.f, 1.f, GW_KIND_SEC,    nullptr)                          \
  X(e3_d,        "ENV3 DECAY",  0.6811f,  0.f, 1.f, GW_KIND_SEC,    nullptr)                          \
  X(e3_s,        "ENV3 SUSTAIN",0.5f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                          \
  X(e3_r,        "ENV3 RELEASE",0.6379f,  0.f, 1.f, GW_KIND_SEC,    nullptr)                          \
  X(e3_curve,    "ENV3 CURVE",  0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                          \
  /* ---- the modulators' radii: this is where the one idea is dialled ---- */                        \
  X(r_e2,        "ENV2 RADIUS", 1.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(r_e3,        "ENV3 RADIUS", 1.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                          \
  X(r_l1,        "LFO1 RADIUS", 1.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(r_l2,        "LFO2 RADIUS", 1.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(r_l3,        "LFO3 RADIUS", 1.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(r_rnd,       "RND RADIUS",  1.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(r_chirp,     "CHIRP RADIUS",1.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  /* ---- LFOs ---- */                                                                                \
  X(l1_rate,     "LFO1 RATE",   0.35f, 0.f, 1.f, GW_KIND_HZ,     nullptr)                             \
  X(l1_shape,    "LFO1 SHAPE",  0.f,   0.f, 4.f, GW_KIND_CHOICE, "SINE|TRI|SAW|SQUARE|CHIRP")         \
  X(l1_sync,     "LFO1 SYNC",   0.f,   0.f, 1.f, GW_KIND_CHOICE, "FREE|TEMPO")                        \
  X(l2_rate,     "LFO2 RATE",   0.2f,  0.f, 1.f, GW_KIND_HZ,     nullptr)                             \
  X(l2_shape,    "LFO2 SHAPE",  1.f,   0.f, 4.f, GW_KIND_CHOICE, "SINE|TRI|SAW|SQUARE|CHIRP")         \
  X(l2_sync,     "LFO2 SYNC",   0.f,   0.f, 1.f, GW_KIND_CHOICE, "FREE|TEMPO")                        \
  X(l3_rate,     "LFO3 RATE",   0.6f,  0.f, 1.f, GW_KIND_HZ,     nullptr)                             \
  X(l3_shape,    "LFO3 SHAPE",  0.f,   0.f, 4.f, GW_KIND_CHOICE, "SINE|TRI|SAW|SQUARE|CHIRP")         \
  X(l3_sync,     "LFO3 SYNC",   0.f,   0.f, 1.f, GW_KIND_CHOICE, "FREE|TEMPO")                        \
  X(l1_dst,      "LFO1 TARGET", 0.f,   0.f, 21.f, GW_KIND_CHOICE, "OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL")\
  X(l1_amt,      "LFO1 AMOUNT", 0.f,  -1.f, 1.f, GW_KIND_BIPOL,  nullptr)                               \
  X(l2_dst,      "LFO2 TARGET", 0.f,   0.f, 21.f, GW_KIND_CHOICE, "OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL")\
  X(l2_amt,      "LFO2 AMOUNT", 0.f,  -1.f, 1.f, GW_KIND_BIPOL,  nullptr)                               \
  X(l3_dst,      "LFO3 TARGET", 0.f,   0.f, 21.f, GW_KIND_CHOICE, "OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL")\
  X(l3_amt,      "LFO3 AMOUNT", 0.f,  -1.f, 1.f, GW_KIND_BIPOL,  nullptr)                               \
  X(rnd_rate,    "RND RATE",    0.3f,  0.f, 1.f, GW_KIND_HZ,     nullptr)                             \
  X(rnd_smooth,  "RND SMOOTH",  0.3f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(chirp_time,  "CHIRP TIME",  0.7117f,  0.f, 1.f, GW_KIND_SEC,    nullptr)                             \
  /* ---- the modulation matrix: eight slots ---- */                                                  \
  X(m1_src,"MOD1 SOURCE",0.f,0.f,13.f,GW_KIND_CHOICE,"OFF|ENV1|ENV2|ENV3|LFO1|LFO2|LFO3|RANDOM|CHIRP|MASS|VELOCITY|KEY|WHEEL|PRESSURE")      \
  X(m1_dst,"MOD1 DEST",  0.f,0.f,21.f,GW_KIND_CHOICE,"OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL") \
  X(m1_amt,"MOD1 AMOUNT",0.f,-1.f,1.f,GW_KIND_BIPOL,nullptr)                                          \
  X(m2_src,"MOD2 SOURCE",0.f,0.f,13.f,GW_KIND_CHOICE,"OFF|ENV1|ENV2|ENV3|LFO1|LFO2|LFO3|RANDOM|CHIRP|MASS|VELOCITY|KEY|WHEEL|PRESSURE")      \
  X(m2_dst,"MOD2 DEST",  0.f,0.f,21.f,GW_KIND_CHOICE,"OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL") \
  X(m2_amt,"MOD2 AMOUNT",0.f,-1.f,1.f,GW_KIND_BIPOL,nullptr)                                          \
  X(m3_src,"MOD3 SOURCE",0.f,0.f,13.f,GW_KIND_CHOICE,"OFF|ENV1|ENV2|ENV3|LFO1|LFO2|LFO3|RANDOM|CHIRP|MASS|VELOCITY|KEY|WHEEL|PRESSURE")      \
  X(m3_dst,"MOD3 DEST",  0.f,0.f,21.f,GW_KIND_CHOICE,"OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL") \
  X(m3_amt,"MOD3 AMOUNT",0.f,-1.f,1.f,GW_KIND_BIPOL,nullptr)                                          \
  X(m4_src,"MOD4 SOURCE",0.f,0.f,13.f,GW_KIND_CHOICE,"OFF|ENV1|ENV2|ENV3|LFO1|LFO2|LFO3|RANDOM|CHIRP|MASS|VELOCITY|KEY|WHEEL|PRESSURE")      \
  X(m4_dst,"MOD4 DEST",  0.f,0.f,21.f,GW_KIND_CHOICE,"OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL") \
  X(m4_amt,"MOD4 AMOUNT",0.f,-1.f,1.f,GW_KIND_BIPOL,nullptr)                                          \
  X(m5_src,"MOD5 SOURCE",0.f,0.f,13.f,GW_KIND_CHOICE,"OFF|ENV1|ENV2|ENV3|LFO1|LFO2|LFO3|RANDOM|CHIRP|MASS|VELOCITY|KEY|WHEEL|PRESSURE")      \
  X(m5_dst,"MOD5 DEST",  0.f,0.f,21.f,GW_KIND_CHOICE,"OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL") \
  X(m5_amt,"MOD5 AMOUNT",0.f,-1.f,1.f,GW_KIND_BIPOL,nullptr)                                          \
  X(m6_src,"MOD6 SOURCE",0.f,0.f,13.f,GW_KIND_CHOICE,"OFF|ENV1|ENV2|ENV3|LFO1|LFO2|LFO3|RANDOM|CHIRP|MASS|VELOCITY|KEY|WHEEL|PRESSURE")      \
  X(m6_dst,"MOD6 DEST",  0.f,0.f,21.f,GW_KIND_CHOICE,"OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL") \
  X(m6_amt,"MOD6 AMOUNT",0.f,-1.f,1.f,GW_KIND_BIPOL,nullptr)                                          \
  X(m7_src,"MOD7 SOURCE",0.f,0.f,13.f,GW_KIND_CHOICE,"OFF|ENV1|ENV2|ENV3|LFO1|LFO2|LFO3|RANDOM|CHIRP|MASS|VELOCITY|KEY|WHEEL|PRESSURE")      \
  X(m7_dst,"MOD7 DEST",  0.f,0.f,21.f,GW_KIND_CHOICE,"OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL") \
  X(m7_amt,"MOD7 AMOUNT",0.f,-1.f,1.f,GW_KIND_BIPOL,nullptr)                                          \
  X(m8_src,"MOD8 SOURCE",0.f,0.f,13.f,GW_KIND_CHOICE,"OFF|ENV1|ENV2|ENV3|LFO1|LFO2|LFO3|RANDOM|CHIRP|MASS|VELOCITY|KEY|WHEEL|PRESSURE")      \
  X(m8_dst,"MOD8 DEST",  0.f,0.f,21.f,GW_KIND_CHOICE,"OFF|PITCH|A SHAPE|B SHAPE|A WIDTH|PM|RING|SUB|NOISE|A CUTOFF|B CUTOFF|A RES|B RES|SPLIT|BLEND|DISK|DRIVE|MASS|REDSHIFT|RINGDOWN|PAN|LEVEL") \
  X(m8_amt,"MOD8 AMOUNT",0.f,-1.f,1.f,GW_KIND_BIPOL,nullptr)                                          \
  /* ---- the sequencer.  A credible 303 is half sequencer: the accent                              \
        circuit ACCUMULATES over consecutive accented steps and that is the                         \
        squelch.  The per-step data (on/note/accent/slide/gate x 32) lives in                       \
        the state blob, NOT as host parameters - 160 automation lanes would                         \
        be unusable, and FMR set that precedent. ---- */                                            \
  X(seq_on,      "SEQ",         0.f,   0.f, 1.f, GW_KIND_CHOICE, "OFF|ON")                           \
  X(seq_len,     "SEQ STEPS",   16.f,  1.f, 32.f,GW_KIND_INT,    nullptr)                             \
  X(seq_div,     "SEQ DIVISION",2.f,   0.f, 5.f, GW_KIND_CHOICE, "1/4|1/8|1/8T|1/16|1/16T|1/32")     \
  X(seq_swing,   "SEQ SWING",   0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(seq_gate,    "SEQ GATE",    0.5f,  0.05f,1.f,GW_KIND_PCT,    nullptr)                             \
  X(seq_slide,   "SEQ SLIDE",   0.3f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(seq_accent,  "SEQ ACCENT",  0.6f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(seq_octave,  "SEQ OCTAVE",  0.f,  -2.f, 2.f, GW_KIND_INT,    nullptr)                             \
  /* ---- drive and output ---- */                                                                    \
  X(drv_type,    "DRIVE",       0.f,   0.f, 3.f, GW_KIND_CHOICE, "IDLE BURN|ION|PLASMA|COLLAPSE")     \
  X(drv_amt,     "DRIVE AMOUNT",0.f,   0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(drv_pos,     "DRIVE POSITION",0.f, 0.f, 1.f, GW_KIND_CHOICE, "PRE|POST")                            \
  X(glide,       "GLIDE",       0.0f,   0.f, 1.f, GW_KIND_SEC,    nullptr)                             \
  X(voicing,     "VOICING",     0.f,   0.f, 1.f, GW_KIND_CHOICE, "MONO|DUO")                          \
  X(bend_range,  "BEND RANGE",  2.f,   0.f, 24.f,GW_KIND_INT,    nullptr)                             \
  X(transpose,   "TRANSPOSE",   0.f,  -1.f, 1.f, GW_KIND_BIPOL,  nullptr)                               \
  X(vel_amt,     "VELOCITY",    0.3f,  0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(vintage,     "VINTAGE",     0.15f, 0.f, 1.f, GW_KIND_PCT,    nullptr)                             \
  X(tilt,        "TONE TILT",   0.f,  -1.f, 1.f, GW_KIND_BIPOL,  nullptr)                          \
  X(out_trim,    "OUT TRIM",    0.f, -24.f, 8.f, GW_KIND_DB,     nullptr)

enum PID {
  #define X(id,...) P_##id,
    GW_PARAMS(X)
  #undef X
    NUM_PARAMS
};

struct PSpec {
    const char* id;
    const char* label;
    float def, lo, hi;
    int   kind;
    const char* choices;      // '|' separated, or nullptr
};

const PSpec& spec (int i);
int          numParams();

/*  The display law of a parameter stored as a 0..1 position on an
    exponential curve: physical = base * span^v (Hz or seconds).  ONE
    statement of it, read by the host text and by the panel.  false for a
    parameter that is shown as it is stored.                            */
bool expLaw (int pid, float& base, float& span);

/*  LFO tempo divisions, slowest first: cycles per BEAT, and the name the
    panel shows.  One table, read by the engine and sent to the page.    */
/*  Which MODE each circuit honours - bit 0 LP, 1 BP, 2 HP, 3 NOTCH.  The
    ladder, the diode and the formant bank are lowpass circuits; GROWL and
    SCREAM do LP and HP; COMB turns NOTCH into its negative comb.  The panel
    dims what a circuit ignores, and the bench proves this table against
    what the circuits actually do.                                        */
constexpr unsigned MODE_MASK[7] = { 0x1, 0x5, 0x5, 0xF, 0x1, 0x9, 0x1 };

//  worst relative error of the wavetable fast path against its additive
//  recipe, over every table, three shapes and two bandwidths (bench only)
double wavetableWorstError();

//  the sine amplitude each DRIVE position is calibrated at: the level it is fed
constexpr float DRIVE_REF_PRE  = 0.80f;
constexpr float DRIVE_REF_POST = 0.40f;
constexpr float DRIVE_REF_COLLAPSE = 0.60f;   //  x the position reference

constexpr int N_LFO_DIV = 16;
extern const float        LFO_DIV_CPB[N_LFO_DIV];
extern const char* const  LFO_DIV_NAME[N_LFO_DIV];

struct Params {
    float v[NUM_PARAMS];
    Params();                 // every value at its own default
};

// ------------------------------------------------------------- the constants
/*  The well's geometry.  A modulator's RADIUS knob maps onto [R_NEAR, R_FAR];
    rs reaches R_HORIZON_MAX at MASS 1 with HORIZON wide open, so a modulator
    at radius 0 is comfortably inside the horizon and really does freeze.      */

/*  An envelope edge the ear hears AS an edge.  timeSec(0) is half a
    millisecond; a half-millisecond attack on a 41 Hz note is a broadband
    transient whatever the oscillator is doing, and a real envelope generator
    cannot slew that fast either.  Measured in the bench, not guessed.      */
constexpr float ATT_FLOOR = 0.0020f;   // 2 ms - still a snappy bass attack
constexpr float REL_FLOOR = 0.0040f;   // 4 ms - a note must not end in a step
constexpr float R_NEAR        = 1.0f;
constexpr float R_FAR         = 12.0f;
constexpr float R_HORIZON_MAX = 6.0f;

/*  Schwarzschild quasi-normal modes, l=2 n=0..2 and l=3,4 n=0, as ratios of
    the fundamental (M*omega = 0.3737 - 0.0890i).  Real, published numbers -
    they are what makes the body inharmonic in a way nobody guessed at.       */
constexpr int   NQNM = 5;
extern const float QNM_F[NQNM];   // frequency ratios
extern const float QNM_D[NQNM];   // damping ratios (omega_I / omega_R of mode 0)

// -------------------------------------------------------------- the pattern
/*  One step is six plain bytes, so the whole pattern is 192 bytes and rides
    in the state blob and in a preset.  note is a semitone offset from the
    played key, so a pattern transposes with whatever you hold.             */
constexpr int MAX_STEPS = 32;
struct Step {
    uint8_t on     = 1;      // 0 = rest
    int8_t  note   = 0;      // semitones from the held key, -24..+24
    uint8_t accent = 0;      // 0/1 - drives the accumulating accent circuit
    uint8_t slide  = 0;      // 0/1 - legato, no envelope retrigger
    uint8_t gate   = 50;     // per-step gate length, percent of the step
    uint8_t ratchet= 1;      // 1..4 retriggers inside the step
};

// --------------------------------------------------------------- the engine
class Engine {
public:
    Engine();
    void  prepare (double sampleRate, int maxBlock);
    void  reset();

    void  noteOn  (int midiNote, float velocity);
    void  noteOff (int midiNote);
    void  allNotesOff();                 // CC123/CC120 - and it lets go of the pedal
    void  sustain (bool down);
    void  pitchBend (float wheel);        //  the pitch wheel, -1..1; BEND RANGE is applied here

    void  setTempo (double bpm)          { hostBpm = bpm; }
    //  the host's bar position, so a synced LFO lands on the grid
    void  setPosition (double ppq, bool playing) { hostPpq = ppq; hostPlaying = playing; }
    //  the mod wheel and aftertouch - matrix sources that used to read 0
    void  setWheel (float v01);
    void  setPressure (float v01);
    void  process (float* L, float* R, int n);

    Params p;                            // the caller writes straight into this

    // --- what the panel draws, and what the bench measures -----------------
    float massNow()      const { return massSm; }        // 0..1
    float rsNow()        const { return rsSm; }          // Schwarzschild radius
    float dilationAt (float radius01) const;             // sqrt(1 - rs/r)
    float redshiftNow()  const;                          // spectral factor
    float modValue (int i) const { return modOut[i]; }   // 0..6 as the matrix numbers them
    bool  modFrozen (int i) const { return modFroze[i]; }
    float scope (int i)  const { return scopeBuf[i]; }
    int   scopeLen()     const { return (int) scopeBuf.size(); }
    int   scopeWrite()   const { return scopeW; }
    float outPeak()      const { return peakHold; }
    //  the OUTPUT meters: peak and rms per channel, and how hard the soft
    //  ceiling is working (0 = not touched, 1 = driven well past it)
    float meterPeak (int ch) const { return ch ? mPkR : mPkL; }
    float meterRms  (int ch) const { return std::sqrt (ch ? mMsR : mMsL); }
    float meterOver ()       const { return mOver; }
    float heavyNow ()        const { return heavy; }
    int   soundingVoices () const;        //  bench: how many voices are still making sound
    float stopJump () const;              //  bench: the largest step to silence when a voice stopped

    //  Measured at prepare, not typed: each drive's own make-up so that
    //  turning DRIVE up does not turn the instrument up.  (Battlestar's rule;
    //  a table written by hand goes stale the first time a curve is retuned.)
    float driveTrim (int type, float amount, bool post = true) const;

    // --- the sequencer -----------------------------------------------
    void  setStep (int i, const Step& s);
    Step  getStep (int i) const;
    int   seqStep() const;                 // which step is sounding, -1 when idle
    float seqPhase() const;                // 0..1 through the current step
    void  seqReset();

private:
    struct Voice;
    struct Impl;
    Impl* im = nullptr;

    double sr = 48000.0, hostBpm = 120.0, hostPpq = 0.0;
    bool   hostPlaying = false;
    float  massSm = 0.f, rsSm = 0.f;
    float  heavy = 0.f;                   //  MASS past noon, smoothed: 0 at or below it
    float  modOut[10] {};
    bool   modFroze[10] {};
    std::vector<float> scopeBuf;
    int    scopeW = 0;
    float  peakHold = 0.f;
    float  mPkL = 0.f, mPkR = 0.f, mMsL = 0.f, mMsR = 0.f, mOver = 0.f;

public:
    ~Engine();
    Engine (const Engine&) = delete;
    Engine& operator= (const Engine&) = delete;
};

// ------------------------------------------------------------------ presets
/*  0..31 are the factory bank, ordered from the ordinary to the strange:
    0-9 foundations, 10-19 the working basses, 20-27 character, 28-31 the
    esoteric end.  32..199 are user slots and live on disk.  Thirty-two that
    are genuinely different beat a hundred with filler in them.              */
constexpr int NUM_FACTORY = 32;    // slots 0..31
constexpr int NUM_USER    = 168;   // slots 32..199, on disk

int          numFactory();
const char*  factoryName (int i);
const char*  factoryGroup (int i);      // "FOUNDATION" | "WORKING" | "CHARACTER" | "EVENT HORIZON"
void         applyFactory (int i, Params& p);
void         applyFactoryPattern (int i, Step* dst, int n);

} // namespace gw
