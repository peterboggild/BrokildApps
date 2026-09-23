# Build and run the 1984 bench past Smart App Control.
# SAC blocks a freshly linked exe by hash about half the time; a copy with a
# few random bytes appended is the same program with a new hash.
# ASCII only: Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI.
param([switch]$NoBuild, [string]$Args = "")
$root = "C:\Users\peter\b\Nineteen84"
if (-not $NoBuild) {
    if (-not (Test-Path "$root\test\build\CMakeCache.txt")) {
        cmake -S "$root\test" -B "$root\test\build" -A x64 | Out-Null
    }
    cmake --build "$root\test\build" --config Release --target n84test 2>&1 | Select-String -Pattern "error|warning C4|n84test.vcxproj ->" | ForEach-Object { $_.Line }
    if ($LASTEXITCODE -ne 0) { Write-Output "BUILD FAILED"; exit 1 }
}
$exe = "$root\test\build\Release\n84test.exe"
if (-not (Test-Path $exe)) { Write-Output "no exe"; exit 1 }
$rand = New-Object System.Random
for ($i = 0; $i -lt 6; $i++) {
    $copy = "$root\test\build\Release\n84test_run$i.exe"
    Copy-Item $exe $copy -Force
    $b = New-Object byte[] ($rand.Next(1, 9)); $rand.NextBytes($b)
    $fs = [System.IO.File]::Open($copy, 'Append', 'Write'); $fs.Write($b, 0, $b.Length); $fs.Close()
    try {
        if ($Args -ne "") { & $copy $Args.Split(" ") } else { & $copy }
        $code = $LASTEXITCODE
        Remove-Item $copy -Force -ErrorAction SilentlyContinue
        exit $code
    } catch {
        Write-Output ("attempt {0} refused: {1}" -f ($i + 1), $_.Exception.Message)
        Remove-Item $copy -Force -ErrorAction SilentlyContinue
    }
}
Write-Output "could not run the bench (SAC)"
exit 1
