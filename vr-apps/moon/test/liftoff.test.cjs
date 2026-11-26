/* Chapter 7 (the lift-off) in an emulated Meta Quest 3 (IWER) with Playwright and the installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/liftoff.test.cjs [folder for screenshots]
 * Enters VR standing in the LM on the ground; checks the flight's figures against Apollo 17's ascent, the two stages
 * (the descent stage left where it stood, the cut along the LM's own deck), their shadows, the straight rise and the
 * pitch-over face down, fast forward through the long burn, the cut-off in orbit, the outside view, the chapter's
 * end, and no errors. Waits on the state, never on time: the emulator runs at a few frames a second. */
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
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/liftoff.html');
  await p.waitForFunction(() => window.__liftoff && __liftoff.S.ready, null, { timeout: 180000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.click('#vr');
  await p.waitForFunction(() => __liftoff.renderer.xr.isPresenting && __liftoff.SEAT.done, null, { timeout: 60000 });
  const frames = n => p.evaluate(n => new Promise(r => { const s = __liftoff.S.frames; const t = () => (__liftoff.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  const eyes = () => p.evaluate(() => new Promise(res => { __liftoff.afterFrame = () => {
    const c = __liftoff.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
    res(out);
  }; }));
  /* the LM's axes against the local vertical, and where you stand in it */
  const pose = () => p.evaluate(() => new Promise(r => { __liftoff.afterFrame = () => { const o = __liftoff, e = o.lmc.group.matrix.elements, u = o.D.upAt(o.cur.r), f = o.D.fwdAt(o.cur.r);
    const V = o.rig.position.constructor, eye = new V().setFromMatrixPosition(o.camera.matrixWorld).applyMatrix4(o.lmc.group.matrix.clone().invert());
    r({ t: +(o.S.t - o.GETA.lift).toFixed(1), h: +o.padH.toFixed(1), yUp: +(e[4] * u[0] + e[5] * u[1] + e[6] * u[2]).toFixed(3), zUp: +(e[8] * u[0] + e[9] * u[1] + e[10] * u[2]).toFixed(3), zFwd: +(e[8] * f[0] + e[9] * f[1] + e[10] * f[2]).toFixed(3),
      eye: [eye.x, eye.y, eye.z].map(v => +v.toFixed(2)), a: +Math.hypot(...o.cur.a).toFixed(2), warp: o.S.warp }); }; }));
  await p.evaluate(() => { __liftoff.S.running = false; __liftoff.restart(__liftoff.GETA.lift - 20); });
  await frames(4);
  const st0 = await pose();
  ok('VR session runs: you stand at the Commander\'s station, the LM on the ground', Math.abs(st0.h) < 0.3 && Math.abs(st0.eye[0] - 0.45) < 0.15 && st0.eye[1] > 3.6 && st0.eye[1] < 4.3 && st0.yUp > 0.99, JSON.stringify(st0));
  await p.waitForFunction(() => __liftoff.radio && __liftoff.radio.lines.every(l => l.dur > 0), null, { timeout: 60000 });
  ok('voices loaded', await p.evaluate(() => __liftoff.radio.lines.length) >= 16);
  /* the flight, against Apollo 17's ascent (441 s of burn, the pitch-over to about 52 degrees, a 9 by 48 nautical
   * mile orbit: 17 by 89 km) */
  const fl = await p.evaluate(() => { const i = __liftoff.D.info(); return { burn: i.burn, pitch: i.pitch0, alt: i.alt, speed: i.speed, climb: i.climb, peri: i.orbit.peri, apo: i.orbit.apo, first: i.firstA, max: i.maxA, down: i.downrange }; });
  ok('the burn lasts about as long as Apollo 17\'s', Math.abs(fl.burn - 441) < 15, fl.burn.toFixed(0) + ' s');
  ok('the pitch-over to about 50 degrees from the vertical', fl.pitch > 44 && fl.pitch < 58, fl.pitch.toFixed(1) + ' deg');
  ok('cut-off about 18 km up, level, at orbital speed', Math.abs(fl.alt - 18300) < 600 && Math.abs(fl.climb) < 6 && Math.abs(fl.speed - 1688) < 12, `${(fl.alt / 1000).toFixed(1)} km, ${fl.climb.toFixed(1)} m/s up, ${fl.speed.toFixed(0)} m/s`);
  ok('into an orbit of about 17 by 89 km', fl.peri > 12000 && fl.peri < 20000 && fl.apo > 80000 && fl.apo < 96000, `${(fl.peri / 1000).toFixed(1)} by ${(fl.apo / 1000).toFixed(1)} km, ${(fl.down / 1000).toFixed(0)} km downrange`);
  ok('the push of the ascent engine: a third of a g growing to under two thirds', fl.first > 2.9 && fl.first < 3.4 && fl.max > 5 && fl.max < 6.5, `${fl.first.toFixed(2)} to ${fl.max.toFixed(2)} m/s2`);
  /* standing: both stages drawn as one LM, each with its shadow; the cut along the LM's own deck */
  const stand = await p.evaluate(() => new Promise(r => { __liftoff.afterFrame = () => { const o = __liftoff, V = o.rig.position.constructor, up = new V(0, 1, 0).transformDirection(o.lmDown.matrixWorld);
    r({ down: o.lmDown.visible && o.lmDown.userData.deployed.visible && o.lmDown.userData.descentTop.visible && !o.lmDown.userData.ascentOnly.visible, upParts: !o.lmUp.userData.deployed.visible && !o.lmUp.userData.descentTop.visible && o.lmUp.userData.ascentOnly.visible,
      cutDown: +o.clipDown.normal.dot(up).toFixed(4), cutUp: +o.clipUp.normal.dot(up).toFixed(4), gap: +o.clipUp.distanceToPoint(o.lmDown.localToWorld(new V(0, 2.39, 0))).toFixed(3), sh: o.lmShadow.mesh.visible, shUp: o.nearG.children.filter(c => c.isMesh && c.visible && c.material.alphaMap).length }); }; }));
  ok('standing: the two stages are one LM, cut along its own deck, each with its shadow', stand.down && stand.upParts && stand.cutDown < -0.9999 && stand.cutUp > 0.9999 && Math.abs(stand.gap) < 0.01 && stand.sh && stand.shUp >= 2, JSON.stringify(stand));
  const e0 = await eyes();
  ok('in the cabin before lift-off: both eyes drawn', e0.every(v => v > 4 && v < 250), e0.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'liftoff-cabin.png') });
  /* ignition: straight up for ten seconds, the descent stage left where it stood */
  await p.evaluate(() => { __liftoff.restart(__liftoff.GETA.lift - 2); __liftoff.S.running = true; });
  await p.waitForFunction(() => __liftoff.S.t > __liftoff.GETA.lift + 6 && __liftoff.S.t < __liftoff.GETA.lift + 9.5, null, { timeout: 600000 });
  const up = await pose(), left = await p.evaluate(() => { const o = __liftoff; return { cue: !!o.S.cues.ignition, down: o.lmDown.position.toArray().map(v => +v.toFixed(2)), land: o.D.LAND.map(v => +v.toFixed(2)) }; });
  ok('ignition: straight up, the descent stage left behind', up.h > 20 && up.h < 80 && up.yUp > 0.995 && up.a > 3 && left.cue && Math.abs(left.down[1] - left.land[1]) < 0.01, JSON.stringify(up) + ' ' + JSON.stringify(left));
  /* the pitch-over: face down, the windows to the Moon */
  await p.waitForFunction(() => __liftoff.S.t > __liftoff.GETA.lift + 32, null, { timeout: 600000 });
  const over = await pose(), told = await p.evaluate(() => ({ ff: !!__liftoff.S.cues.ff, text: __liftoff.toast.text }));
  ok('the pitch-over: about 50 degrees over, the windows turned down to the Moon', over.yUp > 0.5 && over.yUp < 0.78 && over.zUp < -0.6 && over.zFwd > 0.4 && !!(await p.evaluate(() => __liftoff.S.cues.pitch)), JSON.stringify(over));
  ok('thirty seconds after lift-off you are told that fast forward is open', told.ff && /Fast forward enabled \(B\)/.test(told.text), JSON.stringify(told));
  const e1 = await eyes();
  ok('in the ascent: both eyes drawn', e1.every(v => v > 4 && v < 250), e1.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'liftoff-pitch.png') });
  /* the lift-off itself is real time; the long burn may be run faster */
  await p.evaluate(() => { const o = __liftoff; o.restart(o.GETA.lift - 28); o.S.running = true; o.clock.faster(o.S); });
  const w0 = await p.evaluate(() => __liftoff.S.warp);
  await p.waitForFunction(() => __liftoff.S.t > __liftoff.GETA.lift - 18, null, { timeout: 600000 });
  const held = { before: w0, at: await p.evaluate(() => __liftoff.S.warp) };
  await p.evaluate(() => { const o = __liftoff; o.restart(o.GETA.lift + 150); o.S.running = true; o.clock.faster(o.S); });
  const f0 = await p.evaluate(() => __liftoff.S.t); await frames(8);
  const fast = await p.evaluate(t0 => ({ warp: __liftoff.S.warp, went: +(__liftoff.S.t - t0).toFixed(1) }), f0);
  ok('fast forward drops to real time as the lift-off comes up; in the long burn one press of B is ten times', held.at === 1 && fast.warp === 10 && fast.went > 0.5, JSON.stringify({ held, fast }));
  /* the cut-off: in orbit */
  await p.evaluate(() => { const o = __liftoff; o.restart(o.GETA.insert - 6); o.S.running = true; });
  await p.waitForFunction(() => __liftoff.S.t > __liftoff.GETA.insert + 3, null, { timeout: 600000 });
  const cut = await pose(), orb = await p.evaluate(() => { const o = __liftoff, q = o.D.orbit(o.cur), i = o.info(); return { cue: !!o.S.cues.cutoff, peri: q.peri, apo: q.apo, now: i.now, row: i.rows[3].join(': ') }; });
  ok('the cut-off: the engine stops, 18 km up, in orbit', cut.a === 0 && cut.h > 15000 && cut.h < 22000 && orb.cue && orb.peri > 12000 && orb.apo > 80000 && orb.now === 'in orbit' && /Orbit/.test(orb.row), JSON.stringify(cut) + ' ' + JSON.stringify(orb));
  const e2 = await eyes();
  ok('in orbit: both eyes drawn', e2.every(v => v > 1 && v < 250), e2.map(v => v.toFixed(0)).join(' / '));
  /* outside (Y): the ascent stage alone */
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => __liftoff.OUT.on, null, { timeout: 60000 });
  await frames(6);
  const eo = await eyes(), outv = await p.evaluate(() => ({ up: __liftoff.lmUp.visible, cabin: __liftoff.lmc.group.visible }));
  ok('outside view: the ascent stage, both eyes drawn', eo.every(v => v > 0.5 && v < 250) && outv.up, eo.map(v => v.toFixed(1)).join(' / ') + ' ' + JSON.stringify(outv));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'liftoff-outside.png') });
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => !__liftoff.OUT.on, null, { timeout: 60000 });
  /* the end */
  await p.evaluate(() => { const o = __liftoff; o.S.t = o.END - 3; o.S.running = true; });
  await p.waitForFunction(() => __liftoff.S.ended, null, { timeout: 600000 });
  ok('the chapter ends in orbit', await p.evaluate(() => __liftoff.S.ended && /End of chapter 7/.test(__liftoff.info().alert)));
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
