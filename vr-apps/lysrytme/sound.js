/* Lysrytme's sound: Sleeper Agent's noise and synth themes underneath, and
   a pulse in step with the light on top.

   The noise and the themes come from health-apps/sleep-noise/sound-engine.js,
   the same engine Sleeper Agent plays, rendered in a worker (sound-worker.js)
   so the headset never waits a frame for them.

   The pulse is a tone (or a hiss) whose loudness follows the light's own
   waveform: an oscillator at the flicker frequency, built from the Fourier
   series of that waveform, drives the tone's gain. The audio clock is also
   the app's clock: the light reads its phase from the same frequency
   schedule the oscillator was given, so the two stay together however long
   the session runs. */

const SS = () => window.SleeperSound;
const SPACE_AMT = 0.6;          /* Sleeper Agent's default space setting (80) */
const HARM = 24;                /* harmonics in the pulse's modulation */

export function createSound() {
  let ctx = null, worker = null, nextId = 1;
  const jobs = new Map();
  const cache = new Map();      /* 'noise:rain' / 'pad:dusk:bed' -> AudioBuffer */
  let master, vol, noiseBus, tone, padBus, pulseVol, pulseAmp, dc, lfo, carrier = null, carrierKind = 'off';
  let loops = [];               /* running loop sources: { src, g } */
  let preset = 'off', volume = 0.6, pulseKind = 'off', pulseLevel = 0.5, paused = false;
  let wave = null;              /* the light's waveform, p in [0,1) -> 0..1 */
  let dcLevel = 0.5;
  let started = false;

  /* ---- the clock: phase as a function of audio time ---- */
  /* Until the audio context runs (it needs a click), the clock is
     performance time; once it runs, audio time. Switching restarts the
     schedule, which is one jump in phase at the start of a session. */
  let segs = [{ t0: 0, ph0: 0, f: 10 }], segBase = 'p';
  let curF = 10, lastSet = -1;
  let off = null;               /* audio time minus performance time, smoothed */
  const base = () => (ctx && ctx.state === 'running' ? 'a' : 'p');
  function rebase(t0) { segBase = base(); segs = [{ t0, ph0: 0, f: curF }]; }

  function phaseAt(t) {
    if (base() !== segBase) rebase(now());
    let s = segs[0];
    for (let i = segs.length - 1; i >= 0; i--) { if (segs[i].t0 <= t) { s = segs[i]; break; } }
    return s.ph0 + s.f * (t - s.t0);
  }
  function now() { return base() === 'a' ? ctx.currentTime : performance.now() / 1000; }
  function setFreq(f, force) {
    if (!(f > 0)) return;
    if (base() !== segBase) rebase(now());
    const t = now();
    if (!force && Math.abs(f - curF) < curF * 0.0015 && t - lastSet < 1) return;
    if (!force && Math.abs(f - curF) < 1e-6) return;
    const tc = t + (segBase === 'a' ? 0.03 : 0);
    segs.push({ t0: tc, ph0: phaseAt(tc), f });
    if (segs.length > 64) segs.splice(0, segs.length - 64);
    curF = f; lastSet = t;
    if (lfo && segBase === 'a') lfo.frequency.setValueAtTime(f, tc);
  }
  /* Map a performance.now() time (a frame's display time) to audio time. The
     output timestamp accounts for the output latency, so the light is lined
     up with the sound as it leaves the speaker, not as it is computed. */
  function audioTimeOf(perfMs) {
    if (base() === 'p') return perfMs / 1000;
    let raw = null;
    try {
      const ts = ctx.getOutputTimestamp && ctx.getOutputTimestamp();
      if (ts && ts.performanceTime > 0) raw = ts.contextTime - ts.performanceTime / 1000;
    } catch (_) { /* older browsers */ }
    if (raw === null) raw = ctx.currentTime - performance.now() / 1000;
    off = off === null ? raw : off + (raw - off) * 0.02;
    return perfMs / 1000 + off;
  }

  /* ---- the worker ---- */
  function ensureWorker() {
    if (worker) return;
    worker = new Worker(new URL('./sound-worker.js?v=1', import.meta.url));
    worker.onmessage = (e) => {
      const j = jobs.get(e.data.id); if (!j) return;
      jobs.delete(e.data.id);
      if (e.data.error) j.rej(new Error(e.data.error)); else j.res(e.data.ch);
    };
  }
  function render(msg) {
    ensureWorker();
    return new Promise((res, rej) => {
      const id = nextId++;
      jobs.set(id, { res, rej });
      worker.postMessage(Object.assign({ id, sr: ctx.sampleRate, amt: SPACE_AMT }, msg));
    });
  }
  function toBuffer(ch) {
    const b = ctx.createBuffer(2, ch[0].length, ctx.sampleRate);
    b.copyToChannel(ch[0], 0); b.copyToChannel(ch[1], 1);
    return b;
  }

  function ensureCtx() {
    if (ctx) return;
    const AC = window.AudioContext || window.webkitAudioContext;
    ctx = new AC({ latencyHint: 'playback' });
    master = ctx.createGain(); master.gain.value = 0;
    vol = ctx.createGain(); vol.gain.value = volume * volume;
    master.connect(vol); vol.connect(ctx.destination);
    noiseBus = ctx.createGain();
    tone = ctx.createBiquadFilter(); tone.type = 'lowpass'; tone.Q.value = 0.5;
    noiseBus.connect(tone); tone.connect(master);
    padBus = ctx.createGain(); padBus.connect(master);
    /* the pulse: carrier -> pulseAmp (gain = dc + lfo) -> pulseVol -> master */
    pulseAmp = ctx.createGain(); pulseAmp.gain.value = 0;
    pulseVol = ctx.createGain(); pulseVol.gain.value = 0;
    pulseAmp.connect(pulseVol); pulseVol.connect(master);
    dc = ctx.createConstantSource(); dc.offset.value = dcLevel; dc.connect(pulseAmp.gain); dc.start();
  }
  /* A fresh modulation oscillator once the context runs; the schedule
     starts with it, at phase zero. */
  function newLfo() {
    if (lfo) { try { lfo.stop(); } catch (_) {} lfo.disconnect(); }
    lfo = ctx.createOscillator(); lfo.frequency.value = curF;
    applyWave();
    lfo.connect(pulseAmp.gain);
    const t0 = ctx.currentTime + 0.05;
    lfo.start(t0);
    segBase = 'a'; segs = [{ t0, ph0: 0, f: curF }];
  }

  /* The light's waveform as a periodic wave (its DC part goes to the constant
     source), with Lanczos smoothing so a hard edge does not ring. */
  function applyWave() {
    if (!ctx || !wave || !lfo) return;
    const N = 2048, re = new Float32Array(HARM + 1), im = new Float32Array(HARM + 1);
    let mean = 0;
    for (let i = 0; i < N; i++) mean += wave((i + 0.5) / N);
    mean /= N;
    for (let k = 1; k <= HARM; k++) {
      let a = 0, b = 0;
      for (let i = 0; i < N; i++) {
        const p = (i + 0.5) / N, v = wave(p), w = 2 * Math.PI * k * p;
        a += v * Math.cos(w); b += v * Math.sin(w);
      }
      const x = Math.PI * k / (HARM + 1), sig = Math.sin(x) / x;
      re[k] = 2 * a / N * sig; im[k] = 2 * b / N * sig;
    }
    lfo.setPeriodicWave(ctx.createPeriodicWave(re, im, { disableNormalization: true }));
    dcLevel = mean;
    dc.offset.setTargetAtTime(mean, ctx.currentTime, 0.01);
  }

  function makeCarrier(kind) {
    if (carrier) { try { carrier.stop(); } catch (_) {} carrier.disconnect(); carrier = null; }
    carrierKind = kind;
    if (kind === 'tone') {
      carrier = ctx.createOscillator(); carrier.frequency.value = 196; /* G3: low and round */
      const g = ctx.createGain(); g.gain.value = 0.32; carrier.connect(g); g.connect(pulseAmp);
      carrier.start();
    } else if (kind === 'sus') {
      const n = ctx.sampleRate * 2, b = ctx.createBuffer(1, n, ctx.sampleRate), d = b.getChannelData(0);
      for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1;
      carrier = ctx.createBufferSource(); carrier.buffer = b; carrier.loop = true;
      const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 1400; bp.Q.value = 0.8;
      const g = ctx.createGain(); g.gain.value = 0.55;
      carrier.connect(bp); bp.connect(g); g.connect(pulseAmp);
      carrier.start();
    }
  }
  function pulseGain() { return paused || pulseKind === 'off' ? 0 : pulseLevel * pulseLevel; }
  function applyPulse() {
    if (!ctx) return;
    if (pulseKind !== carrierKind) makeCarrier(pulseKind);
    pulseVol.gain.setTargetAtTime(pulseGain(), ctx.currentTime, 0.08);
  }

  /* ---- Sleeper Agent's presets ---- */
  function presetOf(id) { return (SS().PRESETS || []).find((p) => p.id === id) || null; }
  function keysOf(id) {
    const p = presetOf(id); if (!p) return [];
    const k = Object.keys(p.mix).filter((l) => p.mix[l] > 0).map((l) => ({ key: 'noise:' + l, kind: 'noise', name: l }));
    if (p.pad && p.pad !== 'off') {
      k.push({ key: 'pad:' + p.pad + ':bed', kind: 'pad', name: p.pad, layer: 'bed' });
      k.push({ key: 'pad:' + p.pad + ':mel', kind: 'pad', name: p.pad, layer: 'mel' });
    }
    return k;
  }
  /* Render whatever the preset needs and is not cached yet. */
  async function prepare(id, onProgress) {
    if (id === 'off' || !presetOf(id)) { onProgress && onProgress(1, 1); return; }
    ensureCtx();
    const need = keysOf(id), todo = need.filter((k) => !cache.has(k.key));
    let done = need.length - todo.length;
    onProgress && onProgress(done, need.length);
    for (const k of todo) {
      const ch = await render(k);
      cache.set(k.key, toBuffer(ch));
      onProgress && onProgress(++done, need.length);
    }
  }
  function stopLoops(fade) {
    const t = ctx.currentTime;
    for (const L of loops) {
      L.g.gain.cancelScheduledValues(t); L.g.gain.setValueAtTime(L.g.gain.value, t);
      L.g.gain.linearRampToValueAtTime(0, t + fade);
      try { L.src.stop(t + fade + 0.05); } catch (_) {}
    }
    loops = [];
  }
  function startLoops(fade) {
    const p = presetOf(preset); if (!p) return;
    const t = ctx.currentTime;
    tone.frequency.value = 200 * Math.pow(100, p.tone / 100);
    padBus.gain.value = Math.pow(p.padLevel / 100, 2);
    for (const k of keysOf(preset)) {
      const buf = cache.get(k.key); if (!buf) continue;
      const src = ctx.createBufferSource(); src.buffer = buf; src.loop = true;
      const g = ctx.createGain(); g.gain.value = 0;
      const lvl = k.kind === 'noise' ? Math.pow(p.mix[k.name] / 100, 1.3) * 1.25 * (k.name === 'ocean' ? 1.5 : 1) : 1;
      src.connect(g); g.connect(k.kind === 'noise' ? noiseBus : padBus);
      g.gain.linearRampToValueAtTime(lvl, t + fade);
      src.start(t, Math.random() * buf.duration);
      loops.push({ src, g });
    }
  }

  return {
    get ctx() { return ctx; },
    get started() { return started; },
    phaseAt, audioTimeOf, setFreq, now,
    get freq() { return curF; },
    prepare,
    presets() { return SS().PRESETS; },
    /* Must be called from a click (browsers only start sound on a gesture). */
    async start() {
      ensureCtx();
      try { await ctx.resume(); } catch (_) {}
      started = true;
      if (ctx.state === 'running') newLfo();
      master.gain.cancelScheduledValues(ctx.currentTime);
      master.gain.setValueAtTime(master.gain.value, ctx.currentTime);
      master.gain.linearRampToValueAtTime(1, ctx.currentTime + 4);
      startLoops(4);
      applyPulse();
    },
    async setPreset(id, onProgress) {
      preset = id;
      await prepare(id, onProgress);
      if (started && preset === id) { stopLoops(2); startLoops(3); }
    },
    setVolume(v) { volume = v; if (ctx) vol.gain.setTargetAtTime(v * v, ctx.currentTime, 0.05); },
    setPulse(kind, level) { pulseKind = kind; if (level !== undefined) pulseLevel = level; applyPulse(); },
    setWave(fn) { wave = fn; applyWave(); },
    setPaused(p) { paused = p; applyPulse(); },
    /* The program's envelope on everything (0..1). */
    setLevel(v, sec = 0.5) { if (ctx && started) master.gain.setTargetAtTime(v, ctx.currentTime, sec / 3); },
    stop(fade = 1.5) {
      if (!ctx || !started) return;
      started = false;
      const t = ctx.currentTime;
      master.gain.cancelScheduledValues(t); master.gain.setValueAtTime(master.gain.value, t);
      master.gain.linearRampToValueAtTime(0, t + fade);
      stopLoops(fade);
    },
  };
}
