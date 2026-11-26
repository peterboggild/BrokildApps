/* Chapter 2's flight: Earth orbit, the trans-lunar injection burn, and the coast towards the Moon.
 *
 * Frame: the launch's pad frame, frozen at liftoff and used as an inertial frame (x east, y up, z south at the pad,
 * metres; the Earth's centre at (0, -RE, 0)). The Earth turns in it (15 degrees an hour), the Sun stays put.
 *
 * - Parking orbit: circular, 167 km, in the plane of the ascent's final position and velocity (ascent.js at T+11:53).
 * - TLI: the S-IVB's second burn at GET 3:12:36, 5 min 51 s, integrated step by step (thrust along the velocity,
 *   gravity); its thrust is scaled so that the coast ellipse reaches 410,000 km.
 * - Coast: two-body Kepler propagation from the burn's end (exact at any time warp). The Moon moves on a circle of
 *   384,400 km in the same plane, placed so that the spacecraft meets it; its pull is not modelled in this chapter,
 *   which ends where the Moon's gravity takes over (66,000 km from it).
 * GET = ground elapsed time, seconds from liftoff. */
import { state as ascentState, RE } from './ascent.js?v=36';

export const MU = 3.986004418e14, R_PARK = RE + 167000, R_MOON_ORBIT = 384400e3, R_MOON = 1737.4e3, SOI = 66000e3;
export const W_EARTH = 7.2921159e-5, W_MOON = 2 * Math.PI / (27.3217 * 86400);
export const GET = { insertion: 713, tli: 3 * 3600 + 12 * 60 + 36, tliBurn: 351, sep: 3 * 3600 + 42 * 60, dock: 3 * 3600 + 57 * 60, extract: 4 * 3600 + 45 * 60 };
GET.tliEnd = GET.tli + GET.tliBurn;

