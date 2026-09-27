# What is actually inside each decal: the content bounding box (pixels that are
# neither transparent nor near-black), and a row/column brightness profile.
# A part whose content fills only a band of its canvas was letterboxed, and the
# contact sheet cannot tell that from a deliberately dark texture.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$src = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\assets\high-tide-decals"
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
    $sum = 0.0; $sum2 = 0.0; $n = 0; $opaque = 0
    $rowsLit = New-Object 'double[]' 16
    $rowsN = New-Object 'double[]' 16
    $step = [math]::Max(1, [int]($H / 400))
    $xstep = [math]::Max(1, [int]($W / 400))
    for ($y = 0; $y -lt $H; $y += $step) {
        $o0 = $y * $stride
        for ($x = 0; $x -lt $W; $x += $xstep) {
            $o = $o0 + $x * 4
            $b = $buf[$o]; $g = $buf[$o + 1]; $rr = $buf[$o + 2]; $a = $buf[$o + 3]
            $lum = (0.2126 * $rr + 0.7152 * $g + 0.0722 * $b)
            $band = [math]::Min(15, [int](16 * $y / $H))
            $rowsLit[$band] += $lum * ($a / 255.0); $rowsN[$band] += 1
            if ($a -gt 12) { $opaque++ }
            if ($a -gt 12 -and $lum -gt 8) {
                if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }
                if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y }
                $sum += $lum; $sum2 += $lum * $lum; $n++
            }
        }
    }
    $mean = if ($n) { $sum / $n } else { 0 }
    $sd = if ($n) { [math]::Sqrt([math]::Max(0, $sum2 / $n - $mean * $mean)) } else { 0 }
    $covW = if ($x1 -ge 0) { [math]::Round(100.0 * ($x1 - $x0 + $xstep) / $W) } else { 0 }
    $covH = if ($y1 -ge 0) { [math]::Round(100.0 * ($y1 - $y0 + $step) / $H) } else { 0 }
    $prof = ($rowsLit | ForEach-Object -Begin { $i = 0 } -Process {
        $v = if ($rowsN[$i]) { $_ / $rowsN[$i] } else { 0 }; $i++
        if ($v -lt 4) { "." } elseif ($v -lt 20) { ":" } elseif ($v -lt 60) { "o" } elseif ($v -lt 130) { "O" } else { "#" }
    }) -join ""
    "{0,-20} content {1,3}% x {2,3}% of canvas   lum {3,5:N1} sd {4,5:N1}   rows |{5}|" -f $f.Name, $covW, $covH, $mean, $sd, $prof
    $img.Dispose()
}
