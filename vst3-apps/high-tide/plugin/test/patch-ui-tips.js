/*  HIGH TIDE — the hints layer.

    Peter, 2026-09-04: "it is very very difficult to guess what to do ... a
    popup not shadowing the cursor and control too much, explaining what the
    control is and how it is supposed to be used."

    Adds: a TIPS table covering every control, button, tool, stamp, timeline
    lane and the terrain view; a popup that appears after a third of a second
    of hovering and is placed BESIDE its control (never under the pointer);
    a HINTS switch in the header, remembered per machine. Also teaches the
    FACTORY menu about groups and the rail about DETUNE.

    Exact-count anchors; nothing is written on a miss.
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

/* ---------------------------------------------------------------- 1 . CSS */
rep(`</style>`, `
/* ---- the hints layer: a popup beside the control, never under the pointer -- */
#tip{position:fixed;z-index:400;max-width:290px;pointer-events:none;opacity:0;
  transition:opacity .12s ease;background:linear-gradient(#132433,#0c1721);
  border:1px solid var(--brass-dk);border-radius:5px;padding:8px 11px 9px;
  box-shadow:0 8px 26px rgba(0,0,0,.62),0 0 0 1px rgba(0,0,0,.35);
  font:11.5px/1.42 var(--serif);color:#d8e2e6}
#tip.on{opacity:1}
#tip .tt{font-variant:small-caps;letter-spacing:.13em;font-size:11px;color:var(--brass);
  display:block;margin-bottom:3px}
#tip .tw{display:block;color:#e6eef1}
#tip .th{display:block;color:#9fb3ba;margin-top:4px;font-style:italic}
#tip .tv{display:block;margin-top:6px;padding-top:5px;border-top:1px solid rgba(201,162,74,.22);
  font:10px var(--mono);letter-spacing:.06em;color:var(--teal-hi)}
#tip .tv i{color:var(--mut);font-style:normal}
.pmgrp{padding:6px 12px 2px;font:italic 9.5px var(--serif);letter-spacing:.18em;
  text-transform:uppercase;color:var(--brass-dk);cursor:default}
.pmgrp:hover{background:none}
</style>`);

/* ------------------------------------------------------------ 2 . markup */
rep(`      <button class="hb warn" id="b-panic">Panic</button>`,
    `      <button class="hb on" id="b-hints">Hints</button>
      <button class="hb warn" id="b-panic">Panic</button>`);
rep(`<div id="splash">`, `<div id="tip"></div>
<div id="splash">`);

/* -------------------------------------------------- 3 . DETUNE on the rail */
rep(`  ["spread","SPREAD",KP.PCT,0.3,"energy scatter between the balls, and their repulsion"],`,
    `  ["spread","SPREAD",KP.PCT,0.3,"energy scatter between the balls, and their repulsion"],
  ["detune","DETUNE",KP.PCT,0.0,"how far apart the balls' clocks run - fifty cents at full"],`);
rep(`["Voice",   ["voiceMode","unison","spread","width","glide","tune","ceiling","level","quality"]],`,
    `["Voice",   ["voiceMode","unison","spread","detune","width","glide","tune","ceiling","level","quality"]],`);

/* ------------------------------------------- 4 . the factory menu, grouped */
rep(`function openFactory(anchor){ const menu=pmMenu(0);
  if(!FACTORY.length) pmRow(menu,"— no factory patches —","pmdim");
  FACTORY.forEach((n,i)=>{ pmRow(menu,String(n),"",String(i+1)).addEventListener("click",e=>{ e.stopPropagation(); pmClose(0); NB.send({k:"factory",i}); say("factory: "+n); }); });`,
`function openFactory(anchor){ const menu=pmMenu(0);
  if(!FACTORY.length) pmRow(menu,"— no factory patches —","pmdim");
  let lastG=null;
  FACTORY.forEach((e,i)=>{
    if(e.g && e.g!==lastG){ lastG=e.g; const h=el("div","pmgrp",menu); h.textContent=e.g; }
    pmRow(menu,e.n,"",String(i+1)).addEventListener("click",ev=>{ ev.stopPropagation(); pmClose(0); NB.send({k:"factory",i}); say("factory: "+e.n); });
  });`);
rep(`FACTORY=(p.factory||[]).map(String);`,
    `FACTORY=(p.factory||[]).map(e=> typeof e==="string" ? {n:e,g:""} : {n:String(e&&e.n||""),g:String(e&&e.g||"")});`);
