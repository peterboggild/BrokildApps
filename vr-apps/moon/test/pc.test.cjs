/* The journey at a computer: every chapter played on a flat screen with the keyboard and the mouse only, as the
 * band along the top says. For each chapter: the band is there with its keys and (where there is something to do)
 * its hint; each key it lists does what it says; the status and settings open on P and a setting changes with a
 * click; H hides the band; then the chapter is played to its end with keys alone (the number keys to jump, W to
 * run time faster, F / G / the arrows / T where the chapter asks for them) and N opens the next page.
 * Nothing is set by hand in the page's state except, in chapter 6, the pace of the surface missions (ops.rate),
 * which only saves waiting. No headset is emulated here: this is the page as a PC shows it.
 *   python -m http.server 8765        (from the repository root)
 *   set NODE_PATH=<dir>\node_modules  (playwright-core)
 *   node vr-apps/moon/test/pc.test.cjs [page.html ...] */
const { chromium } = require('playwright-core');
const BASE = process.env.BASE || 'http://127.0.0.1:8765/vr-apps/moon/';
let fails = 0;
const ok = (n, c, i = '') => { console.log(`${c ? 'ok  ' : 'FAIL'} ${n}${i ? '  ' + i : ''}`); if (!c) fails++; return c; };
const sleep = ms => new Promise(r => setTimeout(r, ms));

let p, H;                                   /* the page, and the name of its hook */
const ev = (f, a) => p.evaluate(f, a);
const st = code => p.evaluate(([h, c]) => new Function('H', 'return (' + c + ')')(window[h]), [H, code]);
const until = (code, ms = 120000) => p.waitForFunction(([h, c]) => new Function('H', 'return (' + c + ')')(window[h]), [H, code], { timeout: ms, polling: 200 }).then(() => true, () => false);
const frames = async n => { const f0 = await st('H.S.frames'); await until('H.S.frames >= ' + (f0 + n), 60000); };
const key = async k => { await p.keyboard.press(k); };
const chips = () => ev(() => [...document.querySelectorAll('#keysband .kb-k')].map(e => [e.querySelector('kbd').textContent, e.textContent.slice(e.querySelector('kbd').textContent.length), e.classList.contains('hot')]));
const has = async (k, re) => (await chips()).some(([a, b]) => a === k && (!re || re.test(b)));
const hot = async k => (await chips()).some(([a, , h]) => a === k && h);
const hint = () => ev(() => { const e = document.querySelector('#keysband .kb-hint'); return e && e.style.display !== 'none' ? e.textContent : ''; });
const bandShown = () => ev(() => { const e = document.getElementById('keysband'); return !!e && getComputedStyle(e).display !== 'none' && e.getBoundingClientRect().height > 10; });

