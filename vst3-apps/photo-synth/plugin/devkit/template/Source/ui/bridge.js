/* ===================================================================== *
 * Native bridge — generic template.
 *
 * Give the prototype's JavaScript objects with the same shape as the Web
 * Audio nodes it already uses, and marshal every call to the C++ engine.
 * The page's own code then runs completely unchanged.
 *
 * Splice this into the page before the section that builds the audio graph,
 * then replace the graph-building functions with proxy equivalents.
 *
 * Adapt: PID / VF / the message kinds to your engine's enums.
 * ===================================================================== */

/* ---- transport: batched messages to C++ ---------------------------- */
var NB = {
  q: [],
  scheduled: false,
  send: function (m) {
    this.q.push(m);
    if (!this.scheduled) {
      this.scheduled = true;
      var self = this;
      // One IPC message per microtask: a single gesture can touch hundreds
      // of parameters and they must not become hundreds of messages.
      Promise.resolve().then(function () {
        self.scheduled = false;
        var batch = self.q;
        self.q = [];
        try { window.__JUCE__.backend.emitEvent("ps", { b: batch }); } catch (e) {}
      });
    }
  },
  on: function (name, fn) {
    try { window.__JUCE__.backend.addEventListener(name, fn); } catch (e) {}
  }
};

/* ---- ids: keep in the same order as the C++ enums ------------------- */
var PID = { master: 0 /* … one per engine parameter … */ };
var VF  = { gate: 0, pan: 1, gain: 2 /* … one per voice field … */ };

/* ---- clock: the engine pushes {t, fs}; interpolate between pushes --- */
var NATIVE = { t: 0, at: (typeof performance !== "undefined" ? performance.now() : 0), fs: 48000 };
var liveCtx = {
  state: "running",
  resume: function () {},
  audioWorklet: { addModule: function () { return Promise.resolve(); } },
  destination: {}
};
Object.defineProperty(liveCtx, "currentTime", {
  get: function () { return NATIVE.t + (performance.now() - NATIVE.at) / 1000; }
});
Object.defineProperty(liveCtx, "sampleRate", { get: function () { return NATIVE.fs; } });

/* A second "context" that records instead of sending — this is how an
 * offline render is built: the page schedules exactly as it would live. */
function collectorCtx(sr, nVoices) {
  var rec = { sched: [], voices: [] };
  for (var i = 0; i < nVoices; i++) rec.voices.push({ f: [], ev: [] });
  return { __collector: true, rec: rec, currentTime: 0, sampleRate: sr,
           state: "running", resume: function () {},
           audioWorklet: { addModule: function () { return Promise.resolve(); } } };
}

/* ---- AudioParam proxy ---------------------------------------------- */
function sinkParam(ctx, id, mode, v, tc, t) {
  // mode: 0 = setValueAtTime, 1 = setTargetAtTime, 2 = cancelScheduledValues
  if (ctx.__collector) {
    if (mode !== 2) ctx.rec.sched.push([id, mode, v, tc || 0, (t === undefined || t < 0) ? 0 : t]);
    return;
  }
  if (mode === 2) { NB.send({ k: "p", i: id, m: 2 }); return; }
  var tt = t === undefined ? -1 : t;
  if (tt >= 0 && tt <= liveCtx.currentTime + 0.005) tt = -1;   // "now"
  NB.send({ k: "p", i: id, m: mode, v: +v, tc: tc || 0, t: tt });
}

function mkParam(ctx, id) {
  var p = { _v: 0 };
  Object.defineProperty(p, "value", {
    get: function () { return p._v; },
    set: function (v) { p._v = +v; sinkParam(ctx, id, 0, +v, 0, -1); }
  });
  p.setValueAtTime          = function (v, t)     { p._v = +v; sinkParam(ctx, id, 0, +v, 0, t); };
  p.setTargetAtTime         = function (v, t, tc) { p._v = +v; sinkParam(ctx, id, 1, +v, tc, t); };
  p.linearRampToValueAtTime = function (v, t)     { p._v = +v; sinkParam(ctx, id, 1, +v, 0.02, t); };
  p.cancelScheduledValues   = function ()         { sinkParam(ctx, id, 2, 0, 0, -1); };
  return p;
}

/* A param the engine does not model — keeps the page happy, costs nothing. */
function inertParam(v0) {
  return { value: v0 || 0, setValueAtTime: function () {}, setTargetAtTime: function () {},
           linearRampToValueAtTime: function () {}, cancelScheduledValues: function () {} };
}
function inertGain(v0) { return { gain: inertParam(v0), connect: function () {}, disconnect: function () {} }; }

