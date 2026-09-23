const fs = require("fs");
const P = "C:/Users/peter/b/ThinWalls/docs/manual/manual.html";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function once(what, from, to) {
  const n = s.split(from).length - 1;
  if (n !== 1) { miss.push(what + "  (matched " + n + ")"); return; }
  s = s.replace(from, to);
}

/*  Page 11: cap both plates lower. The class cap is 58mm for a page that
    carries two plates AND three control entries AND a closing note; this one
    carries all of that plus a four-line subhead, so it needs its own. */
once("page 11: the plan plate",
`        <img class="shot" src="img/plan-folded.jpg" alt="" />`,
`        <img class="shot" src="img/plan-folded.jpg" alt="" style="max-height:49mm" />`);
once("page 11: the fader plate",
`        <img class="shot" src="img/ctrl-folds.jpg" alt="" />`,
`        <img class="shot" src="img/ctrl-folds.jpg" alt="" style="max-height:49mm" />`);

/*  Page 13: one whole sentence out of the left column. */
once("page 13: drop a sentence from the trims paragraph",
`          That is why there is no &ldquo;reverb amount&rdquo;. Turning one up
          would mean lying about how far away you were standing. What the panel
          gives you instead is three trims &mdash; DIRECT, EARLY and REVERB
          &mdash; each defaulting to 0 dB, for when you want the lie on purpose.`,
`          That is why there is no &ldquo;reverb amount&rdquo;: turning one up
          would mean lying about how far away you were standing. What you get
          instead is three trims &mdash; DIRECT, EARLY and REVERB &mdash; each
          at 0 dB, for when you want the lie on purpose.`);
once("page 13: drop a clause from the room-per-room line",
`          A flat does not have one reverberation time, it has as many as it has
          rooms, and you hear the ones next door through the doors. Stand in the`,
`          A flat does not have one reverberation time, it has as many as it has
          rooms. Stand in the`);

if (miss.length) {
  console.error("NOTHING WRITTEN. " + miss.length + " anchor(s) did not match:");
  miss.forEach(function (m) { console.error("  - " + m); });
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log("trimmed a fourth time");
