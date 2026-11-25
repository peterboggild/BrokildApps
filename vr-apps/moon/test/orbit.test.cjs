/* Chapter 2 (Earth orbit, TLI, transposition and docking, the coast) in an emulated Meta Quest 3 (IWER) with
 * Playwright and the installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/orbit.test.cjs [folder for screenshots]
 * Enters VR, checks seating, the voices, the clock and the time warp (which stops short of the burn and the
 * docking), the trajectory (TLI cutoff speed, the Moon's distance at the end), both eyes drawn through every
 * stretch, the outside view, the S-IVB leaving after the LM is pulled free, the chapter's end, and no errors. */
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright-core');
const IWER = process.env.IWER || require.resolve('iwer/build/iwer.min.js');
const outDir = process.argv[2] || null;
let fails = 0;
const ok = (n, c, i = '') => { console.log(`${c ? 'ok  ' : 'FAIL'} ${n}${i ? '  ' + i : ''}`); if (!c) fails++; };
(async () => {
  const b = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox', '--autoplay-policy=no-user-gesture-required'] });
  const p = await b.newPage({ viewport: { width: 1400, height: 700 } });
  const errs = [];
  p.on('pageerror', e => errs.push(e.message));
  p.on('console', m => { if (m.type() === 'error' && !/favicon|404/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript({ content: fs.readFileSync(IWER, 'utf8') + ';window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.stereoEnabled = true; window.__dev.installRuntime({ forceInstall: true });' });
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/orbit.html');
  await p.waitForFunction(() => window.__orbit && __orbit.S.ready, null, { timeout: 120000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.selectOption('#start', '760');
  await p.click('#vr');
  await p.waitForFunction(() => __orbit.renderer.xr.isPresenting, null, { timeout: 60000 });
  ok('VR session runs', true);
  await p.waitForFunction(() => __orbit.SEAT.done, null, { timeout: 60000 });
  const eye = () => p.evaluate(() => new Promise(r => { __orbit.afterFrame = () => {
    const e = __orbit.camera.matrixWorld.elements, rp = __orbit.rig.position; r([e[12] - rp.x, e[13] - rp.y, e[14] - rp.z]); }; }));
  const e0 = await eye();
  ok('seated: eyes at the eye point', Math.hypot(...e0) < 0.03, JSON.stringify(e0.map(v => +v.toFixed(3))));
  await p.waitForFunction(() => __orbit.radio && __orbit.radio.lines.every(l => l.dur > 0), null, { timeout: 60000 });
  ok('voices loaded', await p.evaluate(() => __orbit.radio.lines.length) >= 30);
  const frames = n => p.evaluate(n => new Promise(r => { const s = __orbit.S.frames; const t = () => (__orbit.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  await p.waitForFunction(() => __orbit.S.running, null, { timeout: 30000 });
  const t0 = await p.evaluate(() => __orbit.S.t); await frames(10); const t1 = await p.evaluate(() => __orbit.S.t);
  ok('the clock runs', t1 > t0, `${t0.toFixed(1)} -> ${t1.toFixed(1)}`);
  /* the warp stops short of the burn and drops to real time */
  const w = await p.evaluate(async () => {
    const T = __orbit.where, tli = 11556;
    __orbit.settings.set('ff', 'major');                 /* events only (the default stops for the radio too) */
    __orbit.setTime(tli - 400); __orbit.S.warp = 1000;
    await new Promise(r => setTimeout(r, 2500));
    return { t: __orbit.S.t, warp: __orbit.S.warp };
  });
  ok('time warp stops before the burn', w.warp === 1 && w.t >= 11556 - 151 && w.t < 11556, JSON.stringify(w));
  /* the trajectory */
  const tr = await p.evaluate(() => { const a = __orbit.where(11907), z = __orbit.where(__orbit.END || 258700); return { v: a.speed, alt: a.alt, dMoon: z.dMoon }; });
  ok('TLI cutoff near 10.8 km/s', tr.v > 10600 && tr.v < 11000, (tr.v / 1000).toFixed(2) + ' km/s at ' + (tr.alt / 1000).toFixed(0) + ' km');
  ok('the chapter ends in lunar space', tr.dMoon < 70000e3, (tr.dMoon / 1e6).toFixed(1) + ' thousand km from the Moon');
  /* the two eyes, through every stretch */
  const eyes = () => p.evaluate(() => new Promise(res => { __orbit.afterFrame = () => {
    const c = __orbit.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
    res(out);
  }; }));
  for (const [t, name] of [[900, 'Earth orbit'], [11700, 'the burn'], [13450, 'turning round'], [13900, 'docked'], [17300, 'LM pulled free'], [86400, 'a day out'], [258000, 'lunar space']]) {
    await p.evaluate(t => { __orbit.setTime(t); __orbit.S.running = false; }, t);
    await frames(4);
    const e = await eyes();
    ok(`${name}: both eyes drawn`, e.every(v => v > 4 && v < 250), e.map(v => v.toFixed(0)).join(' / '));
    if (outDir) await p.screenshot({ path: path.join(outDir, `orbit-${t}.png`) });
  }
  /* the outside view */
  await p.evaluate(() => { __orbit.setTime(13700); (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)); });   /* hold B */
  await p.waitForFunction(() => __orbit.OUT.on, null, { timeout: 30000 });
  await frames(30);
  const eo = await eyes();
  ok('outside view: both eyes drawn', eo.every(v => v > 1 && v < 250), eo.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'orbit-outside.png') });
  await p.evaluate(() => { (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)); });
  /* the S-IVB goes away after the LM is pulled free */
  const sv = await p.evaluate(async () => {
    const vis = async t => { __orbit.setTime(t); await new Promise(r => { __orbit.afterFrame = r; }); await new Promise(r => { __orbit.afterFrame = r; }); return __orbit.vehicle.parts.sivb.visible; };
    return [await vis(13000), await vis(17200), await vis(17100 + 6000)];
  });
  ok('S-IVB: with us, then drifting off, then gone', sv[0] && sv[1] && !sv[2], JSON.stringify(sv));
  /* the end */
  const end = await p.evaluate(async () => { __orbit.S.running = true; __orbit.setTime(__orbit.END - 3); await new Promise(r => setTimeout(r, 6000)); return { ended: __orbit.S.ended, t: __orbit.S.t, END: __orbit.END }; });
  ok('the chapter ends', end.ended, JSON.stringify(end));
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
