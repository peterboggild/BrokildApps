/* Chapter 3 (lunar orbit) in an emulated Meta Quest 3 (IWER) with Playwright and the installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/lunar.test.cjs [folder for screenshots]
 * Enters VR, checks seating, the voices, the clock, the warp (stopping at the burn, x10 at most there), the orbits
 * (LOI 315 by 97 km, a descent orbit low point of 15 km before the site), the radio silence behind the Moon, the
 * attitude never turning faster than 1.5 degrees a second, both eyes drawn through every stretch, the Earth in your
 * window at Earthrise, the outside view, the chapter's end, and no errors. */
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
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/lunar.html');
  await p.waitForFunction(() => window.__lunar && __lunar.S.ready, null, { timeout: 180000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.selectOption('#start', '258760');
  await p.click('#vr');
  await p.waitForFunction(() => __lunar.renderer.xr.isPresenting, null, { timeout: 60000 });
  ok('VR session runs', true);
  await p.waitForFunction(() => __lunar.SEAT.done, null, { timeout: 60000 });
  const e0 = await p.evaluate(() => new Promise(r => { __lunar.afterFrame = () => { const e = __lunar.camera.matrixWorld.elements, rp = __lunar.rig.position; r([e[12] - rp.x, e[13] - rp.y, e[14] - rp.z]); }; }));
  ok('seated: eyes at the eye point', Math.hypot(...e0) < 0.03, JSON.stringify(e0.map(v => +v.toFixed(3))));
  await p.waitForFunction(() => __lunar.radio && __lunar.radio.lines.every(l => l.dur > 0), null, { timeout: 60000 });
  ok('voices loaded', await p.evaluate(() => __lunar.radio.lines.length) >= 40);
  const frames = n => p.evaluate(n => new Promise(r => { const s = __lunar.S.frames; const t = () => (__lunar.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  await p.waitForFunction(() => __lunar.S.running, null, { timeout: 30000 });
  const t0 = await p.evaluate(() => __lunar.S.t); await frames(10); const t1 = await p.evaluate(() => __lunar.S.t);
  ok('the clock runs', t1 > t0, `${t0.toFixed(1)} -> ${t1.toFixed(1)}`);
  /* the flight */
  const fl = await p.evaluate(() => { const L = __lunar.L, i = L.info(); let mn = 1e9, at = 0; for (let t = L.EV.doiEnd; t < L.EV.doiEnd + 7000; t += 10) { const w = L.where(t); if (w.alt < mn) { mn = w.alt; at = w.angle; } }
    return { loi: i.loi, low: mn, lowAngle: at * 180 / Math.PI, loiHidden: L.where(L.EV.loi).hidden, doiHidden: L.where(L.EV.doi).hidden, startDist: L.where(L.EV.soi).rC }; });
  ok('LOI orbit about 315 by 97 km', Math.abs(fl.loi[1] - 315) < 3 && Math.abs(fl.loi[0] - 97) < 5, fl.loi.map(v => v.toFixed(1)).join(' by '));
  ok('descent orbit: 15 km, eight degrees before the site', Math.abs(fl.low / 1000 - 15) < 0.5 && Math.abs(fl.lowAngle + 8) < 1, (fl.low / 1000).toFixed(1) + ' km at ' + fl.lowAngle.toFixed(1) + ' deg');
  ok('both burns behind the Moon, out of touch', fl.loiHidden && fl.doiHidden);
  ok('starts where chapter 2 ends', Math.abs(fl.startDist - 66000e3) < 1e5, (fl.startDist / 1e6).toFixed(1) + ' thousand km');
  /* the warp stops at the burn and allows x10 at most there */
  const wp = await p.evaluate(async () => {
    __lunar.settings.set('ff', 'major');                 /* events only (the default stops for the radio too) */
    __lunar.setTime(319650 - 300); __lunar.S.warp = 1000;
    await new Promise(r => setTimeout(r, 2500));
    const a = { t: __lunar.S.t, warp: __lunar.S.warp };
    __lunar.S.warp = 1000; await new Promise(r => setTimeout(r, 300));
    return [a, __lunar.S.warp];
  });
  ok('time warp stops before the burn, x10 at most there', wp[0].warp === 1 && wp[0].t >= 319650 && wp[0].t < 319700 && wp[1] <= 10, JSON.stringify(wp));
  /* comfort: the attitude as you see it turns no faster than 1.5 deg/s, even at x10,000 in orbit */
  const rate = await p.evaluate(async () => {
    __lunar.setTime(324500); await new Promise(r => { __lunar.afterFrame = r; });
    __lunar.S.warp = 10000; let worst = 0;
    for (let i = 0; i < 30; i++) {
      const q0 = __lunar.att.clone(), s0 = performance.now();
      await new Promise(r => { __lunar.afterFrame = r; });
      const dt = (performance.now() - s0) / 1000; worst = Math.max(worst, q0.angleTo(__lunar.att) / Math.max(dt, 0.1) * 57.3);
    }
    __lunar.S.warp = 1; return worst;
  });
  ok('attitude never turns faster than 1.5 deg/s', rate <= 1.6, rate.toFixed(2) + ' deg/s at most');
  /* both eyes, through every stretch */
  const eyes = () => p.evaluate(() => new Promise(res => { __lunar.afterFrame = () => {
    const c = __lunar.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
    res(out);
  }; }));
  for (const [t, name] of [[258800, 'entering lunar space'], [300000, 'the Moon close'], [320200, 'LOI burn'], [321930, 'Earthrise'], [335310, 'DOI burn'], [338719, 'the low pass']]) {
    await p.evaluate(t => { __lunar.setTime(t); __lunar.S.running = false; }, t);
    await frames(4);
    const e = await eyes();
    ok(`${name}: both eyes drawn`, e.every(v => v > 4 && v < 250), e.map(v => v.toFixed(0)).join(' / '));
    if (outDir) await p.screenshot({ path: path.join(outDir, `lunar-${t}.png`) });
  }
  /* Earthrise: the Earth in front of the window beside you */
  const er = await p.evaluate(async () => {
    __lunar.setTime(321935); await new Promise(r => { __lunar.afterFrame = r; });
    const L = __lunar.L, V = __lunar.camera.position.constructor, w = L.where(321935);
    const earth = new V().fromArray(L.EARTH), win = new V(-0.863, -0.15, -0.25).normalize().applyQuaternion(__lunar.att);
    return { angle: Math.acos(earth.dot(win)) * 57.3, aboveHorizon: Math.asin(earth.dot(new V().fromArray(w.up))) * 57.3 + Math.acos(L.R / w.rC) * 57.3 };
  });
  ok('Earthrise in your window', er.angle < 15 && er.aboveHorizon > 0, `${er.angle.toFixed(1)} deg from the window's line of sight, ${er.aboveHorizon.toFixed(1)} deg above the horizon`);
  /* the outside view */
  await p.evaluate(() => { __lunar.setTime(338719); (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)); });
  await p.waitForFunction(() => __lunar.OUT.on, null, { timeout: 30000 });
  await frames(30);
  const eo = await eyes();
  ok('outside view: both eyes drawn', eo.every(v => v > 4 && v < 250), eo.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'lunar-outside.png') });
  await p.evaluate(() => { (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)); });
  /* the end */
  const end = await p.evaluate(async () => { __lunar.S.running = true; __lunar.setTime(__lunar.END - 3); await new Promise(r => setTimeout(r, 6000)); return { ended: __lunar.S.ended, t: __lunar.S.t }; });
  ok('the chapter ends', end.ended, JSON.stringify(end));
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
