// 260905.1 — landing page + app.json: the bodies, the second head, the resets.
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
  rep(String.raw`Twelve specimens including a simulated brain and three made of edges, a read with its own grain and CT window, a tomography slice you draw the lines on, and a ray-marched gantry." />`,
      String.raw`Fifteen specimens: nine phantoms built from formulas and six bodies in Hounsfield units - a head, a bare skull, a chest, three vertebrae, a thigh, a jaw - with a radiographer's window presets, a second reading head for inharmonic partials, split lines, and a tomography slice you draw the lines on." />`);
  rep(String.raw`<span class="buildtag">Build 260904.3</span>`, String.raw`<span class="buildtag">Build 260905.1</span>`);
  rep(String.raw`<tr><td>Build</td><td>260904.3 — shown on the loading screen and in the About box</td></tr>`,
      String.raw`<tr><td>Build</td><td>260905.1 — shown on the loading screen and in the About box</td></tr>`);
  rep(String.raw`    <h2>Twelve bodies to read</h2>
    <p class="sub">Seven fields built to give ordinary, useful waveforms; three made of edges, spikes and beats; a hollow shell; and a brain.</p>`,
      String.raw`    <h2>Fifteen specimens: nine phantoms and six bodies</h2>
    <p class="sub">The phantoms are formulas and promises; the bodies are anatomy in Hounsfield units, and a bone is a bone.</p>`);
  rep(String.raw`    <p><b>CORTEX</b> is the show-piece: a simulated brain with skull, cerebrospinal fluid, folded cortex,
      the longitudinal fissure, ventricles, cerebellum and stem. A line through it reads bone, then
      fluid, then folds, then a ventricle — a waveform with a hard edge, a smooth stretch, a burst of
      detail and a plateau. Move the line an inch and it meets them in a different order.</p>`,
      String.raw`    <p><b>The bodies (260905.1)</b> are built the way a scanner would record them — air at &minus;1000 HU,
      fat about &minus;90, soft tissue 40, trabecular bone a few hundred, the cortical tables above a thousand,
      enamel at the top of the scale. <b>CORTEX</b> is a head: skull with its tables and sutures, orbits with
      globes, sinuses, folded cortex, ventricles, cerebellum, brainstem, the jaw and both rows of teeth.
      <b>SKULL</b> is the same head with every soft tissue removed. <b>THORAX</b> has ribs, sternum, clavicles,
      a spine with its canal, two lungs with bronchi and vessels, the heart and the aorta. <b>VERTEBRA</b> is
      three lumbar levels with discs, canal, processes and the muscles round them; <b>FEMUR</b> a thigh with
      the shaft flaring into its ends; <b>JAW</b> the mandible with teeth of enamel, dentine and pulp. A line
      through any of them reads what is there — bone, then fluid, then folds, then a ventricle — and moving it
      an inch meets them in a different order. WINDOW and LEVEL read in HU on a body, and four presets on the
      gantry are a radiographer's: BRAIN, SOFT, LUNG, BONE.</p>
    <div class="shot-row" style="display:flex;gap:1rem;flex-wrap:wrap">
      <figure class="hero-shot" style="flex:1;min-width:220px"><img src="img/skull-gantry.jpg" alt="SKULL in the gantry, bone window"><figcaption>SKULL in the gantry, bone window.</figcaption></figure>
      <figure class="hero-shot" style="flex:1;min-width:220px"><img src="img/cortex-axial.jpg" alt="CORTEX axial slice, brain window"><figcaption>CORTEX, axial, brain window.</figcaption></figure>
      <figure class="hero-shot" style="flex:1;min-width:220px"><img src="img/jaw-axial.jpg" alt="JAW axial slice through the lower teeth"><figcaption>JAW, axial through the lower teeth.</figcaption></figure>
    </div>`);
  rep(String.raw`    <p><b>Build 260904.3 doubled the volume and gave the read its own controls.</b> The cube is 128
      texels across (64 before), so a line carries twice the harmonics it did, and three specimens
      are made of edges: <b>SUTURE</b>, a random staircase; <b>ENAMEL</b>, a comb of spikes;
      <b>TENDON</b>, four partials sliding from harmonic to a bell's ratios. <b>GRAIN</b> chooses
      the read kernel — smoothed, or texel by texel, kinks included. <b>CONTRAST</b> is a CT window
      applied to the sound: narrow it and the read saturates toward a square, with more for the
      filter to bite on. <b>FOLD</b> folds the wave back past the window's edges. All of it
      anti-aliased and measured, none of it a tone control: at CONTRAST 0 and FOLD 0 the window is
      not there, and the plain read is the plain read.</p>`,
      String.raw`    <p><b>The read has its own controls.</b> The cube is 128 texels across, so a line carries 64
      harmonics; three phantoms are made of edges: <b>SUTURE</b>, a random staircase; <b>ENAMEL</b>, a
      comb of spikes; <b>TENDON</b>, four partials at a bell's ratios. <b>GRAIN</b> chooses the read
      kernel — smoothed, or texel by texel, kinks included. <b>CONTRAST</b> is a CT window applied to the
      sound: narrow it and the read saturates toward a square. <b>FOLD</b> folds the wave back past the
      window's edges. And a <b>2ND HEAD</b> reads the same line at a ratio of the note: at ×1.00 with a
      phase offset it is a fixed-interval double, off the whole numbers it is a partial no harmonic series
      contains — real inharmonicity, which a single-cycle read can never give on its own; the bench
      measures a partial at 1.5 f<sub>0</sub> within 0.3 dB of the fundamental and no second harmonic. A
      line can be <b>SPLIT</b> into two segments (two edges a cycle, both band-limited), the MOD line can
      drive the window and the grain, unison readers can spread through the scan, and FLATTEN, REVERT and
      UNDO take you back to a simple state to build up from again. The selected line's points can be
      dragged on the gantry too — with shift, along the line of sight.</p>`);
  rep(String.raw`uses it instead of the twelve — a CT or MR of a`, String.raw`uses it instead of the fifteen — a CT or MR of a`);
  rep(String.raw`<p class="sub">Two benches, 115 checks, both in the source.</p>`, String.raw`<p class="sub">Two benches, 161 checks, both in the source.</p>`);
  rep(String.raw`      <tr><td>Cost</td><td>12.6 % of one core for eight voices of four-way unison — 32 readers, the maximum; 14.9 % with GRAIN 1, the window and the fold all on</td></tr>`,
      String.raw`      <tr><td>Cost</td><td>16.7 % of one core for eight voices of four-way unison — 32 readers, the maximum; 18.5 % with GRAIN 1, the window and the fold all on; 39 % with a second head on every reader</td></tr>
      <tr><td>The second head</td><td>at ×1.00 and PHASE 50 % the odd harmonics fall from +3.9 dB over the even to &minus;106 dB under; at ×1.50 a partial at 1.5 f<sub>0</sub> sits 0.3 dB from the fundamental with no 2nd harmonic (&minus;111 dB)</td></tr>
      <tr><td>A split line</td><td>at C5, both edges band-limited: &minus;64.3 dB of non-harmonic energy, &minus;31.8 without the second polyBLEP</td></tr>
      <tr><td>The bodies</td><td>tissue fractions per body; five rays from the centre of the head all cross bone; SKULL's cranium is empty and CORTEX's holds white matter; the chest has two lungs; the vertebra a canal of CSF between two pedicles</td></tr>`);
  rep(String.raw`      <tr><td>The panel</td><td>loaded in a real browser and driven through the plug-in's own message vocabulary: 30 checks, including that a straight line of four control points reads straight to five decimals</td></tr>`,
      String.raw`      <tr><td>The panel</td><td>loaded in a real browser and driven through the plug-in's own message vocabulary: 43 checks, including that a straight line of four control points reads straight to five decimals and that a split line jumps segments at exactly half a cycle, as the engine does</td></tr>`);
  rep(String.raw`<tr><td>Specimens</td><td>Twelve volumes built in, 128 texels across, including a simulated brain and three made of edges</td></tr>`,
      String.raw`<tr><td>Specimens</td><td>Fifteen volumes built in, 128 texels across: nine phantoms and six bodies in Hounsfield units</td></tr>`);
  rep(String.raw`<tr><td>Patches</td><td>Twelve factory studies;`, String.raw`<tr><td>Patches</td><td>Sixteen factory studies;`);
  rep(String.raw`<tr><td>Automation</td><td>33 parameters plus 5 rack macros. SPECIMEN is deliberately not automatable — it rebuilds two million texels.</td></tr>`,
      String.raw`<tr><td>Automation</td><td>39 parameters plus 5 rack macros. SPECIMEN is deliberately not automatable — it rebuilds two million texels.</td></tr>`);
});
const app = edit(web + "/app.json", (rep) => {
  rep(String.raw`Twelve specimens, 128 texels across: seven fields built to give ordinary useful waveforms, three made of edges and spikes, a hollow shell and a simulated brain`,
      String.raw`Fifteen specimens, 128 texels across: nine phantoms built from formulas (six for ordinary useful waveforms, three made of edges and spikes) and six bodies in Hounsfield units - a head, the same head as bare skull, a chest with ribs and lungs, three vertebrae, a thigh, a jaw with teeth - with a radiographer's window presets`);
  rep(String.raw`uses it instead of the twelve - a NIfTI file`, String.raw`uses it instead of the fifteen - a NIfTI file`);
  rep(String.raw`FOLD folds it back past the edges - all anti-aliased. The Brokild World FX rack and five macros. Every claim measured: two benches, 115 checks."`,
      String.raw`FOLD folds it back past the edges; a 2ND HEAD reads the same line at a ratio of the note for real inharmonic partials; lines can be split into two segments, flattened, reverted and undone, and dragged on the gantry - all anti-aliased. The Brokild World FX rack and five macros. Every claim measured: two benches, 161 checks."`);
  rep(String.raw`"note": "Windows VST3 - build 260904.3 - free download - 12 specimens at 128 texels plus your own volume`,
      String.raw`"note": "Windows VST3 - build 260905.1 - free download - 15 specimens (nine phantoms, six bodies in HU) plus your own volume`);
});
if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [landing, app]) fs.writeFileSync(f.file, f.get(), "utf8");
console.log("landing page + app.json patched for 260905.1");
