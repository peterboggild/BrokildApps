/*  BWFX — THE OVERLAY SCROLLS ITSELF.

    Peter, 2026-09-02, on B2311.67: "the BWFX field in 67 cannot be scrolled,
    so its hard to reach the FX in the lowest part of the panel".

    Measured on the live plug-in, which settled it in one reading: the veil's
    scrollHeight is 1108 against a clientHeight of 880 — 228 px of rack below
    the fold — and setting scrollTop from script moves it fine. So the veil is
    scrollable and the wheel was never reaching it. Dispatching a synthetic
    wheel inside the veil showed why:

        host window wheel handlers saw the event: 1 (defaultPrevented=true)

    B2311.67 registers `window.addEventListener("wheel", ...)` and calls
    preventDefault on every wheel event anywhere, to drag the body through the
    cut. It has no way to know a modal is open over it, and it should not need
    one: an overlay that a host page can accidentally disable is the overlay's
    problem, not the host's. The same trap is waiting in any synth whose panel
    takes the wheel globally.

    So the overlay now scrolls ITSELF, in the capture phase on the veil — which
    runs before the event can reach the window's bubble-phase listeners — and
    stops the event there. It no longer depends on the host leaving the default
    action alone, because it no longer uses the default action.
*/
const fs = require('fs');
const P = 'C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js';
let s = fs.readFileSync(P, 'utf8');
const NL = s.indexOf(String.fromCharCode(13,10)) >= 0
  ? String.fromCharCode(13,10) : String.fromCharCode(10);
const miss = [];
const J = a => a.join(NL);
function rep (find, into) {
  const n = s.split(find).length - 1;
  if (n !== 1) { miss.push('[' + n + 'x] ' + find.split(NL)[0].slice(0, 64)); return; }
  s = s.replace(find, into);
}

rep(J([
'    veil.addEventListener("wheel", function (ev) {',
'      if (armed < 0) return;',
'      var el = ev.target && ev.target.closest ? ev.target.closest("[data-bwfx-dest]") : null;',
'      if (!el) return;',
'      if (wheelDepth(el.getAttribute("data-bwfx-dest"), ev.deltaY < 0 ? 1 : -1, ev.shiftKey))',
'      { ev.preventDefault(); ev.stopPropagation(); }',
'    }, { capture: true, passive: false });']),
    J([
'    veil.addEventListener("wheel", function (ev) {',
'      /*  A macro is armed and the wheel is over one of its destinations: the',
'          wheel sets the depth, as it always has. */',
'      if (armed >= 0) {',
'        var el = ev.target && ev.target.closest ? ev.target.closest("[data-bwfx-dest]") : null;',
'        if (el && wheelDepth(el.getAttribute("data-bwfx-dest"), ev.deltaY < 0 ? 1 : -1, ev.shiftKey))',
'        { ev.preventDefault(); ev.stopPropagation(); return; }',
'      }',
'      /*  OTHERWISE THE OVERLAY SCROLLS, AND IT DOES IT ITSELF.',
'',
'          Relying on the browser to scroll the veil means relying on the host',
'          page not to have called preventDefault first, and B2311.67 takes the',
'          wheel on window to drag its body through the cut — so the rack could',
'          not be scrolled at all and the lowest pedals were unreachable. This',
'          listener is in the CAPTURE phase on the veil, so it runs before the',
'          event can bubble to the page, and it moves the veil by hand rather',
'          than asking for a default action the page may already have refused.',
'',
'          deltaMode matters: a wheel reports pixels, lines or pages depending',
'          on the device, and treating lines as pixels makes a mouse wheel move',
'          the rack by three pixels a notch. */',
'      var d = ev.deltaY;',
'      if (ev.deltaMode === 1) d *= 16;',
'      else if (ev.deltaMode === 2) d *= veil.clientHeight;',
'      veil.scrollTop += d;',
'      ev.preventDefault();',
'      ev.stopPropagation();',
'    }, { capture: true, passive: false });']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log('bwfx-rack.js: the overlay scrolls itself');
