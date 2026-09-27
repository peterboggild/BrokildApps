/*  Brings the handbook to build 260925.3: furniture, wall pictures, CINEMATIC
 *  and PHOTO, light sync, record and export; solid switched-off sources; the
 *  seamless doorway; the header's new buttons; the specifications and limits.
 *  Every anchor must match exactly once or nothing is written.
 *  Folios are renumbered from the document's own order afterwards.
 */
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

/* ------------------------------------------------------------- CSS */
rep(`.card .shot{ max-height: 34mm; width: auto; }`,
`.card .shot{ max-height: 34mm; width: auto; }
/*  From 260925.3 the plates are shot at twice the panel's pixels and the 3D
    view is 2.1 to 1, so a grid of three takes the width and follows it. */
.grid3.views .shot{ max-height: none; width: 100%; height: auto; }
.fit .shot{ width: 100%; height: auto; max-height: none; }
.ctl.small p{ font-size: 9pt; }
.pad.tight .rule{ margin: 4mm 0 4.5mm 0; }`);

/* ------------------------------------------------------------- cover */
rep(`<p class="says" style="margin:0">BUILD 260922.1</p>`, `<p class="says" style="margin:0">BUILD 260925.3</p>`);
rep(`      Three rooms, three doors, four sources and a head you can walk around with.
      Everything you hear is a path the engine has actually traced &mdash; through
      the air, off the walls, round the door frame or straight through the leaf.`,
`      Three rooms, three doors, four sources, whatever furniture you put in them,
      and a head you can walk around with. Everything you hear is a path the
      engine has actually traced &mdash; through the air, off the walls, over the
      sofa, round the door frame or straight through the leaf.`);

/* ------------------------------------------------------------- page 2 */
rep(`        <p>
          There is no reverb-amount control, because the amount is decided by
          where you stood.
        </p>`,
`        <p>
          There is no reverb-amount control, because the amount is decided by
          where you stood. You can furnish the rooms, hang your own pictures on
          the walls, and record a walk through the flat as a video with its own
          sound.
        </p>`);

/* ------------------------------------------------------------- page 3, the panel */
rep(`          <figcaption><b>Left:</b> the view from where you are standing.
            <b>Right:</b> the plan. <b>Below:</b> doors, materials, the selected
            source, levels and the test signal. <b>Foot:</b> input, output and
            the direct-to-reverberant ratio.</figcaption>`,
`          <figcaption><b>Top:</b> CINEMATIC, REC and EXPORT, and the build.
            <b>Left:</b> the view from where you are standing. <b>Right:</b> the
            plan. <b>Below:</b> eight groups of controls, in two rows &mdash; the
            second row scrolls into view. <b>Foot:</b> input, output and the
            direct-to-reverberant ratio.</figcaption>`);
rep(`    <h3 class="display" style="font-size:11pt">The seven groups</h3>
    <table class="tight" style="margin-top:3mm">
      <tr>
        <td>Doors</td><td style="color:var(--body)">one aperture each, and a button for the two ends</td>
        <td>Broken walls</td><td style="color:var(--body)">split a wall and push the break out, per room</td>
        <td>View and files</td><td style="color:var(--body)">rays, defaults, save and load, and where you are standing</td>
      </tr>
      <tr>
        <td>Materials</td><td style="color:var(--body)">nine choices &mdash; walls, floor and ceiling of each room</td>
        <td>Levels and head</td><td style="color:var(--body)">the three trims, mix, output and ear spacing</td>
        <td>Test signal</td><td style="color:var(--body)">a file of your own, looped into the input</td>
      </tr>
      <tr>
        <td>Source</td><td style="color:var(--body)">everything about whichever of the four is selected</td>
        <td></td><td></td><td></td><td></td>
      </tr>
    </table>`,
`    <h3 class="display" style="font-size:11pt">The eight groups, and the header</h3>
    <table class="tight" style="margin-top:3mm">
      <tr>
        <td>Doors</td><td style="color:var(--body)">one aperture each, and a button for the two ends</td>
        <td>Source</td><td style="color:var(--body)">everything about whichever of the four is selected</td>
        <td>Test signal</td><td style="color:var(--body)">a file of your own, looped into the input</td>
      </tr>
      <tr>
        <td>Materials</td><td style="color:var(--body)">nine choices &mdash; walls, floor and ceiling of each room</td>
        <td>Levels and head</td><td style="color:var(--body)">the three trims, mix, output and ear spacing</td>
        <td>Furniture</td><td style="color:var(--body)">the tray of ten pieces, and the wall pictures</td>
      </tr>
      <tr>
        <td>Broken walls</td><td style="color:var(--body)">split a wall and push the break out, per room</td>
        <td>View and files</td><td style="color:var(--body)">rays, defaults, save and load, where you stand, and light sync</td>
        <td>Header</td><td style="color:var(--body)">CINEMATIC and its quality, REC, EXPORT</td>
      </tr>
    </table>`);

