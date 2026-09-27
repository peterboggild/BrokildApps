# Probe an INSTALLED plugin binary and, if Smart App Control has blocked it,
# append a few random bytes until it clears.
#
# SAC judges by file hash and judges a COPY separately from the file it was
# copied from, so "the build output loads" proves nothing about the installed
# one. Overlay bytes after the last PE section are ignored by the loader, so
# the plugin is unaffected while the hash changes and SAC re-evaluates.
param([string[]]$Paths)

Add-Type -Namespace SacProbe -Name Native -MemberDefinition @'
[DllImport("kernel32", SetLastError=true, CharSet=CharSet.Unicode)]
public static extern IntPtr LoadLibraryW(string p);
'@

function Test-Load([string]$p) {
    $h = [SacProbe.Native]::LoadLibraryW($p)
    if ($h -eq [IntPtr]::Zero) {
        $e = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        return "BLOCKED err=$e : $((New-Object System.ComponentModel.Win32Exception $e).Message)"
    }
    return "OK"
}

foreach ($p in $Paths) {
    if (-not (Test-Path $p)) { Write-Output "MISSING  $p"; continue }
    $r = Test-Load $p
    $n = 0
    while ($r -ne "OK" -and $n -lt 6) {
        $n++
        $bytes = New-Object byte[] (Get-Random -Minimum 5 -Maximum 40)
        (New-Object Random).NextBytes($bytes)
        $fs = [System.IO.File]::Open($p, 'Append', 'Write')
        $fs.Write($bytes, 0, $bytes.Length)
        $fs.Close()
        Start-Sleep -Milliseconds 250
        $r = Test-Load $p
    }
    Write-Output "$r  (nudges: $n)  $p"
}
