// cdp.js: `sel2` extends an element clip to the union with a second element.
"use strict";
const fs = require("fs");
const p = "C:/Users/peter/b/Nineteen84/tools/cdp.js";
const raw = fs.readFileSync(p, "utf8");
const crlf = raw.indexOf("\r\n") >= 0;
let s = raw.replace(/\r\n/g, "\n");
const a = `        b = q.result && q.result.result && q.result.result.value;
        if (!b) { failures++; console.log("  " + (j.name || "shot") + "\\n     NO ELEMENT " + j.sel); continue; }`;
if (s.split(a).length !== 2) { console.error("anchor miss"); process.exit(1); }
s = s.replace(a, a + `
        if (j.sel2) {
          const expr2 = "(function(){var e=document.querySelector(" + JSON.stringify(j.sel2) + ");if(!e)return null;var r=e.getBoundingClientRect();return {x:r.left+window.scrollX,y:r.top+window.scrollY,w:r.width,h:r.height};})()";
          const q2 = await ws.send("Runtime.evaluate", { expression: expr2, returnByValue: true });
          const c = q2.result && q2.result.result && q2.result.result.value;
          if (c) { const x0 = Math.min(b.x, c.x), y0 = Math.min(b.y, c.y), x1 = Math.max(b.x + b.w, c.x + c.w), y1 = Math.max(b.y + b.h, c.y + c.h); b = { x: x0, y: y0, w: x1 - x0, h: y1 - y0 }; }
        }`);
fs.writeFileSync(p, crlf ? s.replace(/\n/g, "\r\n") : s);
console.log("cdp.js: sel2 union");
