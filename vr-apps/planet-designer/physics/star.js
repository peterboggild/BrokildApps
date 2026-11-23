/* Main-sequence stars: properties from mass, brightening with age, colour, photosynthetic light.
 * Pure functions, SI or solar units as named. No three.js. */
import { STAR_TABLE } from '../data/stars.js?v=1';
import { WIEN_B, H_PLANCK, C_LIGHT, K_B, AGE_SUN } from './const.js?v=1';

export const STAR_MASS_MIN = 0.08;
export const STAR_MASS_MAX = 2.0;
export const STAR_MASS_HZ_VALID = 1.4;   // above this the HZ model is used outside its tested range

/* The table's G2V row is a mean field star (logL 0.010, R 1.012), not the Sun itself. We scale every row by the same
 * small factors so that 1 Msun at 4.57 Gyr is exactly 1 Lsun and 1 Rsun, well inside the table's own scatter. */
const SUN_NORM_LOGL = 0.010;
const SUN_NORM_R = 1.012;

/* Interpolate the Mamajek mean dwarf table in log mass. The table describes typical field dwarfs, i.e. stars of
 * middle main-sequence age; we treat its values as the star at the reference age of refAgeGyr(). */
export function mainSequence(massSun) {
  const lm = Math.log10(massSun);
  const T = STAR_TABLE;                       // sorted by decreasing mass
  let i = 0;
  while (i < T.length - 2 && Math.log10(T[i + 1][4]) > lm) i++;
  const a = T[i], b = T[i + 1];
  const la = Math.log10(a[4]), lb = Math.log10(b[4]);
  let f = (lm - la) / (lb - la);
  const outside = f < 0 || f > 1;
  f = Math.max(0, Math.min(1, f));
  const lerp = (x, y) => x + (y - x) * f;
  const teff = 10 ** lerp(Math.log10(a[1]), Math.log10(b[1]));
  const logL = lerp(a[2], b[2]) - SUN_NORM_LOGL;
  const radius = 10 ** lerp(Math.log10(a[3]), Math.log10(b[3])) / SUN_NORM_R;
  const spt = f < 0.5 ? a[0] : b[0];
  return { teff, lum: 10 ** logL, radius, spt, outsideTable: outside };
}

/* Main-sequence lifetime, Gyr: 10 (M/Msun)^-2.5, a first estimate (crude above 1.5 Msun). */
export function lifetimeGyr(massSun) { return 10 * massSun ** -2.5; }

/* The age at which the table values apply: the Sun's fractional age (4.57/10 of its lifetime) for stars that live
 * shorter than about 10 Gyr; otherwise the Sun's age (field red dwarfs are a few Gyr old). */
export function refAgeGyr(massSun) { return Math.min(AGE_SUN, (AGE_SUN / 10) * lifetimeGyr(massSun)); }

/* Brightening with age after Gough 1981, Solar Physics 74, 21: L(t) = L_now / [1 + 0.4 (1 - t/t_now)].
 * Generalised to other masses by measuring t in units of the star's own lifetime, so the Sun's t_now is 0.457 of it.
 * Normalised so that L(refAge) = the table value. Valid on the main sequence, not before it (red dwarfs' bright
 * pre-main-sequence phase comes with the time machine). */
function goughFactor(x) { return 1 / (1 + 0.4 * (1 - x / 0.457)); }
export function luminosityAt(massSun, ageGyr) {
  const ms = mainSequence(massSun);
  const life = lifetimeGyr(massSun);
  const x = Math.min(ageGyr, life) / life;
  const xr = refAgeGyr(massSun) / life;
  return ms.lum * goughFactor(x) / goughFactor(xr);
}

/* Temperature at a given age: luminosity changes at nearly constant radius over most of the main sequence for
 * Sun-like stars; we keep the table radius and let T follow L^(1/4). An approximation, stated as such. */
export function starAt(massSun, ageGyr) {
  const ms = mainSequence(massSun);
  const lum = luminosityAt(massSun, ageGyr);
  const teff = ms.teff * (lum / ms.lum) ** 0.25;
  return { ...ms, lum, teff, lifetime: lifetimeGyr(massSun), ageGyr,
    outsideMS: ageGyr > lifetimeGyr(massSun) };
}

/* Spectral class letter for colour coding and teaching text. */
export function spectralLetter(spt) { return spt ? spt[0] : '?'; }

/* Wien peak wavelength, m. */
export function peakWavelength(teff) { return WIEN_B / teff; }

