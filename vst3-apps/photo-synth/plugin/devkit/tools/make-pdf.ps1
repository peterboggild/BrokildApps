# Render a print-styled HTML manual to PDF with headless Chrome.
#
#   powershell -ExecutionPolicy Bypass -File make-pdf.ps1 `
#       -Html C:\b\MyPlugin\docs\manual\manual.html `
#       -Pdf  C:\b\MyPlugin\docs\manual\My-Plugin-Manual.pdf

param(
    [string]$Html = '',
    [string]$Pdf  = ''
)
$ErrorActionPreference = 'Stop'
if (-not $Html) { throw 'pass -Html' }
if (-not $Pdf)  { $Pdf = [IO.Path]::ChangeExtension($Html, '.pdf') }

$chrome = @("$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
            "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
            "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe") |
          Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $chrome) { throw 'Chrome not found' }

[IO.File]::Delete($Pdf)
$udd = Join-Path $env:TEMP ('pdf-' + (Get-Random))

# One quoted string: a bare comma or space in an argument array gets re-parsed.
$cArgs = @('--headless=new', '--disable-gpu', '--no-pdf-header-footer',
           '--virtual-time-budget=9000', '--no-first-run', '--no-default-browser-check',
           ('"--user-data-dir=' + $udd + '"'),
           ('"--print-to-pdf=' + $Pdf + '"'),
           ('"file:///' + ($Html -replace '\\','/') + '"')) -join ' '
Start-Process -FilePath $chrome -ArgumentList $cArgs -Wait -NoNewWindow | Out-Null

if (-not (Test-Path $Pdf)) { throw 'no PDF produced' }

$txt   = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($Pdf))
$pages = ([regex]::Matches($txt, '/Type\s*/Page[^s]')).Count
Write-Output ("{0}`n  {1} pages, {2} KB" -f $Pdf, $pages, [math]::Round((Get-Item $Pdf).Length / 1KB))

# Reminders that catch the two usual layout faults:
#  - a stray blank page means a full-bleed cover is exactly the page height;
#    make it 2-3 mm shorter.
#  - missing backgrounds mean print-color-adjust: exact is not set.
