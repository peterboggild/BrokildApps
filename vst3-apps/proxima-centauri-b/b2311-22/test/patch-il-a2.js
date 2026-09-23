/*  THE INTERLOCUTOR, phase A2 — the engine.

    Three layers, as specced in ARTEFACT-B2311-DESIGN.md §13:

    1. SYMPATHETIC LISTENING + RANK REPLY. A second body shares the vessel.
       Its ladder sits at ITS OWN speaking register (voiceHz), so what it can
       physically absorb from you is a real property of the pair: one 2-pole
       resonator per mode, driven by your actual audio. Then, in your
       silences, it answers — the reply's TIMBRE from what it physically
       heard, its STRUCTURE from a rank translation of your utterance (your
       n-th brightest mode -> its n-th brightest: the same sentence spoken
       through different flesh), and its LOUDNESS from kinship, so kin
       converse and strangers answer faintly. The reply then migrates along
       ITS anatomy — a route through a body you do not own.

    2. THE NONLINEAR MEMBRANE. While both sound at once their voices
       MULTIPLY: sidebands at every sum and difference of the two ladders —
       frequencies neither body can produce alone, unique to the pair, gone
       the moment either falls silent. A third, unownable voice.

    3. SPEECH IS SURGERY. Listening remodels the listener. Edges carrying
       energy strengthen (Hebbian anatomy), and because the sound IS the
       graph's Laplacian spectrum, the voice permanently drifts toward what
       it has heard. The remodelling is applied only when the body is at
       REST — between utterances, like healing — so it never glitches a held
       note, and it is deterministic: the scars derive from the audio path
       alone.

    CONVERSE at 0 is exactly the old engine: no resonators, no reply, no
    membrane, no scars, and not one RNG call. The bench memcmps it.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
  for (const [a, b, tag] of subs) {
    const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(tag + " x" + n); continue; }
    s = s.split(A).join(B);
  }
  return () => fs.writeFileSync(path, s);
}

/* ═══ Engine.h ═══════════════════════════════════════════════════════ */
const wH = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.h", [

[`    float slab      = 0.6f;    // section thickness
};`,
`    float slab      = 0.6f;    // section thickness

    //  THE INTERLOCUTOR — the second body (design §13)
    float other     = 61.0f;   // its catalog number (int-valued, not automatable)
    float converse  = 0.45f;   // how strongly it listens and answers
    float commune   = 0.25f;   // the nonlinear membrane between the two voices
    float plastic   = 0.15f;   // how much listening remodels the listener
};`, "params"],

[`//  ----------------------------------------------------------------- engine`,
`/*  ------------------------------------------------------- the interlocutor
    A resident second body. It is NOT in the voice pool — it has no keys, no
    aperture and no player. It has ears (a resonator bank at its own
    eigenfrequencies), a memory of what rang, and a voice of its own. */
struct Interlocutor
{
    //  ears: one 2-pole resonator per mode, at ITS register
    std::array<float, kMaxModes> rz1 {}, rz2 {};
    std::array<float, kMaxModes> rc {}, rr {}, rin {};
    std::array<float, kMaxModes> acc {};       // per-tick absorbed power
    std::array<float, kMaxModes> heardPow {};  // what rang in it, decaying

    //  what it understood: the speaker's profile, rank-translated at reply
    std::array<float, kMaxModes> spoken {};

    //  voice
    std::array<float, kMaxModes> oscX {}, oscY {}, rotC {}, rotS {};
    std::array<float, kMaxModes> freq {}, energy {}, amp {}, ampStep {};
    std::array<float, kMaxModes> sectW {};

    float sentAcc = 0.0f, sentPow = 0.0f;      // what was said TO it
    float spkEnv  = 0.0f;                      // speaker loudness follower
    float quietFor = 0.0f;                     // seconds since it went quiet
    float utter = 0.0f, utterStep = 0.0f;      // the reply's own envelope
    bool  speaking = false;
    float lastKin = 0.0f;                      // measured mutual intelligibility
    float lastReply = 0.0f;                    // amplitude of the last answer

    void silence()
    {
        rz1.fill (0); rz2.fill (0); acc.fill (0); heardPow.fill (0);
        energy.fill (0); amp.fill (0); ampStep.fill (0);
        oscX.fill (1.0f); oscY.fill (0.0f);
        sentAcc = sentPow = spkEnv = quietFor = 0.0f;
        utter = utterStep = 0.0f;
        speaking = false;
    }
};

//  ----------------------------------------------------------------- engine`, "il struct"],

[`    void noteOn  (int note, float vel);
    void noteOff (int note);`,
`    //  the second body (message thread, like loadSpecimen)
    void loadOther (int catalog);
    const Specimen& otherSpecimen() const
        { return oSpec[(size_t) oSpecIdx.load (std::memory_order_acquire)]; }
    int  otherLoadedCatalog() const { return oLoaded; }

    /*  PLASTICITY: the scars are accumulated on the audio thread and applied
        on the message thread, only while the bodies are at rest. */
    bool remodelPending() const { return remodelReq.load (std::memory_order_acquire); }
    void serviceRemodel();
    int  accentSteps() const { return accentCount; }
    void clearAccent();
    //  state: the accumulated edge multipliers, per body
    const float* accentSelf()  const { return accentA.data(); }
    const float* accentOther() const { return accentB.data(); }
    void setAccent (const float* self, int nSelf, const float* other, int nOther);

    float debugKinship()  const { return il.lastKin; }
    float debugReply()    const { return il.lastReply; }
    bool  debugSpeaking() const { return il.speaking; }
    float debugHeard()    const
    {
        float t = 0;
        for (int k = 0; k < otherSpecimen().nModes; ++k) t += il.heardPow[(size_t) k];
        return t;
    }
    void nodeLuminanceOther (float* out) const
    {
        const Specimen& s = otherSpecimen();
        for (int i = 0; i < s.nNodes; ++i) out[i] = 0.0f;
        if (s.nModes <= 0) return;
        for (int k = 0; k < s.nModes; ++k)
        {
            //  a body lit BOTH by what it hears and by what it says
            const float a = il.amp[(size_t) k] * il.utter;
            const float e = a * a + il.heardPow[(size_t) k] * 0.35f;
            if (e <= 1e-9f) continue;
            const float* col = &s.vec[(size_t) k * s.nNodes];
            for (int i = 0; i < s.nNodes; ++i) out[i] += e * col[i] * col[i];
        }
    }

    void noteOn  (int note, float vel);
    void noteOff (int note);`, "il api"],

[`    //  double-buffered specimen (message thread writes spare, publishes)
    Specimen spec[2];
    std::atomic<int> specIdx { 0 };
    int specLoaded = -1;`,
`    //  double-buffered specimen (message thread writes spare, publishes)
    Specimen spec[2];
    std::atomic<int> specIdx { 0 };
    int specLoaded = -1;

    //  the second body, same double-buffer discipline
    Specimen oSpec[2];
    std::atomic<int> oSpecIdx { 0 };
    int oLoaded = -1;
    Interlocutor il;

    /*  PLASTICITY. scar[] accumulates on the audio thread; accent[] is the
        cumulative multiplier actually applied to the catalog graph, and is
        what the session saves. */
    std::array<float, kMaxEdges> scarA {}, scarB {};
    std::array<float, kMaxEdges> accentA {}, accentB {};
    std::atomic<bool> remodelReq { false };
    int accentCount = 0;
    int plastPhase = 0;
    std::array<float, kMaxModes> modeE {};     // per-tick scratch
    Specimen scratch;                          // message-thread rebuild space`, "il members"],

[`    void controlTick();`,
`    void controlTick();
    void interlocutorTick (float dt);
    void updateOtherRotors();
    void accumulateScar (const Specimen& s, const float* modeEnergy,
                         std::array<float, kMaxEdges>& scar, float rate);
    void remodelBody (int catalog, std::array<float, kMaxEdges>& scar,
                      std::array<float, kMaxEdges>& accent,
                      Specimen* buf, std::atomic<int>& idx, bool isSelf);`, "il decls"]
]);

