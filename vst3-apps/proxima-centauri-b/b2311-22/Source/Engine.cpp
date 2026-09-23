/*  ARTEFACT B2311.22 — engine implementation. See Engine.h for the design.  */

#include "Engine.h"
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>

namespace ab
{

namespace
{
    inline float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    /*  Soft ceiling, transparent below 0.7, asymptotic above — applied
        unconditionally on the output (the Blade Ruiner lesson: a limiter
        switch may remove gain riding, never the ceiling). */
    inline float ceilSoft (float x)
    {
        const float a = std::fabs (x);
        if (a <= 0.7f) return x;
        const float e = (a - 0.7f) / 0.55f;
        const float c = 0.7f + 0.55f * std::tanh (e);
        return x < 0 ? -c : c;
    }
}

//  ------------------------------------------------------- parameter table --
//  One table, walked by the APVTS layout, the processor, the panel and the
//  bench (the Hairfryer SPECS[] pattern — the read-order bug cannot exist).
static const PSpec SPECS[] =
{
    { "specimen",  "SPECIMEN",   0.0f,  KP_INT, 0, (float) (kCatalog - 1), &Params::specimen },
    { "volume",    "GAIN",       0.72f, KP_VOL, 0, 1, &Params::volume },
    { "aperture",  "APERTURE",   0.55f, KP_PCT, 0, 1, &Params::aperture },
    { "metabolism","METABOLISM", 0.35f, KP_PCT, 0, 1, &Params::metabolism },
    { "revival",   "REVIVAL",    0.0f,  KP_PCT, 0, 1, &Params::revival },
    { "depth",     "DEPTH",      0.25f, KP_PCT, 0, 1, &Params::depth },
    { "membrane",  "MEMBRANE",   0.15f, KP_PCT, 0, 1, &Params::membrane },
    { "gravity",   "GRAVITY",    0.5f,  KP_PCT, 0, 1, &Params::gravity },
    { "warmth",    "WARMTH",     0.25f, KP_PCT, 0, 1, &Params::warmth },
    { "wake",      "WAKE",       0.15f, KP_PCT, 0, 1, &Params::wake },
    { "sidereal",  "SIDEREAL",   0.0f,  KP_SW,  0, 1, &Params::sidereal },
    /*  The researchers named this one for the apparatus — a winch is what
        they turned. What it does to the object is not established, and the
        question mark is theirs. */
    { "winch",     "4D PULL/PUSH?", 0.5f, KP_PCT, 0, 1, &Params::winch },
    { "transit",   "TRANSIT",    0.5f,  KP_PCT, 0, 1, &Params::transit },
    { "slab",      "SLAB",       0.6f,  KP_PCT, 0, 1, &Params::slab },
    { "retention", "RETENTION",  0.55f, KP_PCT, 0, 1, &Params::retention },
    { "temp",      "TEMPERATURE", 0.298755f, KP_KELVIN, 77, 800, &Params::temp },
    //  THE INTERLOCUTOR (design §13)
    { "other",     "OTHER",      61.0f, KP_INT, 0, (float) (kCatalog - 1), &Params::other },
    { "converse",  "CONVERSE",   0.45f, KP_PCT, 0, 1, &Params::converse },
    { "commune",   "COMMUNION",  0.25f, KP_PCT, 0, 1, &Params::commune },
    { "plastic",   "PLASTICITY", 0.10f, KP_PCT, 0, 1, &Params::plastic },
};

int numParams() { return (int) (sizeof (SPECS) / sizeof (SPECS[0])); }
const PSpec& paramSpec (int i)
{
    const int n = numParams();
    return SPECS[i < 0 ? 0 : (i >= n ? n - 1 : i)];
}
int paramIndex (const char* id)
{
    for (int i = 0; i < numParams(); ++i)
        if (std::strcmp (SPECS[i].id, id) == 0) return i;
    return -1;
}

//  ----------------------------------------------------------------- engine --
Engine::Engine()
{
    setWorldMod (0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);   // FMR lesson: neutral bus
    accentA.fill (1.0f);
    accentB.fill (1.0f);
}

void Engine::prepare (double sampleRate, int)
{
    fs = sampleRate;
    tGlobal = 0;
    ctrlPhase = 0;
    if (specLoaded < 0) loadSpecimen ((int) std::lround (p.specimen));
    if (p.converse > 0.0f && oLoaded < 0) loadOther ((int) std::lround (p.other));
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v = Voice();
    w0 = 0.0f; wVel = 0.0f; wDrift = 0.0f;
    subRng = Rng (0xB2311u);
    subPhase = 0.0f;
    sectW.fill (0.0f);
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
    if (remodelA.exchange (false, std::memory_order_acq_rel))
        remodelBody (specLoaded, scarA, accentA, spec,  specIdx,  true);
    if (remodelB.exchange (false, std::memory_order_acq_rel))
        remodelBody (oLoaded,    scarB, accentB, oSpec, oSpecIdx, false);
    ++accentCount;
}

void Engine::clearMemory() { warmMem.fill (0.0f); }

void Engine::loadSpecimen (int catalog)
{
    catalog = std::max (0, std::min (kCatalog - 1, catalog));
    if (catalog == specLoaded) return;

    const int spare = specIdx.load (std::memory_order_acquire) ^ 1;
    generateSpecimen (catalog, spec[spare]);

    //  precompute per-mode w centroids for the section dynamics
    const Specimen& s = spec[spare];
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

    specIdx.store (spare, std::memory_order_release);
    specLoaded = catalog;

    //  voices from the previous being drain fast (their tables are stale)
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
    }
}

