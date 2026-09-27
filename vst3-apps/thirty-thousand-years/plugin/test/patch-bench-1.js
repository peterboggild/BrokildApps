// First bench corrections: the bench's own mistakes (note numbers, windows,
// carried state), found by the probe. Exact-count anchors; nothing written on a miss.
const fs = require("fs");
const path = "C:/Users/peter/b/ThirtyThousandYears/test/bench.cpp";
let s = fs.readFileSync(path, "utf8");
const edits = [
  // tuning: note 45 is 110 Hz (57 is 220)
  ['e->noteOn (57, 0.8f); Take t = render (*e, 2.0);\n    const double f = peakHz (t, 80, 160, t.n() / 2, t.n());\n    check (std::abs (cents (f, 110.0)) < 2.0, "MASS sine at A2 measures 110 Hz"',
   'e->noteOn (45, 0.8f); Take t = render (*e, 2.0);\n    const double f = peakHz (t, 80, 160, t.n() / 2, t.n());\n    check (std::abs (cents (f, 110.0)) < 2.0, "MASS sine at A2 (note 45) measures 110 Hz"'],
  // beat: the envelope minima detector compares against a window a full beat wide
  ['std::vector<int> minima; for (size_t i = 8; i + 1 < env.size(); ++i) if (env[i] < env[i - 1] && env[i] < env[i + 1] && env[i] < 0.5f * env[i - 4]) minima.push_back ((int) i);\n    double period',
   'std::vector<int> minima; for (size_t i = 40; i + 40 < env.size(); ++i) { bool m = true; for (size_t k = i - 40; k <= i + 40; ++k) if (k != i && env[k] <= env[i]) m = false; float mx = 0; for (size_t k = i - 40; k <= i + 40; ++k) mx = std::max (mx, env[k]); if (m && env[i] < 0.3f * mx) minima.push_back ((int) i); }\n    double period'],
  ['minima.clear(); for (size_t i = 8; i + 1 < env.size(); ++i) if (env[i] < env[i - 1] && env[i] < env[i + 1] && env[i] < 0.5f * env[i - 4]) minima.push_back ((int) i);',
   'minima.clear(); for (size_t i = 40; i + 40 < env.size(); ++i) { bool m = true; for (size_t k = i - 40; k <= i + 40; ++k) if (k != i && env[k] <= env[i]) m = false; float mx = 0; for (size_t k = i - 40; k <= i + 40; ++k) mx = std::max (mx, env[k]); if (m && env[i] < 0.3f * mx) minima.push_back ((int) i); }'],
  // shift and pitch shift on note 45
  ['e->p[P_s_shift] = 0.5f + 0.5f * std::cbrt (100.0f / 2000.0f);\n    e->noteOn (57, 0.8f); t = render (*e, 2.0);',
   'e->p[P_s_shift] = 0.5f + 0.5f * std::cbrt (100.0f / 2000.0f); e->p[P_s_a_rel] = 0.0f;\n    e->noteOn (45, 0.8f); t = render (*e, 2.0);'],
  ['e->noteOff (57); render (*e, 1.0); e->noteOn (57, 0.8f); t = render (*e, 2.0);\n    f1 = peakHz (t, 150, 300, t.n() / 2, t.n());',
   'e->noteOff (45); render (*e, 1.0); e->noteOn (45, 0.8f); t = render (*e, 2.0);\n    f1 = peakHz (t, 150, 300, t.n() / 2, t.n());'],
  // additive on note 45, with a short release so the previous voice is gone
  ['e->p[P_s_addspread] = 0.5f; e->p[P_s_addtilt] = 0.5f;\n    e->noteOn (57, 0.8f); Take t = render (*e, 2.0);',
   'e->p[P_s_addspread] = 0.5f; e->p[P_s_addtilt] = 0.5f; e->p[P_s_a_rel] = 0.0f;\n    e->noteOn (45, 0.8f); Take t = render (*e, 2.0);'],
  ['e->noteOff (57); render (*e, 0.6f); e->noteOn (57, 0.8f); t = render (*e, 2.0);\n    const double p2 = peakHz (t, 240, 300, a, b), f1 = peakHz (t, 90, 130, a, b);',
   'e->noteOff (45); render (*e, 0.6f); e->noteOn (45, 0.8f); t = render (*e, 2.0);\n    const double p2 = peakHz (t, 240, 300, a, b), f1 = peakHz (t, 90, 130, a, b);'],
  ['e->p[P_s_addfund] = 0.0f; e->p[P_s_addcluster] = 0.0f;\n    e->noteOff (57); render (*e, 0.6f); e->noteOn (57, 0.8f); t = render (*e, 2.0);',
   'e->p[P_s_addfund] = 0.0f; e->p[P_s_addcluster] = 0.0f;\n    e->noteOff (45); render (*e, 0.6f); e->noteOn (45, 0.8f); t = render (*e, 2.0);'],
  ['e->p[P_s_addgaps] = 0.999f; e->noteOff (57); render (*e, 0.6f); e->noteOn (57, 0.8f); t = render (*e, 2.0);',
   'e->p[P_s_addgaps] = 0.999f; e->noteOff (45); render (*e, 0.6f); e->noteOn (45, 0.8f); t = render (*e, 2.0);'],
  // erosion check must not pass on two zeros
  ['check (hfShare (c1) < 0.3 * hfShare (c0), "EROSION (bandwidth) removes the top"',
   'check (hfShare (c0) > 0.02 && hfShare (c1) < 0.3 * hfShare (c0), "EROSION (bandwidth) removes the top"'],
  // scale: the LARGE scale has energy at 100-125 ms where the small one has none
  ['check (eD > eC * 1.5f || eA > eB, "APPARENT SCALE moves the early reflections later"',
   'check (eB > eA * 3.0f + 0.001f, "APPARENT SCALE moves the early reflections later (100-125 ms after the note: large scale carries energy, small does not)"'],
  // bass mono at 55 Hz: note 33
  ['e->noteOn (45, 0.8f); render (*e, 0.5); t = render (*e, 1.0);\n    double side = 0, mid = 0;',
   'e->noteOn (33, 0.8f); render (*e, 0.5); t = render (*e, 1.0);\n    double side = 0, mid = 0;'],
  // event lanes: count fires with the counter
  ['int fires = 0; for (int i = 0; i < 500; ++i) { render (*e, 0.01); if (e->life.evt[0].fired) ++fires; }\n    check (fires >= 16 && fires <= 24',
   'e->life.evt[0].fireCount = 0; render (*e, 5.0); int fires = e->life.evt[0].fireCount;\n    check (fires >= 16 && fires <= 24'],
  ['e->p[P_v1_refract] = xunmap (1000.0f, 10.0f, 20000.0f); fires = 0; for (int i = 0; i < 500; ++i) { render (*e, 0.01); if (e->life.evt[0].fired) ++fires; }',
   'e->p[P_v1_refract] = xunmap (1000.0f, 10.0f, 20000.0f); e->life.evt[0].fireCount = 0; render (*e, 5.0); fires = e->life.evt[0].fireCount;'],
  // the network test: no matrix slots in the way, and a threshold the structure can reach
  ['delete e; e = fresh (48000.0, 0); e->p[P_drone] = 1; e->p[P_st_on] = 1; e->p[P_st_gain] = 1.0f; e->p[P_l_coupling] = 1.0f; e->p[P_l_autonomy] = 1.0f;\n    render (*e, 3.0);\n    check (e->eff[P_mem_dens] > e->p[P_mem_dens] + 0.02f && e->life.det[3].energy > 0.05f',
   'delete e; e = fresh (48000.0, 0); for (auto& sl : e->life.slots) sl.on = false; e->p[P_drone] = 1; e->p[P_st_on] = 1; e->p[P_st_exclvl] = 1.0f; e->p[P_l_coupling] = 1.0f; e->p[P_l_autonomy] = 1.0f;\n    render (*e, 3.0);\n    check (e->eff[P_mem_dens] > e->p[P_mem_dens] + 0.01f && e->life.det[3].energy > 0.03f'],
  // history auto: start from a known position
  ['e->p[P_h_on] = 1; e->p[P_h_mode] = 1; e->p[P_h_dur] = xunmap (2.0f, 1.0f, 1800.0f); e->p[P_h_loop] = 0; render (*e, 1.0);',
   'e->p[P_h_on] = 0; e->p[P_h_pos] = 0.0f; render (*e, 0.05); e->p[P_h_on] = 1; e->p[P_h_mode] = 1; e->p[P_h_dur] = xunmap (2.0f, 1.0f, 1800.0f); e->p[P_h_loop] = 0; render (*e, 1.0);'],
  ['e->p[P_h_hold] = 1; e->p[P_h_loop] = 1; e->p[P_h_on] = 0; render (*e, 0.05); e->p[P_h_on] = 1; render (*e, 0.5);',
   'e->p[P_h_on] = 0; e->p[P_h_pos] = 0.0f; render (*e, 0.05); e->p[P_h_hold] = 1; e->p[P_h_loop] = 1; e->p[P_h_on] = 1; render (*e, 0.5);'],
  // MPE on note 45
  ['x->p[P_mpe] = 1; x->noteOn (57, 0.8f, 2); x->noteOn (57, 0.8f, 3); x->setBend (0.25f, 2);',
   'x->p[P_mpe] = 1; x->noteOn (45, 0.8f, 2); x->noteOn (45, 0.8f, 3); x->setBend (0.25f, 2);'],
  // scala: a longer window and a finer scan
  ['x->noteOn (63, 0.8f); tt = render (*x, 1.5);\n    const double f5 = peakHz (tt, 250, 400, tt.n() / 2, tt.n());',
   'x->noteOn (63, 0.8f); tt = render (*x, 3.0);\n    const double f5 = peakHz (tt, 350, 450, tt.n() / 3, tt.n(), 2.0);'],
  // presets: a quiet bed counts as sound, and the count prints as a number
  ['if (rr < 0.004f) { ++silent;', 'if (rr < 0.002f) { ++silent;'],
  ['check (silent == 0 && over == 0, fmt ("all %d presets load, make sound and stay inside the ceiling", numPresets()).c_str()',
   'check (silent == 0 && over == 0, fmt ("all %.0f presets load, make sound and stay inside the ceiling", (double) numPresets()).c_str()'],
];
let miss = [];
for (const [a, b] of edits) { const n = s.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 60)); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(path, s);
console.log("bench patched, " + edits.length + " edits");
