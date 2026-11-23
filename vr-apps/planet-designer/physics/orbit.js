/* Orbits, fluxes, stability, tidal locking and moons. SI inside, named units at the edges. */
import { G, AU, M_SUN, M_EARTH, R_EARTH, YEAR, GYR, M_MOON, A_MOON, R_SUN } from './const.js?v=14';

export const ECC_MAX = 0.4;   // owner's hard cap: "orbits must not be excessive"

/* Kepler's third law: P [yr] = sqrt(a^3 / M), a in AU, M in Msun (planet mass neglected). */
export function periodYears(aAU, massSun) { return Math.sqrt(aAU ** 3 / massSun); }

/* Mean flux relative to Earth's, S = L / a^2; the annual mean over an eccentric orbit is S / sqrt(1 - e^2). */
export function fluxEarth(lum, aAU) { return lum / (aAU * aAU); }
export function eccMeanFactor(e) { return 1 / Math.sqrt(1 - e * e); }
export function periApoFlux(lum, aAU, e) {
  return { peri: lum / (aAU * (1 - e)) ** 2, apo: lum / (aAU * (1 + e)) ** 2 };
}

/* Hill radius at periastron, AU: r_H = a(1 - e)(m / 3M)^(1/3). */
export function hillRadiusAU(aAU, e, massEarth, massSun) {
  return aAU * (1 - e) * Math.cbrt((massEarth * M_EARTH) / (3 * massSun * M_SUN));
}

/* Separation of two neighbouring planets in mutual Hill radii (Gladman 1993; Chambers et al. 1996):
 * R_H,m = [(m1+m2)/(3M)]^(1/3) (a1+a2)/2. Eccentricities shrink the gap between periastron and apoastron, so we
 * use the closest approach a2(1-e2) - a1(1+e1); crossing orbits give a negative number. */
export function mutualHillSpacing(p1, p2, massSun) {
  const [i, o] = p1.aAU < p2.aAU ? [p1, p2] : [p2, p1];
  const rh = Math.cbrt(((i.massEarth + o.massEarth) * M_EARTH) / (3 * massSun * M_SUN)) * (i.aAU + o.aAU) / 2;
  const gap = o.aAU * (1 - (o.e || 0)) - i.aAU * (1 + (i.e || 0));
  return gap / rh;
}
export const HILL_TWO_PLANET = 2 * Math.sqrt(3);   // 3.46, Gladman 1993
export const HILL_MULTI = 10;                       // ~8-10 for Gyr stability with 3+ planets, Chambers et al. 1996

/* Tidal despinning time (Gladman et al. 1996): t = w a^6 I Q / (3 G M*^2 k2 R^5), I = 0.33 m R^2.
 * Initial spin period 12 h by default. Q is uncertain by orders of magnitude, so the result is a band
 * for Q = 10 ... 1000 around the nominal 100. Returns Gyr. */
export function tidalLockGyr(aAU, massSun, massEarth, radiusEarth, { Q = 100, k2 = 0.3, spinHours = 12 } = {}) {
  const w = 2 * Math.PI / (spinHours * 3600);
  const a = aAU * AU, M = massSun * M_SUN, m = massEarth * M_EARTH, R = radiusEarth * R_EARTH;
  const I = 0.33 * m * R * R;
  return (w * a ** 6 * I * Q) / (3 * G * M * M * k2 * R ** 5) / GYR;
}
export function tidalLockBand(aAU, massSun, massEarth, radiusEarth, ageGyr) {
  const lo = tidalLockGyr(aAU, massSun, massEarth, radiusEarth, { Q: 10 });
  const mid = tidalLockGyr(aAU, massSun, massEarth, radiusEarth, { Q: 100 });
  const hi = tidalLockGyr(aAU, massSun, massEarth, radiusEarth, { Q: 1000 });
  const state = hi < ageGyr ? 'locked' : mid < ageGyr ? 'likely' : lo < ageGyr ? 'possible' : 'free';
  return { lo, mid, hi, state };
}

/* Moons. Fluid Roche limit d = 2.44 R_p (rho_p / rho_m)^(1/3); prograde moons stay bound out to about
 * 0.4 r_H (spec; Domingos et al. 2006 give ~0.49 r_H for circular orbits, so 0.4 leaves a margin).
 * Tidal range relative to the Moon's on Earth: proportional to (m_moon / M_p) (R_p / d)^3 R_p. */
export function moonInfo(moon, planet, star, aAU, e) {
  const mm = moon.massMoon * M_MOON;
  const d = moon.aEarthRadii * R_EARTH;
  const Rp = planet.radiusEarth * R_EARTH, Mp = planet.massEarth * M_EARTH;
  const rhoP = Mp / (4 / 3 * Math.PI * Rp ** 3);
  const rhoM = 3340;                                   // the Moon's bulk density, kg/m^3
  const roche = 2.44 * Rp * Math.cbrt(rhoP / rhoM);
  const rH = hillRadiusAU(aAU, e, planet.massEarth, star.massSun) * AU;
  const tideRef = (M_MOON / M_EARTH) * (R_EARTH / A_MOON) ** 3 * R_EARTH;
  const tide = (mm / Mp) * (Rp / d) ** 3 * Rp / tideRef;
  const angMoon = 2 * Math.atan(Math.cbrt(moon.massMoon) * 1737.4e3 / d);    // radius scaled by mass^(1/3), Moon density
  const angStar = 2 * Math.atan(star.radius * R_SUN / (aAU * AU));
  const massRatio = mm / Mp;
  return {
    rocheEarthRadii: roche / R_EARTH, maxStableEarthRadii: 0.4 * rH / R_EARTH,
    insideRoche: d < roche, unbound: d > 0.4 * rH,
    tideRel: tide, angMoonDeg: angMoon * 180 / Math.PI, angStarDeg: angStar * 180 / Math.PI,
    /* Compared at the moon's closest approach, with the Moon's own orbital eccentricity (0.055) as the default. */
    totalEclipse: angMoon / (1 - 0.055) >= angStar, massRatio,
    stabilises: massRatio > 0.01 && d < 100 * Rp,       // qualitative rule, spec §4.5
  };
}
export const MOON_MASS_MIN = 0.001 / 0.0123;   // in Moon masses (0.001 Earth masses)
export const MOON_MASS_MAX = 0.1 / 0.0123;

/* Kepler's equation for positions on the orrery (Newton iteration). Mean anomaly M in radians. */
export function eccentricAnomaly(M, e) {
  let E = e < 0.8 ? M : Math.PI;
  for (let i = 0; i < 12; i++) E -= (E - e * Math.sin(E) - M) / (1 - e * Math.cos(E));
  return E;
}
export function orbitPosition(aAU, e, M) {
  const E = eccentricAnomaly(M, e);
  return { x: aAU * (Math.cos(E) - e), y: aAU * Math.sqrt(1 - e * e) * Math.sin(E) };
}
export { YEAR };
