// 260905.1 — THE BODIES: engine (second head, split lines, scan spread, the
// MOD line on the read), anatomy specimens in HU, processor (import dial,
// revert lines, hu in the volume event, dial migration 12 -> 15).
// Exact-count anchors; nothing is written unless every anchor matches.
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
const BT = String.fromCharCode(96);

function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const rep = (from, to, count = 1) => {
    const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
    const n = s.split(f).length - 1;
    if (n !== count) { misses.push(`${rel}: expected ${count} of [${from.slice(0, 70).replace(/\n/g, "\\n")}...], found ${n}`); return; }
    s = s.split(f).join(t);
  };
  fn(rep);
  return { p, get: () => s };
}

// ============================================================ Engine.h
const eh = edit("Source/Engine.h", (rep) => {
  rep(String.raw`enum PKind { KP_PCT = 0, KP_INT, KP_LIST, KP_BIPOL, KP_HZ, KP_VOL, KP_SEMI, KP_SEC, KP_CENT };`,
      String.raw`enum PKind { KP_PCT = 0, KP_INT, KP_LIST, KP_BIPOL, KP_HZ, KP_VOL, KP_SEMI, KP_SEC, KP_CENT, KP_RATIO };
//  KP_RATIO: 2^((v - 0.5) * 4), i.e. 0.25x .. 4x with exactly 1x in the middle
inline double ratioOf (float v) { return std::pow (2.0, ((double) v - 0.5) * 4.0); }`);
  rep(String.raw`    Vec3  p[MAXPTS];
    int   n = 2;
    bool  closed = false;
    float start = 0.0f;
    float warp = 0.0f;

    Vec3  at (float s) const;                       // s in [0,1)`,
      String.raw`    Vec3  p[MAXPTS];
    int   n = 2;
    bool  closed = false;
    float start = 0.0f;
    float warp = 0.0f;
    /*  A line that is not one curve: split > 0 makes points [0, split) one
        open segment and [split, n) another, read in the two halves of the
        cycle — one cycle, TWO edges, a different family of timbre. Valid
        when both halves have at least two points. */
    int   split = 0;

    bool  isSplit() const { return split >= 2 && split <= n - 2; }
    bool  hasWrapEdge() const { return ! closed || isSplit(); }
    Vec3  at (float s) const;                       // s in [0,1)`);
  rep(String.raw`    float grain    = 0.35f, contrast = 0.0f, fold = 0.0f;`,
      String.raw`    float grain    = 0.35f, contrast = 0.0f, fold = 0.0f;
    /*  THE SECOND HEAD (260905.1): a second reader on the same blended line,
        at a RATIO of the note's speed (1x in the middle: a fixed-interval
        double when PHASE is offset) or off the integers (a partial that is
        not a harmonic — the one thing a single-cycle read cannot be). */
    float head2    = 0.0f, head2Ratio = 0.5f, head2Phase = 0.0f;
    float uniScan  = 0.0f;                                   // unison readers spread through the scan
    float modContrast = 0.5f, modGrain = 0.5f;               // bipolar: the MOD line drives the read`);
  rep(String.raw`        float  edge = 0;             // the wrap jump for the polyBLEP, per tick
        float  shX = 0;              // the window shaper's previous input (ADAA)
    };`,
      String.raw`        float  edge = 0;             // the wrap jump for the polyBLEP, per tick
        float  edge2 = 0;            // the jump at phase 0.5 of a SPLIT line
        float  shX = 0;              // the window shaper's previous input (ADAA)
        double ph2 = 0;              // the second head's own phase
    };`);
  rep(String.raw`        float  tremPh = 0;
        int    lastTickAbs = -1;
    };`,
      String.raw`        float  tremPh = 0;
        int    lastTickAbs = -1;
        //  the read, per voice: the MOD line may move the kernel and the window
        float  kB = 1.0f, kC = 0.0f;
        float  shGain = 1.0f;
        bool   shOn = false;
    };`);
  rep(String.raw`    //  the read, per block: the kernel and the audio window
    float  kB = 1.0f, kC = 0.0f;
    float  shGain = 1.0f, shFold = 0.0f;
    bool   shOn = false;`,
      String.raw`    //  the read, per block: the fold (the kernel and the gain are per voice)
    float  shFold = 0.0f;`);
  rep(String.raw`const char* specimenName (int i);`,
      String.raw`const char* specimenName (int i);
bool        specimenIsHu (int i);        // built in Hounsfield units (-1000 .. 2000 across the cube's 0 .. 1)
const char* specimenWindow (int i);      // the radiographer's window it opens on: BRAIN, SOFT, LUNG, BONE, or ""`);
});

