/* Planet Designer: boot, state, the desktop page (panel, 3D view, ledger), and the climate worker.
 * The VR workshop (M5) attaches to the same state later. */
import { validate, defaultDesign, PRESETS, saveLocal, loadLocal, toHash, fromHash, MAX_HZ, MAX_PLANETS,
  earthPlanet, otherPlanet } from './design.js?v=15';
import { derive, deriveSync } from './physics/derive.js?v=15';
import { ledger } from './physics/ledger.js?v=15';
import { renderPanel, renderLedger } from './ui/dom.js?v=15';
import { t, setLang, getLang } from './i18n.js?v=15';
import { createView } from './scene/view.js?v=15';
import { createAudio } from './audio.js?v=15';
import { CHALLENGES, loadDone, saveDone, meets } from './challenges.js?v=15';

const $ = (id) => document.getElementById(id);

const S = {
  chDone: loadDone(),
  design: defaultDesign(), derived: null, sel: 0, tab: 'star', live: [],
  planet() { return this.design.planets[this.sel] || this.design.planets[0]; },
  derivedPlanet() { return this.derived.planets[this.sel] || this.derived.planets[0]; },
  ledgerFor(P) { return ledger(P, this.derived); },
  ebm: {},                      /* planet id -> { key, result } of the climate model runs */
  recompute() { this.derived = derive(this.design, this.ebm); requestClimate(); },
  /* A value edit: set, re-derive, refresh readouts and the ledger, keep the controls. */
  set(path, value) {
    const parts = path.split('.');
    const head = parts.shift();
    let o = head === 'star' ? this.design.star : head === 'planet' ? this.planet() : this.design;
    if (head !== 'star' && head !== 'planet') parts.unshift(head);
    const last = parts.pop();
    for (const k of parts) o = o[k];
    o[last] = value;
    this.design = validate(this.design);
    if (this.audio && this.audio.param(path, value, soundPos(path, value)) && this.xr && this.xr.tickHaptic) this.xr.tickHaptic();
    this.changed(false);
  },
  /* A structural edit (add/remove planet or moon, role change): apply, validate, redraw everything. */
  structural(fn) {
    const count = (d) => d.planets.length + d.planets.reduce((s, p) => s + (p.moons ? p.moons.length : 0), 0);
    const before = count(this.design);
    fn(); this.design = validate(this.design); this.sel = Math.min(this.sel, this.design.planets.length - 1);
    if (this.audio) { const after = count(this.design); if (after !== before) this.audio.chime(after > before); }
    this.changed(true);
  },
  changed(full) {
    setTimeout(listenLedger, 0);
    this.recompute();
    saveLocal(this.design);
    if (full) this.redraw();
    else { for (const f of this.live) f(); renderLedger($('ledger'), this); if (this.view) this.view.refresh(); }
    scheduleHash();
  },
  redraw() {
    this.recompute();
    renderPanel($('panel'), this);
    renderLedger($('ledger'), this);
    renderHeader();
    if (this.view) this.view.refresh();
  },
  /* the orrery's glowing slot: a new habitable-zone planet at a stable distance */
  addHzAt(aAU) {
    if (this.design.planets.filter((p) => p.role === 'hz').length >= MAX_HZ || this.design.planets.length >= MAX_PLANETS) return;
    let n = this.design.planets.length + 1, id;
    do { id = 'h' + n++; } while (this.design.planets.some((p) => p.id === id));
    const p = earthPlanet(id, getLang() === 'da' ? 'Ny verden' : 'New world');
    p.orbit.aAU = aAU; p.orbit.e = 0.01; p.surface.terrainSeed = 1 + Math.floor(Math.random() * 99998);
    if (this.design.star.massSun < 0.85) p.moons[0].aEarthRadii = 30;
    this.structural(() => { this.design.planets.push(p); this.sel = this.design.planets.length - 1; });
  },
  makeHz(i) {
    if (this.design.planets.filter((p) => p.role === 'hz').length >= MAX_HZ) return;
    const old = this.design.planets[i];
    const p = earthPlanet(old.id, old.name);
    p.orbit = { ...p.orbit, ...old.orbit };
    p.bulk.massEarth = Math.max(0.05, Math.min(10, old.bulk.massEarth));
    this.design.planets[i] = p;
  },
  makeOther(i) {
    const old = this.design.planets[i];
    this.design.planets[i] = otherPlanet(old.id, old.name, 'rocky', old.orbit.aAU, old.bulk.massEarth, old.orbit.e);
  },
  addPlanet() {
    if (this.design.planets.length >= MAX_PLANETS) return;
    const far = Math.max(...this.design.planets.map((p) => p.orbit.aAU));
    let n = this.design.planets.length + 1, id;
    do { id = 'x' + n++; } while (this.design.planets.some((p) => p.id === id));
    this.design.planets.push(otherPlanet(id, (getLang() === 'da' ? 'Planet ' : 'Planet ') + (this.design.planets.length + 1),
      'icy', Math.min(50, far * 1.8), 15, 0.02));
    this.sel = this.design.planets.length - 1;
  },
  removePlanet(i) { this.design.planets.splice(i, 1); this.sel = 0; },
  loadPreset(k) { if (PRESETS[k]) load({ ...PRESETS[k](), level: this.design.level }); },
  /* Put the selected habitable-zone planet where it gets about as much light as Earth (S = 0.95), kept inside the
   * conservative zone for this star and this planet's mass, allowing for its eccentricity. */
  toZone() {
    const P = this.derivedPlanet();
    if (!P || P.role !== 'hz') return;
    const s = P.hz.seff;
    const target = Math.min(s.runaway * 0.95, Math.max(s.maxGreenhouse * 1.05, 0.95));
    const e = this.planet().orbit.e;
    this.set('planet.orbit.aAU', Math.sqrt(this.derived.star.lum / (target * Math.sqrt(1 - e * e))));
  },
};