/* Planck photon spectral radiance (per wavelength), arbitrary units. */
function photonPlanck(lambda, T) {
  const x = H_PLANCK * C_LIGHT / (lambda * K_B * T);
  if (x > 700) return 0;
  return 1 / (lambda ** 4 * (Math.exp(x) - 1));
}
function energyPlanck(lambda, T) {
  const x = H_PLANCK * C_LIGHT / (lambda * K_B * T);
  if (x > 700) return 0;
  return 1 / (lambda ** 5 * (Math.exp(x) - 1));
}

/* Photosynthetically active radiation (400-700 nm), from a black body at teff:
 *   energyFrac  fraction of the bolometric energy in 400-700 nm
 *   photonsPerW PAR photons per joule of total (bolometric) light, relative to the Sun (Teff 5772 K).
 * A black body is an approximation: real red dwarfs have strong absorption bands. */
export function parFraction(teff) {
  const n = 4000, lo = 100e-9, hi = 100e-6;
  let eTot = 0, ePar = 0, nPar = 0;
  const llo = Math.log(lo), lhi = Math.log(hi);
  for (let i = 0; i < n; i++) {
    const l = Math.exp(llo + (lhi - llo) * (i + 0.5) / n);
    const dl = l * (lhi - llo) / n;
    const e = energyPlanck(l, teff) * dl;
    eTot += e;
    if (l >= 400e-9 && l <= 700e-9) { ePar += e; nPar += photonPlanck(l, teff) * dl; }
  }
  return { energyFrac: ePar / eTot, photonsPerJ: nPar / eTot };
}
let sunPar = null;
export function parRelativeToSun(teff) {
  if (!sunPar) sunPar = parFraction(5772);
  const p = parFraction(teff);
  return { energyFrac: p.energyFrac, photonsPerW: p.photonsPerJ / sunPar.photonsPerJ };
}

/* Snow line, AU: 2.7 sqrt(L/Lsun), a common heuristic (Hayashi 1981 scaled). */
export function snowLineAU(lum) { return 2.7 * Math.sqrt(lum); }

/* Black-body colour for display, linear sRGB normalised to max 1. Uses the analytic fit of the CIE 1931 colour
 * matching functions by Wyman, Sloan & Shirley 2013 (JCGT 2, 2), integrated over 380-780 nm. */
function cie(l) {
  const g = (x, m, s1, s2) => Math.exp(-0.5 * ((x - m) / (x < m ? s1 : s2)) ** 2);
  const x = 1.056 * g(l, 599.8, 37.9, 31.0) + 0.362 * g(l, 442.0, 16.0, 26.7) - 0.065 * g(l, 501.1, 20.4, 26.2);
  const y = 0.821 * g(l, 568.8, 46.9, 40.5) + 0.286 * g(l, 530.9, 16.3, 31.1);
  const z = 1.217 * g(l, 437.0, 11.8, 36.0) + 0.681 * g(l, 459.0, 26.0, 13.8);
  return [x, y, z];
}
export function blackbodyRGB(teff) {
  let X = 0, Y = 0, Z = 0;
  for (let l = 380; l <= 780; l += 5) {
    const e = energyPlanck(l * 1e-9, teff);
    const [x, y, z] = cie(l);
    X += e * x; Y += e * y; Z += e * z;
  }
  let r = 3.2406 * X - 1.5372 * Y - 0.4986 * Z;
  let g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
  let b = 0.0557 * X - 0.2040 * Y + 1.0570 * Z;
  r = Math.max(0, r); g = Math.max(0, g); b = Math.max(0, b);
  const m = Math.max(r, g, b) || 1;
  return [r / m, g / m, b / m];
}
export function rgbToCss([r, g, b]) {
  const enc = (c) => Math.round(255 * (c <= 0.0031308 ? 12.92 * c : 1.055 * c ** (1 / 2.4) - 0.055));
  return `rgb(${enc(r)}, ${enc(g)}, ${enc(b)})`;
}

/* Activity class from mass and age: red dwarfs stay active for Gyr (Jackson et al. 2012 show saturation lasting
 * longer for lower masses); a qualitative label only, used by the ledger until the XUV model arrives (M3). */
export function activityAuto(massSun, ageGyr) {
  const activeUntil = massSun < 0.35 ? 8 : massSun < 0.6 ? 3 : massSun < 0.9 ? 1 : 0.6;
  if (ageGyr < activeUntil * 0.5) return 'high';
  if (ageGyr < activeUntil) return 'normal';
  return 'low';
}
