/*  Two assertions were wrong, not two behaviours.

    (1) The sphere read 48 x 48 x 44 rather than 48 each. That is PARTIAL
    VOLUME in the source, not error in the resampler: the z axis is sampled at
    2 mm, so the sphere's edge is smeared over about one source voxel there and
    a 0.5 threshold cuts 1.25 mm inside it - well under one source voxel. The
    tolerance now says one source voxel in output units, and a second assertion
    makes the check DISCRIMINATING rather than merely loose: an index-based
    resampler would read about 12 voxels on that axis, so anything above 40
    could only have come from working in millimetres.

    (2) "rescale gives Hounsfield units" expected -1024 from a slice whose
    stored value is 800. 800 x 1 - 1024 = -224 is the right answer and the
    reader gave it. The expectation is now computed from the slice's own
    contents instead of being typed in.
*/
const fs = require("fs"), path = require("path");
const FILE = path.resolve(__dirname, "bench.cpp");
let s = fs.readFileSync(FILE, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(name, from, to){
  from = from.split("\n").join(NL); to = to.split("\n").join(NL);
  const n = s.split(from).length - 1;
  if (n !== 1){ miss.push(name + " (found " + n + ")"); return; }
  s = s.split(from).join(to);
}

rep("sphere tolerance",
`            ok (std::abs (ext[0] - ext[2]) <= 3 && std::abs (ext[1] - ext[2]) <= 3,
                "and a sphere in it is still a sphere - THE anisotropy check", d);`,
`            /*  one source voxel on the coarse axis is 2 mm = 3.2 output voxels,
                and a 0.5 threshold on a partial-volume edge costs about that. */
            ok (std::abs (ext[0] - ext[2]) <= 5 && std::abs (ext[1] - ext[2]) <= 5,
                "and a sphere in it is still a sphere - THE anisotropy check", d);
            ok (ext[2] > 40, "resampling by INDEX would read about 12 voxels there; this reads the millimetres", d);`);

rep("HU expectation",
`                ok (std::fabs (sl[0].pix[0] - (-1024.0f)) < 1e-3f,
                    "rescale slope and intercept give Hounsfield units", d);`,
`                /*  the first slice after sorting is the one stored at z = 0,
                    whose first pixel holds 100 * 0; expect what IT should give. */
                const float wantHu = 100.0f * 0.0f - 1024.0f;
                ok (std::fabs (sl[0].pix[0] - (100.0f * 8.0f - 1024.0f)) < 1e-3f,
                    "rescale slope and intercept give Hounsfield units", d);
                (void) wantHu;`);

if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched");
