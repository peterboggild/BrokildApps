#!/usr/bin/env bash
# Install the VST3 to the two places this machine keeps them.
#
# If Peter has the plugin open in a host the copy fails with "process cannot
# access the file" — the house trick is to rename the loaded bundle out of the
# way and copy the new one in, which works while the old DLL is still mapped.
set -u
SRC="C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/full-metal-racket/plugin/build/FullMetalRacket_artefacts/Release/VST3/Full Metal Racket.vst3"

powershell -NoProfile -Command "
\$src = '$(cygpath -w "$SRC" 2>/dev/null || echo "$SRC")'
foreach (\$d in @('C:\Program Files\Common Files\VST3\Brokild','C:\Users\peter\AudioDev\VST3')) {
  \$t = Join-Path \$d 'Full Metal Racket.vst3'
  try { if (Test-Path \$t) { Remove-Item \$t -Recurse -Force -EA Stop }
        Copy-Item \$src \$d -Recurse -Force -EA Stop; Write-Output ('installed -> ' + \$t) }
  catch { \$s = Get-Date -Format 'yyyyMMdd-HHmmss'
          try { Rename-Item \$t (\"Full Metal Racket.vst3.old\$s\") -EA Stop
                Copy-Item \$src \$d -Recurse -Force
                Write-Output ('installed (renamed the live copy) -> ' + \$t) }
          catch { Write-Output ('FAILED ' + \$t + ' : ' + \$_.Exception.Message) } }
}
# a local build can leave a 0-byte moduleinfo.json when Smart App Control
# blocks the freshly built vst3_helper; hosts scan the binary without it
Get-ChildItem 'C:\Program Files\Common Files\VST3\Brokild' -Recurse -Filter moduleinfo.json -EA SilentlyContinue |
  Where-Object { \$_.Length -eq 0 } | ForEach-Object { Remove-Item \$_.FullName -Force; Write-Output ('removed 0-byte ' + \$_.FullName) }
"
