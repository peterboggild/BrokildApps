param([string] $BuildDir = "")   # the house rule builds OUTSIDE Dropbox; point this there
# Package Thirty Thousand Years for the website.
#
# An inner folder with the .vst3 bundle, the standalone, the manual and a README.
#
# CUT AFTER A CLEAN RELINK. tools/live.ps1 appends Smart App Control hash-nudge
# bytes to the standalone IN PLACE, and those must not ship. (The same goes for
# the VST3 if it was nudged for a host test: relink both first.)
#
# The script then loads the DLL OUT of the archive it just wrote and compares
# its hash with the build, because "the zip exists" and "the zip contains a
# working plug-in" are different claims.
#
# ASCII only - Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI.

$root  = "$PSScriptRoot\.."
$stage = "$root\dist\stage"
$web   = "C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\thirty-thousand-years"
$out   = Join-Path $web "Thirty-Thousand-Years-VST3-win64.zip"
$build = if ($BuildDir) { $BuildDir } else { "$root\build\ThirtyThousandYears_artefacts\Release" }
$name  = "Thirty Thousand Years"
$buildId = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'TTY_BUILD_ID "([0-9.]+)"').Matches[0].Groups[1].Value

Write-Output "`n$name, build $buildId`n"

New-Item -ItemType Directory -Force $web | Out-Null
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$inner = Join-Path $stage "Thirty-Thousand-Years-VST3-win64"
New-Item -ItemType Directory -Force $inner | Out-Null

$bundle = Join-Path $inner "$name.vst3\Contents\x86_64-win"
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item "$build\VST3\$name.vst3\Contents\x86_64-win\$name.vst3" $bundle -Force
Write-Output "  bundle copied"

$mi = "$build\VST3\$name.vst3\Contents\Resources\moduleinfo.json"
if ((Test-Path $mi) -and ((Get-Item $mi).Length -gt 0)) {
    $res = Join-Path $inner "$name.vst3\Contents\Resources"
    New-Item -ItemType Directory -Force $res | Out-Null
    Copy-Item $mi $res -Force
    Write-Output "  moduleinfo.json included"
} else { Write-Output "  moduleinfo.json skipped (absent or zero-byte)" }

$exe = Get-ChildItem "$build\Standalone" -Filter "*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($exe) { Copy-Item $exe.FullName (Join-Path $inner "$name.exe") -Force
            Write-Output ("  standalone: " + [int]($exe.Length/1MB) + " MB") }
else { Write-Output "  NO STANDALONE"; exit 1 }

#  The manual is PUBLISHED beside the page and no longer lives in the plug-in
#  tree: stage 1 deliberately left PDFs out of the source migration.
$manual = "$root\docs\manual\Thirty-Thousand-Years-Manual.pdf"
if (-not (Test-Path $manual)) { $manual = Join-Path $web "Thirty-Thousand-Years-Manual.pdf" }
if (Test-Path $manual) {
    Copy-Item $manual (Join-Path $inner "Thirty-Thousand-Years-Manual.pdf") -Force
    $pub = Join-Path $web "Thirty-Thousand-Years-Manual.pdf"
    if ((Resolve-Path $manual).Path -ne (Resolve-Path $pub -ErrorAction SilentlyContinue).Path) { Copy-Item $manual $pub -Force }
    Write-Output "  manual included and published"
} else { Write-Output "  NO MANUAL at $manual"; exit 1 }

$readme = @"
Thirty Thousand Years  -  a Brokild instrument  -  build $buildId

A drone synthesizer and evolving soundscape instrument. Four strata that
excite each other (MASS, SIGNAL, MEMORY, STRUCTURE), an environment whose
feedback loop is a generator, a LIFE network that reads the engine's own
audio, HISTORY across four scenes, eight macros, forty-eight presets, and the
Brokild World FX rack.

Install: copy "$name.vst3" (the whole folder) to
  C:\Program Files\Common Files\VST3\
The standalone ($name.exe) needs no install: plug in a MIDI keyboard, or
switch DRONE on, or play the on-screen keys. A fresh instance is silent until
you do.

Patches live in Documents\Brokild patches\$name.
Manual: Thirty-Thousand-Years-Manual.pdf.  Site: https://peterboggild.github.io/BrokildApps/
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
$dll = Join-Path $probe "Thirty-Thousand-Years-VST3-win64\$name.vst3\Contents\x86_64-win\$name.vst3"

$sig = '[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryW(string p); [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr h);'
$k = Add-Type -MemberDefinition $sig -Name LLZ -Namespace TTYZ -PassThru
$h = $k::LoadLibraryW($dll)
if ($h -ne [IntPtr]::Zero) { $k::FreeLibrary($h) | Out-Null; Write-Output "  the DLL in the archive loads" }
else {
    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    if ($err -eq 4551) { Write-Output "  the extracted DLL is SAC-blocked at this path (usual) - hash compared instead" }
    else { Write-Output "  THE DLL IN THE ARCHIVE DOES NOT LOAD (err $err)"; exit 1 }
}

$h1 = (Get-FileHash $dll -Algorithm SHA256).Hash
$h2 = (Get-FileHash "$build\VST3\$name.vst3\Contents\x86_64-win\$name.vst3" -Algorithm SHA256).Hash
if ($h1 -ne $h2) { Write-Output "  HASH MISMATCH between the archive and the build"; exit 1 }
Write-Output "  archive byte-identical to the build"

$bytes = [System.IO.File]::ReadAllBytes($dll)
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
if ($ascii.Contains($buildId)) { Write-Output "  build $buildId is in the shipped bytes" }
else { Write-Output "  BUILD ID $buildId NOT FOUND IN THE SHIPPED DLL"; exit 1 }

Remove-Item $probe -Recurse -Force
Write-Output ""
