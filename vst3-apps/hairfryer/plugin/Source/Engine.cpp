/*  HAIRFRYER — engine implementation. The reasoning lives in Engine.h;
    this file is the machinery. */
#include "Engine.h"

namespace hf
{

//==============================================================================
// The chaos knob.
//
// r = 2.9..4.0, but not linearly: the period-three window — the guttural —
// is 3.8284..3.8415, which on a linear knob is one degree of rotation. The
// map flattens into a plateau there so the knob has a place to rest on it.
float chaosR (float t)
{
    t = clamp01 (t);
    if (t < 0.55f)  return 2.90f   + (t / 0.55f) * (3.5699f - 2.90f);          // cascade
    if (t < 0.80f)  return 3.5699f + ((t - 0.55f) / 0.25f) * (3.8284f - 3.5699f); // chaos
    if (t < 0.90f)  return 3.8319f;                                            // period 3
    return 3.8415f + ((t - 0.90f) / 0.10f) * (4.0f - 3.8415f);                 // deep chaos
}

// RATIO: which multiple of the voice the false folds aim at. Snapped to the
// musically meaningful lockpoints with slopes between them.
float subRatio (float t)
{
    static const float pts[5] = { 0.25f, 0.3333f, 0.5f, 1.0f, 1.5f };
    const float ft = clamp01 (t) * 4.0f;
    const int i = std::min (3, (int) ft);
    return lerp (pts[i], pts[i + 1], ft - (float) i);
}

// THROAT: 0.5 is neutral. The warp is capped where the formant slide is
// still voice-like rather than cartoon (about +/-35 %). The sign is the
// MEASURED one — knob up must move formants up — not the one the allpass
// convention suggested, which turned out to be its mirror.
float throatLambda (float t) { return (0.5f - clamp01 (t)) * 2.0f * 0.42f; }

//==============================================================================
// The parameter table.
#define PROW(field) [] (Params& p) -> float& { return p.field; }

static const PSpec SPECS[] =
{
    // ---- prep ----------------------------------------------------------
    { "gateon",  "GATE ON",        0.0f, KP_SW,   0, 0,        PROW (gateOn)   },
    { "gatethr", "GATE THRESHOLD", 0.2f, KP_DB,   84, 0,       PROW (gateThr)  }, // -70..-20 handled in fmt
    { "gateatt", "GATE ATTACK",    0.15f, KP_MS,  0.1f, 30,    PROW (gateAtt)  },
    { "gatehold","GATE HOLD",      0.3f, KP_MS,   5, 500,      PROW (gateHold) },
    { "gaterel", "GATE RELEASE",   0.4f, KP_MS,   20, 1000,    PROW (gateRel)  },
    { "hpf",     "HIGH PASS",      0.35f, KP_HZ,  20, 400,     PROW (hpf)      },
    { "dson",    "DE-ESS ON",      1.0f, KP_SW,   0, 0,        PROW (deEssOn)  },
    { "dsfrq",   "DE-ESS FREQ",    0.5f, KP_HZ,   3000, 9000,  PROW (deEssFrq) },
    { "dsamt",   "DE-ESS AMOUNT",  0.35f, KP_PCT, 0, 0,        PROW (deEssAmt) },

    // ---- basket --------------------------------------------------------
    { "engon",   "FRYER ON",       1.0f, KP_SW,   0, 0,        PROW (engOn)    },
    { "blend",   "BLEND",          0.85f, KP_PCT, 0, 0,        PROW (blend)    },
    { "crisp",   "CRISP",          0.7f, KP_PCT,  0, 0,        PROW (crisp)    },
    { "match",   "LEVEL MATCH",    1.0f, KP_SW,   0, 0,        PROW (match)    },

    { "alevel",  "RACK A LEVEL",   0.7f, KP_PCT,  0, 0,        PROW (a.level)  },
    { "achaos",  "RACK A CHAOS",   0.5f, KP_CHAOS,0, 0,        PROW (a.chaos)  },
    { "agrip",   "RACK A GRIP",    0.6f, KP_PCT,  0, 0,        PROW (a.grip)   },
    { "ajit",    "RACK A JITTER",  0.25f, KP_PCT, 0, 0,        PROW (a.jitter) },
    { "arasp",   "RACK A RASP",    0.4f, KP_PCT,  0, 0,        PROW (a.rasp)   },
    { "atone",   "RACK A TONE",    0.5f, KP_HZ,   700, 12000,  PROW (a.tone)   },
    { "afold",   "RACK A FOLD",    0.15f, KP_PCT, 0, 0,        PROW (a.fold)   },
    { "asplit",  "RACK A SPLIT",   0.35f, KP_PCT, 0, 0,        PROW (a.split)  },
    { "aratio",  "RACK A RATIO",   0.5f, KP_SUB,  0, 0,        PROW (a.ratio)  },
    { "athroat", "RACK A THROAT",  0.42f, KP_THROAT, 0, 0,     PROW (a.throat) },

    { "blevel",  "RACK B LEVEL",   0.0f, KP_PCT,  0, 0,        PROW (b.level)  },
    { "bchaos",  "RACK B CHAOS",   0.85f, KP_CHAOS, 0, 0,      PROW (b.chaos)  },
    { "bgrip",   "RACK B GRIP",    0.7f, KP_PCT,  0, 0,        PROW (b.grip)   },
    { "bjit",    "RACK B JITTER",  0.35f, KP_PCT, 0, 0,        PROW (b.jitter) },
    { "brasp",   "RACK B RASP",    0.55f, KP_PCT, 0, 0,        PROW (b.rasp)   },
    { "btone",   "RACK B TONE",    0.62f, KP_HZ,  700, 12000,  PROW (b.tone)   },
    { "bfold",   "RACK B FOLD",    0.3f, KP_PCT,  0, 0,        PROW (b.fold)   },
    { "bsplit",  "RACK B SPLIT",   0.5f, KP_PCT,  0, 0,        PROW (b.split)  },
    { "bratio",  "RACK B RATIO",   0.25f, KP_SUB, 0, 0,        PROW (b.ratio)  },
    { "bthroat", "RACK B THROAT",  0.62f, KP_THROAT, 0, 0,     PROW (b.throat) },

    // ---- heat ----------------------------------------------------------
    { "drive",   "DRIVE",          0.3f, KP_PCT,  0, 0,        PROW (drive)    },
    { "tube",    "TUBE",           0.35f, KP_PCT, 0, 0,        PROW (tube)     },
    { "sparkle", "SPARKLE",        0.25f, KP_PCT, 0, 0,        PROW (sparkle)  },
    { "os",      "OVERSAMPLING",   1.0f, KP_OS,   0, 2,        PROW (os)       },

    // ---- seasoning -----------------------------------------------------
    { "eqon",    "EQ ON",          1.0f, KP_SW,   0, 0,        PROW (eqOn)     },
    { "lof",     "EQ LOW FREQ",    0.3f, KP_HZ,   60, 500,     PROW (loF)      },
    { "log",     "EQ LOW GAIN",    0.5f, KP_DB,   30, 0,       PROW (loG)      },
    { "p1f",     "EQ MID 1 FREQ",  0.35f, KP_HZ,  150, 2000,   PROW (p1F)      },
    { "p1g",     "EQ MID 1 GAIN",  0.5f, KP_DB,   30, 0,       PROW (p1G)      },
    { "p1q",     "EQ MID 1 Q",     0.4f, KP_RATIO, 0.4f, 4.0f, PROW (p1Q)      },
    { "p2f",     "EQ MID 2 FREQ",  0.62f, KP_HZ,  600, 8000,   PROW (p2F)      },
    { "p2g",     "EQ MID 2 GAIN",  0.5f, KP_DB,   30, 0,       PROW (p2G)      },
    { "p2q",     "EQ MID 2 Q",     0.4f, KP_RATIO, 0.4f, 4.0f, PROW (p2Q)      },
    { "hif",     "EQ HIGH FREQ",   0.7f, KP_HZ,   1500, 16000, PROW (hiF)      },
    { "hig",     "EQ HIGH GAIN",   0.5f, KP_DB,   30, 0,       PROW (hiG)      },

    // ---- pressure ------------------------------------------------------
    { "compon",  "COMP ON",        1.0f, KP_SW,   0, 0,        PROW (compOn)   },
    { "cthr",    "COMP THRESHOLD", 0.45f, KP_DB,  100, 0,      PROW (cThr)     }, // -50..0 handled in fmt
    { "cratio",  "COMP RATIO",     0.35f, KP_RATIO, 1.5f, 12,  PROW (cRatio)   },
    { "catt",    "COMP ATTACK",    0.25f, KP_MS,  0.3f, 60,    PROW (cAtt)     },
    { "crel",    "COMP RELEASE",   0.35f, KP_MS,  40, 1200,    PROW (cRel)     },
    { "cknee",   "COMP KNEE",      0.5f, KP_DB,   36, 0,       PROW (cKnee)    }, // 0..18 handled in fmt
    { "cmake",   "COMP MAKEUP",    0.0f, KP_DB,   48, 0,       PROW (cMake)    }, // 0..24 handled in fmt
    { "cauto",   "COMP AUTO GAIN", 1.0f, KP_SW,   0, 0,        PROW (cAuto)    },
    { "limon",   "LIMITER ON",     1.0f, KP_SW,   0, 0,        PROW (limOn)    },
    { "limceil", "LIMITER CEILING",0.85f, KP_CEIL, -12, -0.3f, PROW (limCeil)  },
    { "limrel",  "LIMITER RELEASE",0.4f, KP_MS,   30, 800,     PROW (limRel)   },

    // ---- master --------------------------------------------------------
    { "ingain",  "INPUT",          0.5f, KP_DB,   48, 0,       PROW (inGain)   },
    { "outgain", "OUTPUT",         0.5f, KP_DB,   48, 0,       PROW (outGain)  },
    { "mix",     "MIX",            1.0f, KP_PCT,  0, 0,        PROW (mix)      },
};
#undef PROW

int numParams()               { return (int) (sizeof (SPECS) / sizeof (SPECS[0])); }
const PSpec& paramSpec (int i){ return SPECS[(size_t) i]; }

//==============================================================================
// Recipes. Each starts from defaults so a recipe is a complete patch, not a
// diff against whatever was there.
const char* recipeName (int i)
{
    static const char* n[NUM_RECIPES] = {
        "CLEAN PASS", "WARM STRIP", "AIR FRY", "FALSE CORD",
        "GUTTURAL", "PIG SQUEAL", "BLACKENED", "DOOM LOW", "TWO THROATS"
    };
    return (i >= 0 && i < NUM_RECIPES) ? n[i] : "?";
}

const char* recipeBlurb (int i)
{
    static const char* n[NUM_RECIPES] = {
        "the strip alone, fryer off — a clean vocal channel",
        "the strip pushed: tube, glue, sheen — still a singing voice",
        "light fry on top of the clean voice, like a worn edge",
        "period-doubled low growl, false folds locked an octave down",
        "the period-three window: the deep guttural",
        "throat shrunk, folds pushed high — the squeal",
        "chaotic, bright, torn — high screams over a blasting mix",
        "slow, huge, subharmonic — for doom and sludge tempos",
        "both racks at once: growl under, shriek over"
    };
    return (i >= 0 && i < NUM_RECIPES) ? n[i] : "?";
}

void applyRecipe (int i, Params& p)
{
    p = Params{};                          // every recipe starts from the strip's defaults

    switch (i)
    {
        default:
        case 0:  // CLEAN PASS
            p.engOn = 0.0f; p.blend = 0.0f;
            p.drive = 0.1f; p.tube = 0.15f; p.sparkle = 0.15f;
            break;

        case 1:  // WARM STRIP
            p.engOn = 0.0f; p.blend = 0.0f;
            p.drive = 0.35f; p.tube = 0.55f; p.sparkle = 0.35f;
            p.cThr = 0.38f; p.cRatio = 0.42f; p.cKnee = 0.6f;
            p.loG = 0.56f; p.hiG = 0.58f;
            break;

        case 2:  // AIR FRY
            p.blend = 0.45f;
            p.a.level = 0.6f; p.a.chaos = 0.93f; p.a.grip = 0.45f;
            p.a.rasp = 0.35f; p.a.tone = 0.62f; p.a.fold = 0.1f;
            p.a.split = 0.15f; p.a.throat = 0.5f;
            p.b.level = 0.0f;
            p.drive = 0.25f;
            break;

        case 3:  // FALSE CORD
            p.blend = 0.9f;
            p.a.level = 0.75f; p.a.chaos = 0.3f; p.a.grip = 0.75f;
            p.a.jitter = 0.3f; p.a.rasp = 0.45f; p.a.tone = 0.42f;
            p.a.fold = 0.2f; p.a.split = 0.6f; p.a.ratio = 0.5f;   // f0/2
            p.a.throat = 0.4f;
            p.b.level = 0.0f;
            p.drive = 0.4f; p.tube = 0.45f;
            p.loG = 0.56f; p.p2F = 0.55f; p.p2G = 0.56f;
            break;

        case 4:  // GUTTURAL
            p.blend = 0.95f;
            p.a.level = 0.8f; p.a.chaos = 0.85f;                   // the plateau
            p.a.grip = 0.8f; p.a.jitter = 0.4f; p.a.rasp = 0.55f;
            p.a.tone = 0.45f; p.a.fold = 0.3f; p.a.split = 0.5f;
            p.a.ratio = 0.25f;                                     // exactly f0/3
            p.a.throat = 0.35f;
            p.b.level = 0.0f;
            p.drive = 0.45f; p.tube = 0.5f;
            p.loG = 0.58f; p.hiG = 0.54f;
            break;

        case 5:  // PIG SQUEAL
            p.blend = 0.95f;
            p.a.level = 0.75f; p.a.chaos = 0.6f; p.a.grip = 0.6f;
            p.a.jitter = 0.5f; p.a.rasp = 0.4f; p.a.tone = 0.75f;
            p.a.fold = 0.35f; p.a.split = 0.7f; p.a.ratio = 0.85f;
            p.a.throat = 0.85f;
            p.b.level = 0.0f;
            p.drive = 0.5f; p.sparkle = 0.45f;
            p.p2F = 0.7f; p.p2G = 0.58f;
            break;

        case 6:  // BLACKENED
            p.blend = 0.9f; p.crisp = 0.85f;
            p.a.level = 0.75f; p.a.chaos = 0.97f; p.a.grip = 0.65f;
            p.a.jitter = 0.55f; p.a.rasp = 0.8f; p.a.tone = 0.8f;
            p.a.fold = 0.55f; p.a.split = 0.3f; p.a.ratio = 0.75f;
            p.a.throat = 0.58f;
            p.b.level = 0.0f;
            p.drive = 0.55f; p.sparkle = 0.5f;
            p.hiG = 0.6f;
            break;

        case 7:  // DOOM LOW
            p.blend = 0.95f; p.crisp = 0.4f;
            p.a.level = 0.8f; p.a.chaos = 0.35f; p.a.grip = 0.9f;
            p.a.jitter = 0.2f; p.a.rasp = 0.5f; p.a.tone = 0.3f;
            p.a.fold = 0.25f; p.a.split = 0.65f; p.a.ratio = 0.0f;  // f0/4
            p.a.throat = 0.28f;
            p.b.level = 0.0f;
            p.drive = 0.5f; p.tube = 0.6f;
            p.loF = 0.4f; p.loG = 0.6f; p.hiG = 0.44f;
            break;

        case 8:  // TWO THROATS
            p.blend = 0.95f;
            p.a.level = 0.7f; p.a.chaos = 0.32f; p.a.grip = 0.75f;
            p.a.rasp = 0.45f; p.a.tone = 0.4f; p.a.split = 0.6f;
            p.a.ratio = 0.5f; p.a.throat = 0.36f;
            p.b.level = 0.55f; p.b.chaos = 0.9f; p.b.grip = 0.55f;
            p.b.jitter = 0.5f; p.b.rasp = 0.6f; p.b.tone = 0.78f;
            p.b.fold = 0.4f; p.b.split = 0.55f; p.b.ratio = 0.9f;
            p.b.throat = 0.72f;
            p.drive = 0.45f;
            break;
    }
}

//==============================================================================
// PitchTrack
void PitchTrack::prepare (double sampleRate)
{
    sr = sampleRate;
    dsr = sr / DEC;
    aa1.setHz ((float) (dsr * 0.38), sr);
    aa2.setHz ((float) (dsr * 0.38), sr);
    ring.assign ((size_t) (WIN * 2), 0.0f);
    // 60 Hz .. 1000 Hz
    minLag = std::max (4, (int) (dsr / 1000.0));
    maxLag = std::min (WIN / 2, (int) (dsr / 60.0));
    reset();
}

void PitchTrack::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    rw = decCount = hopCount = 0;
    aa1.reset(); aa2.reset();
    f0 = 0.0f; clarity = 0.0f;
}

