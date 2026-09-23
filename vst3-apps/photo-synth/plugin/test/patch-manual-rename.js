/*  The manual carries the name too — including its own split heading at the
    cover (`<h1>Photo-Synth <span>2</span></h1>`), which is why this is done
    as markup and then verified by rendering rather than by grepping.
    Also bumps the build id, since the binary changed.  */
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const miss = [];

//  the cover heading, as markup
{
  const P = "C:/Users/peter/b/PhotoSynth/docs/manual/manual.html";
  let s = fs.readFileSync(P, "utf8");
  const A = "<h1>Photo-Synth <span>2</span></h1>";
  const B = "<h1>Photo <span>Synth</span></h1>";
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push("manual heading x" + n); }
  else {
    s = s.split(A).join(B);
    for (const [a, b] of [["Photo-Synth 2", "Photo Synth"],
                          ["Photo-Synth2", "Photo Synth"],
                          ["Photo-Synth", "Photo Synth"]]) s = s.split(a).join(b);
    fs.writeFileSync(P, s);
  }
}

//  the build id: the binary changed, so it is a new build
{
  const P = "C:/Users/peter/b/PhotoSynth/CMakeLists.txt";
  let s = fs.readFileSync(P, "utf8");
  const m = s.match(/set\(PS_BUILD_ID\s+"([^"]+)"\)/);
  if (!m) miss.push("build id not found");
  else {
    s = s.replace(m[0], 'set(PS_BUILD_ID "260830.1")');
    fs.writeFileSync(P, s);
    console.log("build id " + m[1] + " -> 260830.1");
  }
}

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
console.log("manual renamed");
