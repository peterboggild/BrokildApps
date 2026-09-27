/*  B2311.67 260902.2 — the picture and the sound agree, and the wheel has no end.

    Peter, 2026-09-02:
      "the graphics still show when 'out of range'. Id like the graphics to
       deconstruct to nothing, so that only when there is no graphics, there is
       no sound. There can be a pulsing dot in the middle when out of range."
      "traverse should not have a limit. THe slider may have that, but scolling
       with the mouse just allow you to keep going, with nothing happening. In
       principle something could occur at much bigger values of traverse, as an
       easter egg, undocumented."

    Three things:

    (1) PRESENCE reaches the page, so the body deconstructs on exactly the
        number that silences it. One value, sent from the engine, rather than
        the page recomputing the face law and the two drifting apart.

    (2) THE WHEEL SPILLS PAST THE SLIDER. `tauUi` has been sitting in Engine.h
        since the first build, commented "the wheel's contribution", and
        nothing ever called setTravelDelta. The slider keeps its +-12; when the
        wheel runs off either end the excess accumulates there instead, without
        limit, and unwinds first on the way back so returning is not a long
        scroll against a pinned slider.

    (3) AND THERE IS SOMETHING OUT THERE. B2311.67 was recovered from a vault
        FIELD of sixty-seven, each at the centre of its own void — so the field
        is in the fourth dimension too, and far enough along w the cut meets a
        neighbour. Undocumented, narrow, and quieter than home.
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
  if (n !== 1) { miss.push(p.split('/').pop() + ' [' + n + 'x] ' + find.split(f.nl)[0].slice(0, 56)); return; }
  f.s = f.s.replace(find, into);
}
const J = (p, a) => a.join(files[p].nl);

const UI  = 'C:/Users/peter/b/ArtefactB2311_67/Source/ui/ui.html';
const ENG = 'C:/Users/peter/b/ArtefactB2311_67/Source/Engine.cpp';
const PRC = 'C:/Users/peter/b/ArtefactB2311_67/Source/PluginProcessor.cpp';
load(UI); load(ENG); load(PRC);

//  ------------------------------------------------ (3) the field of vaults
rep(ENG,
    J(ENG, [
'    const double tauAbs = std::abs (tau);',
'    const double faceIn = 8.5, faceOut = 10.4;',
'    double presence = 1.0;',
'    if (tauAbs > faceIn)',
'        presence = tauAbs >= faceOut ? 0.0',
'                 : 0.5 + 0.5 * std::cos (PI * (tauAbs - faceIn) / (faceOut - faceIn));']),
    J(ENG, [
'    const double tauAbs = std::abs (tau);',
'    auto face = [] (double d, double in, double out)',
'    {',
'        d = std::abs (d);',
'        if (d <= in)  return 1.0;',
'        if (d >= out) return 0.0;',
'        return 0.5 + 0.5 * std::cos (PI * (d - in) / (out - in));',
'    };',
'    double presence = face (tauAbs, 8.5, 10.4);',
'    /*  THE FIELD. Sixty-seven vaults were found at the Sabik Terminator, each',
'        at the centre of its own void, and the field is not only a thing on the',
'        ground: carry the cut far enough along w and it meets a neighbour. They',
'        are narrow and quieter than home and nothing says where they are. */',
'    presence = std::max (presence, 0.55 * face (tauAbs -  67.0, 2.2, 4.2));',
'    presence = std::max (presence, 0.30 * face (tauAbs - 134.0, 1.4, 2.8));']));

//  --------------------------------------------- (1) presence reaches the page
rep(PRC,
    '    else if (k == "clearArrests") { engine.clearArrests(); engine.service(); }',
    J(PRC, [
'    else if (k == "clearArrests") { engine.clearArrests(); engine.service(); }',
'    /*  The wheel, past the ends of the slider. TRAVERSE stays a host parameter',
'        on nought to one; this is the free travel beyond it, which is not',
'        automatable and not saved, because it is a place you went and not a',
'        setting you chose. */',
'    else if (k == "twheel") { engine.setTravelDelta ((double) o->getProperty ("d")); }']));

