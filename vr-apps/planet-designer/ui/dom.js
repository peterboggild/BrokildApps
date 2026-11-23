/* Desktop renderer of the UI schema (ui/schema.js). The state object owns the design; this file only draws it and
 * reports edits through state.set(path, value) and state.structural(fn). */
import { TABS, LEVELS } from './schema.js?v=16';
import { t, fmt, esc, getLang } from '../i18n.js?v=16';
import { CARDS, CARD_LABELS } from './cards.js?v=16';
import { nodeForRow } from './causality.js?v=16';
import { esi } from '../physics/esi.js?v=16';
import { limitFor } from '../physics/derive.js?v=16';
import { MAX_HZ, MAX_PLANETS } from '../design.js?v=16';

const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html !== undefined) e.innerHTML = html; return e; };

/* Slider position x in [0, 1] <-> value. */
const GAS_FLOOR = 1e-7;
function toValue(row, x) {
  if (row.log) return row.min * (row.max / row.min) ** x;
  if (row.gas) return x <= 0.002 ? 0 : GAS_FLOOR * (row.max / GAS_FLOOR) ** ((x - 0.002) / 0.998);
  if (row.sqrt) return row.min + (row.max - row.min) * x * x;
  const v = row.min + (row.max - row.min) * x;
  return row.step ? Math.round(v / row.step) * row.step : v;
}
function toPos(row, v) {
  if (row.log) return Math.log(Math.max(v, row.min) / row.min) / Math.log(row.max / row.min);
  if (row.gas) return v <= 0 ? 0 : 0.002 + 0.998 * Math.log(Math.max(v, GAS_FLOOR) / GAS_FLOOR) / Math.log(row.max / GAS_FLOOR);
  if (row.sqrt) return Math.sqrt((v - row.min) / (row.max - row.min));
  return (v - row.min) / (row.max - row.min);
}
const clamp01 = (x) => Math.max(0, Math.min(1, x));

/* Repeat a callback while a button is held (the - and + buttons). */
function autoRepeat(btn, fn) {
  let timer = null;
  const stop = () => { clearTimeout(timer); timer = null; };
  btn.addEventListener('pointerdown', (e) => {
    e.preventDefault(); fn();
    let delay = 380;
    const go = () => { fn(); delay = Math.max(40, delay * 0.8); timer = setTimeout(go, delay); };
    timer = setTimeout(go, delay);
  });
  ['pointerup', 'pointerleave', 'pointercancel'].forEach((ev) => btn.addEventListener(ev, stop));
}

/* A second press confirms a destructive action (owner rule). */
function confirmButton(label, onConfirm) {
  const b = el('button', 'btn danger', label);
  let armed = 0;
  b.onclick = () => {
    if (armed && Date.now() - armed < 3000) { armed = 0; onConfirm(); return; }
    armed = Date.now(); b.textContent = t('btn.confirm');
    setTimeout(() => { if (armed) { armed = 0; b.textContent = label; } }, 3000);
  };
  return b;
}

