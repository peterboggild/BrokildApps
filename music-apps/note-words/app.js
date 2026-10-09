"use strict";

/* ---------- letters → pitch classes ---------- */

// German: the seven naturals, B is B flat, H is B natural, S (Es) is E flat.
const GERMAN = { A: 9, B: 10, C: 0, D: 2, E: 4, F: 5, G: 7, H: 11, S: 3 };
// French (Ravel, Debussy, Dukas for Haydn's centenary): the alphabet laid along
// A–G over and over, all naturals — except H, which keeps its German B.
const NATURALS = [9, 11, 0, 2, 4, 5, 7]; // A B C D E F G
function frenchPc(ch) {
  if (ch === "H") return 11;
  const i = ch.charCodeAt(0) - 65;
  return i >= 0 && i < 26 ? NATURALS[i % 7] : null;
}

// How each pitch class is written: diatonic step (C=0 … B=6) and accidental.
const SPELL = {
  0: [0, ""], 2: [1, ""], 3: [2, "b"], 4: [2, ""], 5: [3, ""],
  7: [4, ""], 8: [5, "b"], 9: [5, ""], 10: [6, "b"], 11: [6, ""],
};
const STEP_NAMES = "CDEFGAB";
function noteName(pc) {
  const [s, a] = SPELL[pc];
  return STEP_NAMES[s] + (a === "b" ? "♭" : "");
}
function germanName(pc) {
  return { 10: "B", 11: "H", 3: "Es", 8: "As" }[pc] || noteName(pc);
}
// The name shown for a note: German names (S = Es, B = B, H = H) in the German
// system, English names in the French one.
function shownName(pc) {
  return state.sys === "de" ? germanName(pc) : noteName(pc);
}

/* ---------- state ---------- */

const state = {
  sys: "de", miss: "skip", contour: "near", ases: false,
  tempo: 120, voice: "piano", loop: false, lang: "en",
};
try {
  const saved = JSON.parse(localStorage.getItem("note-words") || "{}");
  for (const k of Object.keys(state)) if (k in saved) state[k] = saved[k];
} catch (e) {}
function persist() {
  try { localStorage.setItem("note-words", JSON.stringify(state)); } catch (e) {}
}

/* ---------- parse ---------- */

// Turns text into events: {kind:"note", text, pc, midi} | {kind:"rest", text}
// | {kind:"gap"} for word breaks. Non-letters other than spaces are dropped.
function parse(raw) {
  const s = raw.toUpperCase().replace(/[^\p{L}\s]/gu, "").replace(/\s+/g, " ").trim();
  const out = [];
  for (let i = 0; i < s.length; i++) {
    const ch = s[i];
    if (ch === " ") { out.push({ kind: "gap" }); continue; }
    if (state.sys === "de" && state.ases && (ch === "A" || ch === "E") && s[i + 1] === "S") {
      out.push({ kind: "note", text: ch + "S", pc: ch === "A" ? 8 : 3 });
      i++;
      continue;
    }
    const pc = state.sys === "de" ? (ch in GERMAN ? GERMAN[ch] : null) : frenchPc(ch);
    if (pc === null) out.push({ kind: "miss", text: ch });
    else out.push({ kind: "note", text: ch, pc });
  }
  placeOctaves(out);
  return out;
}

// Gives every note an octave. "near" takes the octave closest to the note
// before (the way the motifs are usually written); "flat" keeps C4–B4.
function placeOctaves(evs) {
  let prev = null;
  for (const e of evs) {
    if (e.kind !== "note") continue;
    let m = 60 + e.pc;
    if (state.contour === "near" && prev !== null) {
      m = prev + ((((e.pc - prev) % 12) + 18) % 12) - 6; // within a tritone
      while (m < 60) m += 12;     // keep inside C4 … A5,
      while (m > 81) m -= 12;     // the stave without extra ledger lines
    }
    e.midi = m;
    prev = m;
  }
}

/* ---------- the letter strip ---------- */

const $ = (id) => document.getElementById(id);
const wordEl = $("word");
let events = [];

