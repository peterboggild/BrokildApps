param([string] $In, [string] $Out, [int] $X = 0, [int] $Y = 0, [int] $W = 400, [int] $H = 100, [int] $Zoom = 3)
# Magnify a region of a screenshot, nearest-neighbour, so a detail can be read.
# (PowerShell variables are case-INSENSITIVE, so a local named $out would be
# the same variable as the -Out parameter and would eat the output path.)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$img = New-Object System.Drawing.Bitmap $In
$W = [int][math]::Min($W, $img.Width - $X)
$H = [int][math]::Min($H, $img.Height - $Y)
$ow = [int]($W * $Zoom); $oh = [int]($H * $Zoom)
$bmp = New-Object System.Drawing.Bitmap $ow, $oh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$dst = New-Object System.Drawing.Rectangle 0, 0, $ow, $oh
$g.DrawImage($img, $dst, $X, $Y, $W, $H, [System.Drawing.GraphicsUnit]::Pixel)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose(); $img.Dispose()
Write-Output ("wrote " + $Out + "  " + $ow + "x" + $oh)
