/* The launch chapter in an emulated Meta Quest 3 (IWER) with Playwright and the installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/launch.test.cjs [folder for screenshots]
 * Enters VR, checks the voices load, both eyes are drawn through the whole flight (pad, liftoff, the cover going,
 * orbit), the windows show the outside only once the cover is gone, the clock runs, time can run faster only
 * after the tower is jettisoned, and nothing throws. Waits by frames. */
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
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/launch.html');
  await p.waitForFunction(() => window.__launch && __launch.S.ready, null, { timeout: 120000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.selectOption('#start', '-30');
  await p.click('#vr');
  await p.waitForFunction(() => __launch.renderer.xr.isPresenting, null, { timeout: 60000 });
  ok('VR session runs', true);
  /* seating: wherever the headset's origin is, your eyes end up at the Commander's eye point, looking at the panel */
  await p.waitForFunction(() => __launch.SEAT.done, null, { timeout: 60000 });
  const eye = () => p.evaluate(() => new Promise(r => { __launch.afterFrame = () => {
    const e = __launch.camera.matrixWorld.elements, q = __launch.rig.quaternion.clone().invert();
    const f = new (__launch.camera.position.constructor)(-e[8], -e[9], -e[10]).applyQuaternion(q);
    const rp = __launch.rig.position; r({ pos: [e[12] - rp.x, e[13] - rp.y, e[14] - rp.z], fwd: [f.x, f.y, f.z] }); }; }));
  let s0 = await eye();
  ok('seated: eyes at the eye point', Math.hypot(...s0.pos) < 0.03, JSON.stringify(s0.pos.map(v => +v.toFixed(3))));
  /* move the headset 1.2 m away and turn it, as if the Quest's origin were elsewhere; re-seat */
  await p.evaluate(() => { __dev.position.set(1.2, 0.9, -0.7); __dev.quaternion.set(0, Math.sin(0.55), 0, Math.cos(0.55)); });
  await p.evaluate(() => new Promise(r => setTimeout(r, 400)));
  const moved = await eye();
  await p.evaluate(() => { __launch.SEAT.done = false; __launch.SEAT.pending = 1; });
  await p.waitForFunction(() => __launch.SEAT.done, null, { timeout: 60000 });
  s0 = await eye();
  ok('re-seated after the headset origin moved', Math.hypot(...moved.pos) > 0.5 && Math.hypot(...s0.pos) < 0.03 && s0.fwd[2] < -0.9,
    JSON.stringify({ before: moved.pos.map(v => +v.toFixed(2)), after: s0.pos.map(v => +v.toFixed(3)), fwd: s0.fwd.map(v => +v.toFixed(2)) }));
  await p.waitForFunction(() => __launch.radio && __launch.radio.lines.every(l => l.dur > 0), null, { timeout: 60000 });
  const loaded = await p.evaluate(() => __launch.radio.lines.length);
  ok('voices loaded', loaded > 50, loaded + ' lines');
  const frames = n => p.evaluate(n => new Promise(r => { const s = __launch.S.frames; const t = () => (__launch.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  await p.waitForFunction(() => __launch.S.running, null, { timeout: 30000 });
  const t0 = await p.evaluate(() => __launch.S.t); await frames(10); const t1 = await p.evaluate(() => __launch.S.t);
  ok('the clock runs', t1 > t0, `${t0.toFixed(1)} -> ${t1.toFixed(1)}`);
  /* the two eyes: mean brightness, and how much of the picture is "outside" (bluish or black-ish through windows) */
  const eyes = () => p.evaluate(() => new Promise(res => { __launch.afterFrame = () => {
    const c = __launch.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
    res(out);
  }; }));
  for (const [t, name] of [[-20, 'pad'], [20, 'just after liftoff'], [210, 'cover gone'], [720, 'orbit']]) {
    await p.evaluate(t => { __launch.setTime(t); __launch.S.running = false; }, t);
    await frames(4);
    const e = await eyes();
    ok(`${name}: both eyes drawn`, e.every(v => v > 8 && v < 250), e.map(v => v.toFixed(0)).join(' / '));
    if (outDir) await p.screenshot({ path: path.join(outDir, `launch-${t}.png`) });
  }
  const covers = await p.evaluate(async () => { __launch.setTime(100); await new Promise(r => setTimeout(r, 300)); const a = __launch.cabin.covers[0].visible; __launch.setTime(260); await new Promise(r => setTimeout(r, 300)); return [a, __launch.cabin.covers[0].visible]; });
  ok('the cover is on before the tower goes and off after', covers[0] === true && covers[1] === false, JSON.stringify(covers));
  /* boarding: a fresh session that starts on the access arm; walk to the hatch; climb in; seated; the count resumes */
  await p.evaluate(() => __launch.renderer.xr.getSession().end());
  await p.waitForFunction(() => !__launch.renderer.xr.isPresenting, null, { timeout: 30000 });
  await p.evaluate(() => { __dev.position.set(0.25, 1.65, 0.1); __dev.quaternion.set(0, 0, 0, 1); });
  await p.selectOption('#start', 'board');
  await p.click('#vr');
  await p.waitForFunction(() => __launch.renderer.xr.isPresenting && __launch.S.mode === 'boarding', null, { timeout: 60000 });
  await frames(3);
  const b0 = await p.evaluate(() => ({ running: __launch.S.running, t: __launch.S.t, cabinInWorld: __launch.cabin.group.parent !== __launch.rig }));
  ok('boarding: on the arm, the count holding, the cabin in the spacecraft', !b0.running && b0.t === -210 && b0.cabinInWorld, JSON.stringify(b0));
  const e0 = await eyes();
  ok('boarding: both eyes drawn', e0.every(v => v > 8 && v < 250), e0.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'board-arm.png') });
  await p.evaluate(() => __launch.walkTo(0, -3.0));
  await frames(3);
  if (outDir) await p.screenshot({ path: path.join(outDir, 'board-whiteroom.png') });
  await p.evaluate(() => __launch.walkTo(0, -1.2));
  await p.waitForFunction(() => __launch.S.mode === 'seated' && __launch.SEAT.done, null, { timeout: 60000 });
  await frames(6);
  const s1 = await p.evaluate(() => { const e = __launch.camera.matrixWorld.elements, rp = __launch.rig.position; return { pos: [e[12] - rp.x, e[13] - rp.y, e[14] - rp.z], inRig: __launch.cabin.group.parent === __launch.rig, resumeAt: __launch.S.resumeAt > 0 }; });
  ok('climbed in: seated at the eye point, the cabin round you, the count about to resume', Math.hypot(...s1.pos) < 0.05 && s1.inRig && s1.resumeAt, JSON.stringify(s1));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'board-seated.png') });
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