function renderStrip() {
  const strip = $("strip");
  strip.textContent = "";
  let notes = 0, letters = 0;
  events.forEach((e, i) => {
    const c = document.createElement("div");
    if (e.kind === "gap") { c.className = "chip gap"; strip.appendChild(c); return; }
    letters += e.text.length;
    c.className = "chip" + (e.kind === "note" ? "" : " mute");
    c.dataset.i = i;
    const b = document.createElement("b");
    b.textContent = e.kind === "note" && e.text.length > 1 ? e.text[0] + e.text[1].toLowerCase() : e.text;
    const sp = document.createElement("span");
    sp.textContent = e.kind === "note" ? shownName(e.pc) : "–";
    c.append(b, sp);
    strip.appendChild(c);
    if (e.kind === "note") notes += e.text.length;
  });
  const sum = $("summary");
  if (!letters) { sum.textContent = ""; return; }
  const names = events.filter((e) => e.kind === "note").map((e) => shownName(e.pc)).join(" ");
  if (notes === letters) {
    sum.innerHTML = "";
    const strong = document.createElement("strong");
    strong.textContent = "Every letter is a note";
    sum.append(strong, document.createTextNode(names ? " — " + names : ""));
  } else {
    sum.textContent = `${notes} of ${letters} letters are notes` + (names ? " — " + names : "");
  }
}

/* ---------- the stave ---------- */

const SVGNS = "http://www.w3.org/2000/svg";
function el(name, attrs, parent) {
  const n = document.createElementNS(SVGNS, name);
  for (const k in attrs) n.setAttribute(k, attrs[k]);
  if (parent) parent.appendChild(n);
  return n;
}

const GAP = 9;             // space between stave lines
const HALF = GAP / 2;
const STEP_W = 38;         // horizontal room per note
const WORD_W = 16;         // extra room at a word break
const CLEF_W = 46;
const SYS_H = 132;         // one system, with its letters underneath
const TOP = 34;            // from the top of a system to the top stave line

function stepOf(midi) {  // diatonic position, E4 (bottom line) = 0
  const [s] = SPELL[((midi % 12) + 12) % 12];
  const oct = Math.floor(midi / 12) - 1;
  return oct * 7 + s - (4 * 7 + 2);
}

function drawFlat(g, x, y) {
  el("path", { class: "acc", d: `M${x},${y - 15} L${x},${y + 4} C${x + 8},${y - 1} ${x + 7},${y - 7} ${x},${y - 2}` }, g);
}
function drawNatural(g, x, y) {
  el("path", {
    class: "acc",
    d: `M${x},${y - 11} L${x},${y + 4} M${x + 5},${y - 4} L${x + 5},${y + 11} M${x},${y - 1} L${x + 5},${y - 3.5} M${x},${y + 4} L${x + 5},${y + 1.5}`,
  }, g);
}

