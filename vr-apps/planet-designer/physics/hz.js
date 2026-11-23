/* The habitable zone (Kopparapu et al. 2013, 2014) and the continuously habitable zone. */
import { HZ_COEFFS, HZ_TEFF_MIN, HZ_TEFF_MAX } from '../data/hz.js?v=14';
import { starAt, lifetimeGyr } from './star.js?v=14';

function seffPoly(c, teff) {
  const t = teff - 5780;
  return c[0] + c[1] * t + c[2] * t * t + c[3] * t ** 3 + c[4] * t ** 4;
}

/* Runaway greenhouse for the planet's mass: interpolate the 0.1, 1 and 5 Earth-mass fits in log mass,
 * clamped to that range (the paper gives no others). */
function seffRunaway(teff, massEarth) {
  const lm = Math.log10(Math.max(0.1, Math.min(5, massEarth || 1)));
  const s01 = seffPoly(HZ_COEFFS.runaway01, teff), s1 = seffPoly(HZ_COEFFS.runaway1, teff),
    s5 = seffPoly(HZ_COEFFS.runaway5, teff);
  if (lm <= 0) return s1 + (s01 - s1) * (-lm);              // log10(0.1) = -1
  return s1 + (s5 - s1) * (lm / Math.log10(5));
}

/* Effective fluxes (Earth = 1) and distances (AU) for the five limits.
 * Returns { valid, seff: {...}, d: {...} }, valid=false when teff is outside 2600-7200 K (then extrapolated,
 * and the caller must say "outside the model"). Teff is clamped for the polynomial so it never runs wild. */
export function habitableZone(teff, lum, massEarth = 1) {
  const valid = teff >= HZ_TEFF_MIN && teff <= HZ_TEFF_MAX;
  const tc = Math.max(HZ_TEFF_MIN, Math.min(HZ_TEFF_MAX, teff));
  const seff = {
    recentVenus: seffPoly(HZ_COEFFS.recentVenus, tc),
    runaway: seffRunaway(tc, massEarth),
    moistGreenhouse: seffPoly(HZ_COEFFS.moistGreenhouse, tc),
    maxGreenhouse: seffPoly(HZ_COEFFS.maxGreenhouse, tc),
    earlyMars: seffPoly(HZ_COEFFS.earlyMars, tc),
  };
  const d = {};
  for (const k in seff) d[k] = Math.sqrt(lum / seff[k]);
  return { valid, seff, d };
}

/* Where an effective flux S (Earth = 1) sits relative to the zone:
 *   'hot' (inside recent Venus), 'optInner' (between recent Venus and runaway), 'cons' (conservative zone),
 *   'optOuter' (between maximum greenhouse and early Mars), 'cold' (beyond early Mars). */
export function hzPosition(S, hz) {
  const s = hz.seff;
  if (S > s.recentVenus) return 'hot';
  if (S > s.runaway) return 'optInner';
  if (S >= s.maxGreenhouse) return 'cons';
  if (S >= s.earlyMars) return 'optOuter';
  return 'cold';
}

/* Time in the habitable zone as the star brightens. For a planet receiving S = L(t)/a^2 x eccFactor, sample the
 * star from 0.1 Gyr to the end of its main sequence (capped at 15 Gyr) and count the time inside the conservative
 * zone (and the optimistic one). Returns Gyr spent so far (up to the star's age) and in total, plus when it
 * entered and leaves. The pre-main-sequence phase is not modelled. */
export function timeInHZ(massSun, ageGyr, aAU, eccFactor, massEarth) {
  const end = Math.min(lifetimeGyr(massSun), 15);
  const n = 300, t0 = 0.1;
  const out = { consTotal: 0, consSoFar: 0, optTotal: 0, optSoFar: 0, enter: null, leave: null, end };
  const dt = (end - t0) / n;
  for (let i = 0; i < n; i++) {
    const t = t0 + (i + 0.5) * dt;
    const st = starAt(massSun, t);
    const hz = habitableZone(st.teff, st.lum, massEarth);
    const S = st.lum / (aAU * aAU) * eccFactor;
    const pos = hzPosition(S, hz);
    const cons = pos === 'cons', opt = cons || pos === 'optInner' || pos === 'optOuter';
    if (cons) { out.consTotal += dt; if (t < ageGyr) out.consSoFar += dt;
      if (out.enter === null) out.enter = t - dt / 2; out.leave = t + dt / 2; }
    if (opt) { out.optTotal += dt; if (t < ageGyr) out.optSoFar += dt; }
  }
  return out;
}
