/*  Regenerate the UI fragment's DEFAULT_DESC / DEFAULT_CDESC / DEFAULT_PRESETS
    blocks from the C++ registry.

    These snapshots only serve the standalone mockup — the native side sends
    the real descriptors — but a stale snapshot means the mockup shows knobs
    the plugin does not have, which is worse than no mockup at all. So: never
    hand-type them. This script lives in the repo (the last one lived in a
    scratchpad and was lost with it).

        node test/regen-desc.js <path-to-bwfxtest.exe>

    It rewrites the three `var X = [ ... ];` blocks in place, matching the
    surrounding indentation, and refuses to write anything if a dump fails.
*/
"use strict";
const fs = require("fs");
const { execFileSync } = require("child_process");

const EXE = process.argv[2] || "test/build/Release/bwfxtest.exe";
const P = "C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js";

function dump(flag) {
  const out = execFileSync(EXE, [flag], { encoding: "utf8", maxBuffer: 1 << 24 });
  const t = out.trim();
  if (!t.startsWith("[")) throw new Error(flag + ": not a JSON array");
  return JSON.parse(t);
}

/*  Render the way the file already reads: one object per module, its params
    one per line. Machine-written but meant to be read in a diff. */
function render(rows, indent) {
  const I = " ".repeat(indent);
  const body = rows.map(function (m) {
    const head = Object.keys(m).filter(function (k) { return k !== "params"; })
      .map(function (k) { return k + ": " + JSON.stringify(m[k]); }).join(", ");
    const ps = (m.params || []).map(function (p) {
      return I + "  { " + Object.keys(p).map(function (k) {
        return k + ": " + JSON.stringify(p[k]);
      }).join(", ") + " }";
    }).join("," + "\n");
    if (!ps) return I + "{ " + head + ", params: [] }";
    return I + "{ " + head + ", params: [\n" + ps + "\n" + I + "] }";
  }).join("," + "\n");
  return "[\n" + body + "\n" + " ".repeat(indent - 2) + "]";
}

const BLOCKS = [
  { name: "DEFAULT_DESC",    flag: "--desc" },
  { name: "DEFAULT_CDESC",   flag: "--cdesc" },
  { name: "DEFAULT_PRESETS", flag: "--presets" }
];

let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const LF = String.fromCharCode(10);

for (const b of BLOCKS) {
  const rows = dump(b.flag);
  const marker = "var " + b.name + " = [";
  const at = s.indexOf(marker);
  if (at < 0) { console.error("ABORT: no " + b.name); process.exit(1); }
  //  the block ends at the first "];" that starts a line
  const endTok = NL + "  ];";
  const end = s.indexOf(endTok, at);
  if (end < 0) { console.error("ABORT: unterminated " + b.name); process.exit(1); }
  const text = render(rows, 4).split(LF).join(NL);
  s = s.slice(0, at) + "var " + b.name + " = " + text + ";" + s.slice(end + endTok.length);
  console.log("  " + b.name + ": " + rows.length + " entries");
}

fs.writeFileSync(P, s);
console.log("bwfx-rack.js snapshots regenerated from the C++ registry");
