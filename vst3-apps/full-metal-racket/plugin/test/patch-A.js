// CHUNK A — the parameter surface grows, and three of the wild features land:
//   REBOUND  a physically modelled stick bounce (flam -> drag -> buzz roll)
//   KEY MODE chromatic play, with decay scaling by pitch like a real drum
//   MORPH    kit A/B, blended in the engine
// Plus the scheduled-hit queue that REBOUND needs and the sequencer will reuse.
"use strict";
const fs = require("fs");
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  for (const [a, b, tag] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(path.split("/").pop() + ": " + tag + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(path, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/Source/";

// ─────────────────────────────────────────────────────────────── Engine.h ───
const writeH = edit(R + "Engine.h", [

[`    CP_PAN,
    CP_MUTE,
    NCP
};`,
`    CP_PAN,
    CP_MUTE,
    CP_REBOUND,     // stick bounce: flam -> drag -> buzz roll
    CP_KEY,         // chromatic play, decay scaling with pitch
    NCP
};`, "CP enum"],

[`    GP_BLEED,       // THE WEB
    GP_BODY,        // the shared shell
    NGP
};`,
`    GP_BLEED,       // THE WEB
    GP_BODY,        // the shared shell
    GP_MORPH,       // kit A <-> kit B
    GP_SEQ,         // the sequencer runs
    GP_SWING,       // global swing, lanes offset from it
    GP_FEEL,        // THE HAND: push and pull per subdivision
    GP_GRIP,        // THE HAND: velocity carried between hits
    GP_TEMPO,       // internal clock, used when the host gives none
    NGP
};`, "GP enum"],

[`struct Params
{
    float ch[NCH][NCP];
    float g[NGP];
    double bpm = 120.0;       // not a table param
    Params();
};`,
`struct Params
{
    float ch[NCH][NCP];
    float g[NGP];
    double bpm = 120.0;       // not a table param
    Params();
};

//==============================================================================
/*  The sequencer's data. Not host parameters — twelve lanes of thirty-two
    steps across sixteen patterns is far too much to put in an APVTS, so it
    lives in the same kind of opaque blob the BWFX rack uses, and only the
    steps that are ON are ever written out. */
constexpr int NSTEP = 32;
constexpr int NPAT  = 16;

struct Step
{
    uint8_t on = 0;
    uint8_t vel = 100;        // 0..127
    uint8_t prob = 100;       // 0..100 %
    uint8_t ratchet = 1;      // 1..8 hits on this step
    uint8_t cond = 0;         // 0 none, 1 = 1:2, 2 = 1:3, 3 = 1:4, 4 = FILL, 5 = NOT FILL, 6 = PREV
    int8_t  micro = 0;        // -50..+50 % of a step
};

struct Lane
{
    uint8_t len = 16;         // 1..32 — per lane, so polymeter is free
    uint8_t div = 1;          // 0 = 32nd, 1 = 16th, 2 = 8th, 3 = quarter
    uint8_t dir = 0;          // 0 fwd, 1 rev, 2 pendulum, 3 random
    int8_t  swing = 0;        // added to the global swing
    uint8_t mute = 0;
    Step step[NSTEP];
};

struct Pattern { Lane lane[NCH]; };`, "sequencer types"],

// ---- Voice gains a pitch offset (KEY MODE) --------------------------------
[`    float  f0 = 100.0f;            // base frequency this hit`,
`    float  f0 = 100.0f;            // base frequency this hit
    float  keySemis = 0.0f;        // KEY MODE: how far from the reference note`, "voice key"],

// ---- Engine gains scheduling, transport, morph ----------------------------
[`    void  trigger (int chan, float velocity);      // the one way in
    void  noteOn  (int note, float velocity);`,
`    void  trigger (int chan, float velocity, float keySemis = 0.0f);   // the one way in
    void  noteOn  (int note, float velocity);`, "trigger sig"],

[`    // for the panel's meters and trigger lamps
    float channelLevel (int c) const { return meter[(size_t) c]; }`,
`    // for the panel's meters and trigger lamps
    float channelLevel (int c) const { return meter[(size_t) c]; }

    //  ---- transport and the sequencer ------------------------------------
    void setTransport (double bpm, double ppq, bool playing);
    int  seqStep() const { return uiStep; }          // for the panel's running light
    Pattern pat[NPAT];
    int curPat = 0;

    //  ---- MORPH: two whole kits, blended -------------------------------
    Params kitA, kitB;
    bool   haveA = false, haveB = false;
    /*  Applied to a COPY of the parameters each block, never written back:
        morph is a performance control, and a fader that silently rewrote a
        hundred knobs would make its own automation unusable. */
    void applyMorph (Params& dst) const;`, "engine api"],

[`    void  setupVoice (Voice& v, int c, float velocity);
    float renderVoice (Voice& v, int c);
    void  updateOs();`,
`    void  setupVoice (Voice& v, int c, float velocity, float keySemis);
    float renderVoice (Voice& v, int c);
    void  updateOs();

    /*  Scheduled hits, in oversampled samples from the start of the block.
        REBOUND fills this with a stick's decaying bounce, and the sequencer
        fills it with the step's own sample-accurate position. */
    struct Hit { int chan = 0; float vel = 0.0f; float key = 0.0f; int at = 0; };
    static constexpr int MAXHITS = 512;
    std::array<Hit, MAXHITS> hits { };
    int nHits = 0;
    void scheduleHit (int chan, float vel, float key, int atSamples);
    void fireDue (int sampleInBlock);

    // transport, for the sequencer
    double bpmNow = 120.0, ppqNow = 0.0;
    bool   playing = false;
    int    uiStep = -1;
    float  lastVel[NCH] { };          // THE HAND: what this limb hit last
    void   runSequencer (int nSamples);`, "engine private"]
]);

// ───────────────────────────────────────────────────────────── Engine.cpp ───
const writeC = edit(R + "Engine.cpp", [

[`static const char* CP_NAME[NCP] =
{
    "MODEL", "TUNE", "DECAY", "TONE", "SNAP", "BEND", "DRIVE", "LEVEL", "PAN", "MUTE"
};`,
`static const char* CP_NAME[NCP] =
{
    "MODEL", "TUNE", "DECAY", "TONE", "SNAP", "BEND", "DRIVE", "LEVEL", "PAN", "MUTE",
    "REBOUND", "KEY MODE"
};`, "CP names"],

[`                        static const char* SH[NCP] = { "model","tune","decay","tone","snap","bend","drive","level","pan","mute" };`,
`                        static const char* SH[NCP] = { "model","tune","decay","tone","snap","bend","drive","level","pan","mute",
                                                       "rebound","key" };`, "CP ids"],

[`                        case CP_MUTE:  r.kind = KP_SW;    r.def = 0.0f; break;`,
`                        case CP_MUTE:  r.kind = KP_SW;    r.def = 0.0f; break;
                        case CP_REBOUND: r.def = 0.0f; break;
                        case CP_KEY:   r.kind = KP_SW;    r.def = 0.0f; break;`, "CP specs"],

[`static const char* GLOBAL_ID[NGP] =
{
    "volume", "os", "railsag", "age", "kittune", "hatlink", "bleed", "body"
};
static const char* GLOBAL_NAME[NGP] =
{
    "MASTER VOLUME", "OVERSAMPLING", "RAIL SAG", "AGE", "KIT TUNE", "HAT LINK", "BLEED", "KIT BODY"
};`,
`static const char* GLOBAL_ID[NGP] =
{
    "volume", "os", "railsag", "age", "kittune", "hatlink", "bleed", "body",
    "morph", "seq", "swing", "feel", "grip", "tempo"
};
static const char* GLOBAL_NAME[NGP] =
{
    "MASTER VOLUME", "OVERSAMPLING", "RAIL SAG", "AGE", "KIT TUNE", "HAT LINK", "BLEED", "KIT BODY",
    "KIT MORPH", "SEQUENCER", "SWING", "FEEL", "GRIP", "TEMPO"
};`, "GP names"],

[`                    case GP_BODY:    r.def = 0.20f; break;
                    default: break;`,
`                    case GP_BODY:    r.def = 0.20f; break;
                    case GP_MORPH:   r.def = 0.0f;  break;
                    case GP_SEQ:     r.kind = KP_SW; r.def = 0.0f; break;
                    case GP_SWING:   r.def = 0.5f;  break;      // 50 % = straight
                    case GP_FEEL:    r.def = 0.0f;  break;
                    case GP_GRIP:    r.def = 0.0f;  break;
                    case GP_TEMPO:   r.def = 0.28f; break;      // ~120 BPM on the internal clock
                    default: break;`, "GP specs"],

// ---- trigger gains a key offset and REBOUND -------------------------------
[`void Engine::trigger (int chan, float velocity)
{
    if (chan < 0 || chan >= NCH) return;
    const float vel = clampf (velocity, 0.01f, 1.0f);`,
`void Engine::scheduleHit (int chan, float vel, float key, int atSamples)
{
    if (nHits >= MAXHITS) return;
    hits[(size_t) nHits++] = { chan, vel, key, atSamples };
}

void Engine::fireDue (int sampleInBlock)
{
    for (int i = 0; i < nHits; )
    {
        if (hits[(size_t) i].at <= sampleInBlock)
        {
            const Hit h = hits[(size_t) i];
            hits[(size_t) i] = hits[(size_t) --nHits];
            trigger (h.chan, h.vel, h.key);
        }
        else ++i;
    }
}

void Engine::trigger (int chan, float velocity, float keySemis)
{
    if (chan < 0 || chan >= NCH) return;
    const float vel = clampf (velocity, 0.01f, 1.0f);

    /*  REBOUND — a stick does not strike once and stop. It bounces, and the
        bounces get closer together and quieter, because each one starts from
        a lower height. Low is a flam, middling is a drag, high is a buzz
        roll: one control, three techniques that are otherwise a nightmare to
        program. The gaps follow the physical law (time between bounces is
        proportional to the square root of the height) rather than a taper
        picked by ear. */
    const float rb = clamp01 (p.ch[chan][CP_REBOUND]);
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
            scheduleHit (chan, v, keySemis, at);
            gap *= std::sqrt (restitution);                  // closer together each time
            if (gap < osFs * 0.004f) gap = (float) (osFs * 0.004f);
        }
    }`, "trigger + rebound"],

[`    Voice& v = voices[(size_t) chan][(size_t) rr[(size_t) chan]];
    rr[(size_t) chan] = (rr[(size_t) chan] + 1) % VOICES;
    setupVoice (v, chan, vel);`,
`    Voice& v = voices[(size_t) chan][(size_t) rr[(size_t) chan]];
    rr[(size_t) chan] = (rr[(size_t) chan] + 1) % VOICES;
    setupVoice (v, chan, vel, keySemis);`, "trigger setup"],

// ---- noteOn: KEY MODE -----------------------------------------------------
[`void Engine::noteOn (int note, float velocity)
{
    for (int c = 0; c < NCH; ++c)
        if (CHANS[c].note == note) { trigger (c, velocity); return; }

    /*  A chromatic octave from middle C also plays the twelve, in panel
        order. Not a substitute for an editable map — it is so the machine can
        be played from any keyboard the moment it loads. */
    if (note >= 60 && note < 60 + NCH) trigger (note - 60, velocity);
}`,
`void Engine::noteOn (int note, float velocity)
{
    /*  KEY MODE: a channel switched to chromatic play answers a WHOLE RANGE
        of notes rather than its one mapped note, taking its pitch from the
        distance to that note. A tuned kick as a bassline is the most-used
        trick in modern music and almost nothing does it properly — which
        here means the decay scales with pitch too, so a low note rings
        longer, the way a real drum does. */
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

    for (int c = 0; c < NCH; ++c)
        if (CHANS[c].note == note) { trigger (c, velocity); return; }

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
void Engine::applyMorph (Params& dst) const
{
    const float t = clamp01 (dst.g[GP_MORPH]);
    if (! haveA || ! haveB || t <= 0.0f) return;

    for (int i = 0; i < numParams(); ++i)
    {
        const PSpec& s = paramSpec (i);
        if (s.chan < 0) continue;                       // globals are not a kit's character
        if (s.slot == CP_MUTE) continue;                // a morph must not silence a channel
        const float a = pvalue (const_cast<Params&> (kitA), s);
        const float b = pvalue (const_cast<Params&> (kitB), s);
        float& d = pvalue (dst, s);
        if (s.kind == KP_LIST || s.kind == KP_SW)
        {
            //  staggered thresholds, from the parameter's own index, so the
            //  switched things do not all flip together at the midpoint
            const float th = 0.30f + 0.40f * (float) ((i * 37) % 100) / 100.0f;
            d = t < th ? a : b;
        }
        else d = lerpf (a, b, t);
    }
}`, "keymode + morph"],

// ---- setupVoice applies the key offset -----------------------------------
[`void Engine::setupVoice (Voice& v, int c, float vel)
{`,
`void Engine::setupVoice (Voice& v, int c, float vel, float keySemis)
{`, "setupVoice sig"],

[`    float f0 = xmap (tune, d.tuneLo, d.tuneHi) * tuneTol;
    if (wmActive) f0 *= std::pow (2.0f, wmDet / 1200.0f);
    v.f0 = f0;

    const float decMs = xmap (P[CP_DECAY], d.decLo, d.decHi) * decayTol;`,
`    float f0 = xmap (tune, d.tuneLo, d.tuneHi) * tuneTol;
    if (wmActive) f0 *= std::pow (2.0f, wmDet / 1200.0f);
    v.keySemis = keySemis;
    if (keySemis != 0.0f) f0 *= std::pow (2.0f, keySemis / 12.0f);
    v.f0 = f0;

    /*  Decay scales with pitch under KEY MODE: an octave down rings about
        60 % longer, which is roughly what a bigger drum does and is the
        difference between a playable tuned kick and a chirp. */
    float keyDecay = 1.0f;
    if (keySemis != 0.0f) keyDecay = std::pow (2.0f, -keySemis / 12.0f * 0.68f);
    const float decMs = xmap (P[CP_DECAY], d.decLo, d.decHi) * decayTol * keyDecay;`, "key pitch + decay"],

// ---- process: fire scheduled hits, apply morph ----------------------------
[`    for (int i = 0; i < n; ++i)
    {
        float outL = 0.0f, outR = 0.0f;
        float outA[NCH] = { 0.0f };`,
`    for (int i = 0; i < n; ++i)
    {
        float outL = 0.0f, outR = 0.0f;
        float outA[NCH] = { 0.0f };

        // rebound bounces and sequencer steps land on their own sample
        if (nHits > 0) fireDue (i * osFactor);`, "fire due"],

[`    for (int c = 0; c < NCH; ++c)
        meter[(size_t) c] = std::max (chMeter[(size_t) c], meter[(size_t) c] * 0.86f);
}`,
`    for (int c = 0; c < NCH; ++c)
        meter[(size_t) c] = std::max (chMeter[(size_t) c], meter[(size_t) c] * 0.86f);

    // anything still queued belongs to the next block
    for (int i = 0; i < nHits; ++i) hits[(size_t) i].at -= n * osFactor;
}`, "carry hits"],

[`    rr.fill (0); meter.fill (0.0f); sympExc.fill (0.0f);`,
 `    rr.fill (0); meter.fill (0.0f); sympExc.fill (0.0f);
    nHits = 0; uiStep = -1;
    for (auto& l : lastVel) l = 0.0f;`, "reset hits"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
writeH(); writeC();
console.log("chunk A patched OK — rebound, key mode, morph, hit scheduling");
