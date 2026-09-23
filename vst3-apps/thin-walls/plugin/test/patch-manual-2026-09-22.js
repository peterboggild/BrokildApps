/*  Bring the handbook up to the 2026-09-22 build: five materials, a material
    per surface, breakable walls, door handles, the rebuilt late field, the
    windowed-sinc reader, 65 parameters and the current bench.

    Exact anchors, every one counted. NOTHING is written unless every edit
    matches exactly once - a half-applied patch leaves a document that is
    neither the old one nor the new one, and the overflow gate would then be
    measuring a file nobody wrote.

    Three new pages are INSERTED rather than crammed into page 8, and the
    folios after them are renumbered in DESCENDING order so the numbers cannot
    collide with each other on the way.                                       */

const fs = require("fs");
const P = "C:/Users/peter/b/ThinWalls/docs/manual/manual.html";
let s = fs.readFileSync(P, "utf8");
const miss = [];

function once(what, from, to) {
  const n = s.split(from).length - 1;
  if (n !== 1) { miss.push(what + "  (matched " + n + ", wanted 1)"); return; }
  s = s.replace(from, to);
}

/* ------------------------------------------------------------------ page 3
   Six groups became seven: Broken walls joined them, and Materials is now a
   grid of nine rather than a row of three.                                  */
once("page 3: the groups table",
`    <h3 class="display" style="font-size:11pt">The six groups, left to right</h3>
    <table class="tight" style="margin-top:3mm">
      <tr>
        <td>Doors</td><td style="color:var(--body)">one aperture each, and a button for the two ends</td>
        <td>Source</td><td style="color:var(--body)">everything about whichever of the four is selected</td>
        <td>View and files</td><td style="color:var(--body)">rays, defaults, save and load, and where you are standing</td>
      </tr>
      <tr>
        <td>Materials</td><td style="color:var(--body)">what each of the three rooms is lined with</td>
        <td>Levels and head</td><td style="color:var(--body)">the three trims, mix, output and ear spacing</td>
        <td>Test signal</td><td style="color:var(--body)">a file of your own, looped into the input</td>
      </tr>
    </table>`,
`    <h3 class="display" style="font-size:11pt">The seven groups</h3>
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
    </table>`);

/* ------------------------------------------------------------------ page 4
   Door handles and the movement keys are both about getting about, so they
   belong here rather than in a group of their own.                          */
once("page 4: the three notes become four",
`    <div class="cols" style="margin-top:4mm">
      <div>
        <h3 class="display">Turning your head</h3>
        <p style="margin-top:2.5mm; font-size:9.6pt">
          Facing is not decoration. The binaural rendering is driven by the
          direction each arrival comes from <b>relative to your head</b>, so
          turning on the spot moves every source around you &mdash; including
          the ones you cannot see, behind a shut door.
        </p>
      </div>
      <div>
        <h3 class="display">Where you may stand</h3>
        <p style="margin-top:2.5mm; font-size:9.6pt">
          Anywhere inside the three rooms. The area north-west of the living
          room is outside the flat and is solid; the plan will not let you drag
          there, and if automation sends you there the engine puts you back in
          the nearest room.
        </p>
      </div>
      <div>
        <h3 class="display">Doors as doors</h3>
        <p style="margin-top:2.5mm; font-size:9.6pt">
          A door has a hinge side, so a 30 per cent aperture is a real 27 cm
          strip beside the frame, not a uniformly thinner wall. The leaf swings
          into the room it is hung in and you can see it swing in the 3D view.
        </p>
      </div>
    </div>`,
`    <div class="cols" style="margin-top:4mm">
      <div>
        <h3 class="display">Turning your head</h3>
        <p style="margin-top:2.5mm; font-size:9.6pt">
          Facing is not decoration. The binaural rendering is driven by the
          direction each arrival comes from <b>relative to your head</b>, so
          turning on the spot moves every source around you &mdash; including
          the ones you cannot see, behind a shut door.
        </p>
        <h3 class="display" style="margin-top:4mm">Where you may stand</h3>
        <p style="margin-top:2.5mm; font-size:9.6pt">
          Anywhere inside the three rooms. The area north-west of the living
          room is outside the flat and is solid; the plan will not let you drag
          there, and if automation sends you there the engine puts you back in
          the nearest room.
        </p>
      </div>
      <div>
        <h3 class="display">Take hold of the handle</h3>
        <p style="margin-top:2.5mm; font-size:9.6pt">
          Every door in the 3D view has a handle. Walk up to one &mdash; within
          <b>2.5 m</b>, in front of you and not through a wall &mdash; and it
          lights up as the pointer crosses it. <b>Click</b> to slam it or throw
          it wide; <b>drag</b> to swing it to anywhere in between. Grabbing a
          handle is not a look, so the room does not spin while you do it.
        </p>
        <h3 class="display" style="margin-top:4mm">The keys always win</h3>
        <p style="margin-top:2.5mm; font-size:9.6pt">
          <span class="mono">W A S D</span> and the arrow keys walk and turn
          <em>even when a menu or a fader has the focus</em>. Set a material,
          then walk &mdash; the key never reaches the menu behind it. A key
          typed into a text field is still the field's.
        </p>
      </div>
    </div>`);

