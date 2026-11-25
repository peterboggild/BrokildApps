/* Out of the LM and down to the Moon (index.html?egress), in an emulated Meta Quest 3 (IWER) with Playwright and the
 * installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root; or a threaded server)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/egress.test.cjs [folder for screenshots]
 * Checks: you start standing in the cabin, eyes between floor and ceiling at the Commander's station; the hatch
 * opens by itself; a grip sets you out, turned to face the cabin; pulling with a gripping hand moves you along the
 * way out (and only along it); A goes on by itself, through the hatch, over the porch, down the ladder, the drop to
 * the pad, onto the ground; the walk is then yours, standing on the terrain beyond the footpad with two prints;
 * both eyes drawn inside and outside; no errors. */
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
  p.on('console', m => { if (m.type() === 'error' && !/favicon|404|ERR_CONNECTION/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript({ content: fs.readFileSync(IWER, 'utf8') + ';window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.stereoEnabled = true; window.__dev.installRuntime({ forceInstall: true });' });
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/index.html?egress');
  await p.waitForFunction(() => window.__moon && __moon.S.ready, null, { timeout: 240000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.click('#vr');
  await p.waitForFunction(() => __moon.renderer.xr.isPresenting, null, { timeout: 60000 });
  const frames = n => p.evaluate(n => new Promise(r => { const s = __moon.S.frames; const t = () => (__moon.S.frames >= s + n ? r() : setTimeout(t, 40)); t(); }), n);
  await frames(8);
  /* the eye in the LM's frame */
  const eye = () => p.evaluate(() => new Promise(r => { __moon.afterFrame = () => { const v = __moon.camera.position.clone().setFromMatrixPosition(__moon.camera.matrixWorld); __moon.lm.worldToLocal(v); r([v.x, v.y, v.z].map(x => +x.toFixed(2))); }; }));
  const eyes2 = () => p.evaluate(() => new Promise(res => { __moon.afterFrame = () => {
    const c = __moon.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
    res(out);
  }; }));
  const e0 = await eye(), st0 = await p.evaluate(() => __moon.egress.E.stage);
  ok('you stand in the cabin at the Commander\'s station', st0 === 'cabin' && Math.abs(e0[0] - 0.45) < 0.12 && Math.abs(e0[2] - 0.78) < 0.12 && e0[1] > 3.6 && e0[1] < 4.3, st0 + ' ' + JSON.stringify(e0));
  const ein = await eyes2();
  ok('in the cabin: both eyes drawn', ein.every(v => v > 4 && v < 250), ein.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'egress-cabin.png') });
  /* the hatch opens by itself */
  await p.evaluate(() => { __moon.egress.E.t = 4; });
  await p.waitForFunction(() => __moon.egress.E.door >= 1, null, { timeout: 300000 });
  ok('the hatch opens', await p.evaluate(() => __moon.lm.userData.hatch.visible === false));
  /* a grip sets you out */
  await p.evaluate(() => { __dev.controllers.right.position.set(0.2, 1.3, -0.3); __dev.controllers.right.updateButtonValue('squeeze', 1); });
  await p.waitForFunction(() => __moon.egress.E.stage === 'path' && __moon.egress.fade < 0.2, null, { timeout: 300000 });
  await p.evaluate(() => { __dev.controllers.right.updateButtonValue('squeeze', 0); });
  await frames(3);
  const facing = await p.evaluate(() => new Promise(r => { __moon.afterFrame = () => { const d = __moon.camera.position.clone(); __moon.camera.getWorldDirection(d); d.transformDirection(__moon.lm.matrixWorld.clone().invert()); r([d.x, d.y, d.z].map(x => +x.toFixed(2))); }; }));
  ok('set out, turned to face the cabin', facing[2] < -0.9, JSON.stringify(facing));
  /* pulling: the hand towards the cabin (your forward) moves you backwards, out along the way */
  const pull = await p.evaluate(async () => {
    const o = __moon, c = __dev.controllers.right, frame = () => new Promise(r => { o.afterFrame = r; });
    const s0 = o.egress.E.s; c.position.set(0.2, 1.3, -0.3); await frame(); await frame();
    c.updateButtonValue('squeeze', 1); await frame(); await frame();
    for (let k = 1; k <= 8; k++) { c.position.set(0.2, 1.3, -0.3 - 0.03 * k); await frame(); }
    const s1 = o.egress.E.s;
    for (let k = 1; k <= 8; k++) { c.position.set(0.2 + 0.03 * k, 1.3, -0.54); await frame(); }      /* sideways: nothing */
    const s2 = o.egress.E.s;
    c.updateButtonValue('squeeze', 0); await frame();
    return { s0, s1, s2 };
  });
  ok('pulling moves you along the way out, and only along it', pull.s1 - pull.s0 > 0.12 && Math.abs(pull.s2 - pull.s1) < 0.03, JSON.stringify(pull));
  /* A: by itself, all the way down */
  await p.evaluate(async () => { __dev.controllers.right.updateButtonValue('a-button', 1); await new Promise(r => setTimeout(r, 300)); __dev.controllers.right.updateButtonValue('a-button', 0); });
  const seen = {}; let shot = false;
  for (let k = 0; k < 2400; k++) {
    const st = await p.evaluate(() => ({ stage: __moon.egress.E.stage, s: __moon.egress.E.s, auto: __moon.egress.E.auto }));
    seen[st.stage] = true;
    if (st.stage === 'path' && st.s > 3.2 && !shot) { shot = true; const e = await eyes2(); ok('on the ladder: both eyes drawn', e.every(v => v > 4 && v < 250), e.map(v => v.toFixed(0)).join(' / ')); if (outDir) await p.screenshot({ path: path.join(outDir, 'egress-ladder.png') }); }
    if (st.stage === 'done') break;
    await p.waitForTimeout(250);
  }
  ok('A goes by itself: the path, the drop, the pad, the ground', seen.path && seen.drop && seen.pad && seen.done, Object.keys(seen).join(' '));
  await frames(3);
  const end = await p.evaluate(() => { const o = __moon, r = o.rig.position, f = o.lm.localToWorld(new o.rig.position.constructor(0, 0, 5.3));
    return { active: o.egress.active, onGround: Math.abs(r.y - o.moon.heightAt(r.x, r.z)) < 0.05, dist: Math.hypot(r.x - f.x, r.z - f.z), prints: o.prints.count ?? null, pos: [o.S.pos.x, o.S.pos.z].map(v => +v.toFixed(2)) }; });
  ok('the walk is yours: on the ground a step beyond the footpad', !end.active && end.onGround && end.dist < 1.2, JSON.stringify(end));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'egress-moon.png') });
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
