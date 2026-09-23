/*  ANTITHRUST becomes a width control: header side.
 *  Exact-count anchors; nothing written if any misses. */
const fs = require("fs");
const p = "C:/Users/peter/b/BattlestarOverdrive/Source/Engine.h";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(find, sub) {
  const f = find.join(NL);
  const n = s.split(f).length - 1;
  if (n !== 1) { miss.push(n + " matches: " + find[0].trim().slice(0, 55)); return; }
  s = s.replace(f, sub.join(NL));
}

// --- a Schroeder allpass, for decorrelation without colour -----------------
rep([
"//==============================================================================",
"enum EngineId"
], [
"/*  Schroeder allpass. Its magnitude response is exactly flat - only the phase",
"    moves - which is what makes it the right tool for widening: it decorrelates",
"    without colouring, where a comb would leave notches all over the tone. */",
"struct APDelay",
"{",
"    Delay line;",
"    float c = 0.5f;",
"    int   D = 64;",
"",
"    void setup (int d, float coef) { D = std::max (1, d); line.setMaxSamples (D + 8); c = coef; }",
"    void clear() { line.clear(); }",
"",
"    inline float process (float x)",
"    {",
"        const float v = line.readInt (D + 1);",
"        const float y = -c * x + v;",
"        line.write (flushDenorm (x + c * y));",
"        return y;",
"    }",
"};",
"",
"//==============================================================================",
"enum EngineId"
]);

// --- the members -----------------------------------------------------------
rep([
"    // --- ANTITHRUST --------------------------------------------------------",
"    std::array<Delay, 2> comb;",
"    std::array<float, 2> combFbZ { 0.0f, 0.0f };",
"    float combDelaySm = 0.0f;",
"    float chokeEnv = 0.0f;",
"    std::array<OnePole, 2> chokeLp;"
], [
"    // --- ANTITHRUST: width, and the choke ----------------------------------",
"    /*  Worked in MID/SIDE. The knob ADDS decorrelated side derived from the",
"        mid, and never touches the mid itself, so the mono sum is exactly the",
"        mono sum it always was - no cancellation at any setting, whatever the",
"        source. It also means an incoming stereo image is preserved and widened",
"        rather than replaced, which a sum-to-mono-and-respread widener cannot",
"        do. At zero the side is untouched and the path is an exact identity. */",
"    static constexpr int NAP = 5;",
"    std::array<APDelay, NAP> widthAp;",
"    Delay widthComb;",
"    Biquad sideShelf;",
"    float widthDelaySm = 0.0f;",
"    float chokeEnv = 0.0f;",
"    std::array<OnePole, 2> chokeLp;"
]);

// --- peak meter, for the clipping indicator --------------------------------
rep([
"    float rmsIn = 0.0f;",
"    float meterIn = 0.0f, meterOut = 0.0f, meterDrive = 0.0f;"
], [
"    float rmsIn = 0.0f;",
"    float meterIn = 0.0f, meterOut = 0.0f, meterDrive = 0.0f;",
"    // Fast attack, slow release: what the panel needs to show the tube",
"    // overloading, which is a peak event and invisible in an average.",
"    float meterPeak = 0.0f;"
]);

rep([
"    float outLevel()   const { return meterOut; }"
], [
"    float outLevel()   const { return meterOut; }",
"    float outPeak()    const { return meterPeak; }"
]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(p, s);
console.log("Engine.h patched for width");
