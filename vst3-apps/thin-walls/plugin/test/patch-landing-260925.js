// Landing page for 260925.2: furniture, wall pictures, cinematic + photo,
// light sync, record and export. Validates every anchor, then writes.
const fs = require('fs');
const path = require('path');
const f = path.join(__dirname, '..', '..', 'index.html');
let s = fs.readFileSync(f, 'utf8');
const miss = [];
function rep(a, b) { const n = s.split(a).length - 1; if (n !== 1) { miss.push(JSON.stringify(a.slice(0, 70)) + ' x' + n); return; } s = s.replace(a, b); }

rep(`<section class="block">
  <div class="wrap">
    <h2>Four sources, two kinds, two input buses</h2>`, `<section class="block">
  <div class="wrap">
    <h2>Furnish it, hang your pictures, film it</h2>
    <p class="sub">The flat stopped being empty. Everything you put in it is
      heard as well as seen &mdash; and what you do in it can be exported as a
      video with its own sound.</p>

    <figure style="margin-top:0">
      <img src="img/gallery.jpg" alt="The living room with three framed pictures above a pair of loudspeakers on stands, a sofa, an armchair, a bookcase and a rug" />
      <figcaption>The living room as a listening room: a stereo pair on
        stands, three pictures loaded from files and hung on the wall, a sofa,
        an armchair, a bookcase and a rug &mdash; drawn in PHOTO mode.</figcaption>
    </figure>

    <h3>Furniture you can hear</h3>
    <p>
      Ten pieces &mdash; sofa, armchair, bed, rug, curtain, bookcase, table,
      grand piano, wardrobe and a standing person &mdash; placed and turned in
      the plan. Each one acts on the sound four ways, and each is measured:
    </p>
    <ul>
      <li><b>Absorption</b>, per octave band, from the figures the acoustics
        tables give per object. It adds straight into the room's Eyring decay:
        furnishing the tiled living room takes its reverberation time at 1 kHz
        from <b>3.42 s to 1.03 s</b>, and the rendered decay measures 1.13 s. A
        rug replaces the floor it covers, and the readout shows the engine's own
        net figure.</li>
      <li><b>Scattering</b>: a furnished room breaks its reflections up, and
        what they lose goes to the room's tail, not away.</li>
      <li><b>Blocking</b>: a leg of a path that passes through a piece bends
        over, under or round it with the same edge-diffraction law the doorways
        use. A bookcase between you and a source costs the direct sound
        <b>18.2 dB at 4 kHz</b> &mdash; matching a hand calculation, in the
        rendered audio &mdash; and walking out from behind it changes the loss
        by at most 0.19 dB per centimetre, so an edge never clicks.</li>
      <li><b>Reflection</b> off hard tops &mdash; a table, a closed piano lid
        &mdash; that fades off the edge rather than vanishing.</li>
    </ul>

    <h3>Your own pictures on the walls</h3>
    <p>
      Load an image, click a wall in the plan or in the 3D view to hang it,
      drag it along and up. Four sizes and a width slider, four frames (thin
      black, oak, gilt, canvas), up to eight pictures, saved inside the project
      so it opens complete on another computer. A <b>PRINT</b> is only a
      picture, and the panel says so: a framed print does nothing you can
      hear. An <b>ACOUSTIC PANEL</b> is your picture printed on a 5 cm fabric
      absorber, the way real ones are sold &mdash; 4 m&sup2; of it halves the
      tiled living room's decay at 1 kHz, 3.42 s to 1.69 s.
    </p>

    <h3>CINEMATIC and PHOTO</h3>
    <div class="figrow">
      <figure style="margin-top:0">
        <img src="img/cine-off.jpg" alt="The living room in the ordinary view" />
        <figcaption><b>The ordinary view</b>, which still draws nothing at all
          while nothing changes.</figcaption>
      </figure>
      <figure style="margin-top:0">
        <img src="img/cine-on.jpg" alt="The same room with CINEMATIC on: shadows, soft light, bloom" />
        <figcaption><b>CINEMATIC</b>: shadows from every lamp and from the
          hall's window, ambient occlusion, bloom and film tone mapping, at
          1.5&times; or 2&times; resolution &mdash; about 2 ms of graphics card
          per frame, never the audio thread.</figcaption>
      </figure>
    </div>
    <p>
      Stand still and <b>PHOTO</b> takes over: the light is traced, bouncing
      off the walls and through the open doors, and the picture refines itself
      over a few seconds until it is done &mdash; then the graphics card rests.
      The picture at the top of this section is one.
    </p>

    <h3>Lamps that follow the sound</h3>
    <p>
      <b>LIGHT SYNC</b> lets the lamps of each room follow the sound in that
      room &mdash; a hit flashes, a held chord glows, a sealed room next door
      stays dark &mdash; or pulse on your DAW's beats. It starts subtle and goes
      theatrical; at zero the lamps are exactly as they were.
    </p>

    <h3>Record a walk, export it as a video</h3>
    <div class="figrow">
      <figure style="margin-top:0">
        <img src="img/export.jpg" alt="Frames from an exported video: through the doorway into the hall" />
        <figcaption>Frames from an exported take: out of the living room,
          through the doorway, into the hall.</figcaption>
      </figure>
      <div>
        <p style="margin-top:0">
          <b>REC</b> in the header records up to four minutes: the sound going
          in, and everything that moves &mdash; your walk, where you look, the
          doors, the sources, the furniture. <b>EXPORT</b> renders it as an MP4
          (720p to 4K, 30 or 60 fps, as shown, cinematic or photo quality).
        </p>
        <p>
          The sound is re-rendered offline at full quality and every frame is
          redrawn from the same recording, so picture and sound cannot drift
          apart. Files land in <b>Documents\\Thin Walls videos</b>.
        </p>
      </div>
    </div>
  </div>
</section>

<section class="block">
  <div class="wrap">
    <h2>Four sources, two kinds, two input buses</h2>`);

rep(`stays in the views as a ghost so you can place it before you use it.`,
    `stays in the views, solid but unlit, so you can place it before you use it.`);

rep(`      <tr><td>CPU</td><td>One source about 17 % of a core at 48 kHz; four with every door open, 40 %</td></tr>`,
    `      <tr><td>Furniture</td><td>Ten pieces, up to 24: absorption per octave band into the room's decay, scattering, occlusion by edge diffraction, reflection off hard tops</td></tr>
      <tr><td>Pictures</td><td>Up to eight, loaded from image files and stored in the project; print (visual only) or acoustic panel (a 5 cm printed absorber)</td></tr>
      <tr><td>Picture</td><td>Ordinary, CINEMATIC (shadows, ambient occlusion, bloom, 1.5&times; or 2&times; resolution) and PHOTO (traced light, converging while you stand still); LIGHT SYNC for the lamps</td></tr>
      <tr><td>Video</td><td>Record up to four minutes; export an MP4 (H.264 + AAC) at 720p to 4K, 30 or 60 fps, sound re-rendered offline</td></tr>
      <tr><td>CPU</td><td>One source about 18 % of a core at 48 kHz; four sources and ten pieces of furniture, about 44 %</td></tr>`);

rep(`      Honest limits, also in the handbook: where the rooms are and which doors`,
    `      Honest limits: furniture and pictures act geometrically (absorption,
      scattering, blocking, reflection) rather than on low-frequency room
      modes; the handbook in the download predates the furniture, pictures,
      cinematic view and video export. Also in the handbook: where the rooms are and which doors`);

if (miss.length) { console.log('NOT WRITTEN:\n  ' + miss.join('\n  ')); process.exit(1); }
fs.writeFileSync(f, s);
console.log('written');
