/*  Build the Brain Scan landing page.

    The house shell (top bar, hero, blocks, cards, tables, footer) is High
    Tide's, so every plug-in page stays the same page; only the palette and the
    words change.  node test/make-landing.js
*/
const fs = require("fs"), path = require("path");
const WEB = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps";
const src = fs.readFileSync(path.join(WEB, "high-tide", "index.html"), "utf8");
const a = src.indexOf("<style>"), b = src.indexOf("</style>") + "</style>".length;
if (a < 0 || b < 8) { console.error("no style block in the High Tide page"); process.exit(1); }
let css = src.slice(a, b);
/*  the CT console's blue instead of High Tide's teal */
css = css.replace("--accent: #14808a;", "--accent: #1f6ea8;")
         .replace("--accent-bright: #1fb5c4;", "--accent-bright: #4fb4dd;")
         .replace("--accent: #6fe3ee; --accent-bright: #a9f0f5;", "--accent: #7fc9ee; --accent-bright: #b6e4fb;")
         .replace("--bg: #0c0a07; --bg-soft: #14110b; --card: #17130c; --border: #2a2418;",
                  "--bg: #080b0d; --bg-soft: #0f1316; --card: #121719; --border: #232b30;")
         .replace("--bg-soft: #f7f4ee;", "--bg-soft: #f4f5f3;")
         .replace("--border: #e7e2d8;", "--border: #e0e3e0;")
         .replace("--code-bg: #f2eee4;", "--code-bg: #eceeeb;")
         .replace("--code-bg: #201a10;", "--code-bg: #171d21;");
if (css.indexOf("#1f6ea8") < 0) { console.error("the palette swap missed"); process.exit(1); }

const BUILD = "260904.1";
const page = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Brain Scan — VST3 volume synth | BrokildApps</title>
  <meta name="description" content="Brain Scan is a free Windows VST3 synth that stores no waveforms. A specimen is a volume - a scalar field filling a cube, like the numbers a scanner records - and a scan line is a curve through it: one cycle of the sound is the field along that curve. SCAN blends the geometry of two lines before the field is read, so halfway between two sounds is tissue neither has visited. Nine specimens including a simulated brain, a tomography slice you draw the lines on, and a ray-marched gantry." />
  <meta name="color-scheme" content="light dark" />
  <link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' rx='18' fill='%231f6ea8'/><text x='50' y='68' font-size='52' font-family='Arial,sans-serif' font-weight='700' text-anchor='middle' fill='white'>B</text></svg>" />
  ${css}
</head>
<body>

<div class="topbar">
  <div class="wrap">
    <nav class="nav">
      <a class="brand" href="../../index.html">
        <span class="brand-mark">B</span>
        <span>
          <span class="brand-name">Brokild</span><br>
          <span class="brand-sub">Musical instruments</span>
        </span>
      </a>
      <a class="back" href="../../index.html">← All instruments</a>
    </nav>
  </div>
</div>

<header class="hero">
  <div class="wrap">
    <div class="kicker">VST3 Instrument · Windows · Free</div>
    <h1>Brain <span>Scan</span></h1>
    <p class="tagline">A synth that stores no waveforms. It stores a body, and reads a line through it.</p>
    <p class="lede">A wavetable synth keeps a list of numbers and reads it with a pointer; to get from one
      sound to another it fades between two of them. Brain Scan keeps a <b>volume</b> — a scalar field
      filling a cube, exactly the kind of number a scanner records — and reads a <b>curve</b> through it.
      One cycle of the waveform is the field along that curve. Bend the curve and the timbre changes,
      because the curve is now somewhere else in the body. The filter's cutoff and the modulator are the
      same mechanism read slower: two more curves. And <b>SCAN</b> blends the <i>geometry</i> of two
      curves before anything is read, so the sound halfway between two sounds comes from tissue that
      neither of them has ever visited.</p>
    <div class="btnrow">
      <a class="btn btn-primary" href="Brain-Scan-VST3-win64.zip" download>
        Download for Windows · 7 MB
      </a>
      <span class="buildtag">Build ${BUILD}</span>
      <a class="btn btn-ghost" href="Brain-Scan-Manual.pdf">
        Read the manual (PDF)
      </a>
    </div>
    <figure class="hero-shot" style="margin-top:2.4rem">
      <img src="img/hero.jpg" alt="The Brain Scan console: a tomography slice of a brain with scan lines drawn on it, a ray-marched gantry, an ECG-style monitor, and a desk of knobs">
      <figcaption>The console. Tomography left, gantry centre, monitor and scan lines right.</figcaption>
    </figure>
  </div>
