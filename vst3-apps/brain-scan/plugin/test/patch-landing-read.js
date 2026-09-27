// 260904.3 — landing page + app.json: twelve specimens, the READ controls,
// the build id, the measured rows. Exact-count anchors.
const fs = require("fs");
const web = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/brain-scan";
const misses = [];
function edit(file, fn) {
  let s = fs.readFileSync(file, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const rep = (from, to, count = 1) => {
    const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
    const n = s.split(f).length - 1;
    if (n !== count) { misses.push(`${file.split("/").pop()}: expected ${count} of [${from.slice(0, 70)}...], found ${n}`); return; }
    s = s.split(f).join(t);
  };
  fn(rep);
  return { file, get: () => s };
}

const landing = edit(web + "/index.html", (rep) => {
  rep(String.raw`Nine specimens including a simulated brain, a tomography slice you draw the lines on, and a ray-marched gantry." />`,
      String.raw`Twelve specimens including a simulated brain and three made of edges, a read with its own grain and CT window, a tomography slice you draw the lines on, and a ray-marched gantry." />`);
  rep(String.raw`<span class="buildtag">Build 260904.2</span>`, String.raw`<span class="buildtag">Build 260904.3</span>`);
  rep(String.raw`<tr><td>Build</td><td>260904.2 — shown on the loading screen and in the About box</td></tr>`,
      String.raw`<tr><td>Build</td><td>260904.3 — shown on the loading screen and in the About box</td></tr>`);
  rep(String.raw`    <h2>Nine bodies to read</h2>
    <p class="sub">Seven fields built to give ordinary, useful waveforms; a hollow shell; and a brain.</p>`,
      String.raw`    <h2>Twelve bodies to read</h2>
    <p class="sub">Seven fields built to give ordinary, useful waveforms; three made of edges, spikes and beats; a hollow shell; and a brain.</p>`);
  rep(String.raw`      detail and a plateau. Move the line an inch and it meets them in a different order.</p>
    <div class="shot-row" style="display:flex;gap:1rem;flex-wrap:wrap">`,
      String.raw`      detail and a plateau. Move the line an inch and it meets them in a different order.</p>
    <p><b>Build 260904.3 doubled the volume and gave the read its own controls.</b> The cube is 128
      texels across (64 before), so a line carries twice the harmonics it did, and three specimens
      are made of edges: <b>SUTURE</b>, a random staircase; <b>ENAMEL</b>, a comb of spikes;
      <b>TENDON</b>, four partials sliding from harmonic to a bell's ratios. <b>GRAIN</b> chooses
      the read kernel — smoothed, or texel by texel, kinks included. <b>CONTRAST</b> is a CT window
      applied to the sound: narrow it and the read saturates toward a square, with more for the
      filter to bite on. <b>FOLD</b> folds the wave back past the window's edges. All of it
      anti-aliased and measured, none of it a tone control: at CONTRAST 0 and FOLD 0 the window is
      not there, and the plain read is the plain read.</p>
    <figure class="hero-shot" style="max-width:520px"><img src="img/read.jpg" alt="The READ · WINDOW module: GRAIN, CONTRAST and FOLD knobs"><figcaption>READ · WINDOW — the kernel, the window and the fold.</figcaption></figure>
    <div class="shot-row" style="display:flex;gap:1rem;flex-wrap:wrap">`);
  rep(String.raw`uses it instead of the nine — a CT or MR of a`, String.raw`uses it instead of the twelve — a CT or MR of a`);
  rep(String.raw`<p class="sub">Two benches, 62 checks, both in the source.</p>`, String.raw`<p class="sub">Two benches, 115 checks, both in the source.</p>`);
  rep(String.raw`      <tr><td>Cost</td><td>11.0 % of one core for eight voices of four-way unison — 32 readers, the maximum</td></tr>
      <tr><td>The panel</td><td>loaded in a real browser and driven through the plug-in's own message vocabulary: 24 checks, including that a straight line of four control points reads straight to five decimals</td></tr>`,
      String.raw`      <tr><td>Cost</td><td>12.6 % of one core for eight voices of four-way unison — 32 readers, the maximum; 14.9 % with GRAIN 1, the window and the fold all on</td></tr>
      <tr><td>GRAIN</td><td>bright SPINE at A2: harmonic 40 rises 4.6 dB from GRAIN 0 to 1 while harmonic 20 moves 1.3 dB — a kernel, not a tilt</td></tr>
      <tr><td>CONTRAST</td><td>SINUS THD &minus;120.6 / &minus;18.3 / &minus;10.3 / &minus;7.9 / &minus;6.9 dB at 0 / ¼ / ½ / ¾ / 1 — nothing at zero, monotonic toward a square</td></tr>
      <tr><td>The window's aliasing</td><td>brightest SPINE, GRAIN 1, C5: &minus;58.3 dB plain, &minus;51.2 dB with CONTRAST 1, &minus;64.9 dB with a full FOLD at gain 4</td></tr>
      <tr><td>The gritty three</td><td>energy above the 8th harmonic against the fundamental: SPINE's middle &minus;24.6 dB, SUTURE +4.4, ENAMEL +10.1</td></tr>
      <tr><td>The panel</td><td>loaded in a real browser and driven through the plug-in's own message vocabulary: 30 checks, including that a straight line of four control points reads straight to five decimals</td></tr>`);
  rep(String.raw`<tr><td>Specimens</td><td>Nine volumes built in, including a simulated brain</td></tr>`,
      String.raw`<tr><td>Specimens</td><td>Twelve volumes built in, 128 texels across, including a simulated brain and three made of edges</td></tr>`);
  rep(String.raw`<tr><td>Patches</td><td>Nine factory studies;`, String.raw`<tr><td>Patches</td><td>Twelve factory studies;`);
  rep(String.raw`<tr><td>Automation</td><td>30 parameters plus 5 rack macros. SPECIMEN is deliberately not automatable — it rebuilds a quarter of a million texels.</td></tr>`,
      String.raw`<tr><td>Automation</td><td>33 parameters plus 5 rack macros. SPECIMEN is deliberately not automatable — it rebuilds two million texels.</td></tr>`);
});

const app = edit(web + "/app.json", (rep) => {
  rep(String.raw`Nine specimens, seven of them fields built to give ordinary useful waveforms, plus a hollow shell and a simulated brain`,
      String.raw`Twelve specimens, 128 texels across: seven fields built to give ordinary useful waveforms, three made of edges and spikes, a hollow shell and a simulated brain`);
  rep(String.raw`uses it instead of the nine - a NIfTI file`, String.raw`uses it instead of the twelve - a NIfTI file`);
  rep(String.raw`in Hounsfield units. The Brokild World FX rack and five macros. Every claim measured: two benches, 62 checks."`,
      String.raw`in Hounsfield units. The read has its own controls - GRAIN picks the kernel, smoothed or texel by texel; CONTRAST is a CT window applied to the sound, saturating the read toward a square; FOLD folds it back past the edges - all anti-aliased. The Brokild World FX rack and five macros. Every claim measured: two benches, 115 checks."`);
  rep(String.raw`"note": "Windows VST3 - build 260904.2 - free download - 9 specimens plus your own volume`,
      String.raw`"note": "Windows VST3 - build 260904.3 - free download - 12 specimens at 128 texels plus your own volume`);
});

if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [landing, app]) fs.writeFileSync(f.file, f.get(), "utf8");
console.log("landing page + app.json patched for 260904.3");
