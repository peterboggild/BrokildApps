# Cut the manual's plates out of the live screenshots.
#
# The shots are the whole standalone window at 1200 x 800. The panel geometry
# table is in PANEL pixels (1536 x 1024), so every rectangle from that table is
# scaled by 1200/1536 = 0.78125 to land on a shot.
#
# Everything is written as JPEG: the manual is a PDF, JPEG has no alpha, and a
# dozen 1200x800 PNGs would treble the download for no visible gain.
#
# ASCII only - a UTF-8-no-BOM .ps1 with an em dash is a PARSER ERROR in
# Windows PowerShell 5.1, which reads the file as ANSI.
param(
  [string]$Src  = "C:\Users\peter\b\BattlestarOverdrive\docs",
  [string]$Dest = "C:\Users\peter\b\BattlestarOverdrive\docs\manual\img",
  [int]$Quality = 90
)

Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force -Path $Dest | Out-Null

$K = 1200.0 / 1536.0        # window pixels per panel pixel

# Regions, in PANEL coordinates, straight out of assets/panel-geometry.json.
# A little air is added around each so the crop does not sit on the edge.
$REGIONS = @{
  screen = @{ x = 421 - 14; y = 244 - 14; w = 682 + 28; h = 494 + 28 }
  fuel   = @{ x = 500 - 60; y = 793 - 26; w = 376 + 210; h = 96 + 52 }
}

$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() |
         Where-Object { $_.MimeType -eq 'image/jpeg' }
$eps = New-Object System.Drawing.Imaging.EncoderParameters 1
$eps.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter(
                  [System.Drawing.Imaging.Encoder]::Quality, [long]$Quality)

function Save-Plate {
  param([string]$From, [string]$To, [string]$Region, [double]$Zoom = 1.0)

  $path = Join-Path $Src $From
  if (-not (Test-Path $path)) { Write-Host ("  MISSING  " + $From); return }

  $bmp = [System.Drawing.Bitmap]::FromFile($path)
  try {
    if ($Region -eq 'full') {
      $sx = 0; $sy = 0; $sw = $bmp.Width; $sh = $bmp.Height
    } else {
      $r  = $REGIONS[$Region]
      $sx = [int]([Math]::Round($r.x * $K)); $sy = [int]([Math]::Round($r.y * $K))
      $sw = [int]([Math]::Round($r.w * $K)); $sh = [int]([Math]::Round($r.h * $K))
      # never ask for pixels the shot does not have
      if ($sx -lt 0) { $sx = 0 }; if ($sy -lt 0) { $sy = 0 }
      if ($sx + $sw -gt $bmp.Width)  { $sw = $bmp.Width  - $sx }
      if ($sy + $sh -gt $bmp.Height) { $sh = $bmp.Height - $sy }
    }

    $dw = [int]($sw * $Zoom); $dh = [int]($sh * $Zoom)
    $dst = New-Object System.Drawing.Bitmap $dw, $dh
    $g = [System.Drawing.Graphics]::FromImage($dst)
    $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    # JPEG has no alpha, so lay it on the manual's own page colour first
    $g.Clear([System.Drawing.Color]::FromArgb(255, 9, 7, 5))
    $srcRect = New-Object System.Drawing.Rectangle $sx, $sy, $sw, $sh
    $dstRect = New-Object System.Drawing.Rectangle 0, 0, $dw, $dh
    $g.DrawImage($bmp, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()

    $target = Join-Path $Dest $To
    $dst.Save($target, $codec, $eps)
    $dst.Dispose()
    $kb = [int]((Get-Item $target).Length / 1024)
    Write-Host ("  {0,-22} {1,4} x {2,-4}  {3,5} KB" -f $To, $dw, $dh, $kb)
  } finally { $bmp.Dispose() }
}

Write-Host "`nmanual plates ->  $Dest`n"

# --- whole panel -------------------------------------------------------------
Save-Plate -From 'm-panel.png'      -To 'panel.jpg'        -Region full
Save-Plate -From 'm-nova-shell.png' -To 'panel-nova.jpg'   -Region full
Save-Plate -From 'm-warp.png'       -To 'panel-warp.jpg'   -Region full

# --- the screen, where everything the instrument says is drawn ---------------
Save-Plate -From 'm-readout.png'     -To 'scr-readout.jpg' -Region screen
Save-Plate -From 'm-engine.png'      -To 'scr-engine.jpg'  -Region screen
Save-Plate -From 'm-idle.png'        -To 'scr-idle.jpg'    -Region screen
Save-Plate -From 'm-warp.png'        -To 'scr-warp.jpg'    -Region screen
Save-Plate -From 'm-nova-giant.png'  -To 'scr-giant.jpg'   -Region screen
Save-Plate -From 'm-nova-shell.png'  -To 'scr-nova.jpg'    -Region screen
Save-Plate -From 'm-nebula.png'      -To 'scr-nebula.jpg'  -Region screen
Save-Plate -From 'm-width.png'       -To 'scr-width.jpg'   -Region screen
Save-Plate -From 'm-overload.png'    -To 'scr-overload.jpg' -Region screen
Save-Plate -From 'm-fuel-empty.png'  -To 'scr-empty.jpg'   -Region screen

# --- the fuel tube -----------------------------------------------------------
Save-Plate -From 'm-panel.png'      -To 'fuel-full.jpg'  -Region fuel -Zoom 1.6
Save-Plate -From 'm-fuel-low.png'   -To 'fuel-low.jpg'   -Region fuel -Zoom 1.6
Save-Plate -From 'm-fuel-empty.png' -To 'fuel-empty.jpg' -Region fuel -Zoom 1.6

Write-Host ""
