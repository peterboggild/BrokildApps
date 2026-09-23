/*  The overflow gate found four pages that no longer fit their own sheet, and
    a .page is a FIXED sheet with overflow:hidden - too tall is not a new page,
    it is silently cut off. Trim, then re-run the gate.

      page  3  the seven-group table sat on the folio strip
      page 11  +65 px  (the closing box)
      page 13  +38 px  (the new diffuser paragraph)
      page 16  +4 px   (two added specification rows)                        */

const fs = require("fs");
const P = "C:/Users/peter/b/ThinWalls/docs/manual/manual.html";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function once(what, from, to) {
  const n = s.split(from).length - 1;
  if (n !== 1) { miss.push(what + "  (matched " + n + ")"); return; }
  s = s.replace(from, to);
}

/* --- page 3: give the third row its room by capping the panel plate ------
   .wide .shot is capped by WIDTH because the panel plate fills its column;
   here the column is taller than the page can afford, so this one is capped
   by height and centred instead. */
once("page 3: cap the panel plate by height",
`        <figure class="wide">
          <img class="shot" src="img/panel.jpg" alt="" />`,
`        <figure class="wide">
          <img class="shot" src="img/panel.jpg" alt="" style="max-height:84mm; width:auto; margin:0 auto" />`);

/* --- page 11: the closing box was a page too long ----------------------- */
once("page 11: shorten the closing box",
`      <p>
        The image-source method mirrors a source across a box by arithmetic,
        and it simply cannot describe a wall that is not parallel to an axis.
        So a room is now a <b>list of planar surfaces</b> and a path is a
        sequence of them, traced back from the listener and validated at every
        step. A square room still takes the exact arithmetic. The check that
        matters: a wall broken by <b>two millimetres</b> is geometrically the
        same room, and the general search reproduces the exact one to within
        <b>0.09 dB</b> overall and <b>0.20 dB</b> over the first 50 ms. And
        pushing a wall 0.6 m out lengthens that room's own decay from
        <b>1.766 s</b> to <b>1.801 s</b> &mdash; the room really did get
        bigger.
      </p>`,
`      <p>
        The image-source method mirrors a source across a box by arithmetic and
        cannot describe a wall that is not parallel to an axis. So a room is now
        a <b>list of planar surfaces</b> and a path a sequence of them, traced
        back from the listener and validated at every step; a square room still
        takes the exact arithmetic. The check that matters: a wall broken by
        <b>two millimetres</b> is the same room, and the general search
        reproduces the exact one to <b>0.09 dB</b> overall and <b>0.20 dB</b>
        over the first 50 ms.
      </p>`);

once("page 11: shorten the two figcaptions",
`        <figcaption><b>Four walls broken.</b> The living room's west and south
          walls and the hall's east and north are splayed outward; the diamond
          on each is the break, draggable along the wall and out of it. The
          room's area, its volume and its reverberation time all follow
          &mdash; the hall's floor goes from 108.0 to <b>113.7 m&sup2;</b>.</figcaption>`,
`        <figcaption><b>Four walls broken.</b> The diamond on each is the break:
          drag it along the wall, or out of it. Area, volume and reverberation
          time all follow &mdash; the hall's floor goes from 108.0 to
          <b>113.7 m&sup2;</b>, and its own decay from 1.766 s to
          <b>1.801 s</b>.</figcaption>`);

once("page 11: trim the fader caption",
`        <figcaption><b>Two faders per wall.</b> The first is how far the break
          is pushed, &plusmn;0.6 m, reading out in metres and saying
          <em>flat</em> at the centre. The second is where along the wall it
          sits. The two walls offered are always the ones with no doorway in
          them, so a break can never collide with a door.</figcaption>`,
`        <figcaption><b>Two faders per wall.</b> How far the break is pushed
          (&plusmn;0.6 m, <em>flat</em> at the centre) and where along the wall
          it sits. The two walls offered are always the ones with no doorway,
          so a break can never collide with a door.</figcaption>`);

/* --- page 13: the diffuser paragraph ----------------------------------- */
once("page 13: shorten the diffuser paragraph",
`        <p>
          Inside every one of those lines sits a chain of <b>allpass
          diffusers</b>. An allpass has unity gain at every frequency, so it
          cannot change the decay at all &mdash; but its length counts towards
          the loop, which is what decides whether the network's own resonances
          are close enough together to be heard as a room rather than as
          pitched ringing, and it splits each echo into a train, which is what
          decides whether the tail has grain in it. A perfectly diffuse field
          has a fixed spectral roughness of <b>5.57 dB</b>; this one measures
          <b>5.6 to 6.6 dB</b> where before the diffusers it measured
          <b>6.1 to 10.7</b>. Every one of the fifteen room-and-material
          decays now lands within a few per cent of Eyring.`,
`        <p>
          Inside every one of those lines sits a chain of <b>allpass
          diffusers</b>. An allpass has unity gain at every frequency, so it
          cannot change the decay &mdash; but its length counts towards the
          loop, which decides whether the network's resonances are close
          enough together to hear as a room rather than as ringing, and it
          splits each echo into a train, which decides whether the tail has
          grain. A perfectly diffuse field has a spectral roughness of
          <b>5.57 dB</b>; this one measures <b>5.6 to 6.6</b>, where before
          the diffusers it measured <b>6.1 to 10.7</b>.`);

/* --- page 16: two rows became one -------------------------------------- */
once("page 16: merge the two new specification rows",
`          <tr><td>Materials</td><td>Five, and one for each of the three surfaces of each room &mdash; nine choices</td></tr>
          <tr><td>Room shape</td><td>Two walls of every room can be broken at a movable point and pushed &plusmn;0.6 m</td></tr>`,
`          <tr><td>Materials</td><td>Five, chosen per surface &mdash; walls, floor and ceiling of each room, nine choices. Two walls of every room can be broken and pushed &plusmn;0.6 m</td></tr>`);

if (miss.length) {
  console.error("NOTHING WRITTEN. " + miss.length + " anchor(s) did not match:");
  miss.forEach(function (m) { console.error("  - " + m); });
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log("trimmed");
