/**
 * Builds www/ for the native app.
 *
 * The web application at health-apps/sleep-noise/ is the canonical source and
 * the only copy. This step is a COPY, not a transform: the native adaptations
 * inside index.html are conditional at runtime on whether the Capacitor native
 * bridge is present, so exactly the same file serves the website and the app.
 *
 * That is deliberate. A build step that rewrote the app would create a second
 * version that drifts from the first, and the regression suite would stop
 * testing what ships.
 *
 * Run: npm run build      (or npm run sync, which also runs `cap sync ios`)
 */

import { readFileSync, writeFileSync, mkdirSync, rmSync, existsSync, statSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve, join } from "node:path";

const HERE = dirname(fileURLToPath(import.meta.url));
const PROJECT = resolve(HERE, "..");
const SRC = resolve(PROJECT, "..", "..", "health-apps", "sleep-noise");
const OUT = join(PROJECT, "www");

// Everything the app needs at runtime. app.json is the BrokildApps catalogue
// entry and has no business in the bundle.
const COPY = ["index.html", "icon.svg", "manifest.webmanifest"];

// Things that must be true of the source, checked so that a refactor of the
// web app cannot silently ship a native build that no longer works.
const REQUIRE = [
  ["the native audio bridge hook", "SleeperAudio"],
  ["the runtime platform switch", "isNativePlatform"],
  ["the mixer", "mixRows"],
  ["the presets", "presetChips"],
  ["the sleep timer clock source", "startClock"],
  ["the blackout screen", 'id="dim"'],
];

// Anything that would mean the bundle is not self-contained.
const FORBID = [
  ["an external stylesheet or script", /<(?:link[^>]+href|script[^>]+src)\s*=\s*["']https?:/i],
  ["a remote font", /@font-face[\s\S]{0,400}?https?:/i],
  ["a network request", /\b(?:fetch\(\s*["']https?:|XMLHttpRequest|new\s+WebSocket)/],
  ["an analytics or beacon call", /\b(?:gtag|ga\(|sendBeacon|mixpanel|amplitude)\b/],
];

function fail(msg) {
  console.error("build-www: " + msg);
  process.exit(1);
}

if (!existsSync(SRC)) fail(`cannot find the web app at ${SRC}`);

const html = readFileSync(join(SRC, "index.html"), "utf8");

for (const [what, needle] of REQUIRE) {
  if (!html.includes(needle)) {
    fail(`index.html no longer contains ${what} (looked for "${needle}").\n` +
         `  The web app has been restructured. Update this check and re-verify the native build.`);
  }
}

for (const [what, re] of FORBID) {
  const m = html.match(re);
  if (m) {
    fail(`index.html now contains ${what}: ${JSON.stringify(m[0].slice(0, 90))}\n` +
         `  The native app must be self-contained and work in airplane mode. Bundle it locally instead.`);
  }
}

rmSync(OUT, { recursive: true, force: true });
mkdirSync(OUT, { recursive: true });

let total = 0;
for (const name of COPY) {
  const from = join(SRC, name);
  if (!existsSync(from)) fail(`missing ${name} in ${SRC}`);
  const buf = readFileSync(from);
  writeFileSync(join(OUT, name), buf);
  total += buf.length;
  console.log(`  ${name.padEnd(24)} ${String(buf.length).padStart(7)} bytes`);
}

console.log(`build-www: ${COPY.length} files, ${(total / 1024).toFixed(1)} KB, self-contained -> www/`);
console.log(`           source of truth: health-apps/sleep-noise/index.html`);
