# VR apps: what a new session needs to know

**Start with `BROKILD-VR-HANDOVER.md` in the repository root**: the full handover (state, history, controls, open
questions, ideas offered but not built). This file holds the standing rules, the test recipe and the pitfalls.

Quest 3 passthrough WebXR apps, plain JavaScript modules, three.js r170 vendored in each app (`vendor/`), no
build step. Each app's own `README.md` (Danish) documents its design in detail; this file is the handover:
rules, testing, architecture in brief, and pitfalls.

| App | Folder | State |
|---|---|---|
| Mutant Rotte Angreb | `tank-ambush/` | Finished and live. Difficulty Begynder/Let/Mellem/Svær; the cheese bomb (#75). |
| Portalkniven | `portalkniven/` | Live, in active development. Everything below is about this one. |
| Guldplaneten | `guldplaneten/` | Not touched in the Portalkniven sessions. |
| Bordbyen | `bordbyen/` | New (#78): draw a toy town on the real table, shrink down and race. See its README (Danish); hooks on `window.__bb`, flat preview `?flat` (`&new` for a fresh town). |
| Illusion Studio | `illusion-studio/` | Live, tag "Test" (17000bc). One HTML file, three.js 0.164.1 + three-mesh-bvh from jsDelivr (not vendored, unlike the others). 3D scenes clad in self-moving optical illusions, JPG export, VR and mixed reality. Not yet tried in a headset. The owner's master copy is on the owner's PC: say so before changing it here. Emulator test in `illusion-studio/tests/`. Its handover is §15 of `PLANET-STUDIO-DESIGN.md`. |
| Planet Designer | `planet-designer/` | **M0–M5 built** (calculator and ledger; desktop 3D: star, orrery with draggable planets and HZ slot, LOD procedural planet with cutaway; seasonal EBM in a worker, XUV/shoreline/radiation, scattering atmosphere and clouds; explanation cards for every ledger row and the cause map; VR workshop (`scene/workshop.js`, `scene/xr.js`, `ui/vrpanel.js`) tested in emulated Quest 3 (`test/xr.test.cjs`), not yet in a real headset). Shared terrain function in GLSL (`scene/glsl.js`) and JS (`physics/terrain.js`): change both; JS noise uses `Math.fround(1/7)` to match float32. Specification and handover: `PLANET-STUDIO-DESIGN.md` in the repository root (the working title was Planet Studio); owner's decisions in its §14; state and simplifications in the app's README. Tests: `node test/physics.test.mjs`, `test/flat.test.cjs`. On the Brokild VR page as a "Test" card since M2 (owner asked to follow the working version). |
| Lysrytme | `lysrytme/` | Demo, tag "Test" (Nov 2026). Light flickering at 2–40 Hz (brain-wave entrainment, like light glasses), plain WebGL2/WebXR without three.js: one full-screen shader per eye from view-space ray directions. Asks for 120 Hz; frequencies exact when 120/f is whole. Light's phase comes from the audio clock shared with a pulse oscillator (`sound.js`). Sound is Sleeper Agent's engine, now shared: `health-apps/sleep-noise/sound-engine.js` (used by Sleeper Agent itself too; change it with both in mind), rendered in `sound-worker.js`. Hooks `window.__lys`; test `test/xr.test.cjs` (IWER). Epilepsy warning must stay. README (Danish). Not yet in a headset. |
| Journey to the Moon | `moon/` | **Stage 0 built** (Nov 2026): the Moon test, three views (orbit 110 km, low pass 15 km, standing in Taurus-Littrow) of one Moon baked from NASA LRO data by `tools/build.py` (source data in `%LOCALAPPDATA%\moon-data`, never in the repository). Realistic Apollo-style flight: pad, ascent, coast with time warp, LM descent with optional P66 takeover, moonwalk. All English (owner's decision, unlike the other apps). Design, decisions, stages and what stage 0 settled (§12.2): `MOON-DESIGN.md` in the repository root; how it works: the app's README. Owner's binding direction: realism and immersion, no toy look or game controls. Hooks `window.__moon`; emulator test `test/xr.test.cjs` (IWER, installed Chrome via playwright-core); screenshots `tools/shot.ps1`. Stage 1 (`launch.html`, chapter 1, hooks `__launch`, `test/launch.test.cjs`), briefing (`briefing.html`) and stage 2 (`orbit.html`, chapter 2: orbit, TLI, docking, coast; `src/space.js` trajectory, hooks `__orbit`, `test/orbit.test.cjs`, `tools/shot-orbit.cjs`) and stage 3 part 1 (`lunar.html`, chapter 3: LOI, Earthrise, DOI, low pass; `src/lunar-space.js` in stage 0's site frame, hooks `__lunar`, `test/lunar.test.cjs`, `tools/shot-lunar.cjs`) and stage 3 part 2 (`lm.html`, chapter 4: grab-and-pull through the tunnel, LM cabin `src/lm-cabin.js`, undocking; hooks `__lm`, `test/lm.test.cjs`, `tools/shot-lm.cjs`) and stage 4 (`descent.html`, chapter 5: guided descent `src/descent-space.js`, P66 by hand, shadow and dust; hooks `__descent`, `test/descent.test.cjs`, `tools/shot-descent.cjs`; built on Opus, wants a Fable pass) built (§12.3–12.7). Attitude rate capped at 1.5°/s for comfort (a setting). After the owner's first headset run (§12.9): living crew (`src/crew-life.js`, `src/chatter.js`, close-up `tools/crew.html`), controllers by handedness (`makeControllers` in `src/hands.js`), the panel on the left stick press and settings (`src/wrist.js`, `src/settings.js`), fast forward (`src/clock.js`: B faster, A slower, small clock badge), Continue (`src/progress.js`), grabbable things (`src/floaters.js`); X cheat window, Y outside; test `test/cabin.test.cjs`. Stage 5 part 1 (§12.10): the detailed LM in code (`src/lm-model.js`, viewer `tools/lm.html`), forward hatch opening inside; part 2: the egress, now chapter 6 (`surface.html` = `src/main.js` with `data-chapter="6"`, `src/egress.js`, `test/egress.test.cjs`); next the rover and the flag. Anything skinned or computed in world space breaks far from the origin (chapter 3 starts 66,000 km out): keep GPU values local. |

`vr-apps/index.html` is the public "Brokild VR" page (English) with a card per app; the site's front page
(`index.html`) mentions them in one paragraph. After changing a page's title or description, or adding a page,
run `node tools/sync-site.js` (it writes the social tags and `sitemap.xml`; CI runs it with `--check`).

## Working rules (set by the owner)

- **Danish, plain language.** Short answers that lead with what he needs to know or decide.
- **Branch, PR, wait.** Never merge without an explicit yes ("ja, merge", "jatak"). After merging, check the
  Pages run (`gh api -X GET repos/peterboggild/BrokildApps/actions/runs -f per_page=3`) and report that it is live.
  The live site cannot be fetched from a cloud session; the workflow result is the evidence.
- **Say when you are done.** Send a notification when requested work is finished or live.
- **Be honest about testing.** Every report says: tested in the flat preview / not tested in the headset, and
  lists what he should look for in the headset (frame rate, stereo, comfort, feel).
- **When he asks for a design choice, offer options** as a short question with choices, not an essay.
- **Say no when something will not be convincing.** He asks for that explicitly ("giv mig et nej"). A scoped yes
  with the unconvincing part left out, and said so, is what he wants.
- **Cache busting:** `index.html` loads `portal.js?v=N`, and `portal.js` imports every module (`worlds`, `story`,
  `hands`, `ship`, `ambience`, `combat`) with the same `?v=N`. Raise them all together on every change, or the
  headset keeps old code. Mutant Rotte Angreb: `index.html` loads `game.js?v=YYYYMMDDx`; bump it (and the
  `models.js?v=` import in `game.js` when the models change).
- Code comments and commit messages in English; everything the player or the owner reads in Danish.
- He corrects wording himself; when given Danish text for the game, fix obvious typos and tell him which.
- The band's logos (`portalkniven/img/`, masks made from images he sent) stand on a slab in each song dimension.
  He asked for them to be discreet and in the world's own style; keep it that way.
- Music: the three tracks in `portalkniven/music/` are his own. An earlier set was removed from `main` (history
  rewritten at his request). Do not add audio files he has not supplied.

## Testing without a headset

- Serve the repo root: `python3 -m http.server 8765`, open `/vr-apps/portalkniven/?flat` (story),
  `?flat&free` (free play), `?flat&nohelp`. Also `?sharp=taa|res|off`, `?tilt=deg`, `?skin=wire`.
- Playwright with Chromium and software GL:
  `--use-gl=angle --use-angle=swiftshader --enable-unsafe-swiftshader --ignore-gpu-blocklist --no-sandbox`.
- Software rendering is very slow (seconds per frame in the heavy worlds). **Wait by frames
  (`requestAnimationFrame`), never by time**, and run long tests in the background. A screenshot takes several
  frames, so anything timed must be frozen first (see `VX.pause`).
- Test hooks on `window.__portal`: `S` (state: `cur`, `oth`, `placed`), `placeDirect()` (a door 1.6 m ahead),
  `walk(x,y,z)` (move the head and check for crossing the door), `view(x,y,z,yaw,pitch)`, `hand(x,y,z)` (put a
  hand somewhere; `hand(0,-5,0)` to take it away), `click()`, `hold(v)`, `tip(x,y,z,pressed)` (the knife point),
  `cycleOther(dir)` (with the spiral), `setOther(i)` (at once), `dissolvePortal()`, `igniteAt(...)`, `VX`
  (the spiral: set `VX.pause = true` and step `VX.t`), `SPN`, `FC`, `keys`, `story`, `worlds`
  (`worlds.W`, `worlds.rods`, `worlds.addScan(...)` to fake a room scan).
- After `view()` wait a frame before `placeDirect()`: the door is placed from the head's orientation, which is
  only updated in the loop.
- Look at the screenshots. A test that prints numbers is not enough for a visual effect.

## Portalkniven in brief

Seven files: `combat.js` (a test: alien bugs living in the fractal's own space, see the README), `ambience.js` (the fractal dimensions' soundscapes and the ship's engine, all synthesised, spatial, with
reverb; headless Chromium needs `--autoplay-policy=no-user-gesture-required` to run any of it), `ship.js` (the optional third-person spaceship for the fractal dimensions and deep space, built in
code from the owner's drawings of the ship, nine views; an earlier attempt ported his Blender script
`models/spaceship_model.py`, which did not match the drawings: when he sends reference pictures, they are the
brief. Surfaces lofted by hand come out inside-out easily: check the winding against the outline pass), `hands.js` (the gloved hands round the controllers: the WebXR generic hand model from
`@webxr-input-profiles/assets`, posed by our own forward kinematics because its joints are flat, not a hierarchy;
the model is laid out in grip space already, which confirmed the axes; never seen in a headset from here), `portal.js` (renderer, knife, cutting, the door, sound, controls, the loop), `worlds.js` (the
dimensions, the room scan, fractals), `story.js` (the story, `createStory(c)`).

- **The door is a stencil window, not a texture.** A mask disc writes stencil 1; a full-screen "fill" quad
  (stencil == 1, depth Always) resets depth and paints the other world's sky; the other world's objects are
  drawn where stencil == 1 and clipped to the far side of the door's plane. Render order: home 0, home sky 5,
  mask 10, fill 20, other 30, spiral overlay 40, ring 45. Walking through swaps `S.cur` and `S.oth`.
  World 0 is the real room: alpha 0, passthrough.
- **Opaque and transparent objects are sorted in separate lists** (opaque first). `renderOrder` only orders
  within a list. Anything that must come after the other world's particles has to be `transparent: true`.
  The home world's materials are stencil-tested NotEqual 1 (and the other world's Equal 1), so nothing
  transparent from the world you stand in is laid over the door's other side; over the real room it punched dark
  squares in the camera picture (stars in deep space). Anything new and transparent in a home world needs the same.
- **Dimensions** (`DIMS`): 0 stue, 1 groen, 2 metal, 3 klippe (the three "song dimensions", one music track
  each), 4 rum (deep space), then the fractals (apollo, klein, hyper, menger, boks, kali) and `fire`, the
  four-dimensional room (`fract: true, four: true`: it rides the fractal pipeline with its own shader
  `FOUR_FS` and its own controls; also a button of its own on the start page, `?flat&four`).
  Transparent things in it (the glass Klein bottle, `fourGlass`) are meshes over the march: they undo the slice
  (`transpose(uR4) * (p - uC4)`) themselves and hide behind the room by reading its distance from the march picture's alpha. Space and fractals are `bare` (no surfaces of their own: the room
  shows as a faint outline and opens to passthrough near head and hands) and only reachable in free play or once
  the story is over (`story.fractalsOK()`). Indices are not hard-coded elsewhere; look worlds up by flag.
