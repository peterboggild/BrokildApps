// 260905.1 — the manual learns the bodies, the second head, split lines, the
// resets, the gantry handles, the HU presets and the import-dial change.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "..", "docs", "manual", "manual.html");
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const misses = [];
const rep = (from, to, count = 1) => {
  const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
  const n = s.split(f).length - 1;
  if (n !== count) { misses.push(`expected ${count} of [${from.slice(0, 70).replace(/\n/g, "\\n")}...], found ${n}`); return; }
  s = s.split(f).join(t);
};

rep(String.raw`<div><b>BRAIN SCAN</b>twelve specimens &middot;`, String.raw`<div><b>BRAIN SCAN</b>fifteen specimens &middot;`);
rep(String.raw`    filling a cube — a density, exactly like the numbers a scanner records. Twelve of them ship:
    seven are fields designed to give ordinary, useful waveforms, three are made of edges, spikes
    and beats, one is a hollow shell, and one is a simulated brain, with skull, cortical folds, a
    fissure and ventricles.</p>`,
    String.raw`    filling a cube — a density, exactly like the numbers a scanner records. Fifteen of them ship,
    in two families. Nine are <em>phantoms</em> — calibration objects, each a formula and each a
    promise: six give ordinary, useful waveforms, three are made of edges, spikes and beats. Six are
    <em>bodies</em>, built in Hounsfield units the way a scanner would record them: a head, the
    same head as bare skull, a chest, three lumbar vertebrae, a thigh, and a jaw with its teeth.</p>`);
rep(String.raw`steps through the twelve.`, String.raw`steps through the fifteen.`);
rep(String.raw`holds twelve factory studies`, String.raw`holds sixteen factory studies`, 2);
rep(String.raw`<h2>The twelve specimens</h2>`, String.raw`<h2>The fifteen specimens</h2>`);
rep(String.raw`      <tr><td>LUNG</td><td>smooth noise</td><td colspan="2">rough, breathy, metallic on a loop</td></tr>
      <tr><td>SKULL</td><td>a hollow sphere</td><td colspan="2">two bumps per cycle through the centre</td></tr>
      <tr><td>CORTEX</td><td>the brain</td><td colspan="2">skull, folds, fissure, ventricles</td></tr>
      <tr><td>SUTURE</td><td>a random staircase</td><td>steps per cycle, 4–32</td><td>walks eight patterns</td></tr>
      <tr><td>ENAMEL</td><td>a comb of spikes</td><td>their width</td><td>how many per cycle, 1–8</td></tr>
      <tr><td>TENDON</td><td>four partials</td><td>harmonic to a bell's ratios</td><td>brightness</td></tr>
    </table>`,
    String.raw`      <tr><td>THORAX</td><td>a chest CT</td><td colspan="2">ribs, sternum, spine, two lungs with their vessels, the heart and aorta</td></tr>
      <tr><td>SKULL</td><td>the skull alone</td><td colspan="2">vault, orbits, sinuses, cheekbones, jaw and teeth — no soft tissue</td></tr>
      <tr><td>CORTEX</td><td>a head CT</td><td colspan="2">the same skull with brain, ventricles, eyes and scalp</td></tr>
      <tr><td>SUTURE</td><td>a random staircase</td><td>steps per cycle, 4–32</td><td>walks eight patterns</td></tr>
      <tr><td>ENAMEL</td><td>a comb of spikes</td><td>their width</td><td>how many per cycle, 1–8</td></tr>
      <tr><td>TENDON</td><td>four partials</td><td>harmonic to a bell's ratios</td><td>brightness</td></tr>
      <tr><td>VERTEBRA</td><td>three lumbar levels</td><td colspan="2">bodies, discs, the canal and its roots, processes, the muscles around</td></tr>
      <tr><td>FEMUR</td><td>a thigh</td><td colspan="2">the femoral shaft flaring into its ends, marrow, muscle compartments, vessels</td></tr>
      <tr><td>JAW</td><td>the mandible</td><td colspan="2">both rows of teeth with enamel, dentine and pulp; the tongue and the palate</td></tr>
    </table>`);
