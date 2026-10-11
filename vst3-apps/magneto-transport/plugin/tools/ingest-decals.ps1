# Check a decal delivery against docs/decal_brief.md BEFORE anything is wired in.
#
#   powershell -ExecutionPolicy Bypass -File tools\ingest-decals.ps1 -From <folder> [-Copy]
#
# Per part: present, canvas aspect against the brief, the drawing's bounding box
# against its canvas (a drawing in a strip of a larger canvas is a deviation),
# alpha (outside 0, inside opaque) for transparent parts, edge continuity for
# tiling textures, registration for pairs. -Copy puts the parts that pass into
# assets/decals/ (the canvas untouched: a rotating part keeps its pivot).
# Report only otherwise. ASCII only (Windows PowerShell 5.1 reads ANSI).

param([Parameter(Mandatory = $true)][string] $From, [switch] $Copy)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# name, canvas w, canvas h, kind (T transparent, O opaque, S semi-transparent), tile (none|xy|x), fill (min share of canvas the drawing must span on its long axis)
$parts = @(
  @("mt-deck", 1024, 1024, "O", "xy", 1.0), @("mt-steel", 1024, 1024, "O", "xy", 1.0), @("mt-plate", 1024, 1024, "O", "xy", 1.0),
  @("mt-header", 2048, 128, "O", "x", 1.0),
  @("mt-reel-flange", 1600, 1600, "T", "none", 0.97), @("mt-reel-hub", 1024, 1024, "T", "none", 0.96),
  @("mt-capstan", 256, 256, "T", "none", 0.93), @("mt-pinch", 512, 512, "T", "none", 0.95), @("mt-roller", 256, 256, "T", "none", 0.93),
  @("mt-arm", 512, 512, "T", "none", 0.45),
  @("mt-head-erase", 352, 544, "T", "none", 0.95), @("mt-head-record", 352, 544, "T", "none", 0.95), @("mt-head-play", 352, 544, "T", "none", 0.95),
  @("mt-carriage", 416, 480, "T", "none", 0.95), @("mt-rail", 2048, 64, "T", "x", 1.0),
  @("mt-headplate", 1520, 332, "O", "none", 1.0), @("mt-looptank", 1320, 1200, "S", "none", 1.0),
  @("mt-knob", 512, 512, "T", "none", 0.62),
  @("mt-key-up", 192, 96, "O", "none", 1.0), @("mt-key-down", 192, 96, "O", "none", 1.0),
  @("mt-button-up", 192, 96, "O", "none", 1.0), @("mt-button-down", 192, 96, "O", "none", 1.0),
  @("mt-lamp-amber-off", 128, 128, "T", "none", 0.6), @("mt-lamp-amber-on", 128, 128, "T", "none", 0.6),
  @("mt-lamp-red-off", 128, 128, "T", "none", 0.6), @("mt-lamp-red-on", 128, 128, "T", "none", 0.6),
  @("mt-lamp-green-off", 128, 128, "T", "none", 0.6), @("mt-lamp-green-on", 128, 128, "T", "none", 0.6),
  @("mt-slider-track", 1408, 80, "O", "none", 1.0), @("mt-slider-thumb", 192, 320, "T", "none", 0.9),
  @("mt-screw", 128, 128, "T", "none", 0.9),
  @("mt-vu-face", 896, 448, "O", "none", 1.0), @("mt-vu-needle", 64, 512, "T", "none", 0.9),
  @("mt-plaque-left", 896, 832, "O", "none", 1.0), @("mt-plaque-right", 896, 832, "O", "none", 1.0),
  @("mt-wordmark", 2000, 240, "T", "none", 0.95), @("mt-glass", 384, 192, "S", "none", 1.0)
)
$pairs = @(@("mt-key-up", "mt-key-down"), @("mt-button-up", "mt-button-down"),
           @("mt-lamp-amber-off", "mt-lamp-amber-on"), @("mt-lamp-red-off", "mt-lamp-red-on"), @("mt-lamp-green-off", "mt-lamp-green-on"))