</header>

<section class="block">
  <div class="wrap">
    <h2>Move the question, not the answer</h2>
    <p class="sub">Every scanner has two anchor lines. SCAN builds the line between them and reads once.</p>
    <p>A crossfade can only ever give you a mixture of two things you already had. Blending the two
      <i>paths</i> and reading afterwards gives you something else entirely: at SCAN 50&nbsp;% the
      instrument reads a curve that neither anchor draws, through tissue neither of them touches.</p>
    <figure class="hero-shot">
      <img src="img/scan50.jpg" alt="The tomography slice: a bright scan line halfway between two faint anchor lines, drawn over a brain">
      <figcaption>The bright line is what is being read. The faint pair are its anchors.</figcaption>
    </figure>
    <p>It is not a figure of speech, and it is measured. Take two pulses of duty 0.23 and 0.77. Both
      have a spectral null at the fifth harmonic, so <i>any</i> crossfade of the two keeps that null —
      the bench reads it at &minus;66&nbsp;dB. The midway path reads a pulse of duty 0.50, whose fifth
      harmonic is &minus;14.0&nbsp;dB, exactly what the formula says. Fifty decibels of difference,
      from the same two endpoints.</p>
    <div class="cards">
      <div class="c"><div class="n">closed</div><h4>No edge</h4><p>A loop joins itself, so the cycle has no discontinuity. Soft.</p></div>
      <div class="c"><div class="n">open</div><h4>One edge</h4><p>A segment jumps between its ends once per cycle: bright and saw-like, band-limited so it will not alias.</p></div>
      <div class="c"><div class="n">corner</div><h4>A kink</h4><p>At a fixed phase, wherever you put the point.</p></div>
      <div class="c"><div class="n">crossing</div><h4>One value, two phases</h4><p>Let the line cross itself and the waveform repeats a value it has already been.</p></div>
      <div class="c"><div class="n">warp</div><h4>Phase distortion</h4><p>The same shape read unevenly — fast over one half of the line, slow over the other.</p></div>
      <div class="c"><div class="n">travel</div><h4>Timbre that evolves</h4><p>Move the whole line through the body and the sound changes while the instrument stays itself.</p></div>
    </div>
  </div>
</section>

<section class="block">
  <div class="wrap">
    <h2>Nine bodies to read</h2>
    <p class="sub">Seven fields built to give ordinary, useful waveforms; a hollow shell; and a brain.</p>
    <p>The <code>x</code> axis is the phase axis, so a straight line across the cube reads the specimen's
      natural waveform and an ordinary wavetable turns out to be a special case: a straight line, moved
      sideways. <b>SPINE</b> is a harmonic stack whose brightness and parity are the other two axes —
      the instrument's bread, and the bench checks it against its own formula. <b>PULSE</b> is a pulse
      whose duty is a coordinate. <b>NERVE</b> is phase modulation laid out as a landscape.</p>
    <p><b>CORTEX</b> is the show-piece: a simulated brain with skull, cerebrospinal fluid, folded cortex,
      the longitudinal fissure, ventricles, cerebellum and stem. A line through it reads bone, then
      fluid, then folds, then a ventricle — a waveform with a hard edge, a smooth stretch, a burst of
      detail and a plateau. Move the line an inch and it meets them in a different order.</p>
    <div class="shot-row" style="display:flex;gap:1rem;flex-wrap:wrap">
      <figure class="hero-shot" style="flex:1;min-width:260px"><img src="img/gantry.jpg" alt="The gantry: a ray-marched brain with the scan lines glowing inside it"><figcaption>CORTEX in the gantry, the lines glowing inside the tissue.</figcaption></figure>
      <figure class="hero-shot" style="flex:1;min-width:260px"><img src="img/surface.jpg" alt="The gantry in SURFACE mode showing one level of a waveform field"><figcaption>SPINE in SURFACE — one level of the field, as a surface.</figcaption></figure>
    </div>
  </div>
</section>

