// 260904.3 — the manual learns the READ: GRAIN / CONTRAST / FOLD, the 128^3
// volume, and the three gritty specimens. Exact-count anchors; nothing written
// on a miss.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "..", "docs", "manual", "manual.html");
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const misses = [];
const rep = (from, to, count = 1) => {
  const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
  const n = s.split(f).length - 1;
  if (n !== count) { misses.push(`expected ${count} of [${from.slice(0, 70)}...], found ${n}`); return; }
  s = s.split(f).join(t);
};

rep(String.raw`<div><b>BRAIN SCAN</b>nine specimens &middot;`, String.raw`<div><b>BRAIN SCAN</b>twelve specimens &middot;`);
rep(String.raw`    filling a cube — a density, exactly like the numbers a scanner records. Nine of them ship: seven
    are fields designed to give ordinary, useful waveforms, one is a hollow shell, and one is a
    simulated brain, with skull, cortical folds, a fissure and ventricles.</p>`,
    String.raw`    filling a cube — a density, exactly like the numbers a scanner records. Twelve of them ship:
    seven are fields designed to give ordinary, useful waveforms, three are made of edges, spikes
    and beats, one is a hollow shell, and one is a simulated brain, with skull, cortical folds, a
    fissure and ventricles.</p>`);
rep(String.raw`steps through the nine.`, String.raw`steps through the twelve.`);
rep(String.raw`holds nine factory studies`, String.raw`holds twelve factory studies`, 2);
rep(String.raw`<h2>The nine specimens</h2>`, String.raw`<h2>The twelve specimens</h2>`);
rep(String.raw`      <tr><td>CORTEX</td><td>the brain</td><td colspan="2">skull, folds, fissure, ventricles</td></tr>
    </table>`,
    String.raw`      <tr><td>CORTEX</td><td>the brain</td><td colspan="2">skull, folds, fissure, ventricles</td></tr>
      <tr><td>SUTURE</td><td>a random staircase</td><td>steps per cycle, 4–32</td><td>walks eight patterns</td></tr>
      <tr><td>ENAMEL</td><td>a comb of spikes</td><td>their width</td><td>how many per cycle, 1–8</td></tr>
      <tr><td>TENDON</td><td>four partials</td><td>harmonic to a bell's ratios</td><td>brightness</td></tr>
    </table>`);
rep(String.raw`    in what order. It is not designed to be tidy. It is designed to be somewhere.</p>
  </div>`,
    String.raw`    in what order. It is not designed to be tidy. It is designed to be somewhere.</p>
    <p><b>The gritty three are made of edges.</b> SUTURE is a random staircase — <code>y</code> the
    number of steps per cycle, <code>z</code> a walk through eight patterns — and a step is a hard
    edge held for a while: every harmonic, then nothing until the next. ENAMEL is a comb of spikes,
    <code>z</code> how many per cycle and <code>y</code> their width; one spike a texel wide is the
    buzziest thing in the instrument. TENDON is four partials whose ratios slide from harmonic to a
    bell's. Cut to one cycle their content is harmonic, as every single-cycle read must be; what
    they carry is the strike at the wrap. Measured above the eighth harmonic, against the
    fundamental: SPINE's middle &minus;25 dB, SUTURE +4, ENAMEL +10.</p>
  </div>`);
rep(String.raw`  <div class="cols2" style="margin-top:5mm">
    <h3>A volume of your own</h3>`,
    String.raw`  <div class="cols2" style="margin-top:5mm">
    <h3>How the tissue is read</h3>
    <p>The field is stored as texels and a read lands between them, so a kernel decides what a point
    between texels is worth. <b>GRAIN</b> is that kernel. At 0 it is a smoothing B-spline, which
    rounds every corner and takes the top of the texel band down by up to 16 dB — the soft read the
    first build shipped with. At 1 it is an interpolating Catmull-Rom, which passes every texel as
    it is, kinks included. It is not a tone control: on a bright SPINE, harmonic 20 moves by a
    decibel between the two and harmonic 40 by five.</p>
    <p><b>CONTRAST</b> is a CT window applied to the sound. Narrowing it scales the read up — to
    sixteen times — and saturates it against the window's edges: a sine leans toward a square, and
    there is more for the filter to bite on. <b>FOLD</b> is what the window does past its edges: at
    0 it clips, at 1 it folds the wave back in, which adds harmonics the tissue never had. Both are
    anti-derivative anti-aliased; the hardest window at C5 on the brightest field aliases at
    &minus;51 dB and a full fold at &minus;65. At CONTRAST 0 and FOLD 0 the window is not there at
    all, and the plain read stays the plain read.</p>
    <p>And the volume itself doubled: 128 texels across where the first build had 64. A straight
    line across the cube carried at most 32 harmonics; now 64. Every specimen was rebuilt for the
    resolution — PULSE's edge is half a texel wide at the bottom of <code>z</code>, SPINE reaches
    48 harmonics and a brightness past a saw. The panel still renders 64<sup>3</sup>, the same bytes
    averaged; the engine reads all 128<sup>3</sup>.</p>
    <figure><img src="img/read.jpg" alt="READ · WINDOW"><figcaption>READ &middot; WINDOW — GRAIN, CONTRAST and FOLD</figcaption></figure>
  </div>

  <div class="cols2" style="margin-top:5mm">
    <h3>A volume of your own</h3>`);
