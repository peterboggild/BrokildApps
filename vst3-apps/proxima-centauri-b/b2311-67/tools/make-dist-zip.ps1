<#
    Cut the published archive for Artefact B2311.67.

    WHY THIS EXISTS. The download and the page that describes it drifted apart:
    the zip on the site carried 260830.2 while the tree was at 260830.7, and the
    page's own description had already been written against a feature the
    download did not contain. That is not a mistake anyone makes on purpose --
    it is what happens when cutting an archive is a fiddly job done by hand. So
    it is a script, and it verifies its own output.

    It takes the four entries the published archive has always had, in the same
    layout, so the landing page never needs editing:

        Artefact B2311.67.vst3\Contents\x86_64-win\Artefact B2311.67.vst3
        Artefact B2311.67.exe
        Proxima Centauri b - Field Findings.pdf     (the live one from the site)
        README.txt                                  (dist/README.txt, tracked)

    Then it opens the archive it just wrote, loads the plugin OUT OF IT, and
    reads the build id out of those same bytes. A zip that has not been proved
    to load is not evidence of anything.

        powershell -File tools\make-dist-zip.ps1              cut and verify
        powershell -File tools\make-dist-zip.ps1 -Verify      verify what is published
#>
param(
    [switch] $Verify,
    [string] $Repo = "$PSScriptRoot\..",
    [string] $Site = "C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\proxima-centauri-b"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression.FileSystem

$zipName = "Artefact-B2311-67-win64.zip"
$published = Join-Path $Site $zipName
$rel = Join-Path $Repo "build\ArtefactB2311_67_artefacts\Release"
$dll = Join-Path $rel "VST3\Artefact B2311.67.vst3\Contents\x86_64-win\Artefact B2311.67.vst3"
$exe = Join-Path $rel "Standalone\Artefact B2311.67.exe"

function Get-BuildId([string] $path) {
    $t = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($path))
    $ids = [regex]::Matches($t, '26\d{4}\.\d+') | ForEach-Object { $_.Value } | Sort-Object -Unique
    # the BWFX rack stamps its own version in too; the plugin's is the later one
    ($ids | Sort-Object)[-1]
}

function Test-Loads([string] $path) {
    if (-not ("Loader67" -as [type])) {
        Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public class Loader67 {
  [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Unicode)]
  public static extern IntPtr LoadLibraryW(string p);
}
'@
    }
    $h = [Loader67]::LoadLibraryW($path)
    if ($h -eq [IntPtr]::Zero) {
        $e = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        return "FAILED ($e - " + (New-Object System.ComponentModel.Win32Exception $e).Message + ")"
    }
    return "loads"
}

# ---------------------------------------------------------------- verify only
if ($Verify) {
    if (-not (Test-Path $published)) { throw "nothing published at $published" }
    $tmp = Join-Path $env:TEMP ("ab67verify-" + (Get-Date -Format "HHmmssfff"))
    Expand-Archive -Path $published -DestinationPath $tmp -Force
    $inner = Join-Path $tmp "Artefact B2311.67.vst3\Contents\x86_64-win\Artefact B2311.67.vst3"
    $tree = (Select-String -Path (Join-Path $Repo "CMakeLists.txt") -Pattern 'AB_BUILD_ID "([^"]+)"').Matches[0].Groups[1].Value
    $pub  = Get-BuildId $inner
    "published : $pub"
    "tree      : $tree"
    "loads     : " + (Test-Loads $inner)
    if ($pub -eq $tree) { "IN STEP" } else { "OUT OF STEP -- re-cut it" }
    return
}

# ------------------------------------------------------------------------ cut
foreach ($p in @($dll, $exe)) { if (-not (Test-Path $p)) { throw "not built: $p" } }

$stage = Join-Path $env:TEMP ("ab67dist-" + (Get-Date -Format "HHmmssfff"))
New-Item -ItemType Directory -Force $stage | Out-Null
New-Item -ItemType Directory -Force (Join-Path $stage "Artefact B2311.67.vst3\Contents\x86_64-win") | Out-Null

Copy-Item $dll (Join-Path $stage "Artefact B2311.67.vst3\Contents\x86_64-win\Artefact B2311.67.vst3")
Copy-Item $exe (Join-Path $stage "Artefact B2311.67.exe")
Copy-Item (Join-Path $Repo "dist\README.txt") (Join-Path $stage "README.txt")

#  The findings report belongs to the site and is revised there; take whatever
#  is live rather than a copy that was current when some earlier zip was cut.
$pdf = Join-Path $Site "Proxima-Centauri-b-Findings.pdf"
if (Test-Path $pdf) { Copy-Item $pdf (Join-Path $stage "Proxima Centauri b - Field Findings.pdf") }
else { Write-Warning "no findings PDF at $pdf -- archive cut without it" }

$out = Join-Path $stage "..\$zipName"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $out -CompressionLevel Optimal -Force

#  Prove it, from inside the archive that was just written.
$chk = Join-Path $env:TEMP ("ab67check-" + (Get-Date -Format "HHmmssfff"))
Expand-Archive -Path $out -DestinationPath $chk -Force
$inner = Join-Path $chk "Artefact B2311.67.vst3\Contents\x86_64-win\Artefact B2311.67.vst3"
$loads = Test-Loads $inner
$id    = Get-BuildId $inner
$tree  = (Select-String -Path (Join-Path $Repo "CMakeLists.txt") -Pattern 'AB_BUILD_ID "([^"]+)"').Matches[0].Groups[1].Value

"{0:N1} MB" -f ((Get-Item $out).Length / 1MB)
$z = [System.IO.Compression.ZipFile]::OpenRead($out)
$z.Entries | ForEach-Object { '{0,12}  {1}' -f $_.Length, $_.FullName }
$z.Dispose()
"build id in the archive : $id"
"build id in the tree    : $tree"
"the plugin in the zip   : $loads"

if ($loads -ne "loads") { throw "the archive was NOT published: the plugin in it does not load" }
if ($id -ne $tree)      { throw "the archive was NOT published: it is $id and the tree is $tree" }

Copy-Item $out $published -Force
"published -> $published"
