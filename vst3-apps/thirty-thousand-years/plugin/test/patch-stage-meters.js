/*  Stage meters. Peter: "many presets seem to contain clipping effects although
    the total output is below 0 dB" - which is exactly what a stage saturating
    UPSTREAM of the limiter sounds like. Guessing which one is not a method, so
    every stage gets a peak hold and the probe prints the lot.

    Cheap: one fabs/max per sample per stage, on buffers that are in cache.
*/
const fs = require("fs");
const H = "C:/Users/peter/b/ThirtyThousandYears/Source/Engine.h";
const C = "C:/Users/peter/b/ThirtyThousandYears/Source/Engine.cpp";
let h = fs.readFileSync(H, "utf8");
let c = fs.readFileSync(C, "utf8");

/* ---- the header ------------------------------------------------------- */
const ha = `    float scope[SCOPE_N] = {}; int scopeWrite = 0;`;
const hb = `    float scope[SCOPE_N] = {}; int scopeWrite = 0;
    /*  Peak hold per internal stage, so a stage that saturates while the
        output stays polite can be SEEN. Order is signal flow; names in
        stageName(). Decays 20 %/block so it follows without flickering. */
    enum { ST_MASS, ST_SIGNAL, ST_MEMORY, ST_STRUCT, ST_MIX_STRIP, ST_MIX_LANEA,
           ST_SEND, ST_SEND_LANEB, ST_SPACE_WET, ST_LOOP_SEND, ST_LOOP_RET,
           ST_PRE_LIMIT, ST_OUT, NUM_STAGES };
    float stagePeak[NUM_STAGES] = {};
    static const char* stageName (int i);`;
if (h.split(ha).length !== 2) { console.log("H ANCHOR MISS"); process.exit(1); }
h = h.replace(ha, hb);

/* ---- the source ------------------------------------------------------- */
const edits = [
  [`//==============================================================================
int chordIntervals (int chord, int* o)`,
   `//==============================================================================
const char* Engine::stageName (int i)
{
    static const char* N[NUM_STAGES] = { "MASS", "SIGNAL", "MEMORY", "STRUCT", "mix+strip", "mix+laneA",
                                         "send", "send+laneB", "space wet", "loop send", "loop ret",
                                         "pre-limit", "out" };
    return N[i < 0 ? 0 : (i >= NUM_STAGES ? NUM_STAGES - 1 : i)];
}

/*  A stage's peak over this sub-block, held with a slow decay. */
static inline void stageHit (float& hold, const float* a, const float* b, int n)
{
    float p = 0.0f;
    for (int i = 0; i < n; ++i) { p = std::max (p, std::abs (a[i])); if (b) p = std::max (p, std::abs (b[i])); }
    hold = std::max (p, hold * 0.98f);
}

int chordIntervals (int chord, int* o)`],
  // the four strata, before the strips
  [`        // detectors (before the strips, so a muted channel still informs the network)
        {`,
   `        for (int b = 0; b < 4; ++b) stageHit (stagePeak[ST_MASS + b], busL[b], busR[b], m);
        // detectors (before the strips, so a muted channel still informs the network)
        {`],
  // after the strips
  [`        // ---- the feedback loop node
        {
            const float gs = eff[P_e_fb_send];`,
   `        stageHit (stagePeak[ST_MIX_STRIP], mixL, mixR, m);
        stageHit (stagePeak[ST_SEND], sendL, sendR, m);
        // ---- the feedback loop node
        {
            const float gs = eff[P_e_fb_send];`],
  [`            loop.process (loopSendL, loopSendR, loopRetL, loopRetR, m, eff);`,
   `            stageHit (stagePeak[ST_LOOP_SEND], loopSendL, loopSendR, m);
            loop.process (loopSendL, loopSendR, loopRetL, loopRetR, m, eff);
            stageHit (stagePeak[ST_LOOP_RET], loopRetL, loopRetR, m);`],
  [`        laneA.process (mixL, mixR, m, eff, P_e_a1, P_e_a2, P_e_a3);
        laneB.process (sendL, sendR, m, eff, P_e_b1, P_e_b2, P_e_b3);`,
   `        laneA.process (mixL, mixR, m, eff, P_e_a1, P_e_a2, P_e_a3);
        stageHit (stagePeak[ST_MIX_LANEA], mixL, mixR, m);
        laneB.process (sendL, sendR, m, eff, P_e_b1, P_e_b2, P_e_b3);
        stageHit (stagePeak[ST_SEND_LANEB], sendL, sendR, m);`],
  [`        space.process (sendL, sendR, wetL, wetR, m, eff, eff[P_e_scale]);
        spaceEnergy = space.energy;`,
   `        space.process (sendL, sendR, wetL, wetR, m, eff, eff[P_e_scale]);
        stageHit (stagePeak[ST_SPACE_WET], wetL, wetR, m);
        spaceEnergy = space.energy;`],
  [`        if (stopped) { std::fill (mixL, mixL + m, 0.0f); std::fill (mixR, mixR + m, 0.0f); }
        out.process (mixL, mixR, m, p, duckGain);`,
   `        if (stopped) { std::fill (mixL, mixL + m, 0.0f); std::fill (mixR, mixR + m, 0.0f); }
        stageHit (stagePeak[ST_PRE_LIMIT], mixL, mixR, m);
        out.process (mixL, mixR, m, p, duckGain);
        stageHit (stagePeak[ST_OUT], mixL, mixR, m);`],
];

let miss = [];
for (const [a] of edits) { const n = c.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 60).replace(/\n/g, " ")); }
if (miss.length) { console.log("C ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) c = c.replace(a, b);

fs.writeFileSync(H, h);
fs.writeFileSync(C, c);
console.log("stage meters added");
