/*  BROKILD APPS becomes BROKILD MUSICAL INSTRUMENTS.

    Peter: "I am not going to put other tools on here. Sleeper Agent is almost
    an instrument so it gets to stay. [...] That means that non-music related
    categories also got to go."

    So three labels go: CAMERA and HEALTH because they describe a purpose the
    site no longer has, and MUSIC because when everything is music the label
    stops sorting anything and becomes a badge on every card. What replaces
    them is BROWSER — the honest counterpart to VST3 PLUGIN, and the thing a
    visitor actually wants to know first: does this run in a tab or in my DAW.

    The intro is rewritten to his brief: synthesisers, sound generators and
    effects; unusual things that did not already exist; built for his own
    enjoyment, where nothing on the market did the job the way he wanted.
*/
"use strict";
const fs = require("fs");
const R = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/";
const miss = [];

function edit(rel, subs) {
  const P = R + rel;
  let s;
  try { s = fs.readFileSync(P, "utf8"); } catch (e) { miss.push(rel + ": unreadable"); return; }
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  for (const [a, b, tag] of subs) {
    const A = a.split(String.fromCharCode(10)).join(NL);
    const B = b.split(String.fromCharCode(10)).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(rel + ": " + tag + " x" + n); continue; }
    s = s.split(A).join(B);
  }
  fs.writeFileSync(P, s);
}

// ── 1 · the manifest: labels culled, BROWSER added ─────────────────────────
{
  const P = R + "manifest.json";
  const m = JSON.parse(fs.readFileSync(P, "utf8"));
  m.labels = [
    { id: "vst3",    name: "VST3 Plugin",
      description: "Native audio plugins you download and install \u2014 they run inside your DAW." },
    { id: "browser", name: "Browser",
      description: "Runs in a tab. Nothing to install, nothing uploaded \u2014 the sound is made on your own machine." },
    { id: "synth",   name: "Synth",
      description: "Instruments that make sound \u2014 play them from a keyboard, or let them drone." },
    { id: "effect",  name: "Effect",
      description: "Processors that transform the sound running through them." },
    { id: "phone",   name: "Phone",
      description: "Works on a phone screen \u2014 touch-first, and installable to the home screen." }
  ];
  fs.writeFileSync(P, JSON.stringify(m, null, 2) + "\n");
  console.log("  manifest: 7 labels -> 5, music/camera/health retired");
}

// ── 2 · every app's tags ───────────────────────────────────────────────────
{
  const RETAG = {
    "vst3-apps/collection":        ["vst3"],
    "vst3-apps/clone-wars":        ["vst3", "synth"],
    "vst3-apps/black-rider":       ["vst3", "synth"],
    "vst3-apps/photo-synth-2":     ["vst3", "synth"],
    "vst3-apps/full-metal-racket": ["vst3", "synth"],
    "vst3-apps/blade-ruiner":      ["vst3", "synth"],
    "vst3-apps/escape-room":       ["vst3", "synth"],
    "vst3-apps/martian-gain":      ["vst3", "effect"],
    "vst3-apps/hairfryer":         ["vst3", "effect"],
    "music-apps/photo-synth":      ["browser", "phone", "synth"],
    "health-apps/sleep-noise":     ["browser", "phone", "synth"]
  };
  for (const k of Object.keys(RETAG)) {
    const P = R + k + "/app.json";
    let a;
    try { a = JSON.parse(fs.readFileSync(P, "utf8")); } catch (e) { continue; }
    a.tags = RETAG[k];
    fs.writeFileSync(P, JSON.stringify(a, null, 2) + "\n");
  }
  console.log("  app.json tags rewritten");
}

