// The checker looked in Source/ only. The plugins written in the website repo
// keep their code in src/, so their patchFolder call was invisible and their
// patch folder was reported unclaimed.
const fs = require("fs");
const p = "C:/Users/peter/b/BrokildWorldFX/tools/check-names.js";
let s = fs.readFileSync(p, "utf8");

const block = `  const srcDir = path.join(root, "Source");
  if (fs.existsSync(srcDir)) {
    for (const f of fs.readdirSync(srcDir)) {
      if (!/\\.(cpp|h)$/.test(f)) continue;
      const t = fs.readFileSync(path.join(srcDir, f), "utf8");
      const m = t.match(/patchFolder\\s*\\(\\s*"([^"]+)"/);
      if (m && !folder) folder = m[1];
      //  former names live in the 4th argument, as a brace list of strings
      const fm = t.match(/patchFolder\\s*\\([^;]*?\\{\\s*("(?:[^"\\\\]|\\\\.)*"(?:\\s*,\\s*"(?:[^"\\\\]|\\\\.)*")*)\\s*\\}\\s*\\)\\s*;/s);
      if (fm && !former.length) {
        const parts = fm[1].match(/"(?:[^"\\\\]|\\\\.)*"/g) || [];
        former = parts.map(p => p.slice(1, -1)).filter(p => !p.startsWith("\\\\\\""));
      }
    }
  }`;

if (s.split(block).length !== 2) { console.error("the source-scanning block did not match exactly"); process.exit(1); }

const replacement = `  // Source/ in most trees, src/ in the ones written in the website repo
  for (const srcDir of [path.join(root, "Source"), path.join(root, "src")]) {
    if (!fs.existsSync(srcDir)) continue;
    for (const f of fs.readdirSync(srcDir)) {
      if (!/\\.(cpp|h)$/.test(f)) continue;
      const t = fs.readFileSync(path.join(srcDir, f), "utf8");
      const m = t.match(/patchFolder\\s*\\(\\s*"([^"]+)"/);
      if (m && !folder) folder = m[1];
      //  former names live in the 4th argument, as a brace list of strings
      const fm = t.match(/patchFolder\\s*\\([^;]*?\\{\\s*("(?:[^"\\\\]|\\\\.)*"(?:\\s*,\\s*"(?:[^"\\\\]|\\\\.)*")*)\\s*\\}\\s*\\)\\s*;/s);
      if (fm && !former.length) {
        const parts = fm[1].match(/"(?:[^"\\\\]|\\\\.)*"/g) || [];
        former = parts.map(p => p.slice(1, -1)).filter(p => !p.startsWith("\\\\\\""));
      }
    }
  }`;

fs.writeFileSync(p, s.split(block).join(replacement));
console.log("the checker now scans src/ as well as Source/");
