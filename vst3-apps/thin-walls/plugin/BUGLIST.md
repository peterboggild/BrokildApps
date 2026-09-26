# Thin Walls — collected, awaiting go

House rule (CLAUDE.md): Peter's suggestions are specced here and built in batches when
he says go. Nothing on this list has been started.

---

## 1. SIX SOURCES instead of four — DROPPED (Peter, 2026-09-25)
Asked 2026-09-22. Assessment below is measured against build 260922.1, not estimated.

**Verdict: worth doing, about half a day. The path budget is the only part that needs
care, and it is the part that fails silently.**

### The easy half
- `MAX_SOURCES = 4` in `Source/Engine.h:48` → 6. Every loop in the engine, the processor
  and the param table already runs off it.
- `const NSRC = 4` in `Source/ui/ui.html:377` → 6. The panel is one constant plus three
  cosmetic things: the source tab strip has to fit six tabs in the width four use now,
  `HINT.srctab` says "Four sources", and `PROTOCOL.md` is headed "v3: four sources" with
  `selsrc` documented as 0..3.
- Cost, measured: **19 % of a core for one source, 39 % for four** → ~6.7 % per extra
  source → **six ≈ 52 %**. It fits, but the bench's own bound is `secs < 6.0`
  ("under 60 % of a core", `test/bench.cpp:726`) and that headroom is then gone. The
  bound wants re-deriving rather than re-typing, and the six-source figure must be
  measured in the worst case (all six in the listener's room), not the busy default.

### The three traps, all silent
1. **`MAX_PATHS = 96` (`Engine.h:291`) is the real constraint, and it truncates without
   a word.** A source in the listener's room generates up to **25 image paths**
   (1 direct + 6 first order + 18 second order — which is exactly the "25 paths active"
   the one-source bench prints). Four such sources want 100 paths, so **the cap already
   bites at four when they share the listener's room**; the recorded 72-path four-source
   figure is with sources spread across rooms, where the out-of-room ones only get
   portal and transmission paths. The guards are bare early returns
   (`if (nspecs >= MAX_PATHS - 8) return;`), so reflections simply stop appearing —
   no message, no meter, nothing in the panel. Six sources needs MAX_PATHS ≈ 160, and
   `std::array<PathSlot, MAX_PATHS> slots` grows with it (a delay line and a band filter
   each), so that is memory and per-block work, not just a number.
   **Whatever is decided, the truncation should be reported** — a path count against the
   cap in the scene stream, so a clipped scene is visible instead of merely quieter.
2. **Five arrays are initialised with four values written out longhand**
   (`Engine.h:476-480`): `lastSrcRoomAc`, `lastTypeAc`, `lastDirAc`, `lastActiveAc`,
   `lastLevelAc`, all `{ -1, -1, -1, -1 }`. C++ zero-fills the rest, so sources 5 and 6
   would start at 0 — which for `lastSrcRoomAc` and `lastTypeAc` reads as "already up to
   date with room 0, type PURE" and skips their first update. This is the Black Rider
   `std::array<int,5> uiNotes { -1,-1,-1 }` lesson verbatim: **every -1 must be written
   out**, or better, use a loop like `reset()` at `Engine.cpp:705` already does.
3. **The param table's seven default arrays are `[4]`** (`PluginProcessor.cpp:20-26`:
   `sx sy sz syaw stype sdir sin_`). They are indexed by `n` over `MAX_SOURCES`, so
   growing the constant without growing the arrays is an out-of-bounds read the compiler
   need not catch. Two more entries each, and the two new sources want sensible homes
   (the giant room is the obvious gap).

### What does NOT break
Adding sources **inserts 16 parameters in the middle** of the table, because the
per-source block comes first. Existing projects and automation survive anyway: the VST3
param id is a hash of the id STRING (`ParameterID { s.id, 1 }`, and there is no
`JUCE_FORCE_USE_LEGACY_PARAM_IDS` in CMakeLists), and APVTS state is keyed by id too.
Only the ORDER of the host's automation list changes, which is cosmetic. Worth proving
by reading the ids out of both DLLs the way `b/PhotoSynth/test/classid.js` proves a
class id, rather than asserting it.

