/*  HIGH TIDE — the decals into the panel.

    Seven of the twelve delivered parts have a home. Each is applied only once
    it has actually LOADED (the page adds a `dec-<name>` class on load), so a
    missing file leaves the procedural panel exactly as it was rather than a
    hole where a control used to be.

    ht-pearl      the ball itself, drawn on the overlay over its own glow
    ht-bezel      a round oscilloscope in a brass instrument bezel
    ht-tack       every point on the timeline is a brass chart tack
    ht-flag       the release marker
    ht-nameplate  the engraved plate in the header
    ht-glass      the gauge tubes (turned a quarter turn: drawn standing, used lying)
    ht-wood       two chart-table rails, under the header and over the timeline

    No home yet: ht-knob (this panel has no rotary control), and ht-ground,
    ht-paper, ht-rose came back unusable — see BUGLIST.
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

/* ------------------------------------------------------------- 1 . the CSS */
rep(`  --decal-ground:none; --decal-paper:none; --decal-bezel:none;
  --decal-knob:none; --decal-tack:none; --decal-nameplate:none;`,
`  --decal-ground:none; --decal-paper:none; --decal-knob:none;
  --decal-bezel:url(decals/ht-bezel.png); --decal-tack:url(decals/ht-tack.png);
  --decal-nameplate:url(decals/ht-nameplate.png); --decal-glass:url(decals/ht-glass.png);
  --decal-wood:url(decals/ht-wood.png); --decal-pearl:url(decals/ht-pearl.png);
  --decal-flag:url(decals/ht-flag.png);`);

rep(`</style>`, `
/* ---- the decals. Each applies only when its image has loaded, so the panel
   without them is exactly the panel that shipped. ------------------------- */
.dec-nameplate #plate .np{
  display:inline-block;width:196px;height:27px;color:transparent;
  background-image:var(--decal-nameplate);background-size:contain;
  background-repeat:no-repeat;background-position:left center}
.dec-glass .tube{background-image:var(--decal-glass);background-size:100% 100%;
  background-repeat:no-repeat}
.dec-wood #hdr::after,.dec-wood #tl::before{
  content:"";position:absolute;left:0;right:0;height:5px;z-index:3;
  background-image:var(--decal-wood);background-size:auto 100%;
  background-repeat:repeat-x;pointer-events:none;
  box-shadow:0 1px 3px rgba(0,0,0,.55)}
.dec-wood #hdr::after{bottom:-5px}
.dec-wood #tl::before{top:-5px}
</style>`);
rep(`#hdr{display:flex;align-items:center;gap:8px;padding:0 12px;`,
    `#hdr{position:relative;display:flex;align-items:center;gap:8px;padding:0 12px;`);
rep(`#tl{`, `#tl{position:relative;`);

/* -------------------------------------------------------- 2 . the loader */
rep(`/* ===========================================================================
   12 . the hints layer`,
`/* ===========================================================================
   11b . the decals

   Photographic parts, composited over the drawn panel. A part is used only
   once its image has loaded; until then (and for ever, if a file is missing)
   the panel draws itself exactly as it did before they existed.
   =========================================================================== */
const IMG = {};
["pearl","tack","flag","bezel","nameplate","glass","wood"].forEach(n=>{
  const o = {ok:false, img:new Image()};
  o.img.onload = ()=>{ o.ok = true; document.body.classList.add("dec-"+n); };
  o.img.onerror = ()=>{ o.ok = false; };
  o.img.src = "decals/ht-"+n+".png";
  IMG[n] = o;
});
/* where the balls are this frame, in world space, for the overlay to place a
   pearl on each: filled by fillScene, which already has the projection maths */
const PEARLS = [];

/* ===========================================================================
   12 . the hints layer`);

/* --------------------------------------------- 3 . record the balls' places */
rep(`    spPush(B,x,y+0.03,wZ(zn),0.16+0.1*e,0.5,0.95,1.0,0.12+0.35*e);
    spPush(B,x,y+0.03,wZ(zn),0.055,0.88,0.99,1.0,0.97);`,
    `    spPush(B,x,y+0.03,wZ(zn),0.16+0.1*e,0.5,0.95,1.0,0.12+0.35*e);
    if(IMG.pearl.ok) PEARLS.push({x, y:y+0.03, z:wZ(zn), r:0.055, a:1});
    else spPush(B,x,y+0.03,wZ(zn),0.055,0.88,0.99,1.0,0.97);`);
rep(`    const u=v.u||[]; for(let k=0;k+1<u.length;k+=2){ const ux=clamp(+u[k]||0,XMIN,XMAX), uz=clamp(+u[k+1]||0,0,1); spPush(B,ux,wY(uEffAt(ux,uz))+0.025,wZ(uz),0.036,0.7,0.95,1.0,0.85); }`,
    `    const u=v.u||[]; for(let k=0;k+1<u.length;k+=2){ const ux=clamp(+u[k]||0,XMIN,XMAX), uz=clamp(+u[k+1]||0,0,1);
      if(IMG.pearl.ok) PEARLS.push({x:ux, y:wY(uEffAt(ux,uz))+0.025, z:wZ(uz), r:0.036, a:0.88});
      else spPush(B,ux,wY(uEffAt(ux,uz))+0.025,wZ(uz),0.036,0.7,0.95,1.0,0.85); }`);
rep(`function fillScene(){`, `function fillScene(){\n  PEARLS.length = 0;`);

