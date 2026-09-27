/*  THE INTERLOCUTOR, phase C — the panel.

    The second body is drawn as what it is: another creature, further off in
    the same field, moving to its own clock. It is mostly GHOST — present but
    not in your slice — and it CONDENSES when it speaks, so you see the
    answer arrive before you have finished hearing it. When the membrane is
    live and both are sounding, filaments stretch across the gap: the third
    voice, drawn.

    And you choose who to talk to from the survey field, which is already
    laid out by spectral similarity — so a near neighbour is a fluent
    conversation and a far one is a stranger. Shift-click calls a body.
    Travel by conversation.
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

const wU = edit("C:/Users/peter/b/ArtefactB2311/Source/ui/ui.html", [

//  state for the second body
[`let SPEC = null;                            // the specimen geometry
let secW = 0, secV = 0, LUM = [];           // the section stream`,
`let SPEC = null;                            // the specimen geometry
let secW = 0, secV = 0, LUM = [];           // the section stream
let SPEC2 = null, LUM2 = [], AUD2 = 0;      // THE INTERLOCUTOR: the other body
let KIN = 0, SAY = 0, ACCENT = 0;           // the state of the conversation`, "state2"],

[`const P = { sx: [], sy: [], size: [], vis: [], ghost: [], comp: [], org: [] };`,
`const P = { sx: [], sy: [], size: [], vis: [], ghost: [], comp: [], org: [] };
const P2 = { sx: [], sy: [], size: [], vis: [], ghost: [] };`, "p2"],

//  project the second body: further off, on its own clock
[`/* ── the frame ──────────────────────────────────────────────────────────── */`,
`/*  The other body, further off in the same field and keeping its own time —
    two creatures, not one creature drawn twice. Mostly ghost, because it is
    not in your slice; it condenses as it speaks. */
function project2() {
  if (!SPEC2 || !(VAL.converse > 0)) return;
  const t = performance.now() * 0.001;
  const th = 0.19 * Math.sin(t * 0.041 + 1.7) + 0.08 * Math.sin(t * 0.0093);
  const eps = 0.14 + 0.9 * (VAL.slab !== undefined ? VAL.slab : 0.6);
  const S = Math.min(innerWidth, innerHeight) * 0.335;
  const cx = innerWidth * 0.5 + S * 1.02, cy = innerHeight * 0.47 - S * 0.36;
  const s2 = S * 0.55;                      // further away, so smaller
  const ct = Math.cos(th), st = Math.sin(th);
  const n = SPEC2.nodes.length;
  for (let i = 0; i < n; i++) {
    const nd = SPEC2.nodes[i];
    const x = nd.x * ct + nd.z * st * 0.6;
    const z = nd.z * ct - nd.x * st * 0.6;
    P2.sx[i] = cx + s2 * x;
    P2.sy[i] = cy + s2 * (nd.y - z * 0.16);
    P2.size[i] = s2 * 0.135 * (1.0 + z * 0.30);
    const dw = Math.abs(nd.w - secW);
    P2.vis[i] = dw < eps;
    P2.ghost[i] = !P2.vis[i] && dw < eps + 0.5 ? 1 - (dw - eps) / 0.5 : 0;
  }
}

