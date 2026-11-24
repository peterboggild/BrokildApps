/* The ground round the walker, made away from the landing site (runs in a Worker).
 *
 * Near the lander the finest level (L6) was baked offline. Anywhere else in the valley this worker makes the same
 * kind of ground for a square of 192 m round a given point:
 *
 *   mesh     the valley's real heights (LROC NAC model, 4.9 m samples, bicubic) plus generated craters, laid
 *            exactly onto the coarser level's triangles along its border and round L6
 *   map      R,G = the normal of the generated detail only (the shader adds it to the parent level's normal),
 *            B = the horizon angle towards the Sun of the detail and the rocks (their cast shadows),
 *            A = albedo detail
 *   rocks    positions for instancing
 *
 * Everything generated is a pure function of position (seeded per 32 m tile), so when the square moves with the
 * walker the ground in the overlap is identical: nothing pops, and bootprints stay where they were pressed.
 *
 * Craters of 1.5-7 m inside L5 are the ones the build stamped into L5 (craters.dat): L5's map already shades
 * them, so here they only shape the mesh and cast sharper shadows. Outside L5 they are generated here.
 */
import { makeGeo, gridHeight, rng, hash2, smooth } from './geo.js?v=3';

const HALF = 96, T = 1024, N = 160, WARP = 0.35, TILE = 32, EDGE = 12, PX = 2 * HALF / T;
let man, geo, L4, L5, P4, P5, half5, half6, valley, cr, crIndex, ready = null;

async function loadDat(url) {
  const buf = new Uint8Array(await (await fetch(url)).arrayBuffer());
  if (buf[0] === 0x1f && buf[1] === 0x8b) {
    return await new Response(new Blob([buf]).stream().pipeThrough(new DecompressionStream('gzip'))).arrayBuffer();
  }
  return buf.buffer;        /* a server that already unpacked it */
}

async function init(m) {
  man = m.man; geo = makeGeo(man);
  L4 = man.levels.find(l => l.name === 'L4'); L5 = man.levels.find(l => l.name === 'L5');
  const L6 = man.levels.find(l => l.name === 'L6');
  P4 = m.P4; P5 = m.P5;
  const toM = l => l.dlat * Math.PI / 180 * man.radius;
  half5 = toM(L5); half6 = toM(L6);
  const [vb, cb] = await Promise.all([loadDat(m.base + 'valley.dat'), loadDat(m.base + 'craters.dat')]);
  /* valley heights: uint16 row-wise differences -> metres about the site */
  const n = man.valley.n, d = new Uint16Array(vb), h = new Float32Array(n * n);
  for (let j = 0; j < n; j++) {
    let acc = 0;
    for (let i = 0; i < n; i++) { acc = (acc + d[j * n + i]) & 0xFFFF; h[j * n + i] = acc * man.valley.unit + man.valley.floor; }
  }
  valley = { n, h };
  /* the craters of L5, bucketed in 64 m tiles */
  const dv = new DataView(cb), c = man.craters, nc = c.n;
  cr = { x: new Float32Array(nc), z: new Float32Array(nc), d: new Float32Array(nc), ratio: new Float32Array(nc), fresh: new Float32Array(nc) };
  crIndex = new Map();
  for (let k = 0; k < nc; k++) {
    cr.x[k] = dv.getInt16(k * 8, true) / c.xyScale; cr.z[k] = -dv.getInt16(k * 8 + 2, true) / c.xyScale;     /* north -> -z */
    cr.d[k] = dv.getUint16(k * 8 + 4, true) / c.dScale; cr.ratio[k] = dv.getUint8(k * 8 + 6) / c.ratioScale; cr.fresh[k] = dv.getUint8(k * 8 + 7) / 255;
    const key = Math.floor(cr.x[k] / 64) + ',' + Math.floor(cr.z[k] / 64);
    let a = crIndex.get(key); if (!a) crIndex.set(key, a = []); a.push(k);
  }
}

