# Crop a region of an image, scale it up and draw a fine labelled grid on it,
# so coordinates can be read off a detail by eye. Grid labels are in ORIGINAL
# image coordinates, not crop coordinates.
param(
  [Parameter(Mandatory=$true)][string]$Image,
  [Parameter(Mandatory=$true)][int]$X,
  [Parameter(Mandatory=$true)][int]$Y,
  [Parameter(Mandatory=$true)][int]$CropW,
  [Parameter(Mandatory=$true)][int]$CropH,
  [double]$Scale = 3.0,
  [int]$Step = 16,
  [int]$Major = 64,
  [string]$Out = "C:\Users\peter\AppData\Local\Temp\claude\c--Users-peter-Dropbox-ACTIVITIES-00-VSCODE\8b3690e9-9f89-4a19-97bf-299f811b9f3d\scratchpad\crop.png",
  [switch]$OnWhite     # composite a transparent source on white before cropping
)
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Image))

$dw = [int]($CropW * $Scale); $dh = [int]($CropH * $Scale)
$dst = New-Object System.Drawing.Bitmap $dw, $dh
$g = [System.Drawing.Graphics]::FromImage($dst)
if ($OnWhite) { $g.Clear([System.Drawing.Color]::White) }
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$srcRect = New-Object System.Drawing.Rectangle $X, $Y, $CropW, $CropH
$dstRect = New-Object System.Drawing.Rectangle 0, 0, $dw, $dh
$g.DrawImage($src, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)

$penMinor = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(120, 0, 255, 255)), 1
$penMajor = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(230, 255, 0, 200)), 2
$font = New-Object System.Drawing.Font "Consolas", 12, ([System.Drawing.FontStyle]::Bold)
$brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 255, 0, 200))
$halo  = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(190, 0, 0, 0))

for ($ox = [int][Math]::Ceiling($X / [double]$Step) * $Step; $ox -lt ($X + $CropW); $ox += $Step) {
  $sx = ($ox - $X) * $Scale
  $isMaj = ($ox % $Major -eq 0)
  $g.DrawLine($(if ($isMaj) { $penMajor } else { $penMinor }), $sx, 0, $sx, $dh)
  if ($isMaj) { $g.FillRectangle($halo, $sx, 0, 46, 17); $g.DrawString("$ox", $font, $brush, ($sx + 1), 1) }
}
for ($oy = [int][Math]::Ceiling($Y / [double]$Step) * $Step; $oy -lt ($Y + $CropH); $oy += $Step) {
  $sy = ($oy - $Y) * $Scale
  $isMaj = ($oy % $Major -eq 0)
  $g.DrawLine($(if ($isMaj) { $penMajor } else { $penMinor }), 0, $sy, $dw, $sy)
  if ($isMaj) { $g.FillRectangle($halo, 0, $sy, 46, 17); $g.DrawString("$oy", $font, $brush, 1, ($sy + 1)) }
}
$g.Dispose()
$dst.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$dst.Dispose(); $src.Dispose()
Write-Output "$Out  ($dw x $dh, source region $X,$Y ${CropW}x${CropH} at ${Scale}x)"
