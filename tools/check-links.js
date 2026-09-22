#!/usr/bin/env node
/*
 * check-links.js — every local link and asset on the site must resolve.
 *
 * The site is a pile of hand-written landing pages that link to each other,
 * to zips and to PDFs. Renames have happened (Photo-Synth 2 -> Photo Synth,
 * The Mars Wars -> Martian Gain) and will happen again, and a download button
 * pointing at a file that is no longer there looks exactly like a working one
 * until someone clicks it.
 *
 * Checks, over every .html in the repo plus manifest.json and the app.json files:
 *   - href/src/poster targets that are local paths actually exist on disk
 *   - every app.json preview exists
 *   - every manifest.json entry has an app.json and a page
 *   - no two apps share a preview image (a duplicate means one of them is
 *     showing another app's panel, which is how the Collection card came to
 *     be a picture of Full Metal Racket)
 *
 *   node tools/check-links.js
 */

const fs = require("fs");
const path = require("path");
const crypto = require("crypto");

const ROOT = path.resolve(__dirname, "..");
const SKIP_DIRS = new Set([".git", "node_modules", "build", "webview2", "_deps", "JUCE"]);

/* Pages serves the site; these pages are served by something else and their
 * links resolve somewhere this checker cannot see:
 *   dsw/web, dsw/plugins   the DSW host serves these from its own root
 *   plugin/Source/ui       embedded in the plugin's WebView, and its siblings
 *                          (bwfx-rack.js) are copied in by CMake at build time
 *   mockup                 a design mockup, not a published page
 */
const NOT_SERVED_BY_PAGES = /^(dsw\/(web|plugins)\/|.*\/plugin\/Source\/|.*\/mockup\/)/;

const problems = [];
const note = (file, msg) => problems.push(`${file}: ${msg}`);

function walk(dir, out = []) {
  for (const e of fs.readdirSync(path.join(ROOT, dir), { withFileTypes: true })) {
    const rel = dir ? `${dir}/${e.name}` : e.name;
    if (e.isDirectory()) {
      if (!SKIP_DIRS.has(e.name)) walk(rel, out);
    } else out.push(rel);
  }
  return out;
}

const files = walk("");
const htmls = files.filter((f) => f.endsWith(".html"));

/* Anything the browser resolves against the server, not the filesystem. */
const EXTERNAL = /^(https?:|data:|mailto:|tel:|javascript:|#|\/\/)/i;

for (const page of htmls) {
  if (NOT_SERVED_BY_PAGES.test(page)) continue;
  const src = fs.readFileSync(path.join(ROOT, page), "utf8");
  const dir = path.dirname(page);

  for (const m of src.matchAll(/(?:href|src|poster)\s*=\s*"([^"]*)"/g)) {
    const raw = m[1].trim();
    if (!raw || EXTERNAL.test(raw)) continue;
    /* A value built by string concatenation inside a <script> is a template,
     * not a link — it has quotes and + signs in it. */
    if (/[+{}]|\$\{/.test(raw)) continue;

    let target = raw.split("#")[0].split("?")[0];
    if (!target) continue;
    try { target = decodeURIComponent(target); } catch { /* keep as written */ }

    /* A root-relative link on project pages resolves under /BrokildApps/, so
     * check it against the repo root. */
    const rel = target.startsWith("/")
      ? target.replace(/^\/BrokildApps\/?/, "").replace(/^\//, "") || "index.html"
      : path.normalize(path.join(dir, target));

    const onDisk = rel.endsWith("/") ? `${rel}index.html` : rel;
    if (!fs.existsSync(path.join(ROOT, onDisk))) note(page, `dead link -> ${raw}`);
  }
}

/* ------------------------------------------------------- the site data -- */
const manifest = JSON.parse(fs.readFileSync(path.join(ROOT, "manifest.json"), "utf8"));
const labels = new Set(manifest.labels.map((l) => l.id));
const previews = new Map();

for (const folder of manifest.apps) {
  const appPath = `${folder}/app.json`;
  if (!fs.existsSync(path.join(ROOT, appPath))) {
    note("manifest.json", `lists ${folder}, which has no app.json`);
    continue;
  }
  const app = JSON.parse(fs.readFileSync(path.join(ROOT, appPath), "utf8"));
  const page = app.url || `${folder}/index.html`;
  if (!fs.existsSync(path.join(ROOT, page))) note(appPath, `url -> ${page} does not exist`);
  for (const t of app.tags || [])
    if (!labels.has(t)) note(appPath, `tag "${t}" is not a label in manifest.json`);
  if (app.preview) {
    if (!fs.existsSync(path.join(ROOT, app.preview))) {
      note(appPath, `preview -> ${app.preview} does not exist`);
    } else {
      const sum = crypto.createHash("md5")
        .update(fs.readFileSync(path.join(ROOT, app.preview))).digest("hex");
      if (previews.has(sum) && previews.get(sum) !== app.preview)
        note(appPath, `preview ${app.preview} is byte-identical to ${previews.get(sum)}`);
      previews.set(sum, app.preview);
    }
  }
}

/* An app with a finished card that the manifest never picked up is invisible
 * on the front page - which is how Hairfryer sat unreachable from the grid. */
for (const f of files.filter((f) => f.endsWith("/app.json"))) {
  const folder = path.dirname(f);
  if (!manifest.apps.includes(folder)) note(f, `has an app.json but manifest.json does not list ${folder}`);
}

if (problems.length) {
  console.error(`${problems.length} problem(s):\n`);
  for (const p of problems) console.error("  " + p);
  process.exit(1);
}
console.log(`links OK — ${htmls.length} pages, ${manifest.apps.length} apps, no dead targets`);
