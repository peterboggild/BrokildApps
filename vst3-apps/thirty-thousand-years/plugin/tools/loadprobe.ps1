# Load a DLL the way a host would, and say why if it cannot be loaded.
#   powershell -File tools/loadprobe.ps1 [path]
# 4551 = Smart App Control (per file, per path). ASCII only.
param([string]$Path = "$PSScriptRoot\..\build\ThirtyThousandYears_artefacts\Release\VST3\Thirty Thousand Years.vst3\Contents\x86_64-win\Thirty Thousand Years.vst3")
$sig = '[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryW(string p); [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr h);'
$k = Add-Type -MemberDefinition $sig -Name LLP -Namespace TTYP -PassThru
$h = $k::LoadLibraryW($Path)
if ($h -ne [IntPtr]::Zero) { $k::FreeLibrary($h) | Out-Null; Write-Output ("loads: " + $Path + " (" + [math]::Round((Get-Item $Path).Length / 1MB, 1) + " MB)"); exit 0 }
$err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
Write-Output ("BLOCKED err " + $err + ": " + (New-Object System.ComponentModel.Win32Exception $err).Message)
exit 1
