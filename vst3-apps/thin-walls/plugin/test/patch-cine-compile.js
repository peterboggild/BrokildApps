"use strict";
/* cinematic: shorten the D3D compile - no gradient shadow lookups inside loops,
   and the bounce loop bounded by a uniform so it cannot be unrolled. */
const fs = require("fs"), path = require("path");
const f = path.join(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(f, "utf8");
const BS = String.fromCharCode(92), LNL = BS + "n";
const miss = [], ed = [];
const r = (a, b) => { const n = s.split(a).length - 1; if (n !== 1) miss.push("x" + n + " " + a.slice(0, 70)); else ed.push([a, b]); };
r('"  if(taps<=1) return texture(uSunSh, vec3(q.xy, q.z-0.0009));",',
  '"  if(taps<=1) return textureLod(uSunSh, vec3(q.xy, q.z-0.0009), 0.0);",');
r('"  for(int i=0;i<12;i++) s+=texture(uSunSh, vec3(q.xy+POIS[i]*ts*1.6, q.z-0.0009));",',
  '"  for(int i=0;i<12;i++) s+=textureLod(uSunSh, vec3(q.xy+POIS[i]*ts*1.6, q.z-0.0009), 0.0);",');
r('"    for(int b=0;b<2;b++){ if(b>=uBounces) break;' + LNL + '" +',
  '"    for(int b=0;b<uBounces;b++){' + LNL + '" +');
if (miss.length){ console.error("missing:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of ed) s = s.replace(a, () => b);
fs.writeFileSync(f, s); console.log("ok " + ed.length);