/* The climate model runs in a worker (spec §10.6: heavy models never on the render thread). On the PC it follows
 * edits by itself, debounced; the panel and the ledger show the previous result as 'running' meanwhile. A new run
 * starts from the planet's last climate, so freezing a world and warming it again shows the hysteresis. */
let worker = null, climateTimer = null;
const inFlight = {};
try {
  worker = new Worker(new URL('./physics/worker.js?v=15', import.meta.url), { type: 'module' });
  worker.onmessage = (ev) => {
    const { id, key, result } = ev.data;
    delete inFlight[id];
    if (!result) return;
    S.ebm[id] = { key, result };
    setTimeout(listenLedger, 0);
    S.derived = derive(S.design, S.ebm);
    for (const f of S.live) f();
    renderLedger($('ledger'), S);
    if (S.view) S.view.refresh();
    requestClimate();
  };
} catch (e) { worker = null; }
function requestClimate() {
  clearTimeout(climateTimer);
  climateTimer = setTimeout(() => {
    if (!S.derived) return;
    for (const P of S.derived.planets) {
      if (!P.ebmPending || inFlight[P.id] === P.ebmKey) continue;
      const prev = S.ebm[P.id];
      const params = { ...P.ebmParams, init: prev ? prev.result.final : 'warm' };
      if (worker) { inFlight[P.id] = P.ebmKey; worker.postMessage({ id: P.id, key: P.ebmKey, params }); }
    }
    if (!worker) {
      S.derived = deriveSync(S.design, S.ebm);
      for (const P of S.derived.planets) if (P.ebm) S.ebm[P.id] = { key: P.ebmKey, result: P.ebm };
      renderLedger($('ledger'), S);
    }
  }, 120);
}

/* The share link is kept in the address bar as the design changes (debounced). */
let hashTimer = null;
function scheduleHash() {
  clearTimeout(hashTimer);
  hashTimer = setTimeout(async () => { history.replaceState(null, '', '#' + (await toHash(S.design))); }, 400);
}

