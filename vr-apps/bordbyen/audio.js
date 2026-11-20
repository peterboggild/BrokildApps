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
    whoosh(up) { if (!ctx) return; const t = ctx.currentTime, s = ctx.createBufferSource(); s.buffer = noise; s.loop = true; const f = ctx.createBiquadFilter(); f.type = 'bandpass'; f.Q.value = 2;
      f.frequency.setValueAtTime(up ? 2400 : 300, t); f.frequency.exponentialRampToValueAtTime(up ? 300 : 2400, t + 1.5); const g = gain(0); s.connect(f); f.connect(g); g.connect(out);
      g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.25, t + 0.4); g.gain.exponentialRampToValueAtTime(0.0001, t + 1.6); s.start(t); s.stop(t + 1.7); },
  };
}
