# RITE OF PASSAGE -- turn the delivered decals into the files the panel embeds.
#
#   powershell -File tools/ingest-decals.ps1
#
# WHY THIS EXISTS. A delivery's stated size is not its drawn size. Seven of the
# twelve parts arrived as a wide strip pasted at the TOP of a much taller
# canvas, under a DELIVERED.md that said "Deviations: none" -- the identical
# fault High Tide's decals had, and it is invisible until something measures
# the content box. So nothing is used as delivered: every part is cropped to
# what is actually drawn, and the three tiling grounds are then MIRRORED into a
# seamless tile, which is free on a fine random grain and is the only way to
# make a 200-row strip cover a panel.
#
# Padding is never the answer (it leaves the object floating in its frame) and
# neither is stretching a circular part (it stays oval). Every part is scaled
# to about 2x its largest on-panel size and no further: the knob's 512 px of
# brushing aliases into a rotating fan at 52 px on screen.

Add-Type -AssemblyName System.Drawing

$src = Join-Path $PSScriptRoot "..\..\assets\rite-decals"
$dst = Join-Path $PSScriptRoot "..\decals"
if (-not (Test-Path $src)) { throw "no decals at $src" }
New-Item -ItemType Directory -Force -Path $dst | Out-Null

function Load ($name) { New-Object System.Drawing.Bitmap (Join-Path $src $name) }

function Pixels ($bmp) {
  $r = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
  $d = $bmp.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $buf = New-Object byte[] ($d.Stride * $bmp.Height)
  [System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $buf, 0, $buf.Length)
  $bmp.UnlockBits($d)
  return @{ buf = $buf; stride = $d.Stride; w = $bmp.Width; h = $bmp.Height }
}

# The content box. A transparent part's content is where it is opaque; an
# opaque part's is where it is not near-black, because that is how these
# particular strips were padded.
function ContentBox ($bmp) {
  $p = Pixels $bmp
  $minX = $p.w; $minY = $p.h; $maxX = -1; $maxY = -1
  for ($y = 0; $y -lt $p.h; $y++) {
    $row = $y * $p.stride
    for ($x = 0; $x -lt $p.w; $x++) {
      $i = $row + $x * 4
      $a = $p.buf[$i + 3]
      $lum = ($p.buf[$i] + $p.buf[$i + 1] + $p.buf[$i + 2]) / 3
      if ($a -gt 8 -and $lum -gt 8) {
        if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
        if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
      }
    }
  }
  if ($maxX -lt 0) { return @{ x = 0; y = 0; w = $p.w; h = $p.h } }
  return @{ x = $minX; y = $minY; w = ($maxX - $minX + 1); h = ($maxY - $minY + 1) }
}

function Draw ($bmp, $sx, $sy, $sw, $sh, $dw, $dh, $out, [switch]$Mirror) {
  $H = if ($Mirror) { $dh * 2 } else { $dh }
  $o = New-Object System.Drawing.Bitmap $dw, $H
  $g = [System.Drawing.Graphics]::FromImage($o)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
  $sr = New-Object System.Drawing.Rectangle $sx, $sy, $sw, $sh
  $g.DrawImage($bmp, (New-Object System.Drawing.Rectangle 0, 0, $dw, $dh), $sr, [System.Drawing.GraphicsUnit]::Pixel)
  if ($Mirror) {
    # the same strip flipped, so the seam at the join is a mirror line and the
    # tile repeats without a visible edge
    $flip = $o.Clone()
    $flip.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY)
    $g.DrawImage($flip, (New-Object System.Drawing.Rectangle 0, $dh, $dw, $dh),
                 (New-Object System.Drawing.Rectangle 0, ($H - $dh), $dw, $dh), [System.Drawing.GraphicsUnit]::Pixel)
    $flip.Dispose()
  }
  $g.Dispose()
  $path = Join-Path $dst $out
  $o.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  "{0,-22} {1,5} x {2,-5}" -f $out, $o.Width, $o.Height
  $o.Dispose()
}

