// CHUNK B — the sequencer.
//
// Derived from the host's PPQ rather than counted, so relocating, looping and
// tempo changes in the DAW are handled for free and a step always lands where
// the bar says it should. Each lane has its own length and division, which is
// what makes polymeter free rather than a feature. THE HAND rides on top.
"use strict";
const fs = require("fs");
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  for (const [a, b, tag] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(path.split("/").pop() + ": " + tag + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(path, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/Source/";

const writeH = edit(R + "Engine.h", [
[`    // transport, for the sequencer
    double bpmNow = 120.0, ppqNow = 0.0;
    bool   playing = false;`,
`    // transport, for the sequencer
    double bpmNow = 120.0, ppqNow = 0.0, ppqFree = 0.0;
    bool   playing = false;`, "ppqFree"]
]);

const writeC = edit(R + "Engine.cpp", [
[`//==============================================================================
void Engine::process (float* L, float* R, int n, float* const* aux)
{
    updateOs();`,
`//==============================================================================
//  THE SEQUENCER
//
//  Everything below is derived from the transport position, not counted from
//  the last block: a step's time is a function of the bar, so the DAW can
//  loop, relocate or change tempo and the pattern stays where it belongs.
//==============================================================================
namespace
{
    // step length in sixteenths, per division setting
    const double STEPLEN[4] = { 0.5, 1.0, 2.0, 4.0 };

    int laneIndex (const Lane& L, long long k)
    {
        const int len = L.len < 1 ? 1 : (L.len > NSTEP ? NSTEP : L.len);
        long long m = k % len; if (m < 0) m += len;
        switch (L.dir)
        {
            case 1: return (int) (len - 1 - m);                       // reverse
            case 2:                                                    // pendulum
            {
                const int span = len > 1 ? (len - 1) * 2 : 1;
                long long q = k % span; if (q < 0) q += span;
                return (int) (q < len ? q : span - q);
            }
            case 3:                                                    // random, but repeatable
            {
                Rng r; r.seed ((uint32_t) (k * 2654435761u + 12345u));
                return (int) (r.uni() * (float) len) % len;
            }
            default: return (int) m;
        }
    }
}

void Engine::setTransport (double bpm, double ppq, bool play)
{
    if (bpm > 20.0 && bpm < 999.0) bpmNow = bpm;
    if (play && ppq >= 0.0) ppqNow = ppq;
    playing = play && ppq >= 0.0;
}

void Engine::runSequencer (int n)
{
    if (p.g[GP_SEQ] < 0.5f) { uiStep = -1; return; }

    /*  With no host transport — the standalone, or a stopped DAW — the
        sequencer runs on its own clock so the machine is playable on its own.
        With one, the host's position wins outright. */
    const double bpm = playing ? bpmNow : (double) xmap (p.g[GP_TEMPO], 40.0f, 300.0f);
    const double beats = (double) n / fs * bpm / 60.0;
    double a, b;
    if (playing) { a = ppqNow; b = ppqNow + beats; }
    else         { a = ppqFree; b = ppqFree + beats; ppqFree = b; }

    uiStep = (int) (((long long) std::floor (a * 4.0)) % NSTEP);
    if (uiStep < 0) uiStep += NSTEP;

    const Pattern& P = pat[curPat < 0 ? 0 : (curPat >= NPAT ? NPAT - 1 : curPat)];
    const float gSwing = (p.g[GP_SWING] - 0.5f) * 2.0f;      // -1..+1, 0 = straight
    const float feel   = clamp01 (p.g[GP_FEEL]);
    const float grip   = clamp01 (p.g[GP_GRIP]);

    for (int c = 0; c < NCH; ++c)
    {
        const Lane& L = P.lane[c];
        if (L.mute || p.ch[c][CP_MUTE] >= 0.5f) continue;

        const double sl = STEPLEN[L.div & 3];
        const double sa = a * 4.0 / sl, sb = b * 4.0 / sl;
        const double stepSamples = sl / 4.0 * 60.0 / bpm * fs;

        /*  Swing delays the ODD steps only — that is what swing is. The lane's
            own value is an offset from the global one, so hats can shuffle
            over a straight kick, which is how grooves are actually built. */
        const double sw = clampf (gSwing * 0.66f + (float) L.swing / 100.0f, -0.45f, 0.45f);

        for (long long k = (long long) std::floor (sa) - 2; k <= (long long) std::floor (sb) + 2; ++k)
        {
            if (k < 0) continue;
            const int idx = laneIndex (L, k);
            const Step& st = L.step[idx];
            if (! st.on) continue;

            double fire = (double) k + ((k & 1) ? sw : 0.0) + (double) st.micro / 100.0;

            /*  THE HAND. Not random jitter — a player leans. Downbeats arrive
                a touch early, the offbeats between them lean late, and the
                amount is one knob. */
            if (feel > 0.0f)
            {
                const long long m = ((k % 4) + 4) % 4;
                const double lean = (m == 0) ? -0.020 : (m == 2 ? 0.008 : 0.034);
                fire += lean * feel;
            }

            if (fire < sa || fire >= sb) continue;

            // probability and conditions, deterministic so a render repeats
            Rng r; r.seed ((uint32_t) (k * 2654435761u + (uint32_t) c * 40503u + 7u));
            if (st.prob < 100 && r.uni() * 100.0f >= (float) st.prob) { lastVel[c] = 0.0f; continue; }

            const long long loop = k / (L.len < 1 ? 1 : L.len);
            bool play = true;
            switch (st.cond)
            {
                case 1: play = (loop % 2) == 0; break;                 // 1:2
                case 2: play = (loop % 3) == 0; break;                 // 1:3
                case 3: play = (loop % 4) == 0; break;                 // 1:4
                case 4: play = lastVel[c] > 0.0f; break;               // PREV played
                case 5: play = lastVel[c] <= 0.0f; break;              // PREV did not
                default: break;
            }
            if (! play) continue;

            float vel = (float) st.vel / 127.0f;
            //  GRIP: a limb carries something of its last hit into this one
            if (grip > 0.0f && lastVel[c] > 0.0f)
                vel = lerpf (vel, 0.5f * (vel + lastVel[c]), grip * 0.7f);
            vel = clampf (vel, 0.02f, 1.0f);
            lastVel[c] = vel;

            const double frac = (fire - sa) / std::max (1e-9, sb - sa);
            const int at = (int) (frac * (double) n) * osFactor;

            const int rt = st.ratchet < 1 ? 1 : (st.ratchet > 8 ? 8 : st.ratchet);
            for (int rr2 = 0; rr2 < rt; ++rr2)
            {
                // a ratchet ramps down in level, the way a real roll does
                const float rv = vel * (1.0f - 0.06f * (float) rr2);
                scheduleHit (c, rv, 0.0f, at + (int) (stepSamples * osFactor * rr2 / rt));
            }
        }
    }
}

//==============================================================================
void Engine::process (float* L, float* R, int n, float* const* aux)
{
    updateOs();
    runSequencer (n);`, "sequencer"],

// the host position must advance across the block for the next one
[`    for (int c = 0; c < NCH; ++c)
        meter[(size_t) c] = std::max (chMeter[(size_t) c], meter[(size_t) c] * 0.86f);

    // anything still queued belongs to the next block
    for (int i = 0; i < nHits; ++i) hits[(size_t) i].at -= n * osFactor;`,
`    for (int c = 0; c < NCH; ++c)
        meter[(size_t) c] = std::max (chMeter[(size_t) c], meter[(size_t) c] * 0.86f);

    // anything still queued belongs to the next block
    for (int i = 0; i < nHits; ++i) hits[(size_t) i].at -= n * osFactor;

    /*  Carry the transport forward, so a host that only reports its position
        once per block (or a bench that never reports one at all) still gets a
        continuous timeline rather than the same bar over and over. */
    if (playing) ppqNow += (double) n / fs * bpmNow / 60.0;`, "advance ppq"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
writeH(); writeC();
console.log("chunk B patched OK — the sequencer");
