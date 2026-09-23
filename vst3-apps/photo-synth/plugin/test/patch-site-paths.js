/*  Point everything at the renamed folder, and leave a REDIRECT STUB at the
    old URL — the Martian Gain precedent. A shared link must not die because
    a product was renamed. */
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const ROOT = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps";
const miss = [];

//  1. app.json: url, preview, slug, and the build in the note
{
  const P = ROOT + "/vst3-apps/photo-synth/app.json";
  const j = JSON.parse(fs.readFileSync(P, "utf8"));
  j.slug = "photo-synth-vst3";
  j.url = "vst3-apps/photo-synth/index.html";
  j.preview = "assets/app-previews/photo-synth-vst3.jpg";
  j.note = j.note.replace(/build \d+\.\d+/, "build 260830.1");
  fs.writeFileSync(P, JSON.stringify(j, null, 2) + NLo);
  console.log("app.json: " + j.name + " | " + j.url);
}

//  2. the manifest
{
  const P = ROOT + "/manifest.json";
  let s = fs.readFileSync(P, "utf8");
  const A = '"vst3-apps/photo-synth-2"';
  if (s.split(A).length - 1 !== 1) miss.push("manifest x" + (s.split(A).length - 1));
  else { fs.writeFileSync(P, s.split(A).join('"vst3-apps/photo-synth"')); }
}

//  3. any remaining in-page links to the old folder
{
  const files = [];
  (function walk(d) {
    for (const e of fs.readdirSync(d, { withFileTypes: true })) {
      if (["\.git", "node_modules", "img", "assets", "BrokildWorldFX"].includes(e.name)) continue;
      const p = d + "/" + e.name;
      if (e.isDirectory()) walk(p);
      else if (/\.(html|json)$/i.test(e.name)) files.push(p);
    }
  })(ROOT);
  let n = 0;
  for (const f of files) {
    let s = fs.readFileSync(f, "utf8");
    const b = s;
    s = s.split("vst3-apps/photo-synth-2").join("vst3-apps/photo-synth");
    s = s.split("app-previews/photo-synth-2.jpg").join("app-previews/photo-synth-vst3.jpg");
    if (s !== b) { fs.writeFileSync(f, s); n++; }
  }
  console.log("re-pointed links in " + n + " files");
}

//  4. the redirect stub
{
  const dir = ROOT + "/vst3-apps/photo-synth-2";
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  const html = [
    "<!doctype html>",
    "<html lang=\"en\">",
    "<head>",
    "  <meta charset=\"utf-8\">",
    "  <title>Photo Synth has moved</title>",
    "  <link rel=\"canonical\" href=\"../photo-synth/index.html\">",
    "  <meta http-equiv=\"refresh\" content=\"0; url=../photo-synth/index.html\">",
    "  <style>",
    "    body { background:#0d0f12; color:#cfd8dc; font:15px/1.6 system-ui,Segoe UI,Roboto,sans-serif;",
    "           display:flex; min-height:100vh; margin:0; align-items:center; justify-content:center; }",
    "    a { color:#7fd6c8; }",
    "  </style>",
    "</head>",
    "<body>",
    "  <p>Photo-Synth&nbsp;2 is now simply <strong>Photo Synth</strong> &mdash;",
    "     <a href=\"../photo-synth/index.html\">continue to its page</a>.</p>",
    "</body>",
    "</html>",
    ""].join(NLo);
  fs.writeFileSync(dir + "/index.html", html);
  console.log("redirect stub left at the old URL");
}

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
