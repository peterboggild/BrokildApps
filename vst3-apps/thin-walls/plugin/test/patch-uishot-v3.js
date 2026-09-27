/*  test/uishot.js still sends the v2 parameter list - four materials and no
    per-room block - so the page it photographs has its selectors shrunk back to
    four and none of the new controls bound. Bring it to v3.
*/
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "uishot.js");
let s = fs.readFileSync(p, "utf8");
const misses = [];
const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(c + " for " + a.slice(0, 70)); return; } s = s.split(a).join(b); };

rep(`const MATS = ["ABSORBING","FURNISHED","PLASTER","TILED"];`,
    `const MATS = ["ABSORBING","FURNISHED","PLASTER","TILED","STUDIO"];
const SURF = ["AS WALLS"].concat(MATS);          // the floor and ceiling lists
const ROOMN = ["LARGE","SMALL","GIANT"];`);

rep(`[["mat1",1/3],["mat2",1/3],["mat3",2/3]].forEach(p =>
  PARAMS.push({ id:p[0], v:p[1], stepped:true, steps:4, choices:MATS.join("|") }));`,
`// per room: walls, floor, ceiling, then the two breakable walls
[1/(MATS.length-1), 1/(MATS.length-1), 2/(MATS.length-1)].forEach((wv, i) => {
  const n = i + 1, R = ROOMN[i] + " ";
  PARAMS.push({ id:"mat"+n, name:R+"ROOM WALLS",   v:wv, stepped:true, steps:MATS.length, choices:MATS.join("|") });
  PARAMS.push({ id:"flr"+n, name:R+"ROOM FLOOR",   v:0,  stepped:true, steps:SURF.length, choices:SURF.join("|") });
  PARAMS.push({ id:"cel"+n, name:R+"ROOM CEILING", v:0,  stepped:true, steps:SURF.length, choices:SURF.join("|") });
  ["a","b"].forEach((k, j) => {
    PARAMS.push({ id:"fold"+n+k,  name:R+"FOLD "+k.toUpperCase(),        v:0.5 });
    PARAMS.push({ id:"foldp"+n+k, name:R+"FOLD "+k.toUpperCase()+" POS", v:0.5 });
  });
});`);

rep(`PARAMS.forEach(p => { p.name = p.id.toUpperCase(); });`,
    `PARAMS.forEach(p => { if (!p.name) p.name = p.id.toUpperCase(); });`);

// the scene carries the plan polygons now; a flat room is its own rectangle
rep(`  rt:[[0.62,0.48,0.38,0.29],[0.41,0.33,0.26,0.20],[2.41,1.96,1.42,0.95]],`,
    `  rt:[[0.62,0.48,0.38,0.29],[0.41,0.33,0.26,0.20],[2.41,1.96,1.42,0.95]],
  plan:[ [[0,0],[6,0],[6,5],[0,5]], [[3,5],[6,5],[6,8.5],[3,8.5]], [[6,0],[18,0],[18,9],[6,9]] ],`);

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
fs.writeFileSync(p, s);
console.log("uishot.js brought to v3");
