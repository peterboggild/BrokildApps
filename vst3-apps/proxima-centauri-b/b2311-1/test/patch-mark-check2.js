/*  260902.3 — section 13 measured firing COUNT, which is the one thing the
    coupling pins. A mark's audible effect is that the marked region speaks
    through a different body: it changes the COLOUR, not the census.
*/
const fs = require('fs');
const P = 'C:/Users/peter/b/ArtefactB2311_1/test/bench.cpp';

let s = fs.readFileSync(P, 'utf8');
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const BS = String.fromCharCode(92);
const N = BS + 'n';
const miss = [];
const J = a => a.join(NL);

const start = '        auto after = [&base] (float memory, bool touch, double waitSecs)';
const endMark = '        ok (longMem > shortMem, "a long MEMORY holds the mark longer than a short one");';
const a = s.indexOf(start), b = s.indexOf(endMark);
if (a < 0 || b < 0) { console.error('ABORTED: section 13 not found'); process.exit(1); }

const replacement = J([
'        /*  Measured on the COLOUR, not on the number of firings. A mark speeds',
'            a unit up, and at any real CONDUCTION the coupling pins the',
'            collective rate — measured, the census moved one per cent and',
'            MEMORY made no difference to it whatsoever. What a mark actually',
'            does is move those units into another layer of the body, so what',
'            has to be measured is what the object SOUNDS like afterwards. */',
'        auto colourAfter = [&base] (float memory, bool touch, double waitSecs)',
'        {',
'            ab1::Params p = base; p.memory = memory;',
'            ab1::Engine e; e.p = p;',
'            e.prepare (SR, BLK); e.service();',
'            e.setTransport (120.0, 0.0, true); e.noteOn (48, 0.9f);',
'            std::vector<float> bl (BLK), br (BLK), L;',
'            for (int q = 0; q < (int)(0.5 * SR / BLK); ++q) e.process (bl.data(), br.data(), BLK);',
'            if (touch)',
'                for (int x = 6; x < 26; ++x) e.poke (x, 16, 0.35f, 4);',
'            for (int q = 0; q < (int)(waitSecs * SR / BLK); ++q) e.process (bl.data(), br.data(), BLK);',
'            //  one second of sound, AFTER the touch has long ended',
'            for (int q = 0; q < (int)(1.0 * SR / BLK); ++q)',
'            { e.process (bl.data(), br.data(), BLK); L.insert (L.end(), bl.begin(), bl.end()); }',
'            double se = 0, sd = 0;',
'            for (size_t q = 1; q < L.size(); ++q)',
'            { const double v = L[q], d = v - L[q-1]; se += v*v; sd += d*d; }',
'            return se > 1e-30 ? (SR / (2.0 * ab1::PI)) * std::sqrt (sd / se) : 0.0;',
'        };',
'',
'        const double plain = colourAfter (0.60f, false, 1.0);',
'        const double poked = colourAfter (0.60f, true,  1.0);',
'        const double diff  = std::abs (poked - plain) / std::max (1.0, plain);',
'        std::printf ("  a second after the touch ended: %.0f Hz vs %.0f untouched (%.1f %%)' + N + '",',
'                     poked, plain, 100.0 * diff);',
'        ok (diff > 0.03, "the object still sounds different after the touch has ended");',
'',
'        /*  and MEMORY decides for how long. Eight seconds on, a short memory',
'            should have let go of what a long one is still holding. */',
'        const double sBase = colourAfter (0.05f, false, 8.0);',
'        const double sMark = colourAfter (0.05f, true,  8.0);',
'        const double lBase = colourAfter (0.95f, false, 8.0);',
'        const double lMark = colourAfter (0.95f, true,  8.0);',
'        const double shortMem = std::abs (sMark - sBase) / std::max (1.0, sBase);',
'        const double longMem  = std::abs (lMark - lBase) / std::max (1.0, lBase);',
'        std::printf ("  eight seconds later: MEMORY 0.05 differs %.1f %%, MEMORY 0.95 %.1f %%' + N + '",',
'                     100.0 * shortMem, 100.0 * longMem);',
'        ok (longMem > shortMem, "a long MEMORY holds the mark longer than a short one");']);

s = s.slice(0, a) + replacement + s.slice(b + endMark.length);
fs.writeFileSync(P, s);
console.log('bench.cpp section 13 rewritten to measure colour');
