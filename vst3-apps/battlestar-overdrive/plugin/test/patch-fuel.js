/*  The fuel page, down to one line.
 *
 *  Peter: "Keep Fuel, but make it much shorter. Just say: Watch the fuel - you
 *  dont want to run out, thrust me. (a joke should not be explained)".
 *
 *  So the page states it and shows it. The three plates and the tube are the
 *  whole explanation, and none of them is captioned with the mechanism.
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

rep([
'      <div class="c-55">',
'        <p>',
'          The tank goes down <b>one notch for every five seconds you actually',
'          play</b>. Silence is free. It is not metering anything and it is not',
'          modelling a power supply: it is there because the panel has a fuel',
'          gauge on it, and a gauge that never moves is a lie.',
'        </p>',
'',
'        <h3 class="plain" style="font-size:12pt; margin-top:7mm">WITH AUTOREFILL LIT</h3>',
'        <p style="margin-top:2.5mm">',
'          The default, and the sane one. The tank tops itself up at the notch',
'          above empty, so it never runs dry and nothing ever happens to your',
'          sound. The level just drifts, which is all it is asked to do.',
'        </p>',
'',
'        <h3 class="plain" style="font-size:12pt; margin-top:6mm; color:var(--red)">WITH AUTOREFILL DARK</h3>',
'        <p style="margin-top:2.5mm">',
'          Then it means it. Roughly <b>71 seconds</b> of continuous playing and',
'          the tank is empty; the engine <b>flames out over the following five',
'          seconds</b> while the tube blinks <span class="says">FUEL',
'          EMPTY</span> in red, and then you have a very quiet plugin.',
'        </p>',
'        <p>',
'          Press <b>Autorefill</b>. That is the whole recovery procedure.',
'        </p>',
'',
'        <div class="box" style="margin-top:6mm; border-color:rgba(212,41,28,.4)">',
'          <h4 style="color:var(--red)">BEFORE YOU BOUNCE</h4>',
'          <p style="font-size:9.4pt">',
'            Leave Autorefill lit for anything you intend to keep, unless running',
'            out is the take. It is automatable, so the flame-out can be an',
'            arrangement decision rather than an accident.',
'          </p>',
'        </div>',
'      </div>'
], [
'      <div class="c-55">',
'        <p class="pull" style="font-size:22pt; margin-top:6mm">WATCH THE FUEL.</p>',
'        <p style="font-size:13pt; color:#efe4cf; margin-top:7mm; max-width:118mm">',
'          You don&rsquo;t want to run out. Thrust me.',
'        </p>',
'        <p style="margin-top:12mm; color:var(--dim); font-size:9.4pt; max-width:110mm">',
'          The <b style="color:var(--yellow)">Autorefill</b> button is lit by',
'          default, and while it is lit there is nothing to think about.',
'        </p>',
'      </div>'
]);

if (miss.length) { console.error("ABORTED - nothing written:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(p, s);
console.log("fuel page cut to one line");