function renderHeader() {
  document.title = t('app.title');
  $('sub').textContent = t('app.sub');
  $('m0').textContent = t('app.m0');
  $('foot').textContent = t('foot');
  $('langBtn').textContent = t('lang');
  if (S.audio) {
    $('musicBtn').textContent = t('snd.music') + ': ' + t(S.audio.prefs.music ? 'snd.on' : 'snd.off');
    $('fxBtn').textContent = t('snd.fx') + ': ' + t(S.audio.prefs.fx ? 'snd.on' : 'snd.off');
  }
  $('levelLabel').textContent = t('level');
  $('presetLabel').textContent = t('preset');
  for (const b of document.querySelectorAll('#levels button')) {
    b.textContent = t('level.' + b.dataset.level);
    b.classList.toggle('on', b.dataset.level === S.design.level);
  }
  const ps = $('preset');
  ps.innerHTML = `<option value="">…</option>` + Object.keys(PRESETS).map((k) => `<option value="${k}">${t('preset.' + k)}</option>`).join('');
  $('chBtn').textContent = `${t('ch.btn')} ${S.chDone.size}/${CHALLENGES.length}`;
  $('shareBtn').textContent = t('btn.share');
  $('exportBtn').textContent = t('btn.export');
  $('importBtn').textContent = t('btn.import');
  $('resetBtn').textContent = t('btn.reset');
}

function toast(msg) {
  const e = $('toast'); e.textContent = msg; e.hidden = false;
  clearTimeout(toast.t); toast.t = setTimeout(() => { e.hidden = true; }, 2200);
}

/* Sound: a position 0..1 for the pitch of a value's tick (log scale for the quantities that span decades) */
const RANGES = { aAU: [0.01, 50], massSun: [0.08, 2], ageGyr: [0.01, 15], massEarth: [0.05, 10], oceansEarth: [0.01, 50],
  pN2bar: [1e-4, 10], pO2bar: [1e-4, 2], pCO2bar: [1e-5, 100], pCH4bar: [1e-8, 0.01], pArbar: [1e-5, 1], massMoon: [0.08, 8.1], aEarthRadii: [1.5, 1000] };
function soundPos(path, v) {
  const k = path.split('.').pop();
  if (typeof v !== 'number') return 0.5;
  const R = RANGES[k];
  if (R) return Math.max(0, Math.min(1, Math.log(Math.max(v, R[0]) / R[0]) / Math.log(R[1] / R[0])));
  return Math.max(0, Math.min(1, 0.5 + 0.5 * Math.tanh(v / 10)));
}
/* The ledger sings: a tier reached plays a bright chord, a tier lost a dark tone. Only on the climate model's
 * verdict (not the quick estimate while it runs), at most every 0.6 s, for the planet in view. */
let ledgerPrev = null, ledgerT = 0;
/* Challenges: a newly met one is saved, announced and celebrated */
function checkChallenges() {
  if (!S.derived) return;
  const now = meets(S.derived, (P) => S.ledgerFor(P));
  const fresh = [...now].filter((id) => !S.chDone.has(id));
  if (!fresh.length) return;
  for (const id of fresh) S.chDone.add(id);
  saveDone(S.chDone);
  toast(t('ch.toast', { name: fresh.map((id) => t('ch.' + id + '.title')).join(', ') }));
  if (S.audio) S.audio.tier(true, 4);
  renderHeader();
  if (!$('chBox').hidden) renderChallenges();
  if (S.redrawAll) S.redrawAll();
}
function renderChallenges() {
  const box = $('chBox');
  box.innerHTML = `<button class="cm-pop-x" id="chClose" aria-label="${t('close')}">×</button><h2>${t('ch.title')}</h2><p class="note">${t('ch.intro')}</p>`
    + CHALLENGES.map((c) => {
      const done = S.chDone.has(c.id);
      return `<div class="chal${done ? ' done' : ''}"><div class="chal-top"><span class="chal-mark">${done ? '✓' : '○'}</span><strong>${t('ch.' + c.id + '.title')}</strong>`
        + `<span class="chal-st">${t(done ? 'ch.done' : 'ch.open')}</span></div><p>${t('ch.' + c.id + '.goal')}</p>`
        + (done ? `<p class="chal-think"><b>${t('ch.think')}</b> ${t('ch.' + c.id + '.think')}</p>` : '') + '</div>';
    }).join('');
  $('chClose').onclick = () => { box.hidden = true; };
}

