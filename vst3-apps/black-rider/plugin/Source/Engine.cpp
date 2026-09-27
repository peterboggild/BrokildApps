/*  BLACK RIDER — engine implementation. The reasoning lives in Engine.h;
    this file is the machinery. */
#include "Engine.h"
#include <cstring>

namespace bk
{

//==============================================================================
// knob -> value
float glideMs (float v)    { return v < 0.01f ? 0.0f : xmap (v, 5.0f, 3000.0f); }
float egAttackMs (float v) { return xmap (v, 0.5f, 10000.0f); }
float egDecayMs (float v)  { return xmap (v, 2.0f, 15000.0f); }
float hzOf (const PSpec& s, float v) { return xmap (v, s.lo, s.hi); }

static inline float lfoHz (float v)   { return xmap (v, 0.02f, 2000.0f); }
/*  The feedback coefficient K for the Sallen-Key. Onset of self-oscillation
    is around K = 2. GROWL reaches it only near the top of the dial (v ~ 0.88),
    so it stays a gnarly resonance for most of the travel and screams only at
    the end. SCREAM crosses K = 2 at exactly noon, so from twelve o'clock up it
    self-oscillates and, with KEY track up, plays in tune from the keyboard. */
static inline float k35K (float v)    { return 2.25f * std::pow (clamp01 (v), 0.9f); }
static inline float k35Kscream (float v)
{
    v = clamp01 (v);
    /*  Onset just past two o'clock (v = 0.62 of the travel), and pushed a lot
        harder above it — K runs to 2.9, deep into the clipper, so the top of
        the dial is a loud, filthy scream rather than a polite whistle. */
    return v < 0.62f ? 2.0f * std::pow (v / 0.62f, 0.7f)
                     : 2.0f + 0.9f * ((v - 0.62f) / 0.38f);
}
static inline float ladderR (float v) { return 1.12f * std::pow (clamp01 (v), 0.85f); }

/*  Host-clock divisions for the LFO and the tape delay: quarters per cycle,
    and the triplet / dotted feel multiplier. */
static inline float syncQuarters (int div)   // 0 = 1/1 ... 4 = 1/16
{
    static const float q[5] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f };
    return q[div < 0 ? 0 : (div > 4 ? 4 : div)];
}
static inline float feelMul (int feel) { return feel == 1 ? (2.0f / 3.0f) : (feel == 2 ? 1.5f : 1.0f); }
static inline float semiOf (float v)  { return std::round ((v - 0.5f) * 24.0f); }
static inline float centOf (float v, float span) { return (v - 0.5f) * span; }
static inline float volGain (float v) { return v < 0.005f ? 0.0f : 2.0f * v * v; }

//==============================================================================
// The parameter table.
#define PROW(field) [] (Params& p) -> float& { return p.field; }

static const PSpec SPECS[] =
{
    // ---- voice ---------------------------------------------------------
    { "mode",    "VOICE MODE",       0.0f,  KP_LIST,  0, 2,        PROW (mode)    },
    { "spread",  "CHORD SPREAD",     0.7f,  KP_PCT,   0, 0,        PROW (spread)  },
    { "udet",    "UNISON DETUNE",    0.25f, KP_CENTU, 50, 0,       PROW (udet)    },
    { "glide",   "GLIDE",            0.0f,  KP_GLIDE, 5, 3000,     PROW (glide)   },
    { "legato",  "LEGATO",           1.0f,  KP_SW,    0, 0,        PROW (legato)  },
    { "tune",    "MASTER TUNE",      0.5f,  KP_CENT,  200, 0,      PROW (tune)    },
    { "bend",    "BEND RANGE",       2.0f,  KP_INT,   0, 12,       PROW (bend)    },
    { "vintage", "VINTAGE",          0.35f, KP_PCT,   0, 0,        PROW (vintage) },
    { "os",      "QUALITY",          1.0f,  KP_LIST,  0, 2,        PROW (os)      },
    { "volume",  "VOLUME",           0.5f,  KP_VOL,   0, 0,        PROW (volume)  },
    { "seed",    "SEED PATCH",       0.0f,  KP_INT,   0, NUM_SEEDS - 1, PROW (seed) },

    // ---- vco 1 ---------------------------------------------------------
    { "o1wave",  "VCO1 WAVE",        0.0f,  KP_LIST,  0, 3,        PROW (o1wave)  },
    { "o1oct",   "VCO1 RANGE",       2.0f,  KP_LIST,  0, 3,        PROW (o1oct)   },
    { "o1semi",  "VCO1 SEMITONE",    0.5f,  KP_SEMI,  0, 0,        PROW (o1semi)  },
    { "o1fine",  "VCO1 FINE",        0.5f,  KP_CENT,  100, 0,      PROW (o1fine)  },
    { "o1pw",    "VCO1 WIDTH",       0.5f,  KP_PW,    0, 0,        PROW (o1pw)    },
    { "o1pwm",   "VCO1 PWM",         0.0f,  KP_PCT,   0, 0,        PROW (o1pwm)   },
    { "o1lvl",   "VCO1 LEVEL",       0.8f,  KP_PCT,   0, 0,        PROW (o1lvl)   },

    // ---- vco 2 ---------------------------------------------------------
    { "o2wave",  "VCO2 WAVE",        0.0f,  KP_LIST,  0, 3,        PROW (o2wave)  },
    { "o2oct",   "VCO2 RANGE",       2.0f,  KP_LIST,  0, 3,        PROW (o2oct)   },
    { "o2semi",  "VCO2 SEMITONE",    0.5f,  KP_SEMI,  0, 0,        PROW (o2semi)  },
    { "o2fine",  "VCO2 FINE",        0.52f, KP_CENT,  100, 0,      PROW (o2fine)  },
    { "o2pw",    "VCO2 WIDTH",       0.5f,  KP_PW,    0, 0,        PROW (o2pw)    },
    { "o2pwm",   "VCO2 PWM",         0.0f,  KP_PCT,   0, 0,        PROW (o2pwm)   },
    { "o2lvl",   "VCO2 LEVEL",       0.7f,  KP_PCT,   0, 0,        PROW (o2lvl)   },
    { "o2sync",  "VCO2 SYNC",        0.0f,  KP_SW,    0, 0,        PROW (o2sync)  },
    { "o2fm",    "VCO1 > VCO2 FM",   0.0f,  KP_PCT,   0, 0,        PROW (o2fm)    },
    { "o2kbd",   "VCO2 KEYBOARD",    1.0f,  KP_SW,    0, 0,        PROW (o2kbd)   },

    // ---- mixer ---------------------------------------------------------
    { "suboct",  "SUB RANGE",        0.0f,  KP_LIST,  0, 1,        PROW (suboct)  },
    { "sublvl",  "SUB LEVEL",        0.0f,  KP_PCT,   0, 0,        PROW (sublvl)  },
    { "nzcol",   "NOISE COLOUR",     0.0f,  KP_LIST,  0, 1,        PROW (nzcol)   },
    { "nzlvl",   "NOISE LEVEL",      0.0f,  KP_PCT,   0, 0,        PROW (nzlvl)   },
    { "ringlvl", "RING MOD LEVEL",   0.0f,  KP_PCT,   0, 0,        PROW (ringlvl) },
    { "fdrive",  "FILTER DRIVE",     0.2f,  KP_PCT,   0, 0,        PROW (fdrive)  },

    // ---- filter --------------------------------------------------------
    { "fmodel",  "FILTER MODEL",     0.0f,  KP_LIST,  0, 2,        PROW (fmodel)  },
    { "hpf",     "HPF CUTOFF",       0.0f,  KP_HZ,    20, 8000,    PROW (hpf)     },
    { "hpeak",   "HPF PEAK",         0.0f,  KP_PCT,   0, 0,        PROW (hpeak)   },
    { "lpf",     "LPF CUTOFF",       0.6f,  KP_HZ,    20, 20000,   PROW (lpf)     },
    { "lpeak",   "LPF PEAK",         0.2f,  KP_PCT,   0, 0,        PROW (lpeak)   },
    { "fenv",    "EG1 > CUTOFF",     0.65f, KP_BIPOL, 0, 0,        PROW (fenv)    },
    { "fkey",    "KEY TRACK",        0.5f,  KP_PCT,   0, 0,        PROW (fkey)    },
    { "flfo",    "LFO > CUTOFF",     0.0f,  KP_PCT,   0, 0,        PROW (flfo)    },

    // ---- envelopes -----------------------------------------------------
    { "e1a",     "EG1 ATTACK",       0.1f,  KP_MS,    0.5f, 10000, PROW (e1a)     },
    { "e1d",     "EG1 DECAY",        0.45f, KP_MS,    2, 15000,    PROW (e1d)     },
    { "e1s",     "EG1 SUSTAIN",      0.3f,  KP_PCT,   0, 0,        PROW (e1s)     },
    { "e1r",     "EG1 RELEASE",      0.4f,  KP_MS,    2, 15000,    PROW (e1r)     },
    { "e1vel",   "EG1 VELOCITY",     0.3f,  KP_PCT,   0, 0,        PROW (e1vel)   },
    { "e2a",     "EG2 ATTACK",       0.05f, KP_MS,    0.5f, 10000, PROW (e2a)     },
    { "e2d",     "EG2 DECAY",        0.4f,  KP_MS,    2, 15000,    PROW (e2d)     },
    { "e2s",     "EG2 SUSTAIN",      0.8f,  KP_PCT,   0, 0,        PROW (e2s)     },
    { "e2r",     "EG2 RELEASE",      0.35f, KP_MS,    2, 15000,    PROW (e2r)     },
    { "e2vel",   "EG2 VELOCITY",     0.5f,  KP_PCT,   0, 0,        PROW (e2vel)   },

    // ---- lfo -----------------------------------------------------------
    { "lwave",   "LFO WAVE",         0.0f,  KP_LIST,  0, 5,        PROW (lwave)   },
    { "lrate",   "LFO RATE",         0.45f, KP_HZ,    0.02f, 2000, PROW (lrate)   },
    { "lsync",   "LFO SYNC",         0.0f,  KP_LIST,  0, 5,        PROW (lsync)   },
    { "lfeel",   "LFO SYNC FEEL",    0.0f,  KP_LIST,  0, 2,        PROW (lfeel)   },
    { "lpitch",  "LFO > PITCH",      0.0f,  KP_PCT,   0, 0,        PROW (lpitch)  },
    { "lwheel",  "WHEEL > VIBRATO",  0.5f,  KP_PCT,   0, 0,        PROW (lwheel)  },
    { "lkey",    "LFO KEY RESET",    0.0f,  KP_SW,    0, 0,        PROW (lkey)    },

    // ---- vca -----------------------------------------------------------
    { "vcamode", "VCA MODE",         0.0f,  KP_LIST,  0, 2,        PROW (vcamode) },

    // ---- effects -------------------------------------------------------
    { "drv",     "DRIVE",            0.0f,  KP_PCT,   0, 0,        PROW (drv)     },
    { "drvtone", "DRIVE TONE",       0.6f,  KP_HZ,    1000, 16000, PROW (drvtone) },
    { "chrate",  "CHORUS RATE",      0.4f,  KP_HZ,    0.1f, 8,     PROW (chrate)  },
    { "chdepth", "CHORUS DEPTH",     0.5f,  KP_PCT,   0, 0,        PROW (chdepth) },
    { "chmix",   "CHORUS MIX",       0.0f,  KP_PCT,   0, 0,        PROW (chmix)   },
    { "dltime",  "DELAY TIME",       0.55f, KP_MS,    20, 1500,    PROW (dltime)  },
    { "dlsync",  "DELAY SYNC",       0.0f,  KP_LIST,  0, 5,        PROW (dlsync)  },
    { "dlfeel",  "DELAY SYNC FEEL",  0.0f,  KP_LIST,  0, 2,        PROW (dlfeel)  },
    { "dlfb",    "DELAY FEEDBACK",   0.4f,  KP_PCT,   0, 0,        PROW (dlfb)    },
    { "dltone",  "DELAY TONE",       0.5f,  KP_HZ,    1500, 12000, PROW (dltone)  },
    { "dlwow",   "DELAY WOW",        0.3f,  KP_PCT,   0, 0,        PROW (dlwow)   },
    { "dlmix",   "DELAY MIX",        0.0f,  KP_PCT,   0, 0,        PROW (dlmix)   },
    { "spdwell", "SPRING DWELL",     0.5f,  KP_PCT,   0, 0,        PROW (spdwell) },
    { "sptone",  "SPRING TONE",      0.5f,  KP_HZ,    1500, 7000,  PROW (sptone)  },
    { "spmix",   "SPRING MIX",       0.0f,  KP_PCT,   0, 0,        PROW (spmix)   },

    // ---- patch bay -----------------------------------------------------
    { "c1src", "CABLE 1 FROM", 0, KP_INT, 0, NUM_SOURCES - 1, PROW (cable[0].src) },
    { "c1dst", "CABLE 1 TO",   0, KP_INT, 0, NUM_DESTS - 1,   PROW (cable[0].dst) },
    { "c1amt", "CABLE 1 AMOUNT", 0.5f, KP_BIPOL, 0, 0,       PROW (cable[0].amt) },
    { "c2src", "CABLE 2 FROM", 0, KP_INT, 0, NUM_SOURCES - 1, PROW (cable[1].src) },
    { "c2dst", "CABLE 2 TO",   0, KP_INT, 0, NUM_DESTS - 1,   PROW (cable[1].dst) },
    { "c2amt", "CABLE 2 AMOUNT", 0.5f, KP_BIPOL, 0, 0,       PROW (cable[1].amt) },
    { "c3src", "CABLE 3 FROM", 0, KP_INT, 0, NUM_SOURCES - 1, PROW (cable[2].src) },
    { "c3dst", "CABLE 3 TO",   0, KP_INT, 0, NUM_DESTS - 1,   PROW (cable[2].dst) },
    { "c3amt", "CABLE 3 AMOUNT", 0.5f, KP_BIPOL, 0, 0,       PROW (cable[2].amt) },
    { "c4src", "CABLE 4 FROM", 0, KP_INT, 0, NUM_SOURCES - 1, PROW (cable[3].src) },
    { "c4dst", "CABLE 4 TO",   0, KP_INT, 0, NUM_DESTS - 1,   PROW (cable[3].dst) },
    { "c4amt", "CABLE 4 AMOUNT", 0.5f, KP_BIPOL, 0, 0,       PROW (cable[3].amt) },
    { "c5src", "CABLE 5 FROM", 0, KP_INT, 0, NUM_SOURCES - 1, PROW (cable[4].src) },
    { "c5dst", "CABLE 5 TO",   0, KP_INT, 0, NUM_DESTS - 1,   PROW (cable[4].dst) },
    { "c5amt", "CABLE 5 AMOUNT", 0.5f, KP_BIPOL, 0, 0,       PROW (cable[4].amt) },
    { "c6src", "CABLE 6 FROM", 0, KP_INT, 0, NUM_SOURCES - 1, PROW (cable[5].src) },
    { "c6dst", "CABLE 6 TO",   0, KP_INT, 0, NUM_DESTS - 1,   PROW (cable[5].dst) },
    { "c6amt", "CABLE 6 AMOUNT", 0.5f, KP_BIPOL, 0, 0,       PROW (cable[5].amt) },
    { "c7src", "CABLE 7 FROM", 0, KP_INT, 0, NUM_SOURCES - 1, PROW (cable[6].src) },
    { "c7dst", "CABLE 7 TO",   0, KP_INT, 0, NUM_DESTS - 1,   PROW (cable[6].dst) },
    { "c7amt", "CABLE 7 AMOUNT", 0.5f, KP_BIPOL, 0, 0,       PROW (cable[6].amt) },
    { "c8src", "CABLE 8 FROM", 0, KP_INT, 0, NUM_SOURCES - 1, PROW (cable[7].src) },
    { "c8dst", "CABLE 8 TO",   0, KP_INT, 0, NUM_DESTS - 1,   PROW (cable[7].dst) },
    { "c8amt", "CABLE 8 AMOUNT", 0.5f, KP_BIPOL, 0, 0,       PROW (cable[7].amt) },
};
#undef PROW

