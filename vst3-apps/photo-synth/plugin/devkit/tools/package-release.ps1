# Build a distributable zip: the .vst3 bundle (as a folder), the manual and a
# readme. A VST3 is a directory — the archive must preserve that.
#
#   powershell -ExecutionPolicy Bypass -File package-release.ps1 `
#       -Project C:\b\MyPlugin -Product "My Plugin" -Target MyPlugin

param(
    [string]$Project = 'C:\b\MyPlugin',
    [string]$Product = 'My Plugin',
    [string]$Target  = 'MyPlugin',
    [string]$Vendor  = 'YourVendor',
    [string]$Manual  = ''            # optional path to a PDF
)
$ErrorActionPreference = 'Stop'

$bundle = Join-Path $Project "build\${Target}_artefacts\Release\VST3\$Product.vst3"
if (-not (Test-Path $bundle)) { throw "no built bundle at $bundle — build Release first" }

$dist  = Join-Path $Project 'dist'
$stage = Join-Path $dist ('stage-' + (Get-Date -Format 'HHmmss'))
New-Item -ItemType Directory -Force $stage | Out-Null

Copy-Item -Recurse $bundle (Join-Path $stage "$Product.vst3")
$items = @((Join-Path $stage "$Product.vst3"))

if ($Manual -and (Test-Path $Manual)) {
    Copy-Item $Manual (Join-Path $stage (Split-Path $Manual -Leaf))
    $items += (Join-Path $stage (Split-Path $Manual -Leaf))
}

$readme = @"
$Product - VST3 instrument (Windows, 64-bit)
$('=' * 50)

INSTALL
-------
1. Unzip this archive.
2. Copy the whole "$Product.vst3" FOLDER into:

     C:\Program Files\Common Files\VST3\

   (keep it as a folder - do not copy just the file inside it)
3. Rescan plugins in your DAW. It appears as "$Product" by $Vendor.

REQUIREMENTS
------------
- Windows 10/11, 64-bit
- A VST3 host (Ableton Live, Reaper, Cubase, Bitwig, FL Studio, ...)
- Microsoft Edge WebView2 Runtime, preinstalled on current Windows. If the
  plugin window stays blank, install it from:
  https://developer.microsoft.com/microsoft-edge/webview2/

Note: a DAW maps a plugin once per session, so after updating the plugin you
must restart the DAW - removing and re-adding the device is not enough.
"@
$readme | Out-File (Join-Path $stage 'README.txt') -Encoding utf8
$items += (Join-Path $stage 'README.txt')

$zip = Join-Path $dist "$($Product -replace '\s','-')-VST3-win64.zip"
Compress-Archive -Path $items -DestinationPath $zip -Force

# Verify: the bundle must be inside the zip as a directory tree.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [IO.Compression.ZipFile]::OpenRead($zip)
foreach ($e in $z.Entries) { Write-Output ("  {0,-58} {1,7} KB" -f $e.FullName, [math]::Round($e.Length / 1KB)) }
$z.Dispose()

cmd /c rmdir /s /q "$stage"
Write-Output ("`n$zip  ({0} MB)" -f [math]::Round((Get-Item $zip).Length / 1MB, 2))
