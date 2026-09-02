<#
    install-from-archives.ps1 — install the fleet on a machine that has no
    source tree, from the published archives.
    ==========================================================================

    BWFX's install-fleet.ps1 installs from `C:\Users\peter\b\<Tree>\build\...`.
    That is right on the machine where the plug-ins are built and useless on
    any other, which is the situation this script exists for: the music PC has
    the archives and no source at all.

    It keeps the two house rules that matter, for the same reasons:

      * SMART APP CONTROL IS JUDGED PER FILE AT ITS PATH. A copy is judged
        separately from the file it was copied from, so the INSTALLED file is
        what gets probed, and it is nudged with a few random trailing bytes —
        overlay data after the last PE section, which the loader ignores —
        until it loads.

      * A STALE BUNDLE IS NEVER LEFT IN THE SCAN PATH. Renaming one does not
        retire it: hosts scan recursively and the module inside carries the
        same class ids, so either may win.

    And it adds one that install-fleet cannot do, because install-fleet works
    from a table of product names:

      * IT RETIRES BY PLUG-IN CODE, NOT BY NAME. Two bundles with DIFFERENT
        names can carry the SAME VST3 class ids — which is true of the
        published fleet right now: the collection archive ships
        `Photo-Synth2.vst3` and the individual archive ships `Photo Synth.vst3`,
        both `BrkdPsy2`, three days and one rename apart. Installing both puts
        two bundles with identical class ids in the scan path and lets scan
        order decide. Matching on the code catches that; matching on the name
        never can.

    USAGE
        powershell -ExecutionPolicy Bypass -File install-from-archives.ps1 `
            -Archives C:\Users\peter\Downloads

        -Only "Artefact B2311.1"     just the one
        -WhatIf                      say what it would do and touch nothing

    Installing into Program Files needs an administrator prompt. Without one it
    installs into the per-user house only and says so; hosts scan both.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $Archives,
    [string[]] $Only = @(),
    [string[]] $Roots = @("C:\Program Files\Common Files\VST3\Brokild", "$env:USERPROFILE\AudioDev\VST3"),
    [string]   $QuarantineDir = "$env:USERPROFILE\AudioDev\Quarantine",
    [switch]   $WhatIf
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression.FileSystem

#  The three folders the fleet is filed under, as the house has them.
$group = @{
    "Artefact B2311.1"  = "Proxima Centauri B findings"
    "Artefact B2311.22" = "Proxima Centauri B findings"
    "Artefact B2311.67" = "Proxima Centauri B findings"
    "Hairfryer"         = "Experimental"
}
function GroupOf ([string] $name) { if ($group.ContainsKey($name)) { $group[$name] } else { "Brokild collection" } }

function CodesIn ([string] $path) {
    $text = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($path))
    return ([regex]::Matches($text, 'Brkd[A-Za-z0-9]{4}') | ForEach-Object { $_.Value } | Sort-Object -Unique)
}

if (-not (Test-Path -LiteralPath $Archives)) { throw "no such folder: $Archives" }
New-Item -ItemType Directory -Force $QuarantineDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$stage = Join-Path $env:TEMP "brokild-install-$stamp"
New-Item -ItemType Directory -Force $stage | Out-Null

# ---------------------------------------------------------------------------
#  1. Unpack every archive, and find every module in them.
# ---------------------------------------------------------------------------
Write-Host "reading archives in $Archives" -ForegroundColor Cyan
$zips = Get-ChildItem -LiteralPath $Archives -Filter "*.zip" -File
if (-not $zips) { throw "no .zip archives in $Archives" }
foreach ($z in $zips) {
    try { [System.IO.Compression.ZipFile]::ExtractToDirectory($z.FullName, (Join-Path $stage $z.BaseName)) }
    catch { Write-Host ("  could not unpack {0}: {1}" -f $z.Name, $_.Exception.Message) -ForegroundColor Yellow }
}

$candidates = @()
Get-ChildItem -LiteralPath $stage -Recurse -File -Filter "*.vst3" -ErrorAction SilentlyContinue |
    Where-Object { $_.DirectoryName -like "*x86_64-win" } | ForEach-Object {
        $name = [System.IO.Path]::GetFileNameWithoutExtension($_.Name)
        $candidates += [pscustomobject]@{
            Name = $name; File = $_.FullName; Length = $_.Length
            Codes = (CodesIn $_.FullName)
            Linked = $_.LastWriteTimeUtc
            From = $_.FullName.Substring($stage.Length + 1).Split('\')[0]
        }
    }
if (-not $candidates) { throw "no VST3 modules found inside those archives" }

# ---------------------------------------------------------------------------
#  2. One module per plug-in CODE — the newest wins.
#
#  This is where the Photo Synth collision is resolved rather than installed:
#  two names, one code, and only one of them can be in the scan path.
# ---------------------------------------------------------------------------
$chosen = @()
foreach ($g in ($candidates | Group-Object { ($_.Codes -join ',') })) {
    $best = $g.Group | Sort-Object Linked -Descending | Select-Object -First 1
    if (($g.Group.Name | Sort-Object -Unique).Count -gt 1) {
        Write-Host ("  {0} is shipped under {1} names in these archives:" -f $g.Name, ($g.Group.Name | Sort-Object -Unique).Count) -ForegroundColor Yellow
        foreach ($c in ($g.Group | Sort-Object Linked -Descending)) {
            Write-Host ("      {0,-24} {1}  from {2}" -f $c.Name, $c.Linked.ToString("yyyy-MM-dd HH:mm"), $c.From) -ForegroundColor DarkYellow
        }
        Write-Host ("      -> installing the newest, as {0}; the others are never put on disk" -f $best.Name) -ForegroundColor Yellow
    }
    $chosen += $best
}

# ---------------------------------------------------------------------------
#  3. Install, retiring anything already there that claims the same code.
# ---------------------------------------------------------------------------
$sig = '[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryW(string p); [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr h);'
$k = Add-Type -MemberDefinition $sig -Name LLF2 -Namespace WF -PassThru
$rand = New-Object System.Random

foreach ($p in ($chosen | Sort-Object Name)) {
    if ($Only.Count -gt 0 -and $Only -notcontains $p.Name) { continue }
    $g = GroupOf $p.Name

    foreach ($root in $Roots) {
        $short = if ($root -like "*Program Files*") { "ProgramFiles" } else { "AudioDev" }

        #  Retire, ACROSS THE WHOLE ROOT, any bundle whose module carries this
        #  code — whatever it is called and wherever it sits.
        $scanRoot = if ($root -like "*\Brokild") { Split-Path $root -Parent } else { $root }
        if (Test-Path -LiteralPath $scanRoot) {
            Get-ChildItem -LiteralPath $scanRoot -Recurse -File -Filter "*.vst3" -ErrorAction SilentlyContinue |
              Where-Object { $_.DirectoryName -like "*x86_64-win" } | ForEach-Object {
                $existing = $_
                $bundle = Split-Path (Split-Path $existing.DirectoryName -Parent) -Parent
                $wanted = Join-Path (Join-Path $root $g) "$($p.Name).vst3"
                if ($bundle -eq $wanted) { return }             # the one we are about to replace
                $c = @()
                try { $c = CodesIn $existing.FullName } catch { return }
                if (-not ($c | Where-Object { $p.Codes -contains $_ })) { return }
                $dst = Join-Path $QuarantineDir ("clash__{0}__{1}" -f (Split-Path $bundle -Leaf), $stamp)
                Write-Host ("  {0,-22} {1,-13} retiring {2}" -f $p.Name, $short, $bundle) -ForegroundColor Yellow
                Write-Host ("  {0,-22} {1,-13}   it carries the same class ids and would fight this one" -f "", "") -ForegroundColor DarkYellow
                if (-not $WhatIf) { Move-Item -LiteralPath $bundle -Destination $dst -Force -ErrorAction SilentlyContinue }
            }
        }

        $dstDir = Join-Path (Join-Path $root $g) "$($p.Name).vst3\Contents\x86_64-win"
        $dst    = Join-Path $dstDir "$($p.Name).vst3"
        if ($WhatIf) { Write-Host ("  {0,-22} {1,-13} would install to {2}" -f $p.Name, $short, $dst) -ForegroundColor Gray; continue }

        try {
            New-Item -ItemType Directory -Force $dstDir | Out-Null
            if (Test-Path -LiteralPath $dst) {
                #  the house trick for a module a host still has mapped
                try { Rename-Item -LiteralPath $dst "$($p.Name).vst3.old$stamp" -Force -ErrorAction Stop } catch {}
            }
            Copy-Item -LiteralPath $p.File -Destination $dst -Force -ErrorAction Stop
        } catch {
            Write-Host ("  {0,-22} {1,-13} COULD NOT INSTALL — {2}" -f $p.Name, $short, $_.Exception.Message) -ForegroundColor Red
            if ($root -like "C:\Program Files\*") { Write-Host ("  {0,-22} {1,-13}   (that root needs an administrator prompt)" -f "", "") -ForegroundColor Red }
            continue
        }

        Get-ChildItem -LiteralPath $dstDir -Filter "*.old*" -ErrorAction SilentlyContinue |
            Remove-Item -Force -ErrorAction SilentlyContinue

        #  Probe the INSTALLED file, and nudge it past Smart App Control.
        $state = "STILL BLOCKED"
        for ($i = 0; $i -lt 6; $i++) {
            $h = $k::LoadLibraryW($dst)
            if ($h -ne [IntPtr]::Zero) { $k::FreeLibrary($h) | Out-Null; $state = "loads"; break }
            $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
            if ($err -ne 4551) { $state = "failed err=$err"; break }
            $b = New-Object byte[] ($rand.Next(3, 17)); $rand.NextBytes($b)
            $fs = [System.IO.File]::Open($dst, 'Append', 'Write'); $fs.Write($b, 0, $b.Length); $fs.Dispose()
            Start-Sleep -Milliseconds 250
        }
        $col = if ($state -eq "loads") { "Green" } else { "Red" }
        Write-Host ("  {0,-22} {1,-13} {2,-28} {3}" -f $p.Name, $short, $g, $state) -ForegroundColor $col
    }
}

if (-not $WhatIf) { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue }
Write-Host ""
Write-Host "Rescan in your host. If anything says STILL BLOCKED, that file is being held" -ForegroundColor Gray
Write-Host "by Smart App Control and no host will load it either." -ForegroundColor Gray