rep(String.raw`of the nine. It takes a`, String.raw`of the twelve. It takes a`);
rep(String.raw`    <p>It does not join the nine: it <em>overrides</em> them. The window says IMPORTED while it is
    in use and <b>CLEAR</b> gives the dial back. That is deliberate — adding a tenth entry to the
    dial would have re-pointed every patch anyone had already saved.</p>`,
    String.raw`    <p>It does not join the twelve: it <em>overrides</em> them. The window says IMPORTED while it
    is in use and <b>CLEAR</b> gives the dial back. That is deliberate — adding an entry to the dial
    re-points every patch anyone has already saved, which is why the three specimens that arrived
    in build 260904.3 came with a one-time remap of older patches and projects, and why an import
    still does not join the dial.</p>`);
rep(String.raw`      <tr><td>ARRHYTHMIA</td><td>a line that should not work</td></tr>`,
    String.raw`      <tr><td>ARRHYTHMIA</td><td>a line that should not work</td></tr>
      <tr><td>STAPLES</td><td>the staircase, read texel by texel</td></tr>
      <tr><td>ENAMEL</td><td>spikes through a narrow window</td></tr>
      <tr><td>TENDON</td><td>bell ratios, folded</td></tr>`);
rep(String.raw`bench that renders real audio and asserts on it — 38 checks — and the panel has a second one
    that loads the real page and drives it through the plug-in's own message vocabulary — 24 checks.`,
    String.raw`bench that renders real audio and asserts on it — 85 checks — and the panel has a second one
    that loads the real page and drives it through the plug-in's own message vocabulary — 30 checks.`);
rep(String.raw`      <tr><td>Cost</td><td>11.0 % of one core for eight voices of four-way unison — 32 readers, the
        maximum</td></tr>`,
    String.raw`      <tr><td>Cost</td><td>12.6 % of one core for eight voices of four-way unison — 32 readers, the
        maximum; 14.9 % with GRAIN 1, the window and the fold all on</td></tr>
      <tr><td>GRAIN</td><td>SPINE at y = 1, A2: harmonic 40 rises 4.6 dB from GRAIN 0 to 1 while
        harmonic 20 moves 1.3 dB — a kernel, not a tilt</td></tr>
      <tr><td>CONTRAST</td><td>SINUS THD &minus;120.6 / &minus;18.3 / &minus;10.3 / &minus;7.9 /
        &minus;6.9 dB at CONTRAST 0 / &frac14; / &frac12; / &frac34; / 1: nothing at zero, monotonic
        to a square's order</td></tr>
      <tr><td>The window's aliasing</td><td>brightest SPINE, GRAIN 1, C5: &minus;58.3 dB plain,
        &minus;51.2 dB with CONTRAST 1, &minus;64.9 dB with a full FOLD at gain 4</td></tr>
      <tr><td>The gritty three</td><td>energy above the 8th harmonic against the fundamental:
        SPINE's middle &minus;24.6 dB, SUTURE +4.4, ENAMEL +10.1, TENDON &minus;2.6</td></tr>
      <tr><td>The build</td><td>a 128<sup>3</sup> specimen builds in 21 ms on twenty threads; the
        panel's copy is published from a worker, never from the timer</td></tr>`);
rep(String.raw`      <tr><td>SPECIMEN</td><td>which volume the lines read. Not automatable: it rebuilds a quarter of
        a million texels, and a lane fighting that is unusable</td></tr>`,
    String.raw`      <tr><td>SPECIMEN</td><td>which volume the lines read. Not automatable: it rebuilds two million
        texels on a worker thread, and a lane fighting that is unusable</td></tr>`);
rep(String.raw`      <tr><td>SCAN ENV</td><td>how far the envelope moves the scan, either way &plusmn;100 %</td></tr>
    </table>`,
    String.raw`      <tr><td>SCAN ENV</td><td>how far the envelope moves the scan, either way &plusmn;100 %</td></tr>
    </table>
    <h4>READ &middot; WINDOW</h4>
    <table>
      <tr><td>GRAIN</td><td>the read kernel: 0 smooths across texels, 1 passes every texel as it is</td></tr>
      <tr><td>CONTRAST</td><td>the CT window on the sound: narrower saturates the read toward a square,
        up to sixteen times the gain</td></tr>
      <tr><td>FOLD</td><td>past the window's edges: 0 clips flat, 1 folds the wave back in</td></tr>
    </table>`);
rep(String.raw`<li>Thirty parameters are automatable, plus five rack macros.`, String.raw`<li>Thirty-three parameters are automatable, plus five rack macros.`);
rep(String.raw`build 260904.1 &middot;`, String.raw`build 260904.3 &middot;`);

if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(p, s, "utf8");
console.log("manual patched for 260904.3");
