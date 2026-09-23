/*  260902.3 — prove the mark outlives the touch.

    A touch that changed nothing after the finger lifted would pass every
    bounded test in this file, and the panel would look identical. So: poke,
    stop poking, and measure that the object is STILL different a second later,
    and that MEMORY decides for how long.
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

const anchor = '    std::printf (' + Q + N + '%d checks, %d failed  -  %s' + N + Q + ', checks, fails,';

rep(anchor, J([
'    //------------------------------------------------------------------',
'    head ("13 - a touch is remembered");',
'    {',
'        /*  Peter, 260902.3: "dragging or marking areas should change these tiny',
'            clocks behaviour permanently, or at least a while - not just while',
'            it is pressed down."',
'',
'            The thing to measure is therefore what the object is doing AFTER',
'            the touch has ended, and against an untouched object run for the',
'            same length of time from the same seed. A mark that only lasted',
'            while the button was down would look identical here. */',
'        auto after = [] (float memory, bool touch, double waitSecs)',
'        {',
'            ab1::Params p = base; p.memory = memory;',
'            ab1::Engine e; e.p = p;',
'            e.prepare (SR, BLK); e.service();',
'            e.setTransport (120.0, 0.0, true); e.noteOn (48, 0.9f);',
'            std::vector<float> bl (BLK), br (BLK);',
'            for (int b = 0; b < (int)(0.5 * SR / BLK); ++b) e.process (bl.data(), br.data(), BLK);',
'            //  the drag, and then nothing',
'            if (touch)',
'                for (int x = 8; x < 24; ++x) e.poke (x, 16, 0.35f, 3);',
'            for (int b = 0; b < (int)(waitSecs * SR / BLK); ++b) e.process (bl.data(), br.data(), BLK);',
'            const long long mark = e.totalFired.load();',
'            //  what it does in the SECOND after the wait',
'            for (int b = 0; b < (int)(1.0 * SR / BLK); ++b) e.process (bl.data(), br.data(), BLK);',
'            return (double) (e.totalFired.load() - mark);',
'        };',
'',
'        const double plain = after (0.60f, false, 1.0);',
'        const double poked = after (0.60f, true,  1.0);',
'        const double diff  = std::abs (poked - plain) / std::max (1.0, plain);',
'        std::printf ("  a second after the touch ended: %.0f firings vs %.0f untouched (%.1f %%)' + N + '",',
'                     poked, plain, 100.0 * diff);',
'        ok (diff > 0.02, "the object is still changed after the touch has ended");',
'',
'        /*  and MEMORY is what decides for how long. A short memory should have',
'            let go of it by the time a long one still has it. */',
'        const double shortMem = std::abs (after (0.05f, true, 6.0) - after (0.05f, false, 6.0));',
'        const double longMem  = std::abs (after (0.95f, true, 6.0) - after (0.95f, false, 6.0));',
'        std::printf ("  six seconds later: MEMORY 0.05 differs by %.0f, MEMORY 0.95 by %.0f' + N + '",',
'                     shortMem, longMem);',
'        ok (longMem > shortMem, "a long MEMORY holds the mark longer than a short one");',
'    }',
'',
anchor]));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log('bench.cpp section 13 added');
