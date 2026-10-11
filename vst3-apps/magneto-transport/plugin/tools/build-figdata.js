// Magneto Transport - the cards' figure data (docs/PROTOCOL.md: figdata.json).
//
//   node tools/build-figdata.js
//
// Merges what the models measured with the figures that are closed forms of the same
// physics, into Source/ui/figdata.json:
//   docs/tips/fig-ref.json  - the reference model (mtref --figdata ... ): hysteresis,
//                             anhysteretic, biascurve, dub
//   docs/tips/fig-eng.json  - the real-time engine (mttest --figdata=...): recordzone,
//                             noise, wowflutter, speedhd, eqcurves, erase
//   here                    - karlqvist, playbackloss, delaygeometry, feedbackloop
// Fails if a figure PROTOCOL.md names is missing or empty.
"use strict";
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const read = p => JSON.parse(fs.readFileSync(path.join(root, p), "utf8"));
const out = {};
Object.assign(out, read("docs/tips/fig-ref.json"), read("docs/tips/fig-eng.json"));

const range = (a, b, n, log) => Array.from({ length: n }, (_, i) => log ? a * Math.pow(b / a, i / (n - 1)) : a + (b - a) * i / (n - 1));

// ---- the Karlqvist field of the record head (10 um gap, 0.3 um spacing) at four depths
{
  const g = 10, d = 0.3;
  const field = (x, y) => (Math.atan((g / 2 + x) / y) + Math.atan((g / 2 - x) / y)) / Math.PI;
  out.karlqvist = {
    title: "The record head's field along the tape (Karlqvist, 10 µm gap)",
    x: { label: "distance from the gap centre", unit: "µm", log: false },
    y: { label: "field", unit: "× deep-gap field", log: false },
    series: [1.75, 5.25, 8.75, 12.25].map(y => ({ label: `${y} µm deep`, points: range(-25, 25, 201).map(x => [x, field(x, y + d)]) })),
    marks: [{ x: -5, label: "gap edge" }, { x: 5, label: "gap edge" }], live: null };
}
// ---- what the playback head loses, against wavelength (gap 2.5 um, spacing 0.3 um, 14 um coating)
{
  const gap = 2.5, sp = 0.3, del = 14;
  const lam = range(2, 2000, 160, true);
  const db = v => 20 * Math.log10(Math.max(v, 1e-6));
  const gapL = l => { const k = 2 * Math.PI / l, a = k * gap / 2; return db(Math.abs(Math.sin(a) / a)); };
  const spL = l => db(Math.exp(-2 * Math.PI * sp / l));
  const thL = l => { const k = 2 * Math.PI / l; return db((1 - Math.exp(-k * del)) / (k * del)); };
  out.playbackloss = {
    title: "The playback head's three losses, against the recorded wavelength",
    x: { label: "wavelength", unit: "µm", log: true },
    y: { label: "loss", unit: "dB", log: false },
    series: [
      { label: "gap (2.5 µm)", points: lam.map(l => [l, Math.max(gapL(l), -40)]) },
      { label: "spacing (0.3 µm)", points: lam.map(l => [l, spL(l)]) },
      { label: "coating thickness (14 µm)", points: lam.map(l => [l, thL(l)]) }],
    marks: [{ x: 19.05e4 / 15000, label: "15 kHz at 19 cm/s" }, { x: 19.05e4 / 1000, label: "1 kHz at 19 cm/s" }], live: null };
}
// ---- delay = distance / speed
{
  out.delaygeometry = {
    title: "Delay = head distance / tape speed",
    x: { label: "head distance from the record head", unit: "cm", log: false },
    y: { label: "delay", unit: "ms", log: true },
    series: [4.75, 9.525, 19.05, 38.1, 76].map(v => ({ label: `${v} cm/s`, points: range(1.5, 24, 60).map(h => [h, 1000 * h / v]) })),
    marks: [], live: { param: "h1pos", map: "identity" } };
}
// ---- the feedback loop gain: FEEDBACK x 1.1 x head level / heads on, the measured onset marked
{
  let onset = 68;
  try { const n = read("Source/ui/tips-numbers.json").numbers["eng.feedback_onset_pct"]; if (n && n.value) onset = n.value; } catch (e) {}
  out.feedbackloop = {
    title: "The feedback wire's loop gain (one head at 0 dB, flat tone)",
    x: { label: "FEEDBACK", unit: "%", log: false },
    y: { label: "small-signal loop gain", unit: "", log: false },
    series: [{ label: "1.1 × FEEDBACK", points: range(0, 100, 51).map(f => [f, 1.1 * f / 100]) },
             { label: "unity: an echo as loud as the one before", points: [[0, 1], [100, 1]] }],
    marks: [{ x: onset, label: "runs away (measured)" }],
    live: { param: "feedback", map: "table", table: [[0, 0], [1, 100]] } };
}

// ---- check every PROTOCOL figure is here and has points
const proto = fs.readFileSync(path.join(root, "docs/PROTOCOL.md"), "utf8");
const m = proto.match(/Figure names \(all exist in figdata\.json\):([\s\S]*?)\n\n/);
const names = m ? [...m[1].matchAll(/`([a-z0-9]+)`/g)].map(x => x[1]) : [];
const bad = names.filter(n => !out[n] || !out[n].series || !out[n].series.length || out[n].series.some(s => !s.points.length));
if (bad.length) { console.error("figures missing or empty: " + bad.join(", ")); process.exit(1); }
fs.writeFileSync(path.join(root, "Source/ui/figdata.json"), JSON.stringify(out));
console.log(`figdata.json: ${Object.keys(out).length} figures (${names.length} required, all present)`);
