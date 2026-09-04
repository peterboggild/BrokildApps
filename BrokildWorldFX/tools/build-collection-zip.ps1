# Assemble THE BROKILD COLLECTION from the PUBLISHED per-plugin zips.
#
# Two house rules make this the only sane source:
#
#  * Clone Wars' binary must never come from a local build - only from a zip a
#    green CI run produced. Taking every plugin from its published download
#    honours that by construction, for all of them.
#
#  * The collection must contain exactly what the individual downloads contain,
#    or someone gets a different binary depending on which button they pressed.
#    So the script hash-compares every file it stages against the file inside
#    the source zip, and again after re-extracting the finished archive.
#
# The per-plugin zips do not agree on their internal shape - some carry an
# inner folder, some are flat - so nothing here parses paths: it searches each
# extraction for the .vst3 bundle, the .exe and the .pdf, and insists on
# finding exactly one of each.
#
# Hairfryer is deliberately absent: it is unlisted.

$web   = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps"
$out   = "$web\collection\Brokild-Collection-win64.zip"
$work  = "$env:TEMP\brokild-collection"
$inner = "Brokild-Collection-win64"

# newest first, the way the front page lists them
$plugins = @(
  @{ slug = "brain-scan";        name = "Brain Scan" },
  @{ slug = "high-tide";         name = "High Tide" },
  @{ slug = "photo-synth";       name = "Photo Synth" },
  @{ slug = "escape-room";       name = "Escape Room" },
  @{ slug = "blade-ruiner";      name = "Blade Ruiner" },
  @{ slug = "black-rider";       name = "Black Rider" },
  @{ slug = "clone-wars";        name = "Clone Wars" },
  @{ slug = "full-metal-racket"; name = "Full Metal Racket" },
  @{ slug = "martian-gain";      name = "Martian Gain" }
)

if (Test-Path $work) { Remove-Item $work -Recurse -Force }
$stage = Join-Path $work $inner
New-Item -ItemType Directory -Force $stage | Out-Null

$manifest = @()
foreach ($p in $plugins) {
    $zip = Get-ChildItem "$web\$($p.slug)" -Filter "*-VST3-win64.zip" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $zip) { Write-Output ("  {0,-20} NO PUBLISHED ZIP - abort" -f $p.name); exit 1 }

    $ex = Join-Path $work ("src-" + $p.slug)
    Expand-Archive $zip.FullName -DestinationPath $ex -Force

    $bundle = @(Get-ChildItem $ex -Recurse -Directory -Filter "*.vst3" |
                Where-Object { Test-Path (Join-Path $_.FullName "Contents") })
    $exe    = @(Get-ChildItem $ex -Recurse -File -Filter "*.exe")
    $pdf    = @(Get-ChildItem $ex -Recurse -File -Filter "*.pdf")
    if ($bundle.Count -ne 1) { Write-Output ("  {0,-20} expected one .vst3 bundle, found {1}" -f $p.name, $bundle.Count); exit 1 }
    if ($exe.Count -ne 1)    { Write-Output ("  {0,-20} expected one .exe, found {1}" -f $p.name, $exe.Count); exit 1 }
    if ($pdf.Count -ne 1)    { Write-Output ("  {0,-20} expected one manual, found {1}" -f $p.name, $pdf.Count); exit 1 }

    $dest = Join-Path $stage $p.name
    New-Item -ItemType Directory -Force $dest | Out-Null
    Copy-Item $bundle[0].FullName $dest -Recurse -Force
    Copy-Item $exe[0].FullName (Join-Path $dest ($p.name + ".exe")) -Force
    Copy-Item $pdf[0].FullName (Join-Path $dest ($p.name + " Manual.pdf")) -Force

    $dll = Get-ChildItem (Join-Path $dest ($bundle[0].Name)) -Recurse -File -Filter "*.vst3" | Select-Object -First 1
    $srcDll = Get-ChildItem $bundle[0].FullName -Recurse -File -Filter "*.vst3" | Select-Object -First 1
    $manifest += [pscustomobject]@{
        Name = $p.name
        Zip  = $zip.Name
        Dll  = $dll.FullName
        Hash = (Get-FileHash $srcDll.FullName -Algorithm SHA256).Hash
        Size = [math]::Round($srcDll.Length / 1MB, 1)
    }
    Write-Output ("  {0,-20} from {1,-34} {2} MB" -f $p.name, $zip.Name, [math]::Round($srcDll.Length / 1MB, 1))
}

$readme = @"
THE BROKILD COLLECTION
Nine Windows plugins: eight instruments and one effect.
Brokild Apps  -  September 2026  -  Windows 64-bit  -  free


