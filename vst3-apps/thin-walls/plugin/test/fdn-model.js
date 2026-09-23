// An independent model of the room network, to check the steady-state gain
// formula G = fs RT / (13.8 L_total) against a plain simulation.
const N = 16, fs = 48000;
function isPrime(v) { if (v < 2) return false; for (let i = 2; i * i <= v; i++) if (v % i === 0) return false; return true; }
function nextPrime(n) { while (!isPrime(n)) n++; return n; }
const SGN_SRC = [1,-1, 1, 1,-1, 1,-1,-1, 1,-1,-1, 1, 1,-1, 1,-1];
const SGN_MIX = [1, 1,-1, 1,-1,-1, 1, 1,-1, 1, 1,-1, 1,-1,-1,-1];
const SGN_OUT = [1,-1,-1, 1, 1, 1,-1, 1,-1,-1, 1,-1, 1, 1,-1, 1];

function run(tau, RT, mixSign, outSign, seconds) {
  const len = [];
  for (let i = 0; i < N; i++) len.push(nextPrime(Math.max(32, Math.round(tau * fs * (0.55 + 0.95 * i / (N - 1))))));
  const Ltot = len.reduce((a, b) => a + b, 0);
  const g = len.map(L => Math.pow(10, -60 * L / (fs * RT) / 20));
  const buf = len.map(L => new Float64Array(L)), w = new Array(N).fill(0);
  let ey = 0, elines = 0;
  const total = Math.round(seconds * fs);
  for (let n = 0; n < total; n++) {
    const inj = n === 0 ? 1 : 0;
    const out = new Array(N);
    let y = 0;
    for (let k = 0; k < N; k++) { out[k] = g[k] * buf[k][w[k]]; y += (outSign ? SGN_OUT[k] : 1) * out[k]; }
    y *= 0.25; ey += y * y;
    // Hadamard
    const v = out.slice();
    for (let l = 1; l < N; l <<= 1) for (let a = 0; a < N; a += l << 1) for (let b = a; b < a + l; b++) { const p = v[b], q = v[b + l]; v[b] = p + q; v[b + l] = p - q; }
    for (let k = 0; k < N; k++) { buf[k][w[k]] = 0.25 * (mixSign ? SGN_MIX[k] : 1) * v[k] + 0.25 * SGN_SRC[k] * inj; w[k] = (w[k] + 1) % len[k]; }
  }
  // stored energy check: sum of squares in all lines at the end (should be ~0)
  const Gf = fs * RT / (13.8 * Ltot);
  return { G: ey, Gf, Ltot };
}
for (const [tau, RT] of [[0.00806, 0.5], [0.00806, 1.77], [0.00806, 3.42], [0.00806, 0.09]]) {
  const a = run(tau, RT, false, false, RT * 3 + 1);
  const b = run(tau, RT, true, true, RT * 3 + 1);
  console.log(`RT ${RT}: plain Hadamard G=${a.G.toFixed(3)}  signed G=${b.G.toFixed(3)}  formula ${a.Gf.toFixed(3)}  (ratio signed/formula ${(b.G / a.Gf).toFixed(3)})`);
}

// decay rate check: T from the EDC of y^2 between -5 and -25 dB
function rtOf(tau, RT) {
  const N = 16;
  const len = [];
  for (let i = 0; i < N; i++) len.push(nextPrime(Math.max(32, Math.round(tau * fs * (0.55 + 0.95 * i / (N - 1))))));
  const g = len.map(L => Math.pow(10, -60 * L / (fs * RT) / 20));
  const buf = len.map(L => new Float64Array(L)), w = new Array(N).fill(0);
  const total = Math.round((RT * 2 + 0.5) * fs);
  const ys = new Float64Array(total);
  for (let n = 0; n < total; n++) {
    const inj = n === 0 ? 1 : 0;
    const out = new Array(N); let y = 0;
    for (let k = 0; k < N; k++) { out[k] = g[k] * buf[k][w[k]]; y += SGN_OUT[k] * out[k]; }
    ys[n] = 0.25 * y;
    const v = out.slice();
    for (let l = 1; l < N; l <<= 1) for (let a = 0; a < N; a += l << 1) for (let b = a; b < a + l; b++) { const p = v[b], q = v[b + l]; v[b] = p + q; v[b + l] = p - q; }
    for (let k = 0; k < N; k++) { buf[k][w[k]] = 0.25 * SGN_MIX[k] * v[k] + 0.25 * SGN_SRC[k] * inj; w[k] = (w[k] + 1) % len[k]; }
  }
  const edc = new Float64Array(total); let acc = 0;
  for (let i = total - 1; i >= 0; i--) { acc += ys[i] * ys[i]; edc[i] = acc; }
  const top = 10 * Math.log10(edc[Math.round(0.05 * fs)]);
  let i5 = -1, i25 = -1;
  for (let i = Math.round(0.05 * fs); i < total; i++) { const d = 10 * Math.log10(edc[i]) - top; if (i5 < 0 && d <= -5) i5 = i; if (d <= -25) { i25 = i; break; } }
  return 3 * (i25 - i5) / fs;
}
for (const RT of [0.5, 1.77, 0.09]) console.log(`design RT ${RT}: model T20*3 = ${rtOf(0.00806, RT).toFixed(3)}`);
