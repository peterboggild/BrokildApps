/*  Bring the Thin Walls landing page up to the 2026-09-22 build. Exact
    anchors, counted; nothing is written on a miss.

    Verified afterwards by RENDERING over http, never by grepping - the front
    page builds its cards from JSON at runtime and a file:// load proves
    nothing about either page.                                               */

const fs = require("fs");
const P = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/thin-walls/index.html";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function once(what, from, to) {
  const n = s.split(from).length - 1;
  if (n !== 1) { miss.push(what + "  (matched " + n + ")"); return; }
  s = s.replace(from, to);
}
function all(what, from, to, expect) {
  const n = s.split(from).length - 1;
  if (n !== expect) { miss.push(what + "  (matched " + n + ", wanted " + expect + ")"); return; }
  s = s.split(from).join(to);
}

/* ------------------------------------------------------------------- hero */
once("hero: the lede gains what today added",
`      and the tail each room builds for itself. Walk around it while it plays.
    </p>`,
`      and the tail each room builds for itself. Walk around it while it plays.
    </p>
    <p class="lede" style="margin-top:1rem">
      Five surface treatments, chosen separately for the walls, the floor and
      the ceiling of each room. Two walls of every room can be broken at a
      movable point and splayed outward. And every door has a handle you can
      take hold of and swing.
    </p>`);

all("the handbook page count", "PDF, 14 pages", "PDF, 17 pages", 1);
all("the handbook page count, second button", "<small>14 pages</small>", "<small>17 pages</small>", 1);

/* --------------------------------------------------------------- the flat */
once("the flat: the materials card",
`      <div class="c"><div class="n">four grades</div><h4>Walls you can change</h4><p>Absorbing, furnished, plaster or tiled, per room, from published octave-band absorption. The hall in tiles rings for five and a half seconds at 1 kHz.</p></div>`,
`      <div class="c"><div class="n">nine surfaces</div><h4>Walls you can change</h4><p>Absorbing, furnished, plaster, tiled or studio &mdash; chosen separately for the walls, the floor and the ceiling of each room, from published octave-band absorption. The hall in tiles rings for five and a half seconds at 1 kHz.</p></div>`);

