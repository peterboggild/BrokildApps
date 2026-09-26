# Cut the push buttons from assets/drum-decals/05-buttons.png (delivered
# 2026-09-26): palm HIT up / pressed, emergency STOP up / pressed, left to right.
#
# Unlike the first delivery these four are drawn head-on on identical plates,
# so each pressed drawing is REGISTERED with its up drawing and can simply
# replace it while the button is held. That is checked here, not assumed:
# the plate masks of each pair must line up best at zero shift.
#
# Every part is cut at the SAME box size (the plates are 482 x 453) and kept
# at that aspect, so up and pressed stay interchangeable pixel for pixel.
#
# ASCII only: Windows PowerShell 5.1 reads a no-BOM .ps1 as ANSI.
param(
  [string]$Src = "$PSScriptRoot\..\..\assets\drum-decals\05-buttons.png",
  [string]$Out = "$PSScriptRoot\art"
)
Add-Type -AssemblyName System.Drawing
$sheet = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Src))

# plate boxes measured from the alpha: x starts, all rows 137..589
$starts = @(33, 575, 1117, 1658)
$bw = 486; $bh = 457; $top = 135            # 2 px transparent margin round a 482 x 453 plate

function Crop($x0) {
  $bm = New-Object System.Drawing.Bitmap $bw, $bh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bm)
  $g.DrawImage($sheet, (New-Object System.Drawing.Rectangle 0, 0, $bw, $bh), (New-Object System.Drawing.Rectangle ($x0 - 2), $top, $bw, $bh), [System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose(); return $bm
}
function Mask($bm) {
  $m = New-Object bool[] ($bw * $bh)
  for ($y = 0; $y -lt $bh; $y++) { for ($x = 0; $x -lt $bw; $x++) { $m[$y * $bw + $x] = $bm.GetPixel($x, $y).A -gt 128 } }
  return ,$m
}
function Mismatch($ma, $mb, $dx, $dy) {
  $n = 0
  for ($y = 8; $y -lt $bh - 8; $y += 2) { for ($x = 8; $x -lt $bw - 8; $x += 2) {
    if ($ma[$y * $bw + $x] -ne $mb[($y + $dy) * $bw + ($x + $dx)]) { $n++ } } }
  return $n
}
function Scale($bm, $w) {
  $h = [int][Math]::Round($w * $bh / $bw)
  $o = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($o)
  $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'; $g.CompositingMode = 'SourceCopy'
  $g.DrawImage($bm, 0, 0, $w, $h); $g.Dispose(); return $o
}

$parts = @(@('hit.png', 320), @('hit-down.png', 320), @('estop.png', 256), @('estop-down.png', 256))
$crops = @(); foreach ($x0 in $starts) { $crops += ,(Crop $x0) }

# registration: up against pressed, best shift must be (0,0)
foreach ($pair in @(@(0, 1, 'HIT'), @(2, 3, 'STOP'))) {
  $ma = Mask $crops[$pair[0]]; $mb = Mask $crops[$pair[1]]
  $best = $null
  foreach ($dy in -2..2) { foreach ($dx in -2..2) {
    $v = Mismatch $ma $mb $dx $dy
    if ($best -eq $null -or $v -lt $best[0]) { $best = @($v, $dx, $dy) } } }
  $zero = Mismatch $ma $mb 0 0
  "  {0,-5} plate masks: best shift ({1},{2}) with {3} mismatched samples; at (0,0) {4}" -f $pair[2], $best[1], $best[2], $best[0], $zero
  if ($best[1] -ne 0 -or $best[2] -ne 0) { throw "$($pair[2]) up and pressed are NOT registered" }
}

for ($i = 0; $i -lt 4; $i++) {
  $o = Scale $crops[$i] $parts[$i][1]
  $p = Join-Path $Out $parts[$i][0]
  $o.Save($p, [System.Drawing.Imaging.ImageFormat]::Png)
  "  {0,-16} {1} x {2}  {3:N0} KB" -f $parts[$i][0], $o.Width, $o.Height, ((Get-Item $p).Length / 1KB)
  $o.Dispose()
}
foreach ($c in $crops) { $c.Dispose() }
$sheet.Dispose()
