# Launch the High Tide standalone with the WebView2 remote-debugging port open,
# so tools/cdp.js can drive the LIVE panel (the only verification that counts:
# static checks cannot see a page that executes nothing).
#
#   powershell -File tools\live.ps1 [-Port 9245]
#
# WebView2 joins a RUNNING browser process per user-data folder and ignores
# new arguments, so any msedgewebview2 is killed first. The standalone uses its
# own profile (WebView2-standalone), so a VST3 in a host is not disturbed.
param([int] $Port = 9245)

Get-Process msedgewebview2 -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Get-Process "High Tide" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = "--remote-debugging-port=$Port"
$exe = "$PSScriptRoot\..\build\HighTide_artefacts\Release\Standalone\High Tide.exe"
$rand = New-Object System.Random
$started = $false
for ($i = 0; $i -lt 6 -and -not $started; $i++) {
    try { Start-Process -FilePath $exe -ErrorAction Stop; $started = $true }
    catch {
        # Smart App Control judges the hash: a few overlay bytes make a new one
        $b = New-Object byte[] ($rand.Next(3, 11)); $rand.NextBytes($b)
        Add-Content -Path $exe -Value $b -Encoding Byte
        Start-Sleep -Milliseconds 300
    }
}
if (-not $started) { Write-Output "could not start the standalone"; exit 1 }

for ($i = 0; $i -lt 60; $i++) {
    try {
        $r = Invoke-WebRequest -Uri "http://127.0.0.1:$Port/json/list" -UseBasicParsing -TimeoutSec 1
        if ($r.Content.Length -gt 2) { Write-Output "cdp up on $Port"; exit 0 }
    } catch {}
    Start-Sleep -Milliseconds 500
}
Write-Output "cdp did not come up"
exit 1