//  ------------------------------------------------------------- the UI side
rep(UI,
    'NB.on("body", p => {',
    J(UI, [
'NB.on("body", p => {',
'  /*  How much of the fragment the cut is passing through. The page does NOT',
'      recompute the face law — one number, from the engine, so the picture',
'      cannot disagree with the sound about whether there is an object here. */',
'  if (p.pres !== undefined) S.pres = p.pres;']));

//  the tile shader takes it, and the body comes apart
rep(UI,
    J(UI, [
'uniform vec2 uCentre; uniform float uScale; uniform vec2 uRes;',
'out vec2 vPerp; out vec3 vFOA; out vec2 vUV; out vec2 vAxis; out vec2 vAxis2;',
'out vec2 vCent; out vec2 vWorld;',
'void main(){',
'  vec2 p = (aPos - uCentre) * uScale;']),
    J(UI, [
'uniform vec2 uCentre; uniform float uScale; uniform vec2 uRes;',
'uniform float uPresence;',
'out vec2 vPerp; out vec3 vFOA; out vec2 vUV; out vec2 vAxis; out vec2 vAxis2;',
'out vec2 vCent; out vec2 vWorld;',
'/*  DECONSTRUCTION. The hash is taken from the tile CENTRE, which every vertex',
'    of a tile shares, so a tile leaves whole rather than tearing. Survivors',
'    also shrink towards their own centres, so the body comes apart into',
'    scattered plates and then into nothing — and nothing is exactly where the',
'    sound has gone too, because both are driven by the same number. */',
'float h21(vec2 q){ return fract(sin(dot(q, vec2(127.1, 311.7))) * 43758.5453); }',
'void main(){',
'  if (uPresence < 0.999) {',
'    if (uPresence <= 0.0005 || h21(aCent) > uPresence) {',
'      gl_Position = vec4(2.0, 2.0, 2.0, 1.0);   // off the clip volume: gone',
'      vPerp = aPerp; vFOA = aFOA; vUV = aUV; vAxis = aAxis; vAxis2 = aAxis2;',
'      vCent = aCent; vWorld = aPos;',
'      return;',
'    }',
'  }',
'  vec2 shrunk = mix(aCent, aPos, clamp(uPresence * 1.4, 0.0, 1.0));',
'  vec2 p = (shrunk - uCentre) * uScale;']));

rep(UI, '  vWorld = aPos; vPerp = aPerp; vFOA = aFOA; vUV = aUV;',
        '  vWorld = shrunk; vPerp = aPerp; vFOA = aFOA; vUV = aUV;');

//  the skeleton fades with it
rep(UI,
    J(UI, [
'uniform vec2 uCentre; uniform float uScale; uniform vec2 uRes;',
'out vec4 vCol;',
'void main(){',
'  vec2 p = (aPos - uCentre) * uScale;',
'  vCol = aCol;']),
    J(UI, [
'uniform vec2 uCentre; uniform float uScale; uniform vec2 uRes;',
'uniform float uPresence;',
'out vec4 vCol;',
'void main(){',
'  vec2 p = (aPos - uCentre) * uScale;',
'  /*  The skeleton has no per-line centre to hash, and hashing the endpoints',
'      would tear every line in half, so it fades instead of coming apart. */',
'  vCol = vec4(aCol.rgb, aCol.a * uPresence);']));

//  feed both programs
rep(UI,
    '  gl.uniform1f(gl.getUniformLocation(PROG.tile, "uEdge"), 15.4);',
    J(UI, [
'  gl.uniform1f(gl.getUniformLocation(PROG.tile, "uEdge"), 15.4);',
'  gl.uniform1f(gl.getUniformLocation(PROG.tile, "uPresence"), S.pres === undefined ? 1 : S.pres);']));

rep(UI,
    J(UI, [
'  gl.uniform2f(gl.getUniformLocation(PROG.line, "uRes"), W, H);',
'  gl.bindVertexArray(VAO.line);']),
    J(UI, [
'  gl.uniform2f(gl.getUniformLocation(PROG.line, "uRes"), W, H);',
'  gl.uniform1f(gl.getUniformLocation(PROG.line, "uPresence"), S.pres === undefined ? 1 : S.pres);',
'  gl.bindVertexArray(VAO.line);']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
for (const p of Object.keys(files)) fs.writeFileSync(p, files[p].s);
console.log('patched: Engine.cpp field, PluginProcessor.cpp twheel, ui.html shaders + presence');
