/**
 * Rasterises the icon and launch artwork.
 *
 * There is no ImageMagick, librsvg or Inkscape in this environment, but there
 * is a headless Chromium, which is a perfectly exact SVG renderer. Each source
 * is loaded in a page sized to the target and screenshotted, with the icon
 * composited on an opaque background because Apple rejects app icons that
 * carry an alpha channel.
 *
 * Run: node scripts/render-assets.mjs
 */
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { existsSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const PROJECT = resolve(HERE, "..");
const SRC = join(PROJECT, "assets", "icon");
const XC = join(PROJECT, "ios", "App", "App", "Assets.xcassets");

const CHROMIUM = ["/opt/pw-browsers/chromium",
  "/opt/pw-browsers/chromium-1194/chrome-linux/chrome"].find(existsSync);

async function playwright() {
  for (const c of ["playwright", "/opt/node22/lib/node_modules/playwright/index.js"]) {
    try { const m = await import(c); return m.chromium ? m : m.default; } catch {}
  }
  throw new Error("playwright not found");
}

const TARGETS = [
  // The single universal app icon modern Xcode asks for. Opaque.
  { svg: "icon-src.svg", out: join(XC, "AppIcon.appiconset", "AppIcon-512@2x.png"),
    size: 1024, opaque: true },

  // Launch mark, on transparency, over the storyboard's own dark background.
  { svg: "logo-src.svg", out: join(XC, "LaunchLogo.imageset", "launch-logo.png"),
    size: 120, opaque: false },
  { svg: "logo-src.svg", out: join(XC, "LaunchLogo.imageset", "launch-logo@2x.png"),
    size: 240, opaque: false },
  { svg: "logo-src.svg", out: join(XC, "LaunchLogo.imageset", "launch-logo@3x.png"),
    size: 360, opaque: false },

  // The stock Capacitor splash images are replaced with the app's own ground.
  // Nothing references them once LaunchScreen.storyboard is rewritten, but a
  // white 2732px image sitting in the bundle is exactly the kind of thing that
  // flashes on launch if anything ever does.
  { svg: "splash-src.svg", out: join(XC, "Splash.imageset", "splash-2732x2732.png"),
    size: 2732, opaque: true },
  { svg: "splash-src.svg", out: join(XC, "Splash.imageset", "splash-2732x2732-1.png"),
    size: 2732, opaque: true },
  { svg: "splash-src.svg", out: join(XC, "Splash.imageset", "splash-2732x2732-2.png"),
    size: 2732, opaque: true },
];

// The splash is the app's ground with the mark small in the middle. Nothing
// references it once LaunchScreen.storyboard is rewritten, but a white 2732px
// image left in the bundle is exactly what flashes on launch if anything does.
writeFileSync(join(SRC, "splash-src.svg"), `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" width="1024" height="1024">
  <rect width="1024" height="1024" fill="#06070a"/>
  <svg x="384" y="384" width="256" height="256" viewBox="0 0 1024 1024">
    <svg x="282" y="202" width="460" height="460" viewBox="143 173.5 205.5 205.5">
      <path d="M336 300a92 92 0 1 1-114-114 76 76 0 0 0 114 114z"
            fill="none" stroke="#d9a05b" stroke-width="20"
            stroke-linecap="round" stroke-linejoin="round"/>
    </svg>
    <path d="M312 800q50-62 100 0t100 0t100 0t100 0"
          fill="none" stroke="#d9a05b" stroke-width="26"
          stroke-linecap="round" opacity=".62"/>
  </svg>
</svg>`);

const { chromium } = await playwright();
const browser = await chromium.launch({ executablePath: CHROMIUM, args: ["--no-sandbox"] });

for (const t of TARGETS) {
  const svg = readFileSync(join(SRC, t.svg), "utf8");
  const page = await browser.newPage({
    viewport: { width: t.size, height: t.size },
    deviceScaleFactor: 1,
  });
  await page.setContent(
    `<style>html,body{margin:0;padding:0;width:${t.size}px;height:${t.size}px;` +
    `background:${t.opaque ? "#06070a" : "transparent"}}` +
    // Scoped to the root element: a bare `svg` selector would also resize
    // nested <svg> viewports, which is how the mark gets placed.
    `body>svg{display:block;width:${t.size}px;height:${t.size}px}</style>${svg}`,
    { waitUntil: "load" });
  mkdirSync(dirname(t.out), { recursive: true });
  await page.screenshot({ path: t.out, omitBackground: !t.opaque });
  await page.close();
  console.log(`  ${String(t.size).padStart(4)}px  ${t.out.replace(PROJECT + "/", "")}`);
}

await browser.close();

// Contents.json for the launch mark.
writeFileSync(join(XC, "LaunchLogo.imageset", "Contents.json"), JSON.stringify({
  images: [
    { idiom: "universal", filename: "launch-logo.png", scale: "1x" },
    { idiom: "universal", filename: "launch-logo@2x.png", scale: "2x" },
    { idiom: "universal", filename: "launch-logo@3x.png", scale: "3x" },
  ],
  info: { version: 1, author: "xcode" },
}, null, 2) + "\n");

console.log("render-assets: done");