/* ═══ Engine.cpp ═════════════════════════════════════════════════════ */
const wC = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.cpp", [

[`    { "slab",      "SLAB",       0.6f,  KP_PCT, 0, 1, &Params::slab },
};`,
`    { "slab",      "SLAB",       0.6f,  KP_PCT, 0, 1, &Params::slab },
    //  THE INTERLOCUTOR (design §13)
    { "other",     "OTHER",      61.0f, KP_INT, 0, (float) (kCatalog - 1), &Params::other },
    { "converse",  "CONVERSE",   0.45f, KP_PCT, 0, 1, &Params::converse },
    { "commune",   "COMMUNION",  0.25f, KP_PCT, 0, 1, &Params::commune },
    { "plastic",   "PLASTICITY", 0.15f, KP_PCT, 0, 1, &Params::plastic },
};`, "specs"],

[`    if (specLoaded < 0) loadSpecimen ((int) std::lround (p.specimen));
    reset();`,
`    if (specLoaded < 0) loadSpecimen ((int) std::lround (p.specimen));
    if (p.converse > 0.0f && oLoaded < 0) loadOther ((int) std::lround (p.other));
    reset();`, "prepare"],

[`    sectW.fill (0.0f);
    updateSectionWeights();
}`,
`    sectW.fill (0.0f);
    il.silence();
    updateOtherRotors();
    updateSectionWeights();
}

void Engine::loadOther (int catalog)
{
    catalog = std::max (0, std::min (kCatalog - 1, catalog));
    if (catalog == oLoaded) return;

    const int spare = oSpecIdx.load (std::memory_order_acquire) ^ 1;
    generateSpecimen (catalog, oSpec[spare]);
    //  a body that arrives wearing an accent it earned in this session
    bool scarred = false;
    for (int e = 0; e < oSpec[spare].nEdges; ++e)
        if (accentB[(size_t) e] != 0.0f && accentB[(size_t) e] != 1.0f) scarred = true;
    if (scarred)
    {
        for (int e = 0; e < oSpec[spare].nEdges; ++e)
        {
            const float m = accentB[(size_t) e];
            if (m > 0.0f) oSpec[spare].edgeW[e] *= m;
        }
        resolveSpecimen (oSpec[spare]);
    }
    oSpecIdx.store (spare, std::memory_order_release);
    oLoaded = catalog;
    il.silence();
    updateOtherRotors();
}

void Engine::clearAccent()
{
    accentA.fill (1.0f); accentB.fill (1.0f);
    scarA.fill (0.0f);   scarB.fill (0.0f);
    accentCount = 0;
    const int c = specLoaded, o = oLoaded;
    specLoaded = -1; oLoaded = -1;
    if (c >= 0) loadSpecimen (c);
    if (o >= 0) loadOther (o);
}

void Engine::setAccent (const float* self, int nSelf, const float* other, int nOther)
{
    accentA.fill (1.0f); accentB.fill (1.0f);
    for (int e = 0; e < nSelf  && e < kMaxEdges; ++e) accentA[(size_t) e] = self[e];
    for (int e = 0; e < nOther && e < kMaxEdges; ++e) accentB[(size_t) e] = other[e];
    accentCount = (nSelf > 0 || nOther > 0) ? 1 : 0;
    const int c = specLoaded, o = oLoaded;
    specLoaded = -1; oLoaded = -1;
    if (c >= 0) loadSpecimen (c);
    if (o >= 0) loadOther (o);
}

/*  The ears, retuned: one resonator per mode of the OTHER body, at ITS OWN
    speaking register. Peak gain normalised to ~1 so a loud passage cannot
    detonate a high-Q pole. */
void Engine::updateOtherRotors()
{
    const Specimen& s = otherSpecimen();
    const float nyq = 0.45f * (float) fs;
    for (int k = 0; k < kMaxModes; ++k)
    {
        if (k >= s.nModes) { il.rin[(size_t) k] = 0.0f; il.freq[(size_t) k] = 0.0f; continue; }
        const float f = s.voiceHz * s.ratio[k];
        if (f < 18.0f || f >= nyq)
        {
            il.rin[(size_t) k] = 0.0f; il.freq[(size_t) k] = 0.0f;
            il.rotC[(size_t) k] = 1.0f; il.rotS[(size_t) k] = 0.0f;
            continue;
        }
        const float w = 6.2831853f * f / (float) fs;
        //  Q ~ 25: a band about 70 cents wide — wide enough that kinship is a
        //  gradient rather than a coin toss, narrow enough that it MEANS
        //  something (spectralKinship uses the same width)
        const float R = std::exp (-3.14159265f * f / (25.0f * (float) fs));
        il.rc[(size_t) k]  = 2.0f * R * std::cos (w);
        il.rr[(size_t) k]  = R * R;
        il.rin[(size_t) k] = 1.0f - R;
        il.freq[(size_t) k] = f;
        il.rotC[(size_t) k] = std::cos (w);
        il.rotS[(size_t) k] = std::sin (w);
    }
}

/*  Hebbian anatomy: an edge that CARRIES energy strengthens. The sound is
    the graph's spectrum, so this is the voice being rewritten by use. */
void Engine::accumulateScar (const Specimen& s, const float* modeEnergy,
                             std::array<float, kMaxEdges>& scar, float rate)
{
    for (int e = 0; e < s.nEdges; ++e)
    {
        const int a = s.edgeA[e], b = s.edgeB[e];
        float flow = 0.0f;
        for (int k = 0; k < s.nModes; ++k)
        {
            const float ek = modeEnergy[k];
            if (ek <= 1e-8f) continue;
            const float* col = &s.vec[(size_t) k * s.nNodes];
            flow += ek * std::fabs (col[a] * col[b]);
        }
        scar[(size_t) e] += rate * flow;
    }
}

void Engine::remodelBody (int catalog, std::array<float, kMaxEdges>& scar,
                          std::array<float, kMaxEdges>& accent,
                          Specimen* buf, std::atomic<int>& idx, bool isSelf)
{
    if (catalog < 0) return;
    generateSpecimen (catalog, scratch);
    bool any = false;
    for (int e = 0; e < scratch.nEdges; ++e)
    {
        float m = accent[(size_t) e];
        if (m <= 0.0f) m = 1.0f;
        //  the accent is bounded: a body can be marked by what it heard,
        //  never dissolved by it
        m = clampf (m * (1.0f + scar[(size_t) e]), 0.40f, 2.60f);
        accent[(size_t) e] = m;
        scratch.edgeW[e] *= m;
        if (std::fabs (m - 1.0f) > 1e-4f) any = true;
    }
    scar.fill (0.0f);
    if (! any) return;
    resolveSpecimen (scratch);

    float dep = 0;
    for (int e = 0; e < scratch.nEdges; ++e)
        dep += std::fabs (std::log (std::max (0.05f, accent[(size_t) e])));
    scratch.accentDepth = scratch.nEdges > 0 ? dep / (float) scratch.nEdges : 0.0f;

    const int spare = idx.load (std::memory_order_acquire) ^ 1;
    buf[spare] = scratch;
    idx.store (spare, std::memory_order_release);

    if (isSelf)
    {
        const Specimen& s = buf[spare];
        for (int k = 0; k < s.nModes; ++k)
        {
            float c = 0;
            for (int i = 0; i < s.nNodes; ++i)
            {
                const float v = s.vec[(size_t) k * s.nNodes + i];
                c += s.pw[i] * v * v;
            }
            wCent[(size_t) k] = c;
        }
    }
    else
    {
        il.silence();
        updateOtherRotors();
    }
}

void Engine::serviceRemodel()
{
    if (! remodelReq.exchange (false, std::memory_order_acq_rel)) return;
    remodelBody (specLoaded, scarA, accentA, spec,  specIdx,  true);
    remodelBody (oLoaded,    scarB, accentB, oSpec, oSpecIdx, false);
    ++accentCount;
}`, "il impl"],

