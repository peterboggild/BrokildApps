# Measure feature boxes on the Battlestar Overdrive panel decal.
# Finds the dark CRT screen, and writes a gridded preview for reading off
# the pot centres by eye (an image generator hits no exact coordinate, so the
# geometry table is fixed here and the art is fitted to it).
param(
  [string]$Panel = "C:\Users\peter\Downloads\ChatGPT Image 18. sep. 2026, 19.41.58 (1).png",
  [string]$Out   = "C:\Users\peter\AppData\Local\Temp\claude\c--Users-peter-Dropbox-ACTIVITIES-00-VSCODE\8b3690e9-9f89-4a19-97bf-299f811b9f3d\scratchpad"
)
Add-Type -AssemblyName System.Drawing
$bm = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Panel))
$W = $bm.Width; $H = $bm.Height

# Lock bits for speed.
$rect = New-Object System.Drawing.Rectangle 0,0,$W,$H
$data = $bm.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $data.Stride
$bytes = New-Object byte[] ($stride * $H)
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
$bm.UnlockBits($data)

function Lum([int]$x, [int]$y) {
  $o = $y * $stride + $x * 4
  return (0.299 * $bytes[$o+2] + 0.587 * $bytes[$o+1] + 0.114 * $bytes[$o])
}

# --- the CRT screen: the big dark blob near the centre -----------------------
# Scan the central band only, so the starfield border cannot win.
$cx = [int]($W * 0.5); $cy = [int]($H * 0.5)
$x0 = $cx; while ($x0 -gt 0        -and (Lum $x0 $cy) -lt 70) { $x0-- }
$x1 = $cx; while ($x1 -lt ($W - 1) -and (Lum $x1 $cy) -lt 70) { $x1++ }
$y0 = $cy; while ($y0 -gt 0        -and (Lum $cx $y0) -lt 70) { $y0-- }
$y1 = $cy; while ($y1 -lt ($H - 1) -and (Lum $cx $y1) -lt 70) { $y1++ }
Write-Output ("screen glass  x {0}..{1}  y {2}..{3}   ({4} x {5})" -f ($x0+1), ($x1-1), ($y0+1), ($y1-1), ($x1-$x0-1), ($y1-$y0-1))
Write-Output ("  as fraction  left {0:N4} top {1:N4} w {2:N4} h {3:N4}" -f (($x0+1)/$W), (($y0+1)/$H), (($x1-$x0-1)/$W), (($y1-$y0-1)/$H))

# --- gridded preview ---------------------------------------------------------
$g = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Panel))
$gr = [System.Drawing.Graphics]::FromImage($g)
$penMinor = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(110, 0, 255, 255)), 1
$penMajor = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(220, 0, 255, 60)), 2
$font = New-Object System.Drawing.Font "Consolas", 13, ([System.Drawing.FontStyle]::Bold)
$brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 0, 255, 60))
for ($x = 0; $x -lt $W; $x += 32) {
  $gr.DrawLine($(if ($x % 128 -eq 0) { $penMajor } else { $penMinor }), $x, 0, $x, $H)
  if ($x % 128 -eq 0) { $gr.DrawString("$x", $font, $brush, ($x + 2), 2) }
}
for ($y = 0; $y -lt $H; $y += 32) {
  $gr.DrawLine($(if ($y % 128 -eq 0) { $penMajor } else { $penMinor }), 0, $y, $W, $y)
  if ($y % 128 -eq 0) { $gr.DrawString("$y", $font, $brush, 2, ($y + 2)) }
}
$gr.Dispose()
$dst = Join-Path $Out "panel-grid.png"
$g.Save($dst, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bm.Dispose()
Write-Output "grid -> $dst"
