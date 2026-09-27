// the probe's new overflow check found two more: VOICE had four knobs on one
// row (three fit), and MOD>WINDOW's label was cut at 56 px.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const misses = [];
const rep = (from, to, count = 1) => {
  const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
  const n = s.split(f).length - 1;
  if (n !== count) { misses.push(`expected ${count} of [${from.slice(0, 70).replace(/\n/g, "\\n")}...], found ${n}`); return; }
  s = s.split(f).join(t);
};
rep(String.raw`  const r10 = mkRow(m5); mkKnob(r10, "detune"); mkKnob(r10, "spread"); mkKnob(r10, "glide");
  mkKnob(r10, "uniScan", { name:"SCAN FAN", hint:"SCAN SPREAD — unison readers fan through the SCAN as well as in pitch: each reads the body a little further along than the last, so a chord of readers widens without detuning" });
  const r11 = mkRow(m5); mkKnob(r11, "tune"); mkKnob(r11, "level");`,
    String.raw`  const r10 = mkRow(m5); mkKnob(r10, "detune"); mkKnob(r10, "spread"); mkKnob(r10, "glide");
  const r11 = mkRow(m5);
  mkKnob(r11, "uniScan", { name:"SCAN FAN", hint:"SCAN SPREAD — unison readers fan through the SCAN as well as in pitch: each reads the body a little further along than the last, so a chord of readers widens without detuning" });
  mkKnob(r11, "tune"); mkKnob(r11, "level");`);
rep(String.raw`  mkKnob(r6b, "modContrast", { hint:`, String.raw`  mkKnob(r6b, "modContrast", { name:"MOD>WIN", hint:`);
if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(p, s, "utf8");
console.log("layout2 patched");
