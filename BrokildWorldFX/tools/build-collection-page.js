/*  The collection page: one download, nine plugins.

    Same family shell as every other landing page, so it sits among them rather
    than looking like a different site. The body is a directory of what is in
    the box, each entry saying what the instrument actually IS rather than
    listing its features.

    Moved here from FullMetalRacket/tools on 2026-09-04 and rewritten for nine:
    the collection spans the fleet, so it belongs with the fleet's tooling, and
    two copies of a builder drift (Clone Wars taught that lesson at a cost of
    41 lines).

    Run  build-collection-zip.ps1  first — the size in the download button
    comes from the zip on disk.
*/
"use strict";
const fs = require("fs");

const R = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/";
const SRC = R + "vst3-apps/black-rider/index.html";
const OUT = R + "vst3-apps/collection/index.html";
const ZIP = R + "vst3-apps/collection/Brokild-Collection-win64.zip";

let s = fs.readFileSync(SRC, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);

const MB = fs.existsSync(ZIP) ? Math.round(fs.statSync(ZIP).size / 1048576) : 0;
if (!MB) { console.error("ABORT: no zip - run build-collection-zip.ps1 first"); process.exit(1); }

const heroAt = s.indexOf("<header class=\"hero\">");
const footAt = s.indexOf("<footer>");
if (heroAt < 0 || footAt < 0) { console.error("ABORT: splice points not found"); process.exit(1); }

let head = s.slice(0, heroAt)
  .replace("<title>Black Rider — VST3 analogue monosynth | BrokildApps</title>",
           "<title>The Brokild Collection — nine free VST3 plugins | BrokildApps</title>")
  .replace(/<meta name="description" content="[^"]*" \/>/,
    '<meta name="description" content="Every Brokild plugin in one download: eight instruments and one effect for Windows. Brain Scan, High Tide, Photo Synth, Escape Room, Blade Ruiner, Black Rider, Clone Wars, Full Metal Racket and Martian Gain - with their manuals, their standalones, and the shared world-effects rack that runs inside all of them. Free, no installer, no account." />')
  .replace("--accent: #a9651a;", "--accent: #0e7f6c;")
  .replace("--accent-bright: #c67f28;", "--accent-bright: #14a189;")
  .replace("--accent: #f0a94a; --accent-bright: #ffc474; --amber: #e8b45a;",
           "--accent: #3fd8b8; --accent-bright: #6ff0d4; --amber: #e8b45a;");
if (head.indexOf("#0e7f6c") < 0) { console.error("ABORT: the palette swap missed"); process.exit(1); }
if (head.indexOf("nine free VST3") < 0) { console.error("ABORT: the title swap missed"); process.exit(1); }

const foot = s.slice(footAt);

//  Each entry: what it is in one line, then the thing about it worth knowing.
const ITEMS = [
  ["Brain Scan", "brain-scan", "Instrument",
   "A synth that stores no waveforms but a <em>volume</em> — a scalar field filling a cube, the kind of number a scanner records — and reads a curve through it. One cycle of the sound is the field along that curve, so bending the curve changes the timbre because the curve is now somewhere else in the body.",
   "SCAN blends the <em>geometry</em> of two curves before anything is read, so halfway between two sounds is tissue neither of them has visited. Nine specimens, one of them a simulated brain you draw your lines on."],
  ["High Tide", "high-tide", "Instrument",
   "A wavetable synth that stores no waveforms either: every frame is a bowl on a sculpted terrain, and the sound is what a mass does when it is dropped into it, integrated by Newton's law at audio rate. A parabola is a sine; a pit in the floor is a pulse.",
   "A rising tide floods the passes between the valleys so the ball can be towed anywhere; pins on a timeline put it exactly where you want it, when you want it there. A photograph makes a landscape of bowls."],
  ["Photo Synth", "photo-synth", "Instrument",
   "A synthesiser you play by moving a cursor across a photograph. The colours under the cursor <em>are</em> the sound — hue, saturation and brightness map onto the oscillator, the filter and the effects, so a picture is a patch and moving across it is a performance.",
   "Load your own photos; a recorded cursor path becomes a phrase that plays itself."],
  ["Escape Room", "escape-room", "Instrument",
   "Five noise cells, a playable filter, three traps — and a SIGIL, a number from 0 to 63 that seeds eight modulation wires deterministically. The same sigil is the same machine, on any computer, forever.",
   "The panel is cryptic on purpose. The knobs are named ALEPH and BETH and the real values only appear while you turn one. There is a legend, if you can find it."],
  ["Blade Ruiner", "blade-ruiner", "Instrument",
   "Three layers at once: a drone city of nine detuned saws and filtered rain, an eight-voice polysynth with a ladder filter, and a sixteen-step sequencer built from a six-bit seed. At least one layer is always playing.",
   "A mood organ: dial a number from 0 to 999 and get an instrument and a line of text that belong to each other. All thousand are distinct, and a handful are Philip K. Dick's own codes."],
  ["Black Rider", "black-rider", "Instrument",
   "An analogue monosynth sitting between the MS-20 and the Moog. Two oscillators you can push into instability, drift and injection-lock, and one filter with three circuits on a switch — two Sallen-Key tempers and a transistor ladder.",
   "An eight-cable patch bay on a wing that folds out of the side, including the MS-20 trick of patching the amplifier back into the filter."],
  ["Clone Wars", "clone-wars", "Instrument",
   "Sixteen detuned oscillators in two armies of eight, crossfaded by a single fader — THE WAR — which can be set to take up to five minutes to cross.",
   "The panel visibly ages as you play it. There is a service bay if you want the scars repaired, and a slider that runs the sixteen from perfectly locked to completely adrift."],
  ["Full Metal Racket", "full-metal-racket", "Instrument",
   "An analogue drum machine whose twelve voices share a power rail, a shell and a bleed web — so a hard kick dips the whole machine in pitch as well as level, and hitting the snare rings the toms.",
   "A thirty-two step sequencer with a last step <em>per lane</em>, so polyrhythms cost one number. Two hundred generated kits and a fader that morphs between any two."],
  ["Martian Gain", "martian-gain", "Effect",
   "A multiband distortion. One to five bands, each running one of sixteen algorithms, each with its own limiter — and each level-matched by measurement, so turning DRIVE up changes what a band sounds like without changing how loud it is.",
   "The front panel comes off. Underneath is a patch bay where any band's audio or envelope can drive any other band's knobs, or move the crossovers themselves."]
];