<section class="block">
  <div class="wrap">
    <h2>A curve in a cube, drawn with a mouse</h2>
    <p class="sub">Which is impossible — so the plane moves instead.</p>
    <p>Lines are drawn on the <b>tomography</b> screen: one plane through the specimen, axial, coronal or
      sagittal, driven through the body by the TABLE slider. Drag a numbered point and it moves in that
      plane <i>keeping its depth</i>; click bare tissue and a point is added at the plane's depth, in the
      segment it is nearest; alt-click one to take it away. Points off the plane are drawn hollow and fade
      with distance, and every line's crossing of the current plane is ringed in that scanner's colour.</p>
    <figure class="hero-shot">
      <img src="img/lines.jpg" alt="The scan-line bench: six line buttons, closed, points, start, warp, shape presets">
      <figcaption>Six lines — two anchors each for WAVE, FILTER and MOD — and the tools for the one you are editing.</figcaption>
    </figure>
    <p>The <b>gantry</b> in the middle is for looking, not editing: the specimen ray-marched in three
      dimensions with the lines glowing inside it. DENSITY is opacity — low and the body is a ghost the
      lines blaze through, high and the tissue closes over them. WINDOW and LEVEL are the width and the
      centre of the display window, exactly as on a radiograph, and they drive the slice too. They are
      set for you from each specimen's own histogram as it loads, the way a scanner picks a window per
      protocol, and then stay where you put them.</p>
  </div>
</section>

<section class="block">
  <div class="wrap">
    <h2>The filter is a line as well</h2>
    <p class="sub">And so is the modulator. One law, three scanners.</p>
    <p>Turn <b>F DEPTH</b> up and the cutoff stops being a knob: it becomes a reading of the specimen
      along the FILTER line, swept once per note or looped on the host clock. The shape of the sweep is
      the shape of whatever the line passes through — not a curve from a table, and not something anyone
      designed. The <b>modulator</b> is a third line read at its own rate, sent to scan, pitch and pan.</p>
    <p>One <b>SCAN</b> control moves all three at once, with a bipolar envelope of its own and a
      contribution from the modulator, so a note can travel through the body while it sounds.</p>
    <figure class="hero-shot">
      <img src="img/monitor.jpg" alt="The monitor: three ECG-style leads showing the wave, filter and mod readings, with note, cutoff and scan vitals">
      <figcaption>Three leads: the cycle being read, the filter line, the modulator. Below them, what SCAN has actually reached.</figcaption>
    </figure>
  </div>
</section>

<section class="block">
  <div class="wrap">
    <h2>Everything here is measured</h2>
    <p class="sub">Two benches, 62 checks, both in the source.</p>
    <table>
      <tr><th>What</th><th>Measured</th></tr>
      <tr><td>SINUS is a sine</td><td>total harmonic distortion &minus;120.8 dB</td></tr>
      <tr><td>PULSE obeys its width</td><td>duty 0.229 / 0.500 / 0.771 against a formula giving 0.230 / 0.500 / 0.770</td></tr>
      <tr><td>Aliasing</td><td>the roughest specimen at C6: &minus;70.0 dB, and &minus;26.0 dB with the mip pyramid off</td></tr>
      <tr><td>The edge of an open line</td><td>&minus;53.4 dB with band-limiting, &minus;20.0 without — it is worth 33 dB</td></tr>
      <tr><td>The claim</td><td>fifth harmonic at SCAN 50 %: &minus;14.0 dB, formula &minus;14.0; the crossfade of the same anchors &minus;65.8 dB</td></tr>
      <tr><td>The filter follows its line</td><td>worst error 0.000 octaves against the field read along the blended line</td></tr>
      <tr><td>Offset</td><td>&minus;122.9 dB of DC after a second</td></tr>
      <tr><td>Cost</td><td>11.0 % of one core for eight voices of four-way unison — 32 readers, the maximum</td></tr>
      <tr><td>The panel</td><td>loaded in a real browser and driven through the plug-in's own message vocabulary: 24 checks, including that a straight line of four control points reads straight to five decimals</td></tr>
    </table>
    <p>That last one matters more than it sounds: it is the proof that the panel evaluates a curve the
      same way the audio engine does. What you see drawn is what you hear.</p>
  </div>
</section>

<section class="block">
  <div class="wrap">
    <h2>The world rack</h2>
    <p class="sub">Twelve pedals and six characters, shared by every Brokild synth.</p>
    <p>The globe in the header opens Brokild World FX: modules dragged into whatever order you like, and
      five macros that appear as host parameters so the rack can be automated. A fresh rack is empty and
      changes nothing. Macro 5 arrives assigned to the rack's dry/wet, which is why a new instance sounds
      exactly as it would with no rack at all.</p>
    <figure class="hero-shot">
      <img src="img/bwfx.jpg" alt="The Brokild World FX rack open over the Brain Scan console">
      <figcaption>The rack, over the console.</figcaption>
    </figure>
    <p>Hints are on every control — a note beside the knob saying what it is and how it is used — and the
      HINTS button turns them off when you no longer need them.</p>
  </div>
