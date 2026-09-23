# Assemble ONE Brokild collection from the PUBLISHED per-plug-in zips.
#
#   powershell -File build-collection.ps1 -Collection brokild
#   powershell -File build-collection.ps1 -Collection experimental
#
# Replaces build-collection-zip.ps1, which carried its own hardcoded list of
# plug-ins. There are three collections now, and a second hardcoded list is a
# second thing to forget: the membership lives in vst3-apps/collection/
# contents.json, this reads it, and tools/check-links.js holds the archive on
# disk to the same list.
#
# WHY THE PUBLISHED ZIPS ARE THE ONLY SANE SOURCE
#
#  * Clone Wars' binary must never come from a local build, only from a zip a
#    green CI run produced. Taking every plug-in from its published download
#    honours that by construction, for all of them.
#
#  * A collection must contain exactly what the individual downloads contain,
#    or someone gets a different binary depending on which button they pressed.
#    So every staged file is hash-compared against the file inside its source
#    zip, and again after re-extracting the finished archive.
#
# The per-plug-in zips do not agree on their internal shape - some carry an
# inner folder, some are flat - so nothing here parses paths. It searches each
# extraction for the bundle, the application and the manual.
#
# THE SHAPE RULE IS PER COLLECTION. The main collection promises a plug-in, a
# standalone and a handbook in every folder, and that promise is what makes the
# whole archive worth taking, so a folder that is short ABORTS the build. The
# experimental one accepts a bundle and whatever documentation exists, because
# holding an experiment back until it has a standalone means never shipping it.
#
# ASCII only - Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI.

param(
  [Parameter(Mandatory = $true)][ValidateSet("brokild", "experimental")][string]$Collection,
  [string]$Repo = "C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps"
)

Add-Type -AssemblyName System.IO.Compression.FileSystem

$web  = Join-Path $Repo "vst3-apps"
$spec = Get-Content (Join-Path $web "collection\contents.json") -Raw | ConvertFrom-Json
$c    = $spec.collections.$Collection
if (-not $c) { Write-Output "no such collection: $Collection"; exit 1 }

$strict = ($c.shape -like "*application*")
$folder = Join-Path $Repo ($c.folder -replace "/", "\")
$inner  = if ($Collection -eq "brokild") { "Brokild-Collection-win64" } else { "Brokild-Experimental-Collection-win64" }
$out    = Join-Path $folder ("{0}.zip" -f $inner)
$work   = Join-Path $env:TEMP ("brokild-coll-" + $Collection)

Write-Output ("{0}: {1} plug-ins, shape = {2}" -f $c.title, $c.includes.Count, $c.shape)
Write-Output ""

# The folder that gets zipped holds NOTHING but the collection. The scratch
# extractions live in a sibling, because the first version put them under the
# same root and CreateFromDirectory swept them in: every plug-in shipped twice
# and the archive came out at 227 MB instead of 111. The verification passed
# anyway, because it only asked whether each plug-in was PRESENT.
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
$outRoot = Join-Path $work "out"
$srcRoot = Join-Path $work "src"
$stage   = Join-Path $outRoot $inner
New-Item -ItemType Directory -Force $stage   | Out-Null
New-Item -ItemType Directory -Force $srcRoot | Out-Null

$manifest = @()
foreach ($slug in $c.includes) {
    $src = Join-Path $web $slug
    $zip = Get-ChildItem $src -Filter "*-win64.zip" -ErrorAction SilentlyContinue |
           Where-Object { $_.Name -notlike "*Collection*" } | Select-Object -First 1
    if (-not $zip) { Write-Output ("  {0,-22} NO PUBLISHED ZIP - abort" -f $slug); exit 1 }

    $ex = Join-Path $srcRoot $slug
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zip.FullName, $ex)

    $bundle = @(Get-ChildItem $ex -Recurse -Directory -Filter "*.vst3" |
                Where-Object { Test-Path (Join-Path $_.FullName "Contents") })
    $exe    = @(Get-ChildItem $ex -Recurse -File -Filter "*.exe")
    $pdf    = @(Get-ChildItem $ex -Recurse -File -Filter "*.pdf")
    $txt    = @(Get-ChildItem $ex -Recurse -File -Include "*.md","*.txt")

    if ($bundle.Count -ne 1) {
        Write-Output ("  {0,-22} expected one .vst3 bundle, found {1} - abort" -f $slug, $bundle.Count); exit 1 }
    if ($strict -and $exe.Count -ne 1) {
        Write-Output ("  {0,-22} the main collection promises a standalone; found {1} - abort" -f $slug, $exe.Count); exit 1 }
    if ($strict -and $pdf.Count -ne 1) {
        Write-Output ("  {0,-22} the main collection promises a handbook; found {1} - abort" -f $slug, $pdf.Count); exit 1 }

    # the folder is named after the PRODUCT, which is what the bundle is called
    $name = [System.IO.Path]::GetFileNameWithoutExtension($bundle[0].Name)
    $dest = Join-Path $stage $name
    New-Item -ItemType Directory -Force $dest | Out-Null
    Copy-Item $bundle[0].FullName $dest -Recurse -Force
    if ($exe.Count -ge 1) { Copy-Item $exe[0].FullName (Join-Path $dest ($name + ".exe")) -Force }
    if ($pdf.Count -ge 1) { Copy-Item $pdf[0].FullName (Join-Path $dest ($name + " Manual.pdf")) -Force }
    if ($pdf.Count -eq 0 -and $txt.Count -ge 1) {
        foreach ($t in $txt) { Copy-Item $t.FullName $dest -Force }
    }

    $srcDll = Get-ChildItem $bundle[0].FullName -Recurse -File -Filter "*.vst3" | Select-Object -First 1
    $manifest += [pscustomobject]@{
        Slug = $slug; Name = $name; Zip = $zip.Name
        Hash = (Get-FileHash $srcDll.FullName -Algorithm SHA256).Hash
        Size = [math]::Round($srcDll.Length / 1MB, 1)
        Extras = (@($(if ($exe.Count) { "exe" }), $(if ($pdf.Count) { "manual" }), $(if (-not $pdf.Count -and $txt.Count) { "readme" })) -ne $null) -join "+"
    }
    Write-Output ("  {0,-22} from {1,-38} {2} MB  {3}" -f $name, $zip.Name, $manifest[-1].Size, $manifest[-1].Extras)
}

