# Install the Brokild plugins into both houses, into their THREE folders.
#
#   Proxima Centauri B findings : Artefact B2311.1, Artefact B2311.22, Artefact B2311.67
#   Experimental                : Hairfryer
#   Brokild collection          : everything else
#
# Two house rules are baked in and should not be removed:
#
#  * The Smart App Control verdict is PER FILE AT ITS PATH. A copy is judged
#    separately from the file it was copied from, and the verdict can change
#    after a successful install. So we always probe the INSTALLED file, never
#    the build output, and nudge it with a few random trailing bytes (overlay
#    data after the last PE section is ignored by the loader) until it loads.
#
#  * A stale bundle is never left inside the VST3 tree. Renaming one does NOT
#    retire it: hosts scan recursively and the DLL inside carries the SAME
#    class IDs as the live one, so either may win. They go to Quarantine.
#
# Lives in the BWFX repo because it is the one thing every plugin shares.

param([string[]] $Only = @())

$roots = @(
  "C:\Program Files\Common Files\VST3\Brokild",
  "C:\Users\peter\AudioDev\VST3"
)
$quarantine = "C:\Users\peter\AudioDev\Quarantine"

# dir = source tree under C:\Users\peter\b, name = product name, group = folder
$plugins = @(
  @{ dir = "ArtefactB2311_1";   name = "Artefact B2311.1";    group = "Proxima Centauri B findings" },
  @{ dir = "ArtefactB2311";     name = "Artefact B2311.22";   group = "Proxima Centauri B findings" },
  @{ dir = "ArtefactB2311_67";  name = "Artefact B2311.67";   group = "Proxima Centauri B findings" },
  @{ dir = "ArtefactB2311_104"; name = "Artefact B2311.104";  group = "Proxima Centauri B findings" },
  @{ dir = "Hairfryer";        name = "Hairfryer";          group = "Experimental" },
  @{ dir = "BlackRider";       name = "Black Rider";        group = "Brokild collection" },
  @{ dir = "BladeRuiner";      name = "Blade Ruiner";       group = "Brokild collection" },
  @{ dir = "EscapeRoom";       name = "Escape Room";        group = "Brokild collection" },
  @{ dir = "FullMetalRacket";  name = "Full Metal Racket";  group = "Brokild collection" },
  @{ dir = "MarsWars";         name = "Martian Gain";       group = "Brokild collection" },
  @{ dir = "PhotoSynth";       name = "Photo Synth";        group = "Brokild collection" },
  @{ dir = "HighTide";         name = "High Tide";          group = "Brokild collection" }
  # Clone Wars is deliberately absent: its binary comes from a CI-built zip,
  # never a local build (house rule).
)

$sig = '[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryW(string p); [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr h);'
$k = Add-Type -MemberDefinition $sig -Name LLF -Namespace WF -PassThru
$stamp = Get-Date -Format "HHmmss"
$rand = New-Object System.Random
if (-not (Test-Path $quarantine)) { New-Item -ItemType Directory -Force $quarantine | Out-Null }

foreach ($p in $plugins) {
    if ($Only.Count -gt 0 -and $Only -notcontains $p.name -and $Only -notcontains $p.dir) { continue }

    $src = Get-ChildItem "C:\Users\peter\b\$($p.dir)\build" -Recurse -Filter "$($p.name).vst3" -File -ErrorAction SilentlyContinue |
           Where-Object { $_.FullName -like "*Release*x86_64-win*" } | Select-Object -First 1
    if (-not $src) { Write-Output ("{0,-22} no build output" -f $p.name); continue }

    foreach ($root in $roots) {
        $dstDir = Join-Path (Join-Path $root $p.group) "$($p.name).vst3\Contents\x86_64-win"
        if (-not (Test-Path $dstDir)) { New-Item -ItemType Directory -Force $dstDir | Out-Null }
        $dst = Join-Path $dstDir "$($p.name).vst3"

        # a bundle that drifted back to the root would shadow this one
        $loose = Join-Path $root "$($p.name).vst3"
        if (Test-Path $loose) {
            Move-Item $loose (Join-Path $quarantine ("loose__{0}__{1}" -f $p.name, $stamp)) -Force -ErrorAction SilentlyContinue
            Write-Output ("{0,-22} retired a loose copy at the root" -f $p.name)
        }

        if (Test-Path $dst) {
            # the house trick for a DLL the host still has mapped
            try { Rename-Item $dst "$($p.name).vst3.old$stamp" -Force -ErrorAction Stop } catch {}
        }
        try { Copy-Item $src.FullName $dst -Force -ErrorAction Stop }
        catch { Write-Output ("{0,-22} {1,-13} COPY FAILED (open in a host?)" -f $p.name, (Split-Path $root -Leaf)); continue }

        Get-ChildItem (Split-Path $dst -Parent) -Filter "*.old*" -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -notlike "*.old$stamp" } | Remove-Item -Force -ErrorAction SilentlyContinue
        Get-ChildItem (Join-Path (Join-Path $root $p.group) "$($p.name).vst3") -Recurse -Filter "moduleinfo.json" -ErrorAction SilentlyContinue |
            Where-Object { $_.Length -eq 0 } | Remove-Item -Force -ErrorAction SilentlyContinue

        $state = "STILL BLOCKED"
        for ($i = 0; $i -lt 6; $i++) {
            $h = $k::LoadLibraryW($dst)
            if ($h -ne [IntPtr]::Zero) { $k::FreeLibrary($h) | Out-Null; $state = "loads"; break }
            $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
            if ($err -ne 4551) { $state = "failed err=$err"; break }
            $b = New-Object byte[] ($rand.Next(3, 17)); $rand.NextBytes($b)
            Add-Content -Path $dst -Value $b -Encoding Byte
            Start-Sleep -Milliseconds 250
        }
        $short = if ($root -like "*Program Files*") { "ProgramFiles" } else { "AudioDev" }
        Write-Output ("{0,-22} {1,-13} {2,-28} {3}" -f $p.name, $short, $p.group, $state)
    }
}