- **Surfaces** are procedural in world space: `worldMat(i, surfGLSL, opts)` patches `MeshStandardMaterial`
  through `onBeforeCompile`, with lookups in one tileable 256² noise texture. Sky shaders are plain
  `ShaderMaterial` quads and are **not tone mapped**; world materials are.
- **Room scan** (`mesh-detection`): `addScan`, an 8 cm occupancy grid (`nearest`, `columnTop`, `lineClear`) and a
  16 cm walkability flood fill (`reachable`). Everything placed for the player (story parts, the time machine,
  touchable things) is validated against it. With no scan, everything is allowed.
- The fractals' distance estimators exist twice: in GLSL (`FRACT`) and in JavaScript (`DE` in `worlds.js`, used by
  `worlds.fractDist` for the ship's collisions and for echoes). Change one, change the other.
- **Fractals** are ray-marched into small per-eye render targets and blitted behind everything, with a
  frame-rate guard on the picture size and optional temporal sharpening at rest. Fractal units and the room are
  tied by `toF`; flying moves the fractal, not the player.
- **The spiral** when the stick changes the world behind the door: the window cannot be bent, so a still picture
  of the far side (`worlds.snapshot`, a render target flagged `isXRRenderTarget` so it uses the same shaders and
  tone curve as the screen, nothing to compile) is laid on the door and wound up by `vortexMat`.
