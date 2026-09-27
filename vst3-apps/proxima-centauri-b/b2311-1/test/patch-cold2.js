/*  260902.3 — section 12 measured the wrong thing, in the way this bench keeps
    measuring the wrong thing.

    It read 3072 firings/s at 79, 83, 88 and 94 K, flat, and then 3355 at 99 —
    and 3072 is exactly NUNIT/3 over a 3 s take. Cold, the lattice is fully
    synchronised and fires as ONE event, so a 3 s take resolves nothing finer
    than "it went off once". The flatness was the take length, not the object.
*/
const fs = require('fs');
const P = 'C:/Users/peter/b/ArtefactB2311_1/test/bench.cpp';

let s = fs.readFileSync(P, 'utf8');
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const BS = String.fromCharCode(92);
const N = BS + 'n';
const Q = String.fromCharCode(34);
const miss = [];
function rep (find, into) {
  const n = s.split(find).length - 1;
  if (n !== 1) { miss.push('[' + n + 'x] ' + find.slice(0, 70)); return; }
  s = s.replace(find, into);
}
const J = a => a.join(NL);

//  replace the whole of the old section 12 body
const oldStart = '        std::printf ("  kelvin   firings/s   step' + N + '");';
const oldEnd   = '        ok (peakOf (z.L) == 0.0, "and at 77 K it is still exactly silent");';
const a = s.indexOf(oldStart), b = s.indexOf(oldEnd);
if (a < 0 || b < 0) { console.error('ABORTED: could not find section 12'); process.exit(1); }
const old = s.slice(a, b + oldEnd.length);

rep(old, J([
'        /*  Twenty-second takes and TOTAL firings, not a rate over three',
'            seconds. Cold, the lattice synchronises and goes off as one body of',
'            9216, so the quantity that exists at all is how OFTEN that happens,',
'            and three seconds could only ever report none, one or two of them.  */',
'        std::printf ("  kelvin   whole-body events in 20 s   x previous' + N + '");',
'        double prev = -1, worstRatio = 1.0;',
'        bool monotonic = true, countsWhenNearlyFrozen = false;',
'        for (double k : { 80.0, 90.0, 100.0, 120.0, 150.0, 190.0 })',
'        {',
'            ab1::Params p = base; p.temp = (float) ((k - 77.0) / 723.0);',
'            Take t = run (20.0, p, 120.0, true);',
'            const double ev = (double) t.fired / (double) ab1::NUNIT;',
'            std::printf ("  %6.0f   %25.1f", k, ev);',
'            if (prev > 0.5)',
'            {',
'                const double ratio = ev / prev;',
'                std::printf ("   %.2fx", ratio);',
'                if (ev + 0.5 < prev) monotonic = false;',
'                worstRatio = std::max (worstRatio, ratio);',
'            }',
'            std::printf ("' + N + '");',
'            if (k <= 80.5 && ev > 0.5) countsWhenNearlyFrozen = true;',
'            prev = ev;',
'        }',
'        ok (countsWhenNearlyFrozen,',
'            "three kelvin above the floor it still counts, slowly");',
'        ok (monotonic, "warming it never makes it quieter");',
'        /*  A cliff shows up as one step multiplying the activity many times',
'            over. Smooth means every step is a modest factor. */',
'        std::printf ("  worst single step: %.2fx' + N + '", worstRatio);',
'        ok (worstRatio < 6.0, "no single step across the cold end is a cliff");',
'',
'        ab1::Params c = base; c.temp = 0.0f;',
'        Take z = run (1.0, c, 120.0, true);',
'        ok (peakOf (z.L) == 0.0, "and at 77 K it is still exactly silent");']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log('bench.cpp section 12 rewritten');