//  the section weights must serve BOTH bodies
[`void Engine::updateSectionWeights()
{
    const Specimen& s = specimen();
    if (s.nModes <= 0) return;
    const float eps = 0.14f + 0.9f * clamp01 (p.slab);
    for (int k = 0; k < s.nModes; ++k)
    {
        float wsum = 0;
        for (int i = 0; i < s.nNodes; ++i)
        {
            const float d = std::fabs (s.pw[i] - w0);
            if (d < eps)
            {
                const float vv = s.vec[(size_t) k * s.nNodes + i];
                //  soft slab edge so tissue condenses rather than pops
                const float g = 1.0f - (d / eps) * (d / eps);
                wsum += vv * vv * g;
            }
        }
        //  smooth toward the new weight (tissue-speed, not click-speed)
        sectW[(size_t) k] += 0.25f * (wsum - sectW[(size_t) k]);
    }
}`,
`static void sliceWeights (const Specimen& s, float w0, float eps,
                          std::array<float, kMaxModes>& out)
{
    for (int k = 0; k < s.nModes; ++k)
    {
        float wsum = 0;
        for (int i = 0; i < s.nNodes; ++i)
        {
            const float d = std::fabs (s.pw[i] - w0);
            if (d < eps)
            {
                const float vv = s.vec[(size_t) k * s.nNodes + i];
                //  soft slab edge so tissue condenses rather than pops
                const float g = 1.0f - (d / eps) * (d / eps);
                wsum += vv * vv * g;
            }
        }
        //  smooth toward the new weight (tissue-speed, not click-speed)
        out[(size_t) k] += 0.25f * (wsum - out[(size_t) k]);
    }
}

void Engine::updateSectionWeights()
{
    const Specimen& s = specimen();
    if (s.nModes <= 0) return;
    const float eps = 0.14f + 0.9f * clamp01 (p.slab);
    sliceWeights (s, w0, eps, sectW);
    //  the interlocutor is in the same fourth dimension: the winch moves
    //  what is audible of BOTH bodies, so the section shapes the conversation
    if (p.converse > 0.0f && oLoaded >= 0)
        sliceWeights (otherSpecimen(), w0, eps, il.sectW);
}`, "slice"],

