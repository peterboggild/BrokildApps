# Turn a delivery into the runtime decals.
#
# Nothing here trusts the cover note. Every part is cropped to its own measured
# content box, because three deliveries in a row across this fleet arrived with
# the drawing sitting in a band of a much larger canvas; the two ground tiles
# are mirror-tiled into a seamless square whether or not they were certified
# seamless, because a visible seam repeating across a 1440x900 deck is the one
# thing a ground may never do; and the glass is scaled to the alpha it was
# ordered at, because an opaque "glass" hides the drawing it sits over (which
# is exactly what happened on 1984).
#
#   powershell -File tools\ingest-decals.ps1 -Src C:\Users\peter\Downloads
#
# The delivered files are identified by tools\decal-id.ps1 and named here, so
# a re-run is deterministic and the mapping is visible rather than positional.
param(
  [string]$Src  = "C:\Users\peter\Downloads",
  [string]$Tree = "$PSScriptRoot\.."
)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"

$MAP = [ordered]@{
  "panel"    = "ChatGPT Image 23. sep. 2026, 13.57.31 (1).png"
  "knob"     = "ChatGPT Image 23. sep. 2026, 13.57.31 (2).png"
  "plate"    = "ChatGPT Image 23. sep. 2026, 13.57.32 (3).png"
  "wordmark" = "ChatGPT Image 23. sep. 2026, 13.57.32 (4).png"
  "glass"    = "ChatGPT Image 23. sep. 2026, 13.57.33 (5).png"
  "screw"    = "ChatGPT Image 23. sep. 2026, 13.57.33 (6).png"
}
# target size, and how to treat it
$SPEC = @{
  "panel"    = @{ w =  512; h =  512; tile = $true;  lum = 0.135 }
  "plate"    = @{ w =  512; h =  512; tile = $true;  lum = 0.200 }
  "knob"     = @{ w =  384; h =  384; tile = $false }
  "glass"    = @{ w =  768; h =  384; tile = $false; alpha = 0.35 }
  "wordmark" = @{ w = 1536; h =    0; tile = $false }   # h 0 = keep the aspect
  "screw"    = @{ w =  192; h =  192; tile = $false }
}

$outDir = Join-Path $Tree "assets\decals"
$srcDir = Join-Path $Tree "assets\decals-src"
New-Item -ItemType Directory -Force -Path $outDir, $srcDir | Out-Null

function Get-Pixels($bmp) {
  $r = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
  $d = $bmp.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $buf = New-Object byte[] ($d.Stride * $bmp.Height)
  [System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $buf, 0, $buf.Length)
  $bmp.UnlockBits($d)
  return @{ buf = $buf; stride = $d.Stride; w = $bmp.Width; h = $bmp.Height }
}

function Content-Box($p) {
  # content = opaque AND not near-white, so an isolated part on a white page
  # and a part delivered on alpha are both measured correctly.
  $x0 = $p.w; $y0 = $p.h; $x1 = -1; $y1 = -1
  for ($y = 0; $y -lt $p.h; $y++) {
    $row = $y * $p.stride
    for ($x = 0; $x -lt $p.w; $x++) {
      $i = $row + $x * 4
      $a = $p.buf[$i+3]
      if ($a -le 24) { continue }
      $lum = 0.299*$p.buf[$i+2] + 0.587*$p.buf[$i+1] + 0.114*$p.buf[$i]
      if ($lum -ge 244) { continue }
      if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }
      if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y }
    }
  }
  if ($x1 -lt 0) { return $null }
  return New-Object System.Drawing.Rectangle $x0, $y0, ($x1-$x0+1), ($y1-$y0+1)
}

function Draw-Into($dst, $src, $rect, $dw, $dh, [bool]$flipX, [bool]$flipY, $ox, $oy) {
  $g = [System.Drawing.Graphics]::FromImage($dst)
  $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
  $clone = $src.Clone($rect, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  if ($flipX -and $flipY) { $clone.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipXY) }
  elseif ($flipX)         { $clone.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipX) }
  elseif ($flipY)         { $clone.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY) }
  $g.DrawImage($clone, (New-Object System.Drawing.Rectangle $ox, $oy, $dw, $dh))
  $clone.Dispose(); $g.Dispose()
}

