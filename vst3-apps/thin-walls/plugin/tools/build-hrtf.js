// Build Source/HrtfData.h from the MIT KEMAR compact set (assets/hrtf/elev*/H*.wav).
//
// What it does, in order:
//   1. read every 128-tap stereo IR (44.1 kHz, left = channel 0, right = channel 1;
//      azimuths 0..180 only - the right hemisphere is the mirror, done at runtime)
//   2. diffuse-field equalisation: divide every magnitude by the RMS-average
//      magnitude over the sphere (area weighted), so the measurement rig's own
//      colouration and the dummy head's average response come out, leaving the
//      DIRECTIONAL cues - the standard first step for a rendering HRTF set
//   3. ITD from onset detection on 8x oversampled raw IRs (before any phase edit)
//   4. minimum phase via the real cepstrum, truncated to NTAP taps
//   5. write int16 taps + float ITD per direction as a header the engine and
//      the JUCE-free bench both include
//
// Run:  node tools/build-hrtf.js      (from the ThinWalls tree)

const fs = require("fs");
const path = require("path");

const root = path.join(__dirname, "..");
const src = path.join(root, "assets", "hrtf");
const out = path.join(root, "Source", "HrtfData.h");

const NTAP = 128;      // taps kept after min-phase (energy past ~90 is negligible)
const NFFT = 1024;
const FS = 44100;

// ---------- tiny complex FFT (radix-2) ----------
function fft(re, im, inverse) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = 2 * Math.PI / len * (inverse ? 1 : -1);
    const wr = Math.cos(ang), wi = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let k = 0; k < len / 2; k++) {
        const a = i + k, b = i + k + len / 2;
        const tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
        re[b] = re[a] - tr; im[b] = im[a] - ti;
        re[a] += tr; im[a] += ti;
        const ncr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = ncr;
      }
    }
  }
  if (inverse) for (let i = 0; i < n; i++) { re[i] /= n; im[i] /= n; }
}

function magnitude(h) {
  const re = new Float64Array(NFFT), im = new Float64Array(NFFT);
  for (let i = 0; i < h.length; i++) re[i] = h[i];
  fft(re, im, false);
  const m = new Float64Array(NFFT / 2 + 1);
  for (let k = 0; k <= NFFT / 2; k++) m[k] = Math.hypot(re[k], im[k]);
  return m;
}

// minimum-phase IR from a magnitude spectrum (real cepstrum method)
function minPhase(mag) {
  const re = new Float64Array(NFFT), im = new Float64Array(NFFT);
  for (let k = 0; k < NFFT; k++) {
    const kk = k <= NFFT / 2 ? k : NFFT - k;
    re[k] = Math.log(Math.max(mag[kk], 1e-9));
  }
  fft(re, im, true);                       // real cepstrum
  const c = new Float64Array(NFFT);
  c[0] = re[0];
  for (let n = 1; n < NFFT / 2; n++) c[n] = 2 * re[n];
  c[NFFT / 2] = re[NFFT / 2];
  const cr = Float64Array.from(c), ci = new Float64Array(NFFT);
  fft(cr, ci, false);
  for (let k = 0; k < NFFT; k++) {        // exp of complex
    const e = Math.exp(cr[k]);
    const a = ci[k];
    cr[k] = e * Math.cos(a); ci[k] = e * Math.sin(a);
  }
  fft(cr, ci, true);
  return Float64Array.from(cr.subarray(0, NTAP));
}

// onset time (in samples at FS) by threshold on the 8x oversampled |h|
function onset(h) {
  const up = 8, n = h.length;
  const re = new Float64Array(NFFT), im = new Float64Array(NFFT);
  for (let i = 0; i < n; i++) re[i] = h[i];
  fft(re, im, false);
  const N2 = NFFT * up;
  const r2 = new Float64Array(N2), i2 = new Float64Array(N2);
  for (let k = 0; k <= NFFT / 2; k++) { r2[k] = re[k]; i2[k] = im[k]; }
  for (let k = 1; k < NFFT / 2; k++) { r2[N2 - k] = re[NFFT - k]; i2[N2 - k] = im[NFFT - k]; }
  fft(r2, i2, true);
  let peak = 0;
  for (let i = 0; i < n * up; i++) peak = Math.max(peak, Math.abs(r2[i]) * up);
  const thr = 0.20 * peak;
  for (let i = 0; i < n * up; i++) if (Math.abs(r2[i]) * up >= thr) return i / up;
  return 0;
}

