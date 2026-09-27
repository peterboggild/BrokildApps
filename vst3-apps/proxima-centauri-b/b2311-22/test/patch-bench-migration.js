/*  B2311.22 — check 11 tested one AUDIBLE CONSEQUENCE of migration, and from
    260902.1 the section is a thin selector that pins the audible centroid.

    The old test froze TRANSIT and WAKE to isolate migration from the section,
    then measured the spectral centroid of the OUTPUT. That was a fair proxy
    while the default slab was 0.68 deep and held most of the tissue at once.
    With a slab of 0.30 the audible set is narrow and the section decides the
    centroid, so with the section artificially frozen the output centroid can
    hardly move however far the energy travels — measured, 703 -> 703.

    So this now tests migration where migration lives, and then tests the
    audible consequence under the condition the object is actually in. That is
    two checks where there was one, and neither is weaker than the original.
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
'        const double cEarly = centroid ((int) (0.4 * fs), (int) (0.9 * fs));',
'        const double cLate  = centroid ((int) (4.2 * fs), (int) (4.9 * fs));',
'        std::printf ("   centroid %.0f Hz early -> %.0f Hz late\\n", cEarly, cLate);',
'        CHECK (std::fabs (cLate - cEarly) > 0.04 * std::max (cEarly, 1.0),',
'               "no audible migration (%.0f -> %.0f)", cEarly, cLate);']),
    J([
'        const double cEarly = centroid ((int) (0.4 * fs), (int) (0.9 * fs));',
'        const double cLate  = centroid ((int) (4.2 * fs), (int) (4.9 * fs));',
'        std::printf ("   centroid %.0f Hz early -> %.0f Hz late (section frozen)\\n",',
'                     cEarly, cLate);',
'',
'        /*  THE MECHANISM ITSELF: where the energy is, mode by mode. This is',
'            what migration moves, and it cannot be pinned by the section. */',
'        {',
'            Engine m;',
'            m.p.metabolism = 0.85f; m.p.transit = 0; m.p.wake = 0;',
'            m.loadSpecimen (30);',
'            m.prepare (fs, 512);',
'            std::vector<float> l2 (512), r2 (512);',
'            m.noteOn (50, 0.9f);',
'            auto snap = [&] (std::vector<double>& out)',
'            {',
'                out.assign ((size_t) std::min (32, m.specimen().nModes), 0.0);',
'                for (size_t k = 0; k < out.size(); ++k)',
'                    out[k] = m.debugModeEnergy (0, (int) k);',
'            };',
'            auto advance = [&] (double secs)',
'            {',
'                const int nb = (int) (secs * fs / 512);',
'                for (int b = 0; b < nb; ++b) m.process (l2.data(), r2.data(), 512);',
'            };',
'            advance (0.6);',
'            std::vector<double> e0; snap (e0);',
'            advance (4.0);',
'            std::vector<double> e1; snap (e1);',
'            //  how far the distribution moved, as a share of its own size',
'            double num = 0, d0 = 0, d1 = 0;',
'            for (size_t k = 0; k < e0.size(); ++k)',
'            { num += std::fabs (e1[k] - e0[k]); d0 += e0[k]; d1 += e1[k]; }',
'            const double travelled = num / std::max (1e-12, 0.5 * (d0 + d1));',
'            std::printf ("   energy distribution travelled %.1f %% of itself\\n",',
'                         100.0 * travelled);',
'            CHECK (travelled > 0.10,',
'                   "energy does not migrate along the anatomy (%.1f %%)",',
'                   100.0 * travelled);',
'        }',
'',
'        /*  AND THE AUDIBLE CONSEQUENCE, under the condition the object is',
'            actually in: the section is never frozen in use — it patrols, and',
'            everything played moves it. */',
'        {',
'            Engine a;',
'            a.p.metabolism = 0.85f;',
'            a.loadSpecimen (30);',
'            a.prepare (fs, 512);',
'            const int NA = 30 * 48000;',
'            std::vector<float> AL ((size_t) NA), AR ((size_t) NA);',
'            render (a, { { 0, 0, 50, 0.9f } }, AL.data(), AR.data(), NA);',
'            auto rmsF = [&] (int from, int to)',
'            {',
'                double se = 0, sd = 0;',
'                for (int i = from + 1; i < to; ++i)',
'                { const double x = AL[(size_t) i], d = x - AL[(size_t) i - 1];',
'                  se += x * x; sd += d * d; }',
'                return se > 1e-30 ? (fs / (2.0 * 3.14159265358979)) * std::sqrt (sd / se) : 0.0;',
'            };',
'            const double a0 = rmsF ((int) (0.4 * fs), (int) (1.4 * fs));',
'            const double a1 = rmsF ((int) (27.0 * fs), (int) (29.0 * fs));',
'            const double moved = std::fabs (a1 - a0) / std::max (1.0, a0);',
'            std::printf ("   colour of a held note over 29 s: %.0f -> %.0f Hz (%.1f %%)\\n",',
'                         a0, a1, 100.0 * moved);',
'            CHECK (moved > 0.10,',
'                   "a held note does not travel in use (%.1f %%)", 100.0 * moved);',
'        }']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log('bench check 11 rewritten: mechanism + audible consequence');
