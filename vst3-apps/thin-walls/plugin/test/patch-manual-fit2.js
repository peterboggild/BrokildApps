const fs = require("fs");
const P = "C:/Users/peter/b/ThinWalls/docs/manual/manual.html";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function once(what, from, to) {
  const n = s.split(from).length - 1;
  if (n !== 1) { miss.push(what + "  (matched " + n + ")"); return; }
  s = s.replace(from, to);
}

/* --- page 11: the box becomes a two-line note under the columns --------- */
once("page 11: the closing box becomes a note",
`    <div class="box" style="margin-top:1mm">
      <h4>A room that is not a box needs a different search</h4>
      <p>
        The image-source method mirrors a source across a box by arithmetic and
        cannot describe a wall that is not parallel to an axis. So a room is now
        a <b>list of planar surfaces</b> and a path a sequence of them, traced
        back from the listener and validated at every step; a square room still
        takes the exact arithmetic. The check that matters: a wall broken by
        <b>two millimetres</b> is the same room, and the general search
        reproduces the exact one to <b>0.09 dB</b> overall and <b>0.20 dB</b>
        over the first 50 ms.
      </p>
    </div>`,
`    <p style="margin:0; font-size:9.3pt">
      <b>A room that is not a box needs a different search.</b> The image-source
      method mirrors a source across a box by arithmetic and cannot describe a
      wall that is not parallel to an axis, so a room is now a list of planar
      surfaces and a path a sequence of them, traced back from the listener and
      validated at every step &mdash; while a square room still takes the exact
      arithmetic. A wall broken by <b>two millimetres</b> is the same room, and
      the general search reproduces the exact one to <b>0.09 dB</b> overall and
      <b>0.20 dB</b> over the first 50 ms.
    </p>`);

/* --- page 13: one sentence out of the double slope --------------------- */
once("page 13: trim the double slope",
`          The decay falls off a cliff and then keeps going &mdash;
          which is what a small dead room inside a live building actually does,
          and what a single reverb cannot do at all.
        </p>
        <p style="font-size:9.6pt">
          Shut the door and that second slope goes with it. Open it and the
          little room grows.
        </p>`,
`          The decay falls off a cliff and then keeps going &mdash; which is
          what a small dead room inside a live building does, and what a single
          reverb cannot do at all. Shut the door and that second slope goes
          with it.
        </p>`);

/* --- page 16: back to eleven rows on the left -------------------------- */
once("page 16: fold four rows into two",
`          <tr><td>Level with distance</td><td>6.1 dB per doubling</td></tr>
          <tr><td>Near field</td><td>the interaural level difference grows 4.6 dB by 0.3 m, out of the geometry</td></tr>`,
`          <tr><td>Level with distance</td><td>6.1 dB per doubling &mdash; and the interaural level difference grows 4.6 dB by 0.3 m, out of the geometry</td></tr>`);

once("page 16: reverberation time absorbs the roughness row",
`          <tr><td>Reverberation time</td><td>against Eyring in all fifteen room and material cases</td></tr>
          <tr><td>Spectral roughness of the tail</td><td>5.6 to 6.6 dB, against the 5.57 dB of a perfectly diffuse field</td></tr>`,
`          <tr><td>Reverberation time</td><td>against Eyring in all fifteen room and material cases; the tail's spectral roughness 5.6 to 6.6 dB, against the 5.57 dB of a perfectly diffuse field</td></tr>`);

once("page 16: the studio and the broken wall have their own pages",
`          <tr><td>A studio's decay</td><td>flat to 1.00&times; across 250 Hz, 1 kHz and 4 kHz in the hall, where TILED spreads 3.09&times;</td></tr>
          <tr><td>A wall broken by 2 mm</td><td>the general path search reproduces the exact shoebox to 0.09 dB</td></tr>
`, "");

if (miss.length) {
  console.error("NOTHING WRITTEN. " + miss.length + " anchor(s) did not match:");
  miss.forEach(function (m) { console.error("  - " + m); });
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log("trimmed again");
