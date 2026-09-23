# Resize the delivered decals down to what the panel actually needs.
# A matcap is a smooth, low-frequency image: 512 is indistinguishable from 1024
# on screen and keeps the embedded binary from doubling in size.
param([string]$Src, [string]$Dst)

Add-Type -AssemblyName System.Drawing

$jobs = @(
    @{ f = '01a-matcap-anodised-spectrum.png';        o = 'mc-anodised.png';  s = 512 },
    @{ f = '01b-matcap-dark-mineral.png';             o = 'mc-mineral.png';   s = 512 },
    @{ f = '01c-matcap-wet-glass.png';                o = 'mc-glass.png';     s = 512 },
    @{ f = '01d-matcap-cold-fire.png';                o = 'mc-coldfire.png';  s = 512 },
    @{ f = '03a-crystal-growth-striations-height.png';o = 'tx-striations.png';s = 512 },
    @{ f = '02a-brushed-aluminium-tile.png';          o = 'tx-alu.png';       s = 512 },
    @{ f = '02b-stencil-wear-mask-tile.png';          o = 'tx-wear.png';      s = 512 },
    @{ f = '02c-case-corner-fitting.png';             o = 'tx-corner.png';    s = 512 }
)

if (-not (Test-Path $Dst)) { New-Item -ItemType Directory -Force $Dst | Out-Null }

foreach ($j in $jobs) {
    $inPath = Join-Path $Src $j.f
    if (-not (Test-Path $inPath)) { Write-Output "MISSING $($j.f)"; continue }
    $img = [System.Drawing.Image]::FromFile($inPath)
    $bmp = New-Object System.Drawing.Bitmap($j.s, $j.s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($img, (New-Object System.Drawing.Rectangle(0, 0, $j.s, $j.s)))
    $g.Dispose()
    $outPath = Join-Path $Dst $j.o
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose(); $img.Dispose()
    $kb = [math]::Round((Get-Item $outPath).Length / 1024)
    Write-Output ("{0,-22} -> {1,-20} {2}x{2}  {3} kB" -f $j.f, $j.o, $j.s, $kb)
}
