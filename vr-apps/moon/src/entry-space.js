/* Chapters 11 and 12's flight: the entry, the parachutes and the splashdown.
 *
 * Frame: chapter 2's and 10's (space.js: the launch's pad frame frozen at liftoff, the Earth's centre at
 * (0, -RE, 0)); the Earth's turning under the Command Module is left out.
 *
 * - The entry interface: 122 km up (400,000 ft) at 11.0 km/s, 6.5 degrees below the horizontal, at Apollo 17's time
 *   (GET 301:38:38). Where: on the morning side of the Earth, the Sun 8 degrees up at the interface and ahead, so
 *   that it climbs as you come down and the sea is in morning light at the splash (Apollo 17 came down in the
 *   Pacific at half past eight in the morning, local time). Before it: the same path without air, stepped backwards
 *   (the Service Module is cast off a quarter of an hour before).
 * - In the air: the Command Module alone, 5,500 kg behind a heat shield 3.9 m across. Drag by an exponential
 *   atmosphere (7.1 km scale height); lift a third of the drag (the capsule flies a little nose-up of its path,
 *   its weight off-centre), turned up or down by rolling: up at first, to pull out of the dive; down when it would
 *   climb out again; then up or down to hold a gentle descent. That gives the entry's shape: a first peak of six
 *   to seven g a minute and a half in, a long glide near 60 km, a second, lower peak.
 * - The parachutes: the two drogues at 7.3 km, the three mains at 3.2 km, the sea at 8.5 m/s, a little over
 *   thirteen minutes after the interface (Apollo 17: 13 min 21 s).
 * The whole flight is worked out once, as a table. GET = ground elapsed time, seconds from the launch from Earth. */
import { RE, SUN } from './ascent.js?v=39';
import { MU } from './space.js?v=39';
import { where as coast, GETH } from './home-space.js?v=39';

export const GETE = { ei: GETH.entry, sep: GETH.entry - 15 * 60 };

