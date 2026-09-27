// 260905.1 — the record: design doc §13, the CLAUDE.md entry, the BUGLIST.
const fs = require("fs");
const misses = [];
const design = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/BRAIN-SCAN-DESIGN.md";
const claude = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
const buglist = "C:/Users/peter/b/BrainScan/BUGLIST.md";
const BT = String.fromCharCode(96);
const c = (t) => BT + t + BT;

{
  let s = fs.readFileSync(design, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const add = String.raw`
## 13. The bodies, and the rest of the list — 260905.1

Peter: *"can you make the other scanned body parts look more like CT scans of
those parts, i.e. simulated, but more realistic?"*, then *"it would be great
if the bone-related presets had some actual bones, and the skull looked more
like a skull"*, *"once an external file is imported I cannot switch to the
other samples"*, and *"please also do the other brain scan buglist"*.

**The split, decided by honesty.** The phantoms keep their formulas: SPINE at
(y, z) is a harmonic series with a known rolloff and that promise is the
bench's, so a spine-shaped SPINE would be a spine-shaped lie. The anatomy went
into the bodies instead — the three that existed (SKULL, CORTEX, LUNG → THORAX)
rebuilt from nothing in Hounsfield units in ` + c("Source/Anatomy.cpp") + String.raw`, and three
more (VERTEBRA, FEMUR, JAW) added for the bones Peter asked for. Fifteen slots
on the dial, with the migration extended (9 → 12 → 15, keyed on the build id).

**Anatomy as signed solids.** Every tissue is a test against an ellipsoid, a
capsule, a plate or an arch, layered the way a radiologist would list them —
vault with outer table, diploë and inner table, sutures jittered by noise;
orbits as cones that open on the face with a bony rim, globes and lenses;
nasal cavity and septum, maxillary, frontal and sphenoid sinuses, mastoid air
cells; an alveolar arch with a hard palate and cheekbones; a mandible with
rami and condyles; fourteen teeth on each arch with enamel, dentine, pulp and
tapering roots; brain with a cortical ribbon and sulci from a folded noise
field, falx, ventricles, deep grey, a cerebellum with finer folia, a stem; C1
and C2 with the cord; scalp in three layers; a couch behind the occiput. The
thorax has eight ribs a side sloping down anteriorly, costal cartilage,
sternum, clavicles, scapulae, a spine on a 2.8 cm period with canal and
processes, two lungs with a mottle, a bronchial tree with walls, pulmonary
vessels, a heart in its fat with brighter chambers, the aorta up over the arch
and down, the oesophagus, a liver dome and a stomach bubble. Each texel gets a
few HU of quantum noise so a narrow window looks like a scan. ` + c("toUnit()") + String.raw` maps
−1000..2000 HU onto the cube, so enamel is 1.0 and air 0.

**Measured, not admired.** Tissue fractions per body (THORAX 66 % air, SKULL
93 %, CORTEX 72 %; bone 0.8–6.2 %; enamel present where there are teeth), five
rays from the centre of the head all cross bone, the SKULL's cranium is empty
while CORTEX's holds white matter at the same place, the chest has two lungs
(32 % and 31 % air either side of the mediastinum at mid-height), the vertebra
has a canal of CSF (0.338 against 0.336) between two pedicles of bone.

**What the plates taught, three rounds of them.**

- *The gantry's opacity was linear per step*, ` + c("alpha = shape·density·dt") + String.raw`, so a
  0.7 cm skull table — two steps of 96 — came out a ghost however the density
  was set. Extinction, ` + c("1 − exp(−shape·density·dt)") + String.raw`, with DENSITY mapped
  exponentially (0.045 to 60 per unit, 1.65 at the middle where the phantoms
  were), makes thin bone opaque and leaves the phantoms as they shipped.
- *Under a bone window a body full of soft tissue is fog*: a = 0.27 for muscle
  accumulates over a unit of path into an opaque block. A Hounsfield volume now
  gets a threshold in the shader (` + c("uFloor") + String.raw` 0.30 of the window, a smoothstep
  gate) so a bone window shows bone; a phantom keeps the plain ramp.
- *The orbits were buried inside the vault*: modelled at Y 5.6 ± 2.5 they never
  reached the outer table at 8.8, so from outside the skull was an egg. Moved
  forward to open on the face, with a bony rim. And the maxilla was a
  superellipsoid brick; it is now the alveolar arch with a palate and cheekbones,
  and the nasal aperture is a real notch.
- *The window slider had a floor of 0.06 of the cube* = 180 HU on a body, so a
  BRAIN window (80 HU) could not be set. The panel probe caught it before the
  live run: floor 0.012 now.

**The rest of the list, shipped in the same build.** The import dial: dialling a
specimen while an import is in use clears the import (a dial that moved and
changed nothing read as broken). A SECOND HEAD per reader at a ratio of the
note — at ×1.00 with a phase offset a fixed-interval double (odd harmonics from
+3.9 dB to −106 dB at PHASE 50 %), off the integers a partial no harmonic
series contains (1.5 f₀ within 0.3 dB of f₀, 2nd harmonic −111 dB), reading a
coarser mip level at high ratios (×4 at C5: −56 dB floor). SPLIT lines — two
open segments in the two halves of the cycle, a second polyBLEP at phase 0.5
worth 33 dB. SCAN SPREAD for unison readers. MOD>WINDOW and MOD>GRAIN, the
per-voice window and kernel with an exact +0.0 when centred. FLATTEN, FLATTEN
ALL, REVERT (the processor keeps the lines a patch came with), one-step UNDO
from any gesture. Control points drawn on the gantry and draggable there, with
shift along the view ray. A GRID switch for the 128 texels. Radiographer's
presets in HU with the sliders reading HU on a body.

**Two of my own mistakes the bench caught.** The SPECS table's SPECIMEN entry
still said twelve slots after the list grew, so every dial value was off — the
ENAMEL study loaded CORTEX and played air, and MARROW loaded PULSE; the probe
that printed ` + c("loaded specimen 8") + String.raw` for a patch asking for 10 settled it in one run.
And a bench check written with ` + c("8.0f / 11.0f") + String.raw` failed the moment the dial had
fourteen intervals — bench values must use the dial's own scale. Plus one crash:
a ray probe indexing outside the cube at ` + c("(int)(−0.01 · 128)") + String.raw`.

Bench 118 checks ALL CLEAR; panel probe 43/43; live over CDP: presets in HU on
every body, the import cleared by the dial, FLATTEN ALL then REVERT restoring
the point counts, a split line sounding, UNDO, four handles on the gantry, no
script errors. Cost 16.7 % (32 readers), 18.5 % with the window on, 39 % with
a second head on every reader.
`;
  s = s.replace(/\s*$/, "") + NL + add.split("\n").join(NL);
  fs.writeFileSync(design, s, "utf8");
}
{
  let s = fs.readFileSync(claude, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const anchor = "- 2026-09-04 **real scans, verified live.**";
  const n = s.split(anchor).length - 1;
  if (n !== 1) misses.push("CLAUDE.md: expected 1 of the real-scans anchor, found " + n);
  else
  {
    const entry = String.raw`- 2026-09-05 **260905.1 — THE BODIES + the rest of the BUGLIST.** Peter: "make the other scanned body parts look more like CT scans… simulated, but more realistic", "bone-related presets had some actual bones, and the skull looked more like a skull", "once an external file is imported I cannot switch to the other samples", "please also do the other brain scan buglist". Design doc §13 has the numbers.
  - **The phantoms keep their formulas; the anatomy goes into the bodies.** SPINE's harmonic-series promise is the bench's, so a spine-shaped SPINE would be a lie. SKULL/CORTEX/THORAX rebuilt and VERTEBRA/FEMUR/JAW added, all in Hounsfield units (` + c("Source/Anatomy.cpp") + String.raw`, ` + c("toUnit()") + String.raw` maps −1000..2000 HU onto 0..1) as signed solids layered the way a radiologist lists them. Fifteen slots; ` + c("migrateSpecimen") + String.raw` now steps 9 → 12 → 15 by build id.
  - **A CT specimen is judged by looking, and the plates lied three rounds running.** (1) The gantry's opacity was linear per step, so a 0.7 cm skull table (two steps of 96) was a ghost at any DENSITY — extinction ` + c("1 − exp(−shape·density·dt)") + String.raw` with an exponential DENSITY range (0.045–60/unit, 1.65 at the middle so phantoms are untouched). (2) Under a bone window a soft-tissue body is FOG (a = 0.27 accumulates to opaque) — HU volumes get a shader threshold ` + c("uFloor") + String.raw` at a third of the window. (3) The orbits were modelled INSIDE the vault (never reaching the outer table), so from outside the skull was an egg; opened on the face with a bony rim; the maxilla brick became an alveolar arch + palate + cheekbones. **Verify anatomy by rendering it from outside, not by fractions.**
  - **The window slider had a floor of 180 HU** (0.06 of the cube), so a BRAIN window of 80 HU could not exist; the panel probe caught it (W 180). Floor 0.012 now, one constant pair.
  - **Two dial-scale bugs of my own, caught only by a probe that printed the loaded index**: the SPECS entry still said 12 slots after the list grew (every value off — the ENAMEL study loaded CORTEX and played air, "peak 1.000 rms 0.000"), and a bench check written ` + c("8.0f / 11.0f") + String.raw` failed the moment the dial had fourteen intervals. **A normalised list value must always be written against the dial's own count.** Plus a crash from a ray probe indexing ` + c("(int)(−0.01·128)") + String.raw` = −1.
  - **The rest of the list in the same build**: dialling a specimen while an import is active clears the import (a dial that moves and changes nothing reads as broken); 2ND HEAD / HEAD RATIO / HEAD PHASE (a second reader at a ratio — odd harmonics +3.9 → −106 dB at ×1.00 PHASE 50 %; 1.5 f₀ within 0.3 dB with the 2nd harmonic at −111 dB; ×4 at C5 reads a coarser level, −56 dB floor); SPLIT lines (two open segments, second polyBLEP at phase 0.5 worth 33 dB); SCAN SPREAD; MOD>WINDOW / MOD>GRAIN per voice (exact +0.0 centred); FLATTEN / FLATTEN ALL / REVERT (` + c("loadedLines") + String.raw` kept by the processor) / one-step UNDO hooked on capture-phase pointerdown; control points projected onto the gantry through the line pass's own MVP and draggable, shift = along the view ray; GRID; radiographer's presets in HU. A single-cycle read is still harmonic — the second head is what makes inharmonicity real.
  - Bench 118 ALL CLEAR (cost 16.7 / 18.5 / 39 % with second heads); panel probe 43/43; live CDP tour of all six bodies + the import-dial fix + flatten/revert/split/undo. An fmod per reader per sample cost 40 % of the read — precompute the fan.
`;
    s = s.replace(anchor, entry.split("\n").join(NL) + anchor);
    fs.writeFileSync(claude, s, "utf8");
  }
}
{
  let s = fs.readFileSync(buglist, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const add = String.raw`
## 2026-09-05 · 260905.1 — the bodies, and the list cleared

- **Shipped**: SKULL / CORTEX / THORAX rebuilt as anatomy in Hounsfield units; VERTEBRA, FEMUR, JAW added (fifteen slots, migration by build id); radiographer's window presets (BRAIN / SOFT / LUNG / BONE) with WINDOW and LEVEL reading HU on a body or an import; the gantry's opacity made an extinction with a bone threshold for HU volumes; dialling a specimen clears an import; items 2 (second head → 2ND HEAD / HEAD RATIO / HEAD PHASE, also the parked inharmonicity), 3 (SPLIT lines), 4 (control points on the gantry, shift = along the view ray), 5 (SCAN SPREAD); the parked MOD>WINDOW / MOD>GRAIN and the texel GRID; FLATTEN / FLATTEN ALL / REVERT / UNDO.
- **Still open**: item 1, the decals — waiting on ChatGPT; nothing to build until DELIVERED.md arrives.
- **Ideas from the round, parked**: a rib-cage-only THORAX (the way SKULL is to CORTEX); per-body default lines that trace a real structure (the aorta, the mandibular canal) as a study; a BONE 3D preset that also switches SURFACE for the phantoms; the 64³ panel copy limits how distinct teeth look in the head — a 96³ panel texture would help on the head only.
`;
  s = s.replace(/\s*$/, "") + NL + add.split("\n").join(NL);
  fs.writeFileSync(buglist, s, "utf8");
}
if (misses.length) { console.error("PARTIAL: " + misses.join("; ")); process.exit(1); }
console.log("docs patched: design §13, CLAUDE.md, BUGLIST");
