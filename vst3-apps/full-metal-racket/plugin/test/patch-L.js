// THE KIT BROWSER.
//
// Two hundred kits behind a pair of arrow buttons is not a library, it is a
// queue — and the saved patches had no way in at all short of a file dialog.
// Clicking the kit name now opens a menu: the two hundred grouped under their
// twelve categories, then whatever .fmrkit files are in the patch folder, then
// the actions.
//
// The menu is built on document.body and NOT inside the deck. The deck carries
// a CSS transform for the scale-to-fit, so anything positioned from
// getBoundingClientRect inside it lands somewhere else entirely. That one cost
// an afternoon in Photo-Synth and is written down in CLAUDE.md.
"use strict";
const fs = require("fs");
const miss = [];
function rep(s, a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return s; }
  return s.split(a).join(b);
}
const P = "C:/Users/peter/b/FullMetalRacket/Source/ui/ui.html";
let s = fs.readFileSync(P, "utf8");

// ─────────────────────────────────────────────────────────────────── CSS ───
s = rep(s, `#tip{position:fixed;`,
`/* ── the kit browser ────────────────────────────────────────────────────── */
#scrim{position:fixed;inset:0;z-index:70;display:none}
#scrim.on{display:block}
#menu{position:fixed;z-index:71;display:none;flex-direction:column;
  max-height:82vh;width:520px;border-radius:4px;overflow:hidden;
  background:linear-gradient(180deg,#241c10,#141009);border:1px solid #000;
  box-shadow:0 22px 60px -14px #000,inset 0 1px 0 #ffffff18}
#menu.on{display:flex}
#menu .mh{flex:none;display:flex;align-items:center;gap:10px;padding:11px 14px;
  border-bottom:1px solid #000;background:linear-gradient(180deg,#33291a,#221a0f)}
#menu .mh b{flex:1;font:400 22px/1 var(--f-disp);letter-spacing:.06em;
  text-transform:uppercase;color:var(--cream)}
#menu .mh .k{height:28px;padding:0 12px;font-size:11px}
#mfilter{flex:1;min-width:0;background:#0c0803;border:1px solid #3a2e1b;border-radius:2px;
  color:var(--cream);font:700 12px/1 var(--f-lab);letter-spacing:.08em;padding:8px 10px;outline:none}
#mfilter::placeholder{color:#7a6a4d}
#mlist{flex:1;overflow-y:auto;overflow-x:hidden}
#mlist .grp{position:sticky;top:0;background:linear-gradient(180deg,#2b2213,#1d1609);
  font:700 9px/1 var(--f-lab);letter-spacing:.22em;text-transform:uppercase;color:#a08e6c;
  padding:8px 14px;border-top:1px solid #000;border-bottom:1px solid #000}
#mlist .row{display:flex;align-items:baseline;gap:10px;padding:7px 14px;cursor:pointer;
  border-bottom:1px solid #ffffff08}
#mlist .row:hover{background:#ffffff14}
#mlist .row.cur{background:linear-gradient(90deg,#a32d1255,#0000);box-shadow:inset 3px 0 0 var(--red)}
#mlist .row .no{flex:none;width:34px;font:700 10px/1.3 var(--f-mono);color:#7a6a4d;text-align:right}
#mlist .row .nm{flex:1;min-width:0;font:700 13px/1.3 var(--f-lab);letter-spacing:.06em;
  color:var(--cream);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#mlist .row .tag{flex:none;font:700 9px/1.3 var(--f-lab);letter-spacing:.14em;color:#8d7d5e;
  text-transform:uppercase}
#menu .mf{flex:none;display:flex;gap:6px;padding:9px 12px;border-top:1px solid #000;
  background:linear-gradient(180deg,#241c10,#181207)}
#menu .mf .k{flex:1;height:32px;font-size:11px}
#kitbox{cursor:pointer}
#kitbox:hover #kitname{text-shadow:0 0 22px #ff8f1f90}

#tip{position:fixed;`, "menu css");

// ──────────────────────────────────────────────────────────────── markup ───
s = rep(s, `<div id="tip"></div>`,
`<div id="scrim"></div>
<div id="menu">
  <div class="mh"><b>Kits</b><input id="mfilter" placeholder="Type to filter"><button class="k dark" id="mclose">Close</button></div>
  <div id="mlist"></div>
  <div class="mf">
    <button class="k dark" id="msave">Save as&hellip;</button>
    <button class="k dark" id="mopen">Open&hellip;</button>
    <button class="k dark" id="mscan">Rescan</button>
  </div>
</div>
<div id="tip"></div>`, "menu markup");

