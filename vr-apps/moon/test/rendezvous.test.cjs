/* Chapter 8 (the rendezvous and the docking) in an emulated Meta Quest 3 (IWER) with Playwright and the installed
 * Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/rendezvous.test.cjs [folder for screenshots]
 * Enters VR standing in the ascent stage; checks America far off as a point of light and near as a ship, fast forward
 * dropping to real time for the docking, the docking itself (hard dock, the ascent stage alone), the outside view,
 * the tunnel opening and the word about it, the way up it facing the way you go, sitting down in the Command Module,
 * the chapter's end, a second start on the same page, and no errors. Waits on the state, never on time. */
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
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/rendezvous.html');
  await p.waitForFunction(() => window.__rdv && __rdv.S.ready, null, { timeout: 180000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.click('#vr');
  await p.waitForFunction(() => __rdv.renderer.xr.isPresenting && __rdv.SEAT.done, null, { timeout: 60000 });
  const frames = n => p.evaluate(n => new Promise(r => { const s = __rdv.S.frames; const t = () => (__rdv.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  const eyes = () => p.evaluate(() => new Promise(res => { __rdv.afterFrame = () => {
    const c = __rdv.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
    res(out);
  }; }));
  const tapA = () => p.evaluate(async () => { __dev.controllers.right.updateButtonValue('a-button', 1); await new Promise(r => setTimeout(r, 350)); __dev.controllers.right.updateButtonValue('a-button', 0); });
  /* the eye in the LM's frame and in the Command Module's cabin frame; which way you face in the cabin */
  const where = () => p.evaluate(() => new Promise(r => { __rdv.afterFrame = () => { const o = __rdv, V = o.rig.position.constructor, e = new V().setFromMatrixPosition(o.camera.matrixWorld);
    const lm = e.clone().applyMatrix4(o.lmc.group.matrix.clone().invert()), cm = e.clone().applyMatrix4(o.cabin.group.parent.matrix.clone().invert());
    const f = new V(0, 0, -1).applyQuaternion(o.rig.quaternion).transformDirection(o.cabin.group.parent.matrix.clone().invert());
    r({ mode: o.S.mode, lm: [lm.x, lm.y, lm.z].map(v => +v.toFixed(2)), cm: [cm.x, cm.y, cm.z].map(v => +v.toFixed(2)), faceZ: +f.z.toFixed(2), range: +o.range.toFixed(1), hardK: o.S.hardK }); }; }));
  await p.evaluate(() => { __rdv.S.running = false; __rdv.setTime(__rdv.START); __rdv.standInLM(); });
  await frames(5);
  const far = await where(), farV = await p.evaluate(() => ({ star: __rdv.star.visible, csm: __rdv.vehicle.parts.csm.visible, alt: __rdv.w.alt, speed: __rdv.w.speed }));
  ok('VR session runs: you stand in the ascent stage, in orbit', far.mode === 'lm' && Math.abs(far.lm[0] - 0.45) < 0.15 && far.lm[1] > 3.6 && far.lm[1] < 4.3 && Math.abs(farV.alt - 113000) < 2000 && Math.abs(farV.speed - 1628) < 30, JSON.stringify(far) + ' ' + JSON.stringify(farV));
  ok('America is a point of light, 220 km ahead', far.range > 150000 && far.range < 300000 && farV.star && !farV.csm, (far.range / 1000).toFixed(0) + ' km');
  await p.waitForFunction(() => __rdv.radio && __rdv.radio.lines.every(l => l.dur > 0), null, { timeout: 60000 });
  ok('voices loaded', await p.evaluate(() => __rdv.radio.lines.length) >= 16);
  /* the ascent stage alone: no legs, no deflectors, cut along its own deck */
  const stage = await p.evaluate(() => new Promise(r => { __rdv.afterFrame = () => { const o = __rdv, V = o.rig.position.constructor, up = new V(0, 1, 0).transformDirection(o.lmUp.matrixWorld);
    r({ legs: o.lmUp.userData.deployed.visible, top: o.lmUp.userData.descentTop.visible, bell: o.lmUp.userData.ascentOnly.visible, cut: +o.clipUp.normal.dot(up).toFixed(4), at: +o.clipUp.distanceToPoint(o.lmUp.localToWorld(new V(0, 2.39, 0))).toFixed(3) }); }; }));
  ok('the LM is the ascent stage alone, cut along its own deck', !stage.legs && !stage.top && stage.bell && stage.cut > 0.9999 && Math.abs(stage.at) < 0.01, JSON.stringify(stage));
  /* fast forward: free on the way in, real time as the docking comes up */
  await p.evaluate(() => { const o = __rdv; o.setTime(o.DOCK - 660); o.S.running = true; o.clock.faster(o.S); o.clock.faster(o.S); });          /* (after the last line said on the way in: the clock also drops to real time for each of those) */
  const w0 = await p.evaluate(() => __rdv.S.warp);
  await p.waitForFunction(() => __rdv.S.t > __rdv.DOCK - 418, null, { timeout: 600000 });
  const w1 = await p.evaluate(() => __rdv.S.warp);
  ok('fast forward on the way in; real time as the docking comes up', w0 >= 100 && w1 === 1, `x${w0} -> x${w1}`);
  /* America before the windows */
  await p.evaluate(() => { const o = __rdv; o.setTime(o.DOCK - 335); o.S.running = false; });
  await frames(5);
  const near = await where(), nearV = await p.evaluate(() => ({ star: __rdv.star.visible, csm: __rdv.vehicle.parts.csm.visible, rel: __rdv.csmRel(__rdv.DOCK - 335, new __rdv.rig.position.constructor())[0].toArray().map(v => +v.toFixed(1)) }));
  ok('America stands before the windows, a ship', near.range > 25 && near.range < 60 && nearV.csm && nearV.rel[2] > 20, JSON.stringify(near.range) + ' m ' + JSON.stringify(nearV));
  const e0 = await eyes();
  ok('America before the windows: both eyes drawn', e0.every(v => v > 4 && v < 250), e0.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'rendezvous-near.png') });
  /* the docking */
  await p.evaluate(() => { const o = __rdv; o.setTime(o.DOCK - 6); o.S.running = true; });
  await p.waitForFunction(() => __rdv.S.t > __rdv.DOCK + 2, null, { timeout: 600000 });
  const dock = await where(), dockV = await p.evaluate(() => ({ cue: !!__rdv.S.cues.dock, tunnel: __rdv.S.t >= __rdv.OPEN }));
  ok('the docking: America down on the drogue, hard docked', dock.range < 0.05 && dock.hardK === 1 && dockV.cue && !dockV.tunnel, JSON.stringify(dock) + ' ' + JSON.stringify(dockV));
  /* outside (Y) */
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => __rdv.OUT.on, null, { timeout: 60000 });
  await frames(6);
  const eo = await eyes();
  ok('outside view: the docked pair, both eyes drawn', eo.every(v => v > 1 && v < 250), eo.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'rendezvous-docked.png') });
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => !__rdv.OUT.on, null, { timeout: 60000 });
  /* the tunnel is opened; you are told the way */
  await p.evaluate(() => { const o = __rdv; o.setTime(o.OPEN - 2); o.S.running = true; });
  await p.waitForFunction(() => __rdv.S.cues.open && /Up through the tunnel|up through the tunnel/.test(__rdv.toast.text || ''), null, { timeout: 600000 });
  const open = await p.evaluate(() => ({ cue: !!__rdv.S.cues.open, said: __rdv.toast.text, warp: __rdv.S.warp, now: __rdv.info().now }));
  ok('the tunnel opens, and you are told the way', open.cue && /A/.test(open.said) && open.warp === 1 && /tunnel/.test(open.now), JSON.stringify(open));
  /* A: up into the tunnel, facing the way you go; it floats you to your couch */
  await tapA();
  await p.waitForFunction(() => __rdv.S.mode === 'crawl' && __rdv.FADE.k < 0.3, null, { timeout: 600000 });
  await frames(3);
  const inT = await where(), zH = await p.evaluate(() => __rdv.zH);
  ok('up into the tunnel, facing the way you go', inT.mode === 'crawl' && inT.faceZ > 0.9 && inT.cm[2] > zH - 0.2 && inT.cm[2] < -1.5 && Math.hypot(inT.cm[0] - 0.6, inT.cm[1] + 0.15) < 0.35, JSON.stringify(inT) + ' zH ' + zH.toFixed(2));
  const e1 = await eyes();
  ok('in the tunnel: both eyes drawn', e1.every(v => v > 4 && v < 250), e1.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'rendezvous-tunnel.png') });
  await p.waitForFunction(() => __rdv.S.mode === 'cm' && __rdv.FADE.k < 0.3 && !__rdv.SEAT.pending, null, { timeout: 900000 });
  await frames(4);
  const home = await where(), homeV = await p.evaluate(() => ({ seated: __rdv.S.seatedAt >= 0, now: __rdv.info().now }));
  ok('in your couch in the Command Module again', home.mode === 'cm' && Math.hypot(home.cm[0], home.cm[1], home.cm[2]) < 0.45 && home.faceZ < -0.9 && homeV.seated && /America/.test(homeV.now), JSON.stringify(home) + ' ' + JSON.stringify(homeV));
  const e2 = await eyes();
  ok('in the Command Module: both eyes drawn', e2.every(v => v > 4 && v < 250), e2.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'rendezvous-home.png') });
  /* the end */
  await p.waitForFunction(() => __rdv.S.ended, null, { timeout: 900000 });
  ok('the chapter ends at home in America', await p.evaluate(() => /End of chapter 8/.test(__rdv.info().alert)));
  /* started again on the same page: standing in the LM, the ships as at the start */
  await p.evaluate(() => { document.getElementById('start').value = String(__rdv.START); __rdv.readOptions(); __rdv.S.running = false; });
  await frames(6);
  const again = await where();
  ok('started again on the same page: in the LM, America far off', again.mode === 'lm' && again.range > 150000 && again.hardK === 1 && Math.abs(again.lm[0] - 0.45) < 0.2, JSON.stringify(again));
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
