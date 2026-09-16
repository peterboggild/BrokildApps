# LEGION — Windows release build. Run from the vocal-harmonizer folder.
#
#   powershell -ExecutionPolicy Bypass -File tools\build-win.ps1
#
# Builds the offline bench FIRST and refuses to build the plugin if it does
# not print ALL CLEAR — the house rule: nothing ships past a red bench.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

Write-Host "== bench ==" -ForegroundColor Cyan
cmake -S "$root\test" -B "$root\test\build" | Out-Host
cmake --build "$root\test\build" --config Release | Out-Host

$bench = "$root\test\build\Release\legiontest.exe"
if (-not (Test-Path $bench)) { $bench = "$root\test\build\legiontest.exe" }
$out = & $bench
$out | Out-Host
if ($LASTEXITCODE -ne 0 -or -not ($out -match "ALL CLEAR")) {
    throw "the bench is not clear — not building the plugin"
}

Write-Host "`n== plugin ==" -ForegroundColor Cyan
cmake -S $root -B "$root\build" -A x64 | Out-Host
cmake --build "$root\build" --config Release | Out-Host

Write-Host "`n== host harness ==" -ForegroundColor Cyan
$host_exe = Get-ChildItem -Path "$root\build" -Filter "legionhost.exe" -Recurse -File |
            Select-Object -First 1
if ($null -eq $host_exe) { throw "no legionhost.exe was produced" }
$hout = & $host_exe.FullName
$hout | Out-Host
if ($LASTEXITCODE -ne 0 -or -not ($hout -match "ALL CLEAR")) {
    throw "the host harness is not clear"
}

$vst3 = Get-ChildItem -Path "$root\build" -Filter "Legion.vst3" -Recurse -Directory |
        Select-Object -First 1
if ($null -eq $vst3) { throw "no Legion.vst3 was produced" }

Write-Host "`nbuilt: $($vst3.FullName)" -ForegroundColor Green
Write-Host "install: copy it to C:\Program Files\Common Files\VST3\"
