// Probe checks for the choosers: the slider-row LISTs are slide switches whose
// knob travels with the value and whose legends fit their cell, no HTML select
// is visible anywhere, and the header's patch window opens a bank chart that
// lists every patch, marks the one on the dial, loads on a click and closes.
"use strict";
const fs = require("fs");
const p = "C:/Users/peter/b/Nineteen84/test/uiprobe.js";
const raw = fs.readFileSync(p, "utf8");
const crlf = raw.indexOf("\r\n") >= 0;
let s = raw.replace(/\r\n/g, "\n");
let n = 0;
function edit(from, to) { const parts = s.split(from); if (parts.length !== 2) { console.error("ANCHOR MISS (" + (parts.length - 1) + "): " + from.slice(0, 80)); process.exit(1); } s = parts.join(to); n++; }

edit(`    /* ---- the tooltip sits BESIDE its control -------------------------- */`,
`    /* ---- the choosers are switches, not HTML selects ------------------ */
    var vis = Array.prototype.filter.call(document.querySelectorAll("select"), function (s) { return s.offsetWidth > 0 || s.offsetHeight > 0; });
    ok("no HTML select is visible on the panel", vis.length === 0, vis.length + " visible");
    var sw = byId["a_oct"][0], kb = sw.querySelector(".sknob"), legs = sw.querySelectorAll(".seg b");
    ok("a slider-row LIST is a slide switch with a knob and a legend", !!kb && legs.length === 5, (kb ? "knob, " : "no knob, ") + legs.length + " legends");
    window.__N84.set("a_oct", 0); var kTop0 = parseFloat(kb.style.top);
    window.__N84.set("a_oct", 4); var kTop4 = parseFloat(kb.style.top);
    ok("the switch knob travels with the value", kTop4 > kTop0 + 40, kTop0 + " -> " + kTop4);
    ok("the knob sits on the lit legend and only that one is lit",
       Math.abs(kTop4 - parseFloat(legs[4].style.top)) < 0.5 && legs[4].classList.contains("on") && sw.querySelectorAll(".seg b.on").length === 1,
       kTop4 + " vs " + legs[4].style.top);
    var tight = [];
    Array.prototype.forEach.call(document.querySelectorAll(".ctl.seg"), function (c) {
      var g = c.querySelector(".seg");
      if (g.scrollWidth > g.clientWidth + 1 || g.scrollHeight > g.clientHeight + 1)
        tight.push(c.dataset.id + " " + g.scrollWidth + "/" + g.clientWidth + " " + g.scrollHeight + "/" + g.clientHeight);
    });
    ok("every legend fits its switch cell", tight.length === 0, tight.join(" | "));

    /* ---- the tooltip sits BESIDE its control -------------------------- */`);

edit(`    /* ---- and nothing threw --------------------------------------------- */
    ok("no JS errors", window.__N84ERR.length === 0, window.__N84ERR.join(" | "));
    return Promise.resolve();
  }`,
`    /* ---- the patch window and the bank chart --------------------------- */
    var pw = document.getElementById("pwin");
    feed("patchinfo", { i: 8, name: "PROBE PAD", cat: "PADS" });
    ok("the patch window shows the number of the patch on the dial", document.getElementById("pwinnum").textContent === "09", document.getElementById("pwinnum").textContent);
    ok("the patch window names it", document.getElementById("pwintxt").textContent === (F.patches[8].name || ""), document.getElementById("pwintxt").textContent);
    pw.dispatchEvent(new MouseEvent("click", { bubbles: true }));
    var pm = document.getElementById("pmenu");
    ok("clicking the window opens the bank chart", !!pm);
    var cats = {}; F.patches.forEach(function (q) { cats[q.cat || "PATCHES"] = 1; });
    ok("the chart lists every patch in its category column",
       !!pm && pm.querySelectorAll(".pi").length === F.patches.length && pm.querySelectorAll(".pc").length === Object.keys(cats).length,
       pm ? pm.querySelectorAll(".pi").length + " entries, " + pm.querySelectorAll(".pc").length + " columns" : "no chart");
    ok("the chart marks the patch on the dial", !!pm && pm.querySelectorAll(".pi.on").length === 1 && pm.querySelector(".pi.on").dataset.n === "8",
       pm ? String(pm.querySelectorAll(".pi.on").length) : "-");
    var pr = pm ? pm.getBoundingClientRect() : null;
    ok("the chart is on screen", !!pr && pr.left >= 0 && pr.top >= 0 && pr.right <= innerWidth && pr.bottom <= innerHeight,
       pr ? JSON.stringify([pr.left, pr.top, pr.right, pr.bottom]) : "-");
    document.body.dispatchEvent(new PointerEvent("pointerdown", { bubbles: true }));
    ok("a press outside closes the chart", !document.getElementById("pmenu"));
    pw.dispatchEvent(new MouseEvent("click", { bubbles: true }));
    clr();
    var entry = document.querySelector('#pmenu .pi[data-n="3"]');
    if (entry) entry.dispatchEvent(new MouseEvent("click", { bubbles: true }));
    return tick().then(function () {
      var m = msgs();
      ok("a chart entry loads its patch and closes the chart",
         m.length === 1 && m[0].k === "patch" && m[0].i === 3 && !document.getElementById("pmenu"),
         JSON.stringify(m) + (document.getElementById("pmenu") ? " still open" : ""));
      ok("the window followed the pick", document.getElementById("pwinnum").textContent === "04", document.getElementById("pwinnum").textContent);

      /* ---- and nothing threw ------------------------------------------- */
      ok("no JS errors", window.__N84ERR.length === 0, window.__N84ERR.join(" | "));
    });
  }`);

fs.writeFileSync(p, crlf ? s.replace(/\n/g, "\r\n") : s);
console.log("uiprobe.js: " + n + " edits");
