/*  Move the parameter readout ONTO the glass in blue holo, and stop the panel
 *  explaining itself.
 *
 *  NB: ui.html contains a real U+2009 thin space, not the escape sequence, so
 *  no anchor here may contain one - an earlier version searched for the six
 *  characters backslash-u-2-0-0-9 and matched nothing. */
const fs = require("fs");
const p = "C:/Users/peter/b/BattlestarOverdrive/Source/ui/ui.html";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(find, sub) {
  const f = Array.isArray(find) ? find.join(NL) : find;
  const r = Array.isArray(sub) ? sub.join(NL) : sub;
  const n = s.split(f).length - 1;
  if (n !== 1) { miss.push(n + " matches: " + f.split(NL)[0].trim().slice(0, 52)); return; }
  s = s.replace(f, r);
}

// --- holo() gains a size, so a readout can be small and low -----------------
rep("function holo(now, w, h, title, sub, rgb, alpha, glow, scale, yFrac){",
    "function holo(now, w, h, title, sub, rgb, alpha, glow, scale, yFrac, sizeFrac){");
rep("  const size = h * 0.155;",
    "  const size = h * (sizeFrac || 0.155);");
rep("    nctx.font = \"500 \" + (h*0.050).toFixed(1) + \"px 'Futura','Century Gothic',sans-serif\";",
    "    nctx.font = \"500 \" + (h*(sizeFrac ? sizeFrac*0.33 : 0.050)).toFixed(1) + \"px 'Futura','Century Gothic',sans-serif\";");
rep(", 0, h * 0.125);",
    ", 0, h * (sizeFrac ? sizeFrac*0.80 : 0.125));");

// --- a predicate the draw stage can ask, plus the readout itself -----------
rep([
"function drawEngineName(now, w, h){",
"  const t = (now - engineShownAt) / 1000;",
"  if (t < 0 || t > 2.05) return;"
], [
"function nameShowing(now){",
"  const t = (now - engineShownAt) / 1000;",
"  return t >= 0 && t <= 2.05;",
"}",
"",
"/*  The parameter readout, ON THE GLASS in blue holo.",
"",
"    It used to be an amber DOM label sitting on the METAL just below the bezel",
"    - HTML text lying on a photograph, while everything else the instrument",
"    says is drawn inside the tube. That was the inconsistency. It goes through",
"    the same holo() as the engine name, so it picks up the same scan bands and",
"    the same glow for free. */",
"let readoutText = \"\", readoutAt = -1e9;",
"function showReadout(txt){ readoutText = txt; readoutAt = performance.now(); }",
"",
"function drawReadout(now, w, h){",
"  const t = (now - readoutAt) / 1000;",
"  if (!readoutText || t < 0 || t > 1.45) return;",
"  const HOLD = 1.05, FADE = 0.40;",
"  let alpha = 1, glow = 1;",
"  if (t > HOLD){",
"    const u = Math.min(1, (t - HOLD) / FADE);",
"    alpha = 1 - u * u;",
"    glow = 1 + u * 4.0;",
"  }",
"  holo(now, w, h, readoutText, \"\", [120, 215, 255], alpha, glow, 1.0, 0.82, 0.072);",
"}",
"",
"function drawEngineName(now, w, h){",
"  const t = (now - engineShownAt) / 1000;",
"  if (t < 0 || t > 2.05) return;"
]);

// --- route the value display to the glass ---------------------------------
rep([
"function showValue(id){",
"  const h = document.getElementById(\"hint\");",
"  h.textContent = valueText(id);",
"  h.classList.add(\"show\");",
"  clearTimeout(hintTimer);",
"}",
"function hideValueSoon(){",
"  clearTimeout(hintTimer);",
"  hintTimer = setTimeout(() => document.getElementById(\"hint\").classList.remove(\"show\"), 1100);",
"}"
], [
"function showValue(id){",
"  // ENGINE already says its own name in big letters; a second line",
"  // underneath repeating it would just be noise.",
"  if (id !== \"engine\") showReadout(valueText(id));",
"}",
"function hideValueSoon(){ /* the readout fades itself */ }"
]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(p, s);
console.log("readout moved onto the glass; explanations removed");
