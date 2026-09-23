// Make the delivered cheek usable now, without waiting for a replacement.
//
// Two steps. Crop the rounded ends off — they are the thing that repeats — and
// then MIRROR the remaining slice: an image made of a slice followed by its own
// vertical flip tiles seamlessly by construction, because the join in the
// middle is a reflection and the join at the wrap is the same reflection. On
// straight vertical grain the mirror is invisible.
//
// A proper seamless texture is still better and has been ordered; this makes
// the current one look right in the meantime.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

const w = edit(R + "tools/ingest-decals.ps1", [
[`  $path = Join-Path $ArtDir "$name.png"
  $out.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $out.Dispose()`,
`  # ---- the cheek is tiled vertically, so it has to BE tileable -------------
  if ($name -eq "fmr-cheek") {
    # lose the rounded ends: they are what repeats
    $trim = [int]($out.Height * 0.15)
    $keepH = $out.Height - 2 * $trim
    if ($keepH -gt 16) {
      $slice = $out.Clone((New-Object System.Drawing.Rectangle(0, $trim, $out.Width, $keepH)),
                          [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
      # slice + its own vertical mirror: seamless at the middle AND at the wrap
      $tile = New-Object System.Drawing.Bitmap($out.Width, $keepH * 2,
                          [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
      $tg = [System.Drawing.Graphics]::FromImage($tile)
      try {
        $tg.DrawImage($slice, 0, 0, $out.Width, $keepH)
        $flip = $slice.Clone()
        $flip.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY)
        $tg.DrawImage($flip, 0, $keepH, $out.Width, $keepH)
        $flip.Dispose()
      } finally { $tg.Dispose() }
      $slice.Dispose(); $out.Dispose(); $out = $tile
      $outW = $out.Width; $outH = $out.Height
      $shrunk = "$shrunk   ends cropped, mirror-tiled"
    }
  }

  $path = Join-Path $ArtDir "$name.png"
  $out.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $out.Dispose()`, "cheek tile"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
w();
console.log("cheek: ends cropped and mirror-tiled");
