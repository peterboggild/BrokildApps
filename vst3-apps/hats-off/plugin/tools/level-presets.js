// Level the factory presets BY MEASUREMENT, never by ear-guess:
//
//   hotest --levels > levels.txt
//   node tools/level-presets.js levels.txt
//
// Target: -15 dB RMS over the first 80 ms (the part of a hat the ear
// judges loudness by), but never a peak above -1 dBFS, so a clean preset
// never touches the output ceiling. Re-runnable: it reads each preset's
// CURRENT level and adds the correction, so running it twice converges.
const fs = require("fs");
const path = require("path");
const TARGET_RMS = -17.0, MAX_PEAK = -1.0;

const file = path.resolve(__dirname, "../engine/ho_presets.cpp");
let s = fs.readFileSync(file, "utf8");
const lines = fs.readFileSync(process.argv[2], "utf8").split(/\r?\n/);

// name -> values array, from the preset table
const table = {};
for (const m of s.matchAll(/\{ "([^"]+)",\s*"[A-Z]+",\s*"(?:[^"\\]|\\.)*",\s*(v\w+|nullptr)/g)) table[m[1]] = m[2];

let changed = 0;
for (const ln of lines) {
    const m = ln.match(/^(.+?)\s+peak\s+(-?[\d.]+) dBFS\s+rms\(0-80ms\)\s+(-?[\d.]+) dB/);
    if (!m) continue;
    const name = m[1].trim(), peak = +m[2], rms = +m[3];
    let v = table[name];
    if (!v) { console.log("no preset called", name); process.exit(1); }
    if (v === "nullptr") {
        // Init has no value list yet: give it one
        s = s.replace("#undef HO_VALUES", 'HO_VALUES (vInit, {"level",0})\n\n#undef HO_VALUES');
        s = s.replace('"A plain electronic kick to start from.", nullptr, 0 }', '"A plain electronic kick to start from.", vInit, HO_N (vInit) }');
        v = table[name] = "vInit";
    }
    const delta = Math.min(TARGET_RMS - rms, MAX_PEAK - peak);
    const re = new RegExp("HO_VALUES \\(" + v + ",\\s*");
    const at = s.search(re);
    if (at < 0) { console.log("cannot find", v); process.exit(1); }
    const end = s.indexOf(")\n", at);
    const block = s.slice(at, end);
    const cur = block.match(/\{"level",(-?[\d.]+)f?\}/);
    const now = cur ? +cur[1] : 0;
    //  never write a value LEVEL cannot hold (-24..+6 dB): a source too quiet
    //  at unity is the engine's to fix, not the preset's
    const raw = Math.round((now + delta) * 10) / 10;
    const next = Math.max(-24, Math.min(6, raw));
    if (raw !== next) console.log("  CLAMPED", name, raw, "->", next);
    const lit = '{"level",' + (Number.isInteger(next) ? next : next.toFixed(1) + "f") + "}";
    let nb;
    if (cur) nb = block.replace(cur[0], lit);
    else nb = block.replace(re, (x) => x + lit + ",");
    s = s.slice(0, at) + nb + s.slice(end);
    console.log(name.padEnd(20), "level", now.toFixed(1), "->", next.toFixed(1));
    ++changed;
}
fs.writeFileSync(file, s);
console.log(changed, "presets levelled");
