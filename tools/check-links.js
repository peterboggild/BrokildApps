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
/*  Widened when the plug-in sources moved in. It used to say
 *  ".*\/plugin\/Source\/", which misses the four Artefacts: they sit at
 *  vst3-apps/proxima-centauri-b/b2311-*\/Source/ with no "plugin" segment, so
 *  their ui.html leaked in and was reported for a bwfx-rack.js that CMake
 *  supplies at build time. Any Source folder is plug-in source, never a page.
 *  "reference/" is a preserved copy of an app a plug-in was ported FROM. */
const NOT_SERVED_BY_PAGES =
  /^(dsw\/(web|plugins)\/|.*\/Source\/|.*\/mockup\/|.*\/reference\/|.*\/plugin\/docs\/landing\.html$)/;
/*  plugin/docs/landing.html is the SOURCE a landing page was built from, and
 *  its images resolve at the published location rather than beside it. The
 *  published page is the one that has to be whole, and it is checked. Thirty
 *  Thousand Years' source still names an img/foot.jpg that the finished page
 *  dropped, which is stale source rather than a dead link on the site: eight
 *  images referenced there, eight present.
 *  The MANUAL source stays in scope on purpose — its plates DO sit beside it,
 *  and that check is what caught five manuals losing theirs in the migration. */

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
/*  Only CARDS count. Since the plug-in sources moved in, an app.json can also
 *  turn up deep inside a tree — Photo Synth keeps the browser app it was
 *  ported from under plugin/reference/ — and those are not cards and have no
 *  business in the manifest. A card sits two levels down: <area>/<slug>. */
const CARD = (f) => f.split("/").length === 3 && !NOT_SERVED_BY_PAGES.test(f);
for (const f of files.filter((f) => f.endsWith("/app.json") && CARD(f))) {
  const folder = path.dirname(f);
  if (!manifest.apps.includes(folder)) note(f, `has an app.json but manifest.json does not list ${folder}`);
}

/* --------------------------------------------- the collection is derived -- */
/* The Collection zip is built from the other plugins' published zips, so it
 * goes out of date silently every time the fleet changes - by its own commit
 * message, twice before anyone noticed. This does not let that happen again:
 * every plugin that ships a zip must be listed in contents.json as either in
 * the collection or deliberately out of it, the archive on disk must match the
 * "includes" list, and the count the site quotes must match too.
 *
 * Reading the zip's central directory directly: no unzip, no dependency, and
 * it is the actual file the download button serves. */
function zipEntries(rel) {
  const b = fs.readFileSync(path.join(ROOT, rel));
  /* End of central directory: scan back for the signature. */
  let eocd = -1;
  for (let i = b.length - 22; i >= 0 && i > b.length - 66000; i--)
    if (b.readUInt32LE(i) === 0x06054b50) { eocd = i; break; }
  if (eocd < 0) throw new Error(`${rel}: no zip end-of-central-directory`);
  const count = b.readUInt16LE(eocd + 10);
  let p = b.readUInt32LE(eocd + 16);
  const names = [];
  for (let i = 0; i < count; i++) {
    if (b.readUInt32LE(p) !== 0x02014b50) break;
    const n = b.readUInt16LE(p + 28), m = b.readUInt16LE(p + 30), k = b.readUInt16LE(p + 32);
    names.push(b.slice(p + 46, p + 46 + n).toString("utf8"));
    p += 46 + n + m + k;
  }
  return names;
}

const COLL = "vst3-apps/collection";
/* ------------------------------------------ the downloads are declared, not
 * guessed. Each plug-in's zip lives on the `downloads` release rather than in
 * this repository, so app.json carries its url, byte count and sha256. Those
 * three have to agree with the file on disk and with the page's own link, or
 * a visitor is told one thing and handed another. Whether the url ANSWERS is a
 * separate question and a separate tool, because it needs the network:
 * tools/verify-downloads.js, --full to compare hashes. */
