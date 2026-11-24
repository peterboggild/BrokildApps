/* Encode a raw RGBA8 image to KTX2 (Basis Universal UASTC, zstd, mipmaps).
 *
 *   node ktx2.mjs <in.rgba> <width> <height> <out.ktx2> [srgb] [etc1s] [q=2] [rdo=1.0]
 *
 * Uses the WebAssembly build of the Basis Universal encoder (Apache 2.0), which
 * needs no installation: put basis_encoder.js and basis_encoder.wasm from
 * github.com/BinomialLLC/basis_universal (webgl/encoder/build) in
 * %LOCALAPPDATA%/moon-data/ktx (or set BASIS_ENCODER_DIR).
 */
import { createRequire } from 'node:module';
import fs from 'node:fs';
import path from 'node:path';

const [inFile, W, H, outFile, ...opts] = process.argv.slice(2);
if (!outFile) { console.error('usage: node ktx2.mjs in.rgba w h out.ktx2 [srgb] [q=N] [rdo=X]'); process.exit(1); }
const w = +W, h = +H;
const srgb = opts.includes('srgb');
const etc1s = opts.includes('etc1s');           /* much smaller, lower quality: fine for the slowly varying colour maps */
const opt = (k, d) => { const o = opts.find(s => s.startsWith(k + '=')); return o ? +o.split('=')[1] : d; };
const q = opt('q', 2), rdo = opt('rdo', 1.0);

const dir = process.env.BASIS_ENCODER_DIR || path.join(process.env.LOCALAPPDATA, 'moon-data', 'ktx');
const require = createRequire(import.meta.url);
const BASIS = require(path.join(dir, 'basis_encoder.js'));
const wasmBinary = fs.readFileSync(path.join(dir, 'basis_encoder.wasm'));

const pixels = fs.readFileSync(inFile);
if (pixels.length !== w * h * 4) { console.error(`size mismatch: ${pixels.length} bytes for ${w}x${h}`); process.exit(1); }

const M = await BASIS({ wasmBinary });
M.initializeBasis();
const enc = new M.BasisEncoder();
enc.setCreateKTX2File(true);
enc.setKTX2UASTCSupercompression(true);
enc.setKTX2SRGBTransferFunc(srgb);
enc.setSliceSourceImage(0, new Uint8Array(pixels.buffer, pixels.byteOffset, pixels.length), w, h, false);
if (etc1s) { enc.setUASTC(false); enc.setQualityLevel(220); enc.setCompressionLevel(2); }
else {
  enc.setUASTC(true);
  enc.setPackUASTCFlags(q);            /* 0 fastest .. 3 slower/better */
  if (rdo > 0) { enc.setRDOUASTC(true); enc.setRDOUASTCQualityScalar(rdo); }
}
enc.setMipGen(true);
enc.setPerceptual(srgb);
enc.setMipSRGB(srgb);
enc.setDebug(false);
const out = new Uint8Array(w * h * 2 + 4096);
const t0 = Date.now();
const n = enc.encode(out);
enc.delete();
if (!n) { console.error('encode failed'); process.exit(1); }
fs.writeFileSync(outFile, out.subarray(0, n));
console.log(`${path.basename(outFile)}: ${w}x${h} -> ${(n / 1048576).toFixed(2)} MB in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
