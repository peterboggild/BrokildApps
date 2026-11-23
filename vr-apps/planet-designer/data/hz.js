/* Habitable-zone limits: S_eff = S0 + a T + b T^2 + c T^3 + d T^4, T = Teff - 5780 K, valid 2600-7200 K.
 * Distance d = sqrt(L / S_eff) AU (L in Lsun).
 *
 * Sources:
 *   Kopparapu et al. 2014, ApJL 787, L29, table 1: recent Venus, runaway greenhouse (0.1, 1, 5 Earth masses),
 *     maximum greenhouse, early Mars.
 *   Kopparapu et al. 2013, ApJ 765, 131, with its erratum (ApJ 770, 82): moist greenhouse (water loss), which the
 *     2014 paper does not update.
 * Values cross-checked against two independent published codes (rsnemmen/hz-calculator, hz.py; and
 * zclaytor/kiauhoku, extensions/calc_HZ.py, functions K13 and K14), which agree digit for digit. */
export const HZ_TEFF_MIN = 2600;
export const HZ_TEFF_MAX = 7200;
export const HZ_COEFFS = {
  recentVenus:   [1.776, 2.136e-4, 2.533e-8, -1.332e-11, -3.097e-15],
  runaway01:     [0.99, 1.209e-4, 1.404e-8, -7.418e-12, -1.713e-15],
  runaway1:      [1.107, 1.332e-4, 1.580e-8, -8.308e-12, -1.931e-15],
  runaway5:      [1.188, 1.433e-4, 1.707e-8, -8.968e-12, -2.084e-15],
  moistGreenhouse: [1.0146, 8.1884e-5, 1.9394e-9, -4.3618e-12, -6.8260e-16],
  maxGreenhouse: [0.356, 6.171e-5, 1.698e-9, -3.198e-12, -5.575e-16],
  earlyMars:     [0.320, 5.547e-5, 1.526e-9, -2.874e-12, -5.011e-16],
};
