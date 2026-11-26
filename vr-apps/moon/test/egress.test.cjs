/* Chapter 6, On the Moon (surface.html): out of the LM and down to the Moon, in an emulated Meta Quest 3 (IWER) with Playwright and the
 * installed Chrome, software GL.
 *   python -m http.server 8765        (from the repository root; or a threaded server)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core, iwer)
 *   node vr-apps/moon/test/egress.test.cjs [folder for screenshots]
 * Checks: you start standing in the cabin, eyes between floor and ceiling at the Commander's station; the hatch
 * opens by itself; a grip sets you out, turned to face the cabin; pulling with a gripping hand moves you along the
 * way out (and only along it); A goes on by itself, through the hatch, over the porch, down the ladder, the drop to
 * the pad, onto the ground; the walk is then yours, standing on the terrain beyond the footpad with two prints;
 * both eyes drawn inside and outside; the Lunar Module Pilot then follows you out and down and moves about; the radio
 * answers; your gloves dim in the LM's shadow; the flag, the rover, a sample, the film (nothing kept until you say
 * so, then twelve seconds), the sample bag; he goes up the ladder first and the ladder is his meanwhile; from the
 * ground you climb back up into the cabin; the lift-off (the LM's shadow in two), the finale's console and film
 * (the valley, the flag, the rock, the wave, the lift-off); no errors. */
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
  await p.goto('http://127.0.0.1:8765/vr-apps/moon/surface.html');
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
  /* first the helmets: his is in his hands, yours is not on; then both are */
  const h0 = await p.evaluate(() => ({ mine: __moon.helm.k, his: __moon.lmc.lmp.userData.helmet.position.y }));
  await p.evaluate(() => { __moon.egress.E.t = 9.2; });
  await p.waitForFunction(() => __moon.egress.E.t > 9.6, null, { timeout: 120000 });
  const h1 = await p.evaluate(() => ({ mine: __moon.helm.k, his: __moon.lmc.lmp.userData.helmet.position.y }));
  ok('the helmets go on before the cabin is let down', h0.mine === 0 && Math.abs(h0.his) > 0.05 && h1.mine === 1 && Math.abs(h1.his) < 0.01, JSON.stringify([h0, h1]));
  /* the hatch opens by itself */
  await p.evaluate(() => { __moon.egress.E.t = 13.5; });
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
  /* the Lunar Module Pilot follows you down (lmp.js; three times the speed, for the emulator's sake) */
  ok('the Lunar Module Pilot is in the cabin still', await p.evaluate(() => __moon.lmp.S.phase === 'cabin' && __moon.lmc.lmp.visible));
  await p.evaluate(() => { __moon.lmp.S.rate = 3; __moon.lmp.S.t = 6.9; });
  const seen2 = {};
  for (let k = 0; k < 2400; k++) {
    const ph = await p.evaluate(() => __moon.lmp.S.phase); seen2[ph] = true;
    if (ph === 'roam') break;
    await p.waitForTimeout(250);
  }
  ok('he comes out: the porch, the ladder, the drop, the pad', seen2.crawl && seen2.ladder && seen2.pad && seen2.roam, Object.keys(seen2).join(' '));
  await p.waitForFunction(() => __moon.lmp.S.falls >= 6, null, { timeout: 600000 });
  const man = await p.evaluate(() => { const L = __moon.lmp.S, f = __moon.lmp.fig; return { h: +(L.pos.y - __moon.moon.heightAt(L.pos.x, L.pos.z)).toFixed(2), cabin: __moon.lmc.lmp.visible, far: +Math.hypot(L.pos.x, L.pos.z).toFixed(1), falls: L.falls, ok: [f.position.x, f.position.y, f.position.z, L.yaw].every(isFinite) }; });
  ok('he moves about on the ground in the lope', man.h > -0.05 && man.h < 0.4 && !man.cabin && man.far > 5 && man.far < 20 && man.ok, JSON.stringify(man));
  const e3 = await eyes2();
  ok('with him outside: both eyes drawn', e3.every(v => v > 4 && v < 250), e3.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'egress-lmp.png') });
  /* his visor's mirror is being drawn; you cannot walk into the LM or into him */
  const r0 = await p.evaluate(() => __moon.refl); await frames(6);
  ok('his visor mirrors the scene (the cube map is drawn)', r0 >= 6 && await p.evaluate(() => __moon.refl) > r0, String(r0));
  const hit = await p.evaluate(async () => {
    const o = __moon, frame = () => new Promise(r => { o.afterFrame = r; }), L = o.lmp.S, c = o.lm.position;
    const head = () => { const v = o.camera.position.clone().setFromMatrixPosition(o.camera.matrixWorld); return [v.x, v.z]; };
    L.rate = 0;                                                   /* he stands still meanwhile */
    o.S.pos.set(c.x + 0.4, 0, c.z + 0.3); await frame(); await frame(); await frame(); const a = head();
    o.S.pos.set(L.pos.x + 0.1, 0, L.pos.z); await frame(); await frame(); await frame(); const b = head();
    L.rate = 3;
    return { lm: +Math.hypot(a[0] - c.x, a[1] - c.z).toFixed(2), man: +Math.hypot(b[0] - L.pos.x, b[1] - L.pos.z).toFixed(2) };
  });
  ok('the LM and the other astronaut are solid', hit.lm > 2.4 && hit.man > 0.7, JSON.stringify(hit));
  /* your gloves: in the Sun, and in the LM's shadow (found by asking the shadow's own picture round the LM) */
  const dim = await p.evaluate(async () => {
    const o = __moon, frame = () => new Promise(r => { o.afterFrame = r; }), c = o.lm.position, V = c.constructor, was = o.S.pos.clone();
    const probe = (x, z, want) => { const y = o.moon.heightAt(x, z); return [0.9, 1.3, 1.7].every(h => [-0.4, 0, 0.4].every(d => Math.abs(o.lmShadow.lit(new V(x + d, y + h, z + d)) - want) < 0.05)); };
    let sun = null, at = null;
    for (let a = 0; a < 6.28 && !sun; a += 0.3) { const x = c.x + Math.sin(a) * 22, z = c.z + Math.cos(a) * 22; if (probe(x, z, 1)) sun = [x, z]; }
    if (!sun) return { sun };
    o.S.pos.set(sun[0], 0, sun[1]); for (let k = 0; k < 14; k++) await frame();
    const sunny = [o.hands.left.userData.lit, o.hands.right.userData.lit].map(v => +v.toFixed(2));
    for (let r = 8; r <= 14 && !at; r += 2) for (let a = 0; a < 6.28 && !at; a += 0.1) {
      const x = c.x + Math.sin(a) * r, z = c.z + Math.cos(a) * r;
      if (probe(x, z, 0)) at = [x, z];
    }
    if (!at) return { at };
    o.S.pos.set(at[0], 0, at[1]); for (let k = 0; k < 14; k++) await frame();
    const shade = [o.hands.left.userData.lit, o.hands.right.userData.lit], col = o.hands.left.userData.litF;
    o.S.pos.copy(was); for (let k = 0; k < 3; k++) await frame();
    return { at: at.map(v => +v.toFixed(1)), sunny, shade: shade.map(v => +v.toFixed(2)), col: +col.toFixed(2) };
  });
  ok("your gloves: lit in the Sun, dimmed in the LM's shadow", !!dim.at && dim.sunny.every(v => v > 0.9) && dim.shade.every(v => v < 0.3) && dim.col < 0.55, JSON.stringify(dim));
  /* the radio: X held and let go, and someone answers */
  await p.waitForFunction(() => __moon.radio.ready, null, { timeout: 120000 });
  await p.evaluate(() => { __dev.controllers.left.updateButtonValue('x-button', 1); });
  await p.waitForFunction(() => __moon.radio.transmitting, null, { timeout: 60000 });
  await frames(6);
  await p.evaluate(() => { __dev.controllers.left.updateButtonValue('x-button', 0); });
  await p.waitForFunction(() => __moon.radio.acks >= 1, null, { timeout: 180000 });
  ok('the radio: your transmission is answered', true, await p.evaluate(() => __moon.radio.last.who + ': ' + __moon.radio.last.text));
  /* the flag: he fetches it and holds it; a grip on the pole and a push down plants it */
  await p.evaluate(() => { __moon.ops.st.wait = 100; __moon.lmp.S.rate = 4; });
  await p.waitForFunction(() => __moon.ops.st.step === 'flag-plant', null, { timeout: 600000 });
  const planted = await p.evaluate(async () => {
    const o = __moon, F = o.ops.st.flagAt, c = __dev.controllers.right, frame = () => new Promise(r => { o.afterFrame = r; }), V3 = o.rig.position.constructor;
    o.S.pos.set(F.x + 0.7, 0, F.z); o.S.heading = 0; await frame(); await frame(); await frame();
    const hand = y => { const v = o.rig.worldToLocal(new V3(F.x, F.y + y, F.z)); c.position.set(v.x, v.y, v.z); };
    hand(1.4); await frame(); await frame(); c.updateButtonValue('trigger', 1); await frame(); await frame();
    for (let k = 1; k <= 12; k++) { hand(1.4 - 0.04 * k); await frame(); }
    c.updateButtonValue('trigger', 0); await frame();
    return { step: o.ops.st.step, jumping: !!(o.S.teleporting), planted: +o.ops.st.planted.toFixed(2) };
  });
  ok('the flag: a trigger on the pole and a push down plants it', planted.step === 'flag-up' && planted.planted >= 1, JSON.stringify(planted));
  await p.waitForFunction(() => __moon.ops.st.step === 'rover', null, { timeout: 600000 });
  ok('the flag is out on its crossbar', await p.evaluate(() => __moon.ops.st.unfurl === 1 && __moon.ops.flag.visible));
  /* the rover: a grip by the left seat, the stick drives, he rides along; A steps off */
  const drove = await p.evaluate(async () => {
    const o = __moon, R = o.ops.R, c = __dev.controllers, frame = () => new Promise(r => { o.afterFrame = r; });
    /* six metres off, a trigger takes hold of nothing and does not jump: it says why */
    o.S.pos.set(R.x + 6 * Math.cos(R.yaw), 0, R.z - 6 * Math.sin(R.yaw)); await frame(); await frame(); await frame();
    c.right.position.set(0.2, 1.2, -0.3); c.right.updateButtonValue('trigger', 1); await frame(); await frame(); await frame();
    const warn = o.toast.text, far = !o.ops.driving; c.right.updateButtonValue('trigger', 0); await frame(); await frame();
    const board = o.ops.rover.userData.nav.last;
    /* beside it, on its right, a trigger seats you */
    o.S.pos.set(R.x + 2.3 * Math.cos(R.yaw), 0, R.z - 2.3 * Math.sin(R.yaw)); await frame(); await frame(); await frame();
    c.right.updateButtonValue('trigger', 1); await frame(); await frame(); await frame(); c.right.updateButtonValue('trigger', 0);
    const on = o.ops.driving, x0 = R.x, z0 = R.z;
    c.left.updateAxes('thumbstick', 0, -1);
    for (let k = 0; k < 40; k++) await frame();
    const snd = +o.suit.driveLevel.toFixed(3), nav = o.ops.rover.userData.nav.last, tracks = o.prints.tracks;
    c.left.updateAxes('thumbstick', 0, 0);
    const d = +Math.hypot(R.x - x0, R.z - z0).toFixed(1), mate = o.lmp.S.phase, eyeUp = +(o.camera.position.clone().setFromMatrixPosition(o.camera.matrixWorld).y - R.y).toFixed(2);
    for (let k = 0; k < 80 && Math.abs(R.v) > 0.25; k++) await frame();
    c.right.updateButtonValue('a-button', 1); await frame(); await frame(); c.right.updateButtonValue('a-button', 0); await frame(); await frame();
    for (let k = 0; k < 4; k++) await frame();
    /* the boulders at station 1: there from the start, and solid */
    const b = o.ops.solids[0], q = o.clearOf(b[0], b[1]);
    /* messages: off unless asked for (the radio's words in text were skipped), and a long one goes on lines */
    const skipped = o.toast.skipped; o.toast.say('A long message that would have been squeezed on to one line before, hard to read in the headset, now on lines', 0.5, true);
    return { on, d, mate, eyeUp, off: !o.ops.driving, after: o.lmp.S.phase, warn, far, board, skipped, lines: o.toast.lines, snd, quiet: o.suit.driveLevel, nav, tracks, laid: o.ops.tr.laid, tg: (o.ops.target() || {}).name,
      rows: o.ops.status().map(r => r.join(' ')).join(' | '), boulders: o.ops.solids.length, out: +Math.hypot(q[0] - b[0], q[1] - b[1]).toFixed(2), r: b[2] };
  });
  ok('the rover is heard through the suit while it runs, leaves its tracks, and its console shows the way', drove.snd > 0.01 && drove.quiet === 0 && drove.tracks >= 2 * Math.floor(drove.d / 0.5) - 4 && drove.tracks > 4 && /\d/.test(drove.nav), JSON.stringify({ snd: drove.snd, quiet: drove.quiet, tracks: drove.tracks, d: drove.d, nav: drove.nav }));
  ok('the wrist panel points the way; the boulders of station 1 stand there, solid', drove.tg === 'The rover' && /[←-↙]\s+\d+ m/.test(drove.rows) && drove.boulders >= 6 && drove.out >= drove.r, JSON.stringify({ tg: drove.tg, rows: drove.rows, boulders: drove.boulders, out: drove.out, r: drove.r }));
  ok('near things a trigger does not jump and says why; messages are off unless asked for, and long ones go on lines', drove.far && /Cannot teleport/.test(drove.warn) && /board/.test(drove.board) && drove.skipped > 0 && drove.lines >= 2, JSON.stringify({ warn: drove.warn, far: drove.far, board: drove.board, skipped: drove.skipped, lines: drove.lines }));
  ok('the rover: seated by a trigger beside it, the stick drives it, he rides along, A steps off', drove.on && drove.d > 3 && drove.mate === 'ride' && drove.eyeUp > 1.3 && drove.eyeUp < 1.9 && drove.off && drove.after === 'roam', JSON.stringify(drove));
  /* the samples: a rock lifted with a grip and let go at the rover's bags; the rest by the test's short cut */
  const smp = await p.evaluate(async () => {
    const o = __moon, c = __dev.controllers.right, frame = () => new Promise(r => { o.afterFrame = r; }), V3 = o.rig.position.constructor;
    o.ops.jump('samples'); o.ops.rate = 3;
    const a = o.ops.smp.at; o.ops.R.x = a.x + 4; o.ops.R.z = a.z + 1; await frame(); await frame();
    const n = o.ops.smp.nearest(a.x, a.z), gy = o.moon.heightAt(n.x, n.z);
    const hand = (x, y, z) => { const v = o.rig.worldToLocal(new V3(x, y, z)); c.position.set(v.x, v.y, v.z); };
    o.S.pos.set(n.x + 0.6, 0, n.z); o.S.heading = 0; await frame(); await frame(); await frame();
    hand(n.x, gy + 0.9, n.z); await frame(); await frame(); c.updateButtonValue('squeeze', 1); await frame(); await frame(); await frame();
    const held = o.ops.smp.rocks.some(r => r.state === 'held');
    const bag = o.ops.rover.localToWorld(o.ops.rover.userData.bag.clone());
    o.S.pos.set(bag.x + 0.7, 0, bag.z); await frame(); await frame(); await frame();
    for (let k = 0; k < 8; k++) { hand(bag.x, bag.y + 0.15, bag.z); await frame(); }
    c.updateButtonValue('squeeze', 0); await frame(); await frame();
    return { held, kg: o.ops.smp.kg, rows: o.ops.status().length, mine: o.ops.smp.mine, said: o.toast.text };
  });
  ok('a sample: lifted with a grip, let go at the rover, weighed; you are told it is in the bag', smp.held && smp.kg > 5 && smp.rows === 2 && smp.mine === 1 && /In the bag/.test(smp.said), JSON.stringify(smp));
  /* your crewmate collects too: he fetches a rock (not yours) and stows it; the radio keeps the count */
  /* (he rode out with you: the test moved the rover without him, so he is set beside it) */
  await p.evaluate(() => { const o = __moon, R = o.ops.R, L = o.lmp.S; o.S.pos.set(R.x + 1.5, 0, R.z + 1.5); L.pos.set(R.x - 2.5, o.moon.heightAt(R.x - 2.5, R.z + 2), R.z + 2); L.rate = 4; });
  const seenH = {};
  for (let k = 0; k < 900; k++) {
    const h = await p.evaluate(() => ({ s: __moon.ops.H.s, n: __moon.ops.H.n })); seenH[h.s] = true;
    if (h.n >= 1) break;
    await p.waitForTimeout(250);
  }
  const helped = await p.evaluate(() => { const o = __moon, s = o.ops.smp; return { n: s.n, mine: s.mine, kg: s.kg, his: o.ops.H.n, first: !!o.ops.st.said['samples-first'], at: !!o.ops.st.said['samples-at'], twenty: !!o.ops.st.said['samples-20'], lines: ['samples-first', 'samples-20', 'samples-80', 'lmp-rock', 'film-pan', 'film-wave', 'film-end'].filter(c => !o.radio.lines || o.radio.lines.some(l => l.cue === c)).length }; });
  ok('your crewmate fetches rocks too, and the radio keeps count', seenH.go && seenH.carry && helped.his >= 1 && helped.n === helped.mine + helped.his && helped.kg > smp.kg && helped.first && helped.at && helped.twenty === (helped.kg >= 20), Object.keys(seenH).join(' ') + ' ' + JSON.stringify(helped));
  /* the film: the rover parked before the LM, he goes to stand for it, you stand by him: nine seconds are kept */
  await p.evaluate(() => { const o = __moon, R = o.ops.R, v = o.lm.localToWorld(new o.rig.position.constructor(4, 0, 26)); o.ops.jump('film-go'); R.x = v.x; R.z = v.z; R.yaw = Math.atan2(-(o.lm.position.x - v.x), -(o.lm.position.z - v.z)); o.S.pos.set(v.x + 2.5, 0, v.z + 2.5); o.lmp.S.rate = 4; });
  await p.waitForFunction(() => __moon.ops.st.step === 'film-pose' && __moon.lmp.arrived, null, { timeout: 600000 });
  await p.evaluate(() => { const o = __moon, F = o.ops.F; o.S.pos.set(F.me[0], 0, F.me[1]); });
  await p.waitForFunction(() => __moon.ops.F.canGo, null, { timeout: 600000 });
  await frames(6);
  const idle = await p.evaluate(() => ({ step: __moon.ops.st.step, n: __moon.ops.rec.frames.length, a: __moon.ops.button('a') }));
  await p.waitForFunction(() => __moon.ops.st.step === 'film-rec', null, { timeout: 600000 });
  /* both arms up for the camera (owner: "I waved with both arms up, and that did not show") */
  await p.evaluate(() => { const hd = __dev.position; __dev.controllers.left.position.set(hd.x - 0.3, hd.y + 0.45, hd.z - 0.1); __dev.controllers.right.position.set(hd.x + 0.3, hd.y + 0.45, hd.z - 0.1); });
  await p.waitForFunction(() => __moon.ops.st.step === 'load', null, { timeout: 600000 });
  await p.evaluate(() => { __dev.controllers.left.position.set(-0.25, 1.0, -0.3); __dev.controllers.right.position.set(0.25, 1.0, -0.3); });
  const kept = await p.evaluate(() => { const r = __moon.ops.rec, a = r.sample(r.length / 2, new Float32Array(48), { yaw: [3, 23, 41], quat: [5, 10], hold: [42, 44] });
    return { n: r.frames.length, len: +r.length.toFixed(1), q: +Math.hypot(a[5], a[6], a[7], a[8]).toFixed(3), legs: +a[15].toFixed(2), him: +a[27].toFixed(2) }; });
  ok('the film: nothing is kept until you say so (A); then the count, and twelve seconds of you both', idle.step === 'film-pose' && idle.n === 0 && idle.a && kept.n > 20 && kept.len > 10 && Math.abs(kept.q - 1) < 0.01, JSON.stringify(idle) + ' ' + JSON.stringify(kept));
  /* the sample bag from the back of the rover */
  const bagged = await p.evaluate(async () => {
    const o = __moon, R = o.ops.R, c = __dev.controllers.right, frame = () => new Promise(r => { o.afterFrame = r; });
    const bag = o.ops.rover.localToWorld(o.ops.rover.userData.bag.clone());
    o.S.pos.set(bag.x + 0.9 * Math.sin(R.yaw), 0, bag.z + 0.9 * Math.cos(R.yaw)); await frame(); await frame(); await frame();
    c.updateButtonValue('squeeze', 1); await frame(); await frame(); await frame(); c.updateButtonValue('squeeze', 0); await frame();
    return o.ops.F.bag;
  });
  ok('the sample bag: a grip at the back of the rover', bagged);
  /* he goes up first: to the footpad, up the ladder, in over the porch; the ladder is not yours meanwhile */
  const busy = await p.evaluate(() => ({ boarding: __moon.lmp.boarding, free: __moon.ops.ladderFree }));
  const went = {};
  for (let k = 0; k < 3000; k++) {
    const ph = await p.evaluate(() => ({ p: __moon.lmp.S.phase, in: __moon.lmp.aboard, y: __moon.lmp.S.pos.y - __moon.lm.position.y }));
    went[ph.p] = Math.max(went[ph.p] ?? -9, +ph.y.toFixed(1));
    if (ph.in) break;
    await p.waitForTimeout(200);
  }
  ok('he goes up first: the footpad, the jump, the ladder, the porch, and in (the ladder his meanwhile)', busy.boarding && !busy.free && 'hopon' in went && 'up' in went && 'crawlin' in went && await p.evaluate(() => __moon.lmp.aboard && __moon.ops.ladderFree && !__moon.lmp.on), JSON.stringify(went));
  /* back up: by the footpad a grip puts you on the pad at the ladder; A climbs, into the cabin */
  await p.evaluate(() => { const o = __moon, v = o.lm.localToWorld(new o.rig.position.constructor(0, 0, 5.5)); o.S.pos.set(v.x, 0, v.z); });
  await frames(4);
  const near = await p.evaluate(() => __moon.atLadder());
  await p.evaluate(() => { __dev.controllers.right.updateButtonValue('squeeze', 1); });
  await p.waitForFunction(() => __moon.egress.E.stage === 'pad' && __moon.egress.fade < 0.2, null, { timeout: 300000 });
  await p.evaluate(() => { __dev.controllers.right.updateButtonValue('squeeze', 0); });
  ok('by the footpad a grip takes you onto the pad at the ladder', near && await p.evaluate(() => __moon.egress.active));
  await frames(3);
  await p.evaluate(async () => { __dev.controllers.right.updateButtonValue('a-button', 1); await new Promise(r => setTimeout(r, 400)); __dev.controllers.right.updateButtonValue('a-button', 0); });
  const up = {};
  for (let k = 0; k < 2400; k++) {
    let st = await p.evaluate(() => ({ stage: __moon.egress.E.stage, auto: __moon.egress.E.auto }));
    up[st.stage] = true;
    if (st.stage === 'cabin') break;
    if (st.stage === 'pad' && !st.auto) await p.evaluate(async () => { __dev.controllers.right.updateButtonValue('a-button', 1); await new Promise(r => setTimeout(r, 400)); __dev.controllers.right.updateButtonValue('a-button', 0); });
    await p.waitForTimeout(250);
  }
  await frames(4);
  const e9 = await eye();
  ok('A climbs: the jump for the last rung, the ladder, the porch, and you stand in the cabin again', up.hop && up.path && up.in && up.cabin && e9[2] < 1.2 && e9[1] > 3.6 && e9[1] < 4.3, Object.keys(up).join(' ') + ' ' + JSON.stringify(e9));
  /* aboard with the samples: he is in, the hatch shuts, the count, the lift-off; at 50 m the finale */
  await p.waitForFunction(() => __moon.ops.st.step === 'aboard', null, { timeout: 120000 });
  ok('aboard with the samples: he is in the cabin too', await p.evaluate(() => __moon.lmc.lmp.visible && !__moon.lmp.on));
  await p.waitForFunction(() => __moon.finale, null, { timeout: 900000 });
  const up2 = await p.evaluate(() => ({ alt: +__moon.ops.F.alt.toFixed(0), door: __moon.egress.E.door, y: +(__moon.lm.position.y).toFixed(1) }));
  ok('the hatch shut, the lift-off to 50 m, and the view goes to the console', up2.alt >= 50 && up2.door === 0, JSON.stringify(up2));
  /* the film's far pass is in the picture (the sky is drawn, not left empty: the mountains and stars were lost in the headset) */
  await p.waitForFunction(() => __moon.ops.F.parts && __moon.ops.F.part === 'pan' && __moon.ops.F.on === 1, null, { timeout: 900000 });
  const skyA = await p.evaluate(() => new Promise(r => { __moon.afterFrame = () => { const px = new Uint8Array(4); __moon.renderer.readRenderTargetPixels(__moon.filmRT, 400, 585, 1, 1, px); r(px[3]); }; }));
  ok("the film's sky and far ground are drawn", skyA === 255, 'alpha ' + skyA);
  await p.waitForFunction(() => __moon.ops.F.parts && __moon.ops.F.part === 'wave' && __moon.ops.F.on === 1, null, { timeout: 900000 });
  const parts = await p.evaluate(() => __moon.ops.F.parts.map(q => q.k + ':' + q.d.toFixed(0)));
  /* the wave in the film: your stand-in's arms are up, as yours were; Houston talks over the pictures */
  await p.waitForFunction(() => { const F = __moon.ops.F; return F.part !== 'wave' || F.tf > F.cur.t0 + 6; }, null, { timeout: 900000 });
  const waved = await p.evaluate(() => new Promise(r => { __moon.afterFrame = () => { const g = __moon.ghost, J = g.userData.joints, up = []; g.updateMatrixWorld(true);
    for (const k of ['armL', 'armR']) { const e = J[k].matrixWorld.elements; up.push(+(-e[5] / Math.hypot(e[4], e[5], e[6])).toFixed(2)); }
    r({ part: __moon.ops.F.part, up, said: ['film-pan', 'film-wave'].map(c => __moon.radio.heard ? __moon.radio.heard(c) : null) }); }; }));
  ok('the film shows both your arms up, as you held them', waved.part === 'wave' && waved.up.every(v => v > 0.7), JSON.stringify(waved));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'egress-film-wave.png') });
  const whole = await p.evaluate(() => __moon.shadows);
  await p.waitForFunction(() => __moon.ops.F.part === 'liftoff' && __moon.ops.F.on === 1, null, { timeout: 900000 });
  await p.waitForFunction(() => __moon.shadows.split, null, { timeout: 900000 });
  const two = await p.evaluate(() => __moon.shadows);
  /* the stages part cleanly: the cut follows the LM's own tilt, the plume deflectors stay on the descent stage, the
   * ascent stage has its engine's bell */
  const cut = await p.evaluate(() => { const o = __moon, up = new o.rig.position.constructor(0, 1, 0).transformDirection(o.lmDescent.matrixWorld);
    return { asc: +o.clipAsc.normal.dot(up).toFixed(4), desc: +o.clipDesc.normal.dot(up).toFixed(4), tilt: +up.y.toFixed(4), stays: o.lmDescent.userData.descentTop.visible && !o.lmModel.userData.descentTop.visible, bell: !o.lmDescent.userData.ascentOnly.visible }; });
  ok("the stages part cleanly: the cut along the LM's own deck, the deflectors left behind", cut.asc > 0.9999 && cut.desc < -0.9999 && cut.stays && cut.bell, JSON.stringify(cut));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'egress-film-liftoff.png') });
  ok("the film's scenes in order; the LM's shadow whole while it stands, in two once the ascent stage is off", parts[0].startsWith('pan') && parts.some(q => q.startsWith('wave:12')) && parts[parts.length - 1].startsWith('liftoff') && whole.whole && !whole.split && two.split && two.asc && two.desc && !two.whole, JSON.stringify(parts) + ' ' + JSON.stringify(whole) + ' ' + JSON.stringify(two));
  const e5 = await eyes2();
  ok('the finale: the console and its film, both eyes drawn', e5.every(v => v > 4 && v < 250), e5.map(v => v.toFixed(0)).join(' / '));
  if (outDir) await p.screenshot({ path: path.join(outDir, 'egress-finale.png') });
  await p.waitForFunction(() => __moon.ops.F.end, null, { timeout: 900000 });
  ok('the film ends with the words', await p.evaluate(() => __moon.ops.F.caption === 1));
  ok('no errors', errs.length === 0, errs.slice(0, 3).join(' | '));
  await b.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