void PitchTrack::pushSample (float x)
{
    const float y = aa2.lp (aa1.lp (x));
    if (++decCount < DEC) return;
    decCount = 0;
    ring[(size_t) rw] = y;
    if (++rw >= WIN * 2) rw = 0;
    if (++hopCount >= HOP) { hopCount = 0; analyse(); }
}

void PitchTrack::analyse()
{
    // last WIN decimated samples, oldest first
    float x[WIN];
    int r = rw - WIN; while (r < 0) r += WIN * 2;
    for (int i = 0; i < WIN; ++i)
    {
        x[i] = ring[(size_t) r];
        if (++r >= WIN * 2) r = 0;
    }

    const int W = WIN - maxLag;             // fixed-length comparison window

    double e0 = 1.0e-12;
    for (int i = 0; i < W; ++i) e0 += (double) x[i] * x[i];
    if (e0 < 1.0e-6) { f0 = 0.0f; clarity = 0.0f; return; }

    // NSDF over the lag range
    static thread_local std::vector<float> nsdf;
    nsdf.assign ((size_t) (maxLag + 1), 0.0f);

    double eTau = 1.0e-12;
    for (int i = 0; i < W; ++i) eTau += (double) x[i] * x[i];
    for (int tau = minLag; tau <= maxLag; ++tau)
    {
        double ac = 0.0, eL = 1.0e-12, eR = 1.0e-12;
        for (int i = 0; i < W; ++i)
        {
            ac += (double) x[i] * x[i + tau];
            eL += (double) x[i] * x[i];
            eR += (double) x[i + tau] * x[i + tau];
        }
        nsdf[(size_t) tau] = (float) (2.0 * ac / (eL + eR));
    }

    // McLeod peak picking: the first local max above 0.85 of the global max
    float gmax = 0.0f;
    for (int t = minLag; t <= maxLag; ++t) gmax = std::max (gmax, nsdf[(size_t) t]);
    if (gmax < 0.25f) { clarity *= 0.7f; if (clarity < 0.05f) f0 = 0.0f; return; }

    int pick = 0;
    for (int t = minLag + 1; t < maxLag; ++t)
    {
        const float v = nsdf[(size_t) t];
        if (v > 0.85f * gmax && v >= nsdf[(size_t) (t - 1)] && v >= nsdf[(size_t) (t + 1)])
        { pick = t; break; }
    }
    if (pick == 0) { clarity *= 0.7f; return; }

    // parabolic refinement
    const float ym = nsdf[(size_t) (pick - 1)], y0 = nsdf[(size_t) pick], yp = nsdf[(size_t) (pick + 1)];
    const float den = ym - 2.0f * y0 + yp;
    const float dt = (std::abs (den) > 1.0e-9f) ? clampf (0.5f * (ym - yp) / den, -0.5f, 0.5f) : 0.0f;

    const float lag = (float) pick + dt;
    const float cand = (float) dsr / lag;
    const float cl = clamp01 (y0);

    // slew rather than jump: a tracker that flips octave for one frame must
    // not drag the whole basket with it
    if (f0 <= 0.0f) f0 = cand;
    else            f0 += (cand - f0) * (cl > clarity ? 0.5f : 0.2f);
    clarity += (cl - clarity) * 0.4f;
}