export function renderPanel(root, S) {
  const level = LEVELS[S.design.level] || 1;
  const P = S.planet();
  const isHz = P.role === 'hz';
  root.innerHTML = '';
  S.live = [];

  const tabs = el('div', 'tabs');
  for (const tab of TABS) {
    const b = el('button', 'tab' + (S.tab === tab.id ? ' on' : ''), t('tab.' + tab.id));
    b.onclick = () => { S.tab = tab.id; S.redraw(); };
    tabs.appendChild(b);
  }
  root.appendChild(tabs);

  const tab = TABS.find((x) => x.id === S.tab) || TABS[0];
  const body = el('div', 'tabbody');
  root.appendChild(body);
  for (const row of tab.rows) {
    if ((row.level || 1) > level) continue;
    if (row.hz && !isHz) continue;
    if (row.other && isHz) continue;
    if (tab.hzOnly && !isHz && row.kind !== 'planets') {
      if (row === tab.rows[1]) body.appendChild(el('p', 'note', t('ledger.none')));
      continue;
    }
    if (row.kind === 'slider') body.appendChild(slider(row, S));
    else if (row.kind === 'chips') body.appendChild(chips(row, S));
    else if (row.kind === 'readout') body.appendChild(readout(row, S));
    else if (row.kind === 'warn') body.appendChild(warn(row, S));
    else if (row.kind === 'planets') body.appendChild(planetPicker(S));
    else if (row.kind === 'moons') moons(body, S, level);
    else if (row.kind === 'system') system(body, S);
    else if (row.kind === 'heading') body.appendChild(el('h4', 'subhead', t(row.label)));
    else if (row.kind === 'chart') {
      const c = el('div', 'chartbox');
      const upd = () => { c.innerHTML = row.value(S.derived, S.derivedPlanet(), S.design); };
      upd(); S.live.push(upd); body.appendChild(c);
    }
    else if (row.kind === 'action') {
      const w = el('div', 'row picker');
      const b = el('button', 'btn', t(row.label));
      b.onclick = () => row.act(S);
      w.append(b); body.appendChild(w);
    }
    else if (row.kind === 'seed') {
      const w = el('div', 'row picker');
      const b = el('button', 'btn', '🎲 ' + t('btn.terrain'));
      b.onclick = () => S.structural(() => { S.planet().surface.terrainSeed = 1 + Math.floor(Math.random() * 99998); });
      w.append(b);
      if (level >= 3) w.append(el('span', 'note', `${t('r.seed')} ${P.surface ? P.surface.terrainSeed : ''}`));
      body.appendChild(w);
    }
  }
}

function getPath(S, path) {
  const [head, ...rest] = path.split('.');
  let o = head === 'star' ? S.design.star : head === 'planet' ? S.planet() : S.design;
  if (head !== 'star' && head !== 'planet') rest.unshift(head);
  for (const k of rest) o = o == null ? o : o[k];
  return o;
}

function fmtVal(row, v) {
  if (row.pct) return `${fmt(v * 100, row.digits || 2)} %`;
  const unit = row.unit ? ' ' + t('u.' + row.unit) : '';
  return fmt(v, row.digits || 2) + unit;
}

function slider(row, S, path = row.path, labelKey = 'p.' + row.path.replace('planet.', '')) {
  const wrap = el('div', 'row slider');
  const head = el('div', 'rowhead');
  head.appendChild(el('label', '', t(labelKey)));
  const val = el('input', 'val');
  val.type = 'text'; val.inputMode = 'decimal';
  head.appendChild(val);
  wrap.appendChild(head);
  const line = el('div', 'sline');
  const minus = el('button', 'pm', '−'), plus = el('button', 'pm', '+');
  const range = el('input');
  range.type = 'range'; range.min = 0; range.max = 1000; range.step = 1;
  line.append(minus, range, plus);
  wrap.appendChild(line);
  const get = () => getPath(S, path);
  const show = () => { const v = get(); range.value = Math.round(clamp01(toPos(row, v)) * 1000); if (document.activeElement !== val) val.value = fmtVal(row, v); };
  show();
  const setV = (v) => { S.set(path, Math.max(row.min, Math.min(row.max, v))); show(); };
  range.oninput = () => setV(toValue(row, range.value / 1000));
  const nudge = (dir) => {
    if (row.log) { setV(get() * (1 + dir * 0.005)); return; }      /* log sliders: 0.5 % per press */
    const pos = clamp01(toPos(row, get()) + dir * 0.004);
    let v = toValue(row, pos);
    if (v === get()) v = toValue(row, clamp01(pos + dir * 0.004));
    setV(v);
  };
  autoRepeat(minus, () => nudge(-1));
  autoRepeat(plus, () => nudge(1));
  val.onchange = () => {
    const n = parseFloat(val.value.replace(',', '.'));
    if (isFinite(n)) setV(row.pct ? n / 100 : n);
    val.value = fmtVal(row, get());
  };
  S.live.push(show);
  return wrap;
}

