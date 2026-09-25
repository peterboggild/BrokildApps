# Cut the Kickstart download and PROVE it: the plug-in and the standalone in
# the archive must be byte-identical to the build, and carry the build id.
#
#   powershell -ExecutionPolicy Bypass -File tools\make-zip.ps1 [-BuildDir <dir>]
#
# Relink cleanly before cutting: a standalone launched through a hash-nudging
# harness has extra bytes appended, and those must not ship.

param([string] $BuildDir = "C:\Users\peter\b\_build\Kickstart\plugin")
$ErrorActionPreference = "Stop"

$site  = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)          # vst3-apps\kickstart
$rel   = Join-Path $BuildDir "Kickstart_artefacts\Release"
$vst3  = Join-Path $rel "VST3\Kickstart.vst3"
$exe   = Join-Path $rel "Standalone\Kickstart.exe"
$pdf   = Join-Path $site "Kickstart-Manual.pdf"
$readme = Join-Path $site "README.txt"
foreach ($f in $vst3, $exe, $pdf, $readme) { if (-not (Test-Path $f)) { throw "missing: $f" } }

$cml = Get-Content (Join-Path (Split-Path -Parent $PSScriptRoot) "CMakeLists.txt") -Raw
$id = [regex]::Match($cml, 'KS_BUILD_ID "([0-9.]+)"').Groups[1].Value

$stageRoot = Join-Path $env:TEMP "kickstart-dist"
if (Test-Path $stageRoot) { Remove-Item $stageRoot -Recurse -Force }
$stage = Join-Path $stageRoot "Kickstart-VST3-win64"
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item $vst3 (Join-Path $stage "Kickstart.vst3") -Recurse
Copy-Item $exe, $pdf, $readme $stage

$zip = Join-Path $site "Kickstart-VST3-win64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip

# ---- verify from the archive itself -------------------------------------------
$chk = Join-Path $env:TEMP "kickstart-zipcheck"
if (Test-Path $chk) { Remove-Item $chk -Recurse -Force }
Expand-Archive $zip $chk
$dll = Get-ChildItem $chk -Recurse -Filter "Kickstart.vst3" -File | Select-Object -First 1
$sx  = Get-ChildItem $chk -Recurse -Filter "Kickstart.exe" -File | Select-Object -First 1
$okDll = (Get-FileHash $dll.FullName).Hash -eq (Get-FileHash (Join-Path $vst3 "Contents\x86_64-win\Kickstart.vst3")).Hash
$okExe = (Get-FileHash $sx.FullName).Hash -eq (Get-FileHash $exe).Hash
$txt = [IO.File]::ReadAllText($dll.FullName, [Text.Encoding]::ASCII)
$okId = $txt.Contains($id)
$n = (Get-ChildItem $chk -Recurse -File).Count
"zip {0:N0} bytes, {1} files; plug-in identical={2}; standalone identical={3}; build id {4} inside={5}" -f (Get-Item $zip).Length, $n, $okDll, $okExe, $id, $okId
if (-not ($okDll -and $okExe -and $okId)) { throw "the archive is not the build" }
Remove-Item $chk -Recurse -Force
