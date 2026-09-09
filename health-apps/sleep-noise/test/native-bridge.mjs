/**
 * Sleeper Agent — the native (iOS) code path, exercised against a mock plugin.
 *
 *   node health-apps/sleep-noise/test/native-bridge.mjs
 *
 * The Swift half of the port cannot be compiled in this environment, let alone
 * run. The JavaScript half can be, and it is the half with the fiddly parts:
 * what gets called and in what order, the PCM encoding, the chunking, the
 * parameter mapping, the event handling, and the several dozen places where
 * the app must take the native branch instead of touching Web Audio.
 *
 * So a fake Capacitor bridge is installed before the page's own script runs.
 * It records every call and resolves with plausible values, and the mock
 * tracks which loops have been "cached" so the second start behaves like a
 * second night. Nothing here proves the Swift is right — see
 * IOS_QA_CHECKLIST.md for what only a device can settle — but it does prove
 * the app asks for the right things.
 */

import { existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const HERE = dirname(fileURLToPath(import.meta.url));
const APP_URL = "file://" + resolve(HERE, "..", "index.html");

const CHROMIUM = ["/opt/pw-browsers/chromium",
  "/opt/pw-browsers/chromium-1194/chrome-linux/chrome"].find(existsSync);

async function importPlaywright() {
  for (const c of ["playwright", "/opt/node22/lib/node_modules/playwright/index.js"]) {
    try { const m = await import(c); return m.chromium ? m : m.default; } catch { /* next */ }
  }
  throw new Error("playwright not found");
}

let pass = 0;
const failures = [];
function check(name, ok, detail = "") {
  if (ok) { pass++; console.log(`  ok   ${name}${detail ? "  — " + detail : ""}`); }
  else { failures.push(name + (detail ? " — " + detail : "")); console.log(`  FAIL ${name}${detail ? "  — " + detail : ""}`); }
}

/** The mock bridge. Runs in the page before the app's own script. */
const MOCK = () => {
  const calls = [];
  const listeners = {};
  const cached = new Set();
  const uploads = {};           // id -> { chunks, bytes, sampleRate, channels }
  const SR = 48000;

  const record = (name, args) => { calls.push({ name, args: args || {} }); };

  const SleeperAudio = {
    getInfo(a) {
      record("getInfo", a);
      return Promise.resolve({
        sampleRate: SR,
        cached: (a.ids || []).filter((i) => cached.has(i)),
        native: true,
      });
    },
    prepareBegin(a) {
      record("prepareBegin", { id: a.id, sampleRate: a.sampleRate, channels: a.channels });
      uploads[a.id] = { chunks: 0, bytes: 0, sampleRate: a.sampleRate, channels: a.channels, data: [] };
      return Promise.resolve({ ok: true });
    },
    prepareChunk(a) {
      // The payload itself is not recorded into `calls` (it is megabytes);
      // its size and a sample of its bytes are.
      const u = uploads[a.id];
      const bin = atob(a.data);
      u.chunks++;
      u.bytes += bin.length;
      if (u.data.length < 2) u.data.push(bin.slice(0, 64));
      record("prepareChunk", { id: a.id, bytes: bin.length });
      return Promise.resolve({ bytes: u.bytes });
    },
    prepareEnd(a) {
      record("prepareEnd", { id: a.id, bytes: uploads[a.id].bytes, chunks: uploads[a.id].chunks });
      cached.add(a.id);
      return Promise.resolve({ ok: true });
    },
    start(a) { record("start", a); return Promise.resolve({ playing: true, remainingMs: (a.durationSeconds || 0) * 1000, unlimited: !a.durationSeconds }); },
    stop(a) { record("stop", a); return Promise.resolve({ ok: true }); },
    setLayer(a) { record("setLayer", a); return Promise.resolve(); },
    setPadLevel(a) { record("setPadLevel", a); return Promise.resolve(); },
    setVolume(a) { record("setVolume", a); return Promise.resolve(); },
    setTone(a) { record("setTone", a); return Promise.resolve(); },
    setSwell(a) { record("setSwell", a); return Promise.resolve(); },
    setNowPlaying(a) { record("setNowPlaying", a); return Promise.resolve(); },
    getState(a) {
      record("getState", a);
      return Promise.resolve(window.__mockState || { playing: true, remainingMs: 600000, unlimited: false });
    },
    clearCache(a) { record("clearCache", a); cached.clear(); return Promise.resolve({ removed: 0 }); },
    addListener(name, fn) {
      (listeners[name] = listeners[name] || []).push(fn);
      return Promise.resolve({ remove: () => {} });
    },
  };

  const SleeperShell = {
    haptic(a) { record("haptic", a); return Promise.resolve(); },
    setIdleTimerDisabled(a) { record("setIdleTimerDisabled", a); return Promise.resolve(a); },
    setStatusBarHidden(a) { record("setStatusBarHidden", a); return Promise.resolve(a); },
    setBacking(a) { record("setBacking", { bytes: (a.json || "").length }); window.__backing = a.json; return Promise.resolve({ ok: true }); },
    getBacking() { record("getBacking", {}); return Promise.resolve({ json: window.__backing || "", present: !!window.__backing }); },
    getEnvironment() {
      record("getEnvironment", {});
      return Promise.resolve({
        safeArea: { top: 59, bottom: 34, left: 0, right: 0 }, idiom: "phone",
        systemVersion: "26.0", appVersion: "1.0.0", buildNumber: "1",
        scale: 3, hasHomeIndicator: true,
      });
    },
  };

  window.Capacitor = {
    isNativePlatform: () => true,
    getPlatform: () => "ios",
    Plugins: { SleeperAudio, SleeperShell },
  };

  // Surfaces for the test.
  window.__calls = calls;
  window.__uploads = uploads;
  window.__fire = (name, data) => (listeners[name] || []).forEach((f) => f(data || {}));
  window.__listenerNames = () => Object.keys(listeners);

  // Tripwire: on the native path the app must never build a Web Audio graph.
  // Creating an AudioContext would take the audio session for a context that
  // never plays a sample.
  window.__audioContexts = 0;
  for (const key of ["AudioContext", "webkitAudioContext"]) {
    const Real = window[key];
    if (!Real) continue;
    window[key] = function (...a) { window.__audioContexts++; return new Real(...a); };
  }
};

const wait = (page, fn, arg, timeout = 90000) => page.waitForFunction(fn, arg, { timeout });
const callsOf = (page, name) =>
  page.evaluate((n) => window.__calls.filter((c) => c.name === n), name);
const lastCall = (page, name) =>
  page.evaluate((n) => {
    const m = window.__calls.filter((c) => c.name === n);
    return m.length ? m[m.length - 1] : null;
  }, name);

if (!CHROMIUM) {
  console.log("native-bridge: skipped, no Chromium found");
  process.exit(0);
}

const { chromium } = await importPlaywright();
const browser = await chromium.launch({
  executablePath: CHROMIUM,
  args: ["--autoplay-policy=no-user-gesture-required", "--no-sandbox"],
});
const errors = [];
const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
page.on("pageerror", (e) => errors.push("pageerror: " + e.message));
page.on("console", (m) => { if (m.type() === "error") errors.push("console: " + m.text()); });
await page.addInitScript(MOCK);
await page.goto(APP_URL);
await page.waitForTimeout(400);

console.log("Sleeper Agent — native bridge (mock plugin)");

/* ---------------- platform adaptation ---------------- */
console.log("\nplatform");
const adapt = await page.evaluate(() => ({
  nativeClass: document.documentElement.classList.contains("native"),
  homeHidden: getComputedStyle(document.querySelector(".home")).display === "none",
  bgSwitchHidden: getComputedStyle(document.getElementById("bgSw").closest(".toggle")).display === "none",
  footerLinkHidden: getComputedStyle(document.querySelector("footer .webonly")).display === "none",
  noSelect: getComputedStyle(document.body).webkitUserSelect === "none",
  wakeLabel: document.getElementById("wakeT").textContent,
  contexts: window.__audioContexts,
}));
check("native platform detected", adapt.nativeClass);
check("link back to the website is hidden", adapt.homeHidden);
check("the browser-only 'play with screen locked' switch is hidden", adapt.bgSwitchHidden);
check("footer website link is hidden", adapt.footerLinkHidden);
check("text selection is suppressed", adapt.noSelect);
check("the wake switch is relabelled for blackout", adapt.wakeLabel === "Keep the screen on in blackout", adapt.wakeLabel);
check("no AudioContext built at startup", adapt.contexts === 0, `${adapt.contexts} created`);

/* ---------------- first play: render, upload, start ---------------- */
console.log("\nfirst play (nothing cached)");
await page.click('#presetChips button[data-preset="deep"]');   // brown only, no theme
await page.click("#playBtn");
await wait(page, () => window.__calls.some((c) => c.name === "start"));
await page.waitForTimeout(300);

const info = await callsOf(page, "getInfo");
const begins = await callsOf(page, "prepareBegin");
const ends = await callsOf(page, "prepareEnd");
const uploads = await page.evaluate(() => window.__uploads);
const start = await lastCall(page, "start");

check("asked which loops are cached before rendering", info.length >= 1,
  `ids ${JSON.stringify(info[0]?.args.ids)}`);
check("uploaded exactly the one loop the mix needs",
  begins.length === 1 && begins[0].args.id === "brown",
  begins.map((b) => b.args.id).join(","));
check("uploaded as stereo at the hardware rate",
  begins[0].args.channels === 2 && begins[0].args.sampleRate === 48000,
  `${begins[0].args.channels}ch @ ${begins[0].args.sampleRate}`);

// 20 s stereo 16-bit at 48 kHz = 20 * 48000 * 2 * 2 bytes.
const expectBytes = 20 * 48000 * 2 * 2;
check("PCM payload is exactly the expected size",
  uploads.brown.bytes === expectBytes,
  `${uploads.brown.bytes} vs ${expectBytes}`);
check("payload arrived in more than one chunk", uploads.brown.chunks > 1, `${uploads.brown.chunks} chunks`);
check("every chunk is a whole number of stereo frames",
  uploads.brown.bytes % 4 === 0);
check("upload was finalised", ends.length === 1 && ends[0].args.id === "brown");

// The bytes should look like normalised noise, not silence and not clipping.
const stats = await page.evaluate(() => {
  const bin = window.__uploads.brown.data[0];
  const vals = [];
  for (let i = 0; i + 1 < bin.length; i += 2) {
    let v = bin.charCodeAt(i) | (bin.charCodeAt(i + 1) << 8);
    if (v > 32767) v -= 65536;
    vals.push(v / 32768);
  }
  const peak = Math.max(...vals.map(Math.abs));
  return { n: vals.length, peak, allZero: vals.every((v) => v === 0) };
});
check("PCM is not silence", !stats.allZero && stats.n > 0, `${stats.n} samples inspected`);
check("PCM is within full scale", stats.peak <= 1.0, `peak ${stats.peak.toFixed(3)}`);

/* ---------------- the start payload ---------------- */
console.log("\nstart payload");
const a = start.args;
check("one layer, brown, at a positive gain",
  a.layers.length === 1 && a.layers[0].id === "brown" && a.layers[0].level > 0,
  JSON.stringify(a.layers));
check("no ambient loops for a theme-less preset", (a.padIds || []).length === 0);
check("tone passed as a frequency, not a slider position",
  a.toneHz > 200 && a.toneHz < 20000, `${Math.round(a.toneHz)} Hz`);
check("volume passed as a gain, not a percentage", a.volume > 0 && a.volume <= 1, String(a.volume));
check("duration passed in seconds", Math.abs(a.durationSeconds - 8 * 3600) < 5, `${a.durationSeconds} s`);
check("fade-in passed in seconds", a.fadeInSeconds > 0, `${a.fadeInSeconds} s`);
check("fade-out is clamped inside the session",
  a.fadeOutSeconds > 0 && a.fadeOutSeconds < a.durationSeconds, `${a.fadeOutSeconds} s`);
check("Now Playing title is the preset name", a.title === "Deep", a.title);
check("Now Playing subtitle reports the time left", /left$/.test(a.subtitle), a.subtitle);
check("still no AudioContext after playing",
  (await page.evaluate(() => window.__audioContexts)) === 0);

/* ---------------- live changes ---------------- */
console.log("\nlive changes");
await page.evaluate(() => {
  const i = document.querySelectorAll("#mixRows input")[0];   // brown, already open
  i.value = 40;
  i.dispatchEvent(new Event("input", { bubbles: true }));
});
await page.waitForTimeout(120);
const lvl = await lastCall(page, "setLayer");
check("moving an open fader sends a gain, not a rebuild",
  lvl && lvl.args.id === "brown" && lvl.args.level > 0,
  JSON.stringify(lvl?.args));
check("moving an open fader did not re-upload",
  (await callsOf(page, "prepareBegin")).length === 1);

// A fader coming up off zero must render and upload its loop first.
await page.evaluate(() => {
  const i = document.querySelectorAll("#mixRows input")[3];   // soft rain
  i.value = 45;
  i.dispatchEvent(new Event("input", { bubbles: true }));
  i.dispatchEvent(new Event("change", { bubbles: true }));
});
await wait(page, () => window.__calls.some((c) => c.name === "prepareEnd" && c.args.id === "softrain"));
await page.waitForTimeout(200);
check("a fader raised off zero renders and uploads its loop",
  (await callsOf(page, "prepareBegin")).length === 2);
const soft = await page.evaluate(() =>
  window.__calls.filter((c) => c.name === "setLayer" && c.args.id === "softrain").pop());
check("and is then opened natively", soft && soft.args.level > 0, JSON.stringify(soft?.args));

// Taking it back to zero retires it without touching the cache.
await page.evaluate(() => {
  const i = document.querySelectorAll("#mixRows input")[3];
  i.value = 0;
  i.dispatchEvent(new Event("input", { bubbles: true }));
  i.dispatchEvent(new Event("change", { bubbles: true }));
});
await page.waitForTimeout(500);
const off = await page.evaluate(() =>
  window.__calls.filter((c) => c.name === "setLayer" && c.args.id === "softrain").pop());
check("a fader taken to zero is retired with gain 0", off && off.args.level === 0, JSON.stringify(off?.args));

// Sliders.
await page.evaluate(() => {
  const v = document.getElementById("vol");
  v.value = 80; v.dispatchEvent(new Event("input", { bubbles: true }));
  const t = document.getElementById("tone");
  t.value = 30; t.dispatchEvent(new Event("input", { bubbles: true }));
});
await page.waitForTimeout(120);
check("volume slider reaches the engine", (await lastCall(page, "setVolume")).args.value > 0.5);
check("tone slider reaches the engine", (await lastCall(page, "setTone")).args.hz < 2000,
  `${Math.round((await lastCall(page, "setTone")).args.hz)} Hz`);

// Swell. It lives inside the collapsed "More options" panel.
await page.evaluate(() => { document.getElementById("advanced").open = true; });
await page.waitForTimeout(150);
await page.click("#wavesSw");
await page.waitForTimeout(120);
const swell = await lastCall(page, "setSwell");
check("swell toggle reaches the engine", swell && swell.args.on === true && swell.args.depth > 0,
  JSON.stringify(swell?.args));

/* ---------------- second play uses the cache ---------------- */
console.log("\nsecond play (loops cached)");
await page.click("#playBtn");                     // pause
await page.waitForTimeout(400);
check("pause stops the engine", (await callsOf(page, "stop")).length >= 1);
const beforeSecond = (await callsOf(page, "prepareBegin")).length;
await page.click("#playBtn");                     // play again
await wait(page, () => window.__calls.filter((c) => c.name === "start").length >= 2);
await page.waitForTimeout(200);
check("a second start renders nothing at all",
  (await callsOf(page, "prepareBegin")).length === beforeSecond,
  `${beforeSecond} uploads before and after`);

/* ---------------- a theme needs its two loops ---------------- */
console.log("\nambient theme");
await page.click('#presetChips button[data-preset="rainroom"]');
await wait(page, () => window.__calls.some((c) => c.name === "prepareEnd" && c.args.id === "pad-rainroom-mel"));
await page.waitForTimeout(300);
const padStart = await lastCall(page, "start");
check("theme uploads a chord bed and a melody line",
  (await callsOf(page, "prepareEnd")).some((c) => c.args.id === "pad-rainroom-bed") &&
  (await callsOf(page, "prepareEnd")).some((c) => c.args.id === "pad-rainroom-mel"));
check("the two ambient loops are co-prime lengths",
  (await page.evaluate(() => {
    const u = window.__uploads;
    const frames = (id) => u[id].bytes / 4 / 48000;
    return [frames("pad-rainroom-bed"), frames("pad-rainroom-mel")];
  })).join("/") === "23/31");
check("start is given both ambient loop ids",
  (padStart.args.padIds || []).length === 2, JSON.stringify(padStart.args.padIds));
check("ambient level is passed as a gain", padStart.args.padLevel > 0, String(padStart.args.padLevel));

/* ---------------- blackout ---------------- */
console.log("\nblackout");
await page.evaluate(() => { if (!document.getElementById("dim").classList.contains("show")) document.getElementById("dimBtn").click(); });
await page.waitForTimeout(250);
check("blackout hides the status bar",
  (await lastCall(page, "setStatusBarHidden")).args.value === true);
check("blackout holds the idle timer",
  (await lastCall(page, "setIdleTimerDisabled")).args.value === true);
await page.evaluate(() => document.getElementById("dimExit").click());
await page.waitForTimeout(250);
check("leaving blackout restores the status bar",
  (await lastCall(page, "setStatusBarHidden")).args.value === false);
check("leaving blackout releases the idle timer",
  (await lastCall(page, "setIdleTimerDisabled")).args.value === false);

/* ---------------- haptics ---------------- */
console.log("\nhaptics");
const haptics = await callsOf(page, "haptic");
check("haptics fire on presets, transport and blackout", haptics.length >= 3, `${haptics.length} so far`);
const duringDrag = await page.evaluate(() => {
  const before = window.__calls.filter((c) => c.name === "haptic").length;
  const i = document.querySelectorAll("#mixRows input")[0];
  for (let v = 40; v <= 60; v++) { i.value = v; i.dispatchEvent(new Event("input", { bubbles: true })); }
  return window.__calls.filter((c) => c.name === "haptic").length - before;
});
check("no haptics while a fader is being dragged", duringDrag === 0, `${duringDrag} during 21 input events`);

/* ---------------- version line ---------------- */
console.log("\nversion line");
await page.waitForTimeout(300);
const ver = await page.evaluate(() => document.getElementById("verLine").textContent);
check("the app states its version, so a bug report can name one",
  /Sleeper Agent .+ \(.+\) · iOS /.test(ver), ver || "(empty)");
check("the environment was read from native",
  (await callsOf(page, "getEnvironment")).length >= 1);

/* ---------------- settings mirror ---------------- */
console.log("\nsettings mirror");
await page.waitForTimeout(600);
const backing = await page.evaluate(() => {
  try { return { mirrored: JSON.parse(window.__backing || "null"), local: JSON.parse(localStorage.getItem("brokild-sleep-noise")) }; }
  catch { return null; }
});
check("settings are mirrored to native storage", !!backing?.mirrored);
check("the mirror matches localStorage",
  JSON.stringify(backing.mirrored) === JSON.stringify(backing.local));
const mirrorCalls = await callsOf(page, "setBacking");
check("the mirror is coalesced, not written per keystroke",
  mirrorCalls.length < 12, `${mirrorCalls.length} writes for the whole session`);

/* ---------------- engine events ---------------- */
console.log("\nengine events");
const wired = await page.evaluate(() => window.__listenerNames());
for (const ev of ["finished", "interrupted", "interruptionEnded", "routeLost", "needsRestart",
                  "remotePlay", "remotePause", "remoteToggle"]) {
  check(`listens for "${ev}"`, wired.includes(ev));
}

// An interruption while playing.
await page.evaluate(() => { if (document.getElementById("playBtn").getAttribute("aria-label") === "Play") document.getElementById("playBtn").click(); });
await wait(page, () => document.getElementById("playBtn").getAttribute("aria-label") === "Pause");
await page.evaluate(() => window.__fire("interrupted", { resumable: false }));
await page.waitForTimeout(200);
const afterInt = await page.evaluate(() => ({
  label: document.getElementById("playBtn").getAttribute("aria-label"),
  status: document.getElementById("statusNote").textContent,
}));
check("an interruption shows as paused", afterInt.label === "Play", afterInt.label);
check("and says why", /Paused by the phone/.test(afterInt.status), afterInt.status);

// The lock screen asking to resume.
await page.evaluate(() => window.__fire("remoteToggle", {}));
await wait(page, () => document.getElementById("playBtn").getAttribute("aria-label") === "Pause");
check("the lock-screen toggle resumes playback", true);

// The timer finishing in Swift while this side was frozen.
await page.evaluate(() => window.__fire("finished", {}));
await page.waitForTimeout(200);
const done = await page.evaluate(() => ({
  label: document.getElementById("playBtn").getAttribute("aria-label"),
  status: document.getElementById("statusNote").textContent,
}));
check("the timer finishing natively stops the UI", done.label === "Play");
check("and reports it", /Timer finished/.test(done.status), done.status);

// Coming back to the foreground consults native rather than its own clock.
await page.evaluate(() => {
  window.__mockState = { playing: true, remainingMs: 42 * 60 * 1000, unlimited: false };
  document.dispatchEvent(new Event("visibilitychange"));
});
await page.waitForTimeout(300);
check("returning to the foreground asks native for the clock",
  (await callsOf(page, "getState")).length >= 1);
const remain = await page.evaluate(() => document.getElementById("remain").textContent);
check("and adopts the time native reports", /^4[12] m/.test(remain), remain);

/* ---------------- clean ---------------- */
check("no console or page errors anywhere", errors.length === 0, errors.slice(0, 4).join(" | "));

console.log(`\n${pass} passed, ${failures.length} failed`);
await browser.close();
if (failures.length) {
  console.log("\nfailures:");
  failures.forEach((f) => console.log("  - " + f));
  process.exit(1);
}