function renderStaff() {
  const box = $("staff");
  box.textContent = "";
  const playable = events.filter((e) => e.kind === "note" || (e.kind === "miss" && state.miss === "rest"));
  if (!playable.length) {
    const d = document.createElement("div");
    d.className = "empty";
    d.textContent = events.length ? "No letters here are notes in this system." : "Type a word above.";
    box.appendChild(d);
    return;
  }

  // Lay the events into systems, breaking between words where we can.
  // Draw in stave units and let the viewBox scale it up on wide screens.
  const px = box.clientWidth || 600;
  const width = Math.max(300, px >= 600 ? px / 1.35 : px);
  const avail = width - CLEF_W - 14;
  const words = [];
  let cur = [];
  events.forEach((e, i) => {
    if (e.kind === "gap") { if (cur.length) words.push(cur); cur = []; return; }
    if (e.kind === "miss" && state.miss === "skip") return;
    cur.push(i);
  });
  if (cur.length) words.push(cur);

  const lines = [[]];
  let used = 0;
  for (const w of words) {
    const need = w.length * STEP_W + (lines[lines.length - 1].length ? WORD_W : 0);
    if (used && used + need > avail) { lines.push([]); used = 0; }
    for (const i of w) {
      if (used + STEP_W > avail) { lines.push([]); used = 0; }
      const line = lines[lines.length - 1];
      const breakBefore = line.length && line[line.length - 1].word !== w;
      if (breakBefore) used += WORD_W;
      line.push({ i, word: w, x: CLEF_W + 8 + used + STEP_W / 2 });
      used += STEP_W;
    }
  }

  const svg = el("svg", { viewBox: `0 0 ${width} ${lines.length * SYS_H}`, role: "img",
    "aria-label": "Notes on a treble stave" });
  lines.forEach((line, li) => {
    const top = li * SYS_H + TOP;
    const bottom = top + 4 * GAP;
    const yOf = (p) => bottom - p * HALF;
    const right = line.length ? line[line.length - 1].x + STEP_W / 2 + 4 : width - 10;
    for (let k = 0; k < 5; k++) el("line", { class: "ln", x1: 8, x2: right, y1: top + k * GAP, y2: top + k * GAP }, svg);
    el("line", { class: "ln", x1: 8, x2: 8, y1: top, y2: bottom }, svg);
    el("line", { class: "ln", x1: right, x2: right, y1: top, y2: bottom, "stroke-width": li === lines.length - 1 ? 2.4 : 1 }, svg);
    const clef = el("text", { class: "clef", x: 12, y: bottom + 8, "font-size": GAP * 6.2 }, svg);
    clef.textContent = "𝄞";

    // Accidentals are remembered across a line, the way they would be in a bar.
    const shown = {};
    line.forEach((slot, k) => {
      const e = events[slot.i];
      const prevSlot = line[k - 1];
      if (prevSlot && prevSlot.word !== slot.word) {
        const bx = slot.x - STEP_W / 2 - WORD_W / 2;
        el("line", { class: "ln", x1: bx, x2: bx, y1: top, y2: bottom }, svg);
        for (const key in shown) delete shown[key];
      }
      const g = el("g", { class: "n", "data-i": slot.i }, svg);
      const x = slot.x;
      if (e.kind === "miss") {
        const y = yOf(6);
        el("path", { class: "ink", "stroke-width": 1, d:
          `M${x - 2},${y} L${x + 3},${y + 7} L${x - 2},${y + 13} L${x + 3},${y + 20} C${x - 5},${y + 16} ${x - 4},${y + 25} ${x + 1},${y + 27} C${x - 7},${y + 23} ${x - 4},${y + 15} ${x + 1},${y + 19} L${x - 4},${y + 12} L${x + 1},${y + 6} Z` }, g);
        const t = el("text", { class: "lbl", x, y: top + SYS_H - TOP - 22 }, g);
        t.textContent = e.text;
        return;
      }
      const p = stepOf(e.midi);
      const y = yOf(p);
      // ledger lines
      for (let lp = -2; lp >= p; lp -= 2) el("line", { class: "ln", x1: x - 10, x2: x + 10, y1: yOf(lp), y2: yOf(lp) }, g);
      for (let lp = 10; lp <= p; lp += 2) el("line", { class: "ln", x1: x - 10, x2: x + 10, y1: yOf(lp), y2: yOf(lp) }, g);
      // accidental
      const [, acc] = SPELL[e.pc];
      const key = p;
      if (acc === "b" && shown[key] !== "b") { drawFlat(g, x - 15, y); shown[key] = "b"; }
      else if (acc === "" && shown[key] === "b") { drawNatural(g, x - 16, y); shown[key] = ""; }
      // head and stem
      el("ellipse", { class: "ink", cx: x, cy: y, rx: 5.4, ry: 3.9, transform: `rotate(-20 ${x} ${y})` }, g);
      if (p < 4) el("line", { class: "stem", x1: x + 4.9, x2: x + 4.9, y1: y - 1, y2: y - 3.5 * GAP }, g);
      else el("line", { class: "stem", x1: x - 4.9, x2: x - 4.9, y1: y + 1, y2: y + 3.5 * GAP }, g);
      const t = el("text", { class: "lbl", x, y: top + SYS_H - TOP - 22 }, g);
      t.textContent = e.text.length > 1 ? e.text[0] + e.text[1].toLowerCase() : e.text;
      const t2 = el("text", { class: "lbl", x, y: top + SYS_H - TOP - 8, "font-size": 10, "font-weight": 400 }, g);
      t2.textContent = shownName(e.pc);
      const title = el("title", {}, g);
      title.textContent = `${e.text} → ${shownName(e.pc)} (${noteName(e.pc)}${Math.floor(e.midi / 12) - 1})`;
    });
  });
  svg.addEventListener("click", (ev) => {
    const g = ev.target.closest("g.n");
    if (!g) return;
    const e = events[+g.dataset.i];
    if (e && e.kind === "note") { const ctx = audio(); voiceNote(ctx, e.midi, ctx.currentTime + 0.01, 60 / state.tempo); flash(+g.dataset.i, 300); }
  });
  box.appendChild(svg);
}

