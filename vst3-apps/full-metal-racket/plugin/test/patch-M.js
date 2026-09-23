// The ingest read exactly one DELIVERED.md, so round two's manifest — saved
// beside the first as DELIVERED-round2.md — was never opened and the three
// step-button states were silently absent. Silently is the problem: seventeen
// of twenty parts loading looks identical to all of them loading unless you
// count. It reads every DELIVERED*.md now, and says how many manifests it
// found so a missing one is visible.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

const w = edit(R + "tools/ingest-decals.ps1", [
[`$md = Join-Path $SetDir "DELIVERED.md"
if (-not (Test-Path $md)) {
  Write-Output "no DELIVERED.md yet - only standalone files were ingested"
  Write-Output ("ingested {0} part(s)" -f $standalone.Count)
  exit 0
}

$text = Get-Content $md -Raw
$m = [regex]::Match($text, '(?s)\`\`\`json\\s*(.*?)\`\`\`')
if (-not $m.Success) { Write-Output "DELIVERED.md has no json block - nothing to slice"; exit 0 }

$spec = $m.Groups[1].Value | ConvertFrom-Json
$done = 0
foreach ($sheet in $spec.sheets) {`,
`# EVERY manifest, not just the first: each delivery round writes its own, and
# reading only DELIVERED.md quietly dropped round two's step buttons.
$mds = @(Get-ChildItem $SetDir -Filter "DELIVERED*.md" -ErrorAction SilentlyContinue | Sort-Object Name)
if ($mds.Count -eq 0) {
  Write-Output "no DELIVERED*.md yet - only standalone files were ingested"
  Write-Output ("ingested {0} part(s)" -f $standalone.Count)
  exit 0
}
Write-Output ("{0} manifest(s): {1}" -f $mds.Count, (($mds | ForEach-Object { $_.Name }) -join ", "))

$sheets = @()
foreach ($f in $mds) {
  $text = Get-Content $f.FullName -Raw
  $m = [regex]::Match($text, '(?s)\`\`\`json\\s*(.*?)\`\`\`')
  if (-not $m.Success) { Write-Output ("  {0}: no json block" -f $f.Name); continue }
  try { $sheets += (($m.Groups[1].Value | ConvertFrom-Json).sheets) }
  catch { Write-Output ("  {0}: json would not parse - {1}" -f $f.Name, $_.Exception.Message) }
}

$done = 0
foreach ($sheet in $sheets) {`, "read all manifests"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
w();
console.log("ingest reads every DELIVERED*.md");
