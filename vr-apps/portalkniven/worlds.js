/*
 * Portalkniven — the dimensions on the other side of the door.
 *
 *   0  DIN STUE         the real room (passthrough); nothing virtual in it
 *   1  DEN GRØNNE       damp, mossy, faintly glowing veins, stalks and spores
 *   2  METALVERDENEN    riveted steel plate, brushed and stained, rust in the seams, lamps, pipes, girders
 *   3  KLIPPEVERDENEN   layered sandstone, cracks, boulders and pebbles under a dusk sky with mesas on the horizon
 *
 * Every dimension is the SAME room: the headset's room scan (walls, furniture) is drawn with that dimension's
 * surface, so the other world lines up with what you can really walk in. Surfaces are procedural and live in
 * world space (triplanar lookups in one small tileable noise texture + a bump from screen-space derivatives),
 * so they need no UVs and look right on any scanned shape.
 *
 * Any dimension can be "home" (drawn everywhere) or "seen through the door" (drawn only where stencil == 1 and
 * clipped to the far side of the door's plane), or switched off. Fog, colour grade and sky are per dimension.
 */
import * as THREE from './vendor/three.module.min.js';

const V = THREE.Vector3;
const TAU = Math.PI * 2;
const rnd = (a, b) => a + Math.random() * (b - a);

export const ORDER = { bgHome: -10, home: 0, mask: 10, fill: 20, other: 30 };
export const DIMS = [
  { key: 'stue', name: 'DIN STUE' },
  { key: 'groen', name: 'DEN GRØNNE' },
  { key: 'metal', name: 'METALVERDENEN' },
  { key: 'klippe', name: 'KLIPPEVERDENEN' },
];

/* ------------------------------------------------------------------ noise */

/** One tileable 256² texture: R = soft fbm, G = cell borders (Worley F2-F1), B = fine grain, A = a second fbm. */
function makeNoiseTexture(N = 256) {
  let seed = 20261114;
  const rand = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
  const tabs = new Map();
  const tab = (period, id) => { const k = id + '/' + period; let t = tabs.get(k); if (!t) { t = new Float32Array(period * period); for (let i = 0; i < t.length; i++) t[i] = rand(); tabs.set(k, t); } return t; };
  const vn = (x, y, period, id) => {
    const t = tab(period, id), xi = Math.floor(x), yi = Math.floor(y);
    let fx = x - xi, fy = y - yi; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    const x0 = ((xi % period) + period) % period, y0 = ((yi % period) + period) % period, x1 = (x0 + 1) % period, y1 = (y0 + 1) % period;
    const a = t[y0 * period + x0], b = t[y0 * period + x1], c = t[y1 * period + x0], d = t[y1 * period + x1];
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
  };
  const fbm = (u, v, base, oct, id) => { let s = 0, a = 0.5, p = base, n = 0; for (let o = 0; o < oct; o++) { s += a * vn(u * p, v * p, p, id + o); n += a; a *= 0.5; p *= 2; } return s / n; };
  const K = 10, px = new Float32Array(K * K), py = new Float32Array(K * K);
  for (let i = 0; i < K * K; i++) { px[i] = rand(); py[i] = rand(); }
  const worley = (u, v) => {
    const x = u * K, y = v * K, cx = Math.floor(x), cy = Math.floor(y); let f1 = 9, f2 = 9;
    for (let j = -1; j <= 1; j++) for (let i = -1; i <= 1; i++) {
      const gx = cx + i, gy = cy + j, wx = ((gx % K) + K) % K, wy = ((gy % K) + K) % K, k = wy * K + wx;
      const d = Math.hypot(gx + px[k] - x, gy + py[k] - y);
      if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) f2 = d;
    }
    return f2 - f1;
  };
  const ch = [0, 1, 2, 3].map(() => new Float32Array(N * N));
  for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) {
    const u = x / N, v = y / N, i = y * N + x;
    ch[0][i] = fbm(u, v, 4, 6, 'r'); ch[1][i] = worley(u, v); ch[2][i] = fbm(u, v, 32, 3, 'b'); ch[3][i] = fbm(u, v, 6, 5, 'a');
  }
  const data = new Uint8Array(N * N * 4);
  ch.forEach((c, k) => { let lo = 9, hi = -9; for (const v of c) { if (v < lo) lo = v; if (v > hi) hi = v; } for (let i = 0; i < c.length; i++) data[i * 4 + k] = Math.round(255 * (c[i] - lo) / (hi - lo)); });
  const t = new THREE.DataTexture(data, N, N, THREE.RGBAFormat);
  t.wrapS = t.wrapT = THREE.RepeatWrapping; t.minFilter = THREE.LinearMipmapLinearFilter; t.magFilter = THREE.LinearFilter; t.generateMipmaps = true; t.anisotropy = 4; t.needsUpdate = true;
  return t;
}

/* 3D value noise on the CPU, for shaping boulders */
const h3 = (x, y, z) => { const n = Math.sin(x * 127.1 + y * 311.7 + z * 74.7) * 43758.5453; return n - Math.floor(n); };
function n3(x, y, z) {
  const xi = Math.floor(x), yi = Math.floor(y), zi = Math.floor(z); let fx = x - xi, fy = y - yi, fz = z - zi;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
  const l = (a, b, t) => a + (b - a) * t;
  return l(l(l(h3(xi, yi, zi), h3(xi + 1, yi, zi), fx), l(h3(xi, yi + 1, zi), h3(xi + 1, yi + 1, zi), fx), fy),
           l(l(h3(xi, yi, zi + 1), h3(xi + 1, yi, zi + 1), fx), l(h3(xi, yi + 1, zi + 1), h3(xi + 1, yi + 1, zi + 1), fx), fy), fz);
}
const f3 = (x, y, z) => (0.5 * n3(x, y, z) + 0.25 * n3(x * 2.1, y * 2.1, z * 2.1) + 0.125 * n3(x * 4.3, y * 4.3, z * 4.3) + 0.0625 * n3(x * 8.9, y * 8.9, z * 8.9)) / 0.9375;

/** A boulder: a sphere pushed about by noise, with a few flat fracture faces and a flattened underside; smooth normals. */
function rockGeometry(detail, seed) {
  const g = new THREE.IcosahedronGeometry(1, detail), p = g.attributes.position, n = p.count;
  const cuts = []; for (let i = 0; i < 5; i++) { const c = new V(rnd(-1, 1), rnd(-0.3, 1), rnd(-1, 1)).normalize(); cuts.push({ n: c, d: rnd(0.72, 0.95) }); }
  const key = new Array(n), v = new V();
  for (let i = 0; i < n; i++) {
    v.fromBufferAttribute(p, i); key[i] = Math.round(v.x * 5e3) + ',' + Math.round(v.y * 5e3) + ',' + Math.round(v.z * 5e3);
    let r = 0.72 + 0.5 * f3(v.x * 1.3 + seed, v.y * 1.3 + seed * 2, v.z * 1.3) + 0.1 * (1 - Math.abs(2 * n3(v.x * 3.7 + seed, v.y * 3.7, v.z * 3.7) - 1));
    for (const c of cuts) { const dn = v.dot(c.n); if (dn > 0) r = Math.min(r, c.d / dn * 1.0 + 0.0); }        // planes cut flat faces
    v.multiplyScalar(r); if (v.y < -0.45) v.y = -0.45 - (v.y + 0.45) * 0.15;
    p.setXYZ(i, v.x, v.y, v.z);
  }
  const acc = new Map(), a = new V(), b = new V(), c = new V(), fn = new V();
  for (let i = 0; i < n; i += 3) {
    a.fromBufferAttribute(p, i); b.fromBufferAttribute(p, i + 1); c.fromBufferAttribute(p, i + 2);
    fn.copy(b).sub(a).cross(c.sub(a));
    for (let k = 0; k < 3; k++) { let s = acc.get(key[i + k]); if (!s) acc.set(key[i + k], s = new V()); s.add(fn); }
  }
  const nn = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) { const s = acc.get(key[i]); fn.copy(s).normalize(); nn[i * 3] = fn.x; nn[i * 3 + 1] = fn.y; nn[i * 3 + 2] = fn.z; }
  g.setAttribute('normal', new THREE.BufferAttribute(nn, 3));
  return g;
}