for (const f of files.filter((f) => f.endsWith("/app.json"))) {
  const folder = path.dirname(f);
  let j; try { j = JSON.parse(fs.readFileSync(path.join(ROOT, f), "utf8")); } catch (e) { continue; }
  const list = j.downloads || [];
  const page = path.join(ROOT, folder, "index.html");
  const html = fs.existsSync(page) ? fs.readFileSync(page, "utf8") : "";

  for (const d of list) {
    for (const k of ["name", "url", "bytes", "sha256"])
      if (d[k] === undefined) note(f, `a download is missing "${k}"`);
    if (d.url && !/^https:\/\/github\.com\/[^/]+\/[^/]+\/releases\/download\//.test(d.url))
      note(f, `download url is not a release asset: ${d.url}`);
    if (d.name && html && !html.includes(d.url))
      note(`${folder}/index.html`, `does not link the declared download ${d.name}`);
    /* The zip is normally here as build output. When it is, it must BE the
       file that was declared, or the next re-cut publishes a surprise. */
    const local = path.join(ROOT, folder, d.name || "");
    if (d.name && fs.existsSync(local)) {
      const bytes = fs.statSync(local).size;
      if (bytes !== d.bytes) note(f, `${d.name} on disk is ${bytes} bytes, app.json declares ${d.bytes} — re-publish it`);
    }
  }
  /* A page that still offers a relative zip is one the move missed. */
  for (const m of html.matchAll(/href="([^"]*-win64\.zip|[^"]*Devkit\.zip)"/g))
    if (!/^https:/.test(m[1]))
      note(`${folder}/index.html`, `still links a local zip: ${m[1]} — it should point at the release`);
}

if (fs.existsSync(path.join(ROOT, `${COLL}/contents.json`))) {
  const spec = JSON.parse(fs.readFileSync(path.join(ROOT, `${COLL}/contents.json`), "utf8"));
  const real = (o) => Object.keys(o || {}).filter((k) => !k.startsWith("_"));
  const pending = real(spec.pending);
  const declared = new Set([...spec.includes, ...real(spec.excludes), ...pending]);

  /*  THREE states, and a plug-in belongs to exactly one. `includes` describes
   *  what the archive on disk really holds, so the comparison below means
   *  something; `excludes` is deliberately out; `pending` is decided and
   *  waiting for the next cut. Appearing in two is a contradiction, and it is
   *  the way this file would rot: a plug-in moved in `pending` and left in
   *  `includes` reads as done when it is not. */
  /*  What makes a pending entry contradictory is its DIRECTION against where
   *  the plug-in is today, not which list it also appears in. Currently out
   *  and decided in is the ordinary case and the whole point of the list. */
  for (const p of pending) {
    const goingOut = /^OUT\b/.test(spec.pending[p]);
    if (spec.includes.includes(p) && !goingOut)
      note(`${COLL}/contents.json`, `${p} is pending to JOIN but the archive already carries it`);
    if (real(spec.excludes).includes(p) && goingOut)
      note(`${COLL}/contents.json`, `${p} is pending to LEAVE but the archive does not carry it`);
  }
  if (pending.length)
    console.log(`  note: ${pending.length} decided change(s) waiting for the next collection cut `
              + `(${pending.join(", ")})`);

  /* Every plugin that ships a zip has to be accounted for, one way or the other. */
  for (const dir of fs.readdirSync(path.join(ROOT, "vst3-apps"))) {
    if (dir === "collection") continue;
    const has = fs.readdirSync(path.join(ROOT, "vst3-apps", dir))
      .some((f) => /-VST3-win64\.zip$/.test(f));
    if (has && !declared.has(dir))
      note(`${COLL}/contents.json`, `${dir} ships a zip but is neither included nor excluded — decide which`);
  }

  /* And the archive has to actually be what the list says it is. */
  const zip = `${COLL}/Brokild-Collection-win64.zip`;
  if (fs.existsSync(path.join(ROOT, zip))) {
    const inZip = new Set(
      zipEntries(zip)
        .map((e) => e.replace(/\\/g, "/").split("/")[1])
        .filter((e) => e && !/\.(txt|md)$/i.test(e))
    );
    /* contents.json names folders, the zip names products ("Black Rider"). */
    const norm = (s) => s.toLowerCase().replace(/[^a-z0-9]/g, "");
    const zipNorm = new Set([...inZip].map(norm));
    for (const slug of spec.includes)
      if (!zipNorm.has(norm(slug)))
        note(zip, `contents.json includes ${slug}, but the archive does not carry it`);
    for (const name of inZip)
      if (!spec.includes.some((s) => norm(s) === norm(name)))
        note(zip, `archive carries "${name}", which contents.json does not include`);
    if (inZip.size !== spec.claims.count)
      note(zip, `archive holds ${inZip.size} plugins, contents.json claims ${spec.claims.count}`);
  }

  /* BrokildWorldFX/tools/build-collection-zip.ps1 carries its own hardcoded
   * $plugins list — a third copy of the same membership, on a machine this
   * checker never runs on. If it drifts from contents.json, the next re-cut
   * silently produces the wrong archive, so compare the two here where it is
   * cheap rather than discovering it in a 93 MB download. */
  const ps1 = "BrokildWorldFX/tools/build-collection-zip.ps1";
  if (fs.existsSync(path.join(ROOT, ps1))) {
    const txt = fs.readFileSync(path.join(ROOT, ps1), "utf8");
    const block = txt.match(/\$plugins\s*=\s*@\(([\s\S]*?)\n\)/);
    if (block) {
      const slugs = [...block[1].matchAll(/slug\s*=\s*"([^"]+)"/g)].map((m) => m[1]);
      for (const s of spec.includes)
        if (!slugs.includes(s)) note(ps1, `contents.json includes ${s}, the builder's $plugins list does not`);
      for (const s of slugs)
        if (!spec.includes.includes(s)) note(ps1, `builder stages ${s}, contents.json does not include it`);
    }
  }

  /* A re-cut that updates the zip and leaves the prose saying "all ten" is the
   * same bug wearing a hat. */
  const words = { 8: "eight", 9: "nine", 10: "ten", 11: "eleven", 12: "twelve" };
  const stale = Object.entries(words)
    .filter(([n]) => Number(n) !== spec.claims.count)
    .map(([, w]) => w);
  for (const page of [`${COLL}/index.html`, `${COLL}/app.json`, "index.html"]) {
    const txt = fs.readFileSync(path.join(ROOT, page), "utf8");
    for (const w of stale) {
      const re = new RegExp(`\\ball ${w}\\b|\\b${w} plugins\\b`, "i");
      if (re.test(txt)) note(page, `says "${w}" where the collection holds ${spec.claims.count}`);
    }
  }
}

if (problems.length) {
  console.error(`${problems.length} problem(s):\n`);
  for (const p of problems) console.error("  " + p);
  process.exit(1);
}
console.log(`links OK — ${htmls.length} pages, ${manifest.apps.length} apps, no dead targets`);
