/* Climate: a one-dimensional, seasonal energy balance model (spec §5.9).
 *
 *   C dT/dt = Q(x, t) (1 - a(T)) - OLR(T) + d/dx [ D (1 - x^2) dT/dx ],   x = sin(latitude)
 *
 * after North 1975, North, Cahalan & Coakley 1981 (Rev. Geophys. 19, 91) and Williams & Kasting 1997 (Icarus
 * 129, 254). Daily-mean insolation by latitude and season from the orbit and the obliquity (Berger 1978).
 * Outgoing longwave: the Budyko-type linear fit A + B T for Earth-like air (A = 203.3, B = 2.09 W/m^2, T in deg C),
 * shifted by the CO2 (Myhre et al. 1998) and CH4 forcings, and blended towards a bare black body for thin air.
 * Ice-albedo feedback with a smooth step at -10 deg C, so snowball states and their hysteresis exist.
 * Heat transport D scales with pressure and rotation as in Williams & Kasting 1997. Tidally locked planets use the
 * same equation with x = cosine of the angle from the substellar point and no seasons (qualitative).
 * Implicit in time (backward Euler, tridiagonal), so it is stable for any D. Pure function; runs in a worker. */
import { S_EARTH } from './const.js?v=6';

const SIGMA = 5.670374419e-8;
export const EBM_N = 36;                 /* latitude bands, equal area (uniform in x) */
const C_OCEAN = 1025 * 3990 * 50;        /* 50 m mixed layer, J m^-2 K^-1 */
const C_LAND = C_OCEAN / 40;
const D0 = 0.50;                         /* W m^-2 K^-1, Earth; with A2 tuned to the §5.12 benchmarks (15 C, ice near 70 deg) */
const A2 = 0.04;                         /* latitude dependence of the ice-free albedo, times P2(x): more at low sun */
const A0 = 203.3, B0 = 2.09;

/* Daily-mean insolation (W/m^2) at latitude phi for solar declination dec and distance factor 1/rho^2. */
function dailyQ(S, phi, dec, invRho2) {
  const t = -Math.tan(phi) * Math.tan(dec);
  const h0 = t >= 1 ? 0 : t <= -1 ? Math.PI : Math.acos(t);
  return (S / Math.PI) * invRho2 * (h0 * Math.sin(phi) * Math.sin(dec) + Math.cos(phi) * Math.cos(dec) * Math.sin(h0));
}

/* Kepler: true anomaly at mean anomaly M. */
function trueAnomaly(M, e) {
  let E = M;
  for (let i = 0; i < 10; i++) E -= (E - e * Math.sin(E) - M) / (1 - e * Math.cos(E));
  return { nu: 2 * Math.atan2(Math.sqrt(1 + e) * Math.sin(E / 2), Math.sqrt(1 - e) * Math.cos(E / 2)), rho: 1 - e * Math.cos(E) };
}

/* Stull 2011 (J. Appl. Meteor. Climatol. 50, 2267): wet-bulb temperature (deg C) from T (deg C) and RH (%). */
export function wetBulb(T, RH) {
  return T * Math.atan(0.151977 * Math.sqrt(RH + 8.313659)) + Math.atan(T + RH) - Math.atan(RH - 1.676331)
    + 0.00391838 * RH ** 1.5 * Math.atan(0.023101 * RH) - 4.686035;
}

/* Solve a tridiagonal system in place (Thomas algorithm). a: sub, b: diag, c: super, d: rhs -> d holds x. */
function thomas(a, b, c, d, n) {
  for (let i = 1; i < n; i++) { const w = a[i] / b[i - 1]; b[i] -= w * c[i - 1]; d[i] -= w * d[i - 1]; }
  d[n - 1] /= b[n - 1];
  for (let i = n - 2; i >= 0; i--) d[i] = (d[i] - c[i] * d[i + 1]) / b[i];
}

/* Parameters (all plain numbers, so the object can be posted to a worker):
 *   S (flux relative to Earth, annual mean at a), e, obliquityDeg, argPeriDeg, dayHours, locked,
 *   pBar, molar (kg/mol), forcing (W/m^2: CO2 + CH4, positive warms), greenhouse (0..1, how Earth-like the
 *   longwave is), albedoFree, albedoIce, landFrac, hasWater, init ('warm' | 'cold' | array of T), years. */
