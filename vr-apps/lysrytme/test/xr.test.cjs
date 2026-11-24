/* VR test in an emulated Meta Quest 3 (IWER) with Playwright and software GL.
 *   python3 -m http.server 8765                      (from the repository root)
 *   npm i --prefix /tmp/iwer iwer                    (once; or set IWER to the path of iwer.min.js)
 *   NODE_PATH=$(npm root -g) IWER=/tmp/iwer/node_modules/iwer/build/iwer.min.js node vr-apps/lysrytme/test/xr.test.cjs [shot.png]
 * Enters VR, checks both eyes are drawn, that "eyes in turn" gives the two eyes opposite light, that the
 * trigger pauses, that the right stick steps the frequency, and that both grips end the session.
 * Software GL is slow: everything waits by frames. */
const fs = require('fs');
const { chromium } = require('playwright');
const IWER = process.env.IWER || require.resolve('iwer/build/iwer.min.js');
let fails = 0;
const ok = (n, c, i = '') => { console.log(`${c ? 'ok  ' : 'FAIL'} ${n}${i ? '  ' + i : ''}`); if (!c) fails++; };
(async () => {
  const b = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox', '--autoplay-policy=no-user-gesture-required'] });
  const p = await b.newPage({ viewport: { width: 1000, height: 640 } });
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message));
  p.on('console', (m) => { if (m.type() === 'error' && !/favicon|404/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript({ content: fs.readFileSync(IWER, 'utf8') + ';window.__dev = new IWER.XRDevice(IWER.metaQuest3); window.__dev.stereoEnabled = true; window.__dev.installRuntime({ forceInstall: true });' +
    /* A real Quest only settles updateTargetFrameRate once frames are being drawn; the emulator settles it at
       once. Make the emulator behave like the headset, so waiting for it before the first frame shows up here. */
    ';(() => { const P = XRSession.prototype, raf = P.requestAnimationFrame, utf = P.updateTargetFrameRate; let drawn = false;' +
    ' P.requestAnimationFrame = function (cb) { return raf.call(this, (t, f) => { drawn = true; cb(t, f); }); };' +
    ' if (utf) P.updateTargetFrameRate = function (r) { return new Promise((res, rej) => { const go = () => (drawn ? utf.call(this, r).then(res, rej) : setTimeout(go, 20)); go(); }); }; })();' });
  await p.goto('http://localhost:8765/vr-apps/lysrytme/');
  await p.waitForFunction(() => window.__lys);
  const ev = (f, a) => p.evaluate(f, a);
  await ev(() => { localStorage.clear(); Object.assign(__lys.S, { accepted: true, sound: 'off', freq: 10, eyes: 'ens', fadeIn: 0 }); __lys.syncForm(); });
  await p.waitForSelector('#vrBtn:not([hidden])', { timeout: 30000 });
  ok('headset detected: the Start i VR button is shown', true);
  await p.click('#vrBtn');
  await p.waitForFunction(() => __lys.run.session && __lys.run.frames > 3, null, { timeout: 60000 });
  const frames = (n) => ev((n) => new Promise((r) => { const s = __lys.run.frames; const t = () => (__lys.run.frames >= s + n ? r() : setTimeout(t, 30)); t(); }), n);
  let s = await ev(() => ({ fr: __lys.run.fr, mode: __lys.run.mode, f: __lys.run.f }));
  ok('VR session runs', s.mode === 'vr', JSON.stringify(s));
  /* eyes in turn: sample both eyes over a few frames; they must be mirror images (sum 1 for a soft wave) */
  await ev(() => { __lys.S.eyes = 'skift'; __lys.S.wave = 'soft'; });
  let sums = [];
  for (let i = 0; i < 4; i++) { await frames(1); sums.push(await ev(() => __lys.run.b[0] + __lys.run.b[1])); }
  ok('eyes in turn: left and right get opposite light', sums.every((x) => Math.abs(x - 1) < 0.02), sums.map((x) => x.toFixed(3)).join(' '));
  await ev(() => { __lys.S.eyes = 'ens'; });
  await frames(2);
  const same = await ev(() => Math.abs(__lys.run.b[0] - __lys.run.b[1]));
  ok('both eyes the same', same < 1e-6);
  if (process.argv[2]) { await frames(2); await p.screenshot({ path: process.argv[2] }); }
  /* trigger = select = pause */
  const btn = (h, n, v) => ev(([h, n, v]) => window.__dev.controllers[h].updateButtonValue(n, v), [h, n, v]);
  await btn('right', 'trigger', 1); await frames(3); await btn('right', 'trigger', 0); await frames(3);
  ok('trigger pauses', await ev(() => __lys.run.paused));
  await btn('right', 'trigger', 1); await frames(3); await btn('right', 'trigger', 0); await frames(3);
  ok('trigger again resumes', !(await ev(() => __lys.run.paused)));
  /* right stick up: next exact frequency above 10 */
  await ev(() => window.__dev.controllers.right.updateAxes('thumbstick', 0, -1)); await frames(3);
  await ev(() => window.__dev.controllers.right.updateAxes('thumbstick', 0, 0)); await frames(2);
  s = await ev(() => ({ f: __lys.S.freq, run: __lys.run.f }));
  ok('right stick up steps the frequency to 12 Hz', s.f === 12, JSON.stringify(s));
  await btn('left', 'squeeze', 1); await btn('right', 'squeeze', 1);
  await p.waitForFunction(() => !__lys.run.session, null, { timeout: 60000 }).catch(() => {});
  ok('both grips end the session', !(await ev(() => !!__lys.run.session)));
  ok('no errors', errs.length === 0, errs.join(' | '));
  await b.close();
  console.log(fails ? `${fails} failed` : 'all passed');
  process.exit(fails ? 1 : 0);
})();
