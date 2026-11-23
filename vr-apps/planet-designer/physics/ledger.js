/* The Habitability Ledger: criteria with green/amber/red status, grouped in five tiers (spec §6, §4.11).
 * Returns language-neutral rows: { id, tier, status, value: {k, p}, why: {k, p}, src, contested }.
 * The UI turns the keys into text (i18n.js). Status 'na' = not computed yet (a later milestone); it never blocks. */
import { HILL_TWO_PLANET, HILL_MULTI } from './orbit.js?v=14';
import { ARMSTRONG_PA } from './atmosphere.js?v=14';

const band = (x, g0, g1, a0, a1) => (x >= g0 && x <= g1 ? 'green' : x >= a0 && x <= a1 ? 'amber' : 'red');

export const TIERS = [1, 2, 3, 4, 5];

export function ledger(P, D) {
  const S = D.star;
  const rows = [];
  const add = (tier, id, status, value, why, src, contested = false) =>
    rows.push({ tier, id, status, value, why, src, contested });

  /* ---- Tier 1: liquid water is possible ---- */
  const posStatus = { cons: 'green', optInner: 'amber', optOuter: 'amber', hot: 'red', cold: 'red' }[P.hzPos];
  add(1, 'hz', P.hz.valid ? posStatus : (posStatus === 'green' ? 'amber' : posStatus),
    { k: 'v.flux', p: { s: P.fluxMean } }, { k: 'w.hz.' + P.hzPos + (P.hz.valid ? '' : '.outside') },
    'Kopparapu et al. 2013, 2014');
  /* Only meaningful when the annual mean is inside the optimistic zone; otherwise the 'hz' row already says no. */
  const meanInside = P.hzPos !== 'hot' && P.hzPos !== 'cold';
  const leaves = meanInside && (!P.periInHZ || !P.apoInHZ);
  add(1, 'ecc', leaves || P.e > 0.3 ? 'red' : P.e > 0.1 ? 'amber' : 'green',
    { k: 'v.ecc', p: { e: P.e, r: P.fluxPeri / P.fluxApo } },
    { k: leaves ? 'w.ecc.leaves' : P.e > 0.1 ? 'w.ecc.swing' : 'w.ecc.ok' }, 'Williams & Pollard 2002');
  add(1, 'runaway', P.climate.runaway ? 'red' : 'green',
    { k: 'v.absorbed', p: { f: P.climate.absorbed } }, { k: P.climate.runaway ? 'w.runaway.yes' : 'w.runaway.no' },
    'Goldblatt et al. 2013');
  add(1, 'triple', P.aboveTriple ? 'green' : 'red', { k: 'v.bar', p: { p: P.pBar } },
    { k: P.aboveTriple ? 'w.triple.ok' : 'w.triple.low' }, 'IAPWS triple point 611.657 Pa');
  const frozen = P.ebm ? (P.snowball || P.seasonalOnly) && P.oceans > 0 : P.oceans > 0 && !P.climate.runaway && P.Ts <= 273.15 - 15;
  const tVal = P.climate.runaway ? { k: 'v.runawayT' } : { k: 'v.tempC', p: { t: P.TsC } };
  add(1, 'liquid', P.liquidWater ? 'green' : frozen ? 'amber' : 'red', tVal,
    { k: P.oceans <= 0 ? 'w.liquid.dry' : P.liquidWater ? (P.climate.valid ? 'w.liquid.ok' : 'w.liquid.okRough')
      : frozen ? (P.seasonalOnly ? 'w.liquid.seasonal' : 'w.liquid.ice') : P.climate.runaway ? 'w.liquid.boiled' : 'w.liquid.boil' },
    P.ebm ? 'energy balance model (North 1981; Williams & Kasting 1997)' : 'climate estimate');

  /* ---- Tier 2: microbes ---- */
  const chz = P.chz.consSoFar;
  add(2, 'chz', chz >= 4 ? 'green' : chz >= 1 ? 'amber' : 'red',
    { k: 'v.gyr2', p: { a: chz, b: P.chz.consTotal } }, { k: chz >= 1 ? 'w.chz.ok' : 'w.chz.short' },
    'Kopparapu 2013; Gough 1981');
  /* Keeping the air: the cosmic shoreline (XUV fluence against escape velocity) first, the Jeans parameter of N2
   * at the exosphere temperature as the second check; the worse of the two counts. */
  const lam = P.jeans.N2, sr = P.shoreRatio;
  const stShore = sr < 0.3 ? 'green' : sr < 1 ? 'amber' : 'red';
  const stJeans = lam >= 50 ? 'green' : lam >= 20 ? 'amber' : 'red';
  const rank = { green: 0, amber: 1, red: 2 };
  const stRet = rank[stShore] >= rank[stJeans] ? stShore : stJeans;
  add(2, 'retain', stRet, { k: 'v.shore', p: { r: sr, l: lam } },
    { k: stRet === 'green' ? 'w.retain.ok' : stRet === 'amber' ? 'w.retain.slow' : 'w.retain.lost' },
    'Zahnle & Catling 2017; Jeans escape');
  add(2, 'envelope', P.likelyMiniNeptune ? 'red' : P.radiusEarth > 1.4 ? 'amber' : 'green',
    { k: 'v.radius', p: { r: P.radiusEarth } }, { k: P.likelyMiniNeptune ? 'w.env.gas' : 'w.env.rock' },
    'Rogers 2015; Fulton et al. 2017');
  const shield = P.ozone && P.pBar >= 0.3 ? 'green' : P.oceans > 0 || P.pBar > 1 ? 'amber' : 'red';
  add(2, 'shield', shield, { k: P.ozone ? 'v.ozone' : 'v.noOzone' },
    { k: shield === 'green' ? 'w.shield.ok' : shield === 'amber' ? 'w.shield.sea' : 'w.shield.none' },
    'Segura et al. 2003');
  const mdwarf = S.massSun < 0.5;
  add(2, 'activity', mdwarf || S.activity === 'high' ? 'amber' : 'green', { k: 'v.activity.' + S.activity },
    { k: mdwarf ? 'w.act.mdwarf' : S.activity === 'high' ? 'w.act.young' : 'w.act.calm' },
    'Luger & Barnes 2015; Shields et al. 2016', mdwarf);

  /* ---- Tier 3: complex life ---- */
  add(3, 'time', chz >= 4 ? 'green' : 'red', { k: 'v.gyr', p: { a: chz } },
    { k: chz >= 4 ? 'w.time.ok' : S.lifetime < 4 ? 'w.time.star' : 'w.time.young' }, 'Earth history (fossil record)');
  add(3, 'par', P.parRel >= 0.5 ? 'green' : P.parRel >= 0.1 ? 'amber' : 'red', { k: 'v.rel', p: { x: P.parRel } },
    { k: P.parRel >= 0.5 ? 'w.par.ok' : 'w.par.low' }, 'black body 400-700 nm');
  add(3, 'thermostat', { active: 'green', partial: 'amber', absent: 'red' }[P.thermostat],
    { k: 'v.tect.' + P.tectonics }, { k: 'w.thermo.' + P.thermostat },
    'Walker, Hays & Kasting 1981; Valencia 2007 vs O\'Neill & Lenardic 2007', true);
  add(3, 'obliquity', P.obliquityStable ? 'green' : 'amber', { k: 'v.deg', p: { d: P.obliquityDeg } },
    { k: P.obliquityStable ? 'w.obl.moon' : 'w.obl.free' }, 'Laskar et al. 1993; Lissauer et al. 2012', !P.obliquityStable);
  const hs = P.hillSpacing;
  const need = D.nPlanets >= 3 ? HILL_MULTI : HILL_TWO_PLANET;
  add(3, 'stable', !isFinite(hs) ? 'green' : hs >= HILL_MULTI ? 'green' : hs >= HILL_TWO_PLANET ? 'amber' : 'red',
    { k: isFinite(hs) ? 'v.hill' : 'v.alone', p: { h: hs, n: P.hillNeighbour } },
    { k: !isFinite(hs) || hs >= HILL_MULTI ? 'w.stable.ok' : hs >= need ? 'w.stable.tight' : 'w.stable.no' },
    'Gladman 1993; Chambers et al. 1996');

  /* ---- Tier 4: humans without a suit ---- */
  add(4, 'gravity', band(P.gEarth, 0.8, 1.2, 0.5, 1.5), { k: 'v.g', p: { g: P.gEarth } },
    { k: P.gEarth < 0.8 ? 'w.g.low' : P.gEarth > 1.2 ? 'w.g.high' : 'w.g.ok' }, 'best guess (no long-term data)');
  add(4, 'pO2', band(P.pO2kPa, 16, 30, 11, 50), { k: 'v.kPa', p: { p: P.pO2kPa } },
    { k: P.pO2kPa < 16 ? 'w.o2.low' : P.pO2kPa > 30 ? 'w.o2.high' : 'w.o2.ok' }, 'NASA-STD-3001');
  add(4, 'fire', P.o2Frac < 0.25 ? 'green' : P.o2Frac <= 0.35 ? 'amber' : 'red', { k: 'v.pct', p: { x: P.o2Frac * 100 } },
    { k: P.o2Frac < 0.25 ? 'w.fire.ok' : 'w.fire.risk' }, 'fire-risk estimate');
  add(4, 'pCO2', P.pCO2kPa < 0.5 ? 'green' : P.pCO2kPa <= 3 ? 'amber' : 'red', { k: 'v.kPa', p: { p: P.pCO2kPa } },
    { k: P.pCO2kPa < 0.5 ? 'w.co2.ok' : P.pCO2kPa <= 3 ? 'w.co2.high' : 'w.co2.toxic' }, 'OSHA/NIOSH');
  add(4, 'pN2', P.pN2bar < 2 ? 'green' : P.pN2bar <= 3 ? 'amber' : 'red', { k: 'v.bar', p: { p: P.pN2bar } },
    { k: P.pN2bar < 2 ? 'w.n2.ok' : 'w.n2.narc' }, 'diving physiology');
  const pPa = P.pBar * 1e5;
  add(4, 'pressure', P.pBar >= 0.5 && P.pBar <= 2 ? 'green' : (P.pBar >= 0.3 && P.pBar <= 5) ? 'amber' : 'red',
    { k: 'v.bar', p: { p: P.pBar } },
    { k: pPa < ARMSTRONG_PA ? 'w.p.armstrong' : P.pBar < 0.5 ? 'w.p.low' : P.pBar > 2 ? 'w.p.high' : 'w.p.ok' },
    'Armstrong limit; NASA-STD-3001');
  add(4, 'temp', P.climate.runaway ? 'red' : band(P.TsC, 5, 25, -15, 35), tVal,
    { k: P.ebm ? (P.climate.valid ? 'w.t.ebm' : 'w.t.ebmOutside') : P.ebmPending ? 'w.t.pending' : 'w.t.outside' },
    P.ebm ? 'energy balance model' : 'climate estimate');
  if (P.climate.runaway) add(4, 'wetbulb', 'red', { k: 'v.runawayT' }, { k: 'w.wb.runaway' }, 'Sherwood & Huber 2010');
  else if (P.wetBulbMax == null) add(4, 'wetbulb', P.ebm ? 'green' : 'na', { k: P.ebm ? 'v.dry' : 'v.na' },
    { k: P.ebm ? 'w.wb.dry' : 'w.t.pending' }, 'Sherwood & Huber 2010; Vecellio et al. 2022');
  else add(4, 'wetbulb', P.wetBulbMax < 31 ? 'green' : P.wetBulbMax <= 35 ? 'amber' : 'red', { k: 'v.tempC', p: { t: P.wetBulbMax } },
    { k: P.wetBulbMax < 31 ? 'w.wb.ok' : P.wetBulbMax <= 35 ? 'w.wb.hard' : 'w.wb.deadly' }, 'Sherwood & Huber 2010; Vecellio et al. 2022');
  const dh = P.dayHours;
  /* Locked by the nominal tidal Q (spec §5.8). 'likely' (only the nominal Q locks it) is amber, because Q is
   * uncertain by orders of magnitude; 'locked' (even the weakest tides lock it) is red. */
  const lockSt = P.lock.state;
  add(4, 'day', lockSt === 'locked' ? 'red' : lockSt === 'likely' ? 'amber' : band(dh, 20, 28, 12, 72),
    { k: P.locked ? 'v.locked' : 'v.hours', p: { h: dh } },
    { k: lockSt === 'locked' ? 'w.day.locked' : lockSt === 'likely' ? 'w.day.likely'
      : dh >= 20 && dh <= 28 ? 'w.day.ok' : 'w.day.off' }, 'Gladman et al. 1996; circadian research', lockSt === 'likely');
  const dose = P.doseMSv;
  add(4, 'radiation', dose < 5 ? 'green' : dose <= 20 ? 'amber' : 'red', { k: 'v.dose', p: { d: dose } },
    { k: dose < 5 ? 'w.rad.ok' : dose <= 20 ? 'w.rad.work' : 'w.rad.high' }, 'ICRP 103; Hassler et al. 2014 (cosmic rays only)');
  add(4, 'uv', P.ozone ? 'green' : 'red', { k: P.ozone ? 'v.ozone' : 'v.noOzone' },
    { k: P.ozone ? 'w.uv.ok' : 'w.uv.none' }, 'WHO UV index');
  const tempOk = P.TsC >= -15 && P.TsC <= 35 && !P.climate.runaway;
  const hland = P.ebm ? P.habLand : tempOk ? P.landFrac : 0;
  add(4, 'land', hland >= 0.1 ? 'green' : hland >= 0.01 ? 'amber' : 'red', { k: 'v.pct', p: { x: hland * 100 } },
    { k: P.landFrac < 0.01 ? 'w.land.ocean' : !tempOk ? 'w.land.climate' : 'w.land.ok' }, 'terrain heuristic (M0)');
  const fresh = P.liquidWater && P.landFrac > 0.01 ? 'green' : P.liquidWater ? 'amber' : 'red';
  add(4, 'fresh', fresh, { k: fresh === 'green' ? 'v.rain' : 'v.none' },
    { k: fresh === 'green' ? 'w.fresh.ok' : 'w.fresh.no' }, 'hydrological cycle');

  /* ---- Tier 5: Earth twin ---- */
  const hasMoon = P.moons.length > 0;
  add(5, 'moon', hasMoon && P.obliquityStable ? 'green' : 'amber', { k: hasMoon ? 'v.moons' : 'v.noMoon', p: { n: P.moons.length } },
    { k: hasMoon ? 'w.moon.ok' : 'w.moon.none' }, 'Earth-Moon system');
  add(5, 'balance', P.landFrac >= 0.15 && P.landFrac <= 0.5 ? 'green' : 'amber', { k: 'v.pct', p: { x: P.landFrac * 100 } },
    { k: 'w.balance' }, 'Earth: 29 % land');

  /* Tiers: a tier passes when the one below passes and it has no red rows; tier 5 needs every row green. */
  const pass = {};
  let prev = true;
  for (const t of [1, 2, 3, 4]) {
    const reds = rows.filter((r) => r.tier === t && r.status === 'red');
    pass[t] = prev && reds.length === 0;
    prev = pass[t];
  }
  pass[5] = pass[4] && rows.every((r) => r.status === 'green' || r.status === 'na');
  const firstFail = {};
  for (const t of TIERS) {
    const r = rows.find((x) => x.tier <= t && (t === 5 ? x.status === 'amber' || x.status === 'red' : x.status === 'red'));
    firstFail[t] = r ? r.id : null;
  }
  return { rows, pass, firstFail };
}
