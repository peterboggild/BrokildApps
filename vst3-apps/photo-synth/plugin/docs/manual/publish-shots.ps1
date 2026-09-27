# Put every plate the manual asks for into docs/manual/shots, converting the
# large ones to JPEG (q88, max 1700 px) so the PDF and the download stay small.
Add-Type -AssemblyName System.Drawing

$src  = "C:\Users\peter\AppData\Local\Temp\claude\c--Users-peter-Dropbox-ACTIVITIES-00-VSCODE\cd01d138-dfc1-41c5-9c61-caa69b695d83\scratchpad\manual\shots-new"
$dst  = "$PSScriptRoot\..\..\docs\manual\shots"
$man  = Get-Content "$PSScriptRoot\..\..\docs\manual\manual.html" -Raw

$wanted = [regex]::Matches($man, 'src="shots/([^"]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique

$jpegCodec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() |
             Where-Object { $_.MimeType -eq 'image/jpeg' }
$params = New-Object System.Drawing.Imaging.EncoderParameters 1
$params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter(
                     [System.Drawing.Imaging.Encoder]::Quality, 88)

$missing = @()
foreach ($name in $wanted) {
  $stem = [System.IO.Path]::GetFileNameWithoutExtension($name)
  $ext  = [System.IO.Path]::GetExtension($name)
  $from = Join-Path $src ($stem + ".png")
  if (-not (Test-Path -LiteralPath $from)) { $missing += $name; continue }

  $out = Join-Path $dst $name
  $img = [System.Drawing.Image]::FromFile($from)
  try {
    $maxw = 1700
    if ($img.Width -gt $maxw) {
      $h = [int]([math]::Round($img.Height * $maxw / $img.Width))
      $bmp = New-Object System.Drawing.Bitmap($maxw, $h)
      $g = [System.Drawing.Graphics]::FromImage($bmp)
      $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
      $g.PixelOffsetMode  = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
      $g.DrawImage($img, 0, 0, $maxw, $h)
      $g.Dispose()
    } else {
      $bmp = New-Object System.Drawing.Bitmap($img)
    }
    if ($ext -eq ".jpg") {
      # JPEG has no alpha: lay the plate on the manual's own page colour
      $flat = New-Object System.Drawing.Bitmap($bmp.Width, $bmp.Height)
      $gf = [System.Drawing.Graphics]::FromImage($flat)
      $gf.Clear([System.Drawing.Color]::FromArgb(12, 10, 7))
      $gf.DrawImage($bmp, 0, 0)
      $gf.Dispose()
      $flat.Save($out, $jpegCodec, $params)
      $flat.Dispose()
    } else {
      $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    $bmp.Dispose()
  } finally { $img.Dispose() }

  "{0,-26} {1,6} kB  {2}x{3}" -f $name, [int]((Get-Item $out).Length / 1kb),
      [System.Drawing.Image]::FromFile($out).Width, [System.Drawing.Image]::FromFile($out).Height
}

if ($missing.Count) { "MISSING SOURCE: " + ($missing -join ", ") }

# retire plates the manual no longer references
Get-ChildItem $dst -File | Where-Object { $wanted -notcontains $_.Name } | ForEach-Object {
  Remove-Item -LiteralPath $_.FullName -Force
  "retired: " + $_.Name
}
