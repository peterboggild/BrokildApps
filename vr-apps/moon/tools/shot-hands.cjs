/* Screenshot the hands close-up (tools/hands.html) once the hand model has loaded (Playwright, installed Chrome, software GL).
 *   set NODE_PATH=<dir>\node_modules   (playwright-core)
 *   node tools/shot-hands.cjs "thumb=1" out.png ["opp=30&len=0.85" out2.png ...]
 * Needs the repository root served on port 8765. */
const { chromium } = require('playwright-core');
(async () => {
  const args = process.argv.slice(2);
  const b = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox'] });
  for (let k = 0; k + 1 < args.length; k += 2) {
    const p = await b.newPage({ viewport: { width: 1200, height: 400 } });
    p.on('pageerror', e => console.log('PAGE ERROR', e.message));
    await p.goto('http://127.0.0.1:8765/vr-apps/moon/tools/hands.html?' + args[k]);
    await p.waitForFunction(() => window.__hands && __hands.ready, null, { timeout: 60000 });
    await p.waitForTimeout(500);
    await p.screenshot({ path: args[k + 1] });
    console.log(args[k + 1]);
    await p.close();
  }
  await b.close();
})().catch(e => { console.error(e); process.exit(1); });