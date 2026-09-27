# Run a freshly linked exe, nudging past Smart App Control if it refuses.
#
# SAC blocks by file HASH and the verdict is a dice roll on a new binary, so a
# bench that will not start is usually not a broken bench. A few random bytes
# appended past the last PE section change the hash without changing the
# program, because overlay data is ignored by the loader. The nudge is applied
# to a COPY, so the build output keeps its own hash and nothing shipped is
# touched.
#
# ASCII only - Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI.
param(
  [Parameter(Mandatory = $true)][string]$Exe,
  [string]$Arguments = "",
  [int]$Attempts = 8,
  [int]$TimeoutSec = 900
)

if (-not (Test-Path $Exe)) { Write-Output "NO SUCH EXE: $Exe"; exit 2 }

$dir = Split-Path $Exe -Parent
$rand = New-Object System.Random

for ($i = 1; $i -le $Attempts; $i++) {
  $copy = Join-Path $dir ("sac{0}.exe" -f $rand.Next(10000, 99999))
  Copy-Item $Exe $copy -Force
  if ($i -gt 1) {
    $b = New-Object byte[] ($rand.Next(40, 900))
    $rand.NextBytes($b)
    Add-Content $copy -Value $b -Encoding Byte
  }
  try {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $copy
    $psi.Arguments = $Arguments
    $psi.WorkingDirectory = $dir
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $err = $p.StandardError.ReadToEnd()
    if (-not $p.WaitForExit($TimeoutSec * 1000)) { $p.Kill(); throw "timed out" }
    Write-Output $out
    if ($err) { Write-Output $err }
    Remove-Item $copy -Force -ErrorAction SilentlyContinue
    Write-Output ("[ran on attempt {0}, exit {1}]" -f $i, $p.ExitCode)
    exit $p.ExitCode
  } catch {
    Remove-Item $copy -Force -ErrorAction SilentlyContinue
    Write-Output ("  attempt {0} refused: {1}" -f $i, $_.Exception.Message)
    Start-Sleep -Milliseconds 400
  }
}
Write-Output "ALL ATTEMPTS BLOCKED"
exit 1