//  the interlocutor's own control-rate life
[`void Engine::process (float* L, float* R, int n)
{`,
`/*  The second body's life, once per control tick: what it heard decays,
    what it says migrates and rings down, and in your silences it answers. */
void Engine::interlocutorTick (float dt)
{
    const Specimen& os = otherSpecimen();
    const Specimen& ss = specimen();
    if (os.nModes <= 0) return;

    //  ---- what rang in it, and what was said to it ----
    const float hd = std::exp (-dt / 6.0f);          // ~6 s of memory
    const float a1 = 1.0f - hd;
    const float inv = 1.0f / (float) kCtrl;
    float heardTot = 0.0f;
    for (int k = 0; k < os.nModes; ++k)
    {
        il.heardPow[(size_t) k] = il.heardPow[(size_t) k] * hd
                                + a1 * (il.acc[(size_t) k] * inv);
        il.acc[(size_t) k] = 0.0f;
        heardTot += il.heardPow[(size_t) k];
    }
    il.sentPow = il.sentPow * hd + a1 * (il.sentAcc * inv);
    il.sentAcc = 0.0f;
    il.lastKin = il.sentPow > 1e-10f
               ? clampf (heardTot / il.sentPow, 0.0f, 4.0f) : 0.0f;

    //  ---- the sentence: while you speak, remember the SHAPE of it ----
    if (il.spkEnv > 0.004f)
    {
        il.quietFor = 0.0f;
        const Voice* loud = nullptr;
        float best = 0;
        for (const auto& v : voices)
        {
            if (! v.active) continue;
            float e = 0;
            for (int k = 0; k < ss.nModes; ++k) e += v.amp[(size_t) k];
            if (e > best) { best = e; loud = &v; }
        }
        if (loud != nullptr && best > 1e-6f)
            for (int k = 0; k < ss.nModes; ++k)
                il.spoken[(size_t) k] += 0.08f
                    * (loud->amp[(size_t) k] / best - il.spoken[(size_t) k]);
    }
    else
        il.quietFor += dt;

    //  ---- the reply ----
    const bool wantReply = (! il.speaking)
                        && il.quietFor > 0.18f
                        && il.sentPow > 2.0e-7f
                        && heardTot > 0.0f;
    if (wantReply)
    {
        /*  TIMBRE from what it physically absorbed; STRUCTURE from a rank
            translation of your utterance — your n-th brightest mode onto its
            n-th brightest. Ladders are sorted, so the rank map is a
            proportional index remap: the same sentence, different flesh. */
        float hMax = 1e-12f;
        for (int k = 0; k < os.nModes; ++k) hMax = std::max (hMax, il.heardPow[(size_t) k]);
        float prof[kMaxModes];
        float pn = 0.0f;
        for (int i = 0; i < os.nModes; ++i)
        {
            const int k = (os.nModes > 1 && ss.nModes > 1)
                        ? (i * (ss.nModes - 1)) / (os.nModes - 1) : 0;
            const float rank   = clampf (il.spoken[(size_t) k], 0.0f, 1.0f);
            const float absorb = il.heardPow[(size_t) i] / hMax;
            prof[i] = 0.55f * rank + 0.45f * absorb;
            pn += prof[i] * prof[i];
        }
        pn = std::sqrt (std::max (1e-12f, pn));

        //  loudness: kin answer strongly, strangers faintly but never mutely
        const float kin = clampf (il.lastKin * 1.6f, 0.0f, 1.0f);
        const float A = clamp01 (p.converse) * 9.0f * std::sqrt (il.sentPow)
                      * (0.22f + 0.78f * kin);
        il.lastReply = A;
        for (int k = 0; k < os.nModes; ++k)
        {
            const float a = (prof[k] / pn) * A;
            il.energy[(size_t) k] = a * a;
            //  consumed: it has said what it heard
            il.heardPow[(size_t) k] *= 0.15f;
        }
        il.sentPow *= 0.15f;
        il.speaking = true;
        il.utterStep = 1.0f / std::max (1.0f, 0.025f * (float) fs);
    }

    //  ---- the reply lives: migration along ITS anatomy, then ringdown ----
    if (il.speaking)
    {
        il.utter = clampf (il.utter + il.utterStep, 0.0f, 1.0f);

        const float cr = p.metabolism * p.metabolism * 6.5f * dt
                       * (1.0f + p.membrane * 1.5f);
        if (cr > 0.0f)
            for (int k = 0; k < os.nModes; ++k)
                for (int t = 0; t < kCouplePer; ++t)
                {
                    const int l = os.coupleTo[(size_t) k * kCouplePer + t];
                    if (l <= k) continue;
                    const float cw = os.coupleW[(size_t) k * kCouplePer + t];
                    const float d = clampf (cr * cw, 0.0f, 0.25f)
                                  * (il.energy[(size_t) l] - il.energy[(size_t) k]);
                    il.energy[(size_t) k] += d;
                    il.energy[(size_t) l] -= d;
                }

        const float ap = clamp01 (p.aperture);
        const float tauB = 1.6f + 5.0f * ap;
        float tot = 0.0f;
        for (int k = 0; k < os.nModes; ++k)
        {
            const float tau = std::max (0.30f,
                tauB * std::pow (std::max (1.0f, os.ratio[k]), -0.55f));
            il.energy[(size_t) k] *= std::exp (-dt / tau);
            tot += il.energy[(size_t) k];
            float target = std::sqrt (std::max (0.0f, il.energy[(size_t) k]))
                         * il.sectW[(size_t) k];
            if (il.freq[(size_t) k] <= 0.0f) target = 0.0f;
            il.ampStep[(size_t) k] = (target - il.amp[(size_t) k]) / (float) kCtrl;
        }
        if (tot < 1.0e-9f)
        {
            il.speaking = false;
            il.utter = 0.0f;
            il.amp.fill (0.0f);
            il.ampStep.fill (0.0f);
        }
    }
}

void Engine::process (float* L, float* R, int n)
{`, "il tick"],

