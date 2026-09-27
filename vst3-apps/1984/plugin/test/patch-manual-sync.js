"use strict";
const fs = require("fs");
const p = "C:/Users/peter/b/Nineteen84/docs/manual/manual.html";
const raw = fs.readFileSync(p, "utf8");
const crlf = raw.indexOf("\r\n") >= 0;
let s = raw.replace(/\r\n/g, "\n");
let n = 0;
function edit(from, to) { const parts = s.split(from); if (parts.length !== 2) { console.error("ANCHOR MISS (" + (parts.length - 1) + "): " + from.slice(0, 120)); process.exit(1); } s = parts.join(to); n++; }

edit(`      <!-- HARD SYNC: rank I's wrap resets rank II's phase -->
      <path class="dg-w mod" d="M 205 92 L 205 208" />
      <polygon class="dg-head mod" points="205,208 200,199 210,199" />`,
`      <!-- HARD SYNC: rank II's wrap resets rank I's phase (rank I is the slave) -->
      <path class="dg-w mod" d="M 205 208 L 205 92" />
      <polygon class="dg-head mod" points="205,92 200,101 210,101" />`);
edit(`        Rank I's wrap resets rank II's phase when <em>SYNC</em> is on. Rank
        II's output, taken <em>before</em> its filter, can bend rank I's pitch,`,
`        Rank II's wrap resets rank I's phase when <em>SYNC</em> is on: the
        audible rank is the slave, the way the Prophet does it. Rank
        II's output, taken <em>before</em> its filter, can bend rank I's pitch,`);
edit(`            <em>II SYNC TO I</em> resets rank II's phase every time rank I
            wraps, band-limited at the actual jump height &mdash; so a synced
            rank is a hard edge and not a burst of aliasing.`,
`            <em>SYNC I TO II</em> resets rank I's phase every time rank II
            wraps, band-limited at the actual jump height &mdash; so a synced
            rank is a hard edge and not a burst of aliasing. Rank II is the
            master because the envelope's pitch route lands on rank I: turn
            rank II's level down, and the envelope sweeps the synced, audible
            rank over a note the silent master holds.`);
edit(`<tr><td>Sync Lead</td><td>MONO with glide. Rank II is synced and seven semitones up with its level at zero, and the filter envelope bends rank I's pitch. A little VALVE.</td></tr>`,
`<tr><td>Sync Lead</td><td>MONO with glide. Rank I is synced to a silent rank II and starts seven semitones up; the filter envelope sweeps it. A little VALVE.</td></tr>`);
edit(`<tr><td>Sync</td><td>Rank II hard-synced to rank I, band-limited at the actual jump height</td></tr>`,
`<tr><td>Sync</td><td>Rank I hard-synced to rank II, band-limited at the actual jump height</td></tr>`);
fs.writeFileSync(p, crlf ? s.replace(/\n/g, "\r\n") : s);
console.log("manual: " + n + " edits");
