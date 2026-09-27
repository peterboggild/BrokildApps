// Engine (2026-09-25): a light follower per room (lamps follow the sound in
// their room) and acoustic art panels added into Eyring's A. Validates, then writes.
const fs = require('fs');
const path = require('path');
const src = path.join(__dirname, '..', 'Source');
const miss = [];
const files = {};
function load(f) { if (!files[f]) files[f] = fs.readFileSync(path.join(src, f), 'utf8'); }
function rep(f, a, b, count = 1) {
  load(f);
  const n = files[f].split(a).length - 1;
  if (n !== count) { miss.push(f + ': ' + JSON.stringify(a.slice(0, 80)) + ' x' + n); return; }
  files[f] = files[f].split(a).join(b);
}
const E = 'Engine.cpp', H = 'Engine.h';

// ---------------------------------------------------------------- header
rep(H, `    FurnItem furn[MAX_FURN];
    int nfurn = 0;`, `    FurnItem furn[MAX_FURN];
    int nfurn = 0;
    // acoustic art panels on the walls: their total face area per room, m^2
    float panelArea[NUM_ROOMS] = { 0, 0, 0 };`);
rep(H, `extern const FurnSpec FURN[NUM_FURN_TYPES];`, `extern const FurnSpec FURN[NUM_FURN_TYPES];
/*  An ACOUSTIC PANEL: artwork printed on a 5 cm fabric-wrapped absorber, the
    real product. Per unit area, in place of the wall it hangs on. A plain PRINT
    is visual only and never reaches the engine. */
extern const float PANEL_ALPHA[NBAND];`);
rep(H, `    float furnA[NUM_ROOMS] = {};           // absorption the furniture adds at 1 kHz, m^2 (net: a rug less the floor it covers)`,
`    float furnA[NUM_ROOMS] = {};           // absorption the furniture adds at 1 kHz, m^2 (net: a rug less the floor it covers)
    float light[NUM_ROOMS] = {};           // 0..1: how much sound is in each room right now (the lamps follow it)`);
rep(H, `    float roomScatter (int r) const { return furnScatter[r]; }`,
`    float roomScatter (int r) const { return furnScatter[r]; }
    // the light follower's output for room r, 0..1 (what the scene carries as light)
    float lightLevel (int r) const { return lightOut[r]; }`);
rep(H, `    float furnAbs1k[NUM_ROOMS] = {};`, `    float furnAbs1k[NUM_ROOMS] = {};
    float panelAreaAc[NUM_ROOMS] = { -1, -1, -1 };
    // the light follower: fast and slow envelopes per room, field energy gathered by tickFields
    float lightFast[NUM_ROOMS] = {}, lightSlow[NUM_ROOMS] = {}, lightOut[NUM_ROOMS] = {};
    double fieldAcc[NUM_ROOMS] = {};
    void updateLight (int n);`);

// ---------------------------------------------------------------- catalogue
rep(E, `const FurnSpec FURN[NUM_FURN_TYPES] =`, `const float PANEL_ALPHA[NBAND] = { 0.25f, 0.60f, 0.95f, 0.99f, 0.99f, 0.99f, 0.99f };

const FurnSpec FURN[NUM_FURN_TYPES] =`);

// ---------------------------------------------------------------- Eyring
rep(E, `                            float planArea = 0, float perimeter = 0,
                            const FurnItem* furn = nullptr, int nfurn = 0)`,
`                            float planArea = 0, float perimeter = 0,
                            const FurnItem* furn = nullptr, int nfurn = 0, float panelArea = 0)`);
rep(E, `            A += std::max (0.0f, a);
        }
`, `            A += std::max (0.0f, a);
        }
        // acoustic art panels: their absorber in place of the wall behind them
        if (panelArea > 0) A += panelArea * std::max (0.0f, PANEL_ALPHA[b] - MATERIAL_ALPHA[surf.wall][b]);
`);
rep(E, `roomAbsorption (r, surfNow[r], doorNow, abar, A, F.rt60, gg[r].area, gg[r].perimeter, cur.furn, nfurnNow);`,
       `roomAbsorption (r, surfNow[r], doorNow, abar, A, F.rt60, gg[r].area, gg[r].perimeter, cur.furn, nfurnNow, cur.panelArea[r]);`);
rep(E, `            if (nfurnNow > 0)
            {`, `            if (nfurnNow > 0 || cur.panelArea[r] > 0)
            {`);

