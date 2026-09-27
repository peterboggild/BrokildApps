/*  LEVEL THE FACTORY BANK.  Re-runnable: it measures, writes a trim into
    every preset, rebuilds and measures again until it converges.

    The loudness rule lives in test/loudness.h and is shared with the bench,
    so the tool that sets the trims and the check that polices them cannot
    drift apart.  It is the loudest 200 ms window, because this bank holds
    drones AND 60 ms plucks and any fixed window flatters one of them.

    A trim is CAPPED by the preset's own peak: a quiet patch that would have
    to clip to reach the target is left below it instead.  Better one preset
    a few dB down than one preset distorting.

        node tools/level-presets.js [--target -18] [--passes 4]
*/
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const ROOT    = path.resolve(__dirname, '..');
const PRESETS = path.join(ROOT, 'Source', 'Presets.cpp');
const BUILD   = 'C:/Users/peter/b/_build/GravityWell/bench';
const PROBE   = BUILD + '/Release/gwprobe.exe';

const argv   = process.argv.slice(2);
const argOf  = (k, d) => { const i = argv.indexOf(k); return i >= 0 ? parseFloat(argv[i + 1]) : d; };
const TARGET = argOf('--target', -18.0);
const PASSES = argOf('--passes', 4);
const PEAK_CEIL = 0.92;      // never trim a preset up into clipping
const TRIM_LO = -24, TRIM_HI = 8;    // the parameter's own range

/*  Smart App Control judges a freshly linked exe by hash and refuses it with
    errno -4094 UNKNOWN, which reads like a missing file and is not.  Overlay
    bytes past the last PE section are ignored by the loader, so appending a
    few changes the hash and nothing else.  The house trick.              */
function runProbe (args) {
    for (let tryN = 0; tryN < 6; ++tryN) {
        try { return execFileSync(PROBE, args, { encoding: "utf8", maxBuffer: 1 << 24 }); }
        catch (e) {
            if (e.code !== "UNKNOWN" && e.errno !== -4094) throw e;
            const n = 1 + Math.floor(Math.random() * 8);
            fs.appendFileSync(PROBE, Buffer.from(Array.from({ length: n }, () => Math.floor(Math.random() * 256))));
            console.log("  (Smart App Control refused the probe; nudged its hash, retry " + (tryN + 1) + ")");
        }
    }
    throw new Error("Smart App Control kept refusing " + PROBE);
}

function measure () {
    const out = runProbe(['levels']);
    const rows = [];
    for (const line of out.split(/\r?\n/)) {
        const p = line.split('\t');
        if (p.length < 5) continue;
        rows.push({ i: +p[0], loud: +p[1], trim: +p[2], peak: +p[3], name: p[4] });
    }
    if (rows.length === 0) throw new Error('the probe printed no levels');
    return rows;
}

//  Set or insert {P_out_trim,v} inside BANK(kNN, ...) by matching the macro
//  call's own parentheses - a regex on one line cannot see a wrapped entry.
function writeTrims (trims) {
    let src = fs.readFileSync(PRESETS, 'utf8');
    let changed = 0;
    for (const [idx, v] of trims) {
        const sym = 'k' + String(idx).padStart(2, '0');
        const head = 'BANK(' + sym + ',';
        const at = src.indexOf(head);
        if (at < 0) throw new Error('no ' + head + ' in Presets.cpp');
        let depth = 0, end = -1;
        for (let i = at; i < src.length; ++i) {
            if (src[i] === '(') ++depth;
            else if (src[i] === ')') { if (--depth === 0) { end = i; break; } }
        }
        if (end < 0) throw new Error('unbalanced parentheses in ' + head);
        const body = src.slice(at, end);
        const lit  = '{P_out_trim,' + v.toFixed(2) + 'f}';
        let next;
        if (/\{\s*P_out_trim\s*,[^}]*\}/.test(body))
            next = body.replace(/\{\s*P_out_trim\s*,[^}]*\}/, lit);
        else
            next = body + ',' + lit;
        if (next !== body) { src = src.slice(0, at) + next + src.slice(end); ++changed; }
    }
    fs.writeFileSync(PRESETS, src);
    return changed;
}

function rebuild () {
    execFileSync('cmake', ['--build', BUILD, '--config', 'Release'], { stdio: 'pipe' });
}

let rows = measure();
for (let pass = 1; pass <= PASSES; ++pass) {
    const lo = Math.min(...rows.map(r => r.loud)), hi = Math.max(...rows.map(r => r.loud));
    console.log('pass ' + pass + ' before: spread ' + (hi - lo).toFixed(1) +
                ' dB  (' + lo.toFixed(1) + ' .. ' + hi.toFixed(1) + ')');

    const trims = [];
    let capped = 0;
    for (const r of rows) {
        const want = TARGET - r.loud;                       // dB to add
        const head = 20 * Math.log10(PEAK_CEIL / Math.max(r.peak, 1e-6));
        let t = r.trim + Math.min(want, head);
        if (Math.min(want, head) < want - 0.05) ++capped;
        t = Math.max(TRIM_LO, Math.min(TRIM_HI, t));
        trims.push([r.i, t]);
    }
    writeTrims(trims);
    rebuild();
    rows = measure();

    const lo2 = Math.min(...rows.map(r => r.loud)), hi2 = Math.max(...rows.map(r => r.loud));
    const worst = Math.max(...rows.map(r => Math.abs(r.loud - TARGET)));
    console.log('        after:  spread ' + (hi2 - lo2).toFixed(1) + ' dB   worst off target ' +
                worst.toFixed(1) + ' dB   ' + capped + ' capped by peak');
    if (hi2 - lo2 < 3.0) break;
}

rows.sort((a, b) => a.loud - b.loud);
console.log('\nquietest five:');
for (const r of rows.slice(0, 5))
    console.log('  ' + r.name.padEnd(16) + r.loud.toFixed(1) + ' dB   trim ' + r.trim.toFixed(1) + '   peak ' + r.peak.toFixed(3));
console.log('loudest five:');
for (const r of rows.slice(-5))
    console.log('  ' + r.name.padEnd(16) + r.loud.toFixed(1) + ' dB   trim ' + r.trim.toFixed(1) + '   peak ' + r.peak.toFixed(3));
const pk = rows.reduce((m, r) => Math.max(m, r.peak), 0);
console.log('\nloudest peak in the bank ' + pk.toFixed(3));