/* ---------- sound ---------- */

let ctx = null, master = null, unlocked = false;
// iPhones play Web Audio through the "ambient" session, which the silent switch
// mutes. Asking for the "playback" session (Safari 17+), and on older iOS a
// looping silent <audio> started from a tap, sends it out of the speaker anyway.
function iosUnlock() {
  if (unlocked) return;
  unlocked = true;
  try { if (navigator.audioSession) navigator.audioSession.type = "playback"; } catch (e) {}
  try {
    const n = 4800, buf = new ArrayBuffer(44 + n * 2), v = new DataView(buf);
    const str = (o, s) => [...s].forEach((c, i) => v.setUint8(o + i, c.charCodeAt(0)));
    str(0, "RIFF"); v.setUint32(4, 36 + n * 2, true); str(8, "WAVEfmt "); v.setUint32(16, 16, true);
    v.setUint16(20, 1, true); v.setUint16(22, 1, true); v.setUint32(24, 48000, true);
    v.setUint32(28, 96000, true); v.setUint16(32, 2, true); v.setUint16(34, 16, true);
    str(36, "data"); v.setUint32(40, n * 2, true);
    const a = new Audio(URL.createObjectURL(new Blob([buf], { type: "audio/wav" })));
    a.loop = true;
    a.setAttribute("playsinline", "");
    a.play().catch(() => {});
  } catch (e) {}
}
function audio() {
  iosUnlock();
  if (!ctx) {
    ctx = new (window.AudioContext || window.webkitAudioContext)();
    const comp = ctx.createDynamicsCompressor();
    comp.threshold.value = -14; comp.ratio.value = 4;
    master = ctx.createGain();
    master.gain.value = 0.5;
    master.connect(comp).connect(ctx.destination);
  }
  if (ctx.state === "suspended") ctx.resume();
  return ctx;
}
const hz = (m) => 440 * Math.pow(2, (m - 69) / 12);
let live = [];

function partial(c, type, f, gainPeak, t, attack, decay, end, out) {
  const o = c.createOscillator();
  const g = c.createGain();
  o.type = type;
  o.frequency.value = f;
  g.gain.setValueAtTime(0.0001, t);
  g.gain.exponentialRampToValueAtTime(gainPeak, t + attack);
  if (decay) g.gain.setTargetAtTime(0.0001, t + attack, decay);
  g.gain.setTargetAtTime(0.0001, end, 0.04);
  o.connect(g).connect(out);
  o.start(t);
  o.stop(end + 0.4);
  live.push(o);
  o.onended = () => { live = live.filter((x) => x !== o); };
}

function voiceNote(c, midi, t, dur) {
  const f = hz(midi);
  const v = state.voice;
  if (v === "piano") {
    const lp = c.createBiquadFilter();
    lp.type = "lowpass";
    lp.frequency.setValueAtTime(f * 9, t);
    lp.frequency.setTargetAtTime(f * 2.5, t, 0.25);
    lp.connect(master);
    const end = t + Math.max(dur * 1.6, 0.6);
    partial(c, "triangle", f, 0.42, t, 0.004, 0.55, end, lp);
    partial(c, "sine", f * 2, 0.12, t, 0.004, 0.3, end, lp);
    partial(c, "sine", f * 1.002, 0.2, t, 0.004, 0.8, end, lp);
  } else if (v === "organ") {
    const end = t + dur * 0.92;
    [[1, 0.24], [2, 0.12], [3, 0.07], [4, 0.04], [0.5, 0.08]].forEach(([r, a]) =>
      partial(c, "sine", f * r, a, t, 0.02, 0, end, master));
  } else if (v === "bell") {
    const end = t + 2.6;
    partial(c, "sine", f, 0.32, t, 0.002, 0.9, end, master);
    partial(c, "sine", f * 2.76, 0.1, t, 0.002, 0.35, end, master);
    partial(c, "sine", f * 5.4, 0.05, t, 0.002, 0.15, end, master);
    partial(c, "sine", f * 2, 0.12, t, 0.002, 0.6, end, master);
  } else {
    const lp = c.createBiquadFilter();
    lp.type = "lowpass";
    lp.Q.value = 3;
    lp.frequency.setValueAtTime(f * 12, t);
    lp.frequency.exponentialRampToValueAtTime(Math.max(f * 1.2, 120), t + 0.35);
    lp.connect(master);
    partial(c, "sawtooth", f, 0.22, t, 0.003, 0.22, t + Math.max(dur, 0.5), lp);
  }
}

