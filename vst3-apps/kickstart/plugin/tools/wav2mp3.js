/*  WAV -> MP3, with a pure-JS LAME so nothing has to be installed on the
    machine to publish a demo. 16-bit stereo in, 160 kbps out.

        node towav2mp3.js <indir> <outdir>
*/
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const HERE = __dirname;
const src = fs.readFileSync(path.join(HERE, 'lame.min.js'), 'utf8');
const sandbox = { window: {}, self: {}, console };
vm.createContext(sandbox);
vm.runInContext(src + ';this.__lamejs = lamejs;', sandbox);
/*  The bundle hangs its classes off the FUNCTION object and calls it itself at
    the end of the file, so what is wanted is the function, not its return. */
const lamejs = sandbox.__lamejs;

function readWav (file) {
  const b = fs.readFileSync(file);
  if (b.toString('ascii', 0, 4) !== 'RIFF') throw new Error('not RIFF: ' + file);
  let pos = 12, fmt = null, data = null;
  while (pos + 8 <= b.length) {
    const id = b.toString('ascii', pos, pos + 4);
    const sz = b.readUInt32LE(pos + 4);
    if (id === 'fmt ') fmt = { channels: b.readUInt16LE(pos + 10),
                               rate: b.readUInt32LE(pos + 12),
                               bits: b.readUInt16LE(pos + 22) };
    if (id === 'data') data = b.subarray(pos + 8, pos + 8 + sz);
    pos += 8 + sz + (sz & 1);
  }
  if (!fmt || !data) throw new Error('no fmt/data in ' + file);
  if (fmt.bits !== 16) throw new Error('expected 16-bit, got ' + fmt.bits);
  const frames = data.length / (2 * fmt.channels);
  const L = new Int16Array(frames), R = new Int16Array(frames);
  for (let i = 0; i < frames; i++) {
    L[i] = data.readInt16LE(i * 2 * fmt.channels);
    R[i] = fmt.channels > 1 ? data.readInt16LE(i * 2 * fmt.channels + 2) : L[i];
  }
  return { rate: fmt.rate, channels: fmt.channels, L, R, frames };
}

function encode (wav, kbps) {
  const enc = new lamejs.Mp3Encoder(2, wav.rate, kbps);
  const chunk = 1152;
  const out = [];
  for (let i = 0; i < wav.frames; i += chunk) {
    const l = wav.L.subarray(i, i + chunk);
    const r = wav.R.subarray(i, i + chunk);
    const buf = enc.encodeBuffer(l, r);
    if (buf.length) out.push(Buffer.from(buf));
  }
  const end = enc.flush();
  if (end.length) out.push(Buffer.from(end));
  return Buffer.concat(out);
}

const inDir = process.argv[2], outDir = process.argv[3];
// optional third argument: the file prefix (Beetmachine renders beetmachine-NN-*.wav)
const prefix = process.argv[4] || 'kickstart';
if (!inDir || !outDir) { console.log('usage: node towav2mp3.js <indir> <outdir>'); process.exit(1); }
fs.mkdirSync(outDir, { recursive: true });

const files = fs.readdirSync(inDir).filter(f => new RegExp('^' + prefix + '-\\d\\d-.*\\.wav$').test(f)).sort();
if (!files.length) { console.log('no ' + prefix + '-* wavs in ' + inDir); process.exit(1); }

for (const f of files) {
  const wav = readWav(path.join(inDir, f));
  const mp3 = encode(wav, 160);
  const dst = path.join(outDir, f.replace(/\.wav$/, '.mp3'));
  fs.writeFileSync(dst, mp3);
  console.log(path.basename(dst).padEnd(42)
    + (wav.frames / wav.rate).toFixed(1).padStart(6) + ' s  '
    + (mp3.length / 1024).toFixed(0).padStart(5) + ' KB');
}