---

## 2. SIDECHAINING SIX SOURCES — DROPPED (Peter, 2026-09-25)
Asked in the same breath as item 1.

**Sidechaining already works.** Each source's INPUT is
`OFF | MAIN L | MAIN R | MAIN L+R | AUX L | AUX R | AUX L+R`
(`PluginProcessor.cpp:37`) against a declared stereo "Aux In" bus
(`PluginProcessor.cpp:112`), which is the sidechain in every host that has one. So today
there are **four independent mono feeds**: main L, main R, aux L, aux R — exactly enough
for four sources, which is not a coincidence.

**Six independent sources need six channels, and the plugin side is nearly free:**
widen the aux to quad (main L/R + aux 1-4 = 6), or add a second stereo aux bus. Then
`isBusesLayoutSupported` (`PluginProcessor.cpp:126`) stops rejecting anything but
mono/stereo on the aux — one line — `processBlock` reads the extra pointers where it
already reads `aL`/`aR`, and the INPUT choice list grows. Half an hour.

**The problem is Ableton Live.** Live hands a plugin exactly ONE stereo sidechain and
exposes no multi-channel plugin inputs at all, so in Live the ceiling is four independent
feeds however the plugin is built — which is what it has now. Reaper, Bitwig and Nuendo
can feed a 4- or 6-channel aux, so six independent feeds would work there.

So the choice is Peter's, and it is about where he works:
- **(a) Six sources sharing four feeds** — works everywhere including Live, costs nothing
  beyond item 1. Two sources double up on a feed, which is musically fine (two speakers
  fed the same signal in different rooms is a real thing).
- **(b) A wider aux bus** — six genuinely independent sources in Reaper; in Live the two
  extra channels are simply unreachable and the panel should say so rather than offer a
  selector that silently reads silence. A greyed option with a reason, per the house rule
  about controls that look live and do nothing.
- **(c) A second instance** — six sources across two plugins, four feeds each, no code at
  all. Loses the shared apartment, which is most of the point.

Recommendation: **(a) now, (b) only if Reaper is in the picture** — and if (b) is built,
the unreachable-channel case must be visible on the panel.

---

## 3. THE DOORWAY BUMP — BUILT in 260924.1 (all three fixes; see the commit)
Reported 2026-09-24: walking continuously through an open doorway, both rooms sound
right but there is a small bump exactly at the threshold. **Real, and it is the
architecture.** Measured with `test/doorwalk.cpp` (target `twdoorwalk`): listener
walked through LARGE-GIANT at 1 m/s, source in LARGE, noise low-passed at 1.5 kHz,
10 ms windows, parts isolated with the DIRECT/EARLY/REVERB trims.

### What happens at x = 6.00
`roomOf (listener)` flips, and `buildPaths()` swaps one path model for another in a
single 128-sample sub-block (2.7 ms linear gain fade):
- **Before**: the in-room image model - direct + 24 image paths (orders 1-2) of the
  source's room.
- **After**: the portal model - direct-through-the-door, only the source room's
  FIRST-order images that pass the aperture, and first-order reflections in the new room.
- Measured: the EARLY part (about -29 dB, as loud as the direct at -32) drops to nothing
  in one window; the portal paths replacing it do not make up the energy, so the total
  falls ~1 dB in one step, the reflection pattern (the comb colouring) changes
  instantly, and there is a ~3 dB dip in the 10 ms window of the swap itself while the
  old and new direct slots cross.
- The late field is NOT the culprit: the room weights glide over 200 ms and the reverb
  level measured steady through the crossing (only its colour changes, which is right).
- No click (HF artefact energy at the door is no worse than anywhere else).
- Physically the two models should agree AT the door plane: a listener standing in the
  opening sees every source-room image whose line passes through the opening, i.e.
  nearly all of them. The portal model's order-1-only limit is what makes them disagree.