// change detection: a panel added or resized re-runs the room acoustics
rep(E, `    if (! changed && ! sourcesChanged) return;

    // rebuild the plan of every room whose walls have moved`, `    for (int r = 0; r < NUM_ROOMS; ++r)
        if (cur.panelArea[r] != panelAreaAc[r]) { panelAreaAc[r] = cur.panelArea[r]; changed = true; }
    if (! changed && ! sourcesChanged) return;

    // rebuild the plan of every room whose walls have moved`);
rep(E, `            if (r == 0) { cur.nfurn = target.nfurn; for (int i = 0; i < MAX_FURN; ++i) cur.furn[i] = target.furn[i]; }`,
       `            if (r == 0) { cur.nfurn = target.nfurn; for (int i = 0; i < MAX_FURN; ++i) cur.furn[i] = target.furn[i]; }
            cur.panelArea[r] = target.panelArea[r];`);

// ---------------------------------------------------------------- the light follower
rep(E, `            F.y = y * 0.25f;
            F.feed.write (F.y);`, `            F.y = y * 0.25f;
            fieldAcc[r] += (double) F.y * F.y;
            F.feed.write (F.y);`);
rep(E, `    // ---- late fields first (their feeds are read by the door paths)
    tickFields (n);`, `    // ---- late fields first (their feeds are read by the door paths)
    tickFields (n);
    updateLight (n);`);
rep(E, `    sc.pathsDropped = pathsDropped;`, `    sc.pathsDropped = pathsDropped;
    for (int r = 0; r < NUM_ROOMS; ++r) sc.light[r] = lightOut[r];`);
rep(E, `//==============================================================================
// furniture geometry`, `//==============================================================================
/*  THE LIGHT. How much sound is in each room right now, for the lamps to follow:
    what its sources play (at their level) plus the room's own late field, as an
    RMS per sub-block. A fast envelope (5 ms up, 90 ms down) against a slow one
    (0.45 s) gives the PUNCH - an onset reads as fast/slow above 1 - and the
    fast one on a -48..-12 dB scale gives the LEVEL. The lamps take half of
    each, so a sustained pad glows and a drum hit flashes. Silence is exactly 0. */
void Engine::updateLight (int n)
{
    const float dt = (float) n / (float) fs;
    const float att = 1.0f - std::exp (-dt / 0.005f), rel = 1.0f - std::exp (-dt / 0.09f);
    const float slow = 1.0f - std::exp (-dt / 0.45f);
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        double e = fieldAcc[r]; fieldAcc[r] = 0;
        for (int s = 0; s < MAX_SOURCES; ++s)
        {
            if (! cur.src[s].active() || roomOf (srcPos[s].x, srcPos[s].y) != r) continue;
            const float* mi = monoIn[s].data();
            double a = 0; for (int i = 0; i < n; ++i) a += (double) mi[i] * mi[i];
            e += a * (double) srcLevel[s] * srcLevel[s];
        }
        const float rms = (float) std::sqrt (e / std::max (1, n));
        lightFast[r] += (rms > lightFast[r] ? att : rel) * (rms - lightFast[r]);
        lightSlow[r] += slow * (lightFast[r] - lightSlow[r]);
        if (lightFast[r] < 1.0e-7f) { lightOut[r] = 0.0f; continue; }
        const float level = std::max (0.0f, std::min (1.0f, (20.0f * std::log10 (lightFast[r]) + 48.0f) / 36.0f));
        const float punch = std::max (0.0f, std::min (1.0f, (lightFast[r] / (lightSlow[r] + 1.0e-6f) - 1.0f) / 1.5f));
        lightOut[r] = std::max (0.0f, std::min (1.0f, 0.5f * level + 0.5f * punch * std::min (1.0f, 2.0f * level)));
    }
}

//==============================================================================
// furniture geometry`);
// reset clears it
rep(E, `    inSq = outSq = directSq = revSq = 0;
    activePaths = 0;`, `    inSq = outSq = directSq = revSq = 0;
    activePaths = 0;
    for (int r = 0; r < NUM_ROOMS; ++r) { lightFast[r] = lightSlow[r] = lightOut[r] = 0; fieldAcc[r] = 0; }`);

if (miss.length) { console.log('NOT WRITTEN, anchors missed:\n  ' + miss.join('\n  ')); process.exit(1); }
for (const f of Object.keys(files)) fs.writeFileSync(path.join(src, f), files[f]);
console.log('written: ' + Object.keys(files).join(', '));
