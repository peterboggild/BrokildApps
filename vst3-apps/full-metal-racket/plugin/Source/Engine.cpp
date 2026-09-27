#include "Engine.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>

namespace fmr
{

//==============================================================================
//  The channel table. Order is the tradition's order — every classic machine
//  reads the kit from the floor upward: kick, snare, toms low to high,
//  percussion, hats, cymbals. The 808 is AC BD SD LT MT HT RS/CL MA/CP CB CY
//  OH CH; we keep the two near-universal conventions (toms low to high, and
//  percussion between the toms and the metal) and drop ACCENT, which is per
//  step in a sequencer rather than a channel.
//==============================================================================
struct ChanDef
{
    const char* id;
    const char* name;
    int   fam;
    int   note;             // GM-ish default
    float tuneLo, tuneHi;   // Hz
    float decLo,  decHi;    // ms
    float defTune, defDecay, defTone, defSnap, defBend, defDrive, defLevel;
    int   defModel;
};

/*  LEVEL is squared before it reaches the sum, so these numbers move faster
    than they read: OH and the two cymbals came down 0.05/0.04/0.04 for about
    1.5 dB each. They sit high in the mix for a reason that is not their knob —
    they ring for seconds while a kick is gone in a tenth of one, so equal
    peaks are nowhere near equal loudness in a pattern. CH is deliberately not
    trimmed: it is short, and it was never the one that stuck out. */
static const ChanDef CHANS[NCH] =
{
    //  id     name       family      note  tune Hz     decay ms      tune  dec   tone  snap  bend  drv   lvl  model
    { "bd1", "BD 1",  FAM_KICK,  36,   30.f,  120.f,   40.f, 2200.f,  0.28f,0.46f,0.42f,0.34f,0.40f,0.10f,0.80f, 0 },
    { "bd2", "BD 2",  FAM_KICK,  35,   30.f,  120.f,   40.f, 2200.f,  0.16f,0.62f,0.30f,0.20f,0.55f,0.16f,0.72f, 2 },
    { "sd1", "SD 1",  FAM_SNARE, 38,  100.f,  420.f,   30.f,  900.f,  0.42f,0.34f,0.52f,0.52f,0.34f,0.08f,0.76f, 0 },
    { "sd2", "SD 2",  FAM_SNARE, 40,  100.f,  420.f,   30.f,  900.f,  0.60f,0.26f,0.62f,0.62f,0.58f,0.10f,0.66f, 1 },
    { "tm1", "TOM 1", FAM_TOM,   45,   60.f,  420.f,   80.f, 2000.f,  0.22f,0.46f,0.44f,0.30f,0.42f,0.10f,0.68f, 0 },
    { "tm2", "TOM 2", FAM_TOM,   47,   60.f,  420.f,   80.f, 2000.f,  0.38f,0.42f,0.46f,0.30f,0.42f,0.10f,0.68f, 0 },
    { "tm3", "TOM 3", FAM_TOM,   50,   60.f,  420.f,   80.f, 2000.f,  0.54f,0.38f,0.48f,0.30f,0.42f,0.10f,0.68f, 0 },
    { "fx",  "FX",    FAM_PERC,  37,  100.f, 2400.f,    5.f, 1600.f,  0.55f,0.24f,0.50f,0.60f,0.50f,0.08f,0.62f, 1 },
    { "ch",  "CH",    FAM_METAL, 42,  200.f, 1200.f,   10.f,  420.f,  0.44f,0.24f,0.58f,0.52f,0.50f,0.06f,0.62f, 0 },
    { "oh",  "OH",    FAM_METAL, 46,  200.f, 1200.f,   80.f, 3000.f,  0.44f,0.40f,0.58f,0.52f,0.50f,0.06f,0.53f, 0 },
    { "cy1", "CY 1",  FAM_METAL, 49,  200.f, 1200.f,  200.f, 8000.f,  0.30f,0.62f,0.46f,0.30f,0.62f,0.04f,0.48f, 0 },
    { "cy2", "CY 2",  FAM_METAL, 51,  200.f, 1200.f,  200.f, 8000.f,  0.52f,0.50f,0.54f,0.38f,0.46f,0.04f,0.46f, 1 }
};

const char* channelId     (int c) { return CHANS[c < 0 ? 0 : (c >= NCH ? NCH - 1 : c)].id; }
const char* channelName   (int c) { return CHANS[c < 0 ? 0 : (c >= NCH ? NCH - 1 : c)].name; }
int         channelFamily (int c) { return CHANS[c < 0 ? 0 : (c >= NCH ? NCH - 1 : c)].fam; }
int         defaultNote   (int c) { return CHANS[c < 0 ? 0 : (c >= NCH ? NCH - 1 : c)].note; }

const char* familyName (int f)
{
    static const char* N[] = { "KICK", "SNARE", "TOM", "PERC", "METAL" };
    return N[f < 0 ? 0 : (f > 4 ? 4 : f)];
}

// ---- model names, per family ------------------------------------------------
static const char* MODELS_KICK[]  = { "PING", "PUNCH", "MEMBRANE" };
static const char* MODELS_SNARE[] = { "SHELL", "WIRES", "CLAP" };
static const char* MODELS_TOM[]   = { "MEMBRANE", "CONGA", "BLOCK" };
static const char* MODELS_PERC[]  = { "BELL", "WOOD", "ZAP", "VOX" };
static const char* MODELS_METAL[] = { "CRASH", "RIDE", "SPLASH" };

static const char* const* familyModels (int fam, int& n)
{
    switch (fam)
    {
        case FAM_KICK:  n = 3; return MODELS_KICK;
        case FAM_SNARE: n = 3; return MODELS_SNARE;
        case FAM_TOM:   n = 3; return MODELS_TOM;
        case FAM_PERC:  n = 4; return MODELS_PERC;
        default:        n = 3; return MODELS_METAL;
    }
}

//==============================================================================
//  The parameter table, built once in a loop. 12 channels x 10 + 8 globals.
//==============================================================================
static const char* CP_NAME[NCP] =
{
    "MODEL", "TUNE", "DECAY", "TONE", "SNAP", "BEND", "DRIVE", "LEVEL", "PAN", "MUTE",
    "REBOUND", "KEY MODE"
};

static const char* GLOBAL_ID[NGP] =
{
    "volume", "os", "railsag", "age", "kittune", "hatlink", "bleed", "body",
    "morph", "seq", "swing", "feel", "grip", "tempo", "punch"
};
static const char* GLOBAL_NAME[NGP] =
{
    "MASTER VOLUME", "OVERSAMPLING", "RAIL SAG", "AGE", "KIT TUNE", "HAT LINK", "BLEED", "KIT BODY",
    "KIT MORPH", "SEQUENCER", "SWING", "FEEL", "GRIP", "TEMPO", "PUNCH"
};

static const char* OS_NAMES[] = { "1x", "2x", "4x" };

namespace
{
    struct Table
    {
        std::vector<PSpec> rows;
        std::vector<std::string> ids, names;

        Table()
        {
            const int total = NCH * NCP + NGP;
            ids.reserve ((size_t) total);
            names.reserve ((size_t) total);
            rows.reserve ((size_t) total);

            // pass 1 — own the strings so the const char* in PSpec stays valid
            for (int c = 0; c < NCH; ++c)
                for (int s = 0; s < NCP; ++s)
                {
                    ids.push_back (std::string (CHANS[c].id) + "_" + [&]{
                        static const char* SH[NCP] = { "model","tune","decay","tone","snap","bend","drive","level","pan","mute",
                                                       "rebound","key" };
                        return std::string (SH[s]); }());
                    names.push_back (std::string (CHANS[c].name) + " " + CP_NAME[s]);
                }
            for (int g = 0; g < NGP; ++g) { ids.push_back (GLOBAL_ID[g]); names.push_back (GLOBAL_NAME[g]); }

            // pass 2 — the rows
            int k = 0;
            for (int c = 0; c < NCH; ++c)
            {
                const ChanDef& d = CHANS[c];
                int nm = 0; familyModels (d.fam, nm);
                for (int s = 0; s < NCP; ++s, ++k)
                {
                    PSpec r { ids[(size_t) k].c_str(), names[(size_t) k].c_str(), 0.0f, KP_PCT, 0.0f, 0.0f, c, s };
                    switch (s)
                    {
                        case CP_MODEL: r.kind = KP_LIST;  r.hi = (float) (nm - 1); r.def = (float) d.defModel; break;
                        case CP_TUNE:  r.kind = KP_HZ;    r.lo = d.tuneLo; r.hi = d.tuneHi; r.def = d.defTune;  break;
                        case CP_DECAY: r.kind = KP_MS;    r.lo = d.decLo;  r.hi = d.decHi;  r.def = d.defDecay; break;
                        case CP_TONE:  r.def = d.defTone;  break;
                        case CP_SNAP:  r.def = d.defSnap;  break;
                        case CP_BEND:  r.def = d.defBend;  break;
                        case CP_DRIVE: r.def = d.defDrive; break;
                        case CP_LEVEL: r.def = d.defLevel; break;
                        case CP_PAN:   r.kind = KP_BIPOL; r.def = 0.5f; break;
                        case CP_MUTE:  r.kind = KP_SW;    r.def = 0.0f; break;
                        case CP_REBOUND: r.def = 0.0f; break;
                        case CP_KEY:   r.kind = KP_SW;    r.def = 0.0f; break;
                        default: break;
                    }
                    rows.push_back (r);
                }
            }
            for (int g = 0; g < NGP; ++g, ++k)
            {
                PSpec r { ids[(size_t) k].c_str(), names[(size_t) k].c_str(), 0.5f, KP_PCT, 0.0f, 0.0f, -1, g };
                switch (g)
                {
                    case GP_VOLUME:  r.kind = KP_VOL;  r.def = 0.72f; break;
                    case GP_OS:      r.kind = KP_LIST; r.hi = 2.0f; r.def = 1.0f; break;
                    case GP_SAG:     r.def = 0.22f; break;
                    case GP_AGE:     r.def = 0.30f; break;
                    case GP_KITTUNE: r.kind = KP_SEMI; r.lo = 24.0f; r.def = 0.5f; break;
                    case GP_HATLINK: r.kind = KP_SW;  r.def = 1.0f; break;
                    case GP_BLEED:   r.def = 0.26f; break;
                    case GP_BODY:    r.def = 0.20f; break;
                    case GP_MORPH:   r.def = 0.0f;  break;
                    case GP_SEQ:     r.kind = KP_SW; r.def = 0.0f; break;
                    case GP_SWING:   r.def = 0.5f;  break;      // 50 % = straight
                    case GP_FEEL:    r.def = 0.0f;  break;
                    case GP_GRIP:    r.def = 0.0f;  break;
                    case GP_TEMPO:   r.def = 0.28f; break;      // ~120 BPM on the internal clock
                    case GP_PUNCH:   r.def = 0.0f;  break;      // exactly absent by default
                    default: break;
                }
                rows.push_back (r);
            }
        }
    };

