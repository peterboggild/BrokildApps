// Handbook for build 260925.4: HEAD on page 17, SOUND + BOUNCE on page 21,
// a new page 22 RENDERING, specifications and back renumbered. Exact-count
// anchors; nothing is written on a miss.
const fs = require('fs');
const f = 'C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/thin-walls/plugin/docs/manual/manual.html';
let s = fs.readFileSync(f, 'utf8');
const NL = (s.split('\r\n').length - 1) > (s.split('\n').length - 1) / 2 ? '\r\n' : '\n';
s = s.split('\r\n').join('\n');
const miss = [];
function rep(a, b) { const n = s.split(a).length - 1; if (n !== 1) { miss.push(n + 'x: ' + a.slice(0, 90)); return; } s = s.replace(a, () => b); }

// ---- page 17: the head
rep('<h2 class="display">Six controls at the end of the chain</h2>', '<h2 class="display">Seven controls at the end of the chain</h2>');
rep(`            <p>How far apart your two ears sit. 17.5 cm is a head; a metre is a
              spaced pair of microphones, and the image opens out accordingly.</p>
          </div>
        </div>`,
`            <p>How far apart your two ears sit. 17.5 cm is a head; a metre is a
              spaced pair of microphones, and the image opens out accordingly.</p>
          </div>
          <div class="ctl">
            <h3>Head</h3>
            <p class="what">Built in, or a SOFA file</p>
            <p>The ears you listen with. <b>Load SOFA</b> takes any measured
              head in the standard SOFA format (AES69) &mdash; your own, or one
              from a public set such as SADIE, ARI or HUTUBS &mdash; for live
              playing and every render; <b>Built in</b> goes back. It is kept
              with the project.</p>
          </div>
        </div>`);

// ---- page 21: two new rows in the export table
rep(`            <tr><td>PICTURE</td><td>AS SHOWN, CINEMATIC, PHOTO 16 or PHOTO 64 &mdash; the PHOTO settings trace that many samples into every frame, at seconds a frame</td></tr>`,
`            <tr><td>PICTURE</td><td>AS SHOWN, CINEMATIC, PHOTO 16 or PHOTO 64 &mdash; the PHOTO settings trace that many samples into every frame, at seconds a frame</td></tr>
            <tr><td>SOUND</td><td>LIVE, HIGH, ULTRA or ULTRA + BASS &mdash; how the take's sound is rendered (next page)</td></tr>`);
rep(`            <tr><td>REVEAL</td><td>shows the last video in Explorer</td></tr>`,
`            <tr><td>REVEAL</td><td>shows the last video in Explorer</td></tr>
            <tr><td>BOUNCE WAV</td><td>the take's sound alone, at the SOUND chosen, as a 24-bit WAV beside the videos</td></tr>`);
rep(`          <p>The sound is re-rendered offline, at full quality, through a fresh
            engine.`, `          <p>The sound is re-rendered offline, at the SOUND chosen, through a fresh
            engine.`);
rep('<figcaption><b>The export panel</b>, with a take held.', '<figcaption><b>The export panel</b>, with a take held and SOUND at ULTRA + BASS.');

