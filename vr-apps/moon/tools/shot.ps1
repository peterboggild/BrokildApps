# Screenshot the flat preview with headless Chrome (software GL), for checking a view without a headset.
#   .\shot.ps1 "view=surface&yaw=40" out.png [budgetMs] [width] [height]
# Needs the repository root served on port 8765:  python -m http.server 8765
param([string]$query = "view=surface", [string]$out = "shot.png", [int]$budget = 30000, [int]$w = 1400, [int]$h = 900)
$chrome = "C:\Program Files\Google\Chrome\Application\chrome.exe"
$profile = Join-Path $env:TEMP ("moonshot-" + [guid]::NewGuid().ToString("N").Substring(0, 8))
$log = "$out.log"
$args = @("--headless=new", "--user-data-dir=$profile", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
  "--ignore-gpu-blocklist", "--hide-scrollbars", "--window-size=$w,$h", "--virtual-time-budget=$budget",
  "--enable-logging=stderr", "--v=0", "--screenshot=$out", "http://127.0.0.1:8765/vr-apps/moon/?still&shot&$query")
$p = Start-Process $chrome -ArgumentList $args -RedirectStandardError $log -PassThru -WindowStyle Hidden
if (-not $p.WaitForExit(240000)) { $p.Kill(); "timeout" }
Select-String -Path $log -Pattern "CONSOLE|ERROR:gl|shader" | Select-Object -First 12 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(300, $_.Line.Length)) }
Remove-Item -Recurse -Force $profile -ErrorAction SilentlyContinue
