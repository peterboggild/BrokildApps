// Numbers for ears that are not here: per-file peak, integrated RMS, crest,
// DC, and a spectral centroid (from 4096-point DFT bins over the whole file).
"use strict";
const fs = require("fs"), path = require("path");
const dir = process.argv[2] || "C:/Users/peter/b/Nineteen84/docs/audio";
function readWav(file) {
  const b = fs.readFileSync(file);
  let pos = 12, fmt = null, data = null;
  while (pos + 8 <= b.length) {
    const id = b.toString("ascii", pos, pos + 4), sz = b.readUInt32LE(pos + 4);
    if (id === "fmt ") fmt = { ch: b.readUInt16LE(pos + 10), rate: b.readUInt32LE(pos + 12) };
    if (id === "data") data = b.subarray(pos + 8, pos + 8 + sz);
    pos += 8 + sz + (sz & 1);
  }
  const n = data.length / (2 * fmt.ch);
  const L = new Float32Array(n), R = new Float32Array(n);
  for (let i = 0; i < n; i++) { L[i] = data.readInt16LE(i * 4) / 32768; R[i] = data.readInt16LE(i * 4 + 2) / 32768; }
  return { rate: fmt.rate, L, R, n };
}
const N = 4096;
const cosT = new Float64Array(N * 2), sinT = new Float64Array(N * 2);
for (let k = 0; k < N * 2; k++) { cosT[k] = Math.cos(2 * Math.PI * k / N); sinT[k] = Math.sin(2 * Math.PI * k / N); }
function centroid(x, rate) {
  // average power spectrum over hops of N, crude DFT of 64 log-spaced bins
  const bins = []; for (let i = 0; i < 48; i++) bins.push(Math.round(N * 40 * Math.pow(2, i / 5) / rate));
  const pw = new Float64Array(bins.length);
  let hops = 0;
  for (let s = 0; s + N <= x.length; s += N * 4) {
    hops++;
    for (let bi = 0; bi < bins.length; bi++) {
      const k = bins[bi]; if (k >= N / 2) continue;
      let re = 0, im = 0;
      for (let i = 0; i < N; i++) { const w = 0.5 - 0.5 * cosT[(i * 1) % N]; const idx = (k * i) % N; re += x[s + i] * w * cosT[idx]; im -= x[s + i] * w * sinT[idx]; }
      pw[bi] += re * re + im * im;
    }
  }
  let num = 0, den = 0;
  for (let bi = 0; bi < bins.length; bi++) { const f = bins[bi] * rate / N; num += f * pw[bi]; den += pw[bi]; }
  return den > 0 ? num / den : 0;
}
console.log("file                              peak    rms dBFS  crest  DC       centroid  stereo");
for (const f of fs.readdirSync(dir).filter(f => f.endsWith(".wav")).sort()) {
  const w = readWav(path.join(dir, f));
  let pk = 0, s2 = 0, dc = 0, diff = 0;
  for (let i = 0; i < w.n; i++) { const m = 0.5 * (w.L[i] + w.R[i]); pk = Math.max(pk, Math.abs(w.L[i]), Math.abs(w.R[i])); s2 += m * m; dc += m; diff += Math.abs(w.L[i] - w.R[i]); }
  const rms = Math.sqrt(s2 / w.n);
  const c = centroid(w.L, w.rate);
  console.log(f.padEnd(34) + pk.toFixed(3).padStart(5) + "  " + (20 * Math.log10(rms)).toFixed(1).padStart(7) + "  " + (20 * Math.log10(pk / rms)).toFixed(1).padStart(5) + "  " + (dc / w.n).toExponential(1).padStart(8) + "  " + Math.round(c).toString().padStart(6) + " Hz  " + (diff / w.n / rms).toFixed(2));
}
