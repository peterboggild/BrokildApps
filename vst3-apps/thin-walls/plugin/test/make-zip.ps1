# Package Thin Walls for the website.
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

$root  = "$PSScriptRoot\.."
$stage = "$root\dist\stage"
$web   = "C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\thin-walls"
$out   = Join-Path $web "Thin-Walls-VST3-win64.zip"
$build = "$root\build\ThinWalls_artefacts\Release"
$buildId = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'TW_BUILD_ID "([0-9.]+)"').Matches[0].Groups[1].Value

Write-Output "`nThin Walls, build $buildId`n"

New-Item -ItemType Directory -Force $web | Out-Null
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$inner = Join-Path $stage "Thin-Walls-VST3-win64"
New-Item -ItemType Directory -Force $inner | Out-Null

$bundle = Join-Path $inner "Thin Walls.vst3\Contents\x86_64-win"
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item "$build\VST3\Thin Walls.vst3\Contents\x86_64-win\Thin Walls.vst3" $bundle -Force
Write-Output "  bundle copied"

# Smart App Control blocks the freshly built vst3_helper that writes this, so
# a local build often leaves a zero-byte file. An empty one is worse than none.
$mi = "$build\VST3\Thin Walls.vst3\Contents\Resources\moduleinfo.json"
if ((Test-Path $mi) -and ((Get-Item $mi).Length -gt 0)) {
    $res = Join-Path $inner "Thin Walls.vst3\Contents\Resources"
    New-Item -ItemType Directory -Force $res | Out-Null
    Copy-Item $mi $res -Force
    Write-Output "  moduleinfo.json included"
} else { Write-Output "  moduleinfo.json skipped (absent or zero-byte)" }

$exe = Get-ChildItem "$build\Standalone" -Filter "*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($exe) { Copy-Item $exe.FullName (Join-Path $inner "Thin Walls.exe") -Force
            Write-Output ("  standalone: " + [int]($exe.Length/1MB) + " MB") }
else { Write-Output "  NO STANDALONE"; exit 1 }

$manual = "$root\docs\manual\Thin-Walls-Manual.pdf"
if (Test-Path $manual) {
    Copy-Item $manual (Join-Path $inner "Thin-Walls-Manual.pdf") -Force
    Copy-Item $manual (Join-Path $web "Thin-Walls-Manual.pdf") -Force
    Write-Output "  manual included and published"
} else { Write-Output "  NO MANUAL at $manual"; exit 1 }

$readme = @"
THIN WALLS  -  a Brokild effect  -  build $buildId

An apartment of three rooms and three doors. Place up to four sound sources and
a binaural listener anywhere, walk around, open and shut the doors. Image-source
reflections, doorway diffraction, door and wall transmission, coupled room
fields calibrated to the physics, and a measured head (MIT KEMAR).

Install: copy "Thin Walls.vst3" (the whole folder) to
  C:\Program Files\Common Files\VST3\
The standalone (Thin Walls.exe) needs no install: it can load and loop a WAV
file as the source, for auditioning without a DAW.

Manual: Thin-Walls-Manual.pdf.  Site: https://peterboggild.github.io/BrokildApps/
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
$dll = Join-Path $probe "Thin-Walls-VST3-win64\Thin Walls.vst3\Contents\x86_64-win\Thin Walls.vst3"

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
$h2 = (Get-FileHash "$build\VST3\Thin Walls.vst3\Contents\x86_64-win\Thin Walls.vst3" -Algorithm SHA256).Hash
if ($h1 -ne $h2) { Write-Output "  HASH MISMATCH between the archive and the build"; exit 1 }
Write-Output "  archive byte-identical to the build"

# the build id must be in the bytes that ship, not merely in CMakeLists
$bytes = [System.IO.File]::ReadAllBytes($dll)
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
if ($ascii.Contains($buildId)) { Write-Output "  build $buildId is in the shipped bytes" }
else { Write-Output "  BUILD ID $buildId NOT FOUND IN THE SHIPPED DLL"; exit 1 }

Remove-Item $probe -Recurse -Force
Write-Output ""
