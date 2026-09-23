/*  manual.html and the landing page — the import.

    node test/patch-docs-import.js
*/
const fs = require("fs"), path = require("path");
const MAN = path.resolve(__dirname, "..", "docs", "manual", "manual.html");
const WEB = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/brain-scan/index.html";
const miss = [];
function patch(file, name, from, to){
  let s = fs.readFileSync(file, "utf8");
  const n = s.split(from).length - 1;
  if (n !== 1){ miss.push(path.basename(file) + " / " + name + " (found " + n + ")"); return; }
  fs.writeFileSync(file, s.split(from).join(to));
}

//==============================================================================
//  MANUAL — a subsection of 03, bench lines in 09, reference rows in 10
patch(MAN, "specimens section",
`  <div class="trip">
    <figure><img src="img/spine-slice.jpg" alt="SPINE"><figcaption>SPINE — a harmonic stack, sliced</figcaption></figure>
    <figure><img src="img/pulse-slice.jpg" alt="PULSE"><figcaption>PULSE — width across y</figcaption></figure>
    <figure><img src="img/scan0.jpg" alt="CORTEX"><figcaption>CORTEX — the simulated brain</figcaption></figure>
  </div>
</div>`,
`  <div class="trip">
    <figure><img src="img/spine-slice.jpg" alt="SPINE"><figcaption>SPINE — a harmonic stack, sliced</figcaption></figure>
    <figure><img src="img/pulse-slice.jpg" alt="PULSE"><figcaption>PULSE — width across y</figcaption></figure>
    <figure><img src="img/scan0.jpg" alt="CORTEX"><figcaption>CORTEX — the simulated brain</figcaption></figure>
  </div>

  <div class="cols2" style="margin-top:5mm">
    <h3>A volume of your own</h3>
    <p><b>IMPORT</b>, under the specimen window, reads a real volume off disk and uses it instead
    of the nine. It takes a <b>NIfTI</b> file (<code>.nii</code> or <code>.nii.gz</code>), a folder
    of <b>DICOM</b> slices, or a folder of images. Any volumetric data will do — a CT or MR of a
    head, a micro-CT of a fossil, a confocal stack, an industrial scan of a casting.</p>
    <p>It does not join the nine: it <em>overrides</em> them. The window says IMPORTED while it is
    in use and <b>CLEAR</b> gives the dial back. That is deliberate — adding a tenth entry to the
    dial would have re-pointed every patch anyone had already saved.</p>
    <h4>What it does to the data, and why</h4>
    <p><b>It resamples in millimetres, not in voxels.</b> A clinical CT is typically 0.5 mm across
    a slice and 1–5 mm between them. Resampling by index would squash the body along the slice
    axis by exactly that ratio. The spacing is read from the file, the physical extents are fitted
    into the cube, and a sphere stays a sphere.</p>
    <p><b>It averages when it shrinks.</b> 512 × 512 × 300 into 64³ is about 8 × 8 × 5 source
    voxels for every destination voxel. Point-sampling that is aliasing, and aliasing introduced
    here is in the specimen for good — no later filtering can reach it. Each axis is resampled with
    a filter as wide as that axis's own ratio.</p>
    <p><b>It picks the window from the data.</b> A single surgical clip at 3000 HU would otherwise
    push every brain voxel into the bottom two per cent of the range. The window is a percentile of
    the data rather than its extremes, and the values it chose are printed under the strip in the
    file's own units — Hounsfield units, for a CT.</p>
    <h4>The two controls</h4>
    <p><b>PHASE AXIS</b> chooses which axis of the body is read as one cycle. A head read
    left-to-right, front-to-back and foot-to-head are three quite different instruments.
    <b>SHAPE</b> chooses FIT, which keeps the body's real proportions and pads the rest of the cube
    with air — silence at those phases — or FILL, which stretches it to the cube. Both re-read the
    file, because they change how it is read rather than how it is shown.</p>
    <div class="callout"><b>The window controls are now literal.</b> With a CT loaded, WINDOW and
    LEVEL are a real radiographer's window width and level, in Hounsfield units: air is −1000,
    cerebrospinal fluid about 0 to 15, grey matter 35 to 45, bone 300 and up. A brain window is
    about 80 wide centred on 40, and it is the only way to see cortical detail — at a window wide
    enough to hold the skull, the tissue is all one grey.</div>
    <p>An imported volume is <em>saved with the patch and with the project</em>, compressed, so a
    patch is still the whole machine when the folder it came from has been tidied away.</p>
  </div>
  <div class="plate"><img src="img/import.jpg" style="max-width:78mm" alt="The import strip">
    <div class="cap">IMPORTED in the window, and what the file turned out to be underneath.</div></div>
  <div class="trip">
    <figure><img src="img/import-slice.jpg" alt="an imported CT"><figcaption>An imported scan, at the window the data chose</figcaption></figure>
    <figure><img src="img/import-window.jpg" alt="a brain window"><figcaption>The same slice, narrowed to a brain window</figcaption></figure>
    <figure><img src="img/import-gantry.jpg" alt="the imported volume in the gantry"><figcaption>And in the gantry, with the lines inside it</figcaption></figure>
  </div>
</div>`);

