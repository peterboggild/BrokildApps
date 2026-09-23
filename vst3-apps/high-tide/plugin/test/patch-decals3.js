/*  Three more decals into the panel: the chart-table ground, the tide-table
    paper under the timeline, and the compass rose in the corner of the view.

    The ground and paper hooks already existed (#deck and #tl); they only
    needed a texture worth putting in them and a matching background-size and
    -repeat list, because those elements carry several background layers and a
    single value would apply to all of them.
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

rep(`  --decal-ground:none; --decal-paper:none; --decal-knob:none; --decal-bezel:none;
  --decal-nameplate:none;
  --decal-glass:url(decals/ht-glass.png); --decal-wood:url(decals/ht-wood.png);`,
`  /* A hook stays none unless the delivered part suits what consumes it.
     --decal-bezel is the header band and the tube ferrules, and a ring is
     neither; the bezel is drawn on the round scope by the overlay. --decal-knob
     is the switch dot, whose colour carries its state. The nameplate's
     lettering is cut by the plate's own edge in the artwork. */
  --decal-knob:none; --decal-bezel:none; --decal-nameplate:none;
  --decal-ground:url(decals/ht-ground.png); --decal-paper:url(decals/ht-paper.png);
  --decal-glass:url(decals/ht-glass.png); --decal-wood:url(decals/ht-wood.png);`);

/*  #deck carries four background layers and #tl two, so the size and repeat
    lists have to name every one of them or the texture's setting is applied
    to the gradients as well. */
rep(`.dec-glass .tube{background-image:var(--decal-glass);background-size:100% 100%;`,
`.dec-ground #deck{background-size:auto,auto,auto,auto;
  background-repeat:repeat,no-repeat,repeat,repeat;
  background-blend-mode:normal,screen,normal,normal}
.dec-paper #tl{background-size:auto,auto;background-repeat:repeat,no-repeat}
.dec-glass .tube{background-image:var(--decal-glass);background-size:100% 100%;`);

rep(`["pearl","tack","flag","bezel","glass","wood"].forEach(n=>{`,
    `["pearl","tack","flag","bezel","glass","wood","ground","paper","rose"].forEach(n=>{`);

/*  the rose: a chart ornament rising out of the corner. Only its upper
    quadrant is on screen, which is exactly the half that was drawn. */
rep(`  /* the pearls: the ball, over the glow the terrain pass drew for it */`,
`  /* the compass rose, rising out of the far corner as it would on a chart.
     Only its upper-left quadrant is in frame — which is the half that was
     drawn, so nothing here is invented. */
  if(IMG.rose.ok){
    const RS=Math.min(430, Math.max(210, W*0.30));
    g.globalAlpha=0.10;
    g.drawImage(IMG.rose.img, W-RS*0.62, Hh-RS*0.30, RS, RS*IMG.rose.img.height/IMG.rose.img.width);
    g.globalAlpha=1;
  }
  /* the pearls: the ball, over the glow the terrain pass drew for it */`);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("ground, paper and rose wired in");