function chips(row, S) {
  const wrap = el('div', 'row chips');
  wrap.appendChild(el('label', '', t(row.label === 'act' ? 'p.star.activity' : row.label)));
  const box = el('div', 'chipbox');
  const cur = getPath(S, row.path);
  for (const o of row.options) {
    const b = el('button', 'chip' + (o === cur ? ' on' : ''), t(row.label + '.' + o));
    b.onclick = () => { S.set(row.path, o); S.redraw(); };
    box.appendChild(b);
  }
  wrap.appendChild(box);
  return wrap;
}

function readout(row, S) {
  const wrap = el('div', 'row readout');
  wrap.appendChild(el('span', 'rl', t(row.label)));
  const v = el('span', 'rv');
  wrap.appendChild(v);
  const upd = () => { try { v.innerHTML = row.value(S.derived, S.derivedPlanet(), S.design); } catch (e) { v.textContent = '–'; } };
  upd(); S.live.push(upd);
  return wrap;
}

function warn(row, S) {
  const w = el('div', 'warn');
  const upd = () => { const k = row.value(S.derived, S.derivedPlanet(), S.design); w.textContent = k ? t(k) : ''; w.hidden = !k; };
  upd(); S.live.push(upd);
  return w;
}

function planetPicker(S) {
  const wrap = el('div', 'row picker');
  const sel = el('select');
  S.design.planets.forEach((p, i) => {
    const o = el('option', '', `${esc(p.name)} (${t('role.' + p.role)}${p.role === 'other' ? ', ' + t('type.' + p.type) : ''})`);
    o.value = i; if (i === S.sel) o.selected = true; sel.appendChild(o);
  });
  sel.onchange = () => { S.sel = +sel.value; S.redraw(); };
  wrap.append(el('label', '', t('planet.pick')), sel);
  return wrap;
}

function moons(body, S, level) {
  const P = S.planet();
  P.moons.forEach((m, i) => {
    const box = el('div', 'moon');
    const h = el('div', 'moonhead');
    h.appendChild(el('strong', '', t('moon.n', { n: String(i + 1) })));
    h.appendChild(confirmButton(t('moon.remove'), () => S.structural(() => S.planet().moons.splice(i, 1))));
    box.appendChild(h);
    box.appendChild(slider({ min: 0.081, max: 8.13, log: true, unit: 'Moon', digits: 3 }, S, `planet.moons.${i}.massMoon`, 'p.moon.massMoon'));
    box.appendChild(slider({ min: 1.5, max: 1000, log: true, unit: '', digits: 3 }, S, `planet.moons.${i}.aEarthRadii`, 'p.moon.aEarthRadii'));
    const info = (label, fn, lv = 1) => { if (lv <= level) box.appendChild(readout({ label, value: (D, DP) => fn(DP.moons[i]) }, S)); };
    info('r.roche', (mi) => `${fmt(mi.rocheEarthRadii, 3)} ${t('u.Re')}${mi.insideRoche ? ' ⚠' : ''}`);
    info('r.maxMoon', (mi) => `${fmt(mi.maxStableEarthRadii, 3)} ${t('u.Re')}${mi.unbound ? ' ⚠' : ''}`);
    info('r.tide', (mi) => fmt(mi.tideRel, 3), 1);
    info('r.eclipse', (mi) => t(mi.totalEclipse ? 'yes' : 'no'), 2);
    body.appendChild(box);
  });
  if (P.moons.length < 3) {
    const b = el('button', 'btn', t('moon.add'));
    b.onclick = () => S.structural(() => S.planet().moons.push({ massMoon: 1, aEarthRadii: 60 }));
    body.appendChild(b);
  }
}

