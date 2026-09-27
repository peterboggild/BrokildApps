/*  THIN-WALLS-DESIGN.md is the permanent record and it predates STUDIO, the
    per-surface materials, the broken walls and the rebuilt late field. Bring
    it up to 2026-09-22. Exact anchors, counted; nothing written on a miss. */

const fs = require("fs");
const P = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/THIN-WALLS-DESIGN.md";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function once(what, from, to) {
  const n = s.split(from).length - 1;
  if (n !== 1) { miss.push(what + "  (matched " + n + ")"); return; }
  s = s.replace(from, to);
}

once("the standfirst dates the second day",
`*A Brokild effect. Source tree \`C:\\Users\\peter\\b\\ThinWalls\`, plugin code \`ThWl\`, product
name "Thin Walls". Built 2026-09-21 on Claude Fable 5.1 from Peter's brief:`,
`*A Brokild effect. Source tree \`C:\\Users\\peter\\b\\ThinWalls\`, plugin code \`ThWl\`, product
name "Thin Walls". Built 2026-09-21 and finished 2026-09-22 on Claude Fable 5.1 from Peter's brief:`);

once("the four materials decision",
`- **Four materials, published octave-band absorption**: ABSORBING (drapes, thick carpet,
  soft furniture), FURNISHED (a living room), PLASTER (bare walls, wood floor), TILED.`,
`- **Five materials, published octave-band absorption**: ABSORBING (drapes, thick carpet,
  soft furniture), FURNISHED (a living room), PLASTER (bare walls, wood floor), TILED, and
  STUDIO — see §2a. Any of them on any of nine surfaces: the walls, the floor and the
  ceiling of each of the three rooms, floor and ceiling defaulting to AS WALLS so a patch
  that never touches them is unchanged.
- **Two walls of every room can be broken** at a movable point and that point pushed
  ±0.6 m along the wall's outward normal, so a room stops being a shoebox — see §2b.`);

once("the cost line in section 2",
`Cost: one source about 11 % of a core at 48 kHz, four sources with every door open 28 %.`,
`Cost: one source about 17 % of a core at 48 kHz, four sources with every door open 40 %.
The rise over the first day's 10 / 28 is the rebuilt late field (§2c): the loop had to grow
from 95–242 ms to 288–733 ms to satisfy its own modal criterion, and every line now carries
a chain of allpass diffusers.

## 2a. STUDIO, and why its absorption was not chosen by ear

The other four materials are measured surfaces. STUDIO's coefficients were **solved
backwards from Eyring** for the one property a control room is built to have — a decay that
does not move across the band — by holding \`−S ln(1−α) + 4mV\` constant at \`0.161 V / 0.70 s\`
for the hall. What falls out is **constant absorption near a quarter** with a little relief
at the very top to pay for the air absorption that takes the top octave anyway, which is
what broadband absorbers plus bass traps physically are. Measured on rendered audio: the
hall is **0.71 s at 250 Hz, 1 kHz and 4 kHz alike** (spread 1.00×) where the same room tiled
spreads 3.09×; the living room 0.39 / 0.40 / 0.41 s; the box room 0.29 / 0.30 / 0.31.

Its treatment is **diffusion, not absorption** (\`MATERIAL_SCATTER\`, ISO 17497, rising with
frequency as a diffuser does above its design frequency). A reflection keeps its energy and
loses the specular direction, so the room stays live while the flutter goes: the specular
reflections carry **17.6 %** of what the same room in plaster returns. It also splays its
walls slightly (\`MATERIAL_SPLAY\`, 0.11 m of deterministic perturbation per reflection
order). The early response of a centred source measures **1.1 dB** uneven in STUDIO against
**2.4 dB** in PLASTER. **The other four materials scatter and splay exactly zero**, so every
room that sounded a particular way on day one still does, sample for sample — the bench
asserts it per material.

## 2b. Rooms that are not boxes

The image-source method mirrors a source across a box by arithmetic and **cannot represent a
wall that is not parallel to an axis**. So a room is now a list of planar surfaces and a path
is a sequence of them, traced back from the listener and validated at every step; a square
room still takes the exact arithmetic, and the general search is validated against it — a
wall broken by **two millimetres** is geometrically the same room and the search lands within
**0.09 dB** overall and **0.20 dB** over the first 50 ms (27 paths against 29).

The two breakable walls of each room are the two that carry **no doorway**, so a break can
never collide with a door. Positive pushes the split point out of the room, which disperses;
**negative makes the wall concave, which focuses sound at a point and is worse than leaving
it flat** — the plan says so when a fold goes negative. The room's plan polygon, floor area
and volume all follow, so pushing a wall out really does make the room bigger: the hall's
floor goes 108.0 → 113.7 m² and a room's own decay 1.766 → 1.801 s at 1 kHz. A centred source
in a square hard room is 2.4 dB uneven across 300 Hz–1.2 kHz and 2.0 dB with both walls
broken.

**What it does NOT do, and the manual says so.** Here the splay acts on the early
reflections; the late field is statistical rather than modal, so breaking a wall does not
redistribute a room's standing waves the way it would in a modal model. Above the Schroeder
frequency — about 72 Hz for the hall as a studio — that is where splay does its audible work
in a real room too. Below it, absorption is what helps.

## 2c. The late field, rebuilt (2026-09-22)

Peter, after playing the build: *"the reverb and reflections are somewhat unpleasant …
perhaps that is a limit of the simulation strategy."* It was not; it was three faults, all
measurable. The full account with its numbers is \`docs/reverb-notes.md\` in the source tree.

- **Modal density.** An FDN's resonances sit \`1/L\` apart and are \`2.2/RT60\` wide, so the
  loop must satisfy \`L > RT60/2.2\` or they are individually resolvable — heard as pitched
  ringing. The network had 95–242 ms of loop against the 135–1392 ms its decays needed.
- **The cure is allpass diffusers inside every line.** An allpass has unity gain at every
  frequency so it **cannot change the decay**, but its length counts towards the loop (the
  modal density) and it splits every echo into a train (the echo density). Spectral ripple
  went from 6.1–10.7 dB to **5.6–6.6**, against the **5.57 dB** that a perfectly diffuse —
  i.e. Rayleigh-distributed — field has by construction.
- **A diffuser must not outring its room**: an allpass of length \`La\` at \`g = 0.62\` has a
  decay of \`14.5 La\` of its own, so the chain is sized to the decay and switched off for a
  room that barely reverberates, capped at \`RT60/43.5\` per allpass.
- **A Schroeder allpass holds the signal for exactly its own length** — the energy-weighted
  mean delay is \`La\`, with the \`(1−g²)\` cancelling. Charging the loop \`La/(1−g²)\` instead
  made every live decay 27 % short.
- **What is left over, measure**: each room now runs its own network at four design decays
  at startup, records how far the measured decay misses the design and how much energy the
  steady-state formula really delivers, and interpolates both in log decay at runtime
  (cached per room and rate, being a pure function of them). All fifteen room-and-material
  cases now land within a few per cent of Eyring.
- **Fractional delay was replaced by a 16-tap windowed sinc.** The old reader lost up to
  5 dB of the top octave and up to 1.9 dB broadband depending where a source sat; the direct
  path now sits within a tenth of a decibel of the physics from 10 Hz to 16 kHz, and nothing
  high-passes the bottom end. That is also what made the loop modulation affordable again —
  it had been removed the day before because interpolating inside the loop cost 3–5 dB per
  pass. **Stationary modes ring; moving ones do not.**`);