/* ------------------------------------------------- folios BEFORE the insert
   Descending, so 13 -> 16 before 12 -> 15, or the pass would find its own
   output and carry a page number twice.                                    */
[[13, 16], [12, 15], [11, 14], [10, 13], [9, 12]].forEach(function (pair) {
  const from = '<span class="n">' + String(pair[0]).padStart(2, "0") + "</span>";
  const to = '<span class="n">' + String(pair[1]).padStart(2, "0") + "</span>";
  const n = s.split(from).length - 1;
  if (n !== 1) { miss.push("folio " + pair[0] + " -> " + pair[1] + "  (matched " + n + ")"); return; }
  s = s.replace(from, to);
});

/* -------------------------------------------------- page 8, entirely rewritten
   The table of reverberation times stays here for the four ordinary
   materials; STUDIO has a page of its own, because a material whose whole
   point is that its decay does NOT move with frequency says nothing in a
   column of numbers next to four that do.                                   */
const PAGE8_OLD_START = `<!-- ======================================  8  ROOMS AND MATERIALS  ====== -->`;
const PAGE8_OLD_END = `  <div class="folio"><span>Rooms and materials</span><span class="n">08</span></div>
</section>`;
{
  const i = s.indexOf(PAGE8_OLD_START);
  const j = s.indexOf(PAGE8_OLD_END);
  if (i < 0 || j < 0 || j < i) { miss.push("page 8: could not find its own bounds"); }
  else {
    const page8 = `<!-- ======================================  8  ROOMS AND MATERIALS  ====== -->
<section class="page">
  <div class="pad">
    <p class="kick">Rooms and materials</p>
    <h2 class="display">What the walls are made of</h2>
    <p class="sub">Five surface treatments, and any of them on any surface of
      any room. These are published octave-band absorption coefficients, not
      voicing presets, and the reverberation time follows from them.</p>
    <hr class="rule" />

    <div class="cols">
      <div class="c-44">
        <h3 class="display">The five</h3>
        <table class="tight" style="margin-top:3mm">
          <tr><th>Material</th><th>What it is</th></tr>
          <tr><td>ABSORBING</td><td>heavy drapes, thick carpet, soft furniture</td></tr>
          <tr><td>FURNISHED</td><td>a lived-in living room: plaster, carpet, curtains</td></tr>
          <tr><td>PLASTER</td><td>bare plaster on brick, wooden floor</td></tr>
          <tr><td>TILED</td><td>glazed tiles, a little glass and wood</td></tr>
          <tr><td>STUDIO</td><td>treated: broadband absorbers, bass traps and diffusers &mdash; <b>page 10</b></td></tr>
        </table>

        <p style="margin-top:4mm; font-size:9.3pt">
          Absorption rises with frequency on the first four, which is why a
          live room here gets darker as it decays rather than simply longer.
          ABSORBING takes 80 % at 4 kHz and only 20 % at 125 Hz, so a dead room
          still has a bloom in the bottom octave &mdash; as real ones do.
          STUDIO is the one that does not, and that is its whole point.
        </p>

        <div class="box" style="margin-top:5mm">
          <h4>An open door is a hole</h4>
          <p>
            Sound that leaves through a doorway does not come back, so the
            engine counts an open door as a perfect absorber. The box room in
            bare plaster rings for <b>1.32 s</b> at 1 kHz with its doors shut
            and <b>0.58 s</b> with both wide open. You can hear a room change
            size by opening a door into it.
          </p>
        </div>
      </div>

      <div class="c-54" style="margin-left:auto">
        <h3 class="display">Reverberation time, doors shut</h3>
        <p style="margin-top:2mm; font-size:8.8pt; color:var(--dim)">
          Seconds, from Eyring's formula with ISO 9613-1 air absorption &mdash;
          the same arithmetic the engine runs at every change. STUDIO is on
          page 10.
        </p>
        <table class="tight" style="margin-top:3mm">
          <tr><th>Room</th><th>Material</th><th class="num">125 Hz</th><th class="num">500 Hz</th><th class="num">1 kHz</th><th class="num">4 kHz</th></tr>
          <tr><td>LARGE</td><td>ABSORBING</td><td class="num">0.51</td><td class="num">0.13</td><td class="num">0.09</td><td class="num">0.07</td></tr>
          <tr><td>LARGE</td><td>FURNISHED</td><td class="num">1.07</td><td class="num">0.65</td><td class="num">0.51</td><td class="num">0.34</td></tr>
          <tr><td>LARGE</td><td>PLASTER</td><td class="num">3.57</td><td class="num">2.16</td><td class="num">1.77</td><td class="num">1.17</td></tr>
          <tr><td>LARGE</td><td>TILED</td><td class="num">5.24</td><td class="num">5.18</td><td class="num">3.42</td><td class="num">2.07</td></tr>
          <tr><td>SMALL</td><td>ABSORBING</td><td class="num">0.38</td><td class="num">0.10</td><td class="num">0.07</td><td class="num">0.06</td></tr>
          <tr><td>SMALL</td><td>FURNISHED</td><td class="num">0.78</td><td class="num">0.48</td><td class="num">0.38</td><td class="num">0.25</td></tr>
          <tr><td>SMALL</td><td>PLASTER</td><td class="num">2.50</td><td class="num">1.59</td><td class="num">1.32</td><td class="num">0.90</td></tr>
          <tr><td>SMALL</td><td>TILED</td><td class="num">3.59</td><td class="num">3.83</td><td class="num">2.57</td><td class="num">1.64</td></tr>
          <tr><td>GIANT</td><td>ABSORBING</td><td class="num">0.92</td><td class="num">0.23</td><td class="num">0.15</td><td class="num">0.13</td></tr>
          <tr><td>GIANT</td><td>FURNISHED</td><td class="num">1.94</td><td class="num">1.16</td><td class="num">0.90</td><td class="num">0.58</td></tr>
          <tr><td>GIANT</td><td>PLASTER</td><td class="num">6.58</td><td class="num">3.80</td><td class="num">3.06</td><td class="num">1.88</td></tr>
          <tr><td>GIANT</td><td>TILED</td><td class="num">9.78</td><td class="num">8.90</td><td class="num">5.75</td><td class="num">3.06</td></tr>
        </table>

        <p style="margin-top:4mm; font-size:9.3pt">
          Each door slider is an aperture, continuous from shut to wide open;
          the button beside it does the two ends. Clicking a door in the plan
          does the same thing, dragging along one sets it part open, and in the
          3D view you can simply take hold of its handle.
        </p>
      </div>
    </div>
  </div>
  <div class="folio"><span>Rooms and materials</span><span class="n">08</span></div>
</section>


<!-- =====================================  9  A MATERIAL PER SURFACE  ==== -->
<section class="page">
  <div class="pad">
    <p class="kick">A material per surface</p>
    <h2 class="display">Walls, floor and ceiling,<br>each its own</h2>
    <p class="sub">Nine choices, three for each room. Floor and ceiling start
      at AS WALLS, so a room you never touch is exactly the room it always
      was.</p>
    <hr class="rule" />

    <div class="cols">
      <div class="c-54">
        <p>
          A real room is not made of one thing. Between a source standing on
          the floor and a listener standing on the floor, the <b>first two
          reflections to arrive are the one off the floor and the one off the
          ceiling</b> &mdash; they are the shortest detours there are. They
          arrive within a few milliseconds of the direct sound, which is
          exactly the window the ear uses to decide how big a room is and
          where in it you are.
        </p>
        <p>
          So the floor and the ceiling are worth their own choices, and they
          are the two a real studio treats first.
        </p>

        <div class="box" style="margin-top:5mm">
          <h4>A carpet is the most lopsided absorber in the flat</h4>
          <p>
            Thick carpet takes almost nothing below 250 Hz and a great deal
            above 2 kHz. Setting <em>floor</em> to ABSORBING therefore kills
            the floor bounce and the flutter with it while leaving the bottom
            of the room exactly where it was &mdash; which is what laying a rug
            in a hard room really does, and why it is the first thing anyone
            tries.
          </p>
        </div>

        <div class="box" style="margin-top:5mm">
          <h4>A cloud over the desk</h4>
          <p>
            <em>Ceiling</em> set to ABSORBING is the cloud every control room
            has above the listening position. It is the opposite trade to a
            carpet: it takes the vertical flutter out and leaves the room's
            width and depth ringing.
          </p>
        </div>
      </div>

      <div class="c-40" style="margin-left:auto">
        <figure>
          <img class="shot" src="img/ctrl-materials.jpg" alt="" />
          <figcaption><b>Nine selectors.</b> A row per room, a column per
            surface. AS WALLS is not a fifth material, it is a pointer: that
            surface follows whatever the walls are set to.</figcaption>
        </figure>

        <hr class="rule thin" style="margin-top:6mm" />

        <h3 class="display" style="font-size:11.5pt">What it is worth</h3>
        <p style="margin-top:2.5mm; font-size:9.3pt">
          The living room in bare plaster rings for <b>2.71 s</b> at 250 Hz and
          <b>1.77 s</b> at 1 kHz. Leave the walls in plaster and put a carpet
          down and a cloud up &mdash; two menu changes &mdash; and the same
          room measures:
        </p>
        <table class="tight" style="margin-top:2mm">
          <tr><th>Surfaces</th><th class="num">250&nbsp;Hz</th><th class="num">1&nbsp;kHz</th><th class="num">4&nbsp;kHz</th></tr>
          <tr><td>plaster throughout</td><td class="num">2.71</td><td class="num">1.77</td><td class="num">1.17</td></tr>
          <tr><td>+ carpet and cloud</td><td class="num">0.39</td><td class="num">0.20</td><td class="num">0.17</td></tr>
        </table>
        <p style="margin-top:2.5mm; font-size:8.8pt; color:var(--dim)">
          Read off the panel's own readout with the engine running. Two of the
          six surfaces carry most of the room.
        </p>
      </div>
    </div>
  </div>
  <div class="folio"><span>A material per surface</span><span class="n">09</span></div>
</section>


<!-- =============================================  10  THE STUDIO  ======= -->
<section class="page">
  <div class="pad">
    <p class="kick">The fifth material</p>
    <h2 class="display">STUDIO: treated,<br>and still live</h2>
    <p class="sub">A room whose decay does not move with frequency, and whose
      treatment is diffusion rather than absorption.</p>
    <hr class="rule" />

    <div class="cols">
      <div class="c-54">
        <p>
          The other four materials are measured surfaces. STUDIO is not: its
          absorption was <b>solved backwards from Eyring's formula</b> for the
          one property a control room is built to have &mdash; a reverberation
          time that is the same at the bottom of the band as at the top.
        </p>
        <p>
          What comes out is <b>constant absorption near a quarter</b>, with a
          little relief at the very top to pay for the air absorption that
          takes the top octave anyway. That is not a coincidence: broadband
          absorbers plus bass traps are exactly what it takes to flatten a
          real room, and this is the coefficient curve they add up to.
        </p>

        <table class="tight" style="margin-top:4mm">
          <tr><th>Room as a STUDIO</th><th class="num">125&nbsp;Hz</th><th class="num">500&nbsp;Hz</th><th class="num">1&nbsp;kHz</th><th class="num">4&nbsp;kHz</th><th class="num">spread</th></tr>
          <tr><td>LARGE &middot; living room</td><td class="num">0.39</td><td class="num">0.40</td><td class="num">0.40</td><td class="num">0.41</td><td class="num">1.04&times;</td></tr>
          <tr><td>SMALL &middot; box room</td><td class="num">0.29</td><td class="num">0.29</td><td class="num">0.30</td><td class="num">0.31</td><td class="num">1.06&times;</td></tr>
          <tr><td>GIANT &middot; hall</td><td class="num">0.71</td><td class="num">0.71</td><td class="num">0.71</td><td class="num">0.71</td><td class="num">1.00&times;</td></tr>
          <tr><td>GIANT &middot; the same hall, TILED</td><td class="num">9.78</td><td class="num">8.90</td><td class="num">5.75</td><td class="num">3.06</td><td class="num">3.09&times;</td></tr>
        </table>
        <p style="margin-top:2.5mm; font-size:8.8pt; color:var(--dim)">
          Spread is the ratio of the longest of 250 Hz, 1 kHz and 4 kHz to the
          shortest, measured on the rendered audio. The hall as a studio is
          <b style="color:var(--brass)">1.00&times;</b>: flat to the digit.
        </p>
      </div>

      <div class="c-40" style="margin-left:auto">
        <div class="box cool">
          <h4>Diffusion, not absorption</h4>
          <p>
            The other four materials return a reflection in the specular
            direction and take a bite out of it. A studio's diffusers do
            something quite different: they <b>keep the energy and throw away
            the direction</b>.
          </p>
          <p>
            So what comes back off a treated wall is still in the room &mdash;
            it just no longer arrives as a single sharp image, and it no longer
            bounces between two parallel walls in step with itself. Measured:
            the specular reflections carry <b>17.6 %</b> of what the same room
            in plaster returns, and the rest is scattered into the room rather
            than absorbed out of it.
          </p>
        </div>

        <div class="box" style="margin-top:6mm">
          <h4>Which is why it is not dead</h4>
          <p>
            A hall that rings for 0.71 s is a live room, not an anechoic
            chamber. You can still hear that it is big. What you cannot hear is
            a flutter echo, or the bottom end arriving late: the early response
            of a centred source measures <b>1.1 dB</b> uneven across the middle
            of the band, where the same room in plaster measures
            <b>2.4 dB</b>.
          </p>
        </div>

        <p style="margin-top:6mm; font-size:9.3pt">
          STUDIO also splays its walls very slightly, which is the same trick
          the next page gives you by hand &mdash; and the two add up.
        </p>
      </div>
    </div>
  </div>
  <div class="folio"><span>The studio</span><span class="n">10</span></div>
</section>


<!-- =========================================  11  BREAKING A WALL  ====== -->
<section class="page">
  <div class="pad">
    <p class="kick">Breaking a wall</p>
    <h2 class="display">The rooms are not<br>shoeboxes any more</h2>
    <p class="sub">Two walls of every room can be split at a movable point and
      that point pushed out, so the room stops being rectangular. Drag the
      diamond in the plan, or use the two faders.</p>
    <hr class="rule" />

    <div class="grid2 tight-plates" style="margin-top:1mm">
      <figure>
        <img class="shot" src="img/plan-folded.jpg" alt="" />
        <figcaption><b>Four walls broken.</b> The living room's west and south
          walls and the hall's east and north are splayed outward; the diamond
          on each is the break, draggable along the wall and out of it. The
          room's area, its volume and its reverberation time all follow
          &mdash; the hall's floor goes from 108.0 to <b>113.7 m&sup2;</b>.</figcaption>
      </figure>
      <figure>
        <img class="shot" src="img/ctrl-folds.jpg" alt="" />
        <figcaption><b>Two faders per wall.</b> The first is how far the break
          is pushed, &plusmn;0.6 m, reading out in metres and saying
          <em>flat</em> at the centre. The second is where along the wall it
          sits. The two walls offered are always the ones with no doorway in
          them, so a break can never collide with a door.</figcaption>
      </figure>
    </div>

    <div class="cols" style="margin-top:5mm">
      <div>
        <div class="ctl">
          <h3>Push it out</h3>
          <p class="what">Splay</p>
          <p>
            Two panels that are no longer parallel to the wall facing them
            cannot sustain a flutter between them, and they spread a reflection
            over a range of angles instead of returning it whole. Off centre is
            worth more than centred: two panels of different lengths scatter
            over a wider band than two of the same length.
          </p>
        </div>
      </div>
      <div>
        <div class="ctl" style="border-left-color:#a5453a">
          <h3 style="color:#c8695c">Do not pull it in</h3>
          <p class="what">Concave focuses</p>
          <p>
            Pushed the other way the wall bends <b>into</b> the room, and a
            concave surface gathers sound to a point instead of spreading it.
            That is worse than leaving the wall flat &mdash; real rooms are
            splayed outward, never inward. The plan says so when you do it.
            It is offered because it is audible and instructive, not because
            it is good.
          </p>
        </div>
      </div>
      <div>
        <div class="ctl">
          <h3>What it really does</h3>
          <p class="what">Honestly</p>
          <p>
            In this model the splay acts on the <b>early reflections</b>, not
            on the room's standing waves: the late field is statistical rather
            than modal. Above the Schroeder frequency &mdash; about 72 Hz in
            the hall as a studio &mdash; that is where splay does its audible
            work in a real room too. Below it, absorption is what helps.
          </p>
        </div>
      </div>
    </div>

    <div class="box" style="margin-top:1mm">
      <h4>A room that is not a box needs a different search</h4>
      <p>
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
      </p>
    </div>
  </div>
  <div class="folio"><span>Breaking a wall</span><span class="n">11</span></div>
</section>

`;
    s = s.slice(0, i) + page8 + s.slice(j + PAGE8_OLD_END.length + 1);
  }
}

