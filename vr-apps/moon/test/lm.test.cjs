/* Chapter 4 (into the LM) in an emulated Meta Quest 3 (IWER) with Playwright and the installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/lm.test.cjs [folder for screenshots]
 * Enters VR in the couch; checks the voices, the tunnel's place, the clock holding before undocking while you are
 * still in the CM, pulling yourself along with a grip (you move opposite the hand), staying inside the tunnel, the
 * float through to the LM and standing there, both eyes drawn, undocking (the CSM backing away, then in front of
 * the windows, nose towards you), the outside view, the chapter's end, and no errors. */
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
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/lm.html');
  await p.waitForFunction(() => window.__lm && __lm.S.ready, null, { timeout: 180000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.selectOption('#start', '384000');
  await p.click('#vr');
  await p.waitForFunction(() => __lm.renderer.xr.isPresenting && __lm.SEAT.done, null, { timeout: 60000 });
  ok('VR session runs, seated in the couch', await p.evaluate(() => __lm.S.mode === 'cm'));
  await p.waitForFunction(() => __lm.radio && __lm.radio.lines.every(l => l.dur > 0), null, { timeout: 60000 });
  ok('voices loaded', await p.evaluate(() => __lm.radio.lines.length) >= 20);
  const frames = n => p.evaluate(n => new Promise(r => { const s = __lm.S.frames; const t = () => (__lm.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  const zH = await p.evaluate(() => __lm.zH);
  ok('the tunnel runs from the cabin to the LM hatch', zH < -3 && zH > -3.8, 'LM ceiling at cabin z ' + zH.toFixed(2));
  /* the clock holds ten minutes before undocking while you are still in the CM */
  const hold = await p.evaluate(async () => { __lm.S.running = true; __lm.setTime(__lm.UNDOCK - 700); __lm.S.warp = 10000; await new Promise(r => setTimeout(r, 3000)); return { t: __lm.S.t, held: __lm.S.held, mode: __lm.S.mode }; });
  ok('the clock holds until you are in the LM', hold.held && Math.abs(hold.t - (await p.evaluate(() => __lm.UNDOCK - 600))) < 1 && hold.mode === 'cm', JSON.stringify(hold));
  await p.evaluate(() => __lm.setTime(384100));
  /* pulling yourself: grip, move the hand towards you, and you move away from it (towards the tunnel) */
  const pull = await p.evaluate(async () => {
    const c = __dev.controllers.right; c.position.set(0.25, 1.3, -0.5);
    await new Promise(r => setTimeout(r, 300));
    const b0 = __lm.body.clone();
    c.updateButtonValue('squeeze', 1); await new Promise(r => setTimeout(r, 300));
    for (let i = 1; i <= 10; i++) { c.position.set(0.25, 1.3, -0.5 + i * 0.03); await new Promise(r => setTimeout(r, 60)); }
    await new Promise(r => setTimeout(r, 300));
    c.updateButtonValue('squeeze', 0); await new Promise(r => setTimeout(r, 200));
    return { mode: __lm.S.mode, dz: __lm.body.z - b0.z };
  });
  ok('grip and pull: you move the other way', pull.mode === 'crawl' && pull.dz < -0.15, JSON.stringify(pull));
  /* staying inside: pushed far sideways, you stay within the cabin's cone */
  const inside = await p.evaluate(async () => { __lm.body.set(3, 0, -1.5); await new Promise(r => { __lm.afterFrame = r; }); await new Promise(r => { __lm.afterFrame = r; }); return Math.hypot(__lm.body.x - 0.6, __lm.body.y + 0.15); });
  ok('you stay inside the cabin', inside < 1.0, 'off the axis ' + inside.toFixed(2) + ' m');
  /* float through: A */
  await p.evaluate(async () => { __lm.body.set(0, 0, 0); __dev.controllers.right.updateButtonValue('a-button', 1); await new Promise(r => setTimeout(r, 150)); __dev.controllers.right.updateButtonValue('a-button', 0); });
  if (outDir) { await p.waitForFunction(z => __lm.body.z < z, -2.3, { timeout: 60000 }).catch(() => {}); await p.screenshot({ path: path.join(outDir, 'lm-tunnel.png') }); }
  const t0 = Date.now();
  await p.waitForFunction(() => __lm.S.mode === 'lm', null, { timeout: 90000 }).catch(() => {});
  const arrived = await p.evaluate(() => __lm.S.mode);
  ok('float through: arrived and standing in the LM', arrived === 'lm', ((Date.now() - t0) / 1000).toFixed(0) + ' s');
  await p.waitForFunction(() => __lm.SEAT.done && __lm.SEAT.pending === 0, null, { timeout: 30000 });
  await frames(10);
  /* both eyes */
  const eyes = () => p.evaluate(() => new Promise(res => { __lm.afterFrame = () => {
    const c = __lm.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
    res(out);
  }; }));
  const e1 = await eyes();
  ok('in the LM: both eyes drawn', e1.every(v => v > 4 && v < 250), e1.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'lm-cabin.png') });
  /* you are through: the lines that tell you to come through are off */
  ok('in the LM nobody tells you to come through any more', await p.evaluate(() => __lm.radio.lines.filter(l => l.off).map(l => l.id).join()) === 'q01,q02,q03');
  /* the headset's own "reset view" (its origin moves under you): you stay at your station beside the Lunar Module Pilot */
  const st = () => p.evaluate(() => new Promise(r => { __lm.afterFrame = () => { const v = __lm.camera.position.clone().setFromMatrixPosition(__lm.camera.matrixWorld); __lm.lmc.group.worldToLocal(v); r([v.x, v.y, v.z].map(x => +x.toFixed(2))); }; }));
  const at0 = await st(), n0 = await p.evaluate(() => __lm.SEAT.n);
  await p.evaluate(() => { __dev.position.set(0.9, 1.5, 0.7); __dev.quaternion.set(0, Math.sin(0.5), 0, Math.cos(0.5)); });
  await p.waitForFunction(n => __lm.SEAT.n > n && __lm.SEAT.pending === 0, n0, { timeout: 120000 });
  await frames(3);
  const at1 = await st();
  ok('a view reset leaves you at your station in the LM', Math.hypot(at1[0] - at0[0], at1[2] - at0[2]) < 0.05 && Math.abs(at1[1] - at0[1]) < 0.05 && Math.abs(at1[0] - 0.45) < 0.1, JSON.stringify([at0, at1]));
  /* the tunnel closed off: the two ships are drawn in, ring on ring */
  await p.evaluate(() => { __lm.S.arrivedAt -= 30; });
  await p.waitForFunction(() => __lm.S.hardK === 1, null, { timeout: 300000 });
  ok('with the hatches shut the two ships are hard docked', await p.evaluate(() => __lm.S.hatchShut && __lm.S.hardK === 1));
  /* undocking: the CSM backs away overhead, then comes round in front, nose towards you */
  const und = await p.evaluate(async () => {
    const O = __lm, V = O.camera.position.constructor;
    const at = async t => { O.setTime(t); O.S.running = false; await new Promise(r => { O.afterFrame = r; }); await new Promise(r => { O.afterFrame = r; });
      const lm = new V(), csm = new V(); O.vehicle.lm.getWorldPosition(lm); O.vehicle.group.getWorldPosition(csm);
      const inv = O.lmc.group.matrixWorld.clone().invert(), rel = csm.clone().applyMatrix4(inv);
      const nose = new V(0, 1, 0).applyQuaternion(O.vehicle.group.getWorldQuaternion(new O.camera.quaternion.constructor())).applyQuaternion(new O.camera.quaternion.constructor().setFromRotationMatrix(inv));
      return { rel: rel.toArray().map(v => +v.toFixed(1)), nose: nose.toArray().map(v => +v.toFixed(2)) }; };
    return { docked: await at(O.UNDOCK - 10), up: await at(O.UNDOCK + 50), front: await at(O.UNDOCK + 400) };
  });
  const dy = und.up.rel[1] - und.docked.rel[1];
  ok('undocking: the CSM backs away overhead', dy > 6, `${dy.toFixed(1)} m up after 50 s`);
  ok('then in front of the windows, nose towards you', und.front.rel[2] > 20 && und.front.nose[2] < -0.8, JSON.stringify(und.front));
  const e2 = await eyes();
  ok('America alongside: both eyes drawn', e2.every(v => v > 4 && v < 250), e2.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'lm-formation.png') });
  /* the outside view */
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => __lm.OUT.on, null, { timeout: 30000 });
  await frames(20);
  const eo = await eyes();
  ok('outside view: both eyes drawn', eo.every(v => v > 1 && v < 250), eo.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'lm-outside.png') });
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  /* the end */
  const end = await p.evaluate(async () => { __lm.S.running = true; __lm.setTime(__lm.END - 3); await new Promise(r => setTimeout(r, 6000)); return { ended: __lm.S.ended, t: __lm.S.t }; });
  ok('the chapter ends', end.ended, JSON.stringify(end));
  /* started again from the couch on the same page: the tunnel is there again, the hatch open, the ships at the
   * tunnel's spacing (the hatch stayed shut from the first time: you floated to the LM through open space) */
  await p.evaluate(() => { document.getElementById('start').value = '384000'; __lm.readOptions(); __lm.S.running = true; __lm.beginCrawl(); });
  await frames(4);
  const again = await p.evaluate(() => ({ mode: __lm.S.mode, shut: __lm.S.hatchShut, hardK: __lm.S.hardK, tunnel: __lm.tunnel.visible, out: __lm.OUT.on, off: __lm.radio.lines.filter(l => l.off).length }));
  ok('started again from the couch: the tunnel is drawn, the hatch open', again.mode === 'crawl' && !again.shut && again.hardK === 0 && again.tunnel && !again.out && again.off === 0, JSON.stringify(again));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'lm-again.png') });
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