// One beat per letter that sounds (or rests), one beat for a word break.
function timeline() {
  const beat = 60 / state.tempo;
  const out = [];
  let t = 0;
  events.forEach((e, i) => {
    if (e.kind === "gap") { t += beat; return; }
    if (e.kind === "miss") { if (state.miss === "rest") { out.push({ i, t, rest: true }); t += beat; } return; }
    out.push({ i, t, midi: e.midi });
    t += beat;
  });
  return { list: out, length: t, beat };
}

let playing = false, raf = 0, startAt = 0, tl = null, loopTimer = 0;
function play() {
  stop();
  tl = timeline();
  if (!tl.list.some((x) => !x.rest)) return;
  const c = audio();
  startAt = c.currentTime + 0.06;
  for (const ev of tl.list) if (!ev.rest) voiceNote(c, ev.midi, startAt + ev.t, tl.beat);
  playing = true;
  $("play").textContent = "■ Stop";
  tick();
}
function stop() {
  playing = false;
  cancelAnimationFrame(raf);
  clearTimeout(loopTimer);
  const now = ctx ? ctx.currentTime : 0;
  for (const o of live) { try { o.stop(now + 0.05); } catch (e) {} }
  live = [];
  highlight(-1);
  $("play").textContent = "▶ Play";
}
function tick() {
  if (!playing) return;
  const now = ctx.currentTime - startAt;
  let cur = -1;
  for (const ev of tl.list) if (now >= ev.t && now < ev.t + tl.beat) cur = ev.i;
  highlight(cur);
  if (now >= tl.length) {
    if (state.loop) {
      const c = ctx;
      // re-schedule from the end of this pass so the loop stays in time
      const nextStart = startAt + tl.length + tl.beat;
      tl = timeline();
      for (const ev of tl.list) if (!ev.rest) voiceNote(c, ev.midi, nextStart + ev.t, tl.beat);
      startAt = nextStart;
    } else {
      playing = false;
      highlight(-1);
      $("play").textContent = "▶ Play";
      return;
    }
  }
  raf = requestAnimationFrame(tick);
}
let lit = -1;
function highlight(i) {
  if (i === lit) return;
  document.querySelectorAll(".on").forEach((n) => n.classList.remove("on"));
  lit = i;
  if (i < 0) return;
  document.querySelectorAll(`[data-i="${i}"]`).forEach((n) => n.classList.add("on"));
}
function flash(i, ms) {
  if (playing) return;
  highlight(i);
  setTimeout(() => { if (!playing) highlight(-1); }, ms);
}

/* ---------- MIDI ---------- */

function vlq(n) {
  const bytes = [n & 0x7f];
  while ((n >>= 7)) bytes.unshift((n & 0x7f) | 0x80);
  return bytes;
}
function makeMidi() {
  const PPQ = 480;
  const tr = [];
  const us = Math.round(60000000 / state.tempo);
  tr.push(0, 0xff, 0x51, 0x03, (us >> 16) & 255, (us >> 8) & 255, us & 255);
  const name = Array.from(new TextEncoder().encode(wordEl.value.trim().slice(0, 60) || "Note Words"));
  tr.push(0, 0xff, 0x03, ...vlq(name.length), ...name);
  const prog = { piano: 0, organ: 19, bell: 14, pluck: 24 }[state.voice] || 0;
  tr.push(0, 0xc0, prog);
  let pending = 0;
  for (const e of events) {
    if (e.kind === "gap" || (e.kind === "miss" && state.miss === "rest")) { pending += PPQ; continue; }
    if (e.kind !== "note") continue;
    tr.push(...vlq(pending), 0x90, e.midi, 96);
    tr.push(...vlq(PPQ - 24), 0x80, e.midi, 0);
    pending = 24;
  }
  tr.push(...vlq(pending), 0xff, 0x2f, 0x00);
  const head = [0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, PPQ >> 8, PPQ & 255];
  const len = tr.length;
  const trk = [0x4d, 0x54, 0x72, 0x6b, (len >>> 24) & 255, (len >> 16) & 255, (len >> 8) & 255, len & 255];
  return new Uint8Array([...head, ...trk, ...tr]);
}

