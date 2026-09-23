/*  The hints block has to be DEFINED before anything runs that touches it.
    layout() re-places an open hint on a resize, and layout() is called from
    the boot line — which is above the block, so `const HINT` was still in its
    temporal dead zone and the whole script died at the first call. Move the
    block above layout() and start it from the boot line as intended.

    (The lesson, and it is the second time in one round: a function declaration
    hoists, the consts it closes over do not. Where a block is placed is part
    of whether it works.) */
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/ui/ui.html";
let s = fs.readFileSync(f, "utf8");

const startMark = "/* ===========================================================================\n   12 . the hints layer";
const endMark = "\ninitHints();\n";
const nl = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const S = startMark.split("\n").join(nl), E = endMark.split("\n").join(nl);

const a = s.indexOf(S);
if (a < 0) { console.log("MISS: no hints block"); process.exit(1); }
const b = s.indexOf(E, a);
if (b < 0) { console.log("MISS: no initHints() terminator"); process.exit(1); }
const block = s.slice(a, b + E.length);
s = s.slice(0, a) + s.slice(b + E.length);

//  the block minus its trailing call — the call goes back on the boot line
const body = block.slice(0, block.length - E.length) + nl;

const host = "function layout(){ glResize(); tlResize();";
if (s.split(host).length - 1 !== 1) { console.log("MISS: layout() anchor"); process.exit(1); }
s = s.split(host).join(body + nl + host);

const boot = "layout(); glInit(); buildRail(); paintPatch();";
if (s.split(boot).length - 1 !== 1) { console.log("MISS: boot anchor"); process.exit(1); }
s = s.split(boot).join("initHints();" + nl + boot);

fs.writeFileSync(f, s, "utf8");
console.log("hints block moved above its first use");
