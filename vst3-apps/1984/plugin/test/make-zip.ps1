# Package 1984 for the website.
#
# An inner folder with the .vst3 bundle, the standalone, the manual and a README.
#
# CUT AFTER A CLEAN RELINK. tools/live.ps1 appends Smart App Control hash-nudge
# bytes to the standalone IN PLACE, and those must not ship.
#
# The script then loads the DLL OUT of the archive it just wrote and compares
# its hash with the build, because "the zip exists" and "the zip contains a
# working plug-in" are different claims.
#
# ASCII only - Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI.

$root  = "C:\Users\peter\b\Nineteen84"
$stage = "$root\dist\stage"
$web   = "C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\1984"
$out   = Join-Path $web "1984-VST3-win64.zip"
$build = "$root\build\Nineteen84_artefacts\Release"
$buildId = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'N84_BUILD_ID "([0-9.]+)"').Matches[0].Groups[1].Value

Write-Output "`n1984, build $buildId`n"

New-Item -ItemType Directory -Force $web | Out-Null
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$inner = Join-Path $stage "1984-VST3-win64"
New-Item -ItemType Directory -Force $inner | Out-Null

$bundle = Join-Path $inner "1984.vst3\Contents\x86_64-win"
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item "$build\VST3\1984.vst3\Contents\x86_64-win\1984.vst3" $bundle -Force
Write-Output "  bundle copied"

$mi = "$build\VST3\1984.vst3\Contents\Resources\moduleinfo.json"
if ((Test-Path $mi) -and ((Get-Item $mi).Length -gt 0)) {
    $res = Join-Path $inner "1984.vst3\Contents\Resources"
    New-Item -ItemType Directory -Force $res | Out-Null
    Copy-Item $mi $res -Force
    Write-Output "  moduleinfo.json included"
} else { Write-Output "  moduleinfo.json skipped (absent or zero-byte)" }

$exe = Get-ChildItem "$build\Standalone" -Filter "*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($exe) { Copy-Item $exe.FullName (Join-Path $inner "1984.exe") -Force
            Write-Output ("  standalone: " + [int]($exe.Length/1MB) + " MB") }
else { Write-Output "  NO STANDALONE"; exit 1 }

$manual = "$root\docs\manual\1984-Manual.pdf"
if (Test-Path $manual) {
    Copy-Item $manual (Join-Path $inner "1984-Manual.pdf") -Force
    Copy-Item $manual (Join-Path $web "1984-Manual.pdf") -Force
    Write-Output "  manual included and published"
} else { Write-Output "  NO MANUAL at $manual"; exit 1 }

$readme = @"
1984  -  a Brokild polysynth  -  build $buildId

Eight voices, each of two complete ranks (the CS-80 layout): oscillator,
resonant high-pass, resonant low-pass (12 dB state-variable or a 24 dB ladder),
the initial-level / attack-level filter envelope, an amplifier envelope, level
and pan. Ring modulator, sub-oscillator, touch. Hard sync and poly-mod between
the ranks. Then drive, a three-phase ensemble, a formant choir, a tape with
wow, flutter, saturation, age, dropouts and a VHS mode, and a hall. Forty
factory patches. Brokild World FX rack with five macros.

Install: copy "1984.vst3" (the whole folder) to
  C:\Program Files\Common Files\VST3\
The standalone (1984.exe) needs no install: plug in a MIDI keyboard or play the
on-screen one.

Manual: 1984-Manual.pdf.  Site: https://peterboggild.github.io/BrokildApps/
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
$dll = Join-Path $probe "1984-VST3-win64\1984.vst3\Contents\x86_64-win\1984.vst3"

$sig = '[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryW(string p); [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr h);'
$k = Add-Type -MemberDefinition $sig -Name LLZ -Namespace N84Z -PassThru
$h = $k::LoadLibraryW($dll)
if ($h -ne [IntPtr]::Zero) { $k::FreeLibrary($h) | Out-Null; Write-Output "  the DLL in the archive loads" }
else {
    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    if ($err -eq 4551) { Write-Output "  the extracted DLL is SAC-blocked at this path (usual) - hash compared instead" }
    else { Write-Output "  THE DLL IN THE ARCHIVE DOES NOT LOAD (err $err)"; exit 1 }
}

$h1 = (Get-FileHash $dll -Algorithm SHA256).Hash
$h2 = (Get-FileHash "$build\VST3\1984.vst3\Contents\x86_64-win\1984.vst3" -Algorithm SHA256).Hash
if ($h1 -ne $h2) { Write-Output "  HASH MISMATCH between the archive and the build"; exit 1 }
Write-Output "  archive byte-identical to the build"

$bytes = [System.IO.File]::ReadAllBytes($dll)
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
if ($ascii.Contains($buildId)) { Write-Output "  build $buildId is in the shipped bytes" }
else { Write-Output "  BUILD ID $buildId NOT FOUND IN THE SHIPPED DLL"; exit 1 }

Remove-Item $probe -Recurse -Force
Write-Output ""
