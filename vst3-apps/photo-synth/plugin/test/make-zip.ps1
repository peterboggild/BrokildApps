# Rebuild the Photo Synth release zip with the renamed product.
#
# The zip is FLAT for this plugin (the bundle folder, the manual, the
# standalone and the README all at the top level) - the structure differs per
# Brokild plugin, so it is copied from the shipped one rather than invented.
#
# The README is carried over from the old zip with the name swept, so nothing
# it says about the plugin is lost.

$stage = "C:\Users\peter\b\PhotoSynth\dist\stage"
$out   = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\photo-synth-2\Photo-Synth-VST3-win64.zip"
$old   = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\photo-synth-2\Photo-Synth2-VST3-win64.zip"
$build = "C:\Users\peter\b\PhotoSynth\build\PhotoSynth_artefacts\Release"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

# 1. the VST3 bundle, at its new name
$bundle = Join-Path $stage "Photo Synth.vst3\Contents\x86_64-win"
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item "$build\VST3\Photo Synth.vst3\Contents\x86_64-win\Photo Synth.vst3" $bundle -Force

# moduleinfo: only if the local build produced a real one (SAC can block the
# helper and leave a 0-byte file, which is worse than none)
$mi = "$build\VST3\Photo Synth.vst3\Contents\Resources\moduleinfo.json"
if ((Test-Path $mi) -and ((Get-Item $mi).Length -gt 0)) {
    $res = Join-Path $stage "Photo Synth.vst3\Contents\Resources"
    New-Item -ItemType Directory -Force $res | Out-Null
    Copy-Item $mi $res -Force
    Write-Output "  moduleinfo.json included"
} else { Write-Output "  moduleinfo.json skipped (absent or zero-byte)" }

# 2. the standalone
$exe = Get-ChildItem "$build\Standalone" -Filter "*.exe" -ErrorAction SilentlyContinue |
       Where-Object { $_.Name -notlike "*probe*" -and $_.Name -notlike "*ABp*" } | Select-Object -First 1
if ($exe) { Copy-Item $exe.FullName (Join-Path $stage "Photo Synth.exe") -Force; Write-Output ("  standalone: " + $exe.Name) }
else { Write-Output "  standalone NOT FOUND" }

# 3. the manual
Copy-Item "C:\Users\peter\b\PhotoSynth\docs\manual\Photo-Synth-Manual.pdf" (Join-Path $stage "Photo Synth Manual.pdf") -Force

# 4. the README, carried over from the shipped zip with the name swept
$tmp = Join-Path $env:TEMP ("psreadme" + (Get-Random))
New-Item -ItemType Directory -Force $tmp | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory($old, $tmp)
$rd = Join-Path $tmp "README.txt"
if (Test-Path $rd) {
    $t = [IO.File]::ReadAllText($rd, [Text.Encoding]::UTF8)
    $t = $t.Replace("Photo-Synth2-Manual.pdf", "`u{1}M`u{1}").Replace("Photo Synth 2 Manual.pdf", "`u{1}M`u{1}")
    $t = $t.Replace("Photo-Synth 2", "Photo Synth").Replace("Photo Synth 2", "Photo Synth")
    $t = $t.Replace("Photo-Synth2", "Photo Synth").Replace("Photo-Synth", "Photo Synth")
    $t = $t.Replace("`u{1}M`u{1}", "Photo Synth Manual.pdf")
    [IO.File]::WriteAllText((Join-Path $stage "README.txt"), $t, (New-Object Text.UTF8Encoding($false)))
    Write-Output "  README carried over and swept"
}
Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue

if (Test-Path $out) { Remove-Item $out -Force }
[System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $out)
Write-Output ("  wrote " + $out + "  (" + [math]::Round((Get-Item $out).Length / 1MB, 1) + " MB)")
