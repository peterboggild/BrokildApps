/* Physical constants and reference bodies, SI units (CODATA 2018 / IAU 2015 nominal values). */
export const G = 6.67430e-11;          // m^3 kg^-1 s^-2
export const K_B = 1.380649e-23;       // J/K
export const R_GAS = 8.314462618;      // J mol^-1 K^-1
export const AMU = 1.66053906660e-27;  // kg
export const SIGMA = 5.670374419e-8;   // W m^-2 K^-4
export const H_PLANCK = 6.62607015e-34;
export const C_LIGHT = 2.99792458e8;
export const WIEN_B = 2.897771955e-3;  // m K

export const AU = 1.495978707e11;      // m
export const M_SUN = 1.98847e30;       // kg (from IAU GM_sun nominal / G)
export const R_SUN = 6.957e8;          // m (IAU nominal)
export const L_SUN = 3.828e26;         // W (IAU nominal)
export const TEFF_SUN = 5772;          // K (IAU nominal)
export const AGE_SUN = 4.57;           // Gyr
export const S_EARTH = 1361;           // W/m^2, solar constant (Kopp & Lean 2011)

export const M_EARTH = 5.9722e24;      // kg
export const R_EARTH = 6.371e6;        // m, mean radius
export const M_MOON = 7.342e22;        // kg
export const R_MOON = 1.7374e6;        // m
export const A_MOON = 3.844e8;         // m, mean Earth-Moon distance
export const OCEAN_MASS = 1.4e21;      // kg, one Earth ocean
export const RHO_WATER = 1025;         // kg/m^3, sea water
export const YEAR = 3.15576e7;         // s, Julian year
export const GYR = 1e9 * YEAR;
export const BAR = 1e5;                // Pa

/* Molar masses, kg/mol, for the atmosphere gases. */
export const MOLAR = { N2: 0.0280134, O2: 0.0319988, CO2: 0.0440095, H2O: 0.0180153, Ar: 0.039948, CH4: 0.0160425, H: 0.00100794 };

/* Reference bodies for comparisons and tests (NASA planetary fact sheets). */
export const BODIES = {
  earth: { massEarth: 1, radiusEarth: 1, aAU: 1, g: 9.81, vesc: 11.19 },
  mars: { massEarth: 0.107, radiusEarth: 0.532, aAU: 1.524, g: 3.71, vesc: 5.03 },
  venus: { massEarth: 0.815, radiusEarth: 0.949, aAU: 0.723, g: 8.87, vesc: 10.36 },
};
