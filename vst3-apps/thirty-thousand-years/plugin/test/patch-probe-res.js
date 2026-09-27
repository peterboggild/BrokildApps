// Measure the resonance LIFT (against the same patch at resonance zero), which
// is what "it sings" means, rather than an absolute peak that says nothing.
const fs = require("fs");
const P = "C:/Users/peter/b/ThirtyThousandYears/test/probe.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = "\n";
const a = `            std::printf ("  %-8s RESONANCE 100 %%: bus peak %5.2f   out peak %5.2f   rms %.3f` + String.fromCharCode(92) + `n",
                         fm == 0 ? "LADDER" : "SVF", e.stagePeak[Engine::ST_MASS], pk, rms (t.L));`;
const b = `            Engine e2; bareSine (e2); e2.prepare (48000.0, 512); bareSine (e2);
            e2.p[P_m_o1lvl] = 0.08f; e2.p[P_m_o2lvl] = 0; e2.p[P_m_sub] = 0;
            e2.p[P_m_fmode] = (float) fm; e2.p[P_m_res] = 0.0f; e2.p[P_m_cut] = 0.55f; e2.p[P_m_fdrive] = 0;
            e2.noteOn (45, 1.0f); render (e2, 2.0); Take t2 = render (e2, 1.0);
            std::printf ("  %-8s RESONANCE 100 %%: out peak %5.2f  rms %.4f  lift over RES 0 %+5.1f dB` + String.fromCharCode(92) + `n",
                         fm == 0 ? "LADDER" : "SVF", pk, rms (t.L),
                         20 * std::log10 ((rms (t.L) + 1e-9) / (rms (t2.L) + 1e-9)));`;
if (s.split(a).length !== 1 + 1) { console.log("ANCHOR MISS (" + (s.split(a).length - 1) + ")"); process.exit(1); }
s = s.replace(a, b);
const a2 = `            e.p[P_m_o1lvl] = 0; e.p[P_m_o2lvl] = 0; e.p[P_m_sub] = 0;`;
const b2 = `            /*  A noiseless digital ladder at k = 4 sits at exactly zero for ever:
                with no input there is nothing to start it, so measuring silence
                measures nothing. A quiet oscillator rings it, which is what a
                player hears as resonance anyway. */
            e.p[P_m_o1lvl] = 0.08f; e.p[P_m_o2lvl] = 0; e.p[P_m_sub] = 0;`;
if (s.split(a2).length !== 2) { console.log("ANCHOR MISS 2"); process.exit(1); }
s = s.replace(a2, b2);
fs.writeFileSync(P, s);
console.log("probe patched");
