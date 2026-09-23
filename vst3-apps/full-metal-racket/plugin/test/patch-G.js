// KIT TUNE now tunes the KIT.
//
// It was shifting the three toms' normalised TUNE value and nothing else — so
// a control named for the whole machine moved a quarter of it. Measured: toms
// +16 semitones across the range, kick / snares / hats / cymbals exactly zero.
// It is a global transpose now, in real semitones, applied to every channel's
// frequency and to the sympathetic shells so THE WEB follows the kit.
//
// Displayed in semitones as well: a transpose shown as a percentage is not
// information anyone can act on.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(p.split("/").pop() + ": " + t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/Source/";

const wh = edit(R + "Engine.h", [
["enum PKind { KP_PCT = 0, KP_SW, KP_HZ, KP_MS, KP_LIST, KP_BIPOL, KP_VOL, KP_INT };",
 "enum PKind { KP_PCT = 0, KP_SW, KP_HZ, KP_MS, KP_LIST, KP_BIPOL, KP_VOL, KP_INT, KP_SEMI };",
 "kind enum"]
]);

const wc = edit(R + "Engine.cpp", [
// the global transpose, in semitones
[`    // ---- tuning ------------------------------------------------------------
    float tune = P[CP_TUNE];
    if (d.fam == FAM_TOM) tune = clamp01 (tune + (p.g[GP_KITTUNE] - 0.5f) * 0.5f);`,
`    // ---- tuning ------------------------------------------------------------
    float tune = P[CP_TUNE];
    /*  KIT TUNE is a transpose of the WHOLE kit, in semitones. It used to
        shift the three toms' normalised tune and nothing else, which made a
        control named for the machine move a quarter of it. */
    const float kitSemis = (p.g[GP_KITTUNE] - 0.5f) * 24.0f;`, "tune shift"],

[`    float f0 = xmap (tune, d.tuneLo, d.tuneHi) * tuneTol;
    if (wmActive) f0 *= std::pow (2.0f, wmDet / 1200.0f);`,
`    float f0 = xmap (tune, d.tuneLo, d.tuneHi) * tuneTol;
    if (kitSemis != 0.0f) f0 *= std::pow (2.0f, kitSemis / 12.0f);
    if (wmActive) f0 *= std::pow (2.0f, wmDet / 1200.0f);`, "apply transpose"],

// the sympathetic shells follow the kit, or THE WEB detunes from it
[`        const ChanDef& d = CHANS[c];
        float f = xmap (p.ch[c][CP_TUNE], d.tuneLo, d.tuneHi);
        if (d.fam == FAM_TOM) f = xmap (clamp01 (p.ch[c][CP_TUNE] + (p.g[GP_KITTUNE] - 0.5f) * 0.5f), d.tuneLo, d.tuneHi);
        symp[(size_t) c].setF (clampf (f, 20.0f, fsv * 0.45f), 9.0f, fsv);`,
`        const ChanDef& d = CHANS[c];
        float f = xmap (p.ch[c][CP_TUNE], d.tuneLo, d.tuneHi)
                * std::pow (2.0f, (p.g[GP_KITTUNE] - 0.5f) * 24.0f / 12.0f);
        symp[(size_t) c].setF (clampf (f, 20.0f, fsv * 0.45f), 9.0f, fsv);`, "symp follows"],

[`                    case GP_KITTUNE: r.kind = KP_BIPOL; r.def = 0.5f; break;`,
 `                    case GP_KITTUNE: r.kind = KP_SEMI; r.lo = 24.0f; r.def = 0.5f; break;`, "kittune kind"]
]);

const wp = edit(R + "PluginProcessor.cpp", [
[`            case KP_BIPOL: { const float c = (v - 0.5f) * 200.0f; return (c >= 0 ? "+" : "") + juce::String (c, 0) + " %"; }`,
 `            case KP_BIPOL: { const float c = (v - 0.5f) * 200.0f; return (c >= 0 ? "+" : "") + juce::String (c, 0) + " %"; }
            case KP_SEMI:  { const float st = (v - 0.5f) * s.lo; return (st >= 0 ? "+" : "") + juce::String (st, 1) + " semi"; }`, "fmt semi"]
]);

const wu = edit(R + "ui/ui.html", [
["const KIND = { PCT: 0, SW: 1, HZ: 2, MS: 3, LIST: 4, BIPOL: 5, VOL: 6, INT: 7 };",
 "const KIND = { PCT: 0, SW: 1, HZ: 2, MS: 3, LIST: 4, BIPOL: 5, VOL: 6, INT: 7, SEMI: 8 };", "ui kind"],
[`    case KIND.BIPOL: { const c = (v - 0.5) * 200; return (c >= 0 ? "+" : "") + c.toFixed(0) + " %"; }`,
 `    case KIND.BIPOL: { const c = (v - 0.5) * 200; return (c >= 0 ? "+" : "") + c.toFixed(0) + " %"; }
    case KIND.SEMI:  { const st = (v - 0.5) * s.lo; return (st >= 0 ? "+" : "") + st.toFixed(1) + " semi"; }`, "ui fmt"]
]);

const wt = edit("C:/Users/peter/b/FullMetalRacket/test/test.cpp", [
[`        ok (std::fabs (semis) > 1.0,
            (std::string ("KIT TUNE moves ") + channelName (c)).c_str(), semis, 1.0);`,
`        //  a transpose, so EVERY channel moves, and by the same amount
        ok (std::fabs (semis) > 20.0,
            (std::string ("KIT TUNE transposes ") + channelName (c)).c_str(), semis, 20.0);
        ok (std::fabs (semis - 24.0) < 2.0,
            (std::string ("...by the full two octaves: ") + channelName (c)).c_str(), semis, 24.0);`, "bench expectation"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wh(); wc(); wp(); wu(); wt();
console.log("KIT TUNE transposes the whole kit, in semitones");
