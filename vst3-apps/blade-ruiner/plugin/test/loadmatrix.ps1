Add-Type -Namespace LM2 -Name N -MemberDefinition '[DllImport("kernel32", SetLastError=true, CharSet=CharSet.Unicode)] public static extern System.IntPtr LoadLibraryW(string p);'

$root = 'C:\Program Files\Common Files\VST3\Brokild'
Write-Output ("{0,-20} {1,-7} {2,-14} {3}" -f 'PLUGIN', 'LOADS', 'BUILT', 'ERROR')
Write-Output ('-' * 78)

foreach ($d in (Get-ChildItem $root -Directory | Where-Object { $_.Name -like '*.vst3' })) {
    $stem = $d.Name.Substring(0, $d.Name.Length - 5)     # drop ".vst3"
    $dll  = $d.FullName + '\Contents\x86_64-win\' + $stem + '.vst3'
    if (-not (Test-Path -LiteralPath $dll)) {
        Write-Output ("{0,-20} {1}" -f $stem, 'no binary at ' + $dll)
        continue
    }
    $h = [LM2.N]::LoadLibraryW($dll)
    $e = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
    $ok = ($h -ne [System.IntPtr]::Zero)
    $msg = if ($ok) { '' } else { (New-Object System.ComponentModel.Win32Exception $e).Message }
    Write-Output ("{0,-20} {1,-7} {2,-14} {3}" -f $stem,
        $(if ($ok) { 'yes' } else { 'NO' }),
        (Get-Item -LiteralPath $dll).LastWriteTime.ToString('MM-dd HH:mm'), $msg)
}

Write-Output ''
Write-Output ('SAC policy state : ' + (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' -Name VerifiedAndReputablePolicyState -EA SilentlyContinue).VerifiedAndReputablePolicyState)
Write-Output ('CI enforcement   : ' + (Get-CimInstance -ClassName Win32_DeviceGuard -Namespace root\Microsoft\Windows\DeviceGuard).CodeIntegrityPolicyEnforcementStatus)