    const Table& table() { static Table t; return t; }
}

int          numParams()          { return (int) table().rows.size(); }
const PSpec& paramSpec (int i)    { const auto& r = table().rows;
                                    return r[(size_t) (i < 0 ? 0 : (i >= (int) r.size() ? (int) r.size() - 1 : i))]; }
int          paramIndex (const char* id)
{
    const auto& r = table().rows;
    for (size_t i = 0; i < r.size(); ++i) if (std::strcmp (r[i].id, id) == 0) return (int) i;
    return -1;
}
float paramMax (const PSpec& s) { return s.kind == KP_LIST ? s.hi : (s.kind == KP_SW ? 1.0f : 1.0f); }

const char* const* listNames (const char* id, int& count)
{
    count = 0;
    if (std::strcmp (id, "os") == 0) { count = 3; return OS_NAMES; }
    const int i = paramIndex (id);
    if (i < 0) return nullptr;
    const PSpec& s = paramSpec (i);
    if (s.chan < 0 || s.slot != CP_MODEL) return nullptr;
    return familyModels (CHANS[s.chan].fam, count);
}

Params::Params()
{
    std::memset (ch, 0, sizeof (ch));
    std::memset (g,  0, sizeof (g));
    for (int i = 0; i < numParams(); ++i)
    {
        const PSpec& s = paramSpec (i);
        pvalue (*this, s) = s.def;
    }
}

//==============================================================================
//  BLOCK D — the metal cluster.
//
//  The 808's six oscillators sit at 205.3 / 304.4 / 369.6 / 522.7 / 540 /
//  800 Hz; as ratios of the lowest that is the middle row. SPREAD walks from
//  a mild near-harmonic cluster (which sounds like a bell) through the 808
//  to something wider and more hostile.
//==============================================================================
static const float MET_MILD[NMETAL] = { 1.000f, 1.500f, 2.000f, 2.667f, 3.500f, 4.500f };
static const float MET_808 [NMETAL] = { 1.000f, 1.483f, 1.800f, 2.546f, 2.630f, 3.897f };
static const float MET_WILD[NMETAL] = { 1.000f, 1.387f, 2.093f, 2.897f, 3.769f, 5.187f };

static void metalRatios (float spread, float* out)
{
    const float t = clamp01 (spread);
    for (int i = 0; i < NMETAL; ++i)
        out[i] = t < 0.5f ? lerpf (MET_MILD[i], MET_808[i],  t * 2.0f)
                          : lerpf (MET_808[i],  MET_WILD[i], (t - 0.5f) * 2.0f);
}

//  BLOCK C — mode ratios. Harmonic at 0, the ideal circular membrane at 0.5
//  (1 : 1.593 : 2.135), clangorous at 1.
static void modeRatios (float mode, float* out)
{
    static const float HARM[NRES] = { 1.0f, 2.000f, 3.000f };
    static const float MEMB[NRES] = { 1.0f, 1.593f, 2.135f };
    static const float CLNG[NRES] = { 1.0f, 2.414f, 3.889f };
    const float t = clamp01 (mode);
    for (int i = 0; i < NRES; ++i)
        out[i] = t < 0.5f ? lerpf (HARM[i], MEMB[i], t * 2.0f)
                          : lerpf (MEMB[i], CLNG[i], (t - 0.5f) * 2.0f);
}

//  Q from a decay time: an underdamped resonator falls as exp(-pi f t / Q),
//  so 60 dB takes 6.908 Q / (pi f) seconds.
static float qFromDecay (float decaySec, float f0)
{
    return clampf (decaySec * (float) M_PI * f0 / 6.908f, 0.6f, 3000.0f);
}

/*  The nonlinear damping coefficient, from a 0..3 CHARACTER and the
    resonator's Q — and it has to be derived, not dialled.

    The drag term adds nl*|bp| to the resonator's damping, while the linear
    damping is 1/Q. So a fixed nl means the nonlinearity is negligible on a
    short decay and utterly dominant on a long one. The first version used a
    flat 3.0: at Q=21 the drag was six times the linear damping, which made
    a 1.2-second kick die in a quarter of a second and took most of its level
    with it. Scaling by 1/Q makes CHARACTER mean the same thing at every
    tuning and every decay: 1.0 is "as much again as the linear damping, at
    the peak of the hit". */
static float nlFor (float character, float q, float velL = 1.0f)
{
    /*  refAmp follows the velocity, but only part of the way. Follow it all
        the way and every hit has identical character, which is dead; follow it
        not at all and a hard hit is squashed back down to the level of a soft
        one, which kills accents. Partway: a hard hit really is more squashed,
        by about 1.7x rather than 2.7x. */
    const float refAmp = 0.26f * (0.45f + 0.55f * clampf (velL, 0.05f, 1.4f));
    return character / (clampf (q, 0.6f, 3000.0f) * refAmp);
}

//==============================================================================
void Voice::reset()
{
    for (auto& r : res) r.reset();
    for (auto& b : band) b.reset();
    for (auto& w : wire) w.reset();
    tone.reset(); hp.reset(); clickF.reset();
    amp.reset(); clickE.reset(); noiseE.reset(); wireE.reset();
    for (auto& e : bandE) e.reset();
    for (auto& m : met) m.reset();
    active = false; age = 0; peak = 0.0f; quiet = 0;
    excLeft = 0.0f; pitchV = 0.0f; clapLeft = 0.0f; clapStage = 0;
}

//==============================================================================
void Engine::prepare (double sampleRate, int maxBlock)
{
    fs = sampleRate > 8000.0 ? sampleRate : 48000.0;
    maxBlockSize = maxBlock > 0 ? maxBlock : 512;
    osSel = -1;
    updateOs();

    dec1L.design(); dec1R.design(); dec2L.design(); dec2R.design();
    grng.seed (0xC0FFEEu);

    // AGE tolerances: fixed per instance, so two instances are never identical
    Rng t; t.seed (0x5EEDu);
    for (int c = 0; c < NCH; ++c) { tol[(size_t) c] = t.bi(); tolDrift[(size_t) c] = 0.0f; }

    reset();
}

void Engine::updateOs()
{
    const int sel = (int) std::lround (clampf (p.g[GP_OS], 0.0f, 2.0f));
    if (sel == osSel) return;
    osSel = sel;
    osFactor = sel == 0 ? 1 : (sel == 1 ? 2 : 4);
    osFs = fs * (double) osFactor;
    dec1L.reset(); dec1R.reset(); dec2L.reset(); dec2R.reset();
}

void Engine::reset()
{
    for (auto& chv : voices) for (auto& v : chv) v.reset();
    for (auto& s : symp) s.reset();
    for (auto& b : body) b.reset();
    bodyTone.reset();
    dec1L.reset(); dec1R.reset(); dec2L.reset(); dec2R.reset();
    for (auto& h : auxA) h.reset();
    for (auto& h : auxB) h.reset();
    rr.fill (0); meter.fill (0.0f); sympExc.fill (0.0f);
    nHits = 0; uiStep = -1; uiLaneStep.fill (-1);
    for (auto& l : lastVel) l = 0.0f;
    rail = 1.0f; railEnv = 0.0f;
    masterDcL = masterDcR = masterDcXL = masterDcXR = 0.0f;
    wmPhase = 0.0f;
}

void Engine::allNotesOff()
{
    for (auto& chv : voices) for (auto& v : chv) v.reset();
    for (auto& s : symp) s.reset();
    meter.fill (0.0f);
}

void Engine::setWorldMod (float detCents, float panSpread, float tremDepth,
                          float tremRate, float pitchSag, float filterMul)
{
    wmIn[0].store (detCents);  wmIn[1].store (panSpread); wmIn[2].store (tremDepth);
    wmIn[3].store (tremRate);  wmIn[4].store (pitchSag);  wmIn[5].store (filterMul);
}

//==============================================================================
void Engine::noteOn (int note, float velocity)
{
    /*  KEY MODE: a channel switched to chromatic play answers a WHOLE RANGE
        of notes rather than its one mapped note, taking its pitch from the
        distance to that note. A tuned kick as a bassline is the most-used
        trick in modern music and almost nothing does it properly — which
        here means the decay scales with pitch too, so a low note rings
        longer, the way a real drum does. */
    /*  The exact map wins. A KEY MODE channel answers a two-octave range, and
        those ranges overlap the GM map — with KEY on for BD1 (note 36) its
        range covered 12..60, which swallowed the snare at 38 and the closed
        hat at 42 and silenced most of the kit. So: a note that belongs to a
        channel goes to that channel, and KEY MODE gets everything else. */
    for (int c = 0; c < NCH; ++c)
        if (CHANS[c].note == note) { trigger (c, velocity); return; }

    for (int c = 0; c < NCH; ++c)
        if (p.ch[c][CP_KEY] >= 0.5f)
        {
            const int root = CHANS[c].note;
            if (note >= root - 24 && note <= root + 24)
            {
                trigger (c, velocity, (float) (note - root));
                return;
            }
        }

    /*  A chromatic octave from middle C also plays the twelve, in panel
        order. Not a substitute for an editable map — it is so the machine can
        be played from any keyboard the moment it loads. */
    if (note >= 60 && note < 60 + NCH) trigger (note - 60, velocity);
}

//==============================================================================
/*  MORPH. A union of two whole kits: every continuous value interpolates,
    every switched one steps at its own threshold so the change does not all
    happen at once, and a kit that was never captured means there is nothing
    to morph toward — the fader simply does nothing rather than sweeping to
    silence. */
/*  What belongs to a KIT, and therefore morphs.
    Everything on a channel except its mute, plus the five globals the kit
    generator writes — RAIL SAG, BLEED, KIT BODY, AGE and KIT TUNE. Leaving
    those out was the bug: a morph between two seeds moved the twelve voices
    and left the machine's character exactly where it was, which is most of
    what makes two kits sound unalike.

    Deliberately NOT morphed: the master, the oversampling, and the whole
    transport — those belong to the performance, not to the sound. The same
    list applyKitIndex leaves alone, and it should stay the same list. */
bool morphable (const PSpec& s)
{
    if (s.chan >= 0) return s.slot != CP_MUTE;
    switch (s.slot)
    {
        case GP_SAG: case GP_BLEED: case GP_BODY: case GP_AGE: case GP_KITTUNE: return true;
        default: return false;
    }
}

void Engine::applyMorph (Params& dst) const
{
    const float t = clamp01 (dst.g[GP_MORPH]);
    if (! haveA || ! haveB || t <= 0.0f) return;

    for (int i = 0; i < numParams(); ++i)
    {
        const PSpec& s = paramSpec (i);
        if (! morphable (s)) continue;
        const float a = pvalue (const_cast<Params&> (kitA), s);
        const float b = pvalue (const_cast<Params&> (kitB), s);
        float& d = pvalue (dst, s);
        /*  CONTINUOUS ONLY. A stepped choice — a kick model, a channel
            switch — stays wherever it was last set. It used to flip at a
            staggered threshold so that the kit changed character rather than
            cross-fading; the cost was that a fader you would automate over
            eight bars hid a dozen cliffs inside it, which reads as a defect
            however it was intended. */
        if (s.kind == KP_LIST || s.kind == KP_SW) continue;
        d = lerpf (a, b, t);
    }
}

/*  THE WEB, prototype form. A fixed, musically sensible matrix: what a hit on
    one drum shakes in the others. The excitation goes into that channel's
    SYMPATHETIC resonator, not into a voice — it rings the shell without
    firing an envelope, which is the whole point. */
static const float BLEED[NCH][NCH] =
{
//         bd1   bd2   sd1   sd2   tm1   tm2   tm3   fx    ch    oh    cy1   cy2
/*bd1*/ { 0.00f,0.30f,0.34f,0.22f,0.40f,0.34f,0.26f,0.06f,0.20f,0.24f,0.14f,0.12f },
/*bd2*/ { 0.30f,0.00f,0.30f,0.20f,0.36f,0.30f,0.24f,0.06f,0.18f,0.22f,0.12f,0.10f },
/*sd1*/ { 0.14f,0.12f,0.00f,0.44f,0.46f,0.42f,0.38f,0.10f,0.26f,0.30f,0.22f,0.18f },
/*sd2*/ { 0.12f,0.10f,0.44f,0.00f,0.42f,0.40f,0.36f,0.10f,0.24f,0.28f,0.20f,0.16f },
/*tm1*/ { 0.10f,0.10f,0.34f,0.30f,0.00f,0.36f,0.30f,0.06f,0.14f,0.18f,0.14f,0.12f },
/*tm2*/ { 0.08f,0.08f,0.32f,0.28f,0.36f,0.00f,0.34f,0.06f,0.14f,0.18f,0.14f,0.12f },
/*tm3*/ { 0.08f,0.08f,0.30f,0.26f,0.30f,0.34f,0.00f,0.06f,0.14f,0.18f,0.14f,0.12f },
/*fx */ { 0.04f,0.04f,0.12f,0.10f,0.10f,0.10f,0.10f,0.00f,0.08f,0.10f,0.08f,0.08f },
/*ch */ { 0.02f,0.02f,0.06f,0.06f,0.06f,0.06f,0.06f,0.04f,0.00f,0.34f,0.10f,0.10f },
/*oh */ { 0.03f,0.03f,0.08f,0.08f,0.08f,0.08f,0.08f,0.04f,0.30f,0.00f,0.12f,0.12f },
/*cy1*/ { 0.04f,0.04f,0.12f,0.10f,0.12f,0.12f,0.12f,0.06f,0.16f,0.18f,0.00f,0.26f },
/*cy2*/ { 0.04f,0.04f,0.12f,0.10f,0.12f,0.12f,0.12f,0.06f,0.16f,0.18f,0.26f,0.00f }
};

void Engine::scheduleHit (int chan, float vel, float key, int atSamples, bool allowRebound)
{
    if (nHits >= MAXHITS) return;
    hits[(size_t) nHits++] = { chan, vel, key, atSamples, allowRebound };
}

void Engine::fireDue (int sampleInBlock)
{
    for (int i = 0; i < nHits; )
    {
        if (hits[(size_t) i].at <= sampleInBlock)
        {
            const Hit h = hits[(size_t) i];
            hits[(size_t) i] = hits[(size_t) --nHits];
            trigger (h.chan, h.vel, h.key, h.rb);
        }
        else ++i;
    }
}

void Engine::trigger (int chan, float velocity, float keySemis, bool allowRebound)
{
    if (chan < 0 || chan >= NCH) return;
    const float vel = clampf (velocity, 0.01f, 1.0f);

    /*  RECORD. A played hit is written into the current pattern at the
        NEAREST step of that lane, not the one it has just passed — quantising
        backwards makes everything you play sound late.

        The write is from the audio thread while the panel may be reading the
        pattern to draw it. A Step is six plain bytes, so the worst case is
        one frame of the panel showing a half-written velocity, which is not
        worth a lock in the trigger path. */
    if (recArm.load (std::memory_order_relaxed) && p.g[GP_SEQ] >= 0.5f && allowRebound)
    {
        const int cur = uiLaneStep[(size_t) chan];
        if (cur >= 0)
        {
            Lane& L = pat[curPat < 0 ? 0 : (curPat >= NPAT ? NPAT - 1 : curPat)].lane[(size_t) chan];
            const int len = L.len < 1 ? 1 : L.len;
            int k = cur + (uiLaneFrac[(size_t) chan] > 0.5 ? 1 : 0);
            if (k >= len) k -= len;
            if (k >= 0 && k < NSTEP)
            {
                L.step[k].on = 1;
                L.step[k].vel = (uint8_t) clampf (vel * 127.0f, 1.0f, 127.0f);
            }
        }
    }

    /*  REBOUND — a stick does not strike once and stop. It bounces, and the
        bounces get closer together and quieter, because each one starts from
        a lower height. Low is a flam, middling is a drag, high is a buzz
        roll: one control, three techniques that are otherwise a nightmare to
        program. The gaps follow the physical law (time between bounces is
        proportional to the square root of the height) rather than a taper
        picked by ear. */
    const float rb = allowRebound ? clamp01 (p.ch[chan][CP_REBOUND]) : 0.0f;
    if (rb > 0.005f && velocity > 0.0f)
    {
        const float restitution = 0.30f + 0.62f * rb;        // how lively the stick is
        const int   maxB = 1 + (int) (rb * 11.0f);
        float gap = (float) ((0.130f - 0.105f * rb) * osFs); // the first gap, in samples
        float v = vel;
        int   at = 0;
        for (int b = 0; b < maxB; ++b)
        {
            v *= restitution;
            if (v < 0.012f) break;
            at += (int) gap;
            if (at > (int) (osFs * 2.0)) break;
            scheduleHit (chan, v, keySemis, at, false);   // a bounce does not bounce again
            gap *= std::sqrt (restitution);                  // closer together each time
            if (gap < osFs * 0.004f) gap = (float) (osFs * 0.004f);
        }
    }

    // the hat choke: a closed hat stops an open one, always
    if (chan == 8)
        for (auto& v : voices[9])
            if (v.active) v.amp.dec = std::exp (-1.0f / (float) (0.004 * osFs));

    Voice& v = voices[(size_t) chan][(size_t) rr[(size_t) chan]];
    rr[(size_t) chan] = (rr[(size_t) chan] + 1) % VOICES;
    setupVoice (v, chan, vel, keySemis);

    // ring the other shells. Not a one-sample impulse — a short pulse, the
    // way one drum's air actually pushes another's head.
    const float bl = clamp01 (p.g[GP_BLEED]);
    if (bl > 0.0005f)
        for (int o = 0; o < NCH; ++o)
            if (o != chan) sympExc[(size_t) o] += bl * BLEED[chan][o] * vel * 2.4f;

    // the rail dips when something hits it
    railEnv += vel * 0.9f;
}

//==============================================================================
/*  Build one hit. Everything model-specific happens here, once per trigger —
    the inner loop stays a straight run through the blocks. */
void Engine::setupVoice (Voice& v, int c, float vel, float keySemis)
{
    const ChanDef& d = CHANS[c];
    const float* P = p.ch[c];
    const float fsv = (float) osFs;

    v.reset();
    v.active = true;
    v.fam = d.fam;
    v.vel = vel;
    v.rng.seed ((uint32_t) (0x1234567u + c * 7919u + (uint32_t) (vel * 100000.0f)));

    int nm = 0; familyModels (d.fam, nm);
    v.model = (int) std::lround (clampf (P[CP_MODEL], 0.0f, (float) (nm - 1)));

    // ---- AGE: component tolerance, drifting slowly -------------------------
    const float age = clamp01 (p.g[GP_AGE]);
    const float tolT = tol[(size_t) c] + tolDrift[(size_t) c];
    const float tuneTol  = 1.0f + age * tolT * 0.022f;
    const float decayTol = 1.0f + age * tolT * 0.10f;
    const float lvlTol   = 1.0f + age * tolT * 0.06f;

    // ---- tuning ------------------------------------------------------------
    float tune = P[CP_TUNE];
    /*  KIT TUNE is a transpose of the WHOLE kit, in semitones. It used to
        shift the three toms' normalised tune and nothing else, which made a
        control named for the machine move a quarter of it. */
    const float kitSemis = (p.g[GP_KITTUNE] - 0.5f) * 24.0f;

    // OH follows CH when linked — one pair of cymbals, not two instruments
    if (c == 9 && p.g[GP_HATLINK] >= 0.5f)
    {
        const float* Q = p.ch[8];
        tune = Q[CP_TUNE];
        v.model = (int) std::lround (clampf (Q[CP_MODEL], 0.0f, (float) (nm - 1)));
    }

    float f0 = xmap (tune, d.tuneLo, d.tuneHi) * tuneTol;
    if (kitSemis != 0.0f) f0 *= std::pow (2.0f, kitSemis / 12.0f);
    if (wmActive) f0 *= std::pow (2.0f, wmDet / 1200.0f);
    v.keySemis = keySemis;
    if (keySemis != 0.0f) f0 *= std::pow (2.0f, keySemis / 12.0f);
    v.f0 = f0;

    /*  Decay scales with pitch under KEY MODE: an octave down rings about
        60 % longer, which is roughly what a bigger drum does and is the
        difference between a playable tuned kick and a chirp. */
    float keyDecay = 1.0f;
    if (keySemis != 0.0f) keyDecay = std::pow (2.0f, -keySemis / 12.0f * 0.68f);
    const float decMs = xmap (P[CP_DECAY], d.decLo, d.decHi) * decayTol * keyDecay;
    const float decS  = decMs * 0.001f;
    const float tone  = clamp01 (P[CP_TONE]);
    const float snap  = clamp01 (P[CP_SNAP]);
    const float bend  = clamp01 (P[CP_BEND]);

    v.drive = clamp01 (P[CP_DRIVE]);
    v.tension = 0.0f;
    v.couple  = 0.0f;
    v.nl      = 0.0f;
    v.shape   = 0.0f;
    v.excAmp  = 0.0f;
    v.famGain = 1.0f;
    v.pitchEnv = 0.0f;
    v.noiseAmt = 0.0f; v.clickAmt = 0.0f; v.wireAmt = 0.0f; v.wireThresh = 0.0f;
    for (int i = 0; i < NRES; ++i) { v.ratio[i] = 1.0f; v.resGain[i] = 0.0f; }
    for (int i = 0; i < NBAND; ++i) v.bandMix[i] = 0.0f;

    /*  Velocity: louder, brighter, and slightly sharper. All three, because
        that is what a drum does; one of them is not enough.

        The CURVE matters as much as the depth. A linear map with a 0.18 floor
        gave barely 5 dB from a ghost note to a full hit — no use at all to
        anyone programming a beat, and the nonlinear damping then squashed
        what little was left. A gentle power curve off a low floor gives about
        20 dB before the drag has its say, and about 15 dB after. */
    const float velC = std::pow (clampf (vel, 0.02f, 1.4f), 1.35f);
    const float velB = 0.50f + 0.50f * velC;                // brightness
    const float velL = 0.05f + 0.95f * velC;                // level
    v.amp.reset();

    // ---- the excitation (BLOCK F) -----------------------------------------
    // pulse width from SNAP; a wide pulse feeds the fundamental, a narrow one
    // the whole spectrum. This IS the attack.
    const float pulseMs = lerpf (2.6f, 0.16f, snap);
    v.excDec = std::exp (-1.0f / clampf (pulseMs * 0.001f * fsv, 1.0f, 1e6f));

    switch (d.fam)
    {
        //----------------------------------------------------------------------
        case FAM_KICK:
        {
            if (v.model == 0)               // PING — the twin-T. No VCA at all:
            {                               // DECAY is the resonator's own Q.
                v.q = qFromDecay (decS, f0);
                v.resGain[0] = 1.0f;
                v.ratio[0] = 1.0f;
                v.nl = nlFor (0.7f + tone * 1.6f, v.q, velL);
                v.pitchEnv = bend * 0.55f;
                v.pitchDec = std::exp (-1.0f / clampf (0.030f * fsv, 1.0f, 1e6f));
                v.clickAmt = snap * 0.55f;
                v.famGain = 1.55f;
                v.amp.trigger (1.0f, 1.0f, decS * fsv * 3.0f);   // a slow safety net only
            }
            else if (v.model == 1)          // PUNCH — oscillator, pitch env, VCA
            {
                v.q = 0.0f;
                v.met[0].reset (0.0);
                v.met[0].setF (f0, fsv);
                v.pitchEnv = 0.35f + bend * 2.2f;
                v.pitchDec = std::exp (-1.0f / clampf (lerpf (0.012f, 0.055f, bend) * fsv, 1.0f, 1e6f));
                v.clickAmt = snap * 0.85f;
                v.shape = tone;                                  // waveshaper: no resonator here
                v.famGain = 0.86f;
                v.amp.trigger (1.0f, 1.0f, decS * fsv * 0.34f);
            }
            else                            // MEMBRANE — coupled modes + tension
            {
                float mr[NRES]; modeRatios (0.42f + tone * 0.30f, mr);
                v.q = qFromDecay (decS, f0);
                for (int i = 0; i < NRES; ++i)
                {
                    v.ratio[i]   = mr[i];
                    v.resGain[i] = i == 0 ? 1.0f : (i == 1 ? 0.34f : 0.16f);
                }
                v.couple  = 0.06f;
                v.tension = bend * 0.55f;
                v.nl = nlFor (0.9f + tone * 1.7f, v.q, velL);
                v.clickAmt = snap * 0.45f;
                v.famGain = 1.75f;
                v.amp.trigger (1.0f, 1.0f, decS * fsv * 3.0f);
            }
            v.excAmp = 0.85f * velL;
            v.tone.setLP (clampf (lerpf (400.0f, 5200.0f, tone) * velB, 60.0f, fsv * 0.45f), 0.72f, fsv);
            v.clickF.setHP (clampf (lerpf (700.0f, 4200.0f, snap), 60.0f, fsv * 0.45f), 0.7f, fsv);
            v.clickE.trigger (v.clickAmt * velL, 1.0f, clampf (lerpf (0.0016f, 0.0065f, tone) * fsv, 2.0f, 1e6f));
            break;
        }

        //----------------------------------------------------------------------
        case FAM_SNARE:
        {
            if (v.model == 2)               // CLAP
            {
                v.q = 0.0f;
                v.clapStage = 3;
                v.clapGap = clampf (lerpf (0.007f, 0.020f, bend) * fsv, 8.0f, 1e6f);
                v.clapLeft = v.clapGap;
                v.noiseAmt = 1.0f;
                v.noiseE.trigger (velL, 1.0f, clampf (0.0032f * fsv, 2.0f, 1e6f));
                v.noiseF.setBP (clampf (lerpf (700.0f, 2600.0f, tone) * velB, 100.0f, fsv * 0.45f), 0.85f, fsv);
                v.wireE.trigger (velL * 0.55f, 1.0f, clampf (decS * fsv * 0.9f, 4.0f, 1e6f));   // the tail
                v.famGain = 1.45f;
                v.amp.trigger (1.0f, 1.0f, clampf (decS * fsv * 1.6f, 4.0f, 1e6f));
            }
            else
            {
                // the shell: two bodies about a fifth apart
                v.q = qFromDecay (decS * 0.55f, f0);
                v.ratio[0] = 1.0f;   v.resGain[0] = 1.0f;
                v.ratio[1] = 1.497f; v.resGain[1] = 0.62f;
                v.couple = 0.05f;
                v.nl = nlFor (0.75f, v.q, velL);   // the snare is where accents matter most
                v.pitchEnv = 0.10f;
                v.pitchDec = std::exp (-1.0f / clampf (0.014f * fsv, 1.0f, 1e6f));
                v.noiseAmt = snap * 1.25f;
                v.noiseE.trigger (velL, 1.0f, clampf (decS * fsv * 0.75f, 4.0f, 1e6f));
                v.noiseF.setBP (clampf (lerpf (900.0f, 5200.0f, tone) * velB, 120.0f, fsv * 0.45f), 0.55f, fsv);
                v.famGain = 1.40f;
                v.amp.trigger (1.0f, 1.0f, decS * fsv * 3.0f);

                if (v.model == 1)           // WIRES — the snare bed, done properly
                {
                    v.wireAmt = 0.35f + bend * 1.15f;
                    /*  The wires only rattle above a level: a real snare's ghost
                        notes stop buzzing on their own, and that is most of why
                        they sound like ghost notes. */
                    v.wireThresh = lerpf (0.30f, 0.02f, bend) * (0.35f + 0.65f * vel);
                    static const float WF[NWIRE] = { 1400.f, 2300.f, 3700.f, 5300.f, 7100.f };
                    for (int i = 0; i < NWIRE; ++i)
                    {
                        v.wire[i].setBP (clampf (WF[i] * (0.85f + 0.3f * tone) * velB, 100.0f, fsv * 0.45f), 5.5f, fsv);
                        v.bandMix[i % NBAND] = 1.0f;
                    }
                    v.wireE.trigger (velL, 1.0f, clampf (decS * fsv * 1.05f, 4.0f, 1e6f));
                }
                else                        // SHELL — a plainer rattle
                {
                    v.wireAmt = bend * 0.55f;
                    v.wireThresh = 0.02f;
                    static const float WF[NWIRE] = { 1900.f, 3100.f, 4600.f, 6200.f, 8100.f };
                    for (int i = 0; i < NWIRE; ++i)
                        v.wire[i].setBP (clampf (WF[i] * velB, 100.0f, fsv * 0.45f), 2.6f, fsv);
                    v.wireE.trigger (velL, 1.0f, clampf (decS * fsv * 0.6f, 4.0f, 1e6f));
                }
            }
            v.excAmp = 0.80f * velL;
            v.clickAmt = 0.30f * snap;
            v.clickF.setHP (clampf (2600.0f, 60.0f, fsv * 0.45f), 0.7f, fsv);
            v.clickE.trigger (v.clickAmt * velL, 1.0f, clampf (0.0022f * fsv, 2.0f, 1e6f));
            v.hp.setHP (clampf (lerpf (90.0f, 260.0f, 1.0f - tone), 40.0f, fsv * 0.45f), 0.7f, fsv);
            break;
        }

        //----------------------------------------------------------------------
        case FAM_TOM:
        {
            float mr[NRES];
            const float modeSel = v.model == 0 ? 0.5f : (v.model == 1 ? 0.28f : 0.82f);
            modeRatios (modeSel, mr);
            const float decScale = v.model == 0 ? 1.0f : (v.model == 1 ? 0.55f : 0.16f);
            v.q = qFromDecay (decS * decScale, f0);
            for (int i = 0; i < NRES; ++i)
            {
                v.ratio[i] = mr[i];
                // higher modes damp fastest — the physical truth, and the
                // difference between a drum and a chord
                v.resGain[i] = i == 0 ? 1.0f : (i == 1 ? 0.40f : 0.18f);
            }
            v.couple  = 0.07f;
            v.tension = bend * (v.model == 2 ? 0.25f : 0.85f);
            v.nl = nlFor (0.8f + tone * 1.5f, v.q, velL);
            v.pitchEnv = 0.10f;
            v.pitchDec = std::exp (-1.0f / clampf (0.030f * fsv, 1.0f, 1e6f));
            v.clickAmt = snap * (v.model == 2 ? 1.35f : 0.60f);
            v.noiseAmt = snap * 0.28f;                         // skin
            v.noiseE.trigger (velL, 1.0f, clampf (0.0060f * fsv, 2.0f, 1e6f));
            v.noiseF.setBP (clampf (lerpf (1400.0f, 6000.0f, snap) * velB, 120.0f, fsv * 0.45f), 0.6f, fsv);
            v.excAmp = 0.85f * velL;
            v.famGain = 1.55f;
            v.tone.setLP (clampf (lerpf (700.0f, 7000.0f, tone) * velB, 80.0f, fsv * 0.45f), 0.7f, fsv);
            v.clickF.setHP (clampf (lerpf (1200.0f, 5200.0f, snap), 60.0f, fsv * 0.45f), 0.7f, fsv);
            v.clickE.trigger (v.clickAmt * velL, 1.0f, clampf (0.0022f * fsv, 2.0f, 1e6f));
            v.amp.trigger (1.0f, 1.0f, decS * fsv * 3.0f);
            break;
        }

        //----------------------------------------------------------------------
        case FAM_PERC:
        {
            if (v.model == 0)               // BELL — the metal core, high and long
            {
                float mr[NMETAL]; metalRatios (0.35f + bend * 0.5f, mr);
                for (int i = 0; i < NMETAL; ++i) { v.met[i].reset (v.rng.uni()); v.met[i].setF (f0 * mr[i], fsv); }
                v.q = 0.0f;
                for (int b = 0; b < NBAND; ++b)
                {
                    // derived from f0, so the kit transpose is already in them
                    static const float BF[NBAND] = { 2.0f, 4.4f, 8.4f };
                    v.band[b].setBP (clampf (f0 * BF[b] * (0.6f + tone), 200.0f, fsv * 0.45f), 2.2f, fsv);
                    v.bandE[b].trigger (velL, 1.0f, clampf (decS * fsv * (b == 0 ? 1.3f : b == 1 ? 1.0f : 0.6f), 4.0f, 1e6f));
                    v.bandMix[b] = 1.0f;
                }
                v.hp.setHP (clampf (lerpf (600.0f, 3000.0f, snap), 60.0f, fsv * 0.45f), 0.7f, fsv);
                v.famGain = 1.60f;
                v.amp.trigger (1.0f, 1.0f, decS * fsv * 2.2f);
            }
            else if (v.model == 1)          // WOOD — a very short high-Q ring, mostly click
            {
                v.q = qFromDecay (clampf (decS * 0.20f, 0.004f, 0.4f), f0);
                v.ratio[0] = 1.0f;   v.resGain[0] = 1.0f;
                v.ratio[1] = 2.71f;  v.resGain[1] = 0.45f;
                v.nl = nlFor (1.4f, v.q, velL);
                v.clickAmt = 0.55f + snap * 0.9f;
                v.excAmp = 1.0f * velL;
                v.tone.setBP (clampf (f0 * lerpf (1.0f, 2.6f, tone) * velB, 120.0f, fsv * 0.45f), 0.9f, fsv);
                v.clickF.setHP (clampf (lerpf (1800.0f, 6500.0f, snap), 60.0f, fsv * 0.45f), 0.7f, fsv);
                v.clickE.trigger (v.clickAmt * velL, 1.0f, clampf (0.0012f * fsv, 2.0f, 1e6f));
                v.famGain = 1.10f;
                v.amp.trigger (1.0f, 1.0f, decS * fsv * 2.0f);
            }
            else if (v.model == 2)          // ZAP — a big bipolar sweep
            {
                v.q = 0.0f;
                v.met[0].reset (0.0); v.met[0].setF (f0, fsv);
                v.pitchEnv = (bend - 0.5f) * 8.0f;
                v.pitchDec = std::exp (-1.0f / clampf (lerpf (0.02f, 0.30f, tone) * fsv, 1.0f, 1e6f));
                v.shape = tone;
                v.famGain = 2.40f;
                v.amp.trigger (velL, 1.0f, clampf (decS * fsv * 0.45f, 4.0f, 1e6f));
                v.tone.setLP (clampf (12000.0f, 60.0f, fsv * 0.45f), 0.7f, fsv);
            }
            else                            // VOX — a pair of formants on a pulse
            {
                v.q = 0.0f;
                v.met[0].reset (0.0); v.met[0].setF (f0 * 0.25f, fsv);
                v.noiseAmt = 0.35f + snap * 0.6f;
                v.noiseE.trigger (velL, 1.0f, clampf (decS * fsv * 0.6f, 4.0f, 1e6f));
                v.band[0].setBP (clampf (lerpf (420.0f, 900.0f, bend), 100.0f, fsv * 0.45f), 6.0f, fsv);
                v.band[1].setBP (clampf (lerpf (1100.0f, 2400.0f, tone), 100.0f, fsv * 0.45f), 7.0f, fsv);
                v.band[2].setBP (clampf (2900.0f, 100.0f, fsv * 0.45f), 6.0f, fsv);
                for (int b = 0; b < NBAND; ++b)
                {
                    v.bandMix[b] = b == 2 ? 0.35f : 1.0f;
                    v.bandE[b].trigger (velL, 1.0f, clampf (decS * fsv * 0.7f, 4.0f, 1e6f));
                }
                v.famGain = 5.20f;
                v.amp.trigger (1.0f, 1.0f, clampf (decS * fsv * 1.4f, 4.0f, 1e6f));
            }
            if (v.excAmp <= 0.0f) v.excAmp = 0.8f * velL;
            break;
        }

        //----------------------------------------------------------------------
        default:    // FAM_METAL — hats and cymbals
        {
            float mr[NMETAL]; metalRatios (bend, mr);
            for (int i = 0; i < NMETAL; ++i)
            {
                v.met[i].reset (v.rng.uni());     // random phases: no two hits identical
                v.met[i].setF (clampf (f0 * mr[i], 12.0f, fsv * 0.45f), fsv);
            }
            v.q = 0.0f;

            /*  BLOCK E — three bands, three decays. A real cymbal's bands do
                not decay together: the high modes damp fastest and the mid
                ring hangs on. This is the whole difference between a cymbal
                and a hat with the decay turned up. */
            /*  These are absolute frequencies rather than multiples of f0, so
                the kit transpose has to be applied by hand — without it a hat
                moved a seventh where everything else moved two octaves, and
                the ear barely noticed the difference. */
            const float kitMul = std::pow (2.0f, kitSemis / 12.0f);
            const float centre = lerpf (2600.0f, 6200.0f, tone) * kitMul;
            const float bf[NBAND] = { centre * 0.55f, centre * 1.35f, centre * 2.6f };
            float bd[NBAND] = { 1.35f, 1.0f, 0.55f };

            if (v.model == 1) { bd[0] = 1.6f; bd[1] = 1.1f; bd[2] = 0.42f; }   // RIDE — mid ring
            if (v.model == 2) { bd[0] = 0.5f; bd[1] = 0.7f; bd[2] = 1.0f; }    // SPLASH — bright and gone

            for (int b = 0; b < NBAND; ++b)
            {
                v.band[b].setBP (clampf (bf[b] * velB, 200.0f, fsv * 0.45f), 1.5f, fsv);
                const float atk = (c >= 10 && v.model == 0) ? clampf (0.0032f * fsv, 1.0f, 1e6f) : 1.0f;  // the crash swell
                v.bandE[b].trigger (velL, atk, clampf (decS * fsv * bd[b], 4.0f, 1e6f));
                v.bandMix[b] = v.model == 1 ? (b == 1 ? 1.2f : 0.8f) : 1.0f;
            }

            // RIDE also gets a bell: a real ping on top of the wash
            if (v.model == 1)
            {
                v.q = qFromDecay (clampf (decS * 0.22f, 0.01f, 2.0f), f0 * 2.7f);
                v.ratio[0] = 2.7f; v.resGain[0] = 0.55f;
                v.nl = nlFor (1.0f, v.q, velL);
                v.excAmp = 0.5f * velL;
            }

            v.hp.setHP (clampf (lerpf (900.0f, 9000.0f, snap) * kitMul, 60.0f, fsv * 0.45f), 0.72f, fsv);
            v.tone.setHP (clampf (250.0f * kitMul, 40.0f, fsv * 0.45f), 0.7f, fsv);
            v.famGain = 1.80f;
            v.amp.trigger (1.0f, 1.0f, decS * fsv * 3.0f);
            break;
        }
    }

    // level, with the tolerance
    v.vel = clampf (vel * lvlTol, 0.0f, 1.4f);
    v.excLeft = v.excAmp;
    if (v.q > 0.0f)
    {
        /*  Normalise the ping level against Q so DECAY does not double as a
            volume control — a resonator's gain at resonance IS its Q. A touch
            of it is left in on purpose: a longer decay really is a little
            louder on the real thing. */
        const float norm = std::pow (12.0f / clampf (v.q, 0.6f, 3000.0f), 0.82f);
        v.excLeft *= norm;
        for (int i = 0; i < NRES; ++i)
            if (v.resGain[i] > 0.0f)
                v.res[i].setF (v.f0 * v.ratio[i], v.q / (0.7f + 0.3f * v.ratio[i]), fsv);
    }
    v.pitchV = v.pitchEnv;
}

//==============================================================================
/*  One sample of one voice. Control-rate work (retuning for tension and the
    pitch envelope) happens every CTRL samples — 0.17 ms at 48k x1, far faster
    than any of it moves. */
float Engine::renderVoice (Voice& v, int c)
{
    const float fsv = (float) osFs;
    float out = 0.0f;

    // ---- excitation --------------------------------------------------------
    float exc = 0.0f;
    if (v.age == 0)      exc = v.excLeft;                     // the one-sample edge
    else if (v.excLeft > 1e-7f) { v.excLeft *= v.excDec; exc = v.excLeft; }

    // ---- CLAP re-triggers --------------------------------------------------
    if (v.fam == FAM_SNARE && v.model == 2 && v.clapStage > 0)
    {
        v.clapLeft -= 1.0f;
        if (v.clapLeft <= 0.0f)
        {
            --v.clapStage;
            v.clapGap *= 0.72f;                               // decreasing spacing: a real clap
            v.clapLeft = v.clapGap;
            v.noiseE.trigger (v.vel * (0.85f - 0.12f * (float) (3 - v.clapStage)), 1.0f,
                              clampf (0.0030f * fsv, 2.0f, 1e6f));
        }
    }

    // ---- control-rate retuning (BLOCK B, tension) --------------------------
    if (v.q > 0.0f && (v.age % CTRL) == 0)
    {
        const float pe = v.pitchV;
        /*  Tension: a struck membrane is stiffer the further it is displaced,
            so a hard hit starts sharp and falls to pitch. f = f0*sqrt(1+a x^2),
            keyed to the resonator's OWN displacement — which means the effect
            arrives from the physics rather than from a pitch envelope drawn
            to imitate it. */
        const float x = v.peak;
        const float tens = v.tension > 0.0f ? std::sqrt (1.0f + v.tension * 26.0f * x * x) : 1.0f;
        const float mul = (1.0f + pe) * tens * (wmActive ? wmFilt : 1.0f);
        for (int i = 0; i < NRES; ++i)
            if (v.resGain[i] > 0.0f)
                v.res[i].setF (v.f0 * v.ratio[i] * mul, v.q / (0.7f + 0.3f * v.ratio[i]), fsv);
        v.peak *= 0.92f;
    }
    v.pitchV *= v.pitchDec;

    // ---- BLOCKS A + C — the coupled resonators -----------------------------
    if (v.q > 0.0f)
    {
        float bpPrev[NRES];
        for (int i = 0; i < NRES; ++i) bpPrev[i] = v.res[i].lastBp;
        for (int i = 0; i < NRES; ++i)
        {
            if (v.resGain[i] <= 0.0f) continue;
            float in = exc;
            if (v.couple > 0.0f)
                for (int j = 0; j < NRES; ++j)
                    if (j != i && v.resGain[j] > 0.0f) in += v.couple * bpPrev[j];
            const float s = v.res[i].tick (in, v.nl);
            out += s * v.resGain[i];
        }
        const float a = std::fabs (v.res[0].lastLp);
        if (a > v.peak) v.peak = a;
    }

    // ---- oscillator models (PUNCH, ZAP, VOX carrier) -----------------------
    if ((v.fam == FAM_KICK && v.model == 1) || (v.fam == FAM_PERC && v.model >= 2))
    {
        const float mul = std::exp2 (v.pitchV);
        v.met[0].setF (clampf (v.f0 * mul * (wmActive ? wmFilt : 1.0f), 8.0f, fsv * 0.45f), fsv);
        v.met[0].ph += v.met[0].inc; if (v.met[0].ph >= 1.0) v.met[0].ph -= 1.0;
        float s = (float) std::sin (2.0 * M_PI * v.met[0].ph);
        if (v.shape > 0.0f) { const float g = 1.0f + v.shape * 3.0f; s = std::tanh (s * g) / std::tanh (g) * 0.92f + s * 0.08f; }
        out += s * (v.fam == FAM_PERC && v.model == 3 ? 0.9f : 1.0f);
    }

    // ---- BLOCK D + E — the metal core through its bands --------------------
    if (v.fam == FAM_METAL || (v.fam == FAM_PERC && v.model == 0))
    {
        float m = 0.0f;
        for (int i = 0; i < NMETAL; ++i) m += v.met[i].square();
        m *= 0.1667f;
        float bs = 0.0f;
        for (int b = 0; b < NBAND; ++b)
            if (v.bandMix[b] > 0.0f) bs += v.band[b].tick (m) * v.bandE[b].tick() * v.bandMix[b];
        out += bs * 1.35f;
    }

    // ---- noise paths -------------------------------------------------------
    if (v.noiseAmt > 0.0f)
    {
        const float nz = v.rng.bi();
        if (v.fam == FAM_PERC && v.model == 3)          // VOX formants
        {
            float bs = 0.0f;
            for (int b = 0; b < NBAND; ++b) bs += v.band[b].tick (nz * 0.5f + out * 0.5f) * v.bandE[b].tick() * v.bandMix[b];
            out = out * 0.25f + bs * 1.1f;
        }
        else
        {
            out += v.noiseF.tick (nz) * v.noiseE.tick() * v.noiseAmt;
        }
    }

    // ---- the snare bed -----------------------------------------------------
    if (v.wireAmt > 0.0f)
    {
        const float we = v.wireE.tick();
        // the buzz threshold: below it the wires simply do not rattle
        const float gate = we > v.wireThresh ? (we - v.wireThresh) / (1.0f - v.wireThresh + 1e-6f) : 0.0f;
        if (gate > 0.0f)
        {
            const float nz = v.rng.bi();
            float w = 0.0f;
            for (int i = 0; i < NWIRE; ++i) w += v.wire[i].tick (nz);
            out += w * (0.30f * v.wireAmt) * gate;
        }
        else
        {
            const float nz = 0.0f;
            for (int i = 0; i < NWIRE; ++i) v.wire[i].tick (nz);
        }
    }

    /*  Tone and high pass, exactly once each per sample. Which filter runs is
        a property of the family and model, spelled out rather than inferred —
        the version that inferred it ticked one biquad twice and gave the toms
        a tail seconds longer than their own decay. */
    switch (v.fam)
    {
        case FAM_KICK:
        case FAM_TOM:   out = v.tone.tick (out); break;
        case FAM_SNARE: out = v.hp.tick (out);   break;
        case FAM_METAL: out = v.tone.tick (v.hp.tick (out)); break;
        case FAM_PERC:
            if (v.model == 0)      out = v.hp.tick (out);        // BELL
            else if (v.model <= 2) out = v.tone.tick (out);      // WOOD, ZAP
            break;                                               // VOX is already formant-shaped
        default: break;
    }

    // ---- the click path ----------------------------------------------------
    if (v.clickAmt > 0.0f)
    {
        const float ce = v.clickE.tick();
        if (ce > 1e-6f)
        {
            const float src = (v.age < 3 ? 1.0f : 0.0f) + v.rng.bi() * 0.9f;
            out += v.clickF.tick (src) * ce * 1.5f;
        }
    }

    /*  ---- PUNCH ----------------------------------------------------------
        Keyed to this voice's OWN trigger, so it is exact rather than
        detected. A lift on the strike and a lean on the body: the hit takes
        up less time and more of the moment it is in. Exactly 1.0f at zero —
        an IEEE-exact multiply, so the machine is bit-identical without it. */
    if (p.g[GP_PUNCH] > 0.0f)
    {
        const float pn = clamp01 (p.g[GP_PUNCH]);
        //  the strike is a different length in every family, and the metal
        //  keeps almost none of the sustain lean — a cymbal ringing for four
        //  seconds reads as a broken decay, not as punch
        float tAtt, kAtt, kSus;
        switch (v.fam)
        {
            case FAM_SNARE: tAtt = 0.0100f; kAtt = 1.80f; kSus = 0.34f; break;
            case FAM_TOM:   tAtt = 0.0110f; kAtt = 1.60f; kSus = 0.30f; break;
            case FAM_PERC:  tAtt = 0.0080f; kAtt = 1.70f; kSus = 0.26f; break;
            case FAM_METAL: tAtt = 0.0090f; kAtt = 1.30f; kSus = 0.06f; break;
            default:        tAtt = 0.0070f; kAtt = 2.00f; kSus = 0.38f; break;   // kick
        }
        const float a = std::exp (-(float) v.age / (tAtt * (float) fs));
        out *= (1.0f + pn * kAtt * a) * (1.0f - pn * kSus * (1.0f - a));
    }

    // ---- the amp envelope --------------------------------------------------
    out *= v.amp.tick() * v.famGain;

    ++v.age;

    // culling: below -96 dBFS for a while, stop rendering entirely
    if (std::fabs (out) < 1.6e-5f) { if (++v.quiet > 900) v.active = false; }
    else v.quiet = 0;

    if (! std::isfinite (out)) { v.reset(); return 0.0f; }
    return out;
}

//==============================================================================
//  THE SEQUENCER
//
//  Everything below is derived from the transport position, not counted from
//  the last block: a step's time is a function of the bar, so the DAW can
//  loop, relocate or change tempo and the pattern stays where it belongs.
//==============================================================================
namespace
{
    // step length in sixteenths, per division setting
    const double STEPLEN[4] = { 0.5, 1.0, 2.0, 4.0 };

