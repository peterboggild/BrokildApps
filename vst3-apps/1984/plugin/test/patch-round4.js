// Round 4: the Prophet's sync topology (rank I is the slave, rank II the
// silent master, the envelope sweeps the slave), two patch fixes, the
// bench's sync test, the panel label, the site's brand fonts.
"use strict";
const fs = require("fs");
const path = require("path");
const files = {};
function load(key, p) { const raw = fs.readFileSync(p, "utf8"); files[key] = { p, crlf: raw.indexOf("\r\n") >= 0, s: raw.replace(/\r\n/g, "\n"), n: 0 }; }
function edit(key, from, to, count = 1) {
  const f = files[key];
  const parts = f.s.split(from);
  if (parts.length - 1 !== count) { console.error("ANCHOR MISS in " + key + " (" + (parts.length - 1) + " of " + count + "):\n" + from.slice(0, 160)); process.exit(1); }
  f.s = parts.join(to); f.n++;
}
const root = "C:/Users/peter/b/Nineteen84/";
load("engine", root + "Source/Engine.cpp");
load("bench", root + "test/bench.cpp");
load("patches", root + "Source/Patches.cpp");
load("ui", root + "Source/ui/ui.html");
load("landing", "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/1984/index.html");
load("design", "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/1984-DESIGN.md");

// ---- engine: rank II ticks first and is the master; rank I is reset by it
edit("engine",
`        for (int r = 0; r < NUM_RANKS; ++r)
        {
            Rank& k = v.rk[r];
            const RankBlock& b = rb[r];
            float inc = k.pitchNorm, pw = k.pwHeld;`,
`        /*  Rank II first: it is the sync MASTER and the poly-mod source. Rank I
            is the slave and the poly-mod destination, so the filter envelope
            sweeps the synced rank - the Prophet's topology, where the audible
            oscillator is the one that is reset. */
        for (int rr = 0; rr < NUM_RANKS; ++rr)
        {
            const int r = NUM_RANKS - 1 - rr;
            Rank& k = v.rk[r];
            const RankBlock& b = rb[r];
            float inc = k.pitchNorm, pw = k.pwHeld;`);
edit("engine",
`            k.osc.tick (inc, pw, r == 1 && sync ? syncF : -1.0f);
            if (r == 0) syncF = k.osc.wrapF;`,
`            k.osc.tick (inc, pw, r == 0 && sync ? syncF : -1.0f);
            if (r == 1) syncF = k.osc.wrapF;`);
edit("engine",
`    RROW (1, "b_sync", "II SYNC TO I", 0.0f, KP_SW, 0, 0, sync),`,
`    RROW (1, "b_sync", "SYNC I TO II", 0.0f, KP_SW, 0, 0, sync),`);

// ---- bench: the slave is rank I now
edit("bench",
`                Rig r; r.set ("a_lvl", 0.0f); r.set ("b_lvl", 0.8f); r.set ("b_semi", 0.5f + 7.0f / 24.0f); r.set ("b_sync", sync ? 1.0f : 0.0f);`,
`                Rig r; r.set ("b_lvl", 0.0f); r.set ("a_semi", 0.5f + 7.0f / 24.0f); r.set ("b_sync", sync ? 1.0f : 0.0f);`);
edit("bench",
`            check (withSync < 0.15 && without > 1.0, "hard sync: the slave's own pitch vanishes, the master's harmonics appear", withSync, without);`,
`            check (withSync < 0.15 && without > 1.0, "hard sync (rank I slaved to a silent rank II): the slave's own pitch vanishes, the master's harmonics appear", withSync, without);`);

// ---- patches
edit("patches",
`        c.sw ("b_sync", true); c.semi ("b_semi", 7);`,
`        c.sw ("b_sync", true); c.semi ("a_semi", 7);       // rank I is the slave: the audible, swept one`);
edit("patches",
`        c.list ("lfo_wave", 4); c.hz_ ("lfo_rate", 5.9f); c.pct ("lfo_vcf", 0);`,
`        c.list ("lfo_wave", 4); c.hz_ ("lfo_rate", 5.9f); c.pct ("lfo_vca", 45); c.list ("lfo_mode", 2);   // the pulse`);

// ---- panel label
edit("ui", `buildCtl("b_sync", "SYNC<br>TO I"`, `buildCtl("b_sync", "SYNC<br>I&rarr;II"`);

// ---- landing page: the brand mark's own faces, as every sibling page carries
edit("landing", `<title>`, `<link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Orbitron:wght@500;700;900&family=IBM+Plex+Mono:wght@400;500;600&display=swap">
  <title>`);

// ---- design record
edit("design",
`both loudness and brightness. Rank II can be hard-synced to rank I and both
can be bent by poly-mod (rank II into rank I's pitch, width and filter; the
filter envelope into pitch and width).`,
`both loudness and brightness. Rank I can be hard-synced to rank II (the
Prophet's way round: the audible rank is the slave, the silent master holds
the note) and both can be bent by poly-mod (rank II into rank I's pitch, width
and filter; the filter envelope into rank I's pitch and width — so the
envelope sweeps the synced rank).`);
edit("design",
`per voice, into the filter. Rank II's phase can be reset by rank I's wrap
(hard sync) with the discontinuity band-limited at the actual jump height —
the Black Rider mechanism, verbatim.`,
`per voice, into the filter. Rank I's phase can be reset by rank II's wrap
(hard sync) with the discontinuity band-limited at the actual jump height —
the Black Rider mechanism, verbatim. Rank II is the master because the
poly-mod destinations are on rank I: SYNC LEAD is a silent rank II holding the
note while the envelope sweeps the audible, synced rank I.`);

for (const k in files) { const f = files[k]; fs.writeFileSync(f.p, f.crlf ? f.s.replace(/\n/g, "\r\n") : f.s); console.log(k + ": " + f.n + " edits"); }
