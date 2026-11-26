# Journey to the Moon: handover (26 November 2026)

For whoever picks this up next (a new Claude session, or the same one after its context was compacted). It says
where the work stands, how the owner wants it done, how to test, merge and publish, and what has gone wrong before.
The design and its history are in `MOON-DESIGN.md` (section 12 is the build log, 12.9 to 12.16 hold the owner's
headset verdicts and what came of them); how each part works is in `vr-apps/moon/README.md`. Read those for detail;
this file is the short way in.

## 1. What it is

A realistic Apollo 17 experience for the Quest 3 browser, in `vr-apps/moon/` of the repository
`peterboggild/BrokildApps`, published by GitHub Pages at
`https://peterboggild.github.io/BrokildApps/vr-apps/moon/`. The owner's direction, which decides every open
question: **realism and immersion, "authentic but also allowing curiosity"**.

Plain JavaScript modules on a vendored three.js r170, WebXR, no build step. The owner tests in the headset; the
assistant can only test on the flat screen and in an emulator.

## 2. Where it stands

Live on main after PR #148 (chapter 8, the rendezvous and the docking), with PR #149 (chapter 9, leaving the Moon,
12.22 of `MOON-DESIGN.md`) open at the time of writing. All moon modules carry `?v=35` (`prints.js` its own `?v=4`,
`lm.js` and `liftoff.js` `?v=36` in their pages); `moon.html` and `surface.html` load `src/main.js?v=28`. Check
`git log` for what has landed since. The owner gave a standing yes on 26 November to merge each finished, tested
piece of the journey home and go on.

| Chapter | Page | State |
|---|---|---|
| 1 Briefing | `briefing.html` | done |
| 2 Launch | `launch.html` | done, headset-checked by the owner (12.9) |
| 3 Earth orbit | `orbit.html` | done, headset-checked |
| 4 To the Moon, docking | `lunar.html` | done; the docking's spacing was noted as off, not fixed |
| 5 Descent | `lm.html`, `descent.html` | done, headset-checked |
| 6 On the Moon | `surface.html` (`src/main.js`) | complete to its finale; the owner has played it to the end |
| 7 The lift-off | `liftoff.html` (`src/liftoff.js`) | to orbit insertion; emulator only, not headset-tested |
| 8 The rendezvous | `rendezvous.html` (`src/rendezvous.js`) | to your couch in America; emulator only |
| 9 Leaving the Moon | `leaving.html` (`src/leaving.js`) | the LM cast off, the burn for home, the Moon behind; emulator only |

Chapter 6 as it now runs: suiting up in the cabin (helmets go on), out through the hatch and down the ladder, the
Lunar Module Pilot (LMP) follows and lopes about, radio (push-to-talk on X; no microphone is opened), the flag, the
rover, 100 kg of rock samples, the film (recording starts on A), the sample bag, the LMP climbs the ladder first,
you follow, lift-off, and at 50 m the view goes to a console in Houston that plays the film (the valley, the flag,
a rock, the wave, the lift-off) and ends with "You completed the first part of the Apollo 17 mission. Have a safe
trip back."

