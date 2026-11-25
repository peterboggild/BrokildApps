/* Chapter 5 (the descent) in an emulated Meta Quest 3 (IWER) with Playwright and the installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/descent.test.cjs [folder for screenshots]
 * Enters VR standing in the LM; checks the voices, the guided flight (PDI at 15 km, high gate, low gate, the
 * autopilot's contact on Apollo 17's minute, thrust within reach), the pitch-over, the warp stopping before high
 * gate, taking over with A and landing gently by hand, a hard contact sending you back to high gate, the
 * autopilot's landing with its calls, both eyes drawn from PDI to the ground, the outside view, the end, no errors. */
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
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/descent.html');
  await p.waitForFunction(() => window.__descent && __descent.S.ready, null, { timeout: 180000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.selectOption('#start', '396293');
  await p.click('#vr');
  await p.waitForFunction(() => __descent.renderer.xr.isPresenting && __descent.SEAT.done, null, { timeout: 60000 });
  ok('VR session runs, standing in the LM', true);
  await p.waitForFunction(() => __descent.radio && __descent.radio.lines.every(l => l.dur > 0), null, { timeout: 60000 });
  ok('voices loaded', await p.evaluate(() => __descent.radio.lines.length) >= 25);
  const frames = n => p.evaluate(n => new Promise(r => { const s = __descent.S.frames; const t = () => (__descent.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  /* the flight */
  const fl = await p.evaluate(() => { const D = __descent.D, G = __descent.GETD, i = D.info();
    const tilt = t => { const s = D.state(t), u = D.upAt(s.r), am = Math.hypot(...s.a); return Math.acos((s.a[0] * u[0] + s.a[1] * u[1] + s.a[2] * u[2]) / am) * 57.3; };
    return { contact: i.contact - G.land, maxA: i.maxA, pdi: D.padHeight(D.state(G.pdi)), hg: D.padHeight(D.state(G.hg)), lg: D.padHeight(D.state(G.lg)), before: tilt(G.hg - 15), after: tilt(G.hg + 5) }; });
  ok('PDI at 15 km', Math.abs(fl.pdi - 15000) < 1500, (fl.pdi / 1000).toFixed(1) + ' km');
  ok('high gate about 2.3 km, low gate about 150 m', Math.abs(fl.hg - 2300) < 400 && Math.abs(fl.lg - 150) < 40, `${fl.hg.toFixed(0)} m, ${fl.lg.toFixed(0)} m`);
  ok('the autopilot touches down on Apollo 17\'s minute', Math.abs(fl.contact) < 30, (fl.contact >= 0 ? '+' : '') + fl.contact.toFixed(1) + ' s');
  ok('thrust within reach of the descent engine', fl.maxA < 4.2, fl.maxA.toFixed(2) + ' m/s2 at most');
  ok('the pitch-over at high gate', fl.before - fl.after > 25, `${fl.before.toFixed(0)} -> ${fl.after.toFixed(0)} deg from vertical`);
  /* both eyes, from PDI to the ground */
  const eyes = () => p.evaluate(() => new Promise(res => { __descent.afterFrame = () => {
    const c = __descent.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
    res(out);
  }; }));
  for (const [dtt, name] of [[60, 'braking'], [535, 'after the pitch-over'], [635, 'low gate'], [720, 'the last 30 m']]) {
    await p.evaluate(dt => { __descent.restart(__descent.GETD.pdi + dt); __descent.S.running = false; }, dtt);
    await frames(4);
    const e = await eyes();
    ok(`${name}: both eyes drawn`, e.every(v => v > 4 && v < 250), e.map(v => v.toFixed(0)).join(' / '));
    if (outDir) await p.screenshot({ path: path.join(outDir, `descent-${dtt}.png`) });
  }
  /* the warp stops a minute before high gate */
  const wp = await p.evaluate(async () => { const O = __descent; O.restart(O.GETD.hg - 90); O.S.running = true; O.S.warp = 10; await new Promise(r => setTimeout(r, 4000)); return { warp: O.S.warp, before: O.GETD.hg - O.S.t }; });
  ok('time runs faster only until a minute before high gate', wp.warp === 1 && wp.before < 62, JSON.stringify(wp));
  /* by hand: A takes over below 150 m; a gentle descent lands */
  const hand = await p.evaluate(async () => {
    /* low already (the software renderer runs this at a few frames a second, so the flight is slow here) */
    const O = __descent; O.restart(O.GETD.over + 18); O.S.running = true;
    await new Promise(r => setTimeout(r, 600));
    __dev.controllers.right.updateButtonValue('a-button', 1); await new Promise(r => setTimeout(r, 200)); __dev.controllers.right.updateButtonValue('a-button', 0);
    await new Promise(r => setTimeout(r, 300));
    const mode = O.S.mode; O.S.rod = -2.4;
    const t0 = performance.now(); while (O.S.landed < 0 && performance.now() - t0 < 120000) await new Promise(r => setTimeout(r, 200));
    return { mode, landed: O.S.landed > 0, contact: O.S.contactV, restarts: O.S.restarts };
  });
  ok('A takes over (P66 by hand)', hand.mode === 'hand', hand.mode);
  ok('a gentle contact by hand lands', hand.landed && hand.restarts === 0, JSON.stringify(hand.contact && hand.contact.map(v => +v.toFixed(2))));
  /* too hard: back to high gate */
  const hard = await p.evaluate(async () => {
    const O = __descent; O.restart(O.GETD.over + 10); O.S.running = true; await new Promise(r => setTimeout(r, 400));
    O.toggleHand(); O.S.rod = -6;
    const t0 = performance.now(), r0 = O.S.restarts; while (O.S.restarts === r0 && performance.now() - t0 < 60000) await new Promise(r => setTimeout(r, 200));
    await new Promise(r => setTimeout(r, 500));
    return { restarts: O.S.restarts - r0, mode: O.S.mode, toHG: O.GETD.hg - O.S.t };
  });
  ok('a hard contact by hand: back to high gate', hard.restarts === 1 && hard.mode === 'table' && hard.toHG > -10 && hard.toHG < 10, JSON.stringify(hard));
  /* the autopilot's landing, with the calls */
  const auto = await p.evaluate(async () => {
    const O = __descent; O.restart(O.GETD.land - 5); O.S.running = true;
    const t0 = performance.now(); while (O.S.landed < 0 && performance.now() - t0 < 120000) await new Promise(r => setTimeout(r, 200));
    return { landed: O.S.landed > 0, cues: Object.keys(O.S.cues), padH: O.padH };
  });
  ok('the autopilot lands, contact light called', auto.landed && auto.cues.includes('contact') && Math.abs(auto.padH) < 0.2, JSON.stringify(auto));
  const e5 = await eyes();
  ok('on the ground: both eyes drawn', e5.every(v => v > 4 && v < 250), e5.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'descent-landed.png') });
  /* the outside view */
  await p.evaluate(() => __dev.controllers.right.updateButtonValue('b-button', 1));
  await p.waitForFunction(() => __descent.OUT.on, null, { timeout: 30000 });
  await frames(15);
  const eo = await eyes();
  ok('outside view: both eyes drawn', eo.every(v => v > 4 && v < 250), eo.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'descent-outside.png') });
  await p.evaluate(() => __dev.controllers.right.updateButtonValue('b-button', 0));
  /* the end */
  const end = await p.evaluate(async () => { const O = __descent; O.S.landed = O.S.t - 41; await new Promise(r => setTimeout(r, 1500)); return O.S.ended; });
  ok('the chapter ends after the landing', end);
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
