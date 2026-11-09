/* Synthetic songs with a structure we chose, so the structure analyser can be checked against a
   known right answer rather than against taste.

   Four voices that differ the way real passages differ - instrumentation (which the MFCC half of
   the feature vector sees) and chords (which the chroma half sees) - plus drums that differ in
   density. renderForm() lays them out in whatever form a test asks for, at whatever tempo and
   metre, optionally scaling each bar so a gradual build can be tested too.

   The voices are deliberately crude. The point is not realism; it is that the right answer is
   known exactly, which no real recording gives us. Noise is added by the tests on top. */
const SR = 22050;

const hz = (midi) => 440 * Math.pow(2, (midi - 69) / 12);

const VOICE = {
  // dark: fundamental plus a weak second harmonic, on C
  A: {
    chord: [36, 48, 55, 60], gain: 0.5, hat: false, eighths: false,
    harm: (f, t) => Math.sin(2 * Math.PI * f * t) + 0.25 * Math.sin(4 * Math.PI * f * t)
  },
  // bright: sawtooth, ten harmonics, on G, with a hi-hat on every beat
  B: {
    chord: [43, 55, 62, 67], gain: 0.3, hat: true, eighths: false,
    harm: (f, t) => { let v = 0; for (let h = 1; h <= 10; h += 1) v += Math.sin(2 * Math.PI * f * h * t) / h; return v; }
  },
  // mid: odd harmonics only, on A minor, with something on every eighth
  C: {
    chord: [45, 57, 64, 69], gain: 0.35, hat: false, eighths: true,
    harm: (f, t) => { let v = 0; for (let h = 1; h <= 7; h += 2) v += Math.sin(2 * Math.PI * f * h * t) / h; return v; }
  },
  // tremolo, on F, with a hat
  D: {
    chord: [41, 53, 60, 65], gain: 0.45, hat: true, eighths: false,
    harm: (f, t) => Math.sin(2 * Math.PI * f * t) * (1 + 0.5 * Math.sin(2 * Math.PI * 5 * t))
  }
};

// A fixed generator, so a failing test fails the same way twice.
function prng(seed) {
  let s = seed >>> 0 || 1;
  return () => { s = (s * 1103515245 + 12345) & 0x7fffffff; return s / 0x7fffffff; };
}

/* form     [{ type: 'A'|'B'|'C'|'D', bars }]
   gainAt   optional (bar, totalBars) -> multiplier, for builds and fades
   Returns { x, onsets, duration, totalBars, boundaries } where boundaries are the bar numbers
   the form actually changes at - the answer a test compares against. */
function renderForm(form, bpm, beatsPerBar, opts) {
  const o = opts || {};
  const rnd = prng(o.seed || 12345);
  const spb = 60 / bpm;
  const secPerBar = spb * beatsPerBar;
  const totalBars = form.reduce((a, s) => a + s.bars, 0);
  const n = Math.ceil(totalBars * secPerBar * SR) + SR;
  const x = new Float32Array(n);
  const onsets = [];
  const boundaries = [];
  let bar = 0;
  let acc = 0;

  form.forEach((sec, si) => {
    if (si) boundaries.push(acc);
    acc += sec.bars;
    const V = VOICE[sec.type];
    const notes = V.chord.map(hz);
    for (let b = 0; b < sec.bars; b += 1, bar += 1) {
      const g = o.gainAt ? o.gainAt(bar, totalBars) : 1;
      for (let beat = 0; beat < beatsPerBar; beat += 1) {
        const t0 = bar * secPerBar + beat * spb;
        const i0 = Math.floor(t0 * SR);

        // kick on the downbeat, and on 3 in four-four
        if (beat === 0 || (beatsPerBar === 4 && beat === 2)) {
          onsets.push(t0);
          for (let i = 0; i < SR * 0.12; i += 1) {
            const e = Math.exp(-i / (SR * 0.03));
            const f = 90 * Math.exp(-i / (SR * 0.02)) + 45;
            if (i0 + i < n) x[i0 + i] += 0.9 * g * e * Math.sin(2 * Math.PI * f * i / SR);
          }
        }
        if (V.hat) {
          onsets.push(t0);
          for (let i = 0; i < SR * 0.05; i += 1) {
            const e = Math.exp(-i / (SR * 0.012));
            if (i0 + i < n) x[i0 + i] += 0.33 * g * e * (rnd() * 2 - 1);
          }
        }
        if (V.eighths) {
          for (let eh = 0; eh < 2; eh += 1) {
            const te = t0 + eh * spb / 2;
            const ie = Math.floor(te * SR);
            onsets.push(te);
            for (let i = 0; i < SR * 0.09; i += 1) {
              const e = Math.exp(-i / (SR * 0.025));
              if (ie + i < n) x[ie + i] += 0.3 * g * e * Math.sin(2 * Math.PI * 520 * i / SR) * (0.6 + 0.4 * rnd());
            }
          }
        }

        // one pitched note a beat, cycling the chord
        const f0 = notes[beat % notes.length];
        const dur = spb * 0.9;
        for (let i = 0; i < dur * SR; i += 1) {
          const t = i / SR;
          const env = Math.min(1, t / 0.01) * Math.exp(-t / (dur * 0.5));
          if (i0 + i < n) x[i0 + i] += 0.35 * g * V.gain * env * V.harm(f0, t);
        }
      }
    }
  });

  let peak = 0;
  for (let i = 0; i < n; i += 1) peak = Math.max(peak, Math.abs(x[i]));
  if (peak > 0) for (let i = 0; i < n; i += 1) x[i] /= peak * 1.05;
  onsets.sort((a, b) => a - b);
  return { x, onsets: Float32Array.from(onsets), duration: n / SR, totalBars, boundaries, sampleRate: SR };
}

// Stand-ins for the 100 Hz tracks audio.js supplies; only the descriptors read these.
function tracksOf(x) {
  const RATE = 100;
  const frames = Math.ceil(x.length * RATE / SR);
  const env = new Float32Array(frames);
  const count = new Uint32Array(frames);
  for (let i = 0; i < x.length; i += 1) {
    const f = Math.min(frames - 1, Math.floor(i * RATE / SR));
    env[f] += x[i] * x[i];
    count[f] += 1;
  }
  for (let f = 0; f < frames; f += 1) env[f] = count[f] ? Math.sqrt(env[f] / count[f]) : 0;
  let max = 0;
  for (let f = 0; f < frames; f += 1) max = Math.max(max, env[f]);
  if (max > 0) for (let f = 0; f < frames; f += 1) env[f] /= max;
  return { rate: RATE, env, low: env, mid: env, high: env };
}

function addNoise(x, amp, seed) {
  const rnd = prng(seed || 7);
  return Float32Array.from(x, (v) => v + amp * (rnd() * 2 - 1));
}

// Everything a call to Structure.segment() needs, from a rendered song.
function featsOf(Structure, song) {
  const spec = Structure.spectral(song.x, song.sampleRate || SR);
  return Object.assign(tracksOf(song.x), {
    duration: song.duration, onsetTimes: song.onsets, spec
  });
}

module.exports = { SR, VOICE, renderForm, tracksOf, addNoise, featsOf, prng };