/* ------------------------------------------------------------------ shaders */

const HEAD = `
  varying vec3 vWP; varying vec3 vWN;
  uniform sampler2D uNoise; uniform float uTime; uniform vec3 uFogCol; uniform float uFogDen; uniform vec3 uGrade;
  vec4 tri(vec3 p, vec3 n, float s) { vec3 w = abs(n); w = w * w * w * w; w /= (w.x + w.y + w.z + 1e-5);
    return texture2D(uNoise, p.zy * s) * w.x + texture2D(uNoise, p.xz * s) * w.y + texture2D(uNoise, p.xy * s) * w.z; }
  vec3 wBump(vec3 pv, vec3 N, float h) { vec3 dpdx = dFdx(pv), dpdy = dFdy(pv); float dhdx = dFdx(h), dhdy = dFdy(h);
    vec3 r1 = cross(dpdy, N), r2 = cross(N, dpdx); float det = dot(dpdx, r1); vec3 g = sign(det) * (dhdx * r1 + dhdy * r2);
    return normalize(abs(det) * N - g); }`;

const SURF = {
  /* --- the green one: wet earth, moss on what faces up, standing water in the hollows, a faint lichen glow --- */
  moss: vein => `
    float n1 = tri(P, Nw, 0.09).r, n2 = tri(P, Nw, 0.7).a, n3 = tri(P, Nw, 2.8).b, n4 = tri(P, Nw, 7.0).b;
    float up = smoothstep(0.5, 0.9, Nw.y);
    float mossAmt = smoothstep(0.47, 0.66, n1 * 0.6 + n2 * 0.4 + up * 0.10);
    vec3 soil = mix(vec3(0.030, 0.024, 0.018), vec3(0.095, 0.072, 0.050), n2) * (0.75 + 0.5 * n3);
    vec3 moss = mix(vec3(0.026, 0.050, 0.016), vec3(0.085, 0.125, 0.040), n3) * (0.7 + 0.6 * n4);
    wA = mix(soil, moss, mossAmt);
    float pud = up * (1.0 - smoothstep(0.37, 0.42, n1));                // water lies where the ground is low
    float wet = up * (1.0 - smoothstep(0.40, 0.54, n1));                // and the earth round it is dark and shiny
    wA = mix(wA, wA * 0.45, wet * (1.0 - pud)); wA = mix(wA, vec3(0.010, 0.018, 0.016), pud * 0.92);
    wR = mix(mix(mix(0.62, 0.96, mossAmt), 0.3, wet * 0.8), 0.04, pud);
    wH = (1.0 - pud) * (n1 * 0.035 + mossAmt * (n3 * 0.014 + n4 * 0.004) + (1.0 - mossAmt) * n3 * 0.005)
       + pud * 0.0006 * sin(length(P.xz * 9.0) * 6.0 - uTime * 1.5) * tri(P, Nw, 0.5).r;
    float vn = tri(P + vec3(0.0, uTime * 0.003, 0.0), Nw, 0.11).a;
    float vein = (1.0 - smoothstep(0.0, 0.016, abs(vn - 0.5))) * (1.0 - pud) * (1.0 - 0.6 * mossAmt);
    wE = vec3(0.02, 0.30, 0.22) * vein * (0.5 + 0.5 * sin(uTime * 0.4 + n1 * 9.0)) * ${vein.toFixed(2)};`,
  stalk: () => 'wA = vec3(0.030, 0.050, 0.022) * (0.6 + 0.8 * tri(P, Nw, 3.0).b); wR = 0.6;',
  bud: () => 'float g = 0.6 + 0.4 * sin(uTime * 0.7 + P.x * 5.0 + P.z * 3.0); wA = vec3(0.03, 0.07, 0.05); wE = vec3(0.05, 0.50, 0.38) * g * (0.35 + 0.65 * smoothstep(-0.2, 0.9, Nw.y)); wR = 0.35;',
  bark: () => `
    float f = tri(P * vec3(1.0, 0.25, 1.0), Nw, 4.0).b, n = tri(P, Nw, 0.9).a, ms = smoothstep(0.5, 0.7, n + smoothstep(0.3, 0.9, Nw.y) * 0.3);
    wA = mix(mix(vec3(0.030, 0.022, 0.015), vec3(0.085, 0.062, 0.042), f), vec3(0.05, 0.11, 0.03) * (0.6 + 0.8 * tri(P, Nw, 6.0).b), ms);
    wR = mix(0.5, 0.95, ms); wH = f * 0.006 + ms * tri(P, Nw, 6.0).b * 0.006;`,
  cap: () => `
    float sp = tri(P, Nw, 11.0).b, under = 1.0 - smoothstep(-0.5, 0.15, Nw.y);
    wA = mix(mix(vec3(0.13, 0.10, 0.075), vec3(0.30, 0.25, 0.18), sp), vec3(0.05, 0.10, 0.08), under);
    wR = mix(0.35, 0.7, sp);
    wE = vec3(0.03, 0.40, 0.30) * under * (0.7 + 0.3 * sin(uTime * 0.6 + P.x * 9.0)) + vec3(0.004, 0.022, 0.016) * (1.0 - under);
    wH = sp * 0.0015;`,
  stem: () => 'wA = vec3(0.24, 0.22, 0.17) * (0.6 + 0.6 * tri(P, Nw, 9.0).b); wR = 0.6; wE = vec3(0.003, 0.018, 0.013);',
  blade: () => 'float t = tri(P, Nw, 5.0).b; wA = mix(vec3(0.020, 0.060, 0.015), vec3(0.085, 0.170, 0.040), t) * (0.5 + smoothstep(0.0, 0.14, P.y) * 0.9); wR = 0.5;',

  /* --- metal --- */
  plate: () => `
    vec3 an = abs(Nw);
    vec2 uv = (an.y >= an.x && an.y >= an.z) ? P.xz : (an.x > an.z ? P.zy : P.xy);
    vec2 pg = uv / vec2(1.2, 0.6);                              // plates of 1.2 x 0.6 m in a brick bond
    pg.x += 0.5 * mod(floor(pg.y), 2.0);
    vec2 cell = floor(pg), f = fract(pg);
    vec2 ed = min(f, 1.0 - f) * vec2(1.2, 0.6);                 // metres to the nearest seam on each axis
    float edge = min(ed.x, ed.y);
    float seam = 1.0 - smoothstep(0.003, 0.008, edge);
    float bevel = smoothstep(0.005, 0.028, edge);
    float pid = textureLod(uNoise, (cell + 0.5) / 37.0, 0.0).b, pid2 = textureLod(uNoise, (cell + 0.5) / 23.0 + 0.31, 0.0).a;
    float rv = length(ed - vec2(0.045));                        // a rivet in from each corner
    float rivet = 1.0 - smoothstep(0.011, 0.014, rv);
    float dome = rivet * sqrt(max(0.0, 1.0 - rv * rv / (0.014 * 0.014)));
    vec2 bu = pid > 0.5 ? uv : uv.yx;                           // each plate is brushed its own way
    float brush = texture2D(uNoise, bu * vec2(6.0, 0.08)).b * 0.6 + texture2D(uNoise, bu * vec2(17.0, 0.21)).b * 0.4;
    float grime = tri(P, Nw, 0.14).r;
    float stain = smoothstep(0.50, 0.82, grime + (1.0 - bevel) * 0.2);
    float rust = smoothstep(0.66, 0.9, tri(P, Nw, 0.33).a + (1.0 - bevel) * 0.3) * 0.9;
    vec3 steel = mix(vec3(0.40, 0.42, 0.44), vec3(0.25, 0.27, 0.30), pid2) * (0.84 + 0.32 * brush);
    vec3 rustC = mix(vec3(0.12, 0.05, 0.02), vec3(0.27, 0.12, 0.05), tri(P, Nw, 1.7).a);
    wA = mix(steel, rustC, rust); wA = mix(wA, wA * 0.4, stain * 0.55); wA = mix(wA, vec3(0.02), seam * 0.92); wA = mix(wA, steel * 1.1, rivet * 0.7);
    wM = (1.0 - rust) * (1.0 - seam);
    wR = mix(0.30 + 0.34 * brush, 0.9, max(rust, stain * 0.7)); wR = mix(wR, 0.3, rivet);
    wH = bevel * 0.0032 + dome * 0.0045 + (brush - 0.5) * 0.0004 * (1.0 - rust) + rust * tri(P, Nw, 3.0).b * 0.0025;
    float hot = step(0.955, pid) * (1.0 - smoothstep(0.004, 0.02, ed.y)) * step(0.06, ed.x);    // a few long seams glow dull orange
    wE = vec3(1.0, 0.30, 0.04) * hot * 1.6 * (0.75 + 0.25 * sin(uTime * 1.3 + pid * 50.0));`,
  steel: () => `
    float brush = tri(P * vec3(1.0, 0.03, 1.0), Nw, 9.0).b, grime = tri(P, Nw, 0.2).r;
    float rust = smoothstep(0.56, 0.9, tri(P * vec3(1.0, 0.22, 1.0), Nw, 0.55).a * 0.75 + tri(P, Nw, 1.6).r * 0.25 + (1.0 - smoothstep(0.0, 1.2, P.y)) * 0.2) * 0.8;
    wA = mix(vec3(0.30, 0.32, 0.35) * (0.82 + 0.34 * brush), mix(vec3(0.13, 0.055, 0.025), vec3(0.28, 0.125, 0.05), tri(P, Nw, 1.7).a), rust);
    wA *= 1.0 - 0.45 * smoothstep(0.5, 0.8, grime);
    wM = 1.0 - rust; wR = mix(0.26 + 0.3 * brush, 0.88, rust); wH = (brush - 0.5) * 0.0005 + rust * tri(P, Nw, 3.0).b * 0.002;`,
  copper: () => `
    float pat = smoothstep(0.52, 0.78, tri(P, Nw, 0.6).a + tri(P, Nw, 2.5).b * 0.2);
    wA = mix(vec3(0.78, 0.44, 0.26) * (0.85 + 0.3 * tri(P, Nw, 5.0).b), vec3(0.22, 0.46, 0.38), pat);
    wM = 1.0 - pat * 0.9; wR = mix(0.24, 0.8, pat); wH = pat * tri(P, Nw, 4.0).b * 0.0015;`,
  lamp: () => 'wA = vec3(0.02); wE = vec3(3.0, 3.3, 3.7); wR = 0.5;',
  ember: () => 'wA = vec3(0.02); wE = vec3(2.4, 0.7, 0.10) * (0.8 + 0.2 * sin(uTime * 2.1 + P.y * 9.0)); wR = 0.5;',

  /* --- rock --- */
  rock: () => `
    float warp = tri(P, Nw, 0.11).r;
    float sy = P.y + warp * 0.55 + tri(P, Nw, 0.5).a * 0.07;       // layers, bent by the noise
    float bands = texture2D(uNoise, vec2(sy * 0.33, 0.37)).r, fineB = texture2D(uNoise, vec2(sy * 2.4, 0.71)).a, thin = texture2D(uNoise, vec2(sy * 9.0, 0.13)).b;
    vec3 c1 = vec3(0.36, 0.17, 0.09), c2 = vec3(0.52, 0.29, 0.15), c3 = vec3(0.21, 0.11, 0.07), c4 = vec3(0.60, 0.42, 0.27);
    wA = mix(c1, c2, smoothstep(0.35, 0.65, bands)); wA = mix(wA, c3, smoothstep(0.60, 0.80, fineB) * 0.65); wA = mix(wA, c4, smoothstep(0.72, 0.9, thin) * 0.35);
    float grainLo = tri(P, Nw, 0.6).r;
    float cr = tri(P + warp * 0.6, Nw, 0.27).g, crack = (1.0 - smoothstep(0.012, 0.05, cr)) * smoothstep(0.35, 0.6, tri(P, Nw, 0.19).a);
    float fineCr = (1.0 - smoothstep(0.02, 0.07, tri(P, Nw, 1.3).g)) * smoothstep(0.45, 0.7, grainLo);
    float grain = tri(P, Nw, 2.3).b, speck = tri(P, Nw, 7.0).b;
    wA *= (0.78 + 0.44 * grain) * (0.92 + 0.16 * speck);
    wA *= 1.0 - crack * 0.6 - fineCr * 0.25;
    float top = smoothstep(0.55, 0.9, Nw.y) * smoothstep(0.25, 0.6, tri(P, Nw, 0.3).a + 0.25);     // dust and sand lies on what faces up
    wA = mix(wA, vec3(0.40, 0.25, 0.145) * (0.8 + 0.4 * speck), top * 0.8);
    wR = 0.93 - 0.1 * top; wM = 0.0;
    wH = warp * 0.05 + bands * 0.014 + fineB * 0.006 + grain * 0.005 + speck * 0.0015 - crack * 0.014 - fineCr * 0.004;`,
};

