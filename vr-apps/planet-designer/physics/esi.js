/* Earth Similarity Index (Schulze-Makuch et al. 2011, Astrobiology 11, 1041):
 *   ESI = prod_i (1 - |(x_i - x_i0) / (x_i + x_i0)|)^(w_i / n),  n = 4,
 * radius (w 0.57), bulk density (1.07), escape velocity (0.70), surface temperature in K (5.58), Earth as reference.
 * It measures similarity to Earth, not habitability: shown small, never as the verdict (spec §5.11). */
const REF = { r: 1, rho: 5514, v: 11186, T: 288 };
const W = { r: 0.57, rho: 1.07, v: 0.70, T: 5.58 };
export function esi(P) {
  if (!P || P.role !== 'hz') return null;
  const term = (x, x0, w) => (1 - Math.abs((x - x0) / (x + x0))) ** (w / 4);
  return term(P.radiusEarth, REF.r, W.r) * term(P.density, REF.rho, W.rho) * term(P.vesc, REF.v, W.v) * term(P.Ts, REF.T, W.T);
}