function Read-Px([string] $path) {
  $bm = New-Object System.Drawing.Bitmap $path
  $w = $bm.Width; $h = $bm.Height
  $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
  $data = $bm.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $bytes = New-Object byte[] ($data.Stride * $h)
  [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
  $stride = $data.Stride
  $bm.UnlockBits($data); $bm.Dispose()
  return @{ W = $w; H = $h; B = $bytes; S = $stride }
}

# the drawing's bounding box: alpha > 8 (transparent parts), or differing from the corner colour (opaque parts)
function Get-Box($px, [string] $kind) {
  $b = $px.B; $st = $px.S
  $c0 = @($b[0], $b[1], $b[2])
  $x0 = $px.W; $y0 = $px.H; $x1 = -1; $y1 = -1
  for ($y = 0; $y -lt $px.H; $y += 2) {
    $row = $y * $st
    for ($x = 0; $x -lt $px.W; $x += 2) {
      $i = $row + 4 * $x
      $on = $false
      if ($kind -eq "O") { $on = ([math]::Abs($b[$i] - $c0[0]) + [math]::Abs($b[$i + 1] - $c0[1]) + [math]::Abs($b[$i + 2] - $c0[2])) -gt 24 }
      else { $on = $b[$i + 3] -gt 8 }
      if ($on) { if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }; if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y } }
    }
  }
  if ($x1 -lt 0) { return $null }
  return @{ X = $x0; Y = $y0; W = $x1 - $x0 + 2; H = $y1 - $y0 + 2 }
}

function Get-AlphaStats($px) {
  $b = $px.B; $st = $px.S; $n = 0; $zero = 0; $full = 0; $sum = 0.0
  for ($y = 0; $y -lt $px.H; $y += 3) { $row = $y * $st
    for ($x = 0; $x -lt $px.W; $x += 3) { $a = $b[$row + 4 * $x + 3]; $n++; $sum += $a; if ($a -eq 0) { $zero++ }; if ($a -ge 250) { $full++ } } }
  return @{ Zero = $zero / $n; Full = $full / $n; Mean = $sum / $n }
}

# mean RGB difference between the first and last columns (x) and rows (y), against neighbouring columns inside
function Get-EdgeJump($px, [string] $axis) {
  $b = $px.B; $st = $px.S
  $edge = 0.0; $inner = 0.0; $n = 0
  if ($axis -eq "x") {
    for ($y = 0; $y -lt $px.H; $y += 2) { $row = $y * $st
      $iL = $row; $iR = $row + 4 * ($px.W - 1); $iM = $row + 4 * [int]($px.W / 2); $iM2 = $iM + 4
      for ($c = 0; $c -lt 3; $c++) { $edge += [math]::Abs($b[$iL + $c] - $b[$iR + $c]); $inner += [math]::Abs($b[$iM + $c] - $b[$iM2 + $c]) }
      $n++ }
  } else {
    for ($x = 0; $x -lt $px.W; $x += 2) {
      $iT = 4 * $x; $iB = ($px.H - 1) * $st + 4 * $x; $iM = [int]($px.H / 2) * $st + 4 * $x; $iM2 = $iM + $st
      for ($c = 0; $c -lt 3; $c++) { $edge += [math]::Abs($b[$iT + $c] - $b[$iB + $c]); $inner += [math]::Abs($b[$iM + $c] - $b[$iM2 + $c]) }
      $n++ }
  }
  return @{ Edge = $edge / (3 * $n); Inner = $inner / (3 * $n) }
}