export function runEBM(p) {
  const N = EBM_N;
  const x = new Float64Array(N), xe = new Float64Array(N + 1);
  for (let j = 0; j <= N; j++) xe[j] = -1 + 2 * j / N;
  for (let j = 0; j < N; j++) x[j] = (xe[j] + xe[j + 1]) / 2;
  const dx = 2 / N;
  const S0 = p.S * S_EARTH * Math.sqrt(1 - p.e * p.e);      /* flux at the semi-major axis; the 1/rho^2 cycle adds the eccentric mean */
  const g = Math.max(0, Math.min(1, p.greenhouse));
  /* heat transport: Williams & Kasting 1997, D = D0 (p/p0)(cp/cp0)(m0/m)^2 (Omega0/Omega)^2, capped */
  const rot = p.locked ? 1 : Math.min(25, (Math.max(p.dayHours, 4) / 23.93) ** 2);
  let D = D0 * Math.max(p.pBar, 0.003) * (0.0289 / p.molar) ** 2 * rot * (p.locked ? 3 : 1);
  D = Math.min(D, 60);
  const Cbar = p.hasWater ? (1 - p.landFrac) * C_OCEAN + p.landFrac * C_LAND : C_LAND;
  const C = Math.max(Cbar, C_LAND);
  const aFree = p.albedoFree, aIce = p.hasWater ? p.albedoIce : p.albedoFree;
  const A = A0 - p.forcing;
  /* OLR(T) and its slope, blended: g = 1 Earth-like linear fit, g = 0 bare black body */
  const olr = (Tc) => g * (A + B0 * Tc) + (1 - g) * SIGMA * (Tc + 273.15) ** 4;
  const dolr = (Tc) => g * B0 + (1 - g) * 4 * SIGMA * (Tc + 273.15) ** 3;
  const a2 = p.locked ? 0 : A2;
  const albedo = (Tc, xx) => aIce + (aFree + a2 * (1.5 * xx * xx - 0.5) - aIce) * 0.5 * (1 + Math.tanh((Tc + 10) / 3));

  const T = new Float64Array(N);
  if (Array.isArray(p.init) && p.init.length === N) for (let j = 0; j < N; j++) T[j] = p.init[j];
  else for (let j = 0; j < N; j++) T[j] = p.init === 'cold' ? -45 : 28 - 40 * x[j] * x[j];

  const stepsPerYear = 72, years = p.years || 30;
  const dt = (p.periodYears || 1) * 3.15576e7 / stepsPerYear;
  const obl = p.obliquityDeg * Math.PI / 180, wp = (p.argPeriDeg || 0) * Math.PI / 180;
  const a = new Float64Array(N), b = new Float64Array(N), c = new Float64Array(N), d = new Float64Array(N), Q = new Float64Array(N);
  const sumT = new Float64Array(N), maxT = new Float64Array(N).fill(-1e9), minT = new Float64Array(N).fill(1e9);
  const seasonal = [];                                     /* last year: T at 12 points of the year, for the chart */
  let absorbed = 0;
  for (let step = 0; step < years * stepsPerYear; step++) {
    const M = 2 * Math.PI * ((step % stepsPerYear) + 0.5) / stepsPerYear;
    const { nu, rho } = trueAnomaly(M, p.e);
    const invRho2 = 1 / (rho * rho);
    const dec = Math.asin(Math.sin(obl) * Math.sin(nu + wp));
    for (let j = 0; j < N; j++) {
      Q[j] = p.locked ? S0 * invRho2 * Math.max(x[j], 0) : dailyQ(S0, Math.asin(x[j]), dec, invRho2);
    }
    /* implicit: (C/dt + OLR'(T)) T' - diffusion(T') = C/dt T + Q(1-a) - OLR(T) + OLR'(T) T */
    for (let j = 0; j < N; j++) {
      const kl = j > 0 ? D * (1 - xe[j] * xe[j]) / (dx * dx) : 0;
      const kr = j < N - 1 ? D * (1 - xe[j + 1] * xe[j + 1]) / (dx * dx) : 0;
      const s = dolr(T[j]);
      a[j] = -kl; c[j] = -kr; b[j] = C / dt + s + kl + kr;
      d[j] = C / dt * T[j] + Q[j] * (1 - albedo(T[j], x[j])) - olr(T[j]) + s * T[j];
    }
    thomas(a, b, c, d, N);
    for (let j = 0; j < N; j++) T[j] = d[j];
    if (step >= (years - 1) * stepsPerYear) {
      let ab = 0;
      for (let j = 0; j < N; j++) {
        sumT[j] += T[j]; maxT[j] = Math.max(maxT[j], T[j]); minT[j] = Math.min(minT[j], T[j]);
        ab += Q[j] * (1 - albedo(T[j], x[j]));
      }
      absorbed += ab / N;
      if ((step % stepsPerYear) % 6 === 0) seasonal.push(Array.from(T));
    }
  }
  const mean = Array.from(sumT, (s) => s / stepsPerYear);
  const gmean = mean.reduce((s, v) => s + v, 0) / N;
  /* ice line: equator-most latitude (per hemisphere) where the annual mean falls below -10 deg C */
  let iceN = 90, iceS = -90;
  for (let j = N - 1; j >= 0; j--) if (mean[j] < -10 && x[j] > 0) iceN = Math.asin(x[j]) * 180 / Math.PI;
  for (let j = 0; j < N; j++) if (mean[j] < -10 && x[j] < 0) iceS = Math.asin(x[j]) * 180 / Math.PI;
  const iceFrac = mean.filter((t) => t < -10).length / N;
  return {
    x: Array.from(x), lat: Array.from(x, (v) => Math.asin(v) * 180 / Math.PI), mean, max: Array.from(maxT), min: Array.from(minT),
    seasonal, gmean, iceLatN: iceN, iceLatS: iceS, iceFrac, snowball: iceFrac > 0.95 && p.hasWater,
    absorbed: absorbed / stepsPerYear, D, final: Array.from(T),
  };
}
