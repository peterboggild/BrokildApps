/* Chapter 7's flight: the ascent from Taurus-Littrow to lunar orbit.
 *
 * Frame: stage 0's site frame, as chapters 3 to 6 (x east, y up, z south at the landing site; the Moon's centre at
 * (0, -r0, 0)). What moves is the LM's origin (the bottom of its descent engine, lm-model.js), as in the descent,
 * though only the ascent stage flies: the descent stage is left where it stands.
 *
 * - Lift-off at Apollo 17's time (GET 185:21:37). The ascent engine cannot be throttled or steered: 15.6 kN from
 *   first to last, the stage growing lighter (4950 kg at lift-off, about 5 kg a second burnt), so the push grows
 *   from 3.1 to 5.8 m/s2 (a third of a g to nearly 0.6).
 * - Ten seconds straight up, to clear the ground; then the pitch-over, and the long burn nearly along the horizon.
 * - The steering: every moment the thrust's vertical part is what brings the LM level at the insertion height when
 *   the horizontal speed is there (a cubic in time for the height, with gravity less the lift of the speed already
 *   gained), and all the rest of the thrust goes forward. That gives the real ascent's shape: about 50 degrees over
 *   at the pitch-over, flattening as the speed builds. Cut-off at the insertion speed, a little over seven minutes
 *   after lift-off, 18 km up, in an orbit that reaches 85 km on the far side (Apollo's 9 by 48 nautical miles).
 * - Then the orbit, unpowered.
 * The whole flight is worked out once, as a table. GET = ground elapsed time, seconds from the launch from Earth. */
import { MU_M, HEADING } from './lunar-space.js?v=35';

export const GETA = { lift: 185 * 3600 + 21 * 60 + 37 };
GETA.pitch = GETA.lift + 10;              /* the pitch-over */

const add = (a, b, k = 1) => [a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k];
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const len = a => Math.sqrt(dot(a, a));
const scale = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
const norm = a => scale(a, 1 / len(a));

const THRUST = 15570, M0 = 4950, VE = 3050, MDOT = THRUST / VE;
const H_INS = 18300, H_APO = 85000;

/* man: the Moon's manifest (radius, r0, site.height); y0: where the LM's origin stands on the ground (lm-model.js
 * settleLM); groundAt(x, z): the ground's height near the site */
export function makeAscent(man, y0, groundAt) {
  const R = man.radius, C = [0, -man.r0, 0];
  const U0 = [0, 1, 0], F0 = [Math.sin(HEADING), 0, -Math.cos(HEADING)], N0 = norm(cross(U0, F0));
  const g = r => { const d = sub(r, C), l = len(d); return scale(d, -MU_M / (l * l * l)); };
  const upAt = r => norm(sub(r, C));
  const fwdAt = r => norm(cross(N0, upAt(r)));                 /* ahead along the track, level */
  const LAND = [0, y0, 0];
  const rSite = len(sub(LAND, C)), rI = rSite + H_INS, vI = Math.sqrt(MU_M * (2 / rI - 2 / (rI + R + H_APO)));
  /* the height over the ground under the LM (near the site: the terrain; far out: the site's own level) */
  function height(s) {
    const rc = len(sub(s.r, C)), x = s.r[0], z = s.r[2];
    const gy = groundAt && Math.hypot(x, z) < 60000 ? groundAt(x, z) : null;
    const gr = gy === null || !isFinite(gy) ? R + man.site.height : len(sub([x, gy, z], C));
    return rc - gr;
  }

  const DT = 0.2, T0 = GETA.lift - 120, rows = [];
  for (let t = T0; t < GETA.lift - 1e-6; t += DT) rows.push({ t, r: LAND, v: [0, 0, 0], a: [0, 0, 0], m: M0 });
  let r = [...LAND], v = [0, 0, 0], m = M0, t = GETA.lift, cut = null, frac = 1, pitch0 = null;
  while (t < GETA.lift + 900) {
    const up = upAt(r), fw = fwdAt(r), rr = len(sub(r, C)), vr = dot(v, up), vh = dot(v, fw);
    let a = [0, 0, 0];
    if (cut === null) {
      const am = THRUST / m;
      if (t < GETA.pitch) a = scale(up, am);
      else {
        /* the time to go, from the speed still to gain (the rocket equation, a little added for what goes upward) */
        const tgo = Math.max(0.2, m / MDOT * (1 - Math.exp(-Math.max(0, vI - vh) * 1.02 / VE)));
        if (tgo > 8) frac = Math.max(-0.6, Math.min(0.92, (6 * (rI - rr) / (tgo * tgo) - 4 * vr / tgo + MU_M / (rr * rr) - vh * vh / rr) / am));
        a = add(scale(up, am * frac), fw, am * Math.sqrt(1 - frac * frac));
        if (pitch0 === null && t > GETA.pitch + 12) pitch0 = Math.acos(frac) * 180 / Math.PI;
        if (vh >= vI) { cut = t; a = [0, 0, 0]; }
      }
      if (cut === null) m -= MDOT * DT;
    }
    rows.push({ t, r, v, a, m });
    v = add(v, add(a, g(r)), DT); r = add(r, v, DT); t += DT;
    if (cut !== null && t > cut + 300) break;
  }
  GETA.insert = cut;
  const N = rows.length;
  function state(tt) {
    const f = (tt - rows[0].t) / DT, i = Math.max(0, Math.min(N - 2, Math.floor(f))), k = Math.max(0, Math.min(1, f - i));
    const a = rows[i], b = rows[i + 1], mix = (x, y) => [x[0] + (y[0] - x[0]) * k, x[1] + (y[1] - x[1]) * k, x[2] + (y[2] - x[2]) * k];
    return { t: tt, r: mix(a.r, b.r), v: mix(a.v, b.v), a: tt >= cut || tt < GETA.lift ? [0, 0, 0] : mix(a.a, b.a), m: a.m + (b.m - a.m) * k };
  }
  /* the orbit you are in: its low and high points over the Moon's mean radius, metres */
  function orbit(s) {
    const d = sub(s.r, C), rr = len(d), v2 = dot(s.v, s.v), en = v2 / 2 - MU_M / rr, a = -MU_M / (2 * en), h = len(cross(d, s.v)), e = Math.sqrt(Math.max(0, 1 - h * h / (MU_M * a)));
    return { peri: a * (1 - e) - R, apo: a * (1 + e) - R };
  }
  const info = () => {
    const s = state(cut + 1), up = upAt(s.r);
    let maxA = 0; for (const x of rows) maxA = Math.max(maxA, len(x.a));
    return { burn: cut - GETA.lift, pitch0, alt: len(sub(s.r, C)) - rSite, speed: len(s.v), climb: dot(s.v, up), mass: s.m, maxA, firstA: THRUST / M0,
      downrange: Math.atan2(dot(sub(s.r, C), F0), dot(sub(s.r, C), U0)) * R, orbit: orbit(s), rows: N, end: rows[N - 1].t };
  };
  return { state, info, height, orbit, upAt, fwdAt, LAND, F0, U0, C, R, GETA, T0, vI, end: rows[N - 1].t };
}