const cards = ITEMS.map(function (it) {
  return [
'      <div class="c">',
'        <div class="n">' + it[2] + '</div>',
'        <h4><a href="../' + it[1] + '/index.html">' + it[0] + '</a></h4>',
'        <p>' + it[3] + '</p>',
'        <p style="margin-top:.6rem">' + it[4] + '</p>',
'      </div>'].join(NL);
}).join(NL);

const BODY = [
'<header class="hero">',
'  <div class="wrap">',
'    <div class="kicker">Nine plugins · Windows · Free</div>',
'    <h1>The Brokild <span>Collection</span></h1>',
'    <p class="tagline">Eight instruments and one effect, in a single download.</p>',
'    <p class="lede">Everything Brokild makes, with its manuals and its standalones. They are not a',
'      product line — they were built one at a time, each to answer a different question — but they',
'      share a rack of effects, a patch folder and a way of working: nothing in any of them is',
'      asserted, every claim is measured by a bench that renders real audio and reads the numbers',
'      off it.</p>',
'    <div class="btnrow">',
'      <a class="btn btn-primary" href="Brokild-Collection-win64.zip" download>',
'        Download all nine (' + MB + ' MB)</a>',
'      <span class="buildtag">September 2026</span>',
'    </div>',
'    <figure class="hero-shot" style="margin-top:2.4rem">',
'      <img src="img/collection.jpg" alt="The Full Metal Racket panel, one of the nine plugins in the collection">',
'    </figure>',
'  </div>',
'</header>',
'',
'<section class="block">',
'  <div class="wrap">',
'    <h2>What is in the box</h2>',
'    <p class="sub">Each folder holds the plugin, the standalone and its manual. Every one of them',
'      is also downloadable on its own if you would rather take them one at a time — and the copy',
'      in here is byte-for-byte the same file.</p>',
'    <div class="cards">',
cards,
'    </div>',
'  </div>',
'</section>',
'',
'<section class="block">',
'  <div class="wrap">',
'    <h2>What they have in common</h2>',
'    <div class="cards">',
'      <div class="c"><div class="n">Brokild World FX</div><h4>One rack, eight instruments</h4>',
'        <p>The same global effects behind a teal globe in every instrument — saturation, phaser,',
'        chorus, gate, echo, reverb, rotary, a step glitcher and more. A rack built in one opens in',
'        any of the others. Empty by default, and adds nothing until you put something in it.</p></div>',
'      <div class="c"><div class="n">SPECTRA</div><h4>Characters that possess the machine</h4>',
'        <p>Not effects on the mix — they reach inside and detune, pan, sag and dull the voices',
'        themselves, so the instrument plays differently rather than being processed differently.</p></div>',
'      <div class="c"><div class="n">One patch folder</div><h4>Documents, not Program Files</h4>',
'        <p>Every plugin keeps its patches in <code>Documents\\Brokild patches</code>, a folder each',
'        under one roof — deliberately not beside the installed plugin, which is somewhere an',
'        installer can reach and replace.</p></div>',
'      <div class="c"><div class="n">Measured</div><h4>Nothing is asserted</h4>',
'        <p>Each one is built against an offline bench that renders real audio and measures it —',
'        tuning in cents, aliasing in decibels, timing in samples. The numbers in the manuals come',
'        from that, not from anybody&rsquo;s opinion.</p></div>',
'    </div>',
'  </div>',
'</section>',
'',
'<section class="block">',
'  <div class="wrap">',
'    <h2>Getting them running</h2>',
'    <ol class="steps">',
'      <li><b>Download and unzip.</b> Right-click → <em>Extract All</em>. You get a folder per',
'        plugin.</li>',
'      <li><b>Copy the .vst3 folders.</b> Move each <code>&lt;name&gt;.vst3</code> <em>folder</em> —',
'        the whole folder, not just the file inside it — into',
'        <code>C:\\Program Files\\Common Files\\VST3\\</code>. A sub-folder such as',
'        <code>...\\VST3\\Brokild\\</code> is fine and tidier; hosts look inside.</li>',
'      <li><b>Rescan.</b> In Ableton Live: Preferences → Plug-Ins → Rescan. They appear under',
'        Brokild — eight instruments and one effect.</li>',
'      <li><b>Or just run them.</b> Every folder also has an <code>.exe</code>. No installation at',
'        all.</li>',
'    </ol>',
'    <div class="note"><b>If a window comes up blank</b>, install the free Microsoft Edge WebView2',
'      Runtime and reopen the plugin. The interfaces are drawn in a WebView2 surface, which every',
'      current Windows 10 and 11 already has. The audio keeps working with the window shut either',
'      way.</div>',
'  </div>',
'</section>',
''].join(NL);

fs.mkdirSync(R + "vst3-apps/collection", { recursive: true });
fs.writeFileSync(OUT, head + BODY + foot);
console.log("collection page written - nine plugins, " + MB + " MB");
