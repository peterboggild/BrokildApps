# PNG -> JPEG for manual plates and the landing page: quality 88, optional
# max width, composited onto the page colour because JPEG has no alpha.
#   powershell -File tools\tojpg.ps1 -In a.png -Out a.jpg [-MaxWidth 1700] [-Back "#0a141c"]
param([string] $In, [string] $Out, [int] $MaxWidth = 1700, [string] $Back = "#0a141c")
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Image]::FromFile($In)
$w = $src.Width; $h = $src.Height
if ($w -gt $MaxWidth) { $h = [int]($h * $MaxWidth / $w); $w = $MaxWidth }
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.Clear([System.Drawing.ColorTranslator]::FromHtml($Back))
$g.DrawImage($src, 0, 0, $w, $h)
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
$ep = New-Object System.Drawing.Imaging.EncoderParameters 1
$ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, [long]88)
$bmp.Save($Out, $codec, $ep)
$g.Dispose(); $bmp.Dispose(); $src.Dispose()
Write-Output ("  " + (Split-Path $Out -Leaf) + "  " + $w + "x" + $h)
