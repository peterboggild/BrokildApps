/*
 * Bordbyen — sound, all synthesised: engines (yours in your ears, the opponents' placed where their cars are), bumps, the countdown, the finish,
 * and small clicks and pops while you build. Nothing is loaded.
 *
 * An engine is two detuned sawtooths and a square an octave down through a low-pass filter; its pitch follows the speed through four "gears", so it
 * climbs, drops and climbs again. The opponents' engines are quieter and panned with the cheap equal-power model (HRTF costs on a Quest).
 */
export function createAudio() {
  let ctx = null, out = null, noise = null; const engines = [];
  function init() {
    if (ctx) { if (ctx.state === 'suspended') ctx.resume(); return; }
    try { ctx = new (window.AudioContext || window.webkitAudioContext)(); } catch (_) { return; }
    out = ctx.createGain(); out.gain.value = 0.8; out.connect(ctx.destination);
    noise = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate); const d = noise.getChannelData(0); for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
  }
  const gain = v => { const g = ctx.createGain(); g.gain.value = v; return g; };
  function engine(spatial, level) {
    if (!ctx) return null;
    const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 600; f.Q.value = 3;
    const g = gain(0), oscs = [['sawtooth', 1, 0], ['sawtooth', 1, 9], ['square', 0.5, 0]].map(([t, m, det]) => { const o = ctx.createOscillator(); o.type = t; o.detune.value = det; o.connect(f); o.start(); return { o, m }; });
    let dst = out;
    if (spatial) { const p = ctx.createPanner(); p.panningModel = 'equalpower'; p.distanceModel = 'inverse'; p.refDistance = 4; p.rolloffFactor = 1.4; p.connect(out); dst = p; f.connect(g); g.connect(p); return { oscs, f, g, p, level }; }
    f.connect(g); g.connect(dst); return { oscs, f, g, p: null, level };
  }
  /** speed (m/s) to engine pitch: four gears */
  function rpm(v) { const s = Math.abs(v), gears = [0, 7, 14, 21, 99]; let k = 0; while (s > gears[k + 1]) k++; const u = (s - gears[k]) / (gears[k + 1] - gears[k] || 1); return 0.25 + Math.min(1, u) * 0.75 + k * 0.08; }
  /* ---- the noir music: a scheduler that writes a little ahead of the clock ---- */
  const N = { on: false };
  const hz = m => 440 * Math.pow(2, (m - 69) / 12);
  const CH = [ { root: 38, pad: [53, 57, 60, 64], scale: [62, 64, 65, 67, 69, 72, 74, 76] },           // Dm9
               { root: 46, pad: [50, 53, 57, 60], scale: [62, 65, 67, 69, 70, 72, 74, 77] },           // Bbmaj7
               { root: 43, pad: [50, 53, 57, 58], scale: [62, 65, 67, 69, 70, 74, 77] },               // Gm9
               { root: 45, pad: [49, 52, 55, 58], scale: [61, 64, 67, 69, 70, 73, 76] } ];             // A7b9
  const BEAT = 60 / 68;
  function note(dst, type, f, t, a, d, level, vib = 0) {
    const o = ctx.createOscillator(), g = gain(0); o.type = type; o.frequency.setValueAtTime(f, t); o.connect(g); g.connect(dst);
    if (vib) { const l = ctx.createOscillator(), lg = gain(f * vib); l.frequency.value = 5.2; l.connect(lg); lg.connect(o.frequency); l.start(t); l.stop(t + a + d + 0.1); }
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(level, t + a); g.gain.exponentialRampToValueAtTime(0.0001, t + a + d); o.start(t); o.stop(t + a + d + 0.1);
  }
  function brush(t, level, len) { const s = ctx.createBufferSource(); s.buffer = noise; const f = ctx.createBiquadFilter(); f.type = 'highpass'; f.frequency.value = 3200; const g = gain(0);
    s.connect(f); f.connect(g); g.connect(N.music); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(level, t + len * 0.3); g.gain.exponentialRampToValueAtTime(0.0001, t + len); s.start(t, Math.random()); s.stop(t + len + 0.05); }
  function noirTick() {
    if (!N.on) return;
    while (N.next < ctx.currentTime + 0.35) {
      const t = N.next, b = N.beat, bar = Math.floor(b / 4), inBar = b % 4, ch = CH[Math.floor(bar / 2) % 4], nextCh = CH[Math.floor((bar + 1) / 2) % 4];
      /* the pad: a new chord every two bars, soft saws through a low filter */
      if (inBar === 0 && bar % 2 === 0) { const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 650; f.Q.value = 0.7; const pg = gain(1); f.connect(pg); pg.connect(N.music);
        for (const m of ch.pad) for (const det of [-7, 6]) { const o = ctx.createOscillator(), g = gain(0); o.type = 'sawtooth'; o.frequency.value = hz(m); o.detune.value = det; o.connect(g); g.connect(f);
          g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.022, t + 1.6); g.gain.setValueAtTime(0.022, t + BEAT * 8 - 1.2); g.gain.exponentialRampToValueAtTime(0.0001, t + BEAT * 8 + 0.6); o.start(t); o.stop(t + BEAT * 8 + 0.7); } }
      /* the walking bass: root, a chord tone, the fifth, and a step towards the next root */
      const walk = [ch.root, ch.root + (Math.random() < 0.5 ? 3 : 4), ch.root + 7, (bar % 2 === 1) ? nextCh.root + (Math.random() < 0.5 ? -1 : 1) : ch.root + 10];
      note(N.music, 'triangle', hz(walk[inBar]), t, 0.012, BEAT * 0.95, 0.22); note(N.music, 'sine', hz(walk[inBar] - 12), t, 0.01, BEAT * 0.8, 0.18);
      /* brushes on two and four, a swung ride */
      if (inBar % 2 === 1) brush(t, 0.05, 0.22);
      brush(t, 0.018, 0.08); brush(t + BEAT * 0.66, 0.012, 0.06);
      /* a vibraphone note now and then, into the echo */
      if (Math.random() < 0.3) { const m = ch.scale[Math.floor(Math.random() * ch.scale.length)] + (Math.random() < 0.3 ? 12 : 0), when = t + (Math.random() < 0.5 ? 0 : BEAT * 0.66);
        note(N.music, 'sine', hz(m), when, 0.005, 2.4, 0.06, 0.004); note(N.dl, 'sine', hz(m), when, 0.005, 2.4, 0.035, 0.004); note(N.music, 'sine', hz(m) * 4.0, when, 0.003, 0.4, 0.008); }
      N.next += BEAT; N.beat++;
    }
  }
  return {
    init, get ctx() { return ctx; },
    /** engines: [{ v, gas, pos (city), mine }] — one per car, set every frame */
    engines(list, listener) {
      if (!ctx) return;
      while (engines.length < list.length) engines.push(engine(engines.length > 0, engines.length ? 0.35 : 0.5));
      const t = ctx.currentTime;
      list.forEach((c, i) => { const e = engines[i]; if (!e) return; const r = rpm(c.v), base = 38 + r * 70;
        for (const { o, m } of e.oscs) o.frequency.setTargetAtTime(base * m, t, 0.05);
        e.f.frequency.setTargetAtTime(300 + r * 900 + (c.gas ? 700 : 0), t, 0.08);
        e.g.gain.setTargetAtTime(c.off ? 0 : e.level * (0.35 + 0.4 * (c.gas ? 1 : 0) + 0.25 * Math.min(1, Math.abs(c.v) / 20)) * 0.25, t, 0.1);
        if (e.p) { const p = c.pos; if (e.p.positionX) { e.p.positionX.setTargetAtTime(p.x, t, 0.05); e.p.positionY.setTargetAtTime(p.y, t, 0.05); e.p.positionZ.setTargetAtTime(p.z, t, 0.05); } else e.p.setPosition(p.x, p.y, p.z); } });
      for (let i = list.length; i < engines.length; i++) if (engines[i]) engines[i].g.gain.setTargetAtTime(0, t, 0.1);
      if (listener) { const L = ctx.listener, { p, f, u } = listener;
        if (L.positionX) { L.positionX.value = p.x; L.positionY.value = p.y; L.positionZ.value = p.z; L.forwardX.value = f.x; L.forwardY.value = f.y; L.forwardZ.value = f.z; L.upX.value = u.x; L.upY.value = u.y; L.upZ.value = u.z; }
        else { L.setPosition(p.x, p.y, p.z); L.setOrientation(f.x, f.y, f.z, u.x, u.y, u.z); } }
    },
    silence() { if (!ctx) return; for (const e of engines) if (e) e.g.gain.setTargetAtTime(0, ctx.currentTime, 0.1); },
    beep(high) { if (!ctx) return; const t = ctx.currentTime, o = ctx.createOscillator(), g = gain(0); o.type = 'square'; o.frequency.value = high ? 1046 : 523; o.connect(g); g.connect(out);
      g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.18, t + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + (high ? 0.6 : 0.25)); o.start(t); o.stop(t + 0.7); },
    bump(k) { if (!ctx) return; const t = ctx.currentTime, s = ctx.createBufferSource(); s.buffer = noise; const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 500 + k * 60; const g = gain(0);
      s.connect(f); f.connect(g); g.connect(out); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(Math.min(0.9, 0.15 + k * 0.05), t + 0.005); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.3);
      s.start(t); s.stop(t + 0.35);
      const o = ctx.createOscillator(), g2 = gain(0); o.frequency.setValueAtTime(110, t); o.frequency.exponentialRampToValueAtTime(45, t + 0.2); o.connect(g2); g2.connect(out);
      g2.gain.setValueAtTime(0.0001, t); g2.gain.exponentialRampToValueAtTime(Math.min(0.6, 0.1 + k * 0.04), t + 0.005); g2.gain.exponentialRampToValueAtTime(0.0001, t + 0.25); o.start(t); o.stop(t + 0.3); },
    chime(n = 3, base = 660) { if (!ctx) return; const t = ctx.currentTime; for (let k = 0; k < n; k++) { const o = ctx.createOscillator(), g = gain(0); o.type = 'triangle'; o.frequency.value = base * Math.pow(1.26, k); o.connect(g); g.connect(out);
      const s = t + k * 0.11; g.gain.setValueAtTime(0.0001, s); g.gain.exponentialRampToValueAtTime(0.2, s + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, s + 0.5); o.start(s); o.stop(s + 0.55); } },
    fanfare() { if (!ctx) return; const t = ctx.currentTime, notes = [523, 659, 784, 1046, 784, 1046]; notes.forEach((f, k) => { const o = ctx.createOscillator(), g = gain(0); o.type = 'square'; o.frequency.value = f; o.connect(g); g.connect(out);
      const s = t + k * 0.14, d = k === notes.length - 1 ? 0.8 : 0.16; g.gain.setValueAtTime(0.0001, s); g.gain.exponentialRampToValueAtTime(0.12, s + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, s + d); o.start(s); o.stop(s + d + 0.05); }); },
    click() { if (!ctx) return; const t = ctx.currentTime, o = ctx.createOscillator(), g = gain(0); o.frequency.value = 1800; o.connect(g); g.connect(out);
      g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.08, t + 0.002); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.04); o.start(t); o.stop(t + 0.05); },
    pop() { if (!ctx) return; const t = ctx.currentTime, o = ctx.createOscillator(), g = gain(0); o.type = 'sine'; o.frequency.setValueAtTime(300, t); o.frequency.exponentialRampToValueAtTime(900, t + 0.08); o.connect(g); g.connect(out);
      g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.2, t + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.18); o.start(t); o.stop(t + 0.2); },
    /** The graphic-novel night: rain on everything, and a slow noir jazz that writes itself: dark chords on a soft pad (Dm9, Bbmaj7, Gm9, A7b9,
     *  two bars each), a walking upright bass, brushes on two and four with a swung ride, and now and then a vibraphone note into a long echo. */
    noir(on) {
      if (!ctx) return;
      if (on && !N.on) {
        N.on = true; N.out = gain(0); N.out.connect(out); N.out.gain.setTargetAtTime(0.55, ctx.currentTime, 1.5);
        const rs = ctx.createBufferSource(); rs.buffer = noise; rs.loop = true; const rf = ctx.createBiquadFilter(); rf.type = 'bandpass'; rf.frequency.value = 1400; rf.Q.value = 0.35;
        const rl = ctx.createBiquadFilter(); rl.type = 'lowpass'; rl.frequency.value = 5200; N.rain = gain(0.16); rs.connect(rf); rf.connect(rl); rl.connect(N.rain); N.rain.connect(N.out); rs.start(); N.rainSrc = rs;
        N.dl = ctx.createDelay(1.5); N.dl.delayTime.value = 0.43; const fb = gain(0.38), dlf = ctx.createBiquadFilter(); dlf.type = 'lowpass'; dlf.frequency.value = 1800;
        N.dl.connect(dlf); dlf.connect(fb); fb.connect(N.dl); dlf.connect(N.out); N.music = gain(0.9); N.music.connect(N.out);
        N.beat = 0; N.next = ctx.currentTime + 0.3; N.timer = setInterval(noirTick, 90);
      } else if (!on && N.on) {
        N.on = false; clearInterval(N.timer); const o = N.out; o.gain.setTargetAtTime(0, ctx.currentTime, 0.6); const src = N.rainSrc; setTimeout(() => { try { src.stop(); o.disconnect(); } catch (_) { /* gone */ } }, 3000);
      }
    },
    thunder(dist = 1) { if (!ctx) return; const t = ctx.currentTime + 0.3 + dist * 1.5, s = ctx.createBufferSource(); s.buffer = noise; s.loop = true; const f = ctx.createBiquadFilter(); f.type = 'lowpass';
      f.frequency.setValueAtTime(900 / (1 + dist), t); f.frequency.exponentialRampToValueAtTime(70, t + 3.5); const g = gain(0); s.connect(f); f.connect(g); g.connect(out);
      g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.6 / (1 + dist * 0.6), t + 0.08); g.gain.exponentialRampToValueAtTime(0.25, t + 0.9); g.gain.exponentialRampToValueAtTime(0.0001, t + 4.5); s.start(t); s.stop(t + 4.7); },
    whoosh(up) { if (!ctx) return; const t = ctx.currentTime, s = ctx.createBufferSource(); s.buffer = noise; s.loop = true; const f = ctx.createBiquadFilter(); f.type = 'bandpass'; f.Q.value = 2;
      f.frequency.setValueAtTime(up ? 2400 : 300, t); f.frequency.exponentialRampToValueAtTime(up ? 300 : 2400, t + 1.5); const g = gain(0); s.connect(f); f.connect(g); g.connect(out);
      g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.25, t + 0.4); g.gain.exponentialRampToValueAtTime(0.0001, t + 1.6); s.start(t); s.stop(t + 1.7); },
  };
}
