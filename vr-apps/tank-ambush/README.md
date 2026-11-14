# Mutant Rotte Angreb (Tank Ambush)

The whole game is in **Danish** and the enemies are rats: panservogne driven by rats, helicopters and kamikaze rats. The folder and URL are still `tank-ambush`.

Mixed-reality tank game for the Meta Quest 3 browser (WebXR `immersive-ar`).
No build step: open `index.html` over HTTPS (GitHub Pages) in the Quest browser.

## Files

| File | What it does |
|------|--------------|
| `index.html` | Start page, the two buttons (mixed reality / flat preview). |
| `game.js` | Session setup, room scan intake, tank AI, rockets, shells, waves, HUD. |
| `world.js` | The room as a voxel grid (6 cm cells): line of sight, collisions, tank navigation (flow fields). |
| `models.js` | Tank (with real moving track links), launcher, rocket — all built from code. |
| `fx.js` | Particles, debris and the synthesised sound. |
| `vendor/` | three.js r170 (MIT), bundled so the page has no CDN dependency. |

## How the room is used

1. `mesh-detection` (or `plane-detection` as a fallback) delivers the Space Setup scan.
2. Each mesh is copied to world space, floor-like triangles are dropped, and it is added to the scene as a **depth-only** mesh. That is what makes a tank behind your sofa look cut off by the real sofa.
3. The same triangles are stamped into the voxel grid in small slices over a few frames. Tanks, shells and rockets all query the grid.
4. No scan found → eight virtual barriers and a 3.7 m arena are used instead.

## Gameplay numbers worth tuning (all in `game.js`)

* Tank speed `vmax`, reload `reloadTime`, shell speed `shellSpeed`, aim scatter `aimErr` — in `spawnTank`, scaled by wave.
* Rocket: the `ROCKET` table in `game.js` — burn time, speed, gravity, seeker turn rate (0.5 rad/s) and lock cone (0.2 rad).
* Splash: radius `R = 0.8` and the damage / push in `blast`.
* Player damage: `22` per direct shell in `shellBoom`.

## Ammo, tanks and the hit fade

* Rockets are a shared pool (`G.ammo`: 6 to start, crates add 3, max 12). Heavy rockets (`heavy`, crates from wave 5) are used first: direct hit 260 and a 1.15 m blast, versus 110 and 0.8 m.
* With no rockets left, a held trigger fires bullets (`MG` in `game.js`: 12 damage, 6.7 shots/s per hand), so a 100 HP tank takes about nine.
* Tiers (`TIER` table): green tanks 100 HP; black from wave 6, 320 HP, 1.3x size; from wave 10, 800 HP, 1.75x size, tougher still. `buildQueue` decides the mix per wave.
* Helicopters (`HELI` table, `updateHeli`): from wave 8 an olive attack helicopter (240 HP), from wave 12 a bigger black gunship (520 HP, also fires rockets). They are entries in the same `tanks` array with `air: true`, so rockets, bullets, locking and wave logic all apply; `cyOf()` gives a target's real height. They fly between waypoints that can see the player (`pickHeliWaypoint`), steer clear of the voxel room (`AVD` probes), bank into acceleration, and fall in `updateCrash` when destroyed. The model is `buildHeli` in `models.js`; the rotor sound is `audio.rotor` in `fx.js`.
* A hit runs `updateFade`: red, black, back over about 2 s, with 2 s of invulnerability. Dying holds the dark behind the score board; the best score is kept in `localStorage`.

## Controls

* Quest: trigger = fire that hand's rocket (also starts / restarts); grip = show or hide the room mesh.
* Flat preview: `?flat` in the URL or the button — WASD, mouse (click to capture), click to fire, `G` for the room mesh.
* URL options: `?cover=1` always add virtual barriers, `?mesh=0` ignore the headset's room scan.

## Hands, rings and room light

* **Right hand** = ordinary rockets, and the machine gun once they run out. **Left hand** = heavy rockets; that launcher (painted dark red) is hidden unless heavy rockets are stocked. Roles come from the controller's handedness (`applyRole`); in the flat preview, click = right hand, right-click or Q = left.
* The red lock-on rings are off unless ticked on the start page (`G.rings`, or `?rings=1`). The seeker works either way.
* Room light: the Quest browser gives no camera image, so it cannot be measured. If the headset supports WebXR `light-estimation`, a light probe and the main light are taken from it (`applyEstimate`, untested on hardware). Otherwise the start page's Dark / Normal / Bright sets a brightness for all lights and the environment map, and a thumbstick (or `[` / `]`) adjusts it in play (`applyLight`).
* Helicopters only spawn and fly over floor the player could walk to (`airReachable`), so never behind a wall; if one gets wedged or leaves the room it heads for the player and then respawns (`relocateHeli`).

