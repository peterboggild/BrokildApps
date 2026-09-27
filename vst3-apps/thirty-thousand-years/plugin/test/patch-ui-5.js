/*  Panel round 5 — three faults that only LOOKING found, and none of them was
    visible to the probe as it stood.

    1. THE TITLE WAS UNDER THE PATCH WINDOW. "THIRTY THOUSAND YEARS" at 25 px
       with .115em tracking is about 334 px wide in a 326 px brand block, so the
       last letters ran under the patch panel: the instrument's own name was
       the one unreadable thing on the deck.

    2. THE DRONE PANEL CLIPPED ITS LAST CONTROL. Six controls need about 416 px
       in a 412 px panel, so FORCE wrapped onto a second row and the panel's
       overflow:hidden cut it in half. There were 68 px spare in that row.

    3. THE SPECTRAL RECORD WAS A 200x48 BITMAP STRETCHED OVER A 340x160 BOX.
       It read as a blue smear rather than strata: the one drawing on the panel
       that is supposed to be a record of what has been played. The canvas is
       sized from its wrapper now, and each band is drawn as a band, not a
       single pixel row.
*/
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/Source/ui/ui.html";
let s = fs.readFileSync(p, "utf8");

const edits = [
  // 1. the title
  [`#brand{position:relative;width:326px;flex:none;display:flex;flex-direction:column;
  justify-content:center;padding-left:2px}
#brand h1{margin:0;font:600 25px/22px var(--disp);letter-spacing:.115em;color:var(--bone);
  text-transform:uppercase;white-space:nowrap}`,
   `#brand{position:relative;width:360px;flex:none;display:flex;flex-direction:column;
  justify-content:center;padding-left:2px}
/*  The name has to fit its own block: at 25px/.115em it ran under the patch
    window and the instrument's title was the one thing you could not read. */
#brand h1{margin:0;font:600 23px/22px var(--disp);letter-spacing:.10em;color:var(--bone);
  text-transform:uppercase;white-space:nowrap}`],
  // 2. the drone panel
  [`  { host:"#p-drone", t:"DRONE", amb:1, w:412, c:[A("drone","DRONE",{cls:"big"}),A("drone_root","ROOT"),A("drone_chord","CHORD",W86),`,
   `  { host:"#p-drone", t:"DRONE", amb:1, w:470, c:[A("drone","DRONE",{cls:"big"}),A("drone_root","ROOT"),A("drone_chord","CHORD",W86),`],
  // 3. the spectral record
  [`function recCanvases() {
  RECW.forEach(function (w, i) {
    if (RECC[i]) return;
    var c = el("canvas", "", w); c.id = i === 0 ? "rec" : ""; c.style.cssText = "display:block;width:100%;height:100%;image-rendering:pixelated";
    c.width = 200; c.height = 48; RECC[i] = c;
  });
}
function drawRec() {
  recCanvases();
  var sp = M.spectrum; if (!sp || !sp.length) return;
  RECC.forEach(function (c) {
    if (c.parentNode && c.parentNode.offsetParent === null) return;
    var g = c.getContext("2d"), W = c.width, H = c.height;
    if (MOTION) g.drawImage(c, -1, 0);
    else g.clearRect(W - 1, 0, 1, H);
    for (var b = 0; b < 48; b++) {
      var v = clamp01(sp[b]);
      var y = H - 1 - Math.floor(b / 48 * H);
      g.fillStyle = "rgb(" + Math.round(10 + 85 * v) + "," + Math.round(14 + 170 * v) + "," + Math.round(18 + 185 * v) + ")";
      g.fillRect(W - 1, y, 1, 1);
    }
  });
  DRAWS.rec++;
}`,
   `/*  The record is a picture of what has been played, so it is drawn at the size
    it is shown: a 200x48 bitmap stretched over the box read as a smear. The
    bitmap is sized from the wrapper the first time the wrapper has a size, and
    again if the window is resized past a threshold. */
function recCanvases() {
  RECW.forEach(function (w, i) {
    if (!RECC[i]) {
      var c = el("canvas", "", w); c.id = i === 0 ? "rec" : "";
      c.style.cssText = "display:block;width:100%;height:100%";
      c.width = 2; c.height = 2; RECC[i] = c;
    }
    var cv = RECC[i], ww = w.clientWidth, hh = w.clientHeight;
    if (ww > 8 && hh > 8 && (Math.abs(cv.width - ww) > 8 || Math.abs(cv.height - hh) > 8)) {
      cv.width = ww; cv.height = hh;
      var g = cv.getContext("2d"); g.fillStyle = "#0a0c0e"; g.fillRect(0, 0, ww, hh);
    }
  });
}
function drawRec() {
  recCanvases();
  var sp = M.spectrum; if (!sp || !sp.length) return;
  RECC.forEach(function (c) {
    if (c.width < 8 || (c.parentNode && c.parentNode.offsetParent === null)) return;
    var g = c.getContext("2d"), W = c.width, H = c.height;
    if (MOTION) g.drawImage(c, -1, 0);
    var bh = H / 48;
    g.fillStyle = "#0a0c0e"; g.fillRect(W - 1, 0, 1, H);
    for (var b = 0; b < 48; b++) {
      var v = clamp01(sp[b]);
      if (v < 0.02) continue;
      g.fillStyle = "rgb(" + Math.round(8 + 88 * v) + "," + Math.round(12 + 172 * v) + "," + Math.round(16 + 188 * v) + ")";
      g.fillRect(W - 1, H - (b + 1) * bh, 1, Math.ceil(bh));
    }
  });
  DRAWS.rec++;
}`],
];

let miss = [];
for (const [a] of edits) { const n = s.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 60).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(p, s);
console.log("ui.html patched (" + edits.length + " edits)");
