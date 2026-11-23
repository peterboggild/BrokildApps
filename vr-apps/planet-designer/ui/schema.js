/* The one UI description (spec §11.1): tabs of rows, rendered to the DOM on the PC now (ui/dom.js) and to the VR
 * canvas panel later. Rows are data, not code paths:
 *   { kind: 'slider', path, min, max, step | log: true, unit, level, digits }
 *   { kind: 'chips', path, options: [value...], label: key prefix, level }
 *   { kind: 'readout', label, value: (D, P, d) => string, level }
 *   { kind: 'warn', value: (D, P, d) => key | null }
 *   { kind: 'planets' } | { kind: 'moons' } | { kind: 'system' }   (custom widgets)
 * level: 1 = Explorer, 2 = Student, 3 = Scientist; a row shows at its level and above.
 * Paths: 'star.*' on the design, 'planet.*' on the selected planet. */
import { t, fmt, esc } from '../i18n.js?v=11';
import { ECC_MAX } from '../physics/orbit.js?v=11';
import { rgbToCss } from '../physics/star.js?v=11';
import { climateChart, shorelineChart } from './charts.js?v=11';

const f = fmt;
const u = (k) => t('u.' + k);
const auBand = (lo, hi) => `${f(lo)}–${f(hi)} ${u('AU')}`;

