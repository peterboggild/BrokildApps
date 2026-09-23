# Can each standalone actually start? This is the thing that decides whether
# the panel can be driven against a real backend, or only rendered from HTML.
$apps = @(
    @{ n = 'Photo-Synth2'; p = 'C:\Users\peter\b\PhotoSynth\build\PhotoSynth_artefacts\Release\Standalone\Photo-Synth2.exe' },
    @{ n = 'Escape Room';  p = 'C:\Users\peter\b\EscapeRoom\build\EscapeRoom_artefacts\Release\Standalone\Escape Room.exe' },
    @{ n = 'Mars Wars';    p = 'C:\Users\peter\b\MarsWars\build\MarsWars_artefacts\Release\Standalone\The Mars Wars.exe' },
    @{ n = 'Blade Ruiner'; p = 'C:\Users\peter\b\BladeRuiner\build\BladeRuiner_artefacts\Release\Standalone\Blade Ruiner.exe' }
)

Write-Output ("{0,-15} {1,-9} {2,-14} {3}" -f 'STANDALONE', 'STARTS', 'BUILT', 'WHY NOT')
Write-Output ('-' * 78)

foreach ($a in $apps) {
    if (-not (Test-Path -LiteralPath $a.p)) {
        Write-Output ("{0,-15} {1}" -f $a.n, 'not built')
        continue
    }
    $built = (Get-Item -LiteralPath $a.p).LastWriteTime.ToString('MM-dd HH:mm')
    try {
        $proc = Start-Process -FilePath $a.p -PassThru -ErrorAction Stop
        Start-Sleep -Milliseconds 2500
        if ($proc.HasExited) {
            Write-Output ("{0,-15} {1,-9} {2,-14} {3}" -f $a.n, 'NO', $built, ('exited ' + $proc.ExitCode))
        } else {
            Write-Output ("{0,-15} {1,-9} {2,-14} {3}" -f $a.n, 'yes', $built, '')
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        }
    } catch {
        Write-Output ("{0,-15} {1,-9} {2,-14} {3}" -f $a.n, 'NO', $built, $_.Exception.Message)
    }
    Start-Sleep -Milliseconds 700
}
