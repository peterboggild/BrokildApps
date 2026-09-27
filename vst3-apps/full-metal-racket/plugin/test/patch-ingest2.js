// Register a lit/unlit pair RELATIVE to each part's own crop, not in absolute
// sheet coordinates.
//
// The union of two absolute boxes is only meaningful if both crops sit in the
// same place, which they generally do not — on a sheet the two states are side
// by side or one above the other, so the "union" was a box spanning both and
// the part came out stretched into a smear. What must be unioned is each
// part's offset WITHIN its own crop: that is what "the same object in two
// frames" actually means.
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/FullMetalRacket/tools/ingest-decals.ps1";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function rep(a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(a).join(b);
}

rep(`        $boxes[$part.name] = @{ x = ($cx + $b.x); y = ($cy + $b.y); w = $b.w; h = $b.h }`,
    `        # keep the crop origin AND the offset within it, so a pair can be
        # registered against each other later without dragging their
        # positions on the sheet into it
        $boxes[$part.name] = @{ x = ($cx + $b.x); y = ($cy + $b.y); w = $b.w; h = $b.h
                                ox = $cx; oy = $cy; rx = $b.x; ry = $b.y }`, "keep offsets");

rep(`    foreach ($a in $PAIRS.Keys) {
      $b = $PAIRS[$a]
      if ($boxes.ContainsKey($a) -and $boxes.ContainsKey($b)) {
        $u = @{
          x = [Math]::Min($boxes[$a].x, $boxes[$b].x)
          y = [Math]::Min($boxes[$a].y, $boxes[$b].y)
        }
        # width and height measured from each box's own origin, unioned
        $u.w = [Math]::Max($boxes[$a].x + $boxes[$a].w, $boxes[$b].x + $boxes[$b].w) - $u.x
        $u.h = [Math]::Max($boxes[$a].y + $boxes[$a].h, $boxes[$b].y + $boxes[$b].h) - $u.y
        # only sensible when the two crops overlap; otherwise leave them alone
        if ($u.w -lt $boxes[$a].w * 2.2 -and $u.h -lt $boxes[$a].h * 2.2) {
          $boxes[$a] = $u.Clone(); $boxes[$b] = $u.Clone()
          Write-Output ("  {0} / {1} trimmed together, so the states stay registered" -f $a, $b)
        }
      }
    }`,
`    foreach ($a in $PAIRS.Keys) {
      $b = $PAIRS[$a]
      if ($boxes.ContainsKey($a) -and $boxes.ContainsKey($b)) {
        # union in CROP-RELATIVE coordinates: the same object seen in two
        # frames, wherever those frames happen to sit on the sheet
        $rx = [Math]::Min($boxes[$a].rx, $boxes[$b].rx)
        $ry = [Math]::Min($boxes[$a].ry, $boxes[$b].ry)
        $rw = [Math]::Max($boxes[$a].rx + $boxes[$a].w, $boxes[$b].rx + $boxes[$b].w) - $rx
        $rh = [Math]::Max($boxes[$a].ry + $boxes[$a].h, $boxes[$b].ry + $boxes[$b].h) - $ry
        foreach ($k in @($a, $b)) {
          $boxes[$k] = @{ x = ($boxes[$k].ox + $rx); y = ($boxes[$k].oy + $ry); w = $rw; h = $rh
                          ox = $boxes[$k].ox; oy = $boxes[$k].oy; rx = $rx; ry = $ry }
        }
        Write-Output ("  {0} / {1} registered against each other ({2} x {3})" -f $a, $b, $rw, $rh)
      }
    }`, "relative union");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("pair registration is crop-relative now");
