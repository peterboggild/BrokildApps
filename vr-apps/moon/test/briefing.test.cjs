/* The briefing in an emulated Meta Quest 3 (IWER): enters VR, both eyes drawn, the trigger moves to the next board,
 * the narration is loaded, no errors.   node vr-apps/moon/test/briefing.test.cjs  (NODE_PATH with playwright-core, iwer) */
const fs = require('fs');
const { chromium } = require('playwright-core');
const IWER = require.resolve('iwer/build/iwer.min.js');
let fails = 0;
const ok = (n, c, i = '') => { console.log(`${c ? 'ok  ' : 'FAIL'} ${n}${i ? '  ' + i : ''}`); if (!c) fails++; };
(async () => {
  const b = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox', '--autoplay-policy=no-user-gesture-required'] });
  const p = await b.newPage({ viewport: { width: 1200, height: 600 } });
  const errs = [];
  p.on('pageerror', e => errs.push(e.message));
  p.on('console', m => { if (m.type() === 'error' && !/favicon|404/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript({ content: fs.readFileSync(IWER, 'utf8') + ';window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.stereoEnabled = true; window.__dev.installRuntime({ forceInstall: true });' });
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/briefing.html');
  await p.waitForFunction(() => window.__brief && __brief.S.ready, null, { timeout: 120000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.click('#vr');
  await p.waitForFunction(() => __brief.renderer.xr.isPresenting, null, { timeout: 60000 });
  ok('VR session runs', true);
  const frames = n => p.evaluate(n => new Promise(r => { const s = __brief.S.frames; const t = () => (__brief.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  await frames(4);
  const eyes = await p.evaluate(() => new Promise(res => { __brief.afterFrame = () => {
    const c = __brief.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100; const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = []; for (const x0 of [0, 100]) { let s = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) s += d[(y * 200 + x) * 4 + 2]; out.push(s / 10000); } res(out); }; }));
  ok('both eyes drawn', eyes.every(v => v > 4), eyes.map(v => v.toFixed(0)).join(' / '));
  const s0 = await p.evaluate(() => __brief.S.slide);
  await p.evaluate(() => { const c = __dev.controllers.right; c.updateButtonValue('trigger', 1); });
  await frames(3);
  await p.evaluate(() => { const c = __dev.controllers.right; c.updateButtonValue('trigger', 0); });
  await frames(2);
  const s1 = await p.evaluate(() => __brief.S.slide);
  ok('the trigger moves to the next board', s1 === s0 + 1, `${s0} -> ${s1}`);
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
