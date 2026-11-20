# Brokild VR — handover

> **Til Peter (dansk resumé).** Dette dokument er overleveringen til en ny Claude-session, f.eks. på din anden konto.
> Start en ny session med repoet `peterboggild/BrokildApps` valgt og skriv fx: *"Læs BROKILD-VR-HANDOVER.md og
> vr-apps/CLAUDE.md, og fortsæt med Portalkniven."* Alt, hvad der er bygget, ligger i `main` og er live; der er ingen
> åbne pull requests. Arbejdsreglerne (dansk, PR og vent på dit ja, sig til når noget er færdigt, ærlig om test) står
> her og i `vr-apps/CLAUDE.md`, så den nye session følger dem uden at du skal gentage dem.

This file is for the next Claude session (on any account) that continues the VR work in this repository. Read it
together with `vr-apps/CLAUDE.md` (the standing rules, test recipe and pitfalls, which Claude Code loads by itself)
and the two games' own `README.md` files (Danish, detailed design notes, newest sections at the bottom).

State at the time of writing (20 November 2026): **everything is merged into `main` and live**, last merge PR #75,
commit `04228fc`. No open pull requests, nothing half-done on any branch.

---

## 1. The owner and how he works

- Peter, Danish. **Answer in Danish**, plain language, short; lead with what he needs to know or decide. He is not
  a programmer by trade: explain what a thing does, not how, unless he asks.
- He tests on a **Meta Quest 3** and reports back. Nothing here can be tested in a headset from a cloud session.
  **Every report says what was tested in the flat preview and what could not be tested in the headset**, and what he
  should look for there (frame rate, stereo, comfort, feel, sound).
- **Branch → pull request → wait for his explicit yes** ("ja", "jatak", "flet", "merge"). Never merge on your own.
  After merging, wait for the Pages workflow and tell him it is live:
  `gh api -X GET repos/peterboggild/BrokildApps/actions/runs -f per_page=8 --jq '.workflow_runs[] | [.name,.head_sha[0:7],.status,.conclusion] | @tsv'`
  (look for "pages build and deployment" on the merge commit). The live site cannot be fetched from a cloud session.
- **Tell him when something is finished** (he asked: "Kan du ikke sige til når du er færdig?"): a push notification
  when a requested piece of work is ready for review, and again when it is live.
- He sends ideas in bursts, often several in one message, sometimes in English, sometimes with typos. Build all of
  them in one PR when they fit together, and answer each point. When something is ambiguous, pick the sensible
  reading, say which, and offer the alternative in one line.
- He wants to hear **"nej"** when something would be too heavy or unconvincing. A scoped yes with the weak part left
  out, and said so, is what he likes.
- When he gives Danish text for the game (e.g. the letter), fix obvious typos and tell him which.
- He supplies his own assets: the music (his band, Tidejord), the band's logos, drawings of a spaceship. Use them as
  the brief. Do not add audio, images or models he has not supplied (three.js, its loaders and the WebXR hand model
  are the exceptions, all MIT).
- Before he asked for this document he had also asked about moving accounts; the repository stays where it is.

## 2. The repository

- GitHub `peterboggild/BrokildApps`, served by GitHub Pages at `https://peterboggild.github.io/BrokildApps/`.
  A push to `main` deploys (about two minutes).
- Many unrelated projects live here (audio plugins, tools, other handovers in the root). The VR work is in
  `vr-apps/`:

