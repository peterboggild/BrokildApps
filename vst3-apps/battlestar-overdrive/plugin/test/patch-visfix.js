/*  forcePeak for the probe, a readable AUTOREFILL value, and a nova with more
 *  presence. Exact-count anchors; nothing written if any misses. */
const fs = require("fs");
const p = "C:/Users/peter/b/BattlestarOverdrive/Source/ui/ui.html";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(find, sub) {
  const f = Array.isArray(find) ? find.join(NL) : find;
  const r = Array.isArray(sub) ? sub.join(NL) : sub;
  const n = s.split(f).length - 1;
  if (n !== 1) { miss.push(n + " matches: " + f.split(NL)[0].trim().slice(0, 50)); return; }
  s = s.replace(f, r);
}

// --- the probe needs to hold the peak against the 30 Hz meter stream -------
rep("  forceFuel: null, forceEmpty: null, forceFizzle: null,",
    "  forceFuel: null, forceEmpty: null, forceFizzle: null, forcePeak: null,");
rep("  if (f.forceFizzle !== null) meter.fizzle = f.forceFizzle;",
    "  if (f.forceFizzle !== null) meter.fizzle = f.forceFizzle;\n  if (f.forcePeak   !== null) meter.peak   = f.forcePeak;");

// --- a switch should read as a switch, not as "100" ------------------------
rep([
"  const s = SPEC[id];",
"  return (s ? s.name : id.toUpperCase()) + \"   \" + Math.round(v * 100);"
], [
"  if (id === \"autorefill\") return v > 0.5 ? \"AUTOREFILL   ON\" : \"AUTOREFILL   OFF\";",
"  const s = SPEC[id];",
"  return (s ? s.name : id.toUpperCase()) + \"   \" + Math.round(v * 100);"
]);

// --- the detonation needs to carry across the tube -------------------------
rep([
"    const period = 3.4;",
"    const t = ((now * 0.001) % period) / period;        // 0..1",
"    const flash = Math.exp(-t * 14);                    // the initial glare"
], [
"    const period = 3.4;",
"    const t = ((now * 0.001) % period) / period;        // 0..1",
"    const flash = Math.exp(-t * 9);                     // the initial glare"
]);
rep([
"    ctx.strokeStyle = \"rgba(255,\" + (170 + 70*ring).toFixed(0) + \",\" + (90*ring).toFixed(0) + \",\" + (0.55*ring*ring).toFixed(3) + \")\";",
"    ctx.lineWidth = Math.max(1.5, (h/494) * (2 + 16 * ring * ring));"
], [
"    ctx.strokeStyle = \"rgba(255,\" + (170 + 70*ring).toFixed(0) + \",\" + (90*ring).toFixed(0) + \",\" + (0.85*ring*ring).toFixed(3) + \")\";",
"    ctx.lineWidth = Math.max(2.0, (h/494) * (3 + 24 * ring * ring));"
]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(p, s);
console.log("forcePeak added, AUTOREFILL reads ON/OFF, nova strengthened");
