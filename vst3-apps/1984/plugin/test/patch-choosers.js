// The choosers. Peter: "the HTML-like menu controls break the illusion". The
// list boxes in the slider rows become chrome SLIDE SWITCHES in a groove with
// a printed legend and an LED per position (the Prophet's rocker-and-LED, the
// CS-80's slide switches); the header's patch <select> becomes a display
// window with a hand-built bank chart. Nothing moves: every switch keeps the
// cell its list box had, and the legend elements keep the class names (.seg b)
// so anything addressing them by class still does.
"use strict";
const fs = require("fs");
const files = {};
function load(k, p) { const raw = fs.readFileSync(p, "utf8"); files[k] = { p, crlf: raw.indexOf("\r\n") >= 0, s: raw.replace(/\r\n/g, "\n"), n: 0 }; }
function edit(k, from, to, count = 1) {
  const f = files[k]; const parts = f.s.split(from);
  if (parts.length - 1 !== count) { console.error("ANCHOR MISS in " + k + " (" + (parts.length - 1) + "): " + from.slice(0, 100)); process.exit(1); }
  f.s = parts.join(to); f.n++;
}
function span(k, from, to, repl) {
  const f = files[k]; const a = f.s.indexOf(from);
  if (a < 0 || f.s.indexOf(from, a + 1) >= 0) { console.error("SPAN START MISS in " + k + ": " + from.slice(0, 80)); process.exit(1); }
  const b = f.s.indexOf(to, a + from.length);
  if (b < 0 || f.s.indexOf(to, b + 1) >= 0) { console.error("SPAN END MISS in " + k + ": " + to.slice(0, 80)); process.exit(1); }
  f.s = f.s.slice(0, a) + repl + f.s.slice(b + to.length); f.n++;
}
const root = "C:/Users/peter/b/Nineteen84/";
load("u", root + "Source/ui/ui.html");

/* ---- 1. CSS: the list box and the select become a slide switch ---------- */
span("u",
`/* ---- segmented (vertical, in the slider rows) ------------------------- */`,
`select.n84 optgroup{background:#0e0e11;color:var(--amber);font-style:normal}`,
`/* ---- slide switch (the list choosers in the slider rows) ---------------
   A chrome slide switch in a groove, a printed legend beside it and an LED
   per position. The legends keep the old list box's class (.seg b). */
.seg{position:relative;width:100%;margin-top:2px}
.sslot{position:absolute;left:5px;top:6px;bottom:6px;width:8px;border-radius:4px;cursor:ns-resize;
  background:linear-gradient(90deg,#000,#1a1a1e 30%,#0a0a0c 70%,#000);
  box-shadow:inset 0 0 4px #000, 0 0 0 1px rgba(210,210,214,.16), 0 1px 0 rgba(255,255,255,.05)}
html.d-track .sslot{background-image:var(--decal-track);background-size:100% 100%;box-shadow:none}
.sknob{position:absolute;left:1px;width:16px;height:15px;border-radius:3px;pointer-events:none;
  transform:translateY(-50%);transition:top .07s ease-out;
  background:linear-gradient(180deg,#f6f6f8,#d2d2d6 28%,#8f8f97 60%,#45454b);
  box-shadow:0 2px 4px rgba(0,0,0,.7), inset 0 0 0 1px rgba(0,0,0,.45), inset 0 1px 0 rgba(255,255,255,.7)}
.sknob::after{content:"";position:absolute;left:3px;right:3px;top:6px;height:2px;border-radius:1px;background:rgba(0,0,0,.5)}
.seg b{position:absolute;left:19px;right:0;display:flex;align-items:center;gap:4px;transform:translateY(-50%);
  font-size:7.4px;font-weight:700;letter-spacing:.06em;line-height:8px;color:var(--cream2);cursor:pointer;
  text-align:left;text-shadow:0 1px 0 rgba(0,0,0,.9)}
.seg b::before{content:"";flex:0 0 4px;width:4px;height:4px;border-radius:50%;
  background:radial-gradient(50% 50% at 50% 42%,#2a1806,#140c04 70%,#0a0603);box-shadow:inset 0 0 0 1px rgba(255,178,74,.18)}
.seg b:hover{color:var(--cream)}
.seg b.on{color:var(--cream)}
.seg b.on::before{background:radial-gradient(50% 50% at 50% 42%,#ffd79a,#ff9c1e 55%,#8a4c06);
  box-shadow:0 0 5px rgba(255,150,30,.8), inset 0 0 0 1px rgba(255,220,160,.6)}
.ctl.seg .seg{margin-bottom:6px}
.ctl.seg .lab{font-size:7px;letter-spacing:.17em;color:rgba(232,220,192,.42);padding-top:0}`);

