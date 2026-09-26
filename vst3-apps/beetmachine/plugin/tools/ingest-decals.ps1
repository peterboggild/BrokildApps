# Cut Beetmachine's art out of the drum-decals delivery.
#
# Source: BrokildApps/assets/drum-decals (the delivery as it came, plus its
# manifest.json of crop rectangles). Output: plugin/art/, embedded by CMake
# (re-run configure afterwards - file(GLOB) runs at configure time).
#
# Parts are cropped to the ALPHA BOUNDS the manifest measured, squared where
# the part is round, and scaled to about twice their size on screen: enough
# for a HiDPI display, small enough not to bloat the plug-in.
#
# Deliberately NOT used: the switch/button "pressed" drawings. The delivery
# note says their housings are separate drawings and not pixel-identical, so
# swapping them would make a button jump. The panel presses the UP drawing in
# code instead. The lamp pairs ARE registered (same bezel), so both are used.
#
# ASCII only: Windows PowerShell 5.1 reads a no-BOM .ps1 as ANSI.
param(
  [string]$Src = "$PSScriptRoot\..\..\..\..\assets\drum-decals",
  [string]$Out = "$PSScriptRoot\..\art"
)
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $Out | Out-Null
$man = Get-Content (Join-Path $Src "manifest.json") -Raw | ConvertFrom-Json

function Rect($s) { $a = $s -split ' ' | % { [int]$_ }; New-Object System.Drawing.Rectangle $a[0], $a[1], ($a[2] - $a[0]), ($a[3] - $a[1]) }
function Part($sheet, $name) {
  $it = $man.$sheet.items | ? { $_.name -eq $name }
  if (-not $it) { throw "no part '$name' in $sheet" }
  if ($it.alpha_bounds -and $it.alpha_bounds -ne 'empty / opaque') { return Rect $it.alpha_bounds }
  return Rect $it.crop
}
function Save($bmp, $file) {
  $p = Join-Path $Out $file
  if ($file -like '*.jpg') {
    $enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | ? { $_.MimeType -eq 'image/jpeg' }
    $ps = New-Object System.Drawing.Imaging.EncoderParameters 1
    $ps.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), ([long]88)
    $bmp.Save($p, $enc, $ps)
  } else { $bmp.Save($p, [System.Drawing.Imaging.ImageFormat]::Png) }
  "  {0,-22} {1,4} x {2,-4} {3,6:N0} KB" -f $file, $bmp.Width, $bmp.Height, ((Get-Item $p).Length / 1KB)
}
# crop r out of sheet; square=true pads to a centred square; scale to w x h
function Cut($sheet, $name, $file, $w, $h, [switch]$Square) {
  $img = [System.Drawing.Bitmap]::FromFile((Join-Path $Src $sheet))
  $r = Part $sheet $name
  if ($Square) {
    $side = [Math]::Max($r.Width, $r.Height)
    $r = New-Object System.Drawing.Rectangle ($r.X + [int](($r.Width - $side) / 2)), ($r.Y + [int](($r.Height - $side) / 2)), $side, $side
  }
  $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
  $g.Clear([System.Drawing.Color]::Transparent)
  $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $w, $h), $r, [System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose(); $img.Dispose()
  Save $bmp $file; $bmp.Dispose()
}

"`nBeetmachine art -> $Out`n"
# the cabinet: tiling ground, and the machine bay (drawn nine-slice by the panel)
$gimg = [System.Drawing.Bitmap]::FromFile((Join-Path $Src "beet-ground.png"))
$gb = New-Object System.Drawing.Bitmap 512, 512
$gg = [System.Drawing.Graphics]::FromImage($gb); $gg.InterpolationMode = 'HighQualityBicubic'
$gg.DrawImage($gimg, 0, 0, 512, 512); $gg.Dispose(); $gimg.Dispose()
Save $gb "ground.jpg"; $gb.Dispose()
Cut "beet-machine-bay.png" "machine-bay" "bay.png" 330 450

# the nameplate: the BLANK black enamel plate. The delivered one reads BEET,
# ordered before the name became Beetmachine; the panel letters it instead.
Cut "04-labels-plates.png" "black-enamel-plate" "plate.png" 512 256
Cut "04-labels-plates.png" "black-tape" "tape.png" 512 64

# knobs, pointer up; round, so squared
Cut "01-knobs.png" "bakelite-large" "knob.png" 256 256 -Square
Cut "01-knobs.png" "red-bakelite" "knob-red.png" 256 256 -Square

# the two hero buttons (UP drawings only - see the header)
Cut "02-switches.png" "hit-up" "hit.png" 256 256 -Square
Cut "02-switches.png" "estop-up" "estop.png" 256 256 -Square

# the hit lamp: a registered pair, same bezel
Cut "03-lamps-gauges.png" "amber-off" "lamp-off.png" 128 128 -Square
Cut "03-lamps-gauges.png" "amber-on" "lamp-on.png" 128 128 -Square
""
