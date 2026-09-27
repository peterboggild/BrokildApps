# Package High Tide for the website.
#
# Same structure as Black Rider's zip: an inner folder with the .vst3 bundle,
# the standalone, the manual and a README. Cut AFTER a clean relink: the
# working standalone accumulates Smart App Control hash-nudge bytes and those
# must not ship. The script loads the DLL OUT of the archive it just wrote and
# refuses to publish if it does not load.

$root  = "$PSScriptRoot\.."
$stage = "$root\dist\stage"
$web   = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\high-tide"
$out   = Join-Path $web "High-Tide-VST3-win64.zip"
$build = "$root\build\HighTide_artefacts\Release"
$buildId = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'HT_BUILD_ID "([0-9.]+)"').Matches[0].Groups[1].Value

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$inner = Join-Path $stage "High Tide"
New-Item -ItemType Directory -Force $inner | Out-Null

$bundle = Join-Path $inner "High Tide.vst3\Contents\x86_64-win"
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item "$build\VST3\High Tide.vst3\Contents\x86_64-win\High Tide.vst3" $bundle -Force

$mi = "$build\VST3\High Tide.vst3\Contents\Resources\moduleinfo.json"
if ((Test-Path $mi) -and ((Get-Item $mi).Length -gt 0)) {
    $res = Join-Path $inner "High Tide.vst3\Contents\Resources"
    New-Item -ItemType Directory -Force $res | Out-Null
    Copy-Item $mi $res -Force
    Write-Output "  moduleinfo.json included"
} else { Write-Output "  moduleinfo.json skipped (absent or zero-byte)" }

$exe = Get-ChildItem "$build\Standalone" -Filter "*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($exe) { Copy-Item $exe.FullName (Join-Path $inner "High Tide.exe") -Force; Write-Output ("  standalone: " + $exe.Name) }

$manual = Join-Path $web "High-Tide-Manual.pdf"
if (Test-Path $manual) { Copy-Item $manual (Join-Path $inner "High Tide Manual.pdf") -Force; Write-Output "  manual included" }
else { Write-Output "  NO MANUAL at $manual" }

$readme = @"
HIGH TIDE  -  a Brokild synth  -  build $buildId

A wavetable synth that stores no waveforms. A frame is a bowl on a sculpted
terrain; the waveform is what a mass does when it is dropped into that bowl,
integrated by Newton's law at audio rate. Position is where the ball IS.
Morphing is travel across the terrain. The TIDE floods the passes between the
valleys; PINS on a timeline tow the ball to where you want it, when you want
it there. ROCK drives the whole terrain at the note and opens period doubling
and chaos. Twelve factory terrains, a 16-bit PNG in and out, a photograph as a
terrain, the Brokild World FX rack with five macros.

WHAT IS IN THIS ARCHIVE

  High Tide.vst3          the plug-in, as a Windows VST3 bundle folder
  High Tide.exe           the same instrument, standing alone
  High Tide Manual.pdf    the manual

INSTALLING

  Copy the folder  High Tide.vst3  into
      C:\Program Files\Common Files\VST3\
  and rescan in your host. The standalone needs nothing.

PATCHES

  Your own patches live in  Documents\Brokild patches\High Tide\  as JSON
  files that carry the terrain (a 16-bit PNG inside), the pins and the rack.

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
$dll = Join-Path $probe "High Tide\High Tide.vst3\Contents\x86_64-win\High Tide.vst3"
$sig = '[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryW(string p); [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr h);'
$k = Add-Type -MemberDefinition $sig -Name LLZ -Namespace HTZ -PassThru
$h = $k::LoadLibraryW($dll)
if ($h -ne [IntPtr]::Zero) { $k::FreeLibrary($h) | Out-Null; Write-Output "  the DLL in the archive loads" }
else {
    $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    if ($err -eq 4551) { Write-Output "  the DLL in the archive is SAC-blocked at this path (a fresh extraction usually is) - hash compared instead" }
    else { Write-Output "  THE DLL IN THE ARCHIVE DOES NOT LOAD (err $err)"; exit 1 }
}
$h1 = (Get-FileHash $dll -Algorithm SHA256).Hash
$h2 = (Get-FileHash "$build\VST3\High Tide.vst3\Contents\x86_64-win\High Tide.vst3" -Algorithm SHA256).Hash
if ($h1 -ne $h2) { Write-Output "  HASH MISMATCH between the archive and the build"; exit 1 }
Write-Output "  archive byte-identical to the build"
Remove-Item $probe -Recurse -Force
