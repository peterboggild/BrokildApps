# How opaque is each fuel frame? An "empty" tube is still GLASS, and glass that
# is translucent lets the panel's own full tube show straight through the
# overlay - which renders as a full tank however the frame index is set.
param([string]$Strip = "$PSScriptRoot\..\Source\ui\art\fuel.png")
Add-Type -AssemblyName System.Drawing
$bm = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Strip))
$W = $bm.Width; $H = $bm.Height
$r = New-Object System.Drawing.Rectangle 0,0,$W,$H
$d = $bm.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $d.Stride
$px = New-Object byte[] ($stride * $H)
[System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $px, 0, $px.Length)
$bm.UnlockBits($d); $bm.Dispose()

$cell = [int]($W / 15)
Write-Output ("strip {0} x {1}, cell {2}" -f $W, $H, $cell)
Write-Output "frame   mean alpha   min alpha (over the middle band)"
for ($i = 0; $i -lt 15; $i++) {
  $sum = 0.0; $n = 0; $min = 255
  # middle band only: the rounded ends are legitimately transparent
  for ($y = [int]($H*0.25); $y -lt [int]($H*0.75); $y++) {
    $row = $y * $stride
    for ($x = $i*$cell + [int]($cell*0.15); $x -lt $i*$cell + [int]($cell*0.85); $x++) {
      $a = $px[$row + $x*4 + 3]
      $sum += $a; $n++
      if ($a -lt $min) { $min = $a }
    }
  }
  Write-Output ("{0,4}   {1,10:N1}   {2,6}" -f $i, ($sum/$n), $min)
}