rep(`H.patch({name:FACT[i]||"?",user:false});`, `H.patch({name:(FACT[i]&&FACT[i].n)||FACT[i]||"?",user:false});`);
rep(`patch:{name:FACT[1],user:false}`, `patch:{name:(FACT[1]&&FACT[1].n)||FACT[1],user:false}`);

/* ------------------------------------------------------- 5 . the JS block */
const TIPS = `
/* ===========================================================================
   12 . the hints layer

   Every control says what it is and how it is used. The popup is placed
   BESIDE its control - to the right of the left rail, to the left of the
   right rail, below the header, above the timeline - so it never covers the
   thing being explained or the pointer explaining it.
   =========================================================================== */
const TIPS = {
  /* ---- BALL ---- */
  tapPos:["","The round tap. On its own a parabola gives a pure sine. Raise the other two taps for edge."],
  tapVel:["","The bright tap: the same motion heard as speed. A pit in the floor becomes a pulse here."],
  tapFrc:["","The hardest tap. It hears texture in the walls even while the ball's path stays smooth."],
  tone:["","Pull down for weight, open for air. It is after the taps, so it never changes the pitch."],
  strike:["","How much energy a full-velocity key puts in. At 100 % the ball reaches the top of the walls."],
  velSens:["","At 0 every key sounds the same. At 100 % a soft key stays in the floor of the bowl and sounds round."],
  friction:["","Zero holds the note forever. Raise it and the ball sinks while you hold: the sound closes on its own."],
  release:["","How fast the ball sinks once the key is up. This is the timbre dying; AMP RELEASE is the level dying."],
  servo:["","Leave at 0 for a tune-locked bowl. Raise it to about 70 % to hold a box or a pit in tune."],
  /* ---- TERRAIN ---- */
  position:["","Where the ball is towed when the pin lane is empty. Drag it and listen to the timbre travel."],
  hold:["","Low, the terrain wins and a ridge can strand the ball. High, the ball goes where the pins say."],
  tide:["","Raise it to flood the passes and let the ball travel; drop it to leave the ridges standing."],
  rock:["","Leave at 0 for a clean note. Raise it on a double well and the pitch drops an octave, then breaks up."],
  rockRatio:["","1/1 drives at the note. 1/2 and 1/3 are slower and rougher. FREE uses the ROCK RATE below."],
  rockHz:["","Only used when ROCK RATIO is FREE. Slow rates wobble; fast rates buzz."],
  rockMode:["","ROCK tilts the whole terrain like a see-saw. BREATH steepens and relaxes the bowls, and bows the note."],
  /* ---- VOICE ---- */
  voiceMode:["","POLY plays twelve. MONO restrikes every key. LEGATO only moves the pitch while a key is held."],
  unison:["","More balls in one bowl. Give them DETUNE and SPREAD or they sit exactly on top of each other."],
  spread:["","Scatters how hard each ball is struck, so they differ in brightness and phase, not in pitch."],
  detune:["","The classic unison spread. A few percent thickens; a quarter of the way is a wide chorus."],
  width:["","Stands the unison balls across the stereo field. It does nothing with UNISON at 1."],
  glide:["","Portamento for MONO and LEGATO. It has no effect in POLY."],
  tune:["","Transposes the whole instrument. Leave it at 0 unless you mean it."],
  ceiling:["","Where the output stops growing. Lower it for a harder, more compressed edge."],
  level:["","The output level. Patches are levelled against each other here."],
  quality:["","4X is the sound; 2X is the economy. Drop to 2X only if the CPU asks you to."],
  /* ---- AMP / MOD ---- */
  ampA:["","The level's rise. The ball is already moving; this is only how fast you hear it."],
  ampD:["","The level's fall to the sustain level."],
  ampS:["","The level held while the key is down. At 100 % the note's shape is the ball's own decay."],
  ampR:["","The level's fall after the key is up. RELEASE, in the ball section, closes the timbre at the same time."],
  modA:["","The mod envelope's rise."], modD:["","Its fall to the sustain level."],
  modS:["","The level it holds while the key is down."], modR:["","Its fall after the key is up."],
  modTarget:["","What the mod envelope moves. POSITION is the wavetable sweep; TIDE opens the passes."],
  modAmt:["","How far, and which way. Negative runs the target down instead of up."],
  /* ---- LFOs ---- */
  lfo1Rate:["","Cycles per second. Ignored when SYNC is not FREE."],
  lfo1Sync:["","FREE runs at the rate above; anything else locks to the host clock."],
  lfo1Shape:["","SINE and TRI wander; SAW and SQUARE step; S&H picks a new value each cycle."],
  lfo1Target:["","What it moves. On POSITION it walks the ball along the terrain by itself."],
  lfo1Amt:["","How far, and which way."],
  lfo2Rate:["","Cycles per second. Ignored when SYNC is not FREE."],
  lfo2Sync:["","FREE runs at the rate above; anything else locks to the host clock."],
  lfo2Shape:["","SINE and TRI wander; SAW and SQUARE step; S&H picks a new value each cycle."],
  lfo2Target:["","What it moves. Two LFOs on two targets is most of the movement in a pad."],
  lfo2Amt:["","How far, and which way."],
  /* ---- the sculpt tools ---- */
  "tool:push":["Raise the terrain under the brush.","Drag on a wall to steepen it, on the floor to lift the note out of the valley. With TUNE LOCK on the far wall follows and the pitch holds."],
  "tool:pull":["Lower the terrain under the brush.","Widen a bowl to make it darker and rounder. Pulling the floor down deepens the valley."],
  "tool:smooth":["Relax the terrain toward its neighbours.","The repair tool. Run it over anything that has gone spiky or noisy."],
  "tool:pit":["Dig a narrow pit in the floor.","The ball dives through it and comes out with a spike. Listen on the VELOCITY TAP: it is a pulse wave."],
  "tool:ridge":["Raise a whole band of columns.","This is a morph threshold across the position axis. Its height is the force it takes to cross."],
  "tool:tilt":["Slope a band of columns along the position axis.","A tilt is the direction the timbre drifts when you play hard."],
  "tool:mirror":["Mirror the column about its floor.","Removes the even harmonics. The quickest way back from a shrill bowl to a clean one."],
  "tool:stamp":["Drop a known bowl into the band.","Pick a shape below, then drag across the terrain. The fastest way to get a sound you recognise."],
  "tool:relief":["Edit the floor heights alone.","Drag the strip at the foot of the view. This is where you draw the passes between the valleys."],
  "tool:flatten":["Put the reference bowl back under the brush.","The undo of last resort for one region, without losing the rest."],
  "stamp:sine":["The reference bowl.","A pure sine, exactly in tune. The place to start and the place every note ends."],
  "stamp:box":["A flat floor with steep walls.","A triangle in position and a square in velocity. Not isochronous, so raise SERVO to hold the pitch."],
  "stamp:pitbowl":["A bowl with a pit in the floor.","A pulse twice a cycle. Bright and hollow, and it wants SERVO too."],
  "stamp:double":["Two pits with a hump between.","The chaos bowl. A soft key stays in one pit; a hard one spans both. Raise ROCK here."],
  "stamp:shelf":["A step halfway up one wall.","A timbre that only exists at mezzo-forte. Play softly and loudly to hear both."],
  "ui:radius":["The brush's reach.","It will not go below 0.04: the ball cannot see anything narrower, and what it half-sees is noise."],
  "ui:strength":["How hard the brush bites.","Low and slow for shaping; high for carving a ridge in one pass."],
  "ui:lock":["Keep the bowl exactly in tune while you sculpt.","On, the far wall follows so the width rule holds and the pitch cannot move. Off, the red on the terrain is the pitch bend a hard strike will produce."],
  "ui:generate":["Start from a whole new terrain.","Four grounds: six valleys, a cascade of double wells, the plain reference bowl, and valleys with grain."],
  "ui:undo":["Step back.","Ctrl-Z. It remembers terrain edits and pin edits alike."],
  "ui:redo":["Step forward again.","Ctrl-Y."],
  /* ---- header ---- */
  "btn:factory":["The built-in patches.","STARTERS are safe ground: familiar basses, leads, keys and pads, all in tune whatever the strike. TERRAINS are the instrument showing what it can do."],
  "btn:patches":["Your own saved patches.","Read from Documents\\\\Brokild patches\\\\High Tide. Subfolders appear as groups."],
  "btn:save":["Save everything as one patch.","The terrain, the pins, every control and the world rack go into a single JSON file."],
  "btn:open":["Open a patch file.","Anything saved with SAVE, from anywhere on disk."],
  "btn:pngin":["Read a terrain from an image.","Any 16-bit grey PNG becomes the landscape, so Photoshop or Blender is a terrain editor."],
  "btn:pngout":["Write the terrain out as an image.","A 16-bit grey PNG, 512 by 128. Edit it anywhere and read it back."],
  "btn:photo":["Turn a photograph into a landscape.","Each row of brightness becomes one bowl. Contrasty pictures make the most interesting terrain."],
  "btn:keys":["Show the keyboard.","Two octaves on screen. ZSXDCVGBHNJM and Q2W3ER5T6Y7U play them from the computer keyboard."],
  "btn:panic":["Silence everything now.","Every note off, every ball stopped."],
  "btn:hints":["These hint popups.","Turn them off once the instrument is familiar. The line under the terrain keeps explaining whatever you touch."],
  "btn:globe":["The Brokild World FX rack.","Twelve pedals after the voice, six characters that reach inside it, and five macros your host can automate."],
  "btn:home":["Put the camera back.","Right-drag or alt-drag orbits, the wheel zooms; this returns to the view you started with."],
  /* ---- timeline ---- */
  "lane:amp":["The output level over the note.","Drawn from the four AMP controls on the right. It is a plain VCA; the ball's own decay is the other half of the sound."],
  "lane:mod":["The mod envelope over the note.","Drawn from the four MOD controls. Give it a TARGET and an AMOUNT to make it do something."],
  "lane:lfo1":["LFO 1, drawn at its real rate.","Synced rates are drawn at 120 BPM. Set its target and amount on the right."],
  "lane:lfo2":["LFO 2, drawn at its real rate.","Two slow LFOs on different targets is most of what makes a pad move."],
  "lane:tide":["The tide over the note.","Click to add a point. An empty lane means the TIDE knob rules; points mean the lane owns it for the whole note."],
  "lane:rock":["The drive over the note.","Rising rock walks a note from a clean tone into period doubling and out the other side, the same way every time."],
  "lane:pin":["Where the ball is towed, moment by moment.","Click to add a pin, drag to move it, right-click to remove it, double-click to change its ease. Left of the red flag counts from the key going down, right of it from the key coming up. Shift-drag to set a loop."],
  /* ---- the view and the rest ---- */
  "ui:view":["The terrain, the water and the ball.","Left-drag sculpts with the current tool. Right-drag or alt-drag orbits, the wheel zooms, shift-click drops a ball where you click."],
  "ui:patchname":["What is loaded now.","A tag appears when it came from a file of your own rather than the factory."],
  "ui:kbd":["Two octaves of keyboard.","Click them, or use ZSXDCVGBHNJM and Q2W3ER5T6Y7U on the computer keyboard."]
};

const HINT = { on:true, el:null, key:null, node:null, timer:0 };
try { HINT.on = localStorage.getItem("highTide.hints") !== "0"; } catch(e){}

function tipKeyFor(node, ev){
  if(!node || !node.closest) return null;
  const ctl = node.closest(".ctl[data-id]"); if(ctl) return [ctl.dataset.id, ctl];
  const tool = node.closest("[data-tool]"); if(tool) return ["tool:"+tool.dataset.tool, tool];
  const st = node.closest("[data-stamp]"); if(st) return ["stamp:"+st.dataset.stamp, st];
  const idn = node.closest("[id]");
  if(idn){
    const id = idn.id;
    if(id==="c-radius") return ["ui:radius", idn];
    if(id==="c-strength") return ["ui:strength", idn];
    if(id==="sw-lock") return ["ui:lock", idn];
    if(id==="b-generate"||id==="b-undo"||id==="b-redo") return ["ui:"+id.slice(2), idn];
    if(id==="globe") return ["btn:globe", idn];
    if(id==="homebtn") return ["btn:home", idn];
    if(id==="patchname"||id==="pn"||id==="pntag") return ["ui:patchname", document.getElementById("patchname")];
    if(id==="kbd"||id==="keys") return ["ui:kbd", document.getElementById("kbd")];
    if(/^b-/.test(id)) return ["btn:"+id.slice(2), idn];
  }
  const tl = node.closest("#tl");
  if(tl && ev){
    const c=$("#tlc"), r=c.getBoundingClientRect(); tlLayout();
    const row=rowOf(ev.clientY-r.top);
    if(row) return ["lane:"+row.def.id, tl];
    return null;
  }
  if(node.closest("#view")) return ["ui:view", $("#view")];
  return null;
}

function tipHTML(key){
  const t = TIPS[key]; if(!t) return null;
  const p = P[key];
  const title = p ? p.name : (key.indexOf(":")>0 ? "" : key);
  let what = t[0] || (p ? (p.gloss||"") : "");
  let html = "";
  if(title) html += '<span class="tt">'+esc(title)+'</span>';
  if(what)  html += '<span class="tw">'+esc(what)+'</span>';
  if(t[1])  html += '<span class="th">'+esc(t[1])+'</span>';
  if(p){
    let v = '<span class="tv">'+esc(fmt(key, V[key]));
    if(p.def!==undefined && Math.abs(p.def-V[key])>1e-6) v += '  <i>default '+esc(fmt(key,p.def))+'</i>';
    html += v+'</span>';
  }
  return html;
}
function esc(x){ return String(x).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;"); }

/* placed BESIDE the control: the rails push sideways, the header down, the
   timeline up. Never under the pointer, never over the thing explained. */
function tipPlace(node){
  const tip=HINT.el, r=node.getBoundingClientRect();
  tip.style.left="0px"; tip.style.top="0px";                 // measure unclamped
  const w=tip.offsetWidth, h=tip.offsetHeight, M=10;
  let x, y;
  const inL = !!node.closest("#rail-l"), inR = !!node.closest("#rail-r");
  const inH = !!node.closest("#hdr"), inT = !!node.closest("#tl"), inV = !!node.closest("#view");
  if(inL){ const rl=$("#rail-l").getBoundingClientRect(); x=rl.right+M; y=r.top-4; }
  else if(inR){ const rr=$("#rail-r").getBoundingClientRect(); x=rr.left-w-M; y=r.top-4; }
  else if(inH){ x=r.left; y=r.bottom+M; }
  else if(inT){ const rt=$("#tl").getBoundingClientRect(); x=Math.min(r.left+40, innerWidth-w-M); y=rt.top-h-M; }
  else if(inV){ const rv=$("#view").getBoundingClientRect(); x=rv.left+M; y=rv.bottom-h-M; }
  else { x=r.left; y=r.bottom+M; }
  x=clamp(x, M, Math.max(M, innerWidth-w-M));
  y=clamp(y, M, Math.max(M, innerHeight-h-M));
  tip.style.left=Math.round(x)+"px"; tip.style.top=Math.round(y)+"px";
}
function tipShow(key, node){
  const html=tipHTML(key); if(!html) return;
  HINT.el.innerHTML=html; HINT.el.classList.add("on"); tipPlace(node);
}
function tipHide(){ HINT.key=null; HINT.node=null; clearTimeout(HINT.timer); HINT.timer=0; if(HINT.el) HINT.el.classList.remove("on"); }
function setHints(on){
  HINT.on=!!on; try{ localStorage.setItem("highTide.hints", on?"1":"0"); }catch(e){}
  const b=$("#b-hints"); if(b) b.classList.toggle("on", HINT.on);
  if(!HINT.on) tipHide();
  say(HINT.on ? "hints on: hover anything to be told what it is" : "hints off - the line under the terrain still explains what you touch");
}
function initHints(){
  HINT.el=$("#tip");
  $("#b-hints").addEventListener("click", ()=> setHints(!HINT.on));
  $("#b-hints").classList.toggle("on", HINT.on);
  addEventListener("pointermove", ev=>{
    if(!HINT.on || ev.buttons) { if(ev.buttons) tipHide(); return; }
    const hit=tipKeyFor(ev.target, ev);
    const key=hit?hit[0]:null;
    if(key===HINT.key){ return; }
    tipHide();
    if(!key || !TIPS[key]) return;
    HINT.key=key; HINT.node=hit[1];
    HINT.timer=setTimeout(()=>{ if(HINT.on && HINT.key===key) tipShow(key, hit[1]); }, 350);
  }, {passive:true});
  addEventListener("pointerdown", tipHide, {passive:true});
  addEventListener("wheel", tipHide, {passive:true});
  addEventListener("blur", tipHide);
}
`;
rep(`/* the probe hook: state lives in closures, so hand it out */`,
    TIPS + `\n/* the probe hook: state lives in closures, so hand it out */`);

/* the tip must be built after the markup exists, and re-placed on a resize */
rep(`function layout(){ glResize(); tlResize(); }`,
    `function layout(){ glResize(); tlResize(); if(HINT.el && HINT.key && HINT.node) tipPlace(HINT.node); }`);
rep(`  gl: ()=>GLV.gl, ledger: ()=>(LED.t>0?LED.msg:idleText())`,
    `  gl: ()=>GLV.gl, ledger: ()=>(LED.t>0?LED.msg:idleText()),
  tips: ()=>TIPS, hints: setHints,
  tipAt: (key,node)=>{ tipShow(key, node||document.body); const t=$("#tip").getBoundingClientRect(); return {on:$("#tip").classList.contains("on"), x:t.left, y:t.top, w:t.width, h:t.height, html:$("#tip").innerHTML}; },
  tipHide`);

/* start it with the rest of the page */
rep(`layout(); glInit(); buildRail(); paintPatch();`,
    `layout(); glInit(); buildRail(); paintPatch(); initHints();`);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("ui.html patched: hints layer, grouped factory menu, DETUNE on the rail");