//  wire the tick + plasticity into the control tick
[`        if (v.apEnv <= 0.0f && v.releasing)
            v.active = false;
    }
}`,
`        if (v.apEnv <= 0.0f && v.releasing)
            v.active = false;
    }

    //  ---- THE INTERLOCUTOR ----
    const bool conv = p.converse > 0.0f && oLoaded >= 0;
    if (conv) interlocutorTick (dt);

    //  ---- PLASTICITY: speech is surgery ----
    if (p.plastic > 0.0f)
    {
        if (((++plastPhase) & 7) == 0)
        {
            const float rate = p.plastic * p.plastic * 0.0016f;
            //  the played body, remodelled by what YOU do to it
            for (int k = 0; k < s.nModes; ++k)
            {
                float e = 0;
                for (const auto& v : voices)
                    if (v.active) e += v.amp[(size_t) k] * v.amp[(size_t) k];
                modeE[(size_t) k] = e;
            }
            accumulateScar (s, modeE.data(), scarA, rate);

            //  the listener, remodelled by what it HEARS — the real surgery
            if (conv)
            {
                const Specimen& os = otherSpecimen();
                for (int k = 0; k < os.nModes; ++k)
                    modeE[(size_t) k] = il.heardPow[(size_t) k] * 40.0f
                                      + il.amp[(size_t) k] * il.amp[(size_t) k];
                accumulateScar (os, modeE.data(), scarB, rate);
            }
        }

        //  applied only at REST — between utterances, like healing. A body
        //  never remodels under a held note, so it can never glitch one.
        if (! remodelReq.load (std::memory_order_relaxed))
        {
            bool rest = ! il.speaking;
            for (const auto& v : voices) if (v.active) { rest = false; break; }
            if (rest)
            {
                float mx = 0;
                for (int e = 0; e < s.nEdges; ++e) mx = std::max (mx, scarA[(size_t) e]);
                if (conv)
                    for (int e = 0; e < otherSpecimen().nEdges; ++e)
                        mx = std::max (mx, scarB[(size_t) e]);
                if (mx > 0.05f) remodelReq.store (true, std::memory_order_release);
            }
        }
    }
}`, "ctrl wire"],