function listenLedger() {
  checkChallenges();
  if (!S.audio || !S.derived) return;
  S.audio.setStar(S.derived.star.teff);
  const P = S.derivedPlanet();
  if (!P || P.role !== 'hz' || (!P.ebm && !P.climate.runaway)) return;
  const pass = [1, 2, 3, 4, 5].map((n) => !!S.ledgerFor(P).pass[n]);
  const key = P.id;
  if (ledgerPrev && ledgerPrev.key === key && performance.now() - ledgerT > 600) {
    const gained = pass.map((v, i) => v && !ledgerPrev.pass[i]).lastIndexOf(true);
    const lost = pass.some((v, i) => !v && ledgerPrev.pass[i]);
    if (gained >= 0) { S.audio.tier(true, gained + 1); ledgerT = performance.now(); }
    else if (lost) { S.audio.tier(false); ledgerT = performance.now(); }
  }
  ledgerPrev = { key, pass };
}

function load(design) {
  if (S.audio) { S.audio.whoosh(); S.audio.resetParams(); }
  S.ebm = {};                   /* a new design starts its climate from warm, not from the last world's ice */
  S.design = validate(design); S.sel = Math.max(0, S.design.planets.findIndex((p) => p.role === 'hz'));
  S.changed(true);
}

function wire() {
  $('langBtn').onclick = () => { setLang(getLang() === 'da' ? 'en' : 'da'); try { localStorage.setItem('planetDesigner.lang', getLang()); } catch (e) { /* ignore */ } S.redraw(); };
  for (const b of document.querySelectorAll('#levels button')) b.onclick = () => { S.design.level = b.dataset.level; S.changed(true); };
  $('preset').onchange = (e) => { const k = e.target.value; if (k && PRESETS[k]) load({ ...PRESETS[k](), level: S.design.level }); };
  $('shareBtn').onclick = async () => {
    const url = location.href.split('#')[0] + '#' + (await toHash(S.design));
    try { await navigator.clipboard.writeText(url); toast(t('msg.copied')); } catch (e) { prompt('', url); }
  };
  $('exportBtn').onclick = () => {
    const blob = new Blob([JSON.stringify(S.design, null, 2)], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = (S.design.name || 'planet').replace(/[^\w\-æøåÆØÅ ]+/g, '').trim() + '.planet.json';
    a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  };
  $('importBtn').onclick = () => $('file').click();
  $('file').onchange = async (e) => {
    const f = e.target.files[0]; if (!f) return;
    try { load(JSON.parse(await f.text())); } catch (err) { toast(t('msg.badfile')); }
    e.target.value = '';
  };
  let armed = 0;
  $('chBtn').onclick = () => { const b = $('chBox'); b.hidden = !b.hidden; if (!b.hidden) renderChallenges(); };
  window.addEventListener('keydown', (ev) => { if (ev.key === 'Escape') $('chBox').hidden = true; });
  $('resetBtn').onclick = () => {
    if (armed && Date.now() - armed < 3000) { armed = 0; load({ ...defaultDesign(), level: S.design.level }); return; }
    armed = Date.now(); $('resetBtn').textContent = t('btn.confirm');
    setTimeout(() => { armed = 0; $('resetBtn').textContent = t('btn.reset'); }, 3000);
  };
}

async function boot() {
  let l = 'da';
  try { l = localStorage.getItem('planetDesigner.lang') || (new URLSearchParams(location.search).get('lang')) || 'da'; } catch (e) { /* ignore */ }
  const q = new URLSearchParams(location.search).get('lang');
  setLang(q || l);
  let d = null;
  if (location.hash.length > 2) d = await fromHash(location.hash.slice(1));
  if (!d) d = loadLocal();
  S.design = validate(d || defaultDesign());
  S.sel = Math.max(0, S.design.planets.findIndex((p) => p.role === 'hz'));
  wire();
  S.recompute();
  try { S.view = createView($('view'), S); } catch (e) { console.error(e); $('view').textContent = 'WebGL: ' + e.message; }
  S.redraw();
  S.redrawAll = () => S.redraw();
  S.audio = createAudio();
  $('musicBtn').onclick = () => { S.audio.setPref('music', !S.audio.prefs.music); renderHeader(); };
  $('fxBtn').onclick = () => { S.audio.setPref('fx', !S.audio.prefs.fx); renderHeader(); };
  /* every button and tab on the page clicks */
  document.addEventListener('click', (e) => { if (e.target.closest('button, .tab, .chip, select, .cm-node, .cm-info, input[type=checkbox], input[type=radio]')) S.audio.click(); }, true);
  renderHeader();
  window.__pd = S;    /* test hook */
}
boot();