/* ---- 2. CSS: the patch window and the bank chart ------------------------ */
edit("u", `#patchSel{width:294px}`,
`#patchSel{display:none}                 /* the value store; the window below is what you see */
.pwin{position:relative;width:294px;display:flex;align-items:center;gap:8px;padding:0 9px 0 11px;cursor:pointer;
  border-radius:4px;background:linear-gradient(180deg,#0a0c0b,#050706 60%,#070908);
  box-shadow:inset 0 0 14px rgba(255,178,74,.07), inset 0 2px 6px rgba(0,0,0,.9), 0 0 0 1px rgba(210,210,214,.16), 0 0 0 3px #0c0c0f;
  font-family:var(--mono);font-size:10.5px;letter-spacing:.09em;color:var(--amber);text-shadow:0 0 7px rgba(255,178,74,.5);
  white-space:nowrap;overflow:hidden;user-select:none}
.pwin::after{content:"";position:absolute;inset:0;pointer-events:none;border-radius:4px;
  background:linear-gradient(180deg,rgba(255,255,255,.07),rgba(255,255,255,0) 45%),
             repeating-linear-gradient(180deg,rgba(0,0,0,.2) 0 1px,rgba(0,0,0,0) 1px 3px)}
.pwin em{font-style:normal;color:#a8712c;flex:0 0 auto}
.pwin span{flex:1;overflow:hidden;text-overflow:ellipsis}
.pwin i{font-style:normal;color:#a8712c;font-size:8px;flex:0 0 auto}
.pwin:hover,.pwin.open{box-shadow:inset 0 0 14px rgba(255,178,74,.12), inset 0 2px 6px rgba(0,0,0,.9), 0 0 0 1px rgba(255,178,74,.45), 0 0 0 3px #0c0c0f}
/* the bank chart lives on the body: the deck carries a transform, and a menu
   positioned from getBoundingClientRect() inside it lands in the wrong place */
#pmenu{position:fixed;z-index:60;display:flex;flex-wrap:wrap;align-items:flex-start;gap:0 1.4em;padding:.8em 1.1em .9em 1.1em;
  border-radius:5px;background:linear-gradient(180deg,#0a0c0b,#050706);
  box-shadow:0 0 0 1px rgba(210,210,214,.18), 0 0 0 4px #0c0c0f, 0 14px 30px rgba(0,0,0,.8), inset 0 0 26px rgba(255,178,74,.06);
  font-family:var(--mono);color:var(--amber);letter-spacing:.06em;max-width:calc(100vw - 16px);max-height:calc(100vh - 16px);overflow:auto}
#pmenu .pc{display:flex;flex-direction:column;min-width:10em;padding-bottom:.4em}
#pmenu .ph{color:#c98a3a;font-weight:700;letter-spacing:.24em;font-size:.78em;padding:.2em .5em .5em .5em;
  border-bottom:1px solid rgba(255,178,74,.2);margin-bottom:.35em;white-space:nowrap}
#pmenu .pi{padding:.18em .5em;white-space:nowrap;cursor:pointer;border-radius:2px;color:#d79a45}
#pmenu .pi em{font-style:normal;color:#8a5f2a;margin-right:.6em}
#pmenu .pi:hover{background:rgba(255,178,74,.12);color:var(--cream)}
#pmenu .pi.on{color:var(--amber);text-shadow:0 0 7px rgba(255,178,74,.6)}
#pmenu .pi.on em{color:var(--amber)}`);

/* ---- 3. markup: the window in front of the (hidden) select --------------- */
edit("u", `        <select class="n84" id="patchSel" data-id="patch"></select>`,
`        <div class="pwin" id="pwin" data-id="patch"><em id="pwinnum">--</em><span id="pwintxt">NO PATCH</span><i>&#9662;</i></div>
        <select class="n84" id="patchSel"></select>`);