int          numParams()        { return (int) (sizeof (SPECS) / sizeof (SPECS[0])); }
const PSpec& paramSpec (int i)  { return SPECS[i < 0 ? 0 : (i >= numParams() ? numParams() - 1 : i)]; }
int paramIndex (const char* id)
{
    for (int i = 0; i < numParams(); ++i) if (std::strcmp (SPECS[i].id, id) == 0) return i;
    return -1;
}
float paramMax (const PSpec& s) { return (s.kind == KP_LIST || s.kind == KP_INT) ? s.hi : 1.0f; }

static const char* const MODE_NAMES[]  = { "MONO", "UNISON", "POLY 5" };
static const char* const OS_NAMES[]    = { "1x", "2x", "4x" };
static const char* const WAVE_NAMES[]  = { "SAW", "TRIANGLE", "PULSE", "SINE" };
static const char* const OCT_NAMES[]   = { "32'", "16'", "8'", "4'" };
static const char* const SUB_NAMES[]   = { "-1 OCT", "-2 OCT" };
static const char* const NZ_NAMES[]    = { "WHITE", "PINK" };
static const char* const MODEL_NAMES[] = { "GROWL", "SCREAM", "LADDER" };
static const char* const LFO_NAMES[]   = { "TRIANGLE", "SAW", "SQUARE", "SINE", "S&H", "DRIFT" };
static const char* const VCA_NAMES[]   = { "EG2", "GATE", "DRONE" };
static const char* const SYNC_NAMES[]  = { "FREE", "1/1", "1/2", "1/4", "1/8", "1/16" };
static const char* const FEEL_NAMES[]  = { "STRAIGHT", "TRIPLET", "DOTTED" };

const char* const* listNames (const char* id, int& count)
{
    auto is = [id] (const char* s) { return std::strcmp (id, s) == 0; };
    if (is ("mode"))    { count = 3; return MODE_NAMES; }
    if (is ("os"))      { count = 3; return OS_NAMES; }
    if (is ("o1wave") || is ("o2wave")) { count = 4; return WAVE_NAMES; }
    if (is ("o1oct")  || is ("o2oct"))  { count = 4; return OCT_NAMES; }
    if (is ("suboct")) { count = 2; return SUB_NAMES; }
    if (is ("nzcol"))  { count = 2; return NZ_NAMES; }
    if (is ("fmodel")) { count = 3; return MODEL_NAMES; }
    if (is ("lwave"))  { count = 6; return LFO_NAMES; }
    if (is ("vcamode")){ count = 3; return VCA_NAMES; }
    if (is ("lsync") || is ("dlsync")) { count = 6; return SYNC_NAMES; }
    if (is ("lfeel") || is ("dlfeel")) { count = 3; return FEEL_NAMES; }
    count = 0; return nullptr;
}

static const char* const SRC_NAMES[NUM_SOURCES] =
{ "-", "VCO 1", "VCO 2", "NOISE", "FILTER OUT", "VCA OUT", "EG 1", "EG 2", "LFO", "S&H",
  "KEY CV", "VELOCITY", "MOD WHEEL", "AFTERTOUCH", "PITCH BEND", "GATE" };
static const char* const DST_NAMES[NUM_DESTS] =
{ "-", "VCO1 PITCH", "VCO2 PITCH", "VCO PITCH", "VCO1 WIDTH", "VCO2 WIDTH", "VCO2 LIN FM",
  "HPF CUTOFF", "LPF CUTOFF", "LPF PEAK", "HPF PEAK", "FILTER IN", "VCA CV", "VCA IN",
  "LFO RATE", "PAN", "DRIVE", "DELAY TIME" };

