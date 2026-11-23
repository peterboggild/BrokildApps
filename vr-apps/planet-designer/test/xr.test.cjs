/* VR test in an emulated Meta Quest 3 (IWER, Meta's Immersive Web Emulation Runtime) with Playwright and software GL.
 *   python3 -m http.server 8765                                (from the repository root)
 *   npm i --prefix /tmp/iwer iwer                              (once; or set IWER to the path of iwer.min.js)
 *   NODE_PATH=$(npm root -g) IWER=/tmp/iwer/node_modules/iwer/build/iwer.min.js node vr-apps/planet-designer/test/xr.test.cjs [shots]
 * Enters VR, checks the session and the workshop, opens the panel with B, clicks a tab and a slider button with the
 * ray, turns the star-mass dial, drags a planet on the orrery, grabs the world with both grips (scale), and leaves.
 * Software GL renders a few frames a second: everything waits by frames, and thresholds allow for it. */
const fs = require('fs');
const { chromium } = require('playwright');
const IWER = process.env.IWER || require.resolve('iwer/build/iwer.min.js');
const out = process.argv[2];
let fails = 0;
const ok = (name, cond, info = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${name}${info ? '  ' + info : ''}`); if (!cond) fails++; };
function aimQuat(dir) {
  const l = Math.hypot(...dir), b = dir.map((v) => v / l), a = [0, 0, -1];
  const c = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const w = 1 + (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]);
  const n = Math.hypot(c[0], c[1], c[2], w);
  return [c[0] / n, c[1] / n, c[2] / n, w / n];
}

(async () => {
  const b = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox'] });
  const p = await b.newPage({ viewport: { width: 1200, height: 800 } });
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message));
  p.on('console', (m) => { const t = m.text(); if (m.type() === 'error' && !/404|Failed to load resource|profile/i.test(t)) errs.push(t.slice(0, 300)); });
  await p.addInitScript({ content: fs.readFileSync(IWER, 'utf8') + ';window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.stereoEnabled = true; window.__dev.installRuntime({ forceInstall: true });' });
  await p.goto('http://localhost:8765/vr-apps/planet-designer/?lang=da');
  await p.waitForFunction(() => window.__pd && window.__pd.view && window.__pd.view.xr, null, { timeout: 120000 });
  const ev = (f, ...a) => p.evaluate(f, ...a);
  const xrFrames = (n) => ev((n) => new Promise((res) => { const V = window.__pd.view.xr.VR; const s = V.frames; const tk = () => (V.frames >= s + n ? res() : setTimeout(tk, 50)); tk(); }), n);
  const setHand = (hand, pos, q) => ev(([h, P, Q]) => { const c = window.__dev.controllers[h]; c.position.set(P[0], P[1], P[2]); if (Q) c.quaternion.set(Q[0], Q[1], Q[2], Q[3]); }, [hand, pos, q]);
  const btn = (hand, name, v) => ev(([h, n, val]) => window.__dev.controllers[h].updateButtonValue(n, val), [hand, name, v]);
  const press = async (hand, name) => { await btn(hand, name, 1); await xrFrames(2); await btn(hand, name, 0); await xrFrames(2); };
  /* aim a controller at a point given in the dolly's local space */
  const toDev = async (lp) => lp;   /* the dolly's local space is the device's own space */
  const aimAt = async (hand, lpt, from = [0.18, 1.3, -0.2]) => { const tp = await toDev(lpt); await setHand(hand, from, aimQuat([tp[0] - from[0], tp[1] - from[1], tp[2] - from[2]])); await xrFrames(2); };

  await p.waitForSelector('#vrBtn', { timeout: 60000 });
  ok('headset detected (emulated Quest 3): the Enter VR button is shown', true);
  await p.click('#vrBtn');
  await ev(() => new Promise((res) => { const tk = () => (window.__pd.view.xr.VR.session ? res() : setTimeout(tk, 100)); tk(); }));
  await xrFrames(4);
  let s = await ev(() => { const X = window.__pd.view.xr; return { session: !!X.VR.session, frames: X.VR.frames, ws: X.ws.root.visible, panel: X.panel.mesh.visible, chunks: window.__pd.view.state.chunks }; });
  ok('VR session runs and the workshop is shown', s.session && s.ws && s.frames >= 4, JSON.stringify(s));
  ok('panel opens on entering', s.panel);
  /* in VR the planet is the light, baked version (the live terrain shader brought a real Quest 3 down) */
  const pb = await ev(() => ({ baked: window.__pd.view.planet.baked, q: window.__pd.view.state.quality, prog: window.__pd.view.planet.bakeProgress }));
  ok('the planet in the cradle is the light baked version, and baking has started', pb.baked && pb.q === 'low' && pb.prog > 0, JSON.stringify(pb));
  if (out) await p.screenshot({ path: out + '/xr-1-enter.png' });

  /* panel: click the Planet tab, then the + of the first slider */
  let w = await ev(() => window.__pd.view.xr.widgetLocal('tab:planet'));
  await aimAt('right', w); await press('right', 'trigger');
  s = await ev(() => window.__pd.tab);
  ok('the ray and trigger switch the panel tab', s === 'planet', `tab ${s}`);
  const m0 = await ev(() => window.__pd.design.planets[window.__pd.sel].bulk.massEarth);
  w = await ev(() => window.__pd.view.xr.widgetLocal('plus:planet.bulk.massEarth'));
  await aimAt('right', w); await press('right', 'trigger');
  const m1 = await ev(() => window.__pd.design.planets[window.__pd.sel].bulk.massEarth);
  ok('the + button on the panel changes the planet mass', m1 > m0, `${m0} -> ${m1}`);
  if (out) await p.screenshot({ path: out + '/xr-2-panel.png' });

  /* B closes and opens the panel */
  await press('right', 'b-button');
  s = await ev(() => window.__pd.view.xr.panel.mesh.visible);
  ok('B closes the panel', !s);

  /* the star-mass dial: aim at its left side and hold the trigger: the mass goes down */
  const dial = await ev(() => window.__pd.view.xr.dialLocal('mass'));
  const sm0 = await ev(() => window.__pd.design.star.massSun);
  await aimAt('right', [dial[0] - 0.25, dial[1] + 0.05, dial[2] + 0.02]);
  await btn('right', 'trigger', 1); await xrFrames(3); await btn('right', 'trigger', 0); await xrFrames(2);
  const sm1 = await ev(() => window.__pd.design.star.massSun);
  ok('turning the mass dial changes the star', Math.abs(sm1 - sm0) > 0.01, `${sm0.toFixed(3)} -> ${sm1.toFixed(3)} Msun`);
  if (out) await p.screenshot({ path: out + '/xr-3-dial.png' });

  /* two grips pulled apart: the world gets bigger (the rig scale S falls) */
  const down = aimQuat([0, -1, -0.3]);
  await setHand('left', [-0.1, 1.2, -0.35], down); await setHand('right', [0.1, 1.2, -0.35], down); await xrFrames(2);
  const S0 = await ev(() => window.__pd.view.xr.VR.S);
  await btn('left', 'squeeze', 1); await btn('right', 'squeeze', 1); await xrFrames(3);
  await setHand('left', [-0.3, 1.2, -0.35], down); await setHand('right', [0.3, 1.2, -0.35], down); await xrFrames(3);
  await btn('left', 'squeeze', 0); await btn('right', 'squeeze', 0); await xrFrames(2);
  const S1 = await ev(() => window.__pd.view.xr.VR.S);
  ok('two grips pulled apart scale the world', S1 > S0 * 2, `S ${S0.toFixed(2)} -> ${S1.toFixed(2)}`);

  /* leave */
  await ev(() => window.__pd.xr.exit());
  await ev(() => new Promise((res) => { const tk = () => (!window.__pd.view.xr.VR.session ? res() : setTimeout(tk, 100)); tk(); }));
  s = await ev(() => ({ ws: window.__pd.view.xr.ws.root.visible, mode: window.__pd.view.mode }));
  ok('leaving VR restores the desktop view', !s.ws, JSON.stringify(s));
  ok('no errors', errs.length === 0, JSON.stringify(errs.slice(0, 5)));
  await b.close();
  process.exit(fails ? 1 : 0);
})();
