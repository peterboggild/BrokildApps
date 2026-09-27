/*  Peter: "still don't know how to change the sound by interacting … the
    specimen does not really respond to the mouse when dragged or clicked."

    Three real holes, none of which a synthetic CDP probe could feel:

    1. TOUCH MUST SOUND. With one mouse you cannot hold a key and gesture at
       once, so even a working gesture was inaudible while you made it. Now
       the probe IS an excitation: pressing tissue makes the artefact speak
       from the exact node touched — its own pitch for that place on the
       body, held while you hold, released when you let go. Every gesture is
       audible by itself, and "the artefact displays response" finally
       includes the response that matters: sound.

    2. A SLOW DRAG DID NOTHING. The grammar had stir (circles), stroke
       (fast+straight) and press (still) — and the most natural gesture of
       all, an ordinary deliberate drag, fell between all three. Now it is
       KNEAD: dragging works the organ's quantity directly, signed by the
       drag's direction around the organ (outward/clockwise up,
       inward/counter down). Stir still exists and is engaged earlier.

    3. NOTHING VISIBLY CONFIRMED A GESTURE. The touched organ now flares
       (UI-side glow on its nodes while gripped) and the attention ring
       deepens during a gesture, so the body visibly answers the hand.
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

/* ── engine: the touch voice ──────────────────────────────────────────── */
const wH = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.h", [
[`    void noteOn  (int note, float vel);
    void noteOff (int note);`,
`    void noteOn  (int note, float vel);
    void noteOff (int note);

    /*  The probe is an excitation: pressing tissue sounds it, from exactly
        the node touched. Each place on the body has its own pitch (stable,
        derived from the node), so playing the creature by hand is real. */
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
    //  the place's own pitch: stable per node, spread over two octaves
    const int note = 41 + (node * 5) % 25;
    touchNote = note;
    Voice* pick = nullptr;
    for (auto& v : voices) if (! v.active) { pick = &v; break; }
    if (! pick) pick = &voices[0];
    triggerVoice (*pick, note, vel, node);
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

/* ── processor: the message ───────────────────────────────────────────── */
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

/* ── the panel: knead, touch, visible response ────────────────────────── */
const wU = edit("C:/Users/peter/b/ArtefactB2311/Source/ui/ui.html", [

//  pointerdown: find the actual touched NODE and sound it
[`cv.addEventListener("pointerdown", e => {
  if (fieldOpen) { fieldClick(e.clientX, e.clientY); return; }
  const o = nearestOrgan(e.clientX, e.clientY);
  if (!o) return;
  try { cv.setPointerCapture(e.pointerId); } catch (err) {}
  const id = paramOf(o);
  gest = { organ: o, id, t0: performance.now(), x0: e.clientX, y0: e.clientY,
           lx: e.clientX, ly: e.clientY, angle: 0, path: 0, mode: "?",
           v0: VAL[id] !== undefined ? VAL[id] : 0.5, dv: 0, pressT: null };
  pin.set = new Set(o.nodes);
});`,
`cv.addEventListener("pointerdown", e => {
  if (fieldOpen) { fieldClick(e.clientX, e.clientY); return; }
  const o = nearestOrgan(e.clientX, e.clientY);
  if (!o) return;
  try { cv.setPointerCapture(e.pointerId); } catch (err) {}
  const id = paramOf(o);
  /* the touched NODE, not just the organ: the probe excites that tissue */
  let tn = o.nodes[0], td = 1e9;
  for (const i of o.nodes) {
    const d = Math.hypot(e.clientX - P.sx[i], e.clientY - P.sy[i]);
    if (d < td) { td = d; tn = i; }
  }
  NB.send({ k: "touch", n: tn, on: 1, v: 0.85 });
  gest = { organ: o, id, t0: performance.now(), x0: e.clientX, y0: e.clientY,
           lx: e.clientX, ly: e.clientY, angle: 0, path: 0, mode: "?",
           v0: VAL[id] !== undefined ? VAL[id] : 0.5, dv: 0, node: tn };
  pin.set = new Set(o.nodes);
});`, "down"],

//  the classifier gains KNEAD: a slow deliberate drag always works
[`  if (g.mode === "?") {
    /*  a half-circle arc still measures ~0.78 net/path, so the stroke bar
        sits at 0.93 - only something that truly goes somewhere is a stroke */
    if (Math.abs(g.angle) > Math.PI * 0.55) g.mode = "stir";
    else if (g.path > 90 && (now - g.t0) < 260 && net / g.path > 0.93) g.mode = "stroke";
  }
  if (g.mode === "stir") {
    /* circling winds the quantity: one full turn = about a quarter range */
    sendParam(g.id, (VAL[g.id] !== undefined ? VAL[g.id] : 0.5) + da * 0.06);
  }`,
`  if (g.mode === "?") {
    /*  a half-circle arc still measures ~0.78 net/path, so the stroke bar
        sits at 0.93 - only something that truly goes somewhere is a stroke */
    if (Math.abs(g.angle) > Math.PI * 0.45) g.mode = "stir";
    else if (g.path > 90 && (now - g.t0) < 260 && net / g.path > 0.93) g.mode = "stroke";
    /*  and the gesture everyone actually makes: an ordinary deliberate
        drag. It KNEADS - the organ's quantity follows the working of the
        tissue, signed by the drag's rotation sense about the organ, so
        working one way winds up and the other way winds down. Without
        this, the commonest gesture of all mapped to NOTHING - which read,
        correctly, as "the specimen does not respond". */
    else if (g.path > 26 && (now - g.t0) >= 260) g.mode = "knead";
  }
  if (g.mode === "stir") {
    /* circling winds the quantity: one full turn = about a third of range */
    sendParam(g.id, (VAL[g.id] !== undefined ? VAL[g.id] : 0.5) + da * 0.06);
  }
  if (g.mode === "knead") {
    const dir = (da >= 0 ? 1 : -1);
    sendParam(g.id, (VAL[g.id] !== undefined ? VAL[g.id] : 0.5) + dir * step * 0.0016);
  }`, "knead"],

//  pointerup: release the touch
[`cv.addEventListener("pointerup", e => {
  if (!gest) return;
  const g = gest;`,
`cv.addEventListener("pointerup", e => {
  if (!gest) return;
  NB.send({ k: "touch", on: 0 });
  const g = gest;`, "up release"],

//  the touched organ flares while gripped (UI-side glow, additive)
[`    const hue = (SPEC.tags[i] / 6);
    if (P.vis[i]) {
      items.push([P.sx[i], P.sy[i], P.size[i] * 1.35, 0.85, hue, 0]);
      if (LUM[i] > 0.01)
        items.push([P.sx[i], P.sy[i], P.size[i] * 1.7, LUM[i] * 0.8, hue, 2]);`,
`    const hue = (SPEC.tags[i] / 6);
    if (P.vis[i]) {
      items.push([P.sx[i], P.sy[i], P.size[i] * 1.35, 0.85, hue, 0]);
      /* the gripped organ flares: the body visibly answers the hand */
      const grip = gest && pin.set && pin.set.has(i) ? 0.5 : 0;
      const glo = Math.min(1, (LUM[i] || 0) * 0.8 + grip);
      if (glo > 0.01)
        items.push([P.sx[i], P.sy[i], P.size[i] * 1.7, glo, hue, 2]);`, "flare"],

//  drop the old LUM-only glow line that the flare block replaces
[`      const grip = gest && pin.set && pin.set.has(i) ? 0.5 : 0;
      const glo = Math.min(1, (LUM[i] || 0) * 0.8 + grip);
      if (glo > 0.01)
        items.push([P.sx[i], P.sy[i], P.size[i] * 1.7, glo, hue, 2]);
    } else if (P.ghost[i] > 0.01) {`,
`      const grip = gest && pin.set && pin.set.has(i) ? 0.5 : 0;
      const glo = Math.min(1, (LUM[i] || 0) * 0.8 + grip);
      if (glo > 0.01)
        items.push([P.sx[i], P.sy[i], P.size[i] * 1.7, glo, hue, 2]);
    } else if (P.ghost[i] > 0.01) {`, "noop guard"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wH(); wC(); wP(); wU();
console.log("touch sounds the tissue; knead catches the ordinary drag; the grip flares");
