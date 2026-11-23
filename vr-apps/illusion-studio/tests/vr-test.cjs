// Illusion Studio VR in an emulated Quest 3 (IWER). Drives the real input paths: controller rays at panel
// widgets, trigger clicks, grip grabs, sticks and the Y button.
//   node vr-test.cjs <app dir> <out dir>
const fs = require('fs'), path = require('path'), http = require('http');
const LA = process.env.LOCALAPPDATA;
const puppeteer = require(path.join(LA, 'ink2latex-deps', 'node_modules', 'puppeteer-core'));
const IWER = path.join(LA, 'fv-test', 'node_modules', 'iwer', 'build', 'iwer.min.js');
const [, , root, outDir] = process.argv;
fs.mkdirSync(outDir, { recursive: true });
const srv = http.createServer((q, r) => {
  const u = decodeURIComponent(q.url.split('?')[0]);
  if (u === '/iwer.js') { r.writeHead(200, { 'content-type': 'text/javascript' }); r.end(fs.readFileSync(IWER)); return; }
  if (u === '/favicon.ico') { r.writeHead(204); r.end(); return; }
  const f = path.join(root, u === '/' ? 'IllusionStudio.html' : u);
  if (!fs.existsSync(f) || fs.statSync(f).isDirectory()) { r.writeHead(404); r.end(); return; }
  let body = fs.readFileSync(f);
  if (u === '/') body = body.toString('utf8').replace('<head>', () => '<head><script src="/iwer.js"></script><script>window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.stereoEnabled = true; window.__dev.installRuntime({forceInstall:true});</script>');
  r.writeHead(200, { 'content-type': 'text/html; charset=utf-8' }); r.end(body);
}).listen(0, '127.0.0.1');
let fail = 0;
const ok = (name, cond, info = '') => { console.log((cond ? 'ok   ' : 'FAIL ') + name + (info ? '  ' + info : '')); if (!cond) fail++; };
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
function aimQuat(dir) {
  const l = Math.hypot(...dir); const b = dir.map((v) => v / l); const a = [0, 0, -1];
  const c = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const w = 1 + (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]);
  const n = Math.hypot(c[0], c[1], c[2], w);
  return [c[0] / n, c[1] / n, c[2] / n, w / n];
}

