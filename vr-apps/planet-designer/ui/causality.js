/* The Causality Map (spec §7): parameters and derived quantities as nodes in columns from the star to humans,
 * edges for "this drives that". Nodes carry their current value and the colour of the ledger rows they feed.
 * Selecting a node lights everything downstream; a change sends a pulse along the edges it touched. Inline SVG,
 * drawn in an overlay over the 3D view. Labels sit inside their own boxes, never over the edges' ends.
 * Edges end in arrowheads; a selected box shows what it drives (blue) and what drives it (orange). The "i" on each
 * box opens a small card: what the quantity is, how to read its value, and its direct causes and effects. */
import { t, fmt, esc, getLang } from '../i18n.js?v=16';
import { CM_INFO } from './cminfo.js?v=16';
import { limitFor } from '../physics/derive.js?v=16';

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
function upstream(id) {
  const out = new Set([id]), q = [id];
  while (q.length) { const a = q.shift(); for (const [s, d] of EDGES) if (d === a && !out.has(s)) { out.add(s); q.push(s); } }
  return out;
}
function downstream(id) {
  const out = new Set([id]), q = [id];
  while (q.length) { const a = q.shift(); for (const [s, d] of EDGES) if (s === a && !out.has(d)) { out.add(d); q.push(d); } }
  return out;
}

export function createCausality(container, S) {
  const state = { selected: null, prev: {}, info: null, infoAt: null };
  const pop = document.createElement('div');
  pop.className = 'cm-pop'; pop.hidden = true;
  (container.parentElement || container).appendChild(pop);
  const label = (id) => { const n = NODES.find((x) => x[0] === id); return n ? t(n[2]) : id; };
  function renderPop(vals) {
    if (!state.info) { pop.hidden = true; return; }
    const id = state.info, n = NODES.find((x) => x[0] === id);
    const info = CM_INFO[id], text = info ? info[getLang() === 'en' ? 1 : 0] : '';
    const from = EDGES.filter(([, d]) => d === id).map(([s2]) => label(s2));
    const to = EDGES.filter(([s2]) => s2 === id).map(([, d]) => label(d));
    const v = vals && vals[id] ? ` <span class="cm-pop-v">${esc(vals[id])}</span>` : '';
    pop.innerHTML = `<button class="cm-pop-x" aria-label="${esc(t('close'))}">×</button><strong>${esc(t(n[2]))}</strong>${v}`
      + `<p>${esc(text)}</p>`
      + (from.length ? `<p class="cm-pop-l"><span class="up">← ${esc(t('cm.from'))}</span> ${esc(from.join(', '))}</p>` : '')
      + (to.length ? `<p class="cm-pop-l"><span class="dn">→ ${esc(t('cm.to'))}</span> ${esc(to.join(', '))}</p>` : '');
    pop.hidden = false;
    const r = pop.getBoundingClientRect(), [cx, cy] = state.infoAt || [innerWidth / 2, innerHeight / 2];
    pop.style.left = Math.max(8, Math.min(innerWidth - r.width - 8, cx + 12)) + 'px';
    pop.style.top = Math.max(8, Math.min(innerHeight - r.height - 8, cy + 12)) + 'px';
    pop.querySelector('.cm-pop-x').onclick = () => { state.info = null; renderPop(); };
  }
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
    const up = state.selected ? upstream(state.selected) : null;

    let g = `<svg class="cmap" viewBox="0 0 ${W} ${height}" role="img" aria-label="${esc(t('cm.title'))}">`
      + '<defs>' + [['a', '#3e5068'], ['on', '#6fb7ff'], ['up', '#ffb36b'], ['pulse', '#8ff0b0']].map(([k, c]) =>
        `<marker id="cmh-${k}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,1 L10,5 L0,9 z" fill="${c}"/></marker>`).join('') + '</defs>';
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
      const isUp = up ? up.has(s) && up.has(d) && !on : false;
      const pulse = changed.has(s) || (pulsing.has(s) && pulsing.has(d));
      const head = on ? 'on' : isUp ? 'up' : pulse ? 'pulse' : 'a';
      g += `<path class="cm-edge${on ? ' on' : ''}${isUp ? ' up' : ''}${sel && !on && !isUp ? ' dim' : ''}${pulse ? ' pulse' : ''}" d="${path}" marker-end="url(#cmh-${head})" style="animation-delay:${(NODES.find((n) => n[0] === s)[1]) * 0.12}s"/>`;
    }
    for (const n of NODES) {
      const [id, , key, , rows] = n;
      const p = pos[id];
      let st = 'none';
      for (const r of rows) if (status[r] && (st === 'none' || RANK[status[r]] > RANK[st])) st = status[r];
      const dim = sel && !sel.has(id) && !up.has(id);
      const lim = (id === 'temp' || id === 'green' || id === 'ice' ? ['temp'] : rows).some((r) => limitFor(P, r));
      g += `<g class="cm-node ${st}${dim ? ' dim' : ''}${up && up.has(id) && !sel.has(id) ? ' upn' : ''}${state.selected === id ? ' sel' : ''}${changed.has(id) ? ' changed' : ''}${lim ? ' lim' : ''}" data-id="${id}" transform="translate(${p.x},${p.y})">`;
      g += `<rect width="${boxW}" height="${boxH}" rx="7"/><text class="cm-l" x="8" y="15"${t(key).length * 6.1 > boxW - 30 ? ` textLength="${boxW - 30}" lengthAdjust="spacingAndGlyphs"` : ''}>${esc(t(key))}</text>`;
      g += `<text class="cm-v" x="8" y="30">${esc(vals[id])}</text>`;
      g += `<g class="cm-info" data-info="${id}" role="button" aria-label="info"><circle cx="${boxW - 11}" cy="11" r="8"/><text x="${boxW - 11}" y="15" text-anchor="middle">i</text></g></g>`;
    }
    g += '</svg>';
    const legend = `<span class="cm-legend"><span class="dn">→ ${esc(t('cm.legDn'))}</span> <span class="up">→ ${esc(t('cm.legUp'))}</span> <span class="ii">i</span> ${esc(t('cm.legInfo'))}</span>`;
    container.innerHTML = `<div class="cm-head"><strong>${esc(t('cm.title'))}</strong> · ${esc(P.name)}<span class="cm-hint">${esc(t('cm.hint'))}</span>${legend}</div>` + g;
    container.querySelectorAll('.cm-node').forEach((e) => {
      e.addEventListener('click', (ev) => {
        const id = e.dataset.id;
        if (ev.target.closest('.cm-info')) { state.info = state.info === id ? null : id; state.infoAt = [ev.clientX, ev.clientY]; state.selected = id; }
        else state.selected = state.selected === id ? null : id;
        render();
      });
    });
    renderPop(vals);
  }
  return { render, select(id) { state.selected = id; render(); }, info(id, x, y) { state.info = id; state.infoAt = [x, y]; render(); }, hide() { state.info = null; renderPop(); }, state };
}
