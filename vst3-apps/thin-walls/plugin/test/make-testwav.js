// A test signal for the live audio check: 3 s stereo, a 440 Hz tone on the left
// with a soft click train, pink-ish noise bursts on the right. 48 kHz 16-bit.
const fs = require("fs");
const path = require("path");
const out = process.argv[2] || path.join(__dirname, "testsignal.wav");
const fs_ = 48000, secs = 3, n = fs_ * secs;
const L = new Float32Array(n), R = new Float32Array(n);
let b0 = 0, b1 = 0, b2 = 0, seed = 1;
const rnd = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296 - 0.5; };
for (let i = 0; i < n; i++) {
  const t = i / fs_;
  L[i] = 0.4 * Math.sin(2 * Math.PI * 440 * t) * (0.6 + 0.4 * Math.sin(2 * Math.PI * 2 * t));
  if ((i % 24000) < 60) L[i] += 0.5 * Math.exp(-(i % 24000) / 12);
  const w = rnd();
  b0 = 0.99765 * b0 + w * 0.099; b1 = 0.963 * b1 + w * 0.2965; b2 = 0.57 * b2 + w * 1.0526;
  const pink = (b0 + b1 + b2 + w * 0.1848) * 0.15;
  R[i] = pink * ((Math.floor(t * 2) % 2 === 0) ? 1 : 0.1);
}
const buf = Buffer.alloc(44 + n * 4);
buf.write("RIFF", 0); buf.writeUInt32LE(36 + n * 4, 4); buf.write("WAVE", 8);
buf.write("fmt ", 12); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(2, 22);
buf.writeUInt32LE(fs_, 24); buf.writeUInt32LE(fs_ * 4, 28); buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34);
buf.write("data", 36); buf.writeUInt32LE(n * 4, 40);
for (let i = 0; i < n; i++) {
  buf.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(L[i] * 32767))), 44 + i * 4);
  buf.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(R[i] * 32767))), 46 + i * 4);
}
fs.writeFileSync(out, buf);
console.log("wrote " + out + " " + buf.length + " bytes");
