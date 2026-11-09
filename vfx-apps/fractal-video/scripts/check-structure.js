/* Headless checks of js/structure.js: the FFT and filterbanks against known answers, then
   segmentation against synthetic songs whose form we chose.

   Run with: npm run check:structure   (or npm run check, which calls this too)

   The numbers quoted in the comments here are measurements, and several of them are the reason a
   constant in structure.js is what it is. If one of these tests starts failing after a change to
   the DSP, the measurement is the thing to re-take - not the assertion to relax. */
const assert = require('assert');
const S = require('../js/structure.js');
const F = require('./structure-fixture.js');

let passed = 0;
function test(name, fn) {
  try {
    fn();
    passed += 1;
    console.log(`ok   ${name}`);
  } catch (err) {
    console.error(`FAIL ${name}\n     ${err.message}`);
    process.exitCode = 1;
  }
}

const { fft, melBank, chromaMap, mfcc, snapToBars } = S._internal;

/* ========================================================================= FFT */

test('FFT agrees with a direct DFT', () => {
  const n = 64;
  const re = new Float64Array(n);
  const im = new Float64Array(n);
  for (let i = 0; i < n; i += 1) re[i] = Math.sin(i * 0.7) + 0.3 * Math.cos(i * 2.1);
  const R = new Float64Array(n);
  const I = new Float64Array(n);
  for (let k = 0; k < n; k += 1) {
    for (let t = 0; t < n; t += 1) {
      const a = -2 * Math.PI * k * t / n;
      R[k] += re[t] * Math.cos(a);
      I[k] += re[t] * Math.sin(a);
    }
  }
  fft(re, im);
  let max = 0;
  for (let k = 0; k < n; k += 1) max = Math.max(max, Math.abs(re[k] - R[k]), Math.abs(im[k] - I[k]));
  assert.ok(max < 1e-9, `max error ${max.toExponential(2)}`);
});

test('a pure tone lands in its own bin at magnitude n/2', () => {
  const n = 1024;
  const bin = 37;
  const re = new Float64Array(n);
  const im = new Float64Array(n);
  for (let i = 0; i < n; i += 1) re[i] = Math.cos(2 * Math.PI * bin * i / n);
  fft(re, im);
  let peak = -1;
  let mag = -1;
  for (let k = 0; k < n / 2; k += 1) {
    const m = Math.hypot(re[k], im[k]);
    if (m > mag) { mag = m; peak = k; }
  }
  assert.strictEqual(peak, bin);
  assert.ok(Math.abs(mag - n / 2) < 1e-6, `magnitude ${mag}`);
});

test('FFT conserves energy (Parseval)', () => {
  const n = 256;
  const rnd = F.prng(3);
  const re = new Float64Array(n);
  const im = new Float64Array(n);
  for (let i = 0; i < n; i += 1) re[i] = rnd() * 2 - 1;
  let timeE = 0;
  for (let i = 0; i < n; i += 1) timeE += re[i] * re[i];
  fft(re, im);
  let freqE = 0;
  for (let k = 0; k < n; k += 1) freqE += (re[k] * re[k] + im[k] * im[k]) / n;
  assert.ok(Math.abs(timeE - freqE) / timeE < 1e-10);
});

test('FFT rejects a non-power-of-two size rather than returning nonsense', () => {
  assert.throws(() => fft(new Float64Array(100), new Float64Array(100)), /power of two/);
});

/* ================================================================= filterbanks */

test('mel bands are unit-sum, ascending and inside Nyquist', () => {
  const bank = melBank(22050, 2048, 24);
  assert.strictEqual(bank.length, 24);
  bank.forEach((b, i) => {
    const sum = b.w.reduce((a, c) => a + c, 0);
    assert.ok(Math.abs(sum - 1) < 1e-5, `band ${i} sums to ${sum}`);
    assert.ok(b.to >= b.from, `band ${i} is empty`);
    assert.ok(b.to < 1024, `band ${i} runs past Nyquist`);
    if (i) assert.ok(b.from >= bank[i - 1].from, `band ${i} is out of order`);
  });
});

test('chroma bins map to the right pitch classes', () => {
  const sr = 22050;
  const N = 2048;
  const binHz = sr / N;
  const map = chromaMap(sr, N);
  const at = (f) => map[Math.round(f / binHz)];
  assert.strictEqual(at(440), 9, 'A440 is pitch class 9');
  assert.strictEqual(at(261.63), 0, 'middle C is pitch class 0');
  assert.strictEqual(at(329.63), 4, 'E is pitch class 4');
  assert.strictEqual(at(30), -1, 'below C2 is excluded');
  assert.strictEqual(at(6000), -1, 'above B6 is excluded');
  assert.strictEqual(new Set(Array.from(map).filter((v) => v >= 0)).size, 12);
});

