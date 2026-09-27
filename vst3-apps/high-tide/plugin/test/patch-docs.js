/*  The manual and the landing page learn the starters, the hints, DETUNE and
    the centre-line construction. Exact-count anchors; nothing on a miss. */
"use strict";
const fs = require("fs");
const misses = [];
function edit(file, pairs) {
  let s = fs.readFileSync(file, "utf8");
  for (const [a, b] of pairs) {
    const c = s.split(a).length - 1;
    if (c !== 1) { misses.push(file.split(/[\\/]/).pop() + ": expected 1 of " + JSON.stringify(a.slice(0, 60)) + ", found " + c); continue; }
    s = s.split(a).join(b);
  }
  return [file, s];
}

const MAN = "C:/Users/peter/b/HighTide/docs/manual/manual.html";
const LAND = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/high-tide/index.html";

const man = edit(MAN, [
  //  ---- cover
  [`  <div class="tag">A wavetable synth that stores no waveforms. A frame is a bowl on a sculpted
  terrain, the waveform is what a mass does when it is dropped into it, and position is where the
  ball is. The tide floods the passes; pins on a timeline tow the ball where you want it.</div>`,
   `  <div class="tag">A wavetable synth that stores no waveforms. A frame is a bowl on a sculpted
  terrain, the waveform is what a mass does when it is dropped into it, and position is where the
  ball is. The tide floods the passes; pins on a timeline tow the ball where you want it. Seventeen
  starters to play at once, and every control tells you what it is for.</div>`],
  [`    <div><b>HIGH TIDE</b>terrain &middot; ball &middot; tide &middot; pins &middot; rock &middot; twelve factory terrains &middot; world rack</div>`,
   `    <div><b>HIGH TIDE</b>terrain &middot; ball &middot; tide &middot; pins &middot; rock &middot; 17 starters &middot; 12 terrains &middot; world rack</div>`],

  //  ---- 02: start here, and the hints
  [`    <p>Open the plug-in, or the standalone. It opens on <b>FIRST LIGHT</b>: two sine valleys with a
    small ridge between them, a pin that sweeps the ball from the first to the second over two and a
    half seconds, and a release pin that brings it home.</p>`,
   `    <p>Open the plug-in, or the standalone. It opens on <b>SOFT KEYS</b>, the first of the
    starters: a warm poly key whose timbre travels a little while you hold it.</p>
    <div class="callout"><b>Two things to do first.</b> Hover anything at all &mdash; a knob, a
    button, a lane of the timeline &mdash; and after a moment a note appears beside it saying what
    it is and how it is meant to be used. And open <b>FACTORY</b>: the <b>STARTERS</b> group is
    seventeen ordinary instruments (basses, leads, keys, organs, pads) that behave the way you
    expect, and the <b>TERRAINS</b> group below it is the instrument showing what else it can do.
    The <b>HINTS</b> button in the header turns the notes off once you know your way around.</div>`],
  [`    <div class="plate wide"><img src="img/header.jpg" alt="The header"><div class="cap">FACTORY &middot; PATCHES &middot; SAVE &middot; OPEN &middot; PNG IN &middot; PNG OUT &middot; PHOTO &middot; KEYS &middot; PANIC &middot; the world rack.</div></div>`,
   `    <div class="plate wide"><img src="img/header.jpg" alt="The header"><div class="cap">FACTORY &middot; PATCHES &middot; SAVE &middot; OPEN &middot; PNG IN &middot; PNG OUT &middot; PHOTO &middot; KEYS &middot; HINTS &middot; PANIC &middot; the world rack.</div></div>
    <div class="plate"><img src="img/hints.jpg" alt="A hint open beside a control"><div class="cap">Every control says what it is, how it is used, and what it is set to against its default. The note is always placed beside the control, never over it.</div></div>`],

  //  ---- 03: the centre line
  [`    <h3>Three taps</h3>`,
   `    <h3>The centre line, and why a bowl can be rich and still in tune</h3>
    <p>Write a bowl as a centre line with the parabola's width around it, <b>x = c(h) &plusmn;
    &radic;(2h)</b>. The width at every height is then the reference parabola's whatever <b>c</b>
    does &mdash; so the bowl's centre line may wander with height however it likes and <b>the pitch
    does not move</b>. A straight centre line leans the bowl and brings in the even harmonics; a
    wavy one gives a wave nothing like a sine, exactly in tune. That construction is what every
    starter is built from, and it is what TUNE LOCK maintains while you sculpt.</p>
    <h3>Three taps</h3>`],

  //  ---- 05: DETUNE
  [`    <h3>Unison</h3>
    <p>UNISON puts up to four balls in the same bowl. SPREAD scatters how hard each is struck and
    lets them push each other gently. In an isochronous bowl that is brightness scatter and phase
    scatter with almost no detune; in a box it is a lot of detune, because in a box energy is
    pitch. WIDTH stands the balls across the stereo field.</p>`,
   `    <h3>Unison</h3>
    <p>UNISON puts up to four balls in the same bowl. <b>DETUNE</b> runs their clocks a little
    apart &mdash; fifty cents across the fan at full &mdash; which is the familiar unison spread,
    and the ball that plays your note is never one of the detuned ones, so the pitch stays exact
    however wide it gets. SPREAD scatters how hard each ball is struck instead, so they differ in
    brightness and phase; in an isochronous bowl that is almost no detune, and in a box it is a
    great deal, because in a box energy is pitch. WIDTH stands them across the stereo field.</p>`],

  //  ---- 08: the two factory groups
  [`    <table>
      <tr><th>factory</th><th>what it is</th></tr>
      <tr><td class="k">FIRST LIGHT</td><td>two sine valleys, a small ridge, a slow sweep and a release pin</td></tr>`,
   `    <h3>STARTERS &mdash; the safe ground</h3>
    <p>Seventeen ordinary instruments, every one of them built on tune-locked bowls with no drive,
    so the note is the note whatever the strike. They are the place to begin, and the place to
    start editing from.</p>
    <table>
      <tr><th>starter</th><th>what it is</th></tr>
      <tr><td class="k">SOFT KEYS</td><td>a warm poly key whose timbre travels a little while held</td></tr>
      <tr><td class="k">SUB</td><td>a mono sub on the position tap, dark and short</td></tr>
      <tr><td class="k">PLUCK BASS</td><td>heavy friction: the brightness closes as the ball sinks</td></tr>
      <tr><td class="k">WIDE BASS</td><td>two detuned balls, wide</td></tr>
      <tr><td class="k">SQUARE BASS</td><td>a box held in tune by the servo</td></tr>
      <tr><td class="k">SAW LEAD</td><td>a wavy centre line on the velocity tap, two balls detuned</td></tr>
      <tr><td class="k">SQUARE LEAD</td><td>a box with the servo on, bright</td></tr>
      <tr><td class="k">SWEEP LEAD</td><td>a pin sweeps the centre line open: a filter sweep with no filter</td></tr>
      <tr><td class="k">GLIDE LEAD</td><td>legato with portamento, a slow breath on the tide</td></tr>
      <tr><td class="k">ORGAN</td><td>frictionless and sustaining, two balls barely apart</td></tr>
      <tr><td class="k">ELECTRIC PIANO</td><td>velocity into brightness, the bell decaying into the body</td></tr>
      <tr><td class="k">BELL</td><td>a pit bowl struck hard, long tail</td></tr>
      <tr><td class="k">CLAV</td><td>the force tap, short and hard</td></tr>
      <tr><td class="k">SLOW PAD</td><td>four balls, slow attack, the tide breathing</td></tr>
      <tr><td class="k">MORPH PAD</td><td>a pin strolling across three centre lines over eight seconds</td></tr>
      <tr><td class="k">GLASS</td><td>a wavy bowl, wide open tone, very long release</td></tr>
      <tr><td class="k">DRONE</td><td>frictionless, four balls, the position wandering by itself</td></tr>
    </table>
    <h3>TERRAINS &mdash; the instrument showing what it is</h3>
    <table>
      <tr><th>terrain</th><th>what it is</th></tr>
      <tr><td class="k">FIRST LIGHT</td><td>two sine valleys, a small ridge, a slow sweep and a release pin</td></tr>`],
  [`      <tr><td class="k">GROUNDSWELL</td><td>legato sub-bass, a double well rocked at 1/3</td></tr>
    </table>`,
   `      <tr><td class="k">GROUNDSWELL</td><td>legato sub-bass, a double well rocked at 1/3</td></tr>
    </table>
    <div class="plate wide"><img src="img/starters.jpg" alt="The factory menu, showing the two groups"><div class="cap">FACTORY, in two groups: the starters first, the terrains below.</div></div>`],

  //  ---- 09: the measured table
  [`      <tr><td class="k">cost</td><td>a four-note chord 2.8 % of one core; 12 voices &times; 4 balls 27 % at 4x, 19 % at 2x</td></tr>
    </table>
    <p>Twelve factory terrains, each bounded, exactly silent with no note, deterministic to the
    sample, and no two alike. 123 checks, all clear.</p>`,
   `      <tr><td class="k">detune</td><td>the played ball is 0.000 cents off at any DETUNE; four balls at 40 % swing the envelope 21.9&times;</td></tr>
      <tr><td class="k">the starters</td><td>every one within <b>3 cents</b> of the note at both a brushed and a hammered key, and within 7 dB of each other</td></tr>
      <tr><td class="k">cost</td><td>a four-note chord 2.8 % of one core; 12 voices &times; 4 balls 27 % at 4x, 19 % at 2x</td></tr>
    </table>
    <p>Twenty-nine factory patches, each bounded, exactly silent with no note, deterministic to the
    sample, and no two alike. 283 checks, all clear.</p>`],
  [`    <div class="foot-note">HIGH TIDE is a Brokild instrument.`,
   `    <p>Every control, button, tool and timeline lane carries a hint, and the panel probe refuses
    to pass if one is missing: 89 of 89.</p>
    <div class="foot-note">HIGH TIDE is a Brokild instrument.`]
]);

