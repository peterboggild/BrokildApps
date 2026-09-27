/*  Panel round 7 — Peter's tooltip round.

    "i would like the tooltips to have a different shade (than just black) -
     they are hard to see. Also, i would like them to be switched off by
     default as they block the controls. Make a 'Tooltips' button at the top,
     that is highlighted when its selected. Also, make CTRL hover a shortcut."

    Four changes:
      * the tip is a lit plate, not a black rectangle: a graphite gradient with
        an oxide border and an amber hairline at the top, so it reads AS a
        floating label over a dark panel instead of a hole in it;
      * TOOLTIPS is off on a fresh instance (the setting is remembered per
        machine, so a player who turns them on keeps them);
      * the header button says TOOLTIPS and lights when they are on;
      * holding CTRL shows the tip for whatever the pointer is over, whether
        or not they are switched on - and releasing CTRL takes it away again.

    A drag always shows the tip, however the switch is set: during a drag the
    tip is the value readout, and it is placed beside the control, never over
    it.
*/
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/Source/ui/ui.html";
let s = fs.readFileSync(p, "utf8");

const edits = [
  // ---- 1. the look: a lit plate, not a black rectangle
  [`#tip{position:fixed;z-index:60;max-width:284px;padding:6px 8px;border:1px solid var(--hair);
  border-radius:2px;background:rgba(10,12,14,.97);box-shadow:0 6px 22px rgba(0,0,0,.6);
  opacity:0;pointer-events:none;transition:opacity .09s}
#tip.show{opacity:1}
#tip .tn{font:600 8.5px/12px var(--sans);letter-spacing:.11em;color:var(--bone);text-transform:uppercase}
#tip .tv{font:400 11px/15px var(--mono);color:var(--amber)}
#tip .tt{font:400 9px/13px var(--sans);color:var(--bone2);margin-top:3px}`,
   `/*  A lit plate rather than a black rectangle: against a graphite deck a
    near-black tip reads as a hole. It is lifted two steps, edged in oxide,
    and carries an amber hairline along the top. */
#tip{position:fixed;z-index:60;max-width:300px;padding:7px 9px 8px;
  border:1px solid var(--oxide);border-radius:2px;
  background:linear-gradient(180deg,#333a43 0%,#262c34 34%,#1d2228 100%);
  box-shadow:0 8px 26px rgba(0,0,0,.72),inset 0 1px 0 rgba(255,255,255,.07);
  opacity:0;pointer-events:none;transition:opacity .09s}
#tip::before{content:"";position:absolute;left:0;right:0;top:0;height:1px;
  background:linear-gradient(90deg,var(--amber),rgba(217,154,58,0))}
#tip.show{opacity:1}
#tip .tn{font:600 8.5px/12px var(--sans);letter-spacing:.11em;color:#fbf7ee;text-transform:uppercase}
#tip .tv{font:400 12px/16px var(--mono);color:#ffc266}
#tip .tt{font:400 9.5px/13.5px var(--sans);color:#cfc9bb;margin-top:4px}
#tip .tk{font:400 7.5px/11px var(--mono);letter-spacing:.09em;color:var(--oxide2);margin-top:4px;
  border-top:1px solid rgba(231,227,217,.10);padding-top:3px}`],

  // ---- 2. off by default, remembered, and CTRL as the shortcut
  [`var tipEl = null, tipFor = null, HINTS_ON = true;
function showTip(c, dragging) {
  if (!HINTS_ON && !dragging) { hideTip(); return; }`,
   `/*  Tooltips are OFF on a fresh instance: they sit beside a control, and with
    the deck this dense that is often over the NEXT control. Holding CTRL shows
    one without switching them on; the switch itself is remembered per machine. */
var tipEl = null, tipFor = null, HOVERC = null, CTRLHOVER = false;
var HINTS_ON = (function () {
  try { return localStorage.getItem("tty.tooltips") === "1"; } catch (e) { return false; }
})();
function setTooltips(on) {
  HINTS_ON = !!on;
  try { localStorage.setItem("tty.tooltips", HINTS_ON ? "1" : "0"); } catch (e) {}
  var b = $("#b-hints"); if (b) b.classList.toggle("on", HINTS_ON);
  if (!HINTS_ON && tipFor && !tipFor.classList.contains("drag")) hideTip();
  else if (HINTS_ON && HOVERC) showTip(HOVERC, false);
}
function showTip(c, dragging) {
  if (!HINTS_ON && !CTRLHOVER && !dragging) { hideTip(); return; }`],

  // the CTRL line at the foot of the tip, so the shortcut is discoverable
  [`  el("div", "tt", tipEl, c.getAttribute("data-hint") || "");
  tipEl.classList.add("show");`,
   `  el("div", "tt", tipEl, c.getAttribute("data-hint") || "");
  if (!HINTS_ON) el("div", "tk", tipEl, "HOLD CTRL FOR THIS \\u00b7 TOOLTIPS TURNS THEM ON");
  tipEl.classList.add("show");`],

  // remember what the pointer is over, so CTRL can raise the tip on the spot
  [`  c.addEventListener("mouseenter", function () { showTip(c, false); });
  c.addEventListener("mouseleave", function () { if (!c.classList.contains("drag")) hideTip(); });`,
   `  c.addEventListener("mouseenter", function (e) { HOVERC = c; CTRLHOVER = !!(e && e.ctrlKey); showTip(c, false); });
  c.addEventListener("mouseleave", function () { if (HOVERC === c) HOVERC = null; if (!c.classList.contains("drag")) hideTip(); });`],

  // ---- 3. the button
  [`        <div class="btn" id="b-hints" title="show the hint beside every control">HINTS</div>`,
   `        <div class="btn" id="b-hints" title="show the tooltip beside every control (or hold CTRL)">TOOLTIPS</div>`],
  [`  $("#b-hints").addEventListener("click", function () { HINTS_ON = !HINTS_ON; $("#b-hints").classList.toggle("on", HINTS_ON); });
  $("#b-hints").classList.add("on");`,
   `  $("#b-hints").addEventListener("click", function () { setTooltips(!HINTS_ON); });
  setTooltips(HINTS_ON);`],

  // ---- 4. CTRL held: raise the tip for whatever is under the pointer
  [`document.addEventListener("keydown", function (e) {
  if (e.target && (e.target.tagName === "INPUT" || e.target.isContentEditable)) return;
  if (e.repeat) return;
  var k = e.key.toLowerCase();`,
   `/*  CTRL is the tooltip shortcut: hold it and whatever the pointer is over
    explains itself, without turning them on for good. */
document.addEventListener("keydown", function (e) {
  if (e.key === "Control" && !CTRLHOVER) { CTRLHOVER = true; if (HOVERC) showTip(HOVERC, false); }
  if (e.target && (e.target.tagName === "INPUT" || e.target.isContentEditable)) return;
  if (e.repeat) return;
  var k = e.key.toLowerCase();`],
  [`document.addEventListener("keyup", function (e) {
  if (e.target && (e.target.tagName === "INPUT" || e.target.isContentEditable)) return;
  var k = e.key.toLowerCase(); if (QW[k] !== undefined) noteOff((OCT + 1) * 12 + QW[k]);
});`,
   `document.addEventListener("keyup", function (e) {
  if (e.key === "Control") {
    CTRLHOVER = false;
    if (!HINTS_ON && tipFor && !tipFor.classList.contains("drag")) hideTip();
  }
  if (e.target && (e.target.tagName === "INPUT" || e.target.isContentEditable)) return;
  var k = e.key.toLowerCase(); if (QW[k] !== undefined) noteOff((OCT + 1) * 12 + QW[k]);
});
/*  A window that loses focus never sees the keyup, so CTRL would stay stuck on. */
window.addEventListener("blur", function () {
  CTRLHOVER = false;
  if (!HINTS_ON && tipFor && !tipFor.classList.contains("drag")) hideTip();
});`],
];

let miss = [];
for (const [a] of edits) { const n = s.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 64).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(p, s);
console.log("ui.html patched (" + edits.length + " edits)");