rep(String.raw`    <p><b>CORTEX is the show-piece, not a wavetable.</b> A line through a brain reads bone, then
    fluid, then folded cortex, then a ventricle: a waveform with a hard edge, a smooth stretch, a
    burst of detail and a plateau, and moving the line an inch changes which of those it meets and
    in what order. It is not designed to be tidy. It is designed to be somewhere.</p>`,
    String.raw`    <p><b>The bodies are anatomy, in Hounsfield units.</b> Air is &minus;1000, fat about
    &minus;90, brain and muscle around 40, trabecular bone a few hundred, the cortical tables above a
    thousand and enamel at the top of the scale; the cube's 0 to 1 spans &minus;1000 to 2000 HU. So
    WINDOW and LEVEL read in HU on a body, the four presets on the gantry are a radiographer's, and
    a bone is a bone: a line through CORTEX reads scalp, skull, cerebrospinal fluid, folded cortex,
    a ventricle — a waveform with hard edges, a smooth stretch, a burst of detail and a plateau, and
    moving the line an inch changes which of those it meets and in what order. SKULL is the same
    head with every soft tissue removed, the way a bone reconstruction shows it. None of them is
    designed to be tidy. They are designed to be somewhere.</p>`);
rep(String.raw`  <div class="trip">
    <figure><img src="img/spine-slice.jpg" alt="SPINE"><figcaption>SPINE — a harmonic stack, sliced</figcaption></figure>
    <figure><img src="img/pulse-slice.jpg" alt="PULSE"><figcaption>PULSE — width across y</figcaption></figure>
    <figure><img src="img/scan0.jpg" alt="CORTEX"><figcaption>CORTEX — the simulated brain</figcaption></figure>
  </div>`,
    String.raw`  <div class="trip">
    <figure><img src="img/spine-slice.jpg" alt="SPINE"><figcaption>SPINE — a harmonic stack, sliced</figcaption></figure>
    <figure><img src="img/pulse-slice.jpg" alt="PULSE"><figcaption>PULSE — width across y</figcaption></figure>
    <figure><img src="img/cortex-axial.jpg" alt="CORTEX"><figcaption>CORTEX — an axial slice, brain window</figcaption></figure>
  </div>
  <div class="trip">
    <figure><img src="img/skull-gantry.jpg" alt="SKULL"><figcaption>SKULL in the gantry, bone window</figcaption></figure>
    <figure><img src="img/thorax-axial.jpg" alt="THORAX"><figcaption>THORAX — axial, lung window</figcaption></figure>
    <figure><img src="img/jaw-axial.jpg" alt="JAW"><figcaption>JAW — axial through the lower teeth</figcaption></figure>
  </div>
  <div class="trip">
    <figure><img src="img/vertebra-axial.jpg" alt="VERTEBRA"><figcaption>VERTEBRA — axial through a body</figcaption></figure>
    <figure><img src="img/femur-gantry.jpg" alt="FEMUR"><figcaption>FEMUR in the gantry</figcaption></figure>
    <figure><img src="img/skull-coronal.jpg" alt="SKULL coronal"><figcaption>SKULL — coronal through the orbits</figcaption></figure>
  </div>`);
rep(String.raw`of the twelve. It takes a`, String.raw`of the fifteen. It takes a`);
rep(String.raw`    <p>It does not join the twelve: it <em>overrides</em> them. The window says IMPORTED while it
    is in use and <b>CLEAR</b> gives the dial back. That is deliberate — adding an entry to the dial
    re-points every patch anyone has already saved, which is why the three specimens that arrived
    in build 260904.3 came with a one-time remap of older patches and projects, and why an import
    still does not join the dial.</p>`,
    String.raw`    <p>It does not join the fifteen: it <em>overrides</em> them. The window says IMPORTED while it
    is in use; <b>CLEAR</b> gives the dial back, and so does simply turning the dial — a dial that
    moved without changing anything read as broken, so now it wins. Adding an entry to the dial
    re-points every patch anyone has already saved, which is why the specimens that arrived in
    260904.3 and 260905.1 came with a one-time remap of older patches and projects, and why an
    import still does not join the dial. An import has Hounsfield units of its own (its cube spans
    the window it was read with), so the presets light on it too.</p>`);
rep(String.raw`      <tr><td>TENDON</td><td>bell ratios, folded</td></tr>`,
    String.raw`      <tr><td>TENDON</td><td>bell ratios, folded</td></tr>
      <tr><td>SKULL</td><td>through the orbits, then through the teeth</td></tr>
      <tr><td>VERTEBRA</td><td>a second head at ×1.50 — a beating partial</td></tr>
      <tr><td>FEMUR</td><td>a split line: bone, then muscle, two edges a cycle</td></tr>
      <tr><td>JAW</td><td>the MOD line narrowing the window as it goes</td></tr>`);
