# Render the well demo and screenshot it over CDP.
# --virtual-time-budget hangs on a never-ending rAF loop, so this waits in
# real time and drives Page.captureScreenshot through the house cdp.js.
param(
  [string]$Page = "",
  [string]$Out  = "",
  [int]$Port    = 9251,
  [int]$WaitSec = 7,
  [string]$Hash = ""
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if ($Page -eq "") { $Page = Join-Path $here "well-demo.html" }
if ($Out  -eq "") { $Out  = Join-Path $here "well-shot.png" }

$chrome = @(
  "C:\Program Files\Google\Chrome\Application\chrome.exe",
  "C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
  (Join-Path $env:LOCALAPPDATA "Google\Chrome\Application\chrome.exe")
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $chrome) { throw "no chrome found" }

$profileDir = Join-Path $env:TEMP ("gw-shot-" + $Port)
if (Test-Path $profileDir) { Remove-Item $profileDir -Recurse -Force -ErrorAction SilentlyContinue }

$stage = Join-Path $env:TEMP ("gw-page-" + $Port + ".html")
Copy-Item $Page $stage -Force        # a file:// argument with spaces in it kills chrome
$url = "file:///" + ($stage -replace "\\", "/")
if ($Hash -ne "") { $url = $url + "#" + $Hash }
$args = @(
  "--headless=new",
  "--remote-debugging-port=$Port",
  "--user-data-dir=$profileDir",
  "--enable-unsafe-swiftshader",
  "--hide-scrollbars",
  "--window-size=1280,760",
  $url
)
$proc = Start-Process $chrome -ArgumentList $args -PassThru -WindowStyle Hidden
Write-Output ("chrome pid {0}" -f $proc.Id)

$up = $false
for ($i = 0; $i -lt 30; $i++) {
  Start-Sleep -Milliseconds 700
  try {
    $r = Invoke-WebRequest ("http://127.0.0.1:{0}/json/version" -f $Port) -UseBasicParsing -TimeoutSec 3
    if ($r.StatusCode -eq 200) { $up = $true; break }
  } catch { }
}
if (-not $up) { $proc | Stop-Process -Force -ErrorAction SilentlyContinue; throw "CDP never came up" }
Write-Output "CDP up"

Start-Sleep -Seconds $WaitSec      # let the animation run into a live frame

$cdp = Join-Path $here "cdp.js"
$jobs = Join-Path $env:TEMP ("gw-shot-jobs-" + $Port + ".json")
$body = '[{"name":"errors","eval":"(window.__ERRS||[]).length + \" js errors\""},' +
        '{"name":"canvas","eval":"(function(){var c=document.getElementById(' + "'c'" + ');return c? (c.width+\"x\"+c.height):\"no canvas\"})()"},' +
        '{"name":"readout","eval":"document.getElementById(' + "'foot'" + ').textContent.replace(/\\s+/g,\" \").trim()"},' +
        '{"name":"shot","shoot":"' + ($Out -replace "\\","/") + '"}]'
[System.IO.File]::WriteAllText($jobs, $body, (New-Object System.Text.UTF8Encoding($false)))

& node $cdp $Port $jobs
$proc | Stop-Process -Force -ErrorAction SilentlyContinue
if (Test-Path $Out) { Write-Output ("shot written: {0} bytes" -f (Get-Item $Out).Length) }
else { Write-Output "NO SHOT WRITTEN" }
