# JOURNEY TO THE MOON: an Apollo-style flight to Taurus-Littrow in VR

Design and handover for `vr-apps/moon/`. Written 24 November 2026, before any code. The owner's decisions are in §1
and are binding; everything else is a proposal that the stages in §12 may refine. Where a later stage changes the
design, update this file in the same pull request.

---

## 0. What it is, and what it is not

A seated-then-standing VR experience for the Meta Quest 3 browser: you board an Apollo Command Module on the pad,
fly a Saturn V to orbit, leave Earth, coast to the Moon, crawl through the tunnel into the Lunar Module, descend
to the Taurus-Littrow valley and step out onto the surface.

It is **not a game**. No score, no lives, no arcade controls, no glowing toy interface, no hyperspace. The only
liberty with time is a time-acceleration control and a chapter menu. It is the adult, space-enthusiast sibling of
*Rejsen til guldplaneten* (`vr-apps/guldplaneten/`), and it must look and feel a clear step above it.

### 0.1 Owner's direction (24 November 2026, binding)

> Move away from game/toy look and controls, to realism and immersion. The view through the windows, the view of
> the Moon and walking on the Moon should be a step up from the child's game.

Consequences that apply everywhere:

- **True scale.** Real radii, real distances, real hardware dimensions. Guldplaneten scaled planets down so the
  horizon would sit in its windscreen; we do not.
- **Real proportions and materials.** The spacecraft are modelled from published drawings and photographs, not
  invented.
- **Diegetic interface.** What you read or operate is something that existed: the panel, the DSKY, the mission
  clock, the cuff checklist on your wrist (§8.2). No floating menus in the experience itself. The start page (a
  normal web page before entering VR) is the only out-of-world UI.
- **Light as it really is.** A low Sun, no air, hard shadows, regolith that brightens around your own shadow.
- **Sound as it really is.** Inside the cabin: radio, fans, pumps, the rocket through the structure. On the surface:
  your suit and the radio, nothing from outside.

---

## 1. Decisions (owner, 24 November 2026)

| Topic | Decision |
|---|---|
| Title and folder | **Journey to the Moon**, `vr-apps/moon/` in BrokildApps, a "Test" card on the Brokild VR page while in development |
| Mission | Apollo 17-style J mission to the **Taurus-Littrow valley** (later, larger LM; Lunar Roving Vehicle) |
| Launch | **By day.** Not historical (Apollo 17 was the only night launch); chosen for the view. See §2.2 for how the timeline absorbs it |
| Launch windows | **Historical**: the boost protective cover blinds the CM windows except one small window; the windows clear when the escape tower is jettisoned |
| Landing | **Autopilot with optional takeover** for the final phase (P66), as every Apollo commander did. Doing nothing lands safely |
| NASA material | May be downloaded and used: public-domain data, imagery, reference models, mission photos. Sources listed in the README. **No NASA insignia** (meatball, worm) |
| Language | **All English**: radio, panels, start page, README for players. Code comments English; this file English |
| Moving on the surface | Room-scale walking **plus stick locomotion; teleport as an option**. Comfort: vignette, snap turn 30°/15° or smooth, possibly one comfort slider that sets them together |
| Stars | A setting; **faint stars by default**; "realistic (none in daylight)" and "full" available |
| Pacing | **Time acceleration** (×1 to ×10 000) with a mission clock, plus **chapter jumps** from the start page and the cuff checklist |
| Ending | Version 1 ends with **free time on the surface**. LM liftoff, rendezvous and the trip home are a later stage |
| Rover | **Parked** beside the LM as scenery; driving it is a later stage |

### 1.1 Proposed, not yet confirmed

- **Crew names.** Roles only (Commander, Command Module Pilot, Lunar Module Pilot, CAPCOM, Launch Control), no real
  astronauts' names attached to lines they never said. The player is the Commander. If the owner later supplies
  actual NASA mission audio (public domain), real names come with it naturally.
- **Mission name.** The radio says "Apollo" with a fictional flight number or none; the hardware is J-mission
  hardware. No mission patch, or an original one.

---

## 2. The journey

### 2.1 Chapters

GET = ground elapsed time (mission clock). Times are the historical Apollo 17 values where known, marked ≈ where
not yet checked; stage 1 pins them against the *Apollo 17 Mission Report* and the *Apollo Flight Journal*.

| # | Chapter | GET (≈) | Real time in VR | You are | What happens |
|---|---|---|---|---|---|
| 1 | The pad | −00:05:00 → 0 | ≥ 3.5 min | in the White Room, then lying in the left couch | Walk the last metres of the access arm into the White Room, climb through the hatch, lie down. Radio checks, Launch Control's count, the hatch closed, the arm swings away (felt and heard), T−8.9 s ignition sequence, holddown release |
| 2 | Ascent | 0 → 00:11:50 | ~12 min, ×2/×4 allowed after the tower jettison | strapped in | Liftoff, tower clear, roll and pitch program, max Q, S-IC cutoff (the jolt), S-II ignition, **escape tower jettison: the windows clear**, the sky goes black, S-IVB to orbit. Thin blue limb |
| 3 | Earth orbit | 00:12 → ≈03:12 | free, time warp | weightless in the CM | One and a half orbits; sunrise and sunset on the limb; the night side; TLI checklist |
| 4 | Translunar injection | ≈03:12 → 03:18 | ~6 min (warp allowed) | strapped in | S-IVB relight, Earth visibly shrinking during the burn |
| 5 | Transposition and docking | ≈03:40 → 04:30 | ~5 min condensed | at the window | The CSM separates, turns round, the SLA panels fall away, the LM is revealed on the S-IVB, docking, extraction |
| 6 | The Blue Marble | ≈05:00 | free | at the window | The whole Earth fits in the window for the first time (see §2.2 for which face) |
| 7 | Coast | → ≈86:00 | time warp | weightless, free to float in the CM | Earth recedes, the Moon grows. Midcourse corrections as short events. Passive thermal control roll ("barbecue") makes Sun, Earth and Moon wheel past the windows |
| 8 | Lunar orbit | ≈86:14 → ≈110:00 | time warp | at the window | Behind the Moon (radio silence), LOI burn, Earthrise, the descent orbit; the Moon at full quality from 110 km and from the 15 km low point |
| 9 | Into the LM | | ~3 min | crawling | Hand over hand through the docking tunnel; the LM cabin; standing at the left station with restraints; undocking, the CSM drifting away |
| 10 | Descent | PDI → landing ≈ 12 min | real time, ×2 offered before high gate | standing, flying or watching | Braking phase windows up (Earth and black sky), pitch-over at high gate: **the valley swings into view** between the massifs. Approach phase with the landing point designator, P66 takeover offered, dust, contact light, engine stop |
| 11 | On the surface | after landing | free | EVA suit | Post-landing condensed, hatch, porch, ladder, the last drop to the footpad, first step. Free walk around the LM, the rover, the flag, out to the edge of the play area |