/* Safety: where the scanned room is close to your head or hands, it thins out and the real room (passthrough) shows through. */
const SAFE = `
  { float sd = min(min(distance(vWP, uS0), distance(vWP, uS1)), distance(vWP, uS2));
    gl_FragColor *= 1.0 - 0.85 * uSafeOn * (1.0 - smoothstep(0.28, 0.62, sd)); }`;
const SAFE_HEAD = 'uniform float uSafeOn; uniform vec3 uS0; uniform vec3 uS1; uniform vec3 uS2;\n';

const QUAD_VS = `
  varying vec3 vDir;
  void main() {
    vec4 clipPos = vec4(position.xy, 0.9999, 1.0);
    vec4 v = inverse(projectionMatrix) * clipPos;
    vDir = transpose(mat3(viewMatrix)) * (v.xyz / v.w);
    gl_Position = clipPos;
  }`;
const SKY_HEAD = 'precision highp float; varying vec3 vDir; uniform float uTime; uniform float uBright; uniform sampler2D uNoise;\n';
const SKY = {
  groen: `void main() {
    vec3 d = normalize(vDir); float h = d.y;
    vec3 horizon = vec3(0.040, 0.080, 0.070), zenith = vec3(0.008, 0.020, 0.032);
    vec3 col = mix(horizon, zenith, smoothstep(-0.05, 0.85, h));
    vec2 q = d.xz / (abs(d.y) + 0.45);
    float m = texture2D(uNoise, q * 0.09 + uTime * 0.002).r * 0.65 + texture2D(uNoise, q * 0.27 - uTime * 0.003).a * 0.35;
    col += vec3(0.045, 0.115, 0.095) * pow(m, 2.2) * smoothstep(0.0, 0.5, h);
    float band = exp(-pow((h - 0.30) * 4.5, 2.0)) * pow(texture2D(uNoise, vec2(atan(d.x, d.z) / 6.2832 * 3.0, h * 0.4 + uTime * 0.004)).r, 2.0);
    col += vec3(0.0, 0.20, 0.14) * band * 0.3;
    if (h < 0.0) col = horizon * (0.9 + 0.1 * h);
    gl_FragColor = vec4(col * uBright, 1.0);
  }`,
  /* a hall so large it has weather: a ceiling of girders and strip lights far above, fading into haze */
  metal: `void main() {
    vec3 d = normalize(vDir); float h = d.y;
    vec3 haze = vec3(0.055, 0.065, 0.080);
    vec3 col = haze;
    if (h > 0.015) {
      vec2 uv = d.xz / h * 16.0; float dist = length(uv);
      vec2 g = uv / vec2(13.0, 9.0), f = fract(g) - 0.5;
      float panel = (1.0 - smoothstep(0.30, 0.32, abs(f.x))) * (1.0 - smoothstep(0.035, 0.05, abs(f.y)));
      float glow = (1.0 - smoothstep(0.30, 0.45, abs(f.x))) * exp(-abs(f.y) * 16.0);
      float girder = 1.0 - smoothstep(0.025, 0.04, min(0.5 - abs(f.x), 0.5 - abs(f.y)));
      vec3 ceil = vec3(0.030, 0.034, 0.042) * (0.6 + 0.8 * texture2D(uNoise, uv * 0.03).r) * (1.0 - 0.7 * girder);
      ceil += vec3(1.05, 1.15, 1.3) * panel + vec3(0.16, 0.19, 0.24) * glow;
      col = mix(haze, ceil, exp(-dist * 0.019));
    }
    float az = atan(d.x, d.z) / 6.2832;
    float wl = step(0.9, fract(az * 90.0)) * (1.0 - smoothstep(0.004, 0.012, abs(h - 0.02 - 0.012 * texture2D(uNoise, vec2(az * 4.0, 0.2)).r)));
    col += vec3(1.0, 0.55, 0.2) * wl * 0.5;                                  // a far-off rim of work lights
    col = mix(col, haze * 0.8, 1.0 - smoothstep(-0.25, 0.0, h));
    gl_FragColor = vec4(col * uBright, 1.0);
  }`,
  /* dusk over a canyon land: a low sun, thin high cloud, three ranges of mesas sinking into the haze */
  klippe: `void main() {
    vec3 d = normalize(vDir); float h = d.y;
    vec3 sunDir = normalize(vec3(0.55, 0.13, -0.82));
    vec3 zen = vec3(0.09, 0.19, 0.42), hor = vec3(0.90, 0.60, 0.40);
    float sd = max(dot(d, sunDir), 0.0);
    vec3 col = mix(hor, zen, pow(smoothstep(-0.02, 0.8, h), 0.55));
    col = mix(col, vec3(1.0, 0.70, 0.42), pow(sd, 5.0) * 0.55);
    vec2 q = d.xz / (max(h, 0.0) + 0.22);
    float c = texture2D(uNoise, q * vec2(0.030, 0.085) + uTime * 0.0006).r * 0.7 + texture2D(uNoise, q * vec2(0.09, 0.26)).a * 0.3;
    float cloud = smoothstep(0.50, 0.78, c) * smoothstep(0.03, 0.30, h);
    col = mix(col, mix(vec3(0.80, 0.52, 0.45), vec3(1.0, 0.86, 0.70), pow(sd, 3.0)), cloud * 0.6);
    col += vec3(1.0, 0.80, 0.55) * pow(sd, 220.0) * 1.2 + vec3(3.0, 2.4, 1.6) * smoothstep(0.99955, 0.9998, sd);
    float st = step(0.997, texture2D(uNoise, d.xz / (h + 0.4) * 2.1).b) * smoothstep(0.35, 0.8, h) * (1.0 - cloud);
    col += vec3(0.8) * st * 0.6;
    float az = atan(d.x, d.z) / 6.2832;
    for (int i = 0; i < 3; i++) {
      float fi = float(i);
      float n = texture2D(uNoise, vec2(az * (2.0 + fi * 2.0) + fi * 0.37, 0.21 + fi * 0.3)).r;
      float n2 = texture2D(uNoise, vec2(az * (12.0 + fi * 9.0), 0.63 + fi * 0.11)).a;
      float mh = 0.012 + fi * 0.012 + (0.035 + fi * 0.03) * smoothstep(0.42, 0.56, n) + (0.004 + fi * 0.004) * n2;
      vec3 mc = mix(vec3(0.34, 0.19, 0.14), hor * 0.82, 0.78 - fi * 0.27) * (1.0 + 0.35 * pow(sd, 4.0));
      col = mix(col, mc, 1.0 - smoothstep(mh - 0.002, mh + 0.002, h));
    }
    if (h < 0.0) col = mix(vec3(0.42, 0.27, 0.19), hor * 0.8, exp(h * 14.0));
    gl_FragColor = vec4(col * uBright, 1.0);
  }`,
};
const CLEAR_FS = 'void main() { gl_FragColor = vec4(0.0); }';

