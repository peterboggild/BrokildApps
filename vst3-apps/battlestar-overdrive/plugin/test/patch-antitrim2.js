/*  The ANTITHRUST trim must compensate the LOSSES only, never the width.
 *  Exact-count anchors; nothing written if any misses. */
const fs = require("fs");
const p = "C:/Users/peter/b/BattlestarOverdrive/Source/Engine.cpp";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function cut(startLine, endLine, sub) {
  const i = s.indexOf(startLine);
  const j = s.indexOf(endLine, i);
  if (i < 0 || j < 0) { miss.push("bounds: " + startLine.trim().slice(0, 50)); return; }
  s = s.slice(0, i) + sub.join(NL) + s.slice(j + endLine.length);
}

cut(
"/*  ANTITHRUST's own level compensation, measured the same way as the engines'.",
"float Engine::antiTrimFor (float anti) const",
[
"/*  ANTITHRUST's level compensation - for what it LOSES, and nothing else.",
"",
"    Peter: \"antithrust changes the volume quite a lot... when antithrust is on",
"    full, the level drops when mix is turned up - not what a drive should do.\"",
"    Right, and it is ANTITHRUST at fault rather than MIX: MIX is a correct",
"    linear blend and behaves at zero, and the engines are already matched.",
"",
"    What is compensated is the CHOKE (makeup, applied at the choke itself) and",
"    this lowpass. What is NOT compensated is the width, and that distinction is",
"    the whole point: adding decorrelated side genuinely raises total energy, so",
"    a trim that normalises total energy divides the MID down with it - and the",
"    mid IS the sound. Doing that measured -28.9 dB at full, far worse than the",
"    -22.0 dB being complained about. In mid/side the mid is preserved by",
"    construction; any global scalar undoes exactly that. */",
"void Engine::buildAntiTrim()",
"{",
"    constexpr int N = 16384;",
"    OnePole lp;",
"    Rng r;",
"",
"    for (int i = 0; i < ANTI_PTS; ++i)",
"    {",
"        const float a = (float) i / (float) (ANTI_PTS - 1);",
"        lp.setHz (lerpf (20000.0f, 7000.0f, a * a), sr);",
"        lp.clear();",
"        r.s = 0x51ed2701u;                       // seeded: the table is deterministic",
"",
"        double accIn = 0.0, accOut = 0.0;",
"        for (int n = 0; n < N; ++n)",
"        {",
"            const float x = r.bi() * 0.25f;      // broadband reference",
"            const float y = lp.process (x);",
"            if (n >= N / 4) { accIn += (double) x * x; accOut += (double) y * y; }",
"        }",
"        const float g = (float) std::sqrt (accOut / std::max (1.0e-12, accIn));",
"        antiTrim[(size_t) i] = clampf (1.0f / std::max (1.0e-4f, g), 1.0f, 3.0f);",
"    }",
"}",
"",
"float Engine::antiTrimFor (float anti) const"
]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(p, s);
console.log("antiTrim now compensates losses only");
