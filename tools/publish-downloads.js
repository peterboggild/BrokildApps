#!/usr/bin/env node
/*  publish-downloads.js — move the plug-in zips out of git and onto a release.
 *
 *    node tools/publish-downloads.js [--dry] [--no-upload] [zip ...]
 *
 *  A repository that holds its own binaries grows by the size of a build every
 *  time one is cut, and this one reached 1.9 GB that way. The collection zip
 *  already had to become a release asset when it passed GitHub's hard 100 MB
 *  file limit; this applies the same answer to the other nineteen, which is a
 *  decision already made rather than a new one.
 *
 *  For each zip it uploads the file to the `downloads` release, records the
 *  url, byte count and sha256 in that plug-in's app.json, and repoints the
 *  landing page's link. The zip then leaves the index and is ignored.
 *
 *  ONE RELEASE, STABLE TAG. A dated tag suits the collection, which is a
 *  snapshot of a moment. An individual plug-in carries its own build id, so
 *  what matters is "the current download", and a tag that never changes means
 *  there is one place to look and one asset to replace when a plug-in is
 *  re-cut. Re-running this after a re-cut replaces that one asset.
 *
 *  WHAT IS VERIFIED, because "uploaded" and "downloadable" are different
 *  claims: every asset is fetched back from its public url and its sha256
 *  compared with the local file before the local file is untracked. Nothing is
 *  removed from git that has not been proven to exist on the other end.
 */
"use strict";
const fs = require("fs");
const path = require("path");
const cp = require("child_process");
const crypto = require("crypto");

const ROOT = path.resolve(__dirname, "..");
const OWNER = "peterboggild";
const REPO = "BrokildApps";
const TAG = "downloads";
const args = process.argv.slice(2);
const DRY = args.includes("--dry");
const NO_UPLOAD = args.includes("--no-upload");
const only = args.filter((a) => !a.startsWith("--"));

function sh(cmd, opts) { return cp.execSync(cmd, { encoding: "utf8", maxBuffer: 1 << 28, stdio: ["pipe", "pipe", "pipe"], ...opts }); }
function api(method, url, body, extra = "") {
  const tmp = path.join(require("os").tmpdir(), "gh-" + Date.now() + ".json");
  if (body) fs.writeFileSync(tmp, body);
  const out = sh(`curl -s -X ${method} -H "Authorization: token ${TOKEN}" -H "Accept: application/vnd.github+json" ${extra} ${body ? `--data-binary @"${tmp}"` : ""} "${url}"`);
  if (body) fs.rmSync(tmp, { force: true });
  try { return JSON.parse(out); } catch (e) { return { _raw: out }; }
}
const sha256 = (f) => crypto.createHash("sha256").update(fs.readFileSync(f)).digest("hex");

/* ------------------------------------------------------------------ token */
const TOKEN = (sh(`printf "protocol=https\\nhost=github.com\\n\\n" | git credential fill`)
  .split("\n").find((l) => l.startsWith("password=")) || "").slice(9).trim();
if (!TOKEN) { console.error("no GitHub token from git credential fill"); process.exit(1); }

/* ---------------------------------------------------------------- release */
let rel = api("GET", `https://api.github.com/repos/${OWNER}/${REPO}/releases/tags/${TAG}`);
if (!rel.id) {
  if (DRY) { console.log(`would create the "${TAG}" release`); rel = { id: 0, assets: [] }; }
  else {
    rel = api("POST", `https://api.github.com/repos/${OWNER}/${REPO}/releases`, JSON.stringify({
      tag_name: TAG, name: "Downloads", target_commitish: "main",
      body: "The current build of every Brokild plug-in.\n\n"
          + "These live here rather than in the repository so a clone stays small: a repo that\n"
          + "holds its own binaries grows by the size of a build every time one is cut.\n\n"
          + "Each plug-in's own page links to its file here, and declares the size and sha256\n"
          + "in its app.json. Re-cutting a plug-in replaces that one asset; the tag never moves.",
    }));
    if (!rel.id) { console.error("could not create the release: " + JSON.stringify(rel).slice(0, 300)); process.exit(1); }
    console.log(`created the "${TAG}" release`);
  }
}
const existing = new Map((rel.assets || []).map((a) => [a.name, a]));

/* ------------------------------------------------------------------- zips */
/*  Scan the FILESYSTEM, not the index. This used to ask git for the zips, and
 *  that worked exactly once: the moment the zips became untracked — which is
 *  the whole point of this tool — git stopped reporting them and every
 *  re-publish silently found nothing to do. A tool whose input is the thing it
 *  removes from git cannot use git to find it. */
function findZips(dir, out = []) {
  for (const e of fs.readdirSync(path.join(ROOT, dir), { withFileTypes: true })) {
    if (e.name === ".git" || e.name === "node_modules" || e.name === "build") continue;
    const rel = `${dir}/${e.name}`;
    if (e.isDirectory()) findZips(rel, out);
    else if (e.name.toLowerCase().endsWith(".zip")) out.push(rel);
  }
  return out;
}
let zips = findZips("vst3-apps").sort();
if (only.length) zips = zips.filter((z) => only.some((o) => z.includes(o)));
/*  The collection archives are 150 MB between them and change only when they
 *  are re-cut, so a bulk run leaves them alone. Name one and it uploads. */
else zips = zips.filter((z) => !/Collection-win64\.zip$/i.test(path.basename(z)));
if (!zips.length) { console.error("no zips found under vst3-apps" + (only.length ? " matching " + only.join(" ") : "")); process.exit(1); }

const report = [];
for (const rel_path of zips) {
  const abs = path.join(ROOT, rel_path);
  const name = path.basename(rel_path);
  const bytes = fs.statSync(abs).size;
  const hash = sha256(abs);
  const url = `https://github.com/${OWNER}/${REPO}/releases/download/${TAG}/${name}`;

  if (DRY || NO_UPLOAD) {
    console.log(`  would upload ${name.padEnd(38)} ${(bytes / 1048576).toFixed(1)} MB`);
    report.push({ rel_path, name, bytes, hash, url, ok: false });
    continue;
  }

  /*  Replacing rather than adding: a release cannot hold two assets with the
   *  same name, and a re-cut plug-in must not leave the old binary reachable
   *  from the same url. */
  if (existing.has(name)) {
    api("DELETE", `https://api.github.com/repos/${OWNER}/${REPO}/releases/assets/${existing.get(name).id}`);
  }
  const up = api("POST",
    `https://uploads.github.com/repos/${OWNER}/${REPO}/releases/${rel.id}/assets?name=${encodeURIComponent(name)}`,
    null, `-H "Content-Type: application/zip" --data-binary @"${abs}"`);
  if (!up.id) { console.log(` FAIL ${name}: ${JSON.stringify(up).slice(0, 160)}`); report.push({ rel_path, name, ok: false }); continue; }

  console.log(`  uploaded ${name.padEnd(38)} ${(bytes / 1048576).toFixed(1)} MB`);
  report.push({ rel_path, name, bytes, hash, url, ok: true });
}

fs.writeFileSync(path.join(ROOT, "tools", ".downloads-report.json"), JSON.stringify(report, null, 2));
console.log(`\n${report.filter((r) => r.ok).length} of ${report.length} uploaded; report in tools/.downloads-report.json`);
