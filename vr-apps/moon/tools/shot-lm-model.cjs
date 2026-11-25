/* Screenshot the LM close-up (tools/lm.html) (Playwright, installed Chrome, software GL).
 *   set NODE_PATH=<dir>\node_modules   (playwright-core)
 *   node tools/shot-lm-model.cjs "az=35&el=8&d=13" out.png [more pairs]
 * Needs the repository root served on port 8765. */
const { chromium } = require('playwright-core');
(async () => {
  const args = process.argv.slice(2);
  const b = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox'] });
  for (let k = 0; k + 1 < args.length; k += 2) {
    const p = await b.newPage({ viewport: { width: 1200, height: 800 } });
    p.on('pageerror', e => console.log('PAGE ERROR', e.message));
    p.on('console', m => { if (m.type() === 'error' && !/favicon|404/.test(m.text())) console.log('CONSOLE', m.text()); });
    await p.goto('http://127.0.0.1:8765/vr-apps/moon/tools/lm.html?' + args[k]);
    await p.waitForFunction(() => window.__lm3 && __lm3.frames > 4, null, { timeout: 120000 });
    const info = await p.evaluate(() => ({ calls: __lm3.info.calls, tris: __lm3.info.triangles }));
    await p.screenshot({ path: args[k + 1] });
    console.log(args[k + 1], JSON.stringify(info));
    await p.close();
  }
  await b.close();
})().catch(e => { console.error(e); process.exit(1); });