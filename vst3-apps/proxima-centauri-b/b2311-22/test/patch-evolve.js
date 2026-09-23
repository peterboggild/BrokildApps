/*  B2311.22 260902.1 — THE OBJECT KEEPS WHAT PLAYING PUTS INTO IT.

    Peter, 2026-09-02: more evolution over time, determined by when and what
    notes are played; touching and playing should change the object and not
    only light it up; and the push through the 2D projection of the 4D body
    (the wheel / WINCH) should be striking.

    Measured first, on 260830.2:
      - a HELD note's colour wanders +-8 % over thirty seconds and goes
        nowhere: 955 -> 1015 -> 933 -> 971 -> 1018 -> 1035 Hz.
      - a note displaces the section by 0.026 out of a range of 2 (1.3 %),
        and half of even that is recalled within 3 s.
      - the WINCH across its whole travel spans 714..1037 Hz, half an octave,
        and most of what changes is LEVEL (0.004 .. 0.013) rather than colour.
      - and being played leaves the object differing from a fresh one almost
        entirely in phase: 1480 Hz against 1493 Hz of colour.

    Five changes, all of them to mechanisms the object already had.
*/
const fs = require('fs');
const SRC = 'C:/Users/peter/b/ArtefactB2311/Source/';

const files = {};
function load (rel) {
  const p = SRC + rel;
  const s = fs.readFileSync(p, 'utf8');
  files[rel] = { p, s, nl: s.indexOf(String.fromCharCode(13,10)) >= 0
                          ? String.fromCharCode(13,10) : String.fromCharCode(10) };
  return files[rel];
}
const miss = [];
function rep (rel, find, into) {
  const f = files[rel];
  const n = f.s.split(find).length - 1;
  if (n !== 1) { miss.push(rel + ' [' + n + 'x] ' + find.split(f.nl)[0].slice(0, 62)); return; }
  f.s = f.s.replace(find, into);
}
const J = (rel, a) => a.join(files[rel].nl);

load('Engine.h');
load('Engine.cpp');

//  ---------------------------------------------------------------- 1. param
rep('Engine.h',
    '    float slab      = 0.6f;    // section thickness',
    J('Engine.h', [
'    float slab      = 0.6f;    // section thickness',
'    /*  RETENTION — how long the section keeps where playing put it.',
'',
'        Until 260902.1 a played displacement was recalled to the winch with a',
'        1.2 s time constant, so the object forgot every phrase before the next',
'        one arrived and nothing could accumulate. The winch is now a place the',
'        object is HELD, and playing pushes it away from there; this is how long',
'        that push survives, from about four seconds to two minutes. */',
'    float retention = 0.55f;']));

rep('Engine.h',
    '    std::array<float, kMaxModes> wCent {};     // per-mode w centroid (per specimen)',
    J('Engine.h', [
'    std::array<float, kMaxModes> wCent {};     // per-mode w centroid (per specimen)',
'    /*  What playing has put into the section and the object has not yet let',
'        go of. Separate from w0 on purpose: the winch must stay instant under',
'        the hand while this stays slow. */',
'    float wDrift = 0.0f;']));

rep('Engine.cpp',
    '    { "slab",      "SLAB",       0.6f,  KP_PCT, 0, 1, &Params::slab },',
    J('Engine.cpp', [
'    { "slab",      "SLAB",       0.6f,  KP_PCT, 0, 1, &Params::slab },',
'    { "retention", "RETENTION",  0.55f, KP_PCT, 0, 1, &Params::retention },']));

rep('Engine.cpp',
    '    w0 = 0.0f; wVel = 0.0f;',
    '    w0 = 0.0f; wVel = 0.0f; wDrift = 0.0f;');

//  --------------------------------------------------- 2. the note's own aim
rep('Engine.cpp',
    J('Engine.cpp', [
'    //  playing moves the object: the note\u0027s mode-centroid tugs the section',
'    float cw = 0, tot = 0;',
'    for (int k = 0; k < s.nModes; ++k)',
'    {',
'        cw += wCent[(size_t) k] * v.energy[(size_t) k];',
'        tot += v.energy[(size_t) k];',
'    }',
'    if (tot > 1e-9f)',
'        wVel += p.transit * 0.55f * ((cw / tot) - w0);']),
    J('Engine.cpp', [
'    //  playing moves the object: the note\u0027s mode-centroid tugs the section',
'    float cw = 0, tot = 0;',
'    for (int k = 0; k < s.nModes; ++k)',
'    {',
'        cw += wCent[(size_t) k] * v.energy[(size_t) k];',
'        tot += v.energy[(size_t) k];',
'    }',
'    if (tot > 1e-9f)',
'    {',
'        /*  REFERRED TO THE SPREAD OF THIS SPECIMEN\u0027S OWN CENTROIDS, and that',
'            is what makes the mechanism work at all. Eigenvectors are mostly',
'            delocalised, so their w-centroids cluster hard around the middle of',
'            the body: measured on 260830.2, a note moved the section by 0.026',
'            out of a range of 2, and no amount of TRANSIT could fix it because',
'            the quantity TRANSIT scales was already very nearly zero.',
'',
'            Against the spread, a note lands somewhere DISTINCTIVE in the body,',
'            and different notes land in different places — which is the whole',
'            of "what is played decides where the object goes". */',
'        float mean = 0;',
'        for (int k = 0; k < s.nModes; ++k) mean += wCent[(size_t) k];',
'        mean /= (float) s.nModes;',
'        float var = 0;',
'        for (int k = 0; k < s.nModes; ++k)',
'        { const float d = wCent[(size_t) k] - mean; var += d * d; }',
'        const float spread = std::max (0.02f, std::sqrt (var / (float) s.nModes));',
'        const float aim = clampf (((cw / tot) - mean) / spread, -1.3f, 1.3f);',
'        //  harder blows carry the object further, as they carry it deeper',
'        wVel += p.transit * 0.9f * (0.35f + 0.65f * vel) * (aim - w0);',
'    }']));