/* ------------------------------------- 4 . the round scope, and the pearls */
rep(`  /* scope */
  const sx=W-168, sy=10, sw=150, sh=54;
  g.fillStyle="rgba(7,16,26,.72)"; g.fillRect(sx,sy,sw,sh); g.strokeStyle="rgba(201,162,74,.45)"; g.lineWidth=1; g.strokeRect(sx+.5,sy+.5,sw-1,sh-1);
  g.strokeStyle="rgba(31,181,196,.9)"; g.beginPath();
  for(let i=0;i<256;i++){ const v=scopeOk?clamp(SCOPE[i]||0,-1,1):0; const x=sx+4+i/255*(sw-8), y=sy+sh/2-v*(sh/2-4); if(i) g.lineTo(x,y); else g.moveTo(x,y); } g.stroke();
  g.fillStyle="rgba(31,181,196,.8)"; g.fillRect(sx+4, sy+sh-4, (sw-8)*clamp(lvl,0,1), 2);
  g.fillStyle="rgba(205,214,220,.5)"; g.font="9px "+getComputedStyle(document.body).getPropertyValue("--serif"); g.fillText("scope", sx+5, sy+10);`,
`  /* the pearls: the ball, over the glow the terrain pass drew for it */
  if(IMG.pearl.ok){
    for(const p of PEARLS){
      const a=proj(p.x,p.y,p.z); if(!a) continue; const ax=a.x, ay=a.y;
      const b=proj(p.x,p.y+p.r,p.z); if(!b) continue;
      const rad=clamp(Math.abs(b.y-ay), 3, 90);
      g.globalAlpha=p.a; g.drawImage(IMG.pearl.img, ax-rad, ay-rad, rad*2, rad*2); g.globalAlpha=1;
    }
  }
  /* scope — a round instrument in a brass bezel */
  const R=IMG.bezel.ok?62:44, cxs=W-R-26, cys=R+16;
  g.save(); g.beginPath(); g.arc(cxs,cys,R-7,0,TAU); g.clip();
  g.fillStyle="rgba(7,16,26,.82)"; g.fillRect(cxs-R,cys-R,2*R,2*R);
  g.strokeStyle="rgba(31,181,196,.14)"; g.lineWidth=1; g.beginPath(); g.moveTo(cxs-R,cys+.5); g.lineTo(cxs+R,cys+.5); g.stroke();
  g.strokeStyle="rgba(31,181,196,.9)"; g.beginPath();
  for(let i=0;i<256;i++){ const v=scopeOk?clamp(SCOPE[i]||0,-1,1):0; const x=cxs-(R-9)+i/255*2*(R-9), y=cys-v*(R-13); if(i) g.lineTo(x,y); else g.moveTo(x,y); } g.stroke();
  g.restore();
  g.strokeStyle="rgba(31,181,196,.75)"; g.lineWidth=2.5; g.beginPath();
  g.arc(cxs,cys,R-4.5, Math.PI*0.78, Math.PI*0.78+Math.PI*1.44*clamp(lvl,0,1)); g.stroke(); g.lineWidth=1;
  if(IMG.bezel.ok) g.drawImage(IMG.bezel.img, cxs-R, cys-R, 2*R, 2*R);
  else { g.strokeStyle="rgba(201,162,74,.45)"; g.beginPath(); g.arc(cxs,cys,R-4,0,TAU); g.stroke(); }
  g.fillStyle="rgba(205,214,220,.45)"; g.font="9px "+getComputedStyle(document.body).getPropertyValue("--serif");
  g.fillText("scope", cxs-12, cys+R-15);`);

/* ---------------------------------------- 5 . the timeline: tacks and flag */
rep(`      const tack=(x,y,sel)=>{ g.beginPath(); g.arc(x,y,sel?6:4.5,0,TAU); g.fillStyle="#c9a24a"; g.fill(); g.strokeStyle="#5a4416"; g.stroke(); g.beginPath(); g.arc(x-1,y-1,1.6,0,TAU); g.fillStyle="#f4e2a8"; g.fill(); };`,
    `      const tack=(x,y,sel)=>{ const r=sel?7:5.4;
        if(IMG.tack.ok){ g.drawImage(IMG.tack.img, x-r, y-r, 2*r, 2*r); return; }
        g.beginPath(); g.arc(x,y,sel?6:4.5,0,TAU); g.fillStyle="#c9a24a"; g.fill(); g.strokeStyle="#5a4416"; g.stroke(); g.beginPath(); g.arc(x-1,y-1,1.6,0,TAU); g.fillStyle="#f4e2a8"; g.fill(); };`);
rep(`  g.fillStyle="#c0392b"; g.fillRect(TL.XM-0.5,4,1,Hh-6); g.beginPath(); g.moveTo(TL.XM,4); g.lineTo(TL.XM+9,8); g.lineTo(TL.XM,12); g.fill();`,
    `  g.fillStyle="#c0392b"; g.fillRect(TL.XM-0.5,4,1,Hh-6);
  if(IMG.flag.ok) g.drawImage(IMG.flag.img, TL.XM-1.5, 1, 14, 18);
  else { g.beginPath(); g.moveTo(TL.XM,4); g.lineTo(TL.XM+9,8); g.lineTo(TL.XM,12); g.fill(); }`);

/* -------------------------------------------------------- 6 . probe hooks */
rep(`  tips: ()=>TIPS, hints: setHints,`,
    `  tips: ()=>TIPS, hints: setHints,
  decals: ()=>{ const o={}; for(const k in IMG) o[k]=IMG[k].ok; o._pearls=PEARLS.length; return o; },`);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("ui.html patched: seven decals wired, each behind a load check");
