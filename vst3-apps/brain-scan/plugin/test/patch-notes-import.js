/*  BRAIN-SCAN-DESIGN.md and CLAUDE.md — the import round.
    node test/patch-notes-import.js
*/
const fs = require("fs");
const DES = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/BRAIN-SCAN-DESIGN.md";
const CMD = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
const miss = [];
function patch(file, name, from, to){
  let s = fs.readFileSync(file, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const f = from.split("\r\n").join("\n").split("\n").join(NL);
  const t = to.split("\r\n").join("\n").split("\n").join(NL);
  const n = s.split(f).length - 1;
  if (n !== 1){ miss.push(name + " (found " + n + ")"); return; }
  fs.writeFileSync(file, s.split(f).join(t));
}

patch(DES, "design section 11",
`**Hints are not optional here.** Every parameter carries the gloss the
processor already sends, and the probe fails if any control lacks one — 69
hints, placed beside the control and never over it (High Tide's round, applied
from the start).`,
`**Hints are not optional here.** Every parameter carries the gloss the
processor already sends, and the probe fails if any control lacks one — 69
hints, placed beside the control and never over it (High Tide's round, applied
from the start).

## 11. The import — 260904.2

Peter asked whether a real open CT could be a specimen. The honest answer was
that the DSP is nothing (\`Volume::build\` is the whole seam), that at 64³ a real
brain would be MUSHIER than the procedural CORTEX because cortical sulci are
below the sampling limit, and that the licence and the face-in-the-render
questions are clearance work rather than code — so an IMPORT is worth more than
a bundled scan, and subsumes it. He said build it.

**It overrides the specimen dial; it does not join it.** A tenth entry in a
nine-slot normalised parameter would silently re-point every saved patch —
1.0 means CORTEX today and would mean IMPORTED tomorrow — in project state as
well as in patch files, with nothing to migrate from. So the import lives in
the state blob beside the lines, the volume slot carries a sentinel, and no
existing patch changes meaning.

**Three things decide whether an import is any good, and none is the file
format.** Each is a bench check with a number.

- *Anisotropy.* A clinical CT is ~0.5 mm in plane and 1–5 mm between slices.
  Resampling by index squashes the body by exactly that ratio. Bench: a volume
  of 80 × 80 × 20 voxels at 0.5 × 0.5 × 2.0 mm is physically cubic, so it must
  fill the cube, and a sphere in it must measure the same on all three axes.
  It reads 48 / 48 / 44 — the 4 is partial volume in the source, one voxel of
  the coarse axis — where an index-based resampler would read about 12.
- *Decimation.* 512 × 512 × 300 into 64³ is ~8 × 8 × 5 source voxels each.
  Aliasing introduced here is in the specimen for good. Bench: a one-voxel
  checkerboard decimated 4:1 flattens to sd 0.00005; point sampling leaves 0.5.
- *Outliers.* One surgical clip at 3000 HU crushes every brain voxel into the
  bottom 2 % of a min-to-max window. Bench: one voxel at 30000 among values of
  0–100 gives a window of 0–103.

**A reader must be right or refuse out loud.** Compressed DICOM transfer
syntaxes and NIfTI-2 are named and refused with the remedy (\`dcm2niix\`), never
guessed at. Both readers are round-tripped in the bench from bytes synthesised
in memory, so the parsing is measured rather than mocked.

**Slice order comes from position, never from the filename.** A directory
listing is not slice order, and getting it wrong shuffles or mirrors the body
with no other symptom. Slices are sorted along the slice normal (from the cross
product of ImageOrientationPatient) and the slice spacing is derived from the
positions rather than believed from the tag. Bench: five slices fed in the
order 8, 0, 4, 12, 16 come back as 0, 4, 8, 12, 16 with dz = 4.00 mm.

**The bug the live test found, that nothing else could.** \`emitVolume\` began
\`if (v.specimen < 0 ...) return;\` — the guard for "nothing built yet", which
is −1. \`SPEC_IMPORTED\` is −2, so an imported volume was built, played and
reported correctly while the PANEL was never sent it: the slice and the gantry
went on showing SPINE. Every static check passed. Only reading the page's own
copy of the volume back over CDP showed the checksum had not moved.

**A modal file chooser cannot be driven by a probe**, so the same operation
also exists with the path handed in (\`{k:"importPath"}\`). That is what makes
the feature verifiable at all, and it is what a file dropped on the panel would
call.

**What it changes about the instrument.** With a CT loaded, WINDOW and LEVEL
stop being a metaphor and become a radiographer's window width and level in
Hounsfield units. And it need not be a brain: any volumetric data is a
specimen — a micro-CT of a fossil has the same novelty and none of the licence
or privacy questions.`);

patch(CMD, "claude md",
`- Registered in \`install-fleet.ps1\` and \`check-names.js\`.`,
`- 2026-09-04 **260904.2 — IMPORT: a real volume off disk** (Peter asked whether an open-source CT could be a specimen; the answer was that an import beats a bundled scan and subsumes it, and he said build it). \`Source/Import.{h,cpp}\` is plain C++ so the bench measures it; \`Source/ImportJuce.h\` is the thin file layer. NIfTI-1 (.nii/.nii.gz), an uncompressed DICOM series, or a stack of PNG/JPEG.
  - **It OVERRIDES the specimen dial, it does not join it.** A tenth entry in a nine-slot normalised parameter would silently re-point every saved patch (1.0 = CORTEX today, IMPORTED tomorrow) in project state as well as patch files, with nothing to migrate from. The import lives in the state blob beside the lines; the volume slot carries \`SPEC_IMPORTED = -2\`.
  - **Three things decide whether an import is any good and NONE is the file format**, each a bench number: anisotropy (resample in MILLIMETRES — a 80x80x20 volume at 0.5x0.5x2.0 mm is physically cubic and a sphere in it must stay a sphere; index-based would read ~12 voxels on the coarse axis instead of 44), decimation (a one-voxel checkerboard 4:1 flattens to sd 0.00005 vs 0.5 for point sampling — aliasing introduced at import is in the specimen for good), and outliers (one voxel at 30000 among 0..100 still gives a window of 0..103, because the window is a PERCENTILE not the extremes).
  - **Slice order comes from POSITION, never the filename** (sorted along the slice normal from ImageOrientationPatient; dz derived from the positions, not believed from the tag) — a directory listing is not slice order and getting it wrong mirrors the body with no other symptom. **A reader must be right or refuse out loud**: compressed DICOM and NIfTI-2 are named and refused with the remedy (dcm2niix), never guessed at. Both readers are round-tripped in the bench from bytes synthesised in memory.
  - **THE BUG ONLY A LIVE TEST COULD FIND:** \`emitVolume\` opened \`if (v.specimen < 0 ...) return;\` — the guard for "nothing built yet", which is −1 — so with \`SPEC_IMPORTED = -2\` the imported volume was built, played and reported correctly while the PANEL was never sent it and went on drawing SPINE. Every static check passed; only reading the page's own copy of the volume back over CDP showed the checksum had not moved. **A negative sentinel added next to an existing one will be swallowed by any \`< 0\` guard.**
  - **A modal file chooser cannot be driven by a probe**, so \`{k:"importPath", path}\` does the same job with the path handed in. Without it the feature would have been verifiable only by hand. \`test/make-phantom.js\` writes a deliberately anisotropic NIfTI head (0.5x0.5x2.0 mm, in HU, with a metal clip) to import; \`test/import-jobs.json\` drives the whole round trip live.
  - Bench 65 checks, panel probe 30. The imported cube is SAVED with the patch and the project (gzip+base64) so a patch is still the whole machine when the source folder is tidied away. With a CT loaded WINDOW and LEVEL are literally a radiographer's, in Hounsfield units.
- Registered in \`install-fleet.ps1\` and \`check-names.js\`.`);

if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
console.log("design doc and CLAUDE.md updated");