/* ----------------------------------------------- the late field, rebuilt */
once("late field: the reverberator paragraph",
`          Each room runs its own sixteen-line reverberator, with delays taken
          from that room's mean free path and a decay per octave band from its
          materials, its open doors and the air in it. The rooms are coupled
          through the doors and walls with the energy balance of coupled-room
          theory, so the hall's long tail genuinely arrives in the living room
          and gives you the double slope a real flat has.`,
`          Each room runs its own sixteen-line reverberator, with delays taken
          from that room's mean free path and a decay per octave band from its
          materials, its open doors and the air in it. The rooms are coupled
          through the doors and walls with the energy balance of coupled-room
          theory, so the hall's long tail genuinely arrives in the living room
          and gives you the double slope a real flat has.
        </p>
        <p>
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
          decays now lands within a few per cent of Eyring.`);

once("late field: the DRR paragraph",
`          have for distance &mdash; comes out right by construction. Walk twice
          as far away and it falls by <b>6 dB</b>; walk in to half the distance
          and it rises by <b>6 dB</b>. At the critical distance, where direct
          and reverberant are equal, it reads within <b>3.5 dB</b> of zero for
          every material in the flat.`,
`          have for distance &mdash; comes out right by construction. Walk twice
          as far away and it falls by <b>6.2 to 7.0 dB</b>; walk in to half the
          distance and it rises by <b>3.6 to 5.5</b>. At the critical distance,
          where direct and reverberant are equal, it reads between
          <b>&minus;1.3 and &minus;0.4 dB</b> against the <b>+0.9 dB</b> the
          head's own frontal gain accounts for &mdash; for every one of the
          five materials.`);

