// A SETTINGS window: button finish, and panel light.
//
// Both are things about how the machine LOOKS rather than how it sounds, so
// neither belongs in a patch or in the host's automation. They live in
// localStorage, per instance of the browser profile — the same place
// Photo-Synth keeps its LIGHT slider — so a project loads with the sound it
// was saved with and the panel the way this machine is set up.
//
// The metal finish is drawn rather than photographed: there is no metal-key
// decal in the set yet, and drawing one convincingly is cheaper than another
// round of generation. If it earns its place, it gets ordered properly.
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

// ───────────────────────────────────────────────────────────────────── CSS ──
s = rep(s, `/* ── the kit browser ──`,
`/* ── settings: how the machine looks ────────────────────────────────────── */
#setwin{position:fixed;z-index:72;display:none;flex-direction:column;width:430px;
  border-radius:4px;overflow:hidden;background:linear-gradient(180deg,#241c10,#141009);
  border:1px solid #000;box-shadow:0 22px 60px -14px #000,inset 0 1px 0 #ffffff18}
#setwin.on{display:flex}
#setwin .mh{flex:none;display:flex;align-items:center;gap:10px;padding:11px 14px;
  border-bottom:1px solid #000;background:linear-gradient(180deg,#33291a,#221a0f)}
#setwin .mh b{flex:1;font:700 17px/1 var(--f-disp);letter-spacing:.04em;
  text-transform:uppercase;color:var(--cream)}
#setwin .mh .k{height:28px;padding:0 12px;font-size:11px}
#setwin .sect{padding:14px}
#setwin .sect + .sect{border-top:1px solid #000}
#setwin .st{font:700 9px/1 var(--f-lab);letter-spacing:.22em;text-transform:uppercase;
  color:#a08e6c;margin-bottom:9px}
#setwin .opts{display:flex;gap:6px}
#setwin .opts .k{flex:1;height:38px;font-size:11px}
#setwin .why{margin-top:9px;font:700 9px/1.6 var(--f-lab);letter-spacing:.1em;
  text-transform:uppercase;color:#7a6a4d}
#setwin .lightrow{display:flex;align-items:center;gap:12px}
#setwin .lightrow input{flex:1;accent-color:#d8721a}
#setwin .lightrow span{width:44px;text-align:right;font:700 12px/1 var(--f-mono);color:var(--amber)}

/*  A brushed-aluminium key, drawn. The decal wins by default; this only
    applies when the metal finish is chosen, so it has to out-specify it. */
body.keys-metal .k.dark{
  background:linear-gradient(180deg,#d9dcde,#9aa0a4 46%,#7d8286 54%,#b6babd)!important;
  border:1px solid #63686b!important;color:#1a1d20!important;
  box-shadow:inset 0 1px 0 #ffffffcc,inset 0 -1px 0 #00000040,0 2px 3px #0005!important}
body.keys-metal .k.dark:hover{background:linear-gradient(180deg,#eaeced,#a9afb3 46%,#8b9094 54%,#c4c8cb)!important}
body.keys-metal .k.dark.on,body.keys-metal .k.dark:active{
  background:linear-gradient(180deg,#e07a4a,#a8330f 52%,#8d2409)!important;
  border-color:#6d1c06!important;color:#fff4e8!important}
body.keys-metal .lopts .o{
  background:linear-gradient(180deg,#c8cbcd,#8f9599)!important;border-color:#63686b!important;color:#3a3d40!important}
body.keys-metal .lopts .o b{color:#14171a!important}
body.keys-metal .mute{background:linear-gradient(180deg,#cfd2d4,#8f9599)!important;color:#22262a!important}
body.keys-metal .mute.on{background:linear-gradient(180deg,#e07a4a,#a8330f)!important;color:#fff4e8!important}

/*  Panel light. A gamma lift, not a brightness multiply: multiplying does
    almost nothing to a dark panel and flattens the highlights, where gamma
    lifts the shadows and leaves the top end alone. (The Mars Wars lesson.) */
#deck{filter:var(--lift,none)}

/* ── the kit browser ──`, "settings css");

// ────────────────────────────────────────────────────────────────── markup ──
s = rep(s, `<div id="scrim"></div>`,
`<div id="scrim"></div>
<svg width="0" height="0" style="position:absolute" aria-hidden="true"><filter id="f-lift" color-interpolation-filters="sRGB">
  <feComponentTransfer><feFuncR type="gamma" exponent="1" offset="0"/><feFuncG type="gamma" exponent="1" offset="0"/><feFuncB type="gamma" exponent="1" offset="0"/></feComponentTransfer>
</filter></svg>
<div id="setwin">
  <div class="mh"><b>Settings</b><button class="k dark" id="setclose">Close</button></div>
  <div class="sect">
    <div class="st">Button finish</div>
    <div class="opts">
      <button class="k dark" id="fin-black">Black</button>
      <button class="k dark" id="fin-metal">Metal</button>
    </div>
    <div class="why">Applies to the rail, the lane fields and the mutes</div>
  </div>
  <div class="sect">
    <div class="st">Panel light</div>
    <div class="lightrow"><input type="range" id="setlight" min="0" max="100" value="0"><span id="setlightv">0</span></div>
    <div class="why">Lifts the shadows without flattening the highlights</div>
  </div>
  <div class="sect">
    <div class="why" id="setnote">Saved on this machine, not in the patch &mdash; a project keeps the sound it was saved with</div>
  </div>
</div>`, "settings markup");