float Engine::noteToFreq (int note) const
{
    float f = 440.0f * std::pow (2.0f, ((float) note - 69.0f + bendSemis) / 12.0f);
    if (p.sidereal >= 0.5f)
    {
        /*  SIDEREAL: the keyboard walks the specimen's own ladder — melody in
            the object's intrinsic intervals. Snap the 12-TET request to the
            nearest rung of ladder ratios anchored at C-ish registers. */
        const Specimen& s = specimen();
        if (s.ladderLen > 1)
        {
            const float anchor = 32.7f;                    // C1-ish
            float best = f, bestD = 1e9f;
            for (int oct = 0; oct < 8; ++oct)
                for (int j = 0; j < s.ladderLen; ++j)
                {
                    const float cand = anchor * (float) (1 << oct) * s.ladder[j]
                                     / s.ladder[0];
                    const float dd = std::fabs (std::log (cand / f));
                    if (dd < bestD) { bestD = dd; best = cand; }
                }
            f = best;
        }
    }
    return f;
}

void Engine::triggerVoice (Voice& v, int note, float vel, int forceNode)
{
    const Specimen& s = specimen();
    v = Voice();
    v.active = true;
    v.note = note;
    v.velocity = clampf (vel, 0.02f, 1.0f);
    v.f0 = noteToFreq (note);
    v.age = 0;

    //  excitation node: a TOUCH strikes exactly the tissue under the hand;
    //  a key strikes along the DEPTH path, hub -> most remote tissue
    const int pathAt = std::min (s.pathLen - 1,
                        (int) std::lround (clamp01 (p.depth) * (float) (s.pathLen - 1)));
    const int exNode = (forceNode >= 0 && forceNode < s.nNodes)
                     ? forceNode
                     : s.path[std::max (0, pathAt)];

    //  warmth bias enters the excitation profile; GRAVITY does NOT — it is
    //  LIVE now (see controlTick), so stirring it reshapes a held note
    float norm = 0.0f;
    for (int k = 0; k < s.nModes; ++k)
    {
        const float vk = s.vec[(size_t) k * s.nNodes + exNode];
        /*  A sharpened profile: touching a place on the body strongly favours
            the modes that actually live there. The 1.7 exponent is what makes
            DEPTH a journey rather than a tone control - hub and remote tissue
            must sound like different strikes on the same being. */
        float a = std::pow (std::fabs (vk) + 1.0e-4f, 1.7f) + 0.008f;
        a *= 1.0f + p.warmth * 1.6f * warmMem[(size_t) k];
        v.exciteW[(size_t) k] = a;
        norm += a * a;
    }
    //  CONSERVATION: a voice carries a fixed energy budget. Brightness gained
    //  is body lost — the internal economy (design 6.4), enforced by norm.
    //  a cold body takes nothing from a blow
    const float budget = v.velocity * thermal();
    const float scale = budget / std::sqrt (std::max (1e-12f, norm));
    for (int k = 0; k < s.nModes; ++k)
    {
        const float a = v.exciteW[(size_t) k] * scale;
        v.energy[(size_t) k] = a * a;
        //  keep the strike profile: the sustain floor is shaped by it
        v.exciteW[(size_t) k] = a * a;
        v.amp[(size_t) k] = 0.0f;
        v.ampStep[(size_t) k] = 0.0f;
    }

    //  warmth memory: modes touched now open easier later (deterministic)
    for (int k = 0; k < s.nModes; ++k)
        warmMem[(size_t) k] = std::min (1.5f, warmMem[(size_t) k]
                                + 2.5f * v.energy[(size_t) k]);

    //  APERTURE timing
    const float ap = clamp01 (p.aperture);
    const float atk = 0.002f * std::pow (500.0f, ap);          // 2 ms .. 1 s
    v.apEnv = 0.0f;
    v.apStep = 1.0f / std::max (1.0f, atk * (float) fs);
    v.gateSamples = ap < 0.32f
                  ? (int) ((0.05f + ap * 2.2f) * (float) fs)   // a glimpse
                  : -1;                                        // held open

    //  phases: latched from the global clock — the process was already
    //  running; the key merely opens onto it (design 3.4)
    updateVoiceRotors (v, true);
    for (int k = 0; k < s.nModes; ++k)
    {
        const double ph = 2.0 * 3.14159265358979 *
            std::fmod ((double) v.freq[(size_t) k] * (double) tGlobal / fs, 1.0);
        v.oscX[(size_t) k] = (float) std::cos (ph);
        v.oscY[(size_t) k] = (float) std::sin (ph);
    }

    //  playing moves the object: the note's mode-centroid tugs the section
    float cw = 0, tot = 0;
    for (int k = 0; k < s.nModes; ++k)
    {
        cw += wCent[(size_t) k] * v.energy[(size_t) k];
        tot += v.energy[(size_t) k];
    }
    if (tot > 1e-9f)
    {
        /*  REFERRED TO THE SPREAD OF THIS SPECIMEN'S OWN CENTROIDS, and that
            is what makes the mechanism work at all. Eigenvectors are mostly
            delocalised, so their w-centroids cluster hard around the middle of
            the body: measured on 260830.2, a note moved the section by 0.026
            out of a range of 2, and no amount of TRANSIT could fix it because
            the quantity TRANSIT scales was already very nearly zero.

            Against the spread, a note lands somewhere DISTINCTIVE in the body,
            and different notes land in different places — which is the whole
            of "what is played decides where the object goes". */
        float mean = 0;
        for (int k = 0; k < s.nModes; ++k) mean += wCent[(size_t) k];
        mean /= (float) s.nModes;
        float var = 0;
        for (int k = 0; k < s.nModes; ++k)
        { const float d = wCent[(size_t) k] - mean; var += d * d; }
        const float spread = std::max (0.02f, std::sqrt (var / (float) s.nModes));
        const float aim = clampf (((cw / tot) - mean) / spread, -1.3f, 1.3f);
        //  harder blows carry the object further, as they carry it deeper
        /*  0.45 and not 0.9: at 0.9 a SINGLE note carried the section most of
            the way across and settled at -0.43, so the first note saturated it
            and nothing after that could be told apart. A phrase should walk
            the object, not one strike throw it. */
        wVel += p.transit * 0.45f * (0.35f + 0.65f * vel) * (aim - w0);
    }
}

