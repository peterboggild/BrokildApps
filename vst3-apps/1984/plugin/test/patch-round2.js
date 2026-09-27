"use strict";
const fs = require("fs");
const path = require("path");
const root = "C:/Users/peter/b/Nineteen84";
const files = {};
function load(rel) { const p = path.join(root, rel); files[rel] = { p, s: fs.readFileSync(p, "utf8"), n: 0 }; }
function edit(rel, from, to, count = 1) {
  const f = files[rel];
  const parts = f.s.split(from);
  if (parts.length - 1 !== count) { console.error("ANCHOR MISS in " + rel + " (" + (parts.length - 1) + " of " + count + "):\n" + from.slice(0, 160)); process.exit(1); }
  f.s = parts.join(to); f.n++;
}
load("Source/Engine.h"); load("Source/Engine.cpp"); load("test/bench.cpp");

// VALVE: a genuine even term. b^2/(1+|b|) is even, grows only linearly, and
// keeps the curve monotonic (u' >= 0.55 everywhere), so it cannot fold.
edit("Source/Engine.h",
`                      const float b = x * g * 0.7f + 0.15f * amt;
                      return b >= 0.0f ? ftanh (b) : 0.62f * ftanh (b * 1.61f); }`,
`                      const float b = x * g * 0.7f;
                      const float u = b + 0.45f * b * b / (1.0f + std::abs (b));
                      return ftanh (u); }`);

// ENSEMBLE: three taps of a modulated sine partly cancel; normalise by power
edit("Source/Engine.cpp",
`    const float norm = 1.0f / (float) taps;`,
`    const float norm = taps == 3 ? 0.44f : 0.59f;      // taps^-0.75: between the amplitude and the power sum`);

// HISS gate: release fast enough to reach exact silence within a few seconds
edit("Source/Engine.cpp",
`    envRel = 1.0f - std::exp (-1.0f / (1.5f * (float) fs));`,
`    envRel = 1.0f - std::exp (-1.0f / (0.6f * (float) fs));`);
edit("Source/Engine.cpp",
`    const float hissGate = envF < 1.0e-4f ? 0.0f : std::min (1.0f, envF * 40.0f);`,
`    const float hissGate = envF < 1.0e-3f ? 0.0f : std::min (1.0f, envF * 40.0f);`);

// bench: choir probe on a saw (a 68 % pulse has a null at its 15th harmonic, 1650 Hz - the formant under test)
edit("test/bench.cpp",
`            Rig r; r.set ("a_saw", 0.3f); r.set ("a_pulse", 0.8f); r.set ("a_pw", 0.4f); r.set ("choir_mix", 1.0f); r.set ("choir_vowel", 0.5f); r.set ("choir_reg", 0.5f);`,
`            Rig r; r.set ("a_saw", 1.0f); r.set ("choir_mix", 1.0f); r.set ("choir_vowel", 0.5f); r.set ("choir_reg", 0.5f);`);
edit("test/bench.cpp",
`            Rig i; i.set ("a_saw", 0.3f); i.set ("a_pulse", 0.8f); i.set ("a_pw", 0.4f); i.set ("choir_mix", 1.0f); i.set ("choir_vowel", 1.0f); i.set ("choir_reg", 0.5f);`,
`            Rig i; i.set ("a_saw", 1.0f); i.set ("choir_mix", 1.0f); i.set ("choir_vowel", 1.0f); i.set ("choir_reg", 0.5f);`);
// bench: a slow pad is audible somewhere in its first three seconds, not necessarily in the first half
edit("test/bench.cpp",
`                r.e.noteOn (48, 0.85f); r.e.noteOn (55, 0.85f); r.e.noteOn (64, 0.85f); r.render (48000); r.e.noteOff (48); r.e.noteOff (55); r.e.noteOff (64); r.renderAppend (24000);
                if (! r.finite() || r.peak() > 1.001f) { ++badN; std::printf ("     patch %s: peak %g\\n", patchName (i), r.peak()); }
                if (db (r.rms (0, 24000)) < -45.0) { ++quiet; std::printf ("     patch %s: %.1f dBFS\\n", patchName (i), db (r.rms (0, 24000))); }`,
`                r.e.noteOn (48, 0.85f); r.e.noteOn (55, 0.85f); r.e.noteOn (64, 0.85f); r.render (48000 * 3); r.e.noteOff (48); r.e.noteOff (55); r.e.noteOff (64); r.renderAppend (24000);
                if (! r.finite() || r.peak() > 1.001f) { ++badN; std::printf ("     patch %s: peak %g\\n", patchName (i), r.peak()); }
                double loudest = 0; for (int w = 0; w + 12000 <= 48000 * 3; w += 12000) loudest = std::max (loudest, (double) r.rms (w, w + 12000));
                if (db (loudest) < -45.0) { ++quiet; std::printf ("     patch %s: %.1f dBFS\\n", patchName (i), db (loudest)); }`);
edit("test/bench.cpp",
`            check (quiet == 0, "every factory patch: audible (above -45 dBFS in the first half second of a chord)", quiet, 0);`,
`            check (quiet == 0, "every factory patch: audible (a quarter second above -45 dBFS within three seconds of a chord)", quiet, 0);`);

for (const rel in files) { fs.writeFileSync(files[rel].p, files[rel].s); console.log(rel + ": " + files[rel].n + " edits"); }