### Two fixes, recommended together
1. **A transition zone** (the standard game-audio answer, "rooms and portals" engines
   do the same). Within ~0.4 m either side of the door plane, and within the doorway's
   span, build BOTH path sets - listener-as-in-room-A and listener-as-in-room-B - and
   scale each set's gains by a smooth weight `w` (raised cosine across the zone). The
   two sets have different keys, so they are simply different slots. Crossfade on
   AMPLITUDE, not power: the two sets are mostly the same arrivals, i.e. correlated
   (the SWARM / TUBE lesson). Drive the room weights and the DoorField paths from the
   same `w` instead of a hard switch plus a 200 ms ramp. Cost: roughly double the path
   count for a source while the listener is in the zone - which collides with item 1's
   silent `MAX_PATHS` truncation, so do that cap/report first.
2. **Longer fades for any path that appears or disappears**: today a new or retired slot
   ramps over ONE sub-block (2.7 ms). A per-slot fade envelope over ~20-30 ms softens
   every visibility event in the model (a door edge passing the line of sight, a
   reflection point leaving a wall), not just the threshold. Cheap, global, and on its
   own it would already turn the bump into a short smear.

Optional, for accuracy rather than smoothness: extend portal (b) to second-order
source-room images, so the two models already nearly agree at the plane and the zone
has less to hide.

### How to know it is fixed
Extend `twdoorwalk` into a bench check: the 50 ms level of the total and of
direct+early must move no more across the threshold than across the same distance
elsewhere on the walk, and the early energy must not step by more than ~1 dB in any
10 ms window. Run it through all three doors, both directions.

---

## 4. CINEMATIC MODE — BUILT in 260925.1 (with the MP4 export)
Asked 2026-09-25: a switch that trades load for game-level picture quality.

### What the view is today (read from `Source/ui/ui.html`)
One WebGL2 forward pass: procedural materials (bump-mapped fbm plaster, planks,
tiles, carpet), Blinn-Phong, up to 12 point lights (5 in use: four pendants and
the hall window), fog, a corner-darkening term instead of real ambient
occlusion, **no shadows**, no HDR, no post-processing, device-pixel ratio capped
at 2, and **nothing drawn while idle** (the loop runs only while a key is held
or a door swings).

### Verdict: yes, and it costs GPU, not the audio
The panel runs in WebView2's own GPU process; the DSP runs on the host's audio
thread. A heavy picture cannot glitch the sound (it can warm a laptop), so the
"high load" is paid by the graphics card. This machine has an RTX 5070.

### Two layers, in this order
1. **Real-time "cinematic" (~a day).** HDR render target + ACES tone mapping
   + bloom (lamps, window, the glowing source); GGX/PBR in place of Phong;
   **shadow maps** from the four pendants (cube maps) and the window; SSAO in
   place of the corner-darkening term; a render scale of 1.5-2x on top of the
   device ratio with a proper downsample (or TAA); continuous rendering with
   subtle life (dust in the window light, lamp flicker). All of this is
   standard WebGL2 (float render targets, depth textures, multiple render targets).
2. **"Photo" mode when you stand still (~a day more).** The apartment is a few
   dozen boxes, so a **progressive path tracer** in a fragment shader (analytic
   box intersection, 2-3 bounces, accumulating frame over frame) converges to a
   near-photoreal still in a few seconds: soft shadows, colour bleeding off the
   walls, light spilling through the open doors. Moving drops back to layer 1.
   This goes past most games' look, because games cannot afford it and we can:
   the camera stands still most of the time.

### Things to hold on to
- The idle rule stays true when the mode is OFF (no draws when idle), and the
  mode is remembered per profile, not per project.
- Layer 2's accumulation must STOP once converged, or a still room runs the
  GPU flat out for nothing.
- Door swings and source moves already invalidate the mesh; for the path tracer
  they must also reset the accumulation.
- Measure: frame time at 1x/1.5x/2x, and GPU use when converged (should be 0).

---

## 5. FURNITURE — BUILT in 260925.1 (all four acoustic parts)
Asked 2026-09-25. A big job, but it has three independent halves, and the
acoustic half is what makes it worth doing in an instrument about hearing a room.

