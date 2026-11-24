/* VR test in an emulated Meta Quest 3 (IWER) with Playwright and software GL.
 *   python -m http.server 8765                        (from the repository root)
 *   npm i --prefix <dir> playwright-core iwer         (once, outside the repository)
 *   set NODE_PATH=<dir>\node_modules  &&  node vr-apps/moon/test/xr.test.cjs [folder for screenshots]
 * Enters VR in each view and checks that both eyes are drawn with the Moon in them (not black, not blank),
 * that the two eyes differ on the surface (stereo), and that nothing throws. Uses the installed Chrome.
 * Software GL is slow: everything waits by frames. */
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright-core');
const IWER = process.env.IWER || require.resolve('iwer/build/iwer.min.js');
const outDir = process.argv[2] || null;
let fails = 0;
const ok = (n, c, i = '') => { console.log(`${c ? 'ok  ' : 'FAIL'} ${n}${i ? '  ' + i : ''}`); if (!c) fails++; };
(async () => {
  const b = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox'] });
  const p = await b.newPage({ viewport: { width: 1400, height: 700 } });
  const errs = [];
  p.on('pageerror', e => errs.push(e.message));
  p.on('console', m => { if (m.type() === 'error' && !/favicon|404/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript({ content: fs.readFileSync(IWER, 'utf8') + ';window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.stereoEnabled = true; window.__dev.installRuntime({ forceInstall: true });' });
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/?view=surface&still');
  await p.waitForFunction(() => window.__moon && __moon.S.ready, null, { timeout: 180000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  ok('headset detected: Enter VR is enabled', true);
  await p.click('#vr');
  await p.waitForFunction(() => __moon.renderer.xr.isPresenting, null, { timeout: 60000 });
  const frames = n => p.evaluate(n => new Promise(r => { const s = __moon.S.frames; const t = () => (__moon.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  await frames(3);
  ok('VR session runs', await p.evaluate(() => __moon.renderer.xr.isPresenting));
  /* mean brightness and lit fraction of each half of the canvas (IWER draws the two eyes side by side) */
  const eyes = () => p.evaluate(() => new Promise(res => { __moon.afterFrame = () => {
    const c = __moon.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) {
      let sum = 0, lit = 0, sig = '';
      for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) { const v = d[(y * 200 + x) * 4]; sum += v; if (v > 40) lit++; }
      for (let y = 10; y < 100; y += 20) for (let x = x0 + 10; x < x0 + 100; x += 20) sig += String.fromCharCode(65 + (d[(y * 200 + x) * 4] >> 4));
      out.push({ mean: sum / 10000, lit: lit / 10000, sig });
    }
    res(out);
  }; }));
  for (const view of ['surface', 'low', 'orbit']) {
    await p.evaluate(v => __moon.setView(v, v === 'orbit' ? 4 : v === 'low' ? 1.2 : undefined), view);
    await frames(3);
    const e = await eyes();
    const s = await p.evaluate(() => ({ calls: __moon.S.calls, tris: __moon.S.tris }));
    ok(`${view}: both eyes show the Moon`, e.every(q => q.lit > 0.15 && q.lit < 0.98 && q.mean > 12), JSON.stringify(e.map(q => [q.mean.toFixed(0), q.lit.toFixed(2)])) + ` ${s.calls} calls ${(s.tris / 1000).toFixed(0)}k tris`);
    if (view === 'surface') ok('surface: the two eyes differ (stereo)', e[0].sig !== e[1].sig);
    if (outDir) await p.screenshot({ path: path.join(outDir, `xr-${view}.png`) });
  }
  /* on the surface: teleport moves you and leaves two prints; walking leaves a print per stride */
  await p.evaluate(() => __moon.setView('surface'));
  await frames(3);
  const before = await p.evaluate(() => ({ n: __moon.prints.count, x: __moon.S.pos.x, z: __moon.S.pos.z }));
  await p.evaluate(() => __moon.teleport(40, -30));
  await frames(8);
  const after = await p.evaluate(() => ({ n: __moon.prints.count, x: __moon.S.pos.x, z: __moon.S.pos.z, exp: __moon.moon.shared.uExposure.value }));
  const head = await p.evaluate(() => { const v = new (__moon.camera.position.constructor)(); v.setFromMatrixPosition(__moon.camera.matrixWorld); return [v.x, v.z]; });
  ok('teleport puts your head on the spot and leaves two prints', after.n === before.n + 2 && Math.hypot(head[0] - 40, head[1] + 30) < 0.2 && after.exp > 3,
    JSON.stringify({ before, after, head }));
  const n0 = after.n;
  for (let k = 0; k < 6; k++) { await p.evaluate(k => { __moon.S.pos.x += 0.5; }, k); await frames(1); }
  const n1 = await p.evaluate(() => __moon.prints.count);
  ok('walking 3 m leaves prints', n1 - n0 >= 3, `${n1 - n0} prints`);
  if (outDir) await p.screenshot({ path: path.join(outDir, 'xr-teleported.png') });
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
