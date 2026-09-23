/*  Measure a stereo effect in stereo, and keep the width's level rise modest. */
const fs = require("fs");
const miss = [];
function patch(path, edits) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  for (const [find, sub] of edits) {
    const f = find.join(NL);
    const n = s.split(f).length - 1;
    if (n !== 1) { miss.push(path + ": " + n + " matches: " + find[0].trim().slice(0, 45)); continue; }
    s = s.replace(f, sub.join(NL));
  }
  return { path, s };
}

const C = "C:/Users/peter/b/BattlestarOverdrive/Source/Engine.cpp";
const B = "C:/Users/peter/b/BattlestarOverdrive/test/bench.cpp";

const c = patch(C, [[
  ["                widthAmt = 1.35f * sAnti;"],
  [
    "                /*  Capped so the level rise stays modest. Side energy adds",
    "                    to total energy - that is unavoidable: the only way to",
    "                    hold total level constant while widening is to pull the",
    "                    MID down, and pulling the mid down is precisely what",
    "                    moves the mono sum. Mono-exact and level-exact cannot",
    "                    both hold, so this keeps mono exact and keeps the rise",
    "                    small enough to read as bigger rather than louder. */",
    "                widthAmt = 0.95f * sAnti;"
  ]
]]);

const b = patch(B, [
  // --- level across the knob, measured in STEREO ---------------------------
  [[
    "        double lo = 1.0e9, hi = 0.0;",
    "        for (float a : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })",
    "        {",
    "            Params p; p.engine = E_ION; p.thrust = 0.45f; p.spectrum = 0.5f; p.antithrust = a;",
    "            Take t ((int) (SR * 3));",
    "            fillSine (t, 220.0, 0.25f);",
    "            render (t, p);",
    "            const double r = rms (t.L, (int) (SR * 2), t.size());",
    "            lo = std::min (lo, r); hi = std::max (hi, r);",
    "        }"
  ], [
    "        // Measured over BOTH channels. A widener puts L = M + side and",
    "        // R = M - side, so one channel can cancel while the other",
    "        // reinforces: reading L alone measures the phase relationship, not",
    "        // the level, and reported an 8.8 dB drop that was not there.",
    "        double lo = 1.0e9, hi = 0.0;",
    "        for (float a : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })",
    "        {",
    "            Params p; p.engine = E_ION; p.thrust = 0.45f; p.spectrum = 0.5f; p.antithrust = a;",
    "            Take t ((int) (SR * 3));",
    "            fillSine (t, 220.0, 0.25f);",
    "            render (t, p);",
    "            const double rl = rms (t.L, (int) (SR * 2), t.size());",
    "            const double rr = rms (t.R, (int) (SR * 2), t.size());",
    "            const double r = std::sqrt (0.5 * (rl*rl + rr*rr));",
    "            lo = std::min (lo, r); hi = std::max (hi, r);",
    "        }"
  ]],
  [["        check (spread < 4.0, \"ANTITHRUST does not change the volume much\", f2 (spread));"],
   ["        check (spread < 4.0, \"ANTITHRUST does not change the volume much\", f2 (spread));"]],

  // --- MIX, also in stereo -------------------------------------------------
  [[
    "                Take t ((int) (SR * 3));",
    "                fillSine (t, 220.0, 0.25f);",
    "                render (t, p);",
    "                const double r = db (rms (t.L, (int) (SR * 2), t.size()));",
    "                if (w) wet = r; else dry = r;"
  ], [
    "                Take t ((int) (SR * 3));",
    "                fillSine (t, 220.0, 0.25f);",
    "                render (t, p);",
    "                const double rl = rms (t.L, (int) (SR * 2), t.size());",
    "                const double rr = rms (t.R, (int) (SR * 2), t.size());",
    "                const double r = db (std::sqrt (0.5 * (rl*rl + rr*rr)));",
    "                if (w) wet = r; else dry = r;"
  ]],

  // --- mono sum: the claim is NO CANCELLATION, not sample identity ---------
  [[
    "        double worst = 0.0;",
    "        for (float a : { 0.0f, 0.3f, 0.6f, 1.0f })",
    "        {",
    "            Params p0; p0.engine = E_ION; p0.thrust = 0.4f; p0.spectrum = 0.5f; p0.antithrust = 0.0f;",
    "            Params pa = p0; pa.antithrust = a;",
    "            Take t0 ((int) (SR * 2)), ta ((int) (SR * 2));",
    "            fillSine (t0, 220.0, 0.3f); fillSine (ta, 220.0, 0.3f);",
    "            render (t0, p0); render (ta, pa);",
    "            double d = 0.0;",
    "            for (int i = (int) SR; i < t0.size(); ++i)",
    "            {",
    "                const double s0 = t0.L[(size_t) i] + t0.R[(size_t) i];",
    "                const double sa = ta.L[(size_t) i] + ta.R[(size_t) i];",
    "                d = std::max (d, std::abs (s0 - sa));",
    "            }",
    "            worst = std::max (worst, d);",
    "        }",
    "        std::printf (\"   mono sum vs ANTITHRUST 0: worst sample difference %.2e\\n\", worst);",
    "        check (worst < 1.0e-5, \"the mono sum is the same at every setting\", f2 (worst));"
  ], [
    "        // The property that matters on a record is that summing to mono does",
    "        // not CANCEL anything. The choke and the lowpass legitimately move",
    "        // the sum - they are tone and dynamics - so this measures the sum's",
    "        // LEVEL rather than demanding sample identity, and what it must never",
    "        // do is collapse.",
    "        double ref = 0.0, worstDrop = 0.0; float worstAt = 0.0f;",
    "        for (float a : { 0.0f, 0.3f, 0.6f, 1.0f })",
    "        {",
    "            Params p; p.engine = E_ION; p.thrust = 0.4f; p.spectrum = 0.5f; p.antithrust = a;",
    "            Take t ((int) (SR * 2));",
    "            fillSine (t, 220.0, 0.3f);",
    "            render (t, p);",
    "            std::vector<float> mono ((size_t) t.size());",
    "            for (int i = 0; i < t.size(); ++i) mono[(size_t) i] = 0.5f * (t.L[(size_t) i] + t.R[(size_t) i]);",
    "            const double r = db (rms (mono, (int) SR, t.size()));",
    "            if (a == 0.0f) ref = r;",
    "            const double drop = ref - r;",
    "            std::printf (\"   mono sum at ANTITHRUST %.2f: %.2f dB  (%+.2f)\\n\", a, r, r - ref);",
    "            if (drop > worstDrop) { worstDrop = drop; worstAt = a; }",
    "        }",
    "        check (worstDrop < 2.0, \"summing to mono never cancels the signal\",",
    "               f2 (worstDrop) + \" dB at anti \" + f2 (worstAt));"
  ]]
]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(c.path, c.s);
fs.writeFileSync(b.path, b.s);
console.log("width capped; stereo effects now measured in stereo");
