# The drum family - list (Kickstart, Snare Tactics, Hats Off, Beetmachine)

House rule: things get **collected here** and built in batches when Peter says go.
One list for the four, because most items span them: Beetmachine shows each drum's
OWN panel, so anything done to a drum's panel shows up inside Beetmachine too.

---

## 1. Reskin the three daughter VST3s in the machine style - SHIPPED 2026-09-26 (see below)

Peter, 2026-09-26: "wouldn't it make sense if the three daughter VST3s were styled in the
same way, aesthetically?" Yes - the art for it is already delivered, in `assets/drum-decals/`
(brief: BRIEF.md, delivery note: DELIVERED.md, crop rectangles: manifest.json):

- **Kickstart** = the steam pile-driver: `kickstart-ground.png` (red-lead primer on cast iron),
  `kickstart-nameplate.png` (raised cast-iron letters).
- **Snare Tactics** = military field gear: `snare-tactics-ground.png` (olive drab),
  `snare-tactics-nameplate.png` (white stencil on olive).
- **Hats Off** = the lathe in a cymbal foundry: `hats-off-ground.png` (machine-tool grey-green),
  `hats-off-nameplate.png` (engraved brass).
- **Shared parts**, the same as Beetmachine uses: bakelite knobs (red for DRIVE), the palm HIT
  button, jewel lamps, label tape, the chicken-head selector for stepped choices (ENGINE, KEYS).

How: each drum's panel is native JUCE drawn by its own LookAndFeel (KsLook / StLook / HoLook), so
it is a reskin of an existing layout - the geometry does not move - not a redesign. Beetmachine's
`BeetLook` + `tools/ingest-decals.ps1` are the working pattern to copy (nine-slice for framed parts,
knobs rotated from a pointer-up decal with an amber mark on the scale so the setting stays readable).

Known limits of the delivery, to honour in every panel:
- The switch/button "pressed" drawings are NOT registered with the "up" ones (they jump) - use the
  UP drawing and press it in code. The lamp on/off pairs ARE registered.
- A steel knob skirt over the scale made settings unreadable in Beetmachine - left out there.

Coordinate with whoever owns the drums' trees at the time (another session built them).

## 2. A drive for the whole kit? (question, 2026-09-26)

Peter: "is there a global drive on some kits? I cannot see any such control."
**There isn't one.** Each drum has its own DRIVE section inside its own panel (the four Battlestar
engines IDLE BURN / HYPERDRIVE / RAZOR WING / SUPERNOVA, with DRIVE and COLOUR), and a kit only sets
what each drum's preset sets. Beetmachine itself adds level, pan, chokes and routing, no processing.
Possible addition: a BUS DRIVE on Beetmachine's main mix (one of the same four engines, one knob,
maybe a parallel blend), so a whole kit can be pushed together - the way drums are often crushed as a
bus. Would sit in the header beside MASTER. Awaiting Peter's call.

## 3. Beetmachine: a proper BEETMACHINE nameplate

The delivered plate reads BEET (ordered before the rename). Today the blank enamel plate is lettered
BEETMACHINE in code. Order one plate: "BEETMACHINE in white on a black enamel sign, chipped, four
rivets", same prompt style as BRIEF.md sheet 8.

## 4. Beetmachine: per-slot articulation (idea)

Send Hats Off's closed / pedal / open / bell / edge, or Snare Tactics' snare / rim / cross-stick / clap,
from one slot's choice - so one hi-hat preset can be closed in slot 5 and open in slot 6. Needs the
drum's KEYS on GM KIT; Beetmachine already reads KEYS live.

## 6. Beetmachine: the kits' mix runs hot (found 2026-09-26, rendering the demos)

At MASTER 0 dB the raw main mix of a kit playing a normal groove peaks ABOVE full scale:
STUDIO 1.76 (+4.9 dBFS, kick + snare + crash together), 909 1.53, TECHNO 1.49, 808 1.41,
BOOM BAP 1.27. Inside a DAW's float mix that is harmless until the master bus, but a user
who bounces or monitors straight out clips. The demos are normalised to -1 dBFS, so they
hide it. Options, Peter's call: bring the kits' own slot levels down ~6 dB (a kit edit,
nothing else moves), or a MASTER default of -6 dB, or a soft ceiling on the mix bus.
Measure with `beetrender` (it prints the gain it had to apply per demo).