Chapters can be started from the start page and from the cuff checklist. Starting mid-mission sets the vehicle
state by script (no physics history needed).

### 2.2 Day launch, the timeline, and the Earth's face

- The landing keeps the historical Sun: **elevation 13.3°, azimuth 95.5°** at the site (Sun low in the east, behind
  the approaching LM; its shadow on the ground is the altitude cue the crews relied on).
- So the landing time stays historical and **the launch moves to daylight** with a correspondingly longer coast.
  This has precedent: Apollo 17's own 2 h 40 min launch delay was absorbed by a modified trajectory that still
  arrived at the Moon on schedule. Proposed: launch ≈ 12 h earlier (early afternoon local time, 6 December 1972).
- Consequence, stated honestly: the famous *Blue Marble* view (Africa, Arabia, Antarctica) was a product of the
  real launch time. With a day launch the Earth turns a different face to the window at that moment. We show the
  physically correct face for our timeline, not the photograph.

### 2.3 The pad, the cover and what you see (chapter 1–2)

- Start in the White Room at the end of the crew access arm, about 98 m above the pad. The arm is enclosed, so the
  outside view is through its windows and the open end: the upper Saturn V, the coast, the sea. A narrow view keeps
  the ground-rendering problem small.
- In the CM you lie on your back in the left couch. In VR the player sits upright in a real chair; the cabin is
  oriented so the main display console is in front of and above the player, as it was in front of the crew's
  faces. The couch is drawn reclined around the player's body, legs up.
- **The boost protective cover** covers the CM and its windows during ascent. It had one window of about 20 cm.
  Sources differ on whether it sat over the hatch window or the Commander's forward window; stage 1 decides from the
  Apollo Operations Handbook drawings. Through it: sky, the tower moving down past it at liftoff, steam and smoke,
  then a blue sky darkening.
- **Tower jettison** (≈ T+3 min): a bang, the cover pulled away, and all five windows open onto a sky that is already
  deep blue to black, with the horizon below. This is the reveal the owner chose.
- After S-IVB insertion the view is Earth from about 170 km: the curvature, the thin atmosphere, clouds, the
  terminator (§5.2).

### 2.4 Shaking and noise without making people ill

- **The view is never shaken.** The head stays where the player's head is. Shaking the camera against a still
  inner ear is the fastest way to nausea. What shakes is the cabin around you, at small amplitude (millimetres)
  and high frequency (≥ 8 Hz), plus the couch and loose cables.
- **Controller haptics** carry most of the feeling: both controllers rumble with the rocket, with the S-IC cutoff
  jolt as a sharp pulse.
- **Sound** carries the rest: low-frequency rumble through the structure (synthesised), the crackle of the F-1s
  through the hull, the hiss of the cabin fans, the pumps. The cutoff silence after S-IC is a beat of its own.
- Staging accelerations are shown by things in the cabin (checklist strings, the cuff checklist swinging), never
  by moving the player.
- Comfort setting reduces all of it.

---

## 3. Spaces and scale

| Space | Player posture | Size | Notes |
|---|---|---|---|
| Access arm and White Room | standing, walking | a few m | Enclosed; real handrails |
| Command Module | seated (couch), later floating | ~6 m³ habitable | Three couches; the player in the left one. In coast the couch can be "left": floating with hand-over-hand movement on handholds |
| Docking tunnel | crawling, hand over hand | ~0.8 m diameter, ~1 m long | Grab-and-pull locomotion only, no stick (natural and comfortable) |
| LM cabin | standing at the left station | ~4.5 m³ | The real crew stood. Restraint cables at the waist. Triangular forward windows looking down-forward |
| LM exterior and surface | standing, walking | the play area plus stick | High detail to ~300 m from the LM, decreasing to the horizon and the massifs |

- **One rig, metres, Y up locally.** The world moves around the rig for orbital scales (floating origin): the
  spacecraft is always at the origin, Earth and Moon are placed relative to it in double precision on the CPU and
  sent to the GPU as small numbers.
- **Seated vs standing.** Chapters 1–8 assume seated, 9–11 standing. A "recenter" action on the cuff checklist and
  on the controller's menu button; the start page asks whether the player is seated or standing for the standing
  chapters (room-scale floor vs seated height offset).

---

## 4. Rendering architecture

- **three.js r170, vendored** (as the other VR apps), plain ES modules, no build step for the app. Plus
  `KTX2Loader` with the Basis transcoder, `GLTFLoader` with `meshopt` decoding.
- **Offline asset pipeline** in `vr-apps/moon/tools/` (Node scripts, Blender Python scripts, a PowerShell TTS
  script). Raw source data (gigabytes) is downloaded to a cache **outside Dropbox and outside the repository**
  (`%LOCALAPPDATA%\moon-data`); only the processed, compressed results go into `vr-apps/moon/assets/`.
- **Compressed textures everywhere** (KTX2: UASTC for normal maps and the Moon's colour, ETC1S for big albedo).
- **Per-chapter loading.** The launch chapters never load the Moon tiles; the surface chapter never loads the
  Saturn V.
- **Quest 3 settings:** 72 Hz default (90 Hz option), fixed foveation on, framebuffer scale 1.0–1.2 (the panels need
  sharpness), a frame-rate guard that lowers the scale before anything else. Stage 0 checks whether r170's
  multiview path works in the Quest browser; if it does, it roughly halves CPU draw cost.
- **No per-pixel procedural terrain functions** in VR. Planet Designer crashed a real Quest 3 doing that. Everything
  heavy is baked into textures, offline or progressively at load.

### 4.1 Performance budget (Quest 3, both eyes, 72 Hz)

| Item | Budget |
|---|---|
| Draw calls | ≤ 150 (instancing for switches, rocks, rivets) |
| Triangles in view | ≤ 600 k |
| Texture memory | ≤ 700 MB resident (compressed) |
| Download per chapter | ≤ 60 MB; total ≤ 200 MB in the repo |
| Fragment cost | One texture-lookup-heavy shader on the big surfaces; no loops over noise octaves |

---

## 5. Earth

### 5.1 Sources

- **NASA Blue Marble Next Generation** (public domain): monthly true-colour mosaic (December), 8k for orbit views
  with higher-resolution tiles for the Florida region seen during ascent.
- **NASA Black Marble** (night lights) for the night side, dimmed to 1972 levels (far fewer lights than today:
  a global dimming factor, and the brightest modern clusters reduced more).
- **Clouds:** a public-domain global cloud composite, as a separate layer with its own shadow on the ground.
- **The Cape and the ascent ground track:** Landsat (public domain) for the region seen from 10–100 km; the White
  Room view uses a handcrafted near field (pad, the other tower, the coast) over aerial imagery.

### 5.2 Atmosphere

- From the pad to orbit: the sky colour by single scattering along the view ray (Rayleigh per channel, Mie haze),
  as a function of altitude and Sun angle. **Reuse Planet Designer's scattering model** (`scene/planet.js`, the
  "Sunlight through the air" and "The atmosphere" shaders) at true Earth scale, baked into small look-up tables per
  chapter instead of evaluated per pixel per frame.
