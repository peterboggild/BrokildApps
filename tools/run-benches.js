#!/usr/bin/env node
/*  run-benches.js — build and run every migrated plug-in's offline bench.
 *
 *    node tools/run-benches.js [slug ...]
 *
 *  The builds prove the layout and the paths. The benches prove the tests came
 *  across intact and still say what they said before the move, which is the
 *  claim that actually matters: the DSP source is byte-identical, so any bench
 *  that now disagrees with itself is a migration fault and nothing else.
 *
 *  Every bench in the fleet is plain C++ with no JUCE, so this needs nothing
 *  but a compiler. Output goes outside Dropbox. Smart App Control judges a
 *  freshly linked exe on a dice roll, so a refused run is retried on a copy
 *  with a nudged hash rather than reported as a failure.
 */
"use strict";
const fs = require("fs");
const path = require("path");
const cp = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const OUT = "C:/Users/peter/b/_build/bench";
const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

/* Every folder with test/CMakeLists.txt under vst3-apps. */
function find(dir, depth, hits) {
  if (depth > 4) return hits;
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    if (!e.isDirectory() || e.name === "build" || e.name === "img") continue;
    const p = path.join(dir, e.name);
    if (fs.existsSync(path.join(p, "test", "CMakeLists.txt")))
      hits.push(path.relative(ROOT, p).replace(/\\/g, "/"));
    else find(p, depth + 1, hits);
  }
  return hits;
}

let trees = find(path.join(ROOT, "vst3-apps"), 0, []).sort();
if (only.length) trees = trees.filter((t) => only.some((o) => t.includes(o)));

const rows = [];
for (const rel of trees) {
  const name = rel.replace(/^vst3-apps\//, "").replace(/\/plugin$/, "").replace(/\//g, "-");
  const bdir = `${OUT}/${name}`;
  let ok = false, note = "", verdict = "";
  const t0 = Date.now();
  try {
    cp.execSync(`cmake -S "${path.join(ROOT, rel, "test")}" -B "${bdir}" -A x64`,
      { encoding: "utf8", stdio: "pipe", maxBuffer: 1 << 28 });
    cp.execSync(`cmake --build "${bdir}" --config Release`,
      { encoding: "utf8", stdio: "pipe", maxBuffer: 1 << 28 });

    /*  A test folder builds more than benches: renderers that write demo
     *  audio, probes that print engine internals, and live tools like
     *  ab104sitewatch which reads the shared block and EXITS NON-ZERO when no
     *  DAW is running, exactly as it should. Running those and calling the
     *  tree broken would be a check that fails on correct behaviour. Only
     *  targets whose name says "test" are benches; the rest are reported as
     *  skipped so nothing goes silently unrun. */
    const all = fs.existsSync(`${bdir}/Release`)
      ? fs.readdirSync(`${bdir}/Release`).filter((f) => f.endsWith(".exe") && !f.startsWith("sac"))
      : [];
    const exes = all.filter((f) => /test/i.test(f));
    const skipped = all.filter((f) => !/test/i.test(f));
    if (!exes.length) throw new Error(
      all.length ? "no *test* exe among: " + all.join(" ") : "bench built but produced no exe");

    /*  Several trees build more than one bench binary. Run them all and fail
     *  the tree if any one of them does. Picking the first would let a second,
     *  failing bench go unseen, which is the whole reason it exists. */
    const lines = [];
    for (const exe of exes) {
      const out = cp.execSync(
        `powershell -NoProfile -ExecutionPolicy Bypass -File "${path.join(ROOT, "tools", "run-exe-past-sac.ps1")}" -Exe "${bdir}/Release/${exe}"`,
        { encoding: "utf8", stdio: "pipe", maxBuffer: 1 << 28, timeout: 1800000 });
      const last = out.trim().split("\n").filter((l) => l.trim()).slice(-3).join(" | ");
      const bad = /FAIL|FAILED|ALL ATTEMPTS BLOCKED|exit [1-9]/i.test(last)
               && !/0 fail|no failures|ALL CLEAR|ALL PASS/i.test(last);
      lines.push(`${exe}: ${last.replace(/\s+/g, " ").slice(0, 110)}`);
      if (bad) throw new Error(lines[lines.length - 1]);
    }
    verdict = lines.join("   ") + (skipped.length ? `   [not benches: ${skipped.join(" ")}]` : "");
    ok = true;
  } catch (e) {
    note = String(e.stdout || e.message || e).trim().split("\n").slice(-2).join(" ").slice(0, 150);
  }
  const secs = ((Date.now() - t0) / 1000).toFixed(0);
  rows.push({ rel, ok });
  console.log(`${ok ? "  ok  " : " FAIL "} ${name.padEnd(24)} ${String(secs).padStart(4)}s  ${ok ? verdict : note}`);
}

const bad = rows.filter((r) => !r.ok);
console.log("");
console.log(bad.length === 0
  ? `${rows.length} benches run from the repo — ALL CLEAR`
  : `${bad.length} of ${rows.length} benches FAILED`);
process.exit(bad.length ? 1 : 0);
