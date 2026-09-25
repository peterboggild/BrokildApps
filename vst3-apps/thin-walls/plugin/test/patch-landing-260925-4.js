// Landing page for build 260925.4: render qualities, the personal head, bench numbers
const fs = require('fs');
const f = 'C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/thin-walls/index.html';
let s = fs.readFileSync(f, 'utf8');
const NL = (s.split('\r\n').length - 1) > (s.split('\n').length - 1) / 2 ? '\r\n' : '\n';
s = s.split('\r\n').join('\n');
const miss = [];
function rep(a, b, count) { const n = s.split(a).length - 1; if (n !== (count || 1)) { miss.push(n + 'x: ' + a.slice(0, 90)); return; } s = s.split(a).join(b); }

rep('<span class="buildtag">Build 260925.3</span>', '<span class="buildtag">Build 260925.4</span>');
rep('Handbook <small>PDF, 17 pages</small>', 'Handbook <small>PDF, 24 pages</small>');
rep('Thin-Walls-Manual.pdf <small>17 pages</small>', 'Thin-Walls-Manual.pdf <small>24 pages</small>');

rep(`          The sound is re-rendered offline at full quality and every frame is
          redrawn from the same recording, so picture and sound cannot drift
          apart. Files land in <b>Documents\\Thin Walls videos</b>.
        </p>
      </div>
    </div>`,
`          The sound is re-rendered offline and every frame is redrawn from the
          same recording, so picture and sound cannot drift apart. Files land
          in <b>Documents\\Thin Walls videos</b>; <b>BOUNCE WAV</b> writes the
          sound alone.
        </p>
      </div>
    </div>

    <h3>Render it properly: HIGH, ULTRA, ULTRA + BASS</h3>
    <p>
      Offline there is no audio thread to keep up with, so a take's sound can be
      worked out with far less approximation &mdash; and none of it changes what
      you hear while you play, or what that costs. <b>HIGH</b> traces wall
      reflections to the fourth order instead of the second. <b>ULTRA</b> goes to
      the sixth and replaces the statistical reverb with one <b>traced ray by ray
      through the actual rooms</b>: the folded walls, the furniture, the rug, the
      doors as they stand. Each room rings with its own shape, and the hall next
      door comes round the door or through the wall. <b>ULTRA + BASS</b> also
      simulates everything below 220 Hz as <b>a real sound wave</b> on a 10 cm
      grid of the whole flat: room modes, bass bending round a door, a door
      swinging open mid-take. A four-second take renders in about ten seconds.
    </p>

    <h3>Your own head</h3>
    <p>
      The built-in head is the MIT KEMAR dummy. <b>LOAD SOFA</b> takes any
      measured head in the standard SOFA format (AES69) &mdash; your own ears,
      if you have had them measured, or one from a public set such as SADIE,
      ARI or HUTUBS &mdash; live and in every render. It is diffuse-field
      equalised the same way, so the two compare fairly by ear, and it is kept
      with the project.
    </p>`);

rep(`    <p class="sub">90 checks, all clear, beside 73 that drive the panel. The
      bench renders real audio and measures it; these are its numbers.</p>`,
`    <p class="sub">90 checks, 26 for furniture and light and 33 for the offline
      renders, all clear, beside 141 that drive the panel. The bench renders
      real audio and measures it; these are its numbers.</p>`);
rep(`      <div class="fact"><div class="v">0.09 dB</div><div class="l">how far the general path search lands from the exact shoebox on a wall broken by 2 mm</div></div>`,
`      <div class="fact"><div class="v">0.09 dB</div><div class="l">how far the general path search lands from the exact shoebox on a wall broken by 2 mm</div></div>
      <div class="fact"><div class="v">0.4–0.9 dB</div><div class="l">between the ray-traced tail and the statistical one, in four materials &mdash; two models, one answer</div></div>
      <div class="fact"><div class="v">28.6 / 34.3 Hz</div><div class="l">the living room's axial modes in the wave simulation, exactly where c&nbsp;/&nbsp;2L puts them</div></div>`);

rep(`<tr><td>Reflections</td><td>Image sources to second order; a general surface-sequence search once a wall is broken, validated against the exact arithmetic to 0.09 dB</td></tr>`,
    `<tr><td>Reflections</td><td>Image sources to second order (fourth and sixth in offline renders); a general surface-sequence search once a wall is broken, validated against the exact arithmetic to 0.09 dB</td></tr>`);
rep(`<tr><td>Reverberation</td><td>One 16-line feedback delay network per room, allpass diffusers inside every line, per-band decay from Eyring, coupled through the doors and walls</td></tr>`,
    `<tr><td>Reverberation</td><td>One 16-line feedback delay network per room, allpass diffusers inside every line, per-band decay from Eyring, coupled through the doors and walls. Offline (ULTRA): tails ray-traced through the actual geometry; below 220 Hz (ULTRA + BASS) a finite-difference wave simulation of the whole flat</td></tr>`);
rep(`<tr><td>Head</td><td>MIT KEMAR compact set, 368 directions, resampled at prepare; EAR SPACING 15 to 100 cm</td></tr>`,
    `<tr><td>Head</td><td>MIT KEMAR compact set, 368 directions, resampled at prepare, or any SOFA file (AES69); EAR SPACING 15 to 100 cm</td></tr>`);
rep(`<tr><td>Video</td><td>Record up to four minutes; export an MP4 (H.264 + AAC) at 720p to 4K, 30 or 60 fps, sound re-rendered offline</td></tr>`,
    `<tr><td>Video</td><td>Record up to four minutes; export an MP4 (H.264 + AAC) at 720p to 4K, 30 or 60 fps, or the sound alone as a 24-bit WAV, rendered offline at LIVE, HIGH, ULTRA or ULTRA + BASS</td></tr>`);

rep(`      Honest limits: furniture and pictures act geometrically (absorption,
      scattering, blocking, reflection) rather than on low-frequency room
      modes; the handbook in the download predates the furniture, pictures,
      cinematic view and video export. Also in the handbook: where the rooms are and which doors
      join them cannot change; reflections are specular (only STUDIO scatters)
      and stop at the second order; breaking a wall moves the early reflections
      rather than the room's standing waves;`,
`      Honest limits: while you play, furniture and pictures act geometrically
      (absorption, scattering, blocking, reflection) and there are no
      low-frequency room modes &mdash; only an ULTRA + BASS render has them.
      Where the rooms are and which doors join them cannot change; live
      reflections are specular (only STUDIO scatters) and stop at the second
      order; breaking a wall moves the early reflections rather than the room's
      standing waves;`);

if (miss.length) { console.log('MISSES:\n' + miss.join('\n')); process.exit(1); }
if (NL === '\r\n') s = s.split('\n').join('\r\n');
fs.writeFileSync(f, s);
console.log('ok');
