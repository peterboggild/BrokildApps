/* The Saturn V's ascent to orbit, and where you are and which way you face at any moment.
 *
 * Profile: altitude, speed and pitch against time, approximating published Apollo flight data (Apollo 17 event
 * times; the flight path a smoothed version of a typical Apollo ascent). Downrange distance is integrated from
 * speed and pitch. Not simulated: the vehicle follows the table.
 *
 * Frame: the pad frame. Origin on the ground under the launch vehicle, x east, y up, z south, metres; the Earth's
 * centre at (0, -RE, 0). The spacecraft is EYE_H above the ground on the pad.
 */

export const RE = 6371000;
export const EYE_H = 105.6;                        /* the Commander's eyes above the pad surface (pad.js stacks the Saturn V to it) */
export const PAD = { lat: 28.608, lon: -80.604 };  /* Launch Complex 39A */
const AZ = 72 * Math.PI / 180;                     /* launch azimuth, from north towards east */

/* t (s from liftoff): altitude (m), speed (m/s), pitch from the vertical (deg) */
const T =  [0,  10,  20,   30,   45,   60,   75,    83,    100,   120,   140,   161,   165,   197,   240,    300,    360,    420,    480,    548,    563,    600,    660,    713,    800];
const ALT = [0, 100, 450,  1200, 3000, 6500, 10800, 13300, 21000, 31000, 42000, 67000, 69500, 91000, 115000, 138000, 154000, 165000, 171000, 175000, 174500, 172000, 169000, 167000, 167000];
const V =  [0,  25,  90,   160,  290,  420,  560,   650,   900,   1250,  1650,  2400,  2410,  2500,  2900,   3500,   4100,   4800,   5600,   6600,   6600,   6950,   7400,   7800,   7800];
const PITCH = [0, 0, 2,    5,    12,   22,   30,    34,    45,    55,    62,    68,    68.5,  72,    78,     83,     86,     88,     89,     90,     90,     90,     90,     90,     90];

function interp(xs, ys, x) {
  if (x <= xs[0]) return ys[0];
  if (x >= xs[xs.length - 1]) return ys[ys.length - 1];
  let i = 1; while (xs[i] < x) i++;
  /* monotone cubic (Fritsch-Carlson) would be nicer; smoothstep between knots is enough for a view */
  const f = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
  return ys[i - 1] + (ys[i] - ys[i - 1]) * f;
}
/* downrange: integrate v sin(pitch) once, 0.25 s steps */
const DR_DT = 0.25, DR = [0];
for (let t = DR_DT; t <= 800; t += DR_DT) {
  const tm = t - DR_DT / 2;
  DR.push(DR[DR.length - 1] + interp(T, V, tm) * Math.sin(interp(T, PITCH, tm) * Math.PI / 180) * DR_DT);
}
function downrange(t) {
  if (t <= 0) return 0;
  const k = Math.min(DR.length - 2, Math.floor(t / DR_DT)), f = t / DR_DT - k;
  return DR[k] + (DR[k + 1] - DR[k]) * f;
}

/* Events. `t` from liftoff. */
export const EVENTS = {
  ignition: -8.9, liftoff: 0, towerClear: 10, rollStart: 13, rollEnd: 31, mach1: 66, maxQ: 83,
  iCutoff: 139, sicCutoff: 161, siiIgnition: 162.5, skirtSep: 191, towerJett: 197.8,
  siiCECO: 460, siiCutoff: 559, sivbIgnition: 563, seco: 713
};

export const PHASES = [
  [-Infinity, 'pad'], [EVENTS.ignition, 'ignition'], [0, 'first stage'], [EVENTS.sicCutoff, 'staging'],
  [EVENTS.siiIgnition, 'second stage'], [EVENTS.siiCutoff, 'staging'], [EVENTS.sivbIgnition, 'third stage'],
  [EVENTS.seco, 'orbit']
];
export function phase(t) { let p = 'pad'; for (const [t0, name] of PHASES) if (t >= t0) p = name; return p; }