/* ---- biquad proxy, including the response curve the UI draws -------- */
function biquadCoeffs(type, f0, Q, gdb, fs) {
  var w = 6.2831853 * Math.min(Math.max(f0, 0), fs / 2) / fs;
  var cw = Math.cos(w), sw = Math.sin(w);
  var b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0, alpha, q, A;
  if (type === "lowpass" || type === "highpass") {
    q = Math.pow(10, Q / 20);                    // NB: Q in dB for these two
    alpha = sw / (2 * Math.max(1e-9, q));
    if (type === "lowpass") { b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = b0; }
    else                    { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0; }
    a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
  } else if (type === "bandpass") {
    alpha = sw / (2 * Math.max(1e-9, Q));
    b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
  } else if (type === "notch") {
    alpha = sw / (2 * Math.max(1e-9, Q));
    b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
  } else if (type === "allpass") {
    alpha = sw / (2 * Math.max(1e-9, Q));
    b0 = 1 - alpha; b1 = -2 * cw; b2 = 1 + alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
  } else {                                       // peaking
    A = Math.pow(10, gdb / 40);
    alpha = sw / (2 * Math.max(1e-9, Q));
    b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
    a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
  }
  return [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0];
}

function mkBiquadProxy(ctx, freqId, qId, gainId, type0, q0, sendsType) {
  var bq = {
    frequency: freqId !== null ? mkParam(ctx, freqId) : inertParam(350),
    Q:    (qId    !== null && qId    !== undefined) ? mkParam(ctx, qId)    : inertParam(q0),
    gain: (gainId !== null && gainId !== undefined) ? mkParam(ctx, gainId) : inertParam(0),
    _type: type0,
    connect: function () {}
  };
  Object.defineProperty(bq, "type", {
    get: function () { return bq._type; },
    set: function (t) {
      bq._type = t;
      if (!sendsType) return;
      var idx = { lowpass: 0, highpass: 1, bandpass: 2, notch: 3, allpass: 4, peaking: 5 }[t] || 0;
      if (ctx.__collector) ctx.rec.lptype = idx; else NB.send({ k: "lptype", t: idx });
    }
  });
  bq.getFrequencyResponse = function (freqs, mag, phase) {
    var c = biquadCoeffs(bq._type, bq.frequency.value || 350,
                         bq.Q.value !== undefined ? bq.Q.value : 1,
                         bq.gain.value !== undefined ? bq.gain.value : 0, ctx.sampleRate);
    for (var i = 0; i < freqs.length; i++) {
      var w = 6.2831853 * freqs[i] / ctx.sampleRate;
      var c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
      var nr = c[0] + c[1] * c1 + c[2] * c2, ni = -(c[1] * s1 + c[2] * s2);
      var dr = 1 + c[3] * c1 + c[4] * c2,    di = -(c[3] * s1 + c[4] * s2);
      mag[i] = Math.sqrt((nr * nr + ni * ni) / Math.max(1e-24, dr * dr + di * di));
      if (phase) phase[i] = 0;
    }
  };
  return bq;
}

/* ---- worklet voice proxy ------------------------------------------- */
function voiceMsgFields(p) {
  var f = [], k;
  for (k in p) {
    if (VF[k] === undefined) continue;
    var v = p[k];
    if (k === "gate") v = v ? 1 : 0;
    f.push([VF[k], +v]);
  }
  return f;
}

function NVoice(ctx, index) {
  function post(msg) {
    if (!msg) return;
    if (msg.type === "params" && msg.p) {
      var f = voiceMsgFields(msg.p);
      if (!f.length) return;
      if (ctx.__collector) Array.prototype.push.apply(ctx.rec.voices[index].f, f);
      else NB.send({ k: "v", v: index, f: f });
    } else if (msg.type === "events" && msg.ev) {
      var out = msg.ev.map(function (e) {
        return { f: Math.max(0, Math.round(e.frame)),
                 g: e.gate === undefined ? -1 : (e.gate ? 1 : 0),
                 p: e.p ? voiceMsgFields(e.p).sort(function (a, b) { return a[0] - b[0]; }) : [] };
      });
      if (ctx.__collector) Array.prototype.push.apply(ctx.rec.voices[index].ev, out);
      else NB.send({ k: "ve", v: index, ev: out });
    } else if (msg.type === "clear") {
      if (!ctx.__collector) NB.send({ k: "clear", v: index });
    } else if (msg.type === "reset") {
      if (!ctx.__collector) NB.send({ k: "reset", v: index });
    }
  }
  this.index = index;
  this.postMessage = post;
  this.port = { postMessage: post, onmessage: null };
  this.connect = function () {};
}

/* ---- incoming events from C++ -------------------------------------- */
(function initNativeBridge() {
  NB.on("clock", function (e) {
    NATIVE.t = +e.t || 0;
    NATIVE.at = performance.now();
    if (e.fs) NATIVE.fs = +e.fs;
  });
  NB.on("hostParam", function (e) { /* apply automation to the matching control */ });
  NB.on("midiIn",    function (e) { /* light up keys, set the base note, … */ });
  NB.on("initialState", function (e) { /* restore the page from project state */ });
  NB.send({ k: "getstate" });
})();
