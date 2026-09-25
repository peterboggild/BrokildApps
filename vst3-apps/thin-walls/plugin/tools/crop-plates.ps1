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
  [string]$Raw  = "$PSScriptRoot\..\docs\manual\raw",
  [string]$Dest = "$PSScriptRoot\..\docs\manual\img",
  [int]$MaxWide = 1700,
  [int]$Quality = 88,
  # the window the shots were taken in; a run at another size is refused rather
  # than cropped by numbers that no longer mean anything
  [int]$ShotW = 0,
  [int]$ShotH = 0
)

Add-Type -AssemblyName System.Drawing

# From 260925.3 every plate is shot by ELEMENT: test\manual-plates-260925.json
# names a selector per shot and tools\cdp.js measures that element in the
# live panel at the instant of the shot (at deviceScaleFactor 2), so the PNG
# already IS the plate and no rectangle can be left over from an older
# layout. An entry of 'whole' takes the image as it is; a numeric entry
# (x, y, w, h, scale in the shot's own pixels) still works for a plate cut
# from a whole-window shot. A PNG with no entry at all is refused.
$CROPS = @{
  'panel'           = 'whole'
  'pov'             = 'whole'
  'plan'            = 'whole'
  'plan-labels'     = 'whole'
  'plan-open'       = 'whole'
  'plan-shut'       = 'whole'
  'plan-folded'     = 'whole'
  'pov-speaker'     = 'whole'
  'pov-pure'        = 'whole'
  'ctrl-materials'  = 'whole'
  'ctrl-folds'      = 'whole'
  'ctrl-source'     = 'whole'
  'ctrl-levels'     = 'whole'
  'ctrl-view'       = 'whole'
  'ctrl-wav'        = 'whole'
  'ctrl-furn'       = 'whole'
  'hud'             = 'whole'
  'status'          = 'whole'
  'hdr'             = 'whole'
  'hdr-rec'         = 'whole'
  'plan-furn'       = 'whole'
  'pov-furn'        = 'whole'
  'plan-pics'       = 'whole'
  'pov-pics'        = 'whole'
  'cine-off'        = 'whole'
  'cine-on'         = 'whole'
  'photo'           = 'whole'
  'exp-pop'         = 'whole'
  'sync-flare'      = 'whole'
  'gallery'         = 'whole'
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
  if ($c -is [string] -and $c -eq 'whole') { $c = @(0, 0, $src.Width, $src.Height, 1) }

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
