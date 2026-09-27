# Splice the native bridge into a copy of the prototype page.
#
# Replaces whole named functions between markers, so the operation is
# repeatable: when the prototype is updated, re-copy it and re-run this
# instead of re-porting by hand.
#
# Edit $edits below for your prototype, then:
#   powershell -ExecutionPolicy Bypass -File splice-bridge.ps1
#
# NOTE: save this file as UTF-8 *with BOM* if you put non-ASCII in it, and
# always read/write pages with explicit UTF-8 — Get-Content -Raw / Out-File
# mangle em dashes and degree signs on PowerShell 5.1.

$ErrorActionPreference = 'Stop'

$parts  = Join-Path $PSScriptRoot 'bridge-parts'   # replacement bodies live here
$page   = 'C:\b\MyPlugin\Source\ui\ui.html'        # the working copy (never the reference)
$utf8   = New-Object System.Text.UTF8Encoding($false)

$text = [IO.File]::ReadAllText($page, [Text.Encoding]::UTF8)

function ReadPart([string]$name) {
    return [IO.File]::ReadAllText((Join-Path $parts $name), [Text.Encoding]::UTF8)
}

# Replace everything from $from up to (not including) $to.
function SpliceRange([string]$t, [string]$from, [string]$to, [string]$repl) {
    $i = $t.IndexOf($from)
    if ($i -lt 0) { throw "start marker not found: $from" }
    $j = $t.IndexOf($to, $i + $from.Length)
    if ($j -lt 0) { throw "end marker not found: $to" }
    return $t.Substring(0, $i) + $repl + $t.Substring($j)
}

function ReplaceOnce([string]$t, [string]$old, [string]$new) {
    $i = $t.IndexOf($old)
    if ($i -lt 0) { throw "not found: $old" }
    return $t.Substring(0, $i) + $new + $t.Substring($i + $old.Length)
}

function InsertBefore([string]$t, [string]$anchor, [string]$ins) {
    $i = $t.IndexOf($anchor)
    if ($i -lt 0) { throw "anchor not found: $anchor" }
    return $t.Substring(0, $i) + $ins + $t.Substring($i)
}

# 1. Insert the bridge itself, ahead of the first function that uses it.
$text = InsertBefore $text '  /* ==== colour -> synthesis ====' (ReadPart 'bridge.js')

# 2. Replace the audio-graph functions with proxy equivalents. Each entry:
#    from-marker, to-marker (the start of the NEXT function), replacement file.
$edits = @(
  @{ from = '  function buildChain(ctx, withWorklet'; to = '  function engineIsAnalog('; part = 'buildChain.js' },
  @{ from = '  function wireFXOrder(g) {';            to = '  function buildChain(ctx,';   part = 'wireFXOrder.js' },
  @{ from = '  /* ---- live context ---';             to = '  function ensureAudio() {';   part = 'livectx.js' }
)
foreach ($e in $edits) {
    $text = SpliceRange $text $e.from $e.to (ReadPart $e.part)
    Write-Output ("spliced: " + $e.part)
}

# 3. Small in-place edits (branding, download routing, dialog fallbacks).
$text = ReplaceOnce $text '<title>Prototype</title>' '<title>My Plugin</title>'

[IO.File]::WriteAllText($page, $text, $utf8)
Write-Output ("done — " + $text.Length + " chars")
