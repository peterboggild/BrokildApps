#!/usr/bin/env node
/*  verify-migrated.js — prove a migrated plug-in still builds from the repo.
 *
 *    node tools/verify-migrated.js [--configure-only] [slug ...]
 *
 *  For every plug-in folder that has a CMakeLists.txt under vst3-apps, this
 *  configures it, builds it, and reports. Build output goes OUTSIDE Dropbox,
 *  per the house rule, because a build directory inside a synced folder is a
 *  fight nobody wins.
 *
 *  Configure alone is the cheap gate: it is what catches a BWFX or JUCE path
 *  that did not survive the move, and it takes seconds rather than minutes.
 *  The full build is what catches everything else.
 */
"use strict";
const fs = require("fs");
const path = require("path");
const cp = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const OUT = "C:/Users/peter/b/_build/migrate";
const JUCE = "C:/Users/peter/AudioDev/Projects/BrokildVSTTemplate/external/JUCE";

const args = process.argv.slice(2);
const CONF_ONLY = args.includes("--configure-only");
const only = args.filter((a) => !a.startsWith("--"));

/* Find every migrated plug-in: a CMakeLists.txt with juce_add_plugin in it. */
function find(dir, depth, hits) {
  if (depth > 4) return hits;
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    if (!e.isDirectory() || e.name === "build" || e.name === "img") continue;
    const p = path.join(dir, e.name);
    const cml = path.join(p, "CMakeLists.txt");
    if (fs.existsSync(cml) && /juce_add_plugin/.test(fs.readFileSync(cml, "utf8")))
      hits.push(path.relative(ROOT, p).replace(/\\/g, "/"));
    else find(p, depth + 1, hits);
  }
  return hits;
}

let plugins = find(path.join(ROOT, "vst3-apps"), 0, []).sort();
if (only.length) plugins = plugins.filter((p) => only.some((o) => p.includes(o)));

const rows = [];
for (const rel of plugins) {
  const name = rel.replace(/^vst3-apps\//, "").replace(/\/plugin$/, "").replace(/\//g, "-");
  const bdir = `${OUT}/${name}`;
  let stage = "configure", ok = false, note = "";
  const t0 = Date.now();
  try {
    /*  Smart App Control blocks by file HASH and judges the FRESHLY BUILT
     *  juceaide.exe on a dice roll, so "Testing juceaide failed" is a transient
     *  verdict rather than a broken tree. The documented cure is to try again;
     *  it cleared on the second attempt every time it has been seen here. A
     *  gate that reported this as a failure would cry wolf on a good tree. */
    let tries = 0;
    for (;;) {
      try {
        cp.execSync(`cmake -S "${path.join(ROOT, rel)}" -B "${bdir}" -A x64 -DJUCE_DIR=${JUCE}`,
          { encoding: "utf8", stdio: "pipe", maxBuffer: 1 << 28 });
        break;
      } catch (e) {
        const m = String((e.stdout || "") + (e.stderr || ""));
        if (++tries >= 4 || !/juceaide/i.test(m)) throw e;
        note = `juceaide blocked, retried x${tries}`;
        for (const j of ["juceaide.exe"])
          try {
            cp.execSync(`powershell -NoProfile -Command "Get-ChildItem '${bdir}' -Recurse -Filter ${j} -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue"`,
              { stdio: "pipe" });
          } catch (x) { /* nothing to delete is fine */ }
      }
    }
    ok = true;
    if (!CONF_ONLY) {
      stage = "build";
      ok = false;
      const out = cp.execSync(`cmake --build "${bdir}" --config Release`,
        { encoding: "utf8", stdio: "pipe", maxBuffer: 1 << 28 });
      const built = (out.match(/-> .*\.vst3\s*$/gm) || []).length;
      note = built ? `${built} artefact(s)` : "built, no vst3 in the log";
      ok = true;
    }
  } catch (e) {
    const msg = String((e.stdout || "") + (e.stderr || e.message || ""));
    const line = (msg.match(/^.*(?:CMake Error|error [A-Z]+\d+|fatal error).*$/m) || [""])[0];
    note = line.trim().slice(0, 130) || msg.trim().split("\n").pop().slice(0, 130);
  }
  const secs = ((Date.now() - t0) / 1000).toFixed(0);
  rows.push({ rel, ok, stage, note, secs });
  console.log(`${ok ? "  ok  " : " FAIL "} ${rel.padEnd(46)} ${String(secs).padStart(4)}s  ${note}`);
}

const bad = rows.filter((r) => !r.ok);
console.log("");
console.log(bad.length === 0
  ? `${rows.length} plug-ins ${CONF_ONLY ? "CONFIGURE" : "BUILD"} from the repo — ALL CLEAR`
  : `${bad.length} of ${rows.length} FAILED at ${bad.map((b) => b.stage).join(", ")}`);
process.exit(bad.length ? 1 : 0);
