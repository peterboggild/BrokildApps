# Crop each decal to its actual content and report the true aspect.
# Six of the twelve arrived letterboxed — the image pasted into a band of a
# larger black canvas — so the delivered aspect is not the drawn aspect, and a
# straight rescale to the asked-for size would smear them. This finds what was
# really drawn; whether it is USABLE at that aspect is a judgement per part.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$src = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\assets\high-tide-decals"
$dst = "$PSScriptRoot\..\dist\decals-cropped"
New-Item -ItemType Directory -Force $dst | Out-Null

foreach ($f in (Get-ChildItem (Join-Path $src "*.png") | Sort-Object Name)) {
    $img = New-Object System.Drawing.Bitmap $f.FullName
    $W = $img.Width; $H = $img.Height
    $r = New-Object System.Drawing.Rectangle 0, 0, $W, $H
    $d = $img.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = $d.Stride
    $buf = New-Object byte[] ($stride * $H)
    [System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $buf, 0, $buf.Length)
    $img.UnlockBits($d)

    $x0 = $W; $x1 = -1; $y0 = $H; $y1 = -1
    for ($y = 0; $y -lt $H; $y++) {
        $o0 = $y * $stride
        for ($x = 0; $x -lt $W; $x++) {
            $o = $o0 + $x * 4
            $a = $buf[$o + 3]
            if ($a -le 12) { continue }
            $lum = 0.2126 * $buf[$o + 2] + 0.7152 * $buf[$o + 1] + 0.0722 * $buf[$o]
            if ($lum -le 8) { continue }
            if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }
            if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y }
        }
    }
    if ($x1 -lt 0) { "{0,-20} EMPTY" -f $f.Name; $img.Dispose(); continue }
    $cw = $x1 - $x0 + 1; $ch = $y1 - $y0 + 1
    $asked = "{0}x{1}" -f $W, $H
    $real  = "{0}x{1}" -f $cw, $ch
    $ar = [math]::Round($cw / [double]$ch, 2)
    $arAsked = [math]::Round($W / [double]$H, 2)
    $flag = if ([math]::Abs($ar - $arAsked) / $arAsked -gt 0.08) { "  <-- LETTERBOXED, drawn aspect $ar vs asked $arAsked" } else { "" }
    "{0,-20} asked {1,-11} drawn {2,-11} at ({3},{4}){5}" -f $f.Name, $asked, $real, $x0, $y0, $flag

    $crop = New-Object System.Drawing.Bitmap $cw, $ch
    $g = [System.Drawing.Graphics]::FromImage($crop)
    $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $cw, $ch), $x0, $y0, $cw, $ch, [System.Drawing.GraphicsUnit]::Pixel)
    $crop.Save((Join-Path $dst $f.Name), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $crop.Dispose(); $img.Dispose()
}
Write-Output ("cropped copies in " + $dst)