once("late field: the double slope numbers",
`          box room with it lined in drapes and the hall in tiles: its own field
          is <b>19 dB down inside 40 ms</b> and gone, and then the hall's answer
          comes back through the doorway at the hall's own five and a half
          seconds. The decay falls off a cliff and then keeps going &mdash;`,
`          box room with it lined in drapes and the hall in tiles: its own field
          is <b>16 dB down inside 40 ms</b> and gone, and then the hall's answer
          comes back through the doorway at the hall's own <b>5.8 seconds</b>.
          The decay falls off a cliff and then keeps going &mdash;`);

/* ---------------------------------------------------- page 15, automation */
once("test signal page: the parameter count and the motion figure",
`          <p>
            All <b>47</b> parameters are host parameters, including every
            source position and your own. Walking is recorded as automation, so
            a move you made by hand can be replayed exactly &mdash; and a source
            can be given a path by drawing one in your DAW.
          </p>
          <p>
            A source moving at 2 m/s on a held tone keeps everything above
            6 kHz <b>81 dB down</b>: the geometry changes continuously and
            nothing clicks.
          </p>`,
`          <p>
            All <b>65</b> parameters are host parameters, including every
            source position, every surface, every broken wall and your own
            position. Walking is recorded as automation, so a move you made by
            hand can be replayed exactly &mdash; and a source can be given a
            path by drawing one in your DAW.
          </p>
          <p>
            A source moving at 2 m/s on a held tone keeps everything above
            6 kHz <b>84 dB down</b>, and a wall moved under a sounding note
            <b>92 dB down</b>: the geometry changes continuously and nothing
            clicks.
          </p>`);

