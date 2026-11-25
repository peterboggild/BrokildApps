/* Chapter 3's flight round the Moon: the approach, lunar orbit insertion, the first orbits and descent orbit
 * insertion.
 *
 * Frame: the Moon's site frame of moon.js (x east, y up, z south at the landing site, metres; the Moon's centre at
 * (0, -r0, 0)), used as an inertial frame for the chapter. The Moon turns 0.55 degrees an hour under a true
 * inertial frame; that is left out, so every orbit passes over Taurus-Littrow, as it nearly did. The Sun moves
 * instead, as moon.js's setSunOffset does (0.508 degrees an hour), and stands where stage 0 baked it at the landing.
 *
 * - The orbit plane passes over the landing site with stage 0's ground track (heading -82 degrees: west, a little
 *   north, as Apollo 17 came in from the east).
 * - The approach: a hyperbola from where chapter 2 ends (66,000 km from the Moon at T_SOI) to a perilune of 108 km (97 km once the burn is done)
 *   on the far side, timed to Apollo 17's LOI (ignition GET 88:54:22, 6 min 33 s).
 * - LOI: the main engine against the motion, integrated step by step, its thrust scaled so the orbit's high point is
 *   315 km (Apollo 17: 315 by 97 km).
 * - DOI: 22 s at the next-but-one perilune (GET 93:13, behind the Moon), scaled for a low point of 15 km eight
 *   degrees before the site, where the powered descent will begin (stage 0's low pass).
 * - Between burns: two-body Kepler propagation, exact at any time warp.
 * GET = ground elapsed time, seconds from liftoff. */
import { T_SOI } from './space.js?v=20';

export const MU_M = 4.9028e12, SOI = 66000e3;
const D = Math.PI / 180;
export const GETL = { loi: 88 * 3600 + 54 * 60 + 22, loiBurn: 393, doiNear: 93 * 3600 + 11 * 60, doiBurn: 22, land: 110 * 3600 + 21 * 60 + 58 };
export const HEADING = -82 * D;
const PERI_DOI = -8 * D;                         /* the descent orbit's low point: 8 degrees before the site */

