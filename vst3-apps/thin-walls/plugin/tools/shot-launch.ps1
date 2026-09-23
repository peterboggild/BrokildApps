# Launch an exe under a STRIPPED PATH (only System32), wait, capture the primary
# screen to a PNG, and kill it. Reproduces "the downloaded exe on a PC with no
# developer kits": WebView2Loader.dll must not be found by accident on the PATH.
# ASCII only.
param(
  [string]$Exe = "$PSScriptRoot\..\build\ThinWalls_artefacts\Release\Standalone\Thin Walls.exe",
  [string]$Out = "$PSScriptRoot\..\docs\launch-shot.png",
  [int]$WaitSec = 7,
  [switch]$StripPath
)
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Get-Process "Thin Walls" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
$oldPath = $env:PATH
if ($StripPath) { $env:PATH = "C:\Windows\System32;C:\Windows" }
$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = ""
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $WaitSec
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
$exited = $p.HasExited
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Get-Process msedgewebview2 -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
$env:PATH = $oldPath
Write-Output ("shot " + $Out + "  exited=" + $exited + "  strippedPath=" + $StripPath)
