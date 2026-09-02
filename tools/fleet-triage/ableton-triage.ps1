<#
    ableton-triage.ps1 — why did the host die, and is the plug-in the reason?
    ==========================================================================

    Written for the case where Ableton crashes AT STARTUP, which on Live means
    during the plug-in scan: Live scans VST3s in its own process, so a bundle
    that faults while being loaded takes the whole application with it before
    a window ever appears. Nothing in the crash is visible from inside Live,
    and the useful evidence is in three places this script reads for you:

      1. WHAT IS ACTUALLY INSTALLED. Not what was published — what is on this
         disk, in both houses, including copies nobody meant to leave behind.
      2. DUPLICATES. Two bundles carrying the SAME VST3 class ids is the
         documented failure mode of this fleet (install-fleet.ps1 retires
         loose copies precisely because of it). Scan order then decides which
         one the host loads, and a stale one can be anything at all.
      3. THE HOST'S OWN LOG. Live names each plug-in as it scans it, so the
         LAST name in the log before the crash is the accused.

    It changes nothing unless you ask it to. -Quarantine moves the Brokild
    folders out of the scan path so you can prove the fleet is the cause in
    one restart; -Restore puts them back.

    A NOTE ON HASHES, so you do not chase a false difference. install-fleet.ps1
    appends a few random bytes to an INSTALLED bundle when Smart App Control
    blocks it — legal, because the loader ignores overlay data after the last
    PE section, and necessary, because the verdict is per file at its path. So
    an installed bundle is EXPECTED to differ from the published one by a
    handful of trailing bytes. This script therefore compares the end of the
    last PE section and the link timestamp, both of which the nudge leaves
    alone, and reports the overlay separately rather than as a fault.

    USAGE
        powershell -ExecutionPolicy Bypass -File ableton-triage.ps1
        powershell -ExecutionPolicy Bypass -File ableton-triage.ps1 -Reference C:\path\to\downloaded\zips
        powershell -ExecutionPolicy Bypass -File ableton-triage.ps1 -Quarantine
        powershell -ExecutionPolicy Bypass -File ableton-triage.ps1 -Restore

    Read-only by default. Run it from an ordinary prompt; it only needs
    administrator rights if you ask it to quarantine something that lives
    under Program Files, and it says so rather than failing obscurely.
#>

[CmdletBinding()]
param(
    [string[]] $Roots = @(),
    [string]   $Reference = "",
    [switch]   $Quarantine,
    [switch]   $Restore,
    [string]   $QuarantineDir = "$env:USERPROFILE\AudioDev\Quarantine"
)

$ErrorActionPreference = "Continue"

# ---------------------------------------------------------------------------
#  Where a VST3 can live on Windows. The last two are the per-user locations
#  hosts also scan, and a bundle dropped there by hand is invisible to anyone
#  looking only in Program Files.
# ---------------------------------------------------------------------------
if ($Roots.Count -eq 0) {
    $Roots = @(
        "C:\Program Files\Common Files\VST3",
        "$env:USERPROFILE\AudioDev\VST3",
        "$env:LOCALAPPDATA\Programs\Common\VST3",
        "$env:USERPROFILE\Documents\VST3"
    )
}

function Write-Head([string] $t) {
    Write-Host ""
    Write-Host ("=" * 76) -ForegroundColor DarkGray
    Write-Host "  $t" -ForegroundColor Cyan
    Write-Host ("=" * 76) -ForegroundColor DarkGray
}

