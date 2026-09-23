/*  B2311.22 — the winch recall check asserted the OLD +-1 travel.

    From 260902.2 the control reaches past the body on purpose, so at 0.1 and
    0.9 the section is at -1.44 / +1.44 and not -0.8 / +0.8. Rather than move
    two numbers, this asserts what the control is now FOR: that the middle of
    the travel is inside the tissue and the ends are outside it, and that
    outside it there is nothing to hear.
*/
const fs = require('fs');
const P = 'C:/Users/peter/b/ArtefactB2311/test/bench.cpp';
let s = fs.readFileSync(P, 'utf8');
const NL = s.indexOf(String.fromCharCode(13,10)) >= 0
  ? String.fromCharCode(13,10) : String.fromCharCode(10);
const miss = [];
const J = a => a.join(NL);
function rep (find, into) {
  const n = s.split(find).length - 1;
  if (n !== 1) { miss.push('[' + n + 'x] ' + find.split(NL)[0].slice(0, 64)); return; }
  s = s.replace(find, into);
}

rep(J([
'        CHECK (std::fabs (wLo - (-0.8f)) < 0.08f, "winch low recall off (%.3f)", (double) wLo);',
'        CHECK (std::fabs (wHi - 0.8f) < 0.08f, "winch high recall off (%.3f)", (double) wHi);']),
    J([
'        CHECK (wLo < -1.2f, "the traverse does not reach past the body low (%.3f)", (double) wLo);',
'        CHECK (wHi >  1.2f, "the traverse does not reach past the body high (%.3f)", (double) wHi);',
'        CHECK (std::fabs (wLo + wHi) < 0.05f, "the traverse is lopsided (%.3f)",',
'               (double) (wLo + wHi));',
'',
'        /*  AND WHAT IT IS FOR: wound to either end, the object has been',
'            carried out of the plane and there is nothing there. The widening',
'            guard still protects the gaps BETWEEN organs, so this can only',
'            pass because the section is genuinely outside the tissue. */',
'        {',
'            auto loudAt = [&] (float winch) -> double',
'            {',
'                Engine e;',
'                e.p.winch = winch; e.p.wake = 0; e.p.transit = 0;',
'                e.prepare (fs, 512);',
'                const int N = 6 * 48000;',
'                std::vector<float> L ((size_t) N), R ((size_t) N);',
'                render (e, { { 2 * 48000, 0, 50, 0.9f } }, L.data(), R.data(), N);',
'                double pk = 0;',
'                for (int i = 4 * 48000; i < N; ++i) pk = std::max (pk, (double) std::fabs (L[(size_t) i]));',
'                return pk;',
'            };',
'            const double mid  = loudAt (0.5f);',
'            const double edge = loudAt (0.0f);',
'            const double edg2 = loudAt (1.0f);',
'            std::printf ("   peak: mid %.5f, wound fully out %.5f / %.5f\\n", mid, edge, edg2);',
'            CHECK (mid > 0.01, "the object is not audible in the middle of the traverse");',
'            CHECK (edge < 0.02 * mid && edg2 < 0.02 * mid,',
'                   "wound fully out the structures do not disappear (%.4f, %.4f of %.4f)",',
'                   edge, edg2, mid);',
'        }']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log('bench: winch check now asserts the traverse and the void');
