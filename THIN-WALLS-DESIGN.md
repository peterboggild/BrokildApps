# THIN WALLS — design and what the build measured

*A Brokild effect. Source tree `C:\Users\peter\b\ThinWalls`, plugin code `ThWl`, product
name "Thin Walls". Built 2026-09-21 and finished 2026-09-22 on Claude Fable 5.1 from Peter's brief: an apartment
of three rooms with doors that open and close, a sound source and a binaural listener
placed anywhere, orientation that matters, and "exquisite quality — look at research and
math, so that what is known about this type of psychoacoustic sound treatment is baked in".*

## 1. The brief, and the calls made from it

Peter asked for: a small, a large and a giant room; doors that open or close; independent
placement of source and listener, in the same or different rooms; the orientation of the
binaural microphone mattering; close, far and behind-a-door all clearly audible (not just
level: "the characteristics, reverberation etc."); a POV 3D view one can walk around in and
a plan view; wall materials in at least three grades; CPU allowed to be heavy if it buys
realism. Added during the build at his request: a standalone with a WAV loop player, an
EAR SPACING control (a head up to a 1 m spaced pair), up to four sources with an input
selector each (main or AUX bus, left, right or both), a PURE point source with optional
directionality shown as a sphere on a stand, and a physically modelled LOUDSPEAKER —
"realistically modelled visually and audially".

Decisions taken alone, as the standing rule allows:

- **Three shoebox rooms with a real floor plan.** LARGE 6×5×2.8 m, SMALL 3×3.5×2.5 m,
  GIANT 12×9×5 m, every pair joined by a door, so every "behind the door" case exists,
  including the two-door route through the third room. Doors 0.9×2.05 m with a hinge
  side: a 30 % aperture is a real 27 cm strip beside the frame.
- **Five materials, published octave-band absorption**: ABSORBING (drapes, thick carpet,
  soft furniture), FURNISHED (a living room), PLASTER (bare walls, wood floor), TILED, and
  STUDIO — see §2a. Any of them on any of nine surfaces: the walls, the floor and the
  ceiling of each of the three rooms, floor and ceiling defaulting to AS WALLS so a patch
  that never touches them is unchanged.
- **Two walls of every room can be broken** at a movable point and that point pushed
  ±0.6 m along the wall's outward normal, so a room stops being a shoebox — see §2b.
- **The direct-to-reverberant ratio is physics, not a knob.** The late field is calibrated
  to 16π/A relative to the direct sound at 1 m, so distance cues come out right without
  tuning. The trims (DIRECT, EARLY, REVERB) are there to taste, defaulting to 0 dB.
- **No BWFX rack**: it is an effect (the Martian Gain / Battlestar call).
- **The name.** Sound through a closed door and a party wall is modelled; that is what the
  name says.

## 2. The model

Every path from a source to the listener is rendered with its own delay, 1/r spreading,
ISO 9613-1 air absorption, per-band wall absorption, the source's directivity and a measured
HRTF pair for its direction of arrival:

1. **Direct sound**, with each ear at its own distance (the near-field ILD growth below 1 m
   falls out of the geometry).
2. **Early reflections** by the image-source method to second order in a shoebox (up to 24
   images), each losing √(1−α) per bounce. A bounce that lands in an open doorway is deleted:
   that energy left the room.
3. **Through a doorway**: the straight line from a source (or its first-order images) to the
   listener (or its images) crosses the door plane; inside the open strip it is line of
   sight, otherwise the path bends at the nearest point of the opening and loses Maekawa's
   diffraction attenuation per band, 10·log10(3+20N), N = 2δ/λ, with a smooth tail below the
   shadow boundary so an edge never switches on with a click. The sound is localised at the
   doorway. A two-door route through the third room bends twice.
4. **Through the closed leaf and the party wall**: mass-law transmission loss (a light
   interior door 14→32 dB across 125 Hz–8 kHz; a wall 30→53 dB), radiated from the point of
   the panel nearest the straight line, so a shut door is still exactly where the muffled
   sound comes from.
5. **Late field**: one 16-line feedback delay network per room, delays from the mean free
   path 4V/S, per-band decay from Eyring's formula with open doors counted as absorbers and
   air absorption included. The three networks are coupled through the doors and walls with
   the energy-balance coefficient of coupled-room theory, ONE WAY, down a ranking of the
   rooms by the source power in them (see §4). The listener's own room is rendered as a
   diffuse field through eight decorrelated HRTF directions; a neighbouring room's field is
   heard through its doorway, spatialised at the door and as wide as the door.
6. **The head**: the MIT KEMAR compact set (368 directions), diffuse-field equalised,
   converted offline to minimum phase plus a separate onset ITD (`tools/build-hrtf.js`),
   resampled to the host rate at prepare, bilinearly interpolated on the grid, 128 taps at
   44.1 kHz. EAR SPACING scales the ITD and the near-field ear distances; the pinna
   colouration stays a head's.