rep(String.raw`    <h3>The bench</h3>`,
    String.raw`    <h3>Split, flatten, undo — and the gantry</h3>
    <p><b>SPLIT</b> (the ◂ ▸ pair on the line bench) makes a line two segments instead of one
    curve: the first half of the cycle reads the points before the split, the second half the
    points after it. One cycle, two edges, both band-limited — a family of timbre a single curve
    cannot make. It needs four points or more, and the slice draws the two segments apart.</p>
    <p><b>FLATTEN</b> starts the selected scanner over — two straight lines along x through the
    middle of the body, A a little below B so SCAN still has somewhere to go — and <b>FLATTEN
    ALL</b> does all six: the simplest state the instrument has, to build up from again.
    <b>REVERT</b> puts back the six lines the current patch, study or project was loaded with.
    <b>UNDO</b> takes back the last change to the lines, one step, from any gesture on the slice,
    the gantry or the bench.</p>
    <p>And the gantry is no longer only for looking: the selected line's control points are drawn
    on it as white dots. Drag one to move it across the view; hold <b>shift</b> to push it along the
    line of sight — the one direction the slice cannot reach.</p>
    <h3>The bench</h3>`);
rep(String.raw`    <h3>SOLID and SURFACE</h3>`,
    String.raw`    <h3>The presets, in Hounsfield units</h3>
    <p>Under the sliders sit <b>BRAIN</b> (W80 L40), <b>SOFT</b> (W400 L40), <b>LUNG</b> (W1500
    L&minus;600) and <b>BONE</b> (W2000 L500) — a radiographer's windows. They light on the bodies
    and on an import, both of which carry real CT numbers, and each body opens on its own: the
    head on BRAIN, the chest on LUNG, the bones on BONE. A preset also raises DENSITY, because bone
    is a thin shell and the gantry needs opacity to show it; and on a body the gantry draws nothing
    below a third of the window, so a bone window shows bone and not a fog of soft tissue. The
    phantoms have no HU: the buttons dim, and the sliders work as they always did.</p>
    <h3>SOLID and SURFACE</h3>`);
rep(String.raw`    <h3>ENVELOPE and VOICE</h3>`,
    String.raw`    <h3>The second head, and the MOD line on the read</h3>
    <p><b>2ND HEAD</b> puts a second reader on the same blended line and mixes it in. At <b>HEAD
    RATIO</b> ×1.00 with <b>HEAD PHASE</b> offset it is a fixed-interval double without a second
    voice — at 50 % it cancels the odd harmonics and leaves a hollow. Off the whole numbers it is
    something a single-cycle read can never be on its own: a partial that is not a harmonic. At
    ×1.50 the bench measures a partial at 1.5 f<sub>0</sub> within 0.3 dB of the fundamental and
    no second harmonic at all. Whole ratios give harmonics; anything else beats.</p>
    <p><b>MOD&gt;WINDOW</b> and <b>MOD&gt;GRAIN</b> let the MOD line drive the read itself: the
    tissue under the MOD line narrows the CONTRAST window or sharpens GRAIN as it travels, so the
    body decides how hard it is read. Centred, they do nothing — the bench checks the knob values
    pass through untouched. <b>SCAN SPREAD</b>, in VOICE, spreads unison readers through the SCAN
    as well as in pitch: each reads the body a little further along than the last, and a chord of
    readers widens without detuning.</p>
    <h3>ENVELOPE and VOICE</h3>`);
rep(String.raw`bench that renders real audio and asserts on it — 85 checks — and the panel has a second one
    that loads the real page and drives it through the plug-in's own message vocabulary — 30 checks.`,
    String.raw`bench that renders real audio and asserts on it — 118 checks — and the panel has a second one
    that loads the real page and drives it through the plug-in's own message vocabulary — 43 checks.`);
rep(String.raw`      <tr><td>Aliasing</td><td>the roughest specimen at C6: non-harmonic energy &minus;70.1 dB, and
        &minus;23.8 dB with the mip pyramid disabled</td></tr>`,
    String.raw`      <tr><td>Aliasing</td><td>the roughest specimen (THORAX, grain and all) at C6: non-harmonic
        energy &minus;70.1 dB, and +0.6 dB with the mip pyramid disabled</td></tr>`);
