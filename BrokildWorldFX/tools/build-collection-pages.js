#!/usr/bin/env node
/*  build-collection-pages.js — the landing page and card for each collection.
 *
 *    node BrokildWorldFX/tools/build-collection-pages.js [brokild|experimental|beetmachine]
 *
 *  Replaces build-collection-page.js, which knew one collection and carried
 *  its own ITEMS table: a third copy of the membership, after contents.json
 *  and the zip builder's list. Everything here is read from contents.json and
 *  from each plug-in's own app.json, so a plug-in's own description is what
 *  appears on the collection page, and a re-cut cannot leave a page quoting
 *  the wrong number or describing a plug-in that has moved.
 *
 *  The download url, byte count and sha256 come from the publish report, so
 *  the page states what a visitor is about to fetch and tools/check-links.js
 *  can hold the page to it.
 *
 *  THE EXPERIMENTAL FRAMING IS PURPOSE, NOT POLISH, and that is deliberate:
 *  Photo Synth carries the fleet's largest manual at 38 pages, so calling
 *  these unfinished would be inaccurate and would undersell them. What they
 *  are not is useful the way a compressor is useful.
 */
"use strict";
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "../..");
const SHELL = path.join(ROOT, "vst3-apps/black-rider/index.html");
const SPEC = JSON.parse(fs.readFileSync(path.join(ROOT, "vst3-apps/collection/contents.json"), "utf8"));

const only = process.argv.slice(2).filter((a) => !a.startsWith("--"));

/* the sizes and hashes of what was actually uploaded */
let report = [];
try {
  report = JSON.parse(fs.readFileSync(path.join(ROOT, "tools/.downloads-report.json"), "utf8"));
} catch (e) { /* first run, or nothing published yet */ }

const shellRaw = fs.readFileSync(SHELL, "utf8");
const NL = shellRaw.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const shell = shellRaw.replace(/\r\n/g, "\n");
const heroAt = shell.indexOf('<header class="hero">');
const footAt = shell.indexOf("<footer");
if (heroAt < 0 || footAt < 0) { console.error("shell shape not recognised"); process.exit(1); }
const head = shell.slice(0, heroAt), foot = shell.slice(footAt);

const esc = (t) => String(t).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");

/*  A card's description is written for a grid and runs to a hundred words. On
 *  a collection page each plug-in gets a line, so take the first sentence and
 *  no more — the plug-in's own page is one click away and says the rest. */
function firstSentence(t) {
  const m = String(t).match(/^[\s\S]*?[.!?](\s|$)/);
  return (m ? m[0] : String(t)).trim();
}

