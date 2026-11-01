/* Audio: IndexedDB file store, offline analysis (once per file, cached), tempo suggestion,
   stem alignment and a lightweight transport (plain <audio> elements, no WebAudio graph).

   Analysis runs on the decoded file once, in slices so the UI stays responsive, and the decoded
   PCM is dropped afterwards. Only compact feature tracks at RATE Hz are kept. */
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});

  const RATE = 100;            // feature tracks per second
  const DECODE_RATE = 22050;   // analysis sample rate (halves memory vs 44.1k)
  const FEATURE_VERSION = 4;

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
    const out = {
      version: FEATURE_VERSION,
      rate: RATE,
      duration: n / sr,
      peakMin: pmin,
      peakMax: pmax,
      env: normalize(Float32Array.from(rms)),
      onset: onset.pulse,
      flux: onset.flux,
      onsetTimes: onset.times
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
      if (cached && cached.version === FEATURE_VERSION && (!opts.bands || cached.low)) return cached;
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

  FV.Audio = { RATE, idb, fileKey, decode, analyse, featuresFor, suggestTempo, alignment, Transport };
})();
