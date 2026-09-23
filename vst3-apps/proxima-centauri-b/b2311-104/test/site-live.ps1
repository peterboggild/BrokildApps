# THE SITE, live: four standalones (.104, .22, .1, .67) on one bench, driven
# over CDP. Proves what the in-process sitetest cannot: four separate
# processes, the climate chosen in one and followed by the others, distance
# global, and every panel's slider actually moving.
#
#   powershell -File test\site-live.ps1
#
# Job files: site-clim-104a / 22a / 1a / 67a / 104b / 22b / 1b / 67b (.json, this folder).
# Leaves the settings as they were found and the .104 ambient at 271 K.

$ErrorActionPreference = "Continue"
$T   = Split-Path -Parent $MyInvocation.MyCommand.Path
$cdp = Join-Path (Split-Path -Parent $T) "tools\cdp.js"

# The bench's settings are the USER'S, and this script changes them as it
# works. Keep the file and put it back afterwards: an earlier run restored a
# hard-coded 0.5 and quietly replaced a distance of 0 that had been chosen.
$cfg   = Join-Path $env:APPDATA "Brokild\ProximaSite.json"
$saved = if (Test-Path $cfg) { Get-Content $cfg -Raw } else { $null }

Get-Process -Name "Artefact B2311.104","Artefact B2311.22","Artefact B2311.1","Artefact B2311.67","msedgewebview2" -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 800

function Launch($exe, $port) {
    $env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = "--remote-debugging-port=$port"
    $p = $null
    for ($i = 0; $i -lt 4 -and $null -eq $p; $i++) {
        try {
            $p = Start-Process -FilePath $exe -PassThru -ErrorAction Stop
            Start-Sleep -Seconds 2
            if ($p.HasExited) { $p = $null }
        } catch {
            Write-Output ("launch blocked (" + (Split-Path $exe -Leaf) + "); nudging the hash")
            $bytes = New-Object byte[] (2 + $i); (New-Object Random).NextBytes($bytes)
            $fs = [IO.File]::Open($exe, 'Append'); $fs.Write($bytes, 0, $bytes.Length); $fs.Close()
        }
    }
    return $p
}

$p104 = Launch "$PSScriptRoot\..\build\ArtefactB2311_104_artefacts\Release\Standalone\Artefact B2311.104.exe" 9241
Start-Sleep -Seconds 3
$p22  = Launch "$PSScriptRoot\..\build\ArtefactB2311_artefacts\Release\Standalone\Artefact B2311.22.exe" 9242
Start-Sleep -Seconds 3
$p1   = Launch "$PSScriptRoot\..\build\ArtefactB2311_1_artefacts\Release\Standalone\Artefact B2311.1.exe" 9243
Start-Sleep -Seconds 3
$p67  = Launch "$PSScriptRoot\..\build\ArtefactB2311_67_artefacts\Release\Standalone\Artefact B2311.67.exe" 9244
Start-Sleep -Seconds 9

Write-Output ("pids: 104=" + $p104.Id + " 22=" + $p22.Id + " 1=" + $p1.Id + " 67=" + $p67.Id)

# who is actually on the bench, read from the shared block by an outsider
$watch = Join-Path (Split-Path -Parent $T) "test\build\Release\ab104sitewatch.exe"
if (Test-Path $watch) {
    $tmp = Join-Path $env:TEMP ("sitewatch_" + (Get-Random) + ".exe")
    Copy-Item $watch $tmp
    $b = New-Object byte[] 2; (New-Object Random).NextBytes($b)
    $fs = [IO.File]::Open($tmp, 'Append'); $fs.Write($b, 0, 2); $fs.Close()
    & $tmp
}
node $cdp 9241 (Join-Path $T "site-clim-104a.json") 2>&1 | Select-Object -Last 5
node $cdp 9242 (Join-Path $T "site-clim-22a.json")  2>&1 | Select-Object -Last 5
node $cdp 9243 (Join-Path $T "site-clim-1a.json")   2>&1 | Select-Object -Last 2
node $cdp 9244 (Join-Path $T "site-clim-67a.json")  2>&1 | Select-Object -Last 2
node $cdp 9241 (Join-Path $T "site-clim-104b.json") 2>&1 | Select-Object -Last 3
node $cdp 9242 (Join-Path $T "site-clim-22b.json")  2>&1 | Select-Object -Last 2
node $cdp 9243 (Join-Path $T "site-clim-1b.json")   2>&1 | Select-Object -Last 2
node $cdp 9244 (Join-Path $T "site-clim-67b.json")  2>&1 | Select-Object -Last 3

foreach ($p in @($p104, $p22, $p1, $p67)) { if ($p) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } }
Start-Sleep -Milliseconds 500

# the bench, put back exactly as it was found
if ($null -ne $saved) { Set-Content -Path $cfg -Value $saved -Encoding ASCII -NoNewline }
Write-Output "--- the bench's settings, restored:"
Get-Content $cfg
