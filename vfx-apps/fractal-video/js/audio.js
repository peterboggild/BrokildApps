/* Audio: IndexedDB file store, offline analysis (once per file, cached), tempo suggestion,
   stem alignment and a lightweight transport (plain <audio> elements, no WebAudio graph).

   Analysis runs on the decoded file once, in slices so the UI stays responsive, and the decoded
   PCM is dropped afterwards. Only compact feature tracks at RATE Hz are kept. */
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});

  const RATE = 100;            // feature tracks per second
  const DECODE_RATE = 22050;   // analysis sample rate (halves memory vs 44.1k)
  const FEATURE_VERSION = 5;   // 5 added the spectral frames structure.js needs

  // ---------- IndexedDB ----------
  let dbPromise = null;
  function openDb() {
    if (!dbPromise) {
      dbPromise = new Promise((resolve, reject) => {
        if (!('indexedDB' in window)) { reject(new Error('IndexedDB unavailable')); return; }
        const req = indexedDB.open('fractal-video', 1);
        req.onupgradeneeded = () => {
          const d = req.result;
          if (!d.objectStoreNames.contains('files')) d.createObjectStore('files');
          if (!d.objectStoreNames.contains('features')) d.createObjectStore('features');
        };
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => reject(req.error);
      }).catch((err) => { dbPromise = null; throw err; });
    }
    return dbPromise;
  }

  async function tx(store, mode, fn) {
    const d = await openDb();
    return new Promise((resolve, reject) => {
      const t = d.transaction(store, mode);
      const req = fn(t.objectStore(store));
      t.oncomplete = () => resolve(req ? req.result : undefined);
      t.onerror = () => reject(t.error);
      t.onabort = () => reject(t.error || new Error('IndexedDB transaction aborted'));
    });
  }

  const idb = {
    put: (store, key, value) => tx(store, 'readwrite', (s) => s.put(value, key)),
    get: (store, key) => tx(store, 'readonly', (s) => s.get(key)),
    del: (store, key) => tx(store, 'readwrite', (s) => s.delete(key)),
    keys: (store) => tx(store, 'readonly', (s) => s.getAllKeys())
  };

  const fileKey = (file) => `${file.name}|${file.size}|${file.lastModified || 0}`;
  const tick = () => new Promise((r) => setTimeout(r, 0));

  // ---------- decoding ----------
  async function decode(blob) {
    const data = await blob.arrayBuffer();
    const ctx = new OfflineAudioContext(1, 1, DECODE_RATE);
    return ctx.decodeAudioData(data);
  }

  // ---------- analysis ----------
  function biquad(type, f0, fs, q) {
    const w0 = 2 * Math.PI * f0 / fs;
    const cos = Math.cos(w0);
    const alpha = Math.sin(w0) / (2 * (q || Math.SQRT1_2));
    const a0 = 1 + alpha;
    let b0, b1, b2;
    if (type === 'lp') { b0 = (1 - cos) / 2; b1 = 1 - cos; b2 = (1 - cos) / 2; }
    else { b0 = (1 + cos) / 2; b1 = -(1 + cos); b2 = (1 + cos) / 2; }
    return { b0: b0 / a0, b1: b1 / a0, b2: b2 / a0, a1: (-2 * cos) / a0, a2: (1 - alpha) / a0, x1: 0, x2: 0, y1: 0, y2: 0 };
  }

  function run(f, x) {
    const y = f.b0 * x + f.b1 * f.x1 + f.b2 * f.x2 - f.a1 * f.y1 - f.a2 * f.y2;
    f.x2 = f.x1; f.x1 = x; f.y2 = f.y1; f.y1 = y;
    return y;
  }

  function percentile(arr, q) {
    const copy = Array.from(arr).filter((v) => v > 1e-6).sort((a, b) => a - b);
    if (!copy.length) return 1;
    return copy[Math.min(copy.length - 1, Math.floor(q * copy.length))] || 1;
  }

  function normalize(arr) {
    const ref = percentile(arr, 0.98);
    for (let i = 0; i < arr.length; i += 1) arr[i] = Math.min(1, arr[i] / ref);
    return arr;
  }

  // Onset strength (log-energy rise) -> discrete onsets -> decaying pulse track.
  function onsetsFrom(rms) {
    const n = rms.length;
    const e = new Float32Array(n);
    for (let i = 0; i < n; i += 1) e[i] = Math.log(1e-4 + rms[i]);
    const flux = new Float32Array(n);
    const floor = Math.log(1e-4);
    for (let i = 0; i < n; i += 1) {
      const prev = ((i > 0 ? e[i - 1] : floor) + (i > 1 ? e[i - 2] : floor) + (i > 2 ? e[i - 3] : floor)) / 3;
      flux[i] = Math.max(0, e[i] - prev);
    }
    const win = 30;
    let sum = 0;
    const thr = new Float32Array(n);
    for (let i = 0; i < n + win; i += 1) {
      if (i < n) sum += flux[i];
      if (i - 2 * win - 1 >= 0) sum -= flux[i - 2 * win - 1];
      const c = i - win;
      if (c >= 0 && c < n) thr[c] = (sum / (2 * win + 1)) * 1.6 + 0.08;
    }
    const times = [];
    const strength = [];
    let last = -1e9;
    const at = (i) => (i >= 0 && i < n ? flux[i] : 0);
    for (let i = 0; i < n; i += 1) {
      const v = flux[i];
      if (v > thr[i] && v >= at(i - 1) && v >= at(i + 1) && v >= at(i - 2) && v >= at(i + 2) && i - last >= 5) {
        times.push(i / RATE);
        strength.push(v);
        last = i;
      }
    }
    const ref = strength.length ? percentile(strength, 0.9) : 1;
    const pulse = new Float32Array(n);
    const decay = Math.exp(-1 / (0.12 * RATE));
    let k = 0;
    let y = 0;
    for (let i = 0; i < n; i += 1) {
      y *= decay;
      while (k < times.length && Math.round(times[k] * RATE) === i) {
        y = Math.max(y, Math.min(1, strength[k] / ref));
        k += 1;
      }
      pulse[i] = y;
    }
    return { flux, pulse, times: Float32Array.from(times) };
  }

  async function analyse(buffer, opts) {
    const o = opts || {};
    const sr = buffer.sampleRate;
    const n = buffer.length;
    const frames = Math.max(1, Math.ceil(n * RATE / sr));
    const chans = [];
    for (let c = 0; c < Math.min(2, buffer.numberOfChannels); c += 1) chans.push(buffer.getChannelData(c));
    const inv = 1 / chans.length;
    const sum2 = new Float64Array(frames);
    const count = new Uint32Array(frames);
    const pmin = new Float32Array(frames);
    const pmax = new Float32Array(frames);
    const bands = o.bands ? [new Float64Array(frames), new Float64Array(frames), new Float64Array(frames)] : null;
    const lp = biquad('lp', 150, sr);
    const hp1 = biquad('hp', 150, sr);
    const lp2 = biquad('lp', 2000, sr);
    const hp2 = biquad('hp', 2500, sr);
    const CHUNK = 1 << 18;
    for (let start = 0; start < n; start += CHUNK) {
      const end = Math.min(n, start + CHUNK);
      for (let i = start; i < end; i += 1) {
        let x = chans[0][i];
        if (chans.length > 1) x = (x + chans[1][i]) * inv;
        const f = Math.min(frames - 1, Math.floor(i * RATE / sr));
        sum2[f] += x * x;
        count[f] += 1;
        if (x < pmin[f]) pmin[f] = x;
        if (x > pmax[f]) pmax[f] = x;
        if (bands) {
          const l = run(lp, x);
          const m = run(lp2, run(hp1, x));
          const h = run(hp2, x);
          bands[0][f] += l * l;
          bands[1][f] += m * m;
          bands[2][f] += h * h;
        }
      }
      if (o.onProgress) o.onProgress(end / n);
      await tick();
    }
    const rms = new Float32Array(frames);
    for (let f = 0; f < frames; f += 1) rms[f] = count[f] ? Math.sqrt(sum2[f] / count[f]) : 0;
    const onset = onsetsFrom(rms);

    /* Spectral frames for structure.js, computed here because this is the only place the decoded
       PCM exists: it is dropped as soon as analyse() returns. Structure analysis needs a spectrum,
       which the band filters above cannot give it, so either it happens in this pass or the file
       has to be decoded a second time. Tempo is not known yet and is not needed yet - these are a
       plain 20 Hz grid, and beat-syncing happens later, so changing the BPM re-segments without
       touching the audio again. */
    let spec = null;
    if (o.spectral && typeof window !== 'undefined' && window.FVStructure) {
      if (o.onStage) o.onStage('structure');
      const mono = new Float32Array(n);
      for (let i = 0; i < n; i += 1) {
        let x = chans[0][i];
        if (chans.length > 1) x = (x + chans[1][i]) * inv;
        mono[i] = x;
      }
      try {
        spec = window.FVStructure.spectral(mono, sr);
      } catch (err) {
        console.warn('Spectral analysis failed; structure will be unavailable', err);
      }
      await tick();
    }
    const out = {
      version: FEATURE_VERSION,
      rate: RATE,
      duration: n / sr,
      peakMin: pmin,
      peakMax: pmax,
      env: normalize(Float32Array.from(rms)),
      onset: onset.pulse,
      flux: onset.flux,
      onsetTimes: onset.times,
      spec
    };
    if (bands) {
      ['low', 'mid', 'high'].forEach((name, b) => {
        const arr = new Float32Array(frames);
        for (let f = 0; f < frames; f += 1) arr[f] = count[f] ? Math.sqrt(bands[b][f] / count[f]) : 0;
        out[name] = normalize(arr);
      });
    }
    return out;
  }

  // Load features for a blob, from cache or by decoding + analysing.
  async function featuresFor(blob, key, opts) {
    try {
      const cached = await idb.get('features', key);
      if (cached && cached.version === FEATURE_VERSION && (!opts.bands || cached.low)
        && (!opts.spectral || cached.spec)) return cached;
    } catch (err) {
      console.warn('Feature cache read failed', err);
    }
    if (opts.onStage) opts.onStage('decoding');
    let buffer = await decode(blob);
    if (opts.onStage) opts.onStage('analysing');
    const feats = await analyse(buffer, opts);
    buffer = null;
    try {
      await idb.put('features', key, feats);
    } catch (err) {
      console.warn('Feature cache write failed', err);
    }
    return feats;
  }

  // Tempo suggestion from onset-strength autocorrelation (the grid itself is always set by hand).
  function suggestTempo(feats) {
    const flux = feats.flux;
    if (!flux || flux.length < RATE * 4) return null;
    let best = null;
    for (let bpm = 70; bpm <= 180; bpm += 0.25) {
      const lag = 60 * RATE / bpm;
      const l0 = Math.floor(lag);
      const fr = lag - l0;
      let s = 0;
      for (let i = 0; i + l0 + 1 < flux.length; i += 1) s += flux[i] * (flux[i + l0] * (1 - fr) + flux[i + l0 + 1] * fr);
      const prior = Math.exp(-0.5 * Math.pow(Math.log2(bpm / 120) / 0.6, 2));
      const score = s * prior;
      if (!best || score > best.score) best = { bpm, score };
    }
    const period = 60 * RATE / best.bpm;
    let bestPhase = 0;
    let bestSum = -1;
    for (let ph = 0; ph < period; ph += 1) {
      let s = 0;
      for (let x = ph; x < flux.length; x += period) s += flux[Math.round(x)] || 0;
      if (s > bestSum) { bestSum = s; bestPhase = ph; }
    }
    return { bpm: Math.round(best.bpm * 10) / 10, offset: Math.round(bestPhase) / RATE };
  }

  // Lag (seconds) of a stem envelope against the master envelope, within +-2 s.
  function alignment(stemFeats, masterFeats) {
    const a = stemFeats.env;
    const b = masterFeats.env;
    const n = Math.min(a.length, b.length);
    if (n < RATE * 2) return null;
    const mean = (x) => { let s = 0; for (let i = 0; i < n; i += 1) s += x[i]; return s / n; };
    const ma = mean(a);
    const mb = mean(b);
    const corr = (lag) => {
      let s = 0, sa = 0, sb = 0;
      for (let i = Math.max(0, -lag); i < Math.min(n, n - lag); i += 2) {
        const x = a[i] - ma;
        const y = b[i + lag] - mb;
        s += x * y; sa += x * x; sb += y * y;
      }
      return s / Math.sqrt(sa * sb + 1e-12);
    };
    const r0 = corr(0);
    let best = { lag: 0, r: r0 };
    const maxLag = RATE; // +-1 s
    for (let lag = -maxLag; lag <= maxLag; lag += 1) {
      const r = corr(lag) - 0.0002 * Math.abs(lag); // prefer small lags: drums repeat every beat
      if (r > best.r) best = { lag, r };
    }
    // Only report an offset when it is clearly better than no offset.
    const offset = best.lag !== 0 && best.r > 0.5 && best.r - r0 > 0.08 ? best.lag / RATE : 0;
    return { offset, correlation: Math.max(r0, best.r), silent: !(percentile(a, 0.98) > 0.002) };
  }

  // ---------- transport ----------
  class Transport {
    constructor() {
      this.master = new Audio();
      this.master.preload = 'auto';
      this.masterUrl = null;
      this.stems = new Map(); // id -> { el, url }
      this.solo = null;
      this.playing = false;
      this.t = 0;
      this.duration = 0;
      this.onEnded = null;
      this._clockStart = 0;
      this._clockT0 = 0;
      this._lastAt = -1;
      this._lastPerf = 0;
      this.master.addEventListener('ended', () => this._ended());
    }

    hasMaster() { return !!this.masterUrl; }

    setMaster(blob) {
      this.pause();
      if (this.masterUrl) URL.revokeObjectURL(this.masterUrl);
      this.masterUrl = blob ? URL.createObjectURL(blob) : null;
      if (this.masterUrl) this.master.src = this.masterUrl;
      else this.master.removeAttribute('src');
    }

    setStem(id, blob) {
      this.removeStem(id);
      const url = URL.createObjectURL(blob);
      const el = new Audio();
      el.preload = 'auto';
      el.src = url;
      this.stems.set(id, { el, url });
    }

    removeStem(id) {
      const s = this.stems.get(id);
      if (!s) return;
      s.el.pause();
      s.el.removeAttribute('src');
      URL.revokeObjectURL(s.url);
      this.stems.delete(id);
      if (this.solo === id) this.setSolo(null);
    }

    setSolo(id) {
      const prev = this.solo && this.stems.get(this.solo);
      if (prev) prev.el.pause();
      this.solo = id && this.stems.has(id) ? id : null;
      this.master.muted = !!this.solo;
      if (this.playing && this.solo) this._startStem();
    }

    _startStem() {
      const s = this.solo && this.stems.get(this.solo);
      if (!s) return;
      s.el.currentTime = this.time();
      s.el.play().catch(() => {});
    }

    _ended() {
      this.pause();
      if (this.onEnded) this.onEnded();
    }

    time() {
      if (!this.playing) return this.t;
      if (this.masterUrl) {
        const at = this.master.currentTime;
        const now = performance.now();
        if (at !== this._lastAt) { this._lastAt = at; this._lastPerf = now; }
        return at + Math.min(0.1, (now - this._lastPerf) / 1000);
      }
      return this._clockT0 + (performance.now() - this._clockStart) / 1000;
    }

    // Called every animation frame while playing: clock end, stem drift.
    update() {
      if (!this.playing) return;
      const t = this.time();
      if (!this.masterUrl && this.duration > 0 && t >= this.duration) {
        this.t = this.duration;
        this._ended();
        return;
      }
      const s = this.solo && this.stems.get(this.solo);
      if (s && this.masterUrl && Math.abs(s.el.currentTime - this.master.currentTime) > 0.06) s.el.currentTime = this.master.currentTime;
    }

    async play() {
      if (this.playing) return;
      this.playing = true;
      if (this.masterUrl) {
        this.master.currentTime = this.t;
        this._lastAt = -1;
        try {
          await this.master.play();
        } catch (err) {
          console.warn('Playback failed', err);
          this.playing = false;
          return;
        }
      } else {
        this._clockStart = performance.now();
        this._clockT0 = this.t;
      }
      if (this.solo) this._startStem();
    }

    pause() {
      if (!this.playing) return;
      this.t = this.time();
      this.playing = false;
      this.master.pause();
      this.stems.forEach((s) => s.el.pause());
    }

    seek(t) {
      const v = Math.max(0, Number(t) || 0);
      this.t = v;
      if (this.playing) {
        if (this.masterUrl) {
          this.master.currentTime = v;
          this._lastAt = -1;
        } else {
          this._clockStart = performance.now();
          this._clockT0 = v;
        }
        const s = this.solo && this.stems.get(this.solo);
        if (s) s.el.currentTime = v;
      }
    }
  }

  // ---------- live input (microphone) ----------
  // The same features as the offline master analysis (level, lows, mids, highs, onsets), computed as the
  // sound arrives, so routes keyed to master.* follow whatever the microphone hears: a headset's own mic in
  // a room with music playing works without any audio file. Look-ahead (route lead) is impossible live and
  // is ignored; attack/release are applied per route as frames arrive (value()).
  // The worklet sums 10 ms blocks through the same filters as analyse() and posts [rms, low, mid, high].
  const WORKLET_SRC = `
  class FvLive extends AudioWorkletProcessor {
    constructor() {
      super();
      const f = (type, f0, q) => {
        const w0 = 2 * Math.PI * f0 / sampleRate, cos = Math.cos(w0), alpha = Math.sin(w0) / (2 * (q || Math.SQRT1_2)), a0 = 1 + alpha;
        const b = type === 'lp' ? [(1 - cos) / 2, 1 - cos, (1 - cos) / 2] : [(1 + cos) / 2, -(1 + cos), (1 + cos) / 2];
        return { b0: b[0] / a0, b1: b[1] / a0, b2: b[2] / a0, a1: (-2 * cos) / a0, a2: (1 - alpha) / a0, x1: 0, x2: 0, y1: 0, y2: 0 };
      };
      this.lp = f('lp', 150); this.hp1 = f('hp', 150); this.lp2 = f('lp', 2000); this.hp2 = f('hp', 2500);
      this.block = Math.max(1, Math.round(sampleRate / ${RATE}));
      this.n = 0; this.s = [0, 0, 0, 0];
    }
    run(f, x) { const y = f.b0 * x + f.b1 * f.x1 + f.b2 * f.x2 - f.a1 * f.y1 - f.a2 * f.y2; f.x2 = f.x1; f.x1 = x; f.y2 = f.y1; f.y1 = y; return y; }
    process(inputs) {
      const ch = inputs[0];
      if (!ch || !ch.length) return true;
      const a = ch[0], b = ch.length > 1 ? ch[1] : null, s = this.s;
      for (let i = 0; i < a.length; i += 1) {
        const x = b ? (a[i] + b[i]) * 0.5 : a[i];
        const l = this.run(this.lp, x), m = this.run(this.lp2, this.run(this.hp1, x)), h = this.run(this.hp2, x);
        s[0] += x * x; s[1] += l * l; s[2] += m * m; s[3] += h * h;
        if (++this.n >= this.block) {
          const k = 1 / this.n;
          this.port.postMessage([Math.sqrt(s[0] * k), Math.sqrt(s[1] * k), Math.sqrt(s[2] * k), Math.sqrt(s[3] * k)]);
          this.n = 0; s[0] = s[1] = s[2] = s[3] = 0;
        }
      }
      return true;
    }
  }
  registerProcessor('fv-live', FvLive);`;

  const LIVE_SOURCES = ['master.rms', 'master.low', 'master.mid', 'master.high', 'master.onset'];

  class LiveInput {
    constructor() {
      this.active = false;
      this.starting = false;
      this.error = null;
      this.raw = {};
      LIVE_SOURCES.forEach((id) => { this.raw[id] = 0; });
      this.smooth = new Map(); // `${source}|${attack}|${release}` -> { source, aa, ar, y, used }
      this.frames = 0;
      this._reset();
    }

    _reset() {
      this.ref = [0.02, 0.02, 0.02, 0.02];
      this.e = [Math.log(1e-4), Math.log(1e-4), Math.log(1e-4)];
      this.flux = new Float32Array(64); // ring, newest at fi
      this.fi = 0;
      this.fluxSum = 0;
      this.lastOnset = -1e9;
      this.strengthRef = 0.3;
      this.pulse = 0;
    }

    has(source) { return this.active && Object.prototype.hasOwnProperty.call(this.raw, source); }

    // Current value of a source after the route's attack/release (one-pole, the same law as the evaluator).
    value(source, attack, release) {
      const key = `${source}|${(attack || 0).toFixed(4)}|${(release || 0).toFixed(4)}`;
      let s = this.smooth.get(key);
      if (!s) {
        s = { source, aa: attack > 0 ? Math.exp(-1 / (attack * RATE)) : 0, ar: release > 0 ? Math.exp(-1 / (release * RATE)) : 0, y: this.raw[source] || 0, used: 0 };
        this.smooth.set(key, s);
      }
      s.used = this.frames;
      return s.y;
    }

    async start() {
      if (this.active || this.starting) return;
      this.starting = true;
      this.error = null;
      try {
        if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) throw new Error('This browser gives no microphone access here (it needs https or localhost).');
        // raw room sound: no echo cancelling, noise gate or automatic gain, which would flatten the music
        this.stream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false }, video: false });
        const AC = window.AudioContext || window.webkitAudioContext;
        this.ctx = new AC({ latencyHint: 'interactive' });
        if (this.ctx.state === 'suspended') await this.ctx.resume().catch(() => {});
        this.src = this.ctx.createMediaStreamSource(this.stream);
        if (this.ctx.audioWorklet && typeof AudioWorkletNode !== 'undefined') {
          const url = URL.createObjectURL(new Blob([WORKLET_SRC], { type: 'application/javascript' }));
          try { await this.ctx.audioWorklet.addModule(url); } finally { URL.revokeObjectURL(url); }
          this.node = new AudioWorkletNode(this.ctx, 'fv-live', { numberOfInputs: 1, numberOfOutputs: 0 });
          this.node.port.onmessage = (ev) => this._frame(ev.data);
          this.src.connect(this.node);
        } else {
          // older browsers: same filters on the main thread
          const sr = this.ctx.sampleRate;
          const f = [biquad('lp', 150, sr), biquad('hp', 150, sr), biquad('lp', 2000, sr), biquad('hp', 2500, sr)];
          const block = Math.max(1, Math.round(sr / RATE));
          const s = [0, 0, 0, 0];
          let n = 0;
          this.node = this.ctx.createScriptProcessor(1024, 1, 1);
          this.node.onaudioprocess = (ev) => {
            const a = ev.inputBuffer.getChannelData(0);
            for (let i = 0; i < a.length; i += 1) {
              const x = a[i];
              const l = run(f[0], x);
              const m = run(f[2], run(f[1], x));
              const hh = run(f[3], x);
              s[0] += x * x; s[1] += l * l; s[2] += m * m; s[3] += hh * hh;
              if (++n >= block) {
                const k = 1 / n;
                this._frame([Math.sqrt(s[0] * k), Math.sqrt(s[1] * k), Math.sqrt(s[2] * k), Math.sqrt(s[3] * k)]);
                n = 0; s[0] = s[1] = s[2] = s[3] = 0;
              }
            }
          };
          this.src.connect(this.node);
          this.mute = this.ctx.createGain();
          this.mute.gain.value = 0;
          this.node.connect(this.mute);
          this.mute.connect(this.ctx.destination);
        }
        this._reset();
        this.active = true;
        const track = this.stream.getAudioTracks()[0];
        if (track) track.addEventListener('ended', () => { this.error = 'The microphone stopped.'; this.stop(); });
      } catch (err) {
        this.error = err && err.name === 'NotAllowedError' ? 'Microphone permission was refused. Allow it for this site in the browser, then try again.' : (err && err.message) || String(err);
        this.stop();
        throw new Error(this.error);
      } finally {
        this.starting = false;
      }
      if (this.onChange) this.onChange();
    }

    stop() {
      const was = this.active;
      this.active = false;
      try { if (this.src) this.src.disconnect(); } catch (err) { /* already gone */ }
      try { if (this.node) { this.node.disconnect(); if (this.node.port) this.node.port.onmessage = null; } } catch (err) { /* already gone */ }
      if (this.stream) this.stream.getTracks().forEach((t) => t.stop());
      if (this.ctx) this.ctx.close().catch(() => {});
      this.stream = this.ctx = this.src = this.node = this.mute = null;
      LIVE_SOURCES.forEach((id) => { this.raw[id] = 0; });
      this.smooth.clear();
      if (was && this.onChange) this.onChange();
    }

    // One 10 ms frame: [rms, low, mid, high] -> normalised tracks + onset pulse, then every route's smoothing.
    _frame(v) {
      if (!this.active) return;
      this.frames += 1;
      // normalise like analyse() (98th percentile there): a peak follower with ~10 s memory, never below a
      // floor so a quiet room is not blown up to full scale
      const dec = Math.exp(-1 / (10 * RATE));
      const out = [0, 0, 0, 0];
      for (let b = 0; b < 4; b += 1) {
        this.ref[b] = Math.max(v[b], this.ref[b] * dec, 0.004);
        out[b] = Math.min(1, v[b] / (this.ref[b] * 0.85));
      }
      this.raw['master.rms'] = out[0];
      this.raw['master.low'] = out[1];
      this.raw['master.mid'] = out[2];
      this.raw['master.high'] = out[3];
      // onsets as in onsetsFrom(): log-energy rise over the mean of the last three frames, against a local
      // mean (trailing here) x 1.6 + 0.08, peaks picked two frames late so both neighbours are known
      const e = Math.log(1e-4 + v[0]);
      const fl = Math.max(0, e - (this.e[0] + this.e[1] + this.e[2]) / 3);
      this.e.pop();
      this.e.unshift(e);
      const N = this.flux.length;
      this.fi = (this.fi + 1) % N;
      this.fluxSum += fl - this.flux[this.fi];
      this.flux[this.fi] = fl;
      const at = (k) => this.flux[(this.fi - k + N) % N];
      const c = at(2);
      const thr = (this.fluxSum / N) * 1.6 + 0.08;
      this.pulse *= Math.exp(-1 / (0.12 * RATE));
      // onset strength normalised by a slow peak follower (the 90th percentile of strengths offline)
      this.strengthRef = Math.max(0.15, this.strengthRef * Math.exp(-1 / (30 * RATE)));
      if (c > thr && c >= at(0) && c >= at(1) && c >= at(3) && c >= at(4) && this.frames - this.lastOnset >= 5) {
        this.lastOnset = this.frames;
        this.strengthRef = Math.max(c, this.strengthRef);
        this.pulse = Math.max(this.pulse, Math.min(1, c / (this.strengthRef * 0.8)));
      }
      this.raw['master.onset'] = this.pulse;
      this.smooth.forEach((s, key) => {
        if (this.frames - s.used > 5 * RATE) { this.smooth.delete(key); return; }
        const x = this.raw[s.source] || 0;
        const a = x > s.y ? s.aa : s.ar;
        s.y = a * s.y + (1 - a) * x;
      });
    }

    level() { return this.active ? this.raw['master.rms'] : 0; }
  }

  FV.Audio = { RATE, idb, fileKey, decode, analyse, featuresFor, suggestTempo, alignment, Transport, LiveInput, LIVE_SOURCES };
})();