// ── 3 · the front page ─────────────────────────────────────────────────────
edit("index.html", [
  ["<title>BrokildApps &mdash; Apps &amp; Experiments</title>",
   "<title>Brokild Musical Instruments</title>", "title"],

  ['<meta name="description" content="BrokildApps: apps and experiments by Peter B\u00f8ggild. Music instruments, tools and toys that run right in your browser, plus the free Photo-Synth 2 VST3 plugin for your DAW." />',
   '<meta name="description" content="Brokild Musical Instruments: synthesisers, sound generators and effects by Peter B\u00f8ggild. Unusual instruments that did not already exist \u2014 built because nothing on the market did the job the way he wanted. Free VST3 plugins for Windows, and a few that run in a browser." />', "meta"],

  ['<div class="brand-sub">Apps &amp; experiments by Peter B\u00f8ggild &mdash; browser instruments and audio plugins</div>',
   '<div class="brand-sub">Synthesisers, sound generators and effects by Peter B\u00f8ggild</div>', "brand sub"],

  ['<span class="eyebrow"><span class="dot"></span> Instruments, tools &amp; toys</span>\n'
 + '      <h1>Apps &amp;<br /><span class="accent">Experiments</span></h1>\n'
 + '      <p>A collection of small apps \u2014 musical instruments, tools and experiments. Most run right in your browser with no install and nothing uploaded, on your phone or your laptop; a few, like the Photo-Synth 2 plugin, you download and run inside your DAW.</p>',

   '<span class="eyebrow"><span class="dot"></span> Synthesisers, sound generators &amp; effects</span>\n'
 + '      <h1>Brokild Musical<br /><span class="accent">Instruments</span></h1>\n'
 + '      <p>Instruments that did not exist until they had to. Each one was built for my own use, '
 + 'because nothing I could buy did the particular job the way I wanted it done \u2014 and the point '
 + 'was never to make a better version of something familiar, but to end up somewhere no existing '
 + 'instrument would have taken me. Most are free VST3 plugins for Windows; a couple run in a '
 + 'browser tab. All of them are yours to use.</p>', "hero"],

  ['<h2>Small apps, no strings attached</h2>\n'
 + '        <p>BrokildApps collects apps and experiments built by Peter B\u00f8ggild. Each one is a self-contained static page: free to use, easy to share, and private by design \u2014 photos, audio and data stay on your device. The collection is open and growing; check back as new apps come online.</p>',

   '<h2>Why these exist</h2>\n'
 + '        <p>Every one of these started as a thing I wanted to play and could not find. A synth '
 + 'you perform by moving a cursor across a photograph. A drum machine whose twelve voices share a '
 + 'power supply, so a hard kick makes the whole kit sag. A monosynth built to be pushed until its '
 + 'oscillators lock to each other. None of them is a better version of something that already '
 + 'exists; each one goes somewhere I could not otherwise get to.</p>\n'
 + '        <p>They are made for my own enjoyment, and given away because there is no reason not '
 + 'to. Free, no installer, no account, no telemetry, nothing uploaded anywhere \u2014 the sound is '
 + 'made on your machine and stays there. Nothing in them is asserted either: each is built '
 + 'against a bench that renders real audio and measures it, and the numbers in the manuals come '
 + 'from that rather than from my opinion of it.</p>', "about"],

  ["<span>&copy; <span id=\"year\">2026</span> Peter B\u00f8ggild \u00b7 BrokildApps</span>",
   "<span>&copy; <span id=\"year\">2026</span> Peter B\u00f8ggild \u00b7 Brokild Musical Instruments</span>", "footer"],

  ['<div class="section-label" id="apps">Apps</div>',
   '<div class="section-label" id="apps">The instruments</div>', "section label"],

  ['<a class="btn btn-primary" href="#apps">Explore the apps</a>',
   '<a class="btn btn-primary" href="#apps">See the instruments</a>', "hero button"],

  ['<p class="grid-state" id="gridEmpty" hidden>No apps match the selected labels.</p>',
   '<p class="grid-state" id="gridEmpty" hidden>Nothing matches the selected labels.</p>', "empty state"],

  ['<p class="grid-state" id="gridError" hidden>Couldn\u2019t load the app list. Please try again later.</p>',
   '<p class="grid-state" id="gridError" hidden>Couldn\u2019t load the list. Please try again later.</p>', "error state"],

  ['<p class="grid-state" id="gridLoading">Loading apps\u2026</p>',
   '<p class="grid-state" id="gridLoading">Loading\u2026</p>', "loading state"],

  ['aria-label="Filter apps by label"', 'aria-label="Filter by label"', "aria"]
]);
console.log("  front page rewritten");

// ── 4 · the shared top bar on every landing page ───────────────────────────
{
  const pages = ["black-rider", "blade-ruiner", "clone-wars", "collection", "escape-room",
                 "full-metal-racket", "hairfryer", "martian-gain", "photo-synth-2"];
  let n = 0;
  for (const p of pages) {
    const P = R + "vst3-apps/" + p + "/index.html";
    let s;
    try { s = fs.readFileSync(P, "utf8"); } catch (e) { continue; }
    const before = s;
    s = s.split('<span class="brand-name">BrokildApps</span>')
         .join('<span class="brand-name">Brokild</span>');
    s = s.split('<span class="brand-sub">Apps &amp; experiments</span>')
         .join('<span class="brand-sub">Musical instruments</span>');
    s = s.split('<a class="back" href="../../index.html">\u2190 All apps</a>')
         .join('<a class="back" href="../../index.html">\u2190 All instruments</a>');
    if (s !== before) { fs.writeFileSync(P, s); ++n; }
  }
  console.log("  top bar updated on " + n + " landing page(s)");
}

if (miss.length) { console.error("PROBLEMS:\n  " + miss.join("\n  ")); process.exit(1); }
console.log("Brokild Musical Instruments");