//==============================================================================
// LpcFrame
void LpcFrame::prepare (double)
{
    ring.assign ((size_t) (N * 2), 0.0f);
    work.assign ((size_t) N, 0.0f);
    warped.assign ((size_t) N, 0.0f);
    win.assign ((size_t) N, 0.0f);
    for (int i = 0; i < N; ++i)
        win[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) (N - 1));
    hopLen = 256;
    reset();
}

void LpcFrame::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    rw = hop = 0;
    kAna.fill (0.0f); kSynA.fill (0.0f); kSynB.fill (0.0f);
    gain = 0.0f;
    valid = false;
}

bool LpcFrame::levinson (const double* r, int order, float* k, double& err)
{
    double a[LPC_ORDER + 1] = { 1.0 };
    err = r[0];
    if (err <= 1.0e-9) return false;

    for (int m = 1; m <= order; ++m)
    {
        double acc = r[m];
        for (int i = 1; i < m; ++i) acc -= a[i] * r[m - i];
        double kk = acc / err;
        if (! (kk > -1.0e9 && kk < 1.0e9)) return false;
        kk = std::max (-0.998, std::min (0.998, kk));
        k[m - 1] = (float) kk;

        double an[LPC_ORDER + 1];
        an[m] = kk;
        for (int i = 1; i < m; ++i) an[i] = a[i] - kk * a[m - i];
        for (int i = 1; i <= m; ++i) a[i] = an[i];

        err *= (1.0 - kk * kk);
        if (err <= 1.0e-12) { for (int j = m; j < order; ++j) k[j] = 0.0f; break; }
    }
    return true;
}