/* ------------------------------------------------------------- page 4, walking */
rep(`          at 1.65 m. Drag inside it to look around; <span class="mono">W A S D</span>
          to walk. Walls stop you, and so does a door less than half open.</figcaption>`,
`          at 1.65 m. Drag inside it to look around; <span class="mono">W A S D</span>
          to walk. Walls stop you, so does a door less than half open, and so
          does any furniture that stands in the way of sound.</figcaption>`);
rep(`          room is outside the flat and is solid; the plan will not let you drag
          there, and if automation sends you there the engine puts you back in
          the nearest room.
        </p>`,
`          room is outside the flat and is solid; the plan will not let you drag
          there, and if automation sends you there the engine puts you back in
          the nearest room. Walking through an open doorway is seamless: for
          0.4 m either side of the door the engine hears both rooms at once and
          hands one to the other (page 15).
        </p>`);

/* ------------------------------------------------------------- page 7, inputs */
rep(`          first is <b>OFF</b>: the source makes no sound, costs nothing, and
          stays in the views as a ghost so you can place it before you use it.`,
`          first is <b>OFF</b>: the source makes no sound, costs nothing, and
          stays in the views, drawn solid but unlit &mdash; only a fed source
          glows &mdash; so you can place it before you use it.`);
rep(`<tr><td>OFF</td><td>nothing &mdash; silent, ghosted, still placeable</td></tr>`,
    `<tr><td>OFF</td><td>nothing &mdash; silent, unlit, still placeable</td></tr>`);
rep(`            One source costs about <b>17 %</b> of a core at 48 kHz. Four, with
            every door open, cost <b>40 %</b>. Switching a source to OFF gives
            its share straight back.`,
`            One source costs about <b>18 %</b> of a core at 48 kHz. Four, with
            ten pieces of furniture in the flat, about <b>44 %</b>. Switching a
            source to OFF gives its share straight back.`);

/* ------------------------------------------------------------- behind a door */
rep(`    <p style="margin:1mm 0 0 0; font-size:9.3pt; color:var(--cool)">
      A door part open sits between the two, continuously: the engine does not
      crossfade a shut sound into an open one. The opening is a real strip of a
      real width, and the diffraction, the leaf and the room's own absorption
      all follow from it.
    </p>`,
`    <p style="margin:1mm 0 0 0; font-size:9.1pt; color:var(--cool)">
      A door part open sits between the two, continuously: the engine does not
      crossfade a shut sound into an open one. <b style="color:var(--cool)">Walking
      through</b> an open door is continuous too. In the opening you can see
      almost everything in the room behind you, so the doorway model carries
      that room's reflections to second order with the same keys as the in-room
      model, and for 0.4 m either side of the plane both rooms' fields are
      heard and handed over. Walking into the source's room holds about
      &minus;24.5 dB through the plane where it used to step 2 dB down, and a
      path that appears or vanishes fades over 25 ms rather than 2.7.
    </p>`);

/* ------------------------------------------------------------- levels page */
rep(`          <figcaption><b>Levels and head</b>, with the view and file buttons
            below: Rays, Default, Save, Load, and a readout of where you are
            standing.</figcaption>`,
`          <figcaption><b>Levels and head.</b> Rays, Default, Save and Load
            sit in the group beside it, View and files, with a readout of where
            you are standing (page 20).</figcaption>`);

/* ------------------------------------------------------------- test signal page */
rep(`          patch holds every surface, every broken wall, the doors, all four
          sources and the levels &mdash; the whole arrangement. <em>Default</em>
          puts every parameter back; the floor plan itself never changes.`,
`          patch holds every surface, every broken wall, the doors, all four
          sources, the levels, the furniture and the pictures &mdash; the whole
          arrangement. <em>Default</em> puts every parameter back and empties
          the rooms; the floor plan itself never changes.`);