const add = (a, b, k = 1) => [a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const len = a => Math.sqrt(dot(a, a));
const scale = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = a => scale(a, 1 / len(a));

/* the parking orbit: unit vectors in its plane, from the ascent's last state */
const s0 = ascentState(713);
const U0 = norm([s0.pos[0], s0.pos[1] + RE, s0.pos[2]]);
const F0 = norm(add(s0.fwd, U0, -dot(s0.fwd, U0)));
export const NORMAL = norm(cross(U0, F0));
const N_PARK = Math.sqrt(MU / R_PARK ** 3), V_PARK = Math.sqrt(MU / R_PARK);
function parking(t) {
  const th = N_PARK * (t - GET.insertion), c = Math.cos(th), s = Math.sin(th);
  return { r: add(scale(U0, R_PARK * c), F0, R_PARK * s), v: add(scale(U0, -V_PARK * s), F0, V_PARK * c) };
}

/* the burn: thrust grows as the stage gets lighter (about 7 to 13 m/s^2); k scales it */
function burn(k, until = GET.tliEnd, out = null) {
  let { r, v } = parking(GET.tli), t = GET.tli;
  const dt = 0.5;
  while (t < until - 1e-9) {
    const h = Math.min(dt, until - t);
    const f = (t - GET.tli) / GET.tliBurn, a = k * (7 + 6 * f);
    const acc = rr => { const d = len(rr), g = scale(rr, -MU / (d * d * d)); return g; };
    /* velocity Verlet, thrust along the velocity */
    const th = scale(norm(v), a);
    const a0 = add(acc(r), th);
    r = add(add(r, v, h), a0, 0.5 * h * h);
    const a1 = add(acc(r), scale(norm(v), a));
    v = add(v, add(a0, a1), 0.5 * h);
    t += h;
  }
  return { r, v };
}
function apogee(r, v) { const E = dot(v, v) / 2 - MU / len(r); if (E >= 0) return Infinity; const a = -MU / (2 * E), h = len(cross(r, v)), e = Math.sqrt(Math.max(0, 1 - h * h / (MU * a))); return a * (1 + e); }
/* find the thrust scale for an apogee of 410,000 km (secant) */
let kLo = 0.4, kHi = 1.2;
for (let i = 0; i < 40; i++) { const k = (kLo + kHi) / 2, b = burn(k); if (apogee(b.r, b.v) > 410e6) kHi = k; else kLo = k; }
const k1 = (kLo + kHi) / 2;
export const K_TLI = k1;
const END = burn(K_TLI);

/* Kepler elements of the coast, and propagation by Kepler's equation (elliptic) */
const coast = (() => {
  const r0 = END.r, v0 = END.v, h = cross(r0, v0), E = dot(v0, v0) / 2 - MU / len(r0), a = -MU / (2 * E);
  const evec = add(scale(cross(v0, h), 1 / MU), norm(r0), -1), e = len(evec);
  const P = norm(evec), Qv = norm(cross(h, P)), n = Math.sqrt(MU / a ** 3);
  const nu0 = Math.atan2(dot(r0, Qv), dot(r0, P)), E0 = 2 * Math.atan(Math.sqrt((1 - e) / (1 + e)) * Math.tan(nu0 / 2)), M0 = E0 - e * Math.sin(E0);
  return { a, e, P, Q: Qv, n, M0, t0: GET.tliEnd };
})();
function kepler(t) {
  const { a, e, P, Q, n, M0, t0 } = coast;
  const M = M0 + n * (t - t0);
  let E = M; for (let i = 0; i < 30; i++) { const d = (E - e * Math.sin(E) - M) / (1 - e * Math.cos(E)); E -= d; if (Math.abs(d) < 1e-12) break; }
  const c = Math.cos(E), s = Math.sin(E), b = a * Math.sqrt(1 - e * e);
  const x = a * (c - e), y = b * s, rr = a * (1 - e * c), vx = -a * n * s / (1 - e * c), vy = b * n * c / (1 - e * c);
  return { r: add(scale(P, x), Q, y), v: add(scale(P, vx), Q, vy) };
}

/* the Moon: placed so the coast meets it at the time the spacecraft is 384,400 km out; it moves on its orbit */
const T_MEET = (() => { let lo = GET.tliEnd, hi = GET.tliEnd + 6 * 86400; for (let i = 0; i < 60; i++) { const m = (lo + hi) / 2; if (len(kepler(m).r) < R_MOON_ORBIT) lo = m; else hi = m; } return lo; })();
const MEET = norm(kepler(T_MEET).r);
const MEET_T = norm(cross(NORMAL, MEET));
export function moonPos(t) {                     /* from the Earth's centre */
  const ang = W_MOON * (t - T_MEET) + 6000e3 / R_MOON_ORBIT;          /* 6,000 km ahead of the meeting point */
  return add(scale(MEET, R_MOON_ORBIT * Math.cos(ang)), MEET_T, R_MOON_ORBIT * Math.sin(ang));
}
/* the chapter ends where the Moon's gravity takes over */
export const T_SOI = (() => { let lo = GET.tliEnd, hi = T_MEET; for (let i = 0; i < 60; i++) { const m = (lo + hi) / 2; if (len(add(kepler(m).r, moonPos(m), -1)) > SOI) lo = m; else hi = m; } return lo; })();

/* the spacecraft at time t: position from the Earth's centre and velocity, plus what the views need */
export function where(t) {
  const sv = t < GET.tli ? parking(t) : t < GET.tliEnd ? burn(K_TLI, t) : kepler(t);
  const r = len(sv.r);
  return { rE: sv.r, v: sv.v, alt: r - RE, speed: len(sv.v), up: scale(sv.r, 1 / r),
    pos: [sv.r[0], sv.r[1] - RE, sv.r[2]],                         /* in the pad frame, as earth.js wants it */
    moon: moonPos(t), dMoon: len(add(moonPos(t), sv.r, -1)) };
}
export const INFO = { K_TLI, T_MEET, T_SOI, coast };