- **Touchable things** (`rods` in `worlds.js`): damped springs hinged at the foot or hung from the top, pushed out
  of a hand-sized ball; grass leans in the vertex shader (`TOUCH_VS`). No physics engine, by decision.
- **Story phases:** wait, letter, knife, play, jump, past, placed, back, free. Parts appear one at a time; the
  door closes by itself 10 s after crossing; the stick only chooses a world from the real room until free play.
- **Safety is not optional:** the proximity warning tone, the passthrough opening near head and hands, and the
  warning text stay on. The owner made the safety fade mandatory.

## Pitfalls already paid for

- A portal window flashes the world you are leaving in the last centimetres: the near plane cuts the mask, and
  one eye crosses before the other. Hence `maskFull` (the window is the whole view on the way in) and no mask
  for the first centimetres after the swap. Anything that must survive that (knife, hands) draws after the door.

- The fractal's layout yaw must equal the door's yaw (`w.fr.yaw = yaw`). It was negated for a long time, which
  only showed with a bounded fractal (the KaliBox could not be found unless the door faced along z).
- A positive-scale Mandelbox tiled into a grid is a noisy solid mass; the negative-scale one (-1.8) is the
  classic carved cube. Look at a candidate fractal from several places before adopting it (`k1.js`-style test:
  set `fractState(i).off` and take screenshots).
