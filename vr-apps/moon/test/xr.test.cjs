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
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/moon.html?view=surface&still');
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
  /* on the surface: teleport (near, then 2.7 km away onto generated ground) puts your head on the spot and
   * leaves two prints; walking leaves a print per stride, also on the generated ground */
  await p.evaluate(() => __moon.setView('surface'));
  await frames(3);
  const headAt = () => p.evaluate(() => { const e = __moon.camera.matrixWorld.elements; return [e[12], e[13], e[14]]; });
  const jump = async (x, z) => {
    const n = await p.evaluate(() => __moon.prints.count);
    await p.evaluate(([x, z]) => __moon.teleport(x, z), [x, z]);
    await p.waitForFunction(() => !__moon.teleporting, null, { timeout: 120000 });
    await frames(2);
    const h = await headAt(), st = await p.evaluate(() => ({ n: __moon.prints.count, exp: __moon.moon.shared.uExposure.value, ms: Math.round(__moon.ground.ms), rocks: __moon.ground.rocks, calls: __moon.S.calls, tris: __moon.S.tris }));
    const gy = await p.evaluate(([x, z]) => __moon.moon.heightAt(x, z), [x, z]);
    return { ok: st.n === n + 2 && Math.hypot(h[0] - x, h[2] - z) < 0.2 && st.exp > 3 && h[1] - gy > 0.5 && h[1] - gy < 2.5, h, gy, st };
  };
  let j = await jump(40, -30);
  ok('teleport 50 m: head on the spot, two prints', j.ok, JSON.stringify(j));
  j = await jump(600, -2600);
  ok('teleport 2.7 km: ground generated there, head on it, two prints', j.ok && j.st.rocks > 50, JSON.stringify(j));
  ok('generated ground is detailed under you', await p.evaluate(() => __moon.ground.detailed(600, -2600) && __moon.ground.covers(600, -2600, 14)));
  const n0 = await p.evaluate(() => __moon.prints.count), sq0 = await p.evaluate(() => __moon.ground.count);
  for (let k = 0; k < 60; k++) { await p.evaluate(() => { __moon.S.pos.x += 0.5; }); await frames(1); }
  await p.waitForFunction(() => !__moon.ground.busy, null, { timeout: 120000 });
  await frames(2);
  const w = await p.evaluate(() => ({ n: __moon.prints.count, sq: __moon.ground.count, x: __moon.S.pos.x }));
  ok('walking 30 m leaves prints and the ground follows', w.n - n0 >= 30 && w.sq > sq0 && await p.evaluate(() => __moon.ground.detailed(__moon.S.pos.x, __moon.S.pos.z)), JSON.stringify(w));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'xr-far.png') });
  j = await jump(-20, 30);
  ok('teleport back to the lander', j.ok, JSON.stringify(j.h));
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