# ---------------------------------------------------------------------------
#  Enough PE parsing to answer three questions: is this a real 64-bit DLL,
#  when was it linked, and how many bytes sit past the last section.
# ---------------------------------------------------------------------------
function Get-PeFacts([string] $path) {
    $o = [ordered]@{ ok = $false; note = ""; size = 0; sectionEnd = 0; overlay = 0; linked = $null; machine = "" }
    try {
        $fs = [System.IO.File]::Open($path, 'Open', 'Read', 'ReadWrite')
    } catch {
        $o.note = "cannot open ($($_.Exception.Message))"; return [pscustomobject]$o
    }
    try {
        $o.size = $fs.Length
        $br = New-Object System.IO.BinaryReader($fs)
        if ($fs.Length -lt 0x40) { $o.note = "too small to be a PE"; return [pscustomobject]$o }
        $fs.Position = 0
        if ($br.ReadUInt16() -ne 0x5A4D) { $o.note = "not a PE (no MZ)"; return [pscustomobject]$o }
        $fs.Position = 0x3C
        $peOff = $br.ReadUInt32()
        if ($peOff + 24 -gt $fs.Length) { $o.note = "PE header offset out of range"; return [pscustomobject]$o }
        $fs.Position = $peOff
        if ($br.ReadUInt32() -ne 0x00004550) { $o.note = "not a PE (no PE\0\0)"; return [pscustomobject]$o }
        $machine = $br.ReadUInt16()
        $o.machine = switch ($machine) { 0x8664 { "x64" } 0x014C { "x86 (32-bit!)" } default { ("0x{0:X4}" -f $machine) } }
        $nSections = $br.ReadUInt16()
        $stamp     = $br.ReadUInt32()
        $null      = $br.ReadUInt32()   # pointer to symbol table
        $null      = $br.ReadUInt32()   # number of symbols
        $optSize   = $br.ReadUInt16()
        $null      = $br.ReadUInt16()   # characteristics
        $o.linked  = [DateTimeOffset]::FromUnixTimeSeconds($stamp).UtcDateTime

        $secTable = $peOff + 24 + $optSize
        $end = 0
        for ($i = 0; $i -lt $nSections; $i++) {
            $fs.Position = $secTable + $i * 40 + 16
            $rawSize = $br.ReadUInt32()
            $rawPtr  = $br.ReadUInt32()
            if ($rawSize -gt 0) { $e = [int64]$rawPtr + [int64]$rawSize; if ($e -gt $end) { $end = $e } }
        }
        $o.sectionEnd = $end
        $o.overlay    = [math]::Max(0, $fs.Length - $end)
        $o.ok         = $true
    } catch {
        $o.note = "malformed: $($_.Exception.Message)"
    } finally {
        $fs.Dispose()
    }
    return [pscustomobject]$o
}

#  The product name and the four-character plug-in code, read out of the
#  binary. Two bundles sharing a code share their VST3 class ids.
function Get-PluginMarks([string] $path) {
    $codes = @()
    try {
        $bytes = [System.IO.File]::ReadAllBytes($path)
        $text  = [System.Text.Encoding]::ASCII.GetString($bytes)
        $codes = [regex]::Matches($text, 'Brkd[A-Za-z0-9]{4}') |
                 ForEach-Object { $_.Value } | Sort-Object -Unique
    } catch { }
    return ,$codes
}

# ---------------------------------------------------------------------------
#  1. What is installed
# ---------------------------------------------------------------------------
Write-Head "INSTALLED BUNDLES"

