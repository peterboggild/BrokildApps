/* Benchmarks of spec §5.12 (the EBM rows wait for milestone M3). Run: node test/physics.test.mjs
 * Node resolves the '?v=N' cache-busting suffixes as part of a file URL, so the browser modules run unchanged. */
import { starAt, mainSequence, blackbodyRGB, parRelativeToSun } from '../physics/star.js?v=9';
import { habitableZone } from '../physics/hz.js?v=9';
import { bulk } from '../physics/structure.js?v=9';
import { mutualHillSpacing, moonInfo, tidalLockBand, periodYears } from '../physics/orbit.js?v=9';
import { jeansLambda, scaleHeight, climate0D, boilingPointK } from '../physics/atmosphere.js?v=9';
import { MOLAR } from '../physics/const.js?v=9';
import { derive, deriveSync } from '../physics/derive.js?v=9';
import { runEBM } from '../physics/climate.js?v=9';
import { xuvEnvironment, cosmicDose, shorelineI } from '../physics/space.js?v=9';
import { ledger } from '../physics/ledger.js?v=9';
import { PRESETS, validate } from '../design.js?v=9';

let fails = 0, n = 0;
function near(name, got, want, tol) {
  n++;
  const ok = Math.abs(got - want) <= tol;
  if (!ok) fails++;
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}: ${+got.toPrecision(5)} (want ${want} ± ${tol})`);
}
function truth(name, cond, info = '') {
  n++; if (!cond) fails++;
  console.log(`${cond ? 'ok  ' : 'FAIL'} ${name}${info ? ': ' + info : ''}`);
}

/* The Sun at 4.57 Gyr */
const sun = starAt(1, 4.57);
near('Sun L [Lsun]', sun.lum, 1, 0.03);
near('Sun Teff [K]', sun.teff, 5772, 30);
near('Sun R [Rsun]', sun.radius, 1, 0.03);
truth('Sun class', sun.spt.startsWith('G2'), sun.spt);
near('Faint young Sun, L at 0.1 Gyr', starAt(1, 0.1).lum, 0.72, 0.03);

/* Sun's habitable zone */
const hz = habitableZone(sun.teff, sun.lum, 1);
near('HZ recent Venus [AU]', hz.d.recentVenus, 0.75, 0.02);
near('HZ runaway [AU]', hz.d.runaway, 0.97, 0.03);
near('HZ max greenhouse [AU]', hz.d.maxGreenhouse, 1.67, 0.03);
near('HZ early Mars [AU]', hz.d.earlyMars, 1.77, 0.03);

/* Earth and Mars */
const earth = bulk(1, 0.325, 0.0002);
near('Earth g [m/s2]', earth.g, 9.81, 0.08);
near('Earth v_esc [km/s]', earth.vesc / 1e3, 11.19, 0.08);
near('Earth radius [Re]', earth.radiusEarth, 1, 0.01);
const mars = bulk(0.107, 0.24, 0);
near('Mars g [m/s2]', mars.g, 3.71, 0.08);
near('Mars v_esc [km/s]', mars.vesc / 1e3, 5.03, 0.08);
const cl = climate0D({ S: 1, albedo: 0.3, pCO2bar: 0.00042, pCH4bar: 1.9e-6, totalBar: 1 });
near('Earth T_eq (A = 0.3) [K]', cl.Teq, 255, 2);
near('Earth scale height [km]', scaleHeight(288, MOLAR.N2 * 0.78 + MOLAR.O2 * 0.21 + MOLAR.Ar * 0.01, 9.81) / 1e3, 8.5, 0.2);
near('Boiling point at 1 atm [K]', boilingPointK(101325), 373.15, 0.1);

/* Venus-like flux */
truth('Venus-like S = 1.9: runaway flag', climate0D({ S: 1.9, albedo: 0.3, pCO2bar: 0.0004, pCH4bar: 0, totalBar: 1 }).runaway);

/* Earth-Moon Roche limit */
const mi = moonInfo({ massMoon: 1, aEarthRadii: 60.3 }, { massEarth: 1, radiusEarth: 1 }, { massSun: 1, radius: 1 }, 1, 0.0167);
near('Earth-Moon fluid Roche limit [Re]', mi.rocheEarthRadii, 2.9, 0.1);
near('Lunar tide relative to itself', mi.tideRel, 1, 0.01);
truth('The Moon stabilises the obliquity', mi.stabilises);
truth('Total solar eclipses possible from Earth', mi.totalEclipse);

/* Two Earths at 1.0 and 1.4 AU */
near('Mutual Hill spacing 1.0/1.4 AU [R_H]', mutualHillSpacing({ aAU: 1, massEarth: 1, e: 0 }, { aAU: 1.4, massEarth: 1, e: 0 }, 1), 25, 2.5);

/* Jeans parameter */
near('Jeans lambda N2, Earth, 1000 K', jeansLambda(11186, MOLAR.N2, 1000), 200, 15);
near('Jeans lambda H, Earth, 1000 K', jeansLambda(11186, MOLAR.H, 1000), 7, 1);

/* A 0.3 Msun star */
const rd = starAt(0.3, 5);
const rdhz = habitableZone(rd.teff, rd.lum, 1);
const mid = Math.sqrt(rdhz.d.runaway * rdhz.d.maxGreenhouse);
near('0.3 Msun HZ inner edge (runaway) [AU]', rdhz.d.runaway, 0.1, 0.03);   /* spec: "HZ ~0.1 AU" */
const lock = tidalLockBand(mid, 0.3, 1, 1, 5);
truth('0.3 Msun HZ planet tidally locked', lock.state === 'locked', `${lock.state}, t_lock(Q=100) ${lock.mid.toExponential(1)} Gyr`);
near('Earth period [yr]', periodYears(1, 1), 1, 1e-9);

/* Energy balance model (§5.12 EBM rows) */
{
  const base = { S: 1, e: 0.0167, obliquityDeg: 23.44, argPeriDeg: 283, dayHours: 24, locked: false, pBar: 1, molar: 0.029,
    forcing: 5.35 * Math.log(420 / 330), greenhouse: 1, albedoFree: 0.31, albedoIce: 0.62, landFrac: 0.29, hasWater: true, years: 30 };
  const r = runEBM({ ...base, init: 'warm' });
  near('EBM Earth global mean [C]', r.gmean, 15, 2);
  near('EBM Earth ice line [deg]', r.iceLatN, 70, 8);
  near('EBM Earth equator annual mean [C]', r.mean[18], 27, 3);
  const w = runEBM({ ...base, S: 0.9, init: 'warm' }), c = runEBM({ ...base, S: 0.9, init: 'cold' });
  truth('EBM S x 0.9 from a warm start: no snowball', !w.snowball, `mean ${w.gmean.toFixed(1)} C, ice to ${w.iceLatN.toFixed(0)} deg`);
  truth('EBM S x 0.9 from a frozen start: stays frozen (hysteresis)', c.snowball, `mean ${c.gmean.toFixed(1)} C`);
  const lk = runEBM({ ...base, S: 0.8, locked: true, obliquityDeg: 0, init: 'warm' });
  truth('Locked planet: hot day side, cold night side', lk.mean[35] > lk.mean[0] + 40, `${lk.mean[35].toFixed(0)} / ${lk.mean[0].toFixed(0)} C`);
}
/* XUV, shoreline, radiation */
{
  const x = xuvEnvironment(1, 4.57, 1, 1);
  near('XUV Earth today = 1', x.flux, 1, 1e-6);
  near('XUV fluence Earth = 1', x.fluence, 1, 1e-6);
  truth('Earth well below the cosmic shoreline', 1 / shorelineI(11.19) < 0.2);
  near('Dose at Earth sea level [mSv/yr]', cosmicDose(1033, 'low'), 0.35, 0.01);
  near('Dose on Mars surface [mSv/day]', cosmicDose(16.4, 'low') / 365, 0.64, 0.05);
  const rd = xuvEnvironment(0.3, 5, 0.1, starAt(0.3, 5).lum);
  truth('Red-dwarf HZ planet: much more XUV than Earth', rd.fluence > 5, `fluence ${rd.fluence.toFixed(1)} x Earth`);
}

/* The whole chain on presets */
for (const k of Object.keys(PRESETS)) {
  const d = validate(PRESETS[k]());
  const D = deriveSync(d);
  const P = D.planets.find((p) => p.role === 'hz');
  const L = ledger(P, D);
  const tiers = [1, 2, 3, 4, 5].map((t) => (L.pass[t] ? t : '-')).join('');
  console.log(`   preset ${k}: T ${P.TsC.toFixed(1)} C, S ${P.fluxMean.toFixed(2)}, R ${P.radiusEarth.toFixed(2)}, tiers ${tiers}, ` +
    `first fails ${JSON.stringify(L.firstFail)}`);
  if (k === 'solar') { truth('Solar preset: Earth reaches tier 4', L.pass[4]); near('Earth climate estimate [C]', P.TsC, 15, 3); }
  if (k === 'hothouse') truth('Hothouse preset: runaway, tier 1 fails', P.climate.runaway && !L.pass[1]);
  if (k === 'dryworld') truth('Dry-world preset: fails tier 1', !L.pass[1]);
  if (k === 'reddwarf') truth('Red-dwarf preset: locked, flare warning', P.locked && L.rows.find((r) => r.id === 'activity').status === 'amber');
}

/* Interior layers (M2 done-when: Earth and Mars) */
{
  const E = derive(validate(PRESETS.solar())).planets[0], M = derive(validate(PRESETS.dryworld())).planets[0];
  near('Earth core radius [km] (true 3480)', E.layers.coreKm, 3480, 250);
  near('Earth inner core [km] (true 1220)', E.layers.innerCoreKm, 1220, 120);
  near('Dry world (Mars numbers) core radius [km] (InSight 1830)', M.layers.coreKm, 1830, 200);
  truth('Dry world (Mars mass) has no solid inner core', M.layers.innerCoreKm === 0);
  near('Earth land fraction from the terrain', E.landFrac, 0.29, 0.03);
  near('Earth mean ocean depth [km] (true 3.7)', E.oceanDepthKm, 3.7, 0.4);
  truth('Dry world: all land', M.landFrac === 1);
}

/* M4 done-when: every ledger row has a card (both languages, all parts) and a source */
{
  const { CARDS } = await import('../ui/cards.js?v=9');
  const Dd = deriveSync(validate(PRESETS.solar()));
  const Lg = ledger(Dd.planets[0], Dd);
  const bad = Lg.rows.filter((r) => !r.src || !CARDS[r.id] || ['da', 'en'].some((l) => !CARDS[r.id][l] || ['what', 'why', 'ours', 'try', 'more'].some((f) => !CARDS[r.id][l][f])));
  truth('Every ledger row has a card in both languages and a source', bad.length === 0, `${Lg.rows.length} rows${bad.length ? ', missing: ' + bad.map((r) => r.id).join(' ') : ''}`);
  const { esi } = await import('../physics/esi.js?v=9');
  near('ESI of the Earth-like start', esi(Dd.planets[0]), 1, 0.02);
  near('ESI of the dry world (Mars numbers; Mars 0.64)', esi(deriveSync(validate(PRESETS.dryworld())).planets[0]), 0.64, 0.05);
}

/* Hard limits */
const bad = validate({ star: { massSun: 9 }, planets: [{ role: 'hz', orbit: { e: 0.9 } }, { role: 'hz' }, { role: 'hz' }] });
truth('Eccentricity capped at 0.4', bad.planets[0].orbit.e === 0.4);
truth('At most two HZ planets', bad.planets.filter((p) => p.role === 'hz').length === 2);
truth('Black-body colour of the Sun is near white', blackbodyRGB(5772).every((c) => c > 0.8), blackbodyRGB(5772).map((c) => c.toFixed(2)).join(' '));
/* Black-body sanity check, not a literature value: an M5 dwarf gives roughly a quarter to a third of the Sun's PAR photons per watt. */
near('PAR photons per watt, M5 dwarf vs Sun', parRelativeToSun(3060).photonsPerW, 0.28, 0.1);
truth('Table covers 0.08 Msun', !mainSequence(0.08).outsideTable);

console.log(`\n${n - fails}/${n} passed`);
process.exit(fails ? 1 : 0);
