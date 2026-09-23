/*  A backtick inside a JS template literal ENDS it - so the code span I
    spliced into the replacement body broke patch-design-doc.js itself. The
    body carries no backtick now; the file name is set in plain prose.       */
const fs = require("fs");
const p = "C:/Users/peter/b/ThinWalls/test/patch-design-doc.js";
let s = fs.readFileSync(p, "utf8");
const BT = String.fromCharCode(96);

const i = s.indexOf('once("the bench paragraph gains the new sections",');
const j = s.indexOf('once("the open items",');
if (i < 0 || j < 0) { console.error("MISS"); process.exit(1); }

const body = [
  "rooms; four sources bounded at 40 % of a core.",
  "",
  "Added 2026-09-22: **section 8, the studio** \u2014 the flat decay per room with its spread",
  "against TILED's, the hall still live at 0.71 s, the 17.6 % specular share, the",
  "1.1-against-2.4 dB early response, and *four checks that the other four materials scatter",
  "and splay nothing, so they sound exactly as they did*. **Section 9, broken walls** \u2014 the",
  "2 mm equivalence against the exact shoebox, a 0.6 m push lengthening the decay, the",
  "evenness of a centred source flat against broken, pushed-out dispersing (30 paths) against",
  "pushed-in shadowing itself (28), every room and every fold from \u22120.6 to +0.6 m finite and",
  "bounded (worst peak 1.032), and a wall moved under a sounding note keeping everything above",
  "6 kHz 92 dB down.",
  "",
  "The panel probe, test/uiprobe.js, drives the page in headless Chrome: **73 checks**,",
  "including that W and the arrows walk while a menu or a fader has the focus and the control",
  "never sees the key, that a key typed into a text field is still the field's, that every",
  "door offers its handle from its own doorway, that the handle under the pointer is drawn",
  "differently from one that is not, that the same handle out of reach is not hovered, that",
  "dragging it swings the door inside one host gesture and is not also a look, and that a",
  "choice list which grows to six renders six and one that shrinks takes the selector back",
  "with it."
].join("\n");

if (body.indexOf(BT) >= 0) { console.error("the body still carries a backtick"); process.exit(1); }

const replacement =
  'once("the bench paragraph gains the new sections",\n' +
  BT + "rooms; four sources bounded at 28 % of a core." + BT + ",\n" +
  BT + body + BT + ");\n\n";

s = s.slice(0, i) + replacement + s.slice(j);
fs.writeFileSync(p, s);
console.log("anchor shortened, no backtick in the body");