### The acoustics, in order of value
1. **Absorption (cheap, exact, the biggest audible effect).** Acousticians give
   furniture an *equivalent absorption area per object, per octave band* (a
   sofa ~1-2 m² at mid frequencies, a person ~0.5, a full bookcase
   more at high frequencies than low). That adds straight into `A` in Eyring, so
   the late field, RT60 and the direct-to-reverberant ratio all follow with no new
   machinery. A sofa and a rug in the TILED box should visibly and audibly
   shorten its decay.
2. **Scattering.** Furniture breaks up specular reflections: raise the room's
   effective scattering coefficient with the furnished surface area. The STUDIO
   material already proved the route (the scattered part goes to the late field
   for free).
3. **Occlusion.** A bookcase or a sofa back between source and listener: test
   each path's segments against the furniture boxes and, where one is blocked,
   bend it over the top edge with the existing Maekawa `addDiffraction`. The
   same test covers reflections that would pass through furniture.
4. **Reflections OFF furniture (optional, the most work).** A table top or a
   piano lid gives a strong early reflection. First-order images off finite
   rectangles fit the general surface search in `Geometry.h` (a mirror plus a
   point-in-rectangle check). Only the large flat faces are worth it.

### The picture
A catalogue built from boxes and rounded boxes in the existing mesh builder
(sofa, armchair, table, bed, bookcase, piano, rug, curtains), with the house
procedural materials. It reads well in the current style and even better
under item 4. Imported glTF models are possible but mean a loader and a few MB
of BinaryData each, so they're not recommended for a first version.

