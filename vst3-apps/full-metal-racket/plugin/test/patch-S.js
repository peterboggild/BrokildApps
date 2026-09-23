// THE BOTTOM WAS CUT OFF IN A HOST.
//
// Measured in the standalone at 1800x830 there is no overflow at all — the
// strip fits its space exactly. So the cut is not a layout problem, it is a
// FIT problem: the page was scaling to window.innerWidth/innerHeight, and in
// an embedded WebView inside a plugin window those are not reliable. The host
// resizes the view without the window firing `resize`, so the deck kept a
// scale computed for a size it no longer had, and the overflow went under the
// bottom edge.
//
// Three belts:
//   * measure the ELEMENT we actually draw into, not the window;
//   * a ResizeObserver, which fires for host-driven resizes that never reach
//     the window's resize event;
//   * and a few delayed re-fits after boot, for the hosts that size the view
//     after the first paint.
//
// The fixed aspect ratio stays — it stops the window being dragged into a
// silly shape — but nothing now DEPENDS on the window honouring it.
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

s = rep(s, `function fitDeck() {
  const s = Math.min(window.innerWidth / 1800, window.innerHeight / 830);
  $("#deck").style.transform = "scale(" + s.toFixed(4) + ")";
  requestAnimationFrame(() => { if (WIDG.volume) WIDG.volume.forEach(x => x.draw()); });
}
window.addEventListener("resize", fitDeck);
fitDeck();`,
`let lastFit = "";
function fitDeck() {
  /*  Measure the element we draw into, not the window. In a plugin the
      WebView is embedded, and window.innerHeight has been seen reporting a
      size the view no longer has — which left the deck scaled too large and
      its bottom under the edge of the panel. */
  const fit = $("#fit");
  const w = fit.clientWidth || document.documentElement.clientWidth || window.innerWidth;
  const h = fit.clientHeight || document.documentElement.clientHeight || window.innerHeight;
  if (w < 40 || h < 40) return;                       // mid-resize; wait for the next one
  const sc = Math.min(w / 1800, h / 830);
  const key = w + "x" + h;
  $("#deck").style.transform = "scale(" + sc.toFixed(4) + ")";
  if (key !== lastFit) {
    lastFit = key;
    //  the faders position their caps from a measured height, so they have to
    //  be redrawn after a resize or the cap sits at the old place
    requestAnimationFrame(() => {
      if (WIDG.volume) WIDG.volume.forEach(x => x.draw());
      Object.keys(WIDG).forEach(id => { if (id.endsWith("_level")) WIDG[id].forEach(x => x.draw()); });
    });
  }
}
window.addEventListener("resize", fitDeck);
/*  A ResizeObserver as well: a host that resizes the view directly does not
    always produce a window resize event, and that is exactly the case that
    left the panel scaled for a window it no longer had. */
try { new ResizeObserver(fitDeck).observe($("#fit")); } catch (e) {}
//  and a few late ones, for hosts that size the view after the first paint
[60, 200, 600, 1500].forEach(t => setTimeout(fitDeck, t));
fitDeck();`, "fitDeck");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("fit measures the element, observes resizes, and re-fits after boot");
