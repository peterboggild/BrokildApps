/* The journey's chapters hand over to each other: at each chapter's end, A in the (emulated) headset leaves VR and
 * opens the next page. Launch -> to the Moon -> lunar orbit -> into the LM -> the descent -> the surface.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/chain.test.cjs */
const fs = require('fs');
const { chromium } = require('playwright-core');
const IWER = process.env.IWER || require.resolve('iwer/build/iwer.min.js');
let fails = 0;
const ok = (n, c, i = '') => { console.log(`${c ? 'ok  ' : 'FAIL'} ${n}${i ? '  ' + i : ''}`); if (!c) fails++; };
/* page, its hook, how to bring it to its end, the next page */
const CHAIN = [
  ['launch.html', '__launch', 'H.setTime(800); H.S.running = true;', 'orbit.html'],
  ['orbit.html', '__orbit', 'H.S.running = true; H.setTime(H.END - 1);', 'lunar.html'],
  ['lunar.html', '__lunar', 'H.S.running = true; H.setTime(H.END - 1);', 'lm.html'],
  ['lm.html', '__lm', 'H.enterLM(true); H.S.running = true; H.setTime(H.END - 1);', 'descent.html'],
  ['descent.html', '__descent', 'H.S.running = true; H.S.landed = H.S.t - 41;', 'index.html']
];
(async () => {
  const b = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox', '--autoplay-policy=no-user-gesture-required'] });
  for (const [page, hook, toEnd, next] of CHAIN) {
    const p = await b.newPage({ viewport: { width: 900, height: 500 } });
    const errs = []; p.on('pageerror', e => errs.push(e.message));
    await p.addInitScript({ content: fs.readFileSync(IWER, 'utf8') + ';window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.installRuntime({ forceInstall: true });' });
    await p.goto((process.env.BASE || 'http://127.0.0.1:8765/vr-apps/moon/') + page);
    await p.waitForFunction(h => window[h] && window[h].S.ready, hook, { timeout: 180000 });
    await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
    if (page === 'launch.html') await p.selectOption('#start', '540');
    await p.click('#vr');
    await p.waitForFunction(h => window[h].renderer.xr.isPresenting, hook, { timeout: 60000 });
    await p.evaluate(([h, code]) => { const H = window[h]; new Function('H', code)(H); }, [hook, toEnd]);
    await p.waitForFunction(h => window[h].S.ended || (h === '__launch' && window[h].S.t >= 800), hook, { timeout: 60000 }).catch(() => {});
    await p.evaluate(() => new Promise(r => setTimeout(r, 600)));
    await p.evaluate(() => { __dev.controllers.right.updateButtonValue('a-button', 1); });
    let went = false;
    try { await p.waitForURL(u => u.toString().includes(next), { timeout: 30000 }); went = true; } catch (e) { /* stays */ }
    ok(`${page}: A at the end opens ${next}`, went && errs.length === 0, went ? '' : 'still on ' + p.url() + (errs.length ? ' | ' + errs[0] : ''));
    await p.close();
  }
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