const char* sourceName (int i) { return SRC_NAMES[i < 0 ? 0 : (i >= NUM_SOURCES ? 0 : i)]; }
const char* destName (int i)   { return DST_NAMES[i < 0 ? 0 : (i >= NUM_DESTS ? 0 : i)]; }

//==============================================================================
// The factory patches. Defined here so the bench can render exactly what
// the panel loads.
struct Recipe { const char* name; const char* blurb; void (*fn) (Params&); };

static void cable (Params& p, int slot, int src, int dst, float amt /* -1..1 */)
{
    p.cable[slot].src = (float) src; p.cable[slot].dst = (float) dst; p.cable[slot].amt = 0.5f + 0.5f * amt;
}

static const Recipe RECIPES[NUM_RECIPES] =
{
    { "BLACK BASS", "two saws an octave apart into the ladder, the bass the whole thing was built for",
      [] (Params& p) { p = Params(); p.mode = 0; p.o1oct = 1; p.o2oct = 0; p.o2fine = 0.53f; p.o1lvl = 0.8f; p.o2lvl = 0.75f;
                       p.sublvl = 0.25f; p.fmodel = 2; p.lpf = 0.36f; p.lpeak = 0.22f; p.fenv = 0.68f; p.fkey = 0.35f;
                       p.fdrive = 0.35f; p.e1a = 0.02f; p.e1d = 0.38f; p.e1s = 0.15f; p.e1r = 0.3f;
                       p.e2a = 0.02f; p.e2d = 0.45f; p.e2s = 0.85f; p.e2r = 0.28f; p.glide = 0.0f; p.drv = 0.12f; } },
    { "RIDER LEAD", "saw and pulse through the growl filter, glide, vibrato on the wheel",
      [] (Params& p) { p = Params(); p.o1wave = 0; p.o2wave = 2; p.o2pw = 0.3f; p.o2fine = 0.56f; p.o2lvl = 0.6f;
                       p.fmodel = 0; p.lpf = 0.55f; p.lpeak = 0.62f; p.fenv = 0.62f; p.fkey = 0.6f; p.fdrive = 0.3f;
                       p.e1d = 0.5f; p.e1s = 0.4f; p.e2s = 0.9f; p.e2r = 0.42f; p.glide = 0.35f; p.legato = 1;
                       p.lrate = 0.5f; p.lwheel = 0.6f; p.dltime = 0.5f; p.dlfb = 0.35f; p.dlmix = 0.22f; } },
    { "SCREAM SWEEP", "the SCREAM filter past the edge, self-oscillating, swept by the envelope",
      [] (Params& p) { p = Params(); p.o1wave = 0; p.o2lvl = 0; p.o1lvl = 0.55f; p.fmodel = 1; p.lpf = 0.3f; p.lpeak = 0.85f;
                       p.fenv = 0.85f; p.fkey = 1.0f; p.e1a = 0.2f; p.e1d = 0.6f; p.e1s = 0.0f; p.e1r = 0.5f;
                       p.e2a = 0.1f; p.e2s = 1.0f; p.e2r = 0.5f; p.hpf = 0.25f; p.hpeak = 0.3f; p.spmix = 0.25f; } },
    { "SYNCOPATH", "VCO2 hard-synced, its pitch swept by EG1 through the bay",
      [] (Params& p) { p = Params(); p.o1lvl = 0.0f; p.o2lvl = 0.85f; p.o2sync = 1; p.o2wave = 0; p.o2oct = 2; p.o2semi = 0.5f + 7.0f / 24.0f;
                       p.fmodel = 2; p.lpf = 0.72f; p.lpeak = 0.1f; p.fenv = 0.55f; p.e1a = 0.02f; p.e1d = 0.55f; p.e1s = 0.0f;
                       p.e2s = 0.8f; p.e2r = 0.35f; cable (p, 0, S_EG1, D_P2, 0.75f); p.drv = 0.15f; } },
    { "TAPE PLUCK", "a narrow pulse, a fast envelope, and the tape echo doing the arrangement",
      [] (Params& p) { p = Params(); p.o1wave = 2; p.o1pw = 0.55f; p.o2wave = 2; p.o2pw = 0.2f; p.o2oct = 3; p.o2lvl = 0.4f;
                       p.fmodel = 2; p.lpf = 0.45f; p.lpeak = 0.3f; p.fenv = 0.72f; p.e1a = 0.0f; p.e1d = 0.32f; p.e1s = 0.0f;
                       p.e2a = 0.0f; p.e2d = 0.38f; p.e2s = 0.0f; p.e2r = 0.3f; p.mode = 2; p.spread = 0.6f;
                       p.dltime = 0.62f; p.dlfb = 0.55f; p.dltone = 0.4f; p.dlwow = 0.5f; p.dlmix = 0.4f; } },
    { "SPRING DRONE", "the VCA held open, the LFO breathing the cutoff, a long spring",
      [] (Params& p) { p = Params(); p.vcamode = 2; p.o1wave = 1; p.o2wave = 0; p.o2oct = 1; p.o2fine = 0.47f; p.o2lvl = 0.5f;
                       p.sublvl = 0.35f; p.suboct = 1; p.fmodel = 0; p.lpf = 0.42f; p.lpeak = 0.45f; p.fenv = 0.5f; p.flfo = 0.35f;
                       p.lwave = 0; p.lrate = 0.18f; p.spdwell = 0.85f; p.sptone = 0.45f; p.spmix = 0.5f; p.chmix = 0.3f; } },
    { "THREE RIDERS", "three voices spread across the stereo field, triangle and saw, chorused",
      [] (Params& p) { p = Params(); p.mode = 2; p.spread = 1.0f; p.o1wave = 1; p.o2wave = 0; p.o2fine = 0.55f; p.o2lvl = 0.5f;
                       p.fmodel = 2; p.lpf = 0.5f; p.lpeak = 0.15f; p.fenv = 0.6f; p.e1a = 0.25f; p.e1d = 0.5f; p.e1s = 0.5f;
                       p.e2a = 0.2f; p.e2s = 0.85f; p.e2r = 0.5f; p.chrate = 0.35f; p.chdepth = 0.6f; p.chmix = 0.5f; p.spmix = 0.15f; } },
    { "HOOVER RIDE", "wide unison, PWM, chorus: the rave lead on a black panel",
      [] (Params& p) { p = Params(); p.mode = 1; p.udet = 0.6f; p.spread = 0.9f; p.o1wave = 2; p.o1pwm = 0.5f; p.o2wave = 0;
                       p.o2oct = 1; p.o2fine = 0.45f; p.o2lvl = 0.7f; p.sublvl = 0.3f; p.fmodel = 0; p.lpf = 0.62f; p.lpeak = 0.25f;
                       p.fenv = 0.58f; p.lrate = 0.42f; p.lwave = 0; p.e2a = 0.1f; p.e2s = 1.0f; p.e2r = 0.4f; p.glide = 0.3f;
                       p.chmix = 0.55f; p.chdepth = 0.7f; p.drv = 0.2f; } },
    { "FEEDBACK HOWL", "the VCA output patched back into the filter input: the MS-20 trick",
      [] (Params& p) { p = Params(); p.o1wave = 0; p.o2lvl = 0.3f; p.o2oct = 1; p.fmodel = 0; p.lpf = 0.5f; p.lpeak = 0.78f;
                       p.hpf = 0.3f; p.hpeak = 0.5f; p.fenv = 0.55f; p.fdrive = 0.55f; cable (p, 0, S_VCA, D_FIN, 0.85f);
                       p.e2s = 1.0f; p.e2r = 0.45f; p.drv = 0.3f; p.spmix = 0.2f; } },
    { "IRON BELL", "VCO1 modulating VCO2 exponentially, a long release, a spring",
      [] (Params& p) { p = Params(); p.o1wave = 3; p.o1lvl = 0.0f; p.o2wave = 3; p.o2lvl = 0.9f; p.o1oct = 2; p.o1semi = 0.5f + 7.0f / 24.0f;
                       p.o2fm = 0.55f; p.fmodel = 2; p.lpf = 0.8f; p.lpeak = 0.05f; p.fenv = 0.5f; p.mode = 2; p.spread = 0.8f;
                       p.e2a = 0.0f; p.e2d = 0.7f; p.e2s = 0.0f; p.e2r = 0.7f; p.e1d = 0.6f; p.e1s = 0.0f;
                       cable (p, 0, S_EG1, D_FM2, 0.4f); p.spdwell = 0.7f; p.spmix = 0.35f; } },
    { "ACID RIDER", "one saw, the ladder near its edge, the envelope biting, drive",
      [] (Params& p) { p = Params(); p.o1wave = 0; p.o1oct = 1; p.o2lvl = 0.0f; p.fmodel = 2; p.lpf = 0.34f; p.lpeak = 0.82f;
                       p.fenv = 0.8f; p.fkey = 0.2f; p.fdrive = 0.45f; p.e1a = 0.0f; p.e1d = 0.35f; p.e1s = 0.0f; p.e1r = 0.25f;
                       p.e2a = 0.0f; p.e2d = 0.4f; p.e2s = 0.6f; p.e2r = 0.2f; p.glide = 0.28f; p.legato = 1; p.drv = 0.35f;
                       p.drvtone = 0.5f; p.dltime = 0.58f; p.dlfb = 0.3f; p.dlmix = 0.18f; } },
    { "GHOST ORGAN", "unison triangles with a two-octave sub, slow attack, spring and chorus",
      [] (Params& p) { p = Params(); p.mode = 1; p.udet = 0.15f; p.spread = 0.7f; p.o1wave = 1; p.o2wave = 1; p.o2oct = 3;
                       p.o2lvl = 0.35f; p.sublvl = 0.5f; p.suboct = 1; p.fmodel = 2; p.lpf = 0.55f; p.lpeak = 0.0f; p.fenv = 0.5f;
                       p.e2a = 0.45f; p.e2s = 1.0f; p.e2r = 0.6f; p.e1a = 0.5f; p.e1s = 1.0f; p.lrate = 0.4f; p.lpitch = 0.08f;
                       p.chmix = 0.4f; p.spdwell = 0.75f; p.spmix = 0.4f; p.vintage = 0.6f; } },
};

