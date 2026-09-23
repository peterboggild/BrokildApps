/*  ui.html — the import, on the console.

    It sits under SPECIMEN because that is what it replaces. The dial is not
    extended with a tenth entry (that would re-point every saved patch); an
    import OVERRIDES it, the window says IMPORTED while it does, and CLEAR
    gives the dial back.

    PHASE AXIS and FIT are shown always and greyed when there is nothing
    imported, so the module does not jump about when one arrives. They change
    how the source is READ, so moving either re-runs the import from the file
    it came from.

    node test/patch-ui-import.js
*/
const fs = require("fs"), path = require("path");
const FILE = path.resolve(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(FILE, "utf8");
const miss = [];
function rep(name, from, to){
  const n = s.split(from).length - 1;
  if (n !== 1){ miss.push(name + " (found " + n + ")"); return; }
  s = s.split(from).join(to);
}

/* ---- 1. a little CSS --------------------------------------------------- */
rep("css",
`/* ---------- keyboard ---------------------------------------------------- */`,
`.impnote{font:8.5px/1.25 Consolas,monospace;color:#5f636a;text-align:center;
  min-height:20px;max-height:20px;overflow:hidden;padding:0 2px;width:100%}
.impnote.err{color:#b4432c}
.dim{opacity:.42;pointer-events:none;filter:saturate(.3)}

/* ---------- keyboard ---------------------------------------------------- */`);

/* ---- 2. the strip, built with the SPECIMEN module ---------------------- */
rep("module 1",
`  const sc = el("div", null, m1); sc.style.width = "100%";
  mkSlider(sc, { id:"scan", name:"SCAN",`,
`  buildImportStrip(m1);
  const sc = el("div", null, m1); sc.style.width = "100%";
  mkSlider(sc, { id:"scan", name:"SCAN",`);

/* ---- 3. the strip itself ----------------------------------------------- */
rep("strip builder",
`/* ===========================================================================
   9 . the line bench — which line is being edited, and the tools for it.
   =========================================================================== */`,
`/*  An imported volume: a real body read off disk, resampled into the cube the
    lines read. It overrides the specimen dial rather than joining it. */
const IMP = { on:false, name:"", note:"", err:"", axis:0, stretch:false, lo:0, hi:0, filled:[0,0,0] };
let impBtns = null;
function buildImportStrip(parent){
  const w = el("div", null, parent); w.style.width = "100%";
  const r1 = el("div", "row tight", w);
  const bImp = el("button", "tinybtn", r1); bImp.textContent = "IMPORT…";
  const bClr = el("button", "tinybtn", r1); bClr.textContent = "CLEAR";
  bImp.addEventListener("click", () => { NB.send({ k:"import" }); say("choose a volume"); });
  bClr.addEventListener("click", () => NB.send({ k:"importClear" }));
  hint(bImp, "IMPORT",
    "read a real volume off disk and use it instead of the nine: a NIfTI file (.nii or .nii.gz), a folder of DICOM slices, or a folder of images.  It is resampled into the cube in millimetres, so an anisotropic scan is not squashed, and the window is chosen from the data so one metal clip cannot crush the tissue.");
  hint(bClr, "CLEAR", "throw the imported volume away and give the SPECIMEN dial back.");

  const r2 = el("div", "row tight", w);
  const axW = el("div", "segwrap", r2);
  const axL = el("div", "seglab", axW); axL.textContent = "PHASE AXIS";
  const axS = el("div", "seg", axW);
  const axB = ["X","Y","Z"].map((t, i) => {
    const b = el("button", null, axS); b.textContent = t;
    b.addEventListener("click", () => { IMP.axis = i; drawImport(); NB.send({ k:"importOpts", axis:i, stretch:IMP.stretch }); });
    return b;
  });
  hint(axW, "PHASE AXIS",
    "which axis of the body is read as one cycle of the waveform.  A head sounds quite different read left-to-right, front-to-back or foot-to-head, and this re-reads the file to change it.");
  const ftW = el("div", "segwrap", r2);
  const ftL = el("div", "seglab", ftW); ftL.textContent = "SHAPE";
  const ftS = el("div", "seg", ftW);
  const ftB = ["FIT","FILL"].map((t, i) => {
    const b = el("button", null, ftS); b.textContent = t;
    b.addEventListener("click", () => { IMP.stretch = (i === 1); drawImport(); NB.send({ k:"importOpts", axis:IMP.axis, stretch:IMP.stretch }); });
    return b;
  });
  hint(ftW, "SHAPE",
    "FIT keeps the body's real proportions and pads the rest of the cube with air, which reads as silence at those phases.  FILL stretches it to the cube, which is not anatomy but is often the more useful sound.");

  const note = el("div", "impnote", w);
  impBtns = { bImp, bClr, axW, axB, ftW, ftB, note };
  drawImport();
}
function drawImport(){
  if (!impBtns) return;
  const b = impBtns;
  b.axB.forEach((x, i) => x.classList.toggle("on", i === IMP.axis));
  b.ftB.forEach((x, i) => x.classList.toggle("on", (i === 1) === !!IMP.stretch));
  b.bClr.classList.toggle("dim", !IMP.on);
  b.axW.classList.toggle("dim", !IMP.on);
  b.ftW.classList.toggle("dim", !IMP.on);
  b.note.classList.toggle("err", !!IMP.err);
  if (IMP.err) b.note.textContent = IMP.err;
  else if (IMP.on) b.note.textContent = IMP.name + "  ·  " + IMP.note
      + "  ·  window " + Math.round(IMP.lo) + " to " + Math.round(IMP.hi);
  else b.note.textContent = "";
  const cur = document.querySelector("#console .mod .step .cur");
  if (cur){
    if (IMP.on){ cur.textContent = "IMPORTED"; cur.style.color = "#ffd79a"; }
    else { cur.style.color = ""; cur.textContent = fmt(P.specimen); }
  }
}

/* ===========================================================================
   9 . the line bench — which line is being edited, and the tools for it.
   =========================================================================== */`);

/* ---- 4. the specimen window must keep saying IMPORTED ------------------ */
rep("watcher",
`  const m5 = mkMod(1.25, "VOICE");`,
`  /*  registered after the stepper's own, so it runs last and the window keeps
      saying IMPORTED when the dial moves underneath it */
  onParam("specimen", () => drawImport());

  const m5 = mkMod(1.25, "VOICE");`);

/* ---- 5. the event ------------------------------------------------------ */
rep("handler",
`NB.on("patch", p => { $("#hdPatch").textContent = (p && p.name) ? p.name : "—"; });`,
`NB.on("import", p => {
  if (!p) return;
  IMP.on = !!p.on; IMP.name = p.name || ""; IMP.note = p.note || ""; IMP.err = p.err || "";
  IMP.axis = p.axis || 0; IMP.stretch = !!p.stretch;
  IMP.lo = p.lo || 0; IMP.hi = p.hi || 0;
  IMP.filled = p.filled || [0,0,0];
  drawImport();
});
NB.on("patch", p => { $("#hdPatch").textContent = (p && p.name) ? p.name : "—"; });`);

/* ---- 6. the debug hooks ------------------------------------------------ */
rep("debug",
`  setParam, selectLine, setPlane: i => {`,
`  IMP, drawImport,
  setParam, selectLine, setPlane: i => {`);

if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched ui.html");
