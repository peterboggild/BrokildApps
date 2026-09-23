# Package B2311.104 as recovered material.
#
# Flat structure, matching the other findings (copied from B2311.22's script,
# not invented). The findings report travels with the object rather than a
# manual, because there is no manual: nothing about these objects is operated.
#
# Cut AFTER a clean relink: the working standalone accumulates Smart App
# Control hash-nudge bytes and those must not ship.

$stage = "C:\Users\peter\b\ArtefactB2311_104\dist\stage"
$web   = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\proxima-centauri-b"
$out   = Join-Path $web "Artefact-B2311-104-win64.zip"
$build = "C:\Users\peter\b\ArtefactB2311_104\build\ArtefactB2311_104_artefacts\Release"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

$bundle = Join-Path $stage "Artefact B2311.104.vst3\Contents\x86_64-win"
New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item "$build\VST3\Artefact B2311.104.vst3\Contents\x86_64-win\Artefact B2311.104.vst3" $bundle -Force

$mi = "$build\VST3\Artefact B2311.104.vst3\Contents\Resources\moduleinfo.json"
if ((Test-Path $mi) -and ((Get-Item $mi).Length -gt 0)) {
    $res = Join-Path $stage "Artefact B2311.104.vst3\Contents\Resources"
    New-Item -ItemType Directory -Force $res | Out-Null
    Copy-Item $mi $res -Force
    Write-Output "  moduleinfo.json included"
} else { Write-Output "  moduleinfo.json skipped (absent or zero-byte)" }

$exe = Get-ChildItem "$build\Standalone" -Filter "*.exe" -ErrorAction SilentlyContinue |
       Where-Object { $_.Name -notlike "*probe*" } | Select-Object -First 1
if ($exe) { Copy-Item $exe.FullName (Join-Path $stage "Artefact B2311.104.exe") -Force
            Write-Output ("  standalone: " + $exe.Name) }

Copy-Item (Join-Path $web "Proxima-Centauri-b-Findings.pdf") (Join-Path $stage "Proxima Centauri b - Field Findings.pdf") -Force

$readme = @"
PROXIMA CENTAURI b - ARTEFACT B2311.104
Field record B2311. Accession 104, day 327, lower gallery of Kell Rille.
The most recent finding.

WHAT IS IN THIS ARCHIVE

  Artefact B2311.104.vst3   the frame, as a Windows VST3
  Artefact B2311.104.exe    the same frame, standing alone
  Proxima Centauri b -
    Field Findings.pdf      what was established about the recovered objects,
                            and the questions the survey could not close

INSTALLING

  Copy the .vst3 folder into
      C:\Program Files\Common Files\VST3\
  and rescan in your host. The standalone needs nothing.

A NOTE ON WHAT THIS IS

  This object moves energy, and the moving of it is a sound. It is a WEB: slender
  conduits meeting at junctions, the whole of it lying on the curved
  four-dimensional surface our three dimensions cut through. What confounds the
  survey is that it shares almost nothing with the other three findings.

  It does not answer being struck. Past a threshold of disequilibrium a conduit
  begins to SING on its own, growing from nothing to a steady deep tone, and
  keeps singing while the disequilibrium lasts. There is no oscillator anywhere
  in it; where oscillation happens it is a condition the object has been carried
  into, not a thing switched on.

  Warming a conduit is how it is tuned. Heat sets the speed of sound, the speed
  of sound sets the pitch - so to name a note is to command a TEMPERATURE, and
  the object heats a conduit until its resonance is the note you asked for. A
  hard attack is a fast pour of heat; a slow glide is a great thermal mass; a
  release is the conduit cooling. Every glide you hear is metal warming and
  cooling, not an envelope drawn over a tone.

  The AMBIENT slider across the top warms the whole web at once, from the
  nitrogen temperature to white heat, and it is one axis: cold is ORDER, hot is
  DISORDER. Frozen, the web obeys - notes land where they are ordered, stop
  when the order is lifted, and nothing stirs on its own. Near 234 K, the
  equilibrium temperature of the planet, it settles into an oddly stable
  figure. Warmed past that it begins to wander: released notes carry on,
  wandering heat crosses the threshold here and there, and hot enough the grid
  runs on its own schedule with nobody playing - which is the survey's reason
  for suspecting it a distribution device, a fragment of a grid still running.

  The same delayed heat release that lets a conduit sing at all will, driven too
  hard, carry the tone through period-doubling and into chaos - the way a real
  thermoacoustic engine loses its footing. TURBULENCE is how readily it does so;
  a clean tube tone, a hollow sub-octave and a broadband roar are the same note
  at three settings of it. STIFFNESS rings the conduit like inharmonic metal,
  off any harmonic series. And the four-dimensional body you see is not decor
  for the sound: a conduit's place in the projection sets its timbre, so turning
  the body (drag the empty ground) re-voices the whole web - what you see is
  what you hear.

  The whole passive web retunes with the ambient temperature as the square root
  of it, while a note you are holding stays where you put it. Heat travels the
  junctions from conduit to conduit, so playing one warms its neighbours,
  retunes them, and can carry them over their own threshold until they sound in
  sympathy.

  THE BENCH. The four findings were recovered from one site, and on the bench
  they behave as if they still were. Under THE SITE in the survey annotations
  (triple-click the void) are the bench's settings, which every finding present
  shares: CLIMATE - one temperature for all of them - and TIMING - cooled and
  set close together, they fall into step; warm, or far apart, each keeps its
  own time. This object joins by letting its traffic land on the bench's
  pulse. Both are off unless you turn them on, and off, each finding is
  exactly what it was.

  THE HAND. Every control is on the frame and none of them is labelled; the
  ledger along the foot translates one while it is touched. The mouse wheel
  pushes the three-space through the fourth axis; shift-wheel thickens the slice
  that is present. Drag the ground to turn the body through itself. Drag a
  conduit to pour heat into it; tap one to strike it. A MIDI note is a thermal
  order. Triple-click the point at infinity - the small void at the centre - for
  the survey's pencilled annotations.

  Nothing on the frame was named by whoever made the object. Every word on it is
  ours, and describes what our apparatus does. The survey calls the suspected
  builders the Sonorians, in want of better.

Requires Windows 10 or later, 64-bit, and a VST3 host.
Field record B2311 - the object remains active.
"@
[IO.File]::WriteAllText((Join-Path $stage "README.txt"), $readme, (New-Object Text.UTF8Encoding($false)))

Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path $out) { Remove-Item $out -Force }
[System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $out)
Write-Output ("  wrote " + (Split-Path $out -Leaf) + "  " + [math]::Round((Get-Item $out).Length / 1MB, 1) + " MB")