/* bicubic (Catmull-Rom) sample of the valley heights at a position in L4's map */
function valleyAt(u, v) {
  const n = valley.n, h = valley.h, fx = u * n - 0.5, fy = v * n - 0.5;
  const ix = Math.floor(fx), iy = Math.floor(fy), tx = fx - ix, ty = fy - iy;
  const wgt = t => [((-t + 2) * t - 1) * t / 2, ((3 * t - 5) * t * t + 2) / 2, ((-3 * t + 4) * t + 1) * t / 2, (t - 1) * t * t / 2];
  const wx = wgt(tx), wy = wgt(ty);
  let s = 0;
  for (let b = 0; b < 4; b++) {
    const y = Math.min(n - 1, Math.max(0, iy - 1 + b));
    let r = 0;
    for (let a = 0; a < 4; a++) r += wx[a] * h[y * n + Math.min(n - 1, Math.max(0, ix - 1 + a))];
    s += wy[b] * r;
  }
  return s;
}
const _ll = {}, _uv = {};
/* the real ground: site y at (x, z) from the valley heights */
function groundY(x, z) {
  let y = -(x * x + z * z) / (2 * geo.r0);
  for (let k = 0; k < 2; k++) {
    geo.latlon(x, y, z, _ll); geo.levelUV(L4, _ll, _uv);
    y += geo.r0 + valleyAt(_uv.u, _uv.v) - _ll.r;
  }
  return y;
}
/* the coarser level's mesh as drawn: L5 where it lies, else L4 */
function parentY(x, z, yGuess) {
  geo.latlon(x, yGuess, z, _ll);
  geo.levelUV(L5, _ll, _uv);
  let y = (_uv.u > 0 && _uv.u < 1 && _uv.v > 0 && _uv.v < 1) ? geo.surfaceY(L5, P5, _uv) : null;
  if (y === null) { geo.levelUV(L4, _ll, _uv); y = geo.surfaceY(L4, P4, _uv); }
  return y === null ? yGuess : y;
}
/* how much of L5's own data is present at a point (L5 melts into L4 along its border): 0..1 */
function weightL5(x, z) {
  const d = Math.min(half5 - Math.abs(x), half5 - Math.abs(z)) / (2 * half5);
  return smooth(d / man.craters.edgeBand);
}

function stamp(h, alb, x0, z0, cx, cz, D, ratio, fresh, amp) {
  const rad = D / 2, ext = rad * (fresh > 0.3 ? 2.6 : 1.8), s = 1 / PX;
  const pcx = (cx - x0) * s, pcz = (cz - z0) * s;
  let i0 = Math.floor(pcx - ext * s), i1 = Math.floor(pcx + ext * s) + 2, j0 = Math.floor(pcz - ext * s), j1 = Math.floor(pcz + ext * s) + 2;
  if (i1 <= 0 || j1 <= 0 || i0 >= T || j0 >= T || rad * s < 0.7) return;
  i0 = Math.max(i0, 0); j0 = Math.max(j0, 0); i1 = Math.min(i1, T); j1 = Math.min(j1, T);
  const depth = ratio * D, rim = 0.2 * depth * (0.3 + 0.7 * fresh), er = ext / rad, halo = alb && fresh > 0.85 && D > 0.8;
  for (let j = j0; j < j1; j++) {
    const dz = (j + 0.5 - pcz) * PX;
    for (let i = i0; i < i1; i++) {
      const dx = (i + 0.5 - pcx) * PX, r = Math.sqrt(dx * dx + dz * dz) / rad;
      if (r >= er) continue;
      let p = r <= 1 ? rim - (depth + rim) * (1 - r * r) : rim / (r * r * r);
      if (fresh < 0.5) p = p * (fresh * 2) - depth * Math.exp(-(r * 1.25) * (r * 1.25) * 1.6) * 0.9 * (1 - fresh * 2);
      const fade = Math.min(1, Math.max(0, (er - r) / (0.35 * er)));
      h[j * T + i] += p * fade * amp;
      if (halo) alb[j * T + i] += (fresh - 0.85) * 1.6 * Math.exp(-(r / 1.6) * (r / 1.6)) * fade * amp;
    }
  }
}

const lattice = (i, j, k) => hash2(i, j, k) / 4294967296;
function vnoise(x, z, k) {
  const ix = Math.floor(x), iz = Math.floor(z);
  let tx = x - ix, tz = z - iz; tx = tx * tx * (3 - 2 * tx); tz = tz * tz * (3 - 2 * tz);
  return (lattice(ix, iz, k) * (1 - tx) + lattice(ix + 1, iz, k) * tx) * (1 - tz) + (lattice(ix, iz + 1, k) * (1 - tx) + lattice(ix + 1, iz + 1, k) * tx) * tz - 0.5;
}

