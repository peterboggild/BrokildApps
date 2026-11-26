/* Screenshot chapters 11 and 12's flat preview in real time (Playwright with the installed Chrome, software GL).
 *   set NODE_PATH=<dir>\node_modules   (playwright-core)
 *   node tools/shot-splash.cjs "t=-60&yaw=0&pitch=10" out.png [more pairs]
 * Needs the repository root served on port 8765. The clock is stopped (?still) so the picture is of that moment. */
const { chromium } = require('playwright-core');
(async () => {
  const args = process.argv.slice(2);
  const b = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox'] });
  for (let k = 0; k + 1 < args.length; k += 2) {
    const p = await b.newPage({ viewport: { width: 1400, height: 900 } });
    p.on('pageerror', e => console.log('PAGE ERROR', e.message));
    p.on('console', m => { if (m.type() === 'error' && !/favicon|404/.test(m.text())) console.log('CONSOLE', m.text()); });
    await p.goto('http://127.0.0.1:8765/vr-apps/moon/splash.html?still&shot&' + args[k]);
    await p.waitForFunction(() => window.__entry && __entry.S.ready, null, { timeout: 120000 });
    await p.evaluate(() => new Promise(r => { const s = __entry.S.frames; const t = () => (__entry.S.frames >= s + 6 ? r() : setTimeout(t, 50)); t(); }));
    const info = await p.evaluate(() => ({ t: __entry.S.t, calls: __entry.S.calls, tris: __entry.S.tris }));
    await p.screenshot({ path: args[k + 1] });
    console.log(args[k + 1], JSON.stringify(info));
    await p.close();
  }
  await b.close();
})().catch(e => { console.error(e); process.exit(1); });
