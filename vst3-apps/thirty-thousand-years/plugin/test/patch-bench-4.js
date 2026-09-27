// A click bench: jump CUTOFF, LEVEL and the SPACE amount mid-note and compare the
// largest sample step against the settled chain's own largest step (the BWFX
// transition-echo rule: never an absolute bound).
const fs = require("fs");
const path = "C:/Users/peter/b/ThirtyThousandYears/test/bench.cpp";
let s = fs.readFileSync(path, "utf8");
const anchor = "static void testCost()";
if (s.split(anchor).length !== 2) { console.log("ANCHOR MISS"); process.exit(1); }
const add = `static float maxStep (const Take& t, int from, int to)
{
    float m = 0.0f;
    for (int i = std::max (1, from); i < to; ++i) m = std::max (m, std::max (std::abs (t.L[(size_t) i] - t.L[(size_t) i - 1]), std::abs (t.R[(size_t) i] - t.R[(size_t) i - 1])));
    return m;
}

static void testClicks()
{
    std::printf ("\\n[10b] clicks: a parameter jump mid-note against the settled chain\\n");
    struct J { const char* what; int id; float from, to; };
    J jumps[] = { { "CUTOFF 300 Hz -> 5 kHz", P_m_cut, 0.39f, 0.8f }, { "MASS LEVEL -6 -> 0 dB", P_m_gain, 0.5f, 0.7f },
                  { "SPACE AMOUNT 0 -> 60 %", P_e_rv_mix, 0.0f, 0.6f }, { "OSC 2 FINE +3 -> +40 c", P_m_o2fine, 0.53f, 0.7f },
                  { "HISTORY 0 -> 1 (two scenes, cutoff 0.3 -> 0.7)", P_h_pos, 0.0f, 1.0f } };
    for (const J& j : jumps)
    {
        Engine* e = fresh (48000.0, 16);   // COLD START: saws, no space
        e->p[P_m_a_atk] = 0.0f; e->p[j.id] = j.from;
        if (j.id == P_h_pos) { e->scene[0] = e->p; e->scene[0][P_m_cut] = 0.3f; e->sceneSet[0] = true; e->scene[3] = e->p; e->scene[3][P_m_cut] = 0.7f; e->sceneSet[3] = true; e->p[P_h_on] = 1; }
        e->noteOn (45, 0.8f); render (*e, 1.0);
        Take settled = render (*e, 1.0);
        e->p[j.id] = j.to;
        Take jumped = render (*e, 0.3);
        const float base = maxStep (settled, 0, settled.n()), after = maxStep (jumped, 0, jumped.n());
        check (after <= base * 2.5f + 0.02f, j.what, fmt ("settled max step %.4f, across the jump %.4f", base, after).c_str());
        delete e;
    }
}

`;
s = s.replace(anchor, add + anchor);
s = s.replace("testPanicDeterminismRates(); testRandomAndSoak(); testCost();", "testPanicDeterminismRates(); testRandomAndSoak(); testClicks(); testCost();");
fs.writeFileSync(path, s);
console.log("click bench added");
