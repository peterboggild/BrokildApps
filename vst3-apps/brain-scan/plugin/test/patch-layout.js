// the console at the real deck size (1300 x 860): every module gets three
// knobs per row (a .ctl is 56 px, three of them 180, so a module needs 198 px
// with its padding), the line bench keeps its row count, and the panel probe
// now asserts that NO module and not the line bench overflows.
const fs = require("fs");
const path = require("path");
const misses = [];
function edit(file, fn) {
  let s = fs.readFileSync(file, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const rep = (from, to, count = 1) => {
    const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
    const n = s.split(f).length - 1;
    if (n !== count) { misses.push(`${path.basename(file)}: expected ${count} of [${from.slice(0, 70).replace(/\n/g, "\\n")}...], found ${n}`); return; }
    s = s.split(f).join(t);
  };
  fn(rep);
  return { file, get: () => s };
}
const ui = edit(path.join(__dirname, "..", "Source", "ui", "ui.html"), (rep) => {
  /*  widths: the console is about 1240 px for six modules; three knobs need
      198, and FILTER's circuit switch needs a little more */
  rep(String.raw`  const m1 = mkMod(1.15, "SPECIMEN · SCAN");`, String.raw`  const m1 = mkMod(1.13, "SPECIMEN · SCAN");`);
  rep(String.raw`  const m1b = mkMod(1.00, "READ · WINDOW");`, String.raw`  const m1b = mkMod(1.13, "READ · WINDOW");`);
  rep(String.raw`  const m2 = mkMod(1.30, "FILTER");`, String.raw`  const m2 = mkMod(1.25, "FILTER");`);
  rep(String.raw`  const m3 = mkMod(1.00, "MODULATOR");`, String.raw`  const m3 = mkMod(1.13, "MODULATOR");`);
  rep(String.raw`  const m4 = mkMod(1.20, "ENVELOPE");`, String.raw`  const m4 = mkMod(1.13, "ENVELOPE");`);
  rep(String.raw`  const m5 = mkMod(1.25, "VOICE");`, String.raw`  const m5 = mkMod(1.13, "VOICE");`);
  /*  MODULATOR: rate and sync side by side, so the two new knobs have a row */
  rep(String.raw`  const r5 = mkRow(m3); mkKnob(r5, "mrate");
  const r5b = mkRow(m3); mkStep(r5b, "msync", { width:120 });`,
      String.raw`  const r5 = mkRow(m3); mkKnob(r5, "mrate"); mkStep(r5, "msync", { width:118 });`);
  /*  VOICE: the knob label must fit 56 px */
  rep(String.raw`  mkKnob(r10, "uniScan", { hint:"unison readers spread through the SCAN as well as in pitch: each reads the body a little further along than the last, so a chord of readers widens without detuning" });`,
      String.raw`  mkKnob(r10, "uniScan", { name:"SCAN FAN", hint:"SCAN SPREAD — unison readers fan through the SCAN as well as in pitch: each reads the body a little further along than the last, so a chord of readers widens without detuning" });`);
  /*  the line bench: the resets join the RANDOM row instead of adding one */
  rep(String.raw`  const bRand = el("button", "tinybtn", r4); bRand.textContent = "RANDOMISE BOTH";`,
      String.raw`  const bRand = el("button", "tinybtn", r4); bRand.textContent = "RANDOM";`);
  rep(String.raw`  hint(bRand, "RANDOMISE BOTH", "draw new walks for both anchors of this scanner, so SCAN has somewhere to travel.");`,
      String.raw`  hint(bRand, "RANDOM", "draw new walks for both anchors of this scanner, so SCAN has somewhere to travel.");`);
  rep(String.raw`  /*  starting over, and taking it back */
  const r5 = el("div", "row tight", m);
  const bFlat = el("button", "tinybtn", r5); bFlat.textContent = "FLATTEN";`,
      String.raw`  /*  starting over, and taking it back — on the same row, the bench has no room for another */
  const r5 = r4;
  const bFlat = el("button", "tinybtn", r5); bFlat.textContent = "FLATTEN";`);
});
const probe = edit(path.join(__dirname, "uiprobe.js"), (rep) => {
  rep(String.raw`      const fm = document.querySelector("#console .mod:nth-child(3)");
      const r = fm && fm.getBoundingClientRect(), pr = fm && fm.parentElement.getBoundingClientRect();
      ok("the FILTER module still fits its column", !!r && r.bottom <= pr.bottom + 1 && fm.scrollHeight <= fm.clientHeight + 1, r ? Math.round(r.bottom) + " of " + Math.round(pr.bottom) + ", scroll " + fm.scrollHeight + " / " + fm.clientHeight : "no module");`,
      String.raw`      /*  the console at the deck's own size: nothing may overflow its module —
          this is the check that would have caught the third row of knobs
          spilling under the keyboard */
      const boxes = Array.from(document.querySelectorAll("#console .mod, #lineMod"));
      const over = boxes.filter(m => m.scrollHeight > m.clientHeight + 1 || m.scrollWidth > m.clientWidth + 1)
                        .map(m => (m.querySelector(".plate") || {textContent:"?"}).textContent + " " + m.scrollHeight + "/" + m.clientHeight + " " + m.scrollWidth + "/" + m.clientWidth);
      ok("no module and not the line bench overflows at the deck's size", over.length === 0, over.join("; ") || (boxes.length + " boxes, " + Math.round(__BS.DECK.scale * 100) + " % scale"));
      const nm = Array.from(document.querySelectorAll("#console .ctl .nm")).filter(e => e.scrollWidth > e.clientWidth + 1).map(e => e.textContent);
      ok("every knob label fits its knob", nm.length === 0, nm.join(", "));`);
});
if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [ui, probe]) fs.writeFileSync(f.file, f.get(), "utf8");
console.log("layout patched");
