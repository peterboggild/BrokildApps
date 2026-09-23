/*  FMR BUGLIST 2 and 3.

    2a. A morph belongs to ONE patch. Dialling a kit already cleared the patch
        name and restarted the clock; it left A and B standing, so the fader
        went on blending a kit you were no longer on with one you never chose.

    2b. A morph must not STEP anything. The staggered-threshold flipping of
        KP_LIST / KP_SW was a deliberate choice and a wrong one: a fader you
        would automate across eight bars contained a dozen invisible cliffs —
        a kick model changing mid-note, a channel's circuit swapping under a
        held decay. Stepped values now stay wherever they were last set and
        only the continuous ones move. The panel's morphView() mirrors the
        engine and has to lose the same branch in the same commit, or the
        picture and the sound part company (which is how this was found).

    3.  A patch carries its morph — and it half did. pack()/unpack() stored
        per-channel parameters ONLY, while morphable() (and the panel's own
        emitKit) include the five globals the kit generator writes: RAIL SAG,
        BLEED, KIT BODY, AGE, KIT TUNE. So a morph reloaded from a file moved
        the twelve voices and left the machine's character behind — exactly
        the bug the comment above morphable() says was fixed for the live
        path. Both now use morphable() as the single answer to "what is part
        of a kit", so the file, the panel and the engine cannot disagree.
        Old .fmrkit files simply have no globals to read and behave as before.
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

/* ── 2b: the engine stops stepping ──────────────────────────────────── */
const wE = edit("C:/Users/peter/b/FullMetalRacket/Source/Engine.cpp", [
[`        if (s.kind == KP_LIST || s.kind == KP_SW)
        {
            //  staggered thresholds, from the parameter's own index, so the
            //  switched things do not all flip together at the midpoint
            const float th = 0.30f + 0.40f * (float) ((i * 37) % 100) / 100.0f;
            d = t < th ? a : b;
        }
        else d = lerpf (a, b, t);`,
`        /*  CONTINUOUS ONLY. A stepped choice — a kick model, a channel
            switch — stays wherever it was last set. It used to flip at a
            staggered threshold so that the kit changed character rather than
            cross-fading; the cost was that a fader you would automate over
            eight bars hid a dozen cliffs inside it, which reads as a defect
            however it was intended. */
        if (s.kind == KP_LIST || s.kind == KP_SW) continue;
        d = lerpf (a, b, t);`, "engine step"]
]);

/* ── 2b: the panel mirrors it ───────────────────────────────────────── */
const wU = edit("C:/Users/peter/b/FullMetalRacket/Source/ui/ui.html", [
[`  const sp = SPEC[id];
  if (sp && (sp.kind === KIND.LIST || sp.kind === KIND.SW)) {
    //  the same staggered thresholds the engine uses, so a switched value
    //  flips on the panel at the moment it flips in the sound
    const i = Object.keys(SPEC).indexOf(id);
    const th = 0.30 + 0.40 * ((i * 37) % 100) / 100;
    return t < th ? a : b;
  }
  return a + (b - a) * t;`,
`  const sp = SPEC[id];
  //  stepped choices do not morph at all any more, so the panel shows the
  //  live value for them — null means "not being morphed"
  if (sp && (sp.kind === KIND.LIST || sp.kind === KIND.SW)) return null;
  return a + (b - a) * t;`, "panel step"]
]);

/* ── 2a + 3: the processor ──────────────────────────────────────────── */
const wP = edit("C:/Users/peter/b/FullMetalRacket/Source/PluginProcessor.cpp", [
[`    kitIndex = juce::jlimit (0, fmr::numSeeds() - 1, i);
    patchName = {};                      // a dialled seed is no longer that file
    engine.restartClock();`,
`    kitIndex = juce::jlimit (0, fmr::numSeeds() - 1, i);
    patchName = {};                      // a dialled seed is no longer that file
    /*  a morph belongs to ONE patch: a blend between the kit you have left
        and one you never chose is not a morph, it is a haunting */
    engine.haveA = engine.haveB = false;
    engine.restartClock();`, "clear ab"],

//  the A/B pack: "what is part of a kit" has exactly one answer
[`            const auto& s = fmr::paramSpec (i);
            if (s.chan < 0) continue;
            o->setProperty (s.id, (double) fmr::pvalue (const_cast<fmr::Params&> (p), s));`,
`            const auto& s = fmr::paramSpec (i);
            //  morphable() is the single answer to "does this belong to the
            //  kit" — storing less than it meant a reloaded morph moved the
            //  twelve voices and left the machine's character behind
            if (! fmr::morphable (s)) continue;
            o->setProperty (s.id, (double) fmr::pvalue (const_cast<fmr::Params&> (p), s));`, "pack"],

[`                const auto& s = fmr::paramSpec (i);
                if (s.chan < 0) continue;
                if (so->hasProperty (s.id))`,
`                const auto& s = fmr::paramSpec (i);
                if (! fmr::morphable (s)) continue;
                if (so->hasProperty (s.id))`, "unpack"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wE(); wU(); wP();
console.log("2a: kit change clears A/B | 2b: morph is continuous only | 3: the pack is morphable()");
