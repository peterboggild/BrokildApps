/*  260902.3 — wire MEMORY into the engine.

    Anchor-based, exact-count, and it collects every miss and aborts BEFORE
    writing anything: a script that applies six of eight replacements leaves a
    file that is neither the old one nor the new one.
*/
const fs = require('fs');
const P = 'C:/Users/peter/b/ArtefactB2311_1/Source/Engine.cpp';

let s = fs.readFileSync(P, 'utf8');
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const miss = [];

function rep (find, into) {
  const n = s.split(find).length - 1;
  if (n !== 1) { miss.push('[' + n + 'x] ' + find.split(NL)[0].slice(0, 64)); return; }
  s = s.replace(find, into);
}
const J = a => a.join(NL);

//  1. the parameter
rep('    { "weight",   "WEIGHT",       "how far a heavy blow reaches into the low body",       0.45f,  KP_PCT,   0, 0, P(weight) },',
    J(['    { "weight",   "WEIGHT",       "how far a heavy blow reaches into the low body",       0.45f,  KP_PCT,   0, 0, P(weight) },',
       '    { "memory",   "MEMORY",       "how long the body keeps the mark of being touched",    0.45f,  KP_PCT,   0, 0, P(memory) },']));

//  2. allocate + clear
rep('    bandOf.assign (NUNIT, 0);',
    J(['    bandOf.assign (NUNIT, 0);',
       '    mark.assign (NUNIT, 0.0f);']));

rep('    for (int i = 0; i < NUNIT; ++i) { ph[(size_t)i] = rnd(); heat[(size_t)i] = 0.0f; inQ[(size_t)i] = 0; }',
    '    for (int i = 0; i < NUNIT; ++i) { ph[(size_t)i] = rnd(); heat[(size_t)i] = 0.0f; inQ[(size_t)i] = 0; mark[(size_t)i] = 0.0f; }');

//  3. a touch MARKS, it does not merely shove
rep(J(['        if (ph[(size_t)i] < 0.0f) continue;',
       '        ph[(size_t)i] = (float) std::min (1.0, (double) ph[(size_t)i] + amount);',
       '    }',
       '}']),
    J(['        /*  The shove, as before — a touch is still felt at once.  */',
       '        if (ph[(size_t)i] >= 0.0f)',
       '            ph[(size_t)i] = (float) std::min (1.0, (double) ph[(size_t)i] + amount);',
       '        /*  And the mark it leaves. Falls off towards the edge of the brush so a',
       '            drag lays a stroke rather than a row of tiles, and accumulates, so',
       '            going over the same place twice presses harder. */',
       '        const float d = std::sqrt ((float)(dx*dx + dy*dy)) / (float) std::max (1, radius);',
       '        const float w = clampf (1.0f - d, 0.0f, 1.0f);',
       '        float& mk = mark[(size_t)i];',
       '        mk = clampf (mk + (amount >= 0.0f ? 1.0f : -1.0f) * w * 0.55f, -1.0f, 1.0f);',
       '    }',
       '}']));

//  4. the mark decays, and it scales the rate. Both belong in the step.
rep('            advanceStep (dt * heatRate, firedScratch);',
    J(['            /*  THE MARK FADES. MEMORY is a time constant from two seconds to',
       '                permanent; at the top the multiplier is exactly 1 and a mark',
       '                stays until something else moves it. */',
       '            {',
       '                const float mem = clampf (p.memory, 0.0f, 1.0f);',
       '                if (mem < 0.999f)',
       '                {',
       '                    const double tau = 2.0 * std::pow (140.0, (double) mem);',
       '                    const float k = (float) std::exp (-dt / tau);',
       '                    for (int i = 0; i < NUNIT; ++i) mark[(size_t)i] *= k;',
       '                }',
       '            }',
       '            advanceStep (dt * heatRate, firedScratch);']));

//  5. a marked unit counts faster
rep(J(['        if (v < 0.0f)                        // dead: count back up to zero',
       '        {',
       '            v += (float) (rate[(size_t)i] * noteRate * dt);',
       '            ph[(size_t)i] = v > 0.0f ? 0.0f : v;',
       '            continue;',
       '        }',
       '        v += (float) (rate[(size_t)i] * noteRate * dt);']),
    J(['        /*  A MARKED unit counts faster — up to two octaves either way. Rate is',
       '            a unit\u0027s identity here, so a mark moves it into another layer: it',
       '            speaks through a different body and the panel draws it a different',
       '            colour. Drawing on the object really does redraw it. */',
       '        const float mk = mark[(size_t)i];',
       '        const float rm = mk == 0.0f ? 1.0f : std::exp2 (mk * 2.0f);',
       '        if (v < 0.0f)                        // dead: count back up to zero',
       '        {',
       '            v += (float) (rate[(size_t)i] * rm * noteRate * dt);',
       '            ph[(size_t)i] = v > 0.0f ? 0.0f : v;',
       '            continue;',
       '        }',
       '        v += (float) (rate[(size_t)i] * rm * noteRate * dt);']));

//  6. the panel is shown the mark
rep('            out.rate [y * NX + x] = (uint8_t) std::min (255, (int) ((rate[(size_t)i] - rlo) / rspan * 255.0f));',
    J(['            out.rate [y * NX + x] = (uint8_t) std::min (255, (int) ((rate[(size_t)i] - rlo) / rspan * 255.0f));',
       '            out.mark [y * NX + x] = (uint8_t) clampf (128.0f + mark[(size_t)i] * 127.0f, 0.0f, 255.0f);']));

//  7. the catalogue draws it too (appended, so earlier draws stay identical)
rep('    p.weight   = rng (0.10f, 0.75f);',
    J(['    p.weight   = rng (0.10f, 0.75f);',
       '    p.memory   = rng (0.20f, 0.90f);']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed anchors:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log('Engine.cpp patched: ' + 7 + ' edits');
