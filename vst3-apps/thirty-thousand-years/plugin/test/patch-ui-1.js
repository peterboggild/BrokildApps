/*  Panel round 1 — one real fault, found by the probe.

    classList.add() REFUSES a token containing a space, and it throws. The
    strata switch rows ask for "chip mu" (the mute and solo chips), so place()
    threw on the first of them and every control after it in that pass was
    never placed: 24 of 395 landed, and the panel would have been almost
    empty. A static check cannot see this; only running the page can.

    Also: place() removed "chip"/"big" one at a time and never removed "mu",
    so a control that had once been a mute chip kept the class when it was
    borrowed by another view.
*/
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/Source/ui/ui.html";
let s = fs.readFileSync(p, "utf8");

const edits = [
  // 1. split the class string, and clear the whole set of borrowed classes
  [`      if (w[2] && w[2].cls) c.classList.add(w[2].cls);
      else if (!w[2] || !w[2].cls) c.classList.remove("chip"), c.classList.remove("big");`,
   `      /*  A control is MOVED between views, so every class a view lends it has
          to be taken back when another view borrows it. classList.add refuses a
          token with a space in it (and throws), so the string is split. */
      ["chip", "big", "mu", "sm"].forEach(function (k) { c.classList.remove(k); });
      if (w[2] && w[2].cls) String(w[2].cls).split(/\\s+/).forEach(function (k) { if (k) c.classList.add(k); });`],
  // 2. the hint must not quote a LIST name: the names arrive in initialState
  [`    return { type:"What kind of randomness " + r + " makes. SMOOTH NOISE wanders, SAMPLE & HOLD steps, CHAOS is deterministic but unpredictable.",`,
   `    return { type:"What kind of randomness " + r + " makes: a slow wander, a stepped hold, a burst, or a bounded chaotic trajectory. The names are on the control.",`],
];

let miss = [];
for (const [a] of edits) { const n = s.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 70)); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(p, s);
console.log("ui.html patched (" + edits.length + " edits)");