The owner has driven the rover and planted the flag in the headset ("fantastic work on the rover and flag
experience") and entered the LM in chapter 4; his notes from those runs are sections 12.17 and 12.18 of
`MOON-DESIGN.md`. He has since played chapter 6 to its end ("I love the ending") and seen the tunnel right on a
fresh load; his notes are 12.19. Not yet judged by him: the panels and the view reset of #143, and #145.

### To check in the headset (not yet answered)

- Frame rate near the LMP (his visor is a live mirror), at lift-off, and while the film plays.
- Radio volume, and whether X as push-to-talk feels right.
- The ladder upward, the single push off the footpad, Moon Walk comfort.
- The flag's grip, driving comfort and seat height on the rover, the reach when picking up rocks.
- Crouch, jump and stride in your own shadow and in his visor; your own legs seen from above.
- The countdown and the red dot at the recording; the wait while the LMP climbs (about 25 s from the footpad).
- The film: each scene's framing (the flag and rock scenes most), its length (about a minute), the console's size.
- Gloves dimmed in the LM's shadow.

### Known gaps

- Fingers and the wrists' turn are not in the film (the glove follows the forearm); a bow from the waist shows as
  the whole suit moving.
- The LMP's backpack passes through the top of the hatch frame (the owner: "ok").
- All voices are Windows text-to-speech placeholders; the owner's own recordings replace them file for file.
- No full real-time playthrough has been run by the assistant; pacing and total length are unjudged.
- No picture of the film's rock scene has been looked at (the test covers that it plays).
- Hands are dimmed as a whole (no shadow edge across the fingers).

## 3. How the owner wants the work done

These are standing rules. They came from him, in his words where quoted.

- **Branch, pull request, merge only on his explicit yes for that pull request.** `git fetch` before branching.
  He has given blanket go-aheads twice ("Just do it (this time)" for #135; "merge when you have done it" for
  #136 to #139) and a yes for #140 ("open PR, merge and publish when you are ready"). Each covered only what it
  named. New work needs a new yes.
- **Pull request title and body in Danish; commits and code comments in English.** Commits end with the
  `Co-Authored-By:` line the session's instructions give; pull request bodies end with
  `🤖 Generated with [Claude Code](https://claude.com/claude-code)`.
- **Be honest about testing.** Flat screen and emulator only, never the headset. Every pull request lists what he
  should check in the headset and what is not done.
- **Voice.** He likes a spoken note (Windows text-to-speech) when something is live or he is needed; "dont speak -
  just flag it here" when he is in a meeting.
- **Assets.** No audio, images or models he did not supply, except: NASA public-domain data, the CC BY head scan,
  and Windows text-to-speech placeholder voices. He supplied a Mission Control console photo as inspiration only
  (the console is drawn in code). The NASA patch and the flag on the surface suits are approved ("It would not be
  historically correct without"); the LM carries the flag and "UNITED STATES", no NASA logos.
- **Launch and descent stay bare-headed** (faces visible); helmets go on in the LM cabin before the walk.
- **His email and project details are private**: never in requests to outside services.
- **Do not work round a permission or safety denial.**

**Messages on screen are off by default** (a setting in every chapter): he finds them distracting. A new message
must earn `toast.say(text, secs, true)` (shown whatever the setting) by being an answer to the player's own
button or something the player cannot do without; otherwise it is a hint and stays off.

Comfort and control decisions he made: the pistol triggers take hold like the grips whenever something is in
reach, and jump only when 10 m clear of the LM, the rover, the flag and the crewmate (nearer: "Cannot teleport
close to other objects or astronauts"); the "Moon Walk" hopping gait is on by default and
switchable in the settings' comfort section; the film is redrawn motion, not video ("You decide"; decided in
12.16).

## 4. The code, briefly

`vr-apps/moon/src/`, all sharing one `?v=N`:

| File | What |
|---|---|
| `main.js` | The surface page: render passes, the player's own suit (`placeMe`, `ghostArms`, `shadowMe`), collisions (`clearOf`, `collide`), `reflect` (visor cube map), `liftModel` (ascent stage, split shadow), `filmPass`, `dimHands`, `mountLadder`, test hooks on `window.__moon` |
| `egress.js` | Cabin, hatch, ladder both ways (`mount()`), helmet beat |
| `eva-suit.js` | The suit, jointed (`userData.joints`, `.pose()`), `makeHelmetView` |
| `lmp.js` | The LMP: out and down, roaming, `command`, `ride`, `climb()` back up, `snapshot`/`pose` for the film; also `makeFigureShadow`, `makeDust` |
| `lm-shadow.js` | The LM's baked shadow, `lit(point)`, `bake(…, { clip, read })`, `lift(alt)` |
| `surface-ops.js` | Flag, rover, missions, recording (`clips`, `keep`, `pose`), the film's plan and playback (`plan`, `enter`, `film`), steps `wait … finale` |
| `samples.js`, `flag.js`, `rover.js` | The rocks, the flag, the rover |
| `finale.js` | The Houston console, `makeRecorder` (`add`, `at`, `sample`) |
| `moon-radio.js` | Radio cues, push-to-talk answers, small talk, the engine's roar |
| `lm-model.js`, `lm-cabin.js`, `hands.js`, `settings.js` | The LM outside and inside, the gloves, the settings |

Frames: the site is x east, y up, z south, origin at the LM. The LM's frame is y up, +z forward, +x the
Commander's side. Suits and the rover face -z; +x is the wearer's right; a positive turn about x swings a hanging
limb forward.

Voices: every chapter's files share `assets/script/voice/`. Scripts are in `tools/script/<name>.json`; surface
ids start with `sf`. Run `pwsh -File tools/tts.ps1 surface.json`, then `git checkout -- assets/script/voice`
(the tool re-encodes every line; this keeps the existing files unchanged and leaves the new ones).

## 5. Rendering facts that cost time to learn

- The Quest discards depth between `renderer.render` calls and has no stencil: `clearDepth()` before every pass.
  The cabin pass draws the LM hull (and the LMP) to depth only (`colorWrite = false`).
- Rendering to a texture inside an XR frame: `renderer.xr.enabled = false`, `setRenderTarget(rt)`, render, restore.
- **A multisampled render target loses its picture between two render passes in a WebXR session.** The film (far
  pass, then near pass) had no mountains, stars or sky in the headset from #139 until #140; on the flat screen it
  looked right. Anything drawn into a texture in more than one pass must not be multisampled, and must be checked
  in the XR emulator, not only flat.
- A thing placed from `camera.matrixWorld` inside `frame()` uses last frame's pose; place it in `render()`.
- Clipping planes are in world space. The LM stands tilted on the ground: a plane meant to cut it along its own
  deck must be made from its world matrix (`setClips` in `main.js`), not from a height.
- State set on the way through a chapter must be reset where the chapter is started again on the same page
  (`readOptions` in `lm.js`): a fresh load hides such faults from the tests.
- `Object.assign` copies a getter's value once; use `Object.defineProperty` for live values.
- In XR read the head from `camera.matrixWorld` after `renderer.xr.updateCamera(camera)`.

## 6. Testing

Serve the repository root on port 8765 (`python -m http.server 8765` there). Set
`NODE_PATH=%LOCALAPPDATA%\moon-data\node\node_modules` (Playwright-core and IWER live there; the installed Chrome
is used with SwiftShader).

```
cd vr-apps/moon
node test/egress.test.cjs <folder for pictures>    # chapter 6, 39 checks, about 40 minutes
node test/liftoff.test.cjs <folder for pictures>   # chapter 7, 19 checks, about 6 minutes
node test/rendezvous.test.cjs <folder>             # chapter 8, 17 checks, about 10 minutes
node test/leaving.test.cjs <folder>                # chapter 9, 14 checks, about 5 minutes
node test/xr.test.cjs                              # 12
node test/lm.test.cjs                              # 14
node test/descent.test.cjs                         # 20
node test/chain.test.cjs                           # 5
```

- The emulator runs at about 2 frames a second. Wait on state, never on time. A test player never moves by
  itself, so anything that waits for the player to step aside hangs. Speed-ups: `lmp.S.rate`, `ops.rate`;
  shortcuts: `ops.jump('samples' | 'film-go' | 'load' | 'aboard')`.
- **Run the suites one at a time.** In parallel they give false failures on the timing checks.
- A browser that closes mid-run, or a run that stalls with no page error, has been the PC short of memory.
- Flat-screen looks: `surface.html?walk&shot` starts on the ground without the panel. `__moon.filmPass()` draws
  the film once into `__moon.filmRT` (800 x 600), which can be read back with `readRenderTargetPixels`.
- Viewer for single things: `tools/suit.html` (`?what=rover|flag|console&words`).

## 7. Changing, merging, publishing

1. `git fetch`, branch from `origin/main`.
2. Edit. Files are CRLF with very long lines; the reliable way has been small Python patch scripts in which every
   anchor must occur exactly once and the file's own line endings are kept, then `node --check` on a `.mjs` copy.
   A replace that inserts after an anchor must put the anchor back. Write JavaScript files with the editor tools,
   not shell heredocs (they eat backslashes).
3. Bump the version everywhere at once: `v=N` in `src`, the html pages and `tools/*.html`, and `main.js?v=M` in
   `index.html` and `surface.html`.
4. Run the suites; update `README.md` and `MOON-DESIGN.md` (a new 12.x section for what the owner said and what
   came of it).
5. Commit, push, open the pull request through the GitHub REST API (`POST /repos/peterboggild/BrokildApps/pulls`)
   with the token from `git credential fill`. The `gh` command is not set up.
6. **Wait for his yes.** Then `bash vr-apps/moon/tools/merge.sh <pr> "<title>" "<message>" <file to check>`: it
   waits for the checks, squash-merges, waits for the Pages deployment and fetches the file from the live site.
7. Check the live `surface.html` serves the new `main.js?v=M`; `git checkout main && git pull`.

## 8. What comes next

Waiting for: his headset verdict on #143 (panels, view reset), #145 and chapter 7.

**The journey home** is under way. Agreed with the owner: 7 the lift-off, 8 the rendezvous and docking, 9 leaving
the Moon (all three done), 10 the coast home with the deep-space walk, 11 re-entry, 12 parachutes and splashdown
without helicopter and ship. Next: chapter 10, from `orbit.js` (chapter 2, the coast out, run the other way) with
the walk outside as its new part.

| Part | What it takes | Size |
|---|---|---|
| Ascent to orbit, docking with the command module | Reuses the LM cabin, lunar orbit, chapter 4's docking | Medium |
| Transfer, LM jettison, the burn for home | Reuses the command module cabin | Small to medium |
| The coast home | Earth growing; the deep-space walk for the film canisters is optional | Small, medium with the walk |
| Re-entry | New: plasma in the windows, g-load, radio blackout | Medium to large |
| Parachutes | New: drogues, mains, sky and ocean | Medium to large |
| Splashdown and recovery | New: ocean, the capsule bobbing; helicopter and ship only if wanted | Medium, large with the ship |

The first three are mostly reuse. The risk is in the last three: the plasma and the ocean are new rendering work
that must hold frame rate on the Quest.

Other open items, none asked for now: his own voice recordings; fingers and wrists in the film; chapter 4's
docking spacing; carrying the 17 degree hand angle over to Portalkniven; a help badge in every sim (a separate
plan).
