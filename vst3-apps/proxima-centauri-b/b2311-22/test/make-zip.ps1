# Package B2311.22 as recovered material.
#
# Flat structure, matching Photo Synth and Escape Room (the zip layout differs
# per plugin in this collection - copied from a shipped one, not invented).
# The findings report travels with the object rather than a manual, because
# there is no manual: nothing about these objects is operated.

$stage = "C:\Users\peter\b\ArtefactB2311\dist\stage"
$web   = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\proxima-centauri-b"
$out   = Join-Path $web "Artefact-B2311-22-win64.zip"
$build = "C:\Users\peter\b\ArtefactB2311\build\ArtefactB2311_artefacts\Release"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

$bundle = Join-Path $stage "Artefact B2311.22.vst3\Contents\x86_64-win"
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item "$build\VST3\Artefact B2311.22.vst3\Contents\x86_64-win\Artefact B2311.22.vst3" $bundle -Force

$mi = "$build\VST3\Artefact B2311.22.vst3\Contents\Resources\moduleinfo.json"
if ((Test-Path $mi) -and ((Get-Item $mi).Length -gt 0)) {
    $res = Join-Path $stage "Artefact B2311.22.vst3\Contents\Resources"
    New-Item -ItemType Directory -Force $res | Out-Null
    Copy-Item $mi $res -Force
    Write-Output "  moduleinfo.json included"
} else { Write-Output "  moduleinfo.json skipped (absent or zero-byte)" }

$exe = Get-ChildItem "$build\Standalone" -Filter "*.exe" -ErrorAction SilentlyContinue |
       Where-Object { $_.Name -notlike "*ABp*" -and $_.Name -notlike "*probe*" } | Select-Object -First 1
if ($exe) { Copy-Item $exe.FullName (Join-Path $stage "Artefact B2311.22.exe") -Force
            Write-Output ("  standalone: " + $exe.Name) }

Copy-Item (Join-Path $web "Proxima-Centauri-b-Findings.pdf") (Join-Path $stage "Proxima Centauri b - Field Findings.pdf") -Force

$readme = @"
PROXIMA CENTAURI b - ARTEFACT B2311.22
Field record B2311. Recovered day 81, dry cistern, Kell Rille.

WHAT IS IN THIS ARCHIVE

  Artefact B2311.22.vst3   the frame, as a Windows VST3
  Artefact B2311.22.exe    the same frame, standing alone
  Proxima Centauri b -
    Field Findings.pdf     what was established about both recovered objects,
                           and the questions the survey could not close

INSTALLING

  Copy the .vst3 folder into
      C:\Program Files\Common Files\VST3\
  and rescan in your host. The standalone needs nothing.

A NOTE ON WHAT THIS IS

  The object has no controls. Everything named on the frame was named by the
  expedition, for its own convenience, and describes what the apparatus does
  rather than any faculty the object possesses.

  The object has no off state. It was sounding when it was found and it has
  sounded since; the frame can open onto that sound or close again, and that
  is the whole of the interaction.

  What you handle is a three-dimensional section through a four-dimensional
  body. Moving the section does not alter the object. A different part of it
  is simply present.

  Nothing here is a picture. Every surface shown is computed as the section
  moves, because no picture would stay correct.

  THE BENCH. The four findings were recovered from one site, and on the bench
  they behave as if they still were. SITE on the rail opens the bench's
  settings, which every finding present shares: CLIMATE - one temperature for
  all of them - and TIMING - cooled and set close together, they fall into
  step; warm, or far apart, each keeps its own time. This body joins by
  rocking its section through the tissue in the bench's time, and the second
  body waits for the bench's beat to interject. Both are off unless you turn
  them on, and off, each finding is exactly what it was.

Requires Windows 10 or later, 64-bit, and a VST3 host.
Field record B2311 - both objects remain active.
"@
[IO.File]::WriteAllText((Join-Path $stage "README.txt"), $readme, (New-Object Text.UTF8Encoding($false)))

Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path $out) { Remove-Item $out -Force }
[System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $out)
Write-Output ("  wrote " + (Split-Path $out -Leaf) + "  " + [math]::Round((Get-Item $out).Length / 1MB, 1) + " MB")