// ---------- read the set ----------
const elevs = [];
for (const d of fs.readdirSync(src)) {
  const m = /^elev(-?\d+)$/.exec(d);
  if (m) elevs.push(parseInt(m[1], 10));
}
elevs.sort((a, b) => a - b);

const dirs = [];   // {elev, az, L: Float64Array(128), R: ...}
for (const el of elevs) {
  const dir = path.join(src, "elev" + el);
  const files = fs.readdirSync(dir).filter(f => /^H-?\d+e\d+a\.wav$/.test(f));
  const ring = [];
  for (const f of files) {
    const m = /^H(-?\d+)e(\d+)a\.wav$/.exec(f);
    const az = parseInt(m[2], 10);
    const b = fs.readFileSync(path.join(dir, f));
    if (b.toString("ascii", 0, 4) !== "RIFF" || b.readUInt16LE(22) !== 2 || b.readUInt32LE(24) !== FS)
      throw new Error("unexpected wav layout: " + f);
    const nfr = b.readUInt32LE(40) / 4;
    const L = new Float64Array(nfr), R = new Float64Array(nfr);
    for (let i = 0; i < nfr; i++) {
      L[i] = b.readInt16LE(44 + i * 4) / 32768;
      R[i] = b.readInt16LE(46 + i * 4) / 32768;
    }
    ring.push({ elev: el, az, L, R });
  }
  ring.sort((a, b) => a.az - b.az);
  dirs.push(...ring);
}
console.log("directions read:", dirs.length, "elevations:", elevs.join(" "));

// ---------- diffuse-field average (both ears, area weighted, mirrored hemisphere counted) ----------
const avg = new Float64Array(NFFT / 2 + 1);
let wsum = 0;
for (const d of dirs) {
  const w = Math.cos(d.elev * Math.PI / 180) * ((d.az === 0 || d.az === 180) ? 1 : 2);
  const mL = magnitude(d.L), mR = magnitude(d.R);
  d.mL = mL; d.mR = mR;
  for (let k = 0; k <= NFFT / 2; k++) avg[k] += w * (mL[k] * mL[k] + mR[k] * mR[k]) / 2;
  wsum += w;
}
for (let k = 0; k <= NFFT / 2; k++) avg[k] = Math.sqrt(avg[k] / wsum);
// third-octave smoothing of the average, so the EQ removes colouration, not detail
const avgS = new Float64Array(NFFT / 2 + 1);
for (let k = 0; k <= NFFT / 2; k++) {
  const f = k * FS / NFFT;
  const lo = Math.max(0, Math.round(k / Math.pow(2, 1 / 6))), hi = Math.min(NFFT / 2, Math.round(k * Math.pow(2, 1 / 6)));
  let s = 0, n = 0;
  for (let j = lo; j <= hi; j++) { s += avg[j] * avg[j]; n++; }
  avgS[k] = Math.sqrt(s / Math.max(1, n));
  if (f < 100) avgS[k] = avgS[Math.round(100 / FS * NFFT)] || avgS[k];
}
// the EQ = 1/avgS, boost limited to +12 dB relative to the 1 kHz value
const ref = avgS[Math.round(1000 / FS * NFFT)];
const eq = new Float64Array(NFFT / 2 + 1);
for (let k = 0; k <= NFFT / 2; k++) eq[k] = ref / Math.max(avgS[k], ref / 3.98);

// ---------- per direction: ITD, EQ, min phase ----------
let maxItd = 0;
for (const d of dirs) {
  const tL = onset(d.L), tR = onset(d.R);
  d.itd = (tR - tL) / FS;           // positive = right ear LATER (source on the left)
  maxItd = Math.max(maxItd, Math.abs(d.itd));
  const eL = d.mL.map((v, k) => v * eq[k]), eR = d.mR.map((v, k) => v * eq[k]);
  d.hL = minPhase(eL); d.hR = minPhase(eR);
}

