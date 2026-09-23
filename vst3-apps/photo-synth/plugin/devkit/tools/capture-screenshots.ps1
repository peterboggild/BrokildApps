# Repeatable UI screenshots for the manual and the web page.
#
# Two passes over the same page with identical setups: one --dump-dom to
# measure the target element's rectangle, one --screenshot for pixels, then
# crop. The shot name travels in the URL hash so both passes are identical.
#
# You supply a harness (see $HarnessExample below) that reads location.hash,
# sets the UI up, and writes the target rect into document.title.
#
#   powershell -ExecutionPolicy Bypass -File capture-screenshots.ps1 `
#       -Page C:\b\MyPlugin\Source\ui\ui.html -Harness .\harness.js -Out .\shots

param(
    [string]$Page    = '',
    [string]$Harness = '',
    [string]$Out     = '.\shots',
    [int]$Dpi        = 2          # device scale factor
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

if (-not $Page)    { throw 'pass -Page' }
if (-not $Harness) { throw 'pass -Harness' }

$chrome = @("$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
            "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
            "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe") |
          Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $chrome) { throw 'Chrome not found' }

New-Item -ItemType Directory -Force $Out | Out-Null
$utf8 = New-Object System.Text.UTF8Encoding($false)

# name / window size per shot — add your own
$shots = @(
  @{ n = 'hero';   w = 1300; h = 900  },
  @{ n = 'panel';  w = 1400; h = 2000 }
)

# Build the capture page once: prototype + harness.
$src = [IO.File]::ReadAllText($Page, [Text.Encoding]::UTF8)
$h   = [IO.File]::ReadAllText($Harness, [Text.Encoding]::UTF8)
$i   = $src.LastIndexOf('</body>'); if ($i -lt 0) { throw 'no </body>' }
$capture = Join-Path $env:TEMP ('capture-' + (Get-Random) + '.html')
[IO.File]::WriteAllText($capture, ($src.Substring(0, $i) + $h + $src.Substring($i)), $utf8)
$url = 'file:///' + ($capture -replace '\\','/')

function Invoke-Chrome([string[]]$extra, [int]$w, [int]$h, [string]$u, [string]$stdout) {
    $udd = Join-Path $env:TEMP ('shot-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
    # One quoted string — an array element containing a comma is split and the
    # remainder read as a second URL ("Multiple targets are not supported").
    $line = (@('--headless=new', '--disable-gpu', '--hide-scrollbars',
               ('--force-device-scale-factor=' + $Dpi),
               ('"--window-size=' + $w + ',' + $h + '"'),
               '--virtual-time-budget=4500',
               ('"--user-data-dir=' + $udd + '"'), '--no-first-run',
               '--no-default-browser-check') + $extra + @(('"' + $u + '"'))) -join ' '
    if ($stdout) { Start-Process -FilePath $chrome -ArgumentList $line -Wait -NoNewWindow -RedirectStandardOutput $stdout | Out-Null }
    else         { Start-Process -FilePath $chrome -ArgumentList $line -Wait -NoNewWindow | Out-Null }
}

# ---- pass 1: rectangles ----
$rects = @{}
$domFile = Join-Path $env:TEMP 'shot-dom.txt'
foreach ($s in $shots) {
    [IO.File]::Delete($domFile)
    Invoke-Chrome @('--dump-dom') $s.w $s.h ($url + '#' + $s.n) $domFile
    if (-not (Test-Path $domFile)) { Write-Output ('no dom: ' + $s.n); continue }
    $m = [regex]::Match([IO.File]::ReadAllText($domFile), '<title>(RECT[^<]*)</title>')
    if (-not $m.Success) { Write-Output ('no rect: ' + $s.n); continue }
    $rects[$s.n] = $m.Groups[1].Value
    Write-Output ($s.n + ' -> ' + $rects[$s.n])
}

# ---- pass 2: pixels, then crop ----
foreach ($s in $shots) {
    if (-not $rects.ContainsKey($s.n)) { continue }
    $raw = Join-Path $Out ($s.n + '-raw.png')
    $fin = Join-Path $Out ($s.n + '.png')
    [IO.File]::Delete($raw)

    $ok = $false
    for ($try = 1; $try -le 3 -and -not $ok; $try++) {
        Invoke-Chrome @('"--screenshot=' + $raw + '"') $s.w $s.h ($url + '#' + $s.n) $null
        if (Test-Path $raw) { $ok = $true } else { Start-Sleep -Milliseconds 700 }
    }
    if (-not $ok) { Write-Output ('screenshot failed: ' + $s.n); continue }

    $spec = ($rects[$s.n] -replace '^RECT\s+','') -replace '\s+ERRS.*$',''
    if ($spec -eq 'full') { Copy-Item $raw $fin -Force }
    else {
        $p = $spec.Split(',')
        $img = [System.Drawing.Image]::FromFile($raw)
        try {
            $pad = 16 * $Dpi
            $x = [int]$p[0] * $Dpi - $pad; $y = [int]$p[1] * $Dpi - $pad
            $w = [int]$p[2] * $Dpi + 2 * $pad; $h = [int]$p[3] * $Dpi + 2 * $pad
            if ($x -lt 0) { $w += $x; $x = 0 }
            if ($y -lt 0) { $h += $y; $y = 0 }
            if ($x + $w -gt $img.Width)  { $w = $img.Width  - $x }
            if ($y + $h -gt $img.Height) { $h = $img.Height - $y }
            $bmp = New-Object System.Drawing.Bitmap $w, $h
            $g = [System.Drawing.Graphics]::FromImage($bmp)
            $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $w, $h),
                               (New-Object System.Drawing.Rectangle $x, $y, $w, $h),
                               [System.Drawing.GraphicsUnit]::Pixel)
            $g.Dispose()
            $bmp.Save($fin, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
        } finally { $img.Dispose() }
    }
    [IO.File]::Delete($raw)
    Write-Output ('  saved ' + (Split-Path $fin -Leaf))
}

$HarnessExample = @'
--- harness.js (append to the page; reads location.hash) --------------------
<script>
window.__JUCE__ = { backend: { emitEvent: function(){}, addEventListener: function(){} } };
(function () {
  var SHOTS = {
    "hero":  { setup: function () { /* click into the state you want */ },
               target: function () { return null; } },        // null = full viewport
    "panel": { setup: function () { document.getElementById("settings").open = true; },
               target: function () { return document.getElementById("settings"); } }
  };
  var name = (location.hash || "").replace(/^#/, "") || "hero";
  setTimeout(function () {
    var s = SHOTS[name]; if (!s) { document.title = "RECT-ERR"; return; }
    s.setup();
    setTimeout(function () {
      var t = s.target();
      var r = t == null ? null : t.getBoundingClientRect();
      document.title = "RECT " + (r == null ? "full"
        : [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)].join(","));
    }, 900);
  }, 700);
})();
</script>
'@
