// THE BLASTBEAT, and the dead BWFX button.
//
// REBOUND had no control anywhere on the panel. I added it as a parameter and
// gave the kit generator permission to dial it in — so a seed could set five
// bounces on a channel and there was no knob to see it on, let alone turn it
// down. TUNNEL DETROIT is exactly that: a hat lane already playing sixteenths
// with a five-bounce roll on top of every one. An invisible control that
// wrecks a kit is worse than a missing feature.
//
// Three parts: put REBOUND and KEY MODE on the strip where they belong, make
// the generator treat rebound as the garnish it is, and have the bench refuse
// any kit that dials in more than a flam.
//
// Also: the BWFX globe. The shared fragment scans for [data-bwfx-open] ONCE at
// page load, and I later moved the rail into JS-built markup — so the button
// was created after the scan and never bound to anything.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(p.split("/").pop() + ": " + t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

// ── 1 · the generator stops dialling in rolls ───────────────────────────────
const wk = edit(R + "Source/Kits.cpp", [
[`        //  Rebound is a garnish, not a default: most kits get none at all
        P[CP_REBOUND] = r.uni() < 0.14f ? r.uni() * 0.45f : 0.0f;`,
`        /*  REBOUND is a garnish and the generator should barely reach for it.
            At 0.45 it is five bounces closing to forty milliseconds — on a hat
            lane already playing sixteenths that is not a flam, it is a blast
            beat, and the kit stops sounding like a kit. One channel in twenty,
            and never past a flam. */
        P[CP_REBOUND] = r.uni() < 0.05f ? 0.06f + r.uni() * 0.12f : 0.0f;`, "seed rebound"]
]);

// ── 2 · REBOUND and KEY appear on the strip ─────────────────────────────────
const wu = edit(R + "Source/ui/ui.html", [
[`const KNOBS = [["tune", "Tune"], ["decay", "Decay"], ["tone", "Tone"],
               ["snap", "Snap"], ["bend", "Bend"], ["drive", "Drive"]];`,
`const KNOBS = [["tune", "Tune"], ["decay", "Decay"], ["tone", "Tone"],
               ["snap", "Snap"], ["bend", "Bend"], ["drive", "Drive"],
               ["rebound", "Rebound"]];`, "knob list"],

[`    const kg = el("div", "kgrid", st);
    KNOBS.forEach(([suf, label]) => {
      const w = el("div", "kw", kg);
      makeKnob(el("div", "knob", w), c.id + "_" + suf);
      el("div", "kl", w).textContent = label;
    });`,
`    const kg = el("div", "kgrid", st);
    KNOBS.forEach(([suf, label]) => {
      const w = el("div", "kw", kg);
      makeKnob(el("div", "knob", w), c.id + "_" + suf);
      el("div", "kl", w).textContent = label;
    });
    /*  KEY MODE is a switch, so it gets a key rather than a knob — and it
        sits in the grid's last cell beside REBOUND, because both of them
        change what a trigger MEANS rather than what it sounds like. */
    { const w = el("div", "kw", kg);
      const kid = c.id + "_key";
      const b = el("button", "k", w);
      b.textContent = "KEY";
      b.style.cssText = "width:100%;height:32px;font-size:10px";
      b.addEventListener("click", () => setVal(kid, VAL[kid] >= 0.5 ? 0 : 1));
      reg(kid, { draw() { b.classList.toggle("on", VAL[kid] >= 0.5); } });
      el("div", "kl", w).textContent = "Chromatic"; }`, "key toggle"],

// ── 3 · bind the BWFX globe by hand ────────────────────────────────────────
[`  const g = el("div", "", rail);
  g.id = "globe"; g.setAttribute("data-bwfx-open", ""); g.title = "Brokild World FX";
  g.textContent = "BWFX";`,
`  const g = el("div", "", rail);
  g.id = "globe"; g.setAttribute("data-bwfx-open", ""); g.title = "Brokild World FX";
  g.textContent = "BWFX";
  /*  The shared fragment scans for [data-bwfx-open] once, when it loads. This
      button is built later, from initialState, so the scan never sees it and
      the attribute alone does nothing. Bind it here. */
  g.addEventListener("click", () => {
    if (window.BWFX && typeof BWFX.toggle === "function") BWFX.toggle();
    else if (window.BWFX && typeof BWFX.open === "function") BWFX.open();
  });`, "globe bind"]
]);

// ── 4 · the bench refuses a kit that dials in a roll ───────────────────────
const wt = edit(R + "test/test.cpp", [
[`    //  a kit is reproducible from its integer, which is the whole point`,
`    /*  No kit may dial in more than a flam. REBOUND is a performance garnish;
        a generator reaching for a five-bounce roll turns a hat lane into a
        blast beat, and before it had a knob there was no way to even see it. */
    {
        float worstRb = 0.0f; int withAny = 0, n = 0;
        for (int s2 = 0; s2 < numSeeds(); ++s2)
        {
            Params q; applySeed (s2, q);
            bool any = false;
            for (int c = 0; c < NCH; ++c)
            {
                worstRb = std::max (worstRb, q.ch[c][CP_REBOUND]);
                if (q.ch[c][CP_REBOUND] > 0.0f) { any = true; ++n; }
            }
            if (any) ++withAny;
        }
        std::printf ("    rebound: worst %.2f, on %d of %d channels across %d kits\\n",
                     worstRb, n, numSeeds() * NCH, withAny);
        ok (worstRb <= 0.20f, "no kit dials in more than a flam", worstRb, 0.20);
        ok (n < numSeeds() * NCH / 12, "and it stays a garnish", n, numSeeds() * NCH / 12.0);
    }

    //  a kit is reproducible from its integer, which is the whole point`, "rebound bench"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wk(); wu(); wt();
console.log("rebound exposed and tamed; BWFX globe bound");
