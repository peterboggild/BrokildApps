/*  B2311.22 260902.2 — the fourth dimension gets a name and an outside, and
    the frame gets its kelvin reading.

    Peter: call the winch "4D pull/push?" (the question mark is the survey's,
    not ours — nobody established that this is what is happening); say "(Try
    wheel)" on the face so it can be guessed; let the wheel keep going until
    the structures are gone altogether; and give it a big slider like the
    TEMPERATURE the other two artefacts carry, which .22 should have too,
    down to 77 K.
*/
const fs = require('fs');
const SRC = 'C:/Users/peter/b/ArtefactB2311/Source/';
const files = {};
function load (rel) {
  const p = SRC + rel, s = fs.readFileSync(p, 'utf8');
  files[rel] = { p, s, nl: s.indexOf(String.fromCharCode(13,10)) >= 0
                          ? String.fromCharCode(13,10) : String.fromCharCode(10) };
}
const miss = [];
function rep (rel, find, into) {
  const f = files[rel];
  const n = f.s.split(find).length - 1;
  if (n !== 1) { miss.push(rel + ' [' + n + 'x] ' + find.split(f.nl)[0].slice(0, 60)); return; }
  f.s = f.s.replace(find, into);
}
const J = (rel, a) => a.join(files[rel].nl);

load('Engine.h'); load('Engine.cpp'); load('PluginProcessor.cpp');

//  --------------------------------------------------------------- the kind
rep('Engine.h',
    'enum ParamKind { KP_PCT, KP_SW, KP_INT, KP_VOL };',
    J('Engine.h', [
'/*  KP_KELVIN is APPENDED, so KP_VOL keeps the value 3 that the page tests',
'    for. Renumbering an enum the panel reads by number is a silent way to',
'    make every volume control display as a percentage. */',
'enum ParamKind { KP_PCT, KP_SW, KP_INT, KP_VOL, KP_KELVIN };']));

rep('Engine.h',
    '    float retention = 0.55f;',
    J('Engine.h', [
'    float retention = 0.55f;',
'',
'    /*  TEMPERATURE. The frame has carried a kelvin reading since the first',
'        build and .22 was the one artefact that did not act on it. Cold, the',
'        body does not vibrate: a strike puts nothing into it, nothing',
'        sustains, nothing migrates and it does not patrol. Warm, all four',
'        rise together.',
'',
'        The default is EXACTLY room, and the scaling is written so that at',
'        room every factor is exactly 1.0 — so this build is the previous one',
'        at every setting anybody has played, and the whole bench stays',
'        valid. */',
'    float temp = 0.298755f;    // 0 = 77 K, 1 = 800 K']));

rep('Engine.h',
    '    float wDrift = 0.0f;',
    J('Engine.h', [
'    float wDrift = 0.0f;',
'',
'    /*  How cold it is, as a multiplier that is exactly 1 at room. */',
'    float thermal() const',
'    {',
'        const float w = p.temp < 0.0f ? 0.0f : (p.temp > 1.0f ? 1.0f : p.temp);',
'        const float t = w / 0.298755f;',
'        return t > 2.6f ? 2.6f : t;',
'    }']));

//  ------------------------------------------------------------ the SPECS row
rep('Engine.cpp',
    '    { "winch",     "WINCH",      0.5f,  KP_PCT, 0, 1, &Params::winch },',
    J('Engine.cpp', [
'    /*  The researchers named this one for the apparatus — a winch is what',
'        they turned. What it does to the object is not established, and the',
'        question mark is theirs. */',
'    { "winch",     "4D PULL/PUSH?", 0.5f, KP_PCT, 0, 1, &Params::winch },']));

rep('Engine.cpp',
    '    { "retention", "RETENTION",  0.55f, KP_PCT, 0, 1, &Params::retention },',
    J('Engine.cpp', [
'    { "retention", "RETENTION",  0.55f, KP_PCT, 0, 1, &Params::retention },',
'    { "temp",      "TEMPERATURE", 0.298755f, KP_KELVIN, 77, 800, &Params::temp },']));

//  ------------------------------------------ the traverse reaches past the body
rep('Engine.cpp',
    '    float winchTarget = (clamp01 (p.winch) - 0.5f) * 2.0f;',
    J('Engine.cpp', [
'    /*  THE TRAVERSE REACHES PAST THE BODY, and that is the point of it.',
'',
'        Mapped to +-1 the control could only ever move the section INSIDE the',
'        tissue, so at either end there was still an object and the wheel just',
'        stopped. Mapped past the extent, winding far enough in either',
'        direction carries the body out of the plane entirely and there is',
'        nothing there — which is what a three-dimensional section of a',
'        four-dimensional thing does when you push it far enough.',
'',
'        The curve is expanded rather than linear so the tissue still occupies',
'        most of the travel: the outer fifth at each end is the empty space',
'        beyond the object. */',
'    const float wx = (clamp01 (p.winch) - 0.5f) * 2.0f;',
'    float winchTarget = (wx < 0.0f ? -1.0f : 1.0f)',
'                      * std::pow (std::fabs (wx), 1.9f) * 2.2f;']));

