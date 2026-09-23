#!/usr/bin/env node
/*  migrate-plugin.js — bring one plug-in's source tree into this repository.
 *
 *  Usage:
 *    node tools/migrate-plugin.js <tree> <slug> [--force] [--dry]
 *    node tools/migrate-plugin.js C:/Users/peter/b/BlackRider black-rider
 *
 *  It copies the tree's TRACKED files (git archive HEAD, so nothing ignored and
 *  nothing untracked rides along) into vst3-apps/<slug>/plugin/, drops the
 *  built artefacts, and repoints the CMakeLists at JUCE and BWFX the way the
 *  Clone Wars tree — which has been building in CI from inside this repo for a
 *  month — already does it.
 *
 *  WHY THE SOURCE TREE MUST BE CLEAN: `git archive HEAD` takes the last commit,
 *  not the working copy. A dirty tree migrates silently WITHOUT the edits you
 *  are looking at. It refuses rather than doing that.
 *
 *  WHAT IS LEFT BEHIND, and why each one:
 *    dist/, build/        built output; the published zip lives beside the page
 *    *.wav                bench renders and demo masters; regenerable, and huge
 *    docs/manual/raw|pages|shots   raw captures; the converted plates are kept
 *    docs/**.pdf          the published manual sits in the page folder already
 *
 *  Everything else comes: Source, test, tools, docs, assets and the decal art,
 *  because a panel cannot be rebuilt without the art it references by name.
 *
 *  The per-plug-in git history is NOT grafted in. Each tree's private repo is
 *  archived on GitHub and keeps every commit, so this arrives as one import
 *  commit and the history stays readable where it already is.
 */
"use strict";
const fs = require("fs");
const path = require("path");
const cp = require("child_process");

const ROOT = path.resolve(__dirname, "..");

/* ---------------------------------------------------------------- arguments */
const args = process.argv.slice(2);
const flags = new Set(args.filter((a) => a.startsWith("--")));
const [tree, slug] = args.filter((a) => !a.startsWith("--"));
const DRY = flags.has("--dry");
const FORCE = flags.has("--force");

if (!tree || !slug) {
  console.error("usage: node tools/migrate-plugin.js <tree> <slug> [--force] [--dry]");
  process.exit(2);
}

/* ------------------------------------------------------------- what to drop */
const DROP_DIR = [
  "dist", "build",
  "docs/manual/raw", "docs/manual/pages", "docs/manual/shots",
  "docs/audio", "docs/parts",
  "test/build", "test/wav", "test/aud", "test/renders",
];
const DROP_EXT = [".wav", ".aiff", ".flac"];
const DROP_GLOB = [/^docs\/.*\.pdf$/i, /(^|\/)[^/]*\.vst3$/i, /(^|\/)[^/]*\.exe$/i];

function dropped(rel) {
  const p = rel.replace(/\\/g, "/");
  for (const d of DROP_DIR) if (p === d || p.startsWith(d + "/")) return d + "/";
  for (const e of DROP_EXT) if (p.toLowerCase().endsWith(e)) return "*" + e;
  for (const g of DROP_GLOB) if (g.test(p)) return String(g);
  return null;
}

/* ------------------------------------------------------- the CMake repoint */
/*  The plug-in sits at vst3-apps/<slug>/plugin/, so the repo root is three up
 *  and BWFX is a sibling of vst3-apps. JUCE is not vendored: a local build
 *  passes -DJUCE_DIR, and anything else (a fresh clone, CI) fetches it. */