- From orbit: the limb as a thin bright blue band with the black sky above, the terminator band reddened. Guldplaneten's
  *limb proxy* technique (a sphere at fixed distance with the true angular size, `LIMBS`/`setLimb` in
  `guldplaneten/index.html`) carries over, but with true radii.
- From cislunar space: Earth as a small disc with its atmosphere rim, clouds, specular glint on the oceans,
  night lights on the dark side; the sizes follow the real distance.

---

## 6. The Moon

The hardest part, and the one the owner asked for extra care with.

### 6.1 How much detail the eye can see

Quest 3 resolves about 25 pixels per degree. From the 110 km lunar orbit, looking straight down, one pixel covers
about 75 m; at the 15 km low point of the descent orbit, about 10 m; on the surface, centimetres. So the Moon needs
**different data at every scale**, cross-faded so no seam is visible.

### 6.2 Data sources (all NASA / USGS, public domain)

| Scale | Data | Resolution | Use |
|---|---|---|---|
| Global | **CGI Moon Kit** colour (LROC WAC based, `lroc_color_16bit_srgb`, up to 16k) and LOLA displacement (4, 16, 64 px/degree) | ~470 m (64 px/°) height; ~650 m colour at 16k | The whole Moon from cislunar space and from orbit |
| Approach corridor | LROC WAC GLD100 elevation and the WAC global morphologic mosaic, cut to the last ~1200 km of ground track before landing | ~100 m | Lunar orbit and the braking phase |
| The valley | **LROC NAC DTM of Taurus-Littrow**: 5 m/px over 47 × 50 km | 5 m | The massifs, the valley floor, the approach |
| The traverse area | LROC NAC DTM 2 m/px mosaic, NAC orthophotos (0.5–0.6 m/px) | 2 m height, 0.5 m albedo | The landing site and the walkable area |
| Below 0.5 m | Generated: small craters with the real size–frequency distribution, rocks and boulders with the real size distribution, regolith micro-relief | cm | Everything within ~50 m of the player |

The NAC orthophotos contain the shadows of the Sun they were taken under. The albedo is recovered by dividing out
the shading computed from the DTM for that image's Sun (photometric normalisation), or by picking images taken
near local noon. Stage 0 decides which.

The real site today shows the Apollo 17 descent stage, the rover and the tracks at sub-metre scale in the NAC
images; at the scale we use they are below a pixel or are removed by hand.

### 6.3 Geometry

- **Global:** a cube-sphere with a quadtree of chunks (each chunk one draw call), heights from the displacement
  map, skirts to hide cracks. Chunks split by screen-space error, budgeted for two eyes.
- **Corridor and valley:** the same quadtree, fed from the higher-resolution tiles where they exist. The data is
  pre-cut into tiles by `tools/` (KTX2 colour, 16-bit height as PNG or KTX2 R16).
- **Walkable area:** a dedicated mesh around the LM (~600 × 600 m) at 0.5 m vertex spacing near the LM, decreasing
  outwards, with the generated small craters stamped into the heights offline. Rocks as instanced meshes (a library
  of ~12 scanned-looking rock shapes made in Blender, with LODs), placed by the size distribution and by the DTM
  (more on crater rims and slopes, the North Massif boulder tracks where the data shows them).