"ingesting decals -> $dst"
"{0,-22} {1,5}   {2}" -f "file", "size", ""
"-" * 46

# --- the three tiling grounds: crop to the drawn strip, then mirror ---------
foreach ($t in @(
    @{ f = "01-charred-wood-ground.png"; out = "ground.png";   w = 512 },
    @{ f = "02-fired-clay-slab.png";     out = "clay.png";     w = 512 },
    @{ f = "03-ash-wear-mask.png";       out = "ashwear.png";  w = 512 })) {
  $b = Load $t.f
  $c = ContentBox $b
  $h = [int]([Math]::Round($t.w * $c.h / $c.w))
  Draw $b $c.x $c.y $c.w $c.h $t.w $h $t.out -Mirror
  $b.Dispose()
}

# --- parts used once, cropped to what is drawn ------------------------------
# lintel: the header rail. Drawn 980 wide, so 1024 is twice what it needs.
$b = Load "04-lintel-beam.png"; $c = ContentBox $b
Draw $b $c.x $c.y $c.w $c.h 1024 ([int]([Math]::Round(1024 * $c.h / $c.w))) "lintel.png"
$b.Dispose()

# post: mirrored by the panel for the other side, so only one is needed
$b = Load "05-upright-post.png"; $c = ContentBox $b
Draw $b $c.x $c.y $c.w $c.h 64 ([int]([Math]::Round(64 * $c.h / $c.w))) "post.png"
$b.Dispose()

# marker: the POSITION handle, a river stone in cord
$b = Load "06-marker.png"; $c = ContentBox $b
Draw $b $c.x $c.y $c.w $c.h 96 ([int]([Math]::Round(96 * $c.h / $c.w))) "marker.png"
$b.Dispose()

# knobs and socket are square and full-canvas already; only downsample
$b = Load "07-knob-large.png"; Draw $b 0 0 512 512 192 192 "knob-large.png"; $b.Dispose()
$b = Load "08-knob-small.png"; Draw $b 0 0 256 256 128 128 "knob-small.png"; $b.Dispose()
$b = Load "09-slot-socket.png"; Draw $b 0 0 384 384 128 128 "socket.png"; $b.Dispose()

# arrival mark: the flash on the downbeat
$b = Load "11-arrival-mark.png"; $c = ContentBox $b
Draw $b $c.x $c.y $c.w $c.h 256 ([int]([Math]::Round(256 * $c.h / $c.w))) "arrival.png"
$b.Dispose()

# --- the glyph sheet, sliced into its 12 cells ------------------------------
# The grid is exact in the delivery (checked by drawing the cell boundaries
# over it), so the cells are taken at their nominal 384 x 288 and each is
# trimmed to its own glyph, which is what lets the panel centre them.
$b = Load "10-glyph-sheet.png"
$cw = 384; $ch = 288
for ($i = 0; $i -lt 12; $i++) {
  $cx = ($i % 4) * $cw; $cy = [Math]::Floor($i / 4) * $ch
  $cell = New-Object System.Drawing.Bitmap $cw, $ch
  $cg = [System.Drawing.Graphics]::FromImage($cell)
  $cg.DrawImage($b, (New-Object System.Drawing.Rectangle 0, 0, $cw, $ch),
                (New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch), [System.Drawing.GraphicsUnit]::Pixel)
  $cg.Dispose()
  $c = ContentBox $cell
  Draw $cell $c.x $c.y $c.w $c.h 96 ([int]([Math]::Round(96 * $c.h / $c.w))) ("glyph-{0:00}.png" -f ($i + 1))
  $cell.Dispose()
}
$b.Dispose()

# 12-wordmark.png is NOT ingested: the plank it is burnt into is shorter than
# the lettering, so the bottom of every letter is cut off in the delivery. See
# REDO.md. The panel draws its own title until a replacement arrives.
""
"12-wordmark.png SKIPPED -- the lettering is clipped by its own plank (see REDO.md)"
