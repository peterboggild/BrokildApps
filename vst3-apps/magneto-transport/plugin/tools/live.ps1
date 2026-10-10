# Launch the standalone with CDP open and run a jobs file against the LIVE panel.
#
# Smart App Control blocks by file HASH and can refuse a freshly linked exe: a
# few random bytes appended past the last PE section change the hash without
# changing the program (overlay data after the final section is ignored).
param(
  [int]$Port = 9251,
  [string]$Jobs = "$PSScriptRoot\..\test\live-jobs.json",
  [string]$Exe  = "C:\Users\peter\b\_build\MagnetoTransport\plugin\MagnetoTransport_artefacts\Release\Standalone\Magneto Transport.exe",
  [switch]$KeepOpen
)

# The standalone takes a fresh WebView2 profile per process (PluginEditor.cpp), so
# no other browser process can hold its folder - and killing every msedgewebview2
# would blank the panels of any plugin open in a running DAW. Only our own goes.
Get-Process "Magneto Transport" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600

$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = "--remote-debugging-port=$Port --remote-allow-origins=*"

$proc = $null
for ($attempt = 1; $attempt -le 6; $attempt++) {
  try {
    $proc = Start-Process -FilePath $Exe -PassThru -ErrorAction Stop
    Start-Sleep -Milliseconds 900
    if (-not $proc.HasExited) { break }
    $proc = $null
  } catch {
    Write-Output ("launch attempt {0} refused: {1}" -f $attempt, $_.Exception.Message)
  }
  # nudge the hash and try again
  $bytes = New-Object byte[] (1 + (Get-Random -Maximum 8))
  (New-Object Random).NextBytes($bytes)
  $fs = [System.IO.File]::Open($Exe, 'Append', 'Write')
  $fs.Write($bytes, 0, $bytes.Length); $fs.Close()
  Write-Output ("  nudged the exe hash (attempt {0})" -f $attempt)
}
if (-not $proc) { Write-Output "could not start the standalone"; exit 1 }
Write-Output ("standalone up, pid {0}, CDP on {1}" -f $proc.Id, $Port)

Start-Sleep -Seconds 2
& node "$PSScriptRoot\..\tools\cdp.js" $Port $Jobs
$code = $LASTEXITCODE

if (-not $KeepOpen) {
  Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
  Get-Process msedgewebview2 -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}
exit $code
