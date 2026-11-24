/* What you hear inside the suit. No sound reaches you through vacuum: only what travels through your body and
 * the suit, and the suit's own machinery.
 *
 * - Steps: a dull thud through the boots and legs, and the dry scrape of the overshoe on the regolith, which
 *   is granular, crackly and muffled by the helmet.
 * - The suit fan: the life-support pack's fan and oxygen flow, a soft constant hiss and hum.
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

  function setFan(on) {
    fanOn = on;
    if (fanGain) fanGain.gain.setTargetAtTime(on ? 0.05 : 0, ctx.currentTime, 0.3);
  }

  return { init, step, setFan, get running() { return !!ctx && ctx.state === 'running'; } };
}