/* ---- 4. every LIST in a slider row is a switch ---------------------------- */
edit("u", `    return (NAMES[id] && NAMES[id].length > 5) ? "select" : "seg";`, `    return "seg";`);

/* ---- 5. the builder ------------------------------------------------------- */
span("u",
`  } else if (t === "seg") {
    var sg = el("div", "seg", c);`,
`    c._sel = s;
  }
`,
`  } else if (t === "seg") {
    var sg = el("div", "seg", c);
    var H = (ctx.style === "sliders") ? ctx.trk : 56;
    sg.style.height = H + "px";
    var names = NAMES[id] || [];
    var nPos = Math.max(2, names.length || (maxOf(id) + 1));
    var pad = 14;                                   /* slot inset plus half a knob */
    var pos = function (i) { return pad + (H - 2 * pad) * (nPos > 1 ? i / (nPos - 1) : 0); };
    var slot = el("div", "sslot", sg);
    var knob = el("div", "sknob", sg);
    names.forEach(function (nm, i) {
      var b = el("b", "", sg); b.textContent = nm; b.dataset.i = String(i);
      b.style.top = pos(i).toFixed(1) + "px";
      b.addEventListener("click", function (ev) { ev.stopPropagation(); setP(id, i); });
    });
    var pick = function (e) {                       /* the detent nearest the pointer */
      var r = sg.getBoundingClientRect();
      var yy = r.height > 0 ? (e.clientY - r.top) / r.height * H : 0;
      setP(id, clamp(Math.round((yy - pad) / ((H - 2 * pad) / (nPos - 1))), 0, nPos - 1));
    };
    var sdown = false;
    slot.addEventListener("pointerdown", function (e) {
      if (e.button !== 0) return;
      sdown = true; try { slot.setPointerCapture(e.pointerId); } catch (x) {}
      pick(e); showTip(c, true); e.preventDefault(); e.stopPropagation();
    });
    slot.addEventListener("pointermove", function (e) { if (sdown) pick(e); });
    var sup = function (e) {
      if (!sdown) return; sdown = false;
      try { slot.releasePointerCapture(e.pointerId); } catch (x) {}
      if (!c.matches(":hover")) hideTip();
    };
    slot.addEventListener("pointerup", sup); slot.addEventListener("pointercancel", sup);
    c._seg = sg; c._sknob = knob; c._spos = pos;
  }
`);

/* ---- 6. refresh ---------------------------------------------------------- */
edit("u",
`    } else if (t === "seg") {
      var i = Math.round(v);
      Array.prototype.forEach.call(c._seg.children, function (b, j) { b.classList.toggle("on", j === i); });
    } else if (t === "select") {
      c._sel.value = String(Math.round(v));
    }`,
`    } else if (t === "seg") {
      var i = Math.round(v);
      c._sknob.style.top = c._spos(i).toFixed(1) + "px";
      Array.prototype.forEach.call(c._seg.querySelectorAll("b"), function (b, j) { b.classList.toggle("on", j === i); });
    }`);

/* ---- 7. the window follows the dial -------------------------------------- */
edit("u",
`function syncPatchSel() {
  var s = $("#patchSel"); if (!s) return;
  var i = Math.round(V.patch || 0);
  if (String(i) !== s.value) s.value = String(i);
}`,
`function syncPatchSel() {
  var s = $("#patchSel"); if (!s) return;
  var i = Math.round(V.patch || 0);
  if (String(i) !== s.value) s.value = String(i);
  var p = null;
  for (var k = 0; k < PATCHES.length; k++) { var nn = PATCHES[k].n !== undefined ? PATCHES[k].n : k; if (nn === i) { p = PATCHES[k]; break; } }
  var num = $("#pwinnum"), txt = $("#pwintxt");
  if (num) num.textContent = (i < 9 ? "0" : "") + (i + 1);
  if (txt) txt.textContent = p ? (p.name || ("PATCH " + (i + 1))) : (PATCHES.length ? "PATCH " + (i + 1) : "NO PATCH");
  if (pmenu) Array.prototype.forEach.call(pmenu.querySelectorAll(".pi"), function (it) { it.classList.toggle("on", +it.dataset.n === i); });
}`);

