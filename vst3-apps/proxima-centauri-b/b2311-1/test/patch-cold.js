/*  260902.3 — a permanent guard for the cold ramp.

    Written to a FILE and not passed to `node -e` from bash, because bash eats
    the backslashes and a printf format arrives with real newlines in it. That
    is in CLAUDE.md and it has now happened four times in one session.
*/
const fs = require('fs');
const P = 'C:/Users/peter/b/ArtefactB2311_1/test/bench.cpp';

let s = fs.readFileSync(P, 'utf8');
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const BS = String.fromCharCode(92);          // a real backslash
const N = BS + 'n';                          // the two characters  \ n
const Q = String.fromCharCode(34);           // a double quote
const miss = [];
function rep (find, into) {
  const n = s.split(find).length - 1;
  if (n !== 1) { miss.push('[' + n + 'x] ' + find.slice(0, 60)); return; }
  s = s.replace(find, into);
}
const J = a => a.join(NL);

const anchor = '    std::printf (' + Q + N + '%d checks, %d failed  -  %s' + N + Q + ', checks, fails,';

rep(anchor, J([
'    //------------------------------------------------------------------',
'    head ("12 - it runs down; it is not switched off");',
'    {',
'        /*  Peter, 260902.3: "it is unrealistic that the patterns stop dead at',
'            77 K... they should just be less active - there is big change from',
'            79 to 77 K". There were two cliffs stacked on each other: the rate',
'            law still counted at a QUARTER SPEED at 77.1 K, and a hard return',
'            then muted the object outright at 77.0. */',
'        std::printf ("  kelvin   firings/s   step' + N + '");',
'        double prev = -1, worstJump = 0, top = 0;',
'        bool monotonic = true;',
'        for (double k : { 77.0, 79.0, 83.0, 88.0, 94.0, 99.0, 120.0, 180.0 })',
'        {',
'            ab1::Params p = base; p.temp = (float) ((k - 77.0) / 723.0);',
'            Take t = run (3.0, p, 120.0, true);',
'            const double f = t.fps();',
'            std::printf ("  %6.0f   %9.0f", k, f);',
'            if (prev >= 0)',
'            {',
'                const double jump = f - prev;',
'                std::printf ("   +%.0f", jump);',
'                if (f + 1.0 < prev) monotonic = false;',
'                worstJump = std::max (worstJump, jump);',
'            }',
'            std::printf ("' + N + '");',
'            prev = f; top = f;',
'        }',
'        ok (monotonic, "warming it never makes it quieter");',
'        /*  The signature of a cliff is one step carrying most of the range.  */',
'        ok (worstJump < 0.5 * top,',
'            "no single step across the cold end carries half the range");',
'',
'        ab1::Params c = base; c.temp = 0.0f;',
'        Take z = run (1.0, c, 120.0, true);',
'        ok (peakOf (z.L) == 0.0, "and at 77 K it is still exactly silent");',
'    }',
'',
anchor]));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log('bench.cpp patched: section 12 added');
