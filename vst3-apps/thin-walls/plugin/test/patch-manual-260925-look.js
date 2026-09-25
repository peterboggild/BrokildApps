/*  Third pass for 260925.3: what LOOKING at the rendered sheets found and no
 *  gate could - text laid over the art on the cover and the back page, a
 *  box sitting on the folio rule, a status strip too thin to read, and a
 *  furniture sheet whose plates were smaller than the page had room for. */
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

/* cover: the blurb moves up onto ink; the art is the furnished room in PHOTO */
rep(`  <div class="cover-art">
    <img src="img/panel.jpg" alt="" />
    <div class="cover-fade"></div>
  </div>`,
`  <div class="cover-art" style="height:56%">
    <img src="img/photo.jpg" alt="" style="object-position:center 55%" />
    <div class="cover-fade"></div>
  </div>`);
rep(`  <div class="cover-foot">
    <p style="color:var(--dim); font-size:9.2pt; max-width:160mm; margin:0">`,
`  <div class="cover-foot" style="top:62mm; bottom:auto">
    <p style="color:var(--dim); font-size:9.2pt; max-width:160mm; margin:0">`);

/* furniture sheet: room for full-size plates */
rep(`    <div class="grid2 tight-plates">
      <figure>
        <img class="shot" src="img/plan-furn.jpg" alt="" />`,
`    <div class="grid2">
      <figure>
        <img class="shot" src="img/plan-furn.jpg" alt="" />`);

/* furniture sound: the readout box sat on the folio rule */
rep(`            the engine now calculates. The room on the previous page, tiled and
            with its two doors open, reads <span class="mono">RT60 1.73 s</span>
            empty and <span class="mono">+9.1 m&sup2; &middot; RT60 0.76 s</span>
            with those seven pieces in it. Blocking and scattering you hear
            rather than read.</p>`,
`            the engine now calculates. The room on the previous page, tiled and
            with its two doors open, reads <span class="mono">RT60 1.73 s</span>
            empty and <span class="mono">+9.1 m&sup2; &middot; RT60 0.76 s</span>
            with those seven pieces in it.</p>`);
rep(`        <div class="box" style="margin-top:3mm">
          <h4>The readout line</h4>`,
`        <div class="box" style="margin-top:1mm">
          <h4>The readout line</h4>`);

/* the late field: show the readout end of the foot, large enough to read */
rep(`          <img class="shot" src="img/status.jpg" alt="" />
          <figcaption><b>Along the foot:</b>`,
`          <img class="shot" src="img/status.jpg" alt="" style="height:7mm; object-fit:cover; object-position:right center" />
          <figcaption><b>The right end of the foot:</b>`);

/* back page: a shorter band of art, below the text */
rep(`  <div class="cover-art" style="height:36%">
    <img src="img/pov.jpg" alt="" style="object-position:center 55%" />`,
`  <div class="cover-art" style="height:22%">
    <img src="img/cine-on.jpg" alt="" style="object-position:center 62%" />`);

if (miss.length) { console.log('NOTHING WRITTEN, anchors:\n  ' + miss.join('\n  ')); process.exit(1); }
if (CRLF) s = s.replace(/\n/g, '\r\n');
fs.writeFileSync(F, s);
console.log('written');
