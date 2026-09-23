// Teach tools/cdp.js to shoot an ELEMENT ({ shoot, sel, pad, scale }) or a
// RECT in page coordinates ({ shoot, rect:[x,y,w,h], scale }).
// Line-ending aware: git's autocrlf hands this file back as CRLF.
"use strict";
const fs = require("fs");
const p = "C:/Users/peter/b/Nineteen84/tools/cdp.js";
let raw = fs.readFileSync(p, "utf8");
const crlf = raw.indexOf("\r\n") >= 0;
let s = raw.replace(/\r\n/g, "\n");
const a = '    if (j.shoot) {\n      const r = await ws.send("Page.captureScreenshot", { format: "png" });';
if (s.split(a).length !== 2) { console.error("anchor miss (" + (s.split(a).length - 1) + ")"); process.exit(1); }
const NL = "\\n";
const rep = [
'    if (j.shoot) {',
'      /* an element or rect plate: clip in PAGE coordinates (the scroll added), rendered at scale */',
'      let params = { format: "png" };',
'      let b = null;',
'      if (j.rect) b = { x: j.rect[0], y: j.rect[1], w: j.rect[2], h: j.rect[3] };',
'      else if (j.sel) {',
'        const expr = "(function(){var e=document.querySelector(" + JSON.stringify(j.sel) + ");if(!e)return null;var r=e.getBoundingClientRect();return {x:r.left+window.scrollX,y:r.top+window.scrollY,w:r.width,h:r.height};})()";',
'        const q = await ws.send("Runtime.evaluate", { expression: expr, returnByValue: true });',
'        b = q.result && q.result.result && q.result.result.value;',
'        if (!b) { failures++; console.log("  " + (j.name || "shot") + "' + NL + '     NO ELEMENT " + j.sel); continue; }',
'      }',
'      if (b) {',
'        const pad = j.pad || 0;',
'        params = { format: "png", captureBeyondViewport: true, clip: { x: b.x - pad, y: b.y - pad, width: b.w + 2 * pad, height: b.h + 2 * pad, scale: j.scale || 2 } };',
'      }',
'      const r = await ws.send("Page.captureScreenshot", params);'
].join("\n");
s = s.replace(a, rep);
if (crlf) s = s.replace(/\n/g, "\r\n");
fs.writeFileSync(p, s);
console.log("cdp.js: element and rect plates added (" + (crlf ? "CRLF" : "LF") + ")");
