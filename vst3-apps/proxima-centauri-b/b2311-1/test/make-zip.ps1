# Package B2311.1 as recovered material.
#
# Flat structure, matching the other findings (copied from B2311.22's script,
# not invented). The findings report travels with the object rather than a
# manual, because there is no manual: nothing about these objects is operated.
#
# Cut AFTER a clean relink: the working standalone accumulates Smart App
# Control hash-nudge bytes and those must not ship.

$stage = "$PSScriptRoot\..\dist\stage"
$web   = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\proxima-centauri-b"
$out   = Join-Path $web "Artefact-B2311-1-win64.zip"
$build = "$PSScriptRoot\..\build\ArtefactB2311_1_artefacts\Release"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

$bundle = Join-Path $stage "Artefact B2311.1.vst3\Contents\x86_64-win"
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item "$build\VST3\Artefact B2311.1.vst3\Contents\x86_64-win\Artefact B2311.1.vst3" $bundle -Force

$mi = "$build\VST3\Artefact B2311.1.vst3\Contents\Resources\moduleinfo.json"
if ((Test-Path $mi) -and ((Get-Item $mi).Length -gt 0)) {
    $res = Join-Path $stage "Artefact B2311.1.vst3\Contents\Resources"
    New-Item -ItemType Directory -Force $res | Out-Null
    Copy-Item $mi $res -Force
    Write-Output "  moduleinfo.json included"
} else { Write-Output "  moduleinfo.json skipped (absent or zero-byte)" }

$exe = Get-ChildItem "$build\Standalone" -Filter "*.exe" -ErrorAction SilentlyContinue |
       Where-Object { $_.Name -notlike "*probe*" } | Select-Object -First 1
if ($exe) { Copy-Item $exe.FullName (Join-Path $stage "Artefact B2311.1.exe") -Force
            Write-Output ("  standalone: " + $exe.Name) }

Copy-Item (Join-Path $web "Proxima-Centauri-b-Findings.pdf") (Join-Path $stage "Proxima Centauri b - Field Findings.pdf") -Force

$readme = @"
PROXIMA CENTAURI b - ARTEFACT B2311.1
Field record B2311. Surface collection, day 6, scree above Kell Rille.

WHAT IS IN THIS ARCHIVE

  Artefact B2311.1.vst3    the frame, as a Windows VST3
  Artefact B2311.1.exe     the same frame, standing alone
  Proxima Centauri b -
    Field Findings.pdf     what was established about the recovered objects,
                           and the questions the survey could not close

INSTALLING

  Copy the .vst3 folder into
      C:\Program Files\Common Files\VST3\
  and rescan in your host. The standalone needs nothing.

A NOTE ON WHAT THIS IS

  This object counts. Every part of it holds a count that rises at its own
  rate, and on reaching the top it discharges, resets, and shoves the counts
  beside it forward. A shove can carry a neighbour over with it, so discharges
  run in cascades, and the object speaks in bursts of every length from one
  part to all of them at once. Nobody chose those lengths.

  Its sound is not a vibration. There is no oscillator anywhere in it. What you
  hear is the LIST OF MOMENTS at which something happened, and rhythm, pitch
  and colour are not three properties of it but one - the rate of events - heard
  at three scales. Nothing in the object distinguishes between them, because
  there is no reason it should.

  Exposed to an outside pulse it eases towards it and never arrives. Pushed
  harder it comes apart instead. It can be led; it cannot be forced. The survey
  has no account of why a thing that appears built to be joined would refuse to
  be driven.

  Cold, it does not count. The frame carries a reading in kelvin; what the
  expedition found readable lies between the nitrogen temperature and something
  above a warm room.

  It was collected in the first week and logged as a nodule, and it sat in a
  crate for two years. What made anyone look at it again was that two
  independent clocks stored beside it had stopped drifting apart. It had been
  counting at them the whole time.

  THE BENCH. The four findings were recovered from one site, and on the bench
  they behave as if they still were. The SITE button on the foot of the frame
  opens the bench's settings, which every finding present shares: CLIMATE -
  one temperature for all of them - and TIMING - cooled and set close
  together, they fall into step; warm, or far apart, each keeps its own time.
  This object joins by taking the bench's pulse as one more pulse to lean
  towards. Both are off unless you turn them on, and off, each finding is
  exactly what it was.

  Nothing on the frame was named by whoever made the object. Every word on it
  is ours, and describes what our apparatus does.

Requires Windows 10 or later, 64-bit, and a VST3 host.
Field record B2311 - the object remains active.
"@
[IO.File]::WriteAllText((Join-Path $stage "README.txt"), $readme, (New-Object Text.UTF8Encoding($false)))

Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path $out) { Remove-Item $out -Force }
[System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $out)
Write-Output ("  wrote " + (Split-Path $out -Leaf) + "  " + [math]::Round((Get-Item $out).Length / 1MB, 1) + " MB")
