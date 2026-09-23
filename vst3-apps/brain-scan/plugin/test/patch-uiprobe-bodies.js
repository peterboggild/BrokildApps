// the panel probe for 260905.1: six more parameters, fifteen specimens, and a
// new section — presets in HU, split lines, flatten, undo, gantry handles.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "uiprobe.js");
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const misses = [];
const rep = (from, to, count = 1) => {
  const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
  const n = s.split(f).length - 1;
  if (n !== count) { misses.push(`expected ${count} of [${from.slice(0, 60)}...], found ${n}`); return; }
  s = s.split(f).join(t);
};
rep(String.raw`      mk("specimen","SPECIMEN",2,1/11,"the volume the lines read",0,11,["SINUS","SPINE","PULSE","MARROW","NERVE","RETINA","LUNG","SKULL","CORTEX","SUTURE","ENAMEL","TENDON"]),`,
    String.raw`      mk("specimen","SPECIMEN",2,1/14,"the volume the lines read",0,14,["SINUS","SPINE","PULSE","MARROW","NERVE","RETINA","THORAX","SKULL","CORTEX","SUTURE","ENAMEL","TENDON","VERTEBRA","FEMUR","JAW"]),`);
rep(String.raw`      mk("fold","FOLD",0,0,"what the window does past its edges: clip, or fold back in"),`,
    String.raw`      mk("fold","FOLD",0,0,"what the window does past its edges: clip, or fold back in"),
      mk("head2","2ND HEAD",0,0,"a second reader on the same line, mixed in"),
      mk("head2Ratio","HEAD RATIO",9,0.5,"the second head's speed"),
      mk("head2Phase","HEAD PHASE",0,0,"where along the cycle the second head starts"),
      mk("uniScan","SCAN SPREAD",0,0,"unison readers spread through the scan"),
      mk("modContrast","MOD>WINDOW",3,0.5,"how much the MOD line narrows the window"),
      mk("modGrain","MOD>GRAIN",3,0.5,"how much the MOD line sharpens the read"),`);
rep(String.raw`                 {name:"SUTURE",gloss:"a staircase"},{name:"ENAMEL",gloss:"spikes"},{name:"TENDON",gloss:"partials"}],`,
    String.raw`                 {name:"SUTURE",gloss:"a staircase"},{name:"ENAMEL",gloss:"spikes"},{name:"TENDON",gloss:"partials"},
                 {name:"VERTEBRA",gloss:"lumbar"},{name:"FEMUR",gloss:"a thigh"},{name:"JAW",gloss:"teeth"}],`);
rep(String.raw`    ok("every parameter reached a control", Object.keys(__BS.P).length === 33, Object.keys(__BS.P).length);`,
    String.raw`    ok("every parameter reached a control", Object.keys(__BS.P).length === 39, Object.keys(__BS.P).length);`);
rep(String.raw`    ok("no script error after the round trip", !window.__err, window.__err);`,
    String.raw`    /* ---- 8. 260905.1: HU presets, split lines, flatten, undo, gantry handles ---- */
    {
      const pb = Array.from(document.querySelectorAll("#gantryWrap .tinybtn")).filter(b => /^(BRAIN|SOFT|LUNG|BONE)$/.test(b.textContent));
      ok("the four radiographer's windows are on the gantry", pb.length === 4, pb.length);
      ok("and they are dim on a phantom (no HU)", pb.every(b => b.classList.contains("dim")));
      let s8 = ""; for (let i = 0; i < 64*64*64; i++) s8 += String.fromCharCode((i % 97) === 0 ? 210 : 88);
      H.volume({ n:64, spec:7, name:"SKULL", periodic:false, d:btoa(s8), hu:[-1000, 2000], win:"BONE" });
      ok("a body opens on its own window: BONE, level 500 HU = 0.5 of the cube", Math.abs(__BS.GX.lev - 0.5) < 0.005, __BS.GX.lev.toFixed(3));
      ok("and the buttons light", pb.filter(b => !b.classList.contains("dim")).length === 4 && pb.some(b => b.classList.contains("on") && b.textContent === "BONE"));
      __BS.applyWindowPreset("BRAIN");
      ok("BRAIN: level 40 HU = 0.3467 of the cube, width 80 HU", Math.abs(__BS.GX.lev - 0.3467) < 0.002 && Math.abs((0.06 + __BS.GX.win*1.1)*3000 - 80) < 2,
         __BS.GX.lev.toFixed(4) + " / W " + Math.round((0.06 + __BS.GX.win*1.1)*3000));
      const wv = document.querySelector("#windowSld .vl"), lv = document.querySelector("#levelSld .vl");
      ok("the sliders read in Hounsfield units on a body", !!wv && /HU/.test(wv.textContent) && !!lv && /HU/.test(lv.textContent), (wv && wv.textContent) + " / " + (lv && lv.textContent));
      /* a split line: two segments, the halves of the cycle */
      const L0 = __BS.LINES[0];
      const keep = JSON.parse(JSON.stringify(L0));
      L0.pts = [{x:.05,y:.3,z:0},{x:.5,y:.3,z:0},{x:.5,y:.9,z:0},{x:.95,y:.9,z:0}]; L0.split = 2; L0.closed = false; L0.warp = 0;
      const a = __BS.lineAt(L0, 0.49999), b = __BS.lineAt(L0, 0.5), e0 = __BS.lineAt(L0, 0), e1 = __BS.lineAt(L0, 0.99999);
      ok("a split line reads segment A to its end and jumps to segment B at half a cycle (the engine's numbers: y 0.300 -> 0.900 at x 0.500)",
         Math.abs(a.y - 0.3) < 0.01 && Math.abs(b.y - 0.9) < 0.01 && Math.abs(a.x - 0.5) < 0.01 && Math.abs(e0.x - 0.05) < 0.01 && Math.abs(e1.x - 0.95) < 0.01,
         a.x.toFixed(3) + "," + a.y.toFixed(3) + " -> " + b.x.toFixed(3) + "," + b.y.toFixed(3));
      ok("the slice draws a split line as two", (() => { try { __BS.TOMO.dirty = true; __BS.draw(); return true; } catch (e) { return false; } })());
      /* flatten, and take it back */
      __BS.pushUndo();
      __BS.flattenLine(0);
      ok("FLATTEN: two points, one curve", L0.pts.length === 2 && !__BS.isSplit(L0), L0.pts.length + " pts");
      __BS.undoLines();
      ok("UNDO: the four points and the split are back", L0.pts.length === 4 && __BS.isSplit(L0), L0.pts.length + " pts, split " + L0.split);
      Object.assign(L0, keep);
      /* gantry handles */
      __BS.drawGantryHandles();
      ok("the selected line's points are projected onto the gantry as handles", __BS.GANTRY_HANDLES.length === __BS.LINES[__BS.SEL.i].pts.length, __BS.GANTRY_HANDLES.length + " handles");
      ok("the line bench has FLATTEN, FLATTEN ALL, REVERT, UNDO and SPLIT", ["FLATTEN","FLATTEN ALL","REVERT","UNDO"].every(t => Array.from(document.querySelectorAll("#lineMod .tinybtn")).some(b => b.textContent === t)) && !!document.querySelector("#lineMod .mono"));
      ok("the tomography has a GRID switch", Array.from(document.querySelectorAll("#tomo .tinybtn")).some(b => b.textContent === "GRID"));
    }
    ok("no script error after the round trip", !window.__err, window.__err);`);
if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(p, s, "utf8");
console.log("uiprobe patched for 260905.1");
