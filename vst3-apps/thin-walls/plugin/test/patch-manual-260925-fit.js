/*  Second pass for 260925.3: the three sheets the overflow and folio gates
 *  flagged (behind a door, the test signal, the specifications). */
const fs = require('fs');
const F = 'C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/thin-walls/plugin/docs/manual/manual.html';
let s = fs.readFileSync(F, 'utf8');
const CRLF = s.indexOf('\r\n') >= 0;
if (CRLF) s = s.replace(/\r\n/g, '\n');
const miss = [];
function rep(a, b) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push((n ? n + 'x ' : 'MISSING ') + JSON.stringify(a.slice(0, 70))); return; }
  s = s.replace(a, () => b);
}

/* behind a door: smaller plates, a tighter paragraph */
rep(`        <img class="shot" src="img/plan-open.jpg" alt="" />`,
    `        <img class="shot" src="img/plan-open.jpg" alt="" style="max-height:49mm" />`);
rep(`        <img class="shot" src="img/plan-shut.jpg" alt="" />`,
    `        <img class="shot" src="img/plan-shut.jpg" alt="" style="max-height:49mm" />`);
rep(`      crossfade a shut sound into an open one. <b style="color:var(--cool)">Walking
      through</b> an open door is continuous too. In the opening you can see
      almost everything in the room behind you, so the doorway model carries
      that room's reflections to second order with the same keys as the in-room
      model, and for 0.4 m either side of the plane both rooms' fields are
      heard and handed over. Walking into the source's room holds about
      &minus;24.5 dB through the plane where it used to step 2 dB down, and a
      path that appears or vanishes fades over 25 ms rather than 2.7.`,
`      crossfade a shut sound into an open one. <b style="color:var(--cool)">Walking
      through</b> an open door is continuous too: the doorway carries the room
      behind you to second order, and for 0.4 m either side of the plane both
      rooms are heard and handed over. Walking into the source's room holds
      about &minus;24.5 dB through the plane, where it used to step 2 dB down.`);

/* test signal: the automation box was already full */
rep(`            path by drawing one in your DAW. Furniture and pictures are saved
            with the project, not automated.`,
    `            path by drawing one in your DAW.`);

/* specifications */
rep(`    <p class="sub">The bench renders real audio and measures it: 90 checks, all
      clear, and 26 more for the furniture, the pictures and the light, beside
      141 that drive the panel. These are their numbers, not estimates.</p>`,
`    <p class="sub">The bench renders real audio and measures it &mdash; 90
      checks, and 26 for furniture, pictures and light, all clear, beside 141
      that drive the panel. These are its numbers, not estimates.</p>`);
rep(`    <div class="cols">
      <div class="c-46">
        <table class="tight">
          <tr><th>Item</th><th>Measured</th></tr>`,
`    <div class="cols specs">
      <div class="c-46">
        <table class="tight">
          <tr><th>Item</th><th>Measured</th></tr>`);
rep(`.pad.tight .rule{ margin: 4mm 0 4.5mm 0; }`,
`.pad.tight .rule{ margin: 4mm 0 4.5mm 0; }
.specs table.tight{ font-size: 8.1pt; }
.specs table.tight td{ padding: 0.9mm 1.5mm 0.9mm 0; }`);
rep(`          <tr><td>Type</td><td>Effect &mdash; stereo in, stereo out, plus a second stereo input bus (&ldquo;Aux In&rdquo;, the host's sidechain)</td></tr>`,
    `          <tr><td>Type</td><td>Effect &mdash; stereo in and out, plus &ldquo;Aux In&rdquo;, the host's sidechain</td></tr>`);
rep(`          <tr><td>Materials</td><td>Five, chosen per surface &mdash; walls, floor and ceiling of each room, nine choices. Two walls of every room can be broken and pushed &plusmn;0.6 m</td></tr>`,
    `          <tr><td>Materials</td><td>Five, per surface: nine choices. Two walls of every room break and push &plusmn;0.6 m</td></tr>`);
rep(`          <tr><td>Furniture</td><td>ten pieces, up to 24; pictures up to eight, print or acoustic panel</td></tr>
          <tr><td>Picture</td><td>ordinary, CINEMATIC and PHOTO; light sync for the lamps</td></tr>`,
    `          <tr><td>Furniture</td><td>ten pieces, up to 24; up to eight pictures, print or acoustic panel</td></tr>
          <tr><td>Picture</td><td>ordinary, CINEMATIC, PHOTO; light sync</td></tr>`);
rep(`          <tr><td>Delay reader</td><td>16-tap windowed sinc, so a source may stand anywhere without losing the top octave</td></tr>`,
    `          <tr><td>Delay reader</td><td>16-tap windowed sinc</td></tr>`);

if (miss.length) { console.log('NOTHING WRITTEN, anchors:\n  ' + miss.join('\n  ')); process.exit(1); }
if (CRLF) s = s.replace(/\n/g, '\r\n');
fs.writeFileSync(F, s);
console.log('written');
