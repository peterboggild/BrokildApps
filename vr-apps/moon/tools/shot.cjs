/* Screenshot the flat preview in real time (Playwright with the installed Chrome, software GL).
 * Unlike shot.ps1 this waits for the workers (KTX2 transcoder, ground generator), so it works with the published
 * KTX2 maps and away from the landing site.
 *   set NODE_PATH=<dir>\node_modules   (playwright-core)
 *   node tools/shot.cjs "view=surface&x=-2500&z=1200&heading=40" out.png [more "query" out.png pairs]
 * Needs the repository root served on port 8765. */
const { chromium } = require('playwright-core');
(async () => {
  const args = process.argv.slice(2);
  const b = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox'] });
  for (let k = 0; k + 1 < args.length; k += 2) {
    const p = await b.newPage({ viewport: { width: 1400, height: 900 } });
    p.on('pageerror', e => console.log('PAGE ERROR', e.message));
    p.on('console', m => { if (m.type() === 'error' && !/favicon|404/.test(m.text())) console.log('CONSOLE', m.text()); });
    await p.goto('http://127.0.0.1:8765/vr-apps/moon/moon.html?still&shot&' + args[k]);
    await p.waitForFunction(() => window.__moon && __moon.S.ready, null, { timeout: 240000 });
    await p.waitForFunction(() => !__moon.ground.busy && !__moon.teleporting, null, { timeout: 120000 });
    await p.evaluate(() => new Promise(r => { const s = __moon.S.frames; const t = () => (__moon.S.frames >= s + 4 ? r() : setTimeout(t, 50)); t(); }));
    const info = await p.evaluate(() => ({ ms: Math.round(__moon.ground.ms), rocks: __moon.ground.rocks, squares: __moon.ground.count, calls: __moon.S.calls, tris: __moon.S.tris }));
    await p.screenshot({ path: args[k + 1] });
    console.log(args[k + 1], JSON.stringify(info));
    await p.close();
  }
  await b.close();
})().catch(e => { console.error(e); process.exit(1); });