/*  The warped autocorrelation: r~[m] = sum_n x[n] * (D1^m x)[n], where D1 is
    the first-order allpass (z^-1 - l)/(1 - l z^-1). Levinson on r~ gives an
    AR model of the spectrum on the warped frequency axis; realising it in an
    ORDINARY lattice renders that warped spectrum on the linear axis, which
    slides every formant along the bilinear map while the pitch — which lives
    in the residual, not the filter — stays put. */
void LpcFrame::warpTo (float lambda, std::array<float, LPC_ORDER>& dest, const double* plain)
{
    if (std::abs (lambda) < 1.0e-4f)
    {
        // exactness matters: at neutral the analysis and synthesis lattices
        // must carry the SAME numbers so the pair cancels to the bit
        dest = kAna;
        return;
    }

    double r[LPC_ORDER + 1] = {};
    (void) plain;

    // y0 = windowed frame; repeatedly allpass it, correlating against y0
    const float l = lambda;
    std::copy (work.begin(), work.end(), warped.begin());

    double acc = 1.0e-9;
    for (int n = 0; n < N; ++n) acc += (double) work[(size_t) n] * warped[(size_t) n];
    r[0] = acc;

    for (int m = 1; m <= LPC_ORDER; ++m)
    {
        // in-place allpass pass over the frame
        float z = 0.0f;
        for (int n = 0; n < N; ++n)
        {
            const float in = warped[(size_t) n];
            const float out = z - l * in;
            z = in + l * out;
            warped[(size_t) n] = out;
        }
        acc = 0.0;
        for (int n = 0; n < N; ++n) acc += (double) work[(size_t) n] * warped[(size_t) n];
        r[m] = acc;
    }

    // lag window: regularises the fit, costs nothing audible
    for (int m = 1; m <= LPC_ORDER; ++m)
        r[m] *= std::exp (-0.5 * std::pow (0.012 * (double) m, 2.0));
    r[0] *= 1.0001;

    double e = 0.0;
    if (! levinson (r, LPC_ORDER, dest.data(), e)) dest = kAna;
}

void LpcFrame::analyse (float lamA, float lamB)
{
    // copy the last N samples, windowed
    int r0 = rw - N; while (r0 < 0) r0 += N * 2;
    for (int i = 0; i < N; ++i)
    {
        work[(size_t) i] = ring[(size_t) r0] * win[(size_t) i];
        if (++r0 >= N * 2) r0 = 0;
    }

    double r[LPC_ORDER + 1] = {};
    for (int m = 0; m <= LPC_ORDER; ++m)
    {
        double acc = 0.0;
        for (int n = 0; n < N - m; ++n)
            acc += (double) work[(size_t) n] * work[(size_t) (n + m)];
        r[m] = acc;
    }
    for (int m = 1; m <= LPC_ORDER; ++m)
        r[m] *= std::exp (-0.5 * std::pow (0.012 * (double) m, 2.0));
    r[0] *= 1.0001;
    r[0] += 1.0e-9;

    double err = 0.0;
    if (! levinson (r, LPC_ORDER, kAna.data(), err))
    {
        valid = false;
        return;
    }

    // per-sample residual RMS over the window (window power folded in)
    double wp = 0.0;
    for (int i = 0; i < N; ++i) wp += (double) win[(size_t) i] * win[(size_t) i];
    gain = (float) std::sqrt (std::max (0.0, err) / std::max (1.0, wp));

    warpTo (lamA, kSynA, r);
    warpTo (lamB, kSynB, r);
    valid = true;
}

//==============================================================================
// Rack
void Rack::prepare (double sampleRate)
{
    sr = sampleRate;
    jitBaseF = (float) jitterBase (sr);
    jit.prepare (jitterBase (sr) * 2 + Half::roundTrip() + 8);
    osUp.prepare();
    osDn.prepare();
    ffDc.setHz (20.0f, sr);
    outDc.setHz (10.0f, sr);
    lastTone = -1.0f;
    reset();
}

void Rack::reset()
{
    x = 0.37f;
    gainNow = gainTarget = 1.0f; gainStep = 0.0f; rampLeft = 0;
    jitNow = jitTarget = 0.0f;
    nzAmp = 0.0f;
    hist.fill (1.0f); histW = 0;
    jit.reset();
    noiseTilt.reset(); noisePeak.reset();
    ff.reset(); ffOut = 0.0f;
    ffDc.reset(); outDc.reset();
    syn.reset();
    osUp.reset(); osDn.reset();
}

void Rack::newPeriod (const RackParams& p, float period, float f0, float voiced)
{
    // one logistic step per glottal period — this is the bifurcation cascade
    const float r = chaosR (p.chaos);
    x = r * x * (1.0f - x);
    if (! (x > 1.0e-6f && x < 1.0f)) x = 0.37f + 0.013f * rng.uni();

    /*  The per-period gain. x lives on the attractor (up to ~r/4 -> 1.0);
        map it so the LOUD periods stay near unity and the pattern carves
        DOWN from there — carving up instead just pumps the level. The
        power stretches the carve: the logistic two-cycle near r = 3.2 only
        swings x by ~0.3, which as a linear gain was a polite 4 dB wobble
        when the point of GRIP is a rasp you can feel. */
    const float depth = p.grip * 0.9f;
    const float shaped = std::pow (clamp01 ((x - 0.2f) * 1.6f), 2.2f);
    gainTarget = clampf (1.0f - depth * (1.0f - shaped), 0.05f, 1.2f);

    /*  Short ramp on purpose: it lands at a glottal closure, where the ear
        expects an edge — and a slow ramp turns the period pattern into a
        triangle wave, which halves the subharmonic it exists to make. */
    rampLeft = std::max (6, std::min ((int) (period * 0.15f), 48));
    gainStep = (gainTarget - gainNow) / (float) rampLeft;

    // timing jitter, one draw per period, slewed toward per sample. Capped
    // by the base offset it swings around, or the read runs off the line.
    jitTarget = p.jitter * rng.bi() * std::min (period * 0.08f, jitBaseF * 0.8f);

    // the noise burst rides the SAME chaos: quiet period -> noisier
    nzAmp = p.rasp * (0.35f + 0.65f * (1.0f - shaped)) * (0.55f + 0.45f * voiced);

    // false folds re-tuned once per period (f0 moves slowly)
    if (f0 > 20.0f)
        ff.set (clampf (f0 * subRatio (p.ratio), 25.0f, 2500.0f), 7.0f, sr);

    hist[(size_t) histW] = gainTarget;
    histW = (histW + 1) % GAIN_HIST;
}