void Engine::updateVoiceRotors (Voice& v, bool force)
{
    const Specimen& s = specimen();
    const float rev = clamp01 (p.revival);
    const float nyq = 0.45f * (float) fs;
    const float fm = wmActive ? wmFMul : 1.0f;
    const float sagMul = wmActive ? std::pow (2.0f, -wmSag * (1.0f - v.apEnv) / 12.0f) : 1.0f;

    for (int k = 0; k < s.nModes; ++k)
    {
        float r = s.ratio[k] + rev * (s.ratioSnap[k] - s.ratio[k]);

        //  MEMBRANE: energy bends frequency — modal tension on an impossible
        //  object. Exactly identity at 0 (r *= 1.0f).
        if (p.membrane > 0.0f)
            r *= 1.0f + p.membrane * 0.02f * (v.energy[(size_t) k] * 40.0f - 0.5f);

        float f = v.f0 * r * fm * sagMul;
        if (wmActive && wmDet != 0.0f)
            f *= std::pow (2.0f, wmDet * (((k & 3) - 1.5f) / 1.5f) / 1200.0f);

        v.freq[(size_t) k] = f;
        if (force || p.membrane > 0.0f || rev > 0.0f || wmActive || bendSemis != 0.0f)
        {
            if (f >= nyq || f < 8.0f)
            {
                v.rotC[(size_t) k] = 1.0f; v.rotS[(size_t) k] = 0.0f;   // culled: alias-free by construction
                v.freq[(size_t) k] = 0.0f;
            }
            else
            {
                const float w = 2.0f * 3.14159265f * f / (float) fs;
                v.rotC[(size_t) k] = std::cos (w);
                v.rotS[(size_t) k] = std::sin (w);
            }
        }
    }
}

void Engine::noteOn (int note, float vel)
{
    //  reuse the same note, else the quietest voice
    Voice* pick = nullptr;
    for (auto& v : voices) if (v.active && v.note == note && ! v.releasing) { pick = &v; break; }
    if (! pick)
        for (auto& v : voices) if (! v.active) { pick = &v; break; }
    if (! pick)
    {
        float qMin = 1e9f;
        for (auto& v : voices)
        {
            float e = 0;
            for (int k = 0; k < specimen().nModes; ++k) e += v.amp[(size_t) k];
            if (e < qMin) { qMin = e; pick = &v; }
        }
    }
    triggerVoice (*pick, note, vel);
}

