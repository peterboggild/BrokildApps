/* Screenshot the crew close-up (tools/crew.html) after its face has loaded (Playwright with the installed Chrome, software GL).
 *   set NODE_PATH=<dir>\node_modules   (playwright-core)
 *   node tools/shot-crew.cjs "fov=20" out.png ["fov=20&talk" out2.png ...]
 * Needs the repository root served on port 8765. ?wait=ms waits longer before the picture (the life running). */
const { chromium } = require('playwright-core');
(async () => {
  const args = process.argv.slice(2);
  const b = await chromium.launch({ channel: 'chrome', args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-sandbox'] });
  for (let k = 0; k + 1 < args.length; k += 2) {
    const p = await b.newPage({ viewport: { width: 900, height: 700 } });
    p.on('pageerror', e => console.log('PAGE ERROR', e.message));
    p.on('console', m => { if (m.type() === 'error' && !/favicon|404/.test(m.text())) console.log('CONSOLE', m.text()); });
    await p.goto('http://127.0.0.1:8765/vr-apps/moon/tools/crew.html?' + args[k]);
    await p.waitForFunction(() => window.__crew && __crew.man.userData.eyes && __crew.frames > 5, null, { timeout: 120000 });
    const wait = +(new URLSearchParams(args[k]).get('wait') || 300);
    await p.waitForTimeout(wait);
    await p.screenshot({ path: args[k + 1] });
    console.log(args[k + 1]);
    await p.close();
  }
  await b.close();
})().catch(e => { console.error(e); process.exit(1); });