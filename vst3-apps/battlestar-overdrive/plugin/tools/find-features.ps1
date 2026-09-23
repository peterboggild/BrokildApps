# Locate the six pot shafts, the fuel tube and the Autorefill hole on the panel.
#
# The pots are bright, DESATURATED metal on a strongly saturated yellow/orange
# field, so a saturation test separates them cleanly where a luminance test
# alone would also catch the yellow. The fuel tube and the button hole are
# found as dark/among-the-darkest runs inside their own search boxes.
param(
  [string]$Panel = "C:\Users\peter\Downloads\ChatGPT Image 18. sep. 2026, 19.41.58 (1).png"
)
Add-Type -AssemblyName System.Drawing
$bm = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Panel))
$W = $bm.Width; $H = $bm.Height
$rect = New-Object System.Drawing.Rectangle 0,0,$W,$H
$d = $bm.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $d.Stride
$px = New-Object byte[] ($stride * $H)
[System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $px, 0, $px.Length)
$bm.UnlockBits($d); $bm.Dispose()

function Centroid($bx, $by, $bw, $bh, [double]$satMax, [double]$valMin) {
  $sx = 0.0; $sy = 0.0; $n = 0
  for ($y = $by; $y -lt ($by + $bh); $y++) {
    $row = $y * $stride
    for ($x = $bx; $x -lt ($bx + $bw); $x++) {
      $o = $row + $x * 4
      $b = $px[$o]; $g = $px[$o+1]; $r = $px[$o+2]
      $mx = [Math]::Max($r, [Math]::Max($g, $b)); $mn = [Math]::Min($r, [Math]::Min($g, $b))
      if ($mx -lt 1) { continue }
      $sat = ($mx - $mn) / $mx
      if ($sat -le $satMax -and $mx -ge $valMin) { $sx += $x; $sy += $y; $n++ }
    }
  }
  if ($n -eq 0) { return $null }
  return @{ x = [Math]::Round($sx / $n, 1); y = [Math]::Round($sy / $n, 1); n = $n }
}

# Search boxes read off the 128px grid, generous enough to hold the whole pot
# but tight enough to exclude the chrome screen bezel and the housing rails.
$pots = @(
  @{ name = "MIX";        bx = 210; by = 275; bw = 130; bh = 118 }
  @{ name = "THRUST";     bx = 210; by = 462; bw = 130; bh = 118 }
  @{ name = "ANTITHRUST"; bx = 210; by = 655; bw = 130; bh = 118 }
  @{ name = "ENGINE";     bx = 1198; by = 275; bw = 130; bh = 118 }
  @{ name = "SPACE";      bx = 1198; by = 462; bw = 130; bh = 118 }
  @{ name = "SPECTRUM";   bx = 1198; by = 655; bw = 130; bh = 118 }
)
Write-Output "pot shafts (desaturated metal centroid):"
foreach ($p in $pots) {
  $c = Centroid $p.bx $p.by $p.bw $p.bh 0.34 105
  if ($c) {
    Write-Output ("  {0,-11} centre {1,7} , {2,7}   frac {3:N5} , {4:N5}   px {5}" -f `
      $p.name, $c.x, $c.y, ($c.x / $W), ($c.y / $H), $c.n)
  } else { Write-Output ("  {0,-11} NOT FOUND" -f $p.name) }
}

# --- fuel tube: the orange liquid column is the most saturated thing low down -
$bx = 400; $by = 770; $bw = 600; $bh = 140
$minx = 99999; $maxx = -1; $miny = 99999; $maxy = -1
for ($y = $by; $y -lt ($by + $bh); $y++) {
  $row = $y * $stride
  for ($x = $bx; $x -lt ($bx + $bw); $x++) {
    $o = $row + $x * 4
    $b = $px[$o]; $g = $px[$o+1]; $r = $px[$o+2]
    # bright amber: red high, blue low
    if ($r -gt 150 -and $b -lt ($r - 90) -and $g -gt 60) {
      if ($x -lt $minx) { $minx = $x }; if ($x -gt $maxx) { $maxx = $x }
      if ($y -lt $miny) { $miny = $y }; if ($y -gt $maxy) { $maxy = $y }
    }
  }
}
Write-Output ("fuel amber   x {0}..{1}  y {2}..{3}   ({4} x {5})" -f $minx, $maxx, $miny, $maxy, ($maxx-$minx+1), ($maxy-$miny+1))
Write-Output ("  as fraction left {0:N5} top {1:N5} w {2:N5} h {3:N5}" -f ($minx/$W), ($miny/$H), (($maxx-$minx+1)/$W), (($maxy-$miny+1)/$H))

# --- Autorefill hole: darkest region in its own box --------------------------
$bx = 985; $by = 780; $bw = 120; $bh = 110
$minx = 99999; $maxx = -1; $miny = 99999; $maxy = -1
for ($y = $by; $y -lt ($by + $bh); $y++) {
  $row = $y * $stride
  for ($x = $bx; $x -lt ($bx + $bw); $x++) {
    $o = $row + $x * 4
    $lum = 0.299 * $px[$o+2] + 0.587 * $px[$o+1] + 0.114 * $px[$o]
    if ($lum -lt 48) {
      if ($x -lt $minx) { $minx = $x }; if ($x -gt $maxx) { $maxx = $x }
      if ($y -lt $miny) { $miny = $y }; if ($y -gt $maxy) { $maxy = $y }
    }
  }
}
Write-Output ("button hole  x {0}..{1}  y {2}..{3}   ({4} x {5})  centre {6},{7}" -f `
  $minx, $maxx, $miny, $maxy, ($maxx-$minx+1), ($maxy-$miny+1), [Math]::Round(($minx+$maxx)/2,1), [Math]::Round(($miny+$maxy)/2,1))
Write-Output ("  centre frac {0:N5} , {1:N5}" -f ((($minx+$maxx)/2)/$W), ((($miny+$maxy)/2)/$H))