patch(MAN, "bench table",
`      <tr><td>Cost</td><td>11.0 % of one core for eight voices of four-way unison — 32 readers, the
        maximum</td></tr>
    </table>`,
`      <tr><td>Cost</td><td>11.0 % of one core for eight voices of four-way unison — 32 readers, the
        maximum</td></tr>
      <tr><td><b>The import</b></td><td>a volume that is 80 × 80 × 20 voxels but physically cubic
        fills the cube, and a sphere in it still measures the same on all three axes — resampling
        by index would read about 12 voxels on the coarse one</td></tr>
      <tr><td>Decimation</td><td>a one-voxel checkerboard, decimated 4:1, flattens to standard
        deviation 0.00005; point sampling would leave 0.5</td></tr>
      <tr><td>No shift</td><td>a ramp resamples to a ramp within 0.006 — the half-voxel error that
        breaks a resampler without looking broken</td></tr>
      <tr><td>Outliers</td><td>with one voxel at 30000 among values of 0 to 100, the window comes
        out 0 to 103</td></tr>
      <tr><td>Readers</td><td>NIfTI and DICOM round-tripped from bytes built in memory: dimensions,
        spacing, rescale to Hounsfield units, and slices ordered by their position rather than the
        order they arrived. Compressed DICOM and NIfTI-2 are refused by name.</td></tr>
    </table>`);

patch(MAN, "reference",
`    <h4>The screens</h4>`,
`    <h4>IMPORT</h4>
    <table>
      <tr><td>IMPORT…</td><td>read a NIfTI file, a DICOM folder or a folder of images</td></tr>
      <tr><td>CLEAR</td><td>throw it away and give the SPECIMEN dial back</td></tr>
      <tr><td>PHASE AXIS</td><td>which axis of the body is one cycle of the waveform</td></tr>
      <tr><td>SHAPE</td><td>FIT keeps the body's proportions; FILL stretches it to the cube</td></tr>
    </table>

    <h4>The screens</h4>`);

//==============================================================================
//  LANDING PAGE
patch(WEB, "landing block",
`    <div class="shot-row" style="display:flex;gap:1rem;flex-wrap:wrap">
      <figure class="hero-shot" style="flex:1;min-width:260px"><img src="img/gantry.jpg" alt="The gantry: a ray-marched brain with the scan lines glowing inside it"><figcaption>CORTEX in the gantry, the lines glowing inside the tissue.</figcaption></figure>
      <figure class="hero-shot" style="flex:1;min-width:260px"><img src="img/surface.jpg" alt="The gantry in SURFACE mode showing one level of a waveform field"><figcaption>SPINE in SURFACE — one level of the field, as a surface.</figcaption></figure>
    </div>
  </div>
</section>`,
`    <div class="shot-row" style="display:flex;gap:1rem;flex-wrap:wrap">
      <figure class="hero-shot" style="flex:1;min-width:260px"><img src="img/gantry.jpg" alt="The gantry: a ray-marched brain with the scan lines glowing inside it"><figcaption>CORTEX in the gantry, the lines glowing inside the tissue.</figcaption></figure>
      <figure class="hero-shot" style="flex:1;min-width:260px"><img src="img/surface.jpg" alt="The gantry in SURFACE mode showing one level of a waveform field"><figcaption>SPINE in SURFACE — one level of the field, as a surface.</figcaption></figure>
    </div>
  </div>
</section>

<section class="block">
  <div class="wrap">
    <h2>Or bring your own body</h2>
    <p class="sub">A NIfTI file, a folder of DICOM, or a stack of images.</p>
    <p><b>IMPORT</b> reads a real volume off disk and uses it instead of the nine — a CT or MR of a
      head, a micro-CT of a fossil, a confocal stack, an industrial scan of a casting. Anything with
      three dimensions and a number in each of them is a specimen.</p>
    <p>The three things that decide whether that is any good are none of them the file format, and
      all three are measured:</p>
    <div class="cards">
      <div class="c"><div class="n">millimetres</div><h4>Not voxels</h4><p>A clinical CT is about
        0.5 mm across a slice and 1–5 mm between them. Resampling by index squashes the body by
        exactly that ratio. The spacing is read from the file, so a sphere stays a sphere.</p></div>
      <div class="c"><div class="n">averaged</div><h4>Not point-sampled</h4><p>512 × 512 × 300 into
        64³ is about 8 × 8 × 5 source voxels per destination voxel. Aliasing introduced there is in
        the specimen for good. A one-voxel checkerboard decimated 4:1 flattens to a standard
        deviation of 0.00005.</p></div>
      <div class="c"><div class="n">percentile</div><h4>Not min to max</h4><p>One surgical clip at
        3000 HU would push every brain voxel into the bottom two per cent of the range. The window
        comes from the data's percentiles, and it tells you which values it chose.</p></div>
    </div>
    <figure class="hero-shot">
      <img src="img/import.jpg" alt="The import strip: IMPORTED in the specimen window, with phase axis and shape controls and a line describing the file">
      <figcaption>It overrides the dial rather than joining it — so no patch anyone has already saved changes meaning.</figcaption>
    </figure>
    <p>With a CT loaded, WINDOW and LEVEL stop being a metaphor: they are a radiographer's window
      width and level in Hounsfield units, and a brain window is the only way to see cortical detail.
      An imported volume is saved with the patch and with the project, so it travels.</p>
    <div class="shot-row" style="display:flex;gap:1rem;flex-wrap:wrap">
      <figure class="hero-shot" style="flex:1;min-width:240px"><img src="img/import-slice.jpg" alt="An imported scan on the tomography screen"><figcaption>An imported scan, at the window the data chose</figcaption></figure>
      <figure class="hero-shot" style="flex:1;min-width:240px"><img src="img/import-window.jpg" alt="The same slice at a narrow brain window"><figcaption>The same slice, narrowed to a brain window</figcaption></figure>
    </div>
    <p style="font-size:.92rem;opacity:.8">Compressed DICOM and NIfTI-2 are refused by name rather
      than misread — <code>dcm2niix</code> converts either in one command. A reader that guesses is
      worse than one that says no.</p>
  </div>
</section>`);

if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
console.log("manual and landing page patched");