rep(`            position. Walking is recorded as automation, so a move you made by
            hand can be replayed exactly &mdash; and a source can be given a
            path by drawing one in your DAW.`,
`            position. Walking is recorded as automation, so a move you made by
            hand can be replayed exactly &mdash; and a source can be given a
            path by drawing one in your DAW. Furniture and pictures are saved
            with the project, not automated.`);

/* ------------------------------------------------------------- specifications */
rep(`    <p class="sub">The bench renders real audio and measures it: 90 checks, all
      clear, beside 73 that drive the panel. These are their numbers, not
      estimates.</p>`,
`    <p class="sub">The bench renders real audio and measures it: 90 checks, all
      clear, and 26 more for the furniture, the pictures and the light, beside
      141 that drive the panel. These are their numbers, not estimates.</p>`);
rep(`          <tr><td>MIX at 0</td><td>bit-exact passthrough</td></tr>
        </table>`,
`          <tr><td>MIX at 0</td><td>bit-exact passthrough</td></tr>
          <tr><td>Furnishing</td><td>tiled living room, 1 kHz: 3.42 s to 1.03 s by Eyring, 1.13 s in the rendered decay</td></tr>
          <tr><td>A bookcase in the way</td><td>18.2 dB off the direct sound at 4 kHz; at most 0.19 dB per cm at its edge</td></tr>
        </table>`);
rep(`          <tr><td>Head</td><td>MIT KEMAR compact set, 368 directions, diffuse-field equalised, converted to minimum phase with a separate onset delay, resampled to the host rate and interpolated on the grid</td></tr>`,
    `          <tr><td>Head</td><td>MIT KEMAR compact set, 368 directions, diffuse-field equalised, minimum phase with a separate onset delay</td></tr>`);
rep(`          <tr><td>CPU</td><td>one source about 17 % of a core at 48 kHz; four with every door open, 40 %</td></tr>`,
`          <tr><td>Furniture</td><td>ten pieces, up to 24; pictures up to eight, print or acoustic panel</td></tr>
          <tr><td>Picture</td><td>ordinary, CINEMATIC and PHOTO; light sync for the lamps</td></tr>
          <tr><td>Video</td><td>takes up to 4 minutes; MP4 (H.264 + AAC), 720p to 4K, 30 or 60 fps</td></tr>
          <tr><td>CPU</td><td>one source about 18 % of a core at 48 kHz; four sources and ten pieces of furniture, about 44 %. The picture runs on the graphics card</td></tr>`);
rep(`          <tr><td>Sample rates</td><td>any; bench-tested at 44.1 and 96 kHz</td></tr>
          <tr><td>Hosts</td><td>any VST3 host &mdash; developed against Ableton Live 12</td></tr>`,
`          <tr><td>Sample rates</td><td>any; bench-tested at 44.1 and 96 kHz. Hosts: any VST3 host, developed against Ableton Live 12</td></tr>`);

/* ------------------------------------------------------------- back page */
rep(`        <p style="font-size:9.8pt">
          <b>Two sources in one room share its field.</b> That is correct &mdash;
          a room has one reverberant field &mdash; but the share each hands it is
          taken at 1 kHz rather than per band.
        </p>`,
`        <p style="font-size:9.8pt">
          <b>Two sources in one room share its field.</b> That is correct &mdash;
          a room has one reverberant field &mdash; but the share each hands it is
          taken at 1 kHz rather than per band.
        </p>
        <p style="font-size:9.8pt">
          <b>Furniture is geometric.</b> It absorbs, scatters, blocks and
          reflects; it does not damp a room's bass modes, and a print on the
          wall does nothing you can hear. PHOTO traces a simplified box model of
          the flat, so what it lights is the rooms and their furniture, not
          every detail of the ordinary view.
        </p>`);
rep(`          <b>Breaking a wall moves the early reflections, not the modes.</b>
          The late field here is statistical, so splaying a wall does not
          redistribute a room's standing waves the way it would in a modal
          model. Above the Schroeder frequency that is where splay works in a
          real room too; below it, absorption is what helps.`,
`          <b>Breaking a wall moves the early reflections, not the modes.</b>
          The late field here is statistical, so splaying a wall does not
          redistribute a room's standing waves; above the Schroeder frequency
          that is where splay works in a real room too.`);
