/*  FMR BUGLIST 1 — the transport should follow the DAW.

    Peter: "When DAW starts or stops the FMR should obey that. It should be
    possible to start/stop the FMR without the DAW. Space bar can stop and
    start the FMR both directly or via the DAW."

    GP_SEQ was one switch meaning "the sequencer plays", and when it was on
    the machine played either way — bar-locked if the host was rolling, free-
    running at the host's tempo if it was stopped. So stopping the DAW did
    not stop the machine. Split into an intent and a gate:

        seq        ARMED. The user's intent. Still automatable, still saved.
        freeRun    a session-local LATCH. Not a parameter, not saved. Set by
                   the panel's transport when no host is rolling.
        running    armed && (host is rolling || freeRun)

    The host transport NEVER writes seq — it is an automatable parameter, and
    a transport that toggled it would push undo steps and fight any lane on
    it. The gate is a separate runtime term.

    "Host has a transport at all" and "host is rolling" are two different
    questions, and the processor conflated them: the playhead was only read
    inside `if (auto bpm = pos->getBpm())`, so a host reporting no BPM never
    called setTransport at all and looked exactly like the standalone.

    SPACE: the panel called preventDefault() unconditionally and sent its own
    message — which is precisely why space did not start Ableton. The plugin
    ate the key. With the gate in place the fix is to stop being clever: when
    a host transport exists, let the key through, the host starts, and FMR
    follows. Only the standalone (or a host with no transport) handles it.
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

const wH = edit("C:/Users/peter/b/FullMetalRacket/Source/Engine.h", [
[`    void setTransport (double bpm, double ppq, bool playing);`,
`    /*  The present flag answers "does this host have a transport at all", which is a
        different question from "is it rolling" — the standalone and a host
        that reports no position must not look alike. */
    void setTransport (double bpm, double ppq, bool playing, bool present);

    //  THE GATE (buglist 1). seq is the intent; this is the session-local
    //  latch that lets the machine run when no host transport is rolling.
    void setFreeRun (bool on) { freeRun = on; }
    bool isFreeRunning() const { return freeRun; }
    bool hostHasTransport() const { return hostPresent; }
    bool isRunning() const { return p.g[GP_SEQ] >= 0.5f && (playing || freeRun); }
    bool isPlayingHost() const { return playing; }`, "decl"],

[`    bool   hostBpm = false;`,
 `    bool   hostBpm = false;
    bool   hostPresent = false;      // the host has a transport at all
    bool   freeRun = false;          // session-local latch, never a parameter`, "members"]
]);

const wE = edit("C:/Users/peter/b/FullMetalRacket/Source/Engine.cpp", [
[`void Engine::setTransport (double bpm, double ppq, bool play)
{
    if (bpm > 20.0 && bpm < 999.0) { bpmNow = bpm; hostBpm = true; }
    if (play && ppq >= 0.0) ppqNow = ppq;
    playing = play && ppq >= 0.0;
}`,
`void Engine::setTransport (double bpm, double ppq, bool play, bool present)
{
    if (bpm > 20.0 && bpm < 999.0) { bpmNow = bpm; hostBpm = true; }
    if (play && ppq >= 0.0) ppqNow = ppq;
    const bool wasPlaying = playing;
    playing = play && ppq >= 0.0;
    hostPresent = present;
    /*  the DAW stopping stops the machine: the free-run latch is dropped, so
        it does not carry on by itself afterwards */
    if (wasPlaying && ! playing) freeRun = false;
}`, "setTransport"],

[`    if (p.g[GP_SEQ] < 0.5f) { uiStep = -1; uiLaneStep.fill (-1); return; }`,
`    /*  ARMED is the intent; RUNNING also needs a clock that is going — the
        host's, or the free-run latch the panel sets when no host is rolling.
        Stopping the DAW therefore stops the machine, which arming alone
        used to override. */
    if (p.g[GP_SEQ] < 0.5f || ! (playing || freeRun))
    { uiStep = -1; uiLaneStep.fill (-1); return; }`, "gate"]
]);

const wP = edit("C:/Users/peter/b/FullMetalRacket/Source/PluginProcessor.cpp", [
[`    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0 && *bpm < 999.0)
                {
                    engine.p.bpm = *bpm;
                    bwfxRack.setBpm (*bpm);
                    const auto ppq = pos->getPpqPosition();
                    bwfxRack.setTransport (*bpm, ppq ? *ppq : 0.0, pos->getIsPlaying());
                    engine.setTransport (*bpm, ppq ? *ppq : -1.0, pos->getIsPlaying() && ppq.hasValue());
                }`,
`    /*  A host with a transport must be recognised as such even when it
        reports no tempo — the old form only called setTransport inside
        the getBpm() test, so such a host was indistinguishable from the
        standalone and the machine free-ran through a stopped DAW. */
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            const auto bpm = pos->getBpm();
            const auto ppq = pos->getPpqPosition();
            const bool good = bpm && *bpm > 20.0 && *bpm < 999.0;
            if (good)
            {
                engine.p.bpm = *bpm;
                bwfxRack.setBpm (*bpm);
                bwfxRack.setTransport (*bpm, ppq ? *ppq : 0.0, pos->getIsPlaying());
            }
            engine.setTransport (good ? *bpm : 0.0,
                                 ppq ? *ppq : -1.0,
                                 pos->getIsPlaying() && ppq.hasValue(),
                                 true);
        }
    }`, "playhead"],

//  the panel's transport drives the LATCH; it never lets the host write seq
[`    if (what == "run")        { engine.restartClock(); setParamById ("seq", 1.0f); notice ("RUNNING"); }`,
`    /*  ▶ and ⏵ arm, and additionally raise the free-run latch when no host
        transport is rolling — that is the "start it without the DAW" case.
        With a DAW rolling they simply arm and fall into step with it. */
    if (what == "run")        { engine.restartClock(); engine.setFreeRun (! engine.isPlayingHost());
                                setParamById ("seq", 1.0f); notice ("RUNNING"); }`, "run"],

[`    else if (what == "cont")  { setParamById ("seq", 1.0f);`,
 `    else if (what == "cont")  { engine.setFreeRun (! engine.isPlayingHost()); setParamById ("seq", 1.0f);`, "cont"],

[`    else if (what == "pause") { setParamById ("seq", 0.0f);`,
 `    else if (what == "pause") { engine.setFreeRun (false); setParamById ("seq", 0.0f);`, "pause"],

[`    else if (what == "stop")  { setParamById ("seq", 0.0f); engine.restartClock();`,
 `    else if (what == "stop")  { engine.setFreeRun (false); setParamById ("seq", 0.0f); engine.restartClock();`, "stop"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wH(); wE(); wP();
console.log("1: seq is intent, the latch is the gate, the DAW is in charge");