    int laneIndex (const Lane& L, long long k)
    {
        const int len = L.len < 1 ? 1 : (L.len > NSTEP ? NSTEP : L.len);
        long long m = k % len; if (m < 0) m += len;
        switch (L.dir)
        {
            case 1: return (int) (len - 1 - m);                       // reverse
            case 2:                                                    // pendulum
            {
                const int span = len > 1 ? (len - 1) * 2 : 1;
                long long q = k % span; if (q < 0) q += span;
                return (int) (q < len ? q : span - q);
            }
            case 3:                                                    // random, but repeatable
            {
                Rng r; r.seed ((uint32_t) (k * 2654435761u + 12345u));
                return (int) (r.uni() * (float) len) % len;
            }
            default: return (int) m;
        }
    }
}

void Engine::setTransport (double bpm, double ppq, bool play, bool present)
{
    if (bpm > 20.0 && bpm < 999.0) { bpmNow = bpm; hostBpm = true; }
    if (play && ppq >= 0.0) ppqNow = ppq;
    const bool wasPlaying = playing;
    playing = play && ppq >= 0.0;
    hostPresent = present;
    /*  the DAW stopping stops the machine: the free-run latch is dropped, so
        it does not carry on by itself afterwards */
    if (wasPlaying && ! playing) freeRun = false;
}

void Engine::runSequencer (int n)
{
    /*  ARMED is the intent; RUNNING also needs a clock that is going — the
        host's, or the free-run latch the panel sets when no host is rolling.
        Stopping the DAW therefore stops the machine, which arming alone
        used to override. */
    if (p.g[GP_SEQ] < 0.5f || ! (playing || freeRun))
    { uiStep = -1; uiLaneStep.fill (-1); return; }

    /*  The host's TEMPO is used whenever the host has one, playing or not, so
        a stopped DAW does not silently hand the machine back to its own clock.
        The internal tempo is for the standalone, where there is no host at
        all. The host's POSITION is a separate question, below. */
    const double bpm = hostBpm ? bpmNow : (double) xmap (p.g[GP_TEMPO], 40.0f, 300.0f);
    const double beats = (double) n / fs * bpm / 60.0;
    double a, b;
    //  rolling: locked to the bar. Stopped: free-running at the same tempo,
    //  so the machine can be auditioned without arming the transport.
    if (playing) { a = ppqNow; b = ppqNow + beats; }
    else         { a = ppqFree; b = ppqFree + beats; ppqFree = b; }

    uiStep = (int) (((long long) std::floor (a * 4.0)) % NSTEP);
    if (uiStep < 0) uiStep += NSTEP;

    const Pattern& P = pat[curPat < 0 ? 0 : (curPat >= NPAT ? NPAT - 1 : curPat)];
    const float gSwing = (p.g[GP_SWING] - 0.5f) * 2.0f;      // -1..+1, 0 = straight
    const float feel   = clamp01 (p.g[GP_FEEL]);
    const float grip   = clamp01 (p.g[GP_GRIP]);

    for (int c = 0; c < NCH; ++c)
    {
        const Lane& L = P.lane[c];
        if (L.mute || p.ch[c][CP_MUTE] >= 0.5f) { uiLaneStep[(size_t) c] = -1; continue; }

        const double sl = STEPLEN[L.div & 3];
        const double sa = a * 4.0 / sl, sb = b * 4.0 / sl;
        const double stepSamples = sl / 4.0 * 60.0 / bpm * fs;

        //  where THIS lane is, in its own length and its own division
        {
            const long long kNow = (long long) std::floor (sa);
            uiLaneStep[(size_t) c] = kNow < 0 ? -1 : laneIndex (L, kNow);
            uiLaneFrac[(size_t) c] = sa - (double) kNow;
        }

        /*  Swing delays the ODD steps only — that is what swing is. The lane's
            own value is an offset from the global one, so hats can shuffle
            over a straight kick, which is how grooves are actually built. */
        const double sw = clampf (gSwing * 0.66f + (float) L.swing / 100.0f, -0.45f, 0.45f);

        for (long long k = (long long) std::floor (sa) - 2; k <= (long long) std::floor (sb) + 2; ++k)
        {
            if (k < 0) continue;
            const int idx = laneIndex (L, k);
            const Step& st = L.step[idx];
            if (! st.on) continue;

            double fire = (double) k + ((k & 1) ? sw : 0.0) + (double) st.micro / 100.0;

            /*  THE HAND. Not random jitter — a player leans. Downbeats arrive
                a touch early, the offbeats between them lean late, and the
                amount is one knob. */
            if (feel > 0.0f)
            {
                const long long m = ((k % 4) + 4) % 4;
                const double lean = (m == 0) ? -0.020 : (m == 2 ? 0.008 : 0.034);
                fire += lean * feel;
            }

            if (fire < sa || fire >= sb) continue;

            // probability and conditions, deterministic so a render repeats
            Rng r; r.seed ((uint32_t) (k * 2654435761u + (uint32_t) c * 40503u + 7u));
            if (st.prob < 100 && r.uni() * 100.0f >= (float) st.prob) { lastVel[c] = 0.0f; continue; }

            const long long loop = k / (L.len < 1 ? 1 : L.len);
            bool play = true;
            switch (st.cond)
            {
                case 1: play = (loop % 2) == 0; break;                 // 1:2
                case 2: play = (loop % 3) == 0; break;                 // 1:3
                case 3: play = (loop % 4) == 0; break;                 // 1:4
                case 4: play = lastVel[c] > 0.0f; break;               // PREV played
                case 5: play = lastVel[c] <= 0.0f; break;              // PREV did not
                default: break;
            }
            if (! play) continue;

            float vel = (float) st.vel / 127.0f;
            //  GRIP: a limb carries something of its last hit into this one
            if (grip > 0.0f && lastVel[c] > 0.0f)
                vel = lerpf (vel, 0.5f * (vel + lastVel[c]), grip * 0.7f);
            vel = clampf (vel, 0.02f, 1.0f);
            lastVel[c] = vel;

            const double frac = (fire - sa) / std::max (1e-9, sb - sa);
            const int at = (int) (frac * (double) n) * osFactor;

            const int rt = st.ratchet < 1 ? 1 : (st.ratchet > 8 ? 8 : st.ratchet);
            for (int rr2 = 0; rr2 < rt; ++rr2)
            {
                // a ratchet ramps down in level, the way a real roll does
                const float rv = vel * (1.0f - 0.06f * (float) rr2);
                scheduleHit (c, rv, 0.0f, at + (int) (stepSamples * osFactor * rr2 / rt));
            }
        }
    }
}

//==============================================================================
void Engine::process (float* L, float* R, int n, float* const* aux)
{
    updateOs();
    runSequencer (n);

    if (aux != nullptr && auxA.size() != (size_t) NCH)
    {
        auxA.resize ((size_t) NCH); auxB.resize ((size_t) NCH);
        for (int c = 0; c < NCH; ++c) { auxA[(size_t) c].design(); auxB[(size_t) c].design(); }
    }

    // world-mod bus, once per block; neutral is exact-identity arithmetic
    wmDet   = wmIn[0].load(); wmPan  = wmIn[1].load(); wmTremD = wmIn[2].load();
    wmTremR = wmIn[3].load(); wmSag  = wmIn[4].load(); wmFilt  = wmIn[5].load();
    if (wmTremR <= 0.0f) wmTremR = 4.0f;
    wmActive = (wmDet != 0.0f || wmPan != 0.0f || wmTremD != 0.0f || wmSag != 0.0f || wmFilt != 1.0f);

    const float fsv = (float) osFs;
    const float vol = p.g[GP_VOLUME] < 0.005f ? 0.0f : 2.0f * p.g[GP_VOLUME] * p.g[GP_VOLUME];
    const float sag = clamp01 (p.g[GP_SAG]);
    const float bodyAmt = clamp01 (p.g[GP_BODY]);
    const float railDec = std::exp (-1.0f / clampf (0.035f * fsv, 1.0f, 1e6f));

    // the shared shell and the sympathetic shells, retuned per block.
    // Their damping goes through nlFor too, for the same reason a voice's does.
    const float bodyNl = nlFor (0.9f, 6.0f);
    const float sympNl = nlFor (1.2f, 9.0f);
    body[0].setF (clampf (108.0f, 20.0f, fsv * 0.45f), 6.0f, fsv);
    body[1].setF (clampf (247.0f, 20.0f, fsv * 0.45f), 4.5f, fsv);
    bodyTone.setHP (clampf (70.0f, 20.0f, fsv * 0.45f), 0.7f, fsv);

    // the sympathetic shells of THE WEB
    sympDec = std::exp (-1.0f / clampf (0.0016f * fsv, 1.0f, 1e6f));
    for (int c = 0; c < NCH; ++c)
    {
        const ChanDef& d = CHANS[c];
        float f = xmap (p.ch[c][CP_TUNE], d.tuneLo, d.tuneHi)
                * std::pow (2.0f, (p.g[GP_KITTUNE] - 0.5f) * 24.0f / 12.0f);
        symp[(size_t) c].setF (clampf (f, 20.0f, fsv * 0.45f), 9.0f, fsv);
    }

    // AGE drifts slowly: a machine is never quite what it was an hour ago
    if (++driftTick >= 64)
    {
        driftTick = 0;
        for (int c = 0; c < NCH; ++c)
            tolDrift[(size_t) c] = clampf (tolDrift[(size_t) c] * 0.999f + grng.bi() * 0.004f, -0.6f, 0.6f);
    }

    std::array<float, NCH> chMeter { };

    /*  Pan is constant across a block, so its two trig calls belong here and
        not in the inner loop — where they were running twelve times per
        oversampled sample, 2.3 million times a second at 2x, to produce the
        same two numbers over and over. */
    std::array<float, NCH> panL { }, panR { };
    for (int c = 0; c < NCH; ++c)
    {
        float pan = clampf (p.ch[c][CP_PAN], 0.0f, 1.0f) * 2.0f - 1.0f;
        if (wmActive && wmPan != 0.0f)
            pan = clampf (pan + wmPan * (std::fmod ((float) c * 0.6180339887f, 1.0f) * 2.0f - 1.0f), -1.0f, 1.0f);
        const float th = (pan * 0.5f + 0.5f) * 1.5707963f;
        panL[(size_t) c] = std::cos (th);
        panR[(size_t) c] = std::sin (th);
    }

    for (int i = 0; i < n; ++i)
    {
        float outL = 0.0f, outR = 0.0f;
        float outA[NCH] = { 0.0f };

        // rebound bounces and sequencer steps land on their own sample
        if (nHits > 0) fireDue (i * osFactor);

        for (int os = 0; os < osFactor; ++os)
        {
            float mono = 0.0f, sumL = 0.0f, sumR = 0.0f;

            // the rail: everything shares one supply, and a hard hit pulls it
            // down in level AND in pitch, which is why sag glues where a
            // compressor cannot
            railEnv *= railDec;
            rail = 1.0f / (1.0f + sag * railEnv * 0.55f);

            for (int c = 0; c < NCH; ++c)
            {
                const float* P = p.ch[c];
                if (P[CP_MUTE] >= 0.5f) continue;

                float s = 0.0f;
                for (auto& v : voices[(size_t) c])
                    if (v.active) s += renderVoice (v, c);

                // THE WEB: this channel's shell, rung by the others
                float sy = 0.0f;
                if (sympExc[(size_t) c] > 1e-7f)
                {
                    sy = symp[(size_t) c].tick (sympExc[(size_t) c], sympNl);
                    sympExc[(size_t) c] *= sympDec;
                }
                else if (std::fabs (symp[(size_t) c].lastBp) > 1e-7f)
                {
                    sy = symp[(size_t) c].tick (0.0f, sympNl);
                }
                s += sy * 0.9f;

                // per-channel drive: an asymmetric shaper, roughly level-matched
                const float dr = clamp01 (P[CP_DRIVE]);
                if (dr > 0.001f)
                {
                    /*  Quadratic plus a gentle linear term. The old
                        1 + dr*11 put a 14% setting already deep in the tanh
                        knee, which cost the channel most of its velocity
                        range for nothing anyone had asked for. */
                    const float g = 1.0f + dr * 2.0f + dr * dr * 14.0f;
                    const float x = s * g;
                    s = (std::tanh (x) + 0.14f * dr * std::tanh (x * 0.5f) * std::fabs (std::tanh (x)))
                        / std::pow (g, 0.62f);
                }

                float lvl = clamp01 (P[CP_LEVEL]);
                lvl *= lvl * 1.35f;
                s *= lvl * rail;

                if (wmActive && wmTremD > 0.0f)
                {
                    // fanned across the twelve by the golden angle: zero mean,
                    // so the kit breathes rather than pumping as one
                    const float ph = wmPhase + std::fmod ((float) c * 0.6180339887f, 1.0f);
                    s *= 1.0f - wmTremD * 0.5f * (1.0f - std::cos (2.0f * (float) M_PI * ph));
                }

                sumL += s * panL[(size_t) c];
                sumR += s * panR[(size_t) c];
                mono += s;

                const float m = std::fabs (s);
                if (m > chMeter[(size_t) c]) chMeter[(size_t) c] = m;

                if (aux != nullptr)
                {
                    if (osFactor == 1) outA[c] = s;
                    else if (osFactor == 2)
                    {
                        auxB[(size_t) c].push (s);
                        if (os == osFactor - 1) outA[c] = auxB[(size_t) c].read();
                    }
                    else
                    {
                        auxA[(size_t) c].push (s);
                        if ((os & 1) == 1)
                        {
                            auxB[(size_t) c].push (auxA[(size_t) c].read());
                            if (os == osFactor - 1) outA[c] = auxB[(size_t) c].read();
                        }
                    }
                }
            }

            // KIT BODY — one shell every channel feeds. Not a reverb: a
            // coupling. It is what makes twelve circuits sound like one kit.
            if (bodyAmt > 0.001f)
            {
                const float in = bodyTone.tick (mono);
                const float b = body[0].tick (in, bodyNl) * 0.7f + body[1].tick (in, bodyNl) * 0.45f;
                sumL += b * bodyAmt * 1.6f; sumR += b * bodyAmt * 1.6f;
            }

            wmPhase += wmTremR / fsv; if (wmPhase >= 1.0f) wmPhase -= 1.0f;

            if (osFactor == 1) { outL = sumL; outR = sumR; }
            else if (osFactor == 2)
            {
                dec2L.push (sumL); dec2R.push (sumR);
                if (os == osFactor - 1) { outL = dec2L.read(); outR = dec2R.read(); }
            }
            else
            {
                dec1L.push (sumL); dec1R.push (sumR);
                if ((os & 1) == 1)
                {
                    dec2L.push (dec1L.read()); dec2R.push (dec1R.read());
                    if (os == osFactor - 1) { outL = dec2L.read(); outR = dec2R.read(); }
                }
            }
        }

        // master DC blocker at ~5 Hz, then volume, then the ceiling
        const float rdc = 1.0f - 2.0f * (float) M_PI * 5.0f / (float) fs;
        const float yL = outL - masterDcXL + rdc * masterDcL;
        const float yR = outR - masterDcXR + rdc * masterDcR;
        masterDcXL = outL; masterDcXR = outR; masterDcL = yL; masterDcR = yR;

        L[i] = ceilSoft (yL * vol);
        R[i] = ceilSoft (yR * vol);
        if (aux != nullptr)
            for (int c = 0; c < NCH; ++c)
                if (aux[c] != nullptr)
                    aux[c][i] = std::isfinite (outA[c]) ? ceilSoft (outA[c] * vol) : 0.0f;
        if (! std::isfinite (L[i])) L[i] = 0.0f;
        if (! std::isfinite (R[i])) R[i] = 0.0f;
    }

    for (int c = 0; c < NCH; ++c)
        meter[(size_t) c] = std::max (chMeter[(size_t) c], meter[(size_t) c] * 0.86f);

    // anything still queued belongs to the next block
    for (int i = 0; i < nHits; ++i) hits[(size_t) i].at -= n * osFactor;

    /*  Carry the transport forward, so a host that only reports its position
        once per block (or a bench that never reports one at all) still gets a
        continuous timeline rather than the same bar over and over. */
    if (playing) ppqNow += (double) n / fs * bpmNow / 60.0;
}

} // namespace fmr
