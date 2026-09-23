#!/usr/bin/env node
/*  wire-downloads.js — point the pages and the cards at the release assets.
 *
 *    node tools/wire-downloads.js [--dry]
 *
 *  Reads tools/.downloads-report.json, written by publish-downloads.js, and
 *  for each zip:
 *    - adds a `downloads` entry to that plug-in's app.json carrying the url,
 *      the byte count and the sha256, the same shape contents.json already
 *      uses for the collection;
 *    - rewrites the landing page's href from the relative filename to the
 *      release url.
 *
 *  The size and hash are DECLARED rather than inferred, so a checker can prove
 *  the file on the other end is the file that was meant, and so a reader is
 *  told what they are about to download before they click.
 */
"use strict";
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const DRY = process.argv.includes("--dry");
const report = JSON.parse(fs.readFileSync(path.join(ROOT, "tools", ".downloads-report.json"), "utf8"))
  .filter((r) => r.ok);

if (!report.length) { console.error("nothing uploaded in the report — run publish-downloads.js first"); process.exit(1); }

/* group by the folder the zip sat in: a folder can ship more than one */
const byDir = new Map();
for (const r of report) {
  const dir = path.dirname(r.rel_path);
  if (!byDir.has(dir)) byDir.set(dir, []);
  byDir.get(dir).push(r);
}

let pages = 0, cards = 0, missed = [];
for (const [dir, list] of byDir) {
  /* ---- app.json ------------------------------------------------------- */
  const appPath = path.join(ROOT, dir, "app.json");
  if (fs.existsSync(appPath)) {
    const raw = fs.readFileSync(appPath, "utf8");
    const nl = raw.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
    const j = JSON.parse(raw);
    j.downloads = list.map((r) => ({ name: r.name, url: r.url, bytes: r.bytes, sha256: r.hash }));
    if (!DRY) fs.writeFileSync(appPath, JSON.stringify(j, null, 2).replace(/\n/g, nl) + nl);
    cards++;
  } else missed.push(dir + "/app.json (none)");

  /* ---- index.html ----------------------------------------------------- */
  const pagePath = path.join(ROOT, dir, "index.html");
  if (fs.existsSync(pagePath)) {
    let html = fs.readFileSync(pagePath, "utf8");
    let hits = 0;
    for (const r of list) {
      const from = `href="${r.name}"`;
      if (!html.includes(from)) { missed.push(`${dir}/index.html: no href="${r.name}"`); continue; }
      html = html.split(from).join(`href="${r.url}"`);
      hits++;
    }
    if (hits && !DRY) fs.writeFileSync(pagePath, html);
    if (hits) pages++;
  } else missed.push(dir + "/index.html (none)");

  console.log(`  ${dir.padEnd(34)} ${list.length} download(s)`);
}

console.log(`\n${cards} app.json and ${pages} page(s) ${DRY ? "would be " : ""}wired`);
if (missed.length) { console.log("not placed, check by hand:"); missed.forEach((m) => console.log("   " + m)); }
