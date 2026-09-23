const BROKILD_ROOT = require("path").resolve(__dirname, "..").replace(/\\/g, "/");
/*  Write the measured PATCH TRIM into every preset that needs one.

    The trims come from `ttyprobe levels`, which renders each patch at its
    loudest scene with the limiter and the safety clamp bypassed and reports
    the attenuation (or lift) that lands its loudest 2 s on the bank's target.
    Nothing here is typed by hand: the number in the source is the number that
    was measured.

    Presets inside +-1 dB of target are left alone, so the diff is the set of
    patches that actually needed moving and every other one keeps the exact
    0 dB default.
*/
const fs = require("fs");
const P = "" + BROKILD_ROOT + "/Source/Presets.cpp";
const L = "" + BROKILD_ROOT + "/test/levels4.txt";

const rows = fs.readFileSync(L, "utf8").split(/\r?\n/).slice(1).filter(l => l.trim());
const want = [];
for (const line of rows) {
  const f = line.trim().split(/\s+/);
  if (f.length < 6) continue;
  want.push({ name: f.slice(0, f.length - 5).join(" "),
              db: parseFloat(f[f.length - 2]),
              trim: parseFloat(f[f.length - 1]) });
}
if (want.length !== 48) { console.log("expected 48 rows, parsed " + want.length); process.exit(1); }

/*  An entry is { "NAME", "CATEGORY", "description", "body..." }, so the body
    opens at the 7th unescaped quote after the brace. Counting quotes is the
    only way that survives a body split over several adjacent literals. */
function bodyQuote (s, from) {
  let n = 0;
  for (let i = from; i < s.length; ++i)
    if (s[i] === '"' && s[i - 1] !== "\\" && ++n === 7) return i;
  return -1;
}

let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const plan = [];
let skipped = 0, miss = [];

for (const w of want) {
  if (Math.abs (w.db) < 1.0) { skipped++; continue; }
  const head = '{ "' + w.name + '", "';
  const at = s.indexOf (head);
  if (at < 0) { miss.push ("no entry: " + w.name); continue; }
  if (s.indexOf (head, at + 1) >= 0) { miss.push ("two entries named: " + w.name); continue; }
  const q = bodyQuote (s, at);
  if (q < 0) { miss.push ("no body: " + w.name); continue; }
  if (s.slice (q, q + 60).indexOf ("outtrim=") >= 0) { miss.push ("already trimmed: " + w.name); continue; }
  plan.push ({ q, text: "outtrim=" + w.trim.toFixed (4) + " ", name: w.name, db: w.db });
}
if (miss.length) { console.log ("ABORTED, nothing written:" + NL + miss.join (NL)); process.exit (1); }

// Insert from the end so every earlier offset stays valid.
plan.sort ((a, b) => b.q - a.q);
for (const p of plan) s = s.slice (0, p.q + 1) + p.text + s.slice (p.q + 1);
fs.writeFileSync (P, s);

const n = (s.match (/outtrim=/g) || []).length;
console.log ("trims written: " + plan.length + "   left at exactly 0 dB: " + skipped + "   outtrim= in file: " + n);