for (const [key, c] of Object.entries(SPEC.collections)) {
  if (key === "proxima") continue;                 /* its survey page IS its page */
  if (only.length && !only.includes(key)) continue;

  const dir = path.join(ROOT, c.folder);
  fs.mkdirSync(dir, { recursive: true });

  /* ---- the members, each described by its own card ---------------------- */
  const members = [];
  for (const slug of c.includes) {
    const ap = path.join(ROOT, "vst3-apps", slug, "app.json");
    if (!fs.existsSync(ap)) { console.error(`  ${slug}: no app.json`); process.exit(1); }
    const j = JSON.parse(fs.readFileSync(ap, "utf8"));
    members.push({
      slug,
      /*  Card names carry qualifiers for the front-page grid — "Hairfryer
       *  (VST3) (in development)", "Photo Synth (VST3)" — and a collection
       *  listing wants the product. Strip EVERY trailing parenthetical, not
       *  just one: the first version only took a trailing "(VST3…)" and left
       *  Hairfryer wearing both of its. Whether a plug-in is in development
       *  is what this whole shelf says already. */
      name: String(j.name).replace(/(\s*\([^)]*\))+\s*$/, "").trim(),
      line: firstSentence(j.description),
      effect: (j.tags || []).includes("effect"),
    });
  }
  const instruments = members.filter((m) => !m.effect);
  const effects = members.filter((m) => m.effect);

  /* ---- the download ----------------------------------------------------- */
  const zipName = fs.readdirSync(dir).find((f) => /Collection-win64\.zip$/i.test(f));
  const pub = report.find((r) => r.ok && r.name === zipName);
  const bytes = pub ? pub.bytes : (zipName ? fs.statSync(path.join(dir, zipName)).size : 0);
  const href = pub ? pub.url : (zipName || "#");
  const MB = Math.round(bytes / 1048576);

  const list = (arr) => arr.map((m) =>
    `        <div class="c">
          <h4><a href="../${m.slug}/index.html">${esc(m.name)}</a></h4>
          <p>${esc(m.line)}</p>
        </div>`).join("\n");

  const isExp = key === "experimental";

  /*  The card's description, and the page's own: one sentence of copy, used
   *  for both. */
  const cardDesc =
      `${c.blurb} ${c.claims.phrase[0].toUpperCase() + c.claims.phrase.slice(1)} for Windows` +
      (isExp ? ", with whatever documentation each one has." : ", with their manuals and their standalones.") +
      " Each plug-in in here is byte-for-byte the same file as its own download.";

  /*  The shell is Black Rider's landing page, and its <head> came along whole:
   *  both collection pages shipped with Black Rider's <title> and meta
   *  description, so the browser tab, search results and every link preview
   *  (sync-site.js builds those FROM the title and description) announced the
   *  collection as "Black Rider — VST3 analogue monosynth". A page built from
   *  another page's shell must replace the head's identity, not just the body. */
  const escAttr = (t) => esc(t).replace(/"/g, "&quot;");
  const titleRe = /<title>[\s\S]*?<\/title>/, descRe = /<meta name="description" content="[^"]*" \/>/;
  if (!titleRe.test(head) || !descRe.test(head)) { console.error("shell head has no <title> or meta description to replace"); process.exit(1); }
  const pageHead = head
      .replace(titleRe, `<title>${esc(c.title)} &mdash; Windows VST3 collection | BrokildApps</title>`)
      .replace(descRe, `<meta name="description" content="${escAttr(cardDesc)}" />`);

  const body = `<header class="hero">
  <div class="wrap">
    <div class="kicker">Windows VST3 &middot; one download &middot; free</div>
    <h1>${esc(c.title)}</h1>
    <div class="rule"></div>
    <p class="tagline">${esc(c.blurb)}</p>
    <p class="lede">${c.lede ? esc(c.lede) : isExp
      ? `Each of these exists to ask a question about how sound can be made. A synth
      played by photographs, a synth that reads a scanned volume, a ball rolling on
      terrain, surgery on a singing voice, one voice becoming four. They are not
      products, and the source for every one of them is in the same repository as
      this page, for anyone who wants to take one further.`
      : `${c.claims.phrase[0].toUpperCase() + c.claims.phrase.slice(1)}, with their
      standalones and their handbooks. They were built one at a time, each to answer
      a different question, but they share a rack of global effects that runs inside
      the instruments, one patch folder, and a way of working: nothing asserted,
      everything measured by a bench that renders real audio and reads the numbers
      off it.`}</p>
    <div class="btnrow">
      <a class="btn btn-primary" href="${href}" download>Download all ${c.claims.count}</a>
      <span class="buildtag">${MB} MB</span>
      <a class="btn btn-ghost" href="../../index.html">Browse them one at a time</a>
    </div>
    <p class="sub" style="margin-top:1.2rem">Every plug-in in here is
      <b>byte-for-byte the same file</b> as its own download. The builder re-extracts
      the finished archive and hashes each one against its individual zip before
      publishing, and checks that nothing else is in there.</p>
  </div>
</header>

<section class="block">
  <div class="wrap">
    <h2>${instruments.length === 1 ? "The instrument" : "The instruments"}</h2>
    <div class="cards">
${list(instruments)}
    </div>
  </div>
</section>

${effects.length ? `<section class="block">
  <div class="wrap">
    <h2>${effects.length === 1 ? "The effect" : "The effects"}</h2>
    <div class="cards">
${list(effects)}
    </div>
  </div>