test('MFCC 1..13 are blind to a gain change', () => {
  // This is what makes a fade-in, or a louder mix of the same passage, not look like a new
  // passage: a gain change is a constant offset on every log-power band, which lands entirely in
  // coefficient 0, and coefficient 0 is dropped.
  const nMel = 24;
  const frame = new Float64Array(nMel);
  for (let b = 0; b < nMel; b += 1) frame[b] = Math.log(1e-10 + Math.exp(-b * 0.3) * 0.01);
  const quiet = mfcc(frame, nMel, 13, new Float64Array(13));
  const loud = mfcc(Float64Array.from(frame, (v) => v + Math.log(100)), nMel, 13, new Float64Array(13));
  let max = 0;
  for (let i = 0; i < 13; i += 1) max = Math.max(max, Math.abs(quiet[i] - loud[i]));
  assert.ok(max < 1e-9, `drifted by ${max.toExponential(2)}`);
});

/* ================================================================== bar snapping */

test('boundaries one bar off the 4-bar grid are pulled onto it', () => {
  const out = snapToBars([{ t: 16, v: 1 }, { t: 36, v: 1 }, { t: 64, v: 1 }, { t: 92, v: 1 }], 4, 48);
  assert.deepStrictEqual(out.map((b) => b.bar), [4, 8, 16, 24]);
  assert.strictEqual(out.filter((b) => b.moved).length, 2);
});

test('an odd-length section is not forced onto the grid', () => {
  // two bars off is more likely a real 6-bar section than a detection error
  const out = snapToBars([{ t: 24, v: 1 }, { t: 56, v: 1 }, { t: 88, v: 1 }], 4, 48);
  assert.deepStrictEqual(out.map((b) => b.bar), [6, 14, 22]);
});

test('a consistent off-grid phase is preserved, not dragged to 4n', () => {
  const out = snapToBars([{ t: 8, v: 1 }, { t: 24, v: 1 }, { t: 40, v: 1 }, { t: 56, v: 1 }], 4, 48);
  assert.deepStrictEqual(out.map((b) => b.bar), [2, 6, 10, 14]);
});

test('no boundary is placed at bar 0 or at the end of the song', () => {
  const out = snapToBars([{ t: 0, v: 1 }, { t: 192, v: 1 }], 4, 48);
  assert.ok(out.every((b) => b.bar > 0 && b.bar < 48));
});

/* ================================================================= segmentation */

const seg = (song, grid, opts) => S.segment(F.featsOf(S, song), grid, opts);
const barsOf = (r) => (r.ok ? r.boundaries.map((b) => b.bar) : []);
const typesOf = (r) => (r.ok ? r.segments.map((s) => s.type).join('') : '');
const foundAll = (r, want, slack) => want.every((w) => barsOf(r).some((g) => Math.abs(g - w) <= (slack == null ? 1 : slack)));
const spurious = (r, want, slack) => barsOf(r).filter((g) => !want.some((w) => Math.abs(g - w) <= (slack == null ? 1 : slack))).length;

const FORM_ABABCA = [
  { type: 'A', bars: 8 }, { type: 'B', bars: 8 }, { type: 'A', bars: 8 },
  { type: 'B', bars: 8 }, { type: 'C', bars: 8 }, { type: 'A', bars: 8 }
];

test('A B A B C A: every boundary exact, every repeat recognised', () => {
  const song = F.renderForm(FORM_ABABCA, 120, 4);
  const r = seg(song, { bpm: 120, beatsPerBar: 4, offset: 0 });
  assert.ok(r.ok, r.reason);
  assert.deepStrictEqual(barsOf(r), song.boundaries, `got ${barsOf(r)}`);
  assert.strictEqual(typesOf(r), 'ABABCA');
  assert.strictEqual(r.types, 3);
});

test('the descriptors order the passages as they were built', () => {
  const song = F.renderForm(FORM_ABABCA, 120, 4);
  const r = seg(song, { bpm: 120, beatsPerBar: 4, offset: 0 });
  const [A, B, , , C] = r.segments;
  assert.ok(B.brightness > A.brightness, `sawtooth ${B.brightness} should beat dark bass ${A.brightness}`);
  assert.ok(C.density > A.density, `eighths ${C.density} should beat a kick on 1 and 3 ${A.density}`);
  r.segments.forEach((s) => {
    ['loudness', 'brightness', 'density', 'bass', 'sharpness'].forEach((k) => {
      assert.ok(s[k] >= 0 && s[k] <= 1, `${k} out of range: ${s[k]}`);
    });
  });
});

