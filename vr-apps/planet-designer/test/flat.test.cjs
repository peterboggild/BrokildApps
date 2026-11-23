/* Flat-preview check of Planet Designer in headless Chromium (software GL, so slow: it waits by frames).
 *   python3 -m http.server 8765        (from the repository root)
 *   NODE_PATH=$(npm root -g) node vr-apps/planet-designer/test/flat.test.cjs [screenshot-dir]
 * Panel and ledger (M0), the orrery: a K-dwarf system gets a second stable HZ planet by clicking the glowing slot
 * and a planet is dragged with the mouse (M1), the planet view with its LOD terrain and the cutaway (M2).
 * Look at the screenshots too: numbers alone do not show whether a planet looks right. */
const { chromium } = require('playwright');
const BASE = 'http://localhost:8765/vr-apps/planet-designer/?q=low';
const frames = (p, n) => p.evaluate((n) => new Promise((res) => {
  const s = window.__pd.view.state.frames; const tk = () => (window.__pd.view.state.frames >= s + n ? res() : requestAnimationFrame(tk)); tk();
}), n);
const screenPos = (p, which) => p.evaluate((which) => {
  const v = window.__pd.view, S = window.__pd;
  const obj = which === 'slot' ? v.orrery.slot : v.orrery.beads.find((b) => b.index === S.sel).mesh;
  const q = v.toScreen(obj.getWorldPosition(obj.position.clone()));
  const r = v.renderer.domElement.getBoundingClientRect();
  return { x: r.left + q.x, y: r.top + q.y };
}, which);

(async () => {
  const out = process.argv[2];
  const b = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox'] });
  const p = await b.newPage({ viewport: { width: 1500, height: 900 } });
  const errs = [];
  let fails = 0;
  const check = (name, ok, info = '') => { if (!ok) fails++; console.log(`${ok ? 'ok  ' : 'FAIL'} ${name} ${info}`); };
  p.on('pageerror', (e) => errs.push(e.message));
  p.on('console', (m) => { if (m.type() === 'error') errs.push(m.text()); });
  await p.goto(BASE + '&lang=da');
  await p.waitForFunction(() => window.__pd && window.__pd.view, null, { timeout: 120000 });
  await frames(p, 2);
  check('Earth reaches tier 5 at start', await p.evaluate(() => document.querySelectorAll('.lamp.green').length === 5));
  if (out) await p.screenshot({ path: out + '/t-system.png' });

  /* M1: second HZ planet by clicking the slot, then drag it */
  await p.selectOption('#preset', 'kdwarf');
  await frames(p, 2);
  /* the slow software renderer can still be settling after the preset: wait for the climate run, retry the click */
  await p.waitForFunction(() => window.__pd.derivedPlanet().ebm, null, { timeout: 60000 });
  for (let k = 0; k < 3; k++) {
    await frames(p, 2);
    const slot = await screenPos(p, 'slot');
    await p.mouse.click(slot.x, slot.y);
    await frames(p, 2);
    if (await p.evaluate(() => window.__pd.design.planets.filter((q) => q.role === 'hz').length === 2)) break;
  }
  const hz = await p.evaluate(() => window.__pd.derived.planets.filter((q) => q.role === 'hz').map((q) => ({ a: q.aAU, hill: q.hillSpacing, pos: q.hzPos })));
  check('Slot click adds a second HZ planet', hz.length === 2, JSON.stringify(hz.map((x) => x.a.toFixed(3))));
  check('Both HZ planets in the conservative zone', hz.every((x) => x.pos === 'cons'));
  check('Stable spacing (>= 10 mutual Hill radii)', hz.every((x) => x.hill >= 10), hz.map((x) => x.hill.toFixed(1)).join(' '));
  const a0 = hz[1].a;
  const bead = await screenPos(p, 'bead');
  await p.mouse.move(bead.x, bead.y); await frames(p, 1);
  await p.mouse.down(); await p.mouse.move(bead.x, bead.y + 6, { steps: 3 }); await p.mouse.up();
  await frames(p, 2);
  const a1 = await p.evaluate(() => window.__pd.design.planets[window.__pd.sel].orbit.aAU);
  check('Dragging a planet changes its distance', Math.abs(a1 - a0) > 1e-3, `${a0.toFixed(3)} -> ${a1.toFixed(3)} AU`);
  if (out) await p.screenshot({ path: out + '/t-kdwarf.png' });

  /* M2: planet view, LOD terrain, cutaway */
  await p.selectOption('#preset', 'solar');
  await p.evaluate(() => window.__pd.view.setMode('planet'));
  await frames(p, 3);
  /* "low" (the setting this test runs at, and the Quest's) draws the baked planet; the others the live LOD terrain */
  await p.waitForFunction(() => !window.__pd.view.planet.baked || window.__pd.view.planet.bakeProgress > 0, null, { timeout: 120000, polling: 500 }).catch(() => {});
  const pv = await p.evaluate(() => ({ baked: window.__pd.view.planet.baked, prog: window.__pd.view.planet.bakeProgress, chunks: window.__pd.view.state.chunks }));
  check('Planet view draws the planet (baked at low quality)', pv.baked ? pv.prog > 0 : pv.chunks > 6, JSON.stringify(pv));
  await p.evaluate(() => { const v = window.__pd.view; v.state.cut = true; v.refresh(); });
  await frames(p, 3);
  const labels = await p.evaluate(() => [...document.querySelectorAll('.v-label.layer')].filter((e) => !e.hidden).map((e) => e.textContent));
  check('Cutaway labels the four layers of Earth', labels.length === 4, JSON.stringify(labels));
  if (out) await p.screenshot({ path: out + '/t-cut.png' });

  /* panel, language, share link, phone width (from M0) */
  await p.click('#langBtn');
  check('English after toggle', (await p.textContent('h2')) === 'Can it host life?');
  await p.selectOption('#preset', 'hothouse');
  check('Hothouse world fails tier 1', await p.evaluate(() => document.querySelectorAll('.lamp.red').length === 5));
  await p.waitForFunction(async () => location.hash.length > 2 && (await (await import('./design.js?v=6')).fromHash(location.hash.slice(1)))?.name === 'Calida', null, { timeout: 60000, polling: 500 });
  const p2 = await b.newPage();
  await p2.goto(p.url());
  await p2.waitForFunction(() => window.__pd && window.__pd.derived, null, { timeout: 120000 });
  check('Share link round trip', (await p2.evaluate(() => window.__pd.design.name)) === 'Calida');
  const mob = await b.newPage({ viewport: { width: 390, height: 844 } });
  await mob.goto(BASE);
  await mob.waitForFunction(() => window.__pd && window.__pd.derived, null, { timeout: 120000 });
  check('No horizontal scroll at phone width', (await mob.evaluate(() => document.documentElement.scrollWidth)) <= 390);
  check('No errors', errs.length === 0, JSON.stringify(errs));
  await b.close();
  process.exit(fails ? 1 : 0);
})();