7. **Sources.** PURE: a point with a cardioid tendency (1−d)+d(1+cos θ)/2, frequency
   independent. LOUDSPEAKER: a two-way box — the piston formula |2J₁(ka sin θ)/(ka sin θ)|
   for a 6.5" woofer below 2.2 kHz and a 1" tweeter above, plus a finite-baffle term that
   goes cardioid-like with frequency (22 dB down straight behind at 4 kHz and up), floored
   at −30 dB. The radiated-power correction of that pattern (−10·log₁₀ Q) scales what the
   room's field receives, so a directional source drives the room less than an omni one.

Cost: one source about 17 % of a core at 48 kHz, four sources with every door open 40 %.
The rise over the first day's 10 / 28 is the rebuilt late field (§2c): the loop had to grow
from 95–242 ms to 288–733 ms to satisfy its own modal criterion, and every line now carries
a chain of allpass diffusers.

## 2a. STUDIO, and why its absorption was not chosen by ear

The other four materials are measured surfaces. STUDIO's coefficients were **solved
backwards from Eyring** for the one property a control room is built to have — a decay that
does not move across the band — by holding `−S ln(1−α) + 4mV` constant at `0.161 V / 0.70 s`
for the hall. What falls out is **constant absorption near a quarter** with a little relief
at the very top to pay for the air absorption that takes the top octave anyway, which is
what broadband absorbers plus bass traps physically are. Measured on rendered audio: the
hall is **0.71 s at 250 Hz, 1 kHz and 4 kHz alike** (spread 1.00×) where the same room tiled
spreads 3.09×; the living room 0.39 / 0.40 / 0.41 s; the box room 0.29 / 0.30 / 0.31.

