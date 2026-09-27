# Headless smoke test of the bridged page.
#
# The page still runs in a plain browser because every bridge call is wrapped
# in try/catch, so you can assert on it in ~3 seconds without building.
#
#   powershell -ExecutionPolicy Bypass -File probe-ui.ps1 -Page C:\b\MyPlugin\Source\ui\ui.html

param(
    [string]$Page = 'C:\b\MyPlugin\Source\ui\ui.html',
    [int]$Width = 1400,
    [int]$Height = 1000
)
$ErrorActionPreference = 'Stop'

$chrome = @("$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
            "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
            "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe") |
          Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $chrome) { throw 'Chrome not found' }

$tmp  = Join-Path $env:TEMP ('probe-' + (Get-Random))
New-Item -ItemType Directory -Force $tmp | Out-Null
$utf8 = New-Object System.Text.UTF8Encoding($false)

# Stub the JUCE backend and record everything the page tries to send.
$head = @'
<script>
window.__errs = []; window.__sent = [];
window.addEventListener("error", function (e) { window.__errs.push((e.message||"?") + "@" + (e.lineno||0)); });
window.__JUCE__ = { backend: {
  emitEvent: function (n, p) { window.__sent.push(p && p.b ? p.b.length : 1); },
  addEventListener: function () {}
} };
</script>
'@

# Drive the UI, then report through document.title (which --dump-dom returns).
$tail = @'
<script>
setTimeout(function () {
  var r = [];
  try {
    // --- adapt these assertions to your prototype ---
    var c = document.querySelector("canvas");
    if (c) {
      var g = c.getContext("2d");
      var d = g.getImageData(Math.floor(c.width/2), Math.floor(c.height/2), 1, 1).data;
      r.push("canvas:" + d[0] + "," + d[1] + "," + d[2]);
      var rect = c.getBoundingClientRect();
      c.dispatchEvent(new PointerEvent("pointerdown", { pointerId: 1, bubbles: true,
        clientX: rect.left + rect.width * 0.35, clientY: rect.top + rect.height * 0.45 }));
    }
    setTimeout(function () {
      r.push("msgs:" + window.__sent.reduce(function (a, b) { return a + b; }, 0));
      document.title = ("PROBE " + r.join(" | ") + " ERRS:" + JSON.stringify(window.__errs)).slice(0, 1200);
    }, 500);
  } catch (e) {
    document.title = "PROBEFAIL " + e.message;
  }
}, 1200);
</script>
'@

$src = [IO.File]::ReadAllText($Page, [Text.Encoding]::UTF8)
$i = $src.IndexOf('</head>'); if ($i -lt 0) { throw 'no </head>' }
$src = $src.Substring(0, $i) + $head + $src.Substring($i)
$j = $src.LastIndexOf('</body>'); if ($j -lt 0) { throw 'no </body>' }
$src = $src.Substring(0, $j) + $tail + $src.Substring($j)

$testPage = Join-Path $tmp 'probe.html'
[IO.File]::WriteAllText($testPage, $src, $utf8)

# Chrome's stdout is not capturable with the call operator in some shells, and
# an argument containing a comma gets split — so: one quoted string, redirected.
$dom = Join-Path $tmp 'dom.txt'
$cArgs = @('--headless=new', '--disable-gpu', '--hide-scrollbars',
           ('"--window-size=' + $Width + ',' + $Height + '"'),
           '--virtual-time-budget=5000',
           ('"--user-data-dir=' + $tmp + '\profile"'), '--no-first-run', '--dump-dom',
           ('"file:///' + ($testPage -replace '\\','/') + '"')) -join ' '
Start-Process -FilePath $chrome -ArgumentList $cArgs -Wait -NoNewWindow -RedirectStandardOutput $dom | Out-Null

$m = [regex]::Match([IO.File]::ReadAllText($dom), '<title>(PROBE[^<]*)</title>')
if (-not $m.Success) { Write-Output 'PROBE DID NOT REPORT — the page probably threw before the probe ran'; exit 1 }

$result = $m.Groups[1].Value
Write-Output $result
if ($result -match 'ERRS:\[\]') { Write-Output "`nNo JavaScript errors." }
else { Write-Output "`nJAVASCRIPT ERRORS — fix before building."; exit 1 }