const smooth = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };

/* The state at time t: position of your eyes (pad frame, metres, float64), the local vertical there, nose and
 * head directions (unit), altitude, speed, downrange. Before liftoff: on the pad. */
export function state(t, out = {}) {
  const tt = Math.max(0, t);
  /* the first seconds: holddown release, then a slow climb clear of the tower */
  const h = t <= 0 ? 0 : tt < 10 ? 100 * Math.pow(tt / 10, 2) : interp(T, ALT, tt);
  const s = downrange(tt), th = s / RE;
  const dx = Math.sin(AZ), dz = -Math.cos(AZ);            /* downrange direction on the ground at the pad */
  const c = Math.cos(th), sn = Math.sin(th);
  const u = [sn * dx, c, sn * dz];                          /* local vertical under the vehicle */
  const f = [c * dx, -sn, c * dz];                          /* local horizontal, downrange */
  const r = RE + EYE_H + h;
  const p = [u[0] * r, u[1] * r - RE, u[2] * r];
  const b = interp(T, PITCH, tt) * Math.PI / 180, cb = Math.cos(b), sb = Math.sin(b);
  const nose = [cb * u[0] + sb * f[0], cb * u[1] + sb * f[1], cb * u[2] + sb * f[2]];
  /* heads down: the crew's heads point to the side the vehicle pitches towards, so at the end they face the
   * Earth's horizon above their heads. Before the roll program the heads point at the tower (north). */
  let hd = [-sb * u[0] + cb * f[0], -sb * u[1] + cb * f[1], -sb * u[2] + cb * f[2]];
  const roll = 1 - smooth(EVENTS.rollStart, EVENTS.rollEnd, t);
  if (roll > 0) hd = rotate(hd, nose, ROLL0 * roll);
  return Object.assign(out, { t, pos: p, up: u, fwd: f, nose, head: hd, alt: h, speed: t <= 0 ? 0 : interp(T, V, tt), downrange: s, pitch: b });
}
/* how far the roll program turns: from facing the tower (north) to facing downrange, about the vertical */
const ROLL0 = -(Math.atan2(Math.sin(AZ), -Math.cos(AZ)) - Math.atan2(0, -1));

function rotate(v, k, a) {      /* Rodrigues */
  const c = Math.cos(a), s = Math.sin(a), d = v[0] * k[0] + v[1] * k[1] + v[2] * k[2];
  const x = [k[1] * v[2] - k[2] * v[1], k[2] * v[0] - k[0] * v[2], k[0] * v[1] - k[1] * v[0]];
  return [v[0] * c + x[0] * s + k[0] * d * (1 - c), v[1] * c + x[1] * s + k[1] * d * (1 - c), v[2] * c + x[2] * s + k[2] * d * (1 - c)];
}

/* the Sun in the pad frame: an early December afternoon at the Cape (elevation 37 deg, azimuth 205 deg) */
export const SUN = (() => {
  const el = 37 * Math.PI / 180, az = 205 * Math.PI / 180;
  return [Math.sin(az) * Math.cos(el), Math.sin(el), -Math.cos(az) * Math.cos(el)];
})();

/* pad frame -> Earth-fixed (x to 0N 0E, z to the north pole): rows of the rotation, for the Earth's textures */
export const PAD_TO_ECEF = (() => {
  const la = PAD.lat * Math.PI / 180, lo = PAD.lon * Math.PI / 180;
  const E = [-Math.sin(lo), Math.cos(lo), 0];
  const N = [-Math.sin(la) * Math.cos(lo), -Math.sin(la) * Math.sin(lo), Math.cos(la)];
  const U = [Math.cos(la) * Math.cos(lo), Math.cos(la) * Math.sin(lo), Math.sin(la)];
  /* pad x = E, y = U, z = -N: ecef = E*x + U*y - N*z */
  return [E[0], U[0], -N[0], E[1], U[1], -N[1], E[2], U[2], -N[2]];   /* row-major 3x3 */
})();
