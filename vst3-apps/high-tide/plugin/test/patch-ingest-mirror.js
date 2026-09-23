/*  The redo came back with the same letterboxing, but the DRAWINGS are now
    good — the ground has real grain at last (luminance sd 2.6 -> 21.4) and the
    paper has fibre (3.3 -> 8.8). So the panel takes them from this side rather
    than asking a third time:

      ground, paper   a strip of texture, mirrored top-to-bottom into a tile
                      that repeats without a seam. Fine random grain mirrors
                      invisibly; this is the standard trick for exactly this.
      rose            the top half of a circular rose, cropped — not squashed.
                      Used from a corner, where only its upper quadrant shows,
                      which is a normal chart ornament and invents nothing.

    The nameplate stays out: its lettering is cut by the plate's own bottom
    edge in the delivered artwork, and no crop can put that back.
*/
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/tools/ingest-decals.ps1";
let s = fs.readFileSync(f, "utf8");
const misses = [];
function rep(a, b) {
  const c = s.split(a).length - 1;
  if (c !== 1) { misses.push("expected 1 of " + JSON.stringify(a.slice(0, 60)) + ", found " + c); return; }
  s = s.split(a).join(b);
}

rep(`# part -> target size on disk, and whether to turn it a quarter turn first.
# The glass tube is drawn standing up and the gauges lie down.
$WANT = @{`,
`# part -> target size on disk, whether to turn it a quarter turn first, and
# whether to mirror it top-to-bottom into a seamlessly repeating tile.
# The glass tube is drawn standing up and the gauges lie down; the two ground
# textures arrived as strips and are mirrored into tiles.
$WANT = @{
    "ht-ground.png"    = @{ w = 1024; h = 0;   rot = $false; mirror = $true  }
    "ht-paper.png"     = @{ w = 1024; h = 0;   rot = $false; mirror = $true  }
    "ht-rose.png"      = @{ w = 512;  h = 0;   rot = $false; mirror = $false }`);

rep(`    if ($WANT[$name].rot) { $crop.RotateFlip([System.Drawing.RotateFlipType]::Rotate90FlipNone) }`,
`    if ($WANT[$name].rot) { $crop.RotateFlip([System.Drawing.RotateFlipType]::Rotate90FlipNone) }

    # A strip of texture becomes a tile that repeats without a seam by
    # mirroring it once: the join is the strip's own edge against itself.
    if ($WANT[$name].mirror) {
        $mw = $crop.Width; $mh = $crop.Height * 2
        $mir = New-Object System.Drawing.Bitmap $mw, $mh
        $gm = [System.Drawing.Graphics]::FromImage($mir)
        $gm.DrawImage($crop, 0, 0, $crop.Width, $crop.Height)
        $flip = New-Object System.Drawing.Bitmap $crop
        $flip.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY)
        $gm.DrawImage($flip, 0, $crop.Height, $crop.Width, $crop.Height)
        $gm.Dispose(); $flip.Dispose(); $crop.Dispose()
        $crop = $mir
    }`);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("ingest: ground and paper mirror into tiles, rose comes in");
