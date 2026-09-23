// cdp.js: `pad` may be a number or {l,t,r,b}, so a plate can take its
// nameplate along without a fixed rectangle (the deck scales with the window).
"use strict";
const fs = require("fs");
const p = "C:/Users/peter/b/Nineteen84/tools/cdp.js";
const raw = fs.readFileSync(p, "utf8");
const crlf = raw.indexOf("\r\n") >= 0;
let s = raw.replace(/\r\n/g, "\n");
const a = `        const pad = j.pad || 0;
        params = { format: "png", captureBeyondViewport: true, clip: { x: b.x - pad, y: b.y - pad, width: b.w + 2 * pad, height: b.h + 2 * pad, scale: j.scale || 2 } };`;
if (s.split(a).length !== 2) { console.error("anchor miss"); process.exit(1); }
s = s.replace(a, `        const pd = typeof j.pad === "object" && j.pad ? j.pad : { l: j.pad || 0, t: j.pad || 0, r: j.pad || 0, b: j.pad || 0 };
        params = { format: "png", captureBeyondViewport: true, clip: { x: b.x - (pd.l || 0), y: b.y - (pd.t || 0), width: b.w + (pd.l || 0) + (pd.r || 0), height: b.h + (pd.t || 0) + (pd.b || 0), scale: j.scale || 2 } };`);
fs.writeFileSync(p, crlf ? s.replace(/\n/g, "\r\n") : s);
console.log("cdp.js: per-side pads");
