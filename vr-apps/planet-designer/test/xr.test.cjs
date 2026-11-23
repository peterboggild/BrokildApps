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

  /* the dials are gone (owner: more in the way than useful) */
  const dialsIn = await ev(() => { const X = window.__pd.view.xr; return [X.ws.massDial, X.ws.ageDial, X.ws.distDial].some((d) => !!d.parent); });
  ok('the three dials are no longer in the workshop', !dialsIn);
  const sm1 = await ev(() => window.__pd.design.star.massSun);

  /* A fetches the ledger board to just in front of you; its grab bar carries it */
  await press('right', 'a-button');
  const bn = await ev(() => { const X = window.__pd.view.xr; const g = X.ws.boardGroup; g.updateMatrixWorld(true); const p = X.VR.dolly.worldToLocal(g.getWorldPosition(g.position.clone())); return { near: X.VR.boardNear, p: p.toArray() }; });
  ok('A fetches the board to near you', bn.near && Math.hypot(bn.p[0], bn.p[2]) < 2.5, JSON.stringify(bn));
  if (out) await p.screenshot({ path: out + '/xr-3-board.png' });
  const barL = await ev(() => { const X = window.__pd.view.xr; const b = X.ws.boardGroup.children[2]; b.updateMatrixWorld(true); return X.VR.dolly.worldToLocal(b.getWorldPosition(b.position.clone())).toArray(); });
  await aimAt('right', barL);
  await btn('right', 'trigger', 1); await xrFrames(2);
  await aimAt('right', [barL[0] + 0.8, barL[1] + 0.2, barL[2]]); await xrFrames(3);
  await btn('right', 'trigger', 0); await xrFrames(2);
  const bn2 = await ev(() => { const X = window.__pd.view.xr; const g = X.ws.boardGroup; return X.VR.dolly.worldToLocal(g.getWorldPosition(g.position.clone())).toArray(); });
  ok('the board can be carried by its grab bar', Math.hypot(bn2[0] - bn.p[0], bn2[2] - bn.p[2]) > 0.3, `${bn.p.map((v) => v.toFixed(2))} -> ${bn2.map((v) => v.toFixed(2))}`);
  await press('right', 'a-button');
  ok('A again sends the board home', await ev(() => !window.__pd.view.xr.VR.boardNear));

  /* the Scene tab: move the planet to a temperate place in the zone, then "Start with Earth" */
  await press('right', 'b-button');
  const panelClick = async (prefix) => { const w = await ev((p) => window.__pd.view.xr.widgetLocal(p), prefix); if (!w) return false; await aimAt('right', w); await press('right', 'trigger'); return true; };
  await panelClick('tab:vr');
  const vrIds = await ev(() => { const ws = window.__pd.view.xr.panel; ws.redraw(); return ws.state.widgets.map((x) => x.id).join('|'); });
  ok('the VR tab holds only VR things (no start designs, no time)', !/Start med Jorden|Start with Earth|v\.time|vr\.zoom|level/.test(vrIds) && /vr\.leave/.test(vrIds), vrIds);
  await panelClick('tab:scene');
  const toZone = await ev(() => { const ws = window.__pd.view.xr.panel; ws.redraw(); const w = ws.state.widgets.find((x) => x.id.includes('Flyt til') || x.id.includes('Move to')); return w ? w.id : null; });
  await panelClick(toZone);
  const pos = await ev(() => window.__pd.derivedPlanet().hzPos);
  ok('"Move to a temperate place in the zone" puts the planet in the zone', pos === 'cons', `${pos} at ${sm1.toFixed(2)} Msun`);
  const earthBtn = await ev(() => { const ws = window.__pd.view.xr.panel; ws.redraw(); const w = ws.state.widgets.find((x) => x.id.includes('Start med Jorden') || x.id.includes('Start with Earth')); return w ? w.id : null; });
  await panelClick(earthBtn);
  await ev(() => new Promise((res) => { const tk = () => (window.__pd.derivedPlanet().ebm ? res() : setTimeout(tk, 200)); tk(); }));
  const earth = await ev(() => ({ m: window.__pd.design.star.massSun, lamps: window.__pd.ledgerFor(window.__pd.derivedPlanet()).pass[5] }));
  ok('"Start with Earth" from the VR panel gives an Earth twin', earth.m === 1 && earth.lamps, JSON.stringify(earth));
  /* moons and planets from VR */
  await panelClick('tab:moons');
  const nm0 = await ev(() => window.__pd.planet().moons.length);
  const addMoon = await ev(() => { const ws = window.__pd.view.xr.panel; ws.redraw(); const w = ws.state.widgets.find((x) => x.id.includes('Tilføj måne') || x.id.includes('Add moon')); return w ? w.id : null; });
  await panelClick(addMoon);
  ok('a moon can be added in VR', (await ev(() => window.__pd.planet().moons.length)) === nm0 + 1);
  await panelClick('tab:system');
  const np0 = await ev(() => window.__pd.design.planets.length);
  const addPl = await ev(() => { const ws = window.__pd.view.xr.panel; ws.redraw(); const w = ws.state.widgets.find((x) => x.id.includes('Tilføj planet') || x.id.includes('Add planet')); return w ? w.id : null; });
  await panelClick(addPl);
  ok('a planet can be added in VR', (await ev(() => window.__pd.design.planets.length)) === np0 + 1);
  if (out) await p.screenshot({ path: out + '/xr-4-system.png' });
  await press('right', 'b-button');

  /* two grips pulled apart: the world gets bigger (the rig scale S falls) */
  const down = aimQuat([0, -1, -0.3]);
  await setHand('left', [-0.1, 1.2, -0.35], down); await setHand('right', [0.1, 1.2, -0.35], down); await xrFrames(2);
  const S0 = await ev(() => window.__pd.view.xr.VR.S);
  await btn('left', 'squeeze', 1); await btn('right', 'squeeze', 1); await xrFrames(3);
  await setHand('left', [-0.3, 1.2, -0.35], down); await setHand('right', [0.3, 1.2, -0.35], down); await xrFrames(3);
  await btn('left', 'squeeze', 0); await btn('right', 'squeeze', 0); await xrFrames(2);
  const S1 = await ev(() => window.__pd.view.xr.VR.S);
  ok('two grips pulled apart scale the world', S1 > S0 * 2, `S ${S0.toFixed(2)} -> ${S1.toFixed(2)}`);

  /* the atmosphere follows the planet's size in the cradle (it was drawn at 1 m inside a 1.75 m planet) */
  const atm = await ev(() => { const pl = window.__pd.view.planet; let r0 = null; pl.group.traverse((o) => { if (o.material && o.material.uniforms && o.material.uniforms.uR0) r0 = o.material.uniforms.uR0.value; }); return r0; });
  ok('the atmosphere is scaled with the planet in VR', Math.abs(atm - 1.75) < 0.01, `uR0 ${atm}`);

  /* forward is where you look, also after a snap turn (it ran backwards before) */
  const hand0 = aimQuat([0, -0.3, -1]);
  await setHand('left', [-0.2, 1.2, -0.3], hand0); await setHand('right', [0.2, 1.2, -0.3], hand0);
  const axes = (h, x, y) => ev(([h, x, y]) => window.__dev.controllers[h].updateAxes('thumbstick', x, y), [h, x, y]);
  await axes('right', 1, 0); await xrFrames(2); await axes('right', 0, 0); await xrFrames(2);
  await axes('right', 1, 0); await xrFrames(2); await axes('right', 0, 0); await xrFrames(2);
  const before = await ev(() => { const X = window.__pd.view.xr; const c = window.__pd.view.camera; c.updateMatrixWorld(true); const f = c.getWorldDirection(c.position.clone()); return { p: X.VR.dolly.position.toArray(), f: [f.x, f.z] }; });
  await axes('left', 0, -1); await xrFrames(4); await axes('left', 0, 0); await xrFrames(1);
  const after = await ev(() => window.__pd.view.xr.VR.dolly.position.toArray());
  const mv = [after[0] - before.p[0], after[2] - before.p[2]];
  const along = (mv[0] * before.f[0] + mv[1] * before.f[1]) / (Math.hypot(...mv) * Math.hypot(...before.f) + 1e-9);
  ok('after two snap turns, stick forward moves where the head looks', along > 0.9, `cos ${along.toFixed(2)}`);

  /* carry the star round with the ray */
  const sunW = await ev(() => { const X = window.__pd.view.xr; const p = X.ws.forgeGroup.getWorldPosition(X.ws.forgeGroup.position.clone()); return X.VR.dolly.worldToLocal(p).toArray(); });
  const sun0 = await ev(() => window.__pd.view.xr.ws.forgeGroup.position.toArray());
  await aimAt('right', sunW, [0.2, 1.3, -0.2]);
  await btn('right', 'trigger', 1); await xrFrames(2);
  await aimAt('right', [sunW[0] + 25, sunW[1], sunW[2] + 25], [0.2, 1.3, -0.2]); await xrFrames(2);
  await btn('right', 'trigger', 0); await xrFrames(1);
  const sun1 = await ev(() => window.__pd.view.xr.ws.forgeGroup.position.toArray());
  ok('the star can be carried round with the ray', Math.hypot(sun1[0] - sun0[0], sun1[2] - sun0[2]) > 2, `${sun0.map((v) => v.toFixed(1))} -> ${sun1.map((v) => v.toFixed(1))}`);
  await ev(() => window.__pd.xr.resetSun());

  /* the panel's grab bar carries the panel */
  await press('right', 'b-button');
  const pbx = await ev(() => { const X = window.__pd.view.xr; const m = X.panel.mesh; const b = m.children[0]; m.updateMatrixWorld(true); return { bar: X.VR.dolly.worldToLocal(b.getWorldPosition(b.position.clone())).toArray(), p: m.position.toArray() }; });
  await aimAt('right', pbx.bar);
  await btn('right', 'trigger', 1); await xrFrames(2);
  await aimAt('right', [pbx.bar[0] - 0.4, pbx.bar[1] + 0.1, pbx.bar[2]]); await xrFrames(3);
  await btn('right', 'trigger', 0); await xrFrames(2);
  const pp1 = await ev(() => window.__pd.view.xr.panel.mesh.position.toArray());
  ok('the panel can be carried by its grab bar', Math.hypot(pp1[0] - pbx.p[0], pp1[2] - pbx.p[2]) > 0.15, `${pbx.p.map((v) => v.toFixed(2))} -> ${pp1.map((v) => v.toFixed(2))}`);
  await press('right', 'b-button');
  /* inclination tilts the orbit on the table */
  await ev(() => window.__pd.set('planet.orbit.incDeg', 10)); await xrFrames(2);
  const tiltY = await ev(() => { let m = 0; window.__pd.view.orrery.group.traverse((o) => { if (o.isLine) { const a = o.geometry.attributes.position.array; for (let k = 1; k < a.length; k += 3) m = Math.max(m, Math.abs(a[k])); } }); return m; });
  ok('an inclined orbit is drawn tilted', tiltY > 0.05, `orbit up to ${tiltY.toFixed(3)} above the plane`);
  await ev(() => window.__pd.set('planet.orbit.incDeg', 0));
  if (out) await p.screenshot({ path: out + '/xr-5-tools.png' });

  /* table zoom: the system grows round the selected planet */
  await ev(() => window.__pd.loadPreset('solar')); await xrFrames(2);
  await ev(() => window.__pd.xr.setZoom(30)); await xrFrames(3);
  const zi = await ev(() => { const X = window.__pd.view.xr; return { s: X.ws.tableSlot.scale.x, forge: X.ws.forgeGroup.visible, star: X.ws.tableSlot.position.toArray(), ts: window.__pd.view.state.timeScale }; });
  ok('"Into the system": the system x30, the forge hidden, time at real time', Math.abs(zi.s / 0.046 - 30) < 0.01 && !zi.forge && zi.ts < 1e-7, JSON.stringify(zi));
  /* the star stays put while the planet's distance changes */
  const ang = () => ev(() => { const b = window.__pd.view.orrery.beads[window.__pd.sel].mesh.position; return Math.atan2(-b.z, b.x); });
  const ang0 = await ang();
  await ev(() => window.__pd.set('planet.orbit.aAU', 1.03)); await xrFrames(3);
  const ang1 = await ang();
  ok('a changed orbit keeps the planet at the same place round the star', Math.abs(Math.atan2(Math.sin(ang1 - ang0), Math.cos(ang1 - ang0))) < 0.01, `${ang0.toFixed(3)} -> ${ang1.toFixed(3)} rad`);
  const star2 = await ev(() => window.__pd.view.xr.ws.tableSlot.position.toArray());
  ok('the star stays put while the orbit changes', Math.hypot(star2[0] - zi.star[0], star2[2] - zi.star[2]) < 1e-6);
  const pp = await ev(() => { const v = window.__pd.view; return v.planet.group.getWorldPosition(v.planet.group.position.clone()).distanceTo(v.xr.ws.tableSlot.getWorldPosition(v.xr.ws.tableSlot.position.clone())); });
  ok('the real planet sits in its orbit, away from the star', pp > 3, `${pp.toFixed(1)} m from the star`);
  if (out) await p.screenshot({ path: out + '/xr-6-inside.png' });
  await ev(() => window.__pd.xr.setZoom(1));

  /* leave */
  await ev(() => window.__pd.xr.exit());
  await ev(() => new Promise((res) => { const tk = () => (!window.__pd.view.xr.VR.session ? res() : setTimeout(tk, 100)); tk(); }));
  s = await ev(() => ({ ws: window.__pd.view.xr.ws.root.visible, mode: window.__pd.view.mode }));
  ok('leaving VR restores the desktop view', !s.ws, JSON.stringify(s));
  ok('no errors', errs.length === 0, JSON.stringify(errs.slice(0, 5)));
  await b.close();
  process.exit(fails ? 1 : 0);
})();
