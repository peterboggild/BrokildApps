# Analyse the knob sheet and the fuel sheet: is the alpha real, and where is
# each part? A part's DELIVERED size is not its drawn size (the High Tide
# lesson: parts come back letterboxed), so every box is measured, never assumed.
param(
  [string]$Knobs = "C:\Users\peter\Downloads\ChatGPT Image 18. sep. 2026, 19.41.58 (2).png",
  [string]$Fuel  = "C:\Users\peter\Downloads\ChatGPT Image 18. sep. 2026, 19.42.10.png"
)
Add-Type -AssemblyName System.Drawing

function Load($path) {
  $bm = [System.Drawing.Bitmap]::FromFile((Resolve-Path $path))
  $W = $bm.Width; $H = $bm.Height
  $r = New-Object System.Drawing.Rectangle 0,0,$W,$H
  $d = $bm.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $st = $d.Stride
  $px = New-Object byte[] ($st * $H)
  [System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $px, 0, $px.Length)
  $bm.UnlockBits($d); $bm.Dispose()
  return @{ px = $px; stride = $st; W = $W; H = $H }
}

function AlphaReport($im, $label) {
  $opaque = 0; $clear = 0; $partial = 0
  for ($y = 0; $y -lt $im.H; $y += 2) {
    $row = $y * $im.stride
    for ($x = 0; $x -lt $im.W; $x += 2) {
      $a = $im.px[$row + $x * 4 + 3]
      if ($a -ge 250) { $opaque++ } elseif ($a -le 5) { $clear++ } else { $partial++ }
    }
  }
  $tot = $opaque + $clear + $partial
  Write-Output ("{0}: alpha opaque {1:N1}%  clear {2:N1}%  partial {3:N1}%" -f `
    $label, (100.0*$opaque/$tot), (100.0*$clear/$tot), (100.0*$partial/$tot))
  return $clear -gt ($tot * 0.02)
}

# Content box inside a search window. "Content" = alpha if the image has real
# alpha, otherwise not-near-black (an opaque image on a black ground).
function Box($im, $bx, $by, $bw, $bh, $useAlpha, [int]$thr) {
  $minx = 999999; $maxx = -1; $miny = 999999; $maxy = -1
  for ($y = $by; $y -lt ($by + $bh) -and $y -lt $im.H; $y++) {
    $row = $y * $im.stride
    for ($x = $bx; $x -lt ($bx + $bw) -and $x -lt $im.W; $x++) {
      $o = $row + $x * 4
      $hit = $false
      if ($useAlpha) { $hit = $im.px[$o+3] -gt $thr }
      else {
        $lum = 0.299*$im.px[$o+2] + 0.587*$im.px[$o+1] + 0.114*$im.px[$o]
        $hit = $lum -gt $thr
      }
      if ($hit) {
        if ($x -lt $minx) { $minx = $x }; if ($x -gt $maxx) { $maxx = $x }
        if ($y -lt $miny) { $miny = $y }; if ($y -gt $maxy) { $maxy = $y }
      }
    }
  }
  if ($maxx -lt 0) { return $null }
  return @{ x = $minx; y = $miny; w = ($maxx - $minx + 1); h = ($maxy - $miny + 1) }
}

Write-Output "=== KNOB SHEET ==="
$k = Load $Knobs
$kAlpha = AlphaReport $k "knobs"
Write-Output ("using {0} for content" -f $(if ($kAlpha) { "ALPHA" } else { "LUMINANCE (opaque sheet)" }))
# 3 columns x 2 rows over the top ~78% of the sheet, then the button below.
$cw = [int]($k.W / 3); $rh = [int]($k.H * 0.39)
for ($r = 0; $r -lt 2; $r++) {
  for ($c = 0; $c -lt 3; $c++) {
    $b = Box $k ($c*$cw) ($r*$rh) $cw $rh $kAlpha $(if ($kAlpha) { 24 } else { 46 })
    if ($b) {
      $cx = $b.x + $b.w/2.0; $cy = $b.y + $b.h/2.0
      Write-Output ("  knob r{0}c{1}  box {2},{3} {4}x{5}   centre {6},{7}  aspect {8:N3}" -f `
        $r, $c, $b.x, $b.y, $b.w, $b.h, [Math]::Round($cx,1), [Math]::Round($cy,1), ($b.w/[double]$b.h))
    } else { Write-Output ("  knob r{0}c{1}  EMPTY" -f $r, $c) }
  }
}
$b = Box $k ([int]($k.W*0.33)) ([int]($k.H*0.76)) ([int]($k.W*0.34)) ([int]($k.H*0.24)) $kAlpha $(if ($kAlpha) { 24 } else { 46 })
if ($b) { Write-Output ("  BUTTON      box {0},{1} {2}x{3}   aspect {4:N3}" -f $b.x, $b.y, $b.w, $b.h, ($b.w/[double]$b.h)) }

Write-Output ""
Write-Output "=== FUEL SHEET ==="
$f = Load $Fuel
$fAlpha = AlphaReport $f "fuel"
Write-Output ("using {0} for content" -f $(if ($fAlpha) { "ALPHA" } else { "LUMINANCE (opaque sheet)" }))
$cw = [int]($f.W / 3); $rh = [int]($f.H / 5)
for ($r = 0; $r -lt 5; $r++) {
  for ($c = 0; $c -lt 3; $c++) {
    $b = Box $f ($c*$cw) ($r*$rh) $cw $rh $fAlpha $(if ($fAlpha) { 24 } else { 40 })
    if ($b) {
      Write-Output ("  tube {0,2}  box {1},{2} {3}x{4}  aspect {5:N3}" -f `
        ($r*3+$c+1), $b.x, $b.y, $b.w, $b.h, ($b.w/[double]$b.h))
    } else { Write-Output ("  tube {0,2}  EMPTY" -f ($r*3+$c+1)) }
  }
}
