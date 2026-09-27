# Measure a delivered decal before believing its cover note. Two deliveries in
# a row (High Tide, Rite of Passage) arrived with the drawing sitting in a band
# of a much larger canvas while the note said "Deviations: none", so the canvas
# size tells you nothing and the CONTENT box is the only honest measurement.
#
#   content = opaque and not near-white   (an isolated part on a white page)
#             or just opaque              (a part delivered with alpha)
param([Parameter(Mandatory=$true)][string]$Dir, [string]$Filter = "*.png")
Add-Type -AssemblyName System.Drawing

Get-ChildItem -Path $Dir -Filter $Filter | Sort-Object Name | ForEach-Object {
    $bmp = [System.Drawing.Bitmap]::FromFile($_.FullName)
    $w = $bmp.Width; $h = $bmp.Height
    $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = $data.Stride
    $buf = New-Object byte[] ($stride * $h)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $buf, 0, $buf.Length)
    $bmp.UnlockBits($data)

    $x0 = $w; $y0 = $h; $x1 = -1; $y1 = -1
    $alphaSum = 0.0; $anyAlpha = $false
    for ($y = 0; $y -lt $h; $y++) {
        $row = $y * $stride
        for ($x = 0; $x -lt $w; $x++) {
            $i = $row + $x * 4
            $b = $buf[$i]; $g = $buf[$i+1]; $r = $buf[$i+2]; $a = $buf[$i+3]
            $alphaSum += $a
            if ($a -lt 250) { $anyAlpha = $true }
            $lum = 0.299*$r + 0.587*$g + 0.114*$b
            if ($a -gt 24 -and $lum -lt 244) {
                if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }
                if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y }
            }
        }
    }
    $bmp.Dispose()
    if ($x1 -lt 0) { "{0,-46} {1}x{2}  NO CONTENT" -f $_.Name, $w, $h; return }
    $cw = $x1 - $x0 + 1; $ch = $y1 - $y0 + 1
    $cov = [math]::Round(100.0 * $cw * $ch / ($w * $h), 1)
    $flag = ""
    if ($cov -lt 70) { $flag = "  <== LETTERBOXED / PADDED" }
    "{0,-46} canvas {1}x{2}  content {3},{4} {5}x{6} ({7}%)  aspect {8}  meanA {9}{10}" -f `
        $_.Name, $w, $h, $x0, $y0, $cw, $ch, $cov, [math]::Round($cw/$ch, 3), [math]::Round($alphaSum/($w*$h), 0), $flag
}
