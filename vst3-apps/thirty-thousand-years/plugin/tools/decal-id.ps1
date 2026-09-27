# Identify a delivered part by what it IS, not by the order it arrived in.
# Reports mean luminance, the luminance at the centre and near the rim, the
# fraction of bone-white pixels, and whether the corners are transparent.
param([Parameter(Mandatory=$true)][string]$Dir, [string]$Filter = "*.png")
Add-Type -AssemblyName System.Drawing

"{0,-46} {1,7} {2,7} {3,7} {4,7} {5,7} {6}" -f "file", "meanLum", "centre", "rim", "bone%", "cornerA", "guess"
Get-ChildItem -Path $Dir -Filter $Filter | Sort-Object Name | ForEach-Object {
    $bmp = [System.Drawing.Bitmap]::FromFile($_.FullName)
    $w = $bmp.Width; $h = $bmp.Height
    $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
    $d = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = $d.Stride
    $buf = New-Object byte[] ($stride * $h)
    [System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $buf, 0, $buf.Length)
    $bmp.UnlockBits($d); $bmp.Dispose()

    function Lum([int]$x, [int]$y) {
        $i = $y * $stride + $x * 4
        return 0.299*$buf[$i+2] + 0.587*$buf[$i+1] + 0.114*$buf[$i]
    }
    $sum = 0.0; $n = 0; $bone = 0
    for ($y = 0; $y -lt $h; $y += 3) {
        for ($x = 0; $x -lt $w; $x += 3) {
            $i = $y * $stride + $x * 4
            if ($buf[$i+3] -lt 128) { continue }
            $l = Lum $x $y
            $sum += $l; $n++
            $r = $buf[$i+2]; $g = $buf[$i+1]; $b = $buf[$i]
            if ($l -gt 180 -and [math]::Abs($r - $b) -lt 40) { $bone++ }
        }
    }
    $mean = if ($n) { $sum / $n } else { 0 }
    $cx = [int]($w/2); $cy = [int]($h/2)
    $centre = Lum $cx $cy
    $rim = Lum ([int]($w*0.53)) ([int]($h*0.06))
    $ca = $buf[3]   # top-left pixel alpha
    $bonePct = if ($n) { [math]::Round(100.0*$bone/$n, 1) } else { 0 }

    $guess = "?"
    if ($ca -lt 32) {
        if ($w / $h -gt 3) { $guess = "wordmark" }
        elseif ($bonePct -gt 3) { $guess = "knob (bone bezel)" }
        else { $guess = "screw (no bone)" }
    } else {
        if ($w / $h -gt 1.6) { $guess = "glass" }
        elseif ($mean -lt 40) { $guess = "panel (darker)" }
        else { $guess = "plate (lighter)" }
    }
    "{0,-46} {1,7} {2,7} {3,7} {4,7} {5,7} {6}" -f $_.Name, [math]::Round($mean,1), [math]::Round($centre,0), [math]::Round($rim,0), $bonePct, $ca, $guess
}
