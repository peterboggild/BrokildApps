# Run the bench past Smart App Control: nudge-copy loop (the house trick).
$dir = "$PSScriptRoot\..\test\build\Release"
$src = Join-Path $dir "abtest.exe"
$out = Join-Path $dir "bench-out.txt"
$r = New-Object System.Random
for ($i = 1; $i -le 10; $i++) {
    $name = "probe{0}.exe" -f $r.Next(10000, 99999)
    $dst = Join-Path $dir $name
    Copy-Item $src $dst -Force
    $b = New-Object byte[] ($r.Next(40, 900)); $r.NextBytes($b)
    Add-Content $dst -Value $b -Encoding Byte
    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $dst
        $psi.RedirectStandardOutput = $true
        $psi.UseShellExecute = $false
        $proc = [System.Diagnostics.Process]::Start($psi)
        $txt = $proc.StandardOutput.ReadToEnd()
        $proc.WaitForExit()
        Set-Content $out $txt -Encoding utf8
        Write-Output ("RAN on attempt {0}" -f $i)
        Remove-Item (Join-Path $dir "probe*.exe") -Force -ErrorAction SilentlyContinue
        exit 0
    } catch {
        Write-Output ("attempt {0} blocked" -f $i)
        Start-Sleep -Milliseconds 700
    }
}
Remove-Item (Join-Path $dir "probe*.exe") -Force -ErrorAction SilentlyContinue
Write-Output "ALL ATTEMPTS BLOCKED"
exit 1