// ──────────────────────────────────────────────────────────────────── JS ───
s = rep(s, `/* ─── 4 · kit stepper ────────────────────────────────────────────────────── */`,
`/* ─── 3c · the kit browser ───────────────────────────────────────────────────
   Built on document.body, deliberately: the deck carries the scale-to-fit
   transform, so a menu positioned from a rect measured inside it lands in the
   wrong place. (The Photo-Synth lesson.)                                     */
let CATS = [], KITCAT = [], PATCHES = [], patchDir = "";

function closeMenu() {
  $("#menu").classList.remove("on");
  $("#scrim").classList.remove("on");
}

function openMenu() {
  const m = $("#menu"), box = $("#kitbox").getBoundingClientRect();
  m.classList.add("on");
  $("#scrim").classList.add("on");
  //  measured AFTER it is shown, or the height is zero and it hangs off screen
  const h = Math.min(m.offsetHeight, Math.round(window.innerHeight * 0.82));
  m.style.left = clamp(box.left, 8, window.innerWidth - m.offsetWidth - 8) + "px";
  m.style.top = clamp(box.bottom + 6, 8, Math.max(8, window.innerHeight - h - 8)) + "px";
  $("#mfilter").value = "";
  drawMenu("");
  setTimeout(() => { try { $("#mfilter").focus(); } catch (e) {} }, 30);
}

function drawMenu(filter) {
  const list = $("#mlist");
  list.innerHTML = "";
  const f = (filter || "").trim().toUpperCase();

  const row = (no, name, tag, cur, fn) => {
    const r = el("div", "row" + (cur ? " cur" : ""), list);
    el("div", "no", r).textContent = no;
    el("div", "nm", r).textContent = name;
    if (tag) el("div", "tag", r).textContent = tag;
    r.addEventListener("click", () => { fn(); closeMenu(); });
  };

  //  the two hundred, under their categories
  let shownAny = false;
  CATS.forEach((cat, ci) => {
    const mine = [];
    for (let i = 0; i < KITS.length; i++)
      if (KITCAT[i] === ci && (!f || KITS[i].indexOf(f) >= 0 || cat.toUpperCase().indexOf(f) >= 0))
        mine.push(i);
    if (!mine.length) return;
    shownAny = true;
    el("div", "grp", list).textContent = cat + "  \\u00b7  " + mine.length;
    mine.forEach(i => row(String(i + 1).padStart(3, "0"), KITS[i], "", i === kitIdx,
                          () => loadKit(i)));
  });

  //  then the saved patches, grouped by their subfolder
  const pats = PATCHES.filter(p => !f || p.n.toUpperCase().indexOf(f) >= 0);
  if (pats.length) {
    const subs = {};
    pats.forEach(p => { const k = p.sub || "PATCHES"; (subs[k] = subs[k] || []).push(p); });
    Object.keys(subs).sort().forEach(k => {
      el("div", "grp", list).textContent = k.toUpperCase() + "  \\u00b7  " + subs[k].length;
      subs[k].forEach(p => row("", p.n, "FILE", false, () => NB.send({ k: "presetLoad", path: p.p })));
    });
    shownAny = true;
  }

  if (!shownAny) {
    const r = el("div", "row", list);
    el("div", "nm", r).textContent = "Nothing matches \\u201c" + filter + "\\u201d";
  }
}

$("#kitbox").addEventListener("click", ev => {
  //  the arrows keep working as arrows
  if (ev.target.closest("#kprev") || ev.target.closest("#knext")) return;
  openMenu();
});
$("#scrim").addEventListener("click", closeMenu);
$("#mclose").addEventListener("click", closeMenu);
$("#mfilter").addEventListener("input", e => drawMenu(e.target.value));
$("#mfilter").addEventListener("keydown", e => { if (e.key === "Escape") closeMenu(); });
$("#msave").addEventListener("click", () => { closeMenu(); NB.send({ k: "save" }); });
$("#mopen").addEventListener("click", () => { closeMenu(); NB.send({ k: "open" }); });
$("#mscan").addEventListener("click", () => NB.send({ k: "presetScan" }));
window.addEventListener("keydown", e => { if (e.key === "Escape") closeMenu(); });

/* ─── 4 · kit stepper ────────────────────────────────────────────────────── */`, "browser js");

// the initial state carries the categories, and the patch list arrives separately
s = rep(s, `  if (typeof p.kit === "number") kitIdx = p.kit;`,
`  if (typeof p.kit === "number") kitIdx = p.kit;
  CATS = p.cats || [];
  KITCAT = (p.kits || []).map(x => (typeof x === "string" ? 0 : (x.c | 0)));
  NB.send({ k: "presetScan" });`, "cats");

s = rep(s, `NB.on("patches", p => { say((p.files || []).length + " KITS IN " + (p.dir || "")); });`,
`NB.on("patches", p => {
  PATCHES = p.files || [];
  patchDir = p.dir || "";
  if ($("#menu").classList.contains("on")) drawMenu($("#mfilter").value);
});`, "patches event");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("kit browser added");
