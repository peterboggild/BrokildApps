# The 1280 x 720 card preview for the front page, cut from the panel plate.
#
# The front page renders a card's preview at whatever width the grid gives it,
# so the crop has to say something at postage-stamp size: it takes the two
# VIEWS (the 3D room and the plan) and leaves the control row out, because a
# row of tiny sliders is unreadable there and the rooms are the product.
#
# PowerShell variables are CASE-INSENSITIVE, so a local $src is the same
# variable as the parameter $Src - and because that parameter is TYPED [string]
# the assignment silently coerces the Bitmap to its type name. That cost a
# round here. Locals below are named so they cannot collide.
#
# ASCII only - Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI.
param(
  [string]$Src  = "C:\Users\peter\b\ThinWalls\docs\manual\img\panel.jpg",
  [string]$Out  = "C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\assets\app-previews\thin-walls.jpg",
  [int]$W = 1280,
  [int]$H = 720,
  [int]$Quality = 88
)

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $Src)) { Write-Output "  NO SOURCE PLATE at $Src"; exit 1 }
New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null

$img = [System.Drawing.Image]::FromFile($Src)

# The panel plate is the whole window at 1400 x 900, which is 1.56 - taller
# than 16:9. Keep the FULL WIDTH (both views entire; cropping the sides takes
# the edge off the room and the edge off the plan) and take the 16:9 band from
# the top, so the card carries the header, both views and a sliver of the
# control row. Nothing is squeezed: the aspect is honoured by cropping.
$cropX = 0
$cropY = 0
$cropW = $img.Width
$cropH = [int]([Math]::Round($cropW * 9.0 / 16.0))
if ($cropH -gt $img.Height) {
  # a wider plate than 16:9: crop the sides instead, centred
  $cropH = $img.Height
  $cropW = [int]([Math]::Round($cropH * 16.0 / 9.0))
  $cropX = [int]([Math]::Round(($img.Width - $cropW) / 2.0))
}

$bmp = New-Object System.Drawing.Bitmap($W, $H, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$gfx.Clear([System.Drawing.Color]::FromArgb(13, 11, 10))
$gfx.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gfx.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$gfx.DrawImage($img, (New-Object System.Drawing.Rectangle(0, 0, $W, $H)),
                     $cropX, $cropY, $cropW, $cropH, [System.Drawing.GraphicsUnit]::Pixel)
$gfx.Dispose()

$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() |
         Where-Object { $_.MimeType -eq 'image/jpeg' }
$pars = New-Object System.Drawing.Imaging.EncoderParameters 1
$pars.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter(
                   [System.Drawing.Imaging.Encoder]::Quality, [int64]$Quality)
$bmp.Save($Out, $codec, $pars)
$bmp.Dispose()
$img.Dispose()

Write-Output ("  cropped {0},{1} {2}x{3} of the plate" -f $cropX, $cropY, $cropW, $cropH)
$chk = [System.Drawing.Image]::FromFile($Out)
Write-Output ("  preview {0}x{1}, {2} KB  ->  {3}" -f $chk.Width, $chk.Height, [int]((Get-Item $Out).Length / 1KB), $Out)
$chk.Dispose()
