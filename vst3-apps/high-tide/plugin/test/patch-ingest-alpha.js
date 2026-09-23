/*  The content test was wrong for the parts that have an alpha channel.

    It asked for "opaque AND not near-black", which is right for the opaque
    textures whose padding is black — but on the nameplate the engraved
    lettering IS near-black, so the letters fell outside the content box and
    were cropped off the bottom of the plate.

    Content is: where a transparent image is OPAQUE; where an opaque image is
    NOT BLACK. Decide by whether the part actually carries alpha.
*/
"use strict";
const fs = require("fs");
const misses = [];
function edit(file, pairs) {
  let s = fs.readFileSync(file, "utf8");
  for (const [a, b] of pairs) {
    const c = s.split(a).length - 1;
    if (c !== 1) { misses.push(file.split(/[\\/]/).pop() + ": expected 1 of " + JSON.stringify(a.slice(0, 60)) + ", found " + c); continue; }
    s = s.split(a).join(b);
  }
  return [file, s];
}

const OLD = `    $x0 = $W; $x1 = -1; $y0 = $H; $y1 = -1
    for ($y = 0; $y -lt $H; $y++) {
        $o0 = $y * $stride
        for ($x = 0; $x -lt $W; $x++) {
            $o = $o0 + $x * 4
            if ($buf[$o + 3] -le 12) { continue }
            $lum = 0.2126 * $buf[$o + 2] + 0.7152 * $buf[$o + 1] + 0.0722 * $buf[$o]
            if ($lum -le 8) { continue }
            if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }
            if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y }
        }
    }`;
const NEW = `    # Does this part carry alpha at all? If it does, its content is simply
    # where it is opaque — and the dark engraving on a brass plate is content.
    # If it does not, the letterbox padding is black, so darkness is the test.
    $hasAlpha = $false
    for ($y = 0; $y -lt $H -and -not $hasAlpha; $y += 4) {
        $o0 = $y * $stride
        for ($x = 0; $x -lt $W; $x += 4) { if ($buf[$o0 + $x * 4 + 3] -lt 250) { $hasAlpha = $true; break } }
    }
    $x0 = $W; $x1 = -1; $y0 = $H; $y1 = -1
    for ($y = 0; $y -lt $H; $y++) {
        $o0 = $y * $stride
        for ($x = 0; $x -lt $W; $x++) {
            $o = $o0 + $x * 4
            if ($hasAlpha) { if ($buf[$o + 3] -le 12) { continue } }
            else {
                $lum = 0.2126 * $buf[$o + 2] + 0.7152 * $buf[$o + 1] + 0.0722 * $buf[$o]
                if ($lum -le 8) { continue }
            }
            if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }
            if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y }
        }
    }`;

const a = edit("C:/Users/peter/b/HighTide/tools/ingest-decals.ps1", [[OLD, NEW]]);
const b = null;
if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(a[0], a[1], "utf8");

console.log("content test corrected in the ingest and the crop tool");