/* ------------------------- a new block, after "what you hear behind a door" */
once("insert the room-shaping block",
`<section class="block">
  <div class="wrap">
    <h2>Four sources, two kinds, two input buses</h2>`,
`<section class="block">
  <div class="wrap">
    <h2>The room is yours to change</h2>
    <p class="sub">Not a size knob and a decay knob &mdash; the surfaces and
      the shape, with the reverberation time falling out of them.</p>

    <div class="figrow">
      <figure style="margin-top:0">
        <img src="img/ctrl-materials.jpg" alt="The materials grid: a row per room, a column for walls, floor and ceiling" />
        <figcaption><b>A material per surface.</b> Between a source on the
          floor and a listener on the floor, the first two reflections to
          arrive are the floor's and the ceiling's. A carpet is the most
          lopsided absorber in a normal room &mdash; almost nothing below
          250 Hz, a great deal above 2 kHz &mdash; so <b>floor = ABSORBING</b>
          is a real tuning control, and <b>ceiling = ABSORBING</b> is the cloud
          every control room has over the desk. Both default to AS WALLS, so a
          room you never touch is the room it always was.</figcaption>
      </figure>
      <figure style="margin-top:0">
        <img src="img/plan-folded.jpg" alt="The plan with four walls broken and splayed outward" />
        <figcaption><b>Breakable walls.</b> The two walls of each room that
          carry no doorway can be split at a movable point and that point
          pushed out, so the room stops being rectangular. Drag the diamond in
          the plan. A centred source in a square hard room measures
          <b>2.4 dB</b> uneven across 300 Hz&ndash;1.2 kHz and <b>2.0 dB</b>
          with both walls broken; push a wall 0.6 m out and the room's own
          decay goes from 1.766 s to <b>1.801 s</b>, because the room really is
          bigger.</figcaption>
      </figure>
    </div>

    <div class="note" style="border-color:rgba(200,105,92,.4)">
      <b>Push the break OUTWARD.</b> Pulling it inward makes the wall concave,
      which focuses sound at a point and is worse than leaving it flat. Real
      studios splay outward. And be clear about what it does here: the splay
      acts on the <b>early reflections</b>, not on standing waves &mdash; this
      model's late field is statistical rather than modal. Above the Schroeder
      frequency, about 72 Hz in the hall as a studio, that is where splay does
      its audible work in a real room too. Below it, absorption is what helps.
    </div>

    <h3>STUDIO, the fifth material</h3>
    <p>
      Its absorption was not guessed: it was solved backwards from Eyring's
      formula for a decay that <b>does not move with frequency</b>, and what
      comes out is roughly constant absorption near a quarter with a little
      relief at the top to pay for air absorption &mdash; which is what
      broadband absorbers plus bass traps physically are. The hall as a studio
      measures <b>0.71 s at 250 Hz, 1 kHz and 4 kHz alike</b>, a spread of
      1.00&times;, where the same room tiled spreads 3.09&times;. The living
      room: 0.39 / 0.40 / 0.41 s.
    </p>
    <p>
      Its treatment is <b>diffusion, not absorption</b>. Reflections keep their
      energy and lose the specular direction, so the room stays live while the
      flutter goes: the specular reflections carry <b>18 %</b> of what a
      plastered room's carry, and the rest is scattered into the room rather
      than taken out of it.
    </p>

    <h3>And the doors have handles</h3>
    <p>
      In the 3D view, walk up to a door &mdash; within 2.5 m, in front of you,
      not through a wall &mdash; and its handle lights up under the pointer.
      Click to slam it or throw it open; drag to swing it. Meanwhile
      <b>W&nbsp;A&nbsp;S&nbsp;D and the arrow keys always win</b>: they walk
      and turn even when a menu or a fader has the focus, so you can set a
      material and then simply walk.
    </p>
  </div>
</section>

<section class="block">
  <div class="wrap">
    <h2>Four sources, two kinds, two input buses</h2>`);

/* -------------------------------------------------------------- the bench */
once("bench: the lede",
`    <p class="sub">67 checks, all clear. The bench renders real audio and
      measures it; these are its numbers.</p>`,
`    <p class="sub">90 checks, all clear, beside 73 that drive the panel. The
      bench renders real audio and measures it; these are its numbers.</p>`);

once("bench: the facts",
`      <div class="fact"><div class="v">−686 µs</div><div class="l">interaural delay at 90°, against Woodworth's 666</div></div>
      <div class="fact"><div class="v">6 dB</div><div class="l">per doubling of distance, with each ear at its own distance</div></div>
      <div class="fact"><div class="v">3.5 dB</div><div class="l">worst error in the direct-to-reverberant ratio at the critical distance, every material</div></div>
      <div class="fact"><div class="v">12 of 12</div><div class="l">room and material cases matching Eyring's reverberation time</div></div>
      <div class="fact"><div class="v">−20 dB</div><div class="l">at 1 kHz from shutting a door &mdash; and 4 kHz loses 8 dB more than 250 Hz</div></div>
      <div class="fact"><div class="v">24 dB</div><div class="l">down through the party wall alone, against the same distance in your own room</div></div>
      <div class="fact"><div class="v">18 dB</div><div class="l">brought back by opening both doors of the two-door route</div></div>
      <div class="fact"><div class="v">81 dB</div><div class="l">down above 6 kHz with a source walking at 2 m/s on a held tone: nothing clicks</div></div>`,
`      <div class="fact"><div class="v">−708 µs</div><div class="l">interaural delay at 90°, against Woodworth's 666</div></div>
      <div class="fact"><div class="v">0.1 dB</div><div class="l">how far the direct path sits from the physics, 10 Hz to 16 kHz, wherever a source stands</div></div>
      <div class="fact"><div class="v">15 of 15</div><div class="l">room and material cases matching Eyring's reverberation time</div></div>
      <div class="fact"><div class="v">5.6–6.6 dB</div><div class="l">spectral roughness of the tail, against the 5.57 dB of a perfectly diffuse field</div></div>
      <div class="fact"><div class="v">1.00×</div><div class="l">how far a studio-treated hall's decay moves across 250 Hz, 1 kHz and 4 kHz; tiled, it spreads 3.09×</div></div>
      <div class="fact"><div class="v">−23 dB</div><div class="l">at 1 kHz from shutting a door &mdash; and 4 kHz loses 12 dB more than 250 Hz</div></div>
      <div class="fact"><div class="v">31 dB</div><div class="l">down through the party wall and a shut door, against the same distance in your own room</div></div>
      <div class="fact"><div class="v">0.09 dB</div><div class="l">how far the general path search lands from the exact shoebox on a wall broken by 2 mm</div></div>`);

