/* Sound design: every change makes a sound, and a slow, spacious ambient track plays underneath.
 * Everything is synthesised with Web Audio (owner rule: no audio files he has not supplied).
 *   UI       ticks whose pitch follows the value (sliders, dials, dragging a planet), clicks for buttons and tabs,
 *            two-note toggles for settings, a thunk on taking hold of something and a soft tone on letting go,
 *            chimes up/down for adding/removing, a whoosh for presets and for entering VR,
 *            the ledger: a bright little chord when a tier is reached, a dark low tone when one is lost
 *   Music    pads (detuned oscillators, low-passed, slow attack) moving through a modal progression, a sub drone,
 *            sparse bell tones, a faint band-passed "space wind"; the star's temperature sets the key and brightness
 * Light on a Quest: a handful of voices, one short convolution reverb, nothing spatial (pitfall from Portalkniven:
 * HRTF panners and long convolvers cost frame rate). The context starts on the first user gesture (autoplay rule). */

const PREF = 'planetDesigner.audio';
const MAJ = [0, 2, 4, 7, 9, 12, 14, 16];
const DORIAN = [0, 2, 3, 5, 7, 9, 10];
/* chord roots (scale degrees) of a slow i - VI - III - VII style progression, each held ~18 s */
const PROGRESSION = [[0, 3, 7, 10], [-4, 0, 3, 7], [3, 7, 10, 14], [-2, 2, 5, 9]];
const mtof = (m) => 440 * 2 ** ((m - 69) / 12);
const GAIN = 1.8;                  /* overall level (peaks about -12 dB before the compressor) */