//  --------------------------------------- 3. the section keeps what it is given
rep('Engine.cpp',
    J('Engine.cpp', [
'    w0 += wVel * dt;',
'    wVel *= std::exp (-dt / 1.4f);',
'    w0 += (winchTarget - w0) * (1.0f - std::exp (-dt * 0.85f));',
'    w0 = clampf (w0, -1.0f, 1.0f);']),
    J('Engine.cpp', [
'    /*  THE WINCH IS WHERE IT IS HELD; PLAYING IS WHERE IT HAS BEEN TAKEN.',
'',
'        These were one quantity until 260902.1, and that is why neither worked:',
'        a single 0.85/s recall had to be slow enough to remember a phrase and',
'        fast enough to make the wheel feel like a hand on the object, and it',
'        was neither. Now a played displacement accumulates in wDrift and is',
'        released over RETENTION — four seconds to two minutes — while w0 itself',
'        chases winch-plus-drift in a third of a second, so the wheel is",',
'        immediate and the phrase is remembered. */',
'    wDrift += wVel * dt;',
'    wVel *= std::exp (-dt / 1.4f);',
'    const float retTau = 4.0f * std::pow (30.0f, clamp01 (p.retention));',
'    wDrift *= std::exp (-dt / retTau);',
'    wDrift = clampf (wDrift, -1.4f, 1.4f);',
'    const float want = clampf (winchTarget + wDrift, -1.0f, 1.0f);',
'    w0 += (want - w0) * (1.0f - std::exp (-dt * 3.2f));',
'    w0 = clampf (w0, -1.0f, 1.0f);']));

//  --------------------------------------------------- 4. the section selects
rep('Engine.cpp',
    '    const float eps = 0.14f + 0.9f * clamp01 (p.slab);',
    J('Engine.cpp', [
'    /*  A THINNER SLAB, so that moving it is an event. At 0.14 + 0.9*slab the',
'        default section was 0.68 deep in a body about two deep — it held most',
'        of the tissue at every winch setting, so the whole travel of the wheel',
'        moved the colour by half an octave and mostly changed the LEVEL. The',
'        widen-until-it-holds-tissue guard in sliceWeights still protects the',
'        thin end, so nothing can fall silent between the organs. */',
'    const float eps = 0.045f + 0.42f * clamp01 (p.slab);']));

//  ------------------------------------------- 5. the traffic follows the slab
rep('Engine.cpp',
    J('Engine.cpp', [
'                    const float cw = s.coupleW[(size_t) k * kCouplePer + t];',
'                    const float d = clampf (coupleRate * cw, 0.0f, 0.25f)',
'                                  * (v.energy[(size_t) l] - v.energy[(size_t) k]);',
'                    v.energy[(size_t) k] += d;',
'                    v.energy[(size_t) l] -= d;']),
    J('Engine.cpp', [
'                    const float cw = s.coupleW[(size_t) k * kCouplePer + t];',
'                    /*  DIRECTED, not merely diffusive. Pure diffusion has one',
'                        equilibrium and reaches it: measured, a held note\u0027s',
'                        colour was static within a few seconds and then wandered',
'                        +-8 % for the next thirty. Weighting the exchange by how',
'                        much of each mode lies in the SECTION gives the traffic',
'                        somewhere to go, and because the section itself moves —',
'                        with the wake, with the winch, with everything played —',
'                        the destination keeps moving and the note keeps',
'                        travelling. At sectPull 0 this is exactly the old',
'                        (e_l - e_k), and it is still conserving: what leaves l',
'                        arrives at k. */',
'                    const float aK = 1.0f + 0.9f * sectW[(size_t) k];',
'                    const float aL = 1.0f + 0.9f * sectW[(size_t) l];',
'                    const float d = clampf (coupleRate * cw, 0.0f, 0.25f) * 0.5f',
'                                  * (v.energy[(size_t) l] * aK',
'                                   - v.energy[(size_t) k] * aL);',
'                    v.energy[(size_t) k] += d;',
'                    v.energy[(size_t) l] -= d;']));

//  ------------------------------------------------- 6. a touch takes it there
rep('Engine.cpp',
    J('Engine.cpp', [
'    if (! pick) pick = &voices[0];',
'    triggerVoice (*pick, tnote, vel, node);']),
    J('Engine.cpp', [
'    if (! pick) pick = &voices[0];',
'    triggerVoice (*pick, tnote, vel, node);',
'    /*  A touch also takes the object to where it was touched. triggerVoice',
'        already tugs the section by the note\u0027s mode-centroid; this adds the',
'        place — the node\u0027s own depth in the fourth dimension — so pressing a',
'        far organ carries the body towards it and it stays carried. */',
'    wVel += p.transit * 1.1f * (0.3f + 0.7f * vel)',
'          * (clampf (s.pw[node], -1.0f, 1.0f) - w0);']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed anchors:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
for (const k of Object.keys(files)) fs.writeFileSync(files[k].p, files[k].s);
console.log('patched: Engine.h + Engine.cpp, 6 edits');