const char* recipeName (int i)  { return RECIPES[i < 0 ? 0 : (i >= NUM_RECIPES ? 0 : i)].name; }
const char* recipeBlurb (int i) { return RECIPES[i < 0 ? 0 : (i >= NUM_RECIPES ? 0 : i)].blurb; }
void applyRecipe (int i, Params& p) { RECIPES[i < 0 ? 0 : (i >= NUM_RECIPES ? 0 : i)].fn (p); }

//==============================================================================
void Voice::reset()
{
    o1.reset (0.0f); o2.reset (0.0f);
    k35lp.reset(); k35hp.reset(); lad.reset();
    eg1.reset(); eg2.reset();
    note = -1; gate = false; vel = 0.8f; seat = -1; onAt = 0;
    subPh = 0.0f; subState = -1.0f; subPending = 0.0f; subBq = {};
    pinkB0 = pinkB1 = pinkB2 = 0.0f;
    lastFilt = lastVca = lastNoise = 0.0f; vcaSmooth = 0.0f;
    outDc.reset();
}

void Spring::prepare (double sr, float ms, int stagesAt48k, float coef)
{
    delaySamples = std::max (4, (int) (ms * 0.001 * sr));
    line.prepare (delaySamples + 16);
    nAp = std::min (MAX_AP, std::max (12, (int) std::round (stagesAt48k * sr / 48000.0)));
    apA = coef;
    lp.setHz (3500.0f, sr);
    hp.setHz (70.0f, sr);
    reset();
}
void Spring::reset() { line.reset(); apz.fill (0.0f); lp.reset(); hp.reset(); fbLast = 0.0f; }

//==============================================================================
void Engine::prepare (double sampleRate, int maxBlockIn)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    maxBlock = std::max (16, maxBlockIn);
    osL.assign ((size_t) (maxBlock * MAX_OS), 0.0f);
    osR.assign ((size_t) (maxBlock * MAX_OS), 0.0f);
    lfoBuf.assign ((size_t) (maxBlock * MAX_OS), 0.0f);
    shBuf.assign ((size_t) (maxBlock * MAX_OS), 0.0f);
    driveModBuf.assign ((size_t) (maxBlock * MAX_OS), 0.0f);

    for (int i = 0; i < MAX_VOICES; ++i)
    {
        auto& v = voices[(size_t) i];
        v.rng.seed (0x1234567u + 7919u * (uint32_t) i);
        // component tolerances: fixed per voice, so each voice is its own instrument
        Rng t; t.seed (0xBEEFu + 101u * (uint32_t) i);
        v.tol.tune1 = t.bi() * 2.5f; v.tol.tune2 = t.bi() * 2.5f;
        v.tol.cut   = t.bi() * 0.06f;
        v.tol.envA  = 1.0f + t.bi() * 0.05f; v.tol.envD = 1.0f + t.bi() * 0.05f; v.tol.envR = 1.0f + t.bi() * 0.05f;
        v.tol.drift1 = t.uni() * 6.28f; v.tol.drift2 = t.uni() * 6.28f;
    }
    lfoRng.seed (0x51f0u); chRng.seed (0xc40u); dlRng.seed (0xd1a7u); hissRng.seed (0x4155u);

    chL.prepare ((int) (0.030 * sr)); chR.prepare ((int) (0.030 * sr));
    chBwL.lowpass (9000.0f, 0.6f, sr); chBwR.lowpass (9000.0f, 0.6f, sr);

    dlL.prepare ((int) (2.6 * sr)); dlR.prepare ((int) (2.6 * sr));
    dlHpL.setHz (80.0f, sr); dlHpR.setHz (80.0f, sr);
    dlBumpL.lowShelf (120.0f, 1.5f, sr); dlBumpR.lowShelf (120.0f, 1.5f, sr);
    // start ON the working time — synced or free — so the first echo does not
    // arrive early while the slew is still catching up from the knob's value
    {
        const int dsync = (int) std::round (p.dlsync);
        const float sec = dsync > 0
            ? (float) (60.0 / p.bpm) * syncQuarters (dsync - 1) * feelMul ((int) std::round (p.dlfeel))
            : xmap (p.dltime, 20.0f, 1500.0f) * 0.001f;
        dlTimeSm = clampf (sec * (float) sr, 8.0f, 2.4f * (float) sr);
    }

    springs[0].prepare (sr, 41.0f, 60, 0.74f);
    springs[1].prepare (sr, 47.5f, 52, 0.70f);
    springs[2].prepare (sr, 56.0f, 68, 0.77f);
    dif1.prepare ((int) (0.006 * sr) + 8); dif2.prepare ((int) (0.009 * sr) + 8);

    lastOsParam = -1;
    configureRate();
    reset();
}

void Engine::configureRate()
{
    const int want = p.os < 0.5f ? 1 : (p.os < 1.5f ? 2 : 4);
    osf = want;
    fsOs = sr * osf;
    lastOsParam = want;
    dnL1.prepare(); dnR1.prepare(); dnL2.prepare(); dnR2.prepare();
    drvDcL.setHz (8.0f, fsOs); drvDcR.setHz (8.0f, fsOs);
    for (auto& v : voices)
    {
        const float toneHz = std::min (19000.0f, (float) fsOs * 0.42f);
        v.o1.tone.setHz (toneHz, fsOs); v.o2.tone.setHz (toneHz, fsOs);
        v.o1.dc.setHz (6.0f, fsOs); v.o2.dc.setHz (6.0f, fsOs);
        v.outDc.setHz (5.0f, fsOs);
        v.o1.reset (v.o1.ph); v.o2.reset (v.o2.ph);
        v.k35lp.reset(); v.k35hp.reset(); v.lad.reset();
    }
}

void Engine::reset()
{
    for (auto& v : voices) v.reset();
    samplesDone = 0;
    heldN = 0; heldKeys.fill (false); susKeys.fill (false); sustain = false; heldCount = 0;
    lfoPh = 0.0f; lfoVal = 0.0f; shVal = 0.0f; driftVal = driftTarget = 0.0f;
    chL.reset(); chR.reset(); chBwL.reset(); chBwR.reset(); chPh = 0.0f;
    dlL.reset(); dlR.reset(); dlFbL = dlFbR = 0.0f; dlLpL.reset(); dlLpR.reset(); dlHpL.reset(); dlHpR.reset();
    dlBumpL.reset(); dlBumpR.reset();
    for (auto& s : springs) s.reset();
    dif1.reset(); dif2.reset(); spInSm = 0.0f;
    dnL1.reset(); dnR1.reset(); dnL2.reset(); dnR2.reset();
    drvDcL.reset(); drvDcR.reset(); drvToneL.reset(); drvToneR.reset();
    outRms = outPeak = 0.0f; rmsAcc = peakAcc = 0.0f; rmsN = 0;
    scope.fill (0.0f); scopeWrite = 0;
    uiNotes.fill (-1);
    for (auto& g : globalDst) g = 0.0f;
}

//==============================================================================
// notes

void Engine::assignPans()
{
    const int mode = (int) std::round (p.mode);
    const float s = clamp01 (p.spread);
    if (mode == 1)   // unison: fixed fan
    {
        voices[0].panTarget = 0.0f; voices[1].panTarget = -s; voices[2].panTarget = s;
        for (int i = UNI_VOICES; i < MAX_VOICES; ++i) voices[(size_t) i].panTarget = 0.0f;
        return;
    }
    if (mode == 0) { for (auto& v : voices) v.panTarget = 0.0f; return; }

    /*  Poly: five stereo stations, LL L M R RR. A chord struck together —
        every gated note younger than the strum window — is ranked by pitch
        and seated from the table, so a block chord always comes out in
        order across the field. Once a note has been sounding longer than
        that, its seat is FROZEN: a new note never moves what is already
        playing (a jump mid-note is what the ear objects to), it just takes
        the free station nearest where its pitch rank says it belongs. */
    static const int RANKED[6][5] = { { 0 }, { 2 },                // 1 note: middle
                                      { 0, 4 },                    // 2: LL RR
                                      { 2, 0, 4 },                 // 3: bottom middle, then LL RR
                                      { 0, 1, 3, 4 },              // 4: LL L R RR
                                      { 0, 1, 2, 3, 4 } };         // 5: LL L M R RR
    static const int CENTRAL[5] = { 2, 1, 3, 0, 4 };               // tie-break: prefer the middle
    const float st[5] = { -s, -0.5f * s, 0.0f, 0.5f * s, s };

    int idx[MAX_VOICES]; int n = 0;
    for (int i = 0; i < MAX_VOICES; ++i) if (voices[(size_t) i].gate) idx[n++] = i;
    for (int i = 1; i < n; ++i)          // tiny insertion sort
        for (int j = i; j > 0 && voices[(size_t) idx[j]].note < voices[(size_t) idx[j - 1]].note; --j)
            std::swap (idx[j], idx[j - 1]);
    if (n == 0) return;

    const uint64_t strum = (uint64_t) (0.080 * sr);
    auto young = [this, strum] (const Voice& v) { return samplesDone - v.onAt <= strum; };

    bool allYoung = true;
    for (int k = 0; k < n; ++k) if (! young (voices[(size_t) idx[k]])) { allYoung = false; break; }
    if (allYoung)
    {
        for (int k = 0; k < n; ++k)
        {
            auto& v = voices[(size_t) idx[k]];
            v.seat = RANKED[n][k]; v.panTarget = st[v.seat];
        }
        return;
    }

    bool taken[5] = {};
    for (int k = 0; k < n; ++k)
    {
        const auto& v = voices[(size_t) idx[k]];
        if (! young (v) && v.seat >= 0) taken[v.seat] = true;
    }
    for (int k = 0; k < n; ++k)
    {
        auto& v = voices[(size_t) idx[k]];
        if (! young (v) && v.seat >= 0) continue;              // seated and sounding: stays put
        const float ideal = n > 1 ? -s + 2.0f * s * (float) k / (float) (n - 1) : 0.0f;
        int best = -1;
        for (int c = 0; c < 5; ++c)
        {
            const int j = CENTRAL[c];
            if (taken[j]) continue;
            if (best < 0 || std::abs (st[j] - ideal) < std::abs (st[best] - ideal)) best = j;
        }
        if (best < 0) best = 2;                                // cannot happen: 5 seats, 5 voices
        taken[best] = true;
        v.seat = best; v.panTarget = st[best];
    }
}

