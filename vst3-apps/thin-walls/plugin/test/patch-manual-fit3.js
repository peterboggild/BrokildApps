const fs = require("fs");
const P = "C:/Users/peter/b/ThinWalls/docs/manual/manual.html";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function once(what, from, to) {
  const n = s.split(from).length - 1;
  if (n !== 1) { miss.push(what + "  (matched " + n + ")"); return; }
  s = s.replace(from, to);
}

once("page 11: the note down to two lines",
`      <b>A room that is not a box needs a different search.</b> The image-source
      method mirrors a source across a box by arithmetic and cannot describe a
      wall that is not parallel to an axis, so a room is now a list of planar
      surfaces and a path a sequence of them, traced back from the listener and
      validated at every step &mdash; while a square room still takes the exact
      arithmetic. A wall broken by <b>two millimetres</b> is the same room, and
      the general search reproduces the exact one to <b>0.09 dB</b> overall and
      <b>0.20 dB</b> over the first 50 ms.`,
`      <b>A room that is not a box needs a different search.</b> A room is now a
      list of planar surfaces and a path a sequence of them, traced back from
      the listener and validated at every step; a square room still takes the
      exact arithmetic. A wall broken by <b>two millimetres</b> is the same
      room, and the general search reproduces the exact one to <b>0.09 dB</b>
      overall and <b>0.20 dB</b> over the first 50 ms.`);

once("page 13: two words fewer",
`          What that field is <b>worth</b>, relative to the direct sound, is not
          a taste decision. For a diffuse field it is fixed by the room's total
          absorption, and the engine calibrates it to exactly that. So the
          direct-to-reverberant ratio &mdash; the single strongest cue your ears`,
`          What that field is <b>worth</b>, relative to the direct sound, is not
          a taste decision. For a diffuse field it is fixed by the room's total
          absorption and the engine calibrates it to exactly that, so the
          direct-to-reverberant ratio &mdash; the strongest cue your ears`);

if (miss.length) {
  console.error("NOTHING WRITTEN. " + miss.length + " anchor(s) did not match:");
  miss.forEach(function (m) { console.error("  - " + m); });
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log("trimmed a third time");