test('the form survives noise down to a negative signal-to-noise ratio', () => {
  // The absolute distances between passages collapse towards zero as noise rises, but their
  // ratio holds: measured 57x clean, 275x at 6 dB, 145x at -1 dB. This is the test that fails
  // if anyone puts absolute bounds back on the cluster cut.
  const song = F.renderForm(FORM_ABABCA, 120, 4);
  [0.05, 0.15, 0.35].forEach((amp) => {
    const noisy = Object.assign({}, song, { x: F.addNoise(song.x, amp, 11) });
    const r = seg(noisy, { bpm: 120, beatsPerBar: 4, offset: 0 });
    assert.ok(r.ok, `noise ${amp}: ${r.reason}`);
    assert.ok(foundAll(r, song.boundaries), `noise ${amp}: boundaries ${barsOf(r)}`);
    assert.ok(spurious(r, song.boundaries) <= 1, `noise ${amp}: ${spurious(r, song.boundaries)} spurious`);
    assert.strictEqual(typesOf(r), 'ABABCA', `noise ${amp}: types ${typesOf(r)}`);
  });
});

test('a song with no structure gets none invented for it', () => {
  // The whole reason novelty() is scaled by the kernel's own weight sum rather than by the song's
  // maximum. Measured: a true boundary scores 0.07 to 0.27, this scores under 0.003.
  const song = F.renderForm([{ type: 'A', bars: 48 }], 120, 4);
  const r = seg(song, { bpm: 120, beatsPerBar: 4, offset: 0 });
  assert.ok(r.ok, r.reason);
  assert.strictEqual(r.boundaries.length, 0, `found ${barsOf(r)}`);
  assert.strictEqual(r.types, 1);
});

test('a gradual build is a loudness change, not a new passage', () => {
  const song = F.renderForm([{ type: 'A', bars: 24 }], 120, 4,
    { gainAt: (bar, total) => 0.25 + 0.75 * (bar / total) });
  const r = seg(song, { bpm: 120, beatsPerBar: 4, offset: 0 });
  assert.strictEqual(r.boundaries.length, 0, `found ${barsOf(r)}`);
});

test('an odd-length section comes back odd', () => {
  const form = [{ type: 'A', bars: 8 }, { type: 'B', bars: 6 }, { type: 'A', bars: 8 }, { type: 'C', bars: 10 }];
  const song = F.renderForm(form, 120, 4);
  const r = seg(song, { bpm: 120, beatsPerBar: 4, offset: 0 });
  assert.deepStrictEqual(barsOf(r), song.boundaries, `got ${barsOf(r)}`);
  assert.strictEqual(typesOf(r), 'ABAC');
});

test('three beats to the bar', () => {
  const form = [{ type: 'A', bars: 8 }, { type: 'B', bars: 8 }, { type: 'A', bars: 8 }, { type: 'C', bars: 8 }];
  const song = F.renderForm(form, 120, 3);
  const r = seg(song, { bpm: 120, beatsPerBar: 3, offset: 0 });
  assert.ok(foundAll(r, song.boundaries), `got ${barsOf(r)}`);
  assert.strictEqual(typesOf(r), 'ABAC');
});

test('a section repeated back to back has no seam invented inside it', () => {
  const form = [{ type: 'A', bars: 8 }, { type: 'A', bars: 8 }, { type: 'B', bars: 8 }, { type: 'B', bars: 8 }];
  const song = F.renderForm(form, 120, 4);
  const r = seg(song, { bpm: 120, beatsPerBar: 4, offset: 0 });
  assert.ok(foundAll(r, [16]), `got ${barsOf(r)}`);
  assert.ok(!barsOf(r).some((g) => Math.abs(g - 8) <= 1 || Math.abs(g - 24) <= 1), `got ${barsOf(r)}`);
});

