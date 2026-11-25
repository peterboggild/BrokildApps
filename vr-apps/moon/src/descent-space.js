/* Chapter 5's flight: the powered descent to Taurus-Littrow.
 *
 * Frame: stage 0's site frame (x east, y up, z south at the landing site; the Moon's centre at (0, -r0, 0)), as
 * chapters 3 and 4. The LM's origin (the bottom of its descent stage's engine, vehicle.js makeLM) is what moves.
 *
 * - PDI: 15 km up on the orbit plane of chapter 3 (stage 0's ground track), at the descent orbit's speed, far enough
 *   uprange that the braking needs about the descent engine's full thrust.
 * - P63 (braking) to high gate, P64 (approach) to low gate, then down to 30 m over the landing point: each a cubic
 *   in time from the present state to the phase's target position and velocity (Apollo's guidance flew polynomial
 *   targets too, simplified here): the acceleration needed is 6 (rT - r) / T^2 - (4 v + 2 vT) / T, plus lunar
 *   gravity's opposite for the thrust.
 * - The last 30 m: straight down at 1 m/s, the drift nulled.
 * - P66 by hand (descent.js): the commanded descent rate held, the LM's tilt giving the horizontal acceleration.
 * Timed so the autopilot's contact comes at Apollo 17's (GET 110:21:58), where stage 0's Sun stands.
 * GET = ground elapsed time, seconds from liftoff. */
import { MU_M, HEADING } from './lunar-space.js?v=22';

export const GETD = { pdi: 110 * 3600 + 9 * 60 + 53, land: 110 * 3600 + 21 * 60 + 58 };
GETD.hg = GETD.pdi + 510;                 /* high gate */
GETD.lg = GETD.hg + 125;                  /* low gate */
GETD.over = GETD.land - 29;               /* 30 m over the landing point */
export const PAD_DROP = 0.86 + 0.08;      /* from the LM's origin down to the bottom of its footpads, deployed */

const add = (a, b, k = 1) => [a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k];
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const len = a => Math.sqrt(dot(a, a));
const scale = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
const norm = a => scale(a, 1 / len(a));