int Rack::subOrder() const
{
    // read the period pattern straight off the recent gain history
    float v[8];
    for (int i = 0; i < 8; ++i)
        v[i] = hist[(size_t) ((histW - 1 - i + 2 * GAIN_HIST) % GAIN_HIST)];

    float mean = 0.0f;
    for (float g : v) mean += g;
    mean *= 0.125f;
    float var = 0.0f;
    for (float g : v) var += (g - mean) * (g - mean);
    if (var < 1.0e-4f) return 1;

    float best = 0.0f; int bestLag = 0;
    for (int lag = 2; lag <= 4; ++lag)
    {
        float num = 0.0f, den = 1.0e-9f;
        for (int i = 0; i + lag < 8; ++i)
        {
            num += (v[i] - mean) * (v[i + lag] - mean);
            den += (v[i] - mean) * (v[i] - mean);
        }
        const float c = num / den;
        if (c > best) { best = c; bestLag = lag; }
    }
    return best > 0.55f ? bestLag : 0;
}

float Rack::run (const RackParams& p, float eN, float phase, float voiced,
                 float crisp, float frameGain)
{
    // per-period gain ramp
    if (rampLeft > 0) { gainNow += gainStep; --rampLeft; }
    jitNow += (jitTarget - jitNow) * 0.002f;

    /*  One delay line does two jobs. The first tap, at the base offset plus
        the period's wobble, is the jittered residual; it goes through the
        oversampled folder, which costs Half::roundTrip() more. The second
        tap sits exactly that much later, so the folded and unfolded sides
        line up and the FOLD knob is a crossfade, not a comb filter. The
        rack's total delay at rest is jitterBase + roundTrip — which is what
        the engine delays the dry path by. */
    jit.push (eN);
    const float dj = jitBaseF + jitNow;
    const float eJ = jit.read (dj);

    float e2;
    {
        const float d = 1.0f + 7.0f * p.fold;
        float up[2];
        osUp.up (eJ, up);
        for (int i = 0; i < 2; ++i)
        {
            const float g = clampf (up[i] * d, -3.2f, 3.2f);
            up[i] = std::sin (1.5707963f * g) * 0.72f;   // sine fold
        }
        const float folded  = osDn.down (up[0], up[1]);
        const float aligned = jit.read (dj + (float) Half::roundTrip());
        e2 = lerp (aligned, folded, smoothstep (0.0f, 1.0f, p.fold));
    }

    // ---- pulsed noise ----------------------------------------------------
    if (p.tone != lastTone)
    {
        lastTone = p.tone;
        noiseTilt.lowpass (xmap (p.tone, 700.0f, 12000.0f), 0.6f, sr);
        noisePeak.peak (2800.0f, 0.8f, 4.0f, sr);
    }
    const float burstK = lerp (3.0f, 14.0f, crisp);
    const float w = lerp (1.0f, std::exp (-burstK * phase), voiced);
    const float nz = noisePeak (noiseTilt (rng.bi())) * nzAmp * w * 2.2f;

    // ---- the excitation this period deserves -----------------------------
    float ex = (e2 + nz) * gainNow;

    // ---- false folds: bandpass with a saturator in the loop --------------
    {
        const float fb = p.split * 2.1f;
        const float y = ff.bp (ex * 0.8f + std::tanh (1.3f * ffOut) * fb);
        ffOut = ffDc (clampf (y, -4.0f, 4.0f));
        ex += ffOut * p.split * 1.6f;
    }

    // ---- back through a vocal tract --------------------------------------
    const float out = outDc (syn.synth (ex * frameGain));
    const float lv = p.level * p.level * 1.6f;
    return clean (out) * lv;
}

//==============================================================================
// Chan
void Chan::prepare (double sr, int basketLat, int limSamples, int total)
{
    hp1.bypass(); hp2.bypass(); ds1.bypass(); ds2.bypass();
    up1.prepare(); dn1.prepare(); up2.prepare(); dn2.prepare();
    sparkSplit.reset(); sparkPost.reset();
    heatDc.setHz (12.0f, sr);
    eqLo.bypass(); eqP1.bypass(); eqP2.bypass(); eqHi.bypass();
    dry.prepare (basketLat + 8);
    pad.prepare (32);
    lim.prepare (limSamples + 8);
    mix.prepare (total + 8);
    reset();
}

void Chan::reset()
{
    hp1.reset(); hp2.reset(); ds1.reset(); ds2.reset();
    up1.reset(); dn1.reset(); up2.reset(); dn2.reset();
    os4Held = 0.0f;
    sparkSplit.reset(); sparkPost.reset();
    heatDc.reset();
    eqLo.reset(); eqP1.reset(); eqP2.reset(); eqHi.reset();
    dry.reset(); pad.reset(); lim.reset(); mix.reset();
}

//==============================================================================
// Engine
Engine::Engine() = default;

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;

    /*  The latency budget, all integers, all constant:
          basket = jitter line base + the rack folder's oversampler
          heat   = padded to the 4x case whatever the switch says
          limit  = 3 ms lookahead                                          */
    const int jitBase = (int) (0.0011 * sr) + 4;
    basketLatency = jitBase + Half::roundTrip();
    heatPad  = Half::roundTrip() + (int) (8);      // 15 outer + 8 inner at 4x
    limLen   = (int) (0.003 * sr);
    reportedLatency = basketLatency + heatPad + limLen;

    for (auto& c : ch) c.prepare (sr, basketLatency, limLen, reportedLatency);

    pitch.prepare (sr);
    frame.prepare (sr);

    rackA.prepare (sr);
    rackB.prepare (sr);
    rackA.rng.seed (0x1234567u);
    rackB.rng.seed (0x89abcdeu);

    gateFast.setTimeMs (0.4f, sr);
    dsHf.setTimeMs (1.2f, sr);
    dsAll.setTimeMs (6.0f, sr);
    epochLp.setHz (3000.0f, sr);
    epochFloor.setTimeMs (60.0f, sr);
    scDet.setTimeMs (8.0f, sr);
    autoMk.setTimeMs (900.0f, sr);
    matchDryF.setTimeMs (140.0f, sr);
    matchWetF.setTimeMs (140.0f, sr);
    heatInF.setTimeMs (180.0f, sr);
    heatOutF.setTimeMs (180.0f, sr);
    inM.setTimeMs (80.0f, sr);
    outM.setTimeMs (80.0f, sr);

    lastHpf = lastDs = -1.0f;
    for (auto& v : lastEq) v = -1.0f;
    lastOs = -1;

    reset();
}