test('a full song shape: intro, verses, choruses, bridge, outro', () => {
  const form = [
    { type: 'A', bars: 8 }, { type: 'B', bars: 16 }, { type: 'C', bars: 16 }, { type: 'B', bars: 16 },
    { type: 'C', bars: 16 }, { type: 'D', bars: 16 }, { type: 'C', bars: 16 }, { type: 'A', bars: 8 }
  ];
  const song = F.renderForm(form, 128, 4);
  const r = seg(song, { bpm: 128, beatsPerBar: 4, offset: 0 });
  assert.ok(r.ok, r.reason);
  const hit = song.boundaries.filter((w) => barsOf(r).some((g) => Math.abs(g - w) <= 1)).length;
  assert.ok(hit >= 6, `only ${hit} of ${song.boundaries.length} boundaries: ${barsOf(r)}`);
  assert.ok(spurious(r, song.boundaries) <= 1, `${spurious(r, song.boundaries)} spurious: ${barsOf(r)}`);
  assert.strictEqual(typesOf(r), 'ABCBCDCA', `types ${typesOf(r)}`);
});

test('a half or double tempo grid moves the bar numbers, not the music', () => {
  const song = F.renderForm(FORM_ABABCA, 120, 4);
  const half = seg(song, { bpm: 60, beatsPerBar: 4, offset: 0 });
  assert.ok(foundAll(half, song.boundaries.map((b) => b / 2)), `half: ${barsOf(half)}`);
  const dbl = seg(song, { bpm: 240, beatsPerBar: 4, offset: 0 });
  assert.ok(foundAll(dbl, song.boundaries.map((b) => b * 2)), `double: ${barsOf(dbl)}`);
});

test('a 2 % tempo error still finds the boundaries', () => {
  // a whole beat of drift every 50 beats, which is what really punishes beat-synchronous features
  const song = F.renderForm(FORM_ABABCA, 120, 4);
  const r = seg(song, { bpm: 120 * 1.02, beatsPerBar: 4, offset: 0 });
  const want = song.boundaries.map((b) => b * 1.02);
  assert.ok(want.every((w) => barsOf(r).some((g) => Math.abs(g - w) <= 1.5)), `got ${barsOf(r)} want ~${want.map((w) => w.toFixed(1))}`);
});

test('leading silence shifts the boundaries and is itself a boundary', () => {
  const song = F.renderForm(FORM_ABABCA, 120, 4);
  const lead = Math.round(3 * 2 * F.SR); // three bars at 120 BPM
  const x = new Float32Array(song.x.length + lead);
  x.set(song.x, lead);
  const shifted = {
    x, sampleRate: F.SR, duration: x.length / F.SR,
    onsets: Float32Array.from(song.onsets, (t) => t + lead / F.SR)
  };
  const r = seg(shifted, { bpm: 120, beatsPerBar: 4, offset: 0 });
  assert.ok(foundAll(r, song.boundaries.map((b) => b + 3)), `got ${barsOf(r)}`);
  assert.ok(barsOf(r).some((g) => Math.abs(g - 3) <= 1), `the music starting at bar 3 should itself be a boundary: ${barsOf(r)}`);
});

test('degenerate input returns a reason instead of throwing', () => {
  assert.strictEqual(S.segment({ duration: 60 }, { bpm: 120, beatsPerBar: 4 }).reason, 'no-spectral');
  const tiny = S.spectral(new Float32Array(F.SR * 3), F.SR);
  assert.strictEqual(S.segment({ duration: 3, spec: tiny, rate: 100 }, { bpm: 120, beatsPerBar: 4 }).reason, 'too-short');
  const silent = S.spectral(new Float32Array(F.SR * 60), F.SR);
  const r = S.segment({ duration: 60, spec: silent, rate: 100, env: new Float32Array(6000) }, { bpm: 120, beatsPerBar: 4 });
  assert.ok(r.ok, r.reason);
  assert.strictEqual(r.boundaries.length, 0, 'silence has no boundaries');
});

test('segmenting is fast enough to re-run on every tempo change', () => {
  // The spectral pass is the slow half and is cached; this is the half that re-runs, and it has
  // to feel instant when the BPM field is nudged.
  const song = F.renderForm([
    { type: 'A', bars: 32 }, { type: 'B', bars: 32 }, { type: 'C', bars: 32 },
    { type: 'B', bars: 32 }, { type: 'D', bars: 32 }, { type: 'A', bars: 32 }
  ], 120, 4);
  const feats = F.featsOf(S, song);
  const t0 = Date.now();
  const r = S.segment(feats, { bpm: 120, beatsPerBar: 4, offset: 0 });
  const ms = Date.now() - t0;
  assert.ok(r.ok, r.reason);
  assert.ok(ms < 2000, `${ms} ms for ${(song.duration / 60).toFixed(1)} minutes`);
  console.log(`     (${(song.duration / 60).toFixed(1)} min of audio, ${r.beats} beats, segmented in ${ms} ms)`);
});

/* ============================================== the cut, against real measurements */

