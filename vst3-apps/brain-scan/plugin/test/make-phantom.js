/*  A synthetic head, written as a real NIfTI-1 file, to import.

    Deliberately ANISOTROPIC - 0.5 x 0.5 x 2.0 mm, the shape of a real clinical
    CT - and in Hounsfield units, with a metal clip in it, so an import of this
    exercises every part of the path that matters: physical resampling, 4:1
    decimation in plane, and a window that has to ignore an outlier.

    node test/make-phantom.js <out.nii>
*/
const fs = require("fs");
const out = process.argv[2] || "phantom.nii";
const NX = 192, NY = 192, NZ = 40;
const SX = 0.5, SY = 0.5, SZ = 2.0;           // mm  -> 96 x 96 x 80 mm

const hdr = Buffer.alloc(352, 0);
hdr.writeInt32LE(348, 0);
hdr.writeInt16LE(3, 40);
hdr.writeInt16LE(NX, 42); hdr.writeInt16LE(NY, 44); hdr.writeInt16LE(NZ, 46);
hdr.writeInt16LE(4, 70);                       // int16
hdr.writeInt16LE(16, 72);
hdr.writeFloatLE(SX, 80); hdr.writeFloatLE(SY, 84); hdr.writeFloatLE(SZ, 88);
hdr.writeFloatLE(352, 108);
hdr.writeFloatLE(1, 112); hdr.writeFloatLE(0, 116);   // slope 1, intercept 0
hdr.writeInt16LE(1, 254);                      // sform
hdr.writeFloatLE(SX, 280); hdr.writeFloatLE(SY, 300); hdr.writeFloatLE(SZ, 320);
hdr.write("n+1\0", 344, "binary");

const vox = Buffer.alloc(NX * NY * NZ * 2);
let at = 0;
for (let k = 0; k < NZ; k++)
  for (let j = 0; j < NY; j++)
    for (let i = 0; i < NX; i++) {
      const x = (i + 0.5) * SX - NX * SX / 2;
      const y = (j + 0.5) * SY - NY * SY / 2;
      const z = (k + 0.5) * SZ - NZ * SZ / 2;
      const r = Math.sqrt(x * x + y * y * 1.25 + z * z * 1.1);
      let hu = -1000;                                   // air
      if (r < 44) hu = 40 + 6 * Math.sin(x * 0.9) * Math.cos(y * 0.8);   // brain, with folds
      if (r < 44 && Math.abs(x) < 1.4) hu = 12;         // the fissure
      if (r > 40 && r < 44) hu = 1100;                  // skull
      if (r < 12 && Math.abs(z) < 10) hu = 6;           // ventricles
      if (Math.hypot(x - 20, y - 10, z - 6) < 1.2) hu = 3000;   // a metal clip
      vox.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(hu))), at);
      at += 2;
    }
fs.writeFileSync(out, Buffer.concat([hdr, vox]));
console.log("wrote " + out + "  " + NX + "x" + NY + "x" + NZ +
            "  at " + SX + "x" + SY + "x" + SZ + " mm  (" +
            (NX * SX) + " x " + (NY * SY) + " x " + (NZ * SZ) + " mm)");
