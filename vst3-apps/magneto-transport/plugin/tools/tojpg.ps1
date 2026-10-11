# PNG plates -> JPEG q88, every .png in a folder (the house plate step).
# ASCII only (Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI).
param(
  [string]$Dir,
  [int]$Quality = 88,
  [switch]$Keep
)
Add-Type -AssemblyName System.Drawing
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$ep = New-Object System.Drawing.Imaging.EncoderParameters 1
$ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, [long]$Quality)
Get-ChildItem $Dir -Filter *.png | ForEach-Object {
  $src = [System.Drawing.Bitmap]::FromFile($_.FullName)
  $sw = $src.Width; $sh = $src.Height
  # JPEG has no alpha: composite onto the page's dark
  $dst = New-Object System.Drawing.Bitmap $src.Width, $src.Height, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $g = [System.Drawing.Graphics]::FromImage($dst)
  $g.Clear([System.Drawing.Color]::FromArgb(18, 21, 19))
  $g.DrawImage($src, 0, 0, $src.Width, $src.Height)
  $g.Dispose(); $src.Dispose()
  $outp = [System.IO.Path]::ChangeExtension($_.FullName, '.jpg')
  $dst.Save($outp, $codec, $ep); $dst.Dispose()
  if (-not $Keep) { Remove-Item $_.FullName }
  Write-Host ("  {0,-22} {1}x{2}  {3,5} KB" -f [System.IO.Path]::GetFileName($outp), $sw, $sh, [int]((Get-Item $outp).Length / 1KB))
}
