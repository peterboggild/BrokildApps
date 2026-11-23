/* The Causality Map (spec §7): parameters and derived quantities as nodes in columns from the star to humans,
 * edges for "this drives that". Nodes carry their current value and the colour of the ledger rows they feed.
 * Selecting a node lights everything downstream; a change sends a pulse along the edges it touched. Inline SVG,
 * drawn in an overlay over the 3D view. Labels sit inside their own boxes, never over the edges' ends. */
import { t, fmt, esc } from '../i18n.js?v=8';

/* [id, column, label key, value(P, D), ledger rows whose worst status colours the node] */
const NODES = [
  ['mstar', 0, 'cm.mstar', (P, D) => `${fmt(D.star.massSun, 2)} M☉`, []],
  ['age', 0, 'cm.age', (P, D) => `${fmt(D.star.ageGyr, 2)} ${t('u.Gyr')}`, []],
  ['lum', 1, 'cm.lum', (P, D) => `${fmt(D.star.lum, 2)} L☉`, []],
  ['teff', 1, 'cm.teff', (P, D) => `${fmt(D.star.teff, 3)} K`, ['par']],
  ['life', 1, 'cm.life', (P, D) => `${fmt(D.star.lifetime, 2)} ${t('u.Gyr')}`, ['time']],
  ['xuv', 1, 'cm.xuv', (P) => `${fmt(P.xuv.fluence, 2)} ×`, ['activity']],
  ['a', 2, 'cm.a', (P) => `${fmt(P.aAU, 3)} ${t('u.AU')}`, []],
  ['flux', 2, 'cm.flux', (P) => `${fmt(P.fluxMean, 3)} ×`, ['hz', 'ecc']],
  ['lock', 2, 'cm.lock', (P) => t('lock.' + P.lock.state), ['day']],
  ['chz', 2, 'cm.chz', (P) => `${fmt(P.chz.consSoFar, 2)} ${t('u.Gyr')}`, ['chz', 'time']],
  ['mp', 3, 'cm.mp', (P) => `${fmt(P.massEarth, 3)} M⊕`, []],
  ['radius', 3, 'cm.radius', (P) => `${fmt(P.radiusEarth, 3)} R⊕`, ['envelope']],
  ['g', 3, 'cm.g', (P) => `${fmt(P.gEarth, 3)} g`, ['gravity']],
  ['vesc', 3, 'cm.vesc', (P) => `${fmt(P.vesc / 1e3, 3)} km/s`, []],
  ['tect', 4, 'cm.tect', (P) => t('tect.' + P.tectonics), ['thermostat']],
  ['oceans', 4, 'cm.oceans', (P) => `${fmt(P.oceans, 2)} ×`, []],
  ['land', 4, 'cm.land', (P) => `${fmt(P.landFrac * 100, 2)} %`, ['balance']],
  ['moon', 4, 'cm.moon', (P) => (P.moons.length ? String(P.moons.length) : t('none')), ['moon', 'obliquity']],
  ['retain', 5, 'cm.retain', (P) => `${fmt(P.shoreRatio, 2)} ×`, ['retain']],
  ['press', 5, 'cm.press', (P) => `${fmt(P.pBar, 3)} bar`, ['triple', 'pressure']],
  ['co2', 5, 'cm.co2', (P) => `${fmt(P.pCO2kPa, 2)} kPa`, ['pCO2']],
  ['ozone', 5, 'cm.ozone', (P) => t(P.ozone ? 'yes' : 'no'), ['uv', 'shield']],
  ['green', 6, 'cm.green', (P) => `${fmt(P.climate.forcingCO2, 2)} W/m²`, ['runaway']],
  ['temp', 6, 'cm.temp', (P) => (P.climate.runaway ? t('v.runawayT') : `${fmt(P.TsC, 3)} °C`), ['temp']],
  ['thermo', 6, 'cm.thermo', (P) => t('cm.th.' + P.thermostat), ['thermostat']],
  ['ice', 6, 'cm.ice', (P) => (P.ebm ? `${fmt(P.ebm.iceFrac * 100, 2)} %` : '–'), []],
  ['water', 7, 'cm.water', (P) => t(P.liquidWater ? 'yes' : 'no'), ['liquid']],
  ['photo', 7, 'cm.photo', (P) => `${fmt(P.parRel, 2)} ×`, ['par']],
  ['dose', 7, 'cm.dose', (P) => `${fmt(P.doseMSv, 2)} mSv`, ['radiation']],
  ['lifeN', 8, 'cm.lifeN', () => '', ['liquid', 'chz', 'retain', 'shield', 'time', 'thermostat', 'par']],
  ['humans', 8, 'cm.humans', () => '', ['gravity', 'pO2', 'pCO2', 'pN2', 'pressure', 'temp', 'wetbulb', 'day', 'radiation', 'uv', 'land']],
];