rep(`<p class="says" style="margin-top:6mm; font-size:8.4pt">BUILD 260922.1 &middot; WINDOWS VST3</p>`,
    `<p class="says" style="margin-top:6mm; font-size:8.4pt">BUILD 260925.3 &middot; WINDOWS VST3</p>`);

/* ============================================================== NEW SHEETS */
const FURN = `
<!-- ==========================================  FURNITURE: PLACING  ====== -->
<section class="page">
  <div class="pad tight">
    <p class="kick">Furniture</p>
    <h2 class="display">Furnish the flat</h2>
    <p class="sub">Ten pieces, up to 24 in the apartment, placed and turned in
      the plan and built in the 3D view. Every one is heard as well as seen.</p>
    <hr class="rule" />

    <div class="grid2 tight-plates">
      <figure>
        <img class="shot" src="img/plan-furn.jpg" alt="" />
        <figcaption><b>The plan.</b> A rug, a sofa (selected, with its turn
          handle on its front), an armchair, a bookcase, a table, a curtain on
          the west wall and a standing person. The room's RT60 under its name
          is the engine's, furniture included.</figcaption>
      </figure>
      <figure>
        <img class="shot" src="img/pov-furn.jpg" alt="" />
        <figcaption><b>The same room from where you stand.</b> The loudspeaker
          on the left is fed; the one beside it is switched off and is drawn
          solid, only unlit.</figcaption>
      </figure>
    </div>

    <div class="cols" style="margin-top:4mm">
      <div>
        <div class="ctl small">
          <h3>The tray</h3>
          <p class="what">Click, then click the plan</p>
          <p>The ten pieces sit as a row of icons at the top of the
            <b>Furniture</b> group: sofa, armchair, bed, rug, curtain,
            bookcase, table, grand piano, wardrobe and a person. Click one to
            arm it and click in the plan to set it down &mdash; or drag it
            straight onto the plan.</p>
        </div>
      </div>
      <div>
        <div class="ctl small">
          <h3>Pick up, turn, copy</h3>
          <p class="what">In the plan</p>
          <p>Click a piece to select it and drag it about. Turn it by the round
            handle on its front, in 15&deg; steps (Shift lets go of them), or
            with <b>FACING</b> and its &minus;15&deg; / +15&deg; buttons.
            <b>DUPLICATE</b> sets a copy beside it; <b>DELETE</b> or the Delete
            key takes it out; Escape lets go. <b>CLEAR ALL</b> asks twice.</p>
        </div>
      </div>
      <div>
        <div class="ctl small">
          <h3>You cannot walk through it</h3>
          <p class="what">Rooms, and your body</p>
          <p>A piece belongs to the room its centre is in, and stays inside it.
            Anything that blocks sound blocks you as well, so the sofa and the
            bookcase stop you walking; the rug and the curtain do not. If a
            piece is set down on top of you, you may walk out of it.</p>
        </div>
      </div>
    </div>
  </div>
  <div class="folio"><span>Furniture</span><span class="n">00</span></div>
</section>

<!-- ==========================================  FURNITURE: THE SOUND  ==== -->
<section class="page">
  <div class="pad tight">
    <p class="kick">Furniture</p>
    <h2 class="display">What it does to the sound</h2>
    <p class="sub">Four things, each from the acoustics tables or from the
      geometry, and each measured in the rendered audio.</p>
    <hr class="rule" />

    <div class="cols">
      <div class="c-54">
        <div class="grid2" style="gap:1mm 7mm">
          <div class="ctl small">
            <h3>Absorption</h3>
            <p class="what">Per octave band</p>
            <p>Every piece brings its own equivalent absorption area, and it
              adds straight into the room's Eyring decay. Furnishing the tiled
              living room takes its reverberation time at 1 kHz from
              <b>3.42 s to 1.03 s</b>; the rendered decay measures 1.13 s. A rug
              replaces the floor it covers rather than adding to it.</p>
          </div>
          <div class="ctl small">
            <h3>Scattering</h3>
            <p class="what">Reflections broken up</p>
            <p>A furnished room returns its wall reflections less cleanly. What
              each bounce loses is not thrown away: it goes to the room's tail,
              the same route the STUDIO material uses.</p>
          </div>
          <div class="ctl small">
            <h3>Blocking</h3>
            <p class="what">Over, under or round</p>
            <p>A path that runs into a piece bends over, under or round it with
              the edge-diffraction law the doorways use. A bookcase between you
              and a source costs the direct sound <b>18.2 dB at 4 kHz</b>,
              matching a hand calculation. Walking out from behind it changes
              the loss by at most <b>0.19 dB per centimetre</b>, so an edge
              never clicks.</p>
          </div>
          <div class="ctl small">
            <h3>A hard top</h3>
            <p class="what">Table, piano lid</p>
            <p>The table's top and the closed piano lid are hard, and throw a
              reflection up at a head above them &mdash; the classic table
              bounce. It fades off the edge rather than vanishing. Only the
              table's top slab blocks, 71 to 75 cm up; the piano's body blocks
              from 35 cm to 1 m.</p>
          </div>
        </div>

        <div class="box" style="margin-top:3mm">
          <h4>The readout line</h4>
          <p>Under the tray, for the room you are standing in: how much
            absorption its furniture adds at 1 kHz, and the reverberation time
            the engine now calculates. The room on the previous page, tiled and
            with its two doors open, reads <span class="mono">RT60 1.73 s</span>
            empty and <span class="mono">+9.1 m&sup2; &middot; RT60 0.76 s</span>
            with those seven pieces in it. Blocking and scattering you hear
            rather than read.</p>
        </div>
      </div>

      <div class="c-40" style="margin-left:auto">
        <h3 class="display" style="font-size:11pt">The ten pieces</h3>
        <table class="tight" style="margin-top:2.5mm">
          <tr><th>Piece</th><th class="num">W &times; D &times; H (m)</th><th>Stands in the way</th></tr>
          <tr><td>SOFA</td><td class="num">2.10 &times; 0.90 &times; 0.85</td><td>yes</td></tr>
          <tr><td>ARMCHAIR</td><td class="num">0.85 &times; 0.85 &times; 0.90</td><td>yes</td></tr>
          <tr><td>BED</td><td class="num">2.05 &times; 1.60 &times; 0.55</td><td>yes, low</td></tr>
          <tr><td>RUG</td><td class="num">2.40 &times; 1.70 &times; 0.02</td><td>no &mdash; it replaces the floor</td></tr>
          <tr><td>CURTAIN</td><td class="num">2.40 &times; 0.15 &times; 2.50</td><td>no &mdash; it kills a wall</td></tr>
          <tr><td>BOOKCASE</td><td class="num">1.00 &times; 0.35 &times; 2.00</td><td>yes; also scatters</td></tr>
          <tr><td>TABLE</td><td class="num">1.60 &times; 0.90 &times; 0.75</td><td>its top only; hard</td></tr>
          <tr><td>GRAND PIANO</td><td class="num">2.10 &times; 1.50 &times; 1.00</td><td>its body; lid hard</td></tr>
          <tr><td>WARDROBE</td><td class="num">1.20 &times; 0.60 &times; 2.10</td><td>yes, tall</td></tr>
          <tr><td>PERSON</td><td class="num">0.50 &times; 0.30 &times; 1.75</td><td>yes; absorbs the top</td></tr>
        </table>
        <p style="margin-top:3mm; font-size:8.8pt; color:var(--dim)">
          Hover a piece in the tray for what it is and why it sounds the way it
          does. The layout is saved with the project and in patch files; it is
          not a set of host parameters, so it is not automated.
        </p>
        <div class="box cool" style="margin-top:3mm">
          <h4>Honestly</h4>
          <p>Everything is geometric acoustics. A sofa in a corner does not
            damp a bass mode here, and nothing changes when a person sits
            down.</p>
        </div>
      </div>
    </div>
  </div>
  <div class="folio"><span>Furniture</span><span class="n">00</span></div>
</section>

<!-- ==========================================  WALL PICTURES  =========== -->
<section class="page">
  <div class="pad tight">
    <p class="kick">Wall pictures</p>
    <h2 class="display">Your own pictures on the walls</h2>
    <p class="sub">Load an image, click a wall to hang it. As a print it is
      only a picture; as an acoustic panel it is heard.</p>
    <hr class="rule" />

    <div class="cols">
      <div class="c-54">
        <div class="ctl small">
          <h3>Hanging one</h3>
          <p class="what">Load, then click a wall</p>
          <p><b>LOAD</b> under <b>Wall pictures</b> picks a JPEG or PNG. The
            panel shrinks it to 1024 px, keeps it with the project, and waits
            for you to click a wall &mdash; in the plan or in the 3D view. Click
            a picture to select it; drag it along its wall, and in the 3D view
            up and down too. It is kept on its wall, under the ceiling and clear
            of any door. <b>DELETE</b> takes the selected one down. Up to
            <b>eight</b>; the counter beside the buttons says how many hang.</p>
        </div>
        <div class="ctl small">
          <h3>Size and frame</h3>
          <p class="what">S / M / L / XL, WIDTH, FRAME</p>
          <p>Quick widths: S 40 cm, M 70 cm, L 1 m, XL 1.5 m; or <b>WIDTH</b>
            anywhere from 20 cm to 2 m. The height follows the image's own
            proportions. With nothing selected they set the size of the next one
            you hang. <b>FRAME</b> is thin black, oak or gilt moulding, or an
            unframed canvas whose picture wraps round its edges &mdash; purely
            visual.</p>
        </div>
        <div class="box" style="margin-top:2mm">
          <h4>PRINT or ACOUSTIC PANEL</h4>
          <p>A <b>PRINT</b> is paper behind glass: acoustically it is part of
            the wall, and it changes nothing you hear. The panel says so.</p>
          <p>An <b>ACOUSTIC PANEL</b> is the same picture printed on a 5 cm
            fabric absorber, the kind sold for studios. Its face area joins the
            room's absorption in place of the wall it covers:
            <b>4 m&sup2;</b> of them halve the tiled living room's decay at
            1 kHz, <b>3.42 s to 1.69 s</b>.</p>
        </div>
        <p style="margin-top:3mm; font-size:9pt; color:var(--dim)">
          Pictures are stored inside the project and in patch files, image and
          all, so a project opens complete on another computer. They are drawn
          in the plan, the 3D view, CINEMATIC and PHOTO, and in exported
          videos. On a broken wall a picture hangs on the panel under its
          centre, at that panel's angle.
        </p>
      </div>

      <div class="c-40 fit" style="margin-left:auto">
        <figure>
          <img class="shot" src="img/pov-pics.jpg" alt="" />
          <figcaption><b>Three pictures on the west wall</b> of the living
            room, above a stereo pair of loudspeakers.</figcaption>
        </figure>
        <figure style="margin-top:4mm">
          <img class="shot" src="img/ctrl-furn.jpg" alt="" />
          <figcaption><b>The Furniture group</b>, with <b>Wall pictures</b>
            below the tray. The line at the foot names what is selected: here a
            print, 0.85 &times; 0.87 m, on the living room's west wall.</figcaption>
        </figure>
      </div>
    </div>
  </div>
  <div class="folio"><span>Wall pictures</span><span class="n">00</span></div>
</section>
`;

