#!/usr/bin/env node
/*  repoint-paths.js — make a migrated plug-in's own tooling find itself.
 *
 *    node tools/repoint-paths.js [--dry] [slug ...]
 *
 *  Every tree carried absolute paths into C:\Users\peter\b\<Tree>, which after
 *  the move point at the copy left behind rather than at the tree you are
 *  editing. A script that quietly drives the OLD tree is worse than one that
 *  fails, so this rewrites them.
 *
 *  The rewrite is by DERIVATION, not by a new absolute path:
 *
 *    .ps1   $PSScriptRoot, with as many "\.." as the script is deep inside the
 *           tree. NOT a variable declared at the top: PowerShell binds a
 *           param() block's DEFAULTS before the body runs, so a $BrokildRoot
 *           assigned in the body is empty exactly where these scripts use it
 *           most, and "$BrokildRoot\test\jobs.json" binds as "\test\jobs.json".
 *           $PSScriptRoot is available during binding, so it is the only form
 *           that works in both places.
 *    .js    __dirname, the same way.
 *    .json  job files for the CDP drivers are DATA, not code, and the drivers
 *           take whatever path they are given. Those keep an absolute path,
 *           rewritten to where the tree now is, because a jobs file only ever
 *           describes a session on this machine.
 *
 *  Anything it cannot place is reported rather than guessed at. One-off
 *  patch-*.js scripts are skipped on purpose: they ran once, against the tree
 *  as it was, and their paths are part of that record.
 */
"use strict";
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const args = process.argv.slice(2);
const DRY = args.includes("--dry");
const only = args.filter((a) => !a.startsWith("--"));

/*  Which old tree name belongs to which folder in the repo now. The tree name
 *  is what appears inside the old absolute paths, and it is often NOT the slug
 *  (MarsWars is martian-gain, Nineteen84 is 1984, ArtefactB2311 is b2311-22). */
const MAP = {
  BlackRider: "vst3-apps/black-rider/plugin",
  BladeRuiner: "vst3-apps/blade-ruiner/plugin",
  EscapeRoom: "vst3-apps/escape-room/plugin",
  FullMetalRacket: "vst3-apps/full-metal-racket/plugin",
  Hairfryer: "vst3-apps/hairfryer/plugin",
  MarsWars: "vst3-apps/martian-gain/plugin",
  PhotoSynth: "vst3-apps/photo-synth/plugin",
  HighTide: "vst3-apps/high-tide/plugin",
  BrainScan: "vst3-apps/brain-scan/plugin",
  BattlestarOverdrive: "vst3-apps/battlestar-overdrive/plugin",
  ThinWalls: "vst3-apps/thin-walls/plugin",
  Nineteen84: "vst3-apps/1984/plugin",
  ArtefactB2311_1: "vst3-apps/proxima-centauri-b/b2311-1",
  ArtefactB2311_67: "vst3-apps/proxima-centauri-b/b2311-67",
  ArtefactB2311_104: "vst3-apps/proxima-centauri-b/b2311-104",
  ArtefactB2311: "vst3-apps/proxima-centauri-b/b2311-22",   // LAST: a prefix of the three above
};
/*  BrokildWorldFX is deliberately NOT here. The copy in this repo is still a
 *  mirror of b\BrokildWorldFX at the time of writing, so rewriting it would be
 *  undone by the next mirror run. It gets repointed when the mirror is retired
 *  and the repo copy becomes canonical, which is its own step. */
/*  ArtefactB2311 is a prefix of ArtefactB2311_1, _67 and _104, so the longest
 *  name has to be tried first or every one of them matches the short entry. */
const NAMES = Object.keys(MAP).sort((a, b) => b.length - a.length);

function walk(dir, hits) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    if (e.name === ".git" || e.name === "build" || e.name === "node_modules") continue;
    const p = path.join(dir, e.name);
    if (e.isDirectory()) walk(p, hits);
    else if (/\.(ps1|js|json|txt|md|cmd|sh)$/i.test(e.name) && !/(^|[\\/])patch-/.test(e.name)) hits.push(p);
  }
  return hits;
}