const EDGES = [
  ['mstar', 'lum'], ['mstar', 'teff'], ['mstar', 'life'], ['mstar', 'xuv'], ['mstar', 'lock'],
  ['age', 'lum'], ['age', 'xuv'], ['age', 'chz'],
  ['lum', 'flux'], ['a', 'flux'], ['a', 'lock'], ['a', 'xuv'], ['lum', 'chz'], ['teff', 'photo'], ['life', 'chz'],
  ['mp', 'radius'], ['radius', 'g'], ['radius', 'vesc'], ['mp', 'g'], ['mp', 'vesc'], ['mp', 'tect'],
  ['oceans', 'land'], ['oceans', 'tect'], ['oceans', 'thermo'], ['land', 'thermo'], ['tect', 'thermo'],
  ['vesc', 'retain'], ['xuv', 'retain'], ['retain', 'press'], ['g', 'press'],
  ['press', 'green'], ['co2', 'green'], ['green', 'temp'], ['flux', 'temp'], ['thermo', 'co2'], ['temp', 'ice'], ['ice', 'temp'],
  ['flux', 'green'], ['temp', 'water'], ['press', 'water'], ['oceans', 'water'], ['flux', 'photo'],
  ['press', 'dose'], ['xuv', 'dose'], ['ozone', 'dose'], ['lock', 'temp'], ['moon', 'temp'],
  ['water', 'lifeN'], ['chz', 'lifeN'], ['photo', 'lifeN'], ['ozone', 'lifeN'], ['thermo', 'lifeN'],
  ['lifeN', 'humans'], ['g', 'humans'], ['press', 'humans'], ['co2', 'humans'], ['temp', 'humans'], ['dose', 'humans'],
  ['lock', 'humans'], ['land', 'humans'],
];
export const COLUMNS = ['cm.c0', 'cm.c1', 'cm.c2', 'cm.c3', 'cm.c4', 'cm.c5', 'cm.c6', 'cm.c7', 'cm.c8'];

/* the map node that a ledger row is best explained by (for the cards' "show in the map") */
export function nodeForRow(rowId) {
  const n = NODES.find((x) => x[4][0] === rowId) || NODES.find((x) => x[4].includes(rowId));
  return n ? n[0] : null;
}

const RANK = { green: 0, na: 0, amber: 1, red: 2 };
function downstream(id) {
  const out = new Set([id]), q = [id];
  while (q.length) { const a = q.shift(); for (const [s, d] of EDGES) if (s === a && !out.has(d)) { out.add(d); q.push(d); } }
  return out;
}

