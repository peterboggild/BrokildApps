# Turn the four delivered sheets into the runtime assets the panel embeds.
#
# Rules learned on High Tide / Full Metal Racket and applied here:
#  - a part's DELIVERED size is not its drawn size; every part is cropped to its
#    own measured content box (assets/panel-geometry.json), never to the sheet grid
#  - parts are downsampled to about 2x their on-panel size: a 1024px knob
#    brushing aliases into a rotating fan when drawn at 120px
#  - the panel carries no alpha, so it ships as JPEG; anything with alpha stays PNG
#  - the screen's own highlights and scratches are lifted into their own part, so
#    the miniature visuals can be drawn UNDER the real glass instead of replacing it
param(
  [string]$Root = "$PSScriptRoot\.."
)
Add-Type -AssemblyName System.Drawing
$geo = Get-Content (Join-Path $Root "assets\panel-geometry.json") -Raw | ConvertFrom-Json
$dec = Join-Path $Root "assets\decals"
$outDir = Join-Path $Root "Source\ui\art"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function NewCanvas([int]$w, [int]$h) {
  $bm = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bm)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
  $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
  return @{ bm = $bm; g = $g }
}
function SaveJpeg($bm, $path, [int]$quality) {
  $enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
  $ps = New-Object System.Drawing.Imaging.EncoderParameters 1
  $ps.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), ([long]$quality)
  $bm.Save($path, $enc, $ps)
}

# --- 1. the panel itself ------------------------------------------------------
# No alpha anywhere in it, so JPEG at high quality: a third of the PNG's bytes
# and no visible difference on a photographic plate.
$panel = [System.Drawing.Bitmap]::FromFile((Join-Path $dec "panel.png"))
$flat = NewCanvas $panel.Width $panel.Height
$flat.g.DrawImage($panel, 0, 0, $panel.Width, $panel.Height)
$flat.g.Dispose()
$p = Join-Path $outDir "panel.jpg"
SaveJpeg $flat.bm $p 93
Write-Output ("panel.jpg            {0} x {1}   {2:N0} KB" -f $panel.Width, $panel.Height, ((Get-Item $p).Length / 1KB))

# --- 2. the screen glass ------------------------------------------------------
# The bezel's highlight and the dirt on the tube, lifted out so the visuals can
# be screen-blended UNDER them. Taken a little larger than the glass box so the
# inner edge of the chrome is included and there is no seam.
$s = $geo.screen
$pad = 0   # exactly the glass: any overhang lands on bright chrome, and a
           # screen blend there brightens it into a visible rectangle
$gx = [Math]::Max(0, $s.x - $pad); $gy = [Math]::Max(0, $s.y - $pad)
$gw = $s.w + $pad*2; $gh = $s.h + $pad*2
$glass = NewCanvas $gw $gh
$srcR = New-Object System.Drawing.Rectangle $gx, $gy, $gw, $gh
$dstR = New-Object System.Drawing.Rectangle 0, 0, $gw, $gh
$glass.g.DrawImage($panel, $dstR, $srcR, [System.Drawing.GraphicsUnit]::Pixel)
$glass.g.Dispose()
$p = Join-Path $outDir "screen-glass.jpg"
SaveJpeg $glass.bm $p 92
Write-Output ("screen-glass.jpg     {0} x {1}   {2:N0} KB   (offset {3},{4})" -f $gw, $gh, ((Get-Item $p).Length / 1KB), ($gx - $s.x), ($gy - $s.y))
$glass.bm.Dispose(); $flat.bm.Dispose(); $panel.Dispose()

