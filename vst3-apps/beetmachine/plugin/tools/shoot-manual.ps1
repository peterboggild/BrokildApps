# Beetmachine manual gate: overflow + folio check, then one PNG per page.
# The work is in shoot-manual.js (Node 22+, headless Chrome over CDP).
# ASCII only - Windows PowerShell 5.1 reads a no-BOM .ps1 as ANSI.
param(
  [string]$Html = "$PSScriptRoot\..\docs\manual\manual.html",
  [string]$Dest = "$PSScriptRoot\..\docs\manual\pages"
)
& node "$PSScriptRoot\shoot-manual.js" $Html $Dest
exit $LASTEXITCODE