export function createAudio() {
  let prefs = { music: true, fx: true, vol: 0.7 };
  try { prefs = { ...prefs, ...JSON.parse(localStorage.getItem(PREF) || '{}') }; } catch (e) { /* ignore */ }
  const A = { ctx: null, master: null, fxBus: null, musicBus: null, verb: null, started: false, stats: { fx: 0, notes: 0 }, last: {} };
  let rootMidi = 50, brightness = 1;             /* set from the star */
  let pendingRoot = null;

  function save() { try { localStorage.setItem(PREF, JSON.stringify(prefs)); } catch (e) { /* ignore */ } }

  function impulse(ctx, seconds, decay) {
    const n = Math.floor(ctx.sampleRate * seconds), buf = ctx.createBuffer(2, n, ctx.sampleRate);
    for (let c = 0; c < 2; c++) {
      const d = buf.getChannelData(c);
      for (let i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / n) ** decay;
    }
    return buf;
  }

  function init() {
    if (A.ctx) return;
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    const ctx = new AC();
    A.ctx = ctx;
    A.master = ctx.createGain(); A.master.gain.value = prefs.vol * GAIN;
    const comp = ctx.createDynamicsCompressor(); comp.threshold.value = -18; comp.ratio.value = 3;
    A.master.connect(comp).connect(ctx.destination);
    A.verb = ctx.createConvolver(); A.verb.buffer = impulse(ctx, 2.6, 2.4);
    const verbOut = ctx.createGain(); verbOut.gain.value = 0.55;
    A.verb.connect(verbOut).connect(A.master);
    A.fxBus = ctx.createGain(); A.fxBus.gain.value = prefs.fx ? 0.55 : 0;
    A.fxBus.connect(A.master);
    const fxSend = ctx.createGain(); fxSend.gain.value = 0.25; A.fxBus.connect(fxSend).connect(A.verb);
    A.musicBus = ctx.createGain(); A.musicBus.gain.value = 0;
    A.musicBus.connect(A.master);
    const mSend = ctx.createGain(); mSend.gain.value = 0.9; A.musicBus.connect(mSend).connect(A.verb);
    A.noise = (() => { const b = ctx.createBuffer(1, ctx.sampleRate * 2, ctx.sampleRate); const d = b.getChannelData(0);
      let p = 0; for (let i = 0; i < d.length; i++) { p = 0.97 * p + 0.03 * (Math.random() * 2 - 1); d[i] = p * 6; } return b; })();
  }

  /* start on the first gesture; the music fades in */
  function start() {
    init();
    if (!A.ctx) return;
    if (A.ctx.state === 'suspended') A.ctx.resume();
    if (!A.started) { A.started = true; startMusic(); }
    setMusic(prefs.music);
  }
  const gesture = () => start();
  window.addEventListener('pointerdown', gesture, { passive: true });
  window.addEventListener('keydown', gesture);

  /* ---------- effects ---------- */
  function blip(freq, { dur = 0.09, gain = 0.12, type = 'sine', pan = 0, verb = false } = {}) {
    if (!A.ctx || !prefs.fx || A.ctx.state !== 'running') return;
    const ctx = A.ctx, t = ctx.currentTime;
    const o = ctx.createOscillator(), g = ctx.createGain(), p = ctx.createStereoPanner();
    o.type = type; o.frequency.value = freq;
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(gain, t + 0.005); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    p.pan.value = pan;
    o.connect(g).connect(p).connect(A.fxBus);
    if (verb) p.connect(A.verb);
    o.start(t); o.stop(t + dur + 0.02);
    o.onended = () => { o.disconnect(); g.disconnect(); p.disconnect(); };
    A.stats.fx++;
  }
  function noiseBurst({ dur = 0.03, gain = 0.08, freq = 3000, q = 1, sweepTo = null } = {}) {
    if (!A.ctx || !prefs.fx || A.ctx.state !== 'running') return;
    const ctx = A.ctx, t = ctx.currentTime;
    const s = ctx.createBufferSource(); s.buffer = A.noise;
    const f = ctx.createBiquadFilter(); f.type = 'bandpass'; f.frequency.setValueAtTime(freq, t); f.Q.value = q;
    if (sweepTo) f.frequency.exponentialRampToValueAtTime(sweepTo, t + dur);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(gain, t + Math.min(0.02, dur / 3)); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    s.connect(f).connect(g).connect(A.fxBus);
    if (sweepTo) g.connect(A.verb);
    s.start(t, Math.random()); s.stop(t + dur + 0.05);
    s.onended = () => { s.disconnect(); f.disconnect(); g.disconnect(); };
    A.stats.fx++;
  }
  const click = () => { noiseBurst({ dur: 0.03, gain: 0.1, freq: 3200, q: 2 }); blip(1400, { dur: 0.04, gain: 0.06, type: 'triangle' }); };

  /* A value changed: a tick whose pitch follows the value, at most every ~40 ms and only when the value moved enough
   * to be heard as a step (so a dial at 72 Hz ticks like a ratchet, not a buzz). */
  function param(path, value, pos) {
    const now = performance.now();
    const k = path.replace(/\.\d+\./, '.n.');
    const L = A.last[k] || (A.last[k] = { t: 0, q: null });
    const q = Math.round(pos * 60);
    if (q === L.q || now - L.t < 40) return false;
    const dir = L.q == null ? 0 : Math.sign(q - L.q);
    L.q = q; L.t = now;
    const base = /aAU/.test(k) ? 1900 - 1300 * pos : /massSun/.test(k) ? 300 + 900 * pos : 500 + 900 * pos;
    /* a clear ratchet tick: a short click for the attack, the pitched tone, and a soft octave under it */
    noiseBurst({ dur: 0.02, gain: 0.06, freq: 2600, q: 3 });
    blip(base, { dur: 0.08, gain: 0.13, type: /massSun|ageGyr/.test(k) ? 'triangle' : 'sine' });
    blip(base / 2, { dur: 0.07, gain: 0.05, type: 'triangle' });
    A.lastDir = dir;
    return true;
  }
  /* a setting switched (chips, toggles): two quick notes, rising for "on"/the later option, falling for "off" */
  function toggle(up) {
    const r = rootMidi + 31;
    const seq = up ? [0, 7] : [7, 0];
    seq.forEach((iv, i) => setTimeout(() => blip(mtof(r + iv), { dur: 0.16, gain: 0.1, type: 'triangle', verb: true }), i * 60));
  }
  /* taking hold of something (a dial, a planet, the star, the world) and letting go */
  function grab() { blip(180, { dur: 0.14, gain: 0.16, type: 'triangle' }); noiseBurst({ dur: 0.05, gain: 0.07, freq: 900, q: 1.5 }); }
  function release() { blip(260, { dur: 0.1, gain: 0.08, type: 'sine' }); }
  /* a snap turn: a short air swish */
  function swish() { noiseBurst({ dur: 0.22, gain: 0.07, freq: 700, q: 0.9, sweepTo: 1800 }); }
  function chime(up) {
    const r = rootMidi + 24;
    const seq = up ? [0, 4, 7, 12] : [12, 7, 3, 0];
    seq.forEach((iv, i) => setTimeout(() => blip(mtof(r + iv), { dur: 0.6, gain: 0.06, verb: true }), i * 70));
  }
  function whoosh() { noiseBurst({ dur: 0.9, gain: 0.12, freq: 300, q: 0.8, sweepTo: 2600 }); }
  /* the ledger: a tier reached (bright, more notes for higher tiers) or lost (low, muffled) */
  function tier(up, n) {
    const r = rootMidi + 24;
    if (up) MAJ.slice(0, 2 + n).forEach((iv, i) => setTimeout(() => blip(mtof(r + iv), { dur: 1.2, gain: 0.05, verb: true }), i * 90));
    else [0, -5].forEach((iv, i) => setTimeout(() => blip(mtof(r - 12 + iv), { dur: 0.9, gain: 0.09, type: 'triangle', verb: true }), i * 160));
  }

  /* ---------- music ---------- */
  const music = { voices: [], step: 0, timer: null, bellTimer: null, wind: null, sub: null };
  function pad(midi, when, hold) {
    const ctx = A.ctx;
    const g = ctx.createGain(), f = ctx.createBiquadFilter();
    f.type = 'lowpass'; f.Q.value = 0.7;
    f.frequency.setValueAtTime(300 * brightness, when);
    f.frequency.linearRampToValueAtTime(900 * brightness, when + hold * 0.5);
    f.frequency.linearRampToValueAtTime(350 * brightness, when + hold + 6);
    g.gain.setValueAtTime(0.0001, when);
    g.gain.linearRampToValueAtTime(0.045, when + 6);
    g.gain.setValueAtTime(0.045, when + hold);
    g.gain.linearRampToValueAtTime(0.0001, when + hold + 7);
    const oscs = [-7, 7].map((cents, i) => {
      const o = ctx.createOscillator(); o.type = i ? 'triangle' : 'sawtooth';
      o.frequency.value = mtof(midi); o.detune.value = cents;
      o.connect(f); o.start(when); o.stop(when + hold + 7.5);
      return o;
    });
    f.connect(g).connect(A.musicBus);
    oscs[0].onended = () => { oscs.forEach((o) => o.disconnect()); f.disconnect(); g.disconnect(); };
    A.stats.notes++;
  }
  function nextChord() {
    if (!A.ctx) return;
    if (pendingRoot !== null) { rootMidi = pendingRoot; pendingRoot = null; if (music.sub) music.sub.frequency.setTargetAtTime(mtof(rootMidi - 24), A.ctx.currentTime, 4); }
    const chord = PROGRESSION[music.step % PROGRESSION.length];
    const t = A.ctx.currentTime + 0.1, hold = 14;
    chord.forEach((iv) => pad(rootMidi + 12 + iv, t, hold));
    music.step++;
  }
  function bell() {
    if (!A.ctx || A.musicBus.gain.value < 0.01) return;
    const ctx = A.ctx, t = ctx.currentTime;
    const deg = DORIAN[Math.floor(Math.random() * DORIAN.length)] + 12 * (2 + Math.floor(Math.random() * 2));
    const f0 = mtof(rootMidi + deg);
    const car = ctx.createOscillator(), mod = ctx.createOscillator(), mg = ctx.createGain(), g = ctx.createGain(), p = ctx.createStereoPanner();
    car.frequency.value = f0; mod.frequency.value = f0 * 2.76; mg.gain.setValueAtTime(f0 * 1.2, t); mg.gain.exponentialRampToValueAtTime(1, t + 3);
    mod.connect(mg).connect(car.frequency);
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.03, t + 0.02); g.gain.exponentialRampToValueAtTime(0.0001, t + 4.5);
    p.pan.value = Math.random() * 1.4 - 0.7;
    car.connect(g).connect(p).connect(A.musicBus);
    car.start(t); mod.start(t); car.stop(t + 4.6); mod.stop(t + 4.6);
    car.onended = () => { [car, mod, mg, g, p].forEach((n) => n.disconnect()); };
    A.stats.notes++;
  }
  function startMusic() {
    const ctx = A.ctx;
    /* sub drone */
    music.sub = ctx.createOscillator(); music.sub.type = 'sine'; music.sub.frequency.value = mtof(rootMidi - 24);
    const sg = ctx.createGain(); sg.gain.value = 0.05; music.sub.connect(sg).connect(A.musicBus); music.sub.start();
    /* space wind: looped noise, band-passed, its centre drifting with a slow LFO */
    const w = ctx.createBufferSource(); w.buffer = A.noise; w.loop = true;
    const wf = ctx.createBiquadFilter(); wf.type = 'bandpass'; wf.frequency.value = 500; wf.Q.value = 3;
    const lfo = ctx.createOscillator(); lfo.frequency.value = 0.03; const lg = ctx.createGain(); lg.gain.value = 300;
    lfo.connect(lg).connect(wf.frequency); lfo.start();
    const wg = ctx.createGain(); wg.gain.value = 0.012; w.connect(wf).connect(wg).connect(A.musicBus); w.start();
    nextChord();
    music.timer = setInterval(nextChord, 18000);
    const ringBell = () => { bell(); music.bellTimer = setTimeout(ringBell, 5000 + Math.random() * 9000); };
    music.bellTimer = setTimeout(ringBell, 4000);
  }
  function setMusic(on) {
    if (!A.ctx) return;
    const t = A.ctx.currentTime;
    A.musicBus.gain.cancelScheduledValues(t);
    A.musicBus.gain.setTargetAtTime(on ? 0.6 : 0, t, on ? 2.5 : 0.6);
  }

  return {
    stats: A.stats,
    get ctx() { return A.ctx; },
    prefs,
    start,
    setPref(k, v) { prefs[k] = v; save(); if (!A.ctx) return;
      if (k === 'music') setMusic(v);
      if (k === 'fx') A.fxBus.gain.setTargetAtTime(v ? 0.55 : 0, A.ctx.currentTime, 0.05);
      if (k === 'vol') A.master.gain.setTargetAtTime(v * GAIN, A.ctx.currentTime, 0.05); },
    param, click, chime, whoosh, tier, toggle, grab, release, swish,
    /* the star sets the key (cool stars lower) and the brightness of the pads; applied at the next chord */
    setStar(teff) {
      const k = Math.max(-5, Math.min(5, Math.round(Math.log2(teff / 5772) * 9)));
      pendingRoot = 50 + k;
      brightness = Math.max(0.5, Math.min(1.8, (teff / 5772) ** 0.8));
    },
    /* for a recorded preview: the master bus as a MediaStream */
    tap() { if (!A.ctx) return null; const d = A.ctx.createMediaStreamDestination(); A.master.connect(d); return d.stream; },
    /* for tests: the level on the master bus */
    level() { if (!A.ctx) return 0; const an = A.ctx.createAnalyser(); A.master.connect(an); const d = new Float32Array(an.fftSize);
      return new Promise((res) => setTimeout(() => { an.getFloatTimeDomainData(d); let s = 0; for (const v of d) s += v * v; A.master.disconnect(an); res(Math.sqrt(s / d.length)); }, 200)); },
  };
}