async function open(page, hook, start) {
  H = hook;
  p = await browser.newPage({ viewport: { width: 1280, height: 720 } });
  p.errs = []; p.on('pageerror', e => p.errs.push(e.message));
  await p.goto(BASE + page);
  await p.waitForFunction(h => window[h] && window[h].S.ready, hook, { timeout: 240000 });
  ok(`${page}: no band before the start`, !(await bandShown()));
  if (start) await p.selectOption('#start', start);
  await p.click('#flat');
  await p.waitForFunction(() => document.body.classList.contains('running'), null, { timeout: 30000 });
  await frames(3);
  ok(`${page}: the band along the top, with its keys`, await bandShown() && (await chips()).length >= 3, (await chips()).map(c => c[0]).join(' | '));
}
/* the things every chapter in its seat offers */
async function seat(page, o = {}) {
  await key('Space'); await frames(2);
  ok(`${page}: Space pauses, and the band says so`, !(await st('H.S.running')) && /Paused/.test(await hint()) && await hot('Space'));
  await key('Space'); await frames(2);
  ok(`${page}: Space goes on`, await st('H.S.running') && !/Paused/.test(await hint()));
  if (o.warp !== false) {
    let w = 1; for (let i = 0; i < 3 && w <= 1; i++) { await key('KeyW'); w = await st('H.S.warp'); if (w <= 1) await sleep(700); }
    ok(`${page}: W runs time faster`, w > 1, 'x' + w);
    for (let i = 0; i < 6; i++) await key('KeyS');
    ok(`${page}: S slower, down to real time`, await st('H.S.warp') === 1);
  }
  if (o.win) {
    await key('KeyC'); ok(`${page}: C opens the big window`, await st('H.cabin.cheat') === true);
    await key('KeyC'); ok(`${page}: C closes it`, await st('H.cabin.cheat') === false);
  }
  if (o.out !== false) {
    await key('KeyO'); const on = await until('H.OUT.on', 30000);
    ok(`${page}: O looks from outside, and the band shows its keys`, on && await has('O', /back inside/) && await has('J L'));
    await key('KeyO'); ok(`${page}: O again: back inside`, await until('!H.OUT.on', 30000) && await has('O', /outside view/));
  }
  await panel(page);
  await key('KeyH'); ok(`${page}: H hides the band`, await ev(() => document.getElementById('keysband').classList.contains('off')));
  await key('KeyH'); ok(`${page}: H brings it back`, await ev(() => !document.getElementById('keysband').classList.contains('off')));
}
async function panel(page) {
  await key('KeyP'); await frames(4);
  const shown = await ev(() => { const e = document.getElementById('kb-panel'); return getComputedStyle(e).display !== 'none' && /Status/.test(e.textContent) && /Settings/.test(e.textContent) && e.querySelectorAll('.kb-row').length; });
  const before = await ev(() => document.querySelector('#kb-panel .kb-set b').textContent);
  await p.click('#kb-panel .kb-set button[data-d="1"]');
  const after = await ev(() => document.querySelector('#kb-panel .kb-set b').textContent);
  await p.click('#kb-panel .kb-set button[data-d="-1"]');
  const back = await ev(() => document.querySelector('#kb-panel .kb-set b').textContent);
  ok(`${page}: P opens the status and the settings; a click changes a setting`, shown > 3 && after !== before && back === before, `${shown} rows; "${before}" -> "${after}"`);
  await key('KeyP'); ok(`${page}: P closes it`, await ev(() => getComputedStyle(document.getElementById('kb-panel')).display === 'none'));
}
/* to the end with the keys alone: W whenever time has dropped back to real time, then N */
async function finish(page, next, o = {}) {
  const t0 = Date.now(); let ended = false;
  while (!ended && Date.now() - t0 < (o.ms || 420000)) {
    if (o.warp !== false && await st('H.S.running && H.S.warp < ' + (o.max || 10))) await key('KeyW');
    ended = await until('H.S.ended' + (o.end ? ' || ' + o.end : ''), 1500);
  }
  await frames(3);
  ok(`${page}: played to its end with the keys (${((Date.now() - t0) / 1000).toFixed(0)} s)`, ended);
  ok(`${page}: at the end the band offers N, and says where it goes`, await hot('N') && /N /.test(await hint()), await hint());
  await key('KeyN');
  let went = false; try { await p.waitForURL(u => u.toString().includes(next), { timeout: 30000 }); went = true; } catch (e) { /* stays */ }
  ok(`${page}: N opens ${next}`, went, went ? '' : 'still on ' + p.url());
  ok(`${page}: no errors`, p.errs.length === 0, p.errs.slice(0, 2).join(' | '));
  await p.close();
}
const jumped = async (page, k, code) => { const a = await st('H.S.t'); await key(k); await frames(2); const b = await st('H.S.t'); ok(`${page}: ${k.replace('Digit', '')} jumps in time`, Math.abs(b - a) > 5 && (!code || await st(code)), `${a.toFixed(0)} -> ${b.toFixed(0)}`); };

