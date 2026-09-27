/*  ANTITHRUST becomes a width control: engine side.
 *  Exact-count anchors; nothing written if any misses. */
const fs = require("fs");
const p = "C:/Users/peter/b/BattlestarOverdrive/Source/Engine.cpp";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(find, sub) {
  const f = find.join(NL);
  const n = s.split(f).length - 1;
  if (n !== 1) { miss.push(n + " matches: " + find[0].trim().slice(0, 55)); return; }
  s = s.replace(f, sub.join(NL));
}

// ---------------------------------------------------------------- 1. trim table
rep(["float Engine::trimFor (int engine, float thrust) const"], [
"/*  ANTITHRUST's own level compensation, measured the same way as the engines'.",
"",
"    Peter: \"antithrust changes the volume quite a lot... when antithrust is on",
"    full, the level drops when mix is turned up - not what a drive should do.\"",
"    Right, and it is ANTITHRUST at fault rather than MIX: MIX is a correct",
"    linear blend and behaves at zero, and the engines are already matched.",
"    ANTITHRUST was simply the one block in the chain with no compensation.",
"",
"    Measured on a MONO reference, because that is the worst case: with L == R",
"    the side starts at zero and everything the knob adds is new energy. */",
"void Engine::buildAntiTrim()",
"{",
"    constexpr int N = 16384;",
"",
"    std::array<APDelay, NAP> ap;",
"    Delay wc; wc.setMaxSamples ((int) (0.020 * sr) + 64);",
"    Biquad shelf;",
"    Rng r;",
"",
"    const int base[NAP] = { 47, 89, 131, 191, 257 };",
"    const float coef[NAP] = { 0.62f, 0.55f, 0.48f, 0.41f, 0.35f };",
"",
"    for (int i = 0; i < ANTI_PTS; ++i)",
"    {",
"        const float a = (float) i / (float) (ANTI_PTS - 1);",
"        const float width = 1.35f * a;",
"        const float ms = lerpf (14.0f, 3.0f, a);",
"        const float dly = std::max (1.0f, ms * 0.001f * (float) sr);",
"",
"        for (int k = 0; k < NAP; ++k)",
"        {",
"            ap[(size_t) k].setup ((int) (base[k] * sr / 48000.0), coef[k]);",
"            ap[(size_t) k].clear();",
"        }",
"        wc.clear();",
"        shelf.setHighShelf (2600.0f, sr, 0.70f, 5.0f);",
"        shelf.clear();",
"        r.s = 0x51ed2701u;                       // seeded: the table is deterministic",
"",
"        double accIn = 0.0, accOut = 0.0;",
"        for (int n = 0; n < N; ++n)",
"        {",
"            const float x = r.bi() * 0.25f;      // mono broadband reference",
"            const float m = x;                   // L == R, so mid is the signal",
"            float h = m;",
"            for (int k = 0; k < NAP; ++k) h = ap[(size_t) k].process (h);",
"            wc.write (m);",
"            h = h * 0.68f + wc.readFrac (dly) * 0.32f;",
"            h = shelf.process (h);",
"            const float sd = width * h;          // side starts at 0 on a mono source",
"            const float l = m + sd, rr = m - sd;",
"            if (n >= N / 4)",
"            {",
"                accIn  += 2.0 * (double) x * x;",
"                accOut += (double) l * l + (double) rr * rr;",
"            }",
"        }",
"        const float g = (float) std::sqrt (accOut / std::max (1.0e-12, accIn));",
"        antiTrim[(size_t) i] = clampf (1.0f / std::max (1.0e-4f, g), 0.25f, 4.0f);",
"    }",
"}",
"",
"float Engine::antiTrimFor (float anti) const",
"{",
"    const float t = clampf (anti, 0.0f, 1.0f) * (ANTI_PTS - 1);",
"    const int i = std::min ((int) t, ANTI_PTS - 2);",
"    return lerpf (antiTrim[(size_t) i], antiTrim[(size_t) (i + 1)], t - (float) i);",
"}",
"",
"float Engine::trimFor (int engine, float thrust) const"
]);

// ---------------------------------------------------------------- 2. prepare
rep([
"    for (auto& d : comb) d.setMaxSamples ((int) (0.020 * sr) + 64);"
], [
"    {",
"        const int base[NAP] = { 47, 89, 131, 191, 257 };",
"        const float coef[NAP] = { 0.62f, 0.55f, 0.48f, 0.41f, 0.35f };",
"        for (int k = 0; k < NAP; ++k)",
"            widthAp[(size_t) k].setup ((int) (base[k] * sr / 48000.0), coef[k]);",
"    }",
"    widthComb.setMaxSamples ((int) (0.020 * sr) + 64);",
"    sideShelf.setHighShelf (2600.0f, sr, 0.70f, 5.0f);"
]);

rep(["    buildTrimTable();"], ["    buildTrimTable();", "    buildAntiTrim();"]);

// ---------------------------------------------------------------- 3. reset
rep(["    for (auto& d : comb) d.clear();"], [
"    for (auto& a : widthAp) a.clear();",
"    widthComb.clear();",
"    sideShelf.clear();"
]);

rep([
"    combFbZ = { 0.0f, 0.0f };",
"    tapeFbZ = { 0.0f, 0.0f };"
], [
"    tapeFbZ = { 0.0f, 0.0f };"
]);
rep(["    combDelaySm = 0.0f;"], ["    widthDelaySm = 0.0f;", "    meterPeak = 0.0f;"]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(p, s);
console.log("Engine.cpp: trim table, prepare and reset patched");