void Engine::touchOn (int node, float vel)
{
    const Specimen& s = specimen();
    if (s.nNodes <= 0) return;
    node = std::max (0, std::min (s.nNodes - 1, node));
    //  the place's own pitch: stable per node, spread across two octaves
    const int tnote = 41 + (node * 5) % 25;
    touchNote = tnote;
    Voice* pick = nullptr;
    for (auto& v : voices) if (! v.active) { pick = &v; break; }
    if (! pick) pick = &voices[0];
    triggerVoice (*pick, tnote, vel, node);
    /*  A touch also takes the object to where it was touched. triggerVoice
        already tugs the section by the note's mode-centroid; this adds the
        place — the node's own depth in the fourth dimension — so pressing a
        far organ carries the body towards it and it stays carried. */
    wVel += p.transit * 1.1f * (0.3f + 0.7f * vel)
          * (clampf (s.pw[node], -1.0f, 1.0f) - w0);
}

void Engine::touchOff()
{
    if (touchNote < 0) return;
    noteOff (touchNote);
    touchNote = -1;
}

void Engine::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.note == note && ! v.releasing)
            v.releasing = true;
}

void Engine::allNotesOff()
{
    for (auto& v : voices)
        if (v.active) { v.releasing = true; v.killFast = true; }
}

static void sliceWeights (const Specimen& s, float w0, float eps0,
                          std::array<float, kMaxModes>& out)
{
    /*  The vessel is never empty-handed: if the slab happens to fall between
        the tissue, it widens until it holds some. Whatever the anatomy and
        wherever the winch is parked, the artefact speaks. */
    /*  The widening guard exists so the slab is never empty-handed BETWEEN
        the organs. It must not also rescue a section that has been wound
        clean out of the body: past the tissue there is supposed to be
        nothing, and a guard that widens forever would keep dragging the
        object back into a plane it has left. So it runs only while the
        section is still within reach of the body. */
    float wlo = 1.0e9f, whi = -1.0e9f;
    for (int i = 0; i < s.nNodes; ++i)
    { if (s.pw[i] < wlo) wlo = s.pw[i]; if (s.pw[i] > whi) whi = s.pw[i]; }
    const bool nearBody = (w0 > wlo - eps0) && (w0 < whi + eps0);
    float eps = eps0;
    if (nearBody)
        for (int pass = 0; pass < 6; ++pass)
        {
            int inside = 0;
            for (int i = 0; i < s.nNodes; ++i)
                if (std::fabs (s.pw[i] - w0) < eps) { ++inside; break; }
            if (inside > 0) break;
            eps *= 1.8f;
        }
    /*  THE SECTION TRANSFORMS THE OBJECT; IT DOES NOT FADE IT.

        The raw slice weights fall away as the slab leaves the bulk of the
        tissue, and with a thin slab that is most of its travel: measured, the
        winch swung the level by 15.5 dB and the wheel read as a fade with a
        colour change attached. Normalising the total keeps the object exactly
        as loud wherever the section stands, so what the wheel does is change
        WHICH of the body is sounding — which is what it is for. Same law the
        instrument already applies to GRAVITY: colour, never loudness. */
    float raw[kMaxModes];
    float total = 0.0f;
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
        raw[k] = wsum;
        total += wsum;
    }
    /*  kSectSum is calibrated, not chosen: it is the total the old thick
        default slab produced, so a patch is as loud as it was. */
    const float scale = total > 1e-6f ? (0.62f * (float) s.nModes / total) : 0.0f;
    for (int k = 0; k < s.nModes; ++k)
    {
        //  smooth toward the new weight (tissue-speed, not click-speed)
        out[(size_t) k] += 0.25f * (raw[k] * scale - out[(size_t) k]);
    }
}

void Engine::updateSectionWeights()
{
    const Specimen& s = specimen();
    if (s.nModes <= 0) return;
    /*  A THINNER SLAB, so that moving it is an event. At 0.14 + 0.9*slab the
        default section was 0.68 deep in a body about two deep — it held most
        of the tissue at every winch setting, so the whole travel of the wheel
        moved the colour by half an octave and mostly changed the LEVEL. The
        widen-until-it-holds-tissue guard in sliceWeights still protects the
        thin end, so nothing can fall silent between the organs. */
    const float eps = 0.045f + 0.42f * clamp01 (p.slab);
    //  the site rock is an offset on the SLICE, never on w0 itself: the winch
    //  and the wake keep their own state, and at rock 0 this is w0 + 0.0f
    const float wc = w0 + siteRock;
    sliceWeights (s, wc, eps, sectW);
    //  the interlocutor is in the same fourth dimension: the winch moves
    //  what is audible of BOTH bodies, so the section shapes the conversation
    if (p.converse > 0.0f && oLoaded >= 0)
        sliceWeights (otherSpecimen(), wc, eps, il.sectW);
}