rep('Engine.cpp',
    J('Engine.cpp', [
'    const float want = clampf (winchTarget + wDrift, -1.0f, 1.0f);',
'    w0 += (want - w0) * (1.0f - std::exp (-dt * 3.2f));',
'    w0 = clampf (w0, -1.0f, 1.0f);']),
    J('Engine.cpp', [
'    const float want = clampf (winchTarget + wDrift, -2.6f, 2.6f);',
'    w0 += (want - w0) * (1.0f - std::exp (-dt * 3.2f));',
'    w0 = clampf (w0, -2.6f, 2.6f);']));

//  ---------------------------------------- the guard protects gaps, not the void
rep('Engine.cpp',
    J('Engine.cpp', [
'    float eps = eps0;',
'    for (int pass = 0; pass < 6; ++pass)',
'    {',
'        int inside = 0;',
'        for (int i = 0; i < s.nNodes; ++i)',
'            if (std::fabs (s.pw[i] - w0) < eps) { ++inside; break; }',
'        if (inside > 0) break;',
'        eps *= 1.8f;',
'    }']),
    J('Engine.cpp', [
'    /*  The widening guard exists so the slab is never empty-handed BETWEEN',
'        the organs. It must not also rescue a section that has been wound',
'        clean out of the body: past the tissue there is supposed to be',
'        nothing, and a guard that widens forever would keep dragging the',
'        object back into a plane it has left. So it runs only while the',
'        section is still within reach of the body. */',
'    float wlo = 1.0e9f, whi = -1.0e9f;',
'    for (int i = 0; i < s.nNodes; ++i)',
'    { if (s.pw[i] < wlo) wlo = s.pw[i]; if (s.pw[i] > whi) whi = s.pw[i]; }',
'    const bool nearBody = (w0 > wlo - eps0) && (w0 < whi + eps0);',
'    float eps = eps0;',
'    if (nearBody)',
'        for (int pass = 0; pass < 6; ++pass)',
'        {',
'            int inside = 0;',
'            for (int i = 0; i < s.nNodes; ++i)',
'                if (std::fabs (s.pw[i] - w0) < eps) { ++inside; break; }',
'            if (inside > 0) break;',
'            eps *= 1.8f;',
'        }']));

//  ------------------------------------------------------------- the thermal
rep('Engine.cpp',
    '    const float budget = v.velocity;',
    J('Engine.cpp', [
'    //  a cold body takes nothing from a blow',
'    const float budget = v.velocity * thermal();']));

rep('Engine.cpp',
    J('Engine.cpp', [
'    const float coupleRate = p.metabolism * p.metabolism * 6.5f * dt',
'                           * (1.0f + p.membrane * 1.5f);']),
    J('Engine.cpp', [
'    const float coupleRate = p.metabolism * p.metabolism * 6.5f * dt',
'                           * (1.0f + p.membrane * 1.5f) * thermal();']));

rep('Engine.cpp',
    '    const float sustain = 0.16f + 0.34f * ap;',
    J('Engine.cpp', [
'    //  cold, nothing sustains: a strike rings down and that is all',
'    const float sustain = (0.16f + 0.34f * ap) * thermal();']));

rep('Engine.cpp',
    J('Engine.cpp', [
'        winchTarget += p.wake * 1.15f',
'                     * (0.72f * std::sin (subPhase * 6.2831853f)',
'                      + 0.28f * std::sin (subPhase * 2.6180340f * 6.2831853f));']),
    J('Engine.cpp', [
'        winchTarget += p.wake * 1.15f * thermal()',
'                     * (0.72f * std::sin (subPhase * 6.2831853f)',
'                      + 0.28f * std::sin (subPhase * 2.6180340f * 6.2831853f));']));

//  ------------------------------------------------------ the kelvin readout
rep('PluginProcessor.cpp',
    '            case ab::KP_INT: return juce::String ((int) std::lround (v));',
    J('PluginProcessor.cpp', [
'            case ab::KP_INT: return juce::String ((int) std::lround (v));',
'            case ab::KP_KELVIN:',
'                return juce::String ((int) std::lround (77.0f + 723.0f * v)) + " K";']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed anchors:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
for (const k of Object.keys(files)) fs.writeFileSync(files[k].p, files[k].s);
console.log('patched: Engine.h, Engine.cpp, PluginProcessor.cpp');
