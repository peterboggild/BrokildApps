/*  B2311.67 — two of Peter's reports, 2026-09-02.

    (1) "the BWFX symbol in 67 is on top of text, can you figure out less
        overlap". Measured on the live panel: the globe sits at (1238,22) and
        the right-hand readout column runs under it —

            rOrd  "72"      at 1255,22
            rSig  "30:70"   at 1236,35
            rGR   "0.0 dB"  at 1228,49

        plus the "cut" / "habit" / "ceiling" labels, whose 240 px rows reach to
        x=1268. The whole top-right corner plate is 265x75 and full. Probing
        candidate boxes on the live page, right/below-the-plate (1238,86) came
        back CLEAR, as did mid-height and left-of-the-readouts; top-left is the
        title block and bottom-right is the "still" button. Below the plate is
        the smallest honest move and keeps the globe where a harness button
        belongs.

    (2) "have the slider go 'too far' so that if the mouse wheel is scrolled
        too far, there is no crosssection between the 2D plane and the object;
        i.e. no sound".

        This one needs saying plainly, because the mathematics does NOT give it
        for free: a cut-and-project quasicrystal is infinite and dense, so the
        acceptance window always holds points however far the cut is moved.
        There is no depth at which an ideal tiling stops intersecting a plane.

        What DOES have an edge is the specimen. B2311.67 is a fragment
        recovered from a vault, not an unbounded tiling, and a fragment has
        faces. So the body is given a finite extent in w, and past it the plane
        is cutting the vault rather than the object. TRAVERSE already spans
        +-12; the tissue now occupies the middle +-8.5 with a soft face out to
        10.4, so roughly the middle 70 % of the wheel is inside the object and
        winding to either end leaves it entirely.
*/
const fs = require('fs');
const files = {};
function load (p) {
  const s = fs.readFileSync(p, 'utf8');
  files[p] = { s, nl: s.indexOf(String.fromCharCode(13,10)) >= 0
                      ? String.fromCharCode(13,10) : String.fromCharCode(10) };
}
const miss = [];
function rep (p, find, into) {
  const f = files[p];
  const n = f.s.split(find).length - 1;
  if (n !== 1) { miss.push(p.split('/').pop() + ' [' + n + 'x] ' + find.split(f.nl)[0].slice(0, 58)); return; }
  f.s = f.s.replace(find, into);
}
const J = (p, a) => a.join(files[p].nl);

const UI  = 'C:/Users/peter/b/ArtefactB2311_67/Source/ui/ui.html';
const ENG = 'C:/Users/peter/b/ArtefactB2311_67/Source/Engine.cpp';
load(UI); load(ENG);

//  (1) the globe clears the readout column
rep(UI,
    '  #globe{position:absolute;right:14px;top:22px;width:28px;height:28px;pointer-events:auto;cursor:pointer;opacity:.55}',
    J(UI, [
'  /*  BELOW the top-right corner plate, not on it. The plate is 265x75 and',
'      carries the cut/habit/ceiling readouts right up to its edge; at top:22',
'      the globe sat on all three of them. Measured clear at top:88. */',
'  #globe{position:absolute;right:14px;top:88px;width:28px;height:28px;pointer-events:auto;cursor:pointer;opacity:.55}']));

//  (2) the fragment has faces
rep(ENG,
    '    const float lvl    = (float) (2.2 * (double) p.level * (double) p.level) * habTrim;',
    J(ENG, [
'    /*  THE FRAGMENT HAS FACES, and past them the plane cuts nothing.',
'',
'        An ideal cut-and-project tiling is infinite and dense: the acceptance',
'        window holds points at every depth, so no amount of TRAVERSE would',
'        ever empty the section. That is true of the mathematics and false of',
'        the object — B2311.67 came out of a vault as a fragment, and a',
'        fragment ends. The tissue occupies the middle of the traverse and the',
'        outer reaches are the vault: wind the cut far enough either way and',
'        there is no cross-section, and therefore nothing to hear.',
'',
'        The face is a raised cosine so the object thins out rather than',
'        switching off, and it is exactly zero beyond it. */',
'    const double tauAbs = std::abs (tau);',
'    const double faceIn = 8.5, faceOut = 10.4;',
'    double presence = 1.0;',
'    if (tauAbs > faceIn)',
'        presence = tauAbs >= faceOut ? 0.0',
'                 : 0.5 + 0.5 * std::cos (PI * (tauAbs - faceIn) / (faceOut - faceIn));',
'    presencePub.store ((float) presence, std::memory_order_relaxed);',
'',
'    const float lvl    = (float) (2.2 * (double) p.level * (double) p.level',
'                                  * presence) * habTrim;']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
for (const p of Object.keys(files)) fs.writeFileSync(p, files[p].s);
console.log('patched: ui.html globe, Engine.cpp body faces');