// ============================================================ Engine.cpp
const ec = edit("Source/Engine.cpp", (rep) => {
  rep(String.raw`    { "fold",      "FOLD",       "what the window does past its edges: clip, or fold back in",  0.00f, KP_PCT, 0, 0, P(fold),     nullptr, 0 },`,
      String.raw`    { "fold",      "FOLD",       "what the window does past its edges: clip, or fold back in",  0.00f, KP_PCT, 0, 0, P(fold),     nullptr, 0 },
    { "head2",     "2ND HEAD",   "a second reader on the same line, mixed in",                  0.00f, KP_PCT, 0, 0, P(head2),    nullptr, 0 },
    { "head2Ratio","HEAD RATIO", "the second head's speed: an interval, or a partial that is not a harmonic", 0.5f, KP_RATIO, 0, 0, P(head2Ratio), nullptr, 0 },
    { "head2Phase","HEAD PHASE", "where along the cycle the second head starts",                0.00f, KP_PCT, 0, 0, P(head2Phase), nullptr, 0 },
    { "uniScan",   "SCAN SPREAD","unison readers spread through the scan, each a little off the others", 0.00f, KP_PCT, 0, 0, P(uniScan), nullptr, 0 },
    { "modContrast","MOD>WINDOW","how much the MOD line narrows the window",                    0.50f, KP_BIPOL, 0, 0, P(modContrast), nullptr, 0 },
    { "modGrain",  "MOD>GRAIN",  "how much the MOD line sharpens the read",                     0.50f, KP_BIPOL, 0, 0, P(modGrain), nullptr, 0 },`);
  rep(String.raw`Vec3 Line::at (float sIn) const
{
    if (n <= 1) return p[0];
    float s = sIn - std::floor (sIn);
    s = warpPhase (s, clampf (warp, -0.95f, 0.95f));
    if (closed) { s += start; s -= std::floor (s); }
    const int nseg = closed ? n : n - 1;`,
      String.raw`//  Catmull-Rom through n points, open (phantom ends) or closed, at s in [0,1)
static Vec3 crEval (const Vec3* p, int n, bool closed, float s)
{
    const int nseg = closed ? n : n - 1;`);
  rep(String.raw`    Vec3 r { cr (p0.x, p1.x, p2.x, p3.x), cr (p0.y, p1.y, p2.y, p3.y), cr (p0.z, p1.z, p2.z, p3.z) };
    r.x = clamp01 (r.x); r.y = clamp01 (r.y); r.z = clamp01 (r.z);
    return r;
}`,
      String.raw`    Vec3 r { cr (p0.x, p1.x, p2.x, p3.x), cr (p0.y, p1.y, p2.y, p3.y), cr (p0.z, p1.z, p2.z, p3.z) };
    r.x = clamp01 (r.x); r.y = clamp01 (r.y); r.z = clamp01 (r.z);
    return r;
}

Vec3 Line::at (float sIn) const
{
    if (n <= 1) return p[0];
    float s = sIn - std::floor (sIn);
    s = warpPhase (s, clampf (warp, -0.95f, 0.95f));
    if (isSplit())
    {
        //  two open segments: the first half of the cycle on one, the second on the other
        const bool second = s >= 0.5f;
        const float ss = second ? (s - 0.5f) * 2.0f : s * 2.0f;
        return second ? crEval (p + split, n - split, false, ss) : crEval (p, split, false, ss);
    }
    if (closed) { s += start; s -= std::floor (s); }
    return crEval (p, n, closed, s);
}`);
  rep(String.raw`        v.lastTickAbs = 0;
        //  the wrap jump, for the polyBLEP
        float edge = 0.0f;
        if (! (A.closed && B.closed))
        {
            const Vec3 q0 = mix3 (A.at (0.0f), B.at (0.0f), v.scanNow);
            const Vec3 qe = mix3 (A.at (0.99999f), B.at (0.99999f), v.scanNow);
            const float w0 = vol.sampleBlend (v.lodF, q0.x, q0.y, q0.z, kB, kC);
            const float w1 = vol.sampleBlend (v.lodF, qe.x, qe.y, qe.z, kB, kC);
            edge = (w0 - w1) * 2.0f;
        }
        for (int k = 0; k < v.nUni; ++k) v.u[k].edge = edge;
    }`,
      String.raw`        v.lastTickAbs = 0;
        /*  The read, for this voice: the kernel and the window, which the MOD
            line may move. At MOD>WINDOW and MOD>GRAIN centred the offset is
            an exact +0.0, so the knob values pass through unchanged. */
        {
            const float g = clamp01 (p.grain + v.modV * (p.modGrain * 2.0f - 1.0f));
            v.kB = 1.0f - g; v.kC = 0.5f * g;
            const float c = clamp01 (p.contrast + v.modV * (p.modContrast * 2.0f - 1.0f));
            v.shGain = std::pow (2.0f, 4.0f * c);
            v.shOn = c > 0.0005f || shFold > 0.0005f;
        }
        //  the wrap jump, for the polyBLEP — and the jump at half a cycle of a SPLIT line
        float edge = 0.0f, edge2 = 0.0f;
        if (A.hasWrapEdge() || B.hasWrapEdge())
        {
            const Vec3 q0 = mix3 (A.at (0.0f), B.at (0.0f), v.scanNow);
            const Vec3 qe = mix3 (A.at (0.99999f), B.at (0.99999f), v.scanNow);
            const float w0 = vol.sampleBlend (v.lodF, q0.x, q0.y, q0.z, v.kB, v.kC);
            const float w1 = vol.sampleBlend (v.lodF, qe.x, qe.y, qe.z, v.kB, v.kC);
            edge = (w0 - w1) * 2.0f;
        }
        if (A.isSplit() || B.isSplit())
        {
            const Vec3 q0 = mix3 (A.at (0.5f), B.at (0.5f), v.scanNow);
            const Vec3 qe = mix3 (A.at (0.49999f), B.at (0.49999f), v.scanNow);
            const float w0 = vol.sampleBlend (v.lodF, q0.x, q0.y, q0.z, v.kB, v.kC);
            const float w1 = vol.sampleBlend (v.lodF, qe.x, qe.y, qe.z, v.kB, v.kC);
            edge2 = (w0 - w1) * 2.0f;
        }
        for (int k = 0; k < v.nUni; ++k) { v.u[k].edge = edge; v.u[k].edge2 = edge2; }
    }`);
  rep(String.raw`    const float uniGain = 1.0f / std::sqrt ((float) v.nUni);
    float panOff = v.panMod;`,
      String.raw`    const float uniGain = 1.0f / std::sqrt ((float) v.nUni);
    //  the second head: its mix, its speed, its offset; and how far unison readers spread through the scan
    const float h2Mix = clamp01 (p.head2);
    const bool  h2On = h2Mix > 0.0005f;
    const double h2Ratio = ratioOf (p.head2Ratio);
    const float h2Phase = clamp01 (p.head2Phase);
    const float h2Norm = 1.0f / (1.0f + 0.5f * h2Mix);
    const float lod2 = v.lodF + (float) std::log2 (std::max (1.0, h2Ratio));   // the faster head reads coarser
    const float uniSc = clamp01 (p.uniScan) * 0.3f;
    float panOff = v.panMod;`);
  rep(String.raw`            const double inc = fBase * det / sr;
            const double ph = u.ph;
            const Vec3 q = mix3 (A.at ((float) ph), B.at ((float) ph), scan);
            float w = vol.sampleBlend (v.lodF, q.x, q.y, q.z, kB, kC) * 2.0f - 1.0f;
            if (blepOn && u.edge != 0.0f)
            {
                //  the wrap is at phase 0: after it (ph small) and before it (ph near 1)
                const float dts = (float) inc;
                if (ph < 2.0 * dts)             w += u.edge * blep ((float) ph / dts);
                else if (ph > 1.0 - 2.0 * dts)  w += u.edge * blep (((float) ph - 1.0f) / dts);
            }
            if (shOn)
            {`,
      String.raw`            const double inc = fBase * det / sr;
            const double ph = u.ph;
            //  SCAN SPREAD: reader 0 sits on the scan exactly, the others fan around it
            const float fanK = std::fmod ((float) k * 0.618f + 0.5f, 1.0f) * 2.0f - 1.0f;
            const float scanK = clamp01 (scan + uniSc * fanK);
            const Vec3 q = mix3 (A.at ((float) ph), B.at ((float) ph), scanK);
            float w = vol.sampleBlend (v.lodF, q.x, q.y, q.z, v.kB, v.kC) * 2.0f - 1.0f;
            if (blepOn)
            {
                //  the wrap is at phase 0: after it (ph small) and before it (ph near 1)
                const float dts = (float) inc;
                if (u.edge != 0.0f)
                {
                    if (ph < 2.0 * dts)             w += u.edge * blep ((float) ph / dts);
                    else if (ph > 1.0 - 2.0 * dts)  w += u.edge * blep (((float) ph - 1.0f) / dts);
                }
                if (u.edge2 != 0.0f)
                {
                    const float d = (float) ph - 0.5f;
                    if (std::abs (d) < 2.0f * dts) w += u.edge2 * blep (d / dts);
                }
            }
            if (h2On)
            {
                //  the second head: the same line, its own phase, mixed in before the window
                double ph2 = u.ph2 + (double) h2Phase; ph2 -= std::floor (ph2);
                const Vec3 q2 = mix3 (A.at ((float) ph2), B.at ((float) ph2), scanK);
                float w2 = vol.sampleBlend (lod2, q2.x, q2.y, q2.z, v.kB, v.kC) * 2.0f - 1.0f;
                if (blepOn)
                {
                    const float dts = (float) (inc * h2Ratio);
                    if (u.edge != 0.0f)
                    {
                        if (ph2 < 2.0 * dts)            w2 += u.edge * blep ((float) ph2 / dts);
                        else if (ph2 > 1.0 - 2.0 * dts) w2 += u.edge * blep (((float) ph2 - 1.0f) / dts);
                    }
                    if (u.edge2 != 0.0f)
                    {
                        const float d = (float) ph2 - 0.5f;
                        if (std::abs (d) < 2.0f * dts) w2 += u.edge2 * blep (d / dts);
                    }
                }
                w = (w + h2Mix * w2) * h2Norm;
                u.ph2 += inc * h2Ratio; u.ph2 -= std::floor (u.ph2);
            }
            if (v.shOn)
            {`);
  rep(String.raw`                const float x = w * shGain, xp = u.shX;`,
      String.raw`                const float x = w * v.shGain, xp = u.shX;`);
  rep(String.raw`    //  the read, for this block: the kernel and the audio window
    {
        const float g = clamp01 (p.grain);
        kB = 1.0f - g; kC = 0.5f * g;
        const float c = clamp01 (p.contrast);
        shGain = std::pow (2.0f, 4.0f * c);
        shFold = clamp01 (p.fold);
        shOn = c > 0.0005f || shFold > 0.0005f;
    }`,
      String.raw`    //  the read, for this block: the fold (the kernel and the gain are per voice, in tick)
    shFold = clamp01 (p.fold);`);
  rep(String.raw`        if (! retrigger || ! v.active) { u.ph = 0; u.svf.reset(); }`,
      String.raw`        if (! retrigger || ! v.active) { u.ph = 0; u.ph2 = 0; u.shX = 0; u.svf.reset(); }`);
});

