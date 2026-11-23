/* derive(design) -> every derived number the panel and the ledger show. Pure; recomputed on every change. */
import { starAt, parRelativeToSun, snowLineAU, peakWavelength, blackbodyRGB, activityAuto, spectralLetter,
  STAR_MASS_HZ_VALID } from './star.js?v=5';
import { habitableZone, hzPosition, timeInHZ } from './hz.js?v=5';
import { periodYears, fluxEarth, eccMeanFactor, periApoFlux, hillRadiusAU, mutualHillSpacing, tidalLockBand,
  moonInfo } from './orbit.js?v=5';
import { bulk, layers } from './structure.js?v=5';
import { composition, atmosphereMass, scaleHeight, jeansLambda, T_EXO_EARTH, satVapourPa, boilingPointK,
  hasOzone, climate0D, bondAlbedo, TRIPLE_POINT_PA } from './atmosphere.js?v=5';
import { seaLevel, reliefMeters } from './terrain.js?v=5';
import { MOLAR, OCEAN_MASS, M_EARTH, RHO_WATER } from './const.js?v=5';
import { xuvEnvironment, shorelineI, cosmicDose } from './space.js?v=5';
import { wetBulb, runEBM } from './climate.js?v=5';

/* Default bulk densities for the simplified planets (kg/m^3), giving a radius from the mass:
 * rocky like Earth, icy like Ganymede/Callisto-type bodies, ice giant like Neptune, gas giant like Jupiter. */
const TYPE_DENSITY = { rocky: 5500, icy: 1900, iceGiant: 1640, gasGiant: 1330 };

/* ebm: optional map planet id -> { key, result } of energy-balance runs (they run in a worker; see main.js). */
export function derive(d, ebm = {}) {
  const st = starAt(d.star.massSun, d.star.ageGyr);
  const star = {
    ...st, massSun: d.star.massSun, letter: spectralLetter(st.spt),
    peakNm: peakWavelength(st.teff) * 1e9, rgb: blackbodyRGB(st.teff),
    par: parRelativeToSun(st.teff), snowLineAU: snowLineAU(st.lum),
    activity: d.star.activity === 'auto' ? activityAuto(d.star.massSun, d.star.ageGyr) : d.star.activity,
    remainingGyr: Math.max(0, st.lifetime - d.star.ageGyr),
    hzModelValid: d.star.massSun <= STAR_MASS_HZ_VALID,
  };
  star.hz = habitableZone(st.teff, st.lum, 1);

  const planets = d.planets.map((p) => derivePlanet(p, d, star, ebm));
  /* Stability: each planet against its neighbours, in mutual Hill radii. */
  const order = planets.map((p, i) => ({ i, aAU: p.aAU, e: p.e, massEarth: p.massEarth })).sort((a, b) => a.aAU - b.aAU);
  for (let k = 0; k < order.length; k++) {
    const me = planets[order[k].i];
    let minSpacing = Infinity, neighbour = null;
    for (const j of [k - 1, k + 1]) {
      if (j < 0 || j >= order.length) continue;
      const s = mutualHillSpacing(order[k], order[j], d.star.massSun);
      if (s < minSpacing) { minSpacing = s; neighbour = planets[order[j].i].name; }
    }
    me.hillSpacing = minSpacing; me.hillNeighbour = neighbour;
  }
  return { star, planets, nPlanets: planets.length };
}

