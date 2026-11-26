/* Chapter 10's flight: the coast home.
 *
 * Frame: chapter 2's (space.js: the launch's pad frame frozen at liftoff, the Earth's centre at (0, -RE, 0)).
 *
 * The way home is the way out, flown backwards: the same long ellipse between the Earth and the Moon's distance,
 * with the velocity turned round, timed so that it comes down to the Earth at Apollo 17's entry (GET 301:38:38).
 * The two coasts were nearly of a length (68.5 hours out, 67.6 home), so the Earth grows on the way home as it
 * shrank on the way out. The Moon is where chapter 2 has it for the mirrored time; its pull is not modelled.
 * The chapter ends 25,000 km up, a little over an hour before the entry, which is chapter 11's.
 * GET = ground elapsed time, seconds from the launch from Earth. */
import { where as out, GET } from './space.js?v=39';

export const GETH = { start: 250 * 3600, eva: 254 * 3600 + 54 * 60, entry: 301 * 3600 + 38 * 60 + 38 };
const mirror = t => GET.tliEnd + 400 + (GETH.entry - t);

export function where(t) {
  const o = out(mirror(t));
  return { rE: o.rE, v: [-o.v[0], -o.v[1], -o.v[2]], alt: o.alt, speed: o.speed, up: o.up, pos: o.pos, moon: o.moon, dMoon: o.dMoon };
}
/* where the chapter ends: 25,000 km above the Earth */
GETH.end = (() => { let lo = GETH.start, hi = GETH.entry; for (let i = 0; i < 50; i++) { const m = (lo + hi) / 2; if (where(m).alt > 25e6) lo = m; else hi = m; } return Math.round(lo); })();
