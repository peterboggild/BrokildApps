/*  WAKE now DEFAULTS to 0.15 (the artefact idles alive), so "untouched ==
    wake 0" is no longer the premise. The contracts that still matter:
    wake 0 is deterministic (bit-identical renders), and the default wander
    is real (a default engine does not render like a wake-0 engine).
*/
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/ArtefactB2311/test/bench.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const A = [
"    //  ---- 12. WAKE=0 is exactly inert; wake wanders deterministically ---",
"    {",
"        Engine a, b;",
"        a.prepare (fs, 512);",
"        b.p.wake = 0.0f;      // explicit zero == untouched",
"        b.prepare (fs, 512);",
"        const int N = 48000;",
"        std::vector<float> l1 (N), r1 (N), l2 (N), r2 (N);",
"        render (a, { { 0, 0, 57, 0.8f } }, l1.data(), r1.data(), N);",
"        render (b, { { 0, 0, 57, 0.8f } }, l2.data(), r2.data(), N);",
"        CHECK (std::memcmp (l1.data(), l2.data(), N * 4) == 0,",
'               "wake 0 is not exactly the untouched engine");',
"    }"].join(NL);
const B = [
"    //  ---- 12. WAKE 0 is deterministic; the default wander is real -------",
"    {",
"        //  wake defaults ON (0.15): the artefact idles alive. So the",
"        //  contracts are: wake 0 renders bit-identically twice, and a",
"        //  default engine does NOT render like a wake-0 engine over a",
"        //  wander period (the exploration is real, not cosmetic).",
"        Engine a, b, c;",
"        a.p.wake = 0.0f; a.prepare (fs, 512);",
"        b.p.wake = 0.0f; b.prepare (fs, 512);",
"        c.prepare (fs, 512);          // default wake 0.15",
"        const int N = 6 * 48000;",
"        std::vector<float> l1 (N), r1 (N), l2 (N), r2 (N), l3 (N), r3 (N);",
"        render (a, { { 0, 0, 57, 0.8f } }, l1.data(), r1.data(), N);",
"        render (b, { { 0, 0, 57, 0.8f } }, l2.data(), r2.data(), N);",
"        render (c, { { 0, 0, 57, 0.8f } }, l3.data(), r3.data(), N);",
"        CHECK (std::memcmp (l1.data(), l2.data(), N * 4) == 0,",
'               "wake 0 is not deterministic");',
"        CHECK (std::memcmp (l1.data(), l3.data(), N * 4) != 0,",
'               "default wake changes nothing - the wander is cosmetic");',
"    }"].join(NL);
const n = s.split(A).length - 1;
if (n !== 1) { console.error("anchor x" + n); process.exit(1); }
s = s.split(A).join(B);
fs.writeFileSync(P, s);
console.log("wake test updated");
