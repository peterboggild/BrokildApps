# Cut the machine-style parts for the drum family (Kickstart, Snare Tactics,
# Hats Off, Beetmachine) out of the delivery in assets/drum-decals.
#
# Output: vst3-apps/machine-art/art/, embedded into each plug-in by
# machine-art.cmake. Re-run each plug-in's configure afterwards (file(GLOB)).
#
# Parts are cropped to the ALPHA BOUNDS the delivery's manifest measured,
# squared where the part is round, and scaled to roughly twice their size on
# screen. The first delivery's switch "pressed" drawings are NOT used (not
# registered with the "up" ones); the push buttons come from 05-buttons.png,
# whose up/pressed pairs ARE registered (ingest-buttons.ps1). The lamp pairs
# are registered and both are used.
#
# ASCII only: Windows PowerShell 5.1 reads a no-BOM .ps1 as ANSI.
param(
  [string]$Src = "$PSScriptRoot\..\..\assets\drum-decals",
  [string]$Out = "$PSScriptRoot\art"
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
function Cut($sheet, $name, $file, $w, $h, [switch]$Square, [switch]$KeepAspect) {
  $img = [System.Drawing.Bitmap]::FromFile((Join-Path $Src $sheet))
  $r = Part $sheet $name
  if ($Square) {
    $side = [Math]::Max($r.Width, $r.Height)
    $r = New-Object System.Drawing.Rectangle ($r.X + [int](($r.Width - $side) / 2)), ($r.Y + [int](($r.Height - $side) / 2)), $side, $side
  }
  if ($KeepAspect) { $h = [int][Math]::Round($w * $r.Height / $r.Width) }
  $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
  $g.Clear([System.Drawing.Color]::Transparent)
  $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $w, $h), $r, [System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose(); $img.Dispose()
  Save $bmp $file; $bmp.Dispose()
}
function Ground($file, $groundName) {   # not $out: PowerShell names are case-insensitive, and $Out is the folder
  $gimg = [System.Drawing.Bitmap]::FromFile((Join-Path $Src $file))
  $gb = New-Object System.Drawing.Bitmap 512, 512
  $gg = [System.Drawing.Graphics]::FromImage($gb); $gg.InterpolationMode = 'HighQualityBicubic'
  $gg.DrawImage($gimg, 0, 0, 512, 512); $gg.Dispose(); $gimg.Dispose()
  Save $gb $groundName; $gb.Dispose()
}

"`nmachine art -> $Out`n"
# each machine's paint, tiled by the panel
Ground "beet-ground.png"          "ground-beet.jpg"
Ground "kickstart-ground.png"     "ground-kick.jpg"
Ground "snare-tactics-ground.png" "ground-snare.jpg"
Ground "hats-off-ground.png"      "ground-hats.jpg"

# each machine's nameplate, at its own proportions (the Beetmachine one is the
# blank enamel plate, lettered in code: the delivered plate reads BEET)
Cut "kickstart-nameplate.png"     "kickstart"     "plate-kick.png"  640 0 -KeepAspect
Cut "snare-tactics-nameplate.png" "snare-tactics" "plate-snare.png" 640 0 -KeepAspect
Cut "hats-off-nameplate.png"      "hats-off"      "plate-hats.png"  640 0 -KeepAspect
Cut "04-labels-plates.png" "black-enamel-plate" "plate-blank.png" 512 256

# shared parts
Cut "beet-machine-bay.png" "machine-bay" "bay.png" 330 450
Cut "04-labels-plates.png" "black-tape" "tape.png" 512 64
Cut "01-knobs.png" "bakelite-large" "knob.png" 256 256 -Square
Cut "01-knobs.png" "red-bakelite" "knob-red.png" 256 256 -Square
Cut "01-knobs.png" "bronze-knob" "knob-bronze.png" 256 256 -Square
# the push buttons (hit / estop, up AND pressed) come from the second, registered
# delivery - see ingest-buttons.ps1, run here so one command still makes everything
& (Join-Path $PSScriptRoot "ingest-buttons.ps1") -Out $Out
Cut "03-lamps-gauges.png" "amber-off" "lamp-off.png" 128 128 -Square
Cut "03-lamps-gauges.png" "amber-on" "lamp-on.png" 128 128 -Square
""