function generate(cx, cz) {
  const x0 = cx - HALF, z0 = cz - HALF;
  const hd = new Float32Array(T * T);        /* detail that this map shades */
  const hg = new Float32Array(T * T);        /* shape only: craters that L5's map already shades */
  const alb = new Float32Array(T * T);
  const rocks = [];
  const powD = (r, a, b) => Math.pow(Math.pow(a, -2) - r * (Math.pow(a, -2) - Math.pow(b, -2)), -0.5);
  const cluster = (R, x, z, D, fresh) => {
    const n = Math.floor(16 * D * Math.pow(fresh, 1.5) * (0.5 + R()));
    for (let q = 0; q < n; q++) {
      const rr = D / 2 * (0.75 + 2.6 * Math.pow(R(), 1.7)), th = R() * 6.2832;
      const sz = 0.09 * Math.pow(1 - R() * (1 - Math.pow(0.09 / (0.22 * D), 2)), -0.5);
      rocks.push([x + rr * Math.cos(th), z + rr * Math.sin(th), sz, R() * 6.2832, Math.floor(R() * 3)]);
    }
  };

  /* generated craters and rocks, tile by tile (one ring of tiles outside, for craters that reach in) */
  const t0x = Math.floor(x0 / TILE) - 1, t1x = Math.floor((x0 + 2 * HALF) / TILE) + 1;
  const t0z = Math.floor(z0 / TILE) - 1, t1z = Math.floor((z0 + 2 * HALF) / TILE) + 1;
  for (let tz = t0z; tz <= t1z; tz++) for (let tx = t0x; tx <= t1x; tx++) {
    const bx = tx * TILE, bz = tz * TILE;
    let R = rng(hash2(tx, tz, 1));
    for (let q = 0; q < 630; q++) {                        /* 0.28-1.5 m: N(>D) = 0.05 D^-2 per m^2 */
      const D = powD(R(), 0.28, 1.5), x = bx + R() * TILE, z = bz + R() * TILE, fresh = Math.pow(R(), 2.6);
      stamp(hd, alb, x0, z0, x, z, D, 0.025 + 0.175 * fresh, fresh, 1);
    }
    R = rng(hash2(tx, tz, 2));
    for (let q = 0; q < 22; q++) {                         /* 1.5-7 m, where L5 has none of its own */
      const D = powD(R(), 1.5, 7), x = bx + R() * TILE, z = bz + R() * TILE, fresh = Math.pow(R(), 2.6);
      const amp = 1 - weightL5(x, z);
      if (amp <= 0.01) continue;
      stamp(hd, alb, x0, z0, x, z, D, 0.025 + 0.175 * fresh, fresh, amp);
      if (D > 2.5 && fresh > 0.55 && amp > 0.5) cluster(rng(hash2(Math.round(x * 8), Math.round(z * 8), 5)), x, z, D, fresh);
    }
    R = rng(hash2(tx, tz, 3));
    for (let q = 0; q < 4; q++) {                          /* scattered rocks */
      const sz = 0.10 * Math.pow(1 - R() * (1 - Math.pow(0.10 / 1.1, 2.2)), -1 / 2.2);
      rocks.push([bx + R() * TILE, bz + R() * TILE, sz, R() * 6.2832, Math.floor(R() * 3)]);
    }
  }
  /* L5's own craters */
  for (let kz = Math.floor((z0 - 10) / 64); kz <= Math.floor((z0 + 2 * HALF + 10) / 64); kz++)
    for (let kx = Math.floor((x0 - 10) / 64); kx <= Math.floor((x0 + 2 * HALF + 10) / 64); kx++) {
      const list = crIndex.get(kx + ',' + kz); if (!list) continue;
      for (const k of list) {
        const amp = weightL5(cr.x[k], cr.z[k]);
        if (amp <= 0.01) continue;
        stamp(hg, null, x0, z0, cr.x[k], cr.z[k], cr.d[k], cr.ratio[k], cr.fresh[k], amp);
        if (cr.d[k] > 2.5 && cr.fresh[k] > 0.55 && amp > 0.5) cluster(rng(hash2(Math.round(cr.x[k] * 8), Math.round(cr.z[k] * 8), 5)), cr.x[k], cr.z[k], cr.d[k], cr.fresh[k]);
      }
    }

  /* soil relief, then melt everything to nothing at the border of the square and round L6 */
  const W = new Float32Array(T * T);
  for (let j = 0; j < T; j++) {
    const z = z0 + (j + 0.5) * PX;
    for (let i = 0; i < T; i++) {
      const x = x0 + (i + 0.5) * PX, k = j * T + i;
      const w = fade(x, z, cx, cz);
      W[k] = w;
      if (w <= 0) { hd[k] = 0; hg[k] = 0; alb[k] = 1; continue; }
      const nz = vnoise(x / 0.7, z / 0.7, 11) * 0.022 + vnoise(x / 2.9, z / 2.9, 12) * 0.05;
      hd[k] = (hd[k] + nz) * w; hg[k] *= w;
      alb[k] = 1 + (alb[k] + 0.05 * vnoise(x / 6.1, z / 6.1, 13)) * w;
    }
  }

  /* the mesh */
  const nv = (N + 1) * (N + 1), pos = new Float32Array(nv * 3), uvA = new Float32Array(nv * 2), uvB = new Float32Array(nv * 2);
  const wf = s => s * (WARP + (1 - WARP) * s * s);
  const detail = (x, z) => {
    const fx = Math.min(T - 1.001, Math.max(0, (x - x0) / PX - 0.5)), fz = Math.min(T - 1.001, Math.max(0, (z - z0) / PX - 0.5));
    const i = Math.floor(fx), j = Math.floor(fz), tx = fx - i, tz = fz - j, a = j * T + i;
    return ((hd[a] + hg[a]) * (1 - tx) + (hd[a + 1] + hg[a + 1]) * tx) * (1 - tz) + ((hd[a + T] + hg[a + T]) * (1 - tx) + (hd[a + T + 1] + hg[a + T + 1]) * tx) * tz;
  };
  for (let j = 0; j <= N; j++) for (let i = 0; i <= N; i++) {
    const x = cx + HALF * wf(i / N * 2 - 1), z = cz + HALF * wf(j / N * 2 - 1), k = j * (N + 1) + i;
    const yv = groundY(x, z), yp = parentY(x, z, yv), w = (i === 0 || j === 0 || i === N || j === N) ? 0 : fade(x, z, cx, cz);
    const y = yp + (yv - yp) * w + (w > 0 ? detail(x, z) : 0);
    pos[k * 3] = x; pos[k * 3 + 1] = y; pos[k * 3 + 2] = z;
    geo.latlon(x, y, z, _ll);
    geo.levelUV(L4, _ll, _uv); uvA[k * 2] = _uv.u; uvA[k * 2 + 1] = _uv.v;
    geo.levelUV(L5, _ll, _uv); uvB[k * 2] = _uv.u; uvB[k * 2 + 1] = _uv.v;
  }

  /* rocks: keep those well inside, stand them on the mesh, and raise them in the caster heights for their shadows */
  const hAt = gridHeight(pos, N, N), cast = new Float32Array(T * T);
  for (let k = 0; k < T * T; k++) cast[k] = hd[k] + hg[k];
  const out = [];
  for (const r of rocks) {
    const [x, z, size] = r;
    if (Math.abs(x - cx) > HALF - 3 || Math.abs(z - cz) > HALF - 3 || fade(x, z, cx, cz) < 0.6 || out.length >= 1400 * 8) continue;
    const q = 0.36 * size;
    const y = Math.min(hAt(x, z), hAt(x + q, z), hAt(x - q, z), hAt(x, z + q), hAt(x, z - q)) - 0.06 * size;
    geo.latlon(x, y, z, _ll); geo.levelUV(L4, _ll, _uv);
    out.push(x, y, z, size, r[3], r[4], _uv.u, _uv.v);
    const pcx = (x - x0) / PX, pcz = (z - z0) / PX, pr = size * 0.5 / PX, hh = size * 0.62;
    const base = cast[Math.min(T - 1, Math.max(0, Math.floor(pcz))) * T + Math.min(T - 1, Math.max(0, Math.floor(pcx)))];
    for (let j = Math.max(0, Math.floor(pcz - pr)); j <= Math.min(T - 1, Math.ceil(pcz + pr)); j++)
      for (let i = Math.max(0, Math.floor(pcx - pr)); i <= Math.min(T - 1, Math.ceil(pcx + pr)); i++) {
        const r2 = ((i + 0.5 - pcx) * (i + 0.5 - pcx) + (j + 0.5 - pcz) * (j + 0.5 - pcz)) / (pr * pr);
        if (r2 < 1) cast[j * T + i] = Math.max(cast[j * T + i], base + hh * Math.sqrt(1 - r2 * r2));
      }
  }

  /* horizon towards the Sun: the steepest slope up to a caster along the Sun's azimuth, per texel */
  const sl = man.sun.local, sh = Math.hypot(sl[0], sl[2]), dx = sl[0] / sh, dz = sl[2] / sh;
  const slope = new Float32Array(T * T).fill(-1);
  const steps = [];
  for (let d = 1; d <= 220; d = d < 40 ? d + 1.5 : d * 1.13) steps.push(d);
  for (const d of steps) {
    const ox = dx * d, oz = dz * d, a = Math.floor(ox), b = Math.floor(oz), fa = ox - a, fb = oz - b, inv = 1 / (d * PX);
    const near = d < 14, w00 = (1 - fa) * (1 - fb), w10 = fa * (1 - fb), w01 = (1 - fa) * fb, w11 = fa * fb;
    for (let j = 0; j < T; j++) {
      const j0 = Math.min(T - 1, Math.max(0, j + b)) * T, j1 = Math.min(T - 1, Math.max(0, j + b + 1)) * T, jn = Math.min(T - 1, Math.max(0, Math.round(j + oz))) * T;
      for (let i = 0; i < T; i++) {
        const k = j * T + i;
        let hs;
        if (near) {
          const i0 = Math.min(T - 1, Math.max(0, i + a)), i1 = Math.min(T - 1, Math.max(0, i + a + 1));
          hs = cast[j0 + i0] * w00 + cast[j0 + i1] * w10 + cast[j1 + i0] * w01 + cast[j1 + i1] * w11;
        } else hs = cast[jn + Math.min(T - 1, Math.max(0, Math.round(i + ox)))];
        const s = (hs - hd[k] - hg[k]) * inv;
        if (s > slope[k]) slope[k] = s;
      }
    }
  }

  /* the map */
  const map = new Uint8Array(T * T * 4), h0 = man.hEncode[0], h1 = man.hEncode[1], Rd = rng(hash2(cx | 0, cz | 0, 9));
  for (let j = 0; j < T; j++) for (let i = 0; i < T; i++) {
    const k = j * T + i;
    const gx = (hd[j * T + Math.min(T - 1, i + 1)] - hd[j * T + Math.max(0, i - 1)]) / (2 * PX);
    const gz = (hd[Math.min(T - 1, j + 1) * T + i] - hd[Math.max(0, j - 1) * T + i]) / (2 * PX);
    const inv = 1 / Math.sqrt(gx * gx + gz * gz + 1);
    map[k * 4] = Math.min(255, Math.max(1, Math.floor(-gx * inv * 127 + 128 + Rd())));
    map[k * 4 + 1] = Math.min(255, Math.max(1, Math.floor(gz * inv * 127 + 128 + Rd())));
    const H = Math.atan(slope[k]) * W[k];                  /* no shadows of its own where the detail has melted away */
    map[k * 4 + 2] = Math.min(255, Math.max(0, Math.round((Math.max(H, h0) - h0) / h1 * 255)));
    map[k * 4 + 3] = Math.min(255, Math.max(0, Math.round(alb[k] * 128)));
  }
  return { cx, cz, half: HALF, n: N, t: T, pos, uvA, uvB, map, rocks: new Float32Array(out) };
}

/* 0 at the border of the square and inside or next to L6, 1 well inside */
function fade(x, z, cx, cz) {
  const e = Math.min(HALF - Math.abs(x - cx), HALF - Math.abs(z - cz)) / EDGE;
  const o = Math.max(Math.abs(x) - half6, Math.abs(z) - half6) / 10;
  return smooth(e) * smooth(o);
}

onmessage = async e => {
  const m = e.data;
  if (m.type === 'init') { ready = init(m); await ready; postMessage({ type: 'ready' }); return; }
  if (m.type === 'gen') {
    await ready;
    const t = performance.now(), r = generate(m.cx, m.cz);
    postMessage({ type: 'ground', id: m.id, ms: performance.now() - t, ...r }, [r.pos.buffer, r.uvA.buffer, r.uvB.buffer, r.map.buffer, r.rocks.buffer]);
  }
};