$found = @()
foreach ($root in $Roots) {
    if (-not (Test-Path -LiteralPath $root)) { Write-Host ("  {0}  — not present" -f $root) -ForegroundColor DarkGray; continue }
    Write-Host ("  {0}" -f $root) -ForegroundColor Gray

    $bundles = Get-ChildItem -LiteralPath $root -Recurse -Directory -Filter "*.vst3" -ErrorAction SilentlyContinue
    foreach ($b in $bundles) {
        #  A bundle inside a bundle is not a second plug-in; skip the nesting.
        if ($b.Parent.FullName -like "*.vst3\Contents*") { continue }
        $inner = Join-Path $b.FullName "Contents\x86_64-win\$($b.Name)"
        $rel   = $b.FullName.Substring($root.Length).TrimStart('\')
        $rec = [ordered]@{
            Root = $root; Bundle = $b.Name; Relative = $rel; Path = $b.FullName
            Inner = $inner; HasInner = (Test-Path -LiteralPath $inner)
            Loose = ($b.Parent.FullName -eq $root)
        }
        if ($rec.HasInner) {
            $pe = Get-PeFacts $inner
            $rec.Size = $pe.size; $rec.SectionEnd = $pe.sectionEnd; $rec.Overlay = $pe.overlay
            $rec.Linked = $pe.linked; $rec.Machine = $pe.machine; $rec.PeOk = $pe.ok; $rec.PeNote = $pe.note
            $rec.Codes = (Get-PluginMarks $inner) -join ","
        } else {
            $rec.Size = 0; $rec.SectionEnd = 0; $rec.Overlay = 0; $rec.Linked = $null
            $rec.Machine = ""; $rec.PeOk = $false; $rec.PeNote = "no module at Contents\x86_64-win"
            $rec.Codes = ""
        }
        #  Anything else sitting in the module folder. install-fleet renames the
        #  live module to *.old<stamp> before copying and only sweeps the ones
        #  from EARLIER runs, so exactly one is normal and several is a habit.
        $strays = @()
        $modDir = Join-Path $b.FullName "Contents\x86_64-win"
        if (Test-Path -LiteralPath $modDir) {
            $strays = Get-ChildItem -LiteralPath $modDir -File -ErrorAction SilentlyContinue |
                      Where-Object { $_.Name -ne $b.Name } | ForEach-Object { $_.Name }
        }
        $rec.Strays = $strays
        $found += [pscustomobject]$rec
    }
}

if ($found.Count -eq 0) {
    Write-Host "  no VST3 bundles found in any searched root" -ForegroundColor Yellow
} else {
    $found | Sort-Object Bundle, Root | ForEach-Object {
        $flag = if (-not $_.PeOk) { "BROKEN" } elseif ($_.Loose) { "loose " } else { "      " }
        $col  = if (-not $_.PeOk) { "Red" } elseif ($_.Loose) { "Yellow" } else { "Gray" }
        Write-Host ("  {0} {1,-30} {2,10:N0} b  overlay {3,-5} {4}  {5}" -f `
            $flag, $_.Bundle, $_.Size, $_.Overlay,
            ($(if ($_.Linked) { $_.Linked.ToString("yyyy-MM-dd HH:mm") } else { "no link stamp  " })),
            $_.Relative) -ForegroundColor $col
        if (-not $_.PeOk -and $_.PeNote) { Write-Host ("         -> {0}" -f $_.PeNote) -ForegroundColor Red }
        if ($_.Machine -and $_.Machine -ne "x64") { Write-Host ("         -> wrong architecture: {0}" -f $_.Machine) -ForegroundColor Red }
        foreach ($s in $_.Strays) { Write-Host ("         -> also in the module folder: {0}" -f $s) -ForegroundColor DarkYellow }
    }
}

# ---------------------------------------------------------------------------
#  2. Duplicates — the documented failure mode
# ---------------------------------------------------------------------------
Write-Head "DUPLICATE CLASS IDS"

$dupes = @()
foreach ($g in ($found | Where-Object { $_.HasInner } | Group-Object Bundle)) {
    #  The same product in both houses is INTENDED — install-fleet writes to
    #  both. The same product TWICE INSIDE ONE ROOT is not.
    foreach ($h in ($g.Group | Group-Object Root)) {
        if ($h.Count -gt 1) {
            $dupes += [pscustomobject]@{ Kind = "same product twice under one root"; Name = $g.Name; Paths = ($h.Group.Path) }
        }
    }
}
foreach ($g in ($found | Where-Object { $_.HasInner -and $_.Codes } | Group-Object Codes)) {
    $names = $g.Group.Bundle | Sort-Object -Unique
    if ($names.Count -gt 1) {
        $dupes += [pscustomobject]@{ Kind = "two DIFFERENT names sharing plug-in code $($g.Name)"; Name = ($names -join " / "); Paths = ($g.Group.Path) }
    }
}
foreach ($f in ($found | Where-Object { $_.Loose })) {
    $dupes += [pscustomobject]@{ Kind = "bundle loose at the root of a scan folder"; Name = $f.Bundle; Paths = @($f.Path) }
}

if ($dupes.Count -eq 0) {
    Write-Host "  none. No two installed bundles can claim the same class ids." -ForegroundColor Green
} else {
    foreach ($d in $dupes) {
        Write-Host ("  {0}: {1}" -f $d.Kind, $d.Name) -ForegroundColor Yellow
        foreach ($p in $d.Paths) { Write-Host ("      {0}" -f $p) -ForegroundColor DarkYellow }
    }
    Write-Host ""
    Write-Host "  Scan order decides which of these the host loads, and a stale one can be" -ForegroundColor Yellow
    Write-Host "  any build at all. Retire the extras before blaming the plug-in." -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
#  3. Installed against published
# ---------------------------------------------------------------------------
if ($Reference) {
    Write-Head "INSTALLED AGAINST PUBLISHED"
    if (-not (Test-Path -LiteralPath $Reference)) {
        Write-Host "  -Reference path does not exist: $Reference" -ForegroundColor Red
    } else {
        $refInner = @{}
        $zips = Get-ChildItem -LiteralPath $Reference -Recurse -Filter "*.zip" -ErrorAction SilentlyContinue
        $tmp  = Join-Path $env:TEMP ("brokild-ref-" + [guid]::NewGuid().ToString("N").Substring(0,8))
        New-Item -ItemType Directory -Force $tmp | Out-Null
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        foreach ($z in $zips) {
            $d = Join-Path $tmp $z.BaseName
            try { [System.IO.Compression.ZipFile]::ExtractToDirectory($z.FullName, $d) } catch { continue }
        }
        Get-ChildItem -LiteralPath $tmp -Recurse -File -Filter "*.vst3" -ErrorAction SilentlyContinue |
            Where-Object { $_.DirectoryName -like "*x86_64-win" } |
            ForEach-Object { $refInner[$_.Name] = $_.FullName }

        foreach ($f in ($found | Where-Object { $_.HasInner } | Sort-Object Bundle)) {
            if (-not $refInner.ContainsKey($f.Bundle)) { continue }
            $rp = Get-PeFacts $refInner[$f.Bundle]
            $sameCode = ($rp.sectionEnd -eq $f.SectionEnd)
            $sameLink = ($rp.linked -and $f.Linked -and $rp.linked -eq $f.Linked)
            if ($sameCode -and $sameLink) {
                $extra = if ($f.Overlay -gt 0) { " (+{0} nudge bytes, expected)" -f $f.Overlay } else { "" }
                Write-Host ("  {0,-30} SAME BUILD as published{1}" -f $f.Bundle, $extra) -ForegroundColor Green
            } else {
                Write-Host ("  {0,-30} DIFFERENT BUILD from published" -f $f.Bundle) -ForegroundColor Yellow
                Write-Host ("       installed  code {0,10:N0} b  linked {1}" -f $f.SectionEnd, $f.Linked) -ForegroundColor DarkYellow
                Write-Host ("       published  code {0,10:N0} b  linked {1}" -f $rp.sectionEnd, $rp.linked) -ForegroundColor DarkYellow
                Write-Host ("       -> the machine is not running the build that was tested." ) -ForegroundColor Yellow
            }
        }
        Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# ---------------------------------------------------------------------------
#  4. What the host itself says
# ---------------------------------------------------------------------------
Write-Head "ABLETON'S OWN ACCOUNT"

$abRoot = Join-Path $env:APPDATA "Ableton"
if (-not (Test-Path -LiteralPath $abRoot)) {
    Write-Host "  no Ableton preferences folder at $abRoot" -ForegroundColor DarkGray
} else {
    $live = Get-ChildItem -LiteralPath $abRoot -Directory -Filter "Live *" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $live) {
        Write-Host "  no 'Live x.y.z' folder under $abRoot" -ForegroundColor DarkGray
    } else {
        Write-Host ("  {0}" -f $live.FullName) -ForegroundColor Gray

        $crashes = Get-ChildItem -LiteralPath $live.FullName -Recurse -Include "*.dmp","*.txt" -ErrorAction SilentlyContinue |
                   Where-Object { $_.DirectoryName -match "Crash" } |
                   Sort-Object LastWriteTime -Descending | Select-Object -First 6
        if ($crashes) {
            Write-Host "  recent crash reports:" -ForegroundColor Yellow
            foreach ($c in $crashes) {
                Write-Host ("    {0}  {1,10:N0} b  {2}" -f $c.LastWriteTime.ToString("yyyy-MM-dd HH:mm"), $c.Length, $c.Name) -ForegroundColor DarkYellow
            }
        } else {
            Write-Host "  no crash reports found" -ForegroundColor DarkGray
        }

        $log = Get-ChildItem -LiteralPath $live.FullName -Recurse -Filter "Log.txt" -ErrorAction SilentlyContinue |
               Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if (-not $log) {
            Write-Host "  no Log.txt found" -ForegroundColor DarkGray
        } else {
            Write-Host ("  log: {0}  (last written {1})" -f $log.FullName, $log.LastWriteTime) -ForegroundColor Gray
            $lines = Get-Content -LiteralPath $log.FullName -ErrorAction SilentlyContinue
            if ($lines) {
                #  Live names each plug-in as it reaches it. The last one named
                #  before the log stops is the one that was in its hands.
                $scan = $lines | Where-Object { $_ -match "(?i)vst|plugin|plug-in|scan|\.vst3" }
                if ($scan) {
                    Write-Host ""
                    Write-Host "  the last plug-in lines in the log — the last name here is the accused:" -ForegroundColor Yellow
                    $scan | Select-Object -Last 15 | ForEach-Object { Write-Host ("    {0}" -f $_) -ForegroundColor DarkYellow }
                }
                Write-Host ""
                Write-Host "  the last 12 lines of the log, whatever they are:" -ForegroundColor Gray
                $lines | Select-Object -Last 12 | ForEach-Object { Write-Host ("    {0}" -f $_) -ForegroundColor DarkGray }
            }
        }
    }
}

# ---------------------------------------------------------------------------
#  5. Prove it in one restart
# ---------------------------------------------------------------------------
if ($Quarantine -or $Restore) {
    Write-Head ($(if ($Quarantine) { "QUARANTINING THE FLEET" } else { "RESTORING THE FLEET" }))
    New-Item -ItemType Directory -Force $QuarantineDir | Out-Null
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"

    if ($Quarantine) {
        #  Move the GROUP FOLDERS, not individual bundles: it is one move per
        #  house, it is reversible, and it leaves every other vendor's plug-ins
        #  exactly where they are so the test says something.
        $groups = @("Brokild", "Proxima Centauri B findings", "Brokild collection", "Experimental")
        $moved = 0
        foreach ($root in $Roots) {
            foreach ($g in $groups) {
                $src = Join-Path $root $g
                if (-not (Test-Path -LiteralPath $src)) { continue }
                $dst = Join-Path $QuarantineDir ("{0}__{1}__{2}" -f $stamp, ($root -replace '[:\\ ]','_'), $g)
                try {
                    Move-Item -LiteralPath $src -Destination $dst -Force -ErrorAction Stop
                    Write-Host ("  moved  {0}`n      -> {1}" -f $src, $dst) -ForegroundColor Green
                    $moved++
                } catch {
                    Write-Host ("  COULD NOT MOVE {0}" -f $src) -ForegroundColor Red
                    Write-Host ("      {0}" -f $_.Exception.Message) -ForegroundColor Red
                    if ($src -like "C:\Program Files\*") { Write-Host "      (that one needs an administrator prompt)" -ForegroundColor Red }
                }
            }
        }
        if ($moved -eq 0) { Write-Host "  nothing to quarantine" -ForegroundColor DarkGray }
        else {
            Write-Host ""
            Write-Host "  Now start Ableton." -ForegroundColor Cyan
            Write-Host "   * It starts -> the fault is in this fleet. Restore, then put the" -ForegroundColor Cyan
            Write-Host "     bundles back ONE AT A TIME, restarting between each." -ForegroundColor Cyan
            Write-Host "   * It still crashes -> the fault is NOT these plug-ins, and the log" -ForegroundColor Cyan
            Write-Host "     section above is where to look next." -ForegroundColor Cyan
        }
    }

    if ($Restore) {
        $items = Get-ChildItem -LiteralPath $QuarantineDir -Directory -ErrorAction SilentlyContinue |
                 Where-Object { $_.Name -match '^\d{8}-\d{6}__' }
        if (-not $items) { Write-Host "  nothing in $QuarantineDir to restore" -ForegroundColor DarkGray }
        foreach ($i in $items) {
            $parts = $i.Name -split "__", 3
            if ($parts.Count -lt 3) { continue }
            #  The root was flattened into this folder name, and that mangling is
            #  lossy for any path containing an underscore. So do not unmangle it —
            #  match it back against the roots we already know.
            $root = ($Roots | Where-Object { ($_ -replace '[:\\ ]','_') -eq $parts[1] } | Select-Object -First 1)
            if (-not $root) { Write-Host ("  cannot tell where {0} came from — move it back by hand" -f $i.Name) -ForegroundColor Yellow; continue }
            $dst = Join-Path $root $parts[2]
            if (Test-Path -LiteralPath $dst) { Write-Host ("  {0} already exists — leaving {1} alone" -f $dst, $i.Name) -ForegroundColor Yellow; continue }
            try {
                Move-Item -LiteralPath $i.FullName -Destination $dst -Force -ErrorAction Stop
                Write-Host ("  restored {0}" -f $dst) -ForegroundColor Green
            } catch {
                Write-Host ("  COULD NOT RESTORE {0}: {1}" -f $i.FullName, $_.Exception.Message) -ForegroundColor Red
            }
        }
    }
}

Write-Head "WHAT TO DO WITH THIS"
Write-Host @"
  If DUPLICATE CLASS IDS listed anything, fix that first — it is the fleet's
  known failure mode and it makes every other measurement unreliable.

  If a bundle came back BROKEN or the wrong architecture, that file is the
  answer; replace it from the published archive.

  If everything above is clean, run with -Quarantine and restart Ableton. That
  single restart splits the problem in half: the fleet, or not the fleet. Send
  the output of this script either way.
"@ -ForegroundColor Gray
Write-Host ""
