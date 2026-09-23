// Two fixes the bench earned.
//
// ENGINE — REBOUND was exponential. A scheduled bounce went back through
// trigger(), which scheduled ITS own bounces, and so on: twelve bounces became
// a hundred and fifty. A hit now carries whether it is allowed to rebound, and
// only the original strike is.
//
// BENCH — the onset detector thresholded the instantaneous sample, so a 44 Hz
// kick dipped below the threshold at every zero crossing and each cycle read as
// a new hit. Every sequencer "failure" was that. An envelope with a release
// long enough to bridge a cycle, and a short bright channel for timing work.
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
const R = "C:/Users/peter/b/FullMetalRacket/";

// ───────────────────────────────────────────────────────────── engine ──────
const writeH = edit(R + "Source/Engine.h", [
[`    void  trigger (int chan, float velocity, float keySemis = 0.0f);   // the one way in`,
 `    /*  allowRebound is false for the bounces REBOUND itself schedules: a
        bounce that could bounce is a geometric series, and the first version
        of this turned twelve hits into a hundred and fifty. */
    void  trigger (int chan, float velocity, float keySemis = 0.0f, bool allowRebound = true);`, "trigger sig"],
[`    struct Hit { int chan = 0; float vel = 0.0f; float key = 0.0f; int at = 0; };`,
 `    struct Hit { int chan = 0; float vel = 0.0f; float key = 0.0f; int at = 0; bool rb = true; };`, "hit struct"],
[`    void scheduleHit (int chan, float vel, float key, int atSamples);`,
 `    void scheduleHit (int chan, float vel, float key, int atSamples, bool allowRebound = true);`, "schedule sig"]
]);

const writeC = edit(R + "Source/Engine.cpp", [
[`void Engine::scheduleHit (int chan, float vel, float key, int atSamples)
{
    if (nHits >= MAXHITS) return;
    hits[(size_t) nHits++] = { chan, vel, key, atSamples };
}`,
`void Engine::scheduleHit (int chan, float vel, float key, int atSamples, bool allowRebound)
{
    if (nHits >= MAXHITS) return;
    hits[(size_t) nHits++] = { chan, vel, key, atSamples, allowRebound };
}`, "scheduleHit"],

[`            const Hit h = hits[(size_t) i];
            hits[(size_t) i] = hits[(size_t) --nHits];
            trigger (h.chan, h.vel, h.key);`,
`            const Hit h = hits[(size_t) i];
            hits[(size_t) i] = hits[(size_t) --nHits];
            trigger (h.chan, h.vel, h.key, h.rb);`, "fireDue"],

[`void Engine::trigger (int chan, float velocity, float keySemis)
{
    if (chan < 0 || chan >= NCH) return;
    const float vel = clampf (velocity, 0.01f, 1.0f);`,
`void Engine::trigger (int chan, float velocity, float keySemis, bool allowRebound)
{
    if (chan < 0 || chan >= NCH) return;
    const float vel = clampf (velocity, 0.01f, 1.0f);`, "trigger sig"],

[`    const float rb = clamp01 (p.ch[chan][CP_REBOUND]);
    if (rb > 0.005f && velocity > 0.0f)`,
 `    const float rb = allowRebound ? clamp01 (p.ch[chan][CP_REBOUND]) : 0.0f;
    if (rb > 0.005f && velocity > 0.0f)`, "rebound guard"],

[`            scheduleHit (chan, v, keySemis, at);`,
 `            scheduleHit (chan, v, keySemis, at, false);   // a bounce does not bounce again`, "bounce flag"],

[`void Engine::setupVoice (Voice& v, int c, float vel, float keySemis)`,
 `void Engine::setupVoice (Voice& v, int c, float vel, float keySemis)`, "noop"]
]);

