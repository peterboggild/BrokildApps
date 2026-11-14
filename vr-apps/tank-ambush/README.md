# Tank Ambush

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
