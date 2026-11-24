/* The sky and the air between you and the ground, by single scattering (Rayleigh and Mie), for one altitude and
 * one Sun elevation. Runs in a Worker; the page asks again when the altitude has changed enough.
 *
 * Table: u = angle from the Sun's azimuth (0..pi), v = elevation, packed round the limb (the edge of the Earth seen
 * from this altitude) where the sky changes fastest: v = 0.5 +- 0.5 sqrt(|el - limb| / range).
 * RGB = light scattered towards you along the ray (up to the ground or out of the air), A = transmittance along it. */
const RE = 6371e3, RA = RE + 100e3, HR = 8000, HM = 1200;
const BR = [5.8e-6, 13.5e-6, 33.1e-6], BM = 21e-6, BMX = BM * 1.11, G = 0.76;
const NU = 48, NV = 160, STEPS = 28, LSTEPS = 6;

function hitSphere(ox, oy, oz, dx, dy, dz, r) {         /* far and near roots, or null */
  const b = ox * dx + oy * dy + oz * dz, c = ox * ox + oy * oy + oz * oz - r * r, disc = b * b - c;
  if (disc < 0) return null;
  const q = Math.sqrt(disc);
  return [-b - q, -b + q];
}
/* optical depth to the Sun from a point (Rayleigh, Mie) */
function sunDepth(px, py, pz, sx, sy, sz) {
  const h = hitSphere(px, py, pz, sx, sy, sz, RA);
  if (!h) return [0, 0];
  const g = hitSphere(px, py, pz, sx, sy, sz, RE);
  if (g && g[0] > 0) return [1e9, 1e9];                     /* the Earth is in the way */
  const len = h[1], ds = len / LSTEPS;
  let r = 0, m = 0;
  for (let i = 0; i < LSTEPS; i++) {
    const t = (i + 0.5) * ds, x = px + sx * t, y = py + sy * t, z = pz + sz * t;
    const alt = Math.sqrt(x * x + y * y + z * z) - RE;
    r += Math.exp(-alt / HR) * ds; m += Math.exp(-alt / HM) * ds;
  }
  return [r, m];
}

function build(alt, sunEl) {
  const out = new Float32Array(NU * NV * 4);
  const oy = RE + Math.max(alt, 2);
  const limb = -Math.acos(Math.min(1, RE / oy));
  const sx = Math.cos(sunEl), sy = Math.sin(sunEl), sz = 0;   /* the Sun in the x-y plane; y up */
  for (let j = 0; j < NV; j++) {
    const v = (j + 0.5) / NV, s = v < 0.5 ? -1 : 1, q = (2 * v - 1) * s;
    const el = limb + s * q * q * (s > 0 ? Math.PI / 2 - limb : Math.PI / 2 + limb);
    const ce = Math.cos(el), se = Math.sin(el);
    for (let i = 0; i < NU; i++) {
      const az = (i + 0.5) / NU * Math.PI;
      const dx = ce * Math.cos(az), dy = se, dz = ce * Math.sin(az);
      /* the ray inside the air: from the camera (or where it enters) to the ground or out */
      const a = hitSphere(0, oy, 0, dx, dy, dz, RA);
      const k = (j * NU + i) * 4;
      if (!a || a[1] <= 0) { out[k + 3] = 1; continue; }
      let t0 = Math.max(0, a[0]), t1 = a[1];
      const g = hitSphere(0, oy, 0, dx, dy, dz, RE);
      if (g && g[0] > 0) t1 = Math.min(t1, g[0]);
      const mu = dx * sx + dy * sy + dz * sz;
      const pr = 3 / (16 * Math.PI) * (1 + mu * mu);
      const pm = 3 / (8 * Math.PI) * ((1 - G * G) * (1 + mu * mu)) / ((2 + G * G) * Math.pow(1 + G * G - 2 * G * mu, 1.5));
      const ds = (t1 - t0) / STEPS;
      let odR = 0, odM = 0, R0 = 0, R1 = 0, R2 = 0;
      for (let n = 0; n < STEPS; n++) {
        const t = t0 + (n + 0.5) * ds, x = dx * t, y = oy + dy * t, z = dz * t;
        const h = Math.sqrt(x * x + y * y + z * z) - RE;
        const dr = Math.exp(-h / HR) * ds, dm = Math.exp(-h / HM) * ds;
        odR += dr; odM += dm;
        const [lr, lm] = sunDepth(x, y, z, sx, sy, sz);
        if (lr > 1e8) continue;
        for (let c = 0; c < 3; c++) {
          const tau = BR[c] * (odR + lr) + BMX * (odM + lm);
          const att = Math.exp(-tau);
          const add = att * (dr * BR[c] * pr + dm * BM * pm);
          if (c === 0) R0 += add; else if (c === 1) R1 += add; else R2 += add;
        }
      }
      out[k] = R0 * 20; out[k + 1] = R1 * 20; out[k + 2] = R2 * 20;
      out[k + 3] = Math.exp(-(0.299 * BR[0] + 0.587 * BR[1] + 0.114 * BR[2]) * odR - BMX * odM);
    }
  }
  return { data: out, limb, nu: NU, nv: NV };
}

onmessage = e => {
  const { id, alt, sunEl } = e.data;
  const r = build(alt, sunEl);
  postMessage({ id, alt, sunEl, ...r }, [r.data.buffer]);
};