# --- 3. knob sprite strips ----------------------------------------------------
# One strip per set: six knobs side by side in uniform square cells, each knob
# centred in its cell so the page can rotate about the cell centre and needs to
# know nothing about the original sheet.
$cell = 256                       # ~2x the 120px draw size, with room to rotate
foreach ($setName in @("knobChrome", "knobOrange")) {
  $def = $geo.sheets.$setName
  $sheet = [System.Drawing.Bitmap]::FromFile((Join-Path $dec $def.file))
  $strip = NewCanvas ($cell * 6) $cell
  for ($i = 0; $i -lt 6; $i++) {
    $part = $def.parts[$i]
    # Fit the part inside the cell keeping its own aspect - a knob drawn oval
    # stays oval, so it must never be stretched to square.
    $scale = [Math]::Min($cell / [double]$part.w, $cell / [double]$part.h)
    $dw = $part.w * $scale; $dh = $part.h * $scale
    $dx = $i * $cell + ($cell - $dw) / 2.0; $dy = ($cell - $dh) / 2.0
    $src = New-Object System.Drawing.Rectangle $part.x, $part.y, $part.w, $part.h
    $dst = New-Object System.Drawing.RectangleF ([float]$dx), ([float]$dy), ([float]$dw), ([float]$dh)
    $strip.g.DrawImage($sheet, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
  }
  $strip.g.Dispose()
  $name = $(if ($setName -eq "knobChrome") { "knobs-chrome.png" } else { "knobs-orange.png" })
  $p = Join-Path $outDir $name
  $strip.bm.Save($p, [System.Drawing.Imaging.ImageFormat]::Png)
  Write-Output ("{0,-20} {1} x {2}   {3:N0} KB   cell {4}" -f $name, ($cell*6), $cell, ((Get-Item $p).Length / 1KB), $cell)
  $strip.bm.Dispose()

  # the red cap, from the same set
  $b = $def.button
  $bcell = 192
  $btn = NewCanvas $bcell $bcell
  $bscale = [Math]::Min($bcell / [double]$b.w, $bcell / [double]$b.h)
  $bw = $b.w * $bscale; $bh = $b.h * $bscale
  $src = New-Object System.Drawing.Rectangle $b.x, $b.y, $b.w, $b.h
  $dst = New-Object System.Drawing.RectangleF ([float](($bcell-$bw)/2)), ([float](($bcell-$bh)/2)), ([float]$bw), ([float]$bh)
  $btn.g.DrawImage($sheet, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
  $btn.g.Dispose()
  if ($setName -eq "knobChrome") {
    $p = Join-Path $outDir "button.png"
    $btn.bm.Save($p, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output ("button.png           {0} x {1}   {2:N0} KB" -f $bcell, $bcell, ((Get-Item $p).Length / 1KB))
  }
  $btn.bm.Dispose(); $sheet.Dispose()
}

# --- 4. fuel level strip ------------------------------------------------------
# 15 frames in one horizontal strip, index 0 = full .. 14 = empty, each cell the
# tube's own aspect so the page draws one cell into the glass box.
#
# The row index MUST be [Math]::Floor, not [int]. PowerShell's [int] cast does
# BANKER'S ROUNDING, not truncation - [int](2/3) is 1 - so every third frame
# read from the row below and the gauge lurched instead of stepping. Frame 14
# went further and asked for rows[5], which does not exist: it came back null,
# the rectangle's y became 0, and the frame was cut from the blank margin above
# the first tube. That is what looked like "the empty decal is transparent".
# tools/measure-fuel-levels.ps1 measures the fill of every frame and is the
# check that catches this.
$f = $geo.sheets.fuel
$fsheet = [System.Drawing.Bitmap]::FromFile((Join-Path $dec $f.file))
$fw = 384; $fh = [int][Math]::Round($fw * $f.h / [double]$f.w)
$fstrip = NewCanvas ($fw * 15) $fh
for ($i = 0; $i -lt 15; $i++) {
  $src = New-Object System.Drawing.Rectangle $f.cols[$i % 3], $f.rows[[Math]::Floor($i / 3)], $f.w, $f.h
  $dst = New-Object System.Drawing.RectangleF ([float]($i * $fw)), 0.0, ([float]$fw), ([float]$fh)

  $fstrip.g.DrawImage($fsheet, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
}
$fstrip.g.Dispose()
$p = Join-Path $outDir "fuel.png"
$fstrip.bm.Save($p, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output ("fuel.png             {0} x {1}   {2:N0} KB   cell {3} x {4}" -f ($fw*15), $fh, ((Get-Item $p).Length / 1KB), $fw, $fh)
$fstrip.bm.Dispose(); $fsheet.Dispose()

$total = (Get-ChildItem $outDir -File | Measure-Object -Property Length -Sum).Sum
Write-Output ("-- total embedded art  {0:N0} KB" -f ($total / 1KB))