function derivePlanet(p, d, star, ebm) {
  const out = { id: p.id, name: p.name, role: p.role, type: p.type, aAU: p.orbit.aAU, e: p.orbit.e,
    massEarth: p.bulk.massEarth };
  out.periodYr = periodYears(p.orbit.aAU, d.star.massSun);
  out.flux = fluxEarth(star.lum, p.orbit.aAU);
  out.fluxMean = out.flux * eccMeanFactor(p.orbit.e);
  const pa = periApoFlux(star.lum, p.orbit.aAU, p.orbit.e);
  out.fluxPeri = pa.peri; out.fluxApo = pa.apo;
  out.hillAU = hillRadiusAU(p.orbit.aAU, p.orbit.e, p.bulk.massEarth, d.star.massSun);
  if (p.role !== 'hz') {
    const rho = TYPE_DENSITY[p.type] || 5500;
    const R = Math.cbrt(p.bulk.massEarth * M_EARTH / (4 / 3 * Math.PI * rho));
    out.radiusEarth = R / 6.371e6;
    return out;
  }

  /* Bulk and structure */
  const waterFrac = Math.max(p.bulk.waterMassFraction, p.surface.oceansEarth * OCEAN_MASS / (p.bulk.massEarth * M_EARTH));
  const b = bulk(p.bulk.massEarth, p.bulk.coreMassFraction, waterFrac);
  Object.assign(out, { radiusEarth: b.radiusEarth, g: b.g, gEarth: b.gEarth, vesc: b.vesc, density: b.density,
    validRadius: b.validRadius, likelyMiniNeptune: b.likelyMiniNeptune, waterFrac });
  out.layers = layers(p.bulk.massEarth, p.bulk.coreMassFraction, p.surface.oceansEarth, b.radiusEarth);

  /* HZ for this planet's mass, position in it, and time in it */
  out.hz = habitableZone(star.teff, star.lum, p.bulk.massEarth);
  out.hzPos = hzPosition(out.fluxMean, out.hz);
  out.periInHZ = hzPosition(out.fluxPeri, out.hz) !== 'hot';
  out.apoInHZ = hzPosition(out.fluxApo, out.hz) !== 'cold';
  out.chz = timeInHZ(d.star.massSun, d.star.ageGyr, p.orbit.aAU, eccMeanFactor(p.orbit.e), p.bulk.massEarth);

  /* Spin */
  out.lock = tidalLockBand(p.orbit.aAU, d.star.massSun, p.bulk.massEarth, b.radiusEarth, d.star.ageGyr);
  out.locked = out.lock.state === 'locked' || out.lock.state === 'likely';
  out.dayHours = out.locked ? out.periodYr * 8766 : p.spin.dayHours;
  out.obliquityDeg = p.spin.obliquityDeg;

  /* Moons */
  out.moons = p.moons.map((m) => moonInfo(m, { massEarth: p.bulk.massEarth, radiusEarth: b.radiusEarth },
    { massSun: d.star.massSun, radius: star.radius }, p.orbit.aAU, p.orbit.e));
  out.obliquityStable = out.moons.some((m) => m.stabilises && !m.insideRoche && !m.unbound);

  /* Surface */
  /* Surface: the sea level follows from the ocean volume and the terrain's hypsometry (physics/terrain.js). */
  out.oceans = p.surface.oceansEarth;
  out.terrainSeed = p.surface.terrainSeed;
  out.reliefM = reliefMeters(b.gEarth);
  const sea = seaLevel(p.surface.terrainSeed, p.surface.oceansEarth * OCEAN_MASS / RHO_WATER, b.R, out.reliefM);
  out.sea = sea;
  out.landFrac = sea.landFrac;
  out.oceanDepthKm = sea.meanDepth / 1e3;
  out.highestKm = sea.maxAbove / 1e3;

  /* Atmosphere, with water vapour from the climate (iterate twice: vapour depends on temperature) */
  const albedo = bondAlbedo(out.landFrac, p.surface.landAlbedo);
  let pH2O = 0, comp, clim;
  for (let k = 0; k < 3; k++) {
    comp = composition(p.atmosphere, pH2O);
    clim = climate0D({ S: out.fluxMean, albedo, pCO2bar: p.atmosphere.pCO2bar, pCH4bar: p.atmosphere.pCH4bar,
      totalBar: comp.totalBar });
    /* Relative humidity 0.8 over the oceans, scaled by the ocean cover; none without water. */
    const wet = p.surface.oceansEarth > 0 ? 0.8 * (1 - out.landFrac * 0.5) : 0;
    pH2O = Math.min(wet * satVapourPa(Math.min(clim.Ts, 373)) / 1e5, 0.2);
  }
  out.albedo = albedo;
  out.atm = comp;
  out.pBar = comp.totalBar;
  out.atmMassEarth = atmosphereMass(comp.totalPa, b.radiusEarth * 6.371e6, b.g) / 5.15e18;
  out.climate = clim;
  out.Ts = clim.Ts;
  out.TsC = clim.Ts - 273.15;
  out.Teq = clim.Teq;
  out.scaleHeightKm = scaleHeight(clim.Ts, comp.molar, b.g) / 1e3;
  /* the star's XUV: exosphere temperature, the cosmic shoreline, and the surface dose from cosmic rays */
  out.xuv = xuvEnvironment(d.star.massSun, d.star.ageGyr, p.orbit.aAU, star.lum);
  out.jeans = {};
  for (const k of ['N2', 'O2', 'CO2', 'H2O', 'H']) out.jeans[k] = jeansLambda(b.vesc, MOLAR[k], out.xuv.Texo);
  out.jeansEarthExo = jeansLambda(b.vesc, MOLAR.N2, T_EXO_EARTH);
  out.shoreRatio = out.xuv.fluence / shorelineI(b.vesc / 1e3);
  out.columnGcm2 = comp.totalPa / b.g / 10;
  out.doseMSv = cosmicDose(out.columnGcm2, star.activity);
  out.ozone = hasOzone(p.atmosphere.pO2bar);
  out.boilK = boilingPointK(comp.totalPa);
  out.aboveTriple = comp.totalPa > TRIPLE_POINT_PA;
  out.liquidWater = p.surface.oceansEarth > 0 && out.aboveTriple && !clim.runaway &&
    clim.Ts > 273.15 - 15 && clim.Ts < out.boilK;                    // global mean; ice caps and seas can coexist
  out.o2Frac = comp.totalBar > 0 ? p.atmosphere.pO2bar / comp.totalBar : 0;
  out.pO2kPa = p.atmosphere.pO2bar * 100;
  out.pCO2kPa = p.atmosphere.pCO2bar * 100;
  out.pN2bar = p.atmosphere.pN2bar;

  /* Photosynthesis: PAR photons at the planet relative to Earth's */
  out.parRel = out.fluxMean * star.par.photonsPerW;

  /* Tectonics likelihood (contested; a band, not a verdict): favoured by water, Earth-like mass, internal heat. */
  const m = p.bulk.massEarth;
  out.tectonics = waterFrac > 1e-5 && m >= 0.5 && m <= 5 && p.bulk.radiogenicRel >= 0.5 ? 'likely'
    : waterFrac > 1e-5 && m >= 0.3 && m <= 8 ? 'uncertain' : 'unlikely';
  out.thermostat = out.tectonics === 'likely' && out.landFrac > 0.02 && p.surface.oceansEarth > 0.05 ? 'active'
    : out.tectonics !== 'unlikely' && p.surface.oceansEarth > 0 ? 'partial' : 'absent';

  /* Clouds (for the picture; the climate's albedo already includes Earth-like cloud cover): water and warmth make
   * clouds, a runaway greenhouse makes a total haze deck, a frozen world few. */
  out.cloudFrac = clim.runaway ? 1 : p.surface.oceansEarth <= 0 ? 0 : Math.min(0.75, 0.2 + 0.45 * (1 - out.landFrac)) * (clim.Ts > 250 ? 1 : 0.5);

  /* The energy balance model: its parameters, and its result if a run for exactly these parameters exists. */
  out.ebmParams = clim.runaway ? null : {
    S: out.flux, e: p.orbit.e, obliquityDeg: p.spin.obliquityDeg, argPeriDeg: p.orbit.argPeriDeg || 0,
    dayHours: out.dayHours, locked: out.locked, pBar: comp.totalBar, molar: comp.molar,
    forcing: clim.forcingCO2 + (clim.forcingCH4 || 0), greenhouse: Math.min(1, Math.sqrt(Math.max(comp.totalBar, 0))),
    albedoFree: albedo + 0.015,   /* calibrated: Earth 14.4 C with polar ice */ albedoIce: 0.62, landFrac: out.landFrac, hasWater: p.surface.oceansEarth > 0,
    periodYears: out.periodYr, years: 30,
  };
  out.ebmKey = out.ebmParams ? JSON.stringify(out.ebmParams) : null;
  const run = ebm[p.id];
  if (run && run.key === out.ebmKey) applyEBM(out, run.result);
  else out.ebmPending = !!out.ebmParams;
  return out;
}