</section>
` : ""}
<section class="block">
  <div class="wrap">
    <h2>${isExp ? "What you are getting, honestly" : "Installing"}</h2>
    ${isExp
      ? `<p>Some of these have a standalone and a handbook; some have a bundle and a
      README. That is the point of this shelf rather than an oversight: holding an
      experiment back until it has every accessory means never shipping it at all.
      Each folder carries whatever that plug-in actually has.</p>
      <p style="margin-top:1rem">Copy each <b>.vst3 folder</b> &mdash; the whole
      folder, not the file inside it &mdash; into
      <span class="mono">C:\\Program Files\\Common Files\\VST3\\Experimental\\</span>
      and rescan in your DAW. A sub-folder is fine; hosts look inside.</p>
      <p style="margin-top:1rem">They are AGPLv3, like everything else here. Read the
      source, change it, build it, and publish your changes if you distribute a
      modified build.</p>`
      : `<p>Each folder holds the plug-in, its standalone and its manual. Copy each
      <b>.vst3 folder</b> &mdash; the whole folder, not the file inside it &mdash;
      into <span class="mono">C:\\Program Files\\Common Files\\VST3\\Brokild\\</span>
      and rescan in your DAW. Windows will ask for administrator permission, which is
      normal. The standalones need no installing: run them where they sit.</p>
      <p style="margin-top:1rem">Patches live in
      <span class="mono">Documents\\Brokild patches\\&lt;plug-in&gt;</span>, one folder
      each, and nothing is written anywhere else.</p>`}
    <div class="btnrow" style="margin-top:1.6rem">
      <a class="btn btn-primary" href="${href}" download>Download all ${c.claims.count}</a>
      <span class="buildtag">${MB} MB</span>
    </div>
  </div>
</section>

`;

  /*  The footer came from the same shell and introduced every collection as
   *  "Black Rider - a native VST3 instrument". Its first paragraph is the
   *  shell plug-in's own; replace it with one that names this collection and
   *  links the others. */
  const others = Object.entries(SPEC.collections).filter(([k]) => k !== key)
      .map(([, o]) => `<a href="../${o.folder.split("/").pop()}/index.html">${esc(o.title)}</a>`);
  const footRe = /<footer>\s*<div class="wrap">\s*<p>[\s\S]*?<\/p>/;
  if (!footRe.test(foot)) { console.error("shell footer has no first paragraph to replace"); process.exit(1); }
  const pageFoot = foot.replace(footRe, `<footer>
  <div class="wrap">
    <p><b>${esc(c.title)}</b> &mdash; one download of ${esc(c.claims.phrase)}, native
      C++ and JUCE 8. Source and issues:
      <a href="https://github.com/peterboggild/BrokildApps" target="_blank" rel="noopener">github.com/peterboggild/BrokildApps</a>.
      The other collections are ${others.slice(0, -1).join(", ")} and ${others[others.length - 1]}.</p>`);

  fs.writeFileSync(path.join(dir, "index.html"), (pageHead + body + pageFoot).replace(/\n/g, NL));

  /* ---- the card --------------------------------------------------------- */
  const app = {
    slug: c.cardSlug || (key === "brokild" ? "collection" : "experimental-collection"),
    name: `${c.title} (all ${c.claims.count})`,
    description: cardDesc,
    status: "live",
    collection: true,
    url: `${c.folder}/index.html`,
    icon: "more",
    cta: `Download all ${c.claims.count} \u2192`,
    preview: `assets/app-previews/${c.cardSlug || (key === "brokild" ? "collection" : "experimental-collection")}.jpg`,
    note: `Windows VST3 \u00b7 ${MB} MB \u00b7 ${c.claims.phrase}` +
          (isExp ? " \u00b7 source included." : " \u00b7 all manuals included."),
    tags: ["vst3"],
    downloads: pub ? [{ name: pub.name, url: pub.url, bytes: pub.bytes, sha256: pub.hash }] : undefined,
  };
  if (!app.downloads) delete app.downloads;
  fs.writeFileSync(path.join(dir, "app.json"), (JSON.stringify(app, null, 2) + "\n").replace(/\n/g, NL));

  console.log(`  ${key.padEnd(13)} ${c.claims.count} plug-ins, ${MB} MB -> ${c.folder}/`);
}
