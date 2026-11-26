/* Chapters 11 (the entry) and 12 (the splashdown) in an emulated Meta Quest 3 (IWER) with Playwright and the
 * installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/entry.test.cjs [folder for screenshots] [11|12]
 * Chapter 11: the flight's figures against Apollo's entries, the Service Module cast off, fast forward dropping to
 * real time for the air, the fire (the shell, its light in the cabin), the load at its peak (your sight narrowed,
 * you pressed back), the radio lost and found, the cabin warming, the chapter's end leading on. Chapter 12: the
 * drogues, the swing nose-up (a swing, not a jump), the mains opening in their steps, fast forward on the long ride
 * down, the splash, the sea, the end. Waits on the state, never on time. */
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright-core');
const IWER = process.env.IWER || require.resolve('iwer/build/iwer.min.js');
const outDir = process.argv[2] && process.argv[2] !== '-' ? process.argv[2] : null, only = process.argv[3] || null;
let fails = 0;
const ok = (n, c, i = '') => { console.log(`${c ? 'ok  ' : 'FAIL'} ${n}${i ? '  ' + i : ''}`); if (!c) fails++; };
async function open(b, page, errs) {
  const p = await b.newPage({ viewport: { width: 1400, height: 700 } });
  p.on('pageerror', e => { errs.push(e.message); console.log('PAGE ERROR', e.message); });
  p.on('console', m => { if (m.type() === 'error' && !/favicon|404/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript({ content: fs.readFileSync(IWER, 'utf8') + ';window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.stereoEnabled = true; window.__dev.installRuntime({ forceInstall: true });' });
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/' + page);
  await p.waitForFunction(() => window.__entry && __entry.S.ready, null, { timeout: 180000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.click('#vr');
  await p.waitForFunction(() => __entry.renderer.xr.isPresenting && __entry.SEAT.done, null, { timeout: 60000 });
  await p.waitForFunction(() => __entry.radio && __entry.radio.lines.every(l => l.dur > 0), null, { timeout: 60000 });
  return p;
}
const frames = (p, n) => p.evaluate(n => new Promise(r => { const s = __entry.S.frames; const t = () => (__entry.S.frames >= s + n ? r() : setTimeout(t, 50)); t(); }), n);
const eyes = p => p.evaluate(() => new Promise(res => { __entry.afterFrame = () => {
  const c = __entry.renderer.domElement, t = document.createElement('canvas'); t.width = 200; t.height = 100;
  const g = t.getContext('2d'); g.drawImage(c, 0, 0, 200, 100);
  const d = g.getImageData(0, 0, 200, 100).data, out = [];
  for (const x0 of [0, 100]) { let sum = 0; for (let y = 0; y < 100; y++) for (let x = x0; x < x0 + 100; x++) sum += d[(y * 200 + x) * 4 + 1]; out.push(sum / 10000); }
  res(out);
}; }));
/* how things stand after a frame */
const now = p => p.evaluate(() => new Promise(r => { __entry.afterFrame = () => { const o = __entry, w = o.w, V = o.rig.position.constructor, up = new V().fromArray(w.up), vel = new V().fromArray(w.v).normalize();
  const nose = new V(0, 0, -1).applyQuaternion(o.rig.quaternion), headV = new V(0, 1, 0).applyQuaternion(o.rig.quaternion);
  r({ t: +(o.S.t - o.GETE.ei).toFixed(1), alt: Math.round(w.alt), speed: Math.round(w.speed), g: +w.g.toFixed(2), heat: +w.heat.toFixed(2), temp: +o.S.temp.toFixed(1), fire: o.fire.visible, uHeat: +o.fireU.uHeat.value.toFixed(2), light: +o.fireLight.intensity.toFixed(2),
    grey: +o.grey.toFixed(2), noseVel: +nose.dot(vel).toFixed(2), noseUp: +nose.dot(up).toFixed(2), headUp: +headV.dot(up).toFixed(2), blackout: o.blackout(o.S.t), warp: o.S.warp, chute: w.chute, sea: o.sea.visible, chutes: o.chutes.visible,
    sm: o.vehicle.parts.csm.children.filter(c => c !== o.vehicle.parts.cm && c !== o.fire && c !== o.sparks && c !== o.chutes && c.visible).length, now: o.info().now }); }; }));
const go = (p, t) => p.evaluate(t => { __entry.setTime(__entry.GETE.ei + t); __entry.S.running = true; }, t);
const until = (p, t, ms = 600000) => p.waitForFunction(t => __entry.S.t > __entry.GETE.ei + t, t, { timeout: ms });

async function chapter11(b) {
  const errs = [], p = await open(b, 'entry.html', errs);
  await p.evaluate(() => { __entry.S.running = false; __entry.setTime(__entry.START); });
  await frames(p, 5);
  const fl = await p.evaluate(() => { const i = __entry.FLIGHT.info(), G = __entry.GETE; return { peak: i.peak, at: i.peakAt, bo: i.blackout, drogue: i.drogue, main: i.main, splash: i.splash, loft: i.loft, vd: i.vDrogue, vs: i.vSplash, sun0: i.sunAtEntry, sun1: i.sunAtSplash, range: i.range, sep: G.ei - G.sep }; });
  const st0 = await now(p);
  ok('11: VR session runs: in your couch, falling to the Earth, the Service Module still on', !/12/.test(await p.evaluate(() => __entry.info().chapter)) && st0.alt > 3e6 && st0.speed > 7500 && st0.sm > 0 && !st0.fire, JSON.stringify(st0));
  ok('11: voices loaded', await p.evaluate(() => __entry.radio.lines.length) >= 18);
  ok('the entry: a peak of six to seven g about eighty seconds in, as Apollo\'s were', fl.peak > 5.8 && fl.peak < 7.3 && fl.at > 60 && fl.at < 110, `${fl.peak.toFixed(2)} g at ${fl.at.toFixed(0)} s`);
  ok('it does not skip out again, and glides down to the parachutes', fl.loft < 62000 && fl.drogue > 420 && fl.drogue < 620 && fl.vd < 160, `highest after the pull-out ${(fl.loft / 1000).toFixed(0)} km; drogues at ${fl.drogue.toFixed(0)} s, ${fl.vd.toFixed(0)} m/s`);
  ok('the radio is lost for some minutes in the fire', fl.bo[0] > 10 && fl.bo[0] < 60 && fl.bo[1] - fl.bo[0] > 150 && fl.bo[1] - fl.bo[0] < 300, `from ${fl.bo[0].toFixed(0)} s to ${fl.bo[1].toFixed(0)} s`);
  ok('down in the morning: the Sun low at the entry, well up at the splash, a quarter of an hour after', Math.abs(fl.sun0 - 8) < 1 && fl.sun1 > 20 && fl.sun1 < 40 && fl.splash > 700 && fl.splash < 1020 && Math.abs(fl.vs - 8.5) < 0.6, `Sun ${fl.sun0.toFixed(0)} to ${fl.sun1.toFixed(0)} degrees; the sea at ${fl.splash.toFixed(0)} s, ${fl.vs.toFixed(1)} m/s`);
  /* the Service Module is cast off */
  await go(p, -fl.sep - 3); await until(p, -fl.sep + 3);
  const sep = await now(p);
  ok('the Service Module is cast off: the Command Module alone', sep.sm === 0 && !!(await p.evaluate(() => __entry.S.cues.sep)) && /alone/.test(sep.now), JSON.stringify(sep));
  /* fast forward to the air, and real time from a minute before it */
  await go(p, -300); await p.evaluate(() => __entry.clock.faster(__entry.S)); const w0 = await p.evaluate(() => __entry.S.warp);
  await until(p, -58);
  const w1 = await now(p);
  ok('fast forward towards the air; real time from a minute before it', w0 === 10 && w1.warp === 1, `x${w0} -> x${w1.warp}`);
  /* heat shield forward, heads down, before the air */
  await go(p, -6); await frames(p, 4);
  const att = await now(p);
  ok('heat shield forward, your head towards the Earth', att.noseVel < -0.85 && att.headUp < -0.5, JSON.stringify({ noseVel: att.noseVel, headUp: att.headUp }));
  /* the fire begins */
  await go(p, 30); await p.waitForFunction(() => __entry.S.cues.glow, null, { timeout: 600000 }); await frames(p, 3);
  const glow = await now(p), e1 = await eyes(p);
  ok('the first of the fire: a glow in the windows, its light in the cabin', glow.fire && glow.uHeat > 0.04 && glow.uHeat < 0.5 && glow.light > 0.1 && glow.g < 1.5, JSON.stringify(glow));
  ok('in the glow: both eyes drawn', e1.every(v => v > 4 && v < 250), e1.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'entry-glow.png') });
  /* the peak */
  await go(p, fl.at - 5); await until(p, fl.at + 0.5); await frames(p, 2);
  const peak = await now(p), peakV = await p.evaluate(() => { const o = __entry, V = o.rig.position.constructor, eye = new V().copy(o.rig.position), a = new V().fromArray(o.w.a).normalize(); return { cues: ['fire', 'g2', 'hot', 'g4', 'g6'].filter(k => o.S.cues[k]).length, pressed: +(-eye.dot(a)).toFixed(3), radio: o.info().rows[5][1], shield: o.info().rows[3][1] }; });
  ok('the height of it: over six g, white fire, your sight narrowed, pressed back, the radio gone', peak.g > 6 && peak.heat > 0.85 && peak.grey > 0.25 && peak.light > 1.2 && peak.blackout && /blackout/.test(peakV.radio) && peakV.cues >= 4 && /\d/.test(peakV.shield), JSON.stringify(peak) + ' ' + JSON.stringify(peakV));
  const e2 = await eyes(p);
  ok('at the peak: both eyes drawn', e2.every(v => v > 4 && v < 250), e2.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'entry-peak.png') });
  /* outside (Y): the fireball */
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => __entry.OUT.on, null, { timeout: 60000 }); await frames(p, 5);
  const eo = await eyes(p);
  ok('outside view: the fire round the capsule, both eyes drawn', eo.every(v => v > 4 && v < 255), eo.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'entry-fireball.png') });
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => !__entry.OUT.on, null, { timeout: 60000 });
  /* it eases; the radio comes back; the cabin has warmed */
  await go(p, fl.bo[1] - 6); await p.waitForFunction(() => __entry.S.cues.aos, null, { timeout: 600000 });
  const aos = await now(p);
  ok('through: the load eased, the radio back, the fire dying', aos.g < 4 && !aos.blackout && aos.heat < 0.2 && aos.grey === 0, JSON.stringify(aos));
  await go(p, 420); await until(p, 440);
  const warm = await now(p);
  ok('the cabin has warmed by some degrees', warm.temp > 23, warm.temp + ' degrees');
  /* the end: on to chapter 12 */
  await p.evaluate(() => { const o = __entry; o.setTime(o.END - 3); o.S.running = true; });
  await p.waitForFunction(() => __entry.S.ended, null, { timeout: 600000 });
  const end = await p.evaluate(() => ({ alert: __entry.info().alert, saved: JSON.parse(localStorage.getItem('moon-progress') || '{}').page }));
  ok('the chapter ends before the parachutes, and leads on to chapter 12', /chapter 11/.test(end.alert) && end.saved === 'splash.html', JSON.stringify(end));
  ok('11: no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await p.close();
}

