/**
 * Sleeper Agent — functionality baseline / regression suite.
 *
 * Written for the iOS port: it pins down what the web application does today
 * so that any change made for the sake of the native build can be shown not
 * to have altered it. Run it before and after every port change.
 *
 *   node health-apps/sleep-noise/test/regression.mjs
 *   node health-apps/sleep-noise/test/regression.mjs --slow   (adds the ~90 s
 *                                                              sleep-timer test)
 *
 * Two halves:
 *   DSP    — the generators are pulled out of index.html and run headlessly in
 *            node against a stub AudioContext, so the sound itself is measured
 *            (level, peak, non-finite samples, loop-seam continuity) without a
 *            browser in the way.
 *   APP    — the real page is driven in headless Chromium: presets, mixer,
 *            persistence, migration from pre-mixer settings, transport.
 *
 * No network access and no fixtures: everything is derived from index.html.
 */

import { readFileSync, existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const HERE = dirname(fileURLToPath(import.meta.url));
const APP = resolve(HERE, "..", "index.html");
const APP_URL = "file://" + APP;
const SLOW = process.argv.includes("--slow");

const CHROMIUM = [
  "/opt/pw-browsers/chromium",
  "/opt/pw-browsers/chromium-1194/chrome-linux/chrome",
].find(existsSync);

let pass = 0;
const failures = [];

function check(name, ok, detail = "") {
  if (ok) {
    pass++;
    console.log(`  ok   ${name}${detail ? "  — " + detail : ""}`);
  } else {
    failures.push(`${name}${detail ? " — " + detail : ""}`);
    console.log(`  FAIL ${name}${detail ? "  — " + detail : ""}`);
  }
}

/* ------------------------------------------------------------------ *
 * DSP: run the generators in node
 * ------------------------------------------------------------------ */

const html = readFileSync(APP, "utf8");
const script = html.match(/<script>\n([\s\S]*)\n<\/script>/)[1];

// The generator half of the file: from the biquad down to the end of
// makeBuffer, which is self-contained apart from LOOP_SEC / XFADE_SEC.
const dspStart = script.indexOf("  // Robert Bristow-Johnson biquad");
const dspEnd = script.indexOf("  /* ============================================================\n     Ambient synth");
if (dspStart < 0 || dspEnd < 0) {
  console.error("regression: could not locate the DSP block in index.html — has it been restructured?");
  process.exit(2);
}
const dsp = script.slice(dspStart, dspEnd);

const LAYERS = ["brown", "green", "rain", "softrain", "ocean"];

function stubContext(sampleRate) {
  return {
    sampleRate,
    createBuffer(channels, length, rate) {
      const data = Array.from({ length: channels }, () => new Float32Array(length));
      return {
        length, numberOfChannels: channels, sampleRate: rate,
        getChannelData: (i) => data[i],
      };
    },
  };
}

function measure(buf) {
  const n = buf.length;
  let energy = 0, peak = 0, nonFinite = 0;
  for (let c = 0; c < buf.numberOfChannels; c++) {
    const d = buf.getChannelData(c);
    for (let i = 0; i < n; i++) {
      const v = d[i];
      if (!Number.isFinite(v)) nonFinite++;
      energy += v * v;
      const m = Math.abs(v);
      if (m > peak) peak = m;
    }
  }
  const rms = Math.sqrt(energy / (n * buf.numberOfChannels));

  // The loop seam. A buffer that repeats cleanly must not step further at the
  // wrap than it does anywhere inside itself, or the join is a click.
  const d0 = buf.getChannelData(0);
  const wrapStep = Math.abs(d0[0] - d0[n - 1]);
  let maxStep = 0;
  for (let i = 1; i < n; i++) {
    const s = Math.abs(d0[i] - d0[i - 1]);
    if (s > maxStep) maxStep = s;
  }

  // Mean offset: a DC component would waste headroom and thump on lock.
  let sum = 0;
  for (let i = 0; i < n; i++) sum += d0[i];
  const dc = sum / n;

  // Channel balance: the two channels are generated independently and should
  // land within a hair of each other.
  let bal = 1;
  if (buf.numberOfChannels === 2) {
    const rmsOf = (d) => {
      let e = 0;
      for (let i = 0; i < n; i++) e += d[i] * d[i];
      return Math.sqrt(e / n);
    };
    bal = rmsOf(buf.getChannelData(0)) / rmsOf(buf.getChannelData(1));
  }
  return { rms, peak, nonFinite, wrapStep, maxStep, dc, bal };
}

function dspSuite() {
  console.log("\nDSP — generators (48 kHz, stub context)");
  const make = new Function(
    "ctxFactory",
    dsp + "\nreturn { makeBuffer: makeBuffer, genRaw: genRaw, LOOP_SEC: LOOP_SEC, XFADE_SEC: XFADE_SEC };"
  )();

  check("loop length is 20 s with a 1 s cross-fade",
    make.LOOP_SEC === 20 && make.XFADE_SEC === 1,
    `LOOP_SEC=${make.LOOP_SEC} XFADE_SEC=${make.XFADE_SEC}`);

  for (const sampleRate of [44100, 48000]) {
    for (const id of LAYERS) {
      const t0 = Date.now();
      const buf = make.makeBuffer(stubContext(sampleRate), id);
      const ms = Date.now() - t0;
      const m = measure(buf);
      const tag = `${id}@${sampleRate / 1000}k`;

      check(`${tag}: no non-finite samples`, m.nonFinite === 0, `${m.nonFinite} bad`);
      check(`${tag}: peak within headroom`, m.peak <= 0.9001, `peak ${m.peak.toFixed(3)}`);
      check(`${tag}: audible level`, m.rms > 0.05 && m.rms < 0.30, `rms ${m.rms.toFixed(4)}`);
      check(`${tag}: no DC offset`, Math.abs(m.dc) < 0.002, `dc ${m.dc.toExponential(2)}`);
      check(`${tag}: channels balanced`, m.bal > 0.8 && m.bal < 1.25, `L/R ${m.bal.toFixed(3)}`);
      check(`${tag}: loop seam inaudible`, m.wrapStep < m.maxStep,
        `wrap ${m.wrapStep.toFixed(4)} < max ${m.maxStep.toFixed(4)}`);
      check(`${tag}: builds in reasonable time`, ms < 5000, `${ms} ms`);
    }
  }
}

/* ------------------------------------------------------------------ *
 * APP: drive the real page
 * ------------------------------------------------------------------ */

// Playwright may be a project dependency or only installed globally; ESM does
// not consult NODE_PATH, so the global location is tried explicitly.
async function importPlaywright() {
  const candidates = [
    "playwright",
    "/opt/node22/lib/node_modules/playwright/index.js",
    "/usr/lib/node_modules/playwright/index.js",
  ];
  for (const c of candidates) {
    try {
      const mod = await import(c);
      // Imported by absolute path, playwright arrives as a CommonJS default.
      return mod.chromium ? mod : mod.default;
    } catch { /* next */ }
  }
  throw new Error("playwright not found (tried: " + candidates.join(", ") + ")");
}

async function appSuite() {
  if (!CHROMIUM) {
    console.log("\nAPP — skipped: no Chromium found (looked in /opt/pw-browsers)");
    return;
  }
  const { chromium } = await importPlaywright();
  const browser = await chromium.launch({
    executablePath: CHROMIUM,
    args: ["--autoplay-policy=no-user-gesture-required", "--no-sandbox"],
  });

  const errors = [];
  const newPage = async () => {
    const p = await browser.newPage({ viewport: { width: 390, height: 844 } });
    p.on("pageerror", (e) => errors.push("pageerror: " + e.message));
    p.on("console", (m) => { if (m.type() === "error") errors.push("console: " + m.text()); });
    return p;
  };

  /* ---- panel shape ---- */
  console.log("\nAPP — panel");
  let p = await newPage();
  await p.goto(APP_URL);
  await p.waitForTimeout(300);

  const shape = await p.evaluate(() => ({
    presets: [...document.querySelectorAll("#presetChips button")].map((b) => b.textContent),
    faders: [...document.querySelectorAll("#mixRows .mixrow")].map((r) => r.querySelector(".mixname").textContent),
    pads: [...document.querySelectorAll("#padChips button")].map((b) => b.textContent),
    sliders: ["vol", "tone", "padLevel", "waveDepth", "wavePeriod"].filter((id) => document.getElementById(id)),
    durations: [...document.querySelectorAll("#durChips button")].length,
    hasTimerModes: !!document.getElementById("modeFor") && !!document.getElementById("modeUntil"),
    hasBlackout: !!document.getElementById("dimBtn") && !!document.getElementById("dim"),
    switches: ["wavesSw", "bgSw", "wakeSw", "autoDimSw"].filter((id) => document.getElementById(id)),
  }));

  check("five presets", shape.presets.length === 5, shape.presets.join(", "));
  check("five mixer faders", shape.faders.length === 5, shape.faders.join(", "));
  check("faders are brown/green/rain/soft rain/ocean",
    shape.faders.join("|") === "Brown|Green|Rain|Soft rain|Ocean", shape.faders.join("|"));
  check("six ambient theme buttons", shape.pads.length === 6, shape.pads.join(", "));
  check("volume, tone, ambient level, swell sliders present", shape.sliders.length === 5, shape.sliders.join(", "));
  check("twelve duration chips", shape.durations === 12, String(shape.durations));
  check("both timer modes present", shape.hasTimerModes);
  check("blackout screen present", shape.hasBlackout);
  check("four option switches", shape.switches.length === 4, shape.switches.join(", "));

  /* ---- presets ---- */
  console.log("\nAPP — presets");
  const EXPECTED = {
    deep:     { mix: [72, 0, 0, 0, 0],   tone: 45, pad: "off",      padLevel: 40 },
    rainroom: { mix: [55, 0, 0, 48, 0],  tone: 60, pad: "rainroom", padLevel: 34 },
    forest:   { mix: [18, 62, 24, 0, 0], tone: 70, pad: "off",      padLevel: 40 },
    shore:    { mix: [30, 0, 0, 0, 70],  tone: 62, pad: "off",      padLevel: 40 },
    drift:    { mix: [0, 30, 0, 14, 22], tone: 66, pad: "meadow",   padLevel: 55 },
  };
  for (const [id, want] of Object.entries(EXPECTED)) {
    await p.click(`#presetChips button[data-preset="${id}"]`);
    await p.waitForTimeout(80);
    const got = await p.evaluate(() => ({
      mix: [...document.querySelectorAll("#mixRows input")].map((i) => +i.value),
      tone: +document.getElementById("tone").value,
      padLevel: +document.getElementById("padLevel").value,
      pad: [...document.querySelectorAll("#padChips button")]
        .find((b) => b.getAttribute("aria-pressed") === "true")?.dataset.pad,
      lit: [...document.querySelectorAll("#presetChips button")]
        .filter((b) => b.getAttribute("aria-pressed") === "true").map((b) => b.dataset.preset),
    }));
    check(`preset ${id} sets the whole panel`,
      got.mix.join() === want.mix.join() && got.tone === want.tone &&
      got.pad === want.pad && got.padLevel === want.padLevel,
      `mix ${got.mix.join()} tone ${got.tone} pad ${got.pad} level ${got.padLevel}`);
    check(`preset ${id} is the only one lit`, got.lit.length === 1 && got.lit[0] === id, got.lit.join());
  }

  // The requested mix: brown + soft rain + the rainroom theme.
  await p.click('#presetChips button[data-preset="rainroom"]');
  const rr = await p.evaluate(() => ({
    brown: +document.querySelectorAll("#mixRows input")[0].value,
    soft: +document.querySelectorAll("#mixRows input")[3].value,
    pad: document.getElementById("padVal").textContent,
    level: +document.getElementById("padLevel").value,
  }));
  check("Rainroom preset is brown + soft rain + rainroom theme",
    rr.brown > 0 && rr.soft > 0 && rr.pad === "Rainroom" && rr.level > 0,
    `brown ${rr.brown}, soft rain ${rr.soft}, ${rr.pad} at ${rr.level}`);

  // Moving a fader must drop the panel back to "custom".
  await p.evaluate(() => {
    const i = document.querySelectorAll("#mixRows input")[1];
    i.value = 40;
    i.dispatchEvent(new Event("input", { bubbles: true }));
  });
  check("moving a fader clears the preset",
    (await p.evaluate(() => document.getElementById("presetVal").textContent)) === "custom");

  /* ---- persistence and migration ---- */
  console.log("\nAPP — persistence");
  await p.click('#presetChips button[data-preset="shore"]');
  await p.evaluate(() => {
    const i = document.querySelectorAll("#mixRows input")[2];
    i.value = 33;
    i.dispatchEvent(new Event("input", { bubbles: true }));
  });
  await p.reload();
  await p.waitForTimeout(200);
  check("mixer survives a reload",
    (await p.evaluate(() => [...document.querySelectorAll("#mixRows input")].map((i) => +i.value).join())) === "30,0,33,0,70");

  const MIGRATIONS = {
    brown: "60,0,0,0,0",
    green: "0,60,0,0,0",
    pink: "60,0,0,0,0",      // no fader of its own: nearest neighbour
    white: "0,60,0,0,0",
    rain: "0,0,60,0,0",
    off: "0,0,0,0,0",
  };
  for (const [type, want] of Object.entries(MIGRATIONS)) {
    await p.evaluate((t) => localStorage.setItem("brokild-sleep-noise", JSON.stringify({
      type: t, volume: 70, tone: 40, durationMin: 120, pad: "dusk", padLevel: 45,
    })), type);
    await p.reload();
    await p.waitForTimeout(150);
    const got = await p.evaluate(() => ({
      mix: [...document.querySelectorAll("#mixRows input")].map((i) => +i.value).join(),
      vol: +document.getElementById("vol").value,
      dur: document.getElementById("durVal").textContent,
      pad: document.getElementById("padVal").textContent,
    }));
    check(`pre-mixer settings (type=${type}) migrate`,
      got.mix === want && got.vol === 70 && got.pad === "Dusk" && got.dur === "2 h",
      `mix ${got.mix}, vol ${got.vol}, ${got.dur}, ${got.pad}`);
  }
  await p.close();

  /* ---- transport ---- */
  console.log("\nAPP — transport");
  p = await newPage();
  await p.goto(APP_URL);
  await p.evaluate(() => localStorage.clear());
  await p.reload();

  await p.click('#presetChips button[data-preset="rainroom"]');
  const t0 = Date.now();
  await p.click("#playBtn");
  await p.waitForFunction(() => !document.getElementById("playBtn").classList.contains("busy"), { timeout: 60000 });
  const buildMs = Date.now() - t0;
  await p.waitForTimeout(1500);

  const playing = await p.evaluate(() => ({
    label: document.getElementById("playBtn").getAttribute("aria-label"),
    remain: document.getElementById("remain").textContent,
    status: document.getElementById("statusNote").textContent,
    sink: document.getElementById("sink").currentTime,
    paused: document.getElementById("sink").paused,
    nowPlaying: navigator.mediaSession?.metadata?.title ?? null,
  }));
  check("play starts", playing.label === "Pause", playing.remain);
  check("no error status on start", playing.status === "", playing.status);
  check("audio is flowing through the media element", playing.sink > 0.5 && !playing.paused,
    `sink clock ${playing.sink.toFixed(2)} s`);
  check("Now Playing metadata is set", playing.nowPlaying === "Rainroom", String(playing.nowPlaying));
  check("start build time is tolerable", buildMs < 20000, `${buildMs} ms`);

  // A fader raised mid-playback joins the mix without a dropout.
  const before = await p.evaluate(() => document.getElementById("sink").currentTime);
  await p.evaluate(() => {
    const i = document.querySelectorAll("#mixRows input")[4];
    i.value = 60;
    i.dispatchEvent(new Event("input", { bubbles: true }));
    i.dispatchEvent(new Event("change", { bubbles: true }));
  });
  await p.waitForTimeout(3000);
  const after = await p.evaluate(() => ({
    sink: document.getElementById("sink").currentTime,
    paused: document.getElementById("sink").paused,
    mixVal: document.getElementById("mixVal").textContent,
  }));
  check("a fader raised while playing joins without stopping the audio",
    after.sink > before + 1 && !after.paused, `clock ${before.toFixed(1)} to ${after.sink.toFixed(1)} s`);
  check("mixer summary reflects three open faders", after.mixVal === "3 sounds", after.mixVal);

  // Every fader down and the theme off is legal: the timer still runs.
  await p.evaluate(() => {
    document.querySelectorAll("#mixRows input").forEach((i) => {
      i.value = 0;
      i.dispatchEvent(new Event("input", { bubbles: true }));
    });
    document.querySelector('#padChips button[data-pad="off"]').click();
    document.querySelectorAll("#mixRows input")[0].dispatchEvent(new Event("change", { bubbles: true }));
  });
  await p.waitForTimeout(1200);
  const silent = await p.evaluate(() => ({
    mixVal: document.getElementById("mixVal").textContent,
    label: document.getElementById("playBtn").getAttribute("aria-label"),
    sub: document.getElementById("sub").textContent,
  }));
  check("all faders down is silent but still running",
    silent.mixVal === "silent" && silent.label === "Pause" && silent.sub.startsWith("Silent"), silent.sub);

  // Pause and resume.
  await p.click("#playBtn");
  await p.waitForTimeout(600);
  check("pause stops the transport",
    (await p.evaluate(() => document.getElementById("playBtn").getAttribute("aria-label"))) === "Play");

  /* ---- blackout ---- */
  console.log("\nAPP — blackout");
  await p.click("#dimBtn");
  await p.waitForTimeout(300);
  const dim = await p.evaluate(() => {
    const d = document.getElementById("dim");
    return {
      shown: d.classList.contains("show"),
      bg: getComputedStyle(d).backgroundColor,
      hasExit: !!document.getElementById("dimExit"),
      clock: document.getElementById("dimClock").textContent,
    };
  });
  check("blackout shows", dim.shown);
  check("blackout is true black", dim.bg === "rgb(0, 0, 0)", dim.bg);
  check("blackout has an exit control", dim.hasExit);
  check("blackout shows a clock", /^\d\d:\d\d$/.test(dim.clock), dim.clock);
  await p.evaluate(() => document.getElementById("dimExit").click());
  await p.waitForTimeout(200);
  check("blackout exits",
    !(await p.evaluate(() => document.getElementById("dim").classList.contains("show"))));

  /* ---- defaults a first-time user meets ---- */
  console.log("\nAPP — first-run defaults");
  await p.evaluate(() => localStorage.clear());
  await p.reload();
  await p.waitForTimeout(250);
  const firstRun = await p.evaluate(() => ({
    fadeIn: document.getElementById("fiVal").textContent,
    fadeInLit: [...document.querySelectorAll("#fiChips button")]
      .filter((b) => b.getAttribute("aria-pressed") === "true").map((b) => b.textContent),
    fadeInOptions: [...document.querySelectorAll("#fiChips button")].map((b) => b.textContent),
    volume: +document.getElementById("vol").value,
  }));
  // A long fade-in is a trap on first use: the app is quiet when you tap play,
  // so you turn it up, and the fade then arrives on top of that level.
  check("fade-in defaults to one second", firstRun.fadeIn === "1 s", firstRun.fadeIn);
  check("and its chip is the one lit", firstRun.fadeInLit.join() === "1 s", firstRun.fadeInLit.join());
  check("fade-in offers a short end and a long one",
    firstRun.fadeInOptions.length === 8 && firstRun.fadeInOptions.includes("1 s") &&
    firstRun.fadeInOptions.includes("5 min"), firstRun.fadeInOptions.join(" "));

  /* ---- text is legible on the dark ground ---- */
  console.log("\nAPP — contrast");
  const contrast = await p.evaluate(() => {
    const lin = (c) => { c /= 255; return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4); };
    const parse = (s) => (s.match(/[\d.]+/g) || []).map(Number);
    const lum = ([r, g, b]) => 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
    const ratio = (a, b) => {
      const la = lum(a), lb = lum(b), hi = Math.max(la, lb), lo = Math.min(la, lb);
      return (hi + 0.05) / (lo + 0.05);
    };
    // The first ancestor that actually paints a background.
    const groundOf = (el) => {
      let n = el;
      while (n && n !== document.documentElement) {
        const bg = parse(getComputedStyle(n).backgroundColor);
        if (bg.length >= 3 && (bg[3] === undefined || bg[3] > 0.5)) return bg.slice(0, 3);
        n = n.parentElement;
      }
      return parse(getComputedStyle(document.body).backgroundColor).slice(0, 3);
    };
    const out = {};
    for (const [name, sel] of [
      ["body copy", "body"],
      ["section label", ".lab"],
      ["explanatory note", ".note"],
      ["preset description", ".desc"],
      ["mixer fader name", ".mixname"],
      ["mixer fader value", ".mixval"],
      ["transport subtitle", ".sub"],
      ["chip label", ".chip"],
      ["footer", "footer"],
      ["switch caption", ".toggle .h"],
    ]) {
      const el = document.querySelector(sel);
      if (!el) { out[name] = null; continue; }
      const fg = parse(getComputedStyle(el).color).slice(0, 3);
      out[name] = Math.round(ratio(fg, groundOf(el)) * 100) / 100;
    }
    return out;
  });
  // 4.5:1 is WCAG AA for normal text. Every one of these is prose the user is
  // expected to read, not decoration.
  for (const [name, r] of Object.entries(contrast)) {
    check(`${name} meets 4.5:1`, r !== null && r >= 4.5, r === null ? "selector not found" : `${r}:1`);
  }
  check("body copy is comfortably above AA", contrast["body copy"] >= 10, contrast["body copy"] + ":1");

  // The inverse guard: the blackout screen exists to be dark, so it must not
  // brighten when the panel's palette does.
  const blackout = await p.evaluate(() => {
    const d = getComputedStyle(document.getElementById("dim"));
    const meta = getComputedStyle(document.getElementById("dimMeta"));
    const hint = getComputedStyle(document.getElementById("dimHint"));
    const inner = getComputedStyle(document.getElementById("dimInner"));
    return { bg: d.backgroundColor, meta: meta.color, hint: hint.color, opacity: +inner.opacity };
  });
  check("blackout ground is pure black", blackout.bg === "rgb(0, 0, 0)", blackout.bg);
  check("blackout meta text stayed dim", blackout.meta === "rgb(141, 103, 56)", blackout.meta);
  check("blackout hint stayed nearly invisible", blackout.hint === "rgb(30, 28, 25)", blackout.hint);
  check("blackout content is heavily dimmed at rest", blackout.opacity <= 0.2, String(blackout.opacity));

  /* ---- the sleep timer, end to end ---- */
  if (SLOW) {
    console.log("\nAPP — sleep timer (slow)");
    await p.evaluate(() => {
      const t = new Date(Date.now() + 130000);
      localStorage.setItem("brokild-sleep-noise", JSON.stringify({
        mix: { brown: 50, green: 0, rain: 0, softrain: 40, ocean: 0 },
        volume: 55, tone: 62, mode: "until",
        untilTime: ("0" + t.getHours()).slice(-2) + ":" + ("0" + t.getMinutes()).slice(-2),
        fadeInSec: 0, fadeOutMin: 1, pad: "off", padLevel: 40,
        waves: true, waveDepth: 30, wavePeriod: 16,
        bgPlayback: true, keepAwake: true, autoDim: false,
      }));
    });
    await p.reload();
    await p.click("#playBtn");
    await p.waitForFunction(() => !document.getElementById("playBtn").classList.contains("busy"), { timeout: 60000 });

    let finished = false;
    for (let i = 0; i < 30 && !finished; i++) {
      await p.waitForTimeout(6000);
      finished = await p.evaluate(() =>
        document.getElementById("statusNote").textContent.includes("Timer finished"));
    }
    const end = await p.evaluate(() => ({
      label: document.getElementById("playBtn").getAttribute("aria-label"),
      paused: document.getElementById("sink").paused,
    }));
    check("sleep timer fires and stops playback", finished && end.label === "Play", `finished=${finished}`);
    check("system media slot released at the end", end.paused);
  } else {
    console.log("\nAPP — sleep timer: skipped (pass --slow to include the ~90 s test)");
  }

  check("no console or page errors anywhere", errors.length === 0, errors.slice(0, 4).join(" | "));
  await browser.close();
}

/* ------------------------------------------------------------------ */

console.log("Sleeper Agent — regression suite");
console.log("app: " + APP);

dspSuite();
await appSuite();

console.log(`\n${pass} passed, ${failures.length} failed`);
if (failures.length) {
  console.log("\nfailures:");
  failures.forEach((f) => console.log("  - " + f));
  process.exit(1);
}