const RUN = {
  async 'briefing.html'() {
    await open('briefing.html', '__brief');
    const n0 = await st('H.S.slide'); await key('ArrowRight'); await frames(2);
    ok('briefing.html: the arrow goes to the next board', await st('H.S.slide') === n0 + 1 && await has('→'));
    await key('ArrowLeft'); await frames(2); ok('briefing.html: and back', await st('H.S.slide') === n0);
    let went = false;
    for (let i = 0; i < 40 && !went; i++) { await key('ArrowRight'); await sleep(250); went = p.url().includes('launch.html'); }
    ok('briefing.html: the last board\'s arrow opens launch.html', went); await p.close();
  },
  async 'launch.html'() {
    await open('launch.html', '__launch');
    ok('launch.html: boarding: the band says how to walk and to climb in', await hot('W A S D') && await hot('E') && /swing arm/.test(await hint()));
    const z0 = await st('H.W.z'); await p.keyboard.down('KeyW'); await frames(8); await p.keyboard.up('KeyW');
    ok('launch.html: W walks along the swing arm', Math.abs(await st('H.W.z') - z0) > 0.3, `${z0.toFixed(2)} -> ${(await st('H.W.z')).toFixed(2)}`);
    await panel('launch.html');
    await key('KeyE'); ok('launch.html: E climbs in', await until("H.S.mode === 'seated'", 60000));
    await frames(3);
    if (!(await st('H.S.started'))) { ok('launch.html: in the couch, the band offers Space to start the count', await hot('Space') && /Space starts/.test(await hint())); await key('Space'); await frames(2); }
    ok('launch.html: the count runs', await st('H.S.started && H.S.running'));
    await seat('launch.html', { win: true });
    await jumped('launch.html', 'Digit5');
    await finish('launch.html', 'orbit.html', { end: 'H.S.t >= 800', max: 16 });
  },
  async 'orbit.html'() { await open('orbit.html', '__orbit'); await seat('orbit.html', { win: true }); await jumped('orbit.html', 'Digit6'); await finish('orbit.html', 'lunar.html', { max: 10000 }); },
  async 'lunar.html'() { await open('lunar.html', '__lunar'); await seat('lunar.html', { win: true }); await jumped('lunar.html', 'Digit7'); await finish('lunar.html', 'lm.html', { max: 10000 }); },
  async 'lm.html'() {
    await open('lm.html', '__lm');
    ok('lm.html: in America: the band offers F through the tunnel', await hot('F') && /tunnel/.test(await hint()), await hint());
    await key('KeyF');
    ok('lm.html: F floats you through the tunnel into the LM', await until("H.S.mode === 'lm'", 240000));
    await frames(3);
    await seat('lm.html');
    await jumped('lm.html', 'Digit4');
    await finish('lm.html', 'descent.html', { max: 10000 });
  },
  async 'descent.html'() {
    await open('descent.html', '__descent');
    await seat('descent.html');
    await jumped('descent.html', 'Digit4');
    let offered = false; const t0 = Date.now();
    while (!offered && Date.now() - t0 < 300000) { if (await st('H.S.warp < 10')) await key('KeyW'); offered = await has('T', /take over/); if (!offered) await sleep(800); }
    ok('descent.html: low down the band offers T: take over', offered && await hot('T'), await hint());
    await key('KeyT'); await frames(2);
    ok('descent.html: T: you have her, and the band shows how to fly', await st("H.S.mode === 'hand'") && await hot('← → ↑ ↓') && await has('[  ]'));
    ok('descent.html: the figures you fly by are open (the panel)', await ev(() => getComputedStyle(document.getElementById('kb-panel')).display !== 'none' && /Altitude|Descent|Height|rate/i.test(document.getElementById('kb-panel').textContent)), await ev(() => [...document.querySelectorAll('#kb-panel .kb-row span')].map(e => e.textContent).join(', ')));
    const tilt0 = await st('JSON.stringify(H.S.tilt)'); await p.keyboard.down('ArrowUp'); await frames(5); await p.keyboard.up('ArrowUp');
    ok('descent.html: the arrow keys tilt her', await st('JSON.stringify(H.S.tilt)') !== tilt0, tilt0 + ' -> ' + await st('JSON.stringify(H.S.tilt)'));
    const rod0 = await st('H.S.rod'); await key('BracketRight'); ok('descent.html: ] slows the descent', await st('H.S.rod') > rod0);
    await key('KeyT'); await frames(2); ok('descent.html: T gives her back to the computer', await st("H.S.mode !== 'hand'"));
    await key('KeyP');
    await finish('descent.html', 'surface.html', { ms: 600000 });
  },
  /* chapter 6: the whole of it, from the cabin to the film, with W A S D, the mouse, G, F and N */
  async 'surface.html'() {
    const P = 'surface.html';
    await open(P, '__moon');
    const info = () => st(`(() => { const o = H.ops, S = H.S, R = o.R, b = o.rover.localToWorld(o.rover.userData.bag.clone()); let rock = null, bd = 1e9;
      for (const r of o.smp.rocks) { if (r.state !== 'ground') continue; const d = Math.hypot(r.mesh.position.x - S.pos.x, r.mesh.position.z - S.pos.z); if (d < bd) { bd = d; rock = [r.mesh.position.x, r.mesh.position.z]; } }
      return { step: o.st.step, x: S.pos.x, z: S.pos.z, h: S.heading + S.yaw * Math.PI / 180, R: [R.x, R.z, R.yaw, R.v], driving: R.driving, odo: o.F.odo, kg: o.smp.kg, done: o.smp.done, rock, bag: [b.x, b.z], has: !!o.F.bag,
        flag: o.st.flagAt ? [o.st.flagAt.x, o.st.flagAt.z] : null, me: o.F.me || null, canGo: !!o.F.canGo, station: [o.station.x, o.station.z], ladder: H.ladderAt, atLadder: H.atLadder(), free: o.ladderFree, eg: H.egress.active ? H.egress.E.stage : '', end: !!o.F.end, finale: H.finale }; })()`);
    const wrap = a => Math.atan2(Math.sin(a), Math.cos(a));
    /* turn with the mouse, walk with W (Shift: faster), till within d of the spot */
    async function walkTo(get, d, ms = 240000) {
      const t0 = Date.now(); let i = await info(), near = false;
      while (Date.now() - t0 < ms) {
        i = await info(); const [tx, tz] = get(i), dist = Math.hypot(tx - i.x, tz - i.z);
        if (dist < d) { near = true; break; }
        const turn = wrap(Math.atan2(-(tx - i.x), -(tz - i.z)) - i.h) * 180 / Math.PI;
        if (Math.abs(turn) > 4) { const px = -turn / 0.2; await p.keyboard.up('KeyW'); await p.mouse.move(640 - px / 2, 420); await p.mouse.down(); await p.mouse.move(640 + px / 2, 420, { steps: 6 }); await p.mouse.up(); await frames(1); }
        if (dist > 6) await p.keyboard.down('ShiftLeft'); else await p.keyboard.up('ShiftLeft');
        await p.keyboard.down('KeyW'); await frames(dist > 6 ? 3 : 1);
      }
      await p.keyboard.up('KeyW'); await p.keyboard.up('ShiftLeft'); await frames(1);
      return near;
    }
    /* W forward, A and D steer, till within d of the spot; then let her roll to a stand */
    async function driveTo(get, d, ms = 400000) {
      const t0 = Date.now(); let near = false, turnKey = null;
      const steer = async k => { if (k === turnKey) return; if (turnKey) await p.keyboard.up(turnKey); if (k) await p.keyboard.down(k); turnKey = k; };
      await p.keyboard.down('KeyW');
      while (Date.now() - t0 < ms) {
        const i = await info(), [tx, tz] = get(i), [rx, rz, yaw] = i.R;
        if (Math.hypot(tx - rx, tz - rz) < d) { near = true; break; }
        const err = wrap(Math.atan2(-(tx - rx), -(tz - rz)) - yaw);
        await steer(err > 0.12 ? 'KeyA' : err < -0.12 ? 'KeyD' : null);
        await frames(1);
      }
      await steer(null); await p.keyboard.up('KeyW');
      await until('Math.abs(H.ops.R.v) < 0.25', 60000);
      return near;
    }
    /* ---- out of the cabin and down the ladder */
    ok(`${P}: in the cabin the band is there, and says what is happening`, /hatch|vacuum/.test(await hint()), await hint());
    { const t0 = Date.now(); let last = '';
      while (await st('H.egress.active') && Date.now() - t0 < 600000) {
        const g = await st('H.egress.E.stage'); if (g !== last) { last = g; console.log('     stage', g, '|', await hint()); }
        if (await hot('F') && (await st("H.egress.E.stage === 'cabin' ? H.egress.E.door >= 1 : !H.egress.E.auto"))) await key('KeyF');
        await sleep(700);
      } }
    ok(`${P}: F takes you out of the hatch, down the ladder and on to the Moon`, !(await st('H.egress.active')));
    await frames(3);
    ok(`${P}: on the ground the band shows the walking keys`, await has('W A S D') && await has('Shift') && /Walk about/.test(await hint()), await hint());
    await panel(P);
    await ev(() => { __moon.ops.rate = 3; });                    /* (the missions three times as fast: only to save waiting) */
    const a0 = await info(); await p.keyboard.down('KeyW'); await frames(6); await p.keyboard.up('KeyW'); const a1 = await info();
    ok(`${P}: W walks`, Math.hypot(a1.x - a0.x, a1.z - a0.z) > 0.4, Math.hypot(a1.x - a0.x, a1.z - a0.z).toFixed(2) + ' m');
    /* ---- the flag */
    ok(`${P}: your crewmate brings the flag`, await until("H.ops.st.step === 'flag-plant'", 600000));
    await frames(2);
    ok(`${P}: the band says how to plant it`, await hot('G') && /press G/.test(await hint()), await hint());
    ok(`${P}: walked to the flag (mouse to turn, W to walk)`, await walkTo(i => i.flag, 1.3));
    await key('KeyG');
    ok(`${P}: G plants the flag`, await until("H.ops.st.step === 'flag-up' || H.ops.st.step === 'rover'", 20000));
    /* ---- the rover */
    ok(`${P}: the flag flies; next the rover`, await until("H.ops.st.step === 'rover'", 120000));
    await frames(2); ok(`${P}: the band says how to get on the rover`, await hot('G') && /climb on/.test(await hint()), await hint());
    ok(`${P}: walked to the rover`, await walkTo(i => [i.R[0], i.R[1]], 2.6));
    await key('KeyG'); await frames(2);
    ok(`${P}: G climbs on, and the band shows the driving keys`, (await info()).driving && await hot('W S') && await hot('A D') && await has('F', /step off/), await hint());
    ok(`${P}: drove to the boulder field with W, A and D`, await driveTo(i => i.station, 7) && (await info()).step === 'samples', JSON.stringify((await info()).R.map(v => +v.toFixed(1))));
    await key('KeyF'); await frames(2);
    ok(`${P}: F steps off`, !(await info()).driving);
    /* ---- the samples */
    ok(`${P}: the band says how to collect rocks, and counts them`, await hot('G') && /kg/.test(await hint()), await hint());
    { const t0 = Date.now(); let n = 0;
      while (!(await info()).done && Date.now() - t0 < 900000) {
        const i = await info(); if (!i.rock) { await sleep(1000); continue; }
        if (await walkTo(j => j.rock || [j.x, j.z], 1.7, 90000)) { await key('KeyG'); n++; await frames(2); }
      }
      ok(`${P}: the rocks are collected (G at each: ${n} by you, the rest by your crewmate)`, (await info()).done, (await info()).kg.toFixed(0) + ' kg'); }
    /* ---- the film */
    ok(`${P}: next the film`, await until("H.ops.st.step === 'film-go'", 60000));
    ok(`${P}: back to the rover`, await walkTo(i => [i.R[0], i.R[1]], 2.6)); await key('KeyG'); await frames(2);
    ok(`${P}: drove back to 25 m from the LM`, (await info()).driving && await driveTo(i => { const d = Math.hypot(i.R[0], i.R[1]) || 1; return [i.R[0] / d * 20, i.R[1] / d * 20]; }, 7), JSON.stringify((await info()).R.map(v => +v.toFixed(1))));
    await key('KeyF');
    ok(`${P}: stepped off: the camera is on the LM`, await until("H.ops.st.step === 'film-pose'", 60000), (await info()).step);
    ok(`${P}: walked to your mark by your crewmate`, await walkTo(i => i.me, 1.6));
    ok(`${P}: the band offers F: ready for the camera`, await until('H.ops.F.canGo', 120000) && (await frames(2), await hot('F')), await hint());
    await key('KeyF');
    ok(`${P}: F: the count and the recording`, await until("H.ops.st.step === 'film-rec'", 60000));
    ok(`${P}: cut: time to go home`, await until("H.ops.st.step === 'load'", 120000));
    /* ---- home */
    await frames(2); ok(`${P}: the band says to take the sample bag`, /sample bag/.test(await hint()), await hint());
    ok(`${P}: walked to the back of the rover`, await walkTo(i => i.bag, 1.4)); await key('KeyG'); await frames(2);
    ok(`${P}: G takes the bag`, (await info()).has, await hint());
    ok(`${P}: walked to the foot of the ladder`, await walkTo(i => i.ladder, 1.0) && (await info()).atLadder);
    ok(`${P}: the ladder is free when he is in`, await until('H.ops.ladderFree', 300000));
    await frames(2); ok(`${P}: the band offers F up the ladder`, await hot('F') && /ladder/.test(await hint()), await hint());
    { const t0 = Date.now();
      while ((await info()).step === 'load' && Date.now() - t0 < 600000) {
        if (await hot('F') && (await st("!H.egress.active || !H.egress.E.auto"))) await key('KeyF');
        await sleep(700);
      } }
    ok(`${P}: F takes you up the ladder and in; the hatch is shut`, (await info()).step !== 'load', (await info()).step);
    ok(`${P}: the lift-off, and the film`, await until('H.finale', 600000));
    const end = await until('H.ops.F.end', 900000); await frames(3);
    ok(`${P}: at the film's end the band offers N`, end && await hot('N'), await hint());
    await key('KeyN');
    let went = false; try { await p.waitForURL(u => u.toString().includes('liftoff.html'), { timeout: 30000 }); went = true; } catch (e) { /* stays */ }
    ok(`${P}: N opens liftoff.html`, went); ok(`${P}: no errors`, p.errs.length === 0, p.errs.slice(0, 2).join(' | '));
    await p.close();
  },
  async 'liftoff.html'() { await open('liftoff.html', '__liftoff'); await seat('liftoff.html'); await jumped('liftoff.html', 'Digit3'); await finish('liftoff.html', 'rendezvous.html'); },
  async 'rendezvous.html'() {
    await open('rendezvous.html', '__rdv');
    await seat('rendezvous.html');
    await jumped('rendezvous.html', 'Digit4');
    await frames(3);
    ok('rendezvous.html: docked: the band offers F up the tunnel', await hot('F') && /tunnel/.test(await hint()), await hint());
    await key('KeyF');
    ok('rendezvous.html: F takes you up the tunnel to your couch in America', await until("H.S.mode === 'cm'", 300000));
    await finish('rendezvous.html', 'leaving.html', { warp: false });
  },
  async 'leaving.html'() { await open('leaving.html', '__leaving'); await seat('leaving.html', { win: true }); await jumped('leaving.html', 'Digit6'); await finish('leaving.html', 'homeward.html', { max: 10000 }); },
  async 'homeward.html'() {
    await open('homeward.html', '__home');
    await seat('homeward.html', { win: true });
    await jumped('homeward.html', 'Digit2');
    let suit = false; const t0 = Date.now();
    while (!suit && Date.now() - t0 < 300000) { if (await st("H.EVA.state === 'in' && H.S.warp < 10")) await key('KeyW'); suit = await until("H.EVA.state === 'suit'", 1500); }
    await frames(3);
    ok('homeward.html: the time for the walk: the band offers F to suit up', suit && await hot('F') && /SUIT/.test(await hint()), await hint());
    await key('KeyF');
    ok('homeward.html: F flips the switches; helmets on, the hatch opens', await until("H.EVA.state === 'wait'", 180000));
    await frames(3);
    ok('homeward.html: the band offers F out of the hatch', await has('F', /out of the hatch/) && /hatch is open/.test(await hint()));
    await key('KeyF');
    ok('homeward.html: F takes you out (the way out is not cancelled)', await until("H.EVA.state === 'out'", 120000));
    await frames(3);
    ok('homeward.html: outside: the band shows the rail\'s keys', await hot('↑ ↓') && await has('G') && /rail/.test(await hint()), await hint());
    await p.keyboard.down('ArrowUp');
    const atBay = await until('H.EVA.s > H.RAIL_LEN - 0.9', 300000); await p.keyboard.up('ArrowUp');
    await frames(3);
    ok('homeward.html: arrow up takes you down the rail to the bay, and G lights up', atBay && await hot('G') && /G takes/.test(await hint()), await hint());
    await key('KeyG'); await frames(2);
    ok('homeward.html: G takes the cassette', await st('H.EVA.has') && /back along the rail/.test(await hint()));
    await key('KeyF');
    ok('homeward.html: F takes you back to the hatch and in', await until("H.EVA.state === 'done'", 300000));
    await jumped('homeward.html', 'Digit5');
    await finish('homeward.html', 'entry.html', { max: 10000 });
  },
  async 'entry.html'() { await open('entry.html', '__entry'); await seat('entry.html', { win: true }); await jumped('entry.html', 'Digit5'); await finish('entry.html', 'splash.html', { ms: 600000 }); },
  async 'splash.html'() { await open('splash.html', '__entry'); await seat('splash.html', { win: true }); await jumped('splash.html', 'Digit3'); await finish('splash.html', 'index.html', { ms: 600000 }); }
};

let browser;
(async () => {
  browser = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox', '--autoplay-policy=no-user-gesture-required'] });
  const want = process.argv.slice(2);
  for (const page of Object.keys(RUN)) {
    if (want.length && !want.includes(page)) continue;
    try { await RUN[page](); } catch (e) { ok(`${page}: ran`, false, e.message.split('\n')[0]); try { await p.close(); } catch (x) { /* gone */ } }
  }
  await browser.close();
  console.log(fails ? `${fails} FAILED` : 'all passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
