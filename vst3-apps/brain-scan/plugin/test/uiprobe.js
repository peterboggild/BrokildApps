/*  BRAIN SCAN — the panel probe.

    Static checks cannot see an unclosed tag, a helper that does not exist or a
    render loop that threw on its first frame (B2311.104's missing </script>
    passed node --check and executed nothing).  So this loads the real page in
    headless Chrome, feeds it the processor's own vocabulary — initialState, a
    synthetic volume, a lines payload, a view frame — and reads the answers
    back out of the DOM.

    node test/uiprobe.js [--keep]
*/
const fs = require("fs"), os = require("os"), path = require("path"), cp = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const UI = path.join(ROOT, "Source", "ui", "ui.html");
const BWFX = process.env.BWFX_DIR || "C:/Users/peter/b/BrokildWorldFX";

function findChrome(){
  const c = [
    process.env.CHROME,
    "C:/Program Files/Google/Chrome/Application/chrome.exe",
    "C:/Program Files (x86)/Google/Chrome/Application/chrome.exe",
    "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe",
    "C:/Program Files/Microsoft/Edge/Application/msedge.exe",
  ].filter(Boolean);
  for (const p of c) if (fs.existsSync(p)) return p;
  throw new Error("no chrome or edge found; set CHROME");
}

const PROBE = `
<script>
(function(){
  const R = { checks: [], err: null };
  const ok = (name, cond, detail) => R.checks.push({ name, pass: !!cond, detail: detail === undefined ? "" : String(detail) });
  try {
    /* ---- 1. the page is alive at all --------------------------------- */
    ok("no script error at boot", !window.__err, window.__err);
    ok("the bridge exists", typeof NB === "object" && typeof NB.send === "function");
    ok("the debug hooks exist", !!window.__BS);

    /* ---- 2. initialState, as the processor sends it ------------------- */
    const LSY = ["FREE","4/1","2/1","1/1","1/2","1/4","1/8","1/16","1/8T","1/16T"];
    const mk = (id,name,kind,v,gloss,lo,hi,list) => ({id,name,kind,v,def:v,gloss,lo:lo||0,hi:hi||0,list:list||undefined});
    const params = [
      mk("level","LEVEL",5,0.7,"output level"),
      mk("voices","VOICES",2,0,"polyphonic, or one voice with legato glide",0,1,["POLY","MONO"]),
      mk("unison","UNISON",2,0,"how many readers walk the same line",0,3,["1","2","3","4"]),
      mk("detune","DETUNE",8,0.25,"how far the unison readers are spread in pitch",0,40),
      mk("spread","SPREAD",0,0.5,"how wide the unison readers sit in the stereo field"),
      mk("glide","GLIDE",7,0,"portamento between notes"),
      mk("tune","TUNE",6,0.5,"master tuning"),
      mk("specimen","SPECIMEN",2,1/14,"the volume the lines read",0,14,["SINUS","SPINE","PULSE","MARROW","NERVE","RETINA","THORAX","SKULL","CORTEX","SUTURE","ENAMEL","TENDON","VERTEBRA","FEMUR","JAW"]),
      mk("grain","GRAIN",0,0.35,"how sharply the tissue is read: smoothed, or texel by texel"),
      mk("contrast","CONTRAST",0,0,"the audio window: narrow it and the read saturates, a sine toward a square"),
      mk("fold","FOLD",0,0,"what the window does past its edges: clip, or fold back in"),
      mk("head2","2ND HEAD",0,0,"a second reader on the same line, mixed in"),
      mk("head2Ratio","HEAD RATIO",9,0.5,"the second head's speed"),
      mk("head2Phase","HEAD PHASE",0,0,"where along the cycle the second head starts"),
      mk("uniScan","SCAN SPREAD",0,0,"unison readers spread through the scan"),
      mk("modContrast","MOD>WINDOW",3,0.5,"how much the MOD line narrows the window"),
      mk("modGrain","MOD>GRAIN",3,0.5,"how much the MOD line sharpens the read"),
      mk("scan","SCAN",0,0,"where between A and B every line is read"),
      mk("scanA","SCAN ATK",7,0.3,"how fast the scan envelope rises"),
      mk("scanD","SCAN DEC",7,0.55,"how fast it falls back"),
      mk("scanAmt","SCAN ENV",3,0.5,"how far the envelope moves the scan, either way"),
      mk("ftype","FILTER",2,0,"the filter's response",0,3,["LOW","BAND","HIGH","OFF"]),
      mk("fmodel","CIRCUIT",2,0,"the filter circuit",0,3,["SVF","GROWL","SCREAM","LADDER"]),
      mk("cutoff","CUTOFF",0,0.7,"the cutoff when the FILTER line has no say"),
      mk("reso","RESONANCE",0,0.2,"the filter's resonance"),
      mk("fdepth","F DEPTH",0,0,"how much the FILTER line decides the cutoff"),
      mk("fmode","F MODE",2,0,"read the FILTER line once per note, or keep looping it",0,1,["SWEEP","LOOP"]),
      mk("frate","F RATE",4,0.35,"the sweep time, or the loop rate",0.05,40),
      mk("fsync","F SYNC",2,0,"lock the loop to the host clock",0,9,LSY),
      mk("ftrack","KEY TRACK",0,0.5,"how much the cutoff follows the note"),
      mk("mrate","MOD RATE",4,0.3,"how fast the MOD line is read",0.02,30),
      mk("msync","MOD SYNC",2,0,"lock the MOD line to the host clock",0,9,LSY),
      mk("mscan","MOD>SCAN",3,0.5,"how much the MOD line moves the scan"),
      mk("mpitch","MOD>PITCH",3,0.5,"how much the MOD line bends the pitch"),
      mk("mpan","MOD>PAN",3,0.5,"how much the MOD line moves the voice in the field"),
      mk("ampA","ATTACK",7,0.15,"amplitude attack"),
      mk("ampD","DECAY",7,0.55,"amplitude decay"),
      mk("ampS","SUSTAIN",0,0.75,"amplitude sustain"),
      mk("ampR","RELEASE",7,0.45,"amplitude release"),
      mk("velsens","VELOCITY",0,0.6,"how much velocity decides the level")
    ];
    H.initialState({ params, build:"260904.1",
      factory:["ADMISSION","SINUS RHYTHM","CORTEX","PULSE OX","MARROW","NERVE","RETINAL","VENTILATOR","ARRHYTHMIA"],
      specimens:[{name:"SINUS",gloss:"a sine"},{name:"SPINE",gloss:"a stack"},{name:"PULSE",gloss:"a pulse"},
                 {name:"MARROW",gloss:"a ramp"},{name:"NERVE",gloss:"pm"},{name:"RETINA",gloss:"formants"},
                 {name:"LUNG",gloss:"noise"},{name:"SKULL",gloss:"a shell"},{name:"CORTEX",gloss:"the brain"},
                 {name:"SUTURE",gloss:"a staircase"},{name:"ENAMEL",gloss:"spikes"},{name:"TENDON",gloss:"partials"},
                 {name:"VERTEBRA",gloss:"lumbar"},{name:"FEMUR",gloss:"a thigh"},{name:"JAW",gloss:"teeth"}],
      vn:64, patch:{name:"ADMISSION", user:false} });
    ok("the console was built from initialState", document.querySelectorAll("#console .mod").length === 6,
       document.querySelectorAll("#console .mod").length + " modules");
    ok("every parameter reached a control", Object.keys(__BS.P).length === 40, Object.keys(__BS.P).length);
    const missing = __BS.hintless();
    ok("every parameter has a hint", missing.length === 0, missing.join(","));
    ok("the hint register is not doubled", __BS.hints() < 120, __BS.hints() + " hints");
    ok("the splash was hidden", document.getElementById("splash").classList.contains("hidden"));
    ok("the build id reached the about box", /260904\\.1/.test(document.getElementById("aboutBuild").textContent),
       document.getElementById("aboutBuild").textContent);

    /* ---- 3. a volume, exactly as emitVolume packs one ----------------- */
    const N = 64, vol = new Uint8Array(N*N*N);
    for (let k = 0; k < N; k++) for (let j = 0; j < N; j++) for (let i = 0; i < N; i++){
      const x = i/(N-1)-0.5, y = j/(N-1)-0.5, z = k/(N-1)-0.5;
      const r = Math.sqrt(x*x+y*y+z*z);
      vol[(k*N+j)*N+i] = Math.max(0, Math.min(255, Math.round(255*Math.exp(-Math.pow((r-0.30)/0.09,2)))));
    }
    let s = "";
    for (let i = 0; i < vol.length; i += 8192) s += String.fromCharCode.apply(null, vol.subarray(i, Math.min(i+8192, vol.length)));
    H.volume({ n:64, spec:8, name:"CORTEX", periodic:false, d:btoa(s) });
    ok("the volume arrived", __BS.VOL.spec === 8 && __BS.VOL.data[(32*64+32)*64+51] > 100,
       "centre-shell sample " + __BS.VOL.data[(32*64+32)*64+51]);
    ok("the specimen is named on the plate", document.getElementById("hdSpec").textContent === "CORTEX");

    /* ---- 4. lines, in the processor's flat encoding ------------------- */
    H.lines({ j:{ ver:1, l:[
      { pts:[0.04,0.5,0.5, 0.35,0.8,0.3, 0.7,0.2,0.7, 0.96,0.5,0.5], closed:false, start:0, warp:0 },
      { pts:[0.1,0.2,0.2, 0.5,0.9,0.5, 0.9,0.3,0.8], closed:true, start:0.25, warp:0.4 },
      { pts:[0.2,0.5,0.5, 0.8,0.5,0.5], closed:false, start:0, warp:0 },
      { pts:[0.2,0.2,0.8, 0.8,0.8,0.2], closed:false, start:0, warp:0 },
      { pts:[0.5,0.1,0.5, 0.5,0.9,0.5], closed:false, start:0, warp:0 },
      { pts:[0.3,0.3,0.3, 0.7,0.7,0.7], closed:false, start:0, warp:0 } ] } });
    ok("the lines arrived", __BS.LINES[0].pts.length === 4 && __BS.LINES[1].closed === true,
       __BS.LINES[0].pts.length + " points, B closed " + __BS.LINES[1].closed);
    ok("warp and start survived", Math.abs(__BS.LINES[1].warp - 0.4) < 1e-6 && Math.abs(__BS.LINES[1].start - 0.25) < 1e-6);
    ok("there are six line buttons", document.querySelectorAll(".lnbtn").length === 6);

    /*  the line is evaluated here the way the engine evaluates it: four
        collinear points must read as an exactly straight, unwarped line. */
    __BS.LINES[2].pts = [{x:0,y:0.5,z:0.5},{x:0.3333333,y:0.5,z:0.5},{x:0.6666667,y:0.5,z:0.5},{x:1,y:0.5,z:0.5}];
    let worst = 0;
    for (let i = 0; i < 40; i++){   /* not t = 1: the phase wraps there, and 0 is the right answer */
      const t = i/40, p = __BS.LINES[2];
      const q = (function(){ const o = {x:0,y:0,z:0}; return __BS.lineAt ? __BS.lineAt(p, t, o) : null; })();
      if (q) worst = Math.max(worst, Math.abs(q.x - t));
    }
    ok("a straight line reads straight (phantom end points)", worst < 0.002, "worst " + worst.toFixed(5));

    /* ---- 5. a view frame --------------------------------------------- */
    const f32 = n => { const a = new Float32Array(n); for (let i = 0; i < n; i++) a[i] = Math.sin(i/n*6.283); return a; };
    const tob64 = a => { const u = new Uint8Array(a.buffer); let t = "";
      for (let i = 0; i < u.length; i += 8192) t += String.fromCharCode.apply(null, u.subarray(i, Math.min(i+8192,u.length)));
      return btoa(t); };
    H.view({ lvl:0.4, w:tob64(f32(256)), f:tob64(f32(256)), m:tob64(f32(256)),
             v:[{ id:1, n:60, h:1, scan:0.42, cut:1234.5, fp:0.3, mp:0.7, mod:0.1, lvl:0.5 }] });
    ok("the vitals read the view", document.getElementById("vNote").textContent === "C4",
       document.getElementById("vNote").textContent);
    ok("the cutoff is shown as 1.23 kHz", /1.23 *kHz/.test(document.getElementById("vCut").textContent),
       document.getElementById("vCut").textContent);
    ok("the scan vital follows the voice", document.getElementById("vScan").textContent === "42 %",
       document.getElementById("vScan").textContent);

    /* ---- 6. it draws ------------------------------------------------- */
    __BS.bake();
    ok("the lines were baked into the emission volume", __BS.glowSum() > 10000, __BS.glowSum());
    __BS.draw();
    const tp = __BS.pixels("#tomoCv");
    ok("the tomography canvas has a picture", tp && tp.lit > tp.total*0.02, tp ? (100*tp.lit/tp.total).toFixed(1)+" % lit" : "no canvas");
    const mp = __BS.pixels("#monCv");
    ok("the monitor drew its leads", mp && mp.lit > 200, mp ? mp.lit + " lit" : "no canvas");
    ok("the gantry has a webgl2 context", !!(__BS.gl ? __BS.gl : document.getElementById("gantryCv").getContext("webgl2")));

    /* ---- 7. sending ---------------------------------------------------*/
    const sent = [];
    window.__MOCK = { recv: b => { for (const m of b) sent.push(m); } };
    __BS.P.cutoff.v = 0; document.querySelectorAll("#console .ctl")[0];
    __BS.NB.send({ k:"p", id:"cutoff", v:0.31 });
    __BS.LINES[0].pts[0].y = 0.9;
    (function(){ const b = document.querySelectorAll(".lnbtn")[3]; b.click(); })();
    ok("selecting a line moved the editor", __BS.SEL.i === 3, "line " + __BS.SEL.i);

    /* ---- 7b. the import strip ---------------------------------------- */
    ok("the import strip is on the console",
       document.querySelectorAll("#console .mod .impnote").length === 1 &&
       document.querySelectorAll("#console .mod:nth-child(1) .tinybtn").length === 2);
    (function(){
      var titles = __BS.hintTitles();
      var want = ["IMPORT","CLEAR","PHASE AXIS","SHAPE"], missing = [];
      for (var i = 0; i < want.length; i++) if (titles.indexOf(want[i]) < 0) missing.push(want[i]);
      ok("every import control carries a hint", missing.length === 0, missing.join(","));
    })();
    H.import({ on:true, name:"head.nii", note:"NIfTI-1  192x192x40  at 0.50 x 0.50 x 2.00 mm",
               err:"", axis:0, stretch:false, lo:-1000, hi:1100, filled:[64,64,53] });
    ok("an import event lights the strip and takes over the dial",
       __BS.IMP.on === true &&
       document.querySelector("#console .mod .step .cur").textContent === "IMPORTED",
       document.querySelector("#console .mod .step .cur").textContent);
    ok("and the note says where it came from",
       /head.nii/.test(document.querySelector(".impnote").textContent) &&
       /0.50/.test(document.querySelector(".impnote").textContent),
       document.querySelector(".impnote").textContent);
    H.import({ on:false, name:"", note:"", err:"that is not a NIfTI file.", axis:0, stretch:false,
               lo:0, hi:0, filled:[0,0,0] });
    ok("a refusal is shown, not swallowed",
       /not a NIfTI/.test(document.querySelector(".impnote").textContent) &&
       document.querySelector(".impnote").classList.contains("err"),
       document.querySelector(".impnote").textContent);
    ok("and the dial comes back",
       document.querySelector("#console .mod .step .cur").textContent !== "IMPORTED",
       document.querySelector("#console .mod .step .cur").textContent);

    /* ---- 8. no error accumulated ------------------------------------- */
    /* ---- 8. 260905.1: HU presets, split lines, flatten, undo, gantry handles ---- */
    {
      const pb = Array.from(document.querySelectorAll("#gantryWrap .tinybtn")).filter(b => /^(BRAIN|SOFT|LUNG|BONE)$/.test(b.textContent));
      ok("the four radiographer's windows are on the gantry", pb.length === 4, pb.length);
      ok("and they are dim on a phantom (no HU)", pb.every(b => b.classList.contains("dim")));
      let s8 = ""; for (let i = 0; i < 64*64*64; i++) s8 += String.fromCharCode((i % 97) === 0 ? 210 : 88);
      H.volume({ n:64, spec:7, name:"SKULL", periodic:false, d:btoa(s8), hu:[-1000, 2000], win:"BONE" });
      ok("a body opens on its own window: BONE, level 500 HU = 0.5 of the cube", Math.abs(__BS.GX.lev - 0.5) < 0.005, __BS.GX.lev.toFixed(3));
      ok("and the buttons light", pb.filter(b => !b.classList.contains("dim")).length === 4 && pb.some(b => b.classList.contains("on") && b.textContent === "BONE"));
      __BS.applyWindowPreset("BRAIN");
      ok("BRAIN: level 40 HU = 0.3467 of the cube, width 80 HU", Math.abs(__BS.GX.lev - 0.3467) < 0.002 && Math.abs(__BS.winWidth()*3000 - 80) < 2,
         __BS.GX.lev.toFixed(4) + " / W " + Math.round(__BS.winWidth()*3000));
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
    /* ---- 9. 260905.2: the circuit switch --------------------------------- */
    {
      const segs = Array.from(document.querySelectorAll("#console .mod .seg"));
      const circ = segs.find(s => Array.from(s.querySelectorAll("button")).map(b => b.textContent).join(",") === "SVF,GROWL,SCREAM,LADDER");
      ok("the FILTER module has the four circuits on one switch", !!circ);
      const ft = segs.find(s => Array.from(s.querySelectorAll("button")).map(b => b.textContent).join(",") === "LOW,BAND,HIGH,OFF");
      const band = ft && ft.querySelectorAll("button")[1];
      ok("BAND is lit on the SVF", !!band && !band.classList.contains("dim"));
      __BS.setParam("fmodel", 2/3);
      ok("and dims on a circuit that has no band (SCREAM)", !!band && band.classList.contains("dim"));
      __BS.setParam("fmodel", 0);
      ok("and comes back on the SVF", !!band && !band.classList.contains("dim"));
      /*  the console at the deck's own size: nothing may overflow its module —
          this is the check that would have caught the third row of knobs
          spilling under the keyboard */
      const boxes = Array.from(document.querySelectorAll("#console .mod, #lineMod"));
      const over = boxes.filter(m => m.scrollHeight > m.clientHeight + 1 || m.scrollWidth > m.clientWidth + 1)
                        .map(m => (m.querySelector(".plate") || {textContent:"?"}).textContent + " " + m.scrollHeight + "/" + m.clientHeight + " " + m.scrollWidth + "/" + m.clientWidth);
      ok("no module and not the line bench overflows at the deck's size", over.length === 0, over.join("; ") || (boxes.length + " boxes, " + Math.round(__BS.DECK.scale * 100) + " % scale"));
      const nm = Array.from(document.querySelectorAll("#console .ctl .nm")).filter(e => e.scrollWidth > e.clientWidth + 1).map(e => e.textContent);
      ok("every knob label fits its knob", nm.length === 0, nm.join(", "));
    }
    ok("no script error after the round trip", !window.__err, window.__err);
  } catch (e){ R.err = (e && e.stack) ? e.stack : String(e); }
  const out = document.createElement("div");
  out.id = "probe-out";
  out.textContent = JSON.stringify(R);
  document.body.appendChild(out);
})();
</script>
`;

