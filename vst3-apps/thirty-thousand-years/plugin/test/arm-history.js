const BROKILD_ROOT = require("path").resolve(__dirname, "..").replace(/\\/g, "/");
/*  Arm HISTORY on every preset that ships scenes.

    Eleven of the forty-eight store four scenes each - a whole journey written
    into the patch - and not one of them set `h_on`, which defaults to OFF and
    is F_NS so scene interpolation can never set it either. So the slider moved
    and nothing happened, on every factory patch, which is exactly what Peter
    reported. The mechanism was never broken: measured live on preset 0, the
    feedback send interpolates 0.0000 / 0.1189 / 0.2491 / 0.1500 across the
    four scenes, which are their stored values exactly.

    A preset that stores no scenes is left alone: there is nothing to travel
    between, and arming it would be a switch that lies.
*/
const fs = require ("fs");
const P = "" + BROKILD_ROOT + "/Source/Presets.cpp";
let s = fs.readFileSync (P, "utf8");

/*  An entry is { "NAME", "CATEGORY", "description", "body..." }, so the body
    opens at the 7th unescaped quote after the brace - the same rule the trim
    pass used, for the same reason: a body split over several adjacent string
    literals cannot be found any other way. */
function bodyQuote (str, from) {
  let n = 0;
  for (let i = from; i < str.length; ++i)
    if (str[i] === '"' && str[i - 1] !== "\\" && ++n === 7) return i;
  return -1;
}
function entryEnd (str, from) {
  const next = str.indexOf ('\n{ "', from + 1);
  return next < 0 ? str.length : next;
}

const plan = [];
let miss = [], scanned = 0, noScenes = 0;
for (let at = s.indexOf ('{ "'); at >= 0; at = s.indexOf ('\n{ "', at + 1) >= 0 ? s.indexOf ('\n{ "', at + 1) + 1 : -1) {
  const end = entryEnd (s, at);
  const entry = s.slice (at, end);
  const name = (entry.match (/^\{ "([^"]*)"/) || [])[1];
  if (!name) continue;
  scanned++;
  if (entry.indexOf ("#SCENE") < 0) { noScenes++; continue; }
  if (entry.indexOf ("h_on=") >= 0) { miss.push ("already armed: " + name); continue; }
  const q = bodyQuote (s, at);
  if (q < 0 || q >= end) { miss.push ("no body: " + name); continue; }
  plan.push ({ q, name });
}
if (miss.length) { console.log ("ABORTED, nothing written:\n" + miss.join ("\n")); process.exit (1); }

plan.sort ((a, b) => b.q - a.q);           // from the end, so earlier offsets stay valid
for (const p of plan) s = s.slice (0, p.q + 1) + "h_on=1 " + s.slice (p.q + 1);
fs.writeFileSync (P, s);

console.log ("presets scanned: " + scanned + "   armed: " + plan.length + "   no scenes, left off: " + noScenes);
plan.reverse().forEach (p => console.log ("   " + p.name));