void Engine::reset()
{
    for (auto& c : ch) c.reset();
    pitch.reset();
    frame.reset();
    ana.zero();
    kAnaNow.fill (0.0f); kAnaStep.fill (0.0f);
    kSynANow.fill (0.0f); kSynAStep.fill (0.0f);
    kSynBNow.fill (0.0f); kSynBStep.fill (0.0f);
    interpLeft = 0;
    frameGain = 0.0f; frameGainStep = 0.0f;
    epochLp.reset(); epochFloor.reset();
    ePrev = ePrev2 = 0.0f;
    sinceEpoch = 0;
    periodNow = (float) (sr / 130.0);
    phase = 0.0f;
    rackA.reset(); rackB.reset();
    gateFast.reset(); dsHf.reset(); dsAll.reset();
    gateGain = 0.0f; dsGain = 1.0f; gateHoldLeft = 0;
    scDet.reset(); autoMk.reset();
    compG1 = compG2 = 0.0f;
    limEnv = 0.0f; limG1 = limG2 = 1.0f;
    matchDryF.reset(); matchWetF.reset();
    matchGain = 1.0f;
    heatInF.reset(); heatOutF.reset();
    heatGain = 1.0f;
    inM.reset(); outM.reset();
    inRms = outRms = 0.0f;
    gateGr = deEssGr = compGr = limGr = 0.0f;
    f0Hz = 0.0f; clarity = 0.0f; matchDb = 0.0f; resLevel = 0.0f;
    orderA = orderB = 1;
}

void Engine::updateFilters()
{
    if (p.hpf != lastHpf)
    {
        lastHpf = p.hpf;
        const float f = xmap (p.hpf, 20.0f, 400.0f);
        for (auto& c : ch)
        {
            c.hp1.highpass (f, 0.54f, sr);      // cascaded Butterworth pair -> LR4
            c.hp2.highpass (f, 1.31f, sr);
        }
    }
    if (p.deEssFrq != lastDs)
    {
        lastDs = p.deEssFrq;
        const float f = xmap (p.deEssFrq, 3000.0f, 9000.0f);
        for (auto& c : ch)
        {
            c.ds1.lowpass (f, 0.7071f, sr);
            c.ds2.lowpass (f, 0.7071f, sr);
        }
    }

    const float sig[11] = { p.eqOn, p.loF, p.loG, p.p1F, p.p1G, p.p1Q,
                            p.p2F, p.p2G, p.p2Q, p.hiF, p.hiG };
    bool eqDirty = false;
    for (int i = 0; i < 11; ++i) if (sig[i] != lastEq[i]) { eqDirty = true; break; }
    if (eqDirty)
    {
        for (int i = 0; i < 11; ++i) lastEq[i] = sig[i];
        for (auto& c : ch)
        {
            if (p.eqOn < 0.5f)
            {
                c.eqLo.bypass(); c.eqP1.bypass(); c.eqP2.bypass(); c.eqHi.bypass();
            }
            else
            {
                c.eqLo.lowShelf  (xmap (p.loF, 60.0f, 500.0f),   0.8f, (p.loG - 0.5f) * 30.0f, sr);
                c.eqP1.peak      (xmap (p.p1F, 150.0f, 2000.0f), xmap (p.p1Q, 0.4f, 4.0f),
                                  (p.p1G - 0.5f) * 30.0f, sr);
                c.eqP2.peak      (xmap (p.p2F, 600.0f, 8000.0f), xmap (p.p2Q, 0.4f, 4.0f),
                                  (p.p2G - 0.5f) * 30.0f, sr);
                c.eqHi.highShelf (xmap (p.hiF, 1500.0f, 16000.0f), 0.8f, (p.hiG - 0.5f) * 30.0f, sr);
            }
        }
    }

    const int os = (int) clampf (p.os, 0.0f, 2.0f);
    if (os != lastOs)
    {
        lastOs = os;
        const double heatRate = sr * (os == 0 ? 1.0 : (os == 1 ? 2.0 : 4.0));
        for (auto& c : ch)
        {
            c.sparkSplit.setHz (5500.0f, heatRate);
            c.sparkPost.setHz (16000.0f, heatRate);
        }
    }
}

/*  One sample of the heat stage at whatever rate the oversampler runs. Tube
    first — an asymmetric soft clip whose negative half is driven harder, so
    it seasons with even harmonics — then the sparkle exciter on the band
    above ~5.5 kHz. */
float Engine::heatShape (float x, Chan& c)
{
    /*  With all three knobs at zero the stage is a WIRE, not a polite tanh:
        even unity tanh redistributes a vocal's small partials by ten dB,
        and "off" has to mean off. The crossfade is short so anything
        audible gets the full treatment. */
    const float amt = smoothstep (0.0f, 0.05f,
                                  std::max (p.drive, std::max (p.tube, p.sparkle)));
    if (amt <= 0.0f) return x;

    const float pre = xmap (p.drive, 1.0f, 22.0f);
    const float asym = p.tube;

    const float g = pre * x;
    const float sym = std::tanh (g);
    const float ah = std::tanh (g * (1.0f + 1.15f * asym) - 0.22f * asym);
    float y = lerp (sym, ah, 0.6f * asym);

    // sparkle: drive the top band into its own soft clip and fold it back
    const float hsIn = y - c.sparkSplit.lp (y);
    const float excited = std::tanh (hsIn * (1.6f + 5.0f * p.sparkle));
    y += (excited - hsIn) * p.sparkle * 0.7f;
    y = c.sparkPost.lp (y);

    return lerp (x, y, amt);
}

