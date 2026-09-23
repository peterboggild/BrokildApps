/*  ANTITHRUST width: the per-sample path.
 *  Exact-count anchors; nothing written if any misses. */
const fs = require("fs");
const p = "C:/Users/peter/b/BattlestarOverdrive/Source/Engine.cpp";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(find, sub) {
  const f = find.join(NL);
  const n = s.split(f).length - 1;
  if (n !== 1) { miss.push(n + " matches: " + find[0].trim().slice(0, 55)); return; }
  s = s.replace(f, sub.join(NL));
}

// ------------------------------------------------- 1. the control-rate law
rep([
"                // The comb SHORTENS as the knob rises: a loose flangey ring at",
"                // the bottom, a tight nasal honk at the top. It sits before the",
"                // shaper so the drive emphasises what survives between the",
"                // notches, which is what makes it vocal rather than phasey.",
"                const float ms = 12.0f * std::pow (0.03f, sAnti);",
"                combDelayTarget = ms * 0.001f * (float) sr;",
"                combMix = 0.85f * sAnti;",
"                combFb  = 0.55f * sAnti * sAnti * sAnti;      // bounded well below 1",
"                chokeDepth = sAnti * sAnti;",
"                chokeHz = lerpf (20000.0f, 2400.0f, sAnti * sAnti);"
], [
"                // WIDTH. The knob adds decorrelated side derived from the mid;",
"                // the delay shortens as it rises so the image opens from a",
"                // deep, slow room towards a tight, bright spread.",
"                widthAmt = 1.35f * sAnti;",
"                widthDelayTarget = lerpf (14.0f, 3.0f, sAnti) * 0.001f * (float) sr;",
"                chokeDepth = sAnti * sAnti;",
"                // The lowpass now only takes the very top off at the extreme,",
"                // instead of closing to 2.4 kHz and fighting the comb's own",
"                // first null over the same region.",
"                chokeHz = lerpf (20000.0f, 7000.0f, sAnti * sAnti);"
]);

rep([
"                combMix = combFb = chokeDepth = 0.0f;"
], [
"                widthAmt = chokeDepth = 0.0f;"
]);

rep([
"    float combDelayTarget = 0.0f, combMix = 0.0f, combFb = 0.0f;",
"    float chokeDepth = 0.0f, chokeHz = 20000.0f;"
], [
"    float widthAmt = 0.0f, widthDelayTarget = 0.0f, antiGain = 1.0f;",
"    float chokeDepth = 0.0f, chokeHz = 20000.0f;"
]);

rep([
"            antiOn  = sAnti  > 1.0e-4f;"
], [
"            antiOn  = sAnti  > 1.0e-4f;",
"            antiGain = antiOn ? antiTrimFor (sAnti) : 1.0f;"
]);

// ------------------------------------------------- 2. drop the pre-drive comb
rep([
"            // --- ANTITHRUST: inverted comb, before the drive ---------------",
"            if (antiOn)",
"            {",
"                combDelaySm += (combDelayTarget - combDelaySm) * 0.002f;",
"                const float d = comb[(size_t) c].readFrac (std::max (1.0f, combDelaySm));",
"                comb[(size_t) c].write (flushDenorm (x + d * combFb));",
"                x = x - combMix * d;                      // inverted: notches",
"            }",
"",
"            // --- the engine, at 4x -----------------------------------------"
], [
"            // ANTITHRUST no longer touches the signal before the drive. Width",
"            // has to come AFTER it: distortion is nonlinear, so two slightly",
"            // different signals through two shapers make wildly different",
"            // harmonics and the image smears instead of widening.",
"",
"            // --- the engine, at 4x -----------------------------------------"
]);

// ------------------------------------------------- 3. the choke gets makeup
rep([
"                chokeCand = std::max (chokeCand, std::abs (v));",
"                const float gr = 1.0f / (1.0f + chokeDepth * 6.0f * chokeEnv);",
"                v = chokeLp[(size_t) c].process (v * gr);"
], [
"                chokeCand = std::max (chokeCand, std::abs (v));",
"                // WITH MAKEUP. Without it this is a pure attenuator: it was",
"                // most of ANTITHRUST's 6.8 dB level loss, and a compressor is",
"                // supposed to reduce dynamic RANGE, not average level. The",
"                // makeup is a constant against a nominal level, NOT a tracker -",
"                // a tracker is the auto-gain that was deleted for breathing.",
"                const float makeup = 1.0f + chokeDepth * 6.0f * CHOKE_NOMINAL;",
"                const float gr = makeup / (1.0f + chokeDepth * 6.0f * chokeEnv);",
"                v = chokeLp[(size_t) c].process (v * gr);"
]);

// ------------------------------------------------- 4. the width stage
rep([
"        if (antiOn)",
"        {",
"            const float k = (chokeCand > chokeEnv) ? 0.02f : 0.0009f;   // fast attack",
"            chokeEnv += (chokeCand - chokeEnv) * k;",
"        }"
], [
"        if (antiOn)",
"        {",
"            const float k = (chokeCand > chokeEnv) ? 0.02f : 0.0009f;   // fast attack",
"            chokeEnv += (chokeCand - chokeEnv) * k;",
"",
"            /*  --- WIDTH, in mid/side -----------------------------------",
"                The knob ADDS decorrelated side derived from the mid and never",
"                touches the mid, so L+R is exactly what it always was: no",
"                cancellation at any setting, on any source. That is what makes",
"                it mono-safe by construction rather than by tuning, and it is",
"                also why an incoming stereo image survives - the existing side",
"                passes through untouched and is added to, not replaced.",
"",
"                The decorrelation is allpasses, not a comb: an allpass has a",
"                flat magnitude response, so it widens without leaving notches",
"                across the tone. A little comb is blended in for body.  */",
"            const float M = 0.5f * (y[0] + y[1]);",
"            const float S = 0.5f * (y[0] - y[1]);",
"",
"            widthDelaySm += (widthDelayTarget - widthDelaySm) * 0.002f;",
"            float h = M;",
"            for (int k2 = 0; k2 < NAP; ++k2) h = widthAp[(size_t) k2].process (h);",
"            widthComb.write (M);",
"            h = h * 0.68f + widthComb.readFrac (std::max (1.0f, widthDelaySm)) * 0.32f;",
"            h = sideShelf.process (h);          // shaped: more side up top reads as depth",
"",
"            const float sd = S + widthAmt * h;",
"            y[0] = (M + sd) * antiGain;",
"            y[1] = (M - sd) * antiGain;",
"        }"
]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(p, s);
console.log("Engine.cpp: per-sample width path patched");