| App | Folder | Live URL | State |
|---|---|---|---|
| Brokild VR page | `vr-apps/index.html` | `/vr-apps/` | Public page (English), one card per app. Portalkniven is first. |
| Portalkniven | `vr-apps/portalkniven/` | `/vr-apps/portalkniven/` | The main project. Large, live, in active development. |
| Mutant Rotte Angreb | `vr-apps/tank-ambush/` | `/vr-apps/tank-ambush/` | Finished game; latest addition the cheese bomb (#75). |
| Guldplaneten | `vr-apps/guldplaneten/` | `/vr-apps/guldplaneten/` | Not touched in these sessions. |
| Ræs i Bordbyen | `vr-apps/bordbyen/` | `/vr-apps/bordbyen/` | New city-racing game (#78): build a town on the table (design), pick a toy car, shrink to 1:1 and race: clear all gates in any order, then first over the finish, against 3 AI cars. Files `game.js`, `city.js`, `cars.js`, `race.js`, `audio.js`; README (Danish) has the design; cache `?v=9`. |

- The site's front page (`index.html` in the root) mentions the three VR apps in one paragraph.
- `tools/sync-site.js` writes the social preview tags and `sitemap.xml`. Run `node tools/sync-site.js` after
  changing a page's `<title>` or description or adding a page; CI runs it with `--check`.
- Branch: these sessions used `claude/quest3-vr-tank-game-sdcqlt`, restarted from `main` after every merge (squash
  merges, so restart with `git checkout -B <branch> origin/main` and push with `--force-with-lease`). A new account's
  session will be given its own branch name; use that one.
- Commit messages and code comments in English. PR titles and bodies in Danish (he reads them).

## 3. Portalkniven

A knife cuts a door in the air of your own living room; through it you see, and walk into, other dimensions. WebXR
`immersive-ar` (passthrough), three.js r170 vendored, plain ES modules, no build step.

### 3.1 Files

| File | What it holds |
|---|---|
| `index.html` | Danish start page: the buttons, the options, the how-to list. Loads `portal.js?v=N`. |
| `portal.js` | Renderer, knife and cutting, the door (stencil window), crossing, the spiral when the door changes world, music and sound effects, the HUD boards, all controls (fractals, deep space, 4D room, ship), hands, combat glue, the main loop, the flat preview and the test hooks (`window.__portal`). |
| `worlds.js` | Every dimension: surfaces, skies, decorations, the room scan and its grids, the fractals (GLSL and the same distance functions in JS), deep space, the 4D room (`FOUR_FS`) and its glass Klein bottle, the band's marks, the touchable things. |
| `story.js` | The story (`createStory`): letter, knife, nine time-machine parts in three worlds, the machine, the trip to the past, home, free play. |
| `hands.js` | The gloved hands (WebXR generic hand model, posed by our own forward kinematics). |
| `ship.js` | The optional spaceship, built in code from the owner's drawings, with plasma exhaust. |
| `ambience.js` | All synthesised sound for the fractals, deep space, the ship, parameter changes, explosions. |
| `combat.js` | The combat test: alien bugs living in the fractal's own space. |
| `music/` | Three tracks by the owner's band (one per "song dimension"). |
| `img/` | The band's sigil and name as masks. |
| `models/` | WebXR hand models (MIT, licence file beside them) and the owner's Blender script for the ship (reference only). |
| `vendor/` | three.js, GLTFLoader, BufferGeometryUtils. |
| `README.md` | Danish, very detailed, written in the order things were built (newest at the bottom). |

**Cache busting:** `index.html` loads `portal.js?v=N`, and `portal.js` imports every module with the same `?v=N`.
Raise them all together on every change (currently **v=51**), or the headset keeps old code.

### 3.2 What the player can do

Start page buttons: *Historien med hjælpetekster*, *Historien uden hjælpetekster*, *Fri leg*, *Det firedimensionale
rum*, *Se fladt på computer*. Options (remembered in localStorage): *Grafik* (render scale), *Fraktaler i ro*
(sharpening: build up over time / bigger picture / off), *Flyv efter* (gaze or right hand), *Rumskib i de fraktale
dimensioner*, *Kamp (test)*.

Dimensions (`DIMS` in `worlds.js`), the door chooses where it leads:

| Key | Name | Notes |
|---|---|---|
| `stue` | Din stue | The real room (passthrough). |
| `groen` | Den grønne | Moss, stalks, tall grass, spores. Music: Dødt Træ. Touchable stalks and grass. |
| `metal` | Metalverdenen | Riveted steel hall. Music: Rotation. Valve wheel, hanging cables. |
| `klippe` | Klippeverdenen | Sandstone, dusk sky. Music: Tidejord. Dry straw. |
| `rum` | Verdensrummet | Deep space: Milky Way, nebulae, galaxies, near stars in depth; fly, warp (B/A), black hole (Y/X). |
| `apollo` `klein` `hyper` `menger` `boks` `kali` | Six fractal dimensions | Ray-marched; fly, reshape (Y/X, B/A, eased with sound), scale (both grips), colour schemes (left stick hard sideways). `boks` = Mandelbox city, `kali` = KaliBox temple in front of the door among stars. |
| `fire` | Det firedimensionale rum | A furnished room with four spatial dimensions, seen as a 3D slice. |

Space, fractals and the 4D room open only in free play or once the story is finished.

Controls in short (the start page and README have the full lists):
- **Room / song dimensions:** trigger cuts a ring (close it to light the door), right stick chooses the world behind
  the door, a cut across the ring closes it. The door closes by itself 10 s after you cross (story).
- **Fractals (flying yourself):** right stick flies (where you look, or where the right hand points), left stick
  up/down is speed 1–9, Y/X and B/A reshape, both grips scale, left stick hard sideways changes colour scheme, right
  stick press changes the world behind the door.
- **Spaceship (option):** a 86 cm ship hangs in front of you; the right stick steers (hand steering was removed), left stick is the throttle (0 is stop). It keeps ahead of you in its flying direction, comes round in
  front of you when you turn yourself, can be pushed by hand, collides with the fractal.
- **Deep space:** as the fractals, plus B/A warp and Y/X black hole.
- **4D room:** Y/X pull through w, B/A turn in w (both eased, with sound), left stick walks, right stick snap-turns
  30°, left stick press resets the fourth dimension.
- **Combat test (option):** bugs crawl towards the ship and burst; reshaping the fractal crushes them; all weapons on the
  hand without the knife fire straight along the nose (no aim assist): trigger = piercing plasma bolts (space bar flat),
  right grip = a pair of rockets with smoke trails and fireballs (homing, proximity fuse 90 cm, 1.5 m blast, both in fractal units so they grow with the bugs; key X); a spatial warning chirp for bugs close to the ship or close behind the head (`AMB.warn`), left grip = a 1.2 s cone of
  light that annihilates everything inside it, then 5 s to charge (key C). Bugs speed up each minute (`COMBAT.pace()`, up
  to ×4) and grow in number. Bumping walls damages the ship by speed; the board over the ship shows hull, kills, ray
  charge, rockets and pace.
- **The door is a stencil window.** Mask disc writes stencil 1; a full-screen fill (stencil 1, depth always) paints
  the other world's sky and resets depth; the other world is drawn where stencil = 1 and clipped behind the door
  plane. Render orders: home 0, home sky 5, mask 10, fill 20, other 30, spiral 40, ring 45, hands/knife 46.
  **Home-world materials are stencil-tested NotEqual 1** so nothing transparent from the world you stand in is laid
  over the door (it punched black squares in the camera picture). Anything new and transparent in a world needs it.
- **Doorway:** in the last 11 cm before the door plane the window becomes the whole view (`maskFull`), and for the
  first 13 cm after crossing there is no window; otherwise one eye glimpses the world you are leaving.
- **Fractals** are marched into small per-eye render targets and blitted behind everything. Resolution follows the
  frame rate (`FR.scale`), so **any per-frame cost elsewhere (even audio) makes the fractals blurrier.** The owner
  noticed this once (#74). Keep idle things idle.
- **Distance functions exist twice**: GLSL in `FRACT` and plain-number JavaScript in `DE` (same results, checked on
  24,000 points; no arrays, so no garbage). The JS ones drive ship collisions, echoes and the bugs. Change both.
- **The 4D room** (`FOUR_FS`) maps a room point q to the 4D point `R4·(q,0)+c4`. Pull and turn change `R4`/`c4`
  (`fourStep`), walking adds `R4·(step,0)` to `c4` (`fourWalk`), snap-turning rotates the anchor about the head
  (`fourTurnAbout`). The tesseract stands on a corner in w (a reflection maps w onto its long diagonal) and is drawn
  as its square faces thickened (sticks) plus its edges (brass balls). The glass Klein bottle is a mesh over the
  march that undoes the slice itself and hides behind furniture using the march picture's alpha (distance).
  Its shape: the classic parametric "bottle" with the body fattened and the neck thickened; the neck is lifted 30 cm
  into w where it would cross its own wall (u 2.45–2.50).
- **Hands**: the WebXR generic hand model in grip space; joints are flat in the file, so `hands.js` poses them by its
  own forward kinematics (thumb swung to oppose, then bent).
- **Ship**: lofts and boxes in code from the owner's nine drawings; one painted mesh plus one outline mesh;
  `toneMapped: false` and no environment map (ACES turned the pale blue turquoise).
- **Sound** (`ambience.js`): everything synthesised; placed sounds are HRTF-panned and freed when they end (at most
  six at once); reverbs kept short. Each fractal has its own soundscape; deep space has warp and black-hole voices.
- **Safety is not optional**: proximity warning tone, the room scan opening to passthrough near head and hands,
  warning texts.

### 3.4 Testing without a headset

- Serve the repo root (`python3 -m http.server 8765`) and open `/vr-apps/portalkniven/?flat` with options:
  `free`, `four`, `ship`, `combat`, `nohelp`, `nohand`, `sharp=taa|res|off`, `steer=hand`, `tilt=deg`, `skin=wire`.
- Playwright (installed globally; `NODE_PATH=$(npm root -g)`), Chromium flags
  `--use-gl=angle --use-angle=swiftshader --enable-unsafe-swiftshader --ignore-gpu-blocklist --no-sandbox`, plus
  `--autoplay-policy=no-user-gesture-required` when sound code should run. Software rendering is slow: **wait by
  frames, not time**, and run long tests in the background.
- Hooks: `window.__portal` (`S`, `placeDirect()`, `walk()`, `view()`, `hand()`, `tip()`, `cycleOther()`,
  `setOther()`, `dissolvePortal()`, `VX`, `FC`, `SH`, `keys`, `story`, `worlds`, `ship`, `COMBAT`, `AMB`,
  `shipDamage`, `handModels` …). In the flat preview the keyboard stands in for sticks and buttons (I/J/K/L, U/O,
  M/N, B/V, G/H, T, comma/period; see `portal.js`). `worlds.fourWalk`, `fourTurnAbout`, `fourStep`, `fractParam`,
  `fractScale` can be called directly.
- **Look at screenshots** of anything visual; numbers alone are not enough. Contact sheets with PIL are handy.
- To prove "nothing changed" visually, render an older commit with `git worktree add` and a second server and diff
  the pictures (done in #74).

### 3.5 Open questions (waiting for headset feedback)

Nothing below is known to be broken; it simply has not been seen in a headset:
- Frame rate: fractals with sharpening, the 4D room with tesseract and bottle, combat, deep space with 3000 stars.
- Whether the doorway fix removed the camera flash completely.
- Hands: do they sit right round the controllers; do the fingers follow.
- Ship: size and distance, hand steering, how it swings round you, pushing it.
- All sound (never heard): levels, the six soundscapes, ship engine, parameter sounds, warp, black hole.
- 4D room: walking and snap-turn comfort, how the glass reads in stereo, sharpness at the small march resolution.
- Combat: bug size and visibility (6–8 cm), pace (0.2 m/s), damage; in which fractals reshaping crushes bugs
  (worked in Apollonian, Menger, Hyperbolic; not found in Mandelbox city, Pseudo-Kleinian, Kali temple).

### 3.6 Offered but not built (ask before doing)

- **Phone mode** (touch controls for free play on any phone) — recommended first.
- **Android AR mode** (Chrome on Android supports WebXR AR; no room scan there; not on iPhone).
- **Drone option** for the fractals — saved idea, described at the end of the README.
- **The exact Klein bottle formula** without the fattened proportions, if he prefers mathematical purity.
- Stopping the player (not just the ship) at fractal walls.

### 3.7 History, briefly (PR numbers)

#50–#53 the door, the knife, the green world · #54–#57 dimensions, music, cutting the ring closes it ·
#58–#61 the story (letter, parts, time machine, past) · #62–#64 fractal dimensions and their controls, sharpening ·
#65 shorter letter, spiral, deep space, touchable things · #66 4D room, Mandelbox city, Kali temple ·
#67 outline only after a real step · #68 hands, colour schemes, space flight/warp, band marks · #69 doorway flash,
Brokild VR page · #70 spaceship · #71 fly by hand, ship collisions, all sound · #72 eased parameters, ship keeps ahead ·
#73 combat test · #74 fractals sharp again (audio cost) · #75 tesseract, Klein bottle, 4D buttons, door stars,
cheese bomb.

## 4. Mutant Rotte Angreb (`vr-apps/tank-ambush/`)

Rats in toy tanks (and helicopters, kamikaze rats, super rats) attack in your own room, hiding behind your real
furniture (room scan). Files: `game.js` (everything), `models.js` (all models in code), `fx.js` (effects and sound),
`world.js` (room grid, line of sight, flow fields), `index.html` (Danish start page, loads `game.js?v=YYYYMMDDx`,
currently `20261120b`; `game.js` imports `models.js?v=…`; bump both when they change).

- Difficulty Begynder/Let/Mellem/Svær; rocket crates, heavy rockets, rat traps; room light follows the real room.
- **The cheese bomb (#75, the panic button):** turns up a little into rounds 6, 8, 10 …; picked up it only goes in the
  pocket (max 2, shown on the launcher board); **A or X** throws it (B on the keyboard). While the fuse burns (5.5 s)
  tanks and helicopters within 5 m and rats on foot anywhere in the room make for the cheese and hold fire; then it
  goes off (kills within 1.2 m, damage to 1.9 m, hurts the player within 1.1 m). Code: `CHEESE`, `launchCheese`,
  `updateCheeses`, `lureTargets`, `cheeseBoom`; model `buildCheese`.
- Flat preview: `?flat&cover` (virtual furniture). Hooks on `window.__tank` (`G`, `tanks`, `startWave`,
  `spawnPickup`, `spawnRunner`, `launchCheese`, `look(x,y,z,yaw,pitch)` …). To start from the menu in a test, set
  `G.inputLock = 0` and press Q.
- Not seen in a headset yet: the cheese bomb as a whole.

## 5. Practical notes for a cloud session

- Check out the repo, read `vr-apps/CLAUDE.md`, then the relevant README's newest sections.
- Patch with small Python scripts that **assert each anchor occurs exactly once**, then `node --check` every changed
  JS file (copy to `.mjs` first). Never put a `//` comment in the middle of a line (it has broken two modules).
- Keep screenshots and test scripts in the session's scratch directory, not in the repo.
- When a request touches the start page or a game's description, update the Brokild VR card and run
  `node tools/sync-site.js`.
