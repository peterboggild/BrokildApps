/* The cabin's life and the controls round it, in chapter 2, in an emulated Meta Quest 3 (IWER) with Playwright and
 * the installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/cabin.test.cjs [folder for screenshots]
 * Checks: each hand on its own controller by handedness; the hand angle setting tips the hand up; X shows the panel,
 * then the settings (the stick changes one), then puts it away; a floating thing taken with the grip, carried and
 * let go floats on the way the hand moved; the crewmen look you in the eye when they look at you, blink, and the
 * jaw moves with a voice; fast forward stops at the TLI burn and says why, and runs through it when free; the
 * Earth from far out is the same picture in both eyes (one Earth, not two); no errors. */
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
  p.on('console', m => { if (m.type() === 'error' && !/favicon|404|ERR_CONNECTION/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript({ content: fs.readFileSync(IWER, 'utf8') + ';window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.stereoEnabled = true; window.__dev.installRuntime({ forceInstall: true });' });
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/orbit.html');
  await p.waitForFunction(() => window.__orbit && __orbit.S.ready, null, { timeout: 180000 });
  await p.waitForSelector('#vr:not([disabled])', { timeout: 30000 });
  await p.click('#vr');
  await p.waitForFunction(() => __orbit.renderer.xr.isPresenting && __orbit.SEAT.done, null, { timeout: 60000 });
  await p.evaluate(() => { __orbit.setTime(3000); });
  const frames = n => p.evaluate(n => new Promise(r => { const s = __orbit.S.frames; const t = () => (__orbit.S.frames >= s + n ? r() : setTimeout(t, 30)); t(); }), n);
  const press = (hand, id, ms = 250) => p.evaluate(async ([h, id, ms]) => { __dev.controllers[h].updateButtonValue(id, 1); await new Promise(r => setTimeout(r, ms)); __dev.controllers[h].updateButtonValue(id, 0); await new Promise(r => setTimeout(r, 250)); }, [hand, id, ms]);
  await frames(5);

  /* hands by handedness */
  const hd = await p.evaluate(async () => {
    __dev.controllers.left.position.set(-0.3, 1.2, -0.4); __dev.controllers.right.position.set(0.3, 1.2, -0.4);
    await new Promise(r => { __orbit.afterFrame = r; }); await new Promise(r => { __orbit.afterFrame = r; });
    const o = __orbit, V = o.camera.position.constructor;
    const x = h => { const w = o.hands[h].getWorldPosition(new V()); return o.camera.worldToLocal(w).x; };
    return { lp: o.hands.left.parent === o.ctl.grips.left, rp: o.hands.right.parent === o.ctl.grips.right, lx: x('left'), rx: x('right') };
  });
  ok('each hand on its own controller, by handedness', hd.lp && hd.rp && hd.lx < -0.1 && hd.rx > 0.1, JSON.stringify(hd));

  /* the hand angle: the index finger's first bone, how far above the controller's pointing ray */
  const pitch = () => p.evaluate(async () => {
    await new Promise(r => { __orbit.afterFrame = r; });
    const o = __orbit, V = o.camera.position.constructor, h = o.hands.right, m = h.userData.model;
    if (!m) return null;
    let a = null, c = null; m.traverse(n => { if (n.name === 'index-finger-phalanx-proximal') a = n; if (n.name === 'index-finger-phalanx-intermediate') c = n; });
    const d = c.getWorldPosition(new V()).sub(a.getWorldPosition(new V())).normalize();
    const ray = o.ctl.rays.right, q = ray.getWorldQuaternion(new o.camera.quaternion.constructor()).invert();
    d.applyQuaternion(q);                                         /* in the ray's frame: -z ahead, +y up */
    return Math.atan2(d.y, -d.z) * 57.3;
  });
  await p.waitForFunction(() => !!__orbit.hands.right.userData.model, null, { timeout: 30000 });
  await p.evaluate(() => __orbit.settings.set('hand', 0)); const p0 = await pitch();
  await p.evaluate(() => __orbit.settings.set('hand', 17)); const p17 = await pitch();
  ok('the hand angle setting tips the finger up', p0 !== null && Math.abs(p17 - p0 - 17) < 3, `finger ${p0.toFixed(1)} deg from the ray at 0, ${p17.toFixed(1)} at +17`);

  /* the wrist panel: the left stick pressed three times (A and B are faster and slower, X and Y the cheat window and outside) */
  const w0 = await p.evaluate(() => [__orbit.wrist.on, __orbit.wrist.mesh.visible]);
  await press('left', 'thumbstick'); const w1 = await p.evaluate(() => [__orbit.wrist.on, __orbit.wrist.page]);
  await press('left', 'thumbstick'); const w2 = await p.evaluate(() => [__orbit.wrist.on, __orbit.wrist.page]);
  const ffBefore = await p.evaluate(() => __orbit.settings.get('ff'));
  await p.evaluate(async () => { const c = __dev.controllers.left; c.updateAxes('thumbstick', 1, 0); await new Promise(r => setTimeout(r, 400)); c.updateAxes('thumbstick', 0, 0); await new Promise(r => setTimeout(r, 300)); });
  const ffAfter = await p.evaluate(() => __orbit.settings.get('ff'));
  if (outDir) { await frames(3); await p.screenshot({ path: path.join(outDir, 'cabin-settings.png') }); }
  await press('left', 'thumbstick'); const w3 = await p.evaluate(() => [__orbit.wrist.on]);
  ok('panel hidden at the start; left stick press: status, settings, away', !w0[0] && w1[0] && w1[1] === 'status' && w2[1] === 'settings' && !w3[0], JSON.stringify([w0, w1, w2, w3]));
  ok('the left stick changes a setting', ffBefore !== ffAfter, ffBefore + ' -> ' + ffAfter);
  await p.evaluate(() => __orbit.settings.set('ff', 'events'));

  /* take the pen, carry it, let it go */
  const grab = await p.evaluate(async () => {
    const o = __orbit, V = o.camera.position.constructor, pen = o.floaters.items[1];
    pen.v.set(0, 0, 0); pen.w.set(0, 0, 0);
    const frame = () => new Promise(r => { o.afterFrame = r; });
    /* the controller at the pen: its place in the head's frame (the reference space) */
    o.head.updateMatrixWorld(true);
    const at = o.head.worldToLocal(pen.obj.getWorldPosition(new V()));
    const c = __dev.controllers.right; c.position.set(at.x, at.y, at.z);
    await frame(); await frame();
    c.updateButtonValue('squeeze', 1); await frame(); await frame();
    const held = !!pen.held;
    const p0 = pen.obj.position.clone();
    for (let k = 1; k <= 8; k++) { c.position.set(at.x + 0.025 * k, at.y, at.z); await frame(); }
    const carried = pen.obj.position.clone().sub(p0);
    c.updateButtonValue('squeeze', 0); await frame(); await frame();
    return { held, carried: carried.toArray().map(v => +v.toFixed(3)), released: !pen.held, v: pen.v.toArray().map(v => +v.toFixed(3)) };
  });
  ok('the pen taken with the grip and carried', grab.held && Math.hypot(...grab.carried) > 0.1, JSON.stringify(grab.carried));
  ok('let go, it floats on with the hand\'s motion', grab.released && Math.hypot(...grab.v) > 0.02, JSON.stringify(grab.v));

  /* the crew: eye contact, blinking, the jaw with a voice */
  const crew = await p.evaluate(async () => {
    const o = __orbit, life = o.cabin.life;
    life.force('you');
    const t0 = performance.now(); let blinks = 0, last = 0;
    while (performance.now() - t0 < 9000) { await new Promise(r => { o.afterFrame = r; }); const b = o.cabin.crewmen[0].userData.face.uBlink.value; if (b > 0.5 && last <= 0.5) blinks++; last = b; }
    const pr = life.probe(o.camera);
    life.force(null);
    return { pr, blinks };
  });
  const off = crew.pr.filter(Boolean).map(c => Math.max(...c.eyesOff));
  ok('both crewmen look you in the eye', off.length === 2 && off.every(v => v < 6), off.map(v => v.toFixed(1) + ' deg').join(', ') + '  ' + JSON.stringify(crew.pr.map(c => c && [c.yaw, c.pitch])));
  ok('they blink', crew.blinks >= 1, crew.blinks + ' blinks in 9 s');
  const jaw = await p.evaluate(async () => {
    const o = __orbit; await new Promise(r => setTimeout(r, 200));
    const l = o.radio.lines.find(x => x.who === 'CMP' && typeof x.t === 'number' && x.dur > 2);
    o.setTime(l.t - 0.5); o.S.running = true; let mx = 0; const t0 = performance.now();
    while (performance.now() - t0 < 2500) { await new Promise(r => { o.afterFrame = r; }); mx = Math.max(mx, o.cabin.crewmen[0].userData.face.uJaw.value); }
    o.S.running = false; return { mx, id: l.id };
  });
  ok('the jaw moves with his voice', jaw.mx > 0.02, `${jaw.id}: open up to ${(jaw.mx * 57.3).toFixed(1)} deg`);

  /* fast forward at the TLI burn (its stretch from GET 3:10:06): the default stops there (and for radio lines), says
   * why, and A goes on, x10 at most through the burn; free runs through at full speed */
  const ff = await p.evaluate(async () => {
    const o = __orbit, sleep = ms => new Promise(r => setTimeout(r, ms)), a0 = 11556 - 150;
    o.settings.set('ff', 'events'); o.setTime(a0 - 250); o.S.running = true; o.S.warp = 1000;
    let stop = null, inMax = 0;
    for (let k = 0; k < 90 && o.S.t < 11556 + 20; k++) {
      await sleep(400);
      if (o.S.warp === 1) { if (!stop && o.S.t >= a0 - 1) stop = { t: o.S.t, toast: o.toast.text }; o.clock.faster(o.S); o.clock.faster(o.S); o.clock.faster(o.S); }
      if (o.clock.inHold(o.S.t)) inMax = Math.max(inMax, o.S.warp);
    }
    const events = { stop, reached: o.S.t, inMax };
    /* A (a tap): one step slower; B: one step faster; the small clock while fast; the place saved for Continue */
    o.setTime(30000); o.S.warp = 1000; __dev.controllers.right.updateButtonValue('a-button', 1); await sleep(250); __dev.controllers.right.updateButtonValue('a-button', 0); await sleep(400);
    events.slower = o.S.warp;
    __dev.controllers.right.updateButtonValue('b-button', 1); await sleep(250); __dev.controllers.right.updateButtonValue('b-button', 0); await sleep(400);
    events.faster = o.S.warp; events.badge = o.badge.on;
    await sleep(5600); events.saved = JSON.parse(localStorage.getItem('moon-progress') || 'null');
    o.settings.set('ff', 'free'); o.setTime(a0 - 250); o.S.warp = 1000; await sleep(2500);
    const free = { t: o.S.t, warp: o.S.warp };
    o.S.running = false; o.settings.set('ff', 'events');
    return { events, free };
  });
  ok('fast forward stops at the burn and says why', ff.events.stop && Math.abs(ff.events.stop.t - (11556 - 150)) < 1 && /TLI/.test(ff.events.stop.toast), JSON.stringify(ff.events.stop));
  ok('A goes on, x10 at most through the burn', ff.events.reached > 11556 && ff.events.inMax === 10, JSON.stringify(ff.events));
  ok('A: one step slower, B: one step faster', ff.events.slower === 100 && ff.events.faster === 1000, 'x1000 -> A x' + ff.events.slower + ' -> B x' + ff.events.faster);
  ok('the small clock shows while time runs fast', ff.events.badge === true);
  ok('the place is saved for Continue', ff.events.saved && ff.events.saved.page === 'orbit.html' && ff.events.saved.t > 30000, JSON.stringify(ff.events.saved));
  ok('free fast forward runs through the burn', ff.free.t > 11556 + 400 && ff.free.warp === 1000, JSON.stringify(ff.free));

  /* your hand on a crewman: it stops at his suit, turns red, he turns to you and says something */
  const touch = await p.evaluate(async () => {
    const o = __orbit, V = o.camera.position.constructor, frame = () => new Promise(r => { o.afterFrame = r; });
    const man = o.cabin.crewmen[0], chest = man.localToWorld(new V(0, -0.3, -0.02));
    o.head.updateMatrixWorld(true);
    const at = o.head.worldToLocal(chest.clone()), c = __dev.controllers.right;
    c.position.set(at.x, at.y, at.z);
    let said = null;
    for (let k = 0; k < 12; k++) { await frame(); const t = o.chat && o.chat.radio.talking('CMP'); if (t) said = t.line.text; }
    const h = o.hands.right, gripW = o.ctl.grips.right.getWorldPosition(new V()), handW = h.getWorldPosition(new V());
    const r = { onCrew: !!h.userData.onCrew, pushed: +handW.distanceTo(gripW).toFixed(3), red: h.userData.warn > 0, react: o.cabin.life.crew[0].reactUntil > 0, said };
    c.position.set(0.3, 1.2, -0.4); await frame(); await frame();
    return r;
  });
  ok('the hand stops at his suit and turns red', touch.onCrew && touch.pushed > 0.03 && touch.red, JSON.stringify(touch));
  ok('he turns to you and says something', touch.react && !!touch.said, JSON.stringify(touch.said));

  /* small talk: now and then, in a quiet moment */
  const small = await p.evaluate(async () => {
    const o = __orbit, sleep = ms => new Promise(r => setTimeout(r, ms));
    o.setTime(30000); o.S.running = true; o.S.warp = 1; await sleep(300);
    o.chat.sayNow(); let said = null;
    for (let k = 0; k < 20 && !said; k++) { await sleep(150); for (const w of ['CMP', 'LMP']) { const t = o.chat.radio.talking(w); if (t) said = { who: w, text: t.line.text, cue: t.line.cue, to: o.cabin.life.crew.find(c => c.who === w).to }; } }
    o.S.running = false; return said;
  });
  ok('small talk now and then', small && small.cue === 'small', JSON.stringify(small));
  await p.evaluate(() => __orbit.settings.set('ff', 'events'));

  /* the Earth from a day out, from outside: one Earth, the same in both eyes */
  await p.evaluate(() => { __orbit.setTime(90000); (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)); });
  await p.waitForFunction(() => __orbit.OUT.on, null, { timeout: 30000 });
  await p.evaluate(() => { __orbit.OUT.az = 0; __orbit.OUT.el = 1.4; __orbit.OUT.dist = 40; });
  await frames(6);
  const eyes = await p.evaluate(() => new Promise(res => { __orbit.afterFrame = () => {
    const c = __orbit.renderer.domElement, W = 400, H = 200, t = document.createElement('canvas'); t.width = W; t.height = H;
    const g = t.getContext('2d'); g.drawImage(c, 0, 0, W, H);
    const d = g.getImageData(0, 0, W, H).data, half = [[], []];
    for (let e = 0; e < 2; e++) for (let y = 0; y < H; y++) for (let x = 0; x < W / 2; x++) { const i = (y * W + x + e * W / 2) * 4; half[e].push(d[i] + d[i + 1] + d[i + 2]); }
    /* the blue of the sky's Earth would show where the Earth model is not: count the bright pixels in each eye */
    const lit = half.map(h => h.filter(v => v > 90).length);
    let diff = 0; for (let i = 0; i < half[0].length; i++) diff += Math.abs(half[0][i] - half[1][i]);
    res({ lit, diff: diff / half[0].length });
  }; }));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'cabin-earth-stereo.png') });
  ok('one Earth, the same in both eyes', Math.abs(eyes.lit[0] - eyes.lit[1]) <= Math.max(6, 0.15 * eyes.lit[0]), JSON.stringify(eyes));
  await p.evaluate(() => (__dev.controllers.left.updateButtonValue('y-button', 1), setTimeout(() => __dev.controllers.left.updateButtonValue('y-button', 0), 300)));

  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