const add = (a, b, k = 1) => [a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const len = a => Math.sqrt(dot(a, a));
const scale = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = a => scale(a, 1 / len(a));

/* a two-body conic from a state (r, v relative to the Moon's centre) at time t0: ellipse or hyperbola */
function conic(r0, v0, t0) {
  const h = cross(r0, v0), r = len(r0), E = dot(v0, v0) / 2 - MU_M / r, a = -MU_M / (2 * E);
  const evec = add(scale(cross(v0, h), 1 / MU_M), norm(r0), -1), e = len(evec);
  const P = norm(evec), Qv = norm(cross(h, P));
  const nu0 = Math.atan2(dot(r0, Qv), dot(r0, P));
  if (e < 1) {
    const n = Math.sqrt(MU_M / a ** 3), b = a * Math.sqrt(1 - e * e);
    const E0 = 2 * Math.atan(Math.sqrt((1 - e) / (1 + e)) * Math.tan(nu0 / 2)), M0 = E0 - e * Math.sin(E0);
    return { a, e, P, Q: Qv, n, M0, t0, ellipse: true,
      at(t) {
        const M = M0 + n * (t - t0);
        let Ea = M; for (let i = 0; i < 40; i++) { const d = (Ea - e * Math.sin(Ea) - M) / (1 - e * Math.cos(Ea)); Ea -= d; if (Math.abs(d) < 1e-13) break; }
        const c = Math.cos(Ea), s = Math.sin(Ea), k = 1 - e * c;
        return { r: add(scale(P, a * (c - e)), Qv, b * s), v: add(scale(P, -a * n * s / k), Qv, b * n * c / k) };
      },
      /* the times of perilune after t */
      periAfter(t) { const M = M0 + n * (t - t0), k = Math.ceil(M / (2 * Math.PI)); return t + (2 * Math.PI * k - M) / n; }
    };
  }
  const A = -a, n = Math.sqrt(MU_M / A ** 3), s1 = Math.sqrt(e * e - 1);
  const F0 = 2 * Math.atanh(Math.sqrt((e - 1) / (e + 1)) * Math.tan(nu0 / 2)), M0 = e * Math.sinh(F0) - F0;
  return { a, e, P, Q: Qv, n, M0, t0, ellipse: false,
    at(t) {
      const M = M0 + n * (t - t0);
      let F = Math.asinh(M / e); for (let i = 0; i < 60; i++) { const d = (e * Math.sinh(F) - F - M) / (e * Math.cosh(F) - 1); F -= d; if (Math.abs(d) < 1e-13) break; }
      const ch = Math.cosh(F), sh = Math.sinh(F), Fd = n / (e * ch - 1);
      return { r: add(scale(P, A * (e - ch)), Qv, A * s1 * sh), v: add(scale(P, -A * sh * Fd), Qv, A * s1 * ch * Fd) };
    }
  };
}

/* a burn against the motion, step by step from state s at time t0 for dur seconds; acceleration k * (a0 + a1 * f) */
function burn(s, t0, dur, k, a0, a1, until = t0 + dur) {
  let r = s.r, v = s.v, t = t0;
  const g = rr => { const d = len(rr); return scale(rr, -MU_M / (d * d * d)); };
  while (t < until - 1e-9) {
    const h = Math.min(0.25, until - t), f = (t - t0) / dur;
    const th0 = scale(norm(v), -k * (a0 + a1 * f));
    const acc0 = add(g(r), th0);
    r = add(add(r, v, h), acc0, 0.5 * h * h);
    const acc1 = add(g(r), scale(norm(v), -k * (a0 + a1 * (f + h / dur))));
    v = add(v, add(acc0, acc1), 0.5 * h);
    t += h;
  }
  return { r, v };
}
const apsides = s => { const c = conic(s.r, s.v, 0); return c.ellipse ? [c.a * (1 - c.e), c.a * (1 + c.e)] : [c.a * (1 - c.e), Infinity]; };
const bisect = (lo, hi, f, n = 50) => { for (let i = 0; i < n; i++) { const m = (lo + hi) / 2; if (f(m)) hi = m; else lo = m; } return (lo + hi) / 2; };

export function makeLunarFlight(man) {
  const R = man.radius, C = [0, -man.r0, 0];
  const U0 = [0, 1, 0], F0 = [Math.sin(HEADING), 0, -Math.cos(HEADING)];
  const NORMAL = cross(U0, F0);
  const dirAt = a => add(scale(U0, Math.cos(a)), F0, Math.sin(a));
  const angleOf = r => Math.atan2(dot(r, F0), dot(r, U0));             /* along the orbit from the site, radians */
  const RP = R + 108e3, tP = GETL.loi + GETL.loiBurn / 2;
  const doiI = t => t - GETL.doiBurn / 2;

  /* the whole flight for a given LOI perilune angle; the angle is then tuned so DOI's low point lands at PERI_DOI */
  function build(aLoi) {
    /* the approach: v-infinity such that the fall from the sphere of influence to perilune takes T_SOI -> tP */
    const tof = vi => { const a = -MU_M / (vi * vi), e = 1 + RP * vi * vi / MU_M, F = Math.acosh((1 + SOI * vi * vi / MU_M) / e); return Math.sqrt((-a) ** 3 / MU_M) * (e * Math.sinh(F) - F); };
    const vinf = bisect(200, 4000, vi => tof(vi) < tP - T_SOI);
    const e = 1 + RP * vinf * vinf / MU_M;
    const hyp = conic(scale(dirAt(aLoi), RP), scale(dirAt(aLoi + Math.PI / 2), Math.sqrt(MU_M * (1 + e) / RP)), tP);
    /* LOI */
    const s0 = hyp.at(GETL.loi);
    const kL = bisect(0.5, 2, k => apsides(burn(s0, GETL.loi, GETL.loiBurn, k, 1.95, 0.85))[1] < R + 315e3);
    const sL = burn(s0, GETL.loi, GETL.loiBurn, kL, 1.95, 0.85);
    const loiOrbit = conic(sL.r, sL.v, GETL.loi + GETL.loiBurn);
    /* DOI at the perilune nearest Apollo 17's time */
    let tp = loiOrbit.periAfter(GETL.doiNear - 4000); if (Math.abs(loiOrbit.periAfter(tp + 1) - GETL.doiNear) < Math.abs(tp - GETL.doiNear)) tp = loiOrbit.periAfter(tp + 1);
    const tD = doiI(tp), sD0 = loiOrbit.at(tD);
    const kD = bisect(0.2, 3, k => apsides(burn(sD0, tD, GETL.doiBurn, k, 2.9, 0.05))[0] < R + 15e3);
    const sD = burn(sD0, tD, GETL.doiBurn, kD, 2.9, 0.05);
    const doiOrbit = conic(sD.r, sD.v, tD + GETL.doiBurn);
    const periAngle = angleOf(doiOrbit.P);
    return { vinf, hyp, kL, sL, loiOrbit, tD, kD, sD0, sD, doiOrbit, periAngle };
  }
  let aLoi = Math.PI + PERI_DOI, F = build(aLoi);
  for (let i = 0; i < 4; i++) { const err = Math.atan2(Math.sin(F.periAngle - PERI_DOI), Math.cos(F.periAngle - PERI_DOI)); aLoi -= err; F = build(aLoi); }

  const EV = { soi: T_SOI, loi: GETL.loi, loiEnd: GETL.loi + GETL.loiBurn, doi: F.tD, doiEnd: F.tD + GETL.doiBurn, land: GETL.land };
  function state(t) {
    if (t < EV.loi) return F.hyp.at(t);
    if (t < EV.loiEnd) return burn(F.hyp.at(EV.loi), EV.loi, GETL.loiBurn, F.kL, 1.95, 0.85, t);
    if (t < EV.doi) return F.loiOrbit.at(t);
    if (t < EV.doiEnd) return burn(F.sD0, EV.doi, GETL.doiBurn, F.kD, 2.9, 0.05, t);
    return F.doiOrbit.at(t);
  }
  /* the Earth: over the Moon's 0N 0E point (libration left out) */
  const la = man.site.lat * D, lo = man.site.lon * D;
  const EARTH = norm([-Math.sin(lo), Math.cos(la) * Math.cos(lo), Math.sin(la) * Math.cos(lo)]);
  /* hidden from the Earth behind the Moon (radio silence) */
  const hidden = r => { const along = dot(r, EARTH); return along < 0 && len(add(r, EARTH, -along)) < R; };

  function where(t) {
    const s = state(t), r = len(s.r);
    return { r: s.r, v: s.v, rC: r, alt: r - R, speed: len(s.v), up: scale(s.r, 1 / r), pos: add(C, s.r),
      angle: angleOf(s.r), hidden: hidden(s.r), engine: (t > EV.loi && t < EV.loiEnd) || (t > EV.doi && t < EV.doiEnd) ? 1 : 0 };
  }
  /* times when the radio goes and comes back, and the passes over the site, between two times */
  function crossings(t0, t1, step = 20) {
    const out = { los: [], aos: [], site: [] };
    let prev = where(t0);
    for (let t = t0 + step; t <= t1; t += step) {
      const w = where(t);
      if (!prev.hidden && w.hidden) out.los.push(t);
      if (prev.hidden && !w.hidden) out.aos.push(t);
      if (prev.angle < 0 && w.angle >= 0 && Math.abs(w.angle) < 1) out.site.push(t);
      prev = w;
    }
    return out;
  }
  /* the Sun of stage 0 at landing, rotated back by the time before it (moon.js setSunOffset, degrees) */
  const sunOffset = t => 0.508 * (t - GETL.land) / 3600;
  const info = () => ({ vinf: F.vinf, kL: F.kL, kD: F.kD, aLoi: aLoi / D, periDoi: F.periAngle / D,
    loi: apsides(F.sL).map(x => (x - R) / 1000), doi: apsides(F.sD).map(x => (x - R) / 1000),
    loiPeriod: F.loiOrbit.ellipse ? 2 * Math.PI / F.loiOrbit.n : null, doiPeriod: 2 * Math.PI / F.doiOrbit.n });
  return { where, EV, EARTH, NORMAL, C, R, crossings, sunOffset, info, dirAt };
}