$destDir = Join-Path (Split-Path -Parent $PSScriptRoot) "assets\decals"
if ($Copy -and -not (Test-Path $destDir)) { New-Item -ItemType Directory -Force $destDir | Out-Null }
$boxes = @{}; $pass = 0; $fail = 0; $missing = 0
foreach ($p in $parts) {
  $name = $p[0]; $cw = $p[1]; $ch = $p[2]; $kind = $p[3]; $tile = $p[4]; $fillMin = $p[5]
  $file = Join-Path $From ($name + ".png")
  if (-not (Test-Path $file)) { "{0,-20} MISSING" -f $name; $missing++; continue }
  $px = Read-Px $file
  $notes = @(); $ok = $true
  $aspWant = $cw / $ch; $aspGot = $px.W / $px.H
  if ([math]::Abs($aspGot / $aspWant - 1) -gt 0.02) { $ok = $false; $notes += ("aspect {0:N3} wanted {1:N3}" -f $aspGot, $aspWant) }
  $box = Get-Box $px $kind
  # an opaque part of one even colour has nothing that differs from its corner: it fills its canvas
  if ($null -eq $box -and $kind -eq "O") { $box = @{ X = 0; Y = 0; W = $px.W; H = $px.H } }
  if ($null -eq $box) { $ok = $false; $notes += "EMPTY" }
  else {
    $boxes[$name] = $box
    $span = [math]::Max($box.W / $px.W, $box.H / $px.H)
    if ($kind -ne "O" -and $span -lt $fillMin) { $ok = $false; $notes += ("drawing spans {0:P0} of the canvas, wanted {1:P0}" -f $span, $fillMin) }
    $notes += ("box {0},{1},{2},{3}" -f $box.X, $box.Y, $box.W, $box.H)
  }
  if ($kind -eq "T") {
    $a = Get-AlphaStats $px
    if ($a.Zero -lt 0.02) { $ok = $false; $notes += ("alpha 0 on only {0:P1} (no real transparency?)" -f $a.Zero) }
    $notes += ("alpha 0 {0:P0}, opaque {1:P0}" -f $a.Zero, $a.Full)
  } elseif ($kind -eq "S") {
    $a = Get-AlphaStats $px
    if ($a.Mean -lt 20 -or $a.Mean -gt 235) { $ok = $false }
    $notes += ("mean alpha {0:N0}" -f $a.Mean)
  } else {
    $a = Get-AlphaStats $px
    if ($a.Full -lt 0.98) { $ok = $false; $notes += ("opaque part only {0:P0} opaque" -f $a.Full) }
  }
  if ($tile -ne "none") {
    foreach ($ax in @("x", "y")) {
      if ($tile -eq "x" -and $ax -eq "y") { continue }
      $j = Get-EdgeJump $px $ax
      if ($j.Edge -gt 3.0 * $j.Inner + 4.0) { $ok = $false; $notes += ("seam on {0}: {1:N1} vs {2:N1} inside" -f $ax, $j.Edge, $j.Inner) }
      else { $notes += ("tiles on {0}" -f $ax) }
    }
  }
  if ($ok) { $pass++ } else { $fail++ }
  "{0,-20} {1}  {2}x{3}  {4}" -f $name, ($(if ($ok) { "ok  " } else { "FAIL" })), $px.W, $px.H, ($notes -join "; ")
  if ($Copy -and $ok) { Copy-Item $file (Join-Path $destDir ($name + ".png")) -Force }
}
foreach ($pr in $pairs) {
  $a = $boxes[$pr[0]]; $b = $boxes[$pr[1]]
  if ($null -eq $a -or $null -eq $b) { continue }
  $d = [math]::Max([math]::Max([math]::Abs($a.X - $b.X), [math]::Abs($a.Y - $b.Y)), [math]::Max([math]::Abs($a.W - $b.W), [math]::Abs($a.H - $b.H)))
  "{0} / {1}: {2}  (boxes differ by up to {3} px)" -f $pr[0], $pr[1], ($(if ($d -le 2) { "registered" } else { "NOT REGISTERED" })), $d
  if ($d -gt 2) { $fail++ }
}
""
"{0} pass, {1} fail, {2} missing" -f $pass, $fail, $missing
