/*  B2311.22 260902.1, second pass — measured consequences of the first.

      - the WINCH became striking but swung the LEVEL 6x (0.0011 .. 0.0068,
        15.5 dB): a thin section holds less tissue, so moving it faded the
        object instead of transforming it. Looking at a different part of a
        body does not make the body quieter.
      - one note swung the section most of the way across (settled at -0.43
        from a single strike), so a phrase could not accumulate — the first
        note saturated it.
      - and a held note still SETTLED by twenty seconds: the directed traffic
        reaches its new equilibrium and stops, unless the section it is aiming
        at keeps moving.
*/
const fs = require('fs');
const P = 'C:/Users/peter/b/ArtefactB2311/Source/Engine.cpp';
let s = fs.readFileSync(P, 'utf8');
const NL = s.indexOf(String.fromCharCode(13,10)) >= 0
  ? String.fromCharCode(13,10) : String.fromCharCode(10);
const miss = [];
function rep (find, into) {
  const n = s.split(find).length - 1;
  if (n !== 1) { miss.push('[' + n + 'x] ' + find.split(NL)[0].slice(0, 64)); return; }
  s = s.replace(find, into);
}
const J = a => a.join(NL);

//  1. the section transforms, it does not fade
rep(J([
'    for (int k = 0; k < s.nModes; ++k)',
'    {',
'        float wsum = 0;',
'        for (int i = 0; i < s.nNodes; ++i)',
'        {',
'            const float d = std::fabs (s.pw[i] - w0);',
'            if (d < eps)',
'            {',
'                const float vv = s.vec[(size_t) k * s.nNodes + i];',
'                //  soft slab edge so tissue condenses rather than pops',
'                const float g = 1.0f - (d / eps) * (d / eps);',
'                wsum += vv * vv * g;',
'            }',
'        }',
'        //  smooth toward the new weight (tissue-speed, not click-speed)',
'        out[(size_t) k] += 0.25f * (wsum - out[(size_t) k]);',
'    }']),
    J([
'    /*  THE SECTION TRANSFORMS THE OBJECT; IT DOES NOT FADE IT.',
'',
'        The raw slice weights fall away as the slab leaves the bulk of the',
'        tissue, and with a thin slab that is most of its travel: measured, the',
'        winch swung the level by 15.5 dB and the wheel read as a fade with a',
'        colour change attached. Normalising the total keeps the object exactly',
'        as loud wherever the section stands, so what the wheel does is change',
'        WHICH of the body is sounding — which is what it is for. Same law the',
'        instrument already applies to GRAVITY: colour, never loudness. */',
'    float raw[kMaxModes];',
'    float total = 0.0f;',
'    for (int k = 0; k < s.nModes; ++k)',
'    {',
'        float wsum = 0;',
'        for (int i = 0; i < s.nNodes; ++i)',
'        {',
'            const float d = std::fabs (s.pw[i] - w0);',
'            if (d < eps)',
'            {',
'                const float vv = s.vec[(size_t) k * s.nNodes + i];',
'                //  soft slab edge so tissue condenses rather than pops',
'                const float g = 1.0f - (d / eps) * (d / eps);',
'                wsum += vv * vv * g;',
'            }',
'        }',
'        raw[k] = wsum;',
'        total += wsum;',
'    }',
'    /*  kSectSum is calibrated, not chosen: it is the total the old thick',
'        default slab produced, so a patch is as loud as it was. */',
'    const float scale = total > 1e-6f ? (0.62f * (float) s.nModes / total) : 0.0f;',
'    for (int k = 0; k < s.nModes; ++k)',
'    {',
'        //  smooth toward the new weight (tissue-speed, not click-speed)',
'        out[(size_t) k] += 0.25f * (raw[k] * scale - out[(size_t) k]);',
'    }']));

//  2. a phrase walks it; one note does not slam it
rep('        wVel += p.transit * 0.9f * (0.35f + 0.65f * vel) * (aim - w0);',
    J([
'        /*  0.45 and not 0.9: at 0.9 a SINGLE note carried the section most of',
'            the way across and settled at -0.43, so the first note saturated it',
'            and nothing after that could be told apart. A phrase should walk',
'            the object, not one strike throw it. */',
'        wVel += p.transit * 0.45f * (0.35f + 0.65f * vel) * (aim - w0);']));

//  3. the object patrols itself, so a held note never arrives
rep('        winchTarget += p.wake * 0.45f * std::sin (subPhase * 6.2831853f);',
    J([
'        /*  A REAL PATROL. At 0.45 the wake moved the section by +-0.07 over a',
'            forty-second period — under a thin slab that is nothing, and it is',
'            why a held note reached its new equilibrium in twenty seconds and',
'            then sat there: the directed traffic had nowhere left to go. The',
'            wander is now the same order as the winch travel itself, so the',
'            destination keeps moving and a held note keeps arriving somewhere',
'            else. Still exactly inert at WAKE 0. */',
'        winchTarget += p.wake * 1.15f',
'                     * (0.72f * std::sin (subPhase * 6.2831853f)',
'                      + 0.28f * std::sin (subPhase * 2.6180340f * 6.2831853f));']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed anchors:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log('patched: 3 edits');