</section>

<section class="block">
  <div class="wrap">
    <h2>Getting it running</h2>
    <p class="sub">Windows, 64-bit. No account, no key.</p>
    <p>Unzip and copy the folder <code>Brain Scan.vst3</code> into
      <code>C:\\Program Files\\Common Files\\VST3\\</code> — a subfolder is fine, hosts scan recursively —
      then rescan in your host. The standalone in the same zip needs nothing but itself.</p>

    <div class="note">
      <b>The panel is a web view</b> and needs the Microsoft Edge WebView2 runtime. It is present on every
      up-to-date Windows 10 and 11; if the panel comes up blank, that runtime is what to install. The
      audio keeps working with the window shut either way.
    </div>

    <div class="note">
      <b>If Windows refuses to load it at all</b> and mentions an <i>Application Control policy</i>,
      that is <b>Smart App Control</b>. It decides <i>per file</i>, from a cloud reputation lookup,
      and it can block one unsigned binary while loading another quite happily. The block attaches to
      that exact copy of the file rather than to the plugin. Windows Security → App &amp; browser control
      → Smart App Control lets you turn it off — note that switching it off is not reversible without
      reinstalling Windows. Most machines do not have it enabled at all.
    </div>

    <table>
      <tr><th>Requirement</th><th>Detail</th></tr>
      <tr><td>System</td><td>Windows 10 or 11, 64-bit</td></tr>
      <tr><td>Build</td><td>${BUILD} — shown on the loading screen and in the About box</td></tr>
      <tr><td>Format</td><td>VST3 instrument, stereo out, MIDI in. A standalone application is in the same zip.</td></tr>
      <tr><td>Polyphony</td><td>Eight voices of up to four readers, or mono with legato glide</td></tr>
      <tr><td>Specimens</td><td>Nine volumes built in, including a simulated brain</td></tr>
      <tr><td>Patches</td><td>Nine factory studies; your own saved one file each to <code>Documents\\Brokild patches\\Brain Scan</code></td></tr>
      <tr><td>Automation</td><td>30 parameters plus 5 rack macros. SPECIMEN is deliberately not automatable — it rebuilds a quarter of a million texels.</td></tr>
      <tr><td>Sample rates</td><td>Any. Bench-tested at 44.1, 48 and 96 kHz.</td></tr>
      <tr><td>Hosts</td><td>Any VST3 host — developed against Ableton Live 12</td></tr>
      <tr><td>Price</td><td>Free. No account, no registration, no telemetry.</td></tr>
    </table>
  </div>
</section>

<footer>
  <div class="wrap">
    <p><b>Brain Scan</b> — a native VST3 instrument. Native C++ audio, JUCE 8, interface drawn in
      the plugin's own window. Source and issues:
      <a href="https://github.com/peterboggild/BrokildApps" target="_blank" rel="noopener">github.com/peterboggild/BrokildApps</a>.
      Its stablemates are <a href="../high-tide/index.html">High Tide</a>,
      <a href="../black-rider/index.html">Black Rider</a>,
      <a href="../full-metal-racket/index.html">Full Metal Racket</a>, <a href="../clone-wars/index.html">Clone Wars</a>,
      <a href="../blade-ruiner/index.html">Blade Ruiner</a>,
      <a href="../martian-gain/index.html">Martian Gain</a>,
      <a href="../photo-synth/index.html">Photo Synth</a> and
      <a href="../escape-room/index.html">Escape Room</a>.</p>
    <p>This VST comes from the curved mind of Professor Brokild, and was programmed with VScode,
      ChatGPT and Claude. Use of this is on your own peril. It may generate unique, beautiful,
      scary or useless sounds for you, and it may crash your DAW project. Have fun, play unsafely,
      unwisely, and unboringly.</p>
    <p>Cheers, Peter Bøggild, September 2026.</p>
  </div>
</footer>

</body>
</html>
`;
fs.writeFileSync(path.join(WEB, "brain-scan", "index.html"), page);
console.log("wrote the landing page, " + page.length + " bytes");