Voice* Engine::allocVoice (int note)
{
    // the same note again: retrigger that voice
    for (auto& v : voices) if (v.gate && v.note == note) return &v;
    // a silent one
    for (auto& v : voices) if (! v.sounding()) return &v;
    // one in release, the quietest
    Voice* best = nullptr; float bestLvl = 1.0e9f;
    for (auto& v : voices) if (! v.gate && v.eg2.v < bestLvl) { bestLvl = v.eg2.v; best = &v; }
    if (best) return best;
    // the oldest
    best = &voices[0];
    for (auto& v : voices) if (v.order < best->order) best = &v;
    return best;
}

void Engine::noteOn (int note, float vel)
{
    if (note < 0 || note > 127) return;
    vel = clamp01 (vel);
    const int mode = (int) std::round (p.mode);
    const bool legato = p.legato > 0.5f;

    heldKeys[(size_t) note] = true;
    susKeys[(size_t) note] = false;
    if (p.lkey > 0.5f) lfoPh = 0.0f;

    if (mode == 2)
    {
        Voice* v = allocVoice (note);
        const bool wasOn = v->gate;
        const bool wasSounding = v->sounding();              // before the gate goes up, or it always says yes
        v->note = note; v->vel = vel; v->gate = true; v->order = ++noteSerial;
        v->onAt = samplesDone;
        if (! wasSounding || ! wasOn || ! legato) { v->eg1.gate (true); v->eg2.gate (true); }
        if (! wasSounding) v->pitch = (float) note;          // a fresh voice does not glide from nowhere
        v->lastNote = note;
        leadVoice = (int) (v - &voices[0]);
        assignPans();
        if (! wasSounding) v->pan = v->panTarget;            // silent till now: start ON its station
        return;
    }

    // mono / unison: a note stack, last note priority
    for (int i = 0; i < heldN; ++i) if (heldStack[(size_t) i] == note) { for (int j = i; j < heldN - 1; ++j) heldStack[(size_t) j] = heldStack[(size_t) (j + 1)]; --heldN; break; }
    if (heldN >= 16) { for (int j = 0; j < heldN - 1; ++j) heldStack[(size_t) j] = heldStack[(size_t) (j + 1)]; --heldN; }
    heldStack[(size_t) heldN++] = note;

    const int nv = mode == 1 ? UNI_VOICES : 1;
    for (int i = 0; i < nv; ++i)
    {
        auto& v = voices[(size_t) i];
        const bool wasOn = v.gate;
        const bool wasSounding = v.sounding();
        v.note = note; v.vel = vel; v.gate = true; v.order = ++noteSerial;
        v.onAt = samplesDone;
        if (! wasOn || ! legato || ! wasSounding) { v.eg1.gate (true); v.eg2.gate (true); }
        if (! wasSounding && heldN <= 1) v.pitch = (float) note;
        v.lastNote = note;
    }
    leadVoice = 0;
    assignPans();
}

void Engine::noteOff (int note)
{
    if (note < 0 || note > 127) return;
    heldKeys[(size_t) note] = false;
    if (sustain) { susKeys[(size_t) note] = true; return; }

    const int mode = (int) std::round (p.mode);
    if (mode == 2)
    {
        for (auto& v : voices) if (v.gate && v.note == note)
        {
            v.gate = false; v.eg1.gate (false); v.eg2.gate (false);
        }
        assignPans();
        return;
    }

    for (int i = 0; i < heldN; ++i) if (heldStack[(size_t) i] == note)
    { for (int j = i; j < heldN - 1; ++j) heldStack[(size_t) j] = heldStack[(size_t) (j + 1)]; --heldN; break; }

    const int nv = mode == 1 ? UNI_VOICES : 1;
    for (int i = 0; i < nv; ++i)
    {
        auto& v = voices[(size_t) i];
        if (! v.gate || v.note != note) continue;
        if (heldN > 0)
        {
            // fall back to the most recent note still held
            v.note = heldStack[(size_t) (heldN - 1)];
            if (p.legato < 0.5f) { v.eg1.gate (true); v.eg2.gate (true); }
        }
        else
        {
            v.gate = false; v.eg1.gate (false); v.eg2.gate (false);
        }
    }
    assignPans();
}

void Engine::allNotesOff()
{
    sustain = false;   // a panic lets go of the pedal, or it holds the next note
    heldN = 0; heldKeys.fill (false); susKeys.fill (false);
    for (auto& v : voices) { v.gate = false; v.eg1.gate (false); v.eg2.gate (false); }
    assignPans();
}

void Engine::setSustain (bool on)
{
    if (sustain == on) return;
    sustain = on;
    if (! on)
        for (int n = 0; n < 128; ++n) if (susKeys[(size_t) n]) { susKeys[(size_t) n] = false; noteOff (n); }
}

//==============================================================================
/*  Everything a voice needs from the panel for one block, worked out once. */
namespace
{
    struct Block
    {
        int wave1, wave2, model, vcaMode, lfoWave;
        float off1, off2;                // semitones incl. range + semi + fine + tolerance-free part
        float pw1, pw2, pwm1, pwm2, lvl1, lvl2, sub, subDiv, nz, pink, ring, driveG;
        bool sync, kbd2;
        float fmOct;
        float hpfHz, hpK, lpfHz, lpK, fenvOct, fkey, flfoOct;
        float e1vel, e2vel;
        float vintage, tReset, kappa, glideK, tuneSemi, bendSemi, vibSemi;
        float driftLpA, driftLpB, jitA;
        float cutMaxHz;
    };
}

