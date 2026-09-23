# Crop the whole-window PNGs that test\manual-plates.json shoots down to the
# element each plate is about, and write them as JPEG at the house settings
# (quality 88, max 1700 px wide, composited onto the page colour because JPEG
# has no alpha).
#
# tools\cdp.js can only save a WHOLE-WINDOW screenshot, so the crop happens
# here. The rectangles come from the panel itself: manual-plates.json prints
# every one of them as its first job ("rects"), measured live in the window the
# shots were taken in. Paste those numbers into $CROPS before a run if the
# panel's layout has moved - a crop guessed from an older build silently cuts
# the plate off centre, and the JPEG still looks like a picture.
#
# ASCII only - Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI and an
# em dash is a parser error.

param(
  [string]$Raw  = "C:\Users\peter\b\ThinWalls\docs\manual\raw",
  [string]$Dest = "C:\Users\peter\b\ThinWalls\docs\manual\img",
  [int]$MaxWide = 1700,
  [int]$Quality = 88,
  # the window the shots were taken in; a run at another size is refused rather
  # than cropped by numbers that no longer mean anything
  [int]$ShotW = 0,
  [int]$ShotH = 0
)

Add-Type -AssemblyName System.Drawing

# name = x, y, w, h  in the shot's own pixels, plus the scale the plate is
# rendered at (a small control group needs more than a full window does).
# These are the layout at the editor's own 1400 x 900, measured 2026-09-22:
# a 42 px header, the two views over 424 px splitting 812 / 588, then TWO
# rows of control groups at y 476 and y 696, and a 23 px status strip.
# Overwrite them from the "rects" job whenever the panel's layout moves.
$CROPS = @{
  'panel'          = @(0,      0, 1400, 900, 1)
  'pov'            = @(0,     42,  812, 424, 2)
  'pov-speaker'    = @(0,     42,  812, 424, 2)
  'pov-pure'       = @(0,     42,  812, 424, 2)
  'pov-handle'     = @(0,     42,  812, 424, 2)
  'plan'           = @(812,   42,  588, 424, 2)
  'plan-labels'    = @(812,   42,  588, 424, 2)
  'plan-open'      = @(812,   42,  588, 424, 2)
  'plan-shut'      = @(812,   42,  588, 424, 2)
  'plan-folded'    = @(812,   42,  588, 424, 2)
  'ctrl-materials' = @(245,  476,  449, 132, 3)
  'ctrl-folds'     = @(706,  476,  449, 190, 3)
  'ctrl-source'    = @(1167, 476,  219, 211, 3)
  'ctrl-levels'    = @(14,   696,  449, 173, 3)
  'ctrl-wav'       = @(937,  696,  449, 173, 3)
  'hud'            = @(12,    54,  159, 118, 4)
  'status'         = @(0,    877, 1400,  23, 2)
}

$PAGE = [System.Drawing.Color]::FromArgb(13, 11, 10)   # --ink, the manual's page

New-Item -ItemType Directory -Force -Path $Dest | Out-Null

$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() |
         Where-Object { $_.MimeType -eq 'image/jpeg' }
$pars = New-Object System.Drawing.Imaging.EncoderParameters 1
$pars.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter(
                   [System.Drawing.Imaging.Encoder]::Quality, [int64]$Quality)

$bad = 0
Get-ChildItem $Raw -Filter *.png -ErrorAction SilentlyContinue | ForEach-Object {
  $name = [System.IO.Path]::GetFileNameWithoutExtension($_.Name)
  if (-not $CROPS.ContainsKey($name)) {
    Write-Output ("  {0,-14} NO CROP DEFINED - skipped" -f $name); $bad++; return
  }
  $c = $CROPS[$name]
  $src = [System.Drawing.Image]::FromFile($_.FullName)

  if ($ShotW -gt 0 -and ($src.Width -ne $ShotW -or $src.Height -ne $ShotH)) {
    Write-Output ("  {0,-14} SHOT IS {1}x{2}, EXPECTED {3}x{4} - crops would be wrong" -f `
                  $name, $src.Width, $src.Height, $ShotW, $ShotH)
    $src.Dispose(); $bad++; return
  }

  # clamp the rectangle into the image rather than throwing: a window one pixel
  # short is not a reason to lose the plate
  $x = [Math]::Max(0, [Math]::Min($c[0], $src.Width  - 1))
  $y = [Math]::Max(0, [Math]::Min($c[1], $src.Height - 1))
  $w = [Math]::Min($c[2], $src.Width  - $x)
  $h = [Math]::Min($c[3], $src.Height - $y)

  $scale = [double]$c[4]
  $outW = [int]([Math]::Round($w * $scale))
  $outH = [int]([Math]::Round($h * $scale))
  if ($outW -gt $MaxWide) { $outH = [int]([Math]::Round($outH * $MaxWide / $outW)); $outW = $MaxWide }

  $bmp = New-Object System.Drawing.Bitmap($outW, $outH,
           [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.Clear($PAGE)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawImage($src, (New-Object System.Drawing.Rectangle(0, 0, $outW, $outH)),
                     $x, $y, $w, $h, [System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose()

  $out = Join-Path $Dest ($name + '.jpg')
  $bmp.Save($out, $codec, $pars)
  $bmp.Dispose(); $src.Dispose()
  Write-Output ("  {0,-14} {1,4}x{2,-4} -> {3,4}x{4,-4}  {5,5} KB" -f `
                $name, $w, $h, $outW, $outH, [int]((Get-Item $out).Length / 1KB))
}

if ($bad -gt 0) { Write-Output ("`n  {0} plate(s) not written" -f $bad); exit 1 }
Write-Output ""
