/* Planet Designer, milestone M0: boot, state, the desktop page (parameter panel and ledger as text).
 * The 3D workshop (M1+) and VR (M5) attach to the same state later. */
import { validate, defaultDesign, PRESETS, saveLocal, loadLocal, toHash, fromHash, MAX_HZ, MAX_PLANETS,
  earthPlanet, otherPlanet } from './design.js?v=2';
import { derive } from './physics/derive.js?v=2';
import { ledger } from './physics/ledger.js?v=2';
import { renderPanel, renderLedger } from './ui/dom.js?v=2';
import { t, setLang, getLang } from './i18n.js?v=2';
import { createView } from './scene/view.js?v=2';

const $ = (id) => document.getElementById(id);

const S = {
  design: defaultDesign(), derived: null, sel: 0, tab: 'star', live: [],
  planet() { return this.design.planets[this.sel] || this.design.planets[0]; },
  derivedPlanet() { return this.derived.planets[this.sel] || this.derived.planets[0]; },
  ledgerFor(P) { return ledger(P, this.derived); },
  recompute() { this.derived = derive(this.design); },
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
    this.changed(false);
  },
  /* A structural edit (add/remove planet or moon, role change): apply, validate, redraw everything. */
  structural(fn) { fn(); this.design = validate(this.design); this.sel = Math.min(this.sel, this.design.planets.length - 1); this.changed(true); },
  changed(full) {
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
};

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
  $('levelLabel').textContent = t('level');
  $('presetLabel').textContent = t('preset');
  for (const b of document.querySelectorAll('#levels button')) {
    b.textContent = t('level.' + b.dataset.level);
    b.classList.toggle('on', b.dataset.level === S.design.level);
  }
  const ps = $('preset');
  ps.innerHTML = `<option value="">…</option>` + Object.keys(PRESETS).map((k) => `<option value="${k}">${t('preset.' + k)}</option>`).join('');
  $('shareBtn').textContent = t('btn.share');
  $('exportBtn').textContent = t('btn.export');
  $('importBtn').textContent = t('btn.import');
  $('resetBtn').textContent = t('btn.reset');
}

function toast(msg) {
  const e = $('toast'); e.textContent = msg; e.hidden = false;
  clearTimeout(toast.t); toast.t = setTimeout(() => { e.hidden = true; }, 2200);
}

function load(design) {
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
  window.__pd = S;    /* test hook */
}
boot();