/* derive() with the climate model run synchronously (node tests, and the browser's fallback without workers). */
export function deriveSync(d, ebm = {}) {
  const D0 = derive(d, ebm);
  const runs = { ...ebm };
  let any = false;
  for (const P of D0.planets) if (P.ebmParams && !(runs[P.id] && runs[P.id].key === P.ebmKey)) {
    runs[P.id] = { key: P.ebmKey, result: runEBM(P.ebmParams) }; any = true;
  }
  return any ? derive(d, runs) : D0;
}

/* Bring the climate model's result into the planet: mean temperature, where water is liquid, ice, humid heat,
 * and the land with a liveable climate. */
function applyEBM(out, r) {
  out.ebm = r;
  out.TsC = r.gmean; out.Ts = r.gmean + 273.15;
  const boilC = out.boilK - 273.15;
  const meanWarm = r.mean.some((t) => t > 0 && t < boilC);
  out.liquidWater = out.oceans > 0 && out.aboveTriple && meanWarm;
  out.seasonalOnly = out.oceans > 0 && out.aboveTriple && !meanWarm && r.max.some((t) => t > 0);
  out.snowball = r.snowball;
  out.iceFrac = r.iceFrac;
  let tw = -99;
  for (let j = 0; j < r.max.length; j++) if (r.max[j] > -10) tw = Math.max(tw, wetBulb(Math.min(r.max[j], 55), 70));
  out.wetBulbMax = out.oceans > 0 ? tw : null;
  const comfy = r.mean.filter((t, j) => t >= 0 && t <= 30 && r.max[j] < 45 && r.min[j] > -45).length / r.mean.length;
  out.comfortFrac = comfy;
  out.habLand = comfy * out.landFrac;
}