// sanity: what fraction of energy sits in the kept taps
let tailWorst = 0;
for (const d of dirs) {
  const full = minPhaseFull(d);
  tailWorst = Math.max(tailWorst, full);
}
function minPhaseFull(d) {
  // energy beyond NTAP of a longer min-phase render, relative to total
  const saved = NTAP;
  const m = d.mL.map((v, k) => v * eq[k]);
  const re = new Float64Array(NFFT), im = new Float64Array(NFFT);
  for (let k = 0; k < NFFT; k++) { const kk = k <= NFFT / 2 ? k : NFFT - k; re[k] = Math.log(Math.max(m[kk], 1e-9)); }
  fft(re, im, true);
  const c = new Float64Array(NFFT); c[0] = re[0]; for (let n = 1; n < NFFT / 2; n++) c[n] = 2 * re[n]; c[NFFT / 2] = re[NFFT / 2];
  const cr = Float64Array.from(c), ci = new Float64Array(NFFT);
  fft(cr, ci, false);
  for (let k = 0; k < NFFT; k++) { const e = Math.exp(cr[k]); const a = ci[k]; cr[k] = e * Math.cos(a); ci[k] = e * Math.sin(a); }
  fft(cr, ci, true);
  let tot = 0, tail = 0;
  for (let i = 0; i < NFFT; i++) { tot += cr[i] * cr[i]; if (i >= saved) tail += cr[i] * cr[i]; }
  return tail / tot;
}

// ---------- ring table ----------
const rings = elevs.map(el => {
  const r = dirs.filter(d => d.elev === el);
  return { elev: el, n: r.length, first: dirs.indexOf(r[0]) };
});

// ---------- write header ----------
let peak = 0;
for (const d of dirs) for (const h of [d.hL, d.hR]) for (const v of h) peak = Math.max(peak, Math.abs(v));
const scale = 32000 / peak;

const lines = [];
lines.push("// GENERATED by tools/build-hrtf.js from the MIT KEMAR compact HRTF set");
lines.push("// (Gardner & Martin, MIT Media Lab, 1994; free for use with attribution).");
lines.push("// Diffuse-field equalised, minimum phase, " + NTAP + " taps at 44100 Hz, plus the");
lines.push("// onset ITD of the raw pair. Azimuths 0..180 measured; the right hemisphere is");
lines.push("// the mirror. Do not edit by hand - rerun the script.");
lines.push("#pragma once");
lines.push("namespace tw { namespace hrtfdata {");
lines.push("static const int    NTAP = " + NTAP + ";");
lines.push("static const int    NDIR = " + dirs.length + ";");
lines.push("static const int    NRING = " + rings.length + ";");
lines.push("static const double FS = " + FS + ".0;");
lines.push("static const float  SCALE = " + (1 / scale).toExponential(8) + "f;   // int16 -> linear");
lines.push("// per ring: elevation (deg), number of azimuths (0..180 inclusive, evenly spaced), index of its first entry");
lines.push("static const int RING_ELEV[NRING] = {" + rings.map(r => r.elev).join(",") + "};");
lines.push("static const int RING_N[NRING]    = {" + rings.map(r => r.n).join(",") + "};");
lines.push("static const int RING_FIRST[NRING]= {" + rings.map(r => r.first).join(",") + "};");
lines.push("// azimuth of each entry, degrees, 0 = front, 90 = right");
lines.push("static const short AZ[NDIR] = {" + dirs.map(d => d.az).join(",") + "};");
lines.push("// interaural time difference, seconds, positive = source on the LEFT (right ear later)");
lines.push("static const float ITD[NDIR] = {" + dirs.map(d => d.itd.toExponential(6) + "f").join(",") + "};");
lines.push("// taps, int16, [dir][ear][tap], ear 0 = left");
lines.push("static const short TAPS[NDIR * 2 * NTAP] = {");
for (const d of dirs) {
  for (const h of [d.hL, d.hR]) {
    const row = [];
    for (const v of h) row.push(Math.round(v * scale));
    lines.push(row.join(",") + ",");
  }
}
lines.push("};");
lines.push("} }");
fs.writeFileSync(out, lines.join("\n") + "\n");

console.log("max |ITD| = " + (maxItd * 1e6).toFixed(0) + " us  (Woodworth at 90 deg with r=8.75 cm: 666 us)");
console.log("worst energy beyond " + NTAP + " taps: " + (tailWorst * 100).toExponential(2) + " %");
console.log("eq range: " + (20 * Math.log10(Math.min(...eq))).toFixed(1) + " .. " + (20 * Math.log10(Math.max(...eq))).toFixed(1) + " dB");
const d90 = dirs.find(d => d.elev === 0 && d.az === 90);
console.log("az 90 elev 0: ITD " + (d90.itd * 1e6).toFixed(0) + " us");
console.log("wrote " + out + " (" + (fs.statSync(out).size / 1024).toFixed(0) + " KB)");