once("the bench section header and its content",
`## 3. The bench (\`test/bench.cpp\`, 67 checks, ALL CLEAR)`,
`## 3. The bench (\`test/bench.cpp\`, 90 checks, ALL CLEAR) and the panel probe (73)`);

once("the bench paragraph gains the new sections",
`rooms; four sources bounded at 28 % of a core.`,
`rooms; four sources bounded at 40 % of a core.

Added 2026-09-22: **section 8, the studio** — the flat decay per room with its spread
against TILED's, the hall still live at 0.71 s, the 17.6 % specular share, the
1.1-against-2.4 dB early response, and *four checks that the other four materials scatter
and splay nothing, so they sound exactly as they did*. **Section 9, broken walls** — the
2 mm equivalence against the exact shoebox, a 0.6 m push lengthening the decay, the
evenness of a centred source flat against broken, pushed-out dispersing (30 paths) against
pushed-in shadowing itself (28), every room and every fold from −0.6 to +0.6 m finite and
bounded (worst peak 1.032), and a wall moved under a sounding note keeping everything above
6 kHz 92 dB down.

The panel probe, test/uiprobe.js, drives the page in headless Chrome: **73 checks**,
including that W and the arrows walk while a menu or a fader has the focus and the control
never sees the key, that a key typed into a text field is still the field's, that every
door offers its handle from its own doorway, that the handle under the pointer is drawn
differently from one that is not, that the same handle out of reach is not hovered, that
dragging it swings the door inside one host gesture and is not also a look, and that a
choice list which grows to six renders six and one that shrinks takes the selector back
with it.`);

once("the open items",
`## 6. Not done / open

- Room geometry is fixed (one apartment). A second floor plan would be a table entry.
- Reflections are specular to second order; no scattering, no edge diffraction on the
  reflections themselves.`,
`## 6. Not done / open

- Room geometry is fixed (one apartment) beyond the two breakable walls per room. A second
  floor plan would be a table entry.
- Reflections are specular to second order and only STUDIO scatters; no edge diffraction on
  the reflections themselves.
- Echo density in the hall's first 50 ms is 0.61 rather than 1.0. The early reflections
  cover that region in the full render, so it may not matter; it has not been judged by ear.
- Sixteen lines is few. A larger network would satisfy the modal criterion for bare plaster
  and glazed tile without leaning on the modulation, which currently carries those two.
- Every room's field runs even when nobody is in it, because the coupling needs it. Skipping
  a silent, empty room would give most of the 17 % back.
- **The build id is still \`260921.1\`** although the second day added a great deal. Bumping
  it means a rebuild, and the standalone had just been relinked cleanly for the release zip;
  the shipped bytes and every document agree on 260921.1, which is the property that matters.`);

once("the panel section",
`POV WebGL2 view left (the camera is the listener; WASD walks, drag looks, doors block below
50 % aperture), architect's plan right (drag sources and the head, turn them by their
handles, click a door to toggle, drag along it for an aperture, rays of the live paths),
controls below with a hint on every one.`,
`POV WebGL2 view left (the camera is the listener; WASD walks, drag looks, doors block below
50 % aperture, and every door carries a **handle** — within 2.5 m, in front of you and not
through a wall, it lights up under the pointer, clicks to slam or throw open and drags to
swing), architect's plan right (drag sources and the head, turn them by their handles, click
a door to toggle, drag along it for an aperture, drag the diamond on a breakable wall to
splay it, rays of the live paths), seven control groups below with a hint on every one.
**The movement keys always win**: W A S D and the arrows walk and turn even when a menu or a
fader has the focus, and the control never sees the key — except in a text field, which
keeps its own.`);

if (miss.length) {
  console.error("NOTHING WRITTEN. " + miss.length + " anchor(s) did not match:");
  miss.forEach(function (m) { console.error("  - " + m); });
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log("THIN-WALLS-DESIGN.md updated");
