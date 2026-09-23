/*  Panel round 3: the same fault one step further in. countPlaced() ran before
    buildMacros(), so the eight macro knobs still counted as unplaced and the
    MORE strip still took them out of the macro rail when the MIX view opened.
    Build every panel FIRST, then count, then compute the spare list.
*/
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/Source/ui/ui.html";
let s = fs.readFileSync(p, "utf8");

const a = `    countPlaced();
    var spare = [];
    ORDER.forEach(function (id) { if (!/^bwfx_/.test(id) && !PLACED[id]) spare.push([id, PNAME[id] || id]); });
    if (MOREP) { MOREP._want = spare; MOREP.hidden = spare.length === 0; MOREP.parentNode && (MOREP.parentNode.style.display = ""); }
    buildMacros(); buildPalette();`;
const b = `    /*  Every panel that can own an id must exist before the spare list is
        counted - the views, the header, the foot AND the macro rail - or the
        spare box takes a control out of a strip the moment a view opens. */
    buildMacros(); buildPalette();
    countPlaced();
    var spare = [];
    ORDER.forEach(function (id) { if (!/^bwfx_/.test(id) && !PLACED[id]) spare.push([id, PNAME[id] || id]); });
    if (MOREP) { MOREP._want = spare; MOREP.hidden = spare.length === 0; MOREP.parentNode && (MOREP.parentNode.style.display = ""); }`;

if (s.split(a).length !== 2) { console.log("ANCHOR MISS"); process.exit(1); }
s = s.replace(a, b);
fs.writeFileSync(p, s);
console.log("ui.html patched");
