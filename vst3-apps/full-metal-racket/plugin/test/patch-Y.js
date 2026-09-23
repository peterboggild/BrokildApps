// THREE THINGS.
//
// 1. A LOADED PATCH DID NOT SAY SO. The kit display kept showing whichever
//    seed was last dialled, so after loading a .fmrkit the panel named a kit
//    that was no longer what you were hearing. It shows the file's name and
//    marks it USER PATCH now, and dialling a seed takes it back.
//
// 2. WHERE A LOAD LANDS IN THE BAR. With a host rolling this was already
//    deterministic — every step is derived from the bar position, so step one
//    is on the bar whatever you do. Free-running it was not: the internal
//    clock kept whatever phase it had, so a load could land mid-pattern.
//    RUN now restarts from step one, STOP rewinds, and loading a patch or a
//    kit resets the free clock. Peter's own suggestion, and the right one.
//
// 3. A REAL TRANSPORT. Run, pause, stop and record as symbols. RECORD is the
//    one with teeth: it writes what you play into the current pattern, at the
//    nearest step of that lane. Adding a record button that did nothing would
//    be the same mistake as REBOUND having no knob.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(p.split("/").pop() + ": " + t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

const wh = edit(R + "Source/Engine.h", [
[`    int  seqStepFor (int c) const { return uiLaneStep[(size_t) (c < 0 ? 0 : (c >= NCH ? NCH - 1 : c))]; }`,
`    int  seqStepFor (int c) const { return uiLaneStep[(size_t) (c < 0 ? 0 : (c >= NCH ? NCH - 1 : c))]; }

    /*  Put the free-running clock back to the top. With a host rolling the
        bar governs and this changes nothing; without one it is the difference
        between a pattern that starts at step one and a pattern that starts
        wherever the last one happened to leave off. */
    void restartClock() { ppqFree = 0.0; uiLaneStep.fill (-1); nHits = 0; for (auto& l : lastVel) l = 0.0f; }

    //  RECORD: what you play is written into the current pattern
    std::atomic<bool> recArm { false };`, "restart + rec"],
[`    std::array<int, NCH> uiLaneStep { };`,
 `    std::array<int, NCH> uiLaneStep { };
    std::array<double, NCH> uiLaneFrac { };     // for quantising a recorded hit`, "frac"]
]);

const wc = edit(R + "Source/Engine.cpp", [
[`        {
            const long long kNow = (long long) std::floor (sa);
            uiLaneStep[(size_t) c] = kNow < 0 ? -1 : laneIndex (L, kNow);
        }`,
`        {
            const long long kNow = (long long) std::floor (sa);
            uiLaneStep[(size_t) c] = kNow < 0 ? -1 : laneIndex (L, kNow);
            uiLaneFrac[(size_t) c] = sa - (double) kNow;
        }`, "frac capture"],

[`void Engine::trigger (int chan, float velocity, float keySemis, bool allowRebound)
{
    if (chan < 0 || chan >= NCH) return;
    const float vel = clampf (velocity, 0.01f, 1.0f);`,
`void Engine::trigger (int chan, float velocity, float keySemis, bool allowRebound)
{
    if (chan < 0 || chan >= NCH) return;
    const float vel = clampf (velocity, 0.01f, 1.0f);

    /*  RECORD. A played hit is written into the current pattern at the
        NEAREST step of that lane, not the one it has just passed — quantising
        backwards makes everything you play sound late.

        The write is from the audio thread while the panel may be reading the
        pattern to draw it. A Step is six plain bytes, so the worst case is
        one frame of the panel showing a half-written velocity, which is not
        worth a lock in the trigger path. */
    if (recArm.load (std::memory_order_relaxed) && p.g[GP_SEQ] >= 0.5f && allowRebound)
    {
        const int cur = uiLaneStep[(size_t) chan];
        if (cur >= 0)
        {
            Lane& L = pat[curPat < 0 ? 0 : (curPat >= NPAT ? NPAT - 1 : curPat)].lane[(size_t) chan];
            const int len = L.len < 1 ? 1 : L.len;
            int k = cur + (uiLaneFrac[(size_t) chan] > 0.5 ? 1 : 0);
            if (k >= len) k -= len;
            if (k >= 0 && k < NSTEP)
            {
                L.step[k].on = 1;
                L.step[k].vel = (uint8_t) clampf (vel * 127.0f, 1.0f, 127.0f);
            }
        }
    }`, "record"]
]);

const wp = edit(R + "Source/PluginProcessor.cpp", [
// the loaded patch names itself
[`    o->setProperty ("i", kitIndex);
    o->setProperty ("name", juce::String (fmr::kitName (kitIndex)));
    o->setProperty ("cat", juce::String (fmr::seedCategoryName (fmr::seedCategory (kitIndex))));`,
`    o->setProperty ("i", kitIndex);
    o->setProperty ("name", patchName.isNotEmpty() ? patchName : juce::String (fmr::kitName (kitIndex)));
    o->setProperty ("cat", patchName.isNotEmpty() ? juce::String ("USER PATCH")
                                                  : juce::String (fmr::seedCategoryName (fmr::seedCategory (kitIndex))));
    o->setProperty ("user", patchName.isNotEmpty());
    o->setProperty ("rec", engine.recArm.load());`, "kit name"],

[`    kitIndex = juce::jlimit (0, fmr::numSeeds() - 1, i);
    fmr::Params q;`,
 `    kitIndex = juce::jlimit (0, fmr::numSeeds() - 1, i);
    patchName = {};                      // a dialled seed is no longer that file
    engine.restartClock();
    fmr::Params q;`, "kit clears patch name"],

[`    bwfxRack.fromJson (o->getProperty ("bwfx").toString().toStdString());
    emitBwfx(); emitSeq(); emitKit();
    notice ("LOADED " + name.toUpperCase());`,
`    bwfxRack.fromJson (o->getProperty ("bwfx").toString().toStdString());
    patchName = name;
    /*  Put the free clock back to the top so a load always begins at step
        one. With a host rolling the bar governs and this changes nothing. */
    engine.restartClock();
    emitBwfx(); emitSeq(); emitKit();
    notice ("LOADED " + name.toUpperCase());`, "patch name set"],

// transport messages
[`    else if (k == "seqget") { emitSeq(); }`,
`    else if (k == "seqget") { emitSeq(); }
    else if (k == "transport")
    {
        const juce::String what = o->getProperty ("what").toString();
        if (what == "run")        { engine.restartClock(); setParamById ("seq", 1.0f); notice ("RUNNING"); }
        else if (what == "cont")  { setParamById ("seq", 1.0f); notice ("CONTINUE"); }
        else if (what == "pause") { setParamById ("seq", 0.0f); notice ("PAUSED"); }
        else if (what == "stop")  { setParamById ("seq", 0.0f); engine.restartClock(); notice ("STOPPED"); }
        emitKit();
    }
    else if (k == "rec")
    {
        const bool on = (int) o->getProperty ("on") != 0;
        engine.recArm.store (on);
        notice (on ? "RECORD ARMED " + DOT + " PLAY TO WRITE STEPS" : "RECORD OFF");
        emitKit();
    }`, "transport msgs"]
]);

const wph = edit(R + "Source/PluginProcessor.h", [
[`    juce::File presetFolder;
    int kitIndex = 0;`,
 `    juce::File presetFolder;
    int kitIndex = 0;
    juce::String patchName;              // empty unless a .fmrkit is loaded`, "patch name member"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wh(); wc(); wp(); wph();
console.log("patch name, clock restart, record arm and transport messages");