export function createCausality(container, S) {
  const state = { selected: null, prev: {} };
  const W = 1180, colW = W / 9, boxW = 112, boxH = 38;

  function render() {
    const P = S.derivedPlanet();
    if (!P || P.role !== 'hz') { container.innerHTML = `<p class="note">${esc(t('ledger.none'))}</p>`; return; }
    const L = S.ledgerFor(P);
    const status = {};
    for (const r of L.rows) status[r.id] = r.status;
    /* layout: per column, spread nodes vertically */
    const pos = {};
    const byCol = {};
    for (const n of NODES) (byCol[n[1]] = byCol[n[1]] || []).push(n);
    let H = 0;
    for (const c in byCol) H = Math.max(H, byCol[c].length);
    const rowH = 64, top = 46, height = top + H * rowH + 20;
    for (const c in byCol) {
      const list = byCol[c], off = (H - list.length) * rowH / 2;
      list.forEach((n, i) => { pos[n[0]] = { x: colW * c + (colW - boxW) / 2, y: top + off + i * rowH }; });
    }
    /* which values changed since last time: they send a pulse downstream */
    const vals = {};
    for (const n of NODES) { try { vals[n[0]] = n[3](P, S.derived); } catch (e) { vals[n[0]] = '–'; } }
    const changed = new Set(Object.keys(vals).filter((k) => state.prev[k] !== undefined && state.prev[k] !== vals[k]));
    state.prev = vals;
    const pulsing = new Set();
    for (const c of changed) for (const d of downstream(c)) pulsing.add(d);
    const sel = state.selected ? downstream(state.selected) : null;

    let g = `<svg class="cmap" viewBox="0 0 ${W} ${height}" role="img" aria-label="${esc(t('cm.title'))}">`;
    COLUMNS.forEach((k, c) => { g += `<text class="cm-col" x="${colW * c + colW / 2}" y="22" text-anchor="middle">${esc(t(k))}</text>`; });
    for (const [s, d] of EDGES) {
      const a = pos[s], b = pos[d];
      if (!a || !b) continue;
      const x1 = a.x + boxW, y1 = a.y + boxH / 2, x2 = b.x, y2 = b.y + boxH / 2;
      const back = x2 <= x1;
      const path = back
        ? `M${a.x + boxW / 2},${a.y + boxH} C${a.x + boxW / 2},${a.y + boxH + 30} ${b.x + boxW / 2},${b.y + boxH + 30} ${b.x + boxW / 2},${b.y + boxH}`
        : `M${x1},${y1} C${x1 + 40},${y1} ${x2 - 40},${y2} ${x2},${y2}`;
      const on = sel ? sel.has(s) && sel.has(d) : false;
      const pulse = changed.has(s) || (pulsing.has(s) && pulsing.has(d));
      g += `<path class="cm-edge${on ? ' on' : ''}${sel && !on ? ' dim' : ''}${pulse ? ' pulse' : ''}" d="${path}" style="animation-delay:${(NODES.find((n) => n[0] === s)[1]) * 0.12}s"/>`;
    }
    for (const n of NODES) {
      const [id, , key, , rows] = n;
      const p = pos[id];
      let st = 'none';
      for (const r of rows) if (status[r] && (st === 'none' || RANK[status[r]] > RANK[st])) st = status[r];
      const dim = sel && !sel.has(id);
      g += `<g class="cm-node ${st}${dim ? ' dim' : ''}${state.selected === id ? ' sel' : ''}${changed.has(id) ? ' changed' : ''}" data-id="${id}" transform="translate(${p.x},${p.y})">`;
      g += `<rect width="${boxW}" height="${boxH}" rx="7"/><text class="cm-l" x="8" y="15">${esc(t(key))}</text>`;
      g += `<text class="cm-v" x="8" y="30">${esc(vals[id])}</text></g>`;
    }
    g += '</svg>';
    container.innerHTML = `<div class="cm-head"><strong>${esc(t('cm.title'))}</strong> · ${esc(P.name)}<span class="cm-hint">${esc(t('cm.hint'))}</span></div>` + g;
    container.querySelectorAll('.cm-node').forEach((e) => {
      e.addEventListener('click', () => { const id = e.dataset.id; state.selected = state.selected === id ? null : id; render(); });
    });
  }
  return { render, select(id) { state.selected = id; render(); }, state };
}