// ============================================================ Specimens.cpp
const sp = edit("Source/Specimens.cpp", (rep) => {
  rep(String.raw`#include "Engine.h"
#include <cmath>
#include <cstring>`,
      String.raw`#include "Engine.h"
#include "Anatomy.h"
#include <cmath>
#include <cstring>`);
  rep(String.raw`struct SpecDef { const char* name; const char* gloss; float (*f) (float x, float y, float z); bool periodicX; };`,
      String.raw`struct SpecDef { const char* name; const char* gloss; float (*f) (float x, float y, float z); bool periodicX; bool hu; const char* window; };`);
  rep(String.raw`static float fSkull (float x, float y, float z)
{
    const float r = std::sqrt ((x - 0.5f) * (x - 0.5f) + (y - 0.5f) * (y - 0.5f) + (z - 0.5f) * (z - 0.5f));
    const float shell = smooth (0.34f, 0.38f, r) * (1.0f - smooth (0.42f, 0.46f, r));
    const float inside = (1.0f - smooth (0.30f, 0.36f, r)) * 0.25f;
    return clamp01f (shell + inside);
}`,
      String.raw`/*  THE BODIES (260905.1) are built in Hounsfield units in Anatomy.cpp and
    mapped -1000..2000 HU onto the cube's 0..1 — so a radiographer's window
    means what it says on them, and a bone is a bone. */
static float fSkull    (float x, float y, float z) { return an::toUnit (an::head (x, y, z, false)); }
static float fHead     (float x, float y, float z) { return an::toUnit (an::head (x, y, z, true)); }
static float fThorax   (float x, float y, float z) { return an::toUnit (an::thorax (x, y, z)); }
static float fVertebra (float x, float y, float z) { return an::toUnit (an::vertebra (x, y, z)); }
static float fFemur    (float x, float y, float z) { return an::toUnit (an::femur (x, y, z)); }
static float fJaw      (float x, float y, float z) { return an::toUnit (an::jaw (x, y, z)); }`);
  rep(String.raw`static float fLung (float x, float y, float z)
{
    const float n = fbm (x * 5.0f + 3.1f, y * 5.0f + 7.7f, z * 5.0f + 1.3f, 3);
    return clamp01f (0.5f + (n - 0.5f) * 2.2f);
}

`, "");
  rep(String.raw`static float fCortex (float x, float y, float z)
{
    /*  A head in the cube: skull (bright), CSF (dark), the folded cortex (grey
        with sulci), the longitudinal fissure, two ventricles, a cerebellum with
        finer folds, a brainstem. Densities as a CT window would show them. */
    const float px = (x - 0.5f) / 0.40f, py = (y - 0.5f) / 0.34f, pz = (z - 0.46f) / 0.36f;
    const float E = px * px + py * py + pz * pz;                     // 1 at the skull's outer surface
    float d = 0.0f;
    //  skull
    d += 1.0f * smooth (1.10f, 0.98f, E) * smooth (0.86f, 0.94f, E);
    //  CSF between skull and brain
    d += 0.18f * smooth (0.94f, 0.86f, E) * smooth (0.76f, 0.84f, E);
    //  the cerebrum: an ellipsoid, folded
    {
        const float inBrain = smooth (0.84f, 0.74f, E);
        const float g = fbm (x * 9.0f + 11.0f, y * 9.0f + 5.0f, z * 9.0f + 2.0f, 3, 2.1f, 0.55f);
        const float g2 = fbm (x * 17.0f + 1.0f, y * 17.0f + 9.0f, z * 17.0f + 4.0f, 2, 2.0f, 0.5f);
        const float fold = 0.55f + 0.45f * std::sin (g * 14.0f + g2 * 4.0f);
        float grey = 0.42f + 0.22f * fold;                            // gyri 0.64, sulci 0.42
        grey *= 1.0f - 0.35f * smooth (0.50f, 0.42f, std::sqrt (E)) * (1.0f - fold);   // sulci deepen toward the surface
        //  the longitudinal fissure, upper half
        const float fiss = (1.0f - smooth (0.012f, 0.03f, std::abs (x - 0.5f))) * smooth (0.38f, 0.55f, z);
        grey *= 1.0f - 0.7f * fiss;
        //  ventricles: two dark chambers, lateral
        for (int sgn = -1; sgn <= 1; sgn += 2)
        {
            const float vx = (x - (0.5f + 0.09f * (float) sgn)) / 0.06f, vy = (y - 0.50f) / 0.12f, vz = (z - 0.46f) / 0.05f;
            const float V = vx * vx + vy * vy + vz * vz;
            grey *= 1.0f - 0.85f * smooth (1.4f, 0.8f, V);
        }
        d += grey * inBrain * smooth (0.28f, 0.34f, z);               // nothing below the tentorium
    }
    //  the cerebellum: a smaller folded blob, back and low
    {
        const float cx = (x - 0.5f) / 0.22f, cy = (y - 0.72f) / 0.14f, cz = (z - 0.30f) / 0.11f;
        const float C = cx * cx + cy * cy + cz * cz;
        const float g = fbm (x * 24.0f, y * 24.0f + 3.0f, z * 24.0f, 2);
        const float fold = 0.5f + 0.5f * std::sin (g * 18.0f);
        d += (0.44f + 0.20f * fold) * smooth (1.15f, 0.85f, C);
    }
    //  the brainstem: a short cylinder down from the centre
    {
        const float sx = (x - 0.5f) / 0.06f, sy = (y - 0.56f) / 0.06f;
        const float S = sx * sx + sy * sy;
        d += 0.5f * smooth (1.3f, 0.8f, S) * smooth (0.34f, 0.28f, z) * smooth (0.06f, 0.14f, z);
    }
    return clamp01f (d);
}

`, "");
  rep(String.raw`    { "SINUS",  "a sine; y its amplitude, z a touch of second harmonic",                fSinus,  true  },
    { "SPINE",  "a harmonic stack; y brightness, z parity (saw to square)",             fSpine,  true  },
    { "PULSE",  "a pulse; y its width, z the softness of its edges",                    fPulse,  true  },
    { "MARROW", "a ramp; y its curvature, z folds it toward a triangle",                fMarrow, false },
    { "NERVE",  "a phase-modulated sine; y the index, z the modulator's ratio",         fNerve,  true  },
    { "RETINA", "two decaying formants per cycle; y and z their positions",             fRetina, false },
    { "LUNG",   "smooth noise: rough, breathy, metallic on a loop",                     fLung,   false },
    { "SKULL",  "a hollow sphere: two bumps per cycle through the centre",              fSkull,  false },
    { "CORTEX", "the simulated brain: skull, folded cortex, fissure, ventricles",       fCortex, false },
    { "SUTURE", "a random staircase; y the steps per cycle, z walks the patterns",      fSuture, true  },
    { "ENAMEL", "a comb of spikes; z how many per cycle, y their width",                fEnamel, true  },
    { "TENDON", "four partials; y slides them from harmonic to a bell, z brightness",   fTendon, false },
};
static const int NSPECIMENS = (int) (sizeof (SPECIMENS) / sizeof (SPECIMENS[0]));
static_assert (NSPECIMENS == 12, "the SPECIMEN parameter has twelve slots; change both or neither");`,
      String.raw`    //  the phantoms: calibration objects, each a formula, each a promise
    { "SINUS",  "a sine; y its amplitude, z a touch of second harmonic",                fSinus,  true,  false, ""      },
    { "SPINE",  "a harmonic stack; y brightness, z parity (saw to square)",             fSpine,  true,  false, ""      },
    { "PULSE",  "a pulse; y its width, z the softness of its edges",                    fPulse,  true,  false, ""      },
    { "MARROW", "a ramp; y its curvature, z folds it toward a triangle",                fMarrow, false, false, ""      },
    { "NERVE",  "a phase-modulated sine; y the index, z the modulator's ratio",         fNerve,  true,  false, ""      },
    { "RETINA", "two decaying formants per cycle; y and z their positions",             fRetina, false, false, ""      },
    //  the bodies: anatomy in Hounsfield units, a bone is a bone
    { "THORAX", "a chest CT: ribs, spine, sternum, lungs with their vessels, the heart", fThorax, false, true, "LUNG"  },
    { "SKULL",  "the skull alone: vault, orbits, sinuses, jaw and teeth, no soft tissue", fSkull, false, true, "BONE"  },
    { "CORTEX", "a head CT: the same skull with brain, ventricles, eyes and scalp",      fHead,   false, true,  "BRAIN" },
    { "SUTURE", "a random staircase; y the steps per cycle, z walks the patterns",      fSuture, true,  false, ""      },
    { "ENAMEL", "a comb of spikes; z how many per cycle, y their width",                fEnamel, true,  false, ""      },
    { "TENDON", "four partials; y slides them from harmonic to a bell, z brightness",   fTendon, false, false, ""      },
    { "VERTEBRA","three lumbar vertebrae: bodies, discs, canal, processes, the muscles", fVertebra, false, true, "BONE" },
    { "FEMUR",  "a thigh: the femoral shaft, its marrow, muscle compartments, vessels",  fFemur,  false, true,  "BONE"  },
    { "JAW",    "the mandible with both rows of teeth, the tongue and the palate",      fJaw,    false, true,  "BONE"  },
};
static const int NSPECIMENS = (int) (sizeof (SPECIMENS) / sizeof (SPECIMENS[0]));
static_assert (NSPECIMENS == 15, "the SPECIMEN parameter has fifteen slots; change both or neither");`);
  rep(String.raw`bool specimenPeriodicX (int i)    { return SPECIMENS[i < 0 ? 0 : (i >= NSPECIMENS ? NSPECIMENS - 1 : i)].periodicX; }`,
      String.raw`bool specimenPeriodicX (int i)    { return SPECIMENS[i < 0 ? 0 : (i >= NSPECIMENS ? NSPECIMENS - 1 : i)].periodicX; }
bool specimenIsHu (int i)         { return SPECIMENS[i < 0 ? 0 : (i >= NSPECIMENS ? NSPECIMENS - 1 : i)].hu; }
const char* specimenWindow (int i) { return SPECIMENS[i < 0 ? 0 : (i >= NSPECIMENS ? NSPECIMENS - 1 : i)].window; }`);
  rep(String.raw`static const FactoryPatch FACTORY[] =
{
    { "ADMISSION",     facAdmission   },`,
      String.raw`//  the bodies, played: each patch shows one of the 260905.1 controls off
static void facSkull (Line* l, Params& p)
{
    p.specimen = listVal (7, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.02f, 0.62f, 0.56f }, { 0.98f, 0.62f, 0.56f });   // through the orbits
    l[L_WAVE_B] = Line::straight ({ 0.02f, 0.40f, 0.14f }, { 0.98f, 0.40f, 0.14f });   // through the teeth
    l[L_FILT_A] = Line::straight ({ 0.5f, 0.2f, 0.5f }, { 0.5f, 0.9f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.55f }, 0.3f, 2, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.grain = 0.8f; p.contrast = 0.25f;
    p.scan = 0.0f; p.scanAmt = 0.8f; p.scanA = 0.15f; p.scanD = 0.65f;
    p.filtType = 0; p.cutoff = 0.6f; p.reso = 0.35f; p.filtDepth = 0.6f; p.filtMode = 1; p.filtRate = 0.25f;
    p.modScan = 0.55f; p.modPan = 0.65f; p.modRate = 0.2f;
    p.ampA = 0.02f; p.ampD = 0.5f; p.ampS = 0.6f; p.ampR = 0.45f;
}

static void facVertebra (Line* l, Params& p)
{
    p.specimen = listVal (12, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.05f, 0.60f, 0.50f }, { 0.95f, 0.60f, 0.50f });   // across a body
    l[L_WAVE_B] = Line::straight ({ 0.05f, 0.40f, 0.62f }, { 0.95f, 0.40f, 0.62f });   // across the canal
    l[L_FILT_A] = Line::straight ({ 0.5f, 0.1f, 0.5f }, { 0.5f, 0.9f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::helix ({ 0.5f, 0.5f, 0.5f }, 0.2f, 0.5f, 1.5f, 12);
    l[L_MOD_B]  = l[L_MOD_A];
    p.head2 = 0.6f; p.head2Ratio = 0.5f + std::log2 (1.5f) / 4.0f; p.head2Phase = 0.0f;   // a second head at 1.5x: a beating partial
    p.grain = 0.6f;
    p.scan = 0.0f; p.scanAmt = 0.7f; p.scanA = 0.05f; p.scanD = 0.6f;
    p.filtType = 0; p.cutoff = 0.65f; p.reso = 0.3f; p.filtDepth = 0.5f; p.filtMode = 0; p.filtRate = 0.4f;
    p.modScan = 0.6f; p.modRate = 0.18f;
    p.ampA = 0.01f; p.ampD = 0.55f; p.ampS = 0.5f; p.ampR = 0.5f;
}

static void facFemur (Line* l, Params& p)
{
    p.specimen = listVal (13, NSPECIMENS);
    //  a SPLIT line: two segments, one through the bone, one through the muscle — two edges a cycle
    Line a; a.n = 4; a.split = 2;
    a.p[0] = { 0.25f, 0.62f, 0.5f }; a.p[1] = { 0.55f, 0.62f, 0.5f };
    a.p[2] = { 0.40f, 0.30f, 0.5f }; a.p[3] = { 0.90f, 0.30f, 0.5f };
    l[L_WAVE_A] = a;
    Line b = a; b.p[0] = { 0.20f, 0.60f, 0.85f }; b.p[1] = { 0.60f, 0.60f, 0.85f };
    b.p[2] = { 0.30f, 0.25f, 0.85f }; b.p[3] = { 0.85f, 0.25f, 0.85f };
    l[L_WAVE_B] = b;
    l[L_FILT_A] = Line::straight ({ 0.1f, 0.5f, 0.5f }, { 0.9f, 0.5f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.25f, 1, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.grain = 0.5f; p.fold = 0.35f; p.contrast = 0.3f;
    p.scan = 0.0f; p.scanAmt = 0.75f; p.scanA = 0.02f; p.scanD = 0.55f;
    p.filtType = 0; p.cutoff = 0.55f; p.reso = 0.4f; p.filtDepth = 0.7f; p.filtMode = 0; p.filtRate = 0.35f;
    p.unison = listVal (2, 4); p.detune = 0.15f; p.spread = 0.7f; p.uniScan = 0.5f;
    p.modScan = 0.5f; p.modPan = 0.6f; p.modRate = 0.22f;
    p.ampA = 0.01f; p.ampD = 0.45f; p.ampS = 0.55f; p.ampR = 0.4f;
}

static void facJaw (Line* l, Params& p)
{
    p.specimen = listVal (14, NSPECIMENS);
    l[L_WAVE_A] = Line::straight ({ 0.05f, 0.30f, 0.44f }, { 0.95f, 0.30f, 0.44f });   // along the lower teeth
    l[L_WAVE_B] = Line::circle ({ 0.5f, 0.42f, 0.42f }, 0.32f, 2, 12);                 // round the arch
    l[L_FILT_A] = Line::straight ({ 0.5f, 0.1f, 0.5f }, { 0.5f, 0.9f, 0.5f });
    l[L_FILT_B] = l[L_FILT_A];
    l[L_MOD_A]  = Line::circle ({ 0.5f, 0.5f, 0.5f }, 0.3f, 2, 8);
    l[L_MOD_B]  = l[L_MOD_A];
    p.grain = 1.0f; p.contrast = 0.2f;
    p.modContrast = 0.85f;                                                            // the MOD line narrows the window
    p.scan = 0.3f; p.scanAmt = 0.5f;
    p.filtType = 0; p.cutoff = 0.7f; p.reso = 0.3f; p.filtDepth = 0.4f; p.filtMode = 1; p.filtRate = 0.3f;
    p.modScan = 0.5f; p.modRate = 0.2f;
    p.ampA = 0.01f; p.ampD = 0.5f; p.ampS = 0.55f; p.ampR = 0.4f;
}

static const FactoryPatch FACTORY[] =
{
    { "ADMISSION",     facAdmission   },`);
  rep(String.raw`    { "TENDON",        facTendon      },
};`,
      String.raw`    { "TENDON",        facTendon      },
    { "SKULL",         facSkull       },
    { "VERTEBRA",      facVertebra    },
    { "FEMUR",         facFemur       },
    { "JAW",           facJaw         },
};`);
});

