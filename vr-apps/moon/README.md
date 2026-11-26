# Journey to the Moon

A realistic Apollo-style flight to the Taurus-Littrow valley for the Meta Quest 3 browser. Design, decisions and
stages: `MOON-DESIGN.md` in the repository root. This file describes what is built.

**State: stages 0 to 4, the whole way from the pad to the surface.** The Moon test (below, also the walk on the
surface), the briefing, chapter 1 (the launch), chapter 2 (to the Moon), chapter 3 (lunar orbit), chapter 4 (into the
LM) and chapter 5 (the descent).
The Moon test: three views of one baked Moon, to judge realism and frame rate in the headset
before anything else is built:

| View | What it tests |
|---|---|
| Lunar orbit, 110 km | The whole-Moon and regional data, shading without limb darkening, shadows near the terminator |
| Low pass, 15 km | The 59 m and 11 m data and the joins between levels at speed |
| On the surface | The valley from eye height: the 2 m terrain model, generated small craters, rocks, soil, long shadows |

The LM on the surface is the one you land in in chapter 5 (`makeLMModel` in `src/vehicle.js`, legs deployed, placed
where the descent sets it down), with its long shadow from the low Sun (`src/lm-shadow.js`, shared with the descent);
the hero LM is stage 5. The Earth overhead is the real one (Blue Marble, real clouds), and the stars are real (below).

On the surface (added after the first headset test):

- **Bootprints** (`src/prints.js`): the Apollo overshoe's ribbed sole, a print every stride whether you walk in the
  room or with the stick, two side by side where you land after a teleport, and a trail from the ladder to where
  you start. One instanced mesh (one draw call, up to 1200 prints, the oldest reused). Each print is shaded by the
  terrain's own shader (`moon.printMaterial`) with the print's relief on top and an eight-step march over its height
  for the shadows of its walls and ribs at the low Sun. The relief is made in code; no image files.
- **Sound in the suit** (`src/sound.js`, synthesised): a dull thud through the body and a dry, gritty scrape at
  every step, and the suit fan's soft hiss and hum (can be switched off on the start page). Nothing from outside:
  there is no air.
- **Teleport**: hold the right trigger to aim an arc (thrown from the hand, falling in lunar gravity), let go to
  stand there (a short blink). The right stick forward throws it further: from a hop of ten metres to kilometres,
  out to the massifs. Anywhere in the valley, ±4.8 km round the lander. Can be switched off on the start page.
- **The whole valley is walkable** (`src/ground.js`, `src/ground-worker.js`): see the next section.
- **Standing on the ground as drawn**: walking, rocks and prints use the finest mesh's own triangles
  (`gridHeight` in `src/moon.js`), not the finer data behind its shading. Rocks take the lowest point under their
  footprint and are bedded in, so none hangs in the air on a slope or a crater rim.

## The briefing (`briefing.html`, optional, not authentic)

A dark room with one large curved screen and a 1:60 model of the Apollo-Saturn V on a pedestal (the same model as
the outside view, `src/vehicle.js`). Twelve boards drawn in code (`src/briefing-slides.js`) walk through the whole
mission, each narrated (`tools/script/briefing.json`, Windows voice for now): the crew, the vehicle, launch to
orbit, Earth orbit and trans-lunar injection, transposition and docking, the coast, lunar orbit, into the Lunar
Module, the powered descent, Taurus-Littrow, the way home, and the non-authentic controls. On the vehicle and launch
boards the model turns and runs through its staging. Trigger or A: next board; B or X: back; the last board leads
to the launch. Test: `test/briefing.test.cjs`; screenshots `tools/shot-brief.cjs`.

## Chapter 1: the launch (`launch.html`)

