/*  Correcting the decal hook-up. The panel already had `--decal-*` hooks and I
    redefined two of them without checking what consumed them:

      --decal-bezel  is the HEADER's background layer and the gauge tubes'
                     end ferrules. Pointing it at a 320 px brass RING painted a
                     stretched ring across the whole header. The bezel belongs
                     only on the round scope, which the canvas draws itself.
      --decal-knob   is the TUNE LOCK switch's dot, whose colour is how the
                     switch says it is on. A brass cap over it would hide that.

    Find every consumer of a value before redefining it — the same rule as for
    a parameter, and it caught me on a CSS variable instead.
*/
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/ui/ui.html";
let s = fs.readFileSync(f, "utf8");
const misses = [];
function rep(a, b, n = 1) {
  const c = s.split(a).length - 1;
  if (c !== n) { misses.push("expected " + n + " of " + JSON.stringify(a.slice(0, 72)) + ", found " + c); return; }
  s = s.split(a).join(b);
}

rep(`  --decal-ground:none; --decal-paper:none; --decal-knob:none;
  --decal-bezel:url(decals/ht-bezel.png); --decal-tack:url(decals/ht-tack.png);
  --decal-nameplate:url(decals/ht-nameplate.png); --decal-glass:url(decals/ht-glass.png);
  --decal-wood:url(decals/ht-wood.png); --decal-pearl:url(decals/ht-pearl.png);
  --decal-flag:url(decals/ht-flag.png);`,
`  /* A hook stays `+"`none`"+` unless the delivered part suits the thing that
     consumes it. --decal-bezel is the header band and the tube ferrules, and a
     ring is neither; the bezel is drawn on the round scope by the overlay.
     --decal-knob is the switch dot, whose colour carries its state. ground and
     paper came back as flat strips with no texture in them. */
  --decal-ground:none; --decal-paper:none; --decal-knob:none; --decal-bezel:none;
  --decal-nameplate:url(decals/ht-nameplate.png); --decal-glass:url(decals/ht-glass.png);
  --decal-wood:url(decals/ht-wood.png);`);

/* the nameplate belongs on #plate, which was built for it */
rep(`.dec-nameplate #plate .np{
  display:inline-block;width:196px;height:27px;color:transparent;
  background-image:var(--decal-nameplate);background-size:contain;
  background-repeat:no-repeat;background-position:left center}`,
`.dec-nameplate #plate{background-size:contain;background-repeat:no-repeat;
  background-position:left center;border-color:rgba(90,68,22,.55)}
.dec-nameplate #plate .np{color:transparent}
.dec-nameplate #plate .np2{color:#3a2e18;text-shadow:0 1px 0 rgba(255,235,180,.35)}`);

rep(`#tl{position:relative;position:relative;`, `#tl{position:relative;`);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("decal hooks corrected: bezel and knob back to none, nameplate on #plate");
