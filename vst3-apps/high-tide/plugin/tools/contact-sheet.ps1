param(
    [string] $Src = "",
    [string] $Out = ""
)
# A contact sheet of the delivered decals: every part scaled into a cell on a
# checkerboard, so alpha shows and one look judges the whole delivery.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

if (-not $Src) { $Src = Join-Path $env:USERPROFILE "Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\assets\high-tide-decals" }
if (-not $Out) { $Out = "$PSScriptRoot\..\dist\decal-sheet.png" }
$files = Get-ChildItem (Join-Path $Src "*.png") | Sort-Object Name

$cell = 300; $cols = 4
$rows = [math]::Ceiling($files.Count / $cols)
$W = $cols * $cell; $H = $rows * ($cell + 22)
$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.Clear([System.Drawing.Color]::FromArgb(40, 40, 44))

$light = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(96, 96, 102))
$dark  = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(64, 64, 70))
$font  = New-Object System.Drawing.Font "Consolas", 11
$white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)

for ($i = 0; $i -lt $files.Count; $i++) {
    $cx = ($i % $cols) * $cell
    $cy = [math]::Floor($i / $cols) * ($cell + 22)
    for ($y = 0; $y -lt $cell; $y += 20) {
        for ($x = 0; $x -lt $cell; $x += 20) {
            $b = if ((([math]::Floor($x / 20) + [math]::Floor($y / 20)) % 2) -eq 0) { $light } else { $dark }
            $g.FillRectangle($b, $cx + $x, $cy + $y + 20, [math]::Min(20, $cell - $x), [math]::Min(20, $cell - $y))
        }
    }
    $img = [System.Drawing.Image]::FromFile($files[$i].FullName)
    $s = [math]::Min(($cell - 16) / $img.Width, ($cell - 16) / $img.Height)
    $w = [int]($img.Width * $s); $h = [int]($img.Height * $s)
    $g.DrawImage($img, $cx + ($cell - $w) / 2, $cy + 20 + ($cell - $h) / 2, $w, $h)
    $g.DrawString(("{0}  {1}x{2}" -f $files[$i].Name, $img.Width, $img.Height), $font, $white, $cx + 4, $cy + 2)
    $img.Dispose()
}
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output ("wrote " + $Out + "  (" + $files.Count + " parts)")
