# PNG -> JPEG for the site and the manual. Optional max width, and an optional
# fixed output size for a card preview: cover-cropped from the centre, or with
# -Contain the whole image fitted inside on the panel's charcoal.
#   powershell -File tojpg.ps1 -In a.png -Out a.jpg [-MaxW 1700] [-W 1280 -H 720 [-Contain]] [-Q 88]
# ASCII only - Windows PowerShell 5.1 reads a no-BOM .ps1 as ANSI.
param([string]$In, [string]$Out, [int]$MaxW = 0, [int]$W = 0, [int]$H = 0, [int]$Q = 88, [switch]$Contain)
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Image]::FromFile((Resolve-Path $In))
$sx = 0; $sy = 0; $sw = $src.Width; $sh = $src.Height
if ($W -gt 0 -and $H -gt 0) {
  $tw = $W; $th = $H
  if ($Contain) {
    $s = [Math]::Min($tw / $src.Width, $th / $src.Height)
    $dw = [int][Math]::Round($src.Width * $s); $dh = [int][Math]::Round($src.Height * $s)
    $dx = [int](($tw - $dw) / 2); $dy = [int](($th - $dh) / 2)
  } else {
    $s = [Math]::Max($tw / $src.Width, $th / $src.Height)
    $sw = $tw / $s; $sh = $th / $s
    $sx = ($src.Width - $sw) / 2; $sy = ($src.Height - $sh) / 2
    $dx = 0; $dy = 0; $dw = $tw; $dh = $th
  }
} else {
  $s = 1.0
  if ($MaxW -gt 0 -and $src.Width -gt $MaxW) { $s = $MaxW / $src.Width }
  $tw = [int][Math]::Round($src.Width * $s); $th = [int][Math]::Round($src.Height * $s)
  $dx = 0; $dy = 0; $dw = $tw; $dh = $th
}
$bmp = New-Object System.Drawing.Bitmap $tw, $th
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.Clear([System.Drawing.Color]::FromArgb(14, 15, 19))
$g.DrawImage($src, (New-Object System.Drawing.Rectangle $dx, $dy, $dw, $dh), $sx, $sy, $sw, $sh, [System.Drawing.GraphicsUnit]::Pixel)
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$ep = New-Object System.Drawing.Imaging.EncoderParameters 1
$ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), ([long]$Q)
$full = [System.IO.Path]::GetFullPath($Out)
$bmp.Save($full, $codec, $ep)
$g.Dispose(); $bmp.Dispose(); $src.Dispose()
"{0}  {1}x{2}  {3:N0} bytes" -f $full, $tw, $th, (Get-Item $full).Length