foreach ($name in $MAP.Keys) {
  $file = Join-Path $Src $MAP[$name]
  if (-not (Test-Path $file)) { Write-Host ("MISSING  {0}  ({1})" -f $name, $MAP[$name]); continue }
  Copy-Item $file (Join-Path $srcDir ("{0}-delivered.png" -f $name)) -Force

  $bmp = [System.Drawing.Bitmap]::FromFile($file)
  $px  = Get-Pixels $bmp
  $box = Content-Box $px
  if ($box -eq $null) { Write-Host ("EMPTY    {0}" -f $name); $bmp.Dispose(); continue }
  $cov = [math]::Round(100.0 * $box.Width * $box.Height / ($px.w * $px.h), 1)

  $s = $SPEC[$name]
  $tw = $s.w
  $th = if ($s.h -gt 0) { $s.h } else { [int][math]::Round($s.w * $box.Height / $box.Width) }

  $dst = New-Object System.Drawing.Bitmap $tw, $th, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  if ($s.tile) {
    # Mirror-tiled: [orig | flipX ; flipY | flipXY]. Seamless by construction,
    # and on a fine irregular grain the mirror is invisible.
    $hw = [int]($tw / 2); $hh = [int]($th / 2)
    Draw-Into $dst $bmp $box $hw $hh $false $false 0   0
    Draw-Into $dst $bmp $box $hw $hh $true  $false $hw 0
    Draw-Into $dst $bmp $box $hw $hh $false $true  0   $hh
    Draw-Into $dst $bmp $box $hw $hh $true  $true  $hw $hh
  } else {
    Draw-Into $dst $bmp $box $tw $th $false $false 0 0
  }
  $bmp.Dispose()

  # ---- measured corrections on the finished tile ------------------------
  $dp = Get-Pixels $dst
  $note = ""
  $sum = 0.0; $n = 0; $asum = 0.0; $an = 0
  for ($y = 0; $y -lt $dp.h; $y += 2) {
    for ($x = 0; $x -lt $dp.w; $x += 2) {
      $i = $y * $dp.stride + $x * 4
      $a = $dp.buf[$i+3]
      if ($a -gt 24) { $sum += 0.299*$dp.buf[$i+2] + 0.587*$dp.buf[$i+1] + 0.114*$dp.buf[$i]; $n++ }
      if ($a -gt 8)  { $asum += $a; $an++ }
    }
  }
  $meanLum = if ($n) { $sum / $n / 255.0 } else { 0 }
  $meanA   = if ($an) { $asum / $an / 255.0 } else { 0 }

  $gain = 1.0
  if ($s.ContainsKey("lum") -and $meanLum -gt 0.001) { $gain = $s.lum / $meanLum }
  $ascale = 1.0
  if ($s.ContainsKey("alpha") -and $meanA -gt 0.001) { $ascale = $s.alpha / $meanA }
  if ([math]::Abs($gain - 1.0) -gt 0.02 -or [math]::Abs($ascale - 1.0) -gt 0.02) {
    for ($y = 0; $y -lt $dp.h; $y++) {
      $row = $y * $dp.stride
      for ($x = 0; $x -lt $dp.w; $x++) {
        $i = $row + $x * 4
        if ($gain -ne 1.0) {
          for ($k = 0; $k -lt 3; $k++) {
            $v = [int][math]::Round($dp.buf[$i+$k] * $gain)
            $dp.buf[$i+$k] = [byte][math]::Max(0, [math]::Min(255, $v))
          }
        }
        if ($ascale -ne 1.0) {
          $v = [int][math]::Round($dp.buf[$i+3] * $ascale)
          $dp.buf[$i+3] = [byte][math]::Max(0, [math]::Min(255, $v))
        }
      }
    }
    $r2 = New-Object System.Drawing.Rectangle 0, 0, $dp.w, $dp.h
    $d2 = $dst.LockBits($r2, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    [System.Runtime.InteropServices.Marshal]::Copy($dp.buf, 0, $d2.Scan0, $dp.buf.Length)
    $dst.UnlockBits($d2)
    if ($gain -ne 1.0)   { $note += (" lum x{0:N2} -> {1:P0}" -f $gain, $s.lum) }
    if ($ascale -ne 1.0) { $note += (" alpha x{0:N2} -> {1:P0}" -f $ascale, $s.alpha) }
  }

  $out = Join-Path $outDir ("{0}.png" -f $name)
  $dst.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
  $dst.Dispose()
  $kb = [math]::Round((Get-Item $out).Length / 1024, 0)
  "{0,-9} {1,4}x{2,-4} from content {3}x{4} ({5}% of canvas){6}{7}  {8} KB" -f `
     $name, $tw, $th, $box.Width, $box.Height, $cov, $(if ($s.tile) { "  mirror-tiled" } else { "" }), $note, $kb
}
""
"written to $outDir"