rep(String.raw`      <tr><td>Cost</td><td>12.6 % of one core for eight voices of four-way unison — 32 readers, the
        maximum; 14.9 % with GRAIN 1, the window and the fold all on</td></tr>`,
    String.raw`      <tr><td>Cost</td><td>16.7 % of one core for eight voices of four-way unison — 32 readers, the
        maximum; 18.5 % with GRAIN 1, the window and the fold all on; 39 % with a second head on
        every reader</td></tr>
      <tr><td>The second head</td><td>at ×1.00 and PHASE 50 % the odd harmonics fall from +3.9 dB
        over the even to &minus;106 dB under; at ×1.50 a partial at 1.5 f<sub>0</sub> sits 0.3 dB
        from the fundamental with the 2nd harmonic at &minus;111 dB; at ×4 on the brightest field at
        C5 the floor is &minus;56 dB</td></tr>
      <tr><td>A split line</td><td>reads its first segment to the end and jumps to the second at
        exactly half a cycle; at C5 the two edges band-limited give &minus;64.3 dB of non-harmonic
        energy, &minus;31.8 without — the second polyBLEP is worth 33 dB</td></tr>
      <tr><td>The MOD line on the window</td><td>centred, SINUS reads &minus;120.6 dB of THD; at
        &minus;100 % with the MOD line parked in the sine's trough, &minus;8.5 dB</td></tr>
      <tr><td>The bodies</td><td>tissue fractions per body (air where there is air, a few per
        cent of bone, enamel where there are teeth); five rays from the centre of the head all cross
        bone and the SKULL's cranium is empty while CORTEX's holds white matter; the chest has two
        lungs; the vertebra has a canal of CSF between two pedicles of bone</td></tr>`);
rep(String.raw`      <tr><td>FOLD</td><td>past the window's edges: 0 clips flat, 1 folds the wave back in</td></tr>
    </table>`,
    String.raw`      <tr><td>FOLD</td><td>past the window's edges: 0 clips flat, 1 folds the wave back in</td></tr>
      <tr><td>2ND HEAD</td><td>a second reader on the same line, mixed in</td></tr>
      <tr><td>HEAD RATIO</td><td>its speed against the note, ×0.25 to ×4 with exactly ×1.00 in the middle</td></tr>
      <tr><td>HEAD PHASE</td><td>where along the cycle it starts — the interval, at ×1.00</td></tr>
    </table>
    <h4>THE READ, MODULATED</h4>
    <table>
      <tr><td>MOD&gt;WINDOW</td><td>how much the MOD line narrows the window (in MODULATOR)</td></tr>
      <tr><td>MOD&gt;GRAIN</td><td>how much the MOD line sharpens the read (in MODULATOR)</td></tr>
      <tr><td>SCAN SPREAD</td><td>unison readers spread through the scan, each a little off the others (in VOICE)</td></tr>
    </table>`);
rep(String.raw`      <tr><td>AXIAL / CORONAL / SAGITTAL</td><td>which plane the tomograph cuts</td></tr>`,
    String.raw`      <tr><td>AXIAL / CORONAL / SAGITTAL</td><td>which plane the tomograph cuts</td></tr>
      <tr><td>GRID</td><td>show the 128 texels the engine reads across the plane</td></tr>
      <tr><td>BRAIN / SOFT / LUNG / BONE</td><td>a radiographer's windows, on a body or an import</td></tr>
      <tr><td>SPLIT ◂ ▸</td><td>make the line two segments; move the split; back to one curve</td></tr>
      <tr><td>FLATTEN / FLATTEN ALL</td><td>the scanner, or every line, back to straight reads through the middle</td></tr>
      <tr><td>REVERT</td><td>the six lines as they were loaded</td></tr>
      <tr><td>UNDO</td><td>the last change to the lines, one step</td></tr>`);
rep(String.raw`<li>Thirty-three parameters are automatable, plus five rack macros.`, String.raw`<li>Thirty-nine parameters are automatable, plus five rack macros.`);
rep(String.raw`build 260904.3 &middot;`, String.raw`build 260905.1 &middot;`);

if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(p, s, "utf8");
console.log("manual patched for 260905.1");
