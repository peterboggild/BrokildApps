# RITE OF PASSAGE — Windows release build. Run from the rite-of-passage folder.
#
#   powershell -ExecutionPolicy Bypass -File tools\build-win.ps1
#
# Bench first, and it refuses to build the plugin unless the bench is clear.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

Write-Host "== bench ==" -ForegroundColor Cyan
cmake -S "$root\test" -B "$root\test\build" | Out-Host
cmake --build "$root\test\build" --config Release | Out-Host
$bench = "$root\test\build\Release\roptest.exe"
if (-not (Test-Path $bench)) { $bench = "$root\test\build\roptest.exe" }
$out = & $bench
$out | Out-Host
if ($LASTEXITCODE -ne 0 -or -not ($out -match "ALL CLEAR")) { throw "the bench is not clear" }

Write-Host "`n== plugin ==" -ForegroundColor Cyan
cmake -S $root -B "$root\build" -A x64 | Out-Host
cmake --build "$root\build" --config Release | Out-Host

$hostExe = Get-ChildItem -Path "$root\build" -Filter "ropehost.exe" -Recurse -File | Select-Object -First 1
if ($null -eq $hostExe) { throw "no ropehost.exe was produced" }
$hout = & $hostExe.FullName
$hout | Out-Host
if ($LASTEXITCODE -ne 0 -or -not ($hout -match "ALL CLEAR")) { throw "the host harness is not clear" }

$vst3 = Get-ChildItem -Path "$root\build" -Filter "Rite of Passage.vst3" -Recurse -Directory | Select-Object -First 1
if ($null -eq $vst3) { throw "no Rite of Passage.vst3 was produced" }
Write-Host "`nbuilt: $($vst3.FullName)" -ForegroundColor Green
Write-Host "install: copy it to C:\Program Files\Common Files\VST3\"
