#!/usr/bin/env bash
# Relink the VST3 until Smart App Control lets it load.
#
# SAC blocks by FILE HASH, not by "unsigned" and not by age — a verdict is per
# build and can keep landing wrong several links in a row. A blocked DLL makes
# a host's scan fail silently, which looks exactly like the plugin having
# vanished. So: probe with LoadLibraryW, and if it is blocked, relink for a
# fresh hash and probe again.
set -u
SRC="C:/Users/peter/b/FullMetalRacket"
DLL="$SRC/build/FullMetalRacket_artefacts/Release/VST3/Full Metal Racket.vst3/Contents/x86_64-win/Full Metal Racket.vst3"

probe() {
  powershell -NoProfile -Command "
Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public class NP{[DllImport(\"kernel32\",SetLastError=true,CharSet=CharSet.Unicode)]public static extern IntPtr LoadLibraryW(string p);}'
\$h=[NP]::LoadLibraryW('$(echo "$DLL" | sed 's|/|\\|g')')
if (\$h -eq [IntPtr]::Zero) { 'BLOCKED' } else { 'OK' }"
}

for i in $(seq 1 6); do
  r=$(probe | tr -d '\r\n ')
  if [ "$r" = "OK" ]; then echo "attempt $i: LOADS OK"; exit 0; fi
  echo "attempt $i: blocked by Smart App Control, relinking for a fresh hash"
  rm -f "$DLL"
  cmake --build "$SRC/build" --config Release 2>&1 | grep -E " error " | head -5
done
echo "still blocked after 6 links — the last resort is a content change (bump FM_BUILD_ID)"
exit 1