void Engine::renderVoice (Voice& v, int vi, float* outL, float* outR, int nOs)
{
    (void) vi;
    // ---- block constants (recomputed per voice call; cheap) -------------
    Block b;
    b.wave1 = (int) std::round (p.o1wave); b.wave2 = (int) std::round (p.o2wave);
    b.model = (int) std::round (p.fmodel); b.vcaMode = (int) std::round (p.vcamode);
    b.off1 = ((int) std::round (p.o1oct) - 2) * 12.0f + semiOf (p.o1semi) + centOf (p.o1fine, 100.0f) / 100.0f;
    b.off2 = ((int) std::round (p.o2oct) - 2) * 12.0f + semiOf (p.o2semi) + centOf (p.o2fine, 100.0f) / 100.0f;
    b.pw1 = 0.5f + clamp01 (p.o1pw) * 0.45f; b.pw2 = 0.5f + clamp01 (p.o2pw) * 0.45f;
    b.pwm1 = p.o1pwm * 0.45f; b.pwm2 = p.o2pwm * 0.45f;
    b.lvl1 = p.o1lvl; b.lvl2 = p.o2lvl; b.sub = p.sublvl; b.subDiv = p.suboct < 0.5f ? 2.0f : 4.0f;
    b.nz = p.nzlvl; b.pink = p.nzcol; b.ring = p.ringlvl;
    b.driveG = std::pow (10.0f, clamp01 (p.fdrive) * 1.3f);
    b.sync = p.o2sync > 0.5f; b.kbd2 = p.o2kbd > 0.5f;
    b.fmOct = p.o2fm * p.o2fm * 6.0f;
    b.hpfHz = xmap (p.hpf, 20.0f, 8000.0f);
    b.hpK = b.model == 1 ? k35Kscream (p.hpeak) : k35K (p.hpeak);
    b.lpfHz = xmap (p.lpf, 20.0f, 20000.0f);
    b.lpK = b.model == 0 ? k35K (p.lpeak) : (b.model == 1 ? k35Kscream (p.lpeak) : 4.0f * ladderR (p.lpeak));
    b.fenvOct = (p.fenv - 0.5f) * 12.0f; b.fkey = p.fkey; b.flfoOct = p.flfo * 4.0f;
    b.e1vel = p.e1vel; b.e2vel = p.e2vel;
    b.vintage = clamp01 (p.vintage);
    b.tReset = 2.0e-6f * b.vintage;
    b.kappa = 0.0015f * b.vintage;
    b.glideK = p.glide < 0.01f ? 1.0f : 1.0f - std::exp (-3.0f / (float) (glideMs (p.glide) * 0.001 * fsOs));
    b.tuneSemi = centOf (p.tune, 200.0f) / 100.0f;
    b.cutMaxHz = std::min (20000.0f, (float) fsOs * 0.45f);
    b.driftLpA = 1.0f - std::exp (-6.2831853f * 0.25f / (float) fsOs);
    b.driftLpB = 1.0f - std::exp (-6.2831853f * 3.0f / (float) fsOs);
    b.jitA     = 1.0f - std::exp (-6.2831853f * 40.0f / (float) fsOs);

    v.eg1.set (egAttackMs (p.e1a) * v.tol.envA, egDecayMs (p.e1d) * v.tol.envD, p.e1s, egDecayMs (p.e1r) * v.tol.envR, fsOs);
    v.eg2.set (egAttackMs (p.e2a) * v.tol.envA, egDecayMs (p.e2d) * v.tol.envD, p.e2s, egDecayMs (p.e2r) * v.tol.envR, fsOs);
    v.k35lp.sat = 1.0f; v.k35hp.sat = 1.0f;

    const float panK = 1.0f - std::exp (-1.0f / (0.02f * (float) fsOs));
    const float vcaK = 1.0f - std::exp (-1.0f / (0.0008f * (float) fsOs));
    const float invFs = 1.0f / (float) fsOs;
    const float velAmp = 1.0f - b.e2vel + b.e2vel * v.vel;
    const float velEnv = 1.0f - b.e1vel + b.e1vel * v.vel;
    const float unisonCents = (int) std::round (p.mode) == 1 ? (vi == 0 ? 0.0f : (vi == 1 ? -1.0f : 1.0f)) * p.udet * 50.0f : 0.0f;

    // the bay, as flat arrays
    int   cs[NUM_CABLES], cd[NUM_CABLES]; float ca[NUM_CABLES]; int nc = 0;
    for (int i = 0; i < NUM_CABLES; ++i)
    {
        const int s = (int) std::round (p.cable[i].src), d = (int) std::round (p.cable[i].dst);
        if (s > 0 && s < NUM_SOURCES && d > 0 && d < NUM_DESTS)
        { cs[nc] = s; cd[nc] = d; ca[nc] = (p.cable[i].amt - 0.5f) * 2.0f; ++nc; }
    }
    float src[NUM_SOURCES], dst[NUM_DESTS];
    const float gateV = v.gate ? 1.0f : 0.0f;
    const bool isLead = (vi == leadVoice);

    float lockAcc = 0.0f;

    // world-mod bus, fanned per voice (golden-angle offsets, like the pans)
    const float wmFan  = std::fmod ((float) vi * 0.6180339887f + 0.5f, 1.0f) * 2.0f - 1.0f;
    const float wmFan2 = std::fmod ((float) vi * 0.6180339887f + 0.21f, 1.0f) * 2.0f - 1.0f;
    const float wmU    = std::fmod ((float) vi * 0.6180339887f + 0.71f, 1.0f);
    const float wmRate = wmTremR * (0.75f + 0.5f * wmU);
    const float wmGateK = 1.0f - std::exp (-1.0f / (0.05f * (float) fsOs));
    // a VCA-mode drone is held by definition: it must not sag while it sounds
    const float wmSagGate = b.vcaMode == 2 ? 1.0f : gateV;
    double wmPh = 6.2831853 * (double) wmRate * wmT + (double) vi * 2.39996;
    const double wmPhInc = 6.2831853 * (double) wmRate * invFs;

    for (int i = 0; i < nOs; ++i)
    {
        // ---- envelopes, glide --------------------------------------------
        const float e1 = v.eg1.tick(), e2 = v.eg2.tick();
        v.pitch += ((float) v.note - v.pitch) * b.glideK;
        if (v.note < 0) v.pitch = (float) v.lastNote;

        // ---- sources (last sample's signals) ----------------------------
        src[S_NONE] = 0.0f;
        src[S_VCO1] = v.o1.lastOut; src[S_VCO2] = v.o2.lastOut; src[S_NOISE] = v.lastNoise;
        src[S_FILT] = v.lastFilt; src[S_VCA] = v.lastVca;
        src[S_EG1] = e1; src[S_EG2] = e2; src[S_LFO] = lfoBuf[(size_t) i]; src[S_SH] = shBuf[(size_t) i];
        src[S_KEY] = (v.pitch - 60.0f) / 12.0f; src[S_VEL] = v.vel; src[S_WHEEL] = wheelSm; src[S_AT] = atSm;
        src[S_BEND] = bendSm; src[S_GATE] = gateV;
        for (int d = 0; d < NUM_DESTS; ++d) dst[d] = 0.0f;
        for (int c = 0; c < nc; ++c) dst[cd[c]] += ca[c] * src[cs[c]];

        // ---- drift and jitter --------------------------------------------
        v.driftS1a += (v.rng.bi() - v.driftS1a) * b.driftLpA; v.driftS1b += (v.rng.bi() - v.driftS1b) * b.driftLpB;
        v.driftS2a += (v.rng.bi() - v.driftS2a) * b.driftLpA; v.driftS2b += (v.rng.bi() - v.driftS2b) * b.driftLpB;
        v.jit1 += (v.rng.bi() - v.jit1) * b.jitA; v.jit2 += (v.rng.bi() - v.jit2) * b.jitA;
        // the one-pole on white noise has a tiny RMS; scale it back up to cents
        const float drift1 = b.vintage * (v.driftS1a * 1500.0f + v.driftS1b * 70.0f + v.jit1 * 1.8f) + v.tol.tune1 * b.vintage;
        const float drift2 = b.vintage * (v.driftS2a * 1500.0f + v.driftS2b * 70.0f + v.jit2 * 1.8f) + v.tol.tune2 * b.vintage;

        // ---- pitch -------------------------------------------------------
        const float lfo = lfoBuf[(size_t) i];
        const float vib = lfo * (p.lpitch + p.lwheel * wheelSm) * 2.0f;        // semitones
        float wmSemi = 0.0f;
        if (wmActive)
        {
            // sag keys to the smoothed GATE: in tune while held, sags as the
            // note dies (the Photo-Synth lesson - never the amp envelope)
            v.wmGate += wmGateK * (wmSagGate - v.wmGate);
            wmSemi = (wmDet * wmFan - wmSag * 100.0f * (1.0f - v.wmGate)) * 0.01f;
        }
        const float common = v.pitch + b.tuneSemi + bendSm * p.bend + vib + unisonCents * 0.01f + wmSemi;
        const float n1 = common + b.off1 + drift1 * 0.01f + 12.0f * (dst[D_P1] + dst[D_PBOTH]) * 2.0f;
        float f1 = 440.0f * fexp2 ((n1 - 69.0f) * (1.0f / 12.0f));
        const float base2 = (b.kbd2 ? common : 60.0f + b.tuneSemi + bendSm * p.bend) + b.off2 + drift2 * 0.01f
                          + 12.0f * (dst[D_P2] + dst[D_PBOTH]) * 2.0f;
        float f2 = 440.0f * fexp2 ((base2 - 69.0f) * (1.0f / 12.0f) + b.fmOct * v.o1.lastOut);
        f2 += dst[D_FM2] * f2 * 2.0f;                                        // linear FM from the bay
        if (f2 < 0.0f) f2 = 0.0f;
        // the exponential converter's reset time: flat at the top
        f1 = f1 / (1.0f + f1 * b.tReset);
        f2 = f2 / (1.0f + f2 * b.tReset);
        // injection locking: VCO2 is pulled towards VCO1 when they sit close
        float inc1 = f1 * invFs, inc2 = f2 * invFs;
        if (b.kappa > 0.0f)
        {
            const float dphi = v.o1.ph - v.o2.ph;
            inc2 += b.kappa * inc2 * std::sin (6.2831853f * dphi);
            if (isLead) lockAcc += (std::abs (f2 - f1) < b.kappa * f1 * 0.9f) ? 1.0f : 0.0f;
        }

        // ---- oscillators -------------------------------------------------
        const float pw1 = clampf (b.pw1 + b.pwm1 * lfo + 0.45f * dst[D_PW1], 0.03f, 0.97f);
        const float pw2 = clampf (b.pw2 + b.pwm2 * lfo + 0.45f * dst[D_PW2], 0.03f, 0.97f);
        float o1 = v.o1.tick (b.wave1, inc1, pw1, -1.0f);
        float o2 = v.o2.tick (b.wave2, inc2, pw2, (b.sync && v.o1.wrapped) ? v.o1.wrapF : -1.0f);
        o1 = v.o1.dc (v.o1.tone.lp (o1));
        o2 = v.o2.dc (v.o2.tone.lp (o2));
        v.o1.lastOut = o1; v.o2.lastOut = o2;

        // sub: a divider off VCO1, so it carries VCO1's every wobble
        if (v.o1.wrapped)
        {
            v.subPh += 1.0f;
            if (v.subPh >= b.subDiv) v.subPh = 0.0f;
            const float newState = v.subPh < b.subDiv * 0.5f ? 1.0f : -1.0f;
            if (newState != v.subState) { v.subBq.step (newState - v.subState, v.o1.wrapF); v.subState = newState; }
        }
        const float subOut = v.subPending + v.subBq.prev;
        v.subPending = v.subState + v.subBq.cur; v.subBq.prev = v.subBq.cur = 0.0f;

        // noise
        const float white = v.rng.bi();
        v.pinkB0 = 0.99765f * v.pinkB0 + white * 0.0990460f;
        v.pinkB1 = 0.96300f * v.pinkB1 + white * 0.2965164f;
        v.pinkB2 = 0.57000f * v.pinkB2 + white * 1.0526913f;
        const float pink = (v.pinkB0 + v.pinkB1 + v.pinkB2 + white * 0.1848f) * 0.28f;
        const float noise = b.pink > 0.5f ? pink : white;
        v.lastNoise = white;

        // ---- mixer, saturating ------------------------------------------
        float m = b.lvl1 * o1 + b.lvl2 * o2 + b.sub * subOut + b.nz * noise + b.ring * o1 * o2 + dst[D_FIN];
        m = ftanh (m * b.driveG * 0.8f) * 1.25f;
        // the circuit's own noise floor: what a filter past its edge starts oscillating from
        m += v.rng.bi() * 2.0e-5f;

        // ---- filters -----------------------------------------------------
        if ((v.ctrlPhase++ & 1) == 0)
        {
            const float hpHz = clampf (b.hpfHz * fexp2 (dst[D_HPF] * 5.0f), 10.0f, b.cutMaxHz);
            v.gHp = std::tan (3.14159265f * hpHz * invFs);
            const float cutOct = e1 * velEnv * b.fenvOct                              // octaves
                               + lfo * b.flfoOct + (v.pitch - 60.0f) / 12.0f * b.fkey
                               + dst[D_LPF] * 5.0f + v.tol.cut * b.vintage + v.driftS1a * 0.6f * b.vintage;
            float lpHz = clampf (b.lpfHz * fexp2 (cutOct), 10.0f, b.cutMaxHz);
            if (wmActive) lpHz = clampf (lpHz * wmFmul, 10.0f, b.cutMaxHz);   // world-mod bus
            if (isLead) uiCut = lpHz;
            /*  Both Sallen-Key models take the prewarped coefficient, lifted to
                correct the measured pitch of the self-oscillation — the clipper
                takes a share of the loop, and its share GROWS with K (it is
                driven harder): measured -6 cents at K 2.25, -17 at 2.45, -52 at
                2.9. Piecewise from those anchors, so GROWL and SCREAM both sing
                in tune at any PEAK. Below the oscillation edge the lift shrinks
                to nothing and the passband stays put. */
            float pw = 1.0f;
            if (b.model <= 1)
            {
                const float K = b.lpK;
                float cents = 0.0f;
                if      (K > 2.45f) cents = 17.0f + 78.0f * (K - 2.45f);
                else if (K > 2.25f) cents = 6.0f + 55.0f * (K - 2.25f);
                else if (K > 2.0f)  cents = 24.0f * (K - 2.0f);
                pw = std::pow (2.0f, cents / 1200.0f);
            }
            v.gK35 = std::tan (3.14159265f * std::min (lpHz * pw, b.cutMaxHz) * invFs);
        }
        const bool sallen = b.model <= 1;         // 0 GROWL, 1 SCREAM, 2 LADDER
        // SCREAM is allowed deeper into the clipper than GROWL
        const float kCap = b.model == 1 ? 2.95f : 2.45f;
        v.k35hp.setG (v.gHp);
        const float hpK = clampf (b.hpK + dst[D_HPEAK] * 2.0f, 0.0f, kCap);
        float x = v.k35hp.highpass (m, hpK);
        float y;
        if (sallen)
        {
            v.k35lp.setG (v.gK35);
            y = v.k35lp.lowpass (x, clampf (b.lpK + dst[D_LPEAK] * 2.0f, 0.0f, kCap));
        }
        else
        {
            v.lad.setG (v.gK35);   // ladder takes pw = 1 above; measured 0 cents raw
            y = v.lad.tick (x, clampf (b.lpK + dst[D_LPEAK] * 4.0f, 0.0f, 4.6f));
        }
        y = clean (y);
        v.lastFilt = y;

        // ---- vca ---------------------------------------------------------
        float gain = b.vcaMode == 0 ? e2 * velAmp : (b.vcaMode == 1 ? gateV * velAmp : 1.0f);
        gain = std::max (0.0f, gain + dst[D_VCACV]);
        v.vcaSmooth += (gain - v.vcaSmooth) * vcaK;
        float out = (y + dst[D_VCAIN]) * v.vcaSmooth;
        out = ceilSoft (out, 1.6f);
        out = v.outDc (out);
        v.lastVca = out;

        // ---- world-mod tremolo + pan spread ------------------------------
        float panWmAdd = 0.0f;
        if (wmActive)
        {
            if (wmTremD > 0.0f && wmTremR > 0.0f)
            {
                out *= 1.0f - wmTremD * (0.5f - 0.5f * (float) std::sin (wmPh));
                wmPh += wmPhInc;
            }
            panWmAdd = wmPan * wmFan2;
        }

        // ---- pan ---------------------------------------------------------
        v.pan += (v.panTarget - v.pan) * panK;
        const float pan = clampf (v.pan + dst[D_PAN] + panWmAdd, -1.0f, 1.0f);
        const float a = (pan + 1.0f) * 0.78539816f;
        outL[i] += out * std::cos (a);
        outR[i] += out * std::sin (a);

        if (isLead)
        {
            driveModBuf[(size_t) i] = dst[D_DRIVE];
            if (i == nOs - 1)
            {
                globalDst[D_LRATE] = dst[D_LRATE];
                globalDst[D_DLYT]  = dst[D_DLYT];
                uiEnv1 = e1; uiEnv2 = e2;
            }
        }
    }
    if (isLead) locked = b.kappa > 0.0f && lockAcc > 0.5f * (float) nOs;
}