/* ------------------------------------------------------------------ build */

export function createWorlds(scene, renderer, params) {
  const noise = makeNoiseTexture();
  const timeU = { value: 0 }, brightU = { value: 1 }, noiseU = { value: noise };
  const safeU = { uSafeOn: { value: 0 }, uS0: { value: new V() }, uS1: { value: new V() }, uS2: { value: new V() } };
  const PASS = new V(0, 1, 0);

  const W = DIMS.map((d, i) => ({
    ...d, i, group: new THREE.Group(), deco: new THREE.Group(), scan: new THREE.Group(), mats: [], tick: [], anchored: false,
    clip: new THREE.Plane(PASS.clone(), 1e6), skyHome: null, fill: null, env: null,
    uni: { uFogCol: { value: new THREE.Color(0, 0, 0) }, uFogDen: { value: 0 }, uGrade: { value: new V(1, 1, 1) } },
    base: { fog: new THREE.Color(0, 0, 0), den: 0, grade: new V(1, 1, 1) },
  }));
  for (const w of W) { w.group.add(w.deco, w.scan); scene.add(w.group); w.group.visible = false; }
  const look = (i, fog, den, grade) => { W[i].base.fog.setRGB(...fog); W[i].base.den = den; W[i].base.grade.set(...grade); W[i].uni.uFogDen.value = den; };
  look(1, [0.040, 0.080, 0.070], 0.17, [0.86, 1.0, 0.92]);
  look(2, [0.055, 0.065, 0.080], 0.06, [0.80, 0.85, 0.93]);
  look(3, [0.80, 0.56, 0.40], 0.022, [1.08, 1.0, 0.9]);

  /** Every material of a dimension is registered, so the dimension can be switched between home / seen-through-the-door. */
  function reg(i, m) {
    m.clippingPlanes = [W[i].clip];
    m.stencilWrite = false; m.stencilRef = 1; m.stencilFunc = THREE.EqualStencilFunc;
    m.stencilFail = m.stencilZFail = m.stencilZPass = THREE.KeepStencilOp;
    W[i].mats.push(m);
    return m;
  }

  /* reflections: a small light set per dimension, baked to an environment map */
  const pm = new THREE.PMREMGenerator(renderer);
  function bake(build) {
    const s = new THREE.Scene();
    const panel = (w, h, rgb, x, y, z) => { const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({ side: THREE.DoubleSide })); m.material.color.setRGB(...rgb); m.position.set(x, y, z); m.lookAt(0, 0, 0); s.add(m); };
    const room = rgb => { const m = new THREE.Mesh(new THREE.BoxGeometry(24, 24, 24), new THREE.MeshBasicMaterial({ side: THREE.BackSide })); m.material.color.setRGB(...rgb); s.add(m); };
    build(room, panel);
    return pm.fromScene(s, 0.03).texture;
  }
  W[1].env = bake((room, panel) => { room([0.012, 0.03, 0.026]); panel(10, 10, [0.10, 0.45, 0.36], 0, 10, 0); panel(4, 8, [0.05, 0.3, 0.25], -9, 0, 3); });
  W[2].env = bake((room, panel) => {
    room([0.02, 0.024, 0.03]);
    for (let k = -1; k <= 1; k++) { panel(9, 1.0, [4.2, 4.6, 5.3], k * 5, 10, -3); panel(9, 1.0, [4.2, 4.6, 5.3], k * 5, 10, 4); }
    panel(3, 6, [0.7, 0.8, 1.0], -11, 1, 0); panel(3, 6, [0.5, 0.56, 0.7], 11, 2, -2); panel(2.5, 1.2, [5, 1.6, 0.25], 5, -3, -10); panel(14, 14, [0.10, 0.11, 0.13], 0, -10, 0);
  });
  W[3].env = bake((room, panel) => {
    room([0.28, 0.30, 0.42]);
    panel(22, 22, [0.20, 0.36, 0.80], 0, 11, 0); panel(22, 7, [1.5, 0.95, 0.6], 0, 1.5, -11); panel(7, 22, [0.9, 0.6, 0.45], 11, 1.5, 0); panel(7, 22, [0.5, 0.38, 0.4], -11, 1.5, 0);
    panel(2.2, 2.2, [26, 17, 9], 6.2, 1.6, -9.2); panel(22, 22, [0.30, 0.19, 0.12], 0, -11, 0);
  });
  pm.dispose();

  function worldMat(i, surf, o = {}) {
    const m = new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 1, metalness: 0, side: o.side ?? THREE.DoubleSide, envMap: W[i].env, envMapIntensity: o.env ?? 1 });
    m.onBeforeCompile = sh => {
      Object.assign(sh.uniforms, W[i].uni, { uNoise: noiseU, uTime: timeU }, o.safe ? safeU : {});
      sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\nvarying vec3 vWP; varying vec3 vWN;')
        .replace('#include <begin_vertex>', `#include <begin_vertex>
          #ifdef USE_INSTANCING
            vWP = (modelMatrix * instanceMatrix * vec4(transformed, 1.0)).xyz; vWN = normalize(mat3(modelMatrix) * mat3(instanceMatrix) * objectNormal);
          #else
            vWP = (modelMatrix * vec4(transformed, 1.0)).xyz; vWN = normalize(mat3(modelMatrix) * objectNormal);
          #endif`);
      sh.fragmentShader = sh.fragmentShader.replace('#include <common>', '#include <common>\n' + HEAD + (o.safe ? SAFE_HEAD : ''))
        .replace('#include <color_fragment>', '#include <color_fragment>\nvec3 P = vWP; vec3 Nw = normalize(vWN); float wR = 0.8, wM = 0.0, wH = 0.0; vec3 wE = vec3(0.0), wA = vec3(0.5);\n' + surf + '\ndiffuseColor.rgb = wA;')
        .replace('#include <roughnessmap_fragment>', '#include <roughnessmap_fragment>\nroughnessFactor = clamp(wR, 0.05, 1.0);')
        .replace('#include <metalnessmap_fragment>', '#include <metalnessmap_fragment>\nmetalnessFactor = clamp(wM, 0.0, 1.0);')
        .replace('#include <normal_fragment_maps>', '#include <normal_fragment_maps>\nnormal = wBump(-vViewPosition, normal, wH);')
        .replace('#include <emissivemap_fragment>', '#include <emissivemap_fragment>\ntotalEmissiveRadiance += wE;')
        .replace('#include <fog_fragment>', 'gl_FragColor.rgb *= uGrade; { float fd = length(vViewPosition); gl_FragColor.rgb = mix(gl_FragColor.rgb, uFogCol, 1.0 - exp(-uFogDen * uFogDen * fd * fd)); }' + (o.safe ? SAFE : ''));
    };
    const k = 'w' + i + ':' + surf.length + ':' + surf.slice(0, 40) + (o.key || '');
    m.customProgramCacheKey = () => k;
    return reg(i, m);
  }

  /* skies: one quad drawn everywhere when the dimension is home, one drawn inside the door (which also resets the depth) */
  const quadGeo = new THREE.PlaneGeometry(2, 2);
  function quad(fs, inRing) {
    const m = new THREE.ShaderMaterial({ vertexShader: QUAD_VS, fragmentShader: fs, uniforms: { uTime: timeU, uBright: brightU, uNoise: noiseU }, blending: THREE.NoBlending, depthTest: inRing, depthWrite: inRing, depthFunc: THREE.AlwaysDepth });
    if (inRing) { m.stencilWrite = true; m.stencilRef = 1; m.stencilFunc = THREE.EqualStencilFunc; m.stencilFail = m.stencilZFail = m.stencilZPass = THREE.KeepStencilOp; }
    const q = new THREE.Mesh(quadGeo, m); q.frustumCulled = false; q.renderOrder = inRing ? ORDER.fill : ORDER.bgHome; q.visible = false; scene.add(q);
    return q;
  }
  W[0].fill = quad(CLEAR_FS, true);                               // the real room seen through the door: alpha 0 = passthrough
  for (const w of W.slice(1)) { w.skyHome = quad(SKY_HEAD + SKY[w.key], false); w.fill = quad(SKY_HEAD + SKY[w.key], true); }

  const floorOf = (i, mat) => { const f = new THREE.Mesh(new THREE.PlaneGeometry(80, 80), mat); f.rotation.x = -Math.PI / 2; f.position.y = -0.012; W[i].group.add(f); };
  function motes(i, count, color, size, opacity, box, rise) {
    const dot = document.createElement('canvas'); dot.width = dot.height = 64; const dx = dot.getContext('2d');
    const g = dx.createRadialGradient(32, 32, 0, 32, 32, 32); g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.4, 'rgba(255,255,255,.35)'); g.addColorStop(1, 'rgba(255,255,255,0)');
    dx.fillStyle = g; dx.fillRect(0, 0, 64, 64);
    const A = new Float32Array(count * 3); for (let k = 0; k < count; k++) { A[k * 3] = rnd(-box[0], box[0]); A[k * 3 + 1] = rnd(0.1, box[1]); A[k * 3 + 2] = -rnd(-1.5, box[2]); }
    const geo = new THREE.BufferGeometry(); geo.setAttribute('position', new THREE.BufferAttribute(A, 3));
    const p = new THREE.Points(geo, reg(i, new THREE.PointsMaterial({ size, map: new THREE.CanvasTexture(dot), color, transparent: true, opacity, depthWrite: false, blending: THREE.AdditiveBlending })));
    p.frustumCulled = false; W[i].deco.add(p);
    W[i].tick.push((dt, t) => { for (let k = 0; k < count; k++) { A[k * 3 + 1] += (rise + rise * 0.6 * Math.sin(t * 0.7 + k)) * dt; A[k * 3] += Math.sin(t * 0.3 + k * 1.7) * 0.02 * dt; if (A[k * 3 + 1] > box[1]) A[k * 3 + 1] = 0.1; } geo.attributes.position.needsUpdate = true; });
  }

  /* ---- 0: the real room. Only the laptop preview gets something to look at. ---- */
  if (params.has('flat')) {
    const c = document.createElement('canvas'); c.width = c.height = 256; const x = c.getContext('2d');
    x.fillStyle = '#1b2230'; x.fillRect(0, 0, 256, 256); x.strokeStyle = '#33445e'; x.lineWidth = 4; x.strokeRect(2, 2, 252, 252);
    const t = new THREE.CanvasTexture(c); t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(60, 60); t.colorSpace = THREE.SRGBColorSpace;
    const g = new THREE.Mesh(new THREE.PlaneGeometry(30, 30), reg(0, new THREE.MeshBasicMaterial({ map: t, side: THREE.DoubleSide })));
    g.rotation.x = -Math.PI / 2; g.position.y = -0.005; W[0].group.add(g);
    const orange = reg(0, new THREE.MeshLambertMaterial({ color: 0xff9a3c, emissive: 0x7a3a08, flatShading: true }));
    for (let k = 0; k < 6; k++) { const h = rnd(0.5, 1.5), p = new THREE.Mesh(new THREE.BoxGeometry(0.16, h, 0.16), orange); p.position.set(rnd(-1.3, 1.3), h / 2, rnd(0.6, 2.6)); W[0].deco.add(p); }
  }

  /* ---- 1: the green one ---- */
  {
    const i = 1, D = W[i].deco;
    const ground = worldMat(i, SURF.moss(0.5), { env: 0.9 });
    floorOf(i, ground); W[i].scanMat = worldMat(i, SURF.moss(0.8), { env: 0.9, key: 's', safe: true });
    const stalkM = worldMat(i, SURF.stalk(), { env: 0.5 }), budM = worldMat(i, SURF.bud(), { env: 0.5 }), barkM = worldMat(i, SURF.bark(), { env: 0.6 });
    const capM = worldMat(i, SURF.cap(), { env: 0.6 }), stemM = worldMat(i, SURF.stem(), { env: 0.5 }), bladeM = worldMat(i, SURF.blade(), { env: 0.4 });
    /* mossy stones */
    const stones = [rockGeometry(3, 11.3), rockGeometry(3, 5.1)], spots = [];
    const stone = (x, z, s) => { const m = new THREE.Mesh(stones[(Math.random() * 2) | 0], ground); m.scale.set(s * rnd(0.9, 1.3), s * rnd(0.55, 0.85), s * rnd(0.9, 1.3)); m.position.set(x, 0.3 * m.scale.y, z); m.rotation.y = rnd(0, TAU); D.add(m); spots.push([x, z, s]); };
    stone(-1.5, -2.4, 0.5); stone(1.7, -2.9, 0.75); stone(-2.8, -4.4, 1.0); stone(0.5, -5.2, 0.6); stone(3.2, -1.3, 0.4); stone(-3.4, -1.0, 0.55); stone(2.6, -5.8, 1.2); stone(-0.8, -7.4, 1.4); stone(-2.2, 1.7, 0.45); stone(2.4, 2.2, 0.6);
    /* roots creeping over the ground from the stones */
    for (let k = 0; k < 12; k++) {
      const [sx, sz, ss] = spots[k % spots.length]; let a = rnd(0, TAU), x = sx + Math.cos(a) * ss * 0.7, z = sz + Math.sin(a) * ss * 0.7; const pts = [new V(x, ss * 0.25, z)];
      const n = 6 + ((Math.random() * 5) | 0); for (let j = 0; j < n; j++) { a += rnd(-0.6, 0.6); x += Math.cos(a) * rnd(0.2, 0.4); z += Math.sin(a) * rnd(0.2, 0.4); pts.push(new V(x, j === 0 ? 0.05 : rnd(0.005, 0.045), z)); }
      D.add(new THREE.Mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), n * 6, rnd(0.014, 0.034), 7, false), barkM));
    }
    /* mushrooms in clusters by the stones: a cap that glows faintly from underneath */
    const capPts = []; for (let j = 0; j <= 10; j++) { const t = j / 10; capPts.push(new THREE.Vector2(Math.sin(t * Math.PI / 2) * 1.0, Math.cos(t * Math.PI / 2) * 0.55)); } capPts.push(new THREE.Vector2(0.92, -0.05), new THREE.Vector2(0.12, 0.04));
    const capG = new THREE.LatheGeometry(capPts, 18), stemG = new THREE.CylinderGeometry(0.11, 0.16, 1, 9); stemG.translate(0, 0.5, 0);
    for (let c = 0; c < 9; c++) {
      const [sx, sz, ss] = spots[c % spots.length], a0 = rnd(0, TAU), cx = sx + Math.cos(a0) * (ss * 0.75 + 0.1), cz = sz + Math.sin(a0) * (ss * 0.75 + 0.1);
      for (let k = 0; k < 3 + ((Math.random() * 4) | 0); k++) {
        const r = rnd(0.025, 0.075), h = r * rnd(1.4, 2.6), g = new THREE.Group();
        const st = new THREE.Mesh(stemG, stemM); st.scale.set(r, h, r); g.add(st);
        const cp = new THREE.Mesh(capG, capM); cp.scale.set(r, r * rnd(0.8, 1.2), r); cp.position.y = h; g.add(cp);
        g.position.set(cx + rnd(-0.14, 0.14), 0, cz + rnd(-0.14, 0.14)); g.rotation.set(rnd(-0.2, 0.2), rnd(0, TAU), rnd(-0.2, 0.2)); D.add(g);
      }
    }
    /* tufts of grass */
    const bg = new THREE.PlaneGeometry(0.014, 1, 1, 4); bg.translate(0, 0.5, 0);
    { const p = bg.attributes.position; for (let k = 0; k < p.count; k++) { const y = p.getY(k); p.setZ(k, y * y * 0.35); p.setX(k, p.getX(k) * (1 - y * 0.85)); } bg.computeVertexNormals(); }
    const NB = 520, blades = new THREE.InstancedMesh(bg, bladeM, NB), m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler(), sc = new V(), ps = new V();
    for (let k = 0, t = 0; k < NB; t++) { const tx = rnd(-4.5, 4.5), tz = rnd(-8, 3), cnt = 10 + ((Math.random() * 14) | 0); for (let j = 0; j < cnt && k < NB; j++, k++) { e.set(rnd(-0.25, 0.25), rnd(0, TAU), rnd(-0.25, 0.25)); q.setFromEuler(e); sc.set(rnd(0.8, 1.6), rnd(0.07, 0.22), 1); ps.set(tx + rnd(-0.12, 0.12), 0, tz + rnd(-0.12, 0.12)); blades.setMatrixAt(k, m4.compose(ps, q, sc)); } }
    blades.frustumCulled = false; D.add(blades);
    /* a few tall thin stalks with a dim bud */
    const stalks = [];
    for (let k = 0; k < 7; k++) {
      const d = rnd(1.6, 5), a = rnd(-1.2, 1.2), h = rnd(0.7, 1.7), lean = rnd(-0.3, 0.3), g = new THREE.Group();
      const pts = []; for (let j = 0; j <= 5; j++) { const t = j / 5; pts.push(new V(lean * t * t * h + rnd(-0.012, 0.012) * j, t * h, rnd(-0.012, 0.012) * j)); }
      g.add(new THREE.Mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 20, rnd(0.005, 0.01), 6, false), stalkM));
      const bud = new THREE.Mesh(new THREE.SphereGeometry(rnd(0.014, 0.026), 12, 9), budM); bud.position.copy(pts[5]); bud.scale.y = 1.7; g.add(bud);
      g.position.set(Math.sin(a) * d, 0, -Math.cos(a) * d); g.userData.p = rnd(0, TAU); g.userData.sw = rnd(0.02, 0.05); D.add(g); stalks.push(g);
    }
    W[i].tick.push((dt, t) => { for (const g of stalks) { g.rotation.z = Math.sin(t * 0.5 + g.userData.p) * g.userData.sw; g.rotation.x = Math.cos(t * 0.4 + g.userData.p) * g.userData.sw * 0.7; } });
    motes(i, 110, 0xa8ffe8, 0.022, 0.35, [4, 2.6, 7], 0.02);
  }

  /* ---- 2: metal ---- */
  {
    const i = 2, D = W[i].deco;
    const plate = worldMat(i, SURF.plate(), { env: 0.85 }), steel = worldMat(i, SURF.steel(), { env: 0.85 }), copper = worldMat(i, SURF.copper(), { env: 0.9 });
    const lamp = worldMat(i, SURF.lamp()), ember = worldMat(i, SURF.ember());
    floorOf(i, plate); W[i].scanMat = worldMat(i, SURF.plate(), { env: 0.85, key: 's', safe: true });
    const add = (geo, mat, x, y, z, rx = 0, ry = 0, rz = 0) => { const m = new THREE.Mesh(geo, mat); m.position.set(x, y, z); m.rotation.set(rx, ry, rz); D.add(m); return m; };
    const colG = new THREE.CylinderGeometry(0.26, 0.26, 9, 28, 1, true), baseG = new THREE.CylinderGeometry(0.42, 0.46, 0.34, 28), collarG = new THREE.CylinderGeometry(0.31, 0.31, 0.12, 28);
    const beamG = new THREE.BoxGeometry(8.4, 0.42, 0.3), flangeG = new THREE.BoxGeometry(8.4, 0.05, 0.46), lampG = new THREE.BoxGeometry(2.2, 0.05, 0.22), hoodG = new THREE.BoxGeometry(2.4, 0.08, 0.36);
    for (let r = 0; r < 5; r++) {
      const z = -1.2 - r * 3.6;
      for (const s of [-1, 1]) { add(colG, steel, s * 3.4, 4.5, z); add(baseG, steel, s * 3.4, 0.17, z); add(collarG, steel, s * 3.4, 2.6, z); add(collarG, steel, s * 3.4, 6.2, z); }
      add(beamG, steel, 0, 6.6, z); add(flangeG, steel, 0, 6.83, z); add(flangeG, steel, 0, 6.37, z);
      add(hoodG, steel, 0, 6.3, z); add(lampG, lamp, 0, 6.24, z);
    }
    const pipeG = new THREE.CylinderGeometry(0.085, 0.085, 17, 20, 1, true), pipe2G = new THREE.CylinderGeometry(0.05, 0.05, 17, 16, 1, true), ringG = new THREE.TorusGeometry(0.105, 0.028, 10, 22), brG = new THREE.BoxGeometry(0.06, 0.5, 0.2);
    for (const s of [-1, 1]) {
      add(pipeG, copper, s * 3.85, 1.05, -8, Math.PI / 2); add(pipeG, steel, s * 3.85, 1.42, -8, Math.PI / 2); add(pipe2G, copper, s * 3.9, 1.72, -8, Math.PI / 2);
      for (let k = 0; k < 9; k++) { add(ringG, steel, s * 3.85, 1.05, -0.4 - k * 1.9); add(ringG, steel, s * 3.85, 1.42, -1.3 - k * 1.9); if (k % 2 === 0) add(brG, steel, s * 3.96, 1.25, -0.9 - k * 1.9); }
    }
    /* a console block and a valve near the door */
    const bx = add(new THREE.BoxGeometry(0.72, 1.05, 0.46), plate, 1.75, 0.525, -2.3, 0, -0.35); bx.add(new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.035, 0.02), ember)); bx.children[0].position.set(0, 0.3, 0.235);
    const top = new THREE.Mesh(new THREE.BoxGeometry(0.78, 0.04, 0.52), steel); top.position.y = 0.545; bx.add(top);
    const valve = new THREE.Group(); valve.position.set(-1.6, 0.95, -2.6); D.add(valve);
    const stem = new THREE.Mesh(new THREE.CylinderGeometry(0.05, 0.06, 0.95, 18), steel); stem.position.y = -0.475; valve.add(stem);
    const wheel = new THREE.Group(); wheel.position.y = 0.06; wheel.rotation.x = Math.PI / 2 - 0.5; valve.add(wheel);
    wheel.add(new THREE.Mesh(new THREE.TorusGeometry(0.16, 0.017, 10, 30), copper));
    for (let k = 0; k < 3; k++) { const sp = new THREE.Mesh(new THREE.CylinderGeometry(0.011, 0.011, 0.32, 8), copper); sp.rotation.z = k * Math.PI / 3; wheel.add(sp); }
    /* a great slow wheel far down the hall */
    const gear = new THREE.Group(); gear.position.set(0, 3.4, -17.5); D.add(gear);
    gear.add(new THREE.Mesh(new THREE.TorusGeometry(2.6, 0.3, 10, 48), steel));
    gear.add(new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.5, 0.7, 24).rotateX(Math.PI / 2), steel));
    for (let k = 0; k < 6; k++) { const sp = new THREE.Mesh(new THREE.BoxGeometry(0.22, 5.2, 0.16), steel); sp.rotation.z = k * Math.PI / 6; gear.add(sp); }
    for (let k = 0; k < 24; k++) { const a = k / 24 * TAU, tth = new THREE.Mesh(new THREE.BoxGeometry(0.34, 0.5, 0.5), steel); tth.position.set(Math.cos(a) * 3.05, Math.sin(a) * 3.05, 0); tth.rotation.z = a + Math.PI / 2; gear.add(tth); }
    W[i].tick.push((dt, t) => { gear.rotation.z = t * 0.05; wheel.rotation.z = Math.sin(t * 0.2) * 0.2; });
    motes(i, 60, 0xcfe0ff, 0.018, 0.25, [4, 3.2, 9], 0.008);
  }

  /* ---- 3: rock ---- */
  {
    const i = 3, D = W[i].deco;
    const rock = worldMat(i, SURF.rock(), { env: 0.7 });
    floorOf(i, rock); W[i].scanMat = worldMat(i, SURF.rock(), { env: 0.7, key: 's', safe: true });
    const geos = [rockGeometry(4, 1.3), rockGeometry(4, 7.9), rockGeometry(3, 4.2)];
    const boulder = (x, z, s, sy = 1) => { const m = new THREE.Mesh(geos[(Math.random() * 3) | 0], rock); m.scale.set(s * rnd(0.85, 1.25), s * sy * rnd(0.7, 1.0), s * rnd(0.85, 1.25)); m.position.set(x, 0.42 * m.scale.y, z); m.rotation.y = rnd(0, TAU); D.add(m); return m; };
    boulder(-1.7, -2.6, 0.75); boulder(-2.5, -3.3, 0.45); boulder(1.9, -2.2, 0.5); boulder(2.7, -3.9, 1.0); boulder(0.6, -4.6, 0.6); boulder(-0.9, -5.6, 1.25); boulder(3.6, -1.2, 0.4);
    boulder(-3.7, -1.4, 0.65); boulder(-4.6, -5.2, 1.5); boulder(4.9, -6.4, 1.7); boulder(0.3, -8.6, 1.9, 1.2); boulder(-2.4, 1.6, 0.5); boulder(2.3, 2.0, 0.7);
    const pillar = (x, z, s) => { let y = 0; for (let k = 0; k < 4; k++) { const w = s * (1.15 - k * 0.2) * rnd(0.9, 1.1), h = s * rnd(1.2, 1.7); const m = new THREE.Mesh(geos[(Math.random() * 3) | 0], rock); m.scale.set(w, h, w); m.position.set(x + rnd(-0.1, 0.1) * s, y + h * 0.45, z + rnd(-0.1, 0.1) * s); m.rotation.y = rnd(0, TAU); D.add(m); y += h * 0.5; } };
    pillar(-6.5, -8.5, 1.3); pillar(6.8, -10.5, 1.6); pillar(-3.2, -12.5, 1.1); pillar(9.5, -4.5, 1.2);
    const pg = rockGeometry(1, 2.2), N = 170, inst = new THREE.InstancedMesh(pg, rock, N), m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler(), sc = new V(), ps = new V();
    for (let k = 0; k < N; k++) { const a = rnd(0, TAU), r = Math.sqrt(Math.random()) * 6.5, s = rnd(0.015, 0.075) * (Math.random() < 0.08 ? 2.2 : 1); e.set(rnd(0, 0.5), rnd(0, TAU), rnd(0, 0.5)); q.setFromEuler(e); sc.set(s * rnd(0.8, 1.4), s * rnd(0.5, 0.9), s * rnd(0.8, 1.4)); ps.set(Math.cos(a) * r, sc.y * 0.3, Math.sin(a) * r - 2); inst.setMatrixAt(k, m4.compose(ps, q, sc)); }
    inst.frustumCulled = false; D.add(inst);
    motes(i, 70, 0xffd9b0, 0.016, 0.22, [4, 2.6, 8], 0.006);
  }

  /* ---- the scanned room, wearing each dimension's surface ---- */
  const scanKnown = new Map(); let scanNext = 0; const scanInfo = { feature: false, count: 0 };
  const wire = params.get('skin') === 'wire' ? new THREE.MeshBasicMaterial({ color: 0x40ffd0, wireframe: true, transparent: true, opacity: 0.5 }) : null;
  function updateScan(frame, refSpace) {
    const now = performance.now();
    if (now < scanNext || !frame.detectedMeshes) return scanInfo;
    scanNext = now + 600; scanInfo.feature = true;
    let did = 0; const seen = new Set();
    for (const m of frame.detectedMeshes) {
      seen.add(m);
      const rec = scanKnown.get(m);
      if (rec && m.lastChangedTime <= rec.t) continue;
      if (did >= 1) { scanNext = 0; continue; }
      const pose = frame.getPose(m.meshSpace, refSpace); if (!pose) continue;
      if (rec) { for (const g of rec.gs) g.parent.remove(g); rec.geo.dispose(); }
      const geo = new THREE.BufferGeometry();
      geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(m.vertices), 3));
      geo.setIndex(new THREE.BufferAttribute(new Uint32Array(m.indices), 1));
      geo.computeVertexNormals();
      const gs = [];
      for (const w of W.slice(1)) {
        const g = new THREE.Group(); g.matrixAutoUpdate = false; g.matrix.fromArray(pose.transform.matrix);
        g.add(new THREE.Mesh(geo, w.scanMat)); if (wire) g.add(new THREE.Mesh(geo, wire));
        w.scan.add(g); gs.push(g);
      }
      scanKnown.set(m, { t: m.lastChangedTime, gs, geo }); did++; api.dirty = true;
    }
    for (const [k, rec] of scanKnown) if (!seen.has(k)) { for (const g of rec.gs) g.parent.remove(g); rec.geo.dispose(); scanKnown.delete(k); }
    scanInfo.count = scanKnown.size;
    return scanInfo;
  }

  const api = {
    W, dirty: false, updateScan,
    /** cur is home (drawn everywhere), oth is seen through the door, the rest are off. */
    applyRoles(cur, oth) {
      for (const w of W) {
        const role = w.i === cur ? 'home' : w.i === oth ? 'other' : 'off';
        w.group.visible = role !== 'off';
        for (const m of w.mats) m.stencilWrite = role === 'other';
        if (role !== 'off') w.group.traverse(o => { if (o.isMesh || o.isPoints || o.isLine) o.renderOrder = role === 'home' ? ORDER.home : ORDER.other; });
        if (w.skyHome) w.skyHome.visible = role === 'home';
        w.fill.visible = role === 'other';
        if (role !== 'other') w.clip.set(PASS, 1e6);
      }
      api.dirty = false;
    },
    /** Keep only what is behind the door, seen from where you stand. */
    setClip(oth, normal, point) { W[oth].clip.setFromNormalAndCoplanarPoint(normal, point); },
    /** Put a dimension's own things round the door, once, so they do not jump when you cut a new door while standing among them. */
    anchor(i, x, z, yaw, force) { const w = W[i]; if (w.anchored && !force) return; w.deco.position.set(x, 0, z); w.deco.rotation.set(0, yaw, 0); w.anchored = true; },
    update(dt, t, bright, cur, oth, safe) {
      timeU.value = t; brightU.value = bright;
      safeU.uSafeOn.value = safe ? 1 : 0; if (safe) { safeU.uS0.value.copy(safe[0]); safeU.uS1.value.copy(safe[1]); safeU.uS2.value.copy(safe[2]); }
      for (const w of W) {
        w.uni.uFogCol.value.copy(w.base.fog).multiplyScalar(bright); w.uni.uGrade.value.copy(w.base.grade).multiplyScalar(bright);
        if (w.i === cur || w.i === oth) for (const f of w.tick) f(dt, t);
      }
    },
    /** Compile every dimension's shaders up front, so switching does not stutter in the headset. */
    precompile(camera) { for (const w of W) { w.group.visible = true; if (w.skyHome) w.skyHome.visible = true; w.fill.visible = true; } renderer.compile(scene, camera); },
  };
  return api;
}
