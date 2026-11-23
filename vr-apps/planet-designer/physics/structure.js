/* Planet structure: radius from mass and composition (Zeng et al. tables), gravity, escape velocity, layers. */
import { MR_FE, MR_ROCK, MR_MGSIO3, MR_W50, MR_W100, ROCK_CMF } from '../data/massradius.js?v=6';
import { G, M_EARTH, R_EARTH, OCEAN_MASS, RHO_WATER } from './const.js?v=6';

export const PLANET_MASS_MIN = 0.05;
export const PLANET_MASS_MAX = 10;

/* log-log interpolation in a [mass, radius] table, extrapolated with the end slopes. */
function curve(tab, m) {
  const lm = Math.log(m);
  let i = 0;
  while (i < tab.length - 2 && Math.log(tab[i + 1][0]) < lm) i++;
  const [m0, r0] = tab[i], [m1, r1] = tab[i + 1];
  const f = (lm - Math.log(m0)) / (Math.log(m1) - Math.log(m0));
  return Math.exp(Math.log(r0) + f * (Math.log(r1) - Math.log(r0)));
}
const inRange = (tab, m) => m >= tab[0][0] && m <= tab[tab.length - 1][0];

/* Dry rocky radius for a core mass fraction: interpolate log R between the pure-rock (0), Earth-like (0.325) and
 * pure-iron (1) curves, linear in CMF. */
function rockyRadius(m, cmf) {
  const lr0 = Math.log(curve(MR_MGSIO3, m)), lrE = Math.log(curve(MR_ROCK, m)), lr1 = Math.log(curve(MR_FE, m));
  const lr = cmf <= ROCK_CMF ? lr0 + (lrE - lr0) * (cmf / ROCK_CMF)
    : lrE + (lr1 - lrE) * ((cmf - ROCK_CMF) / (1 - ROCK_CMF));
  return Math.exp(lr);
}

/* Radius with water: between the dry curve (w = 0), the 50 % water curve and the 100 % water curve, linear in w.
 * The water curves are for Earth-like rock under the water; the CMF effect is carried as a ratio. The 50 % table
 * starts at 0.5 Earth masses: below that we keep its ratio to the dry curve at 0.5. Approximate, and said so. */
function waterCurve(tab, m) {
  if (m >= tab[0][0]) return curve(tab, m);
  const m0 = tab[0][0];
  return curve(MR_ROCK, m) * (tab[0][1] / curve(MR_ROCK, m0));
}
export function planetRadius(massEarth, cmf, waterFrac) {
  const m = massEarth;
  const dry = rockyRadius(m, cmf);
  let r = dry;
  if (waterFrac > 0) {
    const scale = dry / curve(MR_ROCK, m);
    const r50 = waterCurve(MR_W50, m) * scale, r100 = curve(MR_W100, m);
    r = waterFrac <= 0.5 ? dry + (r50 - dry) * (waterFrac / 0.5) : r50 + (r100 - r50) * ((waterFrac - 0.5) / 0.5);
  }
  const valid = inRange(MR_ROCK, m) && (waterFrac < 0.001 || m >= MR_W50[0][0]);
  return { radiusEarth: r, valid };
}

/* Bulk properties. Returns SI values plus Earth-relative ones. */
export function bulk(massEarth, cmf, waterFrac) {
  const { radiusEarth, valid } = planetRadius(massEarth, cmf, waterFrac);
  const M = massEarth * M_EARTH, R = radiusEarth * R_EARTH;
  const g = G * M / (R * R);
  const vesc = Math.sqrt(2 * G * M / R);
  const density = M / (4 / 3 * Math.PI * R ** 3);
  return { radiusEarth, R, M, g, gEarth: g / 9.80665, vesc, density, validRadius: valid,
    /* Most planets above ~1.6 Earth radii carry thick hydrogen envelopes (Rogers 2015; Fulton et al. 2017). */
    likelyMiniNeptune: radiusEarth > 1.6 };
}

/* Layers for the cutaway, a two-layer approximation: core radius ~ CMF^0.5 of the rocky radius (Zeng & Jacobsen
 * 2017 give this scaling; Earth: 0.57 against the true 0.55), then mantle and crust, then the ocean as a shell
 * of the given mass. Depths in km, approximate. */
export function layers(massEarth, cmf, oceansEarth, radiusEarth) {
  const R = radiusEarth * R_EARTH;
  const oceanMass = oceansEarth * OCEAN_MASS;
  const oceanDepth = oceanMass / (RHO_WATER * 4 * Math.PI * R * R);       // global layer, before terrain
  const rockR = R - oceanDepth;
  const coreR = Math.sqrt(Math.max(0, cmf)) * rockR;
  const crust = Math.min(40e3, rockR * 0.006);                             // Earth: ~35 km continental, 7 km oceanic
  /* Solid inner core: Earth's is 0.35 of the core radius; small planets cool to a fully liquid or fully frozen core
   * differently, and Mars has no solid inner core (InSight). A placeholder rule until the thermal history (M6):
   * 0.35 above 0.6 Earth masses, fading to none at 0.2. */
  const inner = coreR * 0.35 * Math.min(1, Math.max(0, (massEarth - 0.2) / 0.4));
  return { innerCoreKm: inner / 1e3, coreKm: coreR / 1e3, mantleKm: (rockR - coreR - crust) / 1e3, crustKm: crust / 1e3,
    oceanDepthKm: oceanDepth / 1e3, radiusKm: R / 1e3, rockKm: rockR / 1e3, approximate: true };
}

/* Surface gravity in g for a given mass and radius (Earth units). */
export function gravityG(massEarth, radiusEarth) { return massEarth / (radiusEarth * radiusEarth); }