//  the audio path: ears, voice, membrane
[`        const float sc = master * 0.9f;
        L[i] = ceilSoft (outL * sc);
        R[i] = ceilSoft (outR * sc);
        ++tGlobal;`,
`        /*  ---- THE INTERLOCUTOR: it hears you, and answers ---- */
        if (conv)
        {
            const float spk = 0.5f * (outL + outR);
            const Specimen& os = otherSpecimen();
            const int nm = os.nModes;

            //  ears: a resonator per mode, at its own register. It absorbs
            //  only what it owns a mode for — that is the whole language.
            il.sentAcc += spk * spk;
            for (int k = 0; k < nm; ++k)
            {
                const float g = il.rin[(size_t) k];
                if (g <= 0.0f) continue;
                const float y = g * spk
                              + il.rc[(size_t) k] * il.rz1[(size_t) k]
                              - il.rr[(size_t) k] * il.rz2[(size_t) k];
                il.rz2[(size_t) k] = il.rz1[(size_t) k];
                il.rz1[(size_t) k] = y;
                il.acc[(size_t) k] += y * y;
            }

            //  its voice
            float ilL = 0.0f, ilR = 0.0f;
            if (il.utter > 0.0f)
            {
                for (int k = 0; k < nm; ++k)
                {
                    const float x = il.oscX[(size_t) k], y2 = il.oscY[(size_t) k];
                    const float c = il.rotC[(size_t) k], sn = il.rotS[(size_t) k];
                    il.oscX[(size_t) k] = x * c - y2 * sn;
                    il.oscY[(size_t) k] = x * sn + y2 * c;
                    il.amp[(size_t) k] += il.ampStep[(size_t) k];
                    const float a = il.amp[(size_t) k];
                    if (a <= 0.0f) continue;
                    ilL += a * os.ampL[k] * il.oscX[(size_t) k];
                    ilR += a * os.ampR[k] * (os.phaseSkew[k] > 0.0f
                            ? (il.oscX[(size_t) k] * 0.913f - il.oscY[(size_t) k] * 0.408f)
                            : il.oscX[(size_t) k]);
                }
                ilL *= il.utter; ilR *= il.utter;
            }

            //  speaker loudness follower (drives the reply's timing)
            il.spkEnv += 0.0015f * (std::fabs (spk) - il.spkEnv);

            /*  ---- THE MEMBRANE ----
                While both sound at once their voices MULTIPLY: sum and
                difference frequencies of the two ladders, in neither
                catalog entry, unique to this pair, gone the instant either
                falls silent. A third voice that neither body owns. */
            if (p.commune > 0.0f)
            {
                const float m = p.commune * 13.0f * spk * 0.5f * (ilL + ilR);
                outL += m; outR += m;
            }
            outL += ilL; outR += ilR;
        }

        const float sc = master * 0.9f;
        L[i] = ceilSoft (outL * sc);
        R[i] = ceilSoft (outR * sc);
        ++tGlobal;`, "audio"],

