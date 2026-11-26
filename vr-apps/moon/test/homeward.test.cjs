/* Chapter 10 (the coast home, with the walk outside) in an emulated Meta Quest 3 (IWER) with Playwright and the
 * installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/homeward.test.cjs [folder for screenshots]
 * Enters VR in the couch; checks the flight's figures, fast forward dropping to real time for the walk, the hatch
 * opening with the clock waiting, going out (where you float, which way you face), pulling yourself along the rail
 * with a grip, the walk by itself to the instrument bay, the cassette taken and brought in, the chapter's end, and
 * no errors. Waits on the state, never on time. */
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
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/homeward.html');
  await p.waitForFunction(() => window.__home && __home.S.ready, null, { timeout: 180000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.click('#vr');
  await p.waitForFunction(() => __home.renderer.xr.isPresenting && __home.SEAT.done, null, { timeout: 60000 });
  const frames = n => p.evaluate(n => new Promise(r => { const s = __home.S.frames; const t = () => (__home.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
  const eyes = () => p.evaluate(() => new Promise(res => { __home.afterFrame = () => {
    const c = __home.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
    const d = g.getImageData(0, 0, 200, 100).data, out = [];
    for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
    res(out);
  }; }));
  const tapA = () => p.evaluate(async () => { __dev.controllers.right.updateButtonValue('a-button', 1); await new Promise(r => setTimeout(r, 350)); __dev.controllers.right.updateButtonValue('a-button', 0); });
  /* where your eye is in the ship's own frame, and how you face its axis */
  const where = () => p.evaluate(() => new Promise(r => { __home.afterFrame = () => { const o = __home, V = o.rig.position.constructor, inv = o.vehicle.group.quaternion.clone().invert();
    const e = new V().setFromMatrixPosition(o.camera.matrixWorld).applyQuaternion(inv), f = new V(0, 0, -1).applyQuaternion(o.rig.quaternion).applyQuaternion(inv), u = new V(0, 1, 0).applyQuaternion(o.rig.quaternion).applyQuaternion(inv);
    r({ state: o.EVA.state, s: +o.EVA.s.toFixed(2), eye: [e.x, e.y, e.z].map(v => +v.toFixed(2)), off: +(Math.hypot(e.x, e.z) - 1.96).toFixed(2), inward: +(-(f.x * e.x + f.z * e.z) / Math.hypot(e.x, e.z)).toFixed(2), headToNose: +u.y.toFixed(2), has: o.EVA.has, t: o.S.t, warp: o.S.warp }); }; }));
  await p.evaluate(() => { __home.S.running = false; __home.setTime(__home.START); });
  await frames(5);
  const fl = await p.evaluate(() => { const o = __home, a = o.where(o.START), e = o.where(o.EVA_T), z = o.where(o.END); return { a: a.alt / 1e6, moon: a.dMoon / 1e6, e: e.alt / 1e6, z: z.alt / 1e6, v0: a.speed, v1: z.speed, hours: (o.END - o.START) / 3600 }; });
  ok('VR session runs: in your couch, 300,000 km from home, the Earth ahead', fl.a > 280 && fl.a < 320 && fl.moon > 90 && fl.moon < 160 && fl.v0 < 1200, `${fl.a.toFixed(0)}k km to the Earth, ${fl.moon.toFixed(0)}k km from the Moon, ${fl.v0.toFixed(0)} m/s`);
  ok('the fall home: faster all the way, to 25,000 km in fifty hours', Math.abs(fl.z - 25) < 0.5 && fl.v1 > 4000 && fl.hours > 45 && fl.hours < 55 && fl.e > 270 && fl.e < 300, `${fl.z.toFixed(1)}k km at ${fl.v1.toFixed(0)} m/s after ${fl.hours.toFixed(1)} h; the walk ${fl.e.toFixed(0)}k km out`);
  await p.waitForFunction(() => __home.radio && __home.radio.lines.every(l => l.dur > 0), null, { timeout: 60000 });
  ok('voices loaded', await p.evaluate(() => __home.radio.lines.length) >= 13);
  const e0 = await eyes();
  ok('in the couch: both eyes drawn', e0.every(v => v > 4 && v < 250), e0.map(v => v.toFixed(0)).join(' / '));
  /* fast forward drops to real time for the walk */
  await p.evaluate(() => { const o = __home; o.setTime(o.EVA_T - 400); o.S.running = true; o.clock.faster(o.S); });
  const w0 = await p.evaluate(() => __home.S.warp);
  await p.waitForFunction(() => __home.S.t > __home.EVA_T - 98, null, { timeout: 600000 });
  const w1 = await p.evaluate(() => __home.S.warp);
  ok('fast forward on the coast; real time as the walk comes up', w0 > 1 && w1 === 1, `x${w0} -> x${w1}`);
  /* the hatch opens; the clock waits for you */
  await p.evaluate(() => { const o = __home; o.setTime(o.EVA_T - 3); o.S.running = true; });
  await p.waitForFunction(() => __home.EVA.state === 'wait' && /hatch/.test(__home.toast.text || ''), null, { timeout: 600000 });
  await frames(6);
  const wait = await p.evaluate(() => ({ t: __home.S.t - __home.EVA_T, cue: !!__home.S.cues.hatch, said: __home.toast.text, now: __home.info().now }));
  ok('the hatch is open, the clock waits, and you are told the way', Math.abs(wait.t) < 0.5 && wait.cue && /A/.test(wait.said) && /hatch/.test(wait.now), JSON.stringify(wait));
  /* out */
  await tapA();
  await p.waitForFunction(() => __home.EVA.state === 'out' && __home.FADE.k < 0.3, null, { timeout: 600000 });
  await frames(3);
  const out = await where();
  ok('out of the hatch: floating off the hull, facing it, your head towards the nose', out.state === 'out' && out.s < 0.3 && out.eye[0] < -1.2 && out.eye[1] > 0.4 && out.inward > 0.9 && out.headToNose > 0.9, JSON.stringify(out));
  const e1 = await eyes();
  ok('outside: both eyes drawn', e1.every(v => v > 1 && v < 250), e1.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'homeward-hatch.png') });
  /* a grip on the rail: the hand stays, you go the other way (hand up, you down towards the bay) */
  const pull = await p.evaluate(async () => { const o = __home, c = __dev.controllers.right, frame = () => new Promise(r => { o.afterFrame = r; });
    c.position.set(0.25, 1.1, -0.35); await frame(); await frame();
    const s0 = o.EVA.s; c.updateButtonValue('squeeze', 1); await frame(); await frame();
    for (let k = 1; k <= 8; k++) { c.position.set(0.25, 1.1 + 0.04 * k, -0.35); await frame(); }
    const s1 = o.EVA.s; c.updateButtonValue('squeeze', 0); await frame(); await frame();
    return { s0, s1 }; });
  ok('a grip and a pull: you move along the rail, the other way from your hand', pull.s1 - pull.s0 > 0.2 && pull.s1 - pull.s0 < 0.45, JSON.stringify(pull));
  /* A: by itself to the bay; the cassette is taken; and back in */
  await tapA();
  await p.waitForFunction(() => __home.EVA.has, null, { timeout: 900000 });
  const bay = await where(), bayV = await p.evaluate(() => ({ len: __home.RAIL_LEN, cueBay: !!__home.S.cues.bay, cueGot: !!__home.S.cues.got, inHand: __home.cassette.parent === __home.ctl.grips.right }));
  ok('at the instrument bay, on the Service Module\'s side, the cassette is taken', bay.s > bayV.len - 0.2 && bay.eye[1] < -2 && Math.abs(bay.off - 0.8) < 0.35 && bay.inward > 0.9 && bayV.cueBay && bayV.cueGot && bayV.inHand, JSON.stringify(bay) + ' ' + JSON.stringify(bayV));
  const e2 = await eyes();
  ok('at the bay: both eyes drawn', e2.every(v => v > 1 && v < 250), e2.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'homeward-bay.png') });
  await p.waitForFunction(() => __home.EVA.state === 'done' && __home.FADE.k < 0.3 && !__home.SEAT.pending, null, { timeout: 900000 });
  await frames(4);
  const back = await where(), backV = await p.evaluate(() => ({ cue: !!__home.S.cues.in, film: __home.cassette.visible, now: __home.info().now }));
  ok('back in your couch with the film', back.state === 'done' && Math.hypot(back.eye[0] - 0.6, back.eye[1], back.eye[2] + 0.15) < 0.4 && backV.cue && !backV.film && /film aboard/.test(backV.now), JSON.stringify(back) + ' ' + JSON.stringify(backV));
  /* the end: the Earth close */
  await p.evaluate(() => { const o = __home; o.S.t = o.END - 4; o.S.running = true; });
  await p.waitForFunction(() => __home.S.ended, null, { timeout: 600000 });
  ok('the chapter ends 25,000 km from home', await p.evaluate(() => /End of chapter 10/.test(__home.info().alert)));
  const e3 = await eyes();
  ok('at the end: both eyes drawn', e3.every(v => v > 4 && v < 250), e3.map(v => v.toFixed(0)).join(' / '));
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