/* These are measured, not invented: the 45 pairwise distances between the ten segments of a
   5:29 metal track, as clusterSegments computes them. They are here because no synthetic song
   reproduces this shape. Two attempts were made - varying each take, and putting a shared
   instrumental bed under every passage - and both left the step between same-passage and
   different-passage pairs at 20x or more, because a fixture's passages are only as alike as its
   voices, and four deliberately contrasting voices are nothing like one band playing two riffs.

   On a real recording the distances are a continuum from 0.004 to 0.30 with a step of 2.0x where
   the repeats end. The first version of chooseCut demanded 3x and reported no repetition at all
   on this track: ten segments, ten types. That is the bug these numbers exist to catch. */
const REAL_TRACK_DISTANCES = [
  0.0040, 0.0047, 0.0080, 0.0161, 0.0238, 0.0269, 0.0285, 0.0290, 0.0298, 0.0300,
  0.0332, 0.0333, 0.0347, 0.0349, 0.0366, 0.0371, 0.0378, 0.0395, 0.0482, 0.0493,
  0.0658, 0.0666, 0.0674, 0.0683, 0.0688, 0.0689, 0.0697, 0.0709, 0.0722, 0.0729,
  0.0731, 0.0771, 0.0776, 0.0838, 0.0842, 0.0940, 0.1298, 0.1577, 0.1714, 0.2096,
  0.2110, 0.2152, 0.2385, 0.2401, 0.3027
];

test('the cut finds the repeats in a real recording', () => {
  const cut = S._internal.chooseCut(REAL_TRACK_DISTANCES);
  assert.ok(cut > 0, 'no cut taken: the track would report every segment as its own type');
  // the three closest pairs are the repeats; the fourth distance up is a different passage
  assert.ok(cut > 0.0080, `cut ${cut.toFixed(4)} is below the third repeat at 0.0080`);
  assert.ok(cut < 0.0161, `cut ${cut.toFixed(4)} reaches past the first unrelated pair at 0.0161`);
});

test('the cut is not taken on a continuum with no repetition', () => {
  // evenly spread distances: a song where every passage differs from every other by about as
  // much. Nothing here is a repeat and nothing should be merged.
  const even = [];
  for (let i = 0; i < 45; i += 1) even.push(0.08 + i * 0.005);
  assert.strictEqual(S._internal.chooseCut(even), 0);
});

test('a lone close pair on a continuum is not enough to cut', () => {
  // one pair a little below the rest, but not unusually so against the median: the step test
  // might like it, the median test must not.
  const xs = [0.060];
  for (let i = 0; i < 44; i += 1) xs.push(0.075 + i * 0.004);
  assert.strictEqual(S._internal.chooseCut(xs), 0);
});

test('a song that never changes is one passage, not many', () => {
  const tiny = [];
  for (let i = 0; i < 20; i += 1) tiny.push(0.0001 + i * 0.0001);
  assert.ok(S._internal.chooseCut(tiny) > 0, 'everything should merge');
});

test('the cut still handles the synthetic cliff it was first built for', () => {
  const synthetic = [0.0002, 0.0006, 0.0014, 0.0035, 0.2017, 0.2042, 0.2184, 0.2192, 0.2201,
    0.3017, 0.3033, 0.3105, 0.3118, 0.3338, 0.3350];
  const cut = S._internal.chooseCut(synthetic);
  assert.ok(cut > 0.0035 && cut < 0.2017, `cut ${cut} should sit in the gap`);
});

/* ================================================== descriptors across the song */

test('descriptors stretch across the song rather than against an absolute scale', () => {
  // Measured on the real track: every segment ran 2.7 to 4.6 onsets a beat, so a fixed "2 a beat
  // is dense" put all ten at 1.00 and the descriptor carried no information at all.
  const segs = [2.7, 3.1, 3.5, 4.0, 4.6].map((v) => ({ densityRaw: v }));
  S._internal.spread(segs, 'density', 0.25);
  assert.strictEqual(segs[0].density, 0);
  assert.strictEqual(segs[4].density, 1);
  assert.ok(segs[2].density > 0.3 && segs[2].density < 0.7, `middle at ${segs[2].density}`);
});

test('a descriptor that barely varies is not magnified into a full sweep', () => {
  const segs = [3.0, 3.02, 3.05, 3.1].map((v) => ({ densityRaw: v }));
  S._internal.spread(segs, 'density', 0.25);
  assert.ok(segs.every((s) => s.density === 0.5), segs.map((s) => s.density).join(','));
});

console.log(`\n${passed} passed${process.exitCode ? ', some FAILED' : ''}`);
