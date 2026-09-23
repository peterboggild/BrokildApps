# What fill level is each delivered fuel frame, actually?
#
# The strip is built by walking the sheet row-major (i%3, i/3) and that ORDER is
# an assumption. If the levels do not fall in even steps in that order, the
# gauge jumps. Measure the amber column in every frame and let the numbers say
# what the order should be.
param(
  [string]$Sheet = "C:\Users\peter\b\BattlestarOverdrive\assets\decals\fuel-sheet.png",
  [string]$Geo   = "C:\Users\peter\b\BattlestarOverdrive\assets\panel-geometry.json"
)
Add-Type -AssemblyName System.Drawing
$g = Get-Content $Geo -Raw | ConvertFrom-Json
$f = $g.sheets.fuel

$bm = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Sheet))
$W = $bm.Width; $H = $bm.Height
$r = New-Object System.Drawing.Rectangle 0,0,$W,$H
$d = $bm.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $d.Stride
$px = New-Object byte[] ($stride * $H)
[System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $px, 0, $px.Length)
$bm.UnlockBits($d); $bm.Dispose()

# Amber liquid is bright and strongly orange; empty glass is dark and grey.
# Scored per column over the tube's middle band, then the fill edge is the last
# column that still reads as liquid.
function FillFraction([int]$bx, [int]$by, [int]$bw, [int]$bh) {
  $y0 = $by + [int]($bh * 0.30); $y1 = $by + [int]($bh * 0.70)
  # ignore the rounded glass ends, which are dark in every frame
  $x0 = $bx + [int]($bw * 0.10); $x1 = $bx + [int]($bw * 0.92)
  $lastLiquid = -1
  for ($x = $x0; $x -lt $x1; $x++) {
    $amber = 0; $n = 0
    for ($y = $y0; $y -lt $y1; $y++) {
      $o = $y * $stride + $x * 4
      $b = $px[$o]; $gg = $px[$o+1]; $rr = $px[$o+2]
      $n++
      # bright and orange: red well above blue, and not dark
      if ($rr -gt 120 -and ($rr - $b) -gt 60) { $amber++ }
    }
    if ($n -gt 0 -and ($amber / $n) -gt 0.5) { $lastLiquid = $x }
  }
  if ($lastLiquid -lt 0) { return 0.0 }
  return [Math]::Round((($lastLiquid - $x0) / [double]($x1 - $x0)), 4)
}

Write-Output "frame  sheet cell        fill"
$rows = @()
for ($i = 0; $i -lt 15; $i++) {
  $col = $f.cols[$i % 3]; $row = $f.rows[[Math]::Floor($i / 3)]
  $fill = FillFraction $col $row $f.w $f.h
  $rows += [pscustomobject]@{ Index = $i; Col = ($i % 3); Row = [Math]::Floor($i / 3); Fill = $fill }
  Write-Output ("{0,4}   r{1} c{2}  ({3},{4})   {5,7:N4}" -f $i, [Math]::Floor($i/3), ($i%3), $col, $row, $fill)
}

Write-Output ""
Write-Output "in the order the strip is currently built (index 0 = full):"
$seq = ($rows | ForEach-Object { "{0:N2}" -f $_.Fill }) -join "  "
Write-Output "  $seq"
Write-Output "step-to-step change:"
$deltas = @()
for ($i = 1; $i -lt 15; $i++) { $deltas += ($rows[$i-1].Fill - $rows[$i].Fill) }
Write-Output ("  " + (($deltas | ForEach-Object { "{0:N3}" -f $_ }) -join "  "))
$mn = ($deltas | Measure-Object -Minimum).Minimum
$mx = ($deltas | Measure-Object -Maximum).Maximum
Write-Output ("  smallest step {0:N3}, largest step {1:N3}  -> ratio {2:N1}x" -f $mn, $mx, $(if ($mn -ne 0) { $mx / $mn } else { 999 }))

Write-Output ""
Write-Output "sorted by measured fill, fullest first (this is the order the strip SHOULD use):"
$sorted = $rows | Sort-Object -Property Fill -Descending
Write-Output ("  sheet indices: " + (($sorted | ForEach-Object { $_.Index }) -join ", "))
Write-Output ("  fills        : " + (($sorted | ForEach-Object { "{0:N2}" -f $_.Fill }) -join "  "))
$sd = @()
for ($i = 1; $i -lt 15; $i++) { $sd += ($sorted[$i-1].Fill - $sorted[$i].Fill) }
Write-Output ("  steps        : " + (($sd | ForEach-Object { "{0:N3}" -f $_ }) -join "  "))
$smn = ($sd | Measure-Object -Minimum).Minimum
$smx = ($sd | Measure-Object -Maximum).Maximum
Write-Output ("  smallest {0:N3}, largest {1:N3} -> ratio {2:N1}x" -f $smn, $smx, $(if ($smn -ne 0) { $smx / $smn } else { 999 }))
