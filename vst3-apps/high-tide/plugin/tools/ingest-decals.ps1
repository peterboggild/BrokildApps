# HIGH TIDE — take the delivered decals into the panel.
#
# Six of the twelve arrived LETTERBOXED: the drawing pasted into a band of a
# larger black canvas, so the delivered aspect is not the drawn aspect. Every
# part is therefore cropped to its real content first, and only then scaled —
# a straight rescale to the asked-for size would have smeared them.
#
# Each part is then reduced to about four times its size on the panel. That is
# the Full Metal Racket lesson: a 1024 px knob brushing aliases into a rotating
# fan at 52 px, and a texture read far below its resolution is noise.
#
#   powershell -File tools\ingest-decals.ps1
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$src = Join-Path $env:USERPROFILE "Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\assets\high-tide-decals"
$dst = "C:\Users\peter\b\HighTide\Source\ui\decals"
New-Item -ItemType Directory -Force $dst | Out-Null

# part -> target size on disk, whether to turn it a quarter turn first, and
# whether to mirror it top-to-bottom into a seamlessly repeating tile.
# The glass tube is drawn standing up and the gauges lie down; the two ground
# textures arrived as strips and are mirrored into tiles.
$WANT = @{
    "ht-ground.png"    = @{ w = 1024; h = 0;   rot = $false; mirror = $true  }
    "ht-paper.png"     = @{ w = 1024; h = 0;   rot = $false; mirror = $true  }
    "ht-rose.png"      = @{ w = 512;  h = 0;   rot = $false; mirror = $false }
    "ht-pearl.png"     = @{ w = 128;  h = 128; rot = $false }
    "ht-tack.png"      = @{ w = 96;   h = 96;  rot = $false }
    "ht-flag.png"      = @{ w = 64;   h = 0;   rot = $false }
    "ht-bezel.png"     = @{ w = 320;  h = 320; rot = $false }
    "ht-wood.png"      = @{ w = 2048; h = 0;   rot = $false }
    "ht-glass.png"     = @{ w = 0;    h = 0;   rot = $true  }
}

function Get-ContentBox([System.Drawing.Bitmap] $bmp) {
    $W = $bmp.Width; $H = $bmp.Height
    $r = New-Object System.Drawing.Rectangle 0, 0, $W, $H
    $d = $bmp.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = $d.Stride
    $buf = New-Object byte[] ($stride * $H)
    [System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $buf, 0, $buf.Length)
    $bmp.UnlockBits($d)
    # Does this part carry alpha at all? If it does, its content is simply
    # where it is opaque — and the dark engraving on a brass plate is content.
    # If it does not, the letterbox padding is black, so darkness is the test.
    $hasAlpha = $false
    for ($y = 0; $y -lt $H -and -not $hasAlpha; $y += 4) {
        $o0 = $y * $stride
        for ($x = 0; $x -lt $W; $x += 4) { if ($buf[$o0 + $x * 4 + 3] -lt 250) { $hasAlpha = $true; break } }
    }
    $x0 = $W; $x1 = -1; $y0 = $H; $y1 = -1
    for ($y = 0; $y -lt $H; $y++) {
        $o0 = $y * $stride
        for ($x = 0; $x -lt $W; $x++) {
            $o = $o0 + $x * 4
            if ($hasAlpha) { if ($buf[$o + 3] -le 12) { continue } }
            else {
                $lum = 0.2126 * $buf[$o + 2] + 0.7152 * $buf[$o + 1] + 0.0722 * $buf[$o]
                if ($lum -le 8) { continue }
            }
            if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }
            if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y }
        }
    }
    if ($x1 -lt 0) { return $null }
    return New-Object System.Drawing.Rectangle $x0, $y0, ($x1 - $x0 + 1), ($y1 - $y0 + 1)
}

foreach ($name in ($WANT.Keys | Sort-Object)) {
    $path = Join-Path $src $name
    if (-not (Test-Path $path)) { Write-Output ("  MISSING " + $name); continue }
    $img = New-Object System.Drawing.Bitmap $path
    $box = Get-ContentBox $img
    if (-not $box) { Write-Output ("  EMPTY   " + $name); $img.Dispose(); continue }

    $crop = New-Object System.Drawing.Bitmap $box.Width, $box.Height
    $g = [System.Drawing.Graphics]::FromImage($crop)
    $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $box.Width, $box.Height), $box.X, $box.Y, $box.Width, $box.Height, [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose(); $img.Dispose()

    if ($WANT[$name].rot) { $crop.RotateFlip([System.Drawing.RotateFlipType]::Rotate90FlipNone) }

    # A strip of texture becomes a tile that repeats without a seam by
    # mirroring it once: the join is the strip's own edge against itself.
    if ($WANT[$name].mirror) {
        $mw = $crop.Width; $mh = $crop.Height * 2
        $mir = New-Object System.Drawing.Bitmap $mw, $mh
        $gm = [System.Drawing.Graphics]::FromImage($mir)
        $gm.DrawImage($crop, 0, 0, $crop.Width, $crop.Height)
        $flip = New-Object System.Drawing.Bitmap $crop
        $flip.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY)
        $gm.DrawImage($flip, 0, $crop.Height, $crop.Width, $crop.Height)
        $gm.Dispose(); $flip.Dispose(); $crop.Dispose()
        $crop = $mir
    }

    $cw = $crop.Width; $ch = $crop.Height
    $tw = $WANT[$name].w; $th = $WANT[$name].h
    if ($tw -le 0 -and $th -le 0) { $tw = $cw; $th = $ch }
    elseif ($th -le 0) { $th = [math]::Max(1, [int][math]::Round($ch * $tw / $cw)) }
    elseif ($tw -le 0) { $tw = [math]::Max(1, [int][math]::Round($cw * $th / $ch)) }
    if ($tw -gt $cw) { $tw = $cw; $th = $ch }      # never upscale

    $out = New-Object System.Drawing.Bitmap $tw, $th
    $g2 = [System.Drawing.Graphics]::FromImage($out)
    $g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g2.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g2.Clear([System.Drawing.Color]::Transparent)
    $g2.DrawImage($crop, (New-Object System.Drawing.Rectangle 0, 0, $tw, $th))
    $g2.Dispose()
    $outPath = Join-Path $dst $name
    $out.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $kb = [math]::Round((Get-Item $outPath).Length / 1KB)
    Write-Output ("  {0,-18} delivered {1}x{2} -> drawn {3}x{4} -> panel {5}x{6}  {7} KB" -f $name, $img.Width, $img.Height, $cw, $ch, $tw, $th, $kb)
    $crop.Dispose(); $out.Dispose()
}
$total = (Get-ChildItem (Join-Path $dst "*.png") | Measure-Object -Property Length -Sum).Sum
Write-Output ("  total in the plug-in: " + [math]::Round($total / 1KB) + " KB")