function repointCMake(text) {
  const notes = [];
  let out = text;

  const JUCE_BLOCK =
`# JUCE is not vendored. A local build passes -DJUCE_DIR=<your checkout>;
# a fresh clone or CI fetches it.
if(DEFINED JUCE_DIR)
    add_subdirectory("\${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.13
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()`;

  /*  Two shapes exist in the fleet and both end in the same add_subdirectory:
   *    if(NOT DEFINED JUCE_DIR) set(...) endif()   — three plug-ins
   *    set(JUCE_DIR "...")                         — the other thirteen
   *  Take the guarded one first, since its body also matches the bare form. */
  const juceGuarded = /if\(NOT DEFINED JUCE_DIR\)\s*\r?\n\s*set\(JUCE_DIR "[^"]*"\)\s*\r?\n\s*endif\(\)\s*\r?\n\s*add_subdirectory\("\$\{JUCE_DIR\}" JUCE\)/;
  const juceBare = /set\(JUCE_DIR "[^"]*"\)\s*\r?\n\s*add_subdirectory\("\$\{JUCE_DIR\}" JUCE\)/;
  if (juceGuarded.test(out)) {
    out = out.replace(juceGuarded, JUCE_BLOCK);
    notes.push("JUCE: guarded absolute path replaced with JUCE_DIR-or-fetch");
  } else if (juceBare.test(out)) {
    out = out.replace(juceBare, JUCE_BLOCK);
    notes.push("JUCE: bare absolute path replaced with JUCE_DIR-or-fetch");
  } else if (/FetchContent_Declare\(JUCE/.test(out)) {
    notes.push("JUCE: already fetch-based, left alone");
  } else {
    notes.push("JUCE: NO KNOWN PATTERN — check by hand");
  }

  const bwfxAbs = /set\(BWFX_DIR "C:\/Users\/peter\/b\/BrokildWorldFX"\)/;
  if (bwfxAbs.test(out)) {
    out = out.replace(bwfxAbs,
      `set(BWFX_DIR "\${CMAKE_CURRENT_SOURCE_DIR}/../../../BrokildWorldFX")`);
    notes.push("BWFX: absolute path replaced with a path relative to this file");
  } else if (/BWFX_DIR/.test(out)) {
    notes.push(/CMAKE_CURRENT_SOURCE_DIR\}\/\.\.\/\.\.\/\.\.\/BrokildWorldFX/.test(out)
      ? "BWFX: already relative, left alone"
      : "BWFX: mentioned but not the expected absolute path — check by hand");
  } else {
    notes.push("BWFX: not used by this plug-in");
  }
  return { out, notes };
}

/* --------------------------------------------------------------------- run */
function sh(cmd, opts) { return cp.execSync(cmd, { encoding: "utf8", maxBuffer: 1 << 28, ...opts }); }

if (!fs.existsSync(path.join(tree, ".git"))) {
  console.error("not a git tree: " + tree);
  process.exit(1);
}

const dirty = sh(`git -C "${tree}" status --porcelain`).split("\n")
  .filter((l) => l.trim() && !l.startsWith("??"));
if (dirty.length && !FORCE) {
  console.error("REFUSING: " + tree + " has uncommitted changes, and this migrates HEAD.");
  dirty.slice(0, 8).forEach((l) => console.error("   " + l));
  console.error("Commit them, or pass --force to migrate HEAD anyway and lose them here.");
  process.exit(1);
}

const dest = path.join(ROOT, "vst3-apps", slug, "plugin");
if (fs.existsSync(dest) && !FORCE) {
  console.error("REFUSING: " + path.relative(ROOT, dest) + " already exists. Pass --force to replace it.");
  process.exit(1);
}

const head = sh(`git -C "${tree}" rev-parse --short HEAD`).trim();
const files = sh(`git -C "${tree}" ls-tree -r --name-only HEAD`).split("\n").filter(Boolean);

const keep = [], skip = new Map();
for (const f of files) {
  const why = dropped(f);
  if (why) skip.set(why, (skip.get(why) || 0) + 1);
  else keep.push(f);
}

console.log(`\n${slug}  <-  ${tree}  @ ${head}`);
console.log(`  tracked ${files.length} files, keeping ${keep.length}`);
for (const [why, n] of [...skip].sort((a, b) => b[1] - a[1]))
  console.log(`    dropped ${String(n).padStart(4)}  ${why}`);

if (DRY) { console.log("  (dry run, nothing written)\n"); process.exit(0); }

/* Copy through a staging folder so a failure cannot leave a half tree. */
const stage = path.join(require("os").tmpdir(), "mig-" + slug + "-" + Date.now());
fs.mkdirSync(stage, { recursive: true });
let bytes = 0;
for (const f of keep) {
  const src = path.join(tree, f);
  if (!fs.existsSync(src)) { console.log("    MISSING in working copy, skipped: " + f); continue; }
  const dst = path.join(stage, f);
  fs.mkdirSync(path.dirname(dst), { recursive: true });
  fs.copyFileSync(src, dst);
  bytes += fs.statSync(dst).size;
}

/* Repoint the build file. */
const cml = path.join(stage, "CMakeLists.txt");
if (fs.existsSync(cml)) {
  const raw = fs.readFileSync(cml, "utf8");
  const { out, notes } = repointCMake(raw);
  fs.writeFileSync(cml, out);
  notes.forEach((n) => console.log("    " + n));
} else {
  console.log("    NO CMakeLists.txt at the tree root — check by hand");
}

if (fs.existsSync(dest)) fs.rmSync(dest, { recursive: true, force: true });
fs.mkdirSync(path.dirname(dest), { recursive: true });
fs.renameSync(stage, dest);

console.log(`  -> vst3-apps/${slug}/plugin/   ${(bytes / 1048576).toFixed(1)} MB\n`);