/* ── the frame ──────────────────────────────────────────────────────────── */`, "project2"],

[`function drawGL() {
  if (!glOK || !SPEC) return;
  project();`,
`function drawGL() {
  if (!glOK || !SPEC) return;
  project();
  project2();`, "call p2"],

//  draw it, and the filaments of the membrane
[`  const need = items.length * 6 * 8;`,
`  /*  ---- THE INTERLOCUTOR ----
      Another creature. Present as ghost tissue, condensing where it speaks;
      its own hue, so you never mistake one body for the other. */
  if (SPEC2 && VAL.converse > 0) {
    const n2 = SPEC2.nodes.length;
    const cond = 0.25 + 0.75 * Math.min(1, AUD2 * 2.2);
    for (let i = 0; i < n2; i++) {
      const li = LUM2[i] || 0;
      const hue = 0.52 + 0.10 * ((i * 7) % 5) / 5;
      if (P2.vis[i]) {
        items.push([P2.sx[i], P2.sy[i], P2.size[i] * 1.3 * (1 + li * 0.7),
                    0.30 + 0.55 * cond, hue + li * 0.05, li > 0.02 ? 0 : 1]);
        if (li > 0.01)
          items.push([P2.sx[i], P2.sy[i], P2.size[i] * (1.7 + li * 0.6), li * 0.85, hue, 2]);
      } else if (P2.ghost[i] > 0.01) {
        items.push([P2.sx[i], P2.sy[i], P2.size[i] * 1.15, P2.ghost[i] * 0.38, hue, 1]);
      }
    }
    for (const [a, b] of SPEC2.edges) {
      if (!P2.vis[a] || !P2.vis[b]) continue;
      const vein = Math.min(LUM2[a] || 0, LUM2[b] || 0) * 2.0;
      for (let s = 1; s <= 2; s++) {
        const f = s / 3;
        items.push([P2.sx[a] + (P2.sx[b] - P2.sx[a]) * f,
                    P2.sy[a] + (P2.sy[b] - P2.sy[a]) * f,
                    (P2.size[a] + (P2.size[b] - P2.size[a]) * f) * 0.5 * (1 + vein * 0.6),
                    (0.20 + 0.4 * cond) + vein * 0.45, 0.55, vein > 0.25 ? 2 : 1]);
      }
    }

    /*  THE MEMBRANE, drawn: while both bodies sound at once their voices
        multiply, and the frequencies that makes belong to neither of them.
        The filaments exist only while that is true. */
    const mem = (VAL.commune || 0) * Math.min(AUD, AUD2) * 4.0;
    if (mem > 0.01) {
      const va = [], vb = [];
      for (let i = 0; i < SPEC.nodes.length; i++) if (P.vis[i]) va.push(i);
      for (let i = 0; i < n2; i++) if (P2.vis[i]) vb.push(i);
      const links = Math.min(6, va.length, vb.length);
      const ph = performance.now() * 0.001;
      for (let k = 0; k < links; k++) {
        const a = va[(k * 7 + 1) % va.length], b = vb[(k * 5 + 2) % vb.length];
        for (let s = 1; s <= 7; s++) {
          const f = s / 8;
          const bow = Math.sin(f * Math.PI) * 26 * Math.sin(ph * 1.7 + k);
          items.push([P.sx[a] + (P2.sx[b] - P.sx[a]) * f,
                      P.sy[a] + (P2.sy[b] - P.sy[a]) * f - bow,
                      3.5 + 2.5 * Math.sin(f * Math.PI),
                      Math.min(0.9, mem) * (0.35 + 0.5 * Math.sin(f * Math.PI)),
                      0.80, 2]);
        }
      }
    }
  }

  const need = items.length * 6 * 8;`, "draw2"],

//  the streams
[`NB.on("sec", s => {`,
`NB.on("other", o => {
  SPEC2 = o;
  LUM2 = new Array(o.nodes.length).fill(0);
  notice("SECOND BODY " + String(o.cat).padStart(3, "0")
       + " · LADDER OVERLAP " + Math.round((o.kin || 0) * 100) + "%");
});
NB.on("sec", s => {`, "other ev"],

[`  if (s.lum && SPEC) for (let i = 0; i < s.lum.length; i++) LUM[i] = s.lum[i];`,
`  if (s.lum && SPEC) for (let i = 0; i < s.lum.length; i++) LUM[i] = s.lum[i];
  if (s.lum2 && SPEC2) for (let i = 0; i < s.lum2.length; i++) LUM2[i] = s.lum2[i];
  if (s.kin !== undefined) KIN = s.kin;
  if (s.say !== undefined) SAY = s.say;
  if (s.accent !== undefined) ACCENT = s.accent;`, "sec2"],

[`  AUD = AUD * 0.88 + Math.min(1, __ls / (LUM.length * 0.22 + 1)) * 0.12;`,
`  AUD = AUD * 0.88 + Math.min(1, __ls / (LUM.length * 0.22 + 1)) * 0.12;
  let __l2 = 0;
  for (let i = 0; i < LUM2.length; i++) __l2 += LUM2[i];
  AUD2 = AUD2 * 0.88 + Math.min(1, __l2 / (LUM2.length * 0.22 + 1)) * 0.12;`, "aud2"],

//  the rail: the conversation's controls, and what it is doing
[`const RAILTAGS = ["aperture","metabolism","revival","membrane","gravity","depth",
                  "warmth","wake","transit","slab","winch","volume"];`,
`const RAILTAGS = ["aperture","metabolism","revival","membrane","gravity","depth",
                  "warmth","wake","transit","slab","winch","volume",
                  "converse","commune","plastic"];`, "railtags"],

[`  if (SPEC) document.getElementById("catN").textContent =
      "SPECIMEN " + String(SPEC.cat).padStart(3, "0");`,
`  if (SPEC) {
    let txt = "SPECIMEN " + String(SPEC.cat).padStart(3, "0");
    if (SPEC2 && VAL.converse > 0) {
      txt += (SAY ? "  \\u25b8\\u25b8  " : "  \\u00b7\\u00b7  ")
           + String(SPEC2.cat).padStart(3, "0")
           + "  KIN " + Math.round(KIN * 100) + "%";
    }
    if (ACCENT > 0) txt += "  ACCENT " + ACCENT;
    document.getElementById("catN").textContent = txt;
  }`, "readout"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wU();
console.log("C: the second body is on screen, and the membrane is visible");
