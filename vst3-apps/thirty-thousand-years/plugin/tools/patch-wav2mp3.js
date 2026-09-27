// Let the mp3 tool read the render tool's 24-bit WAVs (rounded to 16-bit for LAME).
const fs = require("fs");
const path = "C:/Users/peter/b/ThirtyThousandYears/tools/wav2mp3.js";
let s = fs.readFileSync(path, "utf8");
const a = `  if (fmt.bits !== 16) throw new Error('expected 16-bit, got ' + fmt.bits);
  const frames = data.length / (2 * fmt.channels);
  const L = new Int16Array(frames), R = new Int16Array(frames);
  for (let i = 0; i < frames; i++) {
    L[i] = data.readInt16LE(i * 2 * fmt.channels);
    R[i] = fmt.channels > 1 ? data.readInt16LE(i * 2 * fmt.channels + 2) : L[i];
  }`;
const b = `  if (fmt.bits !== 16 && fmt.bits !== 24) throw new Error('expected 16- or 24-bit, got ' + fmt.bits);
  const bps = fmt.bits === 24 ? 3 : 2, frame = bps * fmt.channels;
  const frames = Math.floor(data.length / frame);
  const L = new Int16Array(frames), R = new Int16Array(frames);
  const rd = (o) => bps === 3 ? Math.max(-32768, Math.min(32767, Math.round(data.readIntLE(o, 3) / 256))) : data.readInt16LE(o);
  for (let i = 0; i < frames; i++) {
    L[i] = rd(i * frame);
    R[i] = fmt.channels > 1 ? rd(i * frame + bps) : L[i];
  }`;
if (s.split(a).length !== 2) { console.log("ANCHOR MISS"); process.exit(1); }
s = s.replace(a, b);
fs.writeFileSync(path, s);
console.log("wav2mp3 reads 24-bit");