(async () => {
  await new Promise((r) => (srv.listening ? r() : srv.once('listening', r)));
  const browser = await puppeteer.launch({
    executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe', headless: 'new',
    args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--window-size=1100,700',
      `--user-data-dir=${path.join(LA, 'Temp', 'isvr-' + Date.now())}`],
  });
  const p = await browser.newPage();
  await p.setViewport({ width: 1100, height: 700 });
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message));
  p.on('console', (m) => { const t = m.text(); if (m.type() === 'error' && !/404|Failed to load resource|profile/i.test(t)) errs.push(t); if (/^DBG/.test(t)) console.log(t); });
  await p.goto(`http://127.0.0.1:${srv.address().port}/?preset=0`, { waitUntil: 'load' });
  await wait(5000);
  const ev = (f, ...a) => p.evaluate(f, ...a);
  const S = () => ev(() => { const I = window.__illusion, V = I.VR, st = I.state();
    const cw = new (V.dolly.constructor === Object ? Object : Object)();
    const c = I.renderer.xr.getCamera(); const e = c.matrixWorld.elements;
    return { session: !!V.session, frames: V.frames, S: V.S, yaw: V.yaw, dolly: V.dolly.position.toArray(), panel: V.panel && V.panel.visible,
      tab: I.UI.tab, sel: st.sel, pattern: st.look.pattern, head: [e[12], e[13], e[14]],
      ball: st.items.find(i => i.name === 'Ball')?.pos, dists: st.dists.map(d => ({ type: d.type, c: d.c })),
      hands: V.hands.map(h => ({ handed: h.handed, mode: h.mode, hover: h.hover && h.hover.kind })) }; });
  const press = async (hand, btn, ms = 300) => { await ev((h, b) => window.__dev.controllers[h].updateButtonValue(b, 1), hand, btn); await wait(ms); await ev((h, b) => window.__dev.controllers[h].updateButtonValue(b, 0), hand, btn); await wait(500); };
  const setHand = (hand, pos, q) => ev((h, P, Q) => { const c = window.__dev.controllers[h]; c.position.set(P[0], P[1], P[2]); if (Q) c.quaternion.set(Q[0], Q[1], Q[2], Q[3]); }, hand, pos, q);
  const from = [0.18, 1.3, -0.25];
  const aimLocal = async (hand, target, src = from) => { await setHand(hand, src, aimQuat([target[0] - src[0], target[1] - src[1], target[2] - src[2]])); await wait(500); };
  const clickWidget = async (label, tab) => {
    const t = await ev((l, tb) => window.__illusion.vrWidgetLocal(l, tb), label, tab);
    if (!t) return `no widget ${label}`;
    await aimLocal('right', t);
    const hov = await ev(() => { const h = window.__illusion.VR.hands.find(x => x.handed === 'right'); return h && h.hover && h.hover.kind; });
    await press('right', 'trigger');
    return hov;
  };

  const sup = await ev(() => window.__illusion.VR.supported);
  ok('headset detected (emulated Quest 3)', sup.vr, JSON.stringify(sup));
  const btnVisible = await ev(() => !document.getElementById('vrBtn').hidden);
  ok('floating Enter VR button shown', btnVisible);
  await p.click('#vrBtn');
  await wait(5000);
  let s = await S();
  ok('VR session runs', s.session && s.frames > 5, `frames ${s.frames}`);
  ok('panel opens on the View tab', s.panel && s.tab === 'view');
  const cam = await ev(() => window.__illusion.state().cam.pos);
  const dh = Math.hypot(...[0, 1, 2].map(i => s.head[i] - cam[i]));
  ok('you start where the desktop camera was', dh < 0.3, `head ${s.head.map(v => v.toFixed(2))} cam ${cam.map(v => v.toFixed(2))}`);
  await p.screenshot({ path: path.join(outDir, '1-entered.png') });

  let hov = await clickWidget('Pattern');
  s = await S();
  ok('trigger on the Pattern tab switches tab', s.tab === 'pattern', `hover ${hov}`);
  hov = await clickWidget('Ouchi', 'pattern');
  s = await S();
  ok('Ouchi preset applied from VR', s.pattern === 'ouchi', `hover ${hov} pattern ${s.pattern}`);
  await p.screenshot({ path: path.join(outDir, '2-pattern-tab.png') });
  await clickWidget('Hex drift', 'pattern');

  await clickWidget('View');
  hov = await clickWidget('Dollhouse', 'view');
  s = await S();
  ok('Dollhouse shrinks the world', s.S < 0.2, `S ${s.S.toFixed(3)} hover ${hov}`);
  await p.screenshot({ path: path.join(outDir, '3-dollhouse.png') });
  const dollS = s.S;
  await clickWidget('Ant', 'view');
  s = await S();
  ok('Ant makes you small', Math.abs(s.S - 15) < 1e-6, `S ${s.S}`);
  ok('…standing on the floor (eyes ~0.1 m above it)', s.head[1] > 0 && s.head[1] < 0.2, `head y ${s.head[1].toFixed(3)}`);
  await p.screenshot({ path: path.join(outDir, '4-ant.png') });
  await clickWidget('Life size', 'view');
  s = await S();
  ok('Life size', Math.abs(s.S - 1) < 1e-6 && s.head[1] > 1.4 && s.head[1] < 1.8, `S ${s.S} head y ${s.head[1].toFixed(2)}`);

  await press('left', 'y-button');
  s = await S();
  ok('Y closes the panel', !s.panel);
  await press('left', 'y-button');
  s = await S();
  ok('Y opens it again', s.panel);
  await clickWidget('×');
  s = await S();
  ok('× closes it', !s.panel);

  // two grips pulled apart: the world gets 3x bigger
  const down = aimQuat([0, -1, -0.3]);
  await setHand('left', [-0.08, 1.2, -0.35], down);
  await setHand('right', [0.08, 1.2, -0.35], down);
  await wait(500);
  const S0 = (await S()).S;
  await ev(() => { window.__dev.controllers.left.updateButtonValue('squeeze', 1); window.__dev.controllers.right.updateButtonValue('squeeze', 1); });
  await wait(600);
  await setHand('left', [-0.24, 1.2, -0.35], down);
  await setHand('right', [0.24, 1.2, -0.35], down);
  await wait(900);
  await ev(() => { window.__dev.controllers.left.updateButtonValue('squeeze', 0); window.__dev.controllers.right.updateButtonValue('squeeze', 0); });
  await wait(500);
  s = await S();
  ok('two grips pulled apart: world 3x bigger', Math.abs(s.S / S0 - 3) < 0.1, `S ${S0.toFixed(3)} -> ${s.S.toFixed(3)}`);

  // one grip: drag the world
  const d0 = s.dolly;
  await ev(() => window.__dev.controllers.right.updateButtonValue('squeeze', 1));
  await wait(400);
  await setHand('right', [0.24, 1.2, -0.05], down);
  await wait(700);
  await ev(() => window.__dev.controllers.right.updateButtonValue('squeeze', 0));
  await wait(400);
  s = await S();
  const mv = Math.hypot(...[0, 1, 2].map(i => s.dolly[i] - d0[i]));
  ok('one grip drags the world', mv > 0.05, `rig moved ${mv.toFixed(3)}`);

  // sticks
  const d1 = s.dolly;
  await ev(() => window.__dev.controllers.left.updateAxes('thumbstick', 0, -1));
  await wait(1500);
  await ev(() => window.__dev.controllers.left.updateAxes('thumbstick', 0, 0));
  await wait(400);
  s = await S();
  const fl = Math.hypot(...[0, 1, 2].map(i => s.dolly[i] - d1[i])); ok('left stick flies', fl > 0.05, `rig moved ${fl.toFixed(3)} (software GL runs at a few fps)`);
  const y0 = s.yaw;
  await ev(() => window.__dev.controllers.right.updateAxes('thumbstick', 1, 0));
  await wait(500);
  await ev(() => window.__dev.controllers.right.updateAxes('thumbstick', 0, 0));
  await wait(400);
  s = await S();
  ok('right stick snap-turns 30°', Math.abs(Math.abs(s.yaw - y0) - Math.PI / 6) < 1e-3, `${((s.yaw - y0) * 180 / Math.PI).toFixed(1)}°`);

  // carry the ball: dollhouse, aim at it, hold the trigger, move the hand 10 cm to the right
  await press('left', 'y-button');
  await clickWidget('Dollhouse', 'view');
  await press('left', 'y-button');
  s = await S();
  const ballLocal = await ev(() => { const I = window.__illusion, st = I.state(); const b = st.items.find(i => i.name === 'Ball');
    const v = I.VR.dolly.worldToLocal(new (I.renderer.xr.getCamera().position.constructor)(...b.pos)); return v.toArray(); });
  const src = [0.15, 1.25, -0.2];
  await aimLocal('right', ballLocal, src);
  s = await S();
  const rh = s.hands.find(h => h.handed === 'right');
  ok('ray finds the ball', rh.hover === 'item', `hover ${rh.hover} ballLocal ${ballLocal.map(v => v.toFixed(2))}`);
  const b0 = s.ball.slice();
  await ev(() => window.__dev.controllers.right.updateButtonValue('trigger', 1));
  await wait(400);
  const q = aimQuat([ballLocal[0] - src[0], ballLocal[1] - src[1], ballLocal[2] - src[2]]);
  await setHand('right', [src[0] + 0.1, src[1], src[2]], q);
  await wait(800);
  await p.screenshot({ path: path.join(outDir, '5-carrying.png') });
  await ev(() => window.__dev.controllers.right.updateButtonValue('trigger', 0));
  await wait(900);
  s = await S();
  const moved = Math.hypot(...[0, 1, 2].map(i => s.ball[i] - b0[i]));
  ok('trigger-drag carries the ball (10 cm of hand = 0.1/S world)', Math.abs(moved - 0.1 / s.S) < 0.25 * 0.1 / s.S, `moved ${moved.toFixed(3)} expected ${(0.1 / s.S).toFixed(3)}`);
  ok('the ball became the selection', s.sel === (await ev(() => window.__illusion.state().items.find(i => i.name === 'Ball').id)));

  // a distortion placed by pointing
  await press('left', 'y-button');
  await clickWidget('Distort');
  await clickWidget('Bump / dent', 'distort');
  hov = await clickWidget('Set the centre: point at a surface', 'distort');
  s = await S();
  await press('left', 'y-button');                    // close the panel so the ray reaches the floor
  const floorLocal = await ev(() => { const I = window.__illusion; const v = I.VR.dolly.worldToLocal(new (I.renderer.xr.getCamera().position.constructor)(-3, 0, 3)); return v.toArray(); });
  await aimLocal('right', floorLocal, src);
  await press('right', 'trigger');
  s = await S();
  const dc = s.dists[0] && s.dists[0].c;
  ok('distortion centre set by pointing at the floor', dc && Math.abs(dc[0] + 3) < 0.3 && Math.abs(dc[2] - 3) < 0.3 && Math.abs(dc[1]) < 1.2, JSON.stringify(dc) + ' (y = the floor already raised by this bump)');
  await wait(800);
  await p.screenshot({ path: path.join(outDir, '6-bump.png') });

  await ev(() => window.__illusion.VR.session.end());
  await wait(1500);
  s = await S();
  ok('left VR: back to the desktop view', !s.session && Math.abs(s.S - 1) < 1e-9 && s.dolly.every(v => v === 0));
  await p.screenshot({ path: path.join(outDir, '7-desktop.png') });
  ok('no errors in the whole run', errs.length === 0, errs.slice(0, 5).join(' | '));
  await browser.close(); srv.close();
  console.log(fail ? `\n${fail} FAILED` : '\nall passed');
  process.exitCode = fail ? 1 : 0;
})().catch((e) => { console.error(e); process.exit(2); });