const land = edit(LAND, [
  [`      <span class="buildtag">Build 260904.1</span>`, `      <span class="buildtag">Build 260904.2</span>`],
  [`      <tr><td>Build</td><td>260904.1 — shown on the loading screen and in the header</td></tr>`,
   `      <tr><td>Build</td><td>260904.2 — shown on the loading screen and in the header</td></tr>`],
  [`      into chaos, an octave at a time.</p>`,
   `      into chaos, an octave at a time. Seventeen <b>starters</b> to play straight away, and every
      control on the panel tells you what it is for.</p>`],
  [`<section class="block">
  <div class="wrap">
    <h2>Pins, the tide, and a timeline that runs from the key</h2>`,
   `<section class="block">
  <div class="wrap">
    <h2>Seventeen sounds to start from, and a panel that explains itself</h2>
    <p class="sub">A new instrument you cannot guess your way into is a curiosity. These two make it playable on the first day.</p>
    <p>The FACTORY menu opens in two groups. <b>STARTERS</b> is seventeen ordinary instruments —
      subs, plucks, wide basses, saw and square leads, an organ, an electric piano, a bell, a clav,
      pads, a drone — each built on bowls that are exactly in tune whatever the strike, with no
      drive and a predictable tide. They sound the way their names sound, they are levelled against
      each other, and they are the place to start editing. <b>TERRAINS</b> below them is the
      instrument showing what else it is: thresholds, cascades, stranded balls, a parametric pump.</p>
    <p>And every control, button, tool and lane of the timeline carries a <b>hint</b>: hover it and
      a note appears beside it — never under the pointer, never over the control — saying what it
      is, how it is meant to be used, and what it is set to against its default. One button in the
      header turns them off when you no longer need them.</p>
    <figure>
      <img src="img/hints.jpg" alt="A hint open beside a control on the High Tide panel">
      <figcaption>The hint for DETUNE, placed beside the rail rather than over it. Eighty-nine of them, and the panel probe fails if a single control is missing one.</figcaption>
    </figure>
  </div>
</section>

<section class="block">
  <div class="wrap">
    <h2>Pins, the tide, and a timeline that runs from the key</h2>`],
  [`      <tr><td>Cost</td>`, `      <tr><td>Cost</td>`],
  [`      <tr><td>Aliasing</td><td>Non-harmonic floor at 4x: parabola &minus;90 dB, box &minus;59 dB, pit &minus;63 dB at B5</td></tr>`,
   `      <tr><td>Aliasing</td><td>Non-harmonic floor at 4x: parabola &minus;90 dB, box &minus;59 dB, pit &minus;63 dB at B5</td></tr>
      <tr><td>The starters</td><td>Every one within 3 cents of the note at a brushed key and a hammered one, and within 7 dB of each other</td></tr>`],
  [`    <p>123 checks, all clear, on build 260904.1.</p>`,
   `    <p>283 checks, all clear, on build 260904.2 — and a panel probe that refuses to pass if any control is missing its hint.</p>`],
  [`      <tr><td>Polyphony</td><td>Twelve voices of up to four balls, or mono and legato</td></tr>`,
   `      <tr><td>Polyphony</td><td>Twelve voices of up to four balls, or mono and legato</td></tr>
      <tr><td>Patches</td><td>17 starters and 12 terrains built in; your own saved as one file each</td></tr>`]
]);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(man[0], man[1], "utf8");
fs.writeFileSync(land[0], land[1], "utf8");
console.log("manual and landing page updated");
