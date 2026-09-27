// Level the factory presets BY MEASUREMENT, never by ear-guess:
//
//   kstest --levels > levels.txt
//   node tools/level-presets.js levels.txt
//
// Target: -10 dB RMS over the first 150 ms (the part of a kick the ear
// judges loudness by), but never a peak above -1 dBFS, so a clean preset
// never touches the output ceiling. Re-runnable: it reads each preset's
// CURRENT level and adds the correction, so running it twice converges.
const fs = require("fs");
const path = require("path");
const TARGET_RMS = -10.0, MAX_PEAK = -1.0;

const file = path.resolve(__dirname, "../engine/ks_presets.cpp");
let s = fs.readFileSync(file, "utf8");
const lines = fs.readFileSync(process.argv[2], "utf8").split(/\r?\n/);

// name -> values array, from the preset table
const table = {};
for (const m of s.matchAll(/\{ "([^"]+)",\s*"[A-Z]+",\s*"(?:[^"\\]|\\.)*",\s*(v\w+|nullptr)/g)) table[m[1]] = m[2];

let changed = 0;
for (const ln of lines) {
    const m = ln.match(/^(.+?)\s+peak\s+(-?[\d.]+) dBFS\s+rms\(0-150ms\)\s+(-?[\d.]+) dB/);
    if (!m) continue;
    const name = m[1].trim(), peak = +m[2], rms = +m[3];
    let v = table[name];
    if (!v) { console.log("no preset called", name); process.exit(1); }
    if (v === "nullptr") {
        // Init has no value list yet: give it one
        s = s.replace("#undef KS_VALUES", 'KS_VALUES (vInit, {"level",0})\n\n#undef KS_VALUES');
        s = s.replace('"A plain electronic kick to start from.", nullptr, 0 }', '"A plain electronic kick to start from.", vInit, KS_N (vInit) }');
        v = table[name] = "vInit";
    }
    const delta = Math.min(TARGET_RMS - rms, MAX_PEAK - peak);
    const re = new RegExp("KS_VALUES \\(" + v + ",\\s*");
    const at = s.search(re);
    if (at < 0) { console.log("cannot find", v); process.exit(1); }
    const end = s.indexOf(")\n", at);
    const block = s.slice(at, end);
    const cur = block.match(/\{"level",(-?[\d.]+)f?\}/);
    const now = cur ? +cur[1] : 0;
    const next = Math.round((now + delta) * 10) / 10;
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