- **Collision and walking** use the same heights on the CPU (one function, as Guldplaneten's "the map and the
  ground are one function").

### 6.4 Light and shading

What makes the Moon read as the Moon rather than grey plaster:

1. **No limb darkening.** The full Moon is as bright at its edge as at its centre. Lambert shading is wrong. Use the
   **Lommel–Seeliger** law (with a small Lambert share) for the reflectance.
2. **The opposition surge.** The regolith brightens sharply towards the direction exactly opposite the Sun (around
   your own shadow on the surface, around the LM's shadow seen from above). A Hapke-style opposition term, a few
   lines of shader.
3. **Hard, long shadows** at 13° Sun, black where nothing scatters, but **not pure black near sunlit slopes**: the
   regolith lights the shadows with a faint, warm-grey bounce (≈ 10 % albedo). One bounce term from the sunlit
   ground, baked.
4. **Cast shadows without per-frame shadow mapping of the terrain.** The Sun hardly moves during the mission
   (≈ 0.5° per hour). So:
   - For the terrain, bake a **horizon map**: for each texel, the elevation angle of the terrain horizon in the
     direction of the Sun's azimuth. The shader compares it with the Sun's current elevation: correct cast shadows
     and soft penumbrae for any time in the mission, from one texture channel. Computed offline from the DTMs and
     the global LOLA heights (the massifs' shadows on the valley floor).
   - For the LM, the rover, rocks and the player: a conventional shadow map, but only near the player (~50 m),
     where it is needed.
5. **Exposure.** The sunlit surface sets the exposure; looking into the LM's shadow or out at the black sky slowly
   opens it up (eye adaptation over a few seconds). With the stars setting on "realistic", stars appear only when
   the exposure has opened far enough.
6. **Earthshine** lights the night side seen from lunar orbit, faintly blue-grey.

### 6.5 The surface close up

- **Regolith shader:** a tiling micro-relief normal map (fine granular soil, the "fairy-castle" texture), detail
  albedo variation, and the opposition surge. No noise loops: a few texture reads.
- **Footprints:** the player's boots press prints into the soil as they walk. Prints are a decal texture with the
  Apollo overshoe tread pattern (the characteristic ribbed sole), written into a local displacement/normal buffer
  around the LM and kept for the visit. The descent engine's scoured area around the LM is smoother and brighter.
- **The rover's tracks** from where it was driven off (it is parked: a short track from the LM).
- **Dust** kicked by the player: low arcs that fall in vacuum ballistic trajectories (no billowing), as a small
  particle system.

---

## 7. The spacecraft

### 7.1 Lunar Module (J-mission, extended descent stage)

The hero object: seen from inside during descent and walked around on the surface.

- **Built in Blender by Python script** (`tools/blender/lm.py`) from published dimensions and drawings
  (descent stage ≈ 4.2 m across the octagon, ≈ 9.4 m across the footpads, ≈ 7.0 m high), cross-checked against
  NASA 3D Resources models and photographs, exported to glTF with KTX2 textures and meshopt.
- **Materials are the whole look:**
  - Descent stage: the amber-gold and the black aluminised Kapton blankets, **crinkled**, with seams, tape and
    tie-downs. A generated crinkle normal map with large-scale variation so it never looks tiled; strong anisotropic
    specular, the gold tint in the reflections.
  - Landing gear: struts wrapped in gold and black foil, the footpads with their foil, the probes on three legs.
  - Ascent stage: the faceted cabin in grey-black anodised panels and black thermal blankets, the RCS quads with
    their plume deflectors, the rendezvous radar dish, the antennas, the docking target, the forward hatch and porch,
    the ladder on the forward leg.
  - Decals: "UNITED STATES" and the flag, as on the real LMs. No NASA insignia.
- **Baked ambient occlusion** and cavity in the textures; real-time sunlight and one shadow map.
- **Inside:** the two stations, the panels (same approach as §7.2), the triangular windows with their reticle (the
  landing point designator scale), the restraint cables, the hatch.
- Simplified where the eye cannot tell: the interior of the descent stage bays, wiring behind panels.

### 7.2 Command and Service Module

- **Exterior** (seen at docking from inside and at undocking from the LM): the conical CM with its reflective
  Mylar-tape finish, the SM with radiator panels and the high-gain antenna, the SPS engine bell. Same pipeline.
- **Interior:** the main display console is several hundred switches, gauges and the DSKY. A panel that is a flat
  photo looks fake in VR because it has no depth; a fully modelled panel is too heavy. Proposed: the panel layout
  redrawn as a texture from the Apollo Operations Handbook panel drawings (labels sharp, generated by script), with
  every toggle switch, guard and rotary knob as **instanced 3D geometry** on top (a few hundred instances, one draw
  call per type), gauges with live needles, and a working DSKY showing real programs, verbs and nouns
  (P11 at liftoff, P15 for TLI, P63–P66 in the LM).
- Only switches with a role in the story operate. The rest are real objects that do nothing when touched (they click).

### 7.3 Saturn V and the pad

- Seen from the White Room (upper stages and the arm), through the cover window during ascent (the tower, smoke) and
  at staging (the interstage falling away, seen later through the windows when the S-IVB is separated). A medium-
  detail model.
- **Smoke and steam at liftoff:** lit billowing clouds as soft particles with a flipbook of pre-rendered volumetric
  puffs (rendered offline in Blender, with normal maps so they light correctly). True volumetric smoke is not
  affordable on a Quest and would not convince at the frame rate we could afford.
- **The F-1 plume** seen from above at staging: bright core and dark fuel-rich exhaust sheath; the S-II and S-IVB
  plumes are nearly invisible in vacuum (hydrogen), as they were.

### 7.4 The rover, the flag

- The LRV parked beside the LM, modelled from drawings, with its wire-mesh wheels, the fenders (one famously
  repaired with maps and tape on Apollo 17), the high-gain antenna, the TV camera. Same pipeline as the LM.
- The flag on its telescoping crossbar, unmoving.

---

## 8. Interaction and comfort

### 8.1 Hands

- Gloved hands around the controllers (Portalkniven's `hands.js` approach: the WebXR generic hand model posed by our
  own kinematics), with suit gloves: intravehicular gloves in the cabins, EVA gloves with the blue rubber
  fingertips outside. Hand tracking where the Quest offers it.
- **Grab** (grip) holds handrails and handholds, the ladder rungs, the tunnel. **Press** (trigger with the index
  finger near a switch) flips switches. No laser pointers.

### 8.2 The cuff checklist (the only menu inside the experience)

Apollo crews wore a small spiral-bound checklist on the left wrist. Ours is the same object: turn your left wrist
to read it, flip pages with the right hand. It holds the mission clock, **time acceleration** (×1, ×10, ×100,
×1000, ×10 000, with automatic return to ×1 before every event), the chapter list, comfort settings and the stars
setting. It looks like paper and ink, not a screen.

### 8.3 Landing controls (chapter 10)

- The autopilot flies P63 (braking) and P64 (approach). During P64 the player may **redesignate the landing point**
  with the right stick, reading the result against the window's LPD scale as the real Commander did.
- At any time below ≈ 150 m the player may **take over** (P66): the right stick is the attitude hand controller
  (tilting the LM to move over the ground), clicks on the left stick change the descent rate in steps of 1 ft/s,
  as the real rate-of-descent switch did. The LMP voice calls altitude, rate, forward speed and fuel ("60 seconds",
  "30 seconds").
- **Physical hand controllers optional:** grab the modelled hand controller in the cabin and tilt it with your hand.
  More immersive, less precise; a setting.
- **Failure:** a player who never takes over always lands safely. Once the player has taken over, the autopilot does
  not silently save them: at a contact speed above the landing gear's limit the view fades and the chapter restarts
  at high gate. A button on the hand controller hands control back to the computer at any time. (The real
  alternative, an abort-stage burn back to orbit, belongs to the later liftoff stage.)
- **Assist** setting: "Standard" (as above) and "Steady" (wind-free, limits the tilt rate) for first-timers.
- **Dust** from ≈ 30 m: the plume scours the surface and sheets of dust race outwards along the ground, hiding the
  surface texture (radial streak shader on the terrain plus a few low, fast sheets); the LM's shadow growing towards
  the LM is the last altitude cue. Contact light, engine stop, silence.

### 8.4 Moving on the surface

- **Room-scale** walking as the base.
- **Stick locomotion** (left stick) with a **slow lunar gait**: a gentle, damped rise and fall at the 1/6 g bounce
  rhythm, amplitude adjustable to zero.
- **Teleport** as an option (point at the ground, release).
- **Turning:** snap 30°, snap 15°, or smooth.
- **Vignette:** shown during stick movement and turning. Drawn as the **inside edge of the helmet**, so it belongs in
  the world.
- **Comfort slider** on the start page and the cuff checklist: one control that sets vignette strength, turn mode,
  gait amplitude and the cabin shaking together; individual settings below it for those who want them.
- **The gold visor:** lower it with a hand gesture at the helmet; it tints the world gold and dims it, as the real
  sun visor did.
- **The play area:** a few hundred metres around the LM. At the edge, the CAPCOM voice reminds you of your walk-back
  limit; you are not stopped by a wall.

### 8.5 Safety

- The Quest guardian stays authoritative. The experience never asks the player to walk while seated chapters run.
- Stick movement is off during standing chapters if the player chose "seated" on the start page.

---

## 9. Sound

- **Voices for now: Windows TTS, pre-rendered** (`tools/tts.ps1` reads the script file, one WAV per line, a
  different installed voice per speaker, converted to Opus/OGG). Not live browser speech: whether the Quest
  browser's speech synthesis works, and with which voices, is uncertain, and pre-rendered files let the owner's
  own recordings drop in later file for file.
- **The script** (`assets/script/mission.json`): one entry per line with GET, speaker, text, and the event it
  belongs to. Lines are original, in the style of Apollo air-to-ground procedure, not quotes attributed to real
  people (§1.1).
- **The radio** is processed live in Web Audio, so it applies to TTS and recordings alike: band-pass (≈ 300–3000 Hz),
  mild distortion, a noise floor rising with distance to Earth, the **Quindar tones** (2525 Hz intro, 2475 Hz outro)
  on every CAPCOM transmission, and the **signal delay** growing to ≈ 2.6 s round trip at the Moon (Houston answers
  late, as it did).
- **Synthesised:** the rocket rumble and crackle through the structure, the staging thumps, the cabin fans, the
  glycol pumps, the RCS thruster bangs (heard through the hull, loud in the LM), the descent engine's roar through
  the structure, the suit fan and breathing on the surface, the boots felt as thumps through the suit.
- **No outside sound on the Moon.** What you hear is the suit and the radio.
- Quest audio budget as learned in Portalkniven: few simultaneous HRTF sources, short reverbs, free sounds when done.

---

## 10. Trajectory and physics

- **Ephemeris:** Earth, Moon and Sun positions for December 1972 from a compact analytic theory (truncated lunar
  theory, low-precision solar position); accurate to well under a degree, which is what the views need.
- **The spacecraft's path:** numerically integrated in the Earth–Moon–Sun field (simple RK4 with adaptive step;
  ×10 000 warp is a few hundred steps per frame at most). The TLI is solved once offline (`tools/`) to hit the
  historical lunar orbit with the day-launch departure, and stored as initial conditions. Midcourse corrections are
  scripted Δv events.
- **Ascent:** the Saturn V's trajectory (altitude, velocity, pitch versus time) from published flight data as a
  table, not simulated; the vehicle follows it.
- **Lunar orbit and descent:** two-body around the Moon; the powered descent follows the Apollo guidance's
  polynomial targets for P63/P64 (simplified), with the LM's mass, thrust (throttle limits as on the real descent
  engine) and attitude dynamics; P66 holds the commanded descent rate while the player sets attitude.
- **Surface:** walking on the height function; 1/6 g; no physics engine.

---

## 11. Data sources and licences

All material below is from US government agencies (NASA, USGS) and is in the public domain unless the stage that
adds it finds otherwise; each item is listed with its URL in `vr-apps/moon/README.md` when added. **NASA insignia
(the "meatball" and "worm" logos) are not public domain in the same way** (their use is regulated) and are not used.

| Item | Source |
|---|---|
| Moon colour and elevation (global) | NASA SVS CGI Moon Kit (svs.gsfc.nasa.gov/4720) |
| Corridor elevation and mosaic | USGS Astrogeology: LROC WAC GLD100, WAC global morphologic mosaic |
| Taurus-Littrow terrain | LROC NAC DTM Apollo 17 (5 m and 2 m) and orthophotos (data.lroc.im-ldi.com) |
| Earth | NASA Blue Marble Next Generation, Black Marble, cloud composite |
| Florida from altitude | Landsat (USGS) |
| Spacecraft references | NASA 3D Resources models; Apollo Operations Handbooks; LM and CSM drawings and photographs (Apollo Lunar Surface Journal, Apollo Flight Journal) |
| Mission timeline | Apollo 17 Mission Report; Apollo Flight Journal; SP-4029 Apollo by the Numbers |
| Stars | Yale Bright Star Catalogue, 5th revised edition, from NASA HEASARC (heasarc.gsfc.nasa.gov, `heasarc_bsc5p`), added with the stars setting (25 November 2026) |
| Audio (optional, owner's choice) | NASA mission audio (public domain), only if the owner decides to use it |

---

## 12. Stages

Each stage ends in a pull request the owner reviews and tests in the headset. "Fable" marks stages where the
quality of visual and numerical judgement decides whether the result looks real; "Opus" marks well-trodden work.

| Stage | Content | Model | Done when |
|---|---|---|---|
| **0. Foundations and the Moon test** | Folder, vendored libraries, rig, flat preview and test hooks, frame-rate guard, KTX2 pipeline. Data downloader and tile cutter. A test chapter: the Moon from 110 km and from 15 km (global and corridor data, Lommel–Seeliger, opposition, horizon-map shadows) and the Taurus-Littrow valley from the surface (5 m and 2 m DTMs, generated detail, rock instances, placeholder LM box). Multiview checked | **Fable** | The owner has seen it in the Quest 3 at a stable 72 Hz and judged the Moon convincing |
| **1. Pad and ascent** | Access arm and White Room, CM interior (panel, DSKY, couches), hands, the script and TTS pipeline, radio processing, countdown, liftoff, smoke, shaking and haptics, staging, the cover and tower jettison, Earth atmosphere from pad to orbit | Opus | The 3.5 min pre-launch and the whole ascent play through; the owner has felt the launch in the headset |
| **2. Earth orbit to the coast** | Earth from orbit and from afar (clouds, night lights, glint), ephemeris and trajectory, TLI, transposition and docking (S-IVB, SLA panels, LM extraction), time warp, cuff checklist, chapter jumps, the coast with its thermal roll | Opus | One can go from orbit to the Moon's sphere of influence with warp, and docking reads as real |
| **3. Lunar orbit and the tunnel** | The Moon at full quality from orbit (built on stage 0), Earthrise, LOI and the far-side silence, the tunnel crawl, the LM cabin, undocking | **Fable** for the Moon views, Opus for the rest | The orbit views hold up at full warp and at ×1; the crawl is comfortable |
| **4. Descent and landing** | LM flight model and guidance (P63/P64/P66), LPD, takeover controls, callouts, dust, the LM shadow, contact, the seams between terrain scales during the descent, the valley swinging into view | **Fable** | A full descent by autopilot and by hand, without a visible seam between terrain scales, at 72 Hz |
| **5. The LM and the surface** | The hero LM model and materials (Blender), its interior, the hatch, porch and ladder, the rover and the flag, footprints, the gold visor, locomotion and comfort, the walkable area at full detail | **Fable** for the LM model, materials and surface; Opus for interactions and comfort | The owner has walked round the LM and judged it convincing up close |
| **6. Polish** | Sound pass, comfort pass, start page, the Brokild VR card and site sync, README with sources, performance pass across all chapters | Opus | Live on the Brokild VR page |
| Later | LM liftoff and rendezvous; the trip home and re-entry; driving the rover; the owner's own voice recordings | | |

Opus can do all of it; Fable earns its cost in stages 0, 3 (Moon), 4 and 5 (LM, surface), where an approach that
looks fine in a flat screenshot can still fail in the headset and costs several rounds to recover.

### 12.1 What the owner can supply

- His own voice recordings for the script, when ready (the file layout of §9 makes this a drop-in).
- If he has them: photographs or drawings he wants followed (as with the ship in Portalkniven, his references are
  the brief).
- His headset verdicts, after each stage (frame rate, comfort, realism, sound).

---

### 12.2 Stage 0 as built (24 November 2026)

Built and tested in the flat preview and in an emulated Quest 3; **not yet in a real headset**. See
`vr-apps/moon/README.md` for how it works. What stage 0 settled, and where it departs from the plan above:

- **Nested fixed levels instead of a quadtree** (§6.3). Seven lat/lon-aligned levels centred on the site
  (whole Moon, ±12°, ±4°, ±22 km, ±5 km, ±1 km, ±150 m), each one mesh and one map. Simpler and cheaper than
  a general quadtree, and right for an experience with one landing site. The orbit views away from the site
  (chapter 8, behind the Moon) will see only the 1.4 km whole-Moon level: stage 3 must add levels along the orbit
  track or accept that.
- **The intermediate data is SLDEM2015 (59 m)**, not WAC GLD100: it is a height model, and at a 13° Sun the relief
  carries the picture.
- **Albedo comes from the CGI Moon Kit colour map only** (about 400 m). The NAC orthophotos (0.5 m, 3.3 GB) are not
  used yet: they have the Sun's shadows in them and need photometric normalisation. On the surface the albedo is
  therefore uniform except for generated fresh-crater halos. Candidate for stage 5.
- **Baked horizon maps work** (§6.4): cast shadows for any Sun height from one texture channel, including the rocks'
  and the stand-in LM's shadows on the finest level. 8-bit normals need exact zero and dithering: one step is a
  fifth of the brightness next to the terminator.
- **Two depth passes** when standing on the surface (distant terrain, clear depth, near terrain), with the XR
  projection matrices patched per pass. Works in the emulator.
- **Multiview is not available**: three.js r170 has no OCULUS_multiview path. Not pursued.
- **The frame-rate guard can only strengthen foveation**; three.js cannot change XR resolution mid-session.
  Resolution and 72/90 Hz are chosen on the start page.
- **Tools are Python** (numpy, scipy, tifffile), not Node: the raster work needs them. KTX2 encoding uses the
  WebAssembly Basis encoder from Node, because the KTX-Software installer needs administrator rights here.
- **Cost, emulator counts per eye:** surface about 115 draw calls and 385k triangles; orbit about 130 and 250k.
  Over the §4.1 triangle budget on paper; whether the Quest 3 holds 72 Hz is the first thing to learn in the headset.
- **Size:** the Moon chapter is about 100 MB of KTX2, over the 60 MB per chapter of §4.1. If the headset shows the
  finest levels can be 2048², that halves it.
- **Stand-ins:** the LM, the Earth (NASA's Blue Marble server was unreachable on the day), no stars.

Added after the owner's first headset test (frame rate and realism judged great, also at high resolution):
bootprints, steps and fan heard inside the suit, teleport, rocks standing on the drawn ground (PR #107); and
**the whole valley walkable** (±4.8 km): the detailed ground is generated round the walker in a Worker, as detail
on top of the baked levels, a pure function of position (see the app's README, "The ground round the walker").
Stage 4 (descent) and stage 5 (surface) build on this: the landing can come down anywhere in the valley, and the
moonwalk is not tied to 150 m round the lander.

### 12.3 Stage 1, part 1 as built (24 November 2026)

The launch from the couch (`vr-apps/moon/launch.html`), T−3:30 to orbit. Not in it yet: the White Room walk-in
(part 2), Earth orbit and beyond (stage 2). Departures from §2–§9: one chapter page per chapter rather than one
page for all; the crew are present (suits, visors down) rather than voices only; the clouds are generated because
NASA's cloud composite is no longer published. Weak points the owner should judge: the clouds and the open ocean
from orbit (candidates for a Fable pass, or real cloud imagery), and the cabin walls, which are bare.

### 12.4 Stage 2 as built (25 November 2026)

Chapter 2 (`vr-apps/moon/orbit.html`), Earth orbit to the Moon's sphere of influence; the README describes it.
Departures from §10: the coast is two-body Kepler round the Earth with the Moon on a circle placed to meet it, not
an ephemeris with the Moon's pull (honest enough to the Moon's distance; chapter 3 needs the patched conic or a
proper integration); the Sun is fixed. Not built from the stage's list: the cuff checklist (A/X, Y and B do the
work), night lights. Found on the way and fixed in chapter 1 as well: the Command Module's hull and the boost
cover were wound inside out (from the front you saw through the CM). Weak points the owner should judge: how
docking reads from the couch (the rendezvous windows look forward along the nose, so one must lean up to them),
and the coast's long stretches at high warp.

### 12.5 Stage 3, part 1 as built (25 November 2026)

Chapter 3 (`vr-apps/moon/lunar.html`), lunar space to the first low pass; the README describes it. Built on Opus,
not Fable: the Moon views reuse stage 0's renderer unchanged, so the part Fable was reserved for already existed.
Departures from §10: stage 0's site frame serves as the inertial frame (the Moon's rotation, 0.55° an hour, left
out); no libration. Comfort rule added for all chapters: the attitude as seen never turns faster than 1.5° a second.
Weak points the owner should judge: how much of the near side lies in night at this time of the lunar day (set by
stage 0's landing Sun, so the approach shows a crescent and the first passes over the site are near the
terminator), the Earth's size and brightness, and whether the side window is enough or the hatch window should face
the Moon. Part 2: the crawl through the tunnel, the LM cabin and undocking.

### 12.6 Stage 3, part 2 as built (25 November 2026)

Chapter 4 (`vr-apps/moon/lm.html`), design chapter 9; the README describes it. Locomotion through the tunnel is
grab-and-pull (the hand stays where it took hold), with an automatic float and the stick as alternatives; the
change of attitude from lying in the couch to standing in the LM happens in a fade, never as a turn of the view.
The LM's cabin is primitive geometry with the CM's panel textures: stage 5 (Fable) should replace it together with
the LM's outside. Weak points the owner should judge: whether pulling yourself along feels right in the headset
(tested only with an emulated controller), the LM cabin's proportions at your own height, and how the
Command Module reads in formation through the small windows.

### 12.7 Stage 4 as built, on Opus (25 November 2026)

Chapter 5 (`vr-apps/moon/descent.html`), design chapter 10; the README describes it. This stage was marked for
Fable; it was built on Opus while the owner was away, to close the chain from the pad to the surface, and should
get a Fable pass. What was checked: the guided flight against Apollo 17's timeline (contact +1 s), the pitch-over
(60° to 25°), the view at braking, after the pitch-over, at low gate, at 30 m and on the ground (flat screenshots and
the emulator), a gentle and a hard landing by hand. What was not: seams between terrain levels in motion during the
descent (screenshots are stills), the frame rate in the headset with the ground and rocks on, and how flying by hand
feels. The LM's shadow is a draped silhouette, not a true shadow (stage 0's terrain shader casts none); the
landing point designator is not built. Departures from §10: no mass or throttle model (the guidance's
acceleration is the thrust), the windows made deeper than the cabin of chapter 4 first had them.

### 12.8 To check in the headset (written 25 November 2026)

Everything from chapter 2 on was built in one night and seen only on a flat screen and in an emulated Quest 3. In
the order of the journey:

1. **Frame rate** (the left wrist): chapter 3 at the low pass, chapter 5 below 2 km with the ground and rocks, the
   Moon page beside the new LM. If one drops, say which.
2. **Comfort**: in chapters 2 and 3 the spacecraft never turns faster than 1.5° a second as you see it (at high
   warp it holds still); in chapter 5 the pitch-over turns at 2.5° a second. Too much, too little?
3. **Chapter 2, docking**: the LM through the rendezvous window (raise your head to it), or Y for outside.
4. **Chapter 3, Earthrise** in the window beside you (left), at GET 89:25; the crescent's night side in earthshine
   on the approach; the low pass at 15 km.
5. **Chapter 4, the crawl**: grip and pull (does the direction feel right, the speed, the walls holding you?), or
   A to float; the fade into the LM and standing at your own height; the CSM backing away over the docking
   window, then in front through the triangular window.
6. **Chapter 5, the descent**: the windows to the sky while braking, the valley after the pitch-over (lean in to
   the window), the shadow running in, the dust; P66 by hand (A, right stick tilts, left stick flicks the rate);
   **seams between terrain levels while moving** (a Fable item).
7. **Stars**: is "Faint" visible on the Quest's screen, or too strong? (One number: `LEVELS` in `src/stars.js`.)
8. **The surface**: the LM you landed in, its long shadow, the walk.

### 12.9 The owner's first headset run of chapters 2-5, and what came of it (25 November 2026)

His notes, and what was done (PR "moon-crew-life"; tested in the flat preview and the emulated Quest 3 only):

- **The crew were statues that talk.** Now they live (`src/crew-life.js`): real-sized eyes behind openings in the
  scan's lids (set back so the skin covers them from the side, his second note), saccades, blinks, the lids
  following the eyes, eye contact (both eyes converge on you); the head turns at the neck, a little towards whom
  they speak to; the jaw opens with the loudness of their own voice; jointed arms that rest, reach and gesture.
  Your hand on one of them stops at his suit, turns red, and he answers (16 lines). 50 lines of small talk, now and
  then (not authentic, his wish; `tools/script/crew.json`, `src/chatter.js`).
- **The DSKY and the event timer sat on switches.** The panel keeps their places clear. And the panels were
  redrawn in the style of the real ones (his photographs): CM light blue-grey, LM darker; plates with screws, white
  group lines with names, chrome switch rings, guards, black instruments with white scales, talkbacks. Made up, not
  the real layout switch by switch.
- **The hands swapped sides** after the headset was off and on: they now follow each controller by its handedness,
  read from the XR frame every frame (`makeControllers` in `src/hands.js`).
- **The index finger pointed 15-20 degrees low**: the hand is tipped up 17 degrees, and it is a setting ("Hand angle",
  0-32) to match in the headset (no documented offset for the Quest 3 in WebXR). Then the same for Portalkniven.
- **The floating things**: taken with the grip, carried, let go with the hand's speed and spin; a real pen.
- **The status panel spoiled being there**: hidden; a left stick press shows it (redone: phase, what comes next,
  figures, buttons), again the settings, a third time away. A faster, B slower (his wish: one press back, not round
  through the fastest), X the cheat window, Y outside (both latch).
- **Settings in the experience** (`src/settings.js`): fast forward, turning comfort, frame rate, foveation, the two
  volumes, your body, the hand angle. Resolution cannot change in a running WebXR session.
- **Fast forward did not work, without saying why.** `src/clock.js`, the same everywhere: by default it stops at events
  and radio lines and says why, A goes on past them; x10 at most through an event, the big steps for quiet stretches;
  or major events only; or free.
- **The left eye saw two Earths** from far out: the sky dome worked its directions from the middle of the head, not
  from each eye; now from the eye (`src/earth.js`).
- **The CM and LM did not dock**: the probe only touched. Now it retracts after capture and pulls them ring to ring
  before the latches fire; chapter 3 starts hard docked. Chapter 4 keeps the old spacing: the inside of the CM and
  its outside do not match, and hard docked the tunnel you crawl through would be 40 cm. To do: align them.
- **The hands tore into random shapes** at the start of chapter 3: the skinned hand was skinned in world coordinates
  in 32-bit floats, 66,000 km out; now relative to the hand (`src/hands.js`).

**His second run (same day)** and what came of it (PR "moon-round2"):
- B faster, A slower (hold A for real time); a small clock low in the view while fast (the hand faster with the
  speed, 1-4 for the power of ten).
- "It crashed" at the switch to the Moon's pull: that is chapter 2's end, where the clock stops and the way on was only
  on the panel (which he had not found: it is the left stick press; the flat menu button belongs to the browser).
  Now the end is said in front of you, a hint where the panel is comes on entering VR, and Continue
  (`src/progress.js`) picks up where you were after a real crash.
- Hand angle "nearly perfect" at 17. Thumbs were too long and pointed at each other: 85 % long, opposition 46 -> 15
  (they lie forward along the top of the controller); `tools/hands.html` to try values.
- The two crewmates were twins: the LMP now has his own profile (uShape in the face shader: longer nose, narrower
  lower face, stronger chin and brow), warmer skin, brown eyes.

**To check in the headset**, besides §12.8: the faces close up and from the side (eyes, lids, the jaw), whether eye
contact feels right or too much; the hand angle (set it to match, tell me the number); touching a crewman; taking
and throwing the pen; the panel (left stick press) and the settings with the stick; B faster and A slower, the small clock; fast forward through chapter 2; the panels'
new look; the docking from outside; chapter 3's start with your hands in view.

### 12.10 Stage 5, part 1: the Lunar Module itself (25 November 2026, Fable)

The owner chose the LM built in code over waiting for a model, "starting with the hatch". Built: `src/lm-model.js`
(see the README): legs on the axes with every rod joined, the ladder and the porch on the front leg, foil, the flag
and UNITED STATES, the ascent stage's parts; the forward hatch as a real opening inside with its door. It replaced
the stand-in everywhere (same frame, same top at 4.75 m, same pad soles). On the surface the ladder faces west,
away from the Sun, as on Apollo 17.
**Part 2, the egress, built the same day** (`src/egress.js`, `index.html?egress`; see the README): cabin, hatch,
porch, ladder, the drop to the pad, the first step, then the free walk. Open: voices for it (messages only now),
the Lunar Module Pilot following you out, EVA gloves and the gold visor, your hands snapping to the rungs, not
walking through the LM in the free walk. **To check in the headset:** the scale of the cabin and the hatch, the
blink and the turn when you set out, how pulling along the porch and down the ladder feels (speed, direction),
the drop, comfort on the ladder looking down. **Part 3:** the rover parked by the LM, the flag. Not built: the MESA, the rover stowed on the descent stage, the real quad doors.
**To check in the headset:** the LM from the surface (scale, the foil in the low Sun, the ladder and porch), from
outside in chapters 3-5 (Y), the docking in chapter 2 with the new model.

**His third run (chapters 4-5, same day)** and what came of it (same PR):
- The LM's window frames flickered, "a shadow that follows where I look": each frame box had a face lying exactly in
  the wall's plane (z-fighting, its pattern moving with the head). Frames, hatch frame and the docking window's
  reveal now start a centimetre inside the wall. **Never leave two faces in one plane.**
- The LM's panels were "at the knees": raised to just under the windows and leaned back 26 degrees towards the eyes.
  The side panels were tilted plates with their controls cut off: the panel drawing now lays its sub-panels out
  within each panel's own stretch (`sections`), and the side panels top consoles standing against the walls.
- No body in the LM: your own suit stands at the Commander's station (`setBody`, the "Your own body" setting).
- The engine bell was see-through from some angles (the stand-in's one-sided bell): two-sided in the new model.
- The rods under three footpads are real (the contact probes); they go at touchdown now.
- The DSKY is gone from the Command Module (it broke the panel for him); the LM keeps its one.
- **Chapters without leaving VR**: at a chapter's end a card: A continues, B takes a break; a title card at each
  start. The page changes while in VR and the next enters VR by itself on the Quest's 'sessiongranted' event
  (`autoEnter` in `src/progress.js`). Not testable in the emulator: **the first thing to check in the headset**.
  If it does not work there, you land on the next chapter's start page as before.

**After his first landing (same day, same PR):** the footpads hung in the air: the LM now settles on to all four
(`settleLM` in `src/lm-model.js`: it leans with the ground, pads 4 cm into the dust), at touchdown and on the surface
page. The outside camera could go under the ground: held half a metre above the terrain, every frame. Dust: besides
the sheets, grains thrown out in the last 8 m and at touchdown, each on its own arc in lunar gravity (no cloud: there
is no air). No exhaust plume, on purpose (nearly invisible in vacuum). The crew turn to whom they speak to first.
Chapter 4's start: a message after 10 s in the couch (and a card from the second time), the stick sets you off, and
the way to the tunnel rises over the instrument panel. Briefing: B or trigger forward, A back, Y leaves.

**Chapter 6 is a chapter of its own** (`surface.html`, 25 November 2026): the owner could not reach the climb-out on the
Moon page (its buttons were under the browser window's edge in the headset) and asked for a real chapter. Title
card, a start list (cabin or ground), its place kept for Continue, in the chain test and the site's tables. The Moon
page (`index.html`) stays as the hub and the three test views.

**After his first walk outside (25 November 2026):** the cabin was drawn in front of the LM near the hatch: its front
wall lay in the plane of the model's front face and its box poked through the hull. The cabin's walls are now drawn on
their inner side only where it stands inside the model (`makeLMCabin({ inwardOnly })`), the model's front face stands
2 cm clear, and the windows' deep frames are not drawn from outside. A long shadow lay at the landing spot before the
LM arrived: the terrain's baked shadows still held stage 0's stand-in LM (`tools/build.py`); taken out, L6 baked
again. In the LM your eyes are put at standing eye height (1.6 m), level with the other man's, whatever your own
height or posture (chapters 4-6; on the ground your own height returns). **Agreed next (Fable):** real EVA suits in
code (PLSS, chest unit, LEVA helmet with the gold visor, the NASA patch and flag: the owner wants them, for
history's sake), then the Lunar Module Pilot coming down and loping about, then the flag and the rover.

**The cabin over the LM, second round (same day):** inward-only walls were not the cause. The Quest does not keep the
depth buffer from one render call to the next (a PC does), so the cabin's pass, relying on the LM's depth from the
pass before, lay over the LM "like a lift cage". Now the cabin's pass draws the LM's outside into the depth buffer
only (colorWrite off) in the same call, after a clearDepth: nothing depends on an earlier pass. **Every pass on this
app starts with a fresh depth buffer; check multi-pass effects with a clearDepth before each pass on the PC.** The
hull's parts show no face towards the cabin inside them (`boxNo` in `src/lm-model.js`). The LM's side panels: the
second box that hid them is gone.

### 12.11 The surface suits, and the owner's first walk round the LM (25 November 2026, Fable)

- **The suits** (`src/eva-suit.js`): the A7LB with the PLSS, in code, jointed. The owner's corrections on the way,
  from the viewer and a training photograph: the arms bent the wrong way (a sign error), the helmet was too small
  (the LEVA is nearly as wide as the chest), and the whole suit was too slim ("a small spacecraft in its own
  right"). The NASA insignia and the flag are worn, by the owner's decision. In chapter 6 the Lunar Module Pilot
  wears one in the cabin, and you wear the Commander's, in the cabin and on the ground.
- **From the owner's headset run of chapter 6**, fixed in the same change:
  1. The hull was hidden by the cabin from three sides: the test for "outside" only knew the front. It is now
     "anywhere but in the cabin's box".
  2. The hands were black near the LM: their pass had no light at all (the lights hang on the LM's group, which
     that pass hid).
  3. Holes in the LM's shadow: the ground's bumps poked through a coarse drape, and the silhouette was a drawing.
     It is now the model as the Sun sees it, baked once, on a fine drape.
  4. The crewman is not seen through the windows from outside: the glass is dark and mirrors, as on the real LM in
     sunlight. Left so; he is seen through the open hatch.
  5. The ladder was two or three feet beyond the hands: it leans, and you do not. Your eyes' column is moved in.
- **Next**: the Lunar Module Pilot down the ladder and moving about (the lope), the flag, the rover.

## 13. Testing

- As the other VR apps: serve the repository root, open `/vr-apps/moon/?flat`, Playwright with Chromium and
  SwiftShader (`--use-gl=angle --use-angle=swiftshader --enable-unsafe-swiftshader --ignore-gpu-blocklist`), wait by
  frames, not by time; the IWER emulator for XR (`test/xr.test.cjs`, after Lysrytme and Planet Designer).
- Test hooks on `window.__moon`: chapter jump, set GET, set time warp, pause, set the head pose, set the LM state.
- **Look at the screenshots.** For the Moon, compare against real photographs from the same geometry (Apollo 17
  orbital photos of the valley, surface photos at the LM) side by side: the reference strip method used for the
  growth lecture visuals.
- **Every report says what was tested in the flat preview and what must be checked in the headset.**

---

## 14. Risks and honest limits

- **Quest memory and download size.** The data is large; the per-chapter budget (§4.1) is a hard limit. If the 2 m
  DTM plus orthophotos do not fit, the walkable area shrinks before the quality does.
- **Seams between terrain scales** during the descent are the most likely visual failure. Stage 4 owns them.
- **Volumetric smoke and clouds** cannot be real volumes on a Quest at 72 Hz. Pre-rendered puffs lit with normal
  maps are the plan; if they do not convince up close, they are kept further away (the cover window helps).
- **Human figures.** Crew mates in the cabins are a risk (the uncanny valley at arm's length). Proposed: present but
  in suits with helmets, faces not lit, small idle motion; their voices come from their positions. If they do not
  convince in the headset, they become voices only.
- **The CM panel's density** (hundreds of labels) is at the edge of what the Quest's resolution can show; labels may
  have to be slightly enlarged relative to the real ones.
- **The TTS voices** will sound synthetic however processed; the radio band hides some of it. The owner's recordings
  are the real fix.
