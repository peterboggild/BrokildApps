/* Chapter 9's flight: the last orbits, and the burn for home.
 *
 * Frame: the Moon's site frame, as chapters 3 to 8 (lunar-space.js), used as an inertial frame.
 *
 * - Until the burn: the circular orbit of chapter 8, 113 km up in the same plane, timed so that the middle of the
 *   burn comes at the middle of the far side (Apollo's transearth injection was made behind the Moon, out of touch).
 * - The LM's ascent stage is cast off at Apollo 17's time (GET 191:18:31); where it goes is leaving.js's business.
 * - Transearth injection at GET 234:02:09: the main engine along the motion for 144 s (Apollo 17: 143.7 s), stepped
 *   as chapter 3's burns are, its thrust scaled for the real gain of 928 m/s.
 * - After it: the hyperbola away from the Moon, two-body, exact at any time warp.
 * GET = ground elapsed time, seconds from the launch from Earth. */
import { MU_M, conic, burn } from './lunar-space.js?v=41';

export const GETT = { jett: 191 * 3600 + 18 * 60 + 31, tei: 234 * 3600 + 2 * 60 + 9, burn: 144 };
GETT.teiEnd = GETT.tei + GETT.burn;

const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const len = a => Math.sqrt(dot(a, a));

/* L: chapter 3's flight (lunar-space.js makeLunarFlight), for the plane, the Moon and the Earth's direction */
export function makeLeaving(L) {
  const R = L.R, rO = R + 113e3, n = Math.sqrt(MU_M / (rO * rO * rO)), vO = n * rO, E = L.EARTH;
  const U0 = L.dirAt(0), F0 = L.dirAt(Math.PI / 2);
  const angleOf = r => Math.atan2(dot(r, F0), dot(r, U0));
  const aFar = angleOf([-E[0], -E[1], -E[2]]);                         /* the far side's middle, as near as the plane passes it */
  const circ = t => { const a = aFar + n * (t - (GETT.tei + GETT.burn / 2)), u = L.dirAt(a), f = L.dirAt(a + Math.PI / 2); return { r: u.map(x => x * rO), v: f.map(x => x * vO) }; };
  const s0 = circ(GETT.tei);
  const gain = k => len(burn(s0, GETT.tei, GETT.burn, -k, 5.6, 1.3).v) - vO;
  let lo = 0.3, hi = 2; for (let i = 0; i < 50; i++) { const m = (lo + hi) / 2; if (gain(m) > 928) hi = m; else lo = m; }
  const K = (lo + hi) / 2, sB = burn(s0, GETT.tei, GETT.burn, -K, 5.6, 1.3), away = conic(sB.r, sB.v, GETT.teiEnd);
  function state(t) {
    if (t < GETT.tei) return circ(t);
    if (t < GETT.teiEnd) return burn(s0, GETT.tei, GETT.burn, -K, 5.6, 1.3, t);
    return away.at(t);
  }
  const hidden = r => { const along = dot(r, E); return along < 0 && Math.hypot(r[0] - E[0] * along, r[1] - E[1] * along, r[2] - E[2] * along) < R; };
  function where(t) {
    const s = state(t), r = len(s.r);
    return { r: s.r, v: s.v, rC: r, alt: r - R, speed: len(s.v), up: s.r.map(x => x / r), pos: [L.C[0] + s.r[0], L.C[1] + s.r[1], L.C[2] + s.r[2]],
      angle: angleOf(s.r), hidden: hidden(s.r), engine: t >= GETT.tei && t < GETT.teiEnd ? 1 : 0 };
  }
  /* the last loss of signal before the burn, and the signal's return after it */
  let los = GETT.tei, aos = GETT.teiEnd;
  for (let t = GETT.tei; t > GETT.tei - 4000; t -= 2) if (!where(t).hidden) { los = t + 2; break; }
  for (let t = GETT.teiEnd; t < GETT.teiEnd + 6000; t += 2) if (!where(t).hidden) { aos = t; break; }
  const EV = { jett: GETT.jett, los, tei: GETT.tei, teiEnd: GETT.teiEnd, aos };
  const info = () => ({ K, gain: gain(K), vinf: Math.sqrt(Math.max(0, dot(sB.v, sB.v) - 2 * MU_M / len(sB.r))), e: away.e, altEnd: len(sB.r) - R, los: los - GETT.tei, aos: aos - GETT.teiEnd,
    after1h: where(GETT.teiEnd + 3600).alt, after90: where(GETT.teiEnd + 5400).alt, earthAngle: Math.acos(dot(where(GETT.teiEnd + 5400).up, E)) * 180 / Math.PI });
  return { where, EV, info, R, C: L.C, EARTH: E, NORMAL: L.NORMAL, sunOffset: L.sunOffset, dirAt: L.dirAt };
}