/* ---------- toast, url ---------- */

function toast(msg) {
  const t = $("toast");
  t.textContent = msg;
  t.classList.add("show");
  clearTimeout(toast.t);
  toast.t = setTimeout(() => t.classList.remove("show"), 1600);
}
function syncUrl() {
  const p = new URLSearchParams();
  const w = wordEl.value.trim();
  if (w) p.set("w", w);
  if (state.sys !== "de") p.set("sys", state.sys);
  if (state.ases && state.sys === "de") p.set("ases", "1");
  const q = p.toString();
  try { history.replaceState(null, "", q ? "?" + q : location.pathname); } catch (e) {}
}

/* ---------- settings UI ---------- */

function syncControls() {
  document.querySelectorAll("[data-sys]").forEach((b) => b.setAttribute("aria-pressed", b.dataset.sys === state.sys));
  document.querySelectorAll("[data-miss]").forEach((b) => b.setAttribute("aria-pressed", b.dataset.miss === state.miss));
  document.querySelectorAll("[data-contour]").forEach((b) => b.setAttribute("aria-pressed", b.dataset.contour === state.contour));
  $("ases").checked = state.ases;
  $("asesRow").style.display = state.sys === "de" ? "" : "none";
  $("tempo").value = state.tempo;
  $("tempoVal").textContent = state.tempo + " bpm";
  $("voice").value = state.voice;
  $("loop").checked = state.loop;
  $("sysHint").textContent = state.sys === "de"
    ? "German note names: A B C D E F G H, where B is B♭ and H is B natural. S sounds like “Es”, which is E♭ — that is how Shostakovich got D‑S‑C‑H and Schumann got A‑S‑C‑H."
    : "The French system writes the alphabet under the scale again and again (H–N, O–U, V–Z), so every letter has a note. H keeps its German B, as in the 1909 pieces on HAYDN by Ravel and Debussy.";
}

function update() {
  if (playing) stop();
  events = parse(wordEl.value);
  renderStrip();
  renderStaff();
  syncUrl();
}

function load(word) {
  wordEl.value = word;
  update();
  play();
}