once("patches: what a patch holds",
`          patch holds the rooms, the doors, all four sources and the levels
          &mdash; the whole arrangement. <em>Default</em> puts every parameter
          back; the apartment itself never changes.`,
`          patch holds every surface, every broken wall, the doors, all four
          sources and the levels &mdash; the whole arrangement. <em>Default</em>
          puts every parameter back; the floor plan itself never changes.`);

/* -------------------------------------------------------- specifications */
once("specifications: the lede",
`    <p class="sub">The bench renders real audio and measures it: 67 checks, all
      clear. These are its numbers, not estimates.</p>`,
`    <p class="sub">The bench renders real audio and measures it: 90 checks, all
      clear, beside 73 that drive the panel. These are their numbers, not
      estimates.</p>`);

once("specifications: the measured table",
`          <tr><td>Interaural delay at 90&deg;</td><td>&minus;686 &micro;s (Woodworth's formula gives 666)</td></tr>
          <tr><td>Front against back</td><td>differ in spectral shape at 4 and 8 kHz, not in level</td></tr>
          <tr><td>Level with distance</td><td>6 dB per doubling</td></tr>
          <tr><td>Near field</td><td>interaural level difference grows below 1 m, from the geometry</td></tr>
          <tr><td>Air absorption</td><td>8 kHz costs 1.0 dB more than 250 Hz over 11 m (ISO 9613-1: 0.8)</td></tr>
          <tr><td>Reverberation time</td><td>against Eyring in all twelve room and material cases</td></tr>
          <tr><td>Direct-to-reverberant</td><td>within 3.5 dB of zero at the critical distance for every material; +6 / &minus;6 at half and double</td></tr>
          <tr><td>Shutting a door</td><td>&minus;20 dB at 1 kHz, and 4 kHz loses 8 dB more than 250 Hz</td></tr>
          <tr><td>Party wall alone</td><td>24 dB below the same distance in your own room</td></tr>
          <tr><td>Moving a source</td><td>2 m/s on a pure tone: everything above 6 kHz stays 81 dB down</td></tr>
          <tr><td>MIX at 0</td><td>bit-exact passthrough</td></tr>`,
`          <tr><td>Interaural delay at 90&deg;</td><td>&minus;708 &micro;s (Woodworth's formula gives 666)</td></tr>
          <tr><td>Front against back</td><td>5.0 dB apart in spectral shape at 4 and 8 kHz, 1.0 dB in level &mdash; the pinna</td></tr>
          <tr><td>Level with distance</td><td>6.1 dB per doubling</td></tr>
          <tr><td>Near field</td><td>the interaural level difference grows 4.6 dB by 0.3 m, out of the geometry</td></tr>
          <tr><td>Direct path</td><td>within a tenth of a decibel of the physics from 10 Hz to 16 kHz, wherever a source stands</td></tr>
          <tr><td>Air absorption</td><td>8 kHz costs 0.73 dB more than 250 Hz over 11 m (ISO 9613-1: 0.78)</td></tr>
          <tr><td>Reverberation time</td><td>against Eyring in all fifteen room and material cases</td></tr>
          <tr><td>Spectral roughness of the tail</td><td>5.6 to 6.6 dB, against the 5.57 dB of a perfectly diffuse field</td></tr>
          <tr><td>Direct-to-reverberant</td><td>&minus;1.3 to &minus;0.4 dB at the critical distance against the head's own +0.9; +3.6 / &minus;7.0 at half and double</td></tr>
          <tr><td>Shutting a door</td><td>&minus;23 dB at 1 kHz, and 4 kHz loses 12 dB more than 250 Hz</td></tr>
          <tr><td>Party wall and shut door</td><td>31 dB below the same distance in your own room</td></tr>
          <tr><td>A studio's decay</td><td>flat to 1.00&times; across 250 Hz, 1 kHz and 4 kHz in the hall, where TILED spreads 3.09&times;</td></tr>
          <tr><td>A wall broken by 2 mm</td><td>the general path search reproduces the exact shoebox to 0.09 dB</td></tr>
          <tr><td>Moving a source</td><td>2 m/s on a pure tone: everything above 6 kHz stays 84 dB down</td></tr>
          <tr><td>MIX at 0</td><td>bit-exact passthrough</td></tr>`);

