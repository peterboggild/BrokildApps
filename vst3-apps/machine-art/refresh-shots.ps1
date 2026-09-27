# Refresh a drum's page and manual images from its panel-test renders.
#
#   powershell -File refresh-shots.ps1 -Drum kickstart|snare-tactics|hats-off -Renders <folder>
#
# <folder> is where that drum's shot tool wrote its PNGs (ksshot / stshot /
# hoshot <folder>). The landing page AND the manual read vst3-apps/<drum>/img,
# so this updates both; rebuild the manual PDF afterwards. Sizes and file names
# are kept exactly as they were, so no page or manual layout moves.
#
# ASCII only: Windows PowerShell 5.1 reads a no-BOM .ps1 as ANSI.
param(
  [Parameter(Mandatory=$true)][ValidateSet('kickstart','snare-tactics','hats-off')][string]$Drum,
  [Parameter(Mandatory=$true)][string]$Renders
)
Add-Type -AssemblyName System.Drawing
$img = Join-Path $PSScriptRoot "..\$Drum\img"
$enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | ? { $_.MimeType -eq 'image/jpeg' }
$ps = New-Object System.Drawing.Imaging.EncoderParameters 1
$ps.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), ([long]88)

function Emit($srcPng, $srcRect, $dstFile) {
  $src = [System.Drawing.Bitmap]::FromFile($srcPng)
  $old = [System.Drawing.Image]::FromFile($dstFile); $w = $old.Width; $h = $old.Height; $old.Dispose()   # keep the size the pages expect
  if (-not $srcRect) { $srcRect = New-Object System.Drawing.Rectangle 0, 0, $src.Width, $src.Height }
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
  $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $w, $h), $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose(); $src.Dispose()
  $bmp.Save($dstFile, $enc, $ps); $bmp.Dispose()
  "  {0,-18} {1} x {2}   <- {3}" -f (Split-Path $dstFile -Leaf), $w, $h, (Split-Path $srcPng -Leaf)
}

"`n$Drum images -> $img`n"
Emit (Join-Path $Renders "panel-2x.png") $null (Join-Path $img "panel.jpg")
foreach ($f in Get-ChildItem $img -Filter "hit-*.jpg") {
  $name = $f.BaseName.Substring(4)
  if ($Drum -eq 'kickstart') {
    # Kickstart's shot tool renders whole panels; the hit image is the scope
    # row of that preset's panel (the same crop the page always used)
    Emit (Join-Path $Renders "panel-$name.png") (New-Object System.Drawing.Rectangle 16, 76, 1088, 262) $f.FullName
  } else {
    Emit (Join-Path $Renders "hit-$name.png") $null $f.FullName
  }
}
""