once("bench: the closing note gains the reader",
`      separate onset delay and resampled to your host's rate. Air absorption is
      ISO 9613-1, diffraction is Maekawa, absorption coefficients are from the
      standard tables. None of it was tuned by ear afterwards.`,
`      separate onset delay and resampled to your host's rate. Air absorption is
      ISO 9613-1, diffraction is Maekawa, absorption coefficients are from the
      standard tables. Delays are read with a 16-tap windowed sinc, so a source
      may stand anywhere without losing the top octave. None of it was tuned by
      ear afterwards.`);

/* ------------------------------------------------------- specifications */
once("specs: rooms",
`      <tr><td>Rooms</td><td>Three, fixed: 6×5×2.8 m, 3×3.5×2.5 m and 12×9×5 m, joined by three 0.90 m doors</td></tr>`,
`      <tr><td>Rooms</td><td>Three: 6×5×2.8 m, 3×3.5×2.5 m and 12×9×5 m, joined by three 0.90 m doors. Two walls of each can be broken and pushed ±0.6 m</td></tr>
      <tr><td>Materials</td><td>Five &mdash; absorbing, furnished, plaster, tiled, studio &mdash; chosen per surface: walls, floor and ceiling of each room</td></tr>`);

once("specs: reflections, reverberation, parameters and cost",
`      <tr><td>Reflections</td><td>Image sources to second order, up to 24 per room; a bounce into an open doorway is deleted</td></tr>
      <tr><td>Reverberation</td><td>One 16-line feedback delay network per room, per-band decay from Eyring, coupled through the doors and walls</td></tr>`,
`      <tr><td>Reflections</td><td>Image sources to second order; a general surface-sequence search once a wall is broken, validated against the exact arithmetic to 0.09 dB</td></tr>
      <tr><td>Reverberation</td><td>One 16-line feedback delay network per room, allpass diffusers inside every line, per-band decay from Eyring, coupled through the doors and walls</td></tr>
      <tr><td>Delay reader</td><td>16-tap windowed sinc; the direct path holds to a tenth of a decibel from 10 Hz to 16 kHz</td></tr>`);

once("specs: parameters and CPU",
`      <tr><td>Parameters</td><td>47, all automatable &mdash; including every source position and your own</td></tr>
      <tr><td>CPU</td><td>One source about 11 % of a core at 48 kHz; four with every door open, 28 %</td></tr>`,
`      <tr><td>Parameters</td><td>65, all automatable &mdash; including every source position, every surface, every broken wall and your own position</td></tr>
      <tr><td>CPU</td><td>One source about 17 % of a core at 48 kHz; four with every door open, 40 %</td></tr>`);

once("specs: the honest limits note",
`      Honest limits, also in the handbook: the floor plan is fixed, reflections
      are specular and stop at the second order, and two sources in one room
      share that room's field &mdash; correct, but the share each hands it is
      taken at 1 kHz rather than per band.`,
`      Honest limits, also in the handbook: where the rooms are and which doors
      join them cannot change; reflections are specular (only STUDIO scatters)
      and stop at the second order; breaking a wall moves the early reflections
      rather than the room's standing waves; and two sources in one room share
      that room's field &mdash; correct, but the share each hands it is taken
      at 1 kHz rather than per band.`);

if (miss.length) {
  console.error("NOTHING WRITTEN. " + miss.length + " anchor(s) did not match:");
  miss.forEach(function (m) { console.error("  - " + m); });
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log("landing page updated");
