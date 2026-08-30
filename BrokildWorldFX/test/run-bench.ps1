# Run the BWFX bench past Smart App Control (the house nudge-copy loop).
$dir = "C:\Users\peter\b\BrokildWorldFX\test\build\Release"
$src = Join-Path $dir "bwfxtest.exe"
$out = Join-Path $dir "bench-out.txt"
$r = New-Object System.Random
for ($i = 1; $i -le 10; $i++) {
    $dst = Join-Path $dir ("bx{0}.exe" -f $r.Next(10000, 99999))
    Copy-Item $src $dst -Force
    $b = New-Object byte[] ($r.Next(40, 900)); $r.NextBytes($b)
    Add-Content $dst -Value $b -Encoding Byte
    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $dst
        $psi.RedirectStandardOutput = $true
        $psi.UseShellExecute = $false
        $p = [System.Diagnostics.Process]::Start($psi)
        $txt = $p.StandardOutput.ReadToEnd()
        $p.WaitForExit()
        Set-Content $out $txt -Encoding utf8
        Write-Output ("RAN on attempt {0}" -f $i)
        Remove-Item (Join-Path $dir "bx*.exe") -Force -ErrorAction SilentlyContinue
        exit 0
    } catch {
        Write-Output ("attempt {0} blocked" -f $i)
        Start-Sleep -Milliseconds 600
    }
}
Remove-Item (Join-Path $dir "bx*.exe") -Force -ErrorAction SilentlyContinue
Write-Output "ALL ATTEMPTS BLOCKED"
exit 1
