/*  Prove the rename did NOT change the plugin's identity.

    JUCE builds a VST3 class id from the MANUFACTURER code and the PLUGIN code
    and the plugin type — never from the display name. So renaming the product
    must leave the id byte-for-byte identical, and every existing project keeps
    working while showing the new name. This is the check that made the Mars
    Wars -> Martian Gain rename safe; it is measured, not assumed.

    The id appears in the binary as the two four-character codes in sequence,
    so we find that pattern and print the 16 bytes around it from each DLL.  */
"use strict";
const fs = require("fs");

const A = process.argv[2];        // old binary
const B = process.argv[3];        // new binary
const MANU = process.argv[4] || "Brkd";
const CODE = process.argv[5] || "Psy2";

function ids(file) {
  const buf = fs.readFileSync(file);
  const needle = Buffer.from(MANU + CODE, "ascii");
  const out = [];
  let i = 0;
  while ((i = buf.indexOf(needle, i)) !== -1) {
    //  JUCE's id is 16 bytes; the codes sit in the second half
    const start = Math.max(0, i - 8);
    out.push(buf.slice(start, start + 16).toString("hex").toUpperCase()
               .replace(/(.{8})/g, "$1 ").trim());
    i += needle.length;
  }
  return out;
}

const a = ids(A), b = ids(B);
console.log("old (" + A.split(/[\\/]/).pop() + "):");
a.forEach(x => console.log("   " + x));
console.log("new (" + B.split(/[\\/]/).pop() + "):");
b.forEach(x => console.log("   " + x));

const same = a.length > 0 && a.length === b.length && a.every((x, i) => x === b[i]);
console.log(same
  ? "\nIDENTICAL — the rename is invisible to every project that already uses it"
  : "\n*** THE CLASS ID CHANGED — existing projects would lose the plugin ***");
process.exit(same ? 0 : 1);
