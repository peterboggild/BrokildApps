# The landing page's images and the front-page preview, all from the manual plates
# (docs/manual/PLATES.md, "Landing page images"). ASCII only.
param(
  [string]$Plates = "$PSScriptRoot\..\docs\manual\img",
  [string]$Landing = "$PSScriptRoot\..\..\img",
  [string]$Preview = "$PSScriptRoot\..\..\..\..\assets\app-previews\magneto-transport.jpg",
  [int]$Quality = 88
)
Add-Type -AssemblyName System.Drawing
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$ep = New-Object System.Drawing.Imaging.EncoderParameters 1
$ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, [long]$Quality)
New-Item -ItemType Directory -Force -Path $Landing | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $Preview) | Out-Null

function Resample([string]$src, [string]$dst, [int]$w, [int]$h) {
  $s = [System.Drawing.Bitmap]::FromFile($src)
  if ($h -le 0) { $h = [int][Math]::Round($s.Height * $w / $s.Width) }
  $d = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $g = [System.Drawing.Graphics]::FromImage($d)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.Clear([System.Drawing.Color]::FromArgb(18, 21, 19))
  # cover: scale so the source fills the box, crop the overhang centred
  $k = [Math]::Max($w / $s.Width, $h / $s.Height)
  $sw = $s.Width * $k; $sh = $s.Height * $k
  $g.DrawImage($s, [single](($w - $sw) / 2), [single](($h - $sh) / 2), [single]$sw, [single]$sh)
  $g.Dispose(); $s.Dispose()
  $d.Save($dst, $codec, $ep); $d.Dispose()
  Write-Host ("  {0,-44} {1}x{2}  {3,5} KB" -f (Split-Path $dst -Leaf), $w, $h, [int]((Get-Item $dst).Length / 1KB))
}

Resample (Join-Path $Plates 'panel.jpg')        (Join-Path $Landing 'hero.jpg')         1900 0
Resample (Join-Path $Plates 'machine-loop.jpg') (Join-Path $Landing 'machine-loop.jpg') 1400 0
Resample (Join-Path $Plates 'mechanics.jpg')    (Join-Path $Landing 'mechanics.jpg')    848 0
Resample (Join-Path $Plates 'loop.jpg')         (Join-Path $Landing 'loop.jpg')         972 0
Resample (Join-Path $Plates 'card-bias.jpg')    (Join-Path $Landing 'card-bias.jpg')    832 0
Resample (Join-Path $Plates 'panel.jpg')        $Preview                                1280 720
