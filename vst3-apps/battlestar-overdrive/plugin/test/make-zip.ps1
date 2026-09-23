# Package Battlestar Overdrive for the website.
#
# Same shape as Martian Gain's zip - the other Brokild effect - an inner folder
# with the .vst3 bundle, the standalone, the manual and a README.
#
# CUT AFTER A CLEAN RELINK. tools/live.ps1 appends Smart App Control hash-nudge
# bytes to the standalone IN PLACE, and those must not ship.
#
# The script then loads the DLL OUT of the archive it just wrote and compares
# its hash with the build, because "the zip exists" and "the zip contains a
# working plug-in" are different claims.
#
# ASCII only - Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI.

$root  = "C:\Users\peter\b\BattlestarOverdrive"
$stage = "$root\dist\stage"
$web   = "C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\battlestar-overdrive"
$out   = Join-Path $web "Battlestar-Overdrive-VST3-win64.zip"
$build = "$root\build\BattlestarOverdrive_artefacts\Release"
$buildId = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'BO_BUILD_ID "([0-9.]+)"').Matches[0].Groups[1].Value

Write-Output "`nBattlestar Overdrive, build $buildId`n"

New-Item -ItemType Directory -Force $web | Out-Null
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$inner = Join-Path $stage "Battlestar-Overdrive-VST3-win64"
New-Item -ItemType Directory -Force $inner | Out-Null

$bundle = Join-Path $inner "Battlestar Overdrive.vst3\Contents\x86_64-win"
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item "$build\VST3\Battlestar Overdrive.vst3\Contents\x86_64-win\Battlestar Overdrive.vst3" $bundle -Force
Write-Output "  bundle copied"

# Smart App Control blocks the freshly built vst3_helper that writes this, so
# a local build often leaves a zero-byte file. An empty one is worse than none.
$mi = "$build\VST3\Battlestar Overdrive.vst3\Contents\Resources\moduleinfo.json"
if ((Test-Path $mi) -and ((Get-Item $mi).Length -gt 0)) {
    $res = Join-Path $inner "Battlestar Overdrive.vst3\Contents\Resources"
    New-Item -ItemType Directory -Force $res | Out-Null
    Copy-Item $mi $res -Force
    Write-Output "  moduleinfo.json included"
} else { Write-Output "  moduleinfo.json skipped (absent or zero-byte)" }

$exe = Get-ChildItem "$build\Standalone" -Filter "*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($exe) { Copy-Item $exe.FullName (Join-Path $inner "Battlestar Overdrive.exe") -Force
            Write-Output ("  standalone: " + [int]($exe.Length/1MB) + " MB") }
else { Write-Output "  NO STANDALONE"; exit 1 }

$manual = "$root\docs\manual\Battlestar-Overdrive-Manual.pdf"
if (Test-Path $manual) {
    Copy-Item $manual (Join-Path $inner "Battlestar-Overdrive-Manual.pdf") -Force
    Copy-Item $manual (Join-Path $web "Battlestar-Overdrive-Manual.pdf") -Force
    Write-Output "  manual included and published"
} else { Write-Output "  NO MANUAL at $manual"; exit 1 }

$readme = @"
BATTLESTAR OVERDRIVE  -  a Brokild effect  -  build $buildId

I suggested to make a VST3 for Max, and he asked for a Battlestar Overdrive,
named after his solo band. That was too good not to do. So here it is, built
with his blessing.

Eight drive engines on one knob, ordered from polite to unhinged: IDLE BURN,
ION DRIVE, PLASMA COIL, AFTERBURNER, RAZOR WING, WARP FOLD, HYPERDRIVE and
SUPERNOVA. The tube tells you which one you are on, runs a starfield that
speeds up as you climb the knob, goes to warp on the seventh and blows up a
star on the eighth. ANTITHRUST widens the image without ever moving the mono
sum. SPACE flies through no less than five overlapping effects, because too
much is never enough.

Very good at ruining things, beautifully and thoroughly.

And watch the fuel. You don't want to run out. Thrust me.

GO AND LISTEN TO THE BAND

  Battlestar Overdrive is Max Christensen, out of Copenhagen - garage rock
  swagger welded to soul, disco and slacker electronica, performed alone on a
  mashed-up rig of synths, sequencers and samplers with a screen of zany
  visuals behind him. Eclectic, wildly entertaining, and the reason this
  plug-in exists.

WHAT IS IN THIS ARCHIVE

  Battlestar Overdrive.vst3           the plug-in, as a Windows VST3 bundle
  Battlestar Overdrive.exe            the same effect, standing alone
  Battlestar-Overdrive-Manual.pdf     the handbook, 12 pages

INSTALLING

  Copy the folder  Battlestar Overdrive.vst3  into
      C:\Program Files\Common Files\VST3\
  and rescan in your host. The standalone needs nothing installing.

TWO CONTROLS WITH NO KNOB

  The panel has no room for them, so they live in your host's automation list
  only:  SPACE WET  (overall level of the SPACE section; the centre is exactly
  what the SPACE knob does on its own) and  SPACE SYNC  (locks the delay to
  the host clock; FREE by default).

  Brokild  -  https://peterboggild.github.io/BrokildApps/
"@
Set-Content -Path (Join-Path $inner "README.txt") -Value $readme -Encoding ASCII

if (Test-Path $out) { Remove-Item $out -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $out -CompressionLevel Optimal
$size = [math]::Round((Get-Item $out).Length / 1MB, 1)
Write-Output "  zip: $out ($size MB)"

# --- verify, out of the archive ---------------------------------------------
$probe = "$root\dist\zipprobe"
if (Test-Path $probe) { Remove-Item $probe -Recurse -Force }
Expand-Archive -Path $out -DestinationPath $probe -Force
$dll = Join-Path $probe "Battlestar-Overdrive-VST3-win64\Battlestar Overdrive.vst3\Contents\x86_64-win\Battlestar Overdrive.vst3"

$sig = '[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryW(string p); [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr h);'
$k = Add-Type -MemberDefinition $sig -Name LLZ -Namespace BOZ -PassThru
$h = $k::LoadLibraryW($dll)
if ($h -ne [IntPtr]::Zero) { $k::FreeLibrary($h) | Out-Null; Write-Output "  the DLL in the archive loads" }
else {
    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    # A copy is judged separately from the file it came from, so a fresh
    # extraction being refused says nothing about the build. Hash instead.
    if ($err -eq 4551) { Write-Output "  the extracted DLL is SAC-blocked at this path (usual) - hash compared instead" }
    else { Write-Output "  THE DLL IN THE ARCHIVE DOES NOT LOAD (err $err)"; exit 1 }
}

$h1 = (Get-FileHash $dll -Algorithm SHA256).Hash
$h2 = (Get-FileHash "$build\VST3\Battlestar Overdrive.vst3\Contents\x86_64-win\Battlestar Overdrive.vst3" -Algorithm SHA256).Hash
if ($h1 -ne $h2) { Write-Output "  HASH MISMATCH between the archive and the build"; exit 1 }
Write-Output "  archive byte-identical to the build"

# the build id must be in the bytes that ship, not merely in CMakeLists
$bytes = [System.IO.File]::ReadAllBytes($dll)
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
if ($ascii.Contains($buildId)) { Write-Output "  build $buildId is in the shipped bytes" }
else { Write-Output "  BUILD ID $buildId NOT FOUND IN THE SHIPPED DLL"; exit 1 }

Remove-Item $probe -Recurse -Force
Write-Output ""
