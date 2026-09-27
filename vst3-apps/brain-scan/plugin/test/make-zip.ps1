# Package Brain Scan for the website.
#
# Same structure as High Tide's zip: an inner folder with the .vst3 bundle, the
# standalone, the manual and a README. Cut AFTER a clean relink: a working
# standalone accumulates Smart App Control hash-nudge bytes and those must not
# ship. The script loads the DLL OUT of the archive it just wrote and refuses
# to publish if it does not load, then compares hashes with the build.

$root  = "$PSScriptRoot\.."
$stage = "$root\dist\stage"
$web   = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\brain-scan"
$out   = Join-Path $web "Brain-Scan-VST3-win64.zip"
$build = "$root\build\BrainScan_artefacts\Release"
$buildId = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'BS_BUILD_ID "([0-9.]+)"').Matches[0].Groups[1].Value

New-Item -ItemType Directory -Force $web | Out-Null
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$inner = Join-Path $stage "Brain Scan"
New-Item -ItemType Directory -Force $inner | Out-Null

$bundle = Join-Path $inner "Brain Scan.vst3\Contents\x86_64-win"
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item "$build\VST3\Brain Scan.vst3\Contents\x86_64-win\Brain Scan.vst3" $bundle -Force

$mi = "$build\VST3\Brain Scan.vst3\Contents\Resources\moduleinfo.json"
if ((Test-Path $mi) -and ((Get-Item $mi).Length -gt 0)) {
    $res = Join-Path $inner "Brain Scan.vst3\Contents\Resources"
    New-Item -ItemType Directory -Force $res | Out-Null
    Copy-Item $mi $res -Force
    Write-Output "  moduleinfo.json included"
} else { Write-Output "  moduleinfo.json skipped (absent or zero-byte)" }

$exe = Get-ChildItem "$build\Standalone" -Filter "*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($exe) { Copy-Item $exe.FullName (Join-Path $inner "Brain Scan.exe") -Force; Write-Output ("  standalone: " + $exe.Name) }

$manual = Join-Path $web "Brain-Scan-Manual.pdf"
if (Test-Path $manual) { Copy-Item $manual (Join-Path $inner "Brain Scan Manual.pdf") -Force; Write-Output "  manual included" }
else { Write-Output "  NO MANUAL at $manual"; exit 1 }

$readme = @"
BRAIN SCAN  -  a Brokild synth  -  build $buildId

A synth that stores no waveforms. A specimen is a VOLUME - a scalar field
filling a cube, exactly like the numbers a scanner records - and a scan line
is a curve through it. One cycle of the sound is the field read along that
curve. The filter's cutoff and the modulator are the same thing read slower:
two more lines. And SCAN blends the GEOMETRY of two anchor lines before the
field is read, so halfway between two sounds is tissue neither of them has
visited. A wavetable crossfades two answers; this moves the question.

Nine specimens: seven fields built to give ordinary, useful waveforms, a
hollow shell, and a simulated brain with skull, cortical folds, a fissure and
ventricles. Lines are drawn on a tomography slice - drag a point and it keeps
its depth, click the tissue to add one - and watched in a ray-marched gantry
where they glow inside the specimen until the tissue closes over them.

WHAT IS IN THIS ARCHIVE

  Brain Scan.vst3          the plug-in, as a Windows VST3 bundle folder
  Brain Scan.exe           the same instrument, standing alone
  Brain Scan Manual.pdf    the manual

INSTALLING

  Copy the folder  Brain Scan.vst3  into
      C:\Program Files\Common Files\VST3\
  and rescan in your host. The standalone needs nothing.

  The panel is a web view and needs the Microsoft Edge WebView2 runtime,
  which is present on every up-to-date Windows 10 and 11.

PATCHES

  Your own patches live in  Documents\Brokild patches\Brain Scan\  as JSON
  files carrying every control, both anchors of all three scanners, the
  specimen and the world rack.

  Brokild  -  https://peterboggild.github.io/BrokildApps/
"@
Set-Content -Path (Join-Path $inner "README.txt") -Value $readme -Encoding ASCII

if (Test-Path $out) { Remove-Item $out -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $out -CompressionLevel Optimal
$size = [math]::Round((Get-Item $out).Length / 1MB, 1)
Write-Output "  zip: $out ($size MB)"

# verify: load the DLL out of the archive we just wrote
$probe = "$root\dist\zipprobe"
if (Test-Path $probe) { Remove-Item $probe -Recurse -Force }
Expand-Archive -Path $out -DestinationPath $probe -Force
$dll = Join-Path $probe "Brain Scan\Brain Scan.vst3\Contents\x86_64-win\Brain Scan.vst3"
$sig = '[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryW(string p); [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr h);'
$k = Add-Type -MemberDefinition $sig -Name LLZ -Namespace BSZ -PassThru
$h = $k::LoadLibraryW($dll)
if ($h -ne [IntPtr]::Zero) { $k::FreeLibrary($h) | Out-Null; Write-Output "  the DLL in the archive loads" }
else {
    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    if ($err -eq 4551) { Write-Output "  the DLL in the archive is SAC-blocked at this path (a fresh extraction usually is) - hash compared instead" }
    else { Write-Output "  THE DLL IN THE ARCHIVE DOES NOT LOAD (err $err)"; exit 1 }
}
$h1 = (Get-FileHash $dll -Algorithm SHA256).Hash
$h2 = (Get-FileHash "$build\VST3\Brain Scan.vst3\Contents\x86_64-win\Brain Scan.vst3" -Algorithm SHA256).Hash
if ($h1 -ne $h2) { Write-Output "  HASH MISMATCH between the archive and the build"; exit 1 }
Write-Output "  archive byte-identical to the build"
Remove-Item $probe -Recurse -Force
Write-Output "  build $buildId published to $web"