// ---- the new page, after Record and export
const PAGE = `<!-- ================================================  RENDERING  ======== -->
<section class="page">
  <div class="pad tight">
    <p class="kick">Rendering</p>
    <h2 class="display">Four ways to hear a take</h2>
    <p class="sub">Offline there is no audio thread to keep up with, so a take's
      sound can be worked out with far less approximation. None of this changes
      what you hear while you play, or what it costs.</p>
    <hr class="rule" />

    <div class="cols">
      <div class="c-58">
        <div class="grid2" style="gap:1mm 7mm">
          <div class="ctl small">
            <h3>Live</h3>
            <p class="what">What you heard</p>
            <p>The live engine, sample for sample: reflections to the second
              order, a statistical late field per room.</p>
          </div>
          <div class="ctl small">
            <h3>High</h3>
            <p class="what">Reflections to the 4th order</p>
            <p>Five times as many wall reflections (128 against 24 in the tiled
              living room) and twice the distinct early echoes; the late field
              hands over exactly what they now carry.</p>
          </div>
          <div class="ctl small">
            <h3>Ultra</h3>
            <p class="what">6th order, and traced tails</p>
            <p>The statistical reverb is replaced by one traced through the
              actual rooms: rays bounce off the folded walls, the furniture, the
              rug and the doors as they stand, and what crosses a sphere round
              your head becomes the tail, band by band and direction by
              direction. Each room rings with its own shape, and the hall next
              door comes round the door &mdash; or through the wall.</p>
          </div>
          <div class="ctl small">
            <h3>Ultra + bass</h3>
            <p class="what">Below 220 Hz, a real wave</p>
            <p>Under the crossover the whole apartment is simulated as air on a
              10 cm grid: room modes, bass bending round a door, a door opening
              mid-take. Sound through walls and leaves still comes from the
              measured transmission losses.</p>
          </div>
        </div>

        <div class="box" style="margin-top:3mm">
          <h4>Checked against the live engine</h4>
          <p>The traced tail lands within <b>0.4 to 0.9 dB</b> of the
            statistical one in four materials, decays within Eyring's times,
            and puts the hall's tail <b>60 dB</b> louder into a dead room once
            the door opens. Through a shut wall it is within <b>0.6 dB</b>.
            The wave gives the living room's axial modes at <b>28.6</b> and
            <b>34.3 Hz</b>, where c&nbsp;/&nbsp;2L puts them, and its direct
            sound lands within <b>0.2 dB</b> of the engine's. Above the
            crossover ULTRA + BASS changes 1 kHz by <b>0.02 dB</b>.</p>
        </div>
      </div>

      <div class="c-36 fit" style="margin-left:auto">
        <figure>
          <img class="shot" src="img/exp-pop.jpg" alt="" />
          <figcaption><b>SOUND</b> in the export panel. It applies to the
            MP4 and to BOUNCE WAV alike.</figcaption>
        </figure>
        <div class="box cool" style="margin-top:4mm">
          <h4>What it costs</h4>
          <p>Only time, and only when you render. On a many-core desktop HIGH
            takes about one and a half times as long as the take, ULTRA a
            little more, and ULTRA + BASS about two and a half times &mdash; a
            four-second take in about ten seconds. A take that moves is traced again every 25 cm of
            walking.</p>
        </div>
        <div class="box" style="margin-top:4mm">
          <h4>Honestly</h4>
          <p>Rays mean little below about 60 Hz and are not asked to; the wave
            has no walls to pass through. DIRECT, EARLY and REVERB do not reach
            below the crossover in ULTRA + BASS &mdash; the wave is all of them
            at once.</p>
        </div>
      </div>
    </div>
  </div>
  <div class="folio"><span>Rendering</span><span class="n">22</span></div>
</section>

<!-- ============================================  SPECIFICATIONS  ======== -->`;
rep('<!-- ============================================  SPECIFICATIONS  ======== -->', PAGE);
rep('<div class="folio"><span>Specifications</span><span class="n">22</span></div>', '<div class="folio"><span>Specifications</span><span class="n">23</span></div>');

// ---- specifications
rep(`checks, and 26 for furniture, pictures and light, all clear, beside 141
      that drive the panel.`, `checks, 26 for furniture, pictures and light and 33 for the offline
      renders, all clear, beside 141 that drive the panel.`);
rep(`<tr><td>Head</td><td>MIT KEMAR compact set, 368 directions, diffuse-field equalised, minimum phase with a separate onset delay</td></tr>`,
    `<tr><td>Head</td><td>MIT KEMAR compact set, 368 directions, diffuse-field equalised, minimum phase with a separate onset delay; or any SOFA file</td></tr>`);
rep(`<tr><td>Reflections</td><td>image sources to second order; a general surface-sequence search once a wall is broken</td></tr>`,
    `<tr><td>Reflections</td><td>image sources to second order (4th and 6th offline); a general surface-sequence search once a wall is broken</td></tr>`);
rep(`<tr><td>Reverberator</td><td>one 16-line feedback delay network per room, allpass diffusers inside every line, coupled through the doors and walls</td></tr>`,
    `<tr><td>Reverberator</td><td>one 16-line feedback delay network per room, coupled through the doors and walls. Offline: tails traced through the rooms, and a wave simulation below 220 Hz</td></tr>`);

// ---- build tag
s = s.split('BUILD 260925.3').join('BUILD 260925.4');

if (miss.length) { console.log('MISSES:\n' + miss.join('\n')); process.exit(1); }
if (NL === '\r\n') s = s.split('\n').join('\r\n');
fs.writeFileSync(f, s);
console.log('ok');
