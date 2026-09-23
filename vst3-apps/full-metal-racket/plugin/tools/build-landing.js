/*  Build the Full Metal Racket landing page.

    The shell — head, CSS, top bar, footer — is the family's, lifted from an
    existing page so the pages stay siblings rather than drifting apart. Only
    the palette and the body are this machine's own: cream and amber against
    the fleet's black, because that is what the instrument looks like.
*/
"use strict";
const fs = require("fs");

const R = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/";
const SRC = R + "vst3-apps/black-rider/index.html";
const OUT = R + "vst3-apps/full-metal-racket/index.html";

let s = fs.readFileSync(SRC, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);

const heroAt = s.indexOf("<header class=\"hero\">");
const footAt = s.indexOf("<footer>");
if (heroAt < 0 || footAt < 0) { console.error("ABORT: splice points not found"); process.exit(1); }

let head = s.slice(0, heroAt);
const foot = s.slice(footAt);

// ── the shell, retitled and repainted ─────────────────────────────────────
head = head
  .replace(
    "<title>Black Rider — VST3 analogue monosynth | BrokildApps</title>",
    "<title>Full Metal Racket — VST3 analogue drum machine | BrokildApps</title>")
  .replace(/<meta name="description" content="[^"]*" \/>/,
    '<meta name="description" content="Full Metal Racket is a free Windows VST3 analogue drum machine: twelve voices that behave like one kit in one room, with a shared power rail that sags in pitch as well as level, a bleed web so a snare rings the toms, a 32-step sequencer with a per-lane last step for polyrhythms, two hundred generated kits, and a morph fader between any two of them. Nothing sampled, nothing cloned." />')
  //  the panel is cream and amber, so the page is too
  .replace("--accent: #a9651a;", "--accent: #b4600f;")
  .replace("--accent-bright: #c67f28;", "--accent-bright: #d5761c;")
  .replace("--amber: #b4832b;", "--amber: #c08a2e;")
  .replace("--accent: #f0a94a; --accent-bright: #ffc474; --amber: #e8b45a;",
           "--accent: #ffab3d; --accent-bright: #ffc673; --amber: #edb95e;");

