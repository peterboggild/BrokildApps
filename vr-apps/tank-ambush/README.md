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
* Rocket: speed (`launch` / `updateRockets`, 3.2 → 8 m/s), homing rate (`2.3` rad/s), lock cone (`0.44` rad).
* Splash: radius `R = 0.8` and the damage / push in `blast`.
* Player damage: `22` per direct shell in `shellBoom`.

## Controls

* Quest: trigger = fire that hand's rocket (also starts / restarts); grip = show or hide the room mesh.
* Flat preview: `?flat` in the URL or the button — WASD, mouse (click to capture), click to fire, `G` for the room mesh.
* URL options: `?cover=1` always add virtual barriers, `?mesh=0` ignore the headset's room scan.
