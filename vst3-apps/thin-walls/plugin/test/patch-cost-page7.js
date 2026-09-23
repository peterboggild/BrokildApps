/*  The sweep for stale figures found the cost quoted a second time, on page 7,
    where a grep for "47" and "67 checks" would never have looked. Measure the
    RESULT of a find-and-replace pass; do not re-read the input.             */
const fs = require("fs");
const P = "C:/Users/peter/b/ThinWalls/docs/manual/manual.html";
let s = fs.readFileSync(P, "utf8");
const from =
`            One source costs about <b>11 %</b> of a core at 48 kHz. Four, with
            every door open, cost <b>28 %</b>. Switching a source to OFF gives
            its share straight back.`;
const to =
`            One source costs about <b>17 %</b> of a core at 48 kHz. Four, with
            every door open, cost <b>40 %</b>. Switching a source to OFF gives
            its share straight back.`;
const n = s.split(from).length - 1;
if (n !== 1) { console.error("ANCHOR MISS (matched " + n + ")"); process.exit(1); }
fs.writeFileSync(P, s.replace(from, to));
console.log("page 7 cost corrected");
