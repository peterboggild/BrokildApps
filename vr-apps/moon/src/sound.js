/* What you hear inside the suit. No sound reaches you through vacuum: only what travels through your body and
 * the suit, and the suit's own machinery.
 *
 * - Steps: a dull thud through the boots and legs, and the dry scrape of the overshoe on the regolith, which
 *   is granular, crackly and muffled by the helmet.
 * - The suit fan: the life-support pack's fan and oxygen flow, a soft constant hiss and hum.
 * - The rover, through the seat and the hand on its controller: the four hub motors' whirr rising with the speed
 *   and the load, the chassis' low rumble over the ground, and a thump with a rattle at every bump (drive, bump).
 *
 * All synthesised (Web Audio); no recordings. The AudioContext is made on a click (browsers require it). */

export function makeSuitSound() {
  let ctx = null, out = null, fanGain = null, grains = [], fanOn = true;

  function init() {
    if (ctx) { if (ctx.state === 'suspended') ctx.resume(); return; }
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    ctx = new AC();
    /* the helmet: no air outside, so nothing above a few kHz; a slight boxy resonance */
    const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 3200; lp.Q.value = 0.4;
    const box = ctx.createBiquadFilter(); box.type = 'peaking'; box.frequency.value = 520; box.Q.value = 1.1; box.gain.value = 4;
    out = ctx.createGain(); out.gain.value = 0.9;
    out.connect(box).connect(lp).connect(ctx.destination);
    /* a few variants of the scrape, so no two steps sound the same */
    for (let v = 0; v < 6; v++) grains.push(scrapeBuffer(v));
    startFan();
  }

  /* dry granular scrape: noise in short gritty bursts under an envelope that drags on a little */
  function scrapeBuffer(v) {
    const sr = ctx.sampleRate, n = Math.floor(sr * 0.34), b = ctx.createBuffer(1, n, sr), d = b.getChannelData(0);
    let seed = 977 + v * 131;
    const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
    const drag = 0.10 + 0.08 * rnd(), slide = 0.045 + 0.04 * rnd();
    let hp = 0, prev = 0, click = 0;
    for (let i = 0; i < n; i++) {
      const t = i / sr;
      /* heel strike, then the sole sliding and settling */
      const env = Math.min(1, t / 0.008) * (0.55 * Math.exp(-t / slide) + 0.45 * Math.exp(-Math.max(0, t - 0.02) / drag));
      if (rnd() < 0.012) click = 0.6 + rnd();              /* grains cracking */
      click *= 0.86;
      const w = rnd() * 2 - 1;
      hp = 0.82 * (hp + w - prev); prev = w;              /* one-pole high-pass: dry, not boomy */
      d[i] = hp * env * (0.28 + click);
    }
    return b;
  }

  function startFan() {
    const sr = ctx.sampleRate, n = sr * 2, b = ctx.createBuffer(1, n, sr), d = b.getChannelData(0);
    let br = 0;
    for (let i = 0; i < n; i++) { br = 0.985 * br + 0.015 * (Math.random() * 2 - 1); d[i] = br * 6; }
    const src = ctx.createBufferSource(); src.buffer = b; src.loop = true;
    const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 420; bp.Q.value = 0.5;
    const hum = ctx.createOscillator(); hum.frequency.value = 117; const hg = ctx.createGain(); hg.gain.value = 0.05;
    fanGain = ctx.createGain(); fanGain.gain.value = fanOn ? 0.05 : 0;
    src.connect(bp).connect(fanGain); hum.connect(hg).connect(fanGain); fanGain.connect(out);
    src.start(); hum.start();
  }

  function step(strength = 1) {
    if (!ctx || ctx.state !== 'running') return;
    const t = ctx.currentTime + 0.005, k = strength * (0.8 + 0.4 * Math.random());
    /* thud through the body */
    const o = ctx.createOscillator(), og = ctx.createGain();
    o.type = 'sine'; o.frequency.setValueAtTime(92 + 20 * Math.random(), t); o.frequency.exponentialRampToValueAtTime(46, t + 0.11);
    og.gain.setValueAtTime(0.0001, t); og.gain.exponentialRampToValueAtTime(0.55 * k, t + 0.006); og.gain.exponentialRampToValueAtTime(0.0001, t + 0.17);
    o.connect(og).connect(out); o.start(t); o.stop(t + 0.2);
    /* the scrape, heard through the boot and suit: band-limited, a little late after the strike */
    const s = ctx.createBufferSource(); s.buffer = grains[(Math.random() * grains.length) | 0];
    s.playbackRate.value = 0.85 + 0.3 * Math.random();
    const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 900 + 700 * Math.random(); bp.Q.value = 0.7;
    const sg = ctx.createGain(); sg.gain.value = 0.85 * k;
    s.connect(bp).connect(sg).connect(out); s.start(t + 0.012);
  }

  /* The rover under you: speed in m/s, the hand controller's throw -1..1 (0, 0 when you are not on it) */
  let drv = null;
  function drive(speed, thr) {
    if (!ctx || ctx.state !== 'running') return;
    const s = Math.min(1, Math.abs(speed) / 3.6), load = Math.min(1, Math.abs(thr));
    if (!drv) {
      if (s < 0.005 && load < 0.02) return;
      /* the motors: two saws a little apart, well muffled (it comes through structure, not air) */
      const o1 = ctx.createOscillator(), o2 = ctx.createOscillator(); o1.type = o2.type = 'sawtooth';
      const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 420; lp.Q.value = 0.7;
      const g = ctx.createGain(); g.gain.value = 0;
      o1.connect(lp); o2.connect(lp); lp.connect(g).connect(out); o1.start(); o2.start();
      /* the rumble: slow noise, low */
      const sr = ctx.sampleRate, n = sr * 2, b = ctx.createBuffer(1, n, sr), d = b.getChannelData(0); let br = 0;
      for (let i = 0; i < n; i++) { br = 0.97 * br + 0.03 * (Math.random() * 2 - 1); d[i] = br * 5; }
      const src = ctx.createBufferSource(); src.buffer = b; src.loop = true;
      const bp = ctx.createBiquadFilter(); bp.type = 'lowpass'; bp.frequency.value = 140;
      const rg = ctx.createGain(); rg.gain.value = 0; src.connect(bp).connect(rg).connect(out); src.start();
      drv = { o1, o2, lp, g, rg, bp };
    }
    const t = ctx.currentTime, f = 46 + 150 * s;
    drv.o1.frequency.setTargetAtTime(f, t, 0.1); drv.o2.frequency.setTargetAtTime(f * 2.02 + 2, t, 0.1);
    drv.lp.frequency.setTargetAtTime(300 + 500 * s, t, 0.2);
    drv.g.gain.setTargetAtTime(s < 0.005 && load < 0.02 ? 0 : 0.012 + 0.028 * load + 0.022 * s, t, 0.12);
    drv.rg.gain.setTargetAtTime(0.5 * s, t, 0.15); drv.bp.frequency.setTargetAtTime(90 + 110 * s, t, 0.2);
    api.driveLevel = s < 0.005 && load < 0.02 ? 0 : 0.012 + 0.028 * load + 0.022 * s;
  }
  /* a wheel takes a bump: the thump through the seat, and the chassis and its load rattling after it */
  function bump(strength = 1) {
    if (!ctx || ctx.state !== 'running') return;
    const t = ctx.currentTime + 0.005, k = Math.min(1.6, strength) * (0.8 + 0.4 * Math.random());
    const o = ctx.createOscillator(), og = ctx.createGain();
    o.type = 'sine'; o.frequency.setValueAtTime(70 + 16 * Math.random(), t); o.frequency.exponentialRampToValueAtTime(34, t + 0.16);
    og.gain.setValueAtTime(0.0001, t); og.gain.exponentialRampToValueAtTime(0.5 * k, t + 0.008); og.gain.exponentialRampToValueAtTime(0.0001, t + 0.26);
    o.connect(og).connect(out); o.start(t); o.stop(t + 0.3);
    const s = ctx.createBufferSource(); s.buffer = grains[(Math.random() * grains.length) | 0]; s.playbackRate.value = 0.5 + 0.2 * Math.random();
    const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 260 + 180 * Math.random(); bp.Q.value = 2.5;
    const sg = ctx.createGain(); sg.gain.value = 0.35 * k;
    s.connect(bp).connect(sg).connect(out); s.start(t + 0.02);
    api.bumps++;
  }

  function setFan(on) {
    fanOn = on;
    if (fanGain) fanGain.gain.setTargetAtTime(on ? 0.05 : 0, ctx.currentTime, 0.3);
  }

  const api = { init, step, setFan, drive, bump, bumps: 0, driveLevel: 0, get running() { return !!ctx && ctx.state === 'running'; } };
  return api;
}