void Engine::controlTick()
{
    const Specimen& s = specimen();
    const float dt = (float) kCtrl / (float) fs;

    //  ---- the section moves: played impulses + winch + wake drift ----
    /*  WAKE is the artefact exploring itself: the wander rides the winch
        TARGET (the old form added sin*dt against the 0.85/s recall — a
        ~40x-too-weak drive that moved w0 by ~0.01 and was invisible).
        The section now genuinely patrols, and everything downstream —
        slab membership, mode audibility, ghosts — moves with it. */
    /*  THE TRAVERSE REACHES PAST THE BODY, and that is the point of it.

        Mapped to +-1 the control could only ever move the section INSIDE the
        tissue, so at either end there was still an object and the wheel just
        stopped. Mapped past the extent, winding far enough in either
        direction carries the body out of the plane entirely and there is
        nothing there — which is what a three-dimensional section of a
        four-dimensional thing does when you push it far enough.

        The curve is expanded rather than linear so the tissue still occupies
        most of the travel: the outer fifth at each end is the empty space
        beyond the object. */
    const float wx = (clamp01 (p.winch) - 0.5f) * 2.0f;
    float winchTarget = (wx < 0.0f ? -1.0f : 1.0f)
                      * std::pow (std::fabs (wx), 1.9f) * 2.2f;
    if (p.wake > 0.0f)
    {
        subPhase += dt * (0.015f + 0.05f * p.wake);
        /*  A REAL PATROL. At 0.45 the wake moved the section by +-0.07 over a
            forty-second period — under a thin slab that is nothing, and it is
            why a held note reached its new equilibrium in twenty seconds and
            then sat there: the directed traffic had nowhere left to go. The
            wander is now the same order as the winch travel itself, so the
            destination keeps moving and a held note keeps arriving somewhere
            else. Still exactly inert at WAKE 0. */
        winchTarget += p.wake * 1.15f * thermal()
                     * (0.72f * std::sin (subPhase * 6.2831853f)
                      + 0.28f * std::sin (subPhase * 2.6180340f * 6.2831853f));
    }
    /*  THE WINCH IS WHERE IT IS HELD; PLAYING IS WHERE IT HAS BEEN TAKEN.

        These were one quantity until 260902.1, and that is why neither worked:
        a single 0.85/s recall had to be slow enough to remember a phrase and
        fast enough to make the wheel feel like a hand on the object, and it
        was neither. Now a played displacement accumulates in wDrift and is
        released over RETENTION — four seconds to two minutes — while w0 itself
        chases winch-plus-drift in a third of a second, so the wheel is",
        immediate and the phrase is remembered. */
    wDrift += wVel * dt;
    wVel *= std::exp (-dt / 1.4f);
    const float retTau = 4.0f * std::pow (30.0f, clamp01 (p.retention));
    wDrift *= std::exp (-dt / retTau);
    wDrift = clampf (wDrift, -1.4f, 1.4f);
    const float want = clampf (winchTarget + wDrift, -2.6f, 2.6f);
    w0 += (want - w0) * (1.0f - std::exp (-dt * 3.2f));
    w0 = clampf (w0, -2.6f, 2.6f);
    w0Pub.store (w0 + siteRock, std::memory_order_relaxed);   // the panel sees the rock
    wVelPub.store (wVel, std::memory_order_relaxed);

    updateSectionWeights();

    //  ---- warmth memory relaxes (tau ~ 22 s) ----
    if (p.warmth > 0.0f)
    {
        const float rel = std::exp (-dt / 22.0f);
        for (int k = 0; k < s.nModes; ++k) warmMem[(size_t) k] *= rel;
    }

    //  ---- world-mod tremolo phase ----
    if (wmActive && wmTremD > 0.0f)
        wmPhase = std::fmod (wmPhase + wmTremR * dt, 1.0f);

    /*  Migration is a crawl, deliberately: at default the strike profile
        survives for seconds (that profile IS the specimen speaking); at full
        METABOLISM it audibly travels within a second. The first build had
        this ~13x hotter and every note equalised into the same wash before
        the ear could hold it — the bench even said so (the anatomy probe had
        to measure "before migration equalises") and I read past it. */
    const float coupleRate = p.metabolism * p.metabolism * 6.5f * dt
                           * (1.0f + p.membrane * 1.5f) * thermal();

    /*  RINGDOWN + BREATH tables, once per tick.  Fast modes die first, so a
        strike audibly darkens over seconds; the sustain floor each mode
        decays TOWARD (while the aperture is open) undulates at its own slow
        deterministic rate, phase taken from the global clock — the process
        was already running, the key merely opened onto it.  Under full
        REVIVAL the artefact holds its breath: the recurrence must be exact. */
    const float ap = clamp01 (p.aperture);
    const float rev = clamp01 (p.revival);
    const float tauBase = 2.0f + 7.0f * ap;
    const float tNow = (float) ((double) tGlobal / (double) fs);

    /*  THE SITE. With a pull, the local site phase free-runs at the site's
        rate, eases onto the negotiated phase, and the sustain floor swells
        with the bench — on a FOUR-WRAP breath, because this body's modes
        reach their floor over seconds (tau 2-9 s) and a swell at the site's
        own ~0.5 Hz would be filtered to nothing: measured 0.010 vs 0.010.
        A breath of four pulses is what the body can actually carry, and it
        is still the bench's time. Without a pull the multiplier is exactly
        1.0f and the phase does not even advance. */
    {
        const float sPull = sitePull.load();
        if (sPull > 0.0f)
        {
            const double before = sitePhase;
            double target = (double) sitePhaseIn.load();
            double diff = target - sitePhase;
            diff -= std::floor (diff + 0.5);
            sitePhase += diff * 0.35 + (double) siteHz.load() * (double) dt;
            sitePhase -= std::floor (sitePhase);
            if (sitePhase < before - 0.5) siteCycle = (siteCycle + 1) & 3;     // a wrap
            const double breath = ((double) siteCycle + sitePhase) * 0.25;
            siteSwell = 1.0f + sPull * 0.35f * std::sin (6.2831853f * (float) breath);
            siteNearWrap = sitePhase < 0.10 || sitePhase > 0.90;
            /*  ...and THE SECTION ROCKS with the bench, at the site's own
                rate. This is the lean you hear: the slab is what decides which
                modes are audible, and it answers within a tick — the sustain
                breath above is real but slow. On the panel the tissue visibly
                rocks through the plane in time with the other findings. */
            siteRock = sPull * 0.10f * std::sin (6.2831853f * (float) sitePhase);
        }
        else { siteSwell = 1.0f; siteNearWrap = true; siteRock = 0.0f; }
    }

    //  cold, nothing sustains: a strike rings down and that is all
    const float sustain = (0.16f + 0.34f * ap) * thermal() * siteSwell;
    for (int k = 0; k < s.nModes; ++k)
    {
        const float tau = std::max (0.35f,
            tauBase * std::pow (std::max (1.0f, s.ratio[k]), -0.55f));
        decayMul[(size_t) k] = std::exp (-dt / tau);
        const uint32_t h = (uint32_t) (k * 2654435761u);
        const float bf = 0.03f + 0.17f * (float) ((h >> 8) & 1023) / 1023.0f;
        const float bp = (float) (h & 1023) / 1023.0f;
        float br = 0.5f + 0.5f * std::sin (6.2831853f * (bf * tNow + bp));
        br = br * std::sqrt (br);                     // shaped: long dwells
        breathW[(size_t) k] = sustain * ((1.0f - rev) * br + rev * 0.5f);
    }

    for (auto& v : voices)
    {
        if (! v.active) continue;

        //  MIGRATION: pairwise conserving exchange along the anatomy
        if (coupleRate > 0.0f)
            for (int k = 0; k < s.nModes; ++k)
                for (int t = 0; t < kCouplePer; ++t)
                {
                    const int l = s.coupleTo[(size_t) k * kCouplePer + t];
                    if (l <= k) continue;                    // each pair once
                    const float cw = s.coupleW[(size_t) k * kCouplePer + t];
                    /*  DIRECTED, not merely diffusive. Pure diffusion has one
                        equilibrium and reaches it: measured, a held note's
                        colour was static within a few seconds and then wandered
                        +-8 % for the next thirty. Weighting the exchange by how
                        much of each mode lies in the SECTION gives the traffic
                        somewhere to go, and because the section itself moves —
                        with the wake, with the winch, with everything played —
                        the destination keeps moving and the note keeps
                        travelling. At sectPull 0 this is exactly the old
                        (e_l - e_k), and it is still conserving: what leaves l
                        arrives at k. */
                    const float aK = 1.0f + 0.40f * sectW[(size_t) k];
                    const float aL = 1.0f + 0.40f * sectW[(size_t) l];
                    const float d = clampf (coupleRate * cw, 0.0f, 0.25f) * 0.5f
                                  * (v.energy[(size_t) l] * aK
                                   - v.energy[(size_t) k] * aL);
                    v.energy[(size_t) k] += d;
                    v.energy[(size_t) l] -= d;
                }

        //  RINGDOWN: each mode decays toward its breathing sustain floor
        //  while the aperture is open, toward silence once released.
        for (int k = 0; k < s.nModes; ++k)
        {
            const float fl = v.releasing ? 0.0f
                           : breathW[(size_t) k] * v.exciteW[(size_t) k];
            v.energy[(size_t) k] = fl
                + (v.energy[(size_t) k] - fl) * decayMul[(size_t) k];
        }

        //  substrate: WAKE feeds whispers of energy into wandering tissue.
        //  Exactly absent at zero (no RNG consulted — determinism contract).
        if (p.wake > 0.0f)
        {
            const int k = subRng.irange (s.nModes);
            v.energy[(size_t) k] += p.wake * 0.00004f;
        }

        //  release / self-closing aperture handling
        if (v.gateSamples > 0)
        {
            v.gateSamples -= kCtrl;
            if (v.gateSamples <= 0) v.releasing = true;
        }
        if (v.releasing)
        {
            const float relT = v.killFast ? 0.012f
                              : 0.05f + 0.5f * clamp01 (p.aperture);
            v.apStep = -1.0f / std::max (1.0f, relT * (float) fs);
        }

        //  rotors follow the moving spectrum every tick — cheap, and no
        //  stale-frequency corner when REVIVAL or MEMBRANE returns to zero
        updateVoiceRotors (v, true);

        /*  GRAVITY, live: a register lean on the realised amplitudes,
            renormalised inside the voice's own energy budget so it changes
            COLOUR, never loudness — the conservation law audible under a
            moving hand. tiltW is scratch, reused per tick. */
        const float lean = (p.gravity - 0.5f) * 2.4f;
        float eSum = 1e-12f, tSum = 1e-12f;
        for (int k = 0; k < s.nModes; ++k)
        {
            const float tw = std::pow (s.ratio[k], -lean);
            tiltW[(size_t) k] = tw;
            const float ek = std::max (0.0f, v.energy[(size_t) k]);
            eSum += ek;
            tSum += ek * tw * tw;
        }
        const float tiltNorm = std::sqrt (eSum / tSum);

        //  realised amplitude targets: energy x tilt x section, ramped
        for (int k = 0; k < s.nModes; ++k)
        {
            float target = std::sqrt (std::max (0.0f, v.energy[(size_t) k]))
                         * tiltW[(size_t) k] * tiltNorm
                         * sectW[(size_t) k];
            if (v.freq[(size_t) k] <= 0.0f) target = 0.0f;
            v.ampStep[(size_t) k] = (target - v.amp[(size_t) k]) / (float) kCtrl;
        }

        if (v.apEnv <= 0.0f && v.releasing)
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
            /*  fast enough to be an EXPERIENCE: at full plasticity an accent
                step lands every few seconds of playing, at the default it is
                the work of minutes. A change nobody lives long enough to
                hear is not a feature. */
            const float rate = p.plastic * p.plastic * 0.02f;
            //  the played body, remodelled by what YOU do to it
            for (int k = 0; k < s.nModes; ++k)
            {
                float e = 0;
                for (const auto& v : voices)
                    if (v.active) e += v.amp[(size_t) k] * v.amp[(size_t) k];
                modeE[(size_t) k] = e;
            }
            //  the fiction, and the balance: LISTENING is what remodels. The
            //  played body is marked by use too, but a fifth as fast.
            accumulateScar (s, modeE.data(), scarA, rate * 0.2f);

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

        /*  Applied only at rest — between utterances, like healing, so a
            body never remodels under a sounding note. But each body has its
            OWN rest: the played one is busy only while a voice sounds, the
            listener only while it is speaking. */
        if (! remodelReq.load (std::memory_order_relaxed))
        {
            bool voiceQuiet = true;
            for (const auto& v : voices) if (v.active) { voiceQuiet = false; break; }

            if (voiceQuiet && ! remodelA.load (std::memory_order_relaxed))
            {
                float mx = 0;
                for (int e = 0; e < s.nEdges; ++e) mx = std::max (mx, scarA[(size_t) e]);
                if (mx > 0.05f) remodelA.store (true, std::memory_order_relaxed);
            }
            if (conv && ! il.speaking && ! remodelB.load (std::memory_order_relaxed))
            {
                float mx = 0;
                for (int e = 0; e < otherSpecimen().nEdges; ++e)
                    mx = std::max (mx, scarB[(size_t) e]);
                if (mx > 0.05f) remodelB.store (true, std::memory_order_relaxed);
            }
            if (remodelA.load (std::memory_order_relaxed)
             || remodelB.load (std::memory_order_relaxed))
                remodelReq.store (true, std::memory_order_release);
        }
    }
}

