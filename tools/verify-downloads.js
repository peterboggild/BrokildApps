#!/usr/bin/env node
/*  verify-downloads.js — prove every declared download is really there.
 *
 *    node tools/verify-downloads.js [--full] [slug ...]
 *
 *  "Uploaded" and "a stranger can download it and get the same bytes" are
 *  different claims, and only the second one matters. By default this asks for
 *  the asset's headers and checks the size the server reports against the size
 *  declared in app.json. With --full it downloads each file and compares the
 *  sha256, which is the claim in full and costs 161 MB of traffic.
 *
 *  It follows the public url, unauthenticated, exactly as a visitor would.
 */
"use strict";
const fs = require("fs");
const path = require("path");
const cp = require("child_process");
const crypto = require("crypto");

const ROOT = path.resolve(__dirname, "..");
const args = process.argv.slice(2);
const FULL = args.includes("--full");
const only = args.filter((a) => !a.startsWith("--"));

function sh(c) { return cp.execSync(c, { encoding: "utf8", maxBuffer: 1 << 28, stdio: ["pipe", "pipe", "pipe"] }); }

/* Every app.json that declares a download. */
const apps = sh(`git -C "${ROOT}" ls-files "vst3-apps/*/app.json"`).split("\n").filter(Boolean);
let rows = [];
for (const rel of apps) {
  const j = JSON.parse(fs.readFileSync(path.join(ROOT, rel), "utf8"));
  const list = j.downloads || (j.download ? [j.download] : []);
  for (const d of list) rows.push({ app: rel, ...d });
}
if (only.length) rows = rows.filter((r) => only.some((o) => r.app.includes(o) || (r.url || "").includes(o)));

if (!rows.length) { console.log("no app.json declares a download yet"); process.exit(0); }

let bad = 0;
for (const r of rows) {
  const name = (r.url || "").split("/").pop();
  let note = "", ok = false;
  try {
    if (FULL) {
      const tmp = path.join(require("os").tmpdir(), "vd-" + name);
      sh(`curl -sL -o "${tmp}" "${r.url}"`);
      const bytes = fs.statSync(tmp).size;
      const hash = crypto.createHash("sha256").update(fs.readFileSync(tmp)).digest("hex");
      fs.rmSync(tmp, { force: true });
      ok = bytes === r.bytes && hash === r.sha256;
      note = ok ? `${(bytes / 1048576).toFixed(1)} MB, sha256 matches`
                : `got ${bytes} bytes / ${hash.slice(0, 12)}, declared ${r.bytes} / ${String(r.sha256).slice(0, 12)}`;
    } else {
      /*  Ask curl what it actually received rather than parsing headers. A
       *  release url is a 302 to the object store, and the redirect carries
       *  "Content-Length: 0" — so a regex taking the FIRST match in the chain
       *  reports every healthy asset as empty, which is what it did. */
      const full = sh(`curl -sIL "${r.url}"`);
      const lens = [...full.matchAll(/[Cc]ontent-[Ll]ength: *(\d+)/g)].map((m) => parseInt(m[1], 10));
      const len = lens.length ? lens[lens.length - 1] : 0;
      const code = (full.match(/HTTP\/[\d.]+ (\d+)/g) || []).pop() || "";
      ok = /200/.test(code) && len === r.bytes;
      note = ok ? `${(len / 1048576).toFixed(1)} MB, size matches`
                : `${code.trim()}, server says ${len}, declared ${r.bytes}`;
    }
  } catch (e) { note = String(e.message || e).slice(0, 90); }
  if (!ok) bad++;
  console.log(`${ok ? "  ok  " : " FAIL "} ${name.padEnd(38)} ${note}`);
}

console.log("");
console.log(bad === 0 ? `${rows.length} downloads reachable and the right size — ALL CLEAR`
                      : `${bad} of ${rows.length} downloads FAILED`);
process.exit(bad ? 1 : 0);