// ── the body ──────────────────────────────────────────────────────────────
const BODY = [
'<header class="hero">',
'  <div class="wrap">',
'    <div class="kicker">VST3 Instrument · Windows · Free</div>',
'    <h1>Full Metal <span>Racket</span></h1>',
'    <p class="tagline">Twelve channels that behave like one kit in one room.</p>',
'    <p class="lede">An analogue drum machine with twelve voices, a thirty-two step sequencer and',
'      two hundred kits. Nothing in it is sampled and nothing in it is a clone. What makes it',
'      different is not the voices but what they share: one power rail that sags in pitch as well',
'      as level, one shell they all sit in, and a bleed web so that hitting the snare rings the',
'      toms. It is the glue bus compression is an apology for, built in at the circuit.</p>',
'    <div class="btnrow">',
'      <a class="btn btn-primary" href="Full-Metal-Racket-VST3-win64.zip" download>',
'        Download for Windows</a>',
'      <span class="buildtag">Build 260827.22</span>',
'      <a class="btn btn-ghost" href="Full-Metal-Racket-Manual.pdf">',
'        Read the manual (PDF)</a>',
'    </div>',
'    <figure class="hero-shot" style="margin-top:2.4rem">',
'      <img src="img/hero.jpg" alt="The Full Metal Racket plugin window: a cream steel drum machine with walnut cheeks, twelve channel strips of silver knobs with red pointers, and a master strip on the right">',
'    </figure>',
'  </div>',
'</header>',
'',
'<section class="block">',
'  <div class="wrap">',
'    <h2>One machine, not twelve circuits</h2>',
'    <p class="sub">A drum machine is usually twelve independent synthesisers wired to a mixer. The',
'      parts never quite belong together, and everyone reaches for bus compression to fake the',
'      missing glue. This one is built the other way round.</p>',
'    <div class="cards">',
'      <div class="c"><div class="n">Rail sag</div><h4>One supply, shared</h4>',
'        <p>A hard hit pulls the rail down and everything dips for a few milliseconds — level and',
'        <b>pitch</b>. The pitch part is why sag glues in a way a compressor cannot: it can duck',
'        the level, but it cannot make the whole kit sag flat for an instant and spring back.</p></div>',
'      <div class="c"><div class="n">The web</div><h4>A snare rings the toms</h4>',
'        <p>Any trigger <i>excites</i> the other channels&rsquo; resonators without firing their',
'        envelopes. Hit the kick and the hats buzz. It costs almost nothing, because the',
'        resonators are already there.</p></div>',
'      <div class="c"><div class="n">Kit body</div><h4>The shell they sit in</h4>',
'        <p>One resonator every channel feeds, with its own tuning and decay. Not a reverb — a',
'        coupling. It is what makes twelve voices sound like they are in the same box.</p></div>',
'      <div class="c"><div class="n">Age</div><h4>No two hits alike</h4>',
'        <p>Per-channel component tolerances that drift slowly, so no two instances are identical.',
'        At zero it is exactly absent and the render is bit-identical.</p></div>',
'    </div>',
'    <div class="note"><b>Every one of them is absent at zero.</b> You can switch the glue off',
'      entirely and hear precisely what each one was doing.</div>',
'  </div>',
'</section>',
'',
'<section class="block">',
'  <div class="wrap">',
'    <h2>No VCA. The circuit rings and stops ringing.</h2>',
'    <p class="sub">On the ping models DECAY <i>is</i> the resonator&rsquo;s Q, the way the 808 did',
'      it. There is no amplitude envelope shaping a tone behind your back — which is why the knob',
'      behaves like a physical thing rather than a fader.</p>',
'    <div class="cards">',
'      <div class="c"><div class="n">Tension</div><h4>A hard hit starts sharp</h4>',
'        <p>Amplitude bends the resonator&rsquo;s frequency, the way a struck head tightens. Play',
'        harder and the note starts above where it settles.</p></div>',
'      <div class="c"><div class="n">Nonlinear damping</div><h4>Loud is not quiet turned up</h4>',
'        <p>The damping depends on how hard the thing is moving, so a loud hit has a different',
'        shape as well as a different level.</p></div>',
'      <div class="c"><div class="n">Coupled modes</div><h4>Inharmonic by construction</h4>',
'        <p>The metal voices are built from a six-square core and a multi-band decay, so the top',
'        goes before the body does — the way a real cymbal dies.</p></div>',
'      <div class="c"><div class="n">Rebound</div><h4>A stick that bounces</h4>',
'        <p>One knob per channel turns a single trigger into a modelled bounce with shortening',
'        intervals. Low is a flam, middle a drag, high a buzz roll.</p></div>',
'    </div>',
'  </div>',
'</section>',
'',
'<section class="block">',
'  <div class="wrap">',
'    <h2>A sequencer with a last step per lane</h2>',
'    <p class="sub">Thirty-two steps, sixteen patterns, twelve lanes — and each lane has its own',
'      length, clock division, direction and swing. Set the hats to 16 and the toms to 12 and they',
'      drift against each other for four bars before they agree again.</p>',
'    <figure class="shot">',
'      <img src="img/steps.jpg" alt="The sequencer page: twelve lanes of thirty-two steps, with per-lane last step, division, direction and swing at the right of each row">',
'      <figcaption>The dimmed columns are past that lane&rsquo;s last step. They keep what you drew;',
'        they are simply not in the loop.</figcaption>',
'    </figure>',
'    <div class="cards">',
'      <div class="c"><div class="n">Derived, not counted</div><h4>Exact under the host</h4>',
'        <p>Every step comes from the host&rsquo;s bar position rather than being counted forward,',
'        so looping, relocating and tempo changes are free and a bounce matches what you heard.',
'        Measured worst placement error: one sample.</p></div>',
'      <div class="c"><div class="n">Per step</div><h4>Five things, not one</h4>',
'        <p>Velocity, probability, ratchet, microtiming and a condition — every other pass, every',
'        fourth, on fills, off fills, or only if the last conditional step played.</p></div>',
'      <div class="c"><div class="n">The hand</div><h4>Limbs, not jitter</h4>',
'        <p>Random milliseconds never sound human. A drummer has limbs, and a limb cannot play two',
'        things at once — the second hit displaces, which is where flams come from by themselves.</p></div>',
'      <div class="c"><div class="n">Record</div><h4>Play it in</h4>',
'        <p>Written to the <i>nearest</i> step of that lane, never the one just passed —',
'        quantising backwards makes everything you played sound late.</p></div>',
'    </div>',
'  </div>',
'</section>',
'',
'<section class="block">',
'  <div class="wrap">',
'    <h2>Two hundred kits, and a fader between any two</h2>',
'    <p class="sub">Each kit is generated from its number, so the same number is the same kit on any',
'      machine and always will be. The category is decided first and the parameters are then made to',
'      fit it, which is why a kit filed under TECHNO sounds like techno instead of being labelled',
'      after the fact.</p>',
'    <div class="cards">',
'      <div class="c"><div class="n">Capture A, capture B</div><h4>Morph the whole machine</h4>',
'        <p>Every continuous parameter interpolates and every switched one flips at its own point',
'        along the way, so the kit changes character rather than cross-fading.</p></div>',
'      <div class="c"><div class="n">Key mode</div><h4>A tuned kick that is actually tuned</h4>',
'        <p>MIDI notes track the resonator properly and the decay scales with pitch, so a low note',
'        rings longer, like a real one. Very few drum machines do this correctly.</p></div>',
'      <div class="c"><div class="n">Thirteen outputs</div><h4>Main plus one per channel</h4>',
'        <p>A mono aux for every voice, all disabled until your host asks for them.</p></div>',
'      <div class="c"><div class="n">World FX</div><h4>The rack every Brokild has</h4>',
'        <p>The same effects and the same SPECTRA characters as the rest of the family. The',
'        characters possess the machine from the inside rather than treating the mix.</p></div>',
'    </div>',
'  </div>',
'</section>',
'',
'<section class="block">',
'  <div class="wrap">',
'    <h2>Measured, not asserted</h2>',
'    <p class="sub">Every build is run through an offline bench that renders real audio and measures',
'      it. The numbers below come from that, not from anybody&rsquo;s opinion.</p>',
'    <table class="spec">',
'      <tr><td>Tuning</td><td>Within half a cent; a played octave measures 2.0004</td></tr>',
'      <tr><td>Velocity</td><td>Fifteen decibels of range, top to bottom</td></tr>',
'      <tr><td>Sequencer</td><td>Worst placement error one sample against the host clock</td></tr>',
'      <tr><td>The library</td><td>All two hundred kits bounded, audible, and none identical to another</td></tr>',
'      <tr><td>Cost</td><td>About 6&nbsp;% of a core at 1&times;, 10&nbsp;% at 2&times;, 18&nbsp;% at 4&times;</td></tr>',
'      <tr><td>Checks</td><td>1610 at the time of writing, run after every change to the sound</td></tr>',
'      <tr><td>Build</td><td>260827.22 — shown on the loading screen and in the panel footer</td></tr>',
'      <tr><td>Format</td><td>VST3 and standalone, Windows 64-bit. JUCE 8, native C++ audio.</td></tr>',
'    </table>',
'  </div>',
'</section>',
'',
'<section class="block">',
'  <div class="wrap">',
'    <h2>Getting it running</h2>',
'    <ol class="steps">',
'      <li><b>Download the zip.</b>',
'        <a href="Full-Metal-Racket-VST3-win64.zip" download>Full-Metal-Racket-VST3-win64.zip</a>',
'        (about 8&nbsp;MB). It holds the plugin, the standalone, the manual and a readme.</li>',
'      <li><b>Unzip it.</b> Right-click → <em>Extract All</em>.</li>',
'      <li><b>Copy the folder.</b> Move the whole <code>Full Metal Racket.vst3</code> <em>folder</em>',
'        — not just the file inside it — into <code>C:\\Program Files\\Common Files\\VST3\\</code>.',
'        A sub-folder such as <code>...\\VST3\\Brokild\\</code> is fine; hosts look inside.</li>',
'      <li><b>Rescan.</b> In Ableton Live: Preferences → Plug-Ins → Rescan. It appears as an',
'        instrument called <b>Full Metal Racket</b>, by Brokild.</li>',
'      <li><b>Or just run it.</b> <code>Full Metal Racket.exe</code> is the standalone — no',
'        installation at all.</li>',
'    </ol>',
'    <div class="note"><b>Your patches live in Documents.</b>',
'      <code>Documents\\Brokild patches\\Full Metal Racket</code>, where every Brokild plugin has a',
'      folder of its own — deliberately not beside the installed plugin, which is somewhere an',
'      installer can reach and replace.</div>',
'  </div>',
'</section>',
''].join(NL);

const html = head + BODY + foot;
fs.mkdirSync(R + "vst3-apps/full-metal-racket", { recursive: true });
fs.writeFileSync(OUT, html);
console.log("landing page written: " + (html.length / 1024).toFixed(0) + " KB");