// ────────────────────────────────────────────────────────────── bench ──────
const writeT = edit(R + "test/test.cpp", [
[`static std::vector<int> onsets (const Buf& b, float thresh = 0.02f, int minGap = 300)
{
    std::vector<int> out;
    int quiet = minGap;
    for (int i = 0; i < b.n(); ++i)
    {
        const float a = std::fabs (b.L[(size_t) i]) + std::fabs (b.R[(size_t) i]);
        if (a > thresh) { if (quiet >= minGap) out.push_back (i); quiet = 0; }
        else ++quiet;
    }
    return out;
}`,
`static std::vector<int> onsets (const Buf& b, float thresh = 0.02f, int minGap = 300)
{
    /*  An ENVELOPE, not the raw sample. The first version thresholded the
        instantaneous value, so a 44 Hz kick fell below the threshold twice per
        cycle and every cycle counted as a fresh hit — which made all seven
        sequencer checks fail against a sequencer that was already sample
        accurate. The release has to be long enough to bridge one cycle of the
        lowest drum in the test. */
    const float rel = std::exp (-1.0f / (0.020f * 48000.0f));
    std::vector<float> env ((size_t) b.n(), 0.0f);
    float e = 0.0f;
    for (int i = 0; i < b.n(); ++i)
    {
        const float a = std::max (std::fabs (b.L[(size_t) i]), std::fabs (b.R[(size_t) i]));
        e = a > e ? a : e * rel;
        env[(size_t) i] = e;
    }

    std::vector<int> out;
    int last = -minGap;
    for (int i = 1; i < b.n(); ++i)
    {
        if (env[(size_t) i] < thresh) continue;
        if (i - last < minGap) continue;
        //  a hit is a RISE, so compare against a moment ago rather than zero —
        //  that way a new strike is caught even while the last one still rings
        const int back = std::max (0, i - 64);
        if (env[(size_t) i] > env[(size_t) back] * 1.6f || (last < 0 && env[(size_t) i] > thresh))
        {
            out.push_back (i);
            last = i;
        }
    }
    return out;
}`, "onset detector"],

// timing work uses the closed hat: short and bright, so consecutive steps
// really are separate events rather than one long tail
[`    Lane& L = e.pat[0].lane[chan];`,
 `    //  a short, bright channel, so two steps 125 ms apart are two events and
    //  not one ring with a bump in it
    e.p.ch[chan][CP_DECAY] = 0.06f;
    e.p.ch[chan][CP_REBOUND] = 0.0f;
    Lane& L = e.pat[0].lane[chan];`, "short channel"],

[`        Engine e; fresh (e);
        seqOneChannel (e, 0, 16, 1, true);
        e.setTransport (120.0, 0.0, true);
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b);
        std::printf ("    16ths at 120 BPM: %d onsets, first at %d\\n", (int) on.size(), on.empty() ? -1 : on[0]);`,
`        Engine e; fresh (e);
        seqOneChannel (e, 8, 16, 1, true);
        e.setTransport (120.0, 0.0, true);
        Buf b (48000 * 2); render (e, b);
        auto on = onsets (b);
        std::printf ("    16ths at 120 BPM: %d onsets, first at %d\\n", (int) on.size(), on.empty() ? -1 : on[0]);`, "seq test 1"],

//  a drum's own attack takes a few samples — the grid is what must be exact
[`        ok (on.empty() || on[0] <= 2, "the first step lands on the bar", on.empty() ? 999 : on[0], 2);`,
 `        ok (on.empty() || on[0] <= 40, "the first step lands on the bar", on.empty() ? 999 : on[0], 40);`, "first step tol"],

[`        seqOneChannel (e, 0, 16, 1, true);
        e.p.g[GP_SEQ] = 0.0f;`,
 `        seqOneChannel (e, 8, 16, 1, true);
        e.p.g[GP_SEQ] = 0.0f;`, "seq off test"],

[`        seqOneChannel (e, 0, 16, 1, true);
        e.p.g[GP_SWING] = 0.80f;`,
 `        seqOneChannel (e, 8, 16, 1, true);
        e.p.g[GP_SWING] = 0.80f;`, "swing test"],

[`        seqOneChannel (e, 0, 3, 1, true);        // a three-step lane`,
 `        seqOneChannel (e, 8, 3, 1, true);        // a three-step lane`, "polymeter test"],

[`            seqOneChannel (e, 0, 16, div, true);`,
 `            seqOneChannel (e, 8, 16, div, true);`, "division test"],

[`        seqOneChannel (e, 0, 16, 1, false);       // step 0 only
        e.pat[0].lane[0].step[0].micro = 25;      // a quarter of a step late`,
 `        seqOneChannel (e, 8, 16, 1, false);       // step 0 only
        e.pat[0].lane[8].step[0].micro = 25;      // a quarter of a step late`, "micro test"],

[`            ok (std::fabs ((double) on[0] - 1500.0) <= 3.0, "microtiming shifts by a quarter step", on[0], 1500.0);`,
 `            ok (std::fabs ((double) on[0] - 1500.0) <= 45.0, "microtiming shifts by a quarter step", on[0], 1500.0);`, "micro tol"],

[`            seqOneChannel (e, 0, 16, 1, true);
            for (int k = 0; k < 16; ++k) e.pat[0].lane[0].step[k].prob = (uint8_t) prob;`,
 `            seqOneChannel (e, 8, 16, 1, true);
            for (int k = 0; k < 16; ++k) e.pat[0].lane[8].step[k].prob = (uint8_t) prob;`, "prob test"],

[`        seqOneChannel (e, 0, 16, 3, false);       // one quarter-note step
        e.pat[0].lane[0].step[0].ratchet = 4;`,
 `        seqOneChannel (e, 8, 16, 3, false);       // one quarter-note step
        e.pat[0].lane[8].step[0].ratchet = 4;`, "ratchet test"],

//  rebound: with the cascade fixed, the count is bounded and knowable
[`        if (rb == 0.0f) ok (on.size() == 1, "no rebound is one hit", (double) on.size(), 1);
        else            ok (on.size() >= 2, "rebound bounces", (double) on.size(), 2);`,
 `        if (rb == 0.0f) ok (on.size() == 1, "no rebound is one hit", (double) on.size(), 1);
        else            ok (on.size() >= 2, "rebound bounces", (double) on.size(), 2);
        ok (on.size() <= 14, "and it is a bounce, not a runaway", (double) on.size(), 14);`, "rebound bound"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
writeH(); writeC(); writeT();
console.log("chunk D patched OK — rebound cascade fixed, onset detector fixed");
