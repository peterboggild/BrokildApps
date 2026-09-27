// 260904.3 — the record: design doc §12, the CLAUDE.md entry, the BUGLIST.
// (code spans are built with c() — a backtick inside a template literal ends it)
const fs = require("fs");
const misses = [];
const design = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/BRAIN-SCAN-DESIGN.md";
const claude = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
const buglist = "C:/Users/peter/b/BrainScan/BUGLIST.md";
const BT = String.fromCharCode(96);
const c = (t) => BT + t + BT;

// ---- design doc: append ----------------------------------------------------
{
  let s = fs.readFileSync(design, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const add = String.raw`
## 12. The read — 260904.3

Peter, after playing .2: *"more complex waveforms… perhaps they are smoothed
too much?… fairly sinusy… a layer of granularity there could be and which
isn't quite there yet… the sound quality is a bit bland."* He was right, and
the mechanism was measurable before a line changed.

**Three causes, all in the read.** (1) The tricubic B-spline is an
*approximating* kernel — it smooths. Its frequency response is sinc⁴: −3.6 dB
at a quarter of the texel rate, −15.7 dB at the texel Nyquist. (2) Sixty-four
texels: a straight line across the whole cube carried at most 32 harmonics,
and a short line far fewer. (3) The mip level coarsens that further as the
pitch rises. None of it was in the specimens' spirit; SPINE at y = 1 was a
1/n^0.9 stack of 24 harmonics read through a low-pass.

**What shipped.**

- **128³.** ` + c("VN = 128") + String.raw`, ` + c("NLOD = 5") + String.raw`. The panel still renders 64³: it is sent
  ` + c("levelWithSide(64)") + String.raw`, the same bytes binomially averaged. Every specimen was
  rebuilt for the resolution — PULSE's edge 0.004 at the bottom of z (half a
  texel), SPINE 48 harmonics with the roll reaching n^−0.6, past a saw.
- **GRAIN** — the Mitchell-Netravali cubic family (B, C) in ` + c("Volume::sample") + String.raw`,
  from the B-spline (1, 0) to Catmull-Rom (0, ½). Measured on a bright SPINE
  at A2: harmonic 40 rises 4.6 dB from GRAIN 0 to 1 while harmonic 20 moves
  1.3 dB. A kernel, not a tilt. The default is 0.35.
- **CONTRAST / FOLD** — a CT window applied to the *audio*: x = w · 2^(4c),
  then clip to [−1, 1] or a triangle fold of period 4, blended by FOLD. Both
  pieces are piecewise linear, so first-order ADAA uses exact quadratic
  antiderivatives; the shaper is skipped entirely at 0/0, because the ADAA's
  own half-sample average would otherwise take the top octave down 3 dB and
  the plain read must stay the plain read. SINUS THD across CONTRAST 0 / ¼ /
  ½ / ¾ / 1: −120.6 / −18.3 / −10.3 / −7.9 / −6.9 dB, monotonic.
- **The gritty three.** SUTURE, a random staircase (y steps per cycle, z a
  walk through eight patterns, each step a hard edge). ENAMEL, a comb of
  spikes (z how many, y their width; a texel-wide spike is a pulse train with
  every harmonic in it). TENDON, four partials whose ratios slide from
  harmonic to a bell's. Above the 8th harmonic against the fundamental:
  SPINE's middle −24.6 dB, SUTURE +4.4, ENAMEL +10.1, TENDON −2.6.
- **Twelve slots on the dial, with a migration.** The parameter is
  normalised, so 8/8 (CORTEX) reads as 11/11 (TENDON) in any older patch or
  project. ` + c("migrateSpecimen()") + String.raw` remaps once, keyed on the build that wrote the
  file — patch files already carried "build"; the project state did not, so
  it does now, and its absence means "older than every build that writes one".
- **The build moved off the timer.** ` + c("serviceAsync()") + String.raw` builds on a worker and
  publishes on a later tick; ` + c("service()") + String.raw` joins first so the two never race.
  ` + c("buildSpecimen") + String.raw` splits over up to eight threads and keeps an LRU of four.
  MARROW builds in 21 ms on twenty threads — cheap insurance.

**What the update taught.**

- **Measure before spending detail.** The first design coarsened the mip
  level by the window's gain (a ×9 budget at CONTRAST 1: level 4, eight
  texels, at C5). With *no* budget the hardest window on the brightest field
  at C5 aliases at −51.2 dB and a full fold at −64.9: the ADAA carries it.
  The budget was deleted; the only headroom kept is ×1.35 for the
  interpolating kernel, which passes the texel Nyquist the B-spline muffled.
- **A single-cycle read is harmonic, whatever is in the field.** TENDON's
  bell ratios cut to one cycle repeat at f0, so its spectrum sits on multiples
  of f0 by construction; the gloss and the manual say so. Real inharmonicity
  in this engine needs the read to move — scan, modulation, detune — or a
  second reader at a ratio (parked on the BUGLIST).
- **A normalised list parameter cannot grow without a migration**, and the
  migration needs a build id in everything that stores the parameter.
- Bench 85 checks ALL CLEAR; cost 12.6 % (14.9 % with GRAIN 1, the window
  and the fold all on). Panel probe 30/30. Live over CDP: six modules, 33
  parameters, SUTURE published from the worker (the page's volume checksum
  moved), a note through the window, no script errors. Numbers that moved
  with 128³: the claim's anchors −65.8 → −61.2 dB, DC −122.9 → −114.0, LUNG
  with the pyramid forced off −26.0 → −23.8.
`;
  s = s.replace(/\s*$/, "") + NL + add.split("\n").join(NL);
  fs.writeFileSync(design, s, "utf8");
}

// ---- CLAUDE.md: a bullet before the "real scans" entry -------------------
{
  let s = fs.readFileSync(claude, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const anchor = "- 2026-09-04 **real scans, verified live.**";
  const n = s.split(anchor).length - 1;
  if (n !== 1) misses.push("CLAUDE.md: expected 1 of the real-scans anchor, found " + n);
  else
  {
    const entry = String.raw`- 2026-09-04 **260904.3 — THE READ ("the sound quality is a bit bland").** Peter: "more complex waveforms… perhaps they are smoothed too much?… fairly sinusy… a layer of granularity there could be". Right on the mechanism, and measurable before a line changed.
  - **Three causes, all in the read**: the tricubic **B-spline is a smoothing kernel** (sinc⁴: −3.6 dB at a quarter of the texel rate, −16 dB at the texel Nyquist); **64 texels** = 32 harmonics at most across a full line; the mip level coarsens further with pitch. Shipped: VN 64→128 (the panel keeps 64³ — sent ` + c("levelWithSide(64)") + String.raw`, the same bytes binomially averaged); **GRAIN** = the Mitchell-Netravali (B,C) kernel family, B-spline (1,0) → Catmull-Rom (0,½), in ` + c("Volume::sample") + String.raw`; **CONTRAST/FOLD** = a CT window on the AUDIO (gain 2^(4c), clip or triangle-fold, first-order ADAA with exact quadratic antiderivatives, **skipped at 0/0** — the ADAA's own half-sample average would take the top octave down 3 dB); three specimens made of edges (SUTURE staircase, ENAMEL spike comb, TENDON bell-ratio partials); PULSE edge 0.004, SPINE 48 harmonics to n^−0.6. Bench 85 ALL CLEAR (H40 +4.6 dB with H20 moved 1.3; SINUS THD monotonic −120.6 → −6.9 across CONTRAST; SUTURE +4.4 / ENAMEL +10.1 dB above the 8th harmonic vs SPINE's middle −24.6; cost 12.6 %, 14.9 % with everything on); panel probe 30/30; live CDP: 6 modules, 33 params, SUTURE published from the worker, a note through the window, zero JS errors.
  - **Measure before spending detail.** The first design coarsened the mip level by the window's gain (×9 budget at CONTRAST 1 → level 4, eight texels, at C5). With NO budget the hardest window on the brightest field at C5 aliases at −51.2 dB and a full fold at −64.9 — the ADAA carries it. Budget deleted; only ×1.35 headroom kept for the interpolating kernel.
  - **A single-cycle read is harmonic whatever is in the field.** TENDON's bell ratios cut to one cycle repeat at f0 — multiples of f0 by construction; gloss and manual say so. True inharmonicity needs the read to MOVE (scan/mod/detune) or a second reader at a ratio (parked on BUGLIST).
  - **A normalised list parameter cannot grow without a migration**: 8/8 (CORTEX) reads as 11/11 (TENDON). ` + c("migrateSpecimen()") + String.raw` remaps once, keyed on the build that wrote the file — patch files already carried "build"; the project state did NOT, so it does now and its absence means "older than every build that writes one".
  - **The build moved off the timer**: ` + c("serviceAsync()") + String.raw` builds on a worker and publishes on a later tick (a synchronous ` + c("service()") + String.raw` joins first); ` + c("buildSpecimen") + String.raw` splits over ≤8 threads with an LRU of four — MARROW 21 ms on 20 threads.
  - **The Edit tool refused files it had not READ in this session** (bench.cpp, ui.html) even after a Read issued in the same parallel batch — the read must complete first. Node patch scripts with exact-count anchors did the job (` + c("test/patch-read.js") + String.raw`, ` + c("patch-migrate.js") + String.raw`, ` + c("patch-manual-read.js") + String.raw`, ` + c("patch-landing-read.js") + String.raw`), kept. And a backtick inside a JS template literal ends it — code spans in a patch script are built from ` + c("String.fromCharCode(96)") + String.raw`.
  - Numbers that moved with 128³ and were re-quoted in the manual + landing page: the claim's anchors −65.8 → −61.2 dB, DC −122.9 → −114.0, LUNG forced-level −26.0 → −23.8.
`;
    s = s.replace(anchor, entry.split("\n").join(NL) + anchor);
    fs.writeFileSync(claude, s, "utf8");
  }
}

// ---- BUGLIST: append ------------------------------------------------------
{
  let s = fs.existsSync(buglist) ? fs.readFileSync(buglist, "utf8") : "# Brain Scan — BUGLIST\n";
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const add = String.raw`
## 2026-09-04 · after 260904.3 (the read)

- **Done in .3**: 128³ volume; GRAIN / CONTRAST / FOLD; SUTURE / ENAMEL / TENDON; the specimen dial 9 → 12 slots with a one-time migration keyed on the build id (the project state now carries "build"); the build off the timer.
- **Parked — true inharmonicity.** A single-cycle read is harmonic by construction; TENDON's bell ratios only shape the cycle. A second reader per voice at a non-integer ratio (a "second line at r × f0", mixed by amount) would give real beating partials. Peter's call.
- **Parked — the window as a destination.** GRAIN, CONTRAST and FOLD could be driven by the MOD line like scan/pitch/pan (a third scanner destination each), so the tissue itself decides how hard it is read.
- **Parked — a "texel" view.** With GRAIN at 1 the read passes texels as they are; the tomography slice could show the 128 grid when zoomed, so what is heard as grain is seen as grain.
`;
  s = s.replace(/\s*$/, "") + NL + add.split("\n").join(NL);
  fs.writeFileSync(buglist, s, "utf8");
}

if (misses.length) { console.error("PARTIAL: " + misses.join("; ")); process.exit(1); }
console.log("docs patched: design §12, CLAUDE.md, BUGLIST");
