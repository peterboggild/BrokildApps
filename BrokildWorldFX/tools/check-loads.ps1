# Load every installed Brokild VST3 (both houses) with LoadLibraryW, as a host's scan does,
# and print the verdict. -Nudge appends 1-8 random bytes past the end of each BLOCKED dll
# (overlay data the loader ignores) and probes again, up to 6 times.
param([switch]$Nudge)
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public static class L {
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] public static extern IntPtr LoadLibraryExW(string p, IntPtr h, uint f);
  [DllImport("kernel32.dll", SetLastError = true)] public static extern bool FreeLibrary(IntPtr h);
}
"@
# DONT_RESOLVE_DLL_REFERENCES would skip SAC's check on dependents but not the image itself;
# a plain load is what a host does, so use flags 0.
function Probe([string]$f) {
  $h = [L]::LoadLibraryExW($f, [IntPtr]::Zero, 0)
  if ($h -ne [IntPtr]::Zero) { [void][L]::FreeLibrary($h); return 0 }
  return [Runtime.InteropServices.Marshal]::GetLastWin32Error()
}
$roots = @("C:\Program Files\Common Files\VST3\Brokild", "C:\Users\peter\AudioDev\VST3")
$rows = @()
foreach ($r in $roots) {
  if (-not (Test-Path $r)) { continue }
  Get-ChildItem $r -Recurse -Directory -Filter *.vst3 | Where-Object { $_.Name -notmatch '\.old' } | ForEach-Object {
    $dll = Get-ChildItem (Join-Path $_.FullName "Contents\x86_64-win") -Filter *.vst3 -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $dll) { return }
    $house = if ($r -like "*Program Files*") { "ProgFiles" } else { "AudioDev" }
    $grp = Split-Path (Split-Path $_.FullName -Parent) -Leaf
    $e = Probe $dll.FullName
    $note = ""
    if ($e -ne 0 -and $Nudge) {
      for ($i = 1; $i -le 6 -and $e -ne 0; $i++) {
        try {
          $n = Get-Random -Minimum 1 -Maximum 9
          $b = New-Object byte[] $n; (New-Object Random).NextBytes($b)
          $fs = [IO.File]::Open($dll.FullName, 'Append', 'Write'); $fs.Write($b, 0, $n); $fs.Close()
          $e = Probe $dll.FullName
          $note = "nudged x$i"
        } catch { $note = "nudge failed: " + $_.Exception.Message; break }
      }
    }
    $rows += [pscustomobject]@{ House = $house; Group = $grp; Plugin = $_.BaseName; Err = $e; Note = $note }
  }
}
$rows | Sort-Object House, Group, Plugin | ForEach-Object {
  $v = if ($_.Err -eq 0) { "loads" } elseif ($_.Err -eq 4551) { "BLOCKED (Smart App Control)" } else { "FAILS " + $_.Err }
  "{0,-9} {1,-28} {2,-26} {3} {4}" -f $_.House, $_.Group, $_.Plugin, $v, $_.Note
}
$bad = @($rows | Where-Object { $_.Err -ne 0 }).Count
""
"{0} bundles, {1} not loading" -f $rows.Count, $bad
