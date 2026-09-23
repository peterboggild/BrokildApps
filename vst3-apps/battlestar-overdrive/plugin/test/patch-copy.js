/*  Peter's copy pass on the manual.
 *
 *   - open the way it actually happened: he offered, Max asked for this
 *   - the band is a DIY SOLO PERFORMER with synths, sequencers and samplers
 *     and a screen of visuals, NOT a man with a guitar. He has not seen Max
 *     play guitar on stage; the guitar parts are recorded.
 *   - the fuel page says one line. A joke should not be explained.
 *   - nothing about why there is no quality switch: "it sounds defensive,
 *     and is not necessary - it sounds awesome".
 *   - SPACE is FIVE effects, not four (tapeAmt/hallAmt/shimAmt/tremAmt/vibAmt,
 *     each with its own ramp, checked in Engine.cpp).
 */
const fs = require("fs");
const p = "C:/Users/peter/b/BattlestarOverdrive/docs/manual/manual.html";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(find, sub) {
  const f = Array.isArray(find) ? find.join(NL) : find;
  const r = Array.isArray(sub) ? sub.join(NL) : sub;
  const n = s.split(f).length - 1;
  if (n !== 1) { miss.push(n + " matches: " + f.split(NL)[0].trim().slice(0, 54)); return; }
  s = s.replace(f, r);
}

// --- page 2: how it happened, and who the band actually is ------------------
rep([
'        <p>',
'          Max Christensen had the idea, and it was too good to leave alone: an',
'          overdrive pedal named after his band. So here it is, made with his',
'          blessing and built to live in his universe rather than beside it.',
'        </p>',
'        <p>',
'          <b>Battlestar Overdrive</b> is a one-man band out of Copenhagen that',
'          crash-lands somewhere between a late-night discotheque and a sweaty',
'          basement venue: garage rock swagger welded to soul, disco and indie',
'          electronica, played on a guitar and some very determined drums. It is',
'          bonkers, it is hyper-charged, and it is unreasonably optimistic about',
'          it. <em>Go and listen to it.</em> This plugin makes more sense',
'          afterwards, and so, probably, does everything else.',
'        </p>'
], [
'        <p>',
'          I suggested to make a VST3 for Max, and he asked for a Battlestar',
'          Overdrive, named after his solo band. That was too good not to do.',
'          So here it is, built with his blessing.',
'        </p>',
'        <p>',
'          <b>Battlestar Overdrive</b> is Max Christensen, out of Copenhagen, and',
'          it crash-lands somewhere between a late-night discotheque and a sweaty',
'          basement venue: garage rock swagger welded to soul, disco and slacker',
'          electronica. He takes the stage alone, with a mashed-up rig of synths,',
'          sequencers and samplers and a screen of zany visuals behind him. It is',
'          bonkers, it is hyper-charged, and it is unreasonably optimistic about',
'          it. <em>Go and listen to it.</em> This plugin makes more sense',
'          afterwards, and so, probably, does everything else.',
'        </p>'
]);

// --- page 2: the box was a list of things it does NOT have ------------------
rep([
'          <h4>IN SHORT</h4>',
'          <p style="font-size:9.4pt">',
'            Six knobs, one button, one screen. No quality switch, no',
'            oversampling menu, no output trim &mdash; the plugin decides those',
'            for you, because the metal has no room for them and you have',
'            better things to do.',
'          </p>'
], [
'          <h4>IN SHORT</h4>',
'          <p style="font-size:9.4pt">',
'            Six knobs, one button, one screen. Everything is on the front and',
'            everything does something. Turn it up and find out which.',
'          </p>'
]);

// --- page 4: the defensive box goes ---------------------------------------
rep([
'        <div class="box" style="margin-top:8mm">',
'          <h4>WHY THERE IS NO QUALITY SWITCH</h4>',
'          <p style="font-size:9.4pt">',
'            It runs at <b>4&times; oversampling all the time</b>. Worst-case',
'            aliasing across every engine measures &minus;99.8 dB, and the',
'            oversampler itself is flat to &plusmn;0.002 dB out to 19 kHz. That',
'            costs about 2.5&nbsp;% of one core, which is not worth a menu.',
'          </p>',
'        </div>'
], [
'        <div class="box" style="margin-top:8mm">',
'          <h4>PARALLEL IS WHERE IT LIVES</h4>',
'          <p style="font-size:9.4pt">',
'            It is usually more useful to wreck the signal completely and blend',
'            it back than to half-wreck it. Drive hard, then ride MIX.',
'          </p>',
'        </div>'
]);

// --- page 7: SPACE is five ---------------------------------------------------
rep('    <p class="sub">A journey rather than a menu. The effects overlap, so there' + NL +
    '    is no point where one stops and the next begins.</p>',
    '    <p class="sub">Five effects on one knob, overlapping, so there is no point' + NL +
    '    where one stops and the next begins. Too much is never enough.</p>');

rep('          <tr><td>Top</td><td><b>Harmonic tremolo</b> and vibrato take over, and it stops pretending to be a room at all.</td></tr>',
    '          <tr><td>Late</td><td><b>Harmonic tremolo.</b> The band splits at 800 Hz and the halves sway against each other in opposite phase. It stops pretending to be a room at all.</td></tr>' + NL +
    '          <tr><td>Top</td><td><b>Vibrato,</b> true pitch, arriving last and on top of everything already happening.</td></tr>');

// --- page 8: one line, and no explaining ------------------------------------
rep([
'    <p class="kick" style="color:var(--red)">The joke, and it is load-bearing</p>',
'    <h2 class="display">FUEL</h2>',
'    <hr class="rule" />'
], [
'    <p class="kick" style="color:var(--red)">A word of advice</p>',
'    <h2 class="display">FUEL</h2>',
'    <hr class="rule" />'
]);

// --- page 12: the back page ---------------------------------------------------
rep([
'          <b>Battlestar Overdrive</b> is Max Christensen, out of Copenhagen.',
'          Garage rock swagger welded to soul, disco and indie electronica,',
'          played by one person with a guitar, some very determined drums and no',
'          interest whatsoever in tidying it up.'
], [
'          <b>Battlestar Overdrive</b> is Max Christensen, out of Copenhagen.',
'          Garage rock swagger welded to soul, disco and slacker electronica,',
'          performed alone on a mashed-up rig of synths, sequencers and samplers,',
'          with a screen of zany visuals behind him.'
]);

if (miss.length) { console.error("ABORTED - nothing written:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(p, s);
console.log("manual copy updated");
