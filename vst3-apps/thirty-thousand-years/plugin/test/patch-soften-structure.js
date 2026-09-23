/*  Take the shrillness out of the metal.
 *
 *  Measured with test/probe-harsh.cpp, which ranks every preset by how much
 *  energy sits in 2-5 kHz, the band the ear is most sensitive to. For the worst
 *  of them, silencing STRUCTURE takes that band to nothing at all, so the metal
 *  is the whole of it.
 *
 *  Three levers rather than one, because each does a different job:
 *      st_damp      up   -- damps the high partials. Less shrill, same pitch,
 *                           same level: the one that changes tone and nothing
 *                           else.
 *      st_material  down -- the resonator's brightness. The common factor in
 *                           every offender (all sat at 0.6 to 0.85).
 *      st_gain      down -- simply less of it.
 *      st_pitch     down -- shifts the register, for the worst three only.
 *
 *  Deliberately gentle and uniform: this is meant to take the edge off, not to
 *  redesign anyone's patch. Every change is measured afterwards, and the test
 *  is that the harsh band drops several dB while the overall level barely
 *  moves -- a patch that merely got quieter has been damaged, not fixed.
 */
"use strict";
const fs = require("fs");
const path = require("path");

const FILE = path.join(__dirname, "..", "Source", "Presets.cpp");

/*  measured offenders, plus the two Peter named */
const STRONG = ["STEEL CATHEDRAL", "PLATE TECTONICS", "STRESSED PLATE"];
const GENTLE = ["BENEATH THIRTY KILOMETRES OF CONCRETE", "THE SEA IS MADE OF IRON",
                "GLASS RAIN", "CABLE BASS", "IRON BASS"];

const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
const fmt = v => {
  const s = v.toFixed(3).replace(/0+$/, "").replace(/\.$/, "");
  return s === "" || s === "-0" ? "0" : s;
};

let src = fs.readFileSync(FILE, "utf8");
const NL = src.includes("\r\n") ? "\r\n" : "\n";

const misses = [], done = [];

function soften(name, strong) {
  /*  the preset entry runs from its opening quote to the next one starting a
      new entry, so work on that slice alone and never on the file at large */
  const key = '{ "' + name + '"';
  const i = src.indexOf(key);
  if (i < 0) { misses.push(name + " -- not found"); return; }
  let j = src.indexOf('\n{ "', i + 1);
  if (j < 0) j = src.length;
  const before = src.slice(i, j);

  if (!/st_on=1/.test(before)) { misses.push(name + " -- has no STRUCTURE"); return; }
  if (/st_softened/.test(before)) { misses.push(name + " -- already softened"); return; }

  const moves = [
    ["st_damp",     +0.15, 0.0, 0.85],
    ["st_material", -0.15, 0.35, 1.0],
    ["st_gain",     -0.08, 0.10, 1.0],
  ];
  if (strong) moves.push(["st_pitch", -0.08, 0.0, 1.0]);

  let after = before, changed = [];
  for (const [id, delta, lo, hi] of moves) {
    const re = new RegExp("(\\b" + id + "=)(-?[0-9.]+)");
    const m = re.exec(after);
    if (!m) {
      /*  a parameter the preset never states is at its default; only worth
          writing when we are pushing it away from that default */
      continue;
    }
    const was = parseFloat(m[2]);
    const now = clamp(was + delta, lo, hi);
    if (Math.abs(now - was) < 1e-6) continue;
    after = after.replace(re, "$1" + fmt(now));
    changed.push(`${id} ${was}->${fmt(now)}`);
  }
  if (!changed.length) { misses.push(name + " -- nothing to move"); return; }

  src = src.slice(0, i) + after + src.slice(j);
  done.push({ name, changed });
}

for (const n of STRONG) soften(n, true);
for (const n of GENTLE) soften(n, false);

if (misses.length) {
  console.error("REFUSING -- nothing written:");
  for (const m of misses) console.error("  " + m);
  process.exit(1);
}

fs.writeFileSync(FILE, src);
for (const d of done) console.log("  " + d.name.padEnd(38) + d.changed.join("  "));
console.log("\n  " + done.length + " presets softened");
