/* Atmosphere basics: pressure, mass, scale height, retention (Jeans), ozone, water's phase limits.
 * And a zero-dimensional climate estimate that stands in until the energy balance model (milestone M3). */
import { K_B, AMU, R_GAS, MOLAR, BAR, S_EARTH } from './const.js?v=11';

export const GASES = ['N2', 'O2', 'CO2', 'H2O', 'Ar', 'CH4'];

/* Partial pressures in bar (H2O 'auto' resolved by the caller). */
export function composition(atm, pH2O) {
  const p = { N2: atm.pN2bar, O2: atm.pO2bar, CO2: atm.pCO2bar, H2O: pH2O, Ar: atm.pArbar, CH4: atm.pCH4bar };
  let total = 0, mu = 0;
  for (const k of GASES) total += p[k] || 0;
  for (const k of GASES) mu += (p[k] || 0) * MOLAR[k];
  mu = total > 0 ? mu / total : MOLAR.N2;
  return { p, totalBar: total, totalPa: total * BAR, molar: mu };
}

/* Atmosphere mass, kg: M = P 4 pi R^2 / g. */
export function atmosphereMass(totalPa, R, g) { return totalPa * 4 * Math.PI * R * R / g; }

/* Scale height H = R T / (mu g), m. */
export function scaleHeight(T, molar, g) { return R_GAS * T / (molar * g); }

/* Jeans escape parameter lambda = v_esc^2 / v0^2, v0 = sqrt(2 k T / m). */
export function jeansLambda(vesc, molarKgPerMol, Texo) {
  const m = molarKgPerMol / 6.02214076e23;
  return vesc * vesc / (2 * K_B * Texo / m);
}
export const T_EXO_EARTH = 1000;   // K, Earth's exosphere at moderate solar activity

/* Saturation vapour pressure of water, Pa (Magnus-Tetens over water, Alduchov & Eskridge 1996), T in K. */
export function satVapourPa(T) {
  const c = T - 273.15;
  return 610.94 * Math.exp(17.625 * c / (c + 243.04));
}
/* Boiling point of water at pressure P (Pa), K: Clausius-Clapeyron with L = 40.65 kJ/mol from 1 atm, 373.15 K. */
export function boilingPointK(Pa) {
  if (Pa <= 611.657) return 273.16;
  return 1 / (1 / 373.15 - R_GAS * Math.log(Pa / 101325) / 40650);
}
export const TRIPLE_POINT_PA = 611.657;
export const ARMSTRONG_PA = 6.3e3;

/* Ozone forms when O2 is above roughly 1 % of today's level (Kasting & Catling 2003, a rough threshold). */
export function hasOzone(pO2bar) { return pO2bar >= 0.01 * 0.21; }

/* --- Zero-dimensional climate estimate (placeholder for the EBM of milestone M3) ---
 * Absorbed flux  F = S (1 - A) / 4,  S = flux x 1361 W/m^2.
 * Outgoing       OLR = A0 + B T[deg C] - 5.35 ln(pCO2 / 330 ppm) - CH4 term
 * with A0 = 203.3 W/m^2, B = 2.09 W/m^2/K (Budyko-type fit for Earth, North et al. 1981) and the CO2 forcing of
 * Myhre et al. 1998, referred to 330 ppm, the CO2 level of the years the fit was made for. Valid near Earth's state only: about 0.5-2 bar, 100-10 000 ppm CO2, no ice feedback.
 * Runaway check first: absorbed > 282 W/m^2 means a runaway greenhouse (Goldblatt et al. 2013). */
export const RUNAWAY_FLUX = 282;
export function climate0D({ S, albedo, pCO2bar, pCH4bar, totalBar }) {
  const absorbed = S * S_EARTH * (1 - albedo) / 4;
  const Teq = (absorbed / 5.670374419e-8) ** 0.25;
  const runaway = absorbed > RUNAWAY_FLUX;
  const totalSafe = Math.max(totalBar, 1e-6);
  const ppm = Math.max(1, (pCO2bar / totalSafe) * 1e6 * Math.min(1, totalSafe));   // column-weighted mixing ratio
  const fCO2 = 5.35 * Math.log(ppm / 330);
  const ch4ppb = (pCH4bar / totalSafe) * 1e9 * Math.min(1, totalSafe);
  const fCH4 = 0.036 * (Math.sqrt(Math.max(ch4ppb, 0)) - Math.sqrt(1800));
  /* A thin atmosphere holds little water vapour and little greenhouse effect: fade the Earth-calibrated greenhouse
   * (33 K at 1 bar) with pressure, roughly as the square root (pressure broadening), and none below the triple point. */
  const pFade = totalBar <= 0.006 ? 0 : Math.min(1.5, Math.sqrt(totalBar));
  const Tbudyko = (absorbed - 203.3 + fCO2 + fCH4) / 2.09 + 273.15;
  const Ts = Teq + (Tbudyko - Teq) * pFade;
  const valid = totalBar >= 0.5 && totalBar <= 2 && ppm >= 100 && ppm <= 10000 && !runaway;
  return { absorbed, Teq, Ts: runaway ? Math.max(Ts, 600) : Ts, runaway, valid, forcingCO2: fCO2, forcingCH4: fCH4 };
}

/* Default albedo from the surface: ocean 0.06, land as set, clouds raise the planet's albedo; tuned so Earth with
 * 29 % land comes out at 0.30. An estimate for the placeholder climate only. */
export function bondAlbedo(landFrac, landAlbedo, iceFrac = 0) {
  const surf = (1 - landFrac) * 0.06 + landFrac * landAlbedo;
  const clouds = 0.5;                     // cloud cover fraction
  const cloudAlb = 0.45;
  return Math.min(0.9, (1 - clouds) * surf + clouds * cloudAlb + iceFrac * 0.3) + 0.017;
}

/* Land fraction from the ocean inventory for Earth-like terrain (heuristic, spec §4.7): 1 ocean gives Earth's 29 %;
 * land is nearly gone by 2-3 oceans. Piecewise linear in this table of [oceans, land fraction]. */
const LAND = [[0, 1], [0.05, 0.9], [0.2, 0.72], [0.5, 0.48], [1, 0.29], [1.5, 0.13], [2, 0.05], [3, 0.0]];
export function landFraction(oceans) {
  if (oceans >= 3) return 0;
  for (let i = 0; i < LAND.length - 1; i++) {
    const [a, la] = LAND[i], [b, lb] = LAND[i + 1];
    if (oceans <= b) return la + (lb - la) * (oceans - a) / (b - a);
  }
  return 0;
}