function system(body, S) {
  const d = S.design;
  const hzCount = d.planets.filter((p) => p.role === 'hz').length;
  d.planets.forEach((p, i) => {
    const DP = S.derived.planets[i];
    const box = el('div', 'sysrow' + (i === S.sel ? ' on' : ''));
    const name = el('input', 'name'); name.value = p.name; name.maxLength = 40;
    name.onchange = () => S.structural(() => { const q = S.design.planets[i]; q.name = name.value.trim() || q.name; });
    box.appendChild(name);
    box.appendChild(el('span', 'sysinfo',
      `${esc(t('role.' + p.role))}${p.role === 'other' ? ' · ' + t('type.' + p.type) : ''} · ${fmt(p.orbit.aAU, 3)} ${t('u.AU')} · ` +
      `${fmt(p.bulk.massEarth, 3)} ${t('u.Mearth')} · ${fmt(DP.radiusEarth, 3)} ${t('u.Re')}`));
    const btns = el('div', 'sysbtns');
    const pick = el('button', 'btn', '✎'); pick.title = t('planet.pick');
    pick.onclick = () => { S.sel = i; S.tab = 'orbit'; S.redraw(); };
    btns.appendChild(pick);
    if (p.role === 'other') {
      const b = el('button', 'btn', t('planet.makehz'));
      if (hzCount >= MAX_HZ) { b.disabled = true; b.title = t('planet.hzfull'); }
      b.onclick = () => S.structural(() => S.makeHz(i));
      btns.appendChild(b);
    } else {
      const b = el('button', 'btn', t('planet.makeother'));
      b.onclick = () => S.structural(() => S.makeOther(i));
      btns.appendChild(b);
    }
    if (d.planets.length > 1) btns.appendChild(confirmButton(t('planet.remove'), () => S.structural(() => S.removePlanet(i))));
    box.appendChild(btns);
    body.appendChild(box);
  });
  if (hzCount >= MAX_HZ) body.appendChild(el('p', 'note', t('planet.hzfull')));
  if (d.planets.length < MAX_PLANETS) {
    const b = el('button', 'btn', t('planet.add'));
    b.onclick = () => S.structural(() => S.addPlanet());
    body.appendChild(b);
  }
}

/* An explanation card (spec §6.2): Explorer sees what, why, our solar system and something to try; Student and
 * Scientist also the equation, model and uncertainty; contested rows show both views with sources (§6.3). */
function card(r, level, S) {
  const lang = getLang(), c = CARDS[r.id] && CARDS[r.id][lang], Lb = CARD_LABELS[lang];
  const box = el('div', 'card');
  if (!c) return box;
  const part = (k) => { const d = el('div', 'cpart'); d.append(el('span', 'ck', Lb[k]), el('span', 'cv')); d.lastChild.textContent = c[k]; box.appendChild(d); };
  part('what'); part('why'); part('ours'); part('try');
  if (level >= 2) part('more');
  const con = CARDS[r.id].contested;
  if (con) {
    const d = el('div', 'cpart contested-views');
    d.appendChild(el('span', 'ck', Lb.contested));
    for (const v of con[lang]) { const p = el('p', 'view'); p.textContent = v; d.appendChild(p); }
    box.appendChild(d);
  }
  const src = el('div', 'cpart'); src.append(el('span', 'ck', t('src')), el('span', 'cv')); src.lastChild.textContent = r.src; box.appendChild(src);
  const node = nodeForRow(r.id);
  if (node && S.view) {
    const b = el('button', 'btn', Lb.map);
    b.onclick = (ev) => { ev.stopPropagation(); S.view.showCausality(node); };
    box.appendChild(b);
  }
  return box;
}