// ============================================================ PluginProcessor.h
const ph = edit("Source/PluginProcessor.h", (rep) => {
  rep(String.raw`    bs::Line lines[bs::NLINES];       // the canonical copy (the engine holds its own)`,
      String.raw`    bs::Line lines[bs::NLINES];       // the canonical copy (the engine holds its own)
    bs::Line loadedLines[bs::NLINES]; // the lines the current patch / project / factory study came with
    void rememberLoadedLines();
    void clearImport (const juce::String& why);`);
});

// ============================================================ PluginProcessor.cpp
const pc = edit("Source/PluginProcessor.cpp", (rep) => {
  rep(String.raw`        default: return juce::String ((int) std::round (v * 100.0f)) + " %";
    }
}`,
      String.raw`        case KP_RATIO: return "x" + juce::String (ratioOf (v), 2);
        default: return juce::String ((int) std::round (v * 100.0f)) + " %";
    }
}`);
  rep(String.raw`    if (v.hasProperty ("warp"))   l.warp = juce::jlimit (-0.95f, 0.95f, (float) (double) v.getProperty ("warp", 0.0));
    return l;
}`,
      String.raw`    if (v.hasProperty ("warp"))   l.warp = juce::jlimit (-0.95f, 0.95f, (float) (double) v.getProperty ("warp", 0.0));
    if (v.hasProperty ("split"))  l.split = juce::jlimit (0, MAXPTS - 2, (int) v.getProperty ("split", 0));
    return l;
}`);
  rep(String.raw`    o->setProperty ("warp", (double) l.warp);
    return juce::var (o);
}`,
      String.raw`    o->setProperty ("warp", (double) l.warp);
    if (l.split > 0) o->setProperty ("split", l.split);      // a line never split writes what it always did
    return juce::var (o);
}`);
  rep(String.raw`    factory (i).build (lines, p);
    engine.setLines (lines);`,
      String.raw`    factory (i).build (lines, p);
    engine.setLines (lines);
    rememberLoadedLines();`);
  rep(String.raw`    if (k == "p")            setParamById (o->getProperty ("id").toString(), (float) (double) o->getProperty ("v"), true);`,
      String.raw`    if (k == "p")
    {
        const juce::String id = o->getProperty ("id").toString();
        setParamById (id, (float) (double) o->getProperty ("v"), true);
        /*  Peter: "once an external file is imported, I cannot switch to the
            other samples — the switcher works but the image and sound do not
            change." The import OVERRODE the dial, by design, and CLEAR was the
            way out — but a dial that moves and changes nothing reads as
            broken. So: dialling a specimen while an import is in use clears
            the import and takes the specimen. */
        if (id == "specimen" && engine.importedActive()) clearImport ("import cleared - the dial has the room again");
    }`);
  rep(String.raw`    else if (k == "lines")   { if (linesFromVar (o->getProperty ("j"))) { patchIsUser = false; emitLines(); } }`,
      String.raw`    else if (k == "lines")   { if (linesFromVar (o->getProperty ("j"))) { patchIsUser = false; emitLines(); } }
    else if (k == "revertLines")
    {
        //  the lines the patch, project or factory study was loaded with
        for (int i = 0; i < NLINES; ++i) lines[i] = loadedLines[i];
        engine.setLines (lines);
        emitLines();
        notice ("lines put back as loaded");
    }`);
  rep(String.raw`    else if (k == "importClear")
    {
        engine.clearImported();
        importCube.clear(); importPath = {}; importNote = {}; importError = {};
        specimenSent = -1;
        emitImport(); emitVolume();
        notice ("the specimen dial is back");
    }`,
      String.raw`    else if (k == "importClear") clearImport ("the specimen dial is back");`);
  rep(String.raw`    migrateSpecimen (v.getProperty ("build", juce::var ("")).toString());
    linesFromVar (v.getProperty ("lines", juce::var()));`,
      String.raw`    migrateSpecimen (v.getProperty ("build", juce::var ("")).toString());
    linesFromVar (v.getProperty ("lines", juce::var()));
    rememberLoadedLines();`);
  rep(String.raw`        if (ln.isNotEmpty()) { juce::var v; if (! juce::JSON::parse (ln, v).failed()) linesFromVar (v); }`,
      String.raw`        if (ln.isNotEmpty()) { juce::var v; if (! juce::JSON::parse (ln, v).failed()) linesFromVar (v); }
        rememberLoadedLines();`);
  rep(String.raw`    o->setProperty ("periodic", v.periodicX);
    o->setProperty ("d", juce::Base64::toBase64 (bytes.data(), bytes.size()));`,
      String.raw`    o->setProperty ("periodic", v.periodicX);
    /*  Hounsfield units, when the volume has them: the bodies are built in HU
        and mapped -1000..2000 onto 0..1; an import's cube spans its window.
        The page then offers a radiographer's presets and prints HU. */
    if (v.specimen == SPEC_IMPORTED)
    {
        juce::Array<juce::var> hu; hu.add ((double) importWindowLo); hu.add ((double) importWindowHi);
        o->setProperty ("hu", hu);
        o->setProperty ("win", juce::String());
    }
    else if (specimenIsHu (v.specimen))
    {
        juce::Array<juce::var> hu; hu.add (-1000.0); hu.add (2000.0);
        o->setProperty ("hu", hu);
        o->setProperty ("win", juce::String (specimenWindow (v.specimen)));
    }
    o->setProperty ("d", juce::Base64::toBase64 (bytes.data(), bytes.size()));`);
  rep(String.raw`void BrainScanAudioProcessor::migrateSpecimen (const juce::String& writtenBy)
{
    if (writtenBy.isNotEmpty() && writtenBy.compare ("260904.3") >= 0) return;
    auto* rawSpec = apvts.getRawParameterValue ("specimen");
    if (rawSpec == nullptr) return;
    const int idx = juce::jlimit (0, 8, (int) std::lround (rawSpec->load() * 8.0f));
    setParamById ("specimen", (float) idx / 11.0f, false);
}`,
      String.raw`void BrainScanAudioProcessor::migrateSpecimen (const juce::String& writtenBy)
{
    //  how many slots the dial had when this was written
    int oldSlots = 0;
    if (writtenBy.isEmpty() || writtenBy.compare ("260904.3") < 0) oldSlots = 9;      // before the gritty three
    else if (writtenBy.compare ("260905.1") < 0)                   oldSlots = 12;     // before the bodies
    const int newSlots = numSpecimens();
    if (oldSlots == 0 || oldSlots == newSlots) return;
    auto* rawSpec = apvts.getRawParameterValue ("specimen");
    if (rawSpec == nullptr) return;
    const int idx = juce::jlimit (0, oldSlots - 1, (int) std::lround (rawSpec->load() * (float) (oldSlots - 1)));
    setParamById ("specimen", (float) idx / (float) (newSlots - 1), false);
}

void BrainScanAudioProcessor::rememberLoadedLines()
{
    for (int i = 0; i < NLINES; ++i) loadedLines[i] = lines[i];
}

void BrainScanAudioProcessor::clearImport (const juce::String& why)
{
    engine.clearImported();
    importCube.clear(); importPath = {}; importNote = {}; importError = {};
    specimenSent = -1;
    emitImport(); emitVolume();
    notice (why);
}`);
});

if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [eh, ec, sp, ph, pc]) fs.writeFileSync(f.p, f.get(), "utf8");
console.log("patched: Engine.h, Engine.cpp, Specimens.cpp, PluginProcessor.h, PluginProcessor.cpp");