/*  The second body's life, once per control tick: what it heard decays,
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
    /*  MUTUAL INTELLIGIBILITY, and it must be the quantity the name claims:
        the mean fraction of its own modes that actually rang. Summing 67
        unity-gain resonators against the total power instead (the first
        version) saturates for every pair and says nothing. */
    if (il.sentPow > 1e-10f)
    {
        float kinAcc = 0.0f;
        for (int k = 0; k < os.nModes; ++k)
            kinAcc += clampf (il.heardPow[(size_t) k] / il.sentPow, 0.0f, 1.0f);
        il.lastKin = kinAcc / (float) os.nModes;
    }
    else il.lastKin = 0.0f;

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
    il.sinceReply += dt;
    /*  It answers in your silences — and INTERJECTS if you have been going
        on. A body that only ever spoke into gaps would be mute under a held
        drone, and the membrane, which needs both voices at once, would
        almost never exist. */
    const bool heardEnough = il.sentPow > 2.0e-7f && heardTot > 0.0f;
    /*  An interjection is something you do while the other party is STILL
        TALKING. Keyed on elapsed time alone it kept interrupting an empty
        room for the best part of a minute after playing stopped — and a body
        that never falls silent never heals, so no accent was ever applied. */
    //  ...and, on a shared bench, it waits for the site's wrap to do so
    const bool interject = il.sinceReply > 5.0f && il.spkEnv > 0.004f && siteNearWrap;
    const bool wantReply = (! il.speaking) && heardEnough
                        && (il.quietFor > 0.18f || interject);
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

        //  loudness: kin answer strongly, strangers faintly but never mutely.
        //  Measured range of lastKin across pairs is ~0.15..0.65, so the
        //  scaling has to live there or every pair sounds equally understood.
        const float kin = clampf (il.lastKin * 1.5f, 0.0f, 1.0f);
        /*  calibrated so a body answers a vel-0.9 note at conversational
            level — roughly half the speaker. At a tenth of that it was
            technically present and practically inaudible, which is the same
            thing as broken. Bounded, because the input scales it. */
        const float A = std::min (1.2f, clamp01 (p.converse) * 90.0f
                      * std::sqrt (il.sentPow) * (0.30f + 0.70f * kin));
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
        il.sinceReply = 0.0f;
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
{
    const Specimen& s = specimen();
    const float master = p.volume < 0.005f ? 0.0f : 2.0f * p.volume * p.volume;
    const bool conv = p.converse > 0.0f && oLoaded >= 0 && otherSpecimen().nModes > 0;

    for (int i = 0; i < n; ++i)
    {
        if (ctrlPhase == 0) controlTick();
        if (++ctrlPhase >= kCtrl) ctrlPhase = 0;

        float outL = 0.0f, outR = 0.0f;

        for (auto& v : voices)
        {
            if (! v.active) continue;

            //  aperture envelope (raised-cosine feel via smoothstep of ramp)
            v.apEnv = clampf (v.apEnv + v.apStep, 0.0f, 1.0f);
            const float ap = v.apEnv * v.apEnv * (3.0f - 2.0f * v.apEnv);
            ++v.age;

            float vl = 0.0f, vr = 0.0f;
            const int nm = s.nModes;
            for (int k = 0; k < nm; ++k)
            {
                //  rotate
                const float x = v.oscX[(size_t) k], y = v.oscY[(size_t) k];
                const float c = v.rotC[(size_t) k], sn = v.rotS[(size_t) k];
                const float nx = x * c - y * sn;
                const float ny = x * sn + y * c;
                v.oscX[(size_t) k] = nx;
                v.oscY[(size_t) k] = ny;

                v.amp[(size_t) k] += v.ampStep[(size_t) k];
                const float a = v.amp[(size_t) k];
                if (a <= 0.0f) continue;

                //  per-mode binaural: the two ears listen at two nodes of
                //  the body; a small phase skew where the eigenvector flips
                vl += a * s.ampL[k] * nx;
                vr += a * s.ampR[k] * (s.phaseSkew[k] > 0.0f
                                        ? (nx * 0.913f - ny * 0.408f) : nx);
            }

            float g = ap;
            if (wmActive && wmTremD > 0.0f)
            {
                const float ph = wmPhase + 0.6180339887f * (float) v.note;
                g *= 1.0f - wmTremD * 0.5f
                     * (1.0f - std::cos (6.2831853f * ph));
            }
            outL += vl * g;
            outR += vr * g;
        }

        /*  ---- THE INTERLOCUTOR: it hears you, and answers ---- */
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
                //  a true product, so the sidebands are real; bounded, so a
                //  loud pair cannot detonate it
                const float m = clampf (p.commune * 55.0f * spk * 0.5f * (ilL + ilR),
                                        -0.30f, 0.30f);
                outL += m; outR += m;
            }
            outL += ilL; outR += ilR;
        }

        const float sc = master * 0.9f;
        L[i] = ceilSoft (outL * sc);
        R[i] = ceilSoft (outR * sc);
        ++tGlobal;
    }

    //  world-mod pan spread (cheap, block-level; neutral at 0 exactly)
    if (wmActive && wmPan > 0.0f)
    {
        const float m = wmPan * 0.35f;
        for (int i = 0; i < n; ++i)
        {
            const float mid = 0.5f * (L[i] + R[i]);
            L[i] = L[i] + m * (L[i] - mid);
            R[i] = R[i] + m * (R[i] - mid);
        }
    }
}

} // namespace ab