/* The ledger: five tier lamps, then the rows grouped by tier; a row opens its explanation card. */
export function renderLedger(root, S) {
  const level = LEVELS[S.design.level] || 1;
  root.innerHTML = '';
  const hzPlanets = S.derived.planets.filter((p) => p.role === 'hz');
  root.appendChild(el('h2', '', t('ledger')));
  if (!hzPlanets.length) { root.appendChild(el('p', 'note', t('ledger.none'))); return; }
  const DP = S.derivedPlanet().role === 'hz' ? S.derivedPlanet() : hzPlanets[0];
  const L = S.ledgerFor(DP);
  root.appendChild(el('p', 'for', t('ledger.for', { name: DP.name })));
  const lamps = el('div', 'lamps');
  for (const tier of [1, 2, 3, 4, 5]) {
    const ok = L.pass[tier];
    const failRow = L.firstFail[tier];
    const lamp = el('div', 'lamp ' + (ok ? 'green' : 'red'));
    lamp.innerHTML = `<span class="dot"></span><span class="tn">${tier}</span><span class="tt">${t('tier.' + tier)}</span>` +
      `<span class="ts">${ok ? t('tier.pass') : t('tier.fail', { row: failRow ? t('row.' + failRow) : '' })}</span>`;
    lamps.appendChild(lamp);
  }
  root.appendChild(lamps);
  /* where the models stop holding (or hold only roughly) for this planet */
  if (DP.modelLimits && DP.modelLimits.length) {
    const worst = DP.modelLimits.some((l) => l.level === 'outside') ? 'outside' : 'rough';
    const box = el('div', 'limits ' + worst);
    box.innerHTML = `<strong>⚠ ${esc(t('lim.title.' + worst))}</strong><p>${esc(t('lim.intro'))}</p><ul>`
      + DP.modelLimits.map((l) => `<li class="${l.level}">${esc(t('lim.' + l.id))}</li>`).join('') + '</ul>';
    root.appendChild(box);
  }
  /* the Earth Similarity Index, small and qualified: similarity, not habitability (spec §5.11) */
  const e = esi(DP);
  if (e != null) root.appendChild(el('p', 'esi', t('esi', { x: e })));
  root.appendChild(el('p', 'cardhint', t('card.hint')));
  for (const tier of [1, 2, 3, 4, 5]) {
    const sec = el('section', 'tier');
    sec.appendChild(el('h3', '', `${tier}. ${t('tier.' + tier)}`));
    for (const r of L.rows.filter((x) => x.tier === tier)) {
      const open = S.openCard === r.id;
      const row = el('div', 'lrow ' + r.status + (open ? ' open' : ''));
      row.tabIndex = 0;
      row.setAttribute('role', 'button');
      row.setAttribute('aria-expanded', open ? 'true' : 'false');
      const toggle = () => { S.openCard = open ? null : r.id; renderLedger(root, S); };
      row.addEventListener('click', (ev) => { if (!ev.target.closest('.card')) toggle(); });
      row.addEventListener('keydown', (ev) => { if ((ev.key === 'Enter' || ev.key === ' ') && !ev.target.closest('.card')) { ev.preventDefault(); toggle(); } });
      const top = el('div', 'ltop');
      top.innerHTML = `<span class="dot"></span><span class="lname">${t('row.' + r.id)}</span>` +
        `<span class="lval">${t(r.value.k, r.value.p)}</span><span class="lst">${t('st.' + r.status)}</span>`;
      const lim = limitFor(DP, r.id);
      if (lim) top.querySelector('.lname').insertAdjacentHTML('beforeend', ` <span class="mlim ${lim}" title="${esc(t('lim.title.' + lim))}">${esc(t('lim.tag.' + lim))}</span>`);
      row.appendChild(top);
      const why = el('div', 'lwhy', t(r.why.k, r.why.p));
      if (r.contested) why.innerHTML += ` <span class="contested">${t('contested')}</span>`;
      row.appendChild(why);
      if (level >= 2) row.appendChild(el('div', 'lsrc', `${t('src')}: ${esc(r.src)}`));
      if (open) row.appendChild(card(r, level, S));
      sec.appendChild(row);
    }
    root.appendChild(sec);
  }
}
