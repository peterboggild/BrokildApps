# The ghost-transmission stills: Source/ui/art/ghost-1.jpg, ghost-2.jpg ...
#
# The page keys out each still's flat backdrop and fades its edges at load, so
# these are just resized copies. Register a new one in CMakeLists (BinaryData)
# and in GHOST.list in ui.html.
#
# The panel itself goes through ingest-decals.ps1 from assets/decals/panel.png,
# which since 260926.2 IS the Space Panther panel (the original is kept in
# assets/skins/original).
param(
  [string]$Root   = "$PSScriptRoot\..",
  [string[]]$Ghosts = @("C:\Users\peter\Downloads\BSO_Art\BattleStarOverdrive_PressPic2.jpg"),
  [int]$GhostMaxW = 960
)
Add-Type -AssemblyName System.Drawing
$outDir = Join-Path $Root "Source\ui\art"

$enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
$ps = New-Object System.Drawing.Imaging.EncoderParameters 1
$ps.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), ([long]86)

$i = 0
foreach ($gp in $Ghosts) {
  $i++
  $im = [System.Drawing.Bitmap]::FromFile($gp)
  $w = [Math]::Min($GhostMaxW, $im.Width); $h = [int][Math]::Round($im.Height * $w / $im.Width)
  $bm = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bm)
  $g.InterpolationMode = "HighQualityBicubic"; $g.PixelOffsetMode = "HighQuality"
  $g.DrawImage($im, 0, 0, $w, $h); $g.Dispose()
  $p = Join-Path $outDir ("ghost-" + $i + ".jpg")
  $bm.Save($p, $enc, $ps)
  "ghost-$i.jpg  {0} x {1}  {2:N0} KB   <- {3}" -f $w, $h, ((Get-Item $p).Length / 1KB), (Split-Path $gp -Leaf)
  $bm.Dispose(); $im.Dispose()
}