// ────────────────────────────────────────────────────────────────────── JS ──
s = rep(s, `/* ─── 4 · kit stepper ────────────────────────────────────────────────────── */`,
`/* ─── 3d · settings ──────────────────────────────────────────────────────── */
const SET = { finish: "black", light: 0 };

function loadSettings() {
  //  a private browsing profile, or a host that sandboxes storage, throws on
  //  read as well as on write — so the defaults have to survive that
  try {
    const raw = window.localStorage.getItem("fmr.settings");
    if (raw) { const v = JSON.parse(raw); if (v.finish) SET.finish = v.finish; if (typeof v.light === "number") SET.light = v.light; }
  } catch (e) {}
}
function saveSettings() {
  try { window.localStorage.setItem("fmr.settings", JSON.stringify(SET)); } catch (e) {}
}

function applySettings() {
  document.body.classList.toggle("keys-metal", SET.finish === "metal");
  const b = $("#fin-black"), m = $("#fin-metal");
  if (b) b.classList.toggle("on", SET.finish === "black");
  if (m) m.classList.toggle("on", SET.finish === "metal");

  const k = clamp(SET.light, 0, 100) / 100;
  const f = document.querySelector("#f-lift feComponentTransfer");
  if (f) {
    //  exponent below 1 lifts the shadows; the small offset stops the very
    //  darkest pixels staying pinned at black
    const exp = (1 / (1 + 1.15 * k)).toFixed(4), off = (0.045 * k).toFixed(4);
    ["feFuncR", "feFuncG", "feFuncB"].forEach(t => {
      const e = f.querySelector(t);
      if (e) { e.setAttribute("exponent", exp); e.setAttribute("offset", off); }
    });
  }
  //  at zero the filter is removed entirely, so the default costs nothing
  $("#deck").style.setProperty("--lift", k > 0.001 ? "url(#f-lift)" : "none");
  const sl = $("#setlight"), sv = $("#setlightv");
  if (sl) sl.value = String(Math.round(SET.light));
  if (sv) sv.textContent = String(Math.round(SET.light));
}

function openSettings() {
  const w = $("#setwin"), box = $("#rail").getBoundingClientRect();
  w.classList.add("on");
  $("#scrim").classList.add("on");
  w.style.left = clamp(box.right + 8, 8, window.innerWidth - w.offsetWidth - 8) + "px";
  w.style.top = clamp(box.top, 8, Math.max(8, window.innerHeight - w.offsetHeight - 8)) + "px";
}
function closeSettings() { $("#setwin").classList.remove("on"); $("#scrim").classList.remove("on"); }

$("#setclose").addEventListener("click", closeSettings);
$("#fin-black").addEventListener("click", () => { SET.finish = "black"; saveSettings(); applySettings(); });
$("#fin-metal").addEventListener("click", () => { SET.finish = "metal"; saveSettings(); applySettings(); });
$("#setlight").addEventListener("input", e => { SET.light = parseInt(e.target.value, 10) || 0; applySettings(); });
$("#setlight").addEventListener("change", saveSettings);

/* ─── 4 · kit stepper ────────────────────────────────────────────────────── */`, "settings js");

// the scrim closes settings too
s = rep(s, `$("#scrim").addEventListener("click", closeMenu);`,
         `$("#scrim").addEventListener("click", () => { closeMenu(); closeSettings(); });`, "scrim");
s = rep(s, `window.addEventListener("keydown", e => { if (e.key === "Escape") closeMenu(); });`,
         `window.addEventListener("keydown", e => { if (e.key === "Escape") { closeMenu(); closeSettings(); } });`, "escape");

// a SETUP key in the rail
s = rep(s, `  rbtn("Save", () => NB.send({ k: "save" }));`,
`  rbtn("Setup", openSettings);
  rbtn("Save", () => NB.send({ k: "save" }));`, "rail setup");

// and apply what was saved, once the panel exists
s = rep(s, `  buildSeq();
  showPage(0);`,
`  buildSeq();
  showPage(0);
  loadSettings();
  applySettings();`, "apply on build");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("settings window: black or metal buttons, and panel light");
