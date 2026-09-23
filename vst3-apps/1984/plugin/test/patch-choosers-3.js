// A resize re-places the bank chart instead of closing it: the window may be
// resized with the chart open (and every screenshot over CDP fires a resize).
"use strict";
const fs = require("fs");
const p = "C:/Users/peter/b/Nineteen84/Source/ui/ui.html";
const raw = fs.readFileSync(p, "utf8");
const crlf = raw.indexOf("\r\n") >= 0;
let s = raw.replace(/\r\n/g, "\n"), n = 0;
function edit(from, to) { const parts = s.split(from); if (parts.length !== 2) { console.error("ANCHOR MISS (" + (parts.length - 1) + "): " + from.slice(0, 80)); process.exit(1); } s = parts.join(to); n++; }

edit(`  pmenu = el("div", "", document.body); pmenu.id = "pmenu";
  pmenu.style.fontSize = Math.max(10, 11 * DECK_S).toFixed(1) + "px";
  var cur = Math.round(V.patch || 0), col = null, cat = null;`,
`  pmenu = el("div", "", document.body); pmenu.id = "pmenu";
  var cur = Math.round(V.patch || 0), col = null, cat = null;`);

edit(`  var r = w.getBoundingClientRect(), m = pmenu.getBoundingClientRect();
  var x = r.left, y = r.bottom + 6;
  if (x + m.width > innerWidth - 8) x = innerWidth - 8 - m.width;
  if (x < 8) x = 8;
  if (y + m.height > innerHeight - 8) y = Math.max(8, r.top - 6 - m.height);
  pmenu.style.left = Math.round(x) + "px"; pmenu.style.top = Math.round(y) + "px";
  w.classList.add("open");
}`,
`  placePatchMenu();
  w.classList.add("open");
}
function placePatchMenu() {                      /* under the window, on screen, sized to the deck */
  var w = $("#pwin"); if (!pmenu || !w) return;
  pmenu.style.fontSize = Math.max(10, 11 * DECK_S).toFixed(1) + "px";
  var r = w.getBoundingClientRect(), m = pmenu.getBoundingClientRect();
  var x = r.left, y = r.bottom + 6;
  if (x + m.width > innerWidth - 8) x = innerWidth - 8 - m.width;
  if (x < 8) x = 8;
  if (y + m.height > innerHeight - 8) y = Math.max(8, r.top - 6 - m.height);
  pmenu.style.left = Math.round(x) + "px"; pmenu.style.top = Math.round(y) + "px";
}`);

edit(`  DECK_S = s; closePatchMenu();`, `  DECK_S = s; placePatchMenu();`);

fs.writeFileSync(p, crlf ? s.replace(/\n/g, "\r\n") : s);
console.log("ui.html: " + n + " edits");