function main(){
  const keep = process.argv.indexOf("--keep") >= 0;
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "bsprobe-"));
  let html = fs.readFileSync(UI, "utf8");
  if (html.indexOf("</script>") < 0) { console.error("FAIL  ui.html has no closing script tag"); process.exit(1); }
  html = html.replace("</body>", PROBE + "</body>");
  fs.writeFileSync(path.join(dir, "ui.html"), html);
  const rackSrc = path.join(BWFX, "ui", "bwfx-rack.js");
  if (fs.existsSync(rackSrc)) fs.copyFileSync(rackSrc, path.join(dir, "bwfx-rack.js"));
  else fs.writeFileSync(path.join(dir, "bwfx-rack.js"), "/* not found */");

  const chrome = findChrome();
  const args = ["--headless=new", "--disable-gpu-sandbox", "--no-sandbox", "--use-angle=swiftshader",
    "--enable-unsafe-swiftshader", "--allow-file-access-from-files", "--virtual-time-budget=6000",
    "--window-size=1400,940", "--user-data-dir=" + path.join(dir, "profile"),
    "--dump-dom", "file:///" + path.join(dir, "ui.html").replace(/\\/g, "/")];
  const r = cp.spawnSync(chrome, args, { encoding:"utf8", maxBuffer: 64*1024*1024 });
  const dom = r.stdout || "";
  const m = dom.match(/<div id="probe-out">([\s\S]*?)<\/div>/);
  if (!m){
    console.error("FAIL  the probe never ran — the page did not reach it.");
    const e = dom.match(/__err[^<]*/); if (e) console.error(e[0]);
    if (!keep) try { fs.rmSync(dir, { recursive:true, force:true }); } catch(x){}
    process.exit(1);
  }
  const dec = m[1].replace(/&quot;/g, '"').replace(/&amp;/g, "&").replace(/&lt;/g, "<").replace(/&gt;/g, ">");
  const R = JSON.parse(dec);
  let bad = 0;
  console.log("BRAIN SCAN - panel probe\n");
  for (const c of R.checks){
    if (!c.pass) bad++;
    console.log((c.pass ? "  ok   " : "  FAIL ") + c.name + (c.detail ? "   [" + c.detail + "]" : ""));
  }
  if (R.err){ bad++; console.log("\n  FAIL  the probe threw:\n" + R.err); }
  console.log("\n" + R.checks.length + " checks, " + bad + " failed  -  " + (bad ? "SEE ABOVE" : "ALL CLEAR"));
  if (keep) console.log("kept: " + dir);
  else try { fs.rmSync(dir, { recursive:true, force:true }); } catch(x){}
  process.exit(bad ? 1 : 0);
}
main();