const PICTURE = `
<!-- ==========================================  CINEMATIC AND PHOTO  ===== -->
<section class="page">
  <div class="pad tight">
    <p class="kick">The picture</p>
    <h2 class="display">CINEMATIC and PHOTO</h2>
    <p class="sub">A second, much heavier renderer for the 3D view. It costs the
      graphics card and never the audio.</p>
    <hr class="rule" />

    <div class="grid3 views">
      <figure>
        <img class="shot" src="img/cine-off.jpg" alt="" />
        <figcaption><b>The ordinary view.</b> Still draws nothing at all while
          nothing changes.</figcaption>
      </figure>
      <figure>
        <img class="shot" src="img/cine-on.jpg" alt="" />
        <figcaption><b>CINEMATIC, HIGH.</b> Shadows from the lamp, ambient
          occlusion, bloom, film tone curve. The corner reads the frame
          time.</figcaption>
      </figure>
      <figure>
        <img class="shot" src="img/photo.jpg" alt="" />
        <figcaption><b>PHOTO, ULTRA,</b> converged: light traced off the walls,
          soft shadows under the stands. 512 samples a pixel, 5.6 s.</figcaption>
      </figure>
    </div>

    <div class="cols" style="margin-top:4mm">
      <div>
        <div class="ctl small">
          <h3>Cinematic</h3>
          <p class="what">The header button</p>
          <p>An HDR picture with multisampling, real shadows from the pendants
            and the hall's window, ambient occlusion, physically based
            highlights, bloom and a filmic tone curve. It runs continuously
            while on &mdash; dust in the window light, a faint beam under each
            lamp. The first time it is switched on it compiles its shaders, a
            couple of seconds. Remembered on this computer, not in the project.</p>
        </div>
      </div>
      <div>
        <div class="ctl small">
          <h3>HIGH or ULTRA</h3>
          <p class="what">The menu beside it</p>
          <p><b>HIGH</b> renders at 1.5 times the screen's pixels, <b>ULTRA</b>
            at 2 times, both with 4&times; multisampling. About <b>2 ms</b> of
            graphics card per frame at HIGH on a current card; the corner of the
            view shows what yours takes. ULTRA also takes PHOTO further: 512
            samples a pixel instead of 256.</p>
        </div>
      </div>
      <div>
        <div class="ctl small">
          <h3>PHOTO</h3>
          <p class="what">When you stand still</p>
          <p>Stand still for 0.6 s and PHOTO takes over: the light is traced,
            bouncing off the walls and through the open doors, and the picture
            refines itself over a few seconds. The corner counts it up to
            <span class="mono">done</span> &mdash; and then the graphics card
            rests. Move, and the live picture is back at once.</p>
        </div>
      </div>
    </div>

    <figure class="strip" style="margin-top:1mm; width:95mm">
      <img class="shot" src="img/hdr.jpg" alt="" />
      <figcaption><b>The header</b>, CINEMATIC on: its quality, REC with the
        take clock, and EXPORT.</figcaption>
    </figure>
  </div>
  <div class="folio"><span>The picture</span><span class="n">00</span></div>
</section>

<!-- ==========================================  LIGHT SYNC  ============== -->
<section class="page">
  <div class="pad tight">
    <p class="kick">Light sync</p>
    <h2 class="display">Lamps that follow the sound</h2>
    <p class="sub">Each room's pendants can follow the sound in that room, or
      pulse on your host's beat. At zero they are exactly the lamps they always
      were.</p>
    <hr class="rule" />

    <div class="cols">
      <div class="c-58">
        <p>
          The engine measures how much sound is in each room: what that room's
          sources play plus its own late field, half by level and half by
          <b>punch</b> &mdash; a fast envelope over a slow one, so an onset
          flashes and a held sound glows. Measured, an onset reads
          <b>0.93</b> where the same noise held reads <b>0.44</b>; silence is
          exactly zero, and a sealed room next door stays at <b>0.000</b>. The
          lamps you see are the rooms you would hear.
        </p>

        <div class="grid2" style="gap:1mm 7mm; margin-top:3mm">
          <div class="ctl small">
            <h3>Sync</h3>
            <p class="what">0 to 100 %, default 20</p>
            <p>How much the lamps follow. At 0 they are off the hook entirely.
              The default breathes a few percent; all the way up the lamps dim
              in silence and flare on every hit.</p>
          </div>
          <div class="ctl small">
            <h3>Follow</h3>
            <p class="what">The default drive</p>
            <p>Each room's pendants follow the sound in that room. The picture
              is fed by the 30 Hz scene stream, so a hard transient can look a
              frame behind.</p>
          </div>
          <div class="ctl small">
            <h3>Beat</h3>
            <p class="what">The host's clock</p>
            <p>The pendants pulse on the host's beats while its transport plays.
              Stopped, or with no host clock, they follow the sound instead.</p>
          </div>
          <div class="ctl small">
            <h3>Window</h3>
            <p class="what">Off by default</p>
            <p>Lets the hall's daylight window join in. Daylight does not
              usually dance, which is why it waits to be asked.</p>
          </div>
        </div>

        <p style="margin-top:2mm; font-size:9.2pt">
          Like CINEMATIC, light sync is remembered on this computer rather than
          in the project. It costs nothing on the audio thread; with the
          ordinary view it means the view redraws while sound plays instead of
          resting. An exported video uses the offline render's own light and
          the recorded host clock for every frame, so it pulses exactly with
          its sound.
        </p>
      </div>

      <div class="c-36" style="margin-left:auto">
        <figure>
          <img class="shot" src="img/ctrl-view.jpg" alt="" />
          <figcaption><b>View and files</b>, with <b>Light sync</b> at its
            foot.</figcaption>
        </figure>
        <div class="box" style="margin-top:4mm">
          <h4>The rest of the group</h4>
          <p><em>Rays</em> draws the engine's paths on the plan;
            <em>Default</em>, <em>Save</em> and <em>Load</em> are the patch
            buttons of page 18. The readout is where you stand and which way
            you face &mdash; set in the views, never here.</p>
        </div>
      </div>
    </div>
  </div>
  <div class="folio"><span>Light sync</span><span class="n">00</span></div>
</section>

<!-- ==========================================  RECORD AND EXPORT  ======= -->
<section class="page">
  <div class="pad tight">
    <p class="kick">Record and export</p>
    <h2 class="display">Film a walk through the flat</h2>
    <p class="sub">Record a take of up to four minutes, then render it as an MP4
      with its own sound.</p>
    <hr class="rule" />

    <div class="cols">
      <div class="c-54">
        <div class="ctl small">
          <h3>Rec</h3>
          <p class="what">In the header</p>
          <p>Captures a take: the sound going in (the test signal, if it is
            playing), every host parameter as it moves, the furniture, and
            where you look. Walk the rooms, open doors, move sources &mdash; all
            of it is recorded. The button reads <b>STOP</b> while it runs and
            the clock beside it counts against the <b>4:00</b> limit. Press it
            again to stop; the take is held until you record another.</p>
        </div>
        <div class="ctl small">
          <h3>Export</h3>
          <p class="what">The panel under the header button</p>
          <table class="tight" style="margin-top:1mm">
            <tr><td>SIZE</td><td>1280&times;720, 1920&times;1080, 2560&times;1440 or 3840&times;2160</td></tr>
            <tr><td>fps</td><td>30 or 60; 60 doubles the rendering time</td></tr>
            <tr><td>PICTURE</td><td>AS SHOWN, CINEMATIC, PHOTO 16 or PHOTO 64 &mdash; the PHOTO settings trace that many samples into every frame, at seconds a frame</td></tr>
            <tr><td>INSET</td><td>a small plan in the corner; <b>off by default</b></td></tr>
            <tr><td>RENDER MP4</td><td>renders the last take; CANCEL stops it and deletes the partial file</td></tr>
            <tr><td>REVEAL</td><td>shows the last video in Explorer</td></tr>
          </table>
        </div>
        <div class="box" style="margin-top:2mm">
          <h4>Why picture and sound cannot drift</h4>
          <p>The sound is re-rendered offline, at full quality, through a fresh
            engine. Every frame is then redrawn from the same recording &mdash;
            the parameters at that frame's time, not the live controls &mdash;
            so what you see and what you hear come from one timeline. The video
            is H.264 with AAC sound, and it lands in
            <span class="mono">DOCUMENTS\\THIN WALLS VIDEOS</span>.</p>
        </div>
      </div>

      <div class="c-40 fit" style="margin-left:auto">
        <figure>
          <img class="shot" src="img/hdr-rec.jpg" alt="" />
          <figcaption><b>Recording.</b> REC has become STOP, and the take is
            3.2 seconds old.</figcaption>
        </figure>
        <figure style="margin-top:5mm">
          <img class="shot" src="img/exp-pop.jpg" alt="" />
          <figcaption><b>The export panel</b>, with a take held. Until there is
            one, the line at its foot says what is missing; afterwards it names
            the last video written.</figcaption>
        </figure>
        <div class="box cool" style="margin-top:5mm">
          <h4>What it is for</h4>
          <p>Showing someone what a room does &mdash; a walk out of the living
            room and into the hall, the door closing behind you &mdash; with the
            sound it actually makes. The panel is locked while an export
            runs.</p>
        </div>
      </div>
    </div>
  </div>
  <div class="folio"><span>Record and export</span><span class="n">00</span></div>
</section>
`;

rep(`

<!-- =========================================  9  BEHIND A DOOR  ========= -->`,
    FURN + `
<!-- =========================================  BEHIND A DOOR  =========== -->`);
rep(`<!-- ============================================  13  SPECIFICATIONS  ==== -->`,
    PICTURE.trimStart() + `
<!-- ============================================  SPECIFICATIONS  ======== -->`);

if (miss.length) { console.log('NOTHING WRITTEN, anchors:\n  ' + miss.join('\n  ')); process.exit(1); }

/* renumber folios in document order: cover has none, so sheet k gets k */
let k = 1;
s = s.replace(/<section class="page">[\s\S]*?<\/section>/g, sec => {
  const n = k++;
  return sec.replace(/<span class="n">\d+<\/span>/, '<span class="n">' + String(n).padStart(2, '0') + '</span>');
});
if (CRLF) s = s.replace(/\n/g, '\r\n');
fs.writeFileSync(F, s);
console.log('written; sheets: ' + (k - 1));
