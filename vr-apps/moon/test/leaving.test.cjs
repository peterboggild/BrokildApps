/* Chapter 9 (leaving the Moon) in an emulated Meta Quest 3 (IWER) with Playwright and the installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/leaving.test.cjs [folder for screenshots]
 * Enters VR in the couch; checks the flight's figures (the burn's gain, the loss and return of the signal), Challenger
 * as the ascent stage alone and its casting off, the outside view, the burn behind the Moon (nose along the motion,
 * the engine, out of touch), the signal's return with the Earth in your window, the Moon falling away to a whole
 * disc, the chapter's end, and no errors. Waits on the state, never on time. */
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
  p.on('pageerror', e => { errs.push(e.message); console.log('PAGE ERROR', e.message); });
  p.on('console', m => { if (m.type() === 'error' && !/favicon|404/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript({ content: fs.readFileSync(IWER, 'utf8') + ';window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.stereoEnabled = true; window.__dev.installRuntime({ forceInstall: true });' });
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/leaving.html');
  await p.waitForFunction(() => window.__leaving && __leaving.S.ready, null, { timeout: 180000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.click('#vr');
  await p.waitForFunction(() => __leaving.renderer.xr.isPresenting && __leaving.SEAT.done, null, { timeout: 60000 });
  const frames = n => p.evaluate(n => new Promise(r => { const s = __leaving.S.frames; const t = () => (__leaving.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  const eyes = () => p.evaluate(() => new Promise(res => { __leaving.afterFrame = () => {
    const c = __leaving.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
    res(out);
  }; }));
  /* the state of things after a frame */
  const now = () => p.evaluate(() => new Promise(r => { __leaving.afterFrame = () => { const o = __leaving, w = o.w, V = o.rig.position.constructor;
    const nose = new V(0, 0, -1).applyQuaternion(o.rig.quaternion), vel = new V().fromArray(w.v).normalize();
    r({ t: o.S.t, alt: Math.round(w.alt), speed: Math.round(w.speed), hidden: w.hidden, engine: w.engine, noseVel: +nose.dot(vel).toFixed(2), gone: +o.lmGone.toFixed(1), lm: o.vehicle.lm.visible, warp: o.S.warp, now: o.info().now }); }; }));
  await p.evaluate(() => { __leaving.S.running = false; __leaving.setTime(__leaving.START); });
  await frames(5);
  const st0 = await now();
  ok('VR session runs: in your couch, in lunar orbit, Challenger docked', Math.abs(st0.alt - 113000) < 2000 && Math.abs(st0.speed - 1628) < 30 && st0.gone === 0 && /docked/.test(st0.now), JSON.stringify(st0));
  await p.waitForFunction(() => __leaving.radio && __leaving.radio.lines.every(l => l.dur > 0), null, { timeout: 60000 });
  ok('voices loaded', await p.evaluate(() => __leaving.radio.lines.length) >= 17);
  const fl = await p.evaluate(() => { const o = __leaving, i = o.L.info(), E = o.L.EV; return { gain: i.gain, vinf: i.vinf, los: (E.tei - E.los) / 60, aos: (E.aos - E.teiEnd) / 60, far: i.after90 / 1000, holds: o.HOLD.length }; });
  ok('the burn gains what Apollo 17\'s did, and sends you clear of the Moon', Math.abs(fl.gain - 928) < 3 && fl.vinf > 800 && fl.vinf < 1400, `${fl.gain.toFixed(0)} m/s, ${fl.vinf.toFixed(0)} m/s left at a distance`);
  ok('it is made behind the Moon: the signal lost before it, back after it', fl.los > 10 && fl.los < 35 && fl.aos > 3 && fl.aos < 20 && fl.holds === 4, `lost ${fl.los.toFixed(0)} min before, back ${fl.aos.toFixed(0)} min after`);
  /* Challenger: the ascent stage alone */
  const stage = await p.evaluate(() => new Promise(r => { __leaving.afterFrame = () => { const o = __leaving, V = o.rig.position.constructor, up = new V(0, 1, 0).transformDirection(o.lmUp.matrixWorld);
    r({ legs: o.lmUp.userData.deployed.visible, top: o.lmUp.userData.descentTop.visible, bell: o.lmUp.userData.ascentOnly.visible, cut: +o.clipUp.normal.dot(up).toFixed(4) }); }; }));
  ok('Challenger is the ascent stage alone', !stage.legs && !stage.top && stage.bell && stage.cut > 0.9999, JSON.stringify(stage));
  /* cast off */
  await p.evaluate(() => { const o = __leaving; o.setTime(o.GETT.jett - 3); o.S.running = true; });
  await p.waitForFunction(() => __leaving.S.t > __leaving.GETT.jett + 6, null, { timeout: 600000 });
  const off = await now(), offV = await p.evaluate(() => !!__leaving.S.cues.jett);
  ok('Challenger is cast off and drifts away', off.gone > 3 && off.gone < 30 && offV && /last days/.test(off.now), JSON.stringify(off));
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => __leaving.OUT.on, null, { timeout: 60000 });
  await frames(6);
  const eo = await eyes();
  ok('outside view: both eyes drawn', eo.every(v => v > 1 && v < 250), eo.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'leaving-castoff.png') });
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => !__leaving.OUT.on, null, { timeout: 60000 });
  /* the burn for home, behind the Moon */
  await p.evaluate(() => { const o = __leaving; o.setTime(o.GETT.tei - 3); o.S.running = true; });
  await p.waitForFunction(() => __leaving.S.t > __leaving.GETT.tei + 5, null, { timeout: 600000 });
  const burn = await now(), burnV = await p.evaluate(() => ({ cue: !!__leaving.S.cues.tei, radio: __leaving.info().rows[2][1] }));
  ok('the burn: the engine, nose along the motion, out of touch behind the Moon', burn.engine === 1 && burn.noseVel > 0.95 && burn.hidden && burnV.cue && /LOS/.test(burnV.radio) && burn.lm === false, JSON.stringify(burn) + ' ' + JSON.stringify(burnV));
  const e1 = await eyes();
  ok('in the burn: both eyes drawn', e1.every(v => v > 4 && v < 250), e1.map(v => v.toFixed(0)).join(' / '));
  await p.evaluate(() => { const o = __leaving; o.setTime(o.GETT.teiEnd - 3); o.S.running = true; });
  await p.waitForFunction(() => __leaving.S.t > __leaving.GETT.teiEnd + 2, null, { timeout: 600000 });
  const done = await now();
  ok('shutdown: 928 m/s faster, climbing away', done.engine === 0 && Math.abs(done.speed - (1628 + 928)) < 25 && /homeward/.test(done.now), JSON.stringify(done));
  /* the signal comes back; the Earth is in your window */
  await p.evaluate(() => { const o = __leaving; o.setTime(o.L.EV.aos - 3); o.S.running = true; });
  await p.waitForFunction(() => __leaving.S.cues.aos, null, { timeout: 600000 });
  const aos = await p.evaluate(() => new Promise(r => { __leaving.afterFrame = () => { const o = __leaving, V = o.rig.position.constructor, E = new V().fromArray(o.L.EARTH);
    const win = new V(-0.863, -0.15, -0.25).normalize().applyQuaternion(o.att || o.rig.quaternion); r({ hidden: o.w.hidden, earthInWindow: +win.dot(E).toFixed(2), warp: o.S.warp }); }; }));
  ok('the signal comes back; your window is on the Earth', !aos.hidden && aos.earthInWindow > 0.9, JSON.stringify(aos));
  const e2 = await eyes();
  ok('at Earthrise: both eyes drawn', e2.every(v => v > 4 && v < 250), e2.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'leaving-earthrise.png') });
  /* the Moon falls away to a whole disc; the end */
  await p.evaluate(() => { const o = __leaving; o.setTime(o.END - 4); o.S.running = true; });
  await p.waitForFunction(() => __leaving.S.ended, null, { timeout: 600000 });
  const end = await now(), disc = await p.evaluate(() => Math.asin(__leaving.L.R / __leaving.w.rC) * 114.6);
  ok('the chapter ends with the whole Moon behind you', end.alt > 5e6 && disc < 30 && /End of chapter 9/.test(await p.evaluate(() => __leaving.info().alert)), `${(end.alt / 1000).toFixed(0)} km up, the Moon ${disc.toFixed(0)} degrees across`);
  if (outDir) await p.screenshot({ path: path.join(outDir, 'leaving-end.png') });
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