export function makeDescent(man, groundAt) {
  const R = man.radius, C = [0, -man.r0, 0];
  const U0 = [0, 1, 0], F0 = [Math.sin(HEADING), 0, -Math.cos(HEADING)];
  const g = r => { const d = sub(r, C), l = len(d); return scale(d, -MU_M / (l * l * l)); };
  /* the height of the LM's footpads over the ground under it, along the vertical (the Moon is round: far from the
   * site its site-frame y is no measure); beyond the terrain levels, over the mean surface */
  function padHeight(s) {
    const rc = len(sub(s.r, C)), x = s.r[0], z = s.r[2];
    const gy = Math.hypot(x, z) < 60000 ? groundAt(x, z) : null;
    const gr = gy === null ? R + man.site.height : len(sub([x, gy, z], C));        /* far out: the site's own level, no jump */
    return rc - gr - PAD_DROP;
  }
  const upAt = r => norm(sub(r, C));
  /* the landing point: the LM's origin over the ground at the site, pads on the ground */
  const LAND = [0, groundAt(0, 0) + PAD_DROP, 0];
  const at = (s, h) => add(add(LAND, F0, s), U0, h);            /* s metres along the track (negative: before), h up */
  const T_HG = { r: at(-11300, 2300), v: add(scale(F0, 150), U0, -42), t: GETD.hg };   /* far enough out that P64 brakes gently: the pitch-over */
  const T_LG = { r: at(-350, 150), v: add(scale(F0, 18), U0, -5), t: GETD.lg };
  const T_OV = { r: at(0, 30), v: scale(U0, -1), t: GETD.over };
  const PHASES = [T_HG, T_LG, T_OV];

  /* the needed thrust acceleration towards a target (cubic in time), and the last 30 m */
  function guide(r, v, t, out) {
    let T = null;
    for (const p of PHASES) if (t < p.t - 1.5) { T = p; break; }
    if (T) {
      const tg = T.t - t, a = sub(scale(sub(T.r, r), 6 / (tg * tg)), scale(add(scale(v, 4), T.v, 2), 1 / tg));
      return sub(a, g(r));
    }
    /* the final descent: 1 m/s straight down, the drift taken out */
    const up = upAt(r), vu = dot(v, up), vh = add(v, up, -vu), dh = add(sub(r, LAND), up, -dot(sub(r, LAND), up));
    const ah = add(scale(vh, -0.8), dh, -0.15);
    return add(add(ah, up, (-1 - vu) * 1.5), g(r), -1);
  }
  /* the orbit before PDI and the whole autopilot flight, once, as a table (every 0.2 s) */
  const pdiAngle = -(469e3 + 11300) / R;
  /* 15 km over the landing site's level (as Apollo measured it; the site lies 2.6 km below the mean radius) */
  const rP = R + man.site.height + 15000;
  const r0 = add(C, add(scale(U0, Math.cos(pdiAngle)), F0, Math.sin(pdiAngle)), rP);
  const vOrb = Math.sqrt(MU_M / rP) * 1.004;
  const v0 = scale(add(scale(U0, -Math.sin(pdiAngle)), F0, Math.cos(pdiAngle)), vOrb);
  const DT = 0.2, T0 = GETD.pdi - 600, rows = [];
  let r = [...r0], v = [...v0], t = GETD.pdi;
  /* back along the coast before PDI (no thrust), so the chapter can start ten minutes earlier */
  { let rb = [...r0], vb = [...v0]; const back = [];
    /* the forward step is v += g dt, r += v dt; undone: r -= v dt, v -= g dt */
    for (let tt = GETD.pdi; tt > T0 - 1e-6; tt -= DT) { back.push({ t: tt, r: rb, v: vb, a: [0, 0, 0] }); rb = add(rb, vb, -DT); vb = add(vb, g(rb), -DT); }
    back.reverse().forEach(x => rows.push(x)); rows.pop(); }
  let contact = null, out = [0, 0, 0];
  while (t < GETD.land + 60) {
    const a = guide(r, v, t, out);
    rows.push({ t, r, v, a });
    const acc = add(a, g(r));
    v = add(v, acc, DT); r = add(r, v, DT); t += DT;
    if (!contact && t > GETD.lg && padHeight({ r }) <= 0.02) contact = t;
    if (contact && t > contact + 2) break;
  }
  const N = rows.length;
  /* the state at time t (autopilot), interpolated */
  function state(tt) {
    const f = (tt - rows[0].t) / DT, i = Math.max(0, Math.min(N - 2, Math.floor(f))), k = Math.max(0, Math.min(1, f - i));
    const a = rows[i], b = rows[i + 1], mix = (x, y) => [x[0] + (y[0] - x[0]) * k, x[1] + (y[1] - x[1]) * k, x[2] + (y[2] - x[2]) * k];
    return { t: tt, r: mix(a.r, b.r), v: mix(a.v, b.v), a: tt < GETD.pdi ? [0, 0, 0] : mix(a.a, b.a) };
  }
  /* by hand (P66): rate held, tilt -> horizontal acceleration. tilt: [forward, right] in radians (LM frame)
   * along the track and across it; returns the next state. */
  function manual(s, dt, rod, tilt, fwd) {
    const up = upAt(s.r), vu = dot(s.v, up), gm = len(g(s.r));
    const right = norm([fwd[1] * up[2] - fwd[2] * up[1], fwd[2] * up[0] - fwd[0] * up[2], fwd[0] * up[1] - fwd[1] * up[0]]);
    const ah = add(scale(fwd, Math.tan(tilt[0]) * gm), right, Math.tan(tilt[1]) * gm);
    const a = add(add(ah, up, gm + (rod - vu) * 1.2), [0, 0, 0]);
    const acc = add(a, g(s.r));
    const v2 = add(s.v, acc, dt), r2 = add(s.r, v2, dt);
    return { t: s.t + dt, r: r2, v: v2, a };
  }
  const info = () => {
    let maxA = 0, maxAt = 0; for (const x of rows) { if (x.t < GETD.pdi) continue; const m = len(x.a); if (m > maxA) { maxA = m; maxAt = x.t; } }
    const sHG = state(GETD.hg), sLG = state(GETD.lg);
    const hgt = s => dot(sub(s.r, C), upAt(s.r)) - R;
    return { contact, maxA, maxAt: maxAt - GETD.pdi, hg: { alt: hgt(sHG) + R - (R + LAND[1] - PAD_DROP), speed: len(sHG.v) }, lg: { alt: sLG.r[1] - LAND[1], speed: len(sLG.v) },
      pdiAlt: hgt(state(GETD.pdi)), pdiSpeed: len(state(GETD.pdi).v), rows: N };
  };

  return { state, manual, guide, info, padHeight, upAt, LAND, F0, U0, C, R, GETD, T0, contact: () => contact };
}
