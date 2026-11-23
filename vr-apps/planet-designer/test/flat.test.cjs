/* Flat-preview check of Planet Designer in headless Chromium.
 *   python3 -m http.server 8765        (from the repository root)
 *   NODE_PATH=$(npm root -g) node vr-apps/planet-designer/test/flat.test.cjs [screenshot-dir]
 * Loads the page, switches level, language, tabs and presets, moves a slider, round-trips the share link,
 * checks the phone width and that no errors were logged. Look at the screenshots too. */
const { chromium } = require('playwright');
const BASE = 'http://localhost:8765/vr-apps/planet-designer/';
(async () => {
  const out = process.argv[2];
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1400, height: 1000 } });
  const errs = [];
  let fails = 0;
  const check = (name, ok, info = '') => { if (!ok) fails++; console.log(`${ok ? 'ok  ' : 'FAIL'} ${name} ${info}`); };
  p.on('pageerror', (e) => errs.push(e.message));
  p.on('console', (m) => { if (m.type() === 'error') errs.push(m.text()); });
  await p.goto(BASE);
  await p.waitForFunction(() => window.__pd && window.__pd.derived);
  check('Earth reaches tier 5 at start', await p.evaluate(() => document.querySelectorAll('.lamp.green').length === 5));
  if (out) await p.screenshot({ path: out + '/pd-da.png', fullPage: true });
  await p.click('#levels button[data-level=scientist]');
  await p.click('#langBtn');
  check('English after toggle', (await p.textContent('h2')) === 'Can it host life?');
  await p.selectOption('#preset', 'venus');
  check('Venus fails tier 1', await p.evaluate(() => document.querySelectorAll('.lamp.red').length === 5));
  await p.selectOption('#preset', 'solar');
  await p.click('.tab:nth-child(1)');
  const m = await p.evaluate(() => {
    const s = document.querySelector('.slider input[type=range]');
    s.value = 300; s.dispatchEvent(new Event('input'));
    return window.__pd.design.star.massSun;
  });
  check('Star-mass slider moves the design', m < 0.3, String(m));
  await p.waitForTimeout(700);
  const p2 = await b.newPage();
  await p2.goto(p.url());
  await p2.waitForFunction(() => window.__pd && window.__pd.derived);
  check('Share link round trip', Math.abs((await p2.evaluate(() => window.__pd.design.star.massSun)) - m) < 1e-9);
  const mob = await b.newPage({ viewport: { width: 390, height: 844 } });
  await mob.goto(BASE);
  await mob.waitForFunction(() => window.__pd && window.__pd.derived);
  check('No horizontal scroll at phone width', (await mob.evaluate(() => document.documentElement.scrollWidth)) <= 390);
  check('No errors', errs.length === 0, JSON.stringify(errs));
  await b.close();
  process.exit(fails ? 1 : 0);
})();
