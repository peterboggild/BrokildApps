/* The launch as you hear and feel it inside the Command Module: all synthesised.
 *
 * - The cabin: fans and the coolant pumps, always.
 * - The Saturn V through the structure: a deep rumble and the crackle of the five first-stage engines, loudest in
 *   the first half minute (sound reflected off the ground) and through max Q, then steadier and lower. Silence at
 *   staging, a bang from the separation charges, then the smoother second-stage engines; the escape tower's motor
 *   firing above you; the third stage quiet.
 * - The controllers vibrate with it (the view itself is never shaken).
 * - The radio (radio.js) feeds in here, so all sound goes through one compressor. */
import { EVENTS } from './ascent.js?v=37';

const smooth = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };

/* how strongly the rocket is felt and heard at time t: rumble (low), roar (mid), crackle, shake 0..1 */
export function intensity(t) {
  const E = EVENTS;
  if (t < E.ignition) return { rumble: 0, roar: 0, crackle: 0, shake: 0 };
  if (t < 0) { const k = smooth(E.ignition, -1, t); return { rumble: 0.55 * k, roar: 0.25 * k, crackle: 0.3 * k, shake: 0.45 * k }; }
  if (t < E.sicCutoff) {
    const ground = 1 - smooth(5, 35, t);                  /* the noise reflected off the ground, while it is near */
    const q = Math.exp(-Math.pow((t - 75) / 22, 2));      /* transonic buffeting and max Q */
    const inboard = t > E.iCutoff ? 0.85 : 1;
    return { rumble: (0.65 + 0.35 * ground + 0.1 * q) * inboard, roar: (0.35 + 0.45 * ground + 0.35 * q) * inboard,
             crackle: (0.25 + 0.75 * ground) * inboard, shake: (0.55 + 0.45 * ground + 0.5 * q) * inboard };
  }
  if (t < E.siiIgnition) return { rumble: 0, roar: 0, crackle: 0, shake: 0 };
  if (t < E.siiCutoff) { const k = smooth(E.siiIgnition, E.siiIgnition + 2, t) * (t > E.siiCECO ? 0.8 : 1); return { rumble: 0.32 * k, roar: 0.12 * k, crackle: 0, shake: 0.16 * k }; }
  if (t < E.sivbIgnition) return { rumble: 0, roar: 0, crackle: 0, shake: 0 };
  if (t < E.seco) { const k = smooth(E.sivbIgnition, E.sivbIgnition + 2, t); return { rumble: 0.2 * k, roar: 0.05 * k, crackle: 0, shake: 0.08 * k }; }
  return { rumble: 0, roar: 0, crackle: 0, shake: 0 };
}