export const TABS = [
  { id: 'star', rows: [
    { kind: 'slider', path: 'star.massSun', min: 0.08, max: 2.0, log: true, unit: 'Msun', level: 1, digits: 2 },
    { kind: 'slider', path: 'star.ageGyr', min: 0.01, max: 15, log: true, unit: 'Gyr', level: 1, digits: 2 },
    { kind: 'slider', path: 'star.metallicityFeH', min: -1, max: 0.5, step: 0.05, unit: '', level: 3, digits: 2 },
    { kind: 'chips', path: 'star.activity', options: ['auto', 'low', 'normal', 'high'], label: 'act', level: 3 },
    { kind: 'readout', label: 'r.class', level: 1,
      value: (D) => `<span class="swatch" style="background:${rgbToCss(D.star.rgb)}"></span>${D.star.spt}` },
    { kind: 'readout', label: 'r.teff', level: 1, value: (D) => `${f(D.star.teff, 3)} ${u('K')}` },
    { kind: 'readout', label: 'r.lum', level: 1, value: (D) => `${f(D.star.lum, 3)} ${u('Lsun')}` },
    { kind: 'readout', label: 'r.radius', level: 2, value: (D) => `${f(D.star.radius, 3)} ${u('Rsun')}` },
    { kind: 'readout', label: 'r.life', level: 1, value: (D) => `${f(D.star.lifetime, 3)} ${u('Gyr')}` },
    { kind: 'readout', label: 'r.remaining', level: 2, value: (D) => `${f(D.star.remainingGyr, 3)} ${u('Gyr')}` },
    { kind: 'readout', label: 'r.peak', level: 2, value: (D) => `${f(D.star.peakNm, 3)} ${u('nm')}` },
    { kind: 'readout', label: 'r.par', level: 3, value: (D) => f(D.star.par.photonsPerW, 2) },
    { kind: 'readout', label: 'r.hzCons', level: 1, value: (D) => auBand(D.star.hz.d.runaway, D.star.hz.d.maxGreenhouse) },
    { kind: 'readout', label: 'r.hzOpt', level: 2, value: (D) => auBand(D.star.hz.d.recentVenus, D.star.hz.d.earlyMars) },
    { kind: 'readout', label: 'r.snow', level: 2, value: (D) => `${f(D.star.snowLineAU)} ${u('AU')}` },
    { kind: 'warn', value: (D) => (D.star.hz.valid ? null : 'r.hzOutside') },
    { kind: 'warn', value: (D) => (D.star.hzModelValid ? null : 'warn.hzStar') },
    { kind: 'warn', value: (D) => (D.star.outsideMS ? 'warn.ms' : null) },
  ] },
  { id: 'orbit', rows: [
    { kind: 'planets' },
    { kind: 'slider', path: 'planet.orbit.aAU', min: 0.01, max: 50, log: true, unit: 'AU', level: 1, digits: 3 },
    { kind: 'action', label: 'act.toZone', hz: true, act: (S) => S.toZone() },
    { kind: 'slider', path: 'planet.orbit.e', min: 0, max: ECC_MAX, step: 0.005, unit: '', level: 1, digits: 3 },
    { kind: 'slider', path: 'planet.orbit.incDeg', min: 0, max: 10, step: 0.1, unit: 'deg', level: 3, digits: 2 },
    { kind: 'slider', path: 'planet.spin.dayHours', min: 4, max: 10000, log: true, unit: 'h', level: 1, digits: 3, hz: true },
    { kind: 'slider', path: 'planet.spin.obliquityDeg', min: 0, max: 90, step: 0.5, unit: 'deg', level: 1, digits: 3, hz: true },
    { kind: 'readout', label: 'r.hzPos', level: 1, hz: true, value: (D, P) => t('pos.' + P.hzPos) },
    { kind: 'readout', label: 'r.flux', level: 1, value: (D, P) => f(P.fluxMean, 3) },
    { kind: 'readout', label: 'r.fluxPA', level: 2, value: (D, P) => `${f(P.fluxPeri, 3)} / ${f(P.fluxApo, 3)}` },
    { kind: 'readout', label: 'r.period', level: 1,
      value: (D, P) => (P.periodYr < 0.3 ? `${f(P.periodYr * 365.25, 3)} ${u('days')}` : `${f(P.periodYr, 3)} ${u('yr')}`) },
    { kind: 'readout', label: 'r.lock', level: 1, hz: true,
      value: (D, P) => `${t('lock.' + P.lock.state)} (${f(P.lock.lo)}–${f(P.lock.hi)} ${u('Gyr')})` },
    { kind: 'readout', label: 'r.hill', level: 3, value: (D, P) => `${f(P.hillAU, 3)} ${u('AU')}` },
    { kind: 'readout', label: 'r.stable', level: 2,
      value: (D, P) => (isFinite(P.hillSpacing) ? `${f(P.hillSpacing, 3)} (${esc(P.hillNeighbour)})` : '–') },
  ] },
  { id: 'planet', rows: [
    { kind: 'planets' },
    { kind: 'chips', path: 'planet.type', options: ['rocky', 'icy', 'iceGiant', 'gasGiant'], label: 'type', level: 1, other: true },
    { kind: 'slider', path: 'planet.bulk.massEarth', min: 0.05, max: 10, log: true, unit: 'Mearth', level: 1, digits: 3, hz: true },
    { kind: 'slider', path: 'planet.bulk.massEarth', min: 0.01, max: 4000, log: true, unit: 'Mearth', level: 1, digits: 3, other: true },
    { kind: 'slider', path: 'planet.bulk.coreMassFraction', min: 0, max: 0.8, step: 0.005, unit: 'pct', pct: true, level: 1, digits: 3, hz: true },
    { kind: 'slider', path: 'planet.bulk.waterMassFraction', min: 0.00001, max: 0.5, log: true, unit: 'pct', pct: true, level: 2, digits: 2, hz: true },
    { kind: 'slider', path: 'planet.bulk.radiogenicRel', min: 0.1, max: 3, log: true, unit: '', level: 3, digits: 2, hz: true },
    { kind: 'readout', label: 'r.radiusP', level: 1, value: (D, P) => `${f(P.radiusEarth, 3)} ${u('Re')}` },
    { kind: 'readout', label: 'r.g', level: 1, hz: true, value: (D, P) => `${f(P.gEarth, 3)} g (${f(P.g, 3)} m/s²)` },
    { kind: 'readout', label: 'r.vesc', level: 2, hz: true, value: (D, P) => `${f(P.vesc / 1e3, 3)} ${u('kms')}` },
    { kind: 'readout', label: 'r.density', level: 2, hz: true, value: (D, P) => `${f(P.density / 1e3, 3)} ${u('gcc')}` },
    { kind: 'readout', label: 'r.layers', level: 2, hz: true,
      value: (D, P) => t('r.layersV', { c: Math.round(P.layers.coreKm), m: Math.round(P.layers.mantleKm), k: Math.round(P.layers.crustKm) }) },
    { kind: 'readout', label: 'r.tect', level: 2, hz: true, value: (D, P) => t('tect.' + P.tectonics) },
    { kind: 'warn', value: (D, P) => (P.likelyMiniNeptune ? 'warn.mini' : null) },
    { kind: 'warn', value: (D, P) => (P.role === 'hz' && !P.validRadius ? 'warn.radius' : null) },
  ] },
  { id: 'surface', hzOnly: true, rows: [
    { kind: 'planets' },
    { kind: 'slider', path: 'planet.surface.oceansEarth', min: 0, max: 50, step: 0.01, sqrt: true, unit: '', level: 1, digits: 3 },
    { kind: 'slider', path: 'planet.surface.landAlbedo', min: 0.1, max: 0.4, step: 0.01, unit: '', level: 2, digits: 2 },
    { kind: 'seed' },
    { kind: 'readout', label: 'r.land', level: 1, value: (D, P) => `${f(P.landFrac * 100, 3)} %` },
    { kind: 'readout', label: 'r.highest', level: 1, value: (D, P) => `${f(P.highestKm, 3)} ${u('km')}` },
    { kind: 'readout', label: 'r.oceanDepth', level: 1, value: (D, P) => `${f(P.oceanDepthKm, 3)} ${u('km')}` },
    { kind: 'readout', label: 'r.albedo', level: 2, value: (D, P) => f(P.albedo, 2) },
  ] },
  { id: 'air', hzOnly: true, rows: [
    { kind: 'planets' },
    { kind: 'slider', path: 'planet.atmosphere.pN2bar', min: 0, max: 10, gas: true, unit: 'bar', level: 1, digits: 3 },
    { kind: 'slider', path: 'planet.atmosphere.pO2bar', min: 0, max: 2, gas: true, unit: 'bar', level: 1, digits: 3 },
    { kind: 'slider', path: 'planet.atmosphere.pCO2bar', min: 0, max: 100, gas: true, unit: 'bar', level: 1, digits: 3 },
    { kind: 'slider', path: 'planet.atmosphere.pArbar', min: 0, max: 1, gas: true, unit: 'bar', level: 3, digits: 2 },
    { kind: 'slider', path: 'planet.atmosphere.pCH4bar', min: 0, max: 0.01, gas: true, unit: 'bar', level: 2, digits: 2 },
    { kind: 'readout', label: 'r.pTotal', level: 1, value: (D, P) => `${f(P.pBar, 3)} ${u('bar')}` },
    { kind: 'readout', label: 'r.h2o', level: 2, value: (D, P) => `${f(P.atm.p.H2O, 2)} ${u('bar')}` },
    { kind: 'readout', label: 'r.atmMass', level: 2, value: (D, P) => f(P.atmMassEarth, 3) },
    { kind: 'readout', label: 'r.Teq', level: 2, value: (D, P) => `${f(P.Teq - 273.15, 3)} °C` },
    { kind: 'readout', label: 'r.Ts', level: 1, value: (D, P) => (P.climate.runaway ? t('v.runawayT') : `${f(P.TsC, 3)} °C`) },
    { kind: 'readout', label: 'r.boil', level: 2, value: (D, P) => `${f(P.boilK - 273.15, 3)} °C` },
    { kind: 'readout', label: 'r.H', level: 3, value: (D, P) => `${f(P.scaleHeightKm, 3)} ${u('km')}` },
    { kind: 'readout', label: 'r.jeans', level: 3, value: (D, P) => `${f(P.jeans.N2, 3)} / ${f(P.jeans.H, 2)}` },
    { kind: 'readout', label: 'r.ozone', level: 1, value: (D, P) => t(P.ozone ? 'yes' : 'no') },
    { kind: 'warn', value: (D, P) => (P.climate.valid ? null : 'warn.climate') },
  ] },
  { id: 'climate', hzOnly: true, rows: [
    { kind: 'planets' },
    { kind: 'heading', label: 'ch.tlat' },
    { kind: 'chart', value: (D, P) => climateChart(P) },
    { kind: 'readout', label: 'r.Ts', level: 1, value: (D, P) => (P.climate.runaway ? t('v.runawayT') : `${f(P.TsC, 3)} °C`) },
    { kind: 'readout', label: 'r.iceLine', level: 1, value: (D, P) => (!P.ebm ? '–' : P.ebm.snowball ? t('ch.snowball') : P.ebm.iceLatN >= 89.9 ? t('none') : `${f(P.ebm.iceLatN, 2)}°`) },
    { kind: 'readout', label: 'r.wb', level: 2, value: (D, P) => (P.wetBulbMax == null ? '–' : `${f(P.wetBulbMax, 3)} °C`) },
    { kind: 'readout', label: 'r.habLand', level: 1, value: (D, P) => (P.ebm ? `${f(P.habLand * 100, 3)} %` : '–') },
    { kind: 'heading', label: 'ch.shore' },
    { kind: 'chart', value: (D, P) => shorelineChart(P) },
    { kind: 'readout', label: 'r.fluence', level: 1, value: (D, P) => f(P.xuv.fluence, 3) },
    { kind: 'readout', label: 'r.xuvNow', level: 2, value: (D, P) => f(P.xuv.flux, 3) },
    { kind: 'readout', label: 'r.texo', level: 3, value: (D, P) => `${f(P.xuv.Texo, 3)} K` },
    { kind: 'readout', label: 'r.dose', level: 1, value: (D, P) => `${f(P.doseMSv, 3)} mSv/${t('u.yr')}` },
    { kind: 'readout', label: 'r.column', level: 3, value: (D, P) => `${f(P.columnGcm2, 3)} g/cm²` },
  ] },
  { id: 'moons', hzOnly: true, rows: [{ kind: 'planets' }, { kind: 'moons' }] },
  { id: 'system', rows: [{ kind: 'system' }] },
];

export const LEVELS = { explorer: 1, student: 2, scientist: 3 };