const add = (a, b, k = 1) => [a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const len = a => Math.sqrt(dot(a, a));
const scale = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
const norm = a => scale(a, 1 / len(a));

const H_EI = 122e3, V_EI = 11000, GAMMA = 6.5 * Math.PI / 180;
const MASS = 5500, AREA = Math.PI * 1.956 * 1.956, CD = 1.3, LD = 0.32, RHO0 = 1.225, HS = 7100;
const H_DROGUE = 7300, H_MAIN = 3200, V_SPLASH = 8.5, G0 = 9.80665;
const rho = h => RHO0 * Math.exp(-Math.max(0, h) / HS);

export function makeEntry() {
  /* the interface: the Sun 8 degrees up and dead ahead, on the side of the Earth the coast comes down to */
  const c = coast(GETH.entry), sun = norm(SUN), side = norm(add(norm(c.rE), sun, -dot(norm(c.rE), sun))), EL = 8 * Math.PI / 180;
  const up0 = norm(add(scale(sun, Math.sin(EL)), side, Math.cos(EL))), h0 = norm(add(sun, up0, -dot(sun, up0)));
  const r0 = scale(up0, RE + H_EI), v0 = add(scale(h0, V_EI * Math.cos(GAMMA)), up0, -V_EI * Math.sin(GAMMA));
  const g = r => { const d = len(r); return scale(r, -MU / (d * d * d)); };
  const DT = 0.25, rows = [];
  /* before the interface: no air, stepped backwards */
  { let r = r0, v = v0; const back = [];
    for (let t = GETE.ei; t > GETE.ei - 1500; t -= DT) { back.push({ t, r, v, a: [0, 0, 0], q: 0, chute: 0 }); r = add(r, v, -DT); v = add(v, g(r), -DT); }
    back.reverse().forEach(x => rows.push(x)); rows.pop(); }
  /* in the air */
  let r = r0, v = v0, t = GETE.ei, bank = 1, drogue = null, main = null, splash = null, peak = 0, peakAt = 0, maxAlt2 = 0;
  while (t < GETE.ei + 1500) {
    const d = len(r), alt = d - RE, up = scale(r, 1 / d), sp = len(v), vh = norm(v), hdot = dot(v, up);
    if (!drogue && alt < H_DROGUE) drogue = t;
    if (!main && alt < H_MAIN) main = t;
    if (alt <= 0) { splash = t; break; }
    /* the air's force: drag along the motion; lift square to it, in the vertical plane, up or down by the roll.
     * Under the parachutes: drag that settles to the parachutes' own speed. */
    let aero = [0, 0, 0], chute = 0;
    const qd = 0.5 * rho(alt) * sp * sp;
    if (!drogue) {
      const D = qd * CD * AREA / MASS, ln = norm(cross(cross(vh, up), vh));         /* square to the motion, upward */
      /* the roll: lift up to pull out; down when it would climb out of the air again; then holding 40 m/s down */
      const want = sp > 7600 ? (hdot > -20 ? -1 : 1) : (hdot < -45 ? 1 : hdot > -25 ? -0.4 : 0.3);
      bank += Math.max(-DT / 8, Math.min(DT / 8, want - bank));                      /* (a roll takes its time) */
      aero = add(scale(vh, -D), ln, D * LD * bank);
    } else {
      const vt = main ? V_SPLASH + (60 - V_SPLASH) * Math.exp(-(t - main) / 3.5) : 60 + Math.max(0, 110 - 60) * Math.exp(-(t - drogue) / 4);
      const k = (G0 * 0.999) / (vt * vt);                                             /* drag = g at the settling speed */
      aero = scale(vh, -k * sp * sp); chute = main ? 2 : 1;
    }
    const gl = len(aero) / G0;
    if (gl > peak) { peak = gl; peakAt = t; }
    if (t > GETE.ei + 150 && !drogue) maxAlt2 = Math.max(maxAlt2, alt);
    rows.push({ t, r, v, a: aero, q: Math.sqrt(rho(alt)) * sp * sp * sp, chute });
    v = add(v, add(aero, g(r)), DT); r = add(r, v, DT); t += DT;
  }
  const T0 = rows[0].t, N = rows.length, QMAX = rows.reduce((m, x) => Math.max(m, x.q), 0);
  GETE.drogue = drogue; GETE.main = main; GETE.splash = splash; GETE.peak = peakAt;
  /* the radio is lost while the air about the capsule glows hottest (from about 90 km to 55 km the first time down) */
  let b0 = null, b1 = null;
  for (const x of rows) { const k = x.q / QMAX; if (b0 === null && k > 0.12) b0 = x.t; if (b0 !== null && b1 === null && x.t > b0 + 60 && k < 0.1) b1 = x.t; }
  GETE.blackout = b0; GETE.blackoutEnd = b1 || b0 + 200;
  function state(tt) {
    const f = (tt - T0) / DT, i = Math.max(0, Math.min(N - 2, Math.floor(f))), k = Math.max(0, Math.min(1, f - i));
    const a = rows[i], b = rows[i + 1], mix = (x, y) => [x[0] + (y[0] - x[0]) * k, x[1] + (y[1] - x[1]) * k, x[2] + (y[2] - x[2]) * k];
    return { t: tt, r: mix(a.r, b.r), v: mix(a.v, b.v), a: mix(a.a, b.a), q: (a.q + (b.q - a.q) * k) / QMAX, chute: a.chute };
  }
  /* as the views want it (space.js where): the pad frame, the height, the speed, and what the capsule feels */
  function where(tt) {
    const s = state(Math.min(tt, splash)), d = len(s.r), up = scale(s.r, 1 / d);
    return { rE: s.r, v: s.v, alt: d - RE, speed: len(s.v), up, pos: [s.r[0], s.r[1] - RE, s.r[2]], a: s.a, g: len(s.a) / G0, heat: s.q, chute: s.chute,
      climb: dot(s.v, up), down: tt >= splash };
  }
  const info = () => ({ peak, peakAt: peakAt - GETE.ei, drogue: drogue - GETE.ei, main: main - GETE.ei, splash: splash - GETE.ei, blackout: [GETE.blackout - GETE.ei, GETE.blackoutEnd - GETE.ei],
    sunAtEntry: Math.asin(dot(up0, sun)) * 180 / Math.PI, sunAtSplash: Math.asin(dot(norm(state(splash).r), sun)) * 180 / Math.PI, loft: maxAlt2, vDrogue: len(state(drogue).v), vSplash: len(state(splash - 1).v), range: Math.acos(Math.min(1, dot(norm(state(splash).r), up0))) * RE, rows: N });
  return { where, state, info, GETE, T0, end: splash };
}