- The fractal picture's resolution follows the frame rate (`FR.scale`), so anything that costs time every frame,
  even audio, shows up as blurrier fractals. The owner noticed after hands, sound and combat were added. Web Audio
  HRTF panners and long convolvers are not free on a Quest: free every placed sound when it ends, cap how many
  play at once, keep reverbs short. Hide idle effects. Compare fractal pictures against an older commit
  (`git worktree add` plus a second local server) before claiming nothing changed.
- Soft shadows marched with few, clamped steps draw terraces on plain walls. Let the step be the distance itself.

- A trailing `//` comment inserted mid-line once commented out the rest of the line (the letter never appeared). It
  happened again in `ship.js` (the whole module failed to load). Use `/* */` inside a line, and syntax-check every file
  after every patch, including one-line ones.
- Replacing a block of `index.html` dropped unrelated controls. Check what a replaced block contained.
- Patch scripts that abort half-way leave files half-edited: assert every anchor is found exactly once before
  writing, and syntax-check after (`cp x.js /tmp/x.mjs && node --check /tmp/x.mjs`).
- `a.add(b).sub(f(a))` evaluates after `a` has changed. Compute the second term first.
- ACES tone mapping turns bright pale blue to turquoise, and the knife's studio environment map has a green panel
  that tints anything it lights. A flat, drawn-looking object wants `toneMapped: false` and `envMapIntensity: 0`.
- A reversed `smoothstep` edge order and negative emission produce red lines and black holes.
- Test hooks that teleport a hand produce huge velocities; hand speed is clamped for that reason. Keep it.
- Resetting the branch to `origin/main` once lost an unmerged README note. Check `git log origin/main..` first.
- After a PR is merged (squash), the branch is restarted from `main` under the same name with a
  force-with-lease push; a plain push is rejected.
- `atan` seams with mip-mapped lookups draw a line across the sky: use `textureLod(..., 0.0)` there.
- Thresholding fine noise gives speckle, and Worley cell borders used as "dark filaments" look like cracked
  glass. Both were tried in the Milky Way and removed.

- **Planet Designer crashed a real Quest 3** (M5, first headset test): the live terrain shader evaluates the whole
  height function (about 30 noise calls) for every pixel; with the planet 3.5 m away in both eyes the GPU went down,
  and afterwards the browser refused WebGL for the site until restarted. In VR and at "low" quality the planet is now
  baked into equirectangular textures, 16 rows per frame (`bakeStep` in `scene/planet.js`), and drawn on a plain
  sphere; clouds likewise; the Milky Way is per-vertex. Budget any per-pixel procedural work for two Quest eyes.

## Waiting on headset feedback

Frame rate in the fractals and with the temporal sharpening; whether the spiral reads well in stereo (the still
picture is flat while it winds); comfort when turning in deep space; star shimmer; the four-dimensional room (frame rate, sharpness of the
textures at the small march resolution, whether pulling and turning read as 4D); how the touchable things
feel; the knife's tilt in the fist (`?tilt=`).

## Saved, not built

"Drone option" for the fractal dimensions: described at the end of `portalkniven/README.md`.