## The rats

* **Tank crews** (`buildRatHead` / `animRatHead` in `models.js`): a rat looks out of every tank's hatch, two (with black caps and red eyes) on the black tanks. They bob, blink, twitch their ears and follow the player with their eyes, duck into the hatch when the tank retreats, and are thrown clear when it blows up.
* **Kamikaze rats** (`spawnRunner` / `updateRat` / `explodeRat` in `game.js`, model `buildRunnerRat`): from round 3 (`buildQueue`: 2, 2, 3, 3, 4, 4, 5, 5, then 6 per round; queue value 9). They run at the player's feet along the nav flow field, zigzagging, light a fuse about 1.1 m out and go off 0.45 s later with a small firecracker bang (`fx.bang`) for 9-24 damage and only a short red flash (`hurtPlayer(d, true)`). 30 HP: three machine-gun bullets, or any rocket blast nearby (`fx.poof`). They never get a lock-on ring and do not count toward the enemy cap.
* **Dialog rats**: every in-game dialog (`showMsg` / `drawMsg`) has an animated cartoon rat in each corner (`drawRat`, moods: wave, angry, sad, cheer). The start page has the same rat as animated inline SVG.

* **Beeping**: each kamikaze rat carries a positional beeper (`audio.beeper`): slow beeps from afar, faster and higher as it closes in, frantic once the fuse is lit. It stops when the rat dies.
* **Blast zone**: `RUNNER.radius` is 1.6 m (damage 9-26, falling off with distance). A red ring of that radius is drawn on the floor round every rat and pulses with its urgency; `fx.bang(p, s, ringR)` throws a dust shockwave out to that radius.

## Super rats

From round 8 (`buildQueue` value 8: 1, 1, 2, 2, 2, then up to 3) a green mutant kamikaze rat (`SUPER` in `game.js`, `buildRunnerRat(true)`): 1.9x size, 140 HP (a direct standard rocket leaves 30; a heavy one kills), half the speed, a 2.1 m blast (12-36 damage) behind a green floor ring, a deeper beep, and a glowing green aura that sheds goo particles and a slime trail. Shot or detonated they burst in `fx.slime`.

## Rat traps (left hand)

The left hand has two weapons: heavy rockets and rat traps (`G.leftMode`). The **right thumbstick** (press, or flick sideways; `E` in the flat preview) switches; the **left thumbstick** up/down is room light. A trap (`launchTrap` in `game.js`, model `buildTrap`) is lobbed with a ballistic arc (`TRAP.speed` 4.6 m/s), lands on the floor or a surface, arms after 0.35 s and blinks a red light. Anything on the floor stepping on it (a rat within 0.2 m, or a tank hull) sets it off: `trapBoom`, radius 0.95 m, kills kamikaze rats (+30) and only scratches tanks (15 HP). Helicopters are not affected. Traps come in with round 3 (4 to start); trap packs (+3, `PK.trap`) only turn up from round 6. **At most 4 traps may be out at once** (`TRAP.maxActive`): a throw with 4 out is refused ("4 FÆLDER ER UDE") and costs no ammo. A trap does 70 damage to a super rat instead of killing it. When the left hand's weapon runs dry it switches to the other if that has ammo (`autoLeft`).

## Scoring

Kills score 30 (rat) / 100 / 300 / 700 (tanks) / 400 / 900 (helicopters). **Rocket bonus:** at the end of each round, `max(0, 2 x enemies - rockets fired) x 25` points (the machine gun is free). Best score is kept in `localStorage`.

## Room data check

In a headset session the start dialog reports the scan state (`roomLines`): found (green), "looking" for the first 3 s, or not found with the steps to scan the room (Settings -> Physical space -> Space setup; older software: Settings -> Environment -> Space setup). It also notes when the browser did not enable `mesh-detection` / `plane-detection` at all. A web page cannot open the headset's settings, so this is text only; the game can still be started (virtual barriers).

## Where enemies spawn

Anywhere in the room that is valid (open floor or air you can reach, at least 1.7 m from you). `pickSpawn` / `pickHeliSpawn` collect all valid spots and draw one at random (`drawWeighted`); `spawnWeight` only makes a spot less likely — not where one of the last five came from, and not right in front of you where it would visibly pop in. Do not turn this back into "take the best-scoring spot": with 200 candidates any penalty becomes a hard filter and spawns pile up at one end.

## Cache busting

The browser keeps module files for a long time (the Quest browser especially), so a new deploy can show the new start page but run old game code. `index.html` loads `game.js?v=BUILD` and `game.js` imports its siblings with the same `?v=BUILD`. **Bump BUILD in those four places (currently `20261114f`) whenever the game files change.**
