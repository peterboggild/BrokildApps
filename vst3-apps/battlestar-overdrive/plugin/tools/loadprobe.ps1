# Does the INSTALLED (or built) DLL actually load? Smart App Control blocks by
# file hash, and a blocked plugin looks exactly like a vanished one to a host.
# Always probe the file the host will really open, not the build output.
param([Parameter(Mandatory=$true)][string]$Dll)

$src = @"
using System;
using System.Runtime.InteropServices;
public static class L {
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern IntPtr LoadLibraryW(string p);
  [DllImport("kernel32.dll")] public static extern uint GetLastError();
  [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr h);
}
"@
Add-Type -TypeDefinition $src

$full = (Resolve-Path $Dll).Path
$h = [L]::LoadLibraryW($full)
if ($h -eq [IntPtr]::Zero) {
  $err = [L]::GetLastError()
  $msg = (New-Object System.ComponentModel.Win32Exception([int]$err)).Message
  Write-Output ("BLOCKED  error {0}: {1}" -f $err, $msg)
  if ($err -eq 4551) { Write-Output "  -> Smart App Control. Rebuild for a new hash, or nudge the installed file." }
  exit 1
}
[L]::FreeLibrary($h) | Out-Null
$sz = (Get-Item $full).Length
$sha = (Get-FileHash $full -Algorithm SHA256).Hash.Substring(0,16)
Write-Output ("LOADS OK   {0:N0} bytes   sha {1}" -f $sz, $sha)
