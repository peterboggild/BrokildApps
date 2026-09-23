/*  The nameplate goes back on the shelf.

    Its artwork has "HIGH TIDE" set so large and so low that the plate's own
    bottom edge cuts the letters in half — in the DELIVERED file, not in the
    crop. The panel draws its own name perfectly well, so the hook returns to
    `none` until a part arrives whose type sits inside its plate.
*/
"use strict";
const fs = require("fs");
const misses = [];
function edit(file, pairs) {
  let s = fs.readFileSync(file, "utf8");
  for (const [a, b] of pairs) {
    const c = s.split(a).length - 1;
    if (c !== 1) { misses.push(file.split(/[\\/]/).pop() + ": expected 1 of " + JSON.stringify(a.slice(0, 56)) + ", found " + c); continue; }
    s = s.split(a).join(b);
  }
  return [file, s];
}

const ui = edit("C:/Users/peter/b/HighTide/Source/ui/ui.html", [
  [`  --decal-ground:none; --decal-paper:none; --decal-knob:none; --decal-bezel:none;
  --decal-nameplate:url(decals/ht-nameplate.png); --decal-glass:url(decals/ht-glass.png);
  --decal-wood:url(decals/ht-wood.png);`,
   `  --decal-ground:none; --decal-paper:none; --decal-knob:none; --decal-bezel:none;
  --decal-nameplate:none;
  --decal-glass:url(decals/ht-glass.png); --decal-wood:url(decals/ht-wood.png);`],
  [`.dec-nameplate #plate{background-size:contain;background-repeat:no-repeat;
  background-position:left center;border-color:rgba(90,68,22,.55)}
.dec-nameplate #plate .np{visibility:hidden}
.dec-nameplate #plate .np2{color:#3a2e18;text-shadow:0 1px 0 rgba(255,235,180,.35)}
`, ``],
  [`["pearl","tack","flag","bezel","nameplate","glass","wood"].forEach(n=>{`,
   `["pearl","tack","flag","bezel","glass","wood"].forEach(n=>{`]
]);

const cm = edit("C:/Users/peter/b/HighTide/CMakeLists.txt", [
  [`        Source/ui/decals/ht-nameplate.png\n`, ``]
]);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(ui[0], ui[1], "utf8");
fs.writeFileSync(cm[0], cm[1], "utf8");
try { fs.unlinkSync("C:/Users/peter/b/HighTide/Source/ui/decals/ht-nameplate.png"); } catch (e) {}

//  and out of the ingest's want-list, so re-running it does not bring it back
const ing = "C:/Users/peter/b/HighTide/tools/ingest-decals.ps1";
let p = fs.readFileSync(ing, "utf8");
const line = `    "ht-nameplate.png" = @{ w = 1024; h = 0;   rot = $false }\n`;
if (p.split(line).length - 1 === 1) { fs.writeFileSync(ing, p.split(line).join(""), "utf8"); }
else { console.log("  (note: the ingest's nameplate line was not where expected)"); }
console.log("nameplate shelved; six decals remain in the panel");