WHAT IS IN HERE
---------------
  Brain Scan         a synth that stores no waveforms but a VOLUME, and
                     reads a curve through it. Nine specimens, one of
                     them a simulated brain. SCAN blends the geometry of
                     two curves, so halfway between two sounds is tissue
                     neither of them has visited
  High Tide          a wavetable synth that stores no waveforms either:
                     every frame is a bowl and the sound is a ball
                     dropped into it. The tide floods the passes between
                     the valleys; pins on a timeline tow the ball
  Photo Synth        a synthesiser you play by moving a cursor across a
                     photograph - the colours under it are the sound
  Escape Room        five noise cells, a playable filter and a SIGIL, a
                     number 0 to 63 that seeds the whole machine. Same
                     sigil, same instrument, always. Baffling on purpose
  Blade Ruiner       three layers - a drone city, an eight-voice poly
                     synth and a sequencer - and a mood organ: dial a
                     number 0 to 999 and get an instrument and a line of
                     text that belong to each other
  Black Rider        an analogue monosynth between the MS-20 and the
                     Moog. Two oscillators you can push over the edge,
                     three filter circuits on one switch, a patch bay
  Clone Wars         sixteen detuned oscillators in two armies of eight,
                     crossfaded by one fader that can take five minutes
                     to cross. The panel visibly ages as you play it
  Full Metal Racket  an analogue drum machine whose twelve voices share
                     a power rail, a shell and a bleed web, so they
                     behave like one kit in one room
  Martian Gain       a multiband distortion with a patch bay under the
                     front panel, and a gain match measured rather than
                     modelled - so DRIVE changes the sound without
                     changing the level


INSTALL
-------
  Each folder holds the plugin, the standalone and its manual.

  1. Copy each "<name>.vst3" FOLDER - the whole folder, not just the
     file inside it - into your system VST3 directory:

         C:\Program Files\Common Files\VST3\

     A sub-folder such as ...\VST3\Brokild\ is fine and tidier; hosts
     look inside. Windows will ask for administrator permission. That
     is normal.

  2. Start your DAW and rescan plugins.
     In Ableton Live: Preferences - Plug-Ins - Rescan.

  3. They appear under Brokild. Eight are instruments; Martian Gain is
     an effect.

  Or just double-click any of the .exe files. No installation needed.


WHAT THEY HAVE IN COMMON
------------------------
  BROKILD WORLD FX. Eight of the nine carry the same rack of global
  effects behind a teal globe - saturation, phaser, chorus, gate,
  echo, reverb, rotary, a step glitcher and more - plus SPECTRA
  characters, which do not treat the mix but possess the instrument
  from the inside, detuning and sagging its voices. A rack built in
  one of them opens in any of the others. It is empty by default and
  adds nothing until you put something in it. (Martian Gain is an
  effect, so it has no rack: you already have a chain to put things
  in.)

  YOUR PATCHES ALL LIVE TOGETHER, in

      Documents\Brokild patches\<plugin name>

  a folder per plugin under one roof. Deliberately not beside the
  installed plugin: that is somewhere an installer can reach and
  replace, and work has been lost that way.

  NOTHING IS ASSERTED. Every one of these is built against an offline
  bench that renders real audio and measures it - tuning in cents,
  aliasing in decibels, timing in samples - and the numbers in the
  manuals come from that rather than from anyone's opinion.

  THE INTERFACES are drawn in a WebView2 surface, which every current
  Windows 10 and 11 already has. If a window comes up blank, install
  the free Microsoft Edge WebView2 Runtime and reopen the plugin. The
  audio keeps working with the window shut either way.


  Free. No installer, no account, no telemetry, no network access at
  all.

  These come from the curved mind of Professor Brokild, and were
  programmed with VScode, ChatGPT and Claude. Use at your own peril.
  They may generate unique, beautiful, scary or useless sounds, and
  they may crash your DAW project. Have fun; play unsafely, unwisely,
  and unboringly.

  Cheers, Peter Boggild, September 2026.
  https://peterboggild.github.io/BrokildApps/
"@
Set-Content -Path (Join-Path $stage "README.txt") -Value $readme -Encoding ASCII

if (Test-Path $out) { Remove-Item $out -Force }
Compress-Archive -Path $stage -DestinationPath $out -CompressionLevel Optimal
$mb = [math]::Round((Get-Item $out).Length / 1MB, 1)
Write-Output ""
Write-Output "  zip: $out ($mb MB)"

# verify: every plugin in the finished archive is byte-identical to the one in
# its own published download
$probe = "$work\verify"
Expand-Archive -Path $out -DestinationPath $probe -Force
$bad = 0
foreach ($m in $manifest) {
    $f = Get-ChildItem (Join-Path $probe "$inner\$($m.Name)") -Recurse -File -Filter "*.vst3" | Select-Object -First 1
    if (-not $f) { Write-Output ("  {0,-20} MISSING FROM THE ARCHIVE" -f $m.Name); $bad++; continue }
    $h = (Get-FileHash $f.FullName -Algorithm SHA256).Hash
    if ($h -ne $m.Hash) { Write-Output ("  {0,-20} HASH MISMATCH against {1}" -f $m.Name, $m.Zip); $bad++ }
}
if ($bad -gt 0) { Write-Output "  $bad plugin(s) do not match their own download"; exit 1 }
Write-Output ("  all {0} plugins byte-identical to their own published downloads" -f $manifest.Count)
Remove-Item $work -Recurse -Force
Write-Output "  $mb MB"
