/*  Peter: "still very very little evolution, timewise — the notes are
    constant and don't evolve."

    He is right, and structurally so: a held note had NO decay. Energy was
    conserved forever, migration only traded a constant total around, and
    once the winch settled every amplitude target froze. The synth was an
    organ when it should be a creature. Three changes:

    1. RINGDOWN INTO A LIVING BED. Every mode now decays with its own time
       constant (fast modes die first — the strike audibly darkens over
       seconds), but while the aperture is held open it decays toward a
       SUSTAIN FLOOR shaped by the strike profile — the note rings down into
       a quieter, breathing remainder instead of either freezing or dying.

    2. THE APERTURE BREATH. The floor undulates: each mode carries a slow
       deterministic oscillation (0.03–0.2 Hz, phase from the global clock —
       the process was already running; the key merely opened onto it), so a
       held chord visibly and audibly respires. Under full REVIVAL the
       artefact holds its breath (depth -> 0): the sacred recurrence must be
       exact, and the bench proves it stays so.

    3. MIGRATION IN PER-SECOND UNITS. It was applied per control tick —
       sample-rate dependent, a real bug — and, at the corrected crawl,
       nearly frozen at default. Now rate = metabolism^2 * 6.5 /s (times the
       coupling weight), integrated with dt: audible travel at default,
       churning at full.
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

const wH = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.h", [
[`    std::array<float, kMaxModes> tiltW {};     // per-tick gravity scratch`,
`    std::array<float, kMaxModes> tiltW {};     // per-tick gravity scratch
    std::array<float, kMaxModes> decayMul {};  // per-tick ringdown scratch
    std::array<float, kMaxModes> breathW {};   // per-tick breath scratch`, "scratch"]
]);

const wC = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.cpp", [

//  the strike profile is kept: it is what the sustain floor is shaped by
[`        v.energy[(size_t) k] = a * a;`,
`        v.energy[(size_t) k] = a * a;
        //  keep the strike profile: the sustain floor is shaped by it
        v.exciteW[(size_t) k] = a * a;`, "keep profile"],

//  migration in per-second units + the ringdown/breath tables, per tick
[`    const float coupleRate = p.metabolism * p.metabolism * 0.014f
                           * (1.0f + p.membrane * 1.5f);`,
`    const float coupleRate = p.metabolism * p.metabolism * 6.5f * dt
                           * (1.0f + p.membrane * 1.5f);

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
    const float sustain = 0.10f + 0.30f * ap;
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
    }`, "rates"],

//  the exchange integrates with dt (clamp keeps a tick stable at any rate)
[`                    const float d = clampf (coupleRate * cw, 0.0f, 0.2f)
                                  * (v.energy[(size_t) l] - v.energy[(size_t) k]);`,
`                    const float d = clampf (coupleRate * cw, 0.0f, 0.25f)
                                  * (v.energy[(size_t) l] - v.energy[(size_t) k]);`, "clamp"],

//  the ringdown itself, per voice: decay toward the breathing floor
[`        //  substrate: WAKE feeds whispers of energy into wandering tissue.`,
`        //  RINGDOWN: each mode decays toward its breathing sustain floor
        //  while the aperture is open, toward silence once released.
        for (int k = 0; k < s.nModes; ++k)
        {
            const float fl = v.releasing ? 0.0f
                           : breathW[(size_t) k] * v.exciteW[(size_t) k];
            v.energy[(size_t) k] = fl
                + (v.energy[(size_t) k] - fl) * decayMul[(size_t) k];
        }

        //  substrate: WAKE feeds whispers of energy into wandering tissue.`, "ringdown"]
]);

const wB = edit("C:/Users/peter/b/ArtefactB2311/test/bench.cpp", [
[`        CHECK (std::fabs (es - 0.8f * 0.8f) < 0.02f,
               "energy budget %.3f != vel^2 0.640", (double) es);`,
`        //  ringdown has begun by 43 ms (that is the point of it), so the
        //  budget check allows the first few percent of decay
        CHECK (std::fabs (es - 0.8f * 0.8f) < 0.05f,
               "energy budget %.3f != vel^2 0.640", (double) es);`, "budget tol"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wH(); wC(); wB();
console.log("ringdown + breath + per-second migration in");
