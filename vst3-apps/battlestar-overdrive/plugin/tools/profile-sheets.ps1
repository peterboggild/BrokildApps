# Profile-based part finder: sum alpha along rows and columns and cut the sheet
# at the gaps. Fixed bands guess where the parts are and contaminate each other
# when the guess is off (the first pass had row 3 swallowing the top of row 4);
# a profile asks the image where its parts actually are.
param(
  [Parameter(Mandatory=$true)][string]$Image,
  [int]$AlphaThr = 24,
  [int]$GapThr = 2          # a row/col with fewer than this many hits is a gap
)
Add-Type -AssemblyName System.Drawing
$bm = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Image))
$W = $bm.Width; $H = $bm.Height
$r = New-Object System.Drawing.Rectangle 0,0,$W,$H
$d = $bm.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $d.Stride
$px = New-Object byte[] ($stride * $H)
[System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $px, 0, $px.Length)
$bm.UnlockBits($d); $bm.Dispose()

$rowHits = New-Object int[] $H
$colHits = New-Object int[] $W
for ($y = 0; $y -lt $H; $y++) {
  $row = $y * $stride
  for ($x = 0; $x -lt $W; $x++) {
    if ($px[$row + $x * 4 + 3] -gt $AlphaThr) { $rowHits[$y]++; $colHits[$x]++ }
  }
}

function Runs($hits, $n) {
  $out = @(); $start = -1
  for ($i = 0; $i -lt $n; $i++) {
    if ($hits[$i] -ge $GapThr) { if ($start -lt 0) { $start = $i } }
    else { if ($start -ge 0) { $out += ,@($start, ($i - 1)); $start = -1 } }
  }
  if ($start -ge 0) { $out += ,@($start, ($n - 1)) }
  return $out
}

$rows = Runs $rowHits $H
$cols = Runs $colHits $W
Write-Output ("{0}  ({1} x {2})" -f (Split-Path $Image -Leaf), $W, $H)
Write-Output ("row bands : {0}" -f (($rows | ForEach-Object { "{0}..{1} (h {2})" -f $_[0], $_[1], ($_[1]-$_[0]+1) }) -join "  "))
Write-Output ("col bands : {0}" -f (($cols | ForEach-Object { "{0}..{1} (w {2})" -f $_[0], $_[1], ($_[1]-$_[0]+1) }) -join "  "))
Write-Output ""

# Exact box of each part = intersection of a row band and a col band, then
# tightened inside that cell (parts are not perfectly aligned to the grid).
$n = 0
foreach ($rb in $rows) {
  foreach ($cb in $cols) {
    $minx = 999999; $maxx = -1; $miny = 999999; $maxy = -1
    for ($y = $rb[0]; $y -le $rb[1]; $y++) {
      $row = $y * $stride
      for ($x = $cb[0]; $x -le $cb[1]; $x++) {
        if ($px[$row + $x * 4 + 3] -gt $AlphaThr) {
          if ($x -lt $minx) { $minx = $x }; if ($x -gt $maxx) { $maxx = $x }
          if ($y -lt $miny) { $miny = $y }; if ($y -gt $maxy) { $maxy = $y }
        }
      }
    }
    if ($maxx -lt 0) { continue }
    $n++
    $w = $maxx - $minx + 1; $h = $maxy - $miny + 1
    Write-Output ("part {0,2}  x {1,5}  y {2,5}  w {3,4}  h {4,4}   centre {5,7},{6,7}   aspect {7:N3}" -f `
      $n, $minx, $miny, $w, $h, [Math]::Round($minx + $w/2.0, 1), [Math]::Round($miny + $h/2.0, 1), ($w / [double]$h))
  }
}
