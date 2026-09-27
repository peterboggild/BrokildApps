Add-Type -Namespace NC -Name N -MemberDefinition '[DllImport("kernel32", SetLastError=true, CharSet=CharSet.Unicode)] public static extern System.IntPtr LoadLibraryW(string p);'

$tmp = 'C:\Users\peter\b\_sactest'
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

$br = '$PSScriptRoot\..\build\BladeRuiner_artefacts\Release\VST3\Blade Ruiner.vst3\Contents\x86_64-win\Blade Ruiner.vst3'
$mw = '$PSScriptRoot\..\build\MarsWars_artefacts\Release\VST3\The Mars Wars.vst3\Contents\x86_64-win\The Mars Wars.vst3'

function Try-Load($path, $label) {
    $h = [NC.N]::LoadLibraryW($path)
    $e = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    $ok = ($h -ne [System.IntPtr]::Zero)
    Write-Output ("{0,-46} {1,-6} {2}" -f $label, $(if ($ok) { 'LOADS' } else { 'BLOCKED' }),
        $(if ($ok) { '' } else { (New-Object System.ComponentModel.Win32Exception $e).Message }))
}

Write-Output ("{0,-46} {1,-6} {2}" -f 'TEST', 'RESULT', 'ERROR')
Write-Output ('-' * 100)

# 1. the two originals, as a control
Try-Load $br 'Blade Ruiner content, Blade Ruiner name'
Try-Load $mw 'Mars Wars content,    Mars Wars name'

# 2. same bytes, harmless name
$a = "$tmp\Ordinary Widget.vst3"
Copy-Item -LiteralPath $br -Destination $a -Force
Try-Load $a 'Blade Ruiner content, neutral name'

# 3. Mars Wars bytes wearing the Blade Ruiner name
$b = "$tmp\Blade Ruiner.vst3"
Copy-Item -LiteralPath $mw -Destination $b -Force
Try-Load $b 'Mars Wars content,    Blade Ruiner name'

# 4. Blade Ruiner bytes, a name that is not a film
$c = "$tmp\Brokild Atmos.vst3"
Copy-Item -LiteralPath $br -Destination $c -Force
Try-Load $c 'Blade Ruiner content, Brokild Atmos name'

# 5. one byte different, same name - does any edit clear it?
$d = "$tmp\Blade Ruiner b.vst3"
$bytes = [System.IO.File]::ReadAllBytes($br)
$bytes[$bytes.Length - 1] = [byte](($bytes[$bytes.Length - 1] + 1) % 256)
[System.IO.File]::WriteAllBytes($d, $bytes)
Try-Load $d 'Blade Ruiner content +1 byte, similar name'

Write-Output ''
Write-Output ('sha256 Blade Ruiner : ' + (Get-FileHash -LiteralPath $br -Algorithm SHA256).Hash.Substring(0,32))
Write-Output ('sha256 Mars Wars    : ' + (Get-FileHash -LiteralPath $mw -Algorithm SHA256).Hash.Substring(0,32))