export function makeLaunchSound() {
  let ctx = null, out = null, voice = null, rumbleG, roarG, crackleG, fanG, ready = false;
  const level = { sound: 1, voices: 1 };
  const bangsDone = new Set();

  function init() {
    if (ctx) { if (ctx.state === 'suspended') ctx.resume(); return ctx; }
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return null;
    ctx = new AC();
    const comp = ctx.createDynamicsCompressor();
    comp.threshold.value = -14; comp.ratio.value = 4; comp.attack.value = 0.01; comp.release.value = 0.25;
    /* two buses into the compressor: out (engines, the cabin, bangs) and voice (the radio), each with its own volume */
    const master = ctx.createGain(); master.gain.value = 0.9; master.connect(comp).connect(ctx.destination);
    out = ctx.createGain(); out.gain.value = level.sound; out.connect(master);
    voice = ctx.createGain(); voice.gain.value = level.voices; voice.connect(master);
    const sr = ctx.sampleRate;
    const noise = (sec, kind) => {
      const n = Math.floor(sr * sec), b = ctx.createBuffer(1, n, sr), d = b.getChannelData(0);
      let a = 0, lp = 0;
      for (let i = 0; i < n; i++) {
        const w = Math.random() * 2 - 1;
        if (kind === 'brown') { a = 0.996 * a + 0.04 * w; d[i] = a * 3.2; }
        else if (kind === 'crackle') {                        /* sparse, sharp impulses: the sound of the F-1s */
          lp = Math.random() < 0.004 ? (Math.random() * 2 - 1) * 1.0 : lp * 0.9;
          d[i] = lp * (0.6 + 0.4 * Math.random());
        } else d[i] = w;
      }
      const s = ctx.createBufferSource(); s.buffer = b; s.loop = true; return s;
    };
    /* rumble: brown noise, low-passed, its level wandering a little */
    const rb = noise(4, 'brown'), rlp = ctx.createBiquadFilter(); rlp.type = 'lowpass'; rlp.frequency.value = 110; rlp.Q.value = 0.8;
    rumbleG = ctx.createGain(); rumbleG.gain.value = 0;
    const wob = ctx.createOscillator(), wobG = ctx.createGain(); wob.frequency.value = 3.3; wobG.gain.value = 0;
    wob.connect(wobG).connect(rumbleG.gain);
    rb.connect(rlp).connect(rumbleG).connect(out); rb.start(); wob.start();
    rumbleG.wob = wobG;
    /* roar: band-limited noise through the hull */
    const ro = noise(3, 'white'), rbp = ctx.createBiquadFilter(); rbp.type = 'bandpass'; rbp.frequency.value = 320; rbp.Q.value = 0.6;
    const rlp2 = ctx.createBiquadFilter(); rlp2.type = 'lowpass'; rlp2.frequency.value = 900;
    roarG = ctx.createGain(); roarG.gain.value = 0;
    ro.connect(rbp).connect(rlp2).connect(roarG).connect(out); ro.start();
    /* crackle */
    const cr = noise(5, 'crackle'), cbp = ctx.createBiquadFilter(); cbp.type = 'bandpass'; cbp.frequency.value = 700; cbp.Q.value = 0.5;
    crackleG = ctx.createGain(); crackleG.gain.value = 0;
    cr.connect(cbp).connect(crackleG).connect(out); cr.start();
    /* cabin: fans and pumps */
    const fn = noise(3, 'white'), fbp = ctx.createBiquadFilter(); fbp.type = 'bandpass'; fbp.frequency.value = 480; fbp.Q.value = 0.7;
    fanG = ctx.createGain(); fanG.gain.value = 0.035;
    fn.connect(fbp).connect(fanG).connect(out); fn.start();
    for (const [f, a] of [[120, 0.012], [400, 0.004]]) {
      const o = ctx.createOscillator(); o.frequency.value = f; const og = ctx.createGain(); og.gain.value = a;
      o.connect(og).connect(out); o.start();
    }
    ready = true;
    return ctx;
  }

  /* a short, hard noise burst through the structure (pyrotechnics), or the escape motor's roar */
  function bang(level, dur, freq) {
    const t = ctx.currentTime, sr = ctx.sampleRate, n = Math.floor(sr * dur), b = ctx.createBuffer(1, n, sr), d = b.getChannelData(0);
    for (let i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * Math.exp(-i / sr / (dur * 0.25));
    const s = ctx.createBufferSource(); s.buffer = b;
    const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = freq;
    const g = ctx.createGain(); g.gain.value = level;
    s.connect(f).connect(g).connect(out); s.start(t);
    const o = ctx.createOscillator(), og = ctx.createGain();
    o.frequency.setValueAtTime(70, t); o.frequency.exponentialRampToValueAtTime(30, t + 0.3);
    og.gain.setValueAtTime(level * 0.9, t); og.gain.exponentialRampToValueAtTime(0.001, t + 0.5);
    o.connect(og).connect(out); o.start(t); o.stop(t + 0.6);
  }

  /* the hatch closing: a heavy thud, then the latch ratchet */
  function hatch() {
    if (!ready) return;
    bang(0.55, 0.35, 500);
    for (let k = 0; k < 7; k++) setTimeout(() => bang(0.12, 0.05, 2500), 450 + k * 160);
  }
  /* on the arm: wind through the walkway, the vehicle venting (a hiss that comes and goes) */
  let padAmb = null;
  function ambience(on) {
    if (!ready) return;
    if (!padAmb) {
      const sr = ctx.sampleRate, n = sr * 4, b = ctx.createBuffer(1, n, sr), d = b.getChannelData(0);
      for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1;
      const w = ctx.createBufferSource(); w.buffer = b; w.loop = true;
      const wf = ctx.createBiquadFilter(); wf.type = 'bandpass'; wf.frequency.value = 350; wf.Q.value = 0.4;
      const wg = ctx.createGain(); wg.gain.value = 0;
      const lfo = ctx.createOscillator(), lg = ctx.createGain(); lfo.frequency.value = 0.13; lg.gain.value = 0.025; lfo.connect(lg).connect(wg.gain); lfo.start();
      w.connect(wf).connect(wg).connect(out); w.start();
      const v = ctx.createBufferSource(); v.buffer = b; v.loop = true; v.playbackRate.value = 0.7;
      const vf = ctx.createBiquadFilter(); vf.type = 'highpass'; vf.frequency.value = 1800;
      const vg = ctx.createGain(); vg.gain.value = 0;
      v.connect(vf).connect(vg).connect(out); v.start();
      padAmb = { wg, vg, next: 0 };
    }
    padAmb.wg.gain.setTargetAtTime(on ? 0.05 : 0, ctx.currentTime, 0.8);
    if (!on) padAmb.vg.gain.setTargetAtTime(0, ctx.currentTime, 0.3);
    else if (ctx.currentTime > padAmb.next) {
      padAmb.next = ctx.currentTime + 5 + Math.random() * 6;
      const t = ctx.currentTime;
      padAmb.vg.gain.setTargetAtTime(0.035, t, 0.4); padAmb.vg.gain.setTargetAtTime(0, t + 1.5 + Math.random() * 2, 0.8);
    }
  }

  /* every frame: levels follow the flight; one-off sounds at their moments */
  function update(t, warp) {
    if (!ready) return;
    const k = intensity(t), now = ctx.currentTime, mute = warp > 1 ? 0.35 : 1;
    rumbleG.gain.setTargetAtTime(1.1 * k.rumble * mute, now, 0.08);
    rumbleG.wob.gain.setTargetAtTime(0.25 * k.rumble * mute, now, 0.2);
    roarG.gain.setTargetAtTime(0.5 * k.roar * mute, now, 0.08);
    crackleG.gain.setTargetAtTime(1.4 * k.crackle * mute, now, 0.08);
    const E = EVENTS;
    const once = (name, at, fn) => { if (t >= at && t < at + 1.5 && !bangsDone.has(name)) { bangsDone.add(name); fn(); } if (t < at) bangsDone.delete(name); };
    once('holddown', 0, () => bang(0.6, 0.6, 400));
    once('stage1', E.sicCutoff + 0.6, () => bang(0.9, 0.8, 700));
    once('tower', E.towerJett, () => { bang(1.0, 0.5, 1500); bang(0.5, 3.5, 900); });
    once('stage2', E.siiCutoff + 0.8, () => bang(0.6, 0.6, 600));
  }

  /* outside the launch: an engine running (k 0..1: the S-IVB's J-2 at 1, the SPS less), and single bangs */
  function drive(k, warp = 1) {
    if (!ready) return;
    const now = ctx.currentTime, mute = warp > 1 ? 0.3 : 1;
    rumbleG.gain.setTargetAtTime(0.42 * k * mute, now, 0.15); rumbleG.wob.gain.setTargetAtTime(0.06 * k * mute, now, 0.3);
    roarG.gain.setTargetAtTime(0.1 * k * mute, now, 0.15); crackleG.gain.setTargetAtTime(0, now, 0.1);
  }
  function thump(level = 0.5, dur = 0.4, freq = 600) { if (ready) bang(level, dur, freq); }
  /* volumes (0..1) from the settings: sound (everything but the voices), voices (the radio) */
  function setLevels(l) {
    Object.assign(level, l);
    if (out) { out.gain.setTargetAtTime(level.sound, ctx.currentTime, 0.05); voice.gain.setTargetAtTime(level.voices, ctx.currentTime, 0.05); }
  }
  return { init, update, hatch, ambience, drive, thump, setLevels, get ctx() { return ctx; }, get out() { return out; }, get voice() { return voice; }, get ready() { return ready; } };
}

/* controller vibration with the rocket */
export function makeHaptics() {
  let last = 0;
  return function pulse(session, t, scale, now) {
    if (!session || now - last < 0.09) return;
    last = now;
    const k = intensity(t), E = EVENTS;
    let v = Math.min(1, k.shake * 0.85 + k.crackle * 0.15 * Math.random()) * scale;
    if (Math.abs(t - (E.sicCutoff + 0.6)) < 0.25 || Math.abs(t - E.towerJett) < 0.25 || Math.abs(t) < 0.2) v = Math.max(v, scale);
    if (v < 0.02) return;
    for (const src of session.inputSources) {
      const h = src.gamepad && src.gamepad.hapticActuators && src.gamepad.hapticActuators[0];
      if (h && h.pulse) h.pulse(v, 100).catch(() => {});
    }
  };
}