async function chapter12(b) {
  const errs = [], p = await open(b, 'splash.html', errs);
  await p.evaluate(() => { __entry.S.running = false; __entry.setTime(__entry.START); });
  await frames(p, 8);
  const G = await p.evaluate(() => { const g = __entry.GETE; return { drogue: g.drogue - g.ei, main: g.main - g.ei, splash: g.splash - g.ei }; });
  const st0 = await now(p);
  ok('12: VR session runs: seven kilometres up, falling fast, the fire out', /12/.test(await p.evaluate(() => __entry.info().chapter)) && st0.alt > 6000 && st0.alt < 12000 && st0.speed > 100 && !st0.fire && st0.sm === 0, JSON.stringify(st0));
  ok('12: voices loaded', await p.evaluate(() => __entry.radio.lines.length) >= 10);
  /* the drogues; the swing nose-up is a swing */
  await go(p, G.drogue - 3); await until(p, G.drogue + 1.5);
  const dr = await now(p), drV = await p.evaluate(() => ({ cover: !!__entry.S.cues.cover, cue: !!__entry.S.cues.drogues, n: __entry.drogues.filter(g => g.visible).length, mains: __entry.mains.filter(g => g.visible).length }));
  ok('the drogues are out', dr.chutes && drV.n === 2 && drV.mains === 0 && drV.cover && drV.cue && dr.chute === 1, JSON.stringify(dr) + ' ' + JSON.stringify(drV));
  const sw = await p.evaluate(async () => { const o = __entry, V = o.rig.position.constructor, frame = () => new Promise(r => { o.afterFrame = r; });
    o.setTime(o.GETE.drogue + 0.3); o.att.copy(o.att); const q0 = o.att.clone(); await frame(); const a1 = q0.angleTo(o.att); return { step: +(a1 * 57.3).toFixed(1) }; });
  ok('the swing on to the parachutes is a swing, not a jump', sw.step < 8, sw.step + ' degrees in a frame');
  await go(p, G.drogue + 9); await until(p, G.drogue + 14);
  const hang = await now(p);
  ok('hanging under them, nose up', hang.noseUp > 0.75, JSON.stringify({ noseUp: hang.noseUp }));
  /* the mains, opening in their steps */
  await go(p, G.main - 2); await until(p, G.main + 3);
  const reef = await p.evaluate(() => ({ n: __entry.mains.filter(g => g.visible).length, size: +__entry.mains[0].scale.x.toFixed(2), drogues: __entry.drogues.filter(g => g.visible).length }));
  await until(p, G.main + 14);
  const full = await now(p), fullV = await p.evaluate(() => ({ size: +__entry.mains[0].scale.x.toFixed(2), cue: !!__entry.S.cues.three }));
  ok('the three mains: out reefed, then full, and you are slowed to a walk', reef.n === 3 && reef.size < 0.35 && reef.drogues === 0 && fullV.size > 0.95 && fullV.cue && full.speed < 14 && full.chute === 2, JSON.stringify(reef) + ' ' + JSON.stringify(fullV) + ' ' + full.speed + ' m/s');
  const e1 = await eyes(p);
  ok('under the mains, in daylight: both eyes drawn, the sky bright', e1.every(v => v > 30 && v < 250), e1.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'splash-mains.png') });
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => __entry.OUT.on, null, { timeout: 60000 }); await frames(p, 5);
  const eo = await eyes(p);
  ok('outside view under the parachutes: both eyes drawn', eo.every(v => v > 30 && v < 250), eo.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'splash-outside.png') });
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));
  await p.waitForFunction(() => !__entry.OUT.on, null, { timeout: 60000 });
  /* the long ride down may be run faster; real time again for the sea */
  await go(p, G.main + 60); await p.evaluate(() => __entry.clock.faster(__entry.S)); const w0 = await p.evaluate(() => __entry.S.warp);
  await until(p, G.splash - 43);
  const w1 = await now(p);
  ok('the ride down can be run faster; real time for the last of it', w0 === 10 && w1.warp === 1 && w1.sea, `x${w0} -> x${w1.warp}`);
  /* the sea */
  await go(p, G.splash - 4); await p.waitForFunction(() => __entry.S.cues.splash, null, { timeout: 600000 }); await frames(p, 2);
  const sp = await now(p);
  ok('splashdown', sp.alt < 2 && sp.sea && /sea/.test(sp.now), JSON.stringify(sp));
  await until(p, G.splash + 10);
  const sea = await now(p), e2 = await eyes(p), seaV = await p.evaluate(() => ({ stable: !!__entry.S.cues.stable, mains: __entry.mains.filter(g => g.visible).length, vis: __entry.chutes.visible }));
  ok('on the sea: upright, the parachutes gone', sea.noseUp > 0.9 && !seaV.vis && seaV.stable && sea.speed < 10, JSON.stringify({ noseUp: sea.noseUp }) + ' ' + JSON.stringify(seaV));
  ok('on the sea: both eyes drawn', e2.every(v => v > 20 && v < 250), e2.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'splash-sea.png') });
  /* the end of the journey */
  await p.evaluate(() => { const o = __entry; o.setTime(o.END - 3); o.S.running = true; });
  await p.waitForFunction(() => __entry.S.ended, null, { timeout: 600000 });
  ok('the end: Apollo 17 is home', /home/.test(await p.evaluate(() => __entry.info().alert)));
  ok('12: no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await p.close();
}

(async () => {
  const b = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox', '--autoplay-policy=no-user-gesture-required'] });
  if (only !== '12') await chapter11(b);
  if (only !== '11') await chapter12(b);
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