### The panel
Place, drag and rotate in the PLAN view, with the catalogue as a tray. The layout
is state, not host parameters (the BWFX-blob precedent: an opaque attribute on
the APVTS state, empty = today's apartment, so old projects are untouched).

### Estimate
Absorption + catalogue + plan placement + visual models: about 1.5 days.
Occlusion: +0.5 day. Furniture reflections: +1 day. Scattering: hours. Bench:
RT60 falls by Eyring's own prediction when a sofa is added; a bookcase
between source and listener costs the direct path the Maekawa figure; an empty
layout is memcmp-identical to today.

### Honest limits
Everything stays geometric-acoustic: no low-frequency modal effects (a
sofa in a corner damping a bass mode), and nothing that moves with a person
sitting down. MAX_PATHS is 192 now, so the budget has room for item 4.

---

## 6. LIGHT SYNC - BUILT in 260925.2
Asked 2026-09-25: "optional lamps and window light beat sync with sound,
adjustable with slider, default low", plus a CPU assessment.

### Design
- LIGHT SYNC slider (off .. strong, default low) and a WINDOW switch (default
  off - pulsing daylight reads wrong). Page preference, per profile.
- Two drives:
  - FOLLOW: each room's pendants follow the sound IN THAT ROOM - a native
    envelope/onset follower per room (source level x the room's field and its
    direct share), sent in the scene stream. Arrives 30-50 ms late (30 Hz stream
    plus a frame), so a hard transient can look a touch behind.
  - LOCK: pulses on the host's beats from the playhead (BPM + ppq), exact and
    latency-free, only while the transport runs (FOLLOW otherwise).
- Export: the per-frame light levels are computed from the take's OWN rendered
  audio at each frame's time, so a video is exactly in sync.
- PHOTO mode cannot converge under moving light: pause sync while photo refines,
  or refine only in silence.

### Cost (measured figures from 260925.1)
- Native follower: well under 0.1 % of a core (engine is 18-44 %).
- Messages: one small field in the 30 Hz scene (or a 60 Hz `light` event) - negligible.
- OFF view: goes from zero idle draws to continuous redraw WHILE SOUND PLAYS -
  ~0.3 ms GPU and ~0.2-0.6 ms main-thread per frame, i.e. ~2-4 % of one core in
  the WebView process. Cinematic already redraws continuously: nothing extra.
- None of it on the audio thread; sync off = exactly today, idle rule intact.

---

## 7. WALL PICTURES - BUILT in 260925.2
Asked 2026-09-25.

### Design
- PICTURE: load a JPEG/PNG (a file input in the page works in WebView2); the page
  downsizes to ~1024 px, sends it to native, which stores it. Place by clicking a
  wall in the plan or the POV; slide along the wall, set the height.
- Size: S / M / L / XL presets or a width in cm, aspect kept. Frame: thin black,
  oak, gilt, unframed canvas.
- Drawn: a strip on the wall in the plan; textured quad + frame in 3D (a new
  material kind sampling a per-picture texture), lit, shadowed in cinematic, in
  the photo path tracer's box model (average albedo), and in exported videos.
- Stored with the project and presets as embedded JPEG base64 (~100-200 KB each,
  cap ~8), so a project opens on another machine intact.

### Acoustics, honestly
- PRINT: visual only - a framed print does essentially nothing audible, and the
  panel should say so.
- ACOUSTIC PANEL: the artwork printed on a 5 cm fabric absorber (a real product
  category), alpha ~0.25/0.60/0.95/1.0/1.0/1.0/1.0 over 125 .. 8k Hz times its
  area, added into Eyring's A less the wall it covers - same route as the rug.

### Cost
CPU/audio: none measurable (panels are a constant in A). GPU: one texture per
picture. Work: half a day to a day with probe checks and a live check.

---

## 8. RENDER QUALITY - an offline "ultra realism" render for exports and bounces - BUILT in 260925.4 (all four tiers)

**Built, and what it measured** (`test/render.cpp`, twrender, 33 checks ALL CLEAR):
- Tier 3, HIGH/ULTRA image order 4/6: 24 -> 128 -> 376 reflections, energy at 1 kHz within 0.2 dB (the late field hands over). Order 6 needed `pts[10]` and `b[8]`: the old arrays overflowed (0xC0000409).
- Tier 4, a personal head from SOFA (libmysofa + zlib compiled from pinned sources, `cmake/sofa.cmake`): KEMAR from its own SOFA file reproduces the built-in set, ITD -729 vs -708 us, ILD at 4 kHz 9.9 vs 9.5 dB. Live and in every render; kept with the project.
- Tier 2, traced tails (`Source/LateRays.*`): 16 000 rays through the real geometry, excluding exactly the specular chains the engine renders; transmission as a second stage from each closed leaf and party wall. Against the statistical field: +0.4 to +0.9 dB in four materials, T20 within Eyring (tiled 13 % long), through a shut wall +0.6 dB. **Bug on the way: every level 15 dB high** - a ray's band energy is the whole broadband impulse with that band's losses, so the noise must NOT be divided by the band's share of the spectrum (1/0.034 = 15 dB at 1 kHz).
- Tier 1, the wave below 220 Hz (`Source/Fdtd.*`): SLF leapfrog at the Courant limit, 10 cm cells so every wall lands on a cell face, face-centred impedance boundaries, the actual source signals injected, doors and furniture rebuilt when they move. Free field +0.23 dB against 1/r, axial modes 28.6 / 34.3 Hz exactly, 1 kHz untouched (0.02 dB). **Two bugs only a LIVE bounce found**: a pressure source that never takes back the air it pushes fills a closed room (DC 0.78 of full scale in 4 s) - fixed with a 10 Hz 2nd-order high pass (zero area and zero first moment); and the lowest traced band spread flat to 0 Hz rang at DC - it now stops at 25 Hz.

(The original spec follows.)

Asked 2026-09-25. The take export already re-renders the sound offline through a
fresh engine; this adds a quality choice to it (and a plain "bounce the take to
WAV"). Real-time playing is untouched.

What the real-time engine approximates, and what offline can do instead, ranked
by audible gain:
1. LOW-END WAVE SIMULATION (biggest gain, most work, ~a week). Geometric
   acoustics has no room modes: no corner boom in the box room, no exact
   low-frequency diffraction at doorways. FDTD on the GPU below ~400 Hz over the
   real geometry (furniture included), recording pressure + velocity at the
   listener's recorded path points (one run per source serves every point),
   rendered binaurally and crossed over to the geometric part. Hall at 500 Hz:
   ~1.6 M cells x ~17 k steps for a 2 s response - roughly 10-30 s per source on
   the RTX 5070.
2. ROOM-SPECIFIC TAILS (days). Stochastic ray tracing per room with the real
   geometry, scattering and door coupling, band energy-decay curves turned into
   the late response (the ODEON/RAVEN hybrid), replacing the statistical FDN. IRs
   along the path every ~0.1 m: about a minute for a one-minute walk.
3. LIFT THE REAL-TIME CAPS (about a day). Image sources to order 4-6 with full
   visibility, no MAX_PATHS, a denser late network, finer HRTF interpolation.
   Renders a few times slower than real time.
4. PERSONAL HRTF (cheap). Import a SOFA file in place of the MIT KEMAR set.

Recommended order: 3 + 4 together, then 2, then 1.

## 9. SMOOTH MOTION IN THE VIDEO EXPORT - BUILT in 260925.6
Peter: "Video export is a bit janky, skipping many frames - is there a better way of doing it? Especially at high resolution."

**The export is not dropping frames.** Every frame is rendered, encoded and acknowledged before the next (`runExport`). What judders is the TAKE: the panel sends the listener's position once per drawn frame (`stepWalk` -> `queueP`, flushed per rAF), the audio thread records what arrives, and `vidPlan` samples that record per video frame with no interpolation. Whenever the panel draws slower than the export's frame rate, positions repeat and then jump.

Measured live (`test/live-walk-judder-jobs.json`, 260925.5, standalone at its default window): panel 56 fps in the ordinary view, CINEMATIC HIGH and ULTRA alike. Against a 60 fps export the recorded position changes on only 83-86 % of frames, so one frame in six or seven is a repeat. A bigger window, a higher render scale or a slower GPU lowers the panel rate, and the repeats become longer holds; that is the "especially at high resolution". `stepWalk` also clamps dt at 0.1 s, so below 10 fps the walk also runs slower than real time.

Proposed fix (native, in the `vidPlan` builder; the audio is untouched):
- For the motion parameters (lisx, lisy, lisyaw, s1..s4 x/y/z/yaw), replace each held run by linear interpolation between the neighbouring CHANGE points, taking time from the block index. Yaw goes the short way round the circle.
- Only bridge gaps shorter than ~0.25 s, so a deliberate stop stays a stop.
- Optionally, a light 2-3 frame smoothing of the camera path for a steadier "dolly" look.
- A check: record a walk while the panel is artificially throttled to 10 fps. The exported position must change on every frame of the walk, and must match the unthrottled walk within a few cm.

Cheaper stopgaps that need no build: export at 30 fps (the panel almost always manages that), and record with CINEMATIC off or the window smaller, then turn CINEMATIC on for the export itself. The export draws offline, so its own resolution and quality cost time, not smoothness.

**Built in 260925.6** (`Source/MotionTrack.h`, used by the vidPlan builder in PluginProcessor.cpp): positions, facings and doors are read from a track of change points instead of sampled - blended between points closer than 0.3 s, held across a longer gap (a real stop), eased in when a walk starts after a stop, and left as a snap for an isolated change. Facings blend the short way round. Audio untouched.

Measured: bench (a 12 fps panel, 60 fps export) - without it 134 of 167 frames of the walk repeat, with it the position moves on 167 of 167 and stays within 3 cm of the true walk; live standalone, walk recorded and the export plan captured - 100 % of frames move at 56 fps (ordinary, cinematic HIGH, ULTRA; was 83-86 %) and at 12.9 fps with the panel deliberately throttled.

Still true: below 10 fps the walk itself is recorded SLOWER than real time (`stepWalk` clamps dt at 0.1 s). The export is now smooth whatever the rate; the pace is what was recorded.

Note left from item 1: MAX_PATHS = 96 still truncates silently if four sources sit in the listener's room at once (4 x 25 image paths). Not reported anywhere yet.

---

## 10. THE DOORWAY GLITCH, RE-REPORTED at 260925.7 — AWAITING GO

**Peter, 2026-09-26:** *"there is a glitch when i continuously move through an open
doorway. Can this be avoided?"*

Item 3 is the same complaint, and it was BUILT in 260924.1. So the first job was to
find out whether it had come back. It has not.

### What was ruled out, with measurements

**Not a stale install.** Both houses carry **260925.7** in their own bytes
(`Brokild collection\Thin Walls.vst3`, installed 2026-09-26 09:16) — so he is running
the build that contains all three item-3 fixes. Worth stating because this plugin has
the recorded history of `b\BrokildWorldFX\tools\install-fleet.ps1` being the stale copy
and installing an older build while still reporting "loads".

**Not a regression of the item-3 bump.** `twdoorwalk` — the probe written for that
report — now measures the threshold as QUIETER than the rest of the walk on every
part. Worst 10 ms level step, near the door vs elsewhere on the same walk:

| part | at the door | elsewhere |
|---|---|---|
| all | 4.43 dB | 6.11 dB |
| direct | 3.27 | 5.75 |
| early | **3.47** | 8.29 |
| reverb | 3.59 | 7.28 |

The EARLY part was the whole of the original fault (it dropped to nothing in one
window); it is now the best-behaved of the three. HF artefact energy at the door is
−61 to −69 dB, in every case BELOW the worst elsewhere. The transition zone is doing
its job.

Also worth recording, because the buglist's own recommendation was not what shipped:
item 3 recommended building BOTH path sets in the zone and crossfading them, and
warned that this roughly doubles the path count. What was actually built is better —
`addPortalPaths` was extended to second order and made to emit **the same keys** as
the in-room model (`imageKey(nx,ny,nz)`), so the two models agree at the plane and
every slot simply carries across instead of one set fading out while another fades in.
The sets are either/or, never both, so the zone does not double the path count.

### So it is something the probe does not cover

`doorwalk.cpp` walks the listener **straight through, perpendicular, at a steady
1 m/s, with ONE source**. Two candidates, both found by reading rather than measured —
neither is confirmed:

**(a) The listener TELEPORTS instead of gliding, near a jamb or when turning.**
`Engine.cpp:2339`:

    if (roomOf (lT.x, lT.y) != roomOf (lisPos.x, lisPos.y) && ! throughOpenDoor (lisPos, lT))
        lisPos = lT;                                   // snap
    else lisPos = lisPos + (lT - lisPos) * k;          // glide

The glide is a 25 ms time constant, so at walking pace the smoothed position lags the
target by roughly 5–10 cm. `throughOpenDoor` asks whether the segment from the
**smoothed** position to the **target** crosses the door plane inside the open strip.
Cross near the frame, or turn while crossing, and that short segment can cross through
the WALL while the player is genuinely walking through the opening — and the listener
jumps up to 10 cm in one sub-block. That moves every path delay at once: a click, not
a bump, and intermittent and position-dependent, which is what "a glitch" rather than
"a bump" sounds like. The snap is correct for a teleport and wrong for a walk; it
needs to ask whether the player is in the doorway, not whether one short segment
threads it.

**(b) Silent path truncation with several sources.** A source in the listener's room
generates up to 25 image paths, so four of them fill 100 of `MAX_PATHS` (192) before
portal, transmission and furniture paths. `pathsFull()` refuses the rest and counts
them in `Scene::pathsDropped` — and **nothing on the panel ever shows that number**
(checked: `ui.html` has no reader for it). Reflections would simply stop appearing,
and which ones are refused changes as the listener moves. Item 1 flagged this and it
was never surfaced.

### What would settle it

Three questions to him, because the two candidates behave differently: does it happen
on EVERY crossing or only some; is he walking straight through the middle or at an
angle / close to the frame / turning as he goes; and how many sources are active.

Then extend `doorwalk.cpp`, which already has the machinery: a diagonal crossing and a
brush past the jamb for (a), and a four-source walk printing `pathsDropped` for (b).
A teleport is a large single-window HF spike, so the probe's existing artefact metric
will catch (a) the moment the path provokes it.
