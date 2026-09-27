// PROTOCOL v3: a material per surface, and two breakable walls per room.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "..", "PROTOCOL.md");
let s = fs.readFileSync(p, "utf8");
const misses = [];
const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(c + " for " + a.slice(0, 60)); return; } s = s.split(a).join(b); };

rep(`| \`mat1\` | LARGE ROOM WALLS | 1 | choice \`ABSORBING|FURNISHED|PLASTER|TILED|STUDIO\` | room 0 |
| \`mat2\` | SMALL ROOM WALLS | 1 | choice (same) | room 1 |
| \`mat3\` | GIANT ROOM WALLS | 2 | choice (same) | room 2 |`,
`Then, for each room n = 1..3 (n = 1 LARGE, 2 SMALL, 3 GIANT), seven rows in this order:

| id | name | default | kind | real value |
|---|---|---|---|---|
| \`mat{n}\` | ROOM n WALLS | 1, 1, 2 | choice \`ABSORBING|FURNISHED|PLASTER|TILED|STUDIO\` | the four vertical walls |
| \`flr{n}\` | ROOM n FLOOR | 0 | choice \`AS WALLS|ABSORBING|FURNISHED|PLASTER|TILED|STUDIO\` | index 0 means follow the walls |
| \`cel{n}\` | ROOM n CEILING | 0 | choice (same as the floor) | index 0 means follow the walls |
| \`fold{n}a\` | ROOM n FOLD A | 0.5 | float | metres = (v − 0.5)·1.2, so ±0.6 m; 0.5 is a flat wall |
| \`foldp{n}a\` | ROOM n FOLD A POS | 0.5 | float | along the wall, 0.15 + v·0.7 |
| \`fold{n}b\` | ROOM n FOLD B | 0.5 | float | the second breakable wall |
| \`foldp{n}b\` | ROOM n FOLD B POS | 0.5 | float | |

**The breakable walls** are the two of each room that carry no doorway, so a fold
never collides with a door. Wall numbering: 0 = west (x0), 1 = east (x1),
2 = south (y0), 3 = north (y1).

| room | fold A | fold B |
|---|---|---|
| LARGE | west wall, x = 0 | south wall, y = 0 |
| SMALL | west wall, x = 3 | north wall, y = 8.5 |
| GIANT | east wall, x = 18 | north wall, y = 9 |

A fold splits its wall at \`foldp\` along its length and pushes that point \`fold\`
metres along the wall's OUTWARD normal. Positive pushes the point out of the
room, which disperses; **negative makes the wall concave, which focuses sound at
a point and is worse than leaving it flat** — the plan should say so when a fold
goes negative. The room's plan polygon, its floor area and its volume all follow
the fold, so pushing a wall out really does make the room bigger.`);

rep(`47 parameters. Choice parameters travel normalised: v = index / (steps − 1).`,
    `65 parameters. Choice parameters travel normalised: v = index / (steps − 1).`);

rep(`  rt:[ [t250,t1k,t4k,t8k], [..], [..] ],   // per room, seconds, current materials + doors`,
    `  rt:[ [t250,t1k,t4k,t8k], [..], [..] ],   // per room, seconds, current materials + doors
  plan:[ [[x,y],[x,y],...], [..], [..] ],  // each room's plan polygon, following the folds`);

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
fs.writeFileSync(p, s);
console.log("protocol v3 written");