## 5. Beetmachine: the drum's own HIT pad does not choke other slots (known)

The pad inside a drum's panel plays that drum directly, so Beetmachine never sees the hit: no lamp,
no chokes. Beetmachine's own palm button on the slot does both.

---

## SHIPPED

- **Beetmachine 260926.2** (2026-09-26): a slot selected for the SECOND time showed "SLOT n IS EMPTY"
  (Peter, in the host). JUCE's editor destructor does not tell its processor it is gone - the host
  must call `editorBeingDeleted()` first, and inside Beetmachine, Beetmachine is the host. The drum
  kept a dangling "active editor" and refused to make another. Fixed, and `beetshot` now replays a
  DAW session (reopen, preset, kit, empty/refill, state load open and closed, every slot twice).
  Same build: the NOTES readout said CUSTOM over a plain C3 row - the layout is now computed from the
  notes instead of stored.

- **The daughters in the machine style** (2026-09-26, Kickstart 260926.1, Snare Tactics 260926.2, Hats Off
  260926.2, Beetmachine 260926.3). One shared parts set, st3-apps/machine-art/ (ingest.ps1 cuts the art,
  MachineArt.h/.cpp draws it, machine-art.cmake embeds it; refresh-shots.ps1 remakes each drum's page and manual
  images from its panel test). Layout untouched; knobs keep their coloured value arcs; DRIVE red, Hats Off's METAL
  bronze; Kickstart's pad became the palm button, while the snare's and the cymbal's pads kept their drawings
  because WHERE you hit them matters. Beetmachine's private art copy retired - one set for the family.

- **Beetmachine 260926.4** (2026-09-26): an EMPTY card drew "empty bay / costs nothing" on top of its
  greyed-out knobs and note buttons. An empty card now hides its preset row, pad, LEVEL/PAN and M/S and
  centres the text where the knobs were; `beetshot` asserts it (15 controls shown against a live card's 23).
  Released with the manual, the landing page, and the Beetmachine Collection re-cut as FOUR (49 MB).

- **Kickstart 260926.2 + Beetmachine 260926.5** (2026-09-26): Peter's new head-on push buttons
  (`assets/drum-decals/05-buttons.png`, cut by `machine-art/ingest-buttons.ps1`). Unlike the first
  delivery, up and pressed are REGISTERED (checked at ingest: plate masks best at zero shift), so a held
  button now shows the real pressed drawing instead of a sunk-and-darkened up one.
- **Beetmachine 260926.5: the BWFX rack** on the main mix (Peter: a BWFX button on Beetmachine, not
  on the daughters). One machine-style steel button with the teal globe; the STANDARD native panel
  (Rite of Passage's, a port of `BrokildWorldFX/ui/bwfx-rack.js`; Legion carries the same code); five
  macros as host parameters. A slot on its OWN output goes round the rack; a kit load leaves it alone;
  pre-BWFX projects load with the empty rack. beettest 33, beetshot checks the rack opens opaque and draws.
- **SUPERSEDED the same day - drawn pads for the whole family (Kickstart/Snare Tactics/Hats Off 260926.3,
  Beetmachine 260926.6).** Peter: the mushroom buttons were only on two of the four, "it has to be
  consistent", and he likes the zoned pads. One drawing, `machineart::drawPad`, for every pad: kick = cast-iron
  hoop with slotted lugs round a worn coated head; snare = chrome hoop that IS the rim-shot zone (kRimFrom);
  hats = bronze cymbal, lathe grooves, raised bell inside kBellFrom; STOP = glossy red head in a chipped
  yellow ring. Beetmachine's slot pads show the drum in the slot. Mouse zones untouched - only the drawing.
  The palm button and its four PNGs were retired from the kit (05-buttons.png + ingest-buttons.ps1 kept).
- **Beetmachine demos**: six on the landing page (`test/beetrender.cpp` -> Kickstart's `wav2mp3.js`,
  now with a prefix argument). No hat pattern puts closed and open on the same step - the closed hat
  chokes the open one, so a shared step would silence it the instant it starts.