// Chooser follow-ups after the first live look:
//  - a long legend on a short switch (two or three positions) breaks at its
//    first space on purpose ("LADDER" over "24 dB") instead of rag-wrapping;
//  - the probe's chart check filters for the patch message rather than
//    demanding it arrive alone (the page batches sends into one flush).
"use strict";
const fs = require("fs");
const files = {};
function load(k, p) { const raw = fs.readFileSync(p, "utf8"); files[k] = { p, crlf: raw.indexOf("\r\n") >= 0, s: raw.replace(/\r\n/g, "\n"), n: 0 }; }
function edit(k, from, to, count = 1) {
  const f = files[k]; const parts = f.s.split(from);
  if (parts.length - 1 !== count) { console.error("ANCHOR MISS in " + k + " (" + (parts.length - 1) + "): " + from.slice(0, 100)); process.exit(1); }
  f.s = parts.join(to); f.n++;
}
const root = "C:/Users/peter/b/Nineteen84/";
load("u", root + "Source/ui/ui.html");
load("p", root + "test/uiprobe.js");
load("j", root + "test/choosers-jobs.json");

edit("u",
`.seg b:hover{color:var(--cream)}`,
`.seg b span{display:block;white-space:nowrap}
.seg b:hover{color:var(--cream)}`);

edit("u",
`      var b = el("b", "", sg); b.textContent = nm; b.dataset.i = String(i);
      b.style.top = pos(i).toFixed(1) + "px";`,
`      var b = el("b", "", sg); b.dataset.i = String(i);
      var lt = el("span", "", b), sp = nm.indexOf(" ");
      if (nPos <= 3 && nm.length > 9 && sp > 0) {        /* a long legend on a short switch: break it on purpose */
        lt.appendChild(document.createTextNode(nm.slice(0, sp)));
        el("br", "", lt);
        lt.appendChild(document.createTextNode(nm.slice(sp + 1)));
      } else lt.textContent = nm;
      b.style.top = pos(i).toFixed(1) + "px";`);

edit("p",
`      var m = msgs();
      ok("a chart entry loads its patch and closes the chart",
         m.length === 1 && m[0].k === "patch" && m[0].i === 3 && !document.getElementById("pmenu"),
         JSON.stringify(m) + (document.getElementById("pmenu") ? " still open" : ""));`,
`      var m = msgs().filter(function (x) { return x && x.k === "patch"; });
      ok("a chart entry loads its patch and closes the chart",
         m.length === 1 && m[0].i === 3 && !document.getElementById("pmenu"),
         JSON.stringify(m) + (document.getElementById("pmenu") ? " still open" : ""));`);

edit("j",
`  { "name": "pick entry 8", "eval": "document.querySelectorAll('#pmenu .pi')[7].click(); 'picked'" },`,
`  { "name": "reopen (a screenshot fires a resize, and a resize closes the chart)", "eval": "document.getElementById('pmenu') ? 'open' : (document.getElementById('pwin').click(), 'reopened')" },
  { "name": "pick entry 8", "eval": "document.querySelectorAll('#pmenu .pi')[7].click(); 'picked'" },`);

for (const k in files) { const f = files[k]; fs.writeFileSync(f.p, f.crlf ? f.s.replace(/\n/g, "\r\n") : f.s); console.log(k + ": " + f.n + " edits"); }
