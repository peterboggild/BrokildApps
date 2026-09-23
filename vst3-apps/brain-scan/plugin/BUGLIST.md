# BRAIN SCAN — the list

Peter's standing rule: ideas are **collected here, not built**, until he says
go for a batch. A question like "can it do X?" gets an answer and an entry.

## Open — awaiting go

1. **Decals.** `BrokildApps/assets/brain-scan-decals/BRIEF.md` asks ChatGPT for
   twelve isolated parts in the CT-console register (knob cap, membrane button,
   bezel, nameplate, painted-steel texture, screen glass…). None delivered yet.
   The panel is fully procedural and does not need them; they would slot in
   through the `--decal-*` custom properties without touching the layout.

2. **A second reading head per line.** Two cursors on one line at different
   phases would give a fixed-interval double without a second voice. Cheap;
   changes nothing that exists.

3. **A line that is not a curve.** Everything here assumes one connected path.
   Two disjoint segments read as one cycle with two edges would be a different
   family of timbre.

4. **Draw on the gantry.** Editing happens on the slice because a mouse lives
   on a plane. Dragging a point *along the view ray* in the 3D view would be a
   real third gesture, and is the only obvious thing the panel cannot do.

5. **Per-voice SCAN offset.** Unison readers currently share a scan position.
   Spreading them through the body — a tiny offset per reader — is the natural
   companion to DETUNE and would widen without detuning.

## Shipped

- **260904.2** - IMPORT: a real volume off disk (NIfTI, a DICOM series, or a stack of images)
  resampled into the cube in millimetres, area-averaged, and windowed by percentile. 27 new bench
  checks and 6 new panel checks. Closes the old items 3 and 4 together - the import subsumed the
  bundled-scan idea, exactly as the recommendation said it would.

- **2026-09-04** — the Brokild Collection re-cut as all nine (75 MB), every binary taken from
  its own published download and hash-verified against it. Builders now in
  `BrokildWorldFX/tools/`.

- **260904.1** — the instrument: engine, nine specimens, three scanners on six
  lines, the CT console, the offline bench (38 checks) and the panel probe
  (24 checks). Published with a 17-page manual, landing page and zip.

## 2026-09-04 · after 260904.3 (the read)

- **Done in .3**: 128³ volume; GRAIN / CONTRAST / FOLD; SUTURE / ENAMEL / TENDON; the specimen dial 9 → 12 slots with a one-time migration keyed on the build id (the project state now carries "build"); the build off the timer.
- **Parked — true inharmonicity.** A single-cycle read is harmonic by construction; TENDON's bell ratios only shape the cycle. A second reader per voice at a non-integer ratio (a "second line at r × f0", mixed by amount) would give real beating partials. Peter's call.
- **Parked — the window as a destination.** GRAIN, CONTRAST and FOLD could be driven by the MOD line like scan/pitch/pan (a third scanner destination each), so the tissue itself decides how hard it is read.
- **Parked — a "texel" view.** With GRAIN at 1 the read passes texels as they are; the tomography slice could show the 128 grid when zoomed, so what is heard as grain is seen as grain.

## 2026-09-05 - line resets (Peter's question: "reset the slice curves individually and together, to start over with something easier and build complexity up again")

- **What exists**: per-line shape buttons in the line tool bar - LINE / RING Y / RING X / HELIX / WALK / LOOP - each lays that shape through the selected line's present centre (LINE is the "start simple" one: a straight line along x at the line's y, z). RANDOMISE BOTH redraws both anchors of one scanner; A -> B copies A onto B. There is NO reset for all six lines at once, and no way back to the patch's saved lines short of reloading the patch (ADMISSION is the fresh-instance state).
- **Proposed**: (1) a **FLATTEN** button per scanner pair and a **FLATTEN ALL** in the line tool bar: straight lines along x through the middle of the cube, A and B at slightly different y so SCAN still has somewhere to go - the simplest legible state; (2) **REVERT LINES** - put back the lines the current patch was loaded with (the processor keeps the loaded set); (3) an **UNDO** for the last line edit (one level is enough). Native side: a `{k:"shape", i:-1, kind:0}` meaning "every line" would do (1) with the existing handler.

## 2026-09-05 · 260905.1 — the bodies, and the list cleared

- **Shipped**: SKULL / CORTEX / THORAX rebuilt as anatomy in Hounsfield units; VERTEBRA, FEMUR, JAW added (fifteen slots, migration by build id); radiographer's window presets (BRAIN / SOFT / LUNG / BONE) with WINDOW and LEVEL reading HU on a body or an import; the gantry's opacity made an extinction with a bone threshold for HU volumes; dialling a specimen clears an import; items 2 (second head → 2ND HEAD / HEAD RATIO / HEAD PHASE, also the parked inharmonicity), 3 (SPLIT lines), 4 (control points on the gantry, shift = along the view ray), 5 (SCAN SPREAD); the parked MOD>WINDOW / MOD>GRAIN and the texel GRID; FLATTEN / FLATTEN ALL / REVERT / UNDO.
- **Still open**: item 1, the decals — waiting on ChatGPT; nothing to build until DELIVERED.md arrives.
- **Ideas from the round, parked**: a rib-cage-only THORAX (the way SKULL is to CORTEX); per-body default lines that trace a real structure (the aorta, the mandibular canal) as a study; a BONE 3D preset that also switches SURFACE for the phantoms; the 64³ panel copy limits how distinct teeth look in the head — a 96³ panel texture would help on the head only.

## 2026-09-05 · 260905.2 — the circuits

- **Shipped**: CIRCUIT (SVF / GROWL / SCREAM / LADDER), Black Rider's Sallen-Key and ladder per reader, each as LOW or HIGH; BAND stays the SVF's. SCREAM and LADDER sing in tune at full resonance; no oversampling needed at the measured −67 dB floor.
- **Parked**: a resonant highpass in series the way Black Rider has it (a second cutoff — a new line destination, arguably the FILTER B anchor's job); KEY TRACK as a per-circuit switch; the K laws as a MOD destination.
