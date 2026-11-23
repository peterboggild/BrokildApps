/* The space environment: the star's X-ray and extreme-UV history, the exosphere it heats, the cosmic shoreline,
 * and the radiation dose at the surface. Estimates, labelled as such in the ledger. */
import { luminosityAt } from './star.js?v=16';

/* XUV fraction of the bolometric light: saturated at 10^-3.2 while young, then falling as (t / t_sat)^-1.5.
 * Saturation lasts ~0.1 Gyr for a Sun-like star and much longer for red dwarfs (Jackson, Davis & Wheatley 2012
 * find longer saturation at lower mass; the scaling with mass here is a fit to that trend, not their table). */
export function satTimeGyr(massSun) { return Math.min(8, 0.1 * massSun ** -2.5); }
export function xuvFraction(massSun, ageGyr) {
  const ts = satTimeGyr(massSun);
  return 10 ** -3.2 * (ageGyr <= ts ? 1 : (ageGyr / ts) ** -1.5);
}
/* XUV flux at a planet (Earth's today = 1) and the fluence over the star's life so far (Earth's = 1). */
let earthRef = null;
function fluence(massSun, ageGyr, aAU) {
  const n = 120, t0 = 0.01;
  let sum = 0;
  for (let i = 0; i < n; i++) {
    const t1 = t0 * (ageGyr / t0) ** (i / n), t2 = t0 * (ageGyr / t0) ** ((i + 1) / n);
    const t = Math.sqrt(t1 * t2);
    sum += luminosityAt(massSun, t) * xuvFraction(massSun, t) * (t2 - t1);
  }
  return sum / (aAU * aAU);
}
export function xuvEnvironment(massSun, ageGyr, aAU, lumNow) {
  if (!earthRef) earthRef = { flux: 1 * xuvFraction(1, 4.57), fluence: fluence(1, 4.57, 1) };
  const flux = lumNow * xuvFraction(massSun, ageGyr) / (aAU * aAU) / earthRef.flux;
  const fl = fluence(massSun, ageGyr, aAU) / earthRef.fluence;
  /* Exosphere temperature: Earth's ~1000 K, rising with the XUV flux roughly as its square root, capped. */
  const Texo = Math.min(20000, Math.max(300, 1000 * Math.sqrt(flux)));
  return { flux, fluence: fl, Texo };
}

/* The cosmic shoreline (Zahnle & Catling 2017, ApJ 843, 122): bodies above the line I = I0 (v_esc / v0)^4 have
 * no atmosphere. We draw the line through Mars (v_esc 5.03 km/s, XUV fluence 0.43 of Earth's), which sits on it;
 * that one calibration point is ours, the slope of 4 is theirs. */
export const SHORE_V0 = 5.03, SHORE_I0 = 0.43;
export function shorelineI(vKms) { return SHORE_I0 * (vKms / SHORE_V0) ** 4; }
export const SOLAR_BODIES = [
  /* name, escape velocity (km/s), insolation relative to Earth (= XUV fluence ratio within the Sun's system), atmosphere? */
  ['Merkur', 4.25, 6.67, false], ['Venus', 10.36, 1.91, true], ['Jorden', 11.19, 1, true], ['Månen', 2.38, 1, false],
  ['Mars', 5.03, 0.43, true], ['Io', 2.56, 0.037, false], ['Europa', 2.03, 0.037, false], ['Ganymedes', 2.74, 0.037, false],
  ['Callisto', 2.44, 0.037, false], ['Titan', 2.64, 0.011, true], ['Triton', 1.46, 0.0011, true], ['Pluto', 1.21, 0.00064, true],
];

/* Surface dose from cosmic rays (mSv per year) against the atmosphere's column mass (g/cm^2), interpolated in log
 * through: free space ~660 (1.8 mSv/day), Mars' surface (16 g/cm^2) ~230 (0.64 mSv/day, Hassler et al. 2014),
 * airliner altitude (~230 g/cm^2) ~30, ~500 g/cm^2 ~3, Earth's sea level (1033 g/cm^2) ~0.35. Beyond that it falls
 * by e every 150 g/cm^2. Active stars add particles (x1, x2, x4). No magnetic-field term yet: the dynamo comes
 * with the interior model (M6). Cosmic radiation only (no radon or rocks). */
const DOSE = [[0, 660], [16, 230], [230, 30], [500, 3], [1033, 0.35]];
export function cosmicDose(columnGcm2, activity) {
  const x = Math.max(0, columnGcm2);
  let d;
  if (x >= 1033) d = 0.35 * Math.exp(-(x - 1033) / 150);
  else for (let i = 0; i < DOSE.length - 1; i++) {
    const [x0, d0] = DOSE[i], [x1, d1] = DOSE[i + 1];
    if (x <= x1) { d = Math.exp(Math.log(d0) + (Math.log(d1) - Math.log(d0)) * (x - x0) / (x1 - x0)); break; }
  }
  return d * ({ low: 1, normal: 2, high: 4 }[activity] || 1);
}
