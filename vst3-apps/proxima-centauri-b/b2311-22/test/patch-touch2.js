/*  Second half of the touch patch — the knead anchor in patch-touch.js
    assumed a block that two earlier patches had already reshaped, so the
    whole script aborted (correctly, before writing anything). These anchors
    match the file as it actually is.
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
[`    void noteOn  (int note, float vel);
    void noteOff (int note);`,
`    void noteOn  (int note, float vel);
    void noteOff (int note);

    /*  The probe is an excitation: pressing tissue sounds it, from exactly
        the node touched. Each place on the body has its own stable pitch,
        so playing the creature by hand is real. */
    void touchOn (int node, float vel);
    void touchOff();`, "touch decl"],
[`    void triggerVoice (Voice& v, int note, float vel);`,
 `    void triggerVoice (Voice& v, int note, float vel, int forceNode = -1);
    int touchNote = -1;             // the sounding touch, if any`, "trigger sig"]
]);

const wC = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.cpp", [
[`void Engine::triggerVoice (Voice& v, int note, float vel)
{`,
`void Engine::triggerVoice (Voice& v, int note, float vel, int forceNode)
{`, "trigger sig cpp"],

[`    //  excitation node from the DEPTH organ: hub -> most remote tissue
    const int pathAt = std::min (s.pathLen - 1,
                        (int) std::lround (clamp01 (p.depth) * (float) (s.pathLen - 1)));
    const int exNode = s.path[std::max (0, pathAt)];`,
`    //  excitation node: a TOUCH strikes exactly the tissue under the hand;
    //  a key strikes along the DEPTH path, hub -> most remote tissue
    const int pathAt = std::min (s.pathLen - 1,
                        (int) std::lround (clamp01 (p.depth) * (float) (s.pathLen - 1)));
    const int exNode = (forceNode >= 0 && forceNode < s.nNodes)
                     ? forceNode
                     : s.path[std::max (0, pathAt)];`, "force node"],

[`void Engine::noteOff (int note)
{`,
`void Engine::touchOn (int node, float vel)
{
    const Specimen& s = specimen();
    if (s.nNodes <= 0) return;
    node = std::max (0, std::min (s.nNodes - 1, node));
    //  the place's own pitch: stable per node, spread across two octaves
    const int tnote = 41 + (node * 5) % 25;
    touchNote = tnote;
    Voice* pick = nullptr;
    for (auto& v : voices) if (! v.active) { pick = &v; break; }
    if (! pick) pick = &voices[0];
    triggerVoice (*pick, tnote, vel, node);
}

void Engine::touchOff()
{
    if (touchNote < 0) return;
    noteOff (touchNote);
    touchNote = -1;
}

void Engine::noteOff (int note)
{`, "touch impl"]
]);

const wP = edit("C:/Users/peter/b/ArtefactB2311/Source/PluginProcessor.cpp", [
[`    else if (k == "bwfx")    { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }`,
`    else if (k == "touch")
    {
        if ((int) m.getProperty ("on", 0) != 0)
            engine.touchOn ((int) m.getProperty ("n", 0),
                            (float) (double) m.getProperty ("v", 0.8));
        else
            engine.touchOff();
    }
    else if (k == "bwfx")    { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }`, "touch msg"]
]);

const wU = edit("C:/Users/peter/b/ArtefactB2311/Source/ui/ui.html", [

//  pointerdown: touch the actual node, and sound it
[`  try { cv.setPointerCapture(e.pointerId); } catch (err) {}
  const id = paramOf(o);
  gest = { organ: o, id, t0: performance.now(), x0: e.clientX, y0: e.clientY,
           lx: e.clientX, ly: e.clientY, angle: 0, path: 0, mode: "?",
           v0: VAL[id] !== undefined ? VAL[id] : 0.5, dv: 0, pressT: null };
  pin.set = new Set(o.nodes);`,
`  try { cv.setPointerCapture(e.pointerId); } catch (err) {}
  const id = paramOf(o);
  /* the touched NODE, not just the organ: the probe excites that tissue,
     so every gesture is audible by itself — one mouse cannot hold a key
     and work an organ at the same time */
  let tn = o.nodes[0], td = 1e9;
  for (const i of o.nodes) {
    const d = Math.hypot(e.clientX - P.sx[i], e.clientY - P.sy[i]);
    if (d < td) { td = d; tn = i; }
  }
  NB.send({ k: "touch", n: tn, on: 1, v: 0.85 });
  gest = { organ: o, id, t0: performance.now(), x0: e.clientX, y0: e.clientY,
           lx: e.clientX, ly: e.clientY, angle: 0, path: 0, mode: "?",
           v0: VAL[id] !== undefined ? VAL[id] : 0.5, dv: 0, node: tn };
  pin.set = new Set(o.nodes);`, "down"],

//  the classifier gains KNEAD
[`    if (Math.abs(g.angle) > Math.PI * 0.55) g.mode = "stir";
    else if (g.path > 90 && (now - g.t0) < 260 && net / g.path > 0.93) g.mode = "stroke";
  }`,
`    if (Math.abs(g.angle) > Math.PI * 0.45) g.mode = "stir";
    else if (g.path > 90 && (now - g.t0) < 260 && net / g.path > 0.93) g.mode = "stroke";
    /*  and the gesture everyone actually makes: an ordinary deliberate
        drag. It KNEADS — the quantity follows the working of the tissue,
        signed by the drag's rotation sense about the organ. Without this
        the commonest gesture of all mapped to NOTHING, which read —
        correctly — as "the specimen does not respond". */
    else if (g.path > 26 && (now - g.t0) >= 260) g.mode = "knead";
  }`, "knead classify"],

[`  if (g.mode === "stir") {
    /* circling winds the quantity: one full turn = about a quarter range */
    sendParam(g.id, (VAL[g.id] !== undefined ? VAL[g.id] : 0.5) + da * 0.06);
  }`,
`  if (g.mode === "stir") {
    /* circling winds the quantity: one full turn = about a third of range */
    sendParam(g.id, (VAL[g.id] !== undefined ? VAL[g.id] : 0.5) + da * 0.06);
  }
  if (g.mode === "knead") {
    const dir = (da >= 0 ? 1 : -1);
    sendParam(g.id, (VAL[g.id] !== undefined ? VAL[g.id] : 0.5) + dir * step * 0.0016);
  }`, "knead act"],

//  pointerup releases the touch
[`cv.addEventListener("pointerup", e => {
  if (!gest) return;
  const g = gest;`,
`cv.addEventListener("pointerup", e => {
  if (!gest) return;
  NB.send({ k: "touch", on: 0 });
  const g = gest;`, "up release"],

//  the gripped organ flares
[`    if (P.vis[i]) {
      items.push([P.sx[i], P.sy[i], P.size[i] * 1.35, 0.85, hue, 0]);
      if (LUM[i] > 0.01)
        items.push([P.sx[i], P.sy[i], P.size[i] * 1.7, LUM[i] * 0.8, hue, 2]);`,
`    if (P.vis[i]) {
      items.push([P.sx[i], P.sy[i], P.size[i] * 1.35, 0.85, hue, 0]);
      /* the gripped organ flares: the body visibly answers the hand */
      const grip = gest && pin.set && pin.set.has(i) ? 0.5 : 0;
      const glo = Math.min(1, (LUM[i] || 0) * 0.8 + grip);
      if (glo > 0.01)
        items.push([P.sx[i], P.sy[i], P.size[i] * 1.7, glo, hue, 2]);`, "flare"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wH(); wC(); wP(); wU();
console.log("touch sounds the tissue; knead catches the ordinary drag; the grip flares");
