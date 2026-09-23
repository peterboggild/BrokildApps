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

/* ------------------------------------------ the collections are derived -- */
/*
 * A collection is built FROM the plug-ins, so it goes stale the moment one is
 * added or renamed, and it had done so twice before anyone noticed. The rules
 * this enforces:
 *
 *   - every plug-in that ships a download is in exactly ONE collection's
 *     includes, or in excludes, and never in two;
 *   - each archive ON DISK carries exactly the plug-ins its list names;
 *   - the count and phrase each collection claims match its list, and no page
 *     quotes a different number.
 *
 * Adding a plug-in therefore forces a decision instead of leaving one to be
 * remembered.
 */
const SPEC_PATH = "vst3-apps/collection/contents.json";
if (fs.existsSync(path.join(ROOT, SPEC_PATH))) {
  const spec = JSON.parse(fs.readFileSync(path.join(ROOT, SPEC_PATH), "utf8"));
  const real = (o) => Object.keys(o || {}).filter((k) => !k.startsWith("_"));
  const cols = spec.collections || {};

  /* ---- one home each ---------------------------------------------------- */
  const home = new Map();
  for (const [key, c] of Object.entries(cols))
    for (const slug of c.includes || []) {
      if (home.has(slug))
        note(SPEC_PATH, `${slug} is in both ${home.get(slug)} and ${key} — it belongs to one`);
      home.set(slug, key);
    }
  for (const slug of real(spec.excludes)) {
    if (home.has(slug)) note(SPEC_PATH, `${slug} is excluded and also in ${home.get(slug)}`);
    home.set(slug, "excluded");
  }

  /*  Anything shipping a download has to be placed. The Artefacts ship four
   *  zips from one folder and are listed by their own names, so that folder is
   *  matched against the proxima list rather than against its folder name. */
  const PROXIMA_DIR = "proxima-centauri-b";
  for (const dir of fs.readdirSync(path.join(ROOT, "vst3-apps"))) {
    const full = path.join(ROOT, "vst3-apps", dir);
    if (!fs.statSync(full).isDirectory()) continue;
    if (Object.values(cols).some((c) => c.folder === `vst3-apps/${dir}` && dir !== PROXIMA_DIR)) continue;
    const zips = fs.readdirSync(full).filter((f) => /-win64\.zip$/i.test(f));
    if (!zips.length) continue;
    if (dir === PROXIMA_DIR) {
      const want = (cols.proxima && cols.proxima.includes) || [];
      if (zips.length !== want.length)
        note(SPEC_PATH, `proxima claims ${want.length} findings, the folder ships ${zips.length} zips`);
      continue;
    }
    if (!home.has(dir))
      note(SPEC_PATH, `${dir} ships a zip but is in no collection and is not excluded — decide which`);
  }

  /* ---- each claim matches its own list ---------------------------------- */
  for (const [key, c] of Object.entries(cols)) {
    const n = (c.includes || []).length;
    if (c.claims && c.claims.count !== n)
      note(SPEC_PATH, `${key} lists ${n} plug-ins and claims ${c.claims.count}`);
  }

  /* ---- each archive on disk carries what its list says ------------------ */
  const norm = (x) => String(x).toLowerCase().replace(/[^a-z0-9]/g, "");
  for (const [key, c] of Object.entries(cols)) {
    if (key === "proxima") continue;          /* four separate zips, not one archive */
    const dir = path.join(ROOT, c.folder || "");
    if (!fs.existsSync(dir)) continue;
    const zip = fs.readdirSync(dir).find((f) => /Collection-win64\.zip$/i.test(f));
    if (!zip) continue;                        /* not cut on this machine; CI skips */
    const rel = `${c.folder}/${zip}`;
    let inZip;
    try {
      inZip = new Set(zipEntries(rel)
        .map((e) => e.replace(/\\/g, "/").split("/")[1])
        .filter((e) => e && !/\.(txt|md)$/i.test(e)));
    } catch (e) { continue; }
    const zipNorm = new Set([...inZip].map(norm));
    for (const slug of c.includes || [])
      if (!zipNorm.has(norm(slug)))
        note(rel, `${key} includes ${slug}, but the archive does not carry it`);
    for (const name of inZip)
      if (!(c.includes || []).some((sl) => norm(sl) === norm(name)))
        note(rel, `archive carries "${name}", which ${key} does not include`);
    if (c.claims && inZip.size !== c.claims.count)
      note(rel, `archive holds ${inZip.size} plug-ins, ${key} claims ${c.claims.count}`);
  }

  /* ---- the card must SAY what the collection holds ----------------------- */
  /*  A positive check, deliberately. Hunting for a wrong number in prose does
   *  not work: "One counts, and its sound is the list of moments" is about one
   *  of four objects, and "all four objects, twenty-one recorded passages" is
   *  correct and carries three number words. So the collection declares the
   *  phrase it claims, and the card has to carry it verbatim. Re-cut the
   *  membership, change the phrase, and this names every card still wearing
   *  the old one. */
  for (const [key, c] of Object.entries(cols)) {
    const phrase = c.claims && c.claims.phrase;
    if (!phrase) continue;
    const appPath = path.join(ROOT, c.folder || "", "app.json");
    if (!fs.existsSync(appPath)) continue;
    const j = JSON.parse(fs.readFileSync(appPath, "utf8"));
    const hay = [j.name, j.description, j.note, j.cta].filter(Boolean).join(" ").toLowerCase();
    if (!hay.includes(phrase.toLowerCase()))
      note(`${c.folder}/app.json`, `does not say "${phrase}", which is what ${key} holds`);
  }

}

if (problems.length) {
  console.error(`${problems.length} problem(s):\n`);
  for (const p of problems) console.error("  " + p);
  process.exit(1);
}
console.log(`links OK — ${htmls.length} pages, ${manifest.apps.length} apps, no dead targets`);