[`void Engine::process (float* L, float* R, int n)
{
    const Specimen& s = specimen();
    const float master = p.volume < 0.005f ? 0.0f : 2.0f * p.volume * p.volume;`,
`void Engine::process (float* L, float* R, int n)
{
    const Specimen& s = specimen();
    const float master = p.volume < 0.005f ? 0.0f : 2.0f * p.volume * p.volume;
    const bool conv = p.converse > 0.0f && oLoaded >= 0 && otherSpecimen().nModes > 0;`, "conv flag"],

//  a new specimen is a new being: its accent goes with it
[`    //  voices from the previous being drain fast (their tables are stale)
    for (auto& v : voices)
        if (v.active) { v.releasing = true; v.killFast = true; }
    warmMem.fill (0.0f);`,
`    //  voices from the previous being drain fast (their tables are stale)
    for (auto& v : voices)
        if (v.active) { v.releasing = true; v.killFast = true; }
    warmMem.fill (0.0f);
    //  a new being wears its own accent, if it earned one this session
    {
        Specimen& sp = spec[spare];
        bool scarred = false;
        for (int e = 0; e < sp.nEdges; ++e)
            if (accentA[(size_t) e] != 0.0f && accentA[(size_t) e] != 1.0f) scarred = true;
        if (scarred)
        {
            for (int e = 0; e < sp.nEdges; ++e)
            {
                const float m = accentA[(size_t) e];
                if (m > 0.0f) sp.edgeW[e] *= m;
            }
            resolveSpecimen (sp);
            for (int k = 0; k < sp.nModes; ++k)
            {
                float c = 0;
                for (int i = 0; i < sp.nNodes; ++i)
                {
                    const float v = sp.vec[(size_t) k * sp.nNodes + i];
                    c += sp.pw[i] * v * v;
                }
                wCent[(size_t) k] = c;
            }
        }
    }`, "accent on load"],

[`Engine::Engine()
{
    setWorldMod (0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);   // FMR lesson: neutral bus
}`,
`Engine::Engine()
{
    setWorldMod (0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);   // FMR lesson: neutral bus
    accentA.fill (1.0f);
    accentB.fill (1.0f);
}`, "ctor"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wH(); wC();
console.log("A2: the interlocutor listens, answers, and is changed by listening");
