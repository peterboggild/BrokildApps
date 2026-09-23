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
const destFlag = args.find((a) => a.startsWith("--dest="));
const DRY = flags.has("--dry");
const FORCE = flags.has("--force");

if (!tree || !slug) {
  console.error("usage: node tools/migrate-plugin.js <tree> <slug> [--force] [--dry]");
  process.exit(2);
}

/* ------------------------------------------------------------- what to drop */
/*  NOT docs/manual/shots: in five trees that IS the plate folder, holding the
 *  JPEGs the manual references, and dropping it cost Photo Synth, Blade
 *  Ruiner, Escape Room, Full Metal Racket and Martian Gain the ability to
 *  rebuild their own manuals. The line is CONTENT, not folder name: a
 *  converted plate is a JPEG and is kept wherever it lives; a raw capture is a
 *  PNG in raw/, pages/ or shots/ and is dropped. */
const DROP_DIR = [
  "dist", "build",
  "docs/manual/raw", "docs/manual/pages",
  "docs/audio", "docs/parts",
  "test/build", "test/wav", "test/aud", "test/renders",
];
const DROP_EXT = [".wav", ".aiff", ".flac"];
const DROP_GLOB = [
  /^docs\/.*\.pdf$/i,              // the published manual sits beside the page
  /^docs\/manual\/shots\/.*\.png$/i,  // raw captures in a plate folder; the
                                      // JPEGs beside them are the plates
  /^docs\/[^/]*\.png$/i,           // loose captures at the docs root: session
                                   // diagnostics, never referenced by any
                                   // document — checked across the fleet, and
                                   // every one scored zero references. Plates
                                   // live in docs/manual/img/ and are kept.
  /(^|\/)[^/]*\.vst3$/i,
  /(^|\/)[^/]*\.exe$/i,
];

function dropped(rel) {
  const p = rel.replace(/\\/g, "/");
  for (const d of DROP_DIR) if (p === d || p.startsWith(d + "/")) return d + "/";
  for (const e of DROP_EXT) if (p.toLowerCase().endsWith(e)) return "*" + e;
  for (const g of DROP_GLOB) if (g.test(p)) return String(g);
  return null;
}

/* ------------------------------------------------------- the CMake repoint */
/*  BWFX is at the repository root, so the hop out is computed from where this
 *  plug-in actually lands rather than assumed: most sit at
 *  vst3-apps/<slug>/plugin/ and need three, the Artefacts sit one level
 *  shallower inside their shared survey folder and need two. Assuming a depth
 *  is how a tree builds on one machine and not in CI.
 *  JUCE is not vendored: a local build passes -DJUCE_DIR, and anything else,
 *  a fresh clone or CI, fetches it. */
function repointCMake(text, upToRoot) {
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
      `set(BWFX_DIR "\${CMAKE_CURRENT_SOURCE_DIR}/${upToRoot}/BrokildWorldFX")`);
    notes.push("BWFX: absolute path replaced with a path relative to this file");
  } else if (/BWFX_DIR/.test(out)) {
    notes.push(out.includes("CMAKE_CURRENT_SOURCE_DIR}/" + upToRoot + "/BrokildWorldFX")
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

/*  Default is vst3-apps/<slug>/plugin. --dest=<path relative to the repo root>
 *  places it anywhere, which is what the four Artefacts need: they live inside
 *  their shared survey folder rather than each having a page of its own. */
const destRel = destFlag ? destFlag.slice(7).replace(/\\/g, "/")
                         : "vst3-apps/" + slug + "/plugin";
const dest = path.join(ROOT, destRel);
const upToRoot = destRel.split("/").filter(Boolean).map(() => "..").join("/");
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

console.log(`\n${destRel}  <-  ${tree}  @ ${head}`);
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
  const { out, notes } = repointCMake(raw, upToRoot);
  fs.writeFileSync(cml, out);
  notes.forEach((n) => console.log("    " + n));
} else {
  console.log("    NO CMakeLists.txt at the tree root — check by hand");
}

/*  THE DROPBOX EPERM TRAP. This repository lives inside Dropbox, and
 *  fs.rmSync(dir, {recursive:true}) there deletes the CONTENTS and then throws
 *  EPERM on the folder itself, leaving an empty directory and a half-run
 *  script. Walk the children instead, and treat a folder that will not go as
 *  fine, because the contents are what matter. Same reason the final placement
 *  moves child by child rather than renaming the staging folder over the top. */
function rmTree(dir) {
  if (!fs.existsSync(dir)) return;
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) rmTree(p);
    else fs.rmSync(p, { force: true, maxRetries: 5, retryDelay: 120 });
  }
  try { fs.rmdirSync(dir); } catch (e) { /* held by Dropbox; it is empty now */ }
}
function moveInto(from, to) {
  fs.mkdirSync(to, { recursive: true });
  for (const e of fs.readdirSync(from, { withFileTypes: true })) {
    const a = path.join(from, e.name), b = path.join(to, e.name);
    if (e.isDirectory()) moveInto(a, b);
    else { fs.copyFileSync(a, b); fs.rmSync(a, { force: true, maxRetries: 5, retryDelay: 120 }); }
  }
  try { fs.rmdirSync(from); } catch (e) { /* the temp folder can linger */ }
}

rmTree(dest);
moveInto(stage, dest);

console.log(`  -> ${destRel}/   ${(bytes / 1048576).toFixed(1)} MB\n`);