once("specifications: the product table",
`          <tr><td>Parameters</td><td>47, all automatable</td></tr>`,
`          <tr><td>Parameters</td><td>65, all automatable</td></tr>
          <tr><td>Materials</td><td>Five, and one for each of the three surfaces of each room &mdash; nine choices</td></tr>
          <tr><td>Room shape</td><td>Two walls of every room can be broken at a movable point and pushed &plusmn;0.6 m</td></tr>`);

once("specifications: reflections and reverberator",
`          <tr><td>Reflections</td><td>image sources to second order, up to 24 per room</td></tr>
          <tr><td>Reverberator</td><td>one 16-line feedback delay network per room, coupled through the doors and walls</td></tr>
          <tr><td>CPU</td><td>one source about 11 % of a core at 48 kHz; four with every door open, 28 %</td></tr>`,
`          <tr><td>Reflections</td><td>image sources to second order; a general surface-sequence search once a wall is broken</td></tr>
          <tr><td>Reverberator</td><td>one 16-line feedback delay network per room, allpass diffusers inside every line, coupled through the doors and walls</td></tr>
          <tr><td>Delay reader</td><td>16-tap windowed sinc, so a source may stand anywhere without losing the top octave</td></tr>
          <tr><td>CPU</td><td>one source about 17 % of a core at 48 kHz; four with every door open, 40 %</td></tr>`);