Its treatment is **diffusion, not absorption** (`MATERIAL_SCATTER`, ISO 17497, rising with
frequency as a diffuser does above its design frequency). A reflection keeps its energy and
loses the specular direction, so the room stays live while the flutter goes: the specular
reflections carry **17.6 %** of what the same room in plaster returns. It also splays its
walls slightly (`MATERIAL_SPLAY`, 0.11 m of deterministic perturbation per reflection
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
measurable. The full account with its numbers is `docs/reverb-notes.md` in the source tree.

- **Modal density.** An FDN's resonances sit `1/L` apart and are `2.2/RT60` wide, so the
  loop must satisfy `L > RT60/2.2` or they are individually resolvable — heard as pitched
  ringing. The network had 95–242 ms of loop against the 135–1392 ms its decays needed.
- **The cure is allpass diffusers inside every line.** An allpass has unity gain at every
  frequency so it **cannot change the decay**, but its length counts towards the loop (the
  modal density) and it splits every echo into a train (the echo density). Spectral ripple
  went from 6.1–10.7 dB to **5.6–6.6**, against the **5.57 dB** that a perfectly diffuse —
  i.e. Rayleigh-distributed — field has by construction.
- **A diffuser must not outring its room**: an allpass of length `La` at `g = 0.62` has a
  decay of `14.5 La` of its own, so the chain is sized to the decay and switched off for a
  room that barely reverberates, capped at `RT60/43.5` per allpass.
- **A Schroeder allpass holds the signal for exactly its own length** — the energy-weighted
  mean delay is `La`, with the `(1−g²)` cancelling. Charging the loop `La/(1−g²)` instead
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
  pass. **Stationary modes ring; moving ones do not.**

## 3. The bench (`test/bench.cpp`, 90 checks, ALL CLEAR) and the panel probe (73)

Silence exact; determinism by memcmp; a burst decays and stays bounded in every room and
material; ITD −686 µs at 90° (Woodworth 666), mirrored at 270°, none front or back; front vs
back differ in spectral shape at 4 and 8 kHz; elevation moves the high band only; ILD none in
front, the right ear louder at 90°, larger at 0.3 m; 6 dB per doubling; air absorption costs
8 kHz 1.0 dB more than 250 Hz over 11 m (ISO: 0.8); a loudspeaker turned away loses 21.5 dB
at 4 kHz and 5.5 dB at 250 Hz; RT60 at 1 and 4 kHz against Eyring in all twelve room/material
cases; DRR at the critical distance within 3.5 dB of zero for every material, +6/−6 at half
and double; shutting the door drops 1 kHz by 20 dB and darkens it (4 kHz loses 8 dB more than
250 Hz); a door ajar sits between; sound through the doorway arrives from the door (ITD near
zero when the source's bearing would give 550 µs) and the bent path loses 6 dB more at 4 kHz
than the line of sight; the dead room's own field is 19 dB down by 40 ms and the hall's
returns at its own 5.4 s; the party wall leaks 24 dB below the same distance in one room;
opening both doors of the two-door route brings the sound up 18 dB; a source walking at
2 m/s on a pure tone keeps everything above 6 kHz 81 dB down (no clicks); a 4 m jump
crossfades; MIX 0 passes the input untouched; 44.1 and 96 kHz; a source with INPUT off is
bit-identical to none; MAIN L and R to two speakers land on the correct sides; AUX feeds a
source; PURE omni/cardioid and the cardioid's −4.8 dB radiated power; two sources in two
rooms; four sources bounded at 40 % of a core.

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
with it.

## 4. What the build taught (every line was measured, not reasoned)

- **A block read must advance within the block.** The first build wrote the whole sub-block
  into the source line and then read every sample at the same delay from the block's end,
  so nothing ever arrived. Found only by reading the delay slot's history: taps fine, gain
  fine, history all zero.
- **Two coupled feedback networks with long decays cannot be coupled both ways.** A PLASTER
  hall ran away at +40 dB/s with an energy-balance coefficient of 0.19: modal peaks sit
  tens of dB above the average gain and a coincident pair exceeds unity whatever the
  coefficient. The coupling is now a directed acyclic graph down the ranking of the rooms
  by source power. The double-slope decay a real flat has survives, because the neighbour's
  slow field is heard through the door-field paths as well as through the coupling.
- **Any interpolation inside a recirculating delay is a lowpass applied hundreds of times a
  second.** Linear interpolation of a 0.7-sample modulation cost 3–5× of the late field's
  energy and shortened every 4 kHz decay by a quarter. Integer delays in the network,
  Hermite for the one-shot path reads.
- **The steady-state gain formula G = fs·RT/(13.8·L) is right for a well-mixed network and
  wrong by up to 40 % for this one**: a plain Hadamard is an involution and the injection
  pattern sums to zero, so the state part-hides from the observer. Sign-flipped mixing, an
  observer with its own signs, and the efficiency MEASURED at prepare (impulse in, pair
  energy out, against the formula) — the Martian Gain rule applied to a reverb.
- **A first-order shelf realises only ~80 % of its gain one octave above its corner.** The
  four-anchor loss filter is fitted iteratively on the shelves' true digital response;
  before that, strongly frequency-dependent losses (ABSORBING) came out 40 % long.
- **Three "engine" faults were the bench's own windows**: a 4 ms window cannot resolve
  250 Hz; the room weight ramped from zero over 0.2 s at the start of every take and
  stretched short decays; an impulse fired 10 samples in landed inside a fresh path's
  2.7 ms gain ramp and read as a near-field loss.
- **A test source that walks straight through the listener's head is not a motion test.**
  And a max-sample-step metric on a moving pure tone measures the tone's own slope
  changing; a click is broadband, so the metric is energy above 6 kHz.
- **JUCE hands a choice parameter's raw value as its index.** The first live build read every
  room as TILED: caught by the RT numbers in the scene stream, not by any bench.
- **The Bash heredoc ate backslashes four more times** in this build alone. Patch scripts go
  in files via the Write tool. It is in CLAUDE.md six times.

## 5. Panel (`Source/ui/ui.html`, written on Opus against PROTOCOL.md)

POV WebGL2 view left (the camera is the listener; WASD walks, drag looks, doors block below
50 % aperture, and every door carries a **handle** — within 2.5 m, in front of you and not
through a wall, it lights up under the pointer, clicks to slam or throw open and drags to
swing), architect's plan right (drag sources and the head, turn them by their handles, click
a door to toggle, drag along it for an aperture, drag the diamond on a breakable wall to
splay it, rays of the live paths), seven control groups below with a hint on every one.
**The movement keys always win**: W A S D and the arrows walk and turn even when a menu or a
fader has the focus, and the control never sees the key — except in a text field, which
keeps its own. Zero draws when idle, the speaker glow at most
10 fps while the input is live. Probe: `test/uiprobe.js`; screenshot: `test/uishot.js`.
Live: `tools/live.ps1` + `tools/cdp.js` + `test/live-jobs.json` / `live-audio-jobs.json`
(`{k:"wavPath"}` loads a file without a dialog, the Brain Scan `importPath` precedent).

## 6. Not done / open

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
- ~~The build id is still `260921.1`~~ **Fixed 2026-09-22: the shipped build is 260922.1.**
  "Every document agrees" was the wrong property to be satisfied by — they agreed on a
  number that was a day and a great deal of work out of date, and the back page of the
  handbook printed it. A build id exists so a user can tell one build from another BY
  LOOKING. Bump it as part of the change, not as part of the release. The plates had the
  same disease one level down: the panel photographed on the landing page still showed
  260921.1 in its own corner beside a download button reading 260922.1.
- A single room network per room: two sources in one room share its field (correct), but
  the early-energy fraction each hands the field is taken at 1 kHz only.
- The DRR readout counts doorway/leaf/wall arrivals as "direct" (the shortest route), which
  is the useful reading but not the textbook one.
- The standalone's WAV player replaces the MAIN input while playing; AUX is untouched.