document.querySelectorAll("[data-sys]").forEach((b) => b.addEventListener("click", () => { state.sys = b.dataset.sys; persist(); syncControls(); update(); renderMap(); }));
document.querySelectorAll("[data-miss]").forEach((b) => b.addEventListener("click", () => { state.miss = b.dataset.miss; persist(); syncControls(); update(); }));
document.querySelectorAll("[data-contour]").forEach((b) => b.addEventListener("click", () => { state.contour = b.dataset.contour; persist(); syncControls(); update(); }));
$("ases").addEventListener("change", (e) => { state.ases = e.target.checked; persist(); update(); });
$("tempo").addEventListener("input", (e) => { state.tempo = +e.target.value; $("tempoVal").textContent = state.tempo + " bpm"; persist(); });
$("tempo").addEventListener("change", () => { if (playing) play(); });
$("voice").addEventListener("change", (e) => { state.voice = e.target.value; persist(); if (playing) play(); });
$("loop").addEventListener("change", (e) => { state.loop = e.target.checked; persist(); });
wordEl.addEventListener("input", update);
wordEl.addEventListener("keydown", (e) => { if (e.key === "Enter") { e.preventDefault(); play(); } });
$("play").addEventListener("click", () => (playing ? stop() : play()));
$("midi").addEventListener("click", () => {
  if (!events.some((e) => e.kind === "note")) { toast("Nothing to save yet"); return; }
  const blob = new Blob([makeMidi()], { type: "audio/midi" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = (wordEl.value.trim().replace(/[^\p{L}\p{N}]+/gu, "-").replace(/^-|-$/g, "") || "note-words") + ".mid";
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
});
$("share").addEventListener("click", async () => {
  syncUrl();
  try { await navigator.clipboard.writeText(location.href); toast("Link copied"); }
  catch (e) { prompt("Copy this link:", location.href); }
});
let resizeT = 0;
window.addEventListener("resize", () => { clearTimeout(resizeT); resizeT = setTimeout(() => { renderStaff(); if (lit >= 0) { const i = lit; lit = -1; highlight(i); } }, 120); });

/* ---------- motifs, finder, table ---------- */

const MOTIFS = [
  ["BACH", "J. S. Bach signs The Art of Fugue: B♭ A C B"],
  ["DSCH", "Shostakovich’s monogram — D. Schostakowitsch"],
  ["ASCH", "Schumann’s Carnaval, from the town Asch; try “As” as A♭"],
  ["ABEGG", "Schumann’s Op. 1, for Countess Pauline von Abegg"],
  ["GADE", "Schumann’s album leaf for the Danish composer Niels Gade"],
  ["FAE", "“Frei aber einsam” — the F‑A‑E Sonata for Joachim"],
  ["CAGE", "John Cage, as set by Pauline Oliveros and others"],
  ["HAYDN", "Ravel’s Menuet sur le nom d’Haydn — switch to French"],
];
function renderMotifs() {
  const box = $("motifs");
  for (const [w, d] of MOTIFS) {
    const b = document.createElement("button");
    b.type = "button";
    b.className = "motif";
    b.innerHTML = `<b></b><small></small>`;
    b.querySelector("b").textContent = w;
    b.querySelector("small").textContent = d;
    b.addEventListener("click", () => {
      if (w === "HAYDN" && state.sys !== "fr") { state.sys = "fr"; persist(); syncControls(); renderMap(); }
      load(w);
    });
    box.appendChild(b);
  }
}

// Common English words (SCOWL sizes 10–50) using only A–H and S.
const WORDS_EN = "BACH BEACHHEADS ABSCESSED ABSCESSES BEACHHEAD BESEECHES HEADACHES ABBESSES ACCESSED ACCESSES ASSESSED ASSESSES BEHEADED CABBAGES CASCADED CASCADES DECEASED DECEASES EGGHEADS FEEDBAGS HEADACHE ABASHED ABASHES ABSCESS ACCEDED ACCEDES BAGGAGE BEACHED BEACHES BEECHES BEHEADS BESEECH CABBAGE CASCADE CHAFFED CHEESED CHEESES DEBASED DEBASES DECADES DECEASE DEFACED DEFACES EFFACED EFFACES EGGHEAD FACADES FEEDBAG SCABBED SEABEDS SECEDED SECEDES ABASED ABASES ABBESS ACCEDE ACCESS ADAGES ASSESS BADGES BAGGED BASHED BASHES BASSES BEADED BEDDED BEEFED BEGGED BEHEAD CABBED CACHED CACHES CADGED CADGES CASHED CASHES CEASED CEASES CHAFED CHAFES CHAFFS CHASED CHASES CHEESE DABBED DACHAS DASHED DASHES DEBASE DECADE DEEDED DEFACE EFFACE FACADE GABBED GADDED GAFFED GAFFES GAGGED GASHED GASHES GASSED HASHED HASHES HEADED HEDGED HEDGES HEEDED SAGGED SASHES SASSED SASSES SEABED SECEDE SEEDED SHADED SHADES ABASE ABASH ACHED ACHES ADAGE ADDED AHEAD ASHED ASHES BAAED BABES BADGE BASED BASES BEACH BEADS BEECH BEEFS CACHE CADGE CAGED CAGES CASED CASES CEASE CEDED CEDES CHAFE CHAFF CHASE CHEFS CHESS DACHA DECAF DEEDS EASED EASES EBBED EDGED EDGES EGGED FACED FACES FADED FADES FEEDS GAFFE GAFFS GASES GEESE HEADS HEDGE HEEDS SAFES SAGAS SAGES SCABS SEDGE SEEDS SHADE SHADS SHAHS SHEAF SHEDS ABED ACED ACES ACHE ADDS AGED AGES BAAS BABE BADE BAGS BASE BASH BASS BEAD BEDS BEEF BEES BEGS CABS CADS CAGE CASE CASH CEDE CHEF DABS DADS DASH DEAD DEAF DEED EACH EASE EBBS EDGE EGGS FACE FADE FADS FEDS FEED FEES GABS GAFF GAGS GASH GEED GEES HAGS HASH HEAD HEED SACS SAFE SAGA SAGE SAGS SASH SASS SCAB SEAS SECS SEED SEES SHAD SHAH SHED ACE ADD ADS AGE AHA ASH BAA BAD BAG BAH BED BEE BEG CAB CAD DAB DAD DEB EBB EGG FAD FED FEE GAB GAD GAG GAS GEE HAD HAG HAS SAC SAD SAG SEA SEE SHE".split(" ");
// Danish words in common use (subtitle frequencies, checked against the
// Stavekontrolden dictionary) using only A–H and S, picked by hand: no
// abbreviations or English loans. Danish names its notes the German way.
const WORDS_DA = "BACH BEHAGEDE BEDEDAG BADEDAG GASSEDE HADEDE BADEDE EBBEDE FASEDE AFFASE AFGASSE BEHAGE BAGAGE FACADE GEDDE ABSCES ABBED BEHAG BASSE GASSE CHEFS FEDES BEDES DAGES GABES GAGES BADES DAFFE GAFFE CACHE EBBE GAGA FADE DAGE BEDE FEDE HADE BASE FASE GADE BADE BAGE GABE HAGE HEDE SAGA CAFE GAGE ABES GEDE ESSE DASE CHEF DAG BAD BAG SAG BED HED FED GAS ABE HAD GED AHA GAB FAD BAS BAH BEG GAG".split(" ");
const wordList = () => (state.lang === "da" ? WORDS_DA : WORDS_EN);
function renderWords() {
  const q = $("find").value.toUpperCase().replace(/[^A-Z]/g, "");
  const list = wordList().filter((w) => !q || w.includes(q));
  const box = $("words");
  box.textContent = "";
  const frag = document.createDocumentFragment();
  for (const w of list) {
    const b = document.createElement("button");
    b.type = "button";
    b.textContent = w;
    b.addEventListener("click", () => {
      if (state.sys !== "de") { state.sys = "de"; persist(); syncControls(); renderMap(); }
      load(w);
    });
    frag.appendChild(b);
  }
  box.appendChild(frag);
  $("count").textContent = state.lang === "da"
    ? `${list.length} ord`
    : `${list.length} word${list.length === 1 ? "" : "s"}`;
  $("findHint").textContent = state.lang === "da"
    ? "Almindelige danske ord, der kun består af A–H og S, længste først. Klik på et ord for at spille det."
    : "Common English words spelled entirely from A–H and S, longest first. Click one to play it.";
  document.querySelectorAll("[data-lang]").forEach((b) => b.setAttribute("aria-pressed", b.dataset.lang === state.lang));
}
$("find").addEventListener("input", renderWords);
document.querySelectorAll("[data-lang]").forEach((b) => b.addEventListener("click", () => { state.lang = b.dataset.lang; persist(); renderWords(); }));
$("random").addEventListener("click", () => {
  const long = wordList().filter((w) => w.length >= 4);
  const w = long[Math.floor(Math.random() * long.length)];
  if (state.sys !== "de") { state.sys = "de"; persist(); syncControls(); renderMap(); }
  load(w);
});

function renderMap() {
  const t = $("map");
  t.textContent = "";
  const letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const rows = [["Letter", (ch) => ch], ["Note", (ch) => {
    const pc = state.sys === "de" ? (ch in GERMAN ? GERMAN[ch] : null) : frenchPc(ch);
    return pc === null ? "–" : shownName(pc);
  }]];
  for (const [head, fn] of rows) {
    const tr = document.createElement("tr");
    const th = document.createElement("th");
    th.textContent = head;
    tr.appendChild(th);
    for (const ch of letters) {
      const td = document.createElement("td");
      td.textContent = fn(ch);
      if (head === "Note" && td.textContent === "–") td.style.color = "var(--faint)";
      tr.appendChild(td);
    }
    t.appendChild(tr);
  }
}

/* ---------- start ---------- */

(function init() {
  const p = new URLSearchParams(location.search);
  if (p.get("sys") === "fr" || p.get("sys") === "de") state.sys = p.get("sys");
  if (p.has("ases")) state.ases = p.get("ases") === "1";
  wordEl.value = p.get("w") || "BACH";
  syncControls();
  renderMotifs();
  renderWords();
  renderMap();
  update();
})();
