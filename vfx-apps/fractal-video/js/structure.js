/* Structure: where a song changes, and which passages are the same passage.

   No learning, no model. This is the classical MIR pipeline (Foote 2000, Cooper & Foote 2003),
   which is enough here for one reason: the output has to land on bars. Section lengths in this
   project are whole beats, so every boundary gets quantised to a bar and then nudged onto the
   4-bar grid the music is almost certainly built on. A boundary estimate 300 ms out snaps to
   exactly the right bar. The musical grid does the error correction that a learned model would
   otherwise have to do.

   Two passes, deliberately split:

     spectral(samples, sr)  runs over the decoded PCM, once, inside audio.js's analyse(). It is
                            the only part that needs the audio, and the buffer is dropped straight
                            after, so this has to happen there or not at all. It knows nothing
                            about tempo: it writes a plain 20 Hz grid of log-mel and chroma.

     segment(feats, grid)   runs on those cached frames whenever the tempo changes. Beat-syncing
                            happens here, so correcting the BPM re-segments in milliseconds
                            instead of decoding the file again. That split is the whole reason
                            the two are not one function.

   What it gives you: boundaries on the bar grid, a type letter per segment (the segments that are
   the same passage share a letter), and four descriptors per segment. What it does NOT give you is
   names. "This is the chorus" needs priors about song form that this pipeline has no access to;
   calling the loudest recurring type the chorus is a guess that is wrong often enough that the
   caller should make it, visibly, rather than have this file pretend to know.
*/
(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.FVStructure = factory();
  }
})(typeof self !== 'undefined' ? self : globalThis, function () {
  'use strict';

  const SPEC_RATE = 20;    // feature frames per second (50 ms hop)
  const FFT_SIZE = 2048;   // 93 ms at 22050 Hz; bins are 10.8 Hz apart
  const N_MEL = 24;
  const N_CHROMA = 12;
  const N_MFCC = 13;       // coefficients 1..13; 0 is loudness and is dropped on purpose
  const VERSION = 1;

  const clamp = (x, a, b) => Math.min(b, Math.max(a, x));

  /* ===================================================================== FFT == */

  // Iterative in-place radix-2. Tables are built once per size and reused for every frame.
  const twiddleCache = {};
  function tables(n) {
    if (twiddleCache[n]) return twiddleCache[n];
    const levels = Math.log2(n);
    if (levels !== Math.floor(levels)) throw new Error('FFT size must be a power of two');
    const cos = new Float64Array(n / 2);
    const sin = new Float64Array(n / 2);
    for (let i = 0; i < n / 2; i += 1) {
      cos[i] = Math.cos(-2 * Math.PI * i / n);
      sin[i] = Math.sin(-2 * Math.PI * i / n);
    }
    const rev = new Uint32Array(n);
    for (let i = 0; i < n; i += 1) {
      let x = i;
      let r = 0;
      for (let b = 0; b < levels; b += 1) { r = (r << 1) | (x & 1); x >>= 1; }
      rev[i] = r;
    }
    twiddleCache[n] = { cos, sin, rev, levels };
    return twiddleCache[n];
  }

  // re/im are modified in place.
  function fft(re, im) {
    const n = re.length;
    const t = tables(n);
    for (let i = 0; i < n; i += 1) {
      const j = t.rev[i];
      if (j > i) {
        let tmp = re[i]; re[i] = re[j]; re[j] = tmp;
        tmp = im[i]; im[i] = im[j]; im[j] = tmp;
      }
    }
    for (let size = 2; size <= n; size <<= 1) {
      const half = size >> 1;
      const step = n / size;
      for (let i = 0; i < n; i += size) {
        for (let j = i, k = 0; j < i + half; j += 1, k += step) {
          const l = j + half;
          const tre = re[l] * t.cos[k] - im[l] * t.sin[k];
          const tim = re[l] * t.sin[k] + im[l] * t.cos[k];
          re[l] = re[j] - tre;
          im[l] = im[j] - tim;
          re[j] += tre;
          im[j] += tim;
        }
      }
    }
  }

  /* ============================================================ filterbanks == */

  const hzToMel = (f) => 2595 * Math.log10(1 + f / 700);
  const melToHz = (m) => 700 * (Math.pow(10, m / 2595) - 1);

  // Triangular mel bands over the usable spectrum. Returns, per band, the bin range and weights,
  // so the per-frame loop touches only the bins that matter.
  function melBank(sr, fftSize, bands) {
    const nyq = sr / 2;
    const hi = Math.min(10000, nyq * 0.97);
    const lo = 40;
    const binHz = sr / fftSize;
    const edges = [];
    for (let i = 0; i < bands + 2; i += 1) {
      edges.push(melToHz(hzToMel(lo) + (hzToMel(hi) - hzToMel(lo)) * i / (bands + 1)));
    }
    const out = [];
    for (let b = 0; b < bands; b += 1) {
      const f0 = edges[b];
      const f1 = edges[b + 1];
      const f2 = edges[b + 2];
      const from = Math.max(1, Math.ceil(f0 / binHz));
      const to = Math.min(fftSize / 2 - 1, Math.floor(f2 / binHz));
      const w = [];
      let sum = 0;
      for (let k = from; k <= to; k += 1) {
        const f = k * binHz;
        const v = f <= f1 ? (f - f0) / Math.max(1e-9, f1 - f0) : (f2 - f) / Math.max(1e-9, f2 - f1);
        const g = Math.max(0, v);
        w.push(g);
        sum += g;
      }
      // Normalise each band to unit sum so a band's value is a mean, not a bin count: without
      // this the wide high bands read far louder than the narrow low ones.
      const inv = sum > 0 ? 1 / sum : 0;
      out.push({ from, to, w: Float32Array.from(w, (g) => g * inv) });
    }
    return out;
  }

  // Which pitch class each bin belongs to, over C2..B6. -1 means "ignore this bin": below C2 is
  // mostly kick drum and above B6 is mostly cymbal, and neither carries harmony.
  function chromaMap(sr, fftSize) {
    const binHz = sr / fftSize;
    const map = new Int8Array(fftSize / 2);
    map.fill(-1);
    const lo = 65.4;   // C2
    const hi = 1975.5; // B6
    for (let k = 1; k < fftSize / 2; k += 1) {
      const f = k * binHz;
      if (f < lo || f > hi) continue;
      const midi = 69 + 12 * Math.log2(f / 440);
      map[k] = ((Math.round(midi) % 12) + 12) % 12;
    }
    return map;
  }

  /* =============================================================== pass one == */

  /* Log-mel and chroma on a plain 20 Hz grid.

     `samples` is mono Float32Array at `sr`. Runs synchronously in slices: pass onSlice to yield
     to the UI between them (audio.js already has that pattern for its own loop).

     Returns Float32Arrays flattened frame-major, which is what IndexedDB stores cheaply and what
     the second pass wants to walk. */
  function spectral(samples, sr, opts) {
    const o = opts || {};
    const hop = Math.max(1, Math.round(sr / SPEC_RATE));
    const n = samples.length;
    const frames = Math.max(1, Math.floor((n - FFT_SIZE) / hop) + 1);
    const mel = new Float32Array(frames * N_MEL);
    const chroma = new Float32Array(frames * N_CHROMA);
    const bank = melBank(sr, FFT_SIZE, N_MEL);
    const cmap = chromaMap(sr, FFT_SIZE);
    const win = new Float64Array(FFT_SIZE);
    for (let i = 0; i < FFT_SIZE; i += 1) win[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / FFT_SIZE);
    const re = new Float64Array(FFT_SIZE);
    const im = new Float64Array(FFT_SIZE);
    const power = new Float64Array(FFT_SIZE / 2);

    for (let f = 0; f < frames; f += 1) {
      const start = f * hop;
      for (let i = 0; i < FFT_SIZE; i += 1) {
        const s = start + i;
        re[i] = (s < n ? samples[s] : 0) * win[i];
        im[i] = 0;
      }
      fft(re, im);
      for (let k = 0; k < FFT_SIZE / 2; k += 1) power[k] = re[k] * re[k] + im[k] * im[k];

      for (let b = 0; b < N_MEL; b += 1) {
        const band = bank[b];
        let s = 0;
        for (let k = band.from, j = 0; k <= band.to; k += 1, j += 1) s += power[k] * band.w[j];
        // log of power: compresses the 60 dB a mix spans into something a distance behaves on
        mel[f * N_MEL + b] = Math.log(1e-10 + s);
      }
      const c = f * N_CHROMA;
      for (let k = 1; k < FFT_SIZE / 2; k += 1) {
        const pc = cmap[k];
        if (pc >= 0) chroma[c + pc] += Math.sqrt(power[k]);
      }
      if (o.onSlice && (f % 400 === 399 || f === frames - 1)) o.onSlice((f + 1) / frames);
    }
    return { version: VERSION, rate: SPEC_RATE, frames, nMel: N_MEL, nChroma: N_CHROMA, mel, chroma };
  }

  /* =============================================================== pass two == */

  // DCT-II of a log-mel frame, coefficients 1..count. Dropping coefficient 0 drops loudness, so
  // two passages of the same instrumentation match whether or not one is the louder mix.
  function mfcc(melFrame, nMel, count, out) {
    for (let i = 1; i <= count; i += 1) {
      let s = 0;
      for (let b = 0; b < nMel; b += 1) s += melFrame[b] * Math.cos(Math.PI * i * (b + 0.5) / nMel);
      out[i - 1] = s * Math.sqrt(2 / nMel);
    }
    return out;
  }

  function l2(v, from, to) {
    let s = 0;
    for (let i = from; i < to; i += 1) s += v[i] * v[i];
    const inv = s > 1e-12 ? 1 / Math.sqrt(s) : 0;
    for (let i = from; i < to; i += 1) v[i] *= inv;
  }

  /* Per-beat feature vectors.

     Each beat gets the mean of the frames inside it, then timbre and harmony are L2-normalised
     separately so neither can drown the other, whatever their natural scale. Beats are the unit
     rather than frames because the answer has to be in beats; averaging here also throws away the
     within-beat drum pattern, which is noise at this scale. */
  function beatVectors(spec, beats) {
    const nb = beats.length - 1;
    const dim = N_MFCC + N_CHROMA;
    const vecs = [];
    const acc = new Float64Array(spec.nMel);
    const cep = new Float64Array(N_MFCC);
    for (let i = 0; i < nb; i += 1) {
      const f0 = clamp(Math.round(beats[i] * spec.rate), 0, spec.frames - 1);
      const f1 = clamp(Math.round(beats[i + 1] * spec.rate), f0 + 1, spec.frames);
      acc.fill(0);
      const v = new Float32Array(dim);
      let count = 0;
      for (let f = f0; f < f1; f += 1) {
        for (let b = 0; b < spec.nMel; b += 1) acc[b] += spec.mel[f * spec.nMel + b];
        for (let c = 0; c < N_CHROMA; c += 1) v[N_MFCC + c] += spec.chroma[f * N_CHROMA + c];
        count += 1;
      }
      if (count) for (let b = 0; b < spec.nMel; b += 1) acc[b] /= count;
      mfcc(acc, spec.nMel, N_MFCC, cep);
      for (let k = 0; k < N_MFCC; k += 1) v[k] = cep[k];
      l2(v, 0, N_MFCC);
      l2(v, N_MFCC, dim);
      vecs.push(v);
    }
    return vecs;
  }

  // Moving average over a few beats. One beat of drums is noisy; a section is not.
  function smoothVectors(vecs, half) {
    if (half < 1) return vecs;
    const dim = vecs[0] ? vecs[0].length : 0;
    return vecs.map((_, i) => {
      const out = new Float32Array(dim);
      let count = 0;
      for (let j = Math.max(0, i - half); j <= Math.min(vecs.length - 1, i + half); j += 1) {
        for (let k = 0; k < dim; k += 1) out[k] += vecs[j][k];
        count += 1;
      }
      for (let k = 0; k < dim; k += 1) out[k] /= count || 1;
      l2(out, 0, N_MFCC);
      l2(out, N_MFCC, dim);
      return out;
    });
  }

  // Cosine self-similarity over one slice of the vector (timbre or harmony), as a flat N*N matrix.
  // Both halves are already unit length, so the cosine is just a dot product.
  function ssm(vecs, from, to) {
    const n = vecs.length;
    const S = new Float32Array(n * n);
    for (let i = 0; i < n; i += 1) {
      for (let j = i; j < n; j += 1) {
        let s = 0;
        for (let k = from; k < to; k += 1) s += vecs[i][k] * vecs[j][k];
        S[i * n + j] = s;
        S[j * n + i] = s;
      }
    }
    return S;
  }

  /* Foote's checkerboard novelty.

     Slide a 2L x 2L kernel down the diagonal. The kernel is +1 where both indices sit on the same
     side of the centre and -1 where they straddle it, tapered by a Gaussian. So it answers one
     question per beat: does everything before here look like everything before here, everything
     after like everything after, and the two unlike each other? That is what a section boundary
     is. A passage that merely gets louder scores nothing, because loudness left the vectors with
     MFCC coefficient 0. */
  function novelty(S, n, L) {
    const size = 2 * L;
    const K = new Float64Array(size * size);
    const sigma = L / 2;
    let pos = 0;
    for (let i = 0; i < size; i += 1) {
      for (let j = 0; j < size; j += 1) {
        const di = i - L + 0.5;
        const dj = j - L + 0.5;
        const g = Math.exp(-(di * di + dj * dj) / (2 * sigma * sigma));
        const same = (i < L) === (j < L);
        K[i * size + j] = (same ? 1 : -1) * g;
        if (same) pos += g;
      }
    }
    // Dividing by the positive weight sum is what makes the score mean the same thing in every
    // song. At a perfect boundary the same-side cells are ~1 and the crossing cells ~0, so the
    // score approaches 1; where nothing changes every cell is alike and the quadrants cancel to 0.
    // Without this the curve can only be read relative to its own maximum, and a song with no
    // structure has its loudest noise bump stretched up to look exactly like a boundary.
    // Measured: true boundaries 0.07..0.27, noise below 0.016, a song with no structure below
    // 0.003. Hence ABS_THRESH.
    const inv = pos > 0 ? 1 / pos : 1;
    const out = new Float64Array(n);
    for (let t = 0; t < n; t += 1) {
      let s = 0;
      for (let i = 0; i < size; i += 1) {
        const si = t - L + i;
        if (si < 0 || si >= n) continue;
        for (let j = 0; j < size; j += 1) {
          const sj = t - L + j;
          if (sj < 0 || sj >= n) continue;
          s += K[i * size + j] * S[si * n + sj];
        }
      }
      out[t] = s * inv;
    }
    // The kernel is truncated near both ends, so those scores are not comparable: blank them
    // rather than let a half-kernel masquerade as a peak.
    for (let t = 0; t < Math.min(L, n); t += 1) out[t] = 0;
    for (let t = Math.max(0, n - L); t < n; t += 1) out[t] = 0;
    return out;
  }

  /* Subtract a local baseline, and deliberately do NOT rescale.

     A passage that is internally varied - a solo, a breakdown - sits on a raised novelty
     baseline, and subtracting the neighbourhood mean stops that raised floor reading as a row of
     boundaries. For homogeneous music the baseline is near zero (measured: median 0.0008) so this
     changes nothing there.

     The earlier version also divided by the song's maximum. That is the thing to avoid: it throws
     away the absolute scale that novelty() just established, and with it the only evidence that
     distinguishes a song with no structure from a song with structure. */
  function detrend(nv, half) {
    const n = nv.length;
    const out = new Float64Array(n);
    for (let t = 0; t < n; t += 1) {
      let sum = 0;
      let count = 0;
      for (let j = Math.max(0, t - half); j <= Math.min(n - 1, t + half); j += 1) { sum += nv[j]; count += 1; }
      out[t] = Math.max(0, nv[t] - sum / (count || 1));
    }
    return out;
  }

  // Local maxima above `thresh`, no two within `minGap`, strongest first.
  function peaks(nv, minGap, thresh) {
    const found = [];
    for (let t = 1; t < nv.length - 1; t += 1) {
      if (nv[t] > thresh && nv[t] >= nv[t - 1] && nv[t] >= nv[t + 1]) found.push({ t, v: nv[t] });
    }
    found.sort((a, b) => b.v - a.v);
    const kept = [];
    found.forEach((p) => {
      if (kept.every((q) => Math.abs(q.t - p.t) >= minGap)) kept.push(p);
    });
    kept.sort((a, b) => a.t - b.t);
    return kept;
  }

  /* Quantise to bars, then onto the 4-bar grid.

     This is the step that makes the whole thing usable. First every boundary becomes a whole bar.
     Then: pop, rock and metal put their boundaries on multiples of 4 bars almost without
     exception, so of the four possible phases we take whichever already explains the most
     boundaries, and pull the stragglers onto it — but only by a single bar. A boundary two bars
     off the grid is more likely to be a real odd-length section than a detection error, and
     forcing it would invent structure the song does not have. */
  function snapToBars(beatIdx, beatsPerBar, totalBars) {
    const bars = [];
    beatIdx.forEach((p) => {
      const bar = clamp(Math.round(p.t / beatsPerBar), 1, totalBars - 1);
      if (!bars.some((b) => b.bar === bar)) bars.push({ bar, v: p.v, beat: p.t });
    });
    bars.sort((a, b) => a.bar - b.bar);
    if (bars.length < 2) return bars;

    let best = { phase: 0, score: -1 };
    for (let phase = 0; phase < 4; phase += 1) {
      let score = 0;
      bars.forEach((b) => {
        const off = ((b.bar - phase) % 4 + 4) % 4;
        // on the grid is worth most, one bar either side still counts, weighted by peak strength
        if (off === 0) score += 2 * b.v;
        else if (off === 1 || off === 3) score += 0.5 * b.v;
      });
      if (score > best.score) best = { phase, score };
    }
    const out = [];
    bars.forEach((b) => {
      const off = ((b.bar - best.phase) % 4 + 4) % 4;
      let bar = b.bar;
      if (off === 1) bar = b.bar - 1;
      else if (off === 3) bar = b.bar + 1;
      bar = clamp(bar, 1, totalBars - 1);
      if (!out.some((x) => x.bar === bar)) out.push({ bar, v: b.v, beat: b.beat, moved: bar !== b.bar });
    });
    out.sort((a, b) => a.bar - b.bar);
    return out;
  }

  /* The one tuned constant, and it is measured rather than chosen.

     novelty() returns an absolute score: 1 would be a passage that is wholly self-similar
     followed by another wholly unlike it. On the test song the five true boundaries score 0.13
     to 0.27 clean and 0.07 to 0.21 at 6 dB SNR, while the loudest false peak anywhere reaches
     0.016 and a song with no structure at all tops out at 0.003. This sits at the geometric
     middle of that gap, so it is roughly twice the worst noise and half the weakest real
     boundary. Callers can override it; the UI exposes it as a sensitivity. */
  const ABS_THRESH = 0.035;

  /* If the most dissimilar pair of passages in the whole song is this close, the song does not
     change: one type, and no cliff to look for. */
  const UNIFORM_EPS = 0.01;
  /* How much of a step counts as a cliff.

     Synthetic repeats are bit-identical and step by 50x to 275x. Real ones are not: a chorus comes
     back with a different vocal take, a different fill, a different moment in the mix, so on a real
     recording (measured on a 5:29 metal track) the same-passage pairs sit at 0.004 to 0.008, the
     rest form a continuum from 0.016 to 0.30, and the step between them is 2.0x. A threshold
     calibrated on the synthetic case alone rejects that and reports no repetition at all, which is
     what it did until a real song was tried. */
  const MIN_CLIFF = 1.5;
  /* A step alone is too weak a signal at 1.5x, so the cut must also land far below the typical
     distance: the pairs under it have to be unusual for this song, not merely the low tail of one
     distribution. On the real track the cut falls at 0.011 against a median of 0.067, a sixth; on
     a song where nothing repeats, no candidate cut gets near its median and none is taken. */
  const CUT_BELOW_MEDIAN = 3;
  /* A cut must leave most pairs on the "different" side. If more than this fraction of all pairs
     were the same passage, the segmentation has split one passage rather than found several. */
  const MAX_SAME_FRACTION = 0.65;

  /* Which segments are the same passage.

     Single-linkage agglomerative on the mean vector of each segment, cut at the largest gap in
     the sorted pairwise distances rather than at a fixed threshold or a percentile.

     The gap is there to be found. Two instances of the same passage differ only by performance
     and mix; two different passages differ by instrumentation or harmony, which moves the vector
     an order of magnitude further. On a song with real repetition the sorted distances therefore
     fall into two obvious groups with a cliff between them, and the cliff is the answer. A
     percentile cannot find it, because it presumes how MUCH of the song repeats, which is
     precisely what we are trying to learn.

     Measured on the test song: the four same-passage pairs sit at 0.0002 to 0.0035 and everything
     else at 0.20 upward - a factor of 58. Bury the same song in noise until the signal-to-noise
     ratio is negative and the absolute numbers all collapse towards zero, but the RATIO holds:
     93x at 16 dB, 275x at 6 dB, 145x at -1 dB. That is why the cut is chosen by ratio and why it
     carries no absolute bounds. An earlier version held the cut inside a fixed window and got
     both failure modes at once - every segment its own type on a clean mix, every segment the
     same type on a noisy one - because a window in absolute units is exactly the assumption a
     scale-free method must not make.

     If there is no cliff worth the name, the song has told us nothing about repetition, and we say
     so by making every segment its own type rather than inventing groups. */
  /* Where to cut a sorted list of pairwise distances, or 0 for "nowhere".

     Separated from the clustering so it can be tested directly against distance lists measured
     from real recordings, which is the only way to check it against the regime that matters: a
     synthetic song cannot be made to produce one. Two attempts at that - varying each take, then
     putting a shared instrumental bed under every passage - both left the step at 20x or more,
     because a fixture's passages are only as alike as its voices and its voices are far less
     alike than one band playing two riffs.

     Returns the threshold; callers merge every pair at or below it. */
  function chooseCut(sorted) {
    if (!sorted.length) return 0;
    if (sorted[sorted.length - 1] < UNIFORM_EPS) return UNIFORM_EPS; // nothing here changes at all
    const eps = sorted[sorted.length - 1] * 1e-3;
    const median = sorted[Math.floor(sorted.length / 2)];
    const last = Math.max(0, Math.floor(sorted.length * MAX_SAME_FRACTION) - 1);
    const cuts = [];
    for (let i = 0; i <= last && i < sorted.length - 1; i += 1) {
      const lo = sorted[i] + eps;
      const hi = sorted[i + 1] + eps;
      const thresh = Math.sqrt(lo * hi);
      // Both tests, because either alone is fooled: a step without the median test fires on
      // ripple halfway up a continuum, and the median test without a step has no idea where to
      // put the cut.
      if (thresh * CUT_BELOW_MEDIAN > median) continue;
      cuts.push({ thresh, ratio: hi / lo });
    }
    cuts.sort((a, b) => b.ratio - a.ratio);
    return cuts.length && cuts[0].ratio >= MIN_CLIFF ? cuts[0].thresh : 0;
  }

  function clusterSegments(segs, vecs, maxTypes) {
    const n = segs.length;
    if (n === 0) return 0;
    const means = segs.map((s) => {
      const dim = vecs[0] ? vecs[0].length : 0;
      const m = new Float32Array(dim);
      let count = 0;
      for (let b = s.beat0; b < Math.min(s.beat1, vecs.length); b += 1) {
        for (let k = 0; k < dim; k += 1) m[k] += vecs[b][k];
        count += 1;
      }
      if (count) for (let k = 0; k < dim; k += 1) m[k] /= count;
      l2(m, 0, N_MFCC);
      l2(m, N_MFCC, dim);
      return m;
    });
    const dist = (a, b) => {
      let s = 0;
      for (let k = 0; k < means[a].length; k += 1) s += means[a][k] * means[b][k];
      return 1 - s / 2; // both halves unit length, so the dot runs 0..2
    };
    const pairs = [];
    for (let i = 0; i < n; i += 1) for (let j = i + 1; j < n; j += 1) pairs.push(dist(i, j));
    pairs.sort((a, b) => a - b);

    const run = (thresh) => {
      const owner = segs.map((_, i) => i);
      const find = (i) => { while (owner[i] !== i) i = owner[i] = owner[owner[i]]; return i; };
      for (let i = 0; i < n; i += 1) {
        for (let j = i + 1; j < n; j += 1) {
          if (dist(i, j) <= thresh) {
            const a = find(i);
            const b = find(j);
            if (a !== b) owner[a] = b;
          }
        }
      }
      return segs.map((_, i) => find(i));
    };

    let groups = segs.map((_, i) => i); // the default: no cliff found, so nothing is grouped
    let chosen = 0;

    if (pairs.length === 0) {
      // one segment: nothing to compare it with
      groups = [0];
    } else if (pairs[pairs.length - 1] < UNIFORM_EPS) {
      // even the least alike pair is nearly identical: the song is one passage throughout
      groups = segs.map(() => 0);
      chosen = UNIFORM_EPS;
    } else {
      // Candidate cuts: the geometric midpoint of every gap, scored by ratio. The epsilon is a
      // fraction of the song's own spread, not a constant, so a pair of bit-identical segments
      // cannot produce an infinite ratio and win on a rounding artefact.
      const cut = chooseCut(pairs);
      if (cut > 0) {
        const g = run(cut);
        if (new Set(g).size <= maxTypes) { groups = g; chosen = cut; }
      }
    }

    // Letter the types in order of first appearance, so A is always the opening passage.
    const letter = {};
    let next = 0;
    segs.forEach((s, i) => {
      const g = groups[i];
      if (!(g in letter)) { letter[g] = String.fromCharCode(65 + Math.min(25, next)); next += 1; }
      s.type = letter[g];
    });
    return chosen;
  }

  /* Four descriptors per segment, each 0..1.

     These come from the cheap 100 Hz tracks audio.js already has, except brightness which needs
     the mel frames. They are what a caller maps to scene and look choices; this file computes
     them and takes no view on what they should mean. */
  function describe(seg, feats, spec, beatTimes) {
    const t0 = beatTimes[seg.beat0] || 0;
    const t1 = beatTimes[Math.min(seg.beat1, beatTimes.length - 1)] || t0;
    const span = Math.max(1e-6, t1 - t0);
    const mean = (track, rate) => {
      if (!track || !track.length) return 0;
      const a = clamp(Math.floor(t0 * rate), 0, track.length - 1);
      const b = clamp(Math.ceil(t1 * rate), a + 1, track.length);
      let s = 0;
      for (let i = a; i < b; i += 1) s += track[i];
      return s / (b - a);
    };
    const rate = feats.rate || 100;
    const low = mean(feats.low, rate);
    const mid = mean(feats.mid, rate);
    const high = mean(feats.high, rate);
    const total = low + mid + high;

    // brightness: centre of gravity across the mel bands, as a fraction of the band count
    let bright = 0;
    if (spec && spec.frames) {
      const f0 = clamp(Math.floor(t0 * spec.rate), 0, spec.frames - 1);
      const f1 = clamp(Math.ceil(t1 * spec.rate), f0 + 1, spec.frames);
      let num = 0;
      let den = 0;
      for (let f = f0; f < f1; f += 1) {
        for (let b = 0; b < spec.nMel; b += 1) {
          // back to linear from the stored log, floored so near-silence does not dominate
          const e = Math.max(0, Math.exp(spec.mel[f * spec.nMel + b]) - 1e-10);
          num += e * b;
          den += e;
        }
      }
      bright = den > 0 ? num / den / (spec.nMel - 1) : 0;
    }

    let hits = 0;
    if (feats.onsetTimes) for (let i = 0; i < feats.onsetTimes.length; i += 1) {
      const t = feats.onsetTimes[i];
      if (t >= t0 && t < t1) hits += 1;
    }
    const beats = Math.max(1, seg.beat1 - seg.beat0);

    seg.loudness = clamp(mean(feats.env, rate), 0, 1);
    seg.bass = total > 0 ? clamp(low / total, 0, 1) : 0;
    seg.seconds = span;
    // Raw, in their own units. spread() turns these two into 0..1 across the song afterwards.
    seg.brightnessRaw = clamp(bright, 0, 1);
    seg.densityRaw = hits / beats;
    seg.brightness = seg.brightnessRaw;
    seg.density = clamp(seg.densityRaw / 2, 0, 1);
  }

  /* Stretch a descriptor across the song's own range.

     An absolute scale is the wrong instrument here. Measured on a real metal track, every segment
     ran between 2.7 and 4.6 onsets a beat, so a fixed "2 a beat is dense" put all ten segments at
     1.00 and the descriptor said nothing at all; brightness was squeezed just as flat, between
     0.05 and 0.11. These numbers exist to choose between scenes WITHIN one song, so the song is
     the right thing to measure them against.

     Below `floor` the spread is not real - a song whose segments genuinely differ by nothing
     should not have that nothing magnified into a full sweep - and everything sits at the middle. */
  function spread(segs, key, floor) {
    if (segs.length < 2) { if (segs.length) segs[0][key] = 0.5; return; }
    let lo = Infinity;
    let hi = -Infinity;
    segs.forEach((s) => { const v = s[`${key}Raw`]; if (v < lo) lo = v; if (v > hi) hi = v; });
    const range = hi - lo;
    segs.forEach((s) => {
      s[key] = range < floor ? 0.5 : clamp((s[`${key}Raw`] - lo) / range, 0, 1);
    });
  }

  /* ================================================================= public == */

  /* segment(feats, grid)

       feats  the cached analysis from audio.js: { rate, duration, env, low, mid, high,
              onsetTimes, spec } where spec is what spectral() returned.
       grid   { bpm, offset, beatsPerBar }

     Returns { ok, reason?, beats, novelty, boundaries, segments, bars } with segments carrying
     { bar0, bar1, bars, beat0, beat1, type, loudness, brightness, density, bass, sharpness }.
     Nothing here touches the project: turning segments into sections is the caller's business,
     which keeps this testable and keeps a bad guess out of the undo history. */
  function segment(feats, grid, opts) {
    const o = opts || {};
    const spec = feats.spec;
    if (!spec || !spec.frames || !spec.mel) return { ok: false, reason: 'no-spectral' };
    const bpm = Number(grid.bpm) || 120;
    const beatsPerBar = Math.max(1, Math.round(grid.beatsPerBar || 4));
    const offset = Number(grid.offset) || 0;
    const secPerBeat = 60 / bpm;
    const duration = feats.duration || spec.frames / spec.rate;

    // Beat times from the grid the user is actually working to, trimmed to the audio.
    const beatTimes = [];
    for (let t = offset, i = 0; t <= duration + 1e-6 && i < 20000; i += 1, t = offset + i * secPerBeat) {
      if (t >= -1e-6) beatTimes.push(t);
    }
    const nBeats = beatTimes.length - 1;
    const totalBars = Math.floor(nBeats / beatsPerBar);
    if (nBeats < beatsPerBar * 8 || totalBars < 4) return { ok: false, reason: 'too-short' };

    let vecs = beatVectors(spec, beatTimes);
    vecs = smoothVectors(vecs, 1);

    const n = vecs.length;
    const St = ssm(vecs, 0, N_MFCC);
    const Sc = ssm(vecs, N_MFCC, N_MFCC + N_CHROMA);
    const L = clamp(2 * beatsPerBar, 6, 16);
    const nt = detrend(novelty(St, n, L), 8 * beatsPerBar);
    const nc = detrend(novelty(Sc, n, L), 8 * beatsPerBar);
    // Timbre carries more weight: instrumentation almost always changes at a boundary, while a
    // chord loop can run straight through one. Harmony is kept because when it does change it is
    // decisive, and the two together are steadier than either alone.
    const nv = new Float64Array(n);
    for (let i = 0; i < n; i += 1) nv[i] = 0.62 * nt[i] + 0.38 * nc[i];

    const minGapBars = o.minBars != null ? o.minBars : 4;
    const found = peaks(nv, minGapBars * beatsPerBar, o.thresh != null ? o.thresh : ABS_THRESH);
    const bounds = snapToBars(found, beatsPerBar, totalBars);
    // Report sharpness relative to the strongest change in this song, which is what a caller
    // wants for "how long should the transition into this section be". The absolute score stays
    // on the boundary as `score` for anyone who needs to compare across songs.
    const peakMax = bounds.reduce((m, b) => Math.max(m, b.v), 0);

    // Segments between boundaries, bar 0 to the last whole bar.
    const edges = [0].concat(bounds.map((b) => b.bar), [totalBars]);
    const segs = [];
    for (let i = 0; i < edges.length - 1; i += 1) {
      const bar0 = edges[i];
      const bar1 = edges[i + 1];
      if (bar1 <= bar0) continue;
      const at = bounds.find((b) => b.bar === bar0);
      segs.push({
        bar0, bar1, bars: bar1 - bar0,
        beat0: bar0 * beatsPerBar, beat1: bar1 * beatsPerBar,
        start: beatTimes[Math.min(bar0 * beatsPerBar, beatTimes.length - 1)],
        type: 'A',
        sharpness: at && peakMax > 0 ? at.v / peakMax : 0, // abruptness, against this song's biggest change
        score: at ? at.v : 0,                              // the absolute novelty score
        snapped: at ? !!at.moved : false
      });
    }
    const typeThresh = clusterSegments(segs, vecs, o.maxTypes || 8);
    segs.forEach((s) => describe(s, feats, spec, beatTimes));
    // The smallest spread worth stretching: a quarter of an onset a beat, and half a mel band.
    // Both are small on purpose - the floor is there to stop a song whose segments differ by
    // nothing having that nothing magnified, not to demand a dramatic difference before the
    // descriptor says anything. Measured ranges: 0.05 brightness across the synthetic song's
    // three voices, 0.24 across a real track's ten segments; half a band is 0.021.
    spread(segs, 'density', 0.25);
    spread(segs, 'brightness', 0.5 / N_MEL);

    return {
      ok: true,
      bpm, beatsPerBar, offset,
      bars: totalBars,
      beats: nBeats,
      novelty: nv,
      kernel: L,
      boundaries: bounds,
      segments: segs,
      types: new Set(segs.map((s) => s.type)).size,
      // 0 means no repetition was clear enough to group anything: every segment is its own type
      typeThreshold: typeThresh
    };
  }

  return {
    VERSION, SPEC_RATE, FFT_SIZE, N_MEL, N_CHROMA, N_MFCC, ABS_THRESH,
    spectral, segment,
    // exported for tests
    _internal: { fft, melBank, chromaMap, mfcc, ssm, novelty, detrend, peaks, snapToBars, beatVectors, chooseCut, spread }
  };
});
