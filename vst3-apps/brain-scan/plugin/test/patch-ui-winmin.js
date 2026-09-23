// the window's floor: 0.06 of the cube was 180 HU on a body, so a BRAIN
// window (80 HU) could not be set. One constant pair now, floor 0.012 (36 HU).
const fs = require("fs");
const path = require("path");
const misses = [];
function edit(file, fn) {
  let s = fs.readFileSync(file, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const rep = (from, to, count = 1) => {
    const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
    const n = s.split(f).length - 1;
    if (n !== count) { misses.push(`${path.basename(file)}: expected ${count} of [${from.slice(0, 60)}...], found ${n}`); return; }
    s = s.split(f).join(t);
  };
  fn(rep);
  return { file, get: () => s };
}
const ui = edit(path.join(__dirname, "..", "Source", "ui", "ui.html"), (rep) => {
  rep(String.raw`const GX = { density:0.50, win:0.42, lev:0.55, band:false, preset:"" };`,
      String.raw`const GX = { density:0.50, win:0.42, lev:0.55, band:false, preset:"" };
/*  the window's width in cube units from the slider: a floor and a range.
    The floor was 0.06 (180 HU on a body) and a BRAIN window is 80 HU wide. */
const WIN_MIN = 0.012, WIN_RANGE = 1.15;
function winWidth(v){ return WIN_MIN + (v === undefined ? GX.win : v) * WIN_RANGE; }`);
  rep(String.raw`  const lo = (GX.lev - (0.06 + GX.win*1.1)*0.5) * 255, hi = (GX.lev + (0.06 + GX.win*1.1)*0.5) * 255;`,
      String.raw`  const lo = (GX.lev - winWidth()*0.5) * 255, hi = (GX.lev + winWidth()*0.5) * 255;`);
  rep(String.raw`  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uWin"), 0.06 + GX.win * 1.1);`,
      String.raw`  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uWin"), winWidth());`);
  rep(String.raw`    fmt: v => VOL.hu ? "W " + Math.round((0.06 + v*1.1) * huSpan()) + " HU" : (0.06 + v*1.1).toFixed(2),`,
      String.raw`    fmt: v => VOL.hu ? "W " + Math.round(winWidth(v) * huSpan()) + " HU" : winWidth(v).toFixed(3),`);
  rep(String.raw`  GX.win = clamp((win - 0.06) / 1.1, 0, 1);`,
      String.raw`  GX.win = clamp((win - WIN_MIN) / WIN_RANGE, 0, 1);`);
  rep(String.raw`  GX.win = clamp((W / span - 0.06) / 1.1, 0, 1);`,
      String.raw`  GX.win = clamp((W / span - WIN_MIN) / WIN_RANGE, 0, 1);`);
  rep(String.raw`  applyWindowPreset, WIN_PRESETS, drawPresets,`,
      String.raw`  applyWindowPreset, WIN_PRESETS, drawPresets, winWidth,`);
});
const probe = edit(path.join(__dirname, "uiprobe.js"), (rep) => {
  rep(String.raw`Math.abs((0.06 + __BS.GX.win*1.1)*3000 - 80) < 2,
         __BS.GX.lev.toFixed(4) + " / W " + Math.round((0.06 + __BS.GX.win*1.1)*3000));`,
      String.raw`Math.abs(__BS.winWidth()*3000 - 80) < 2,
         __BS.GX.lev.toFixed(4) + " / W " + Math.round(__BS.winWidth()*3000));`);
});
if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [ui, probe]) fs.writeFileSync(f.file, f.get(), "utf8");
console.log("window floor patched");