let changed = 0, skipped = 0, unplaced = [];
const targets = Object.values(MAP).filter((d) => !only.length || only.some((o) => d.includes(o)));

for (const rel of targets) {
  const base = path.join(ROOT, rel);
  if (!fs.existsSync(base)) continue;
  for (const file of walk(base, [])) {
    const raw = fs.readFileSync(file, "utf8");
    if (!/[Uu]sers[\\/]peter[\\/]b[\\/]/.test(raw)) continue;
    const ext = path.extname(file).toLowerCase();
    let out = raw, used = false;

    for (const name of NAMES) {
      const dest = MAP[name];
      /*  Both separators and both quotings appear: a .ps1 writes backslashes,
       *  a .js forward slashes, a .json escaped backslashes. */
      const variants = [
        ["C:\\\\Users\\\\peter\\\\b\\\\" + name, "json"],
        ["C:\\Users\\peter\\b\\" + name, "win"],
        ["C:/Users/peter/b/" + name, "posix"],
      ];
      for (const [prefix, kind] of variants) {
        if (!out.includes(prefix)) continue;
        used = true;
        let repl;
        if (ext === ".ps1" && kind !== "json") {
          /*  How deep this script sits inside the tree decides how far back up
           *  the root is: tools/ and test/ are one, docs/manual/ is two. */
          const depth = path.relative(base, path.dirname(file)).split(path.sep).filter(Boolean).length;
          repl = "$PSScriptRoot" + "\\..".repeat(depth);
        } else if (ext === ".js" && kind === "posix") {
          repl = '" + BROKILD_ROOT + "';
        } else {
          /* data, or a shape we do not rewrite by derivation: point it at the
             new home, keeping the separator style it already used */
          const abs = path.join(ROOT, dest);
          repl = kind === "json" ? abs.replace(/\\/g, "\\\\")
               : kind === "win" ? abs
               : abs.replace(/\\/g, "/");
        }
        out = out.split(prefix).join(repl);
      }
    }

    if (!used) { unplaced.push(path.relative(ROOT, file)); continue; }

    if (ext === ".js" && out.includes("BROKILD_ROOT") && !/const BROKILD_ROOT/.test(out)) {
      /*  After any shebang and any "use strict": a directive only counts as
       *  one when it is the first statement, so prepending above it would
       *  silently take the file out of strict mode. */
      const decl = `const BROKILD_ROOT = require("path").resolve(__dirname, "..").replace(/\\\\/g, "/");\n`;
      const lead = out.match(/^(#![^\n]*\n)?(\s*["']use strict["'];\s*\n)?/);
      const at = lead ? lead[0].length : 0;
      out = out.slice(0, at) + decl + out.slice(at);
    }

    if (DRY) { console.log("  would fix  " + path.relative(ROOT, file)); changed++; continue; }

    /*  A bulk rewrite of source has to prove it did not break the source. A
     *  .js gets parsed; if it no longer parses the file is put back exactly as
     *  it was and reported, because a script that fails to load is worse than
     *  one pointing at an old tree. */
    fs.writeFileSync(file, out);
    if (ext === ".js") {
      try {
        require("child_process").execSync(`node --check "${file}"`, { stdio: "pipe" });
      } catch (e) {
        fs.writeFileSync(file, raw);
        unplaced.push(path.relative(ROOT, file) + "  (rewrite would not parse, reverted)");
        continue;
      }
    }
    changed++;
  }
}

console.log(`\n${changed} file(s) ${DRY ? "would be " : ""}repointed`);
if (unplaced.length) {
  console.log(`${unplaced.length} file(s) mention b\\ but no known tree name — check by hand:`);
  unplaced.forEach((u) => console.log("   " + u));
}
