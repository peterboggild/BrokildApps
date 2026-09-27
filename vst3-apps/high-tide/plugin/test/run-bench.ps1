# Run a freshly built test exe past Smart App Control.
# SAC judges each file by hash; a copy with a few random bytes appended is a
# new hash the loader ignores (overlay after the last PE section).
#   powershell -File test\run-bench.ps1 [-Exe httest] [-Args "..."]
param([string] $Exe = "httest", [string] $ArgLine = "")
$src = "$PSScriptRoot\..\test\build\Release\$Exe.exe"
if (-not (Test-Path $src)) { Write-Output "no $src"; exit 2 }
$rand = New-Object System.Random
for ($i = 0; $i -lt 8; $i++) {
    $copy = "$PSScriptRoot\..\test\build\Release\$Exe-run$i.exe"
    Copy-Item $src $copy -Force
    $b = New-Object byte[] ($rand.Next(3, 17)); $rand.NextBytes($b)
    Add-Content -Path $copy -Value $b -Encoding Byte
    try {
        if ($ArgLine -ne "") { $p = Start-Process -FilePath $copy -ArgumentList $ArgLine -NoNewWindow -PassThru -Wait -ErrorAction Stop }
        else              { $p = Start-Process -FilePath $copy -NoNewWindow -PassThru -Wait -ErrorAction Stop }
        Remove-Item $copy -Force -ErrorAction SilentlyContinue
        exit $p.ExitCode
    } catch {
        Remove-Item $copy -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 300
    }
}
Write-Output "SAC kept blocking $Exe after 8 nudges"
exit 3