/* ------------------------------------------------------------- back page */
once("back page: the fixed plan",
`          <b>The floor plan is fixed.</b> One apartment, three rooms, three
          doors. You can change everything in it and nothing about it. A second
          plan would be a different instrument, and this one is tuned against
          the geometry it has.`,
`          <b>The floor plan is fixed.</b> One apartment, three rooms, three
          doors. Two walls of each room can be broken and pushed, so the rooms
          are not stuck as boxes &mdash; but where the rooms are, and which
          doors join them, you cannot change. A second plan would be a
          different instrument.`);

once("back page: the second-order note",
`          <b>Reflections are specular and go to second order.</b> There is no
          scattering off the furniture and no diffraction round the edges of the
          reflections themselves. Beyond the second bounce the room's own
          reverberator takes over, which is where that energy belongs anyway.`,
`          <b>Reflections are specular and go to second order.</b> Only STUDIO
          scatters, and there is no diffraction round the edges of the
          reflections themselves. Beyond the second bounce the room's own
          reverberator takes over, which is where that energy belongs anyway.`);

once("back page: add the splay limit",
`        <p style="font-size:9.8pt">
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
          <b>Breaking a wall moves the early reflections, not the modes.</b>
          The late field here is statistical, so splaying a wall does not
          redistribute a room's standing waves the way it would in a modal
          model. Above the Schroeder frequency that is where splay works in a
          real room too; below it, absorption is what helps.
        </p>`);

/* --------------------------------------------- a comment that went stale */
once("css note: the status strip's own size",
`/*  The status strip is 410 x 26 - about sixteen to one. Capping THAT by height
    at 16mm would make it 260mm wide. A strip is capped by width; everything
    else on these pages is capped by height. */`,
`/*  The status strip is 1400 x 23 - about sixty to one. Capping THAT by height
    at 16mm would make it a metre wide. A strip is capped by width; everything
    else on these pages is capped by height. */`);

/* ------------------------------------------------------------------ write */
if (miss.length) {
  console.error("NOTHING WRITTEN. " + miss.length + " anchor(s) did not match:");
  miss.forEach(function (m) { console.error("  - " + m); });
  process.exit(1);
}
fs.writeFileSync(P, s);
const pages = s.split('<section class="page">').length - 1;
console.log("manual.html updated - " + pages + " pages");