//==============================================================================
void Engine::process (float* L, float* R, int n)
{
    if (n <= 0) return;
    const int wantOs = p.os < 0.5f ? 1 : (p.os < 1.5f ? 2 : 4);
    if (wantOs != lastOsParam) configureRate();

    int done = 0;
    while (done < n)
    {
        const int nb = std::min (maxBlock, n - done);
        const int nOs = nb * osf;
        if ((int) lfoBuf.size() < nOs) { lfoBuf.assign ((size_t) nOs, 0.0f); shBuf.assign ((size_t) nOs, 0.0f); driveModBuf.assign ((size_t) nOs, 0.0f); }
        std::fill (osL.begin(), osL.begin() + nOs, 0.0f);
        std::fill (osR.begin(), osR.begin() + nOs, 0.0f);
        std::fill (driveModBuf.begin(), driveModBuf.begin() + nOs, 0.0f);

        // ---- the LFO and the performance controls, per OS sample ---------
        {
            const int lsync = (int) std::round (p.lsync);
            /*  Synced, the LFO is locked to the host clock and the RATE knob
                (and the bay's LFO RATE cable) stand aside — a synced LFO that
                could be detuned would not be synced. */
            const float rate = lsync > 0
                ? (float) (p.bpm / 60.0) / (syncQuarters (lsync - 1) * feelMul ((int) std::round (p.lfeel)))
                : lfoHz (p.lrate) * fexp2 (clampf (globalDst[D_LRATE] * 4.0f, -8.0f, 8.0f));
            const float inc = clampf (rate / (float) fsOs, 0.0f, 0.45f);
            const int wave = (int) std::round (p.lwave);
            const float smK = 1.0f - std::exp (-1.0f / (0.004f * (float) fsOs));
            const float drK = 1.0f - std::exp (-1.0f / (0.08f * (float) fsOs));
            for (int i = 0; i < nOs; ++i)
            {
                lfoPh += inc;
                bool wrapped = false;
                if (lfoPh >= 1.0f) { lfoPh -= 1.0f; wrapped = true; }
                if (wrapped) { shVal = lfoRng.bi(); driftTarget = lfoRng.bi(); }
                driftVal += (driftTarget - driftVal) * drK;
                float out;
                switch (wave)
                {
                    case 0:  out = lfoPh < 0.5f ? 4.0f * lfoPh - 1.0f : 3.0f - 4.0f * lfoPh; break;
                    case 1:  out = 1.0f - 2.0f * lfoPh; break;
                    case 2:  out = lfoPh < 0.5f ? 1.0f : -1.0f; break;
                    case 3:  out = std::sin (6.2831853f * lfoPh); break;
                    case 4:  out = shVal; break;
                    default: out = driftVal; break;
                }
                lfoVal = out;
                lfoBuf[(size_t) i] = out; shBuf[(size_t) i] = shVal;
                bendSm += (bendIn - bendSm) * smK; wheelSm += (wheelIn - wheelSm) * smK; atSm += (atIn - atSm) * smK;
            }
            uiLfo = lfoVal;
        }

        // ---- Brokild World FX bus: copy once per block; neutral = inert ----
        wmDet   = wmIn[0].load (std::memory_order_relaxed);
        wmPan   = wmIn[1].load (std::memory_order_relaxed);
        wmTremD = wmIn[2].load (std::memory_order_relaxed);
        wmTremR = wmIn[3].load (std::memory_order_relaxed);
        wmSag   = wmIn[4].load (std::memory_order_relaxed);
        wmFmul  = wmIn[5].load (std::memory_order_relaxed);
        wmActive = wmDet != 0.0f || wmPan != 0.0f || wmTremD != 0.0f
                || wmSag != 0.0f || wmFmul != 1.0f;
        if (wmActive) wmT += (double) nOs / fsOs; else wmT = 0.0;

        // ---- voices --------------------------------------------------------
        const int mode = (int) std::round (p.mode);
        const int nv = mode == 0 ? 1 : (mode == 1 ? UNI_VOICES : MAX_VOICES);
        const bool drone = (int) std::round (p.vcamode) == 2;
        const int droneN = mode == 2 ? MAX_VOICES : UNI_VOICES;   // drone keeps its three voices outside poly
        heldCount = 0;
        for (int vi = 0; vi < MAX_VOICES; ++vi)
        {
            auto& v = voices[(size_t) vi];
            uiNotes[(size_t) vi] = v.gate ? v.note : -1;
            uiPan[(size_t) vi] = v.pan;
            if (v.gate) ++heldCount;
            if (vi >= nv) { if (v.sounding()) { v.gate = false; v.eg1.gate (false); v.eg2.gate (false); } }
            if (! v.sounding() && v.vcaSmooth < 1.0e-4f && ! (drone && vi < droneN)) continue;
            renderVoice (v, vi, osL.data(), osR.data(), nOs);
        }
        if ((int) std::round (p.vcamode) == 2 && nv == 1) { /* drone renders voice 0 above */ }

        // ---- drive, in the oversampled domain -----------------------------
        {
            const float baseDrv = clamp01 (p.drv);
            drvToneL.setHz (xmap (p.drvtone, 1000.0f, 16000.0f), fsOs);
            drvToneR.a = drvToneL.a;
            for (int i = 0; i < nOs; ++i)
            {
                const float a = clamp01 (baseDrv + driveModBuf[(size_t) i]);
                if (a < 0.002f) continue;
                const float g = 1.0f + a * a * 24.0f;
                const float comp = std::pow (g, -0.35f);
                /*  A biased, asymmetric stage. The bias moves the operating
                    point so the two half-cycles clip at different drive —
                    that is where even harmonics come from; asymmetric
                    LEVELS alone would only make DC. */
                const float bias = 0.18f + 0.3f * a;
                auto shape = [bias] (float u) { const float t = u + bias; return t >= 0.0f ? ftanh (t) : ftanh (1.4f * t) * 0.72f; };
                const float dcOff = shape (0.0f);
                float l = (shape (osL[(size_t) i] * g) - dcOff) * comp;
                float r = (shape (osR[(size_t) i] * g) - dcOff) * comp;
                l = drvToneL.lp (drvDcL (l)); r = drvToneR.lp (drvDcR (r));
                osL[(size_t) i] = l; osR[(size_t) i] = r;
            }
        }

        // ---- decimate ----------------------------------------------------
        float* outL = L + done; float* outR = R + done;
        if (osf == 1)      { for (int i = 0; i < nb; ++i) { outL[i] = osL[(size_t) i]; outR[i] = osR[(size_t) i]; } }
        else if (osf == 2) { for (int i = 0; i < nb; ++i) { outL[i] = dnL1 (osL[(size_t) (2 * i)], osL[(size_t) (2 * i + 1)]); outR[i] = dnR1 (osR[(size_t) (2 * i)], osR[(size_t) (2 * i + 1)]); } }
        else
        {
            for (int i = 0; i < nb; ++i)
            {
                const float l0 = dnL1 (osL[(size_t) (4 * i)], osL[(size_t) (4 * i + 1)]);
                const float l1 = dnL1 (osL[(size_t) (4 * i + 2)], osL[(size_t) (4 * i + 3)]);
                const float r0 = dnR1 (osR[(size_t) (4 * i)], osR[(size_t) (4 * i + 1)]);
                const float r1 = dnR1 (osR[(size_t) (4 * i + 2)], osR[(size_t) (4 * i + 3)]);
                outL[i] = dnL2 (l0, l1); outR[i] = dnR2 (r0, r1);
            }
        }

        // ---- chorus (BBD) -------------------------------------------------
        if (p.chmix > 0.001f)
        {
            const float mix = clamp01 (p.chmix);
            const float inc = xmap (p.chrate, 0.1f, 8.0f) / (float) sr;
            const float depth = clamp01 (p.chdepth);
            for (int i = 0; i < nb; ++i)
            {
                chPh += inc; if (chPh >= 1.0f) chPh -= 1.0f;
                const float tri = chPh < 0.5f ? 4.0f * chPh - 1.0f : 3.0f - 4.0f * chPh;
                const float lfo = 0.8f * tri + 0.2f * std::sin (6.2831853f * chPh);
                const float base = 0.0055f * (float) sr, sw = 0.0045f * (float) sr * depth;
                const float dL = base + sw * (1.0f + lfo), dR = base + sw * (1.0f - lfo);
                chL.push (outL[i]); chR.push (outR[i]);
                float wl = chBwL (chL.readHermite (dL)), wr = chBwR (chR.readHermite (dR));
                wl = ftanh (wl * 1.3f) * 0.77f;
                wr = ftanh (wr * 1.3f) * 0.77f;
                outL[i] = outL[i] * (1.0f - 0.35f * mix) + wl * mix * 0.85f;
                outR[i] = outR[i] * (1.0f - 0.35f * mix) + wr * mix * 0.85f;
            }
        }

        // ---- tape delay ---------------------------------------------------
        {
            const float mix = clamp01 (p.dlmix);
            const int dsync = (int) std::round (p.dlsync);
            /*  Synced, the time comes from the host clock; the slew below still
                applies, so a tempo change bends the pitch like a real tape. */
            const float wantSec = dsync > 0
                ? (float) (60.0 / p.bpm) * syncQuarters (dsync - 1) * feelMul ((int) std::round (p.dlfeel))
                : xmap (p.dltime, 20.0f, 1500.0f) * 0.001f * fexp2 (clampf (globalDst[D_DLYT], -1.0f, 1.0f));
            const float target = clampf (wantSec * (float) sr, 8.0f, 2.4f * (float) sr);
            const float smK = 1.0f - std::exp (-1.0f / (0.25f * (float) sr));
            const float fb = clamp01 (p.dlfb) * 1.08f;
            dlLpL.setHz (xmap (p.dltone, 1500.0f, 12000.0f), sr); dlLpR.a = dlLpL.a;
            const float wow = clamp01 (p.dlwow);
            const float wowInc = 0.6f / (float) sr, flInc = 6.3f / (float) sr;
            const float wowSlew = 1.0f - std::exp (-1.0f / (0.06f * (float) sr));
            const bool active = mix > 0.001f || std::abs (dlFbL) > 1.0e-5f || std::abs (dlFbR) > 1.0e-5f;
            if (active)
            {
                for (int i = 0; i < nb; ++i)
                {
                    dlTimeSm += (target - dlTimeSm) * smK;
                    wowPh += wowInc; if (wowPh >= 1.0f) wowPh -= 1.0f;
                    flutPh += flInc; if (flutPh >= 1.0f) flutPh -= 1.0f;
                    /*  The random part of the wow WANDERS — it must never step.
                        A new target every 50 ms, slewed; the old hard jump in
                        the read position was audible as a tiny glitch. */
                    if (++wowRndT > 0.05f * (float) sr) { wowRndT = 0.0f; wowRndTgt = dlRng.bi(); }
                    wowRnd += (wowRndTgt - wowRnd) * wowSlew;
                    const float wL = wow * (0.004f * std::sin (6.2831853f * wowPh) + 0.0012f * std::sin (6.2831853f * flutPh) + 0.0015f * wowRnd);
                    const float wR = wow * (0.004f * std::sin (6.2831853f * wowPh + 1.7f) + 0.0012f * std::sin (6.2831853f * flutPh + 0.9f) + 0.0015f * wowRnd);
                    const float inL = outL[i] + 0.15f * outR[i], inR = outR[i] + 0.15f * outL[i];
                    dlL.push (ceilSoft (inL + dlFbL, 2.0f)); dlR.push (ceilSoft (inR + dlFbR, 2.0f));
                    float tl = dlL.readHermite (dlTimeSm * (1.0f + wL)), tr = dlR.readHermite (dlTimeSm * (1.0f + wR));
                    /*  The tape loop: head bump, high cut, saturation. No added
                        hiss — an injected noise recirculates with the feedback
                        and builds up to something clearly audible, which is a
                        vice, not a character. The wow, the tone loss and the
                        saturation carry the tape on their own. */
                    tl = dlLpL.lp (dlBumpL (dlHpL.hp (tl))); tr = dlLpR.lp (dlBumpR (dlHpR.hp (tr)));
                    tl = ftanh (tl * 1.5f) * 0.6667f;
                    tr = ftanh (tr * 1.5f) * 0.6667f;
                    dlFbL = tl * fb; dlFbR = tr * fb;
                    if (std::abs (dlFbL) < 1.0e-7f) dlFbL = 0.0f;
                    if (std::abs (dlFbR) < 1.0e-7f) dlFbR = 0.0f;
                    outL[i] = outL[i] * (1.0f - 0.3f * mix) + tl * mix * 0.9f;
                    outR[i] = outR[i] * (1.0f - 0.3f * mix) + tr * mix * 0.9f;
                }
            }
        }

        // ---- spring reverb -----------------------------------------------
        {
            const float mix = clamp01 (p.spmix);
            const bool active = mix > 0.001f || std::abs (springs[0].fbLast) > 1.0e-5f;
            if (active)
            {
                const float fb = 0.5f + 0.47f * clamp01 (p.spdwell);
                const float toneHz = xmap (p.sptone, 1500.0f, 7000.0f);
                for (auto& s : springs) s.lp.setHz (toneHz, sr);
                for (int i = 0; i < nb; ++i)
                {
                    float x = 0.5f * (outL[i] + outR[i]);
                    // two short allpasses: the splash
                    { const float d = dif1.readInt ((int) (0.0037f * (float) sr)); const float u = x - 0.5f * d; dif1.push (u); x = d + 0.5f * u; }
                    { const float d = dif2.readInt ((int) (0.0059f * (float) sr)); const float u = x - 0.5f * d; dif2.push (u); x = d + 0.5f * u; }
                    const float s0 = springs[0].tick (x, fb), s1 = springs[1].tick (x, fb), s2 = springs[2].tick (x, fb);
                    const float wl = (0.7f * s0 + 0.5f * s1 + 0.25f * s2) * 0.9f;
                    const float wr = (0.25f * s0 + 0.5f * s1 + 0.7f * s2) * 0.9f;
                    outL[i] = outL[i] * (1.0f - 0.3f * mix) + wl * mix;
                    outR[i] = outR[i] * (1.0f - 0.3f * mix) + wr * mix;
                }
            }
        }

        // ---- master --------------------------------------------------------
        {
            const float g = volGain (p.volume) * 0.9f;
            const float hiss = 2.5e-4f * clamp01 (p.vintage);
            for (int i = 0; i < nb; ++i)
            {
                float l = outL[i] * g + hissRng.bi() * hiss;
                float r = outR[i] * g + hissRng.bi() * hiss;
                l = ceilSoft (clean (l), 0.985f); r = ceilSoft (clean (r), 0.985f);
                outL[i] = l; outR[i] = r;
                const float m = 0.5f * (l + r);
                rmsAcc += m * m; peakAcc = std::max (peakAcc, std::max (std::abs (l), std::abs (r))); ++rmsN;
                scope[(size_t) scopeWrite] = m; scopeWrite = (scopeWrite + 1) & (SCOPE_N - 1);
            }
            if (rmsN >= 1024)
            {
                outRms = rmsAcc / (float) rmsN; outPeak = peakAcc;
                rmsAcc = 0.0f; peakAcc = 0.0f; rmsN = 0;
            }
        }

        done += nb;
        samplesDone += (uint64_t) nb;
    }
}

} // namespace bk