# ---- the archive ----------------------------------------------------------
Remove-Item $out -Force -ErrorAction SilentlyContinue
[System.IO.Compression.ZipFile]::CreateFromDirectory($outRoot, $out, 'Optimal', $false)
$mb = [math]::Round((Get-Item $out).Length / 1MB, 1)
Write-Output ""
Write-Output ("  archive: {0}  ({1} MB)" -f (Split-Path $out -Leaf), $mb)

# ---- and it must BE what was staged ---------------------------------------
# "the zip exists" and "the zip contains the plug-ins you think it does" are
# different claims. Re-extract and hash every DLL against its source.
$probe = Join-Path $work "verify"
[System.IO.Compression.ZipFile]::ExtractToDirectory($out, $probe)
$bad = 0
foreach ($m in $manifest) {
    $dll = Get-ChildItem (Join-Path $probe $inner) -Recurse -File -Filter "*.vst3" |
           Where-Object { $_.FullName -like ("*" + $m.Name + "*") } | Select-Object -First 1
    if (-not $dll) { Write-Output ("  MISSING FROM ARCHIVE: " + $m.Name); $bad++; continue }
    if ((Get-FileHash $dll.FullName -Algorithm SHA256).Hash -ne $m.Hash) {
        Write-Output ("  HASH MISMATCH: " + $m.Name); $bad++
    }
}
# NOTHING EXTRA. The check above only asks whether each plug-in is PRESENT, and
# a check that looks only for what should be there cannot see what should not:
# the first build of this swept the scratch extractions into the archive, every
# plug-in shipped twice at 227 MB instead of 111, and it passed.
$topFolders = @(Get-ChildItem (Join-Path $probe $inner) -Directory | Select-Object -ExpandProperty Name)
$want = @($manifest | Select-Object -ExpandProperty Name)
foreach ($f in $topFolders) {
    if ($want -notcontains $f) { Write-Output ("  UNEXPECTED IN ARCHIVE: " + $f); $bad++ }
}
$stray = @(Get-ChildItem $probe -Directory | Where-Object { $_.Name -ne $inner })
foreach ($s in $stray) { Write-Output ("  UNEXPECTED AT THE ROOT: " + $s.Name); $bad++ }
if ($topFolders.Count -ne $want.Count) {
    Write-Output ("  archive holds {0} folders, the list names {1}" -f $topFolders.Count, $want.Count); $bad++
}

if ($bad -eq 0) {
    Write-Output ("  verified: {0} of {0} plug-ins byte-identical to their own downloads," -f $manifest.Count)
    Write-Output  "            and nothing else in the archive"
} else {
    Write-Output ("  {0} PROBLEM(S) - do not publish this archive" -f $bad); exit 1
}

$sha = (Get-FileHash $out -Algorithm SHA256).Hash
Write-Output ("  sha256:  {0}" -f $sha)
Write-Output ("  bytes:   {0}" -f (Get-Item $out).Length)