/* ---- 8. the bank chart --------------------------------------------------- */
edit("u",
`  V.patch = i; syncPatchSel();
  NB.send({ k: "patch", i: i });
}
`,
`  V.patch = i; syncPatchSel();
  NB.send({ k: "patch", i: i });
}

/* ---- the bank chart: a hand-built menu on the body. The deck carries a
   transform, so anything positioned inside it from a client rect lands in
   the wrong place; on the body the rect is the truth. --------------------- */
var pmenu = null, DECK_S = 1;
function openPatchMenu() {
  closePatchMenu();
  var w = $("#pwin"); if (!w || !PATCHES.length) return;
  pmenu = el("div", "", document.body); pmenu.id = "pmenu";
  pmenu.style.fontSize = Math.max(10, 11 * DECK_S).toFixed(1) + "px";
  var cur = Math.round(V.patch || 0), col = null, cat = null;
  PATCHES.forEach(function (p, i) {
    var n = p.n !== undefined ? p.n : i, c = p.cat || "PATCHES";
    if (c !== cat) { cat = c; col = el("div", "pc", pmenu); el("div", "ph", col).textContent = c; }
    var it = el("div", "pi" + (n === cur ? " on" : ""), col); it.dataset.n = String(n);
    el("em", "", it).textContent = (n < 9 ? "0" : "") + (n + 1);
    it.appendChild(document.createTextNode(p.name || ("PATCH " + (n + 1))));
    it.addEventListener("click", function () { loadPatch(n); closePatchMenu(); });
  });
  var r = w.getBoundingClientRect(), m = pmenu.getBoundingClientRect();
  var x = r.left, y = r.bottom + 6;
  if (x + m.width > innerWidth - 8) x = innerWidth - 8 - m.width;
  if (x < 8) x = 8;
  if (y + m.height > innerHeight - 8) y = Math.max(8, r.top - 6 - m.height);
  pmenu.style.left = Math.round(x) + "px"; pmenu.style.top = Math.round(y) + "px";
  w.classList.add("open");
}
function closePatchMenu() {
  if (pmenu && pmenu.parentNode) pmenu.parentNode.removeChild(pmenu);
  pmenu = null;
  var w = $("#pwin"); if (w) w.classList.remove("open");
}
document.addEventListener("pointerdown", function (e) {
  if (!pmenu) return;
  var w = $("#pwin");
  if (pmenu.contains(e.target) || (w && w.contains(e.target))) return;   /* a press inside must not eat the click it was armed for */
  closePatchMenu();
}, true);
addEventListener("keydown", function (e) { if (pmenu && e.key === "Escape") closePatchMenu(); });
`);

/* ---- 9. the header binds the window, the select stays the store ---------- */
edit("u",
`  var ps = $("#patchSel");
  ps.setAttribute("data-hint", hintFor("patch"));
  ps.addEventListener("mouseenter", function () { showTip(ps, false); });
  ps.addEventListener("mouseleave", hideTip);
  ps.addEventListener("change", function () { loadPatch(+this.value); });`,
`  var ps = $("#patchSel");
  ps.addEventListener("change", function () { loadPatch(+this.value); });
  var pw = $("#pwin");
  pw.setAttribute("data-hint", hintFor("patch"));
  pw.addEventListener("mouseenter", function () { showTip(pw, false); });
  pw.addEventListener("mouseleave", hideTip);
  pw.addEventListener("click", function () { if (pmenu) closePatchMenu(); else openPatchMenu(); });`);

/* ---- 10. the deck scale, for the chart's type size ----------------------- */
edit("u",
`  d.style.transform = "translate(" + ((innerWidth - 1600 * s) / 2) + "px," + ((innerHeight - 980 * s) / 2) + "px) scale(" + s + ")";`,
`  d.style.transform = "translate(" + ((innerWidth - 1600 * s) / 2) + "px," + ((innerHeight - 980 * s) / 2) + "px) scale(" + s + ")";
  DECK_S = s; closePatchMenu();`);

for (const k in files) { const f = files[k]; fs.writeFileSync(f.p, f.crlf ? f.s.replace(/\n/g, "\r\n") : f.s); console.log(k + ": " + f.n + " edits"); }
