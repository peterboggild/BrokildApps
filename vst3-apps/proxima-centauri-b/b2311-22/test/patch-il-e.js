/*  Measured live: the scars reached 0.66 — thirteen times the threshold —
    and not one accent was ever applied, because the bodies never reached
    "rest". Two causes, both mine:

    1. IT NEVER STOPPED TALKING. The interjection rule fired on "it has been
       five seconds since I spoke" alone, and what it had heard decays over
       six seconds — so once you stopped playing it went on interjecting into
       an empty room for the best part of a minute. An interjection is
       something you do while the other party is still speaking: it now
       requires that you actually are. After you stop, it answers once into
       the silence and then listens.

    2. ONE GATE FOR TWO BODIES. Remodelling was held until BOTH the voices
       and the interlocutor were silent — but the two surgeries are
       independent. Remodelling the played body is unsafe only while a note
       is sounding; remodelling the listener is unsafe only while it is
       speaking. Two gates, so each body heals at its own first opportunity.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
  for (const [a, b, tag] of subs) {
    const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(tag + " x" + n); continue; }
    s = s.split(A).join(B);
  }
  return () => fs.writeFileSync(path, s);
}

const wH = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.h", [
[`    std::atomic<bool> remodelReq { false };`,
`    //  two surgeries, two gates: a body is remodelled at ITS own first
    //  moment of quiet, not at some moment of shared silence that a talkative
    //  interlocutor may never allow
    std::atomic<bool> remodelReq { false };      // either body is ready
    std::atomic<bool> remodelA { false }, remodelB { false };`, "gates"],
[`    bool debugAtRest() const
    {
        if (il.speaking) return false;
        for (const auto& v : voices) if (v.active) return false;
        return true;
    }`,
`    bool debugAtRest() const
    {
        for (const auto& v : voices) if (v.active) return false;
        return true;
    }
    bool debugSurgeryPending() const
    {
        return remodelA.load (std::memory_order_relaxed)
            || remodelB.load (std::memory_order_relaxed);
    }`, "rest"],

[`        if (self)  { accentA.fill (1.0f); scarA.fill (0.0f); }
        if (other) { accentB.fill (1.0f); scarB.fill (0.0f); }
        accentCount = 0;`,
`        if (self)  { accentA.fill (1.0f); scarA.fill (0.0f); remodelA.store (false); }
        if (other) { accentB.fill (1.0f); scarB.fill (0.0f); remodelB.store (false); }
        accentCount = 0;`, "forget"]
]);

const wC = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.cpp", [

//  an interjection happens while the other party is speaking
[`    const bool heardEnough = il.sentPow > 2.0e-7f && heardTot > 0.0f;
    const bool wantReply = (! il.speaking) && heardEnough
                        && (il.quietFor > 0.18f || il.sinceReply > 5.0f);`,
`    const bool heardEnough = il.sentPow > 2.0e-7f && heardTot > 0.0f;
    /*  An interjection is something you do while the other party is STILL
        TALKING. Keyed on elapsed time alone it kept interrupting an empty
        room for the best part of a minute after playing stopped — and a body
        that never falls silent never heals, so no accent was ever applied. */
    const bool interject = il.sinceReply > 5.0f && il.spkEnv > 0.004f;
    const bool wantReply = (! il.speaking) && heardEnough
                        && (il.quietFor > 0.18f || interject);`, "interject"],

//  two gates
[`        //  applied only at REST — between utterances, like healing. A body
        //  never remodels under a held note, so it can never glitch one.
        if (! remodelReq.load (std::memory_order_relaxed))
        {
            bool rest = ! il.speaking;
            for (const auto& v : voices) if (v.active) { rest = false; break; }
            if (rest)
            {
                float mx = 0;
                for (int e = 0; e < s.nEdges; ++e) mx = std::max (mx, scarA[(size_t) e]);
                if (conv)
                    for (int e = 0; e < otherSpecimen().nEdges; ++e)
                        mx = std::max (mx, scarB[(size_t) e]);
                if (mx > 0.05f) remodelReq.store (true, std::memory_order_release);
            }
        }`,
`        /*  Applied only at rest — between utterances, like healing, so a
            body never remodels under a sounding note. But each body has its
            OWN rest: the played one is busy only while a voice sounds, the
            listener only while it is speaking. */
        if (! remodelReq.load (std::memory_order_relaxed))
        {
            bool voiceQuiet = true;
            for (const auto& v : voices) if (v.active) { voiceQuiet = false; break; }

            if (voiceQuiet && ! remodelA.load (std::memory_order_relaxed))
            {
                float mx = 0;
                for (int e = 0; e < s.nEdges; ++e) mx = std::max (mx, scarA[(size_t) e]);
                if (mx > 0.05f) remodelA.store (true, std::memory_order_relaxed);
            }
            if (conv && ! il.speaking && ! remodelB.load (std::memory_order_relaxed))
            {
                float mx = 0;
                for (int e = 0; e < otherSpecimen().nEdges; ++e)
                    mx = std::max (mx, scarB[(size_t) e]);
                if (mx > 0.05f) remodelB.store (true, std::memory_order_relaxed);
            }
            if (remodelA.load (std::memory_order_relaxed)
             || remodelB.load (std::memory_order_relaxed))
                remodelReq.store (true, std::memory_order_release);
        }`, "two gates"],

[`void Engine::serviceRemodel()
{
    if (! remodelReq.exchange (false, std::memory_order_acq_rel)) return;
    remodelBody (specLoaded, scarA, accentA, spec,  specIdx,  true);
    remodelBody (oLoaded,    scarB, accentB, oSpec, oSpecIdx, false);
    ++accentCount;
}`,
`void Engine::serviceRemodel()
{
    if (! remodelReq.exchange (false, std::memory_order_acq_rel)) return;
    if (remodelA.exchange (false, std::memory_order_acq_rel))
        remodelBody (specLoaded, scarA, accentA, spec,  specIdx,  true);
    if (remodelB.exchange (false, std::memory_order_acq_rel))
        remodelBody (oLoaded,    scarB, accentB, oSpec, oSpecIdx, false);
    ++accentCount;
}`, "service"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wH(); wC();
console.log("E: it stops talking when you do; each body heals at its own quiet moment");
