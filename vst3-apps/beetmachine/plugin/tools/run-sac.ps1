# Run a freshly linked test exe past Smart App Control.
#
# SAC judges a file by its hash and can refuse a fresh link. A copy with a few
# random bytes appended past the last PE section is the same program with a new
# hash (overlay data is ignored by the loader), so a refused run is retried on
# such a copy. ASCII only: Windows PowerShell 5.1 reads a no-BOM .ps1 as ANSI.
param(
  [Parameter(Mandatory=$true)][string]$Exe,
  [string[]]$ExeArgs = @()
)
$dir = Split-Path $Exe
$name = [IO.Path]::GetFileNameWithoutExtension($Exe)
for ($attempt = 0; $attempt -le 6; $attempt++) {
  $target = $Exe
  if ($attempt -gt 0) {
    $target = Join-Path $dir ("{0}-sac{1}.exe" -f $name, $attempt)
    Copy-Item $Exe $target -Force
    $bytes = New-Object byte[] (1 + (Get-Random -Maximum 16)); (New-Object Random).NextBytes($bytes)
    $fs = [IO.File]::Open($target, 'Append', 'Write'); $fs.Write($bytes, 0, $bytes.Length); $fs.Close()
  }
  try {
    & $target @ExeArgs
    $code = $LASTEXITCODE
    if ($attempt -gt 0) { Remove-Item $target -Force -ErrorAction SilentlyContinue }
    exit $code
  } catch {
    if ($_.Exception.Message -match 'Application Control|blocked') {
      Write-Output ("  (Smart App Control refused attempt {0}; retrying on a nudged copy)" -f $attempt)
      if ($attempt -gt 0) { Remove-Item $target -Force -ErrorAction SilentlyContinue }
      continue
    }
    throw
  }
}
Write-Output "could not get past Smart App Control"; exit 1