**Curiosity, not authentic** (the owner's wish: authentic, but allowing curiosity):
- **X**: a large window opens in the wall to your left (`setCheat` in `src/cabin.js`; its own wall cells, a red rim);
  X again closes it.
- **Y**: you leave the spacecraft and look at it from outside (`placeOutside` in `src/launch.js`): left stick
  round it and nearer or further (7-600 m), right stick up and down; the horizon stays level. Y again to return.
  (Both stay till pressed again, so you can change the speed or the settings meanwhile; until November 2026 the
  cheat window was Y and outside was B held.)
- The **vehicle from outside** (`src/vehicle.js`): the three stages with their paint, the F-1 and J-2 engines with
  their flames (the F-1 plume blooms with height), the stages and the interstage skirt falling away at staging with
  the retro-rocket flash, the escape tower pulling the boost cover off and flying away, the Service Module, and the
  Command Module in its aluminised tape (windows, hatch, docking probe). Steam and smoke roll out of the flame trench
  at ignition (`src/pad.js`).

**Hands, faces, body:**
- Your **gloved hands** follow the controllers (Portalkniven's hands, `src/hands.js`, in the cabin glove colour),
  each by its handedness (they could swap sides after the headset was taken off and on), tipped up 17° against the
  controller (a setting, "Hand angle"). Touching a panel turns the hand red, a small "Not now" sign appears and the
  controller ticks. Touching a crewmate the hand stops at his suit and turns red, and he answers.
- The crew's **faces** are a photoscanned head (Lee Perry-Smith, see Sources) seen through the bubble helmet, under
  the communications cap, the two crewmates tinted and proportioned differently. They live (`src/crew-life.js`):
  see "The crew" below.
- Looking down you see **your own suit** in the couch (torso, hoses, legs; no arms: your hands are the
  controllers). An option on the start page.

**Boarding** (the default start, `src/pad.js`): you stand at the tower end of the crew access arm, 106 m up, the
count holding at T−3:30. Walk (left stick, or in the room) along the enclosed arm into the White Room, where the
Command Module's hatch stands open under the boost protective cover, your crewmates strapped in inside. Walk up to
the hatch (or pull the trigger there): the view goes dark, the hatch closes with its latch ratchet, you are seated
in the couch, the arm swings back to its park position, and the count resumes. From the arm: the Saturn V's upper
stages beside you venting vapour, the escape tower above, the launch umbilical tower round you, the pad and its
flame trench below, the crawlerway towards the Vehicle Assembly Building, the propellant spheres, the beach and the
Atlantic to the east. The pad leader and Launch Control speak while you board (cued radio lines).

From the Commander's couch (the left one) on top of the Saturn V: T−3:30 to orbit insertion at T+11:53 and a minute
beyond. You sit upright; the cabin is turned round you so the main display console is in front, the hatch above.

- **The flight** (`src/ascent.js`): altitude, speed and pitch against time from published Apollo flight data,
  downrange integrated from them; Apollo 17's event times. The roll program turns you from facing the tower to
  facing downrange; you fly heads down, so the Earth's horizon comes into the windows above your head.
- **The cabin** (`src/cabin.js`): the 33° cone with its five windows where they were (the hatch window over the
  centre couch, two rendezvous windows looking forward, a side window each side), set deep in the wall; the main
  display console as three panels with a generated panel face and a few hundred 3D toggle switches; the DSKY showing
  the real launch programs (P02 on the pad, then P11, verb 06 noun 62: velocity, altitude rate, altitude); the
  mission event timer; an attitude ball that turns with the spacecraft. The walls are painted, riveted panels with
  Velcro patches, stencilled markings and wear (a generated texture), with the hatch and its latch gear, handholds,
  cable runs, floodlights and couch struts on them, and stowage lockers on the aft bulkhead. Your crewmates wear
  the launch suit: white, with the red and blue hose connectors and hoses, wrist rings, gloves, the flag on the
  shoulder, and the clear bubble helmet over the communications cap.
- **Seating in VR**: the headset's own origin is wherever the Quest was last recentred, so the couch is brought to
  you when the session starts (`seat()` in `src/launch.js`); a thumbstick press re-seats you. The headset's own "reset view" does the same by
  itself in every seated chapter and at your station in the LM (`watchRecentre` in `settings.js`: the reference
  space reports it, or the pose jumps): you stay where you belong, not where the new origin would put you.
- **The cover**: until the escape tower is jettisoned (T+3:18) the boost protective cover blinds every window except
  a 20 cm hole over the hatch window; then the windows open onto a sky already going black.
- **Outside** (`src/earth.js`, `src/sky-worker.js`): the sky and the air in front of the ground by single
  scattering (Rayleigh and Mie), computed in a Worker as a table for the current altitude, packed round the limb
  (the thin blue line on the Earth's edge from orbit comes out of it); the Earth from NASA's Blue Marble (December)
  with a sharper map of Florida and the Atlantic down the launch track, sun glint on the oceans, generated clouds;
  on the pad, the ground and the red launch umbilical tower with its swing arms; the escape tower and cover flying
  away ahead at jettison.
- **Drawn through the windows only**: the cabin is drawn first and writes the stencil; the outside is drawn only
  where it did not, in two more passes with their own near and far planes.
- **Sound** (`src/launch-sound.js`, `src/radio.js`): the rocket through the structure (rumble, roar, crackle;
  loudest in the first half minute and through max Q, silence at staging), the separation charges, the escape
  motor; cabin fans and pumps. The radio: 63 lines (`tools/script/launch.json`) in five voices on four channels,
  Launch Control and the pad loop band-limited, Houston with the Quindar tones, your crewmates on the intercom.
  Original lines in the style of Apollo procedure, not quotes. Voices are Windows voices rendered by
  `tools/tts.ps1` (run it in PowerShell 7: Windows PowerShell 5 does not see all the voices); your own recordings
  replace them file for file in `assets/script/voice/`.
- **Shaking**: the cabin shakes round you (a few millimetres, the S-IC cutoff jolt); your view never does. The
  controllers vibrate with the rocket. A setting on the start page: as it was, half, none.
- **Time**: A or X runs it ×4 or ×16 once the windows are clear, never across the staging or the shutdown.

Earth textures: `tools/earth.py` from NASA's Blue Marble Next Generation, December
(https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/bmng-base/december/). The clouds are
real: the cloud cover of 6 December 2019, taken from NASA's VIIRS true-colour mosaic of that day (Suomi NPP, from
NASA GIBS) against the cloud-free Blue Marble, for the whole Earth (10 km) and for the launch track (1 km). The
day was picked for a clear sky over Florida and a frontal band and fields of cumulus over the Atlantic.

Test aids: `launch.html?t=` (start time), `?still`, `?nocabin` (the outside without the spacecraft), `?shot`;
`tools/shot-launch.cjs`; `test/launch.test.cjs` (emulated Quest 3). Hooks on `window.__launch`.

## Chapter 2: to the Moon (`orbit.html`)

From Earth orbit to the edge of lunar space, GET 0:12 to about 71:53, in the Commander's couch. The same cabin,
spacecraft and Earth as chapter 1; the pad frame of the launch, frozen at liftoff, is the inertial frame.

- **The flight** (`src/space.js`): a circular 167 km parking orbit in the plane the ascent ended in; the S-IVB's
  second burn at GET 3:12:36 (5 min 51 s), integrated step by step with its thrust scaled so the coast reaches
  410,000 km (cutoff at 10.83 km/s, 322 km up); then two-body Kepler propagation, exact at any time warp. The Moon
  moves on a circle in the same plane, placed so the coast meets it. Its gravity is not modelled: the chapter ends
  where it would take over, 66,000 km from the Moon. The Earth turns (15° an hour); the Sun stays put.
- **Transposition and docking** (`src/orbit.js`, `src/vehicle.js`): the CSM separates, the four adapter panels
  open and drift away, the CSM pitches round, closes in and docks with the LM (probe, capture, the twelve latches),
  then pulls it out of the S-IVB on springs; the S-IVB drifts off and is gone after 8 km. Bangs at each event.
  The rendezvous windows look forward along the nose: to see the LM coming, raise your head to the window, as the
  pilot did at the sighting device, or Y for the outside view.
- **The coast**: the attitude follows the procedure (nose along the velocity before the burn, the Earth overhead;
  from GET 6:00 the slow roll of passive thermal control, never faster than 1.4° a second to look at). In the
  Earth's shadow the Sun goes out, outside and in the cabin. The Moon (`assets/earth/moon2k.jpg`, from the colour
  map of NASA's CGI Moon Kit) is lit like regolith, its near side to the Earth.
- **Time**: A or X: ×10, ×100, ×1000, ×10,000. The warp stops short of the burn, the docking and the extraction
  and runs them at real time. The start page and the keys 1–6 jump to the chapter's points.
- **Radio**: 33 lines (`tools/script/orbit.json`), Houston and the intercom, as in chapter 1.
- **The end**: A (or N on the screen) leaves VR and opens the Moon page, until the lunar orbit and the landing
  are built. Chapter 1 ends the same way, with A taking you to chapter 2.

Not in it yet: the cuff checklist, midcourse corrections, the Moon's pull, city lights on the night side.

Test aids: `orbit.html?t=` (GET, s), `?still`, `?shot`, `?out&od=&oa=&oe=` (outside view: distance, azimuth,
elevation), `?yaw=&pitch=`; `tools/shot-orbit.cjs`; `test/orbit.test.cjs` (emulated Quest 3). Hooks on
`window.__orbit`.

## Chapter 3: lunar orbit (`lunar.html`)

From where chapter 2 ends (GET 71:52, 66,000 km out) to the first low pass of the descent orbit (GET 94:20).

- **The flight** (`src/lunar-space.js`), in stage 0's site frame (x east, y up, z south at the landing site), used
  as an inertial frame for the chapter: the Moon's turning (0.55° an hour) is left out, so every orbit passes over
  Taurus-Littrow; the Sun turns back from where stage 0 baked it at the landing (0.508° an hour, `setSunOffset`), so
  the light here leads into the landing's. The orbit plane follows stage 0's ground track. The approach is a
  hyperbola timed to Apollo 17's LOI (ignition GET 88:54:22); LOI (6 min 33 s) and DOI (22 s, GET 93:08) are
  integrated step by step, their thrust scaled for 315 by 97 km and for a low point of 15 km eight degrees before
  the site. Between burns, two-body Kepler. Both burns happen behind the Moon; the radio goes silent there
  (computed: loss of signal 88:49:46, back at 89:25:16 with Earthrise).
- **The Moon** is stage 0's (`src/moon.js`) as it is; **the Earth** in its sky (`src/extras.js`) is now the real one
  (Blue Marble and the clouds of chapters 1 and 2, turning), also on the Moon page.
- **Your window**: on the approach the Moon is in the side window beside you; in orbit the Moon is below and that
  window looks ahead along the track, turned 21° towards the Earth's side and down to the horizon, so the Earth
  rises in it. For the burns the spacecraft turns engine first.
- **Comfort**: the attitude never turns faster than 1.5° a second as you see it. At high warp the spacecraft holds
  still while the Moon turns below, and catches up when time slows. (Chapter 2 now does the same, except during
  docking.)
- **Time**: ×10 to ×10,000; round the burns, Earthrise and the low pass ×10 at most.
- **Radio**: 41 lines (`tools/script/lunar.json`), timed to the computed events.
- **The end**: A (or N) leaves VR and opens the Moon page, until the crawl into the LM and the landing are built.
  Chapter 2 now ends with A taking you to chapter 3.

Not in it yet: the crawl into the LM and undocking (next), earthshine on the Moon's night side, stars.

Test aids: `lunar.html?t=`, `?still`, `?shot`, `?out&od=&oa=&oe=`, `?yaw=&pitch=` (the window beside you: yaw 62);
`tools/shot-lunar.cjs`; `test/lunar.test.cjs` (emulated Quest 3). Hooks on `window.__lunar`.

## Chapter 4: into the LM (`lm.html`)

GET 106:40 to 110:05, in the descent orbit (chapter 3's flight and Moon). Design chapter 9.

- **The crawl**: you start in your couch; the tunnel to the LM is open (probe, drogue and hatch stowed) and the LMP
  has gone ahead. **Grip** with either hand takes hold where the hand is; pull, and you move the other way (the hand
  stays where it took hold); let go and you drift, slowing gently. Or **A** (F on the screen) floats you through, or
  the left stick. You stay inside the cabin's cone and the tunnel. Through the LM's overhead hatch and down into its
  cabin, a short fade, and you stand at the Commander's station at your own height (seated players are lower,
  which is fine). The clock will not pass ten minutes before undocking until you are aboard.
- **The LM's cabin** (`src/lm-cabin.js`), simplified from the 1969 LM, in the LM model's own frame so the hatch
  meets the tunnel outside: the two triangular windows (forward and down, as for the landing), the docking window
  over your head, the flight panels with their attitude balls, the DSKY on the centre console, side consoles,
  armrests with the hand controllers, restraint cables, the ascent engine cover and the alignment telescope. The
  LMP stands beside you (the crewman of `cabin.js`, standing).
- **Undocking** at GET 107:47:56: the latches let go, the CSM backs away overhead (look up through the docking
  window), comes round to a point ahead, right and below, where both windows look, nose towards you, for the
  inspection of your legs; the separation burn at GET 108:17:56 opens the distance. The LM's legs are deployed in
  this chapter (`makeLM` in `src/vehicle.js` now has both: folded, and deployed with each strut meeting its pad and
  the contact probes).
- **Radio**: 21 lines (`tools/script/lm.json`), two cued on your arrival.
- **The end**: go for powered descent (GET 110:00). A (or N) opens the Moon page until the descent is built.
  Chapter 3 now ends with A taking you here.

Test aids: `lm.html?t=`, `?lm` (standing in the LM), `?float=z` (on the tunnel's axis at cabin z), `?still`,
`?shot`, `?out&od=&oa=&oe=`; `tools/shot-lm.cjs`; `test/lm.test.cjs` (emulated Quest 3, including a real grip-and-pull
with the emulated controller). Hooks on `window.__lm`.

## Chapter 5: the descent (`descent.html`)

GET 110:04:53 to the landing at 110:21:58, standing at the Commander's station (design chapter 10).

- **The flight** (`src/descent-space.js`), in stage 0's site frame with the Moon's gravity: PDI at 15 km, 480 km
  before the site, at the descent orbit's speed. Guidance as cubic targets in time (Apollo flew polynomial targets
  too, simplified): P63 braking to high gate (11.3 km out, 2.3 km up, 150 m/s), P64 approach to low gate (350 m out,
  150 m up), then 30 m over the landing point and straight down at 1 m/s. High gate is placed so the approach
  brakes gently: the LM pitches from 60° to 25° off the vertical there, and the valley swings into the windows. The
  autopilot's contact comes at Apollo 17's minute (+1 s), at the stand-in LM's place on the Moon page, so the walk
  begins where you land. Peak thrust 3.8 m/s², a little above the real engine's at the pitch-over.
- **The attitude** follows the thrust (the LM's +y), windows to the sky while braking, turning ahead as the thrust
  comes up; at most 2.5° a second as you see it (8° by hand).
- **P66 by hand** (offered below 150 m): A takes over; the right stick tilts the LM (up to 20°: that is how it moves
  over the ground); a flick of the left stick up or down changes the descent rate by 1 ft/s, as the real switch did;
  A again gives it back to the computer (which then flies live to the landing point). Fuel counts from low gate
  (hover seconds, about two minutes left at the autopilot's contact; "sixty seconds", "thirty seconds"). Contact
  faster than 3 m/s down or 1.5 m/s across, or out of fuel, by hand: back to high gate. The autopilot always lands.
- **Looking out**: the triangular windows were made deeper (down to 45° below eye level, more when you lean in).
  The Moon is stage 0's: one pass from high up, its two surface passes below 3.5 km, the roaming ground and the
  rocks near the site below 2 km. The LM's shadow is laid on the ground from the low Sun (a silhouette draped over
  the terrain), running in towards you as you come down; dust sheets race out from under the engine below 30 m.
- **Radio**: 29 lines (`tools/script/descent.json`): time lines to high gate, then the LMP's calls by altitude, fuel
  and events (contact light, engine stop, "on the ground in the valley of Taurus-Littrow").
- **The end**: 40 s after contact; A (or N) opens the Moon page, standing beside the LM. Chapter 4 now ends with A
  taking you here.

Not in it yet: redesignating the landing point (LPD), the engine's own sound and the abort.

Test aids: `descent.html?t=`, `?still`, `?shot`, `?out&od=&oa=&oe=`; keys T (take over), arrows (tilt), [ ] (rate);
`tools/shot-descent.cjs`; `test/descent.test.cjs` (emulated Quest 3: the guided flight, the pitch-over, a gentle and
a hard landing by hand, the autopilot's landing). Hooks on `window.__descent`.

## The panel, the settings, fast forward

**The panel over your left hand** (`src/wrist.js`) is hidden until you press **the left stick**: the status (the chapter, where
you are in it and what comes next with its time, the figures that matter there: heights, distances, speed, fuel;
the mission clock and the time warp; the buttons; the frame rate in small type). Again: **the settings**
(`src/settings.js`; left stick up and down to choose, left and right to change), a third time puts it away. At a
chapter's end it opens by itself with the way on (A); in the descent it opens when you take over by hand (its
figures are what you fly by). The settings are kept in the browser and follow you from chapter to chapter: fast
forward, turning comfort (the fastest the spacecraft turns as you see it: 0.75, 1.5 or 3°/s), frame rate (72, 90,
120 Hz), foveation, the radio's volume and the engines', your own body, the hand angle. And **messages on screen**,
**off unless you turn them on**: hints and the radio's words in text (`makeToast` in `wrist.js`). A few are shown
whatever it says: answers to your own button, and the handful of things you cannot do without (`say(text, secs, true)`).
Long messages go on two or three lines. Resolution is not there: a
WebXR session cannot change it while running (the start page).

**Fast forward** (B one step faster, A one step slower, hold A for real time at once; `src/clock.js`, the same in every chapter). While time runs fast a small clock sits low in your view (it follows your head): its hand goes round faster the faster time runs, and the number beside it is the power of ten (1 for ×10 ... 4 for ×10,000):
- *stops at events and voices* (the default): fast until an event or a radio line, then real time there with a word
  in front of you saying why; B goes on fast again, past it. Through an event (a burn, the docking, Earthrise, the
  landing...) ×10 at most; the big steps (to ×10,000) are for the long quiet stretches.
- *stops at major events*: the same, not stopping for the radio.
- *never stops*: the flight in chapters 1-4 is worked out from the clock alone, so staging, docking and orbits are
  right wherever you land. Two things stay real time in every setting: chapter 4 waits until you are in the LM,
  and flying the descent yourself.

**From chapter to chapter**: when a chapter starts, a card shows its number and title; at its end the card offers
**A: continue** into the next chapter and **B: take a break**. Continuing changes the page while still in VR, and on
the Quest the next page enters VR by itself (`autoEnter`, the browser's 'sessiongranted' event); elsewhere its start
page shows.

**Continue** (`src/progress.js`): each chapter keeps where you are (page and mission time, every few seconds) in
the browser, and at its end the start of the next chapter. After a crash or a break, the chapter's start list offers
"Continue where you were" (chosen already), and the front page a Continue link. At a chapter's end the way on (A) is
also said in front of you, not only on the panel.

**Things floating** in the Command Module (`src/floaters.js`): the checklist card (with its cord in chapter 2) and
a Fisher space pen. Close the grip near one and it is in your hand; let go and it floats on with the speed, direction
and spin your hand gave it, bouncing softly off the cabin.

## The crew

`src/crew-life.js` moves them; `src/cabin.js` (`makeCrewman`) builds them; `tools/crew.html` is a close-up to look
at them (`tools/shot-crew.cjs` takes its pictures).
- **Eyes**: real-sized eyeballs (iris, pupil, a wet highlight) behind openings cut in the scan's closed lids, set back
  so the skin covers them from the side. They look at one thing at a time and jump to the next (saccades), with tiny
  movements while fixing; both eyes converge on what they look at, so eye contact is real. The lids follow the eyes
  up and down, and blink every few seconds and with big glances.
- **Head**: turns at the neck, part of the way to what the eyes look at, slowly; a little drift and breathing.
- **What they look at**: the panel mostly, now and then the other crewman, something off to the side, or you; back
  at you if you look at one of them for a while. Speaking, a little towards whom they speak to: you, the other
  crewman when answering him, the panel when calling Houston. In the descent the LMP keeps to the panel.
- **Jaw**: opens with the loudness of the speaker's own voice as it plays (the radio measures each speaker).
- **Hands**: jointed arms (two-bone IK), resting on the armrests, now and then reaching to the panel, the chest
  connectors or the thigh; talking to you, the hand nearer you lifts a little.
- **Touched** (your hand on one of them: spheres round the body): the hand stops at the suit and turns red, he turns
  to you and says something ("Hands off, Commander", "What are you doing?"... eight each).
- **Small talk** (`src/chatter.js`, `tools/script/crew.json`, 50 lines): a remark every few minutes of real time,
  only in a quiet moment and where it fits (the pad, weightless, the Earth or the Moon in the windows, the LM). Not
  authentic, by the owner's wish. Fast forward never stops for it.

## The Lunar Module (`src/lm-model.js`)

Stage 5's detailed model, built in code (the owner's choice over a supplied model), in every chapter from the
docking on. The same frame as the cabin inside (`src/lm-cabin.js`): y up from the engine bell, +z forward.
- **Descent stage**: the octagon in crinkled gold, amber and black foil (a generated normal map of flat facets),
  the engine bell, the landing radar, the flag and UNITED STATES (no NASA insignia, as on the real LMs).
- **Legs** on the four axes: outrigger truss, primary strut (foil-wrapped cylinder and piston), two secondary
  struts, the footpad on its ball joint; every rod ends on a shared joint. Contact probes under three pads (gone
  once landed: `setLanded`). Stowed or deployed: `setLegs`.
- **The front leg**: the ladder (nine rungs) and, over its outrigger, the porch from the hatch's sill with a
  handrail each side. `userData.ladder` and `.porch` say where they are, for the climb down (to come).
- **Ascent stage**: the faceted cabin drum with the two triangular windows and the square forward hatch (its own
  mesh, `userData.hatch`), midsection, aft equipment bay, the two tank shrouds (the Commander's further out), four
  thruster quads with plume deflectors, docking tunnel and drogue, rendezvous radar, S-band dish, VHF aerials,
  docking target, docking window.
- One mesh per material: about 19 draw calls, 10,000 triangles.
- **Inside**, the forward hatch is now a real opening in the front wall under the instrument panels, with a door
  hinged on the Lunar Module Pilot's side that swings inwards (`setForwardHatch(k)`); the DSKY moved up between
  the two flight panels, clear of it.
- `tools/lm.html` shows the model alone (`tools/shot-lm-model.cjs` takes pictures; `lm.html?lm&fhatch=1` shows
  the hatch open from inside).

## The surface suit (`src/eva-suit.js`)

The Apollo A7LB with its backpack, built in code as a jointed figure (pelvis, waist, arms and elbows, legs and
knees; `userData.joints`, `userData.pose()`): the Portable Life Support System with the Oxygen Purge System and its
aerial on top, the Remote Control Unit on the chest, the hoses round from the pack, the big helmet shell with the
gold visor (it mirrors a small generated sky: black, the sunlit ground, the Sun, the Earth; `suitEnvironment`), the
overshoes with blue soles, grey gloves with blue fingertips, bellows at shoulder, elbow and knee, the Commander's red
stripes, dust on the lower legs. The NASA insignia on the right breast and the flag on the left shoulder, as worn:
the owner's decision, for history's sake (the rest of the app carries no NASA insignia). Bulky on purpose: it is a
small spacecraft. About 43 draw calls and 25 000 triangles a figure.
- `makeEVASuit({ commander, player, env })`. `player`: your own suit, without helmet and arms (your hands are the
  controllers, in the suit's grey gloves with the blue and red wrist rings).
- Chapter 6: the Lunar Module Pilot stands in the cabin in his (a slow life of its own: the stiff torso turns,
  now and then to you); you wear the Commander's, in the cabin and on the ground (it stands under your eyes, turns
  with you and is scaled to your height).
- Viewer: `tools/suit.html` (`?az &el &d &at &lmp &player &sun`), pictures with `tools/shot-suit.cjs`.
- **The helmets go on in the cabin** (the journey is flown bare-headed): chapter 6 opens with the Lunar Module
  Pilot's helmet in his hands and his head bare in the communications cap; he lifts it on, then yours comes down
  over your head, and only then is the cabin let down to vacuum (about ten seconds in all).
- **Your helmet from inside** (`makeHelmetView`): the shell's dark lining at the edge of your view, the brow's rim
  above, a faint amber towards the visor's edges, clear in the middle. It is fixed to the suit, not to your head:
  you look about inside it (it turns with your body, a little behind your look).
- Next: the flag; the rover.

## The Lunar Module Pilot on the surface (`src/lmp.js`)

Seven seconds after you are down he follows, as you did: backwards through the hatch on hands and knees, along the
porch, down the ladder a foot and a hand at a time, the slow drop to the footpad, a hop back onto the ground (to one side
if you stand where he would land). Then he moves about in front of the LM in the lope: long low
hops from one foot to the other, leaning into it. At each spot he looks round, bends to work at something, or turns
to you and lifts an arm; if you stand in his way he stops and turns to you. If the chapter starts on the ground he is
out already. All motion is made in code from the suit's joints, eased from pose to pose.
- **What grounds him**: bootprints, dust thrown out at each landing on clean arcs (no air), and his shadow: the
  figure as the Sun sees it, drawn into a small texture seven times a second and draped along the ground (it leaves
  his feet when he hops). `makeFigureShadow` also gives you your own long shadow, from a whole suit that is never
  drawn itself.
- **The LM's shadow dims** him, and your own suit: `lm-shadow.js` keeps its baked silhouette on the page's side too
  (`lit(point)`), and a figure in the shade is lit by the ground's glow only.
- **Drawing**: he is drawn with the world; where the cabin's pass follows (near the LM) he is drawn into its depth
  buffer too, as the hull is, so the cabin behind the hatch does not show through him on the porch.
- **His visor is a mirror** of the valley, the LM and you: a 256-pixel cube map drawn from his helmet in `main.js`
  (`reflect`), one face every other frame in the view's own two passes (far, then near), only while you are within
  14 m of him; the frame-rate guard slows it to every sixth frame before it touches the foveation. In it you are a
  whole suit (`meGhost`: helmet, and arms that follow your hands by a two-bone reach), the same suit your shadow is
  drawn from, so your shadow lifts its arms with you.
- **Solid**: you cannot pass through the LM (from above: the descent stage, the four legs with struts and pads, the
  ladder on the front leg) or through him (`clearOf`, `collide` in `main.js`). Your body is a 0.3 m circle under
  your head, wherever in your room you stand; it is pushed out the shortest way, so you slide along. Jumps land
  clear too.
- Known: the backpack passes through the top of the hatch's frame as he crawls out (the real crews barely cleared
  it); no voice yet.
- Test: the last part of `test/egress.test.cjs` (his clock runs three times as fast there).

## Chapter 6: on the Moon (`surface.html`, `src/egress.js`)

Its own chapter page (the Moon page's code, `src/main.js`, with `data-chapter="6"` on the body: the surface only, no
test views). Chapter 5's A lands you here. Start in the cabin after the landing, or on the ground beside the LM
(chosen for you if you had already climbed down: the place is kept). You stand at the Commander's station in the
landed LM, the valley in the windows. The cabin is let down to vacuum and the Lunar Module Pilot (standing back,
clear of the door) swings the forward hatch in. Then, as it was done: backwards out through the hatch on hands and
knees, facing the cabin, along the porch, feet first down the ladder, the drop from the last rung to the footpad
(0.8 m, falling in the Moon's gravity), the step off the pad. Two bootprints, and the free walk is yours.
- **Moving**: grip and pull, as in the tunnel, along the one way out (a hand moved sideways does nothing); or
  **A** (F on the screen) and it goes by itself, A again stops. Setting out, the view blinks and you are turned to
  face the cabin whichever way you stood: no turning round in your room. Your own height is kept: on the ladder
  and the ground your feet are where they would be; crawling, your eyes are where a crawling man's would be.
- **The ladder** leans with the leg, and you stand upright in your room where the astronauts leaned onto it: your
  eyes' column is therefore set where the rungs at chest height are an arm's bend away (a little inside the rung
  your feet are on).
- **Drawing**: the cabin (`lm-cabin.js`) stands inside the model; it is drawn in its own pass with its own lights
  (they would flood the LM's outside). The headset keeps no depth from one pass to the next, so that pass draws the
  LM's outside too, into the depth buffer only, whenever your eyes are anywhere but inside the cabin's own box (in
  front, at the sides, behind): the hull hides the cabin, which shows through the open hatch. Your hands come in a
  last pass, lit by the Sun outside and by a fill light inside.
- **The LM's shadow** (`lm-shadow.js`, `bake`): the model itself as the Sun sees it, drawn once into a texture and
  draped over the ground on a fine grid (chapter 5 keeps the drawn silhouette, which follows the LM down).
- **Both ways**: from the ground, a grip or a trigger by the front footpad puts you on the pad at the ladder
  (`egress.mount()`; the view blinks and you face the ladder). A pull up, or A, and you jump for the last rung,
  climb, crawl in over the porch and stand up in the cabin; out again as before. On the pad, one push back from
  the ladder (or A, coming down) sets you on the ground in one gliding step, as the other astronaut does.
- **The triggers**: climbing, they grip as the grips do. Within 10 m of the LM the right trigger does not aim a
  jump ("Move further away from the lunar lander to teleport"): there it belongs to the ladder.
- **The moon walk**: walking with the stick, each stride is a small arc (0.11 m), the bobbing gait of one sixth
  of a g. On by default; "Moon walk (comfort)" on the settings page turns it level.
- **The radio** (`src/moon-radio.js`, script `tools/script/surface.json`): Houston and the Lunar Module Pilot say
  what happens (the helmets, the go for the surface, the hatch, the porch, the last rung, the first step) and what
  comes next (`setNext`). Hold **X** (T on the screen) to transmit: the key clicks, the channel hisses, and when
  you let go one of them answers in general terms ("Roger that", "Say again?"): nobody can understand you, and
  nothing you say is recorded or leaves the headset (no microphone is opened). If the answer is his and he is
  standing, he turns to you. Small talk comes every minute or two while you both walk about; walk into him and he
  says so. One line at a time; the words are shown too. The voices are the Windows voices for now.
- Test: `test/egress.test.cjs`.

## The flag and the rover (`src/surface-ops.js`, `src/flag.js`, `src/rover.js`)

What there is to do once you are both down (chapter 6 only).
- **The flag**: half a minute after he is down, the Lunar Module Pilot fetches the flag, carries it out into the
  sunlight to one side of the LM and holds it there. Take hold of the pole (a grip; G on the screen) and push it
  down, hand over hand if you like (0.32 m of pushing in all). When it is in, the crossbar swings up and the flag
  hangs out on it (there is no wind to do it), with the wrinkles of its packing; he salutes. It casts its shadow.
- **The rover** (the Lunar Roving Vehicle, in code: chassis, four wire-mesh wheels with chevrons under orange
  fenders, the two seats, console and T-handle, batteries under mirrors, the high-gain umbrella, the low-gain
  antenna, the television camera in gold foil, the pallet with tools and sample bags; about 30 draw calls). It
  stands parked on the other side of the LM. A grip by its left seat (the Commander's) seats you, and he takes the
  right seat. The left stick is the T-handle: forward and back (to 3.6 m/s), left and right to steer. It follows
  the ground on its four wheels (pitch, roll, a soft bounce; your own view stays level), throws dust from the rear
  wheels, casts its shadow, and keeps clear of the LM. A, standing still, and you step off to the left; he gets
  off and stays about the rover. Parked, it is solid to you.
- **Heard and felt through the suit** (`sound.js`: `drive`, `bump`; nothing comes through the vacuum): the hub
  motors' muffled whirr, rising with speed and with the throw of the handle; the chassis' low rumble over the
  ground; a thump and a rattle when the springs take a bump. The controllers tremble with it, the left one (the
  hand on the T-handle) more, and jolt at a bump.
- **Tracks** (`prints.js`: `track`): the rear wheels press a stretch of track for every half metre rolled, the
  wheel's shallow trough with its chevrons, shaded by the ground's own shader like the bootprints (3000 stretches
  kept on each kind of ground, some 700 m of driving; then the oldest go).
- **Getting on**: pull a trigger or grip anywhere beside the rover (within 2.9 m of its middle; it was only by
  the left seat before, and not clear). The console says so while it stands empty, and a message does the first
  time you come near.
- **The triggers** (`readControllers` in `main.js`): the pistol triggers take hold like the grips, of the flag's
  pole, a rock, the rover, the sample bag, the ladder. The right trigger jumps (teleports) only when you are 10 m
  clear of the LM, the rover, the flag and your crewmate and have nothing in a hand; nearer, a pull that took
  hold of nothing says "Cannot teleport close to other objects or astronauts".
- **Finding the way**: the console between the seats is the rover's navigation display, as the real one had:
  a dial with the rover's nose up and north marked, a large yellow arrow and the range to where you are going, a
  small blue arrow and the range back to the LM, the heading, and (which the real one had not) the samples
  loaded (`rover.userData.nav`). While the rover stands empty it says how to get on. On foot the wrist panel has the
  same: an arrow as you look (up is ahead) and the distance, to the flag, the rover, station 1, the nearest rock,
  the LM (`ops.target()`). And **station 1 is a boulder field you can see**: seven boulders, from knee-high to
  taller than you, stand there from the start with their shadows, visible from the LM some 90 m off; they are
  solid to you and to the rover (`samples.js`: `boulders`).
- The radio says each step (`flag-go`, `flag-here`, `flag-done`, `rover-go`, `drive`) and what comes next.
- **The missions**, once you have driven 30 m (Houston says each; the wrist panel shows where you are with it):
  1. *Samples* (`src/samples.js`): station 1 is a boulder field some 90 m ahead of the LM (the panel gives the
     distance and whether it is ahead, to a side or behind). The suit does not bend, so the tongs do it: a grip
     with your hand over a rock within a long arm lifts it to your hand; carry it to the back of the rover and let
     go there, and it is in the bags, weighed (9 to 17 kg each; 100 kg wanted). Let go elsewhere and it falls.
  2. *The film*: park the rover 20 to 30 m from the LM and step off. The camera on its mast turns to the LM; he goes
     to stand before it and calls you; stand by him. Nothing is kept until you say so: **A**, or a word on the
     radio (X held and let go). Then "Recording in 3, 2, 1", a red dot with the seconds left for twelve seconds
     (face the rover and wave, or whatever you like), and "Cut". The wrist panel says the same.
  3. *Home*: a grip at the back of the rover gives you the sample bag. **He goes up first** (the Commander was the
     last one on the Moon): round to the front of the LM if he is behind it, a hop onto the footpad, the jump for
     the lowest rung, up the ladder, down on his knees over the porch and in through the hatch (`lmp.climb()`).
     The ladder is his meanwhile ("Your crewmate goes up first"); when he calls that he is in, up with the bag,
     into the cabin. The hatch swings shut and is locked, Houston gives the go, he counts, and the ascent stage
     lifts off (1.75 m/s2 net), the dust blown out flat and the descent stage's foil torn loose.
  4. *The finale* (`src/finale.js`): at 50 m the view goes dark, and you are at a console in Mission Control: a
     picture of one, drawn in code (pale green cabinet, lamp buttons, number wheels; no room, no people), its tube
     drawn large. It plays the film in muted colour with scan lines, a blink of black at each cut: the valley (a
     slow turn that ends on the LM), the flag going up, a rock lifted and stowed, the two of you waving, then the
     lift-off as the rover's camera saw it, its operator a little behind as he was. Then: "You completed the first part of the
     Apollo 17 mission. Have a safe trip back." A returns to the chapters.
- **How the film is made**: not pictures but motion, and the scene drawn again. Thirty times a second 48 numbers
  are kept: where your suit stood, its turn and height, both arms and elbows as your hands had them, your legs and
  waist, his place and whole pose, the flag (where, how far in, how far out), the rock in your hand. The flag's
  scene and the first rock you stow are kept as they happen (the last 12 and 9 seconds of each); the wave when you
  say so. Played back, the motion is blended between the kept frames (`makeRecorder().sample`) and set on the whole
  suit that stands for you, on him, the flag, the rock and the rover where it stood then, and the scene is drawn
  from a camera's place into an 800 x 600 texture (the view's own two passes; not multisampled, or the headset gives
  up the first pass, the mountains and the sky, before the second): beyond the flag with the LM behind,
  to the Sun's side of you and the rover, and the rover's own camera for the wave and the lift-off. It is the same
  scene, so it shows what a camera there would have seen, movement for movement. Why not a video: frames from a
  second camera would have to be drawn, read back and encoded in the headset while you play (frame rate), and the
  result could only be coarser than this. Scenes that never happened are left out (no flag scene if you planted
  none); with no wave kept, he waves alone beside a standing suit.
- **Your own suit moves as you do** (`placeMe` in `main.js`; it is what your shadow, his visor and the film show).
  Its height follows how tall you stand, slowly; your eyes lower than that is a crouch (the knees bend, the boots
  stay under you), higher is a jump (the boots leave the ground); moving over the ground swings the legs in a
  stride. The arms follow your hands as before. Not kept or shown: the fingers and the wrists' turn (the gloves
  follow the forearms), and a bow from the waist (the suit moves under your head instead).
- **The ascent stage** is the LM's model cut off below its joint by a clipping plane that rises with it; a second
  model, cut off above, stays as the descent stage (`liftModel` in `main.js`). **The LM's shadow goes in two** with
  it: at lift-off each stage's own silhouette is drawn once (`lm-shadow.js`: `bake` with the clipping plane); the
  descent stage's stays, the ascent stage's is laid out again every frame further down-Sun (`lift(alt)`: a point
  alt higher throws its shadow alt / tan(elevation) further), in the cabin's view and in the film alike. While the
  film shows the LM standing, the whole shadow is back.
- **Your gloves in the LM's shadow** are lit by the ground's glow only, like the rest of your suit (`dimHands`): the
  shadow's kept picture is asked whether the Sun reaches each glove (two look-ups a frame, nothing drawn), and the
  glove's colours are turned down, each glove as a whole with a short fade (no shadow edge across the fingers).
- Viewer: `tools/suit.html?what=rover`, `?what=flag`, `?what=console&words`. Test: in `test/egress.test.cjs`.

## The stars

`src/stars.js`: the Yale Bright Star Catalogue (5th revised edition) to magnitude 6.5, 8,404 stars, baked by
`tools/stars.py` into `assets/stars.bin` (168 KB) from NASA HEASARC's copy (public domain), downloaded to
`%LOCALAPPDATA%\moon-data\stars`. Drawn at infinity, so no page's depth planes clip them, and placed by two
directions every page has: the Sun's (where it stood among the stars on 11 December 1972) and a pole (the Earth's
axis on the launch's pages, the Moon's on the others). A setting on every start page, remembered: **faint** (the
default, the owner's choice), **realistic** (only in the shadow of the Earth or the Moon: none in daylight), **full**,
**off**; `?stars=` in the URL. Hidden by the blue sky below 100 km on the launch.

## How the Moon is drawn

Seven nested levels centred on the landing site, each a grid mesh with one RGBA map, built offline by
`tools/build.py`:

| Level | Covers | Map resolution | Source |
|---|---|---|---|
| G | the whole Moon | 1.4 km | NASA SVS CGI Moon Kit (LOLA, 64 px/degree) |
| L1 | ±12° | 355 m | the same |
| L2 | ±4° (121 km) | 59 m | SLDEM2015 (LOLA + Kaguya), 512 px/degree |
| L3 | ±22 km | 11 m | LROC NAC terrain model of the Apollo 17 site, 5 m |
| L4 | ±5 km | 2.4 m | LROC NAC terrain model, 2 m |
| L5 | ±1 km | 0.5 m | the 2 m model plus generated craters |
| L6 | ±150 m | 7 cm | generated craters (0.3 m and up), soil relief, rocks |

Map channels: R, G = surface normal (east, north); B = the elevation angle of the terrain horizon in the direction of
the Sun; A = albedo detail. The shader compares B with the Sun's height: cast shadows with a penumbra for any Sun
height, from one texture read. A coarser level is not drawn where the next finer one lies; the finer level's border
is laid exactly on the coarser level's triangles and melts into its own data over a few cells.

Light: Lunar-Lambert reflectance (no limb darkening), a phase function with the opposition surge (the ground
brightens around the point opposite the Sun), shadows filled only by light bounced from the sunlit ground. The Sun
is where it was at the Apollo 17 landing: 13.3° high, azimuth 95.5°.

Nothing procedural runs per pixel in the headset: everything heavy is baked.

Depth: standing on the surface, the distant levels are drawn first with far clipping planes, then the depth buffer
is cleared and everything within a few kilometres is drawn with near ones. In XR the per-eye projection matrices
are patched for each pass (`setPlanes` in `src/main.js`).

## The ground round the walker

Near the lander the finest level (L6, ±150 m) was baked offline. Anywhere else in the valley a Worker makes the
same kind of ground for a square of 192 m round the walker, in about half a second on a PC:

- **Mesh**: the valley's real heights (`assets/moon/valley.dat`: the LROC NAC model at 4.9 m, bicubic) plus
  generated craters, laid exactly onto the coarser level's triangles along the square's border and round L6, so
  there is no step anywhere.
- **Map** (1024², 19 cm): the normal of the generated detail only, the horizon angle towards the Sun of that detail
  and of the rocks, and albedo detail. The shader adds the normal to the parent level's (L4, or L5 where it lies),
  multiplies the two shadow terms, and takes the albedo from the parent: so the mountains' shadows, the real
  landforms and the colour carry straight through.
- **Rocks**: instanced, lit against the square's own map and L4's (so they go dark in a massif's shadow).
- **Everything generated is a pure function of position** (seeded per 32 m tile). When the square moves with the
  walker (re-centred after 22 m), the overlap is identical: nothing pops, and bootprints stay where they were made.
- **Craters of 1.5-7 m inside L5** are the ones the build stamped into L5 (`assets/moon/craters.dat`): L5's map
  shades them already, so the worker only gives them shape and sharper shadows. Outside L5 they are generated.

The levels leave the square out (`uPatch`); the square leaves L6 out. A teleport blinks to black, waits for the
square at the target if it is not there, and blinks back in. Walking on it, rocks, bootprints and the frame-rate
panel work as by the lander. Known limits: the generated detail is coarser than by the lander (19 cm against 7 cm),
and in a band of about 25 m round L6's border only the 0.5 m data shows.

## Generated detail, and what is real

Real: every landform down to about 5 m (the massifs, the valley floor, craters larger than about 10 m).
Generated, statistically right but not the actual ones: craters below about 7 m, the rocks, the soil texture.
The 2 m terrain model is smoothed below its true resolution before the generated craters are added, because its
stereo noise turns into lumps when magnified.

## Files

| File | What it holds |
|---|---|
| `index.html` | Start page (English), import map for the vendored three.js |
| `src/main.js` | Renderer, the two-pass depth scheme, views, controls, frame-rate panel, test hooks |
| `src/moon.js` | Loads the levels, builds the meshes, the lunar shader |
| `src/extras.js` | Sun, stand-in Earth, rocks, stand-in LM, the handrail of the orbit views |
| `src/ground.js`, `src/ground-worker.js` | The generated ground round the walker anywhere in the valley |
| `src/geo.js` | Site frame against lunar latitude and longitude, mesh heights, seeded random numbers (page and worker) |
| `src/prints.js`, `src/sound.js` | Bootprints; the sound inside the suit |
| `assets/moon/` | Built data: `moon.json`, one map and one mesh per level, colour maps, rocks, walking heights |
| `tools/download.py` | Fetches the NASA source data (3.9 GB) to `%LOCALAPPDATA%\moon-data` (never into the repository) |
| `tools/build.py` | Builds `assets/moon/` from the source data. Python 3 with numpy, scipy, tifffile, imagecodecs, pillow |
| `tools/encode.py`, `tools/ktx2.mjs` | Encode the built maps to KTX2 (Basis Universal UASTC) with the WebAssembly encoder |
| `test/xr.test.cjs` | Emulated Quest 3 test (IWER, playwright-core with the installed Chrome) |
| `tools/shot.ps1` | Headless Chrome screenshot of the flat preview (PNG maps, near the lander only) |
| `tools/shot.cjs` | The same in real time with Playwright: works with KTX2 and anywhere in the valley |
| `vendor/` | three.js r170, KTX2Loader and the Basis transcoder (MIT / Apache 2.0) |

## Rebuilding the data

```
python tools/download.py          # once, 3.9 GB
python tools/build.py             # PNG maps, about ten minutes: for development
python tools/encode.py            # PNG/JPG -> KTX2 as published, about five minutes, switches moon.json to ktx2
```

The KTX2 encoder: put `basis_encoder.js` and `basis_encoder.wasm` from
github.com/BinomialLLC/basis_universal (`webgl/encoder/build`) in `%LOCALAPPDATA%\moon-data\ktx`.

Built assets are large. Commit them only when they have changed for good: every commit of them stays in the
repository's history.

## Testing

Serve the repository root (`python -m http.server 8765`) and open `/vr-apps/moon/`.

Each chapter has its own emulator test (`test/<chapter>.test.cjs`); `test/chain.test.cjs` checks that A at each
chapter's end opens the next, from the launch to the surface. With NODE_PATH set to the node_modules under
`%LOCALAPPDATA%\moon-data\node`. The software renderer runs the later chapters at a few frames a second: the tests
start close to the events they check.

URL parameters: `debug=shadow` (the baked shadows only, each detail level in its own colour: shows whether a dark
patch is a shadow and whether shadows meet across level borders), `view=orbit|low|surface`, `theta=` (degrees along the orbit before the site), `yaw=`, `pitch=`,
`x=`, `z=`, `heading=` (surface position and facing, anywhere in the valley), `trail` (bootprints ahead), `sun=` (degrees of lunar rotation after landing, 0.51 per
hour), `exposure=`, `scale=` (VR resolution), `still` (no motion), `shot` (hide the panel).

`tools/shot.ps1` (headless Chrome, virtual time) only works with the PNG maps: the KTX2 transcoder runs in
workers that virtual time does not wait for. Set `"ext": "png"` in `assets/moon/moon.json` for screenshots, and
back to `"ktx2"` before committing. `test/xr.test.cjs` runs in real time and works with either; it enters VR in
an emulated Quest 3 and checks both eyes in all three views.

Hooks on `window.__moon`: `S` (state), `setView(name, theta)`, `setSun(deg)`, `look(yaw, pitch)`,
`stand(x, z, heading)`, `teleport(x, z)` and `teleporting`, `prints` (`count`, `add`, `trail`), `suit` (`step()`),
`ground` (`count`, `ms`, `rocks`, `busy`, `detailed(x, z)`, `request(x, z)`).

In XR, read the head from `camera.matrixWorld` after `renderer.xr.updateCamera(camera)`; the XR array camera
(`renderer.xr.getCamera()`) reports the world origin from `getWorldPosition`.

## Sources

- Crew faces: "Infinite, 3D Head Scan" by Lee Perry-Smith, licensed under Creative Commons Attribution 3.0
  Unported (https://creativecommons.org/licenses/by/3.0/), based on a work at www.triplegangers.com; taken from the
  three.js examples (`vendor/head/`, licence file beside it). Modified: cut below the neck, eyes opened, tinted.
- Hands: the WebXR generic hand model (@webxr-input-profiles/assets, MIT), `models/`, licence file beside it.

All NASA / USGS, public domain. No NASA insignia are used.

- NASA Scientific Visualization Studio, CGI Moon Kit: https://svs.gsfc.nasa.gov/4720 (colour: LROC WAC; elevation: LOLA)
- SLDEM2015, Barker et al. 2016: http://imbrium.mit.edu/DATA/SLDEM2015/
- LROC NAC digital terrain models `NAC_DTM_APOLLO17` and `NAC_DTM_APOLLO17_2M` (NASA/GSFC/Arizona State University):
  https://data.lroc.im-ldi.com/lroc/view_rdr_product/NAC_DTM_APOLLO17
- Sun position at landing: Apollo 17 traverse planning data (Apollo Lunar Surface Journal)

## Chapter 7: the lift-off (`liftoff.html`)

The first part of the journey home: from thirty seconds before the ascent engine lights to orbit insertion, standing
at the Commander's station. `src/liftoff-space.js` simulates the flight (fixed thrust, ten seconds straight up, the
pitch-over, cut-off 18 km up in a 16 by 89 km orbit); `src/liftoff.js` is the page, made from `src/descent.js`. The
descent stage stays on the ground: two copies of the LM model, each cut at the joint. Voices: `tools/script/liftoff.json`
(`as01` to `as16`). Pictures: `tools/shot-liftoff.cjs "t=667303&out&od=30" out.png`. Test: `test/liftoff.test.cjs`.
Not to be confused with `src/ascent.js`, the Saturn V's ascent of chapter 1.