void Engine::process (float* left, float* right, int numSamples)
{
    updateFilters();

    const float inG  = std::pow (10.0f, (p.inGain  - 0.5f) * 48.0f / 20.0f);
    const float outG = std::pow (10.0f, (p.outGain - 0.5f) * 48.0f / 20.0f);

    const bool basketOn = p.engOn >= 0.5f;
    const float lamA = throatLambda (p.a.throat);
    const float lamB = throatLambda (p.b.throat);

    // gate constants
    const float gThr = std::pow (10.0f, lerp (-70.0f, -20.0f, p.gateThr) / 20.0f);
    const float gAttC = 1.0f - std::exp (-1.0f / (float) (xmap (p.gateAtt, 0.1f, 30.0f) * 0.001 * sr));
    const float gRelC = 1.0f - std::exp (-1.0f / (float) (xmap (p.gateRel, 20.0f, 1000.0f) * 0.001 * sr));
    const int   gHold = (int) (xmap (p.gateHold, 5.0f, 500.0f) * 0.001 * sr);

    // compressor constants
    const float cThrDb  = lerp (-50.0f, 0.0f, p.cThr);
    const float cRatio  = xmap (p.cRatio, 1.5f, 12.0f);
    const float cKneeDb = lerp (0.5f, 18.0f, p.cKnee);
    const float cAttC = 1.0f - std::exp (-1.0f / (float) (xmap (p.cAtt, 0.3f, 60.0f) * 0.001 * sr));
    const float cRelC = 1.0f - std::exp (-1.0f / (float) (xmap (p.cRel, 40.0f, 1200.0f) * 0.001 * sr));
    const float makeDb = p.cMake * 24.0f;

    // limiter constants
    const float ceilAmp = std::pow (10.0f, lerp (-12.0f, -0.3f, p.limCeil) / 20.0f);
    const float lRelC = 1.0f - std::exp (-1.0f / (float) (xmap (p.limRel, 30.0f, 800.0f) * 0.001 * sr));
    const float lSm = 1.0f - std::exp (-1.0f / (float) (0.0006 * sr));

    const int os = (int) clampf (p.os, 0.0f, 2.0f);
    // heat latency by mode: 0, 15, or 15+8; pad the difference
    const int heatLat = os == 0 ? 0 : (os == 1 ? Half::roundTrip() : Half::roundTrip() + 8);
    const int padLen = heatPad - heatLat;

    for (int n = 0; n < numSamples; ++n)
    {
        float l = left[n] * inG;
        float r = right != nullptr ? right[n] * inG : l;

        // the fully-dry copy for MIX rides the whole latency
        ch[0].mix.push (l);
        ch[1].mix.push (r);

        // ---- HPF --------------------------------------------------------
        l = ch[0].hp2 (ch[0].hp1 (l));
        r = ch[1].hp2 (ch[1].hp1 (r));

        float m = 0.5f * (l + r);

        // ---- gate -------------------------------------------------------
        if (p.gateOn >= 0.5f)
        {
            const float env = gateFast.lp (std::abs (m));
            if (env > gThr) { gateGain += (1.0f - gateGain) * gAttC; gateHoldLeft = gHold; }
            else if (gateHoldLeft > 0) --gateHoldLeft;
            else gateGain += (0.0f - gateGain) * gRelC;
            l *= gateGain; r *= gateGain; m *= gateGain;
            gateGr += ((1.0f - gateGain) - gateGr) * 0.01f;
        }
        else { gateGain = 1.0f; gateGr *= 0.99f; }

        // ---- de-esser ---------------------------------------------------
        if (p.deEssOn >= 0.5f)
        {
            const float lowL = ch[0].ds2 (ch[0].ds1 (l));
            const float lowR = ch[1].ds2 (ch[1].ds1 (r));
            /*  Sibilance is judged by the RATIO of high band to everything,
                not by the high band alone — a singer getting louder is not
                an ess, and must not be ducked like one. */
            const float hiM = m - 0.5f * (lowL + lowR);
            const float eh = dsHf.lp (std::abs (hiM));
            const float ea = dsAll.lp (std::abs (m)) + 1.0e-6f;
            const float ratio = eh / ea;
            const float over = smoothstep (0.28f, 0.75f, ratio);
            const float target = std::pow (10.0f, -p.deEssAmt * 16.0f * over / 20.0f);
            dsGain += (target - dsGain) * (target < dsGain ? 0.25f : 0.015f);
            l = lowL + dsGain * (l - lowL);
            r = lowR + dsGain * (r - lowR);
            m = 0.5f * (l + r);
            deEssGr += ((1.0f - dsGain) - deEssGr) * 0.01f;
        }
        else { dsGain = 1.0f; deEssGr *= 0.99f; }

        inRms = inM.lp (m * m);

        // ---- the basket -------------------------------------------------
        ch[0].dry.push (l);
        ch[1].dry.push (r);
        const float dl = ch[0].dry.readInt (basketLatency);
        const float dr = ch[1].dry.readInt (basketLatency);

        float bl = dl, br = dr;

        if (basketOn)
        {
            pitch.pushSample (m);
            frame.push (m);
            if (frame.due())
            {
                frame.analyse (lamA, lamB);
                if (frame.valid)
                {
                    interpLeft = 64;
                    const float inv = 1.0f / 64.0f;
                    for (int i = 0; i < LPC_ORDER; ++i)
                    {
                        kAnaStep[(size_t) i]  = (frame.kAna[(size_t) i]  - kAnaNow[(size_t) i])  * inv;
                        kSynAStep[(size_t) i] = (frame.kSynA[(size_t) i] - kSynANow[(size_t) i]) * inv;
                        kSynBStep[(size_t) i] = (frame.kSynB[(size_t) i] - kSynBNow[(size_t) i]) * inv;
                    }
                    frameGainStep = (frame.gain - frameGain) * inv;
                }
            }
            if (interpLeft > 0)
            {
                --interpLeft;
                for (int i = 0; i < LPC_ORDER; ++i)
                {
                    kAnaNow[(size_t) i]  += kAnaStep[(size_t) i];
                    kSynANow[(size_t) i] += kSynAStep[(size_t) i];
                    kSynBNow[(size_t) i] += kSynBStep[(size_t) i];
                }
                frameGain += frameGainStep;
            }

            ana.k = kAnaNow;
            const float e = ana.analyse (m);
            const float fg = std::max (frameGain, 1.0e-5f);
            const float eN = clampf (e / fg, -12.0f, 12.0f);
            resLevel += (std::abs (eN) * fg - resLevel) * 0.001f;

            // ---- glottal epochs -----------------------------------------
            f0Hz = pitch.f0;
            clarity = pitch.clarity;
            const float voiced = smoothstep (0.35f, 0.6f, clarity);
            if (pitch.f0 > 20.0f && voiced > 0.2f)
                periodNow += ((float) (sr / (double) pitch.f0) - periodNow) * 0.05f;

            const float rect = epochLp.lp (std::abs (eN));
            const float floorv = epochFloor.lp (rect) + 1.0e-4f;
            ++sinceEpoch;
            bool fire = false;
            if ((float) sinceEpoch > 0.55f * periodNow
                && rect < ePrev && ePrev >= ePrev2
                && ePrev > 1.15f * floorv)
                fire = true;
            if ((float) sinceEpoch > 1.8f * periodNow) fire = true;   // free-run
            ePrev2 = ePrev; ePrev = rect;

            if (fire)
            {
                rackA.newPeriod (p.a, periodNow, pitch.f0, voiced);
                rackB.newPeriod (p.b, periodNow, pitch.f0, voiced);
                sinceEpoch = 0;
            }
            phase = clampf ((float) sinceEpoch / std::max (16.0f, periodNow), 0.0f, 1.0f);

            // ---- the racks ----------------------------------------------
            rackA.syn.k = kSynANow;
            rackB.syn.k = kSynBNow;
            float wet = 0.0f;
            if (p.a.level > 0.001f) wet += rackA.run (p.a, eN, phase, voiced, p.crisp, fg);
            if (p.b.level > 0.001f) wet += rackB.run (p.b, eN, phase, voiced, p.crisp, fg);

            // ---- level match --------------------------------------------
            const float dp = matchDryF.lp (0.5f * (dl * dl + dr * dr)) + 1.0e-9f;
            const float wp = matchWetF.lp (wet * wet) + 1.0e-9f;
            if (p.match >= 0.5f)
            {
                const float tgt = clampf (std::sqrt (dp / wp), 0.05f, 8.0f);
                matchGain += (tgt - matchGain) * 0.003f;
            }
            else matchGain += (1.0f - matchGain) * 0.003f;
            matchDb = 20.0f * std::log10 (std::max (1.0e-3f, matchGain));

            const float wetOut = wet * matchGain;
            bl = lerp (dl, wetOut, p.blend);
            br = lerp (dr, wetOut, p.blend);
        }

        // ---- heat -------------------------------------------------------
        {
            const float hIn = heatInF.lp (0.5f * (bl * bl + br * br)) + 1.0e-9f;
            float hl, hr;
            if (os == 0)
            {
                hl = heatShape (bl, ch[0]);
                hr = heatShape (br, ch[1]);
            }
            else if (os == 1)
            {
                float u0[2], u1[2];
                ch[0].up1.up (bl, u0);
                ch[1].up1.up (br, u1);
                for (int i = 0; i < 2; ++i)
                {
                    u0[i] = heatShape (u0[i], ch[0]);
                    u1[i] = heatShape (u1[i], ch[1]);
                }
                hl = ch[0].dn1.down (u0[0], u0[1]);
                hr = ch[1].dn1.down (u1[0], u1[1]);
            }
            else
            {
                float u0[2], u1[2];
                ch[0].up1.up (bl, u0);
                ch[1].up1.up (br, u1);
                for (int i = 0; i < 2; ++i)
                {
                    float q0[2], q1[2];
                    ch[0].up2.up (u0[i], q0);
                    ch[1].up2.up (u1[i], q1);
                    for (int j = 0; j < 2; ++j)
                    {
                        q0[j] = heatShape (q0[j], ch[0]);
                        q1[j] = heatShape (q1[j], ch[1]);
                    }
                    // one extra 2x-domain sample of hold makes the inner
                    // round trip an integer at the base rate
                    const float d0 = ch[0].dn2.down (q0[0], q0[1]);
                    const float d1 = ch[1].dn2.down (q1[0], q1[1]);
                    u0[i] = ch[0].os4Held; ch[0].os4Held = d0;
                    u1[i] = ch[1].os4Held; ch[1].os4Held = d1;
                }
                hl = ch[0].dn1.down (u0[0], u0[1]);
                hr = ch[1].dn1.down (u1[0], u1[1]);
            }

            hl = ch[0].heatDc (hl);
            hr = ch[1].heatDc (hr);

            // heat is a texture, not a fader: gain-match around it
            const float hOut = heatOutF.lp (0.5f * (hl * hl + hr * hr)) + 1.0e-9f;
            const float tgt = clampf (std::sqrt (hIn / hOut), 0.1f, 4.0f);
            heatGain += (tgt - heatGain) * 0.002f;
            hl *= heatGain;
            hr *= heatGain;

            // equalise the three modes' delay
            ch[0].pad.push (hl);
            ch[1].pad.push (hr);
            bl = ch[0].pad.readInt (padLen);
            br = ch[1].pad.readInt (padLen);
        }

        // ---- seasoning --------------------------------------------------
        if (p.eqOn >= 0.5f)
        {
            bl = ch[0].eqHi (ch[0].eqP2 (ch[0].eqP1 (ch[0].eqLo (bl))));
            br = ch[1].eqHi (ch[1].eqP2 (ch[1].eqP1 (ch[1].eqLo (br))));
        }

        // ---- compressor -------------------------------------------------
        if (p.compOn >= 0.5f)
        {
            const float d = scDet.lp (std::max (std::abs (bl), std::abs (br)));
            const float lvl = 20.0f * std::log10 (d + 1.0e-6f);

            float grDb = 0.0f;
            const float overDb = lvl - cThrDb;
            if (overDb > -cKneeDb * 0.5f)
            {
                if (overDb < cKneeDb * 0.5f)
                {
                    const float t = overDb + cKneeDb * 0.5f;
                    grDb = (1.0f / cRatio - 1.0f) * t * t / (2.0f * cKneeDb);
                }
                else grDb = (1.0f / cRatio - 1.0f) * overDb;
            }

            // two-stage smoothing reads as analogue: a fast stage does the
            // catching, a slower one does the breathing
            compG1 += (grDb - compG1) * (grDb < compG1 ? cAttC : cRelC);
            compG2 += (compG1 - compG2) * 0.12f;

            float mk = makeDb;
            if (p.cAuto >= 0.5f) mk += -autoMk.lp (compG2) * 0.8f;

            const float cg = std::pow (10.0f, (compG2 + mk) / 20.0f);
            bl *= cg; br *= cg;
            compGr += ((-compG2) - compGr) * 0.02f;
        }
        else { compG1 = compG2 = 0.0f; compGr *= 0.99f; }

        // ---- limiter ----------------------------------------------------
        ch[0].lim.push (bl);
        ch[1].lim.push (br);
        const float ll = ch[0].lim.readInt (limLen);
        const float lr = ch[1].lim.readInt (limLen);

        if (p.limOn >= 0.5f)
        {
            const float pk = std::max (std::abs (bl), std::abs (br));
            if (pk > limEnv) limEnv = pk;
            else limEnv += (pk - limEnv) * lRelC;

            const float want = limEnv > ceilAmp ? ceilAmp / limEnv : 1.0f;
            limG1 += (want - limG1) * lSm;
            limG2 += (limG1 - limG2) * lSm;

            bl = ll * limG2;
            br = lr * limG2;
            limGr += ((1.0f - limG2) - limGr) * 0.02f;
        }
        else
        {
            bl = ll; br = lr;
            limG1 = limG2 = 1.0f;
            limGr *= 0.99f;
        }

        // ---- out --------------------------------------------------------
        const float mixDl = ch[0].mix.readInt (reportedLatency);
        const float mixDr = ch[1].mix.readInt (reportedLatency);
        bl = lerp (mixDl, bl, p.mix) * outG;
        br = lerp (mixDr, br, p.mix) * outG;

        bl = ceilSoft (clean (bl), 1.02f);
        br = ceilSoft (clean (br), 1.02f);

        outRms = outM.lp (0.5f * (bl * bl + br * br));

        left[n] = bl;
        if (right != nullptr) right[n] = br;
    }

    if (basketOn)
    {
        orderA = rackA.subOrder();
        orderB = rackB.subOrder();
        gainsA = rackA.hist;
        gainsB = rackB.hist;
    }
}

} // namespace hf
