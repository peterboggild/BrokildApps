// Stretch to the target aspect rather than padding to it.
//
// Padding preserves an object's own proportions, which sounds like the careful
// choice and is the wrong one twice over: it leaves the object floating inside
// a transparent frame, so a key ends up smaller than the button it dresses;
// and an object DRAWN oval stays oval, just inside a square frame. Stretching
// puts the object exactly where the panel expects it and quietly corrects a
// generator that got the proportions slightly wrong — which is the thing
// ChatGPT flagged. Severe distortion is reported so it can be sent back
// instead of silently accepted.
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/FullMetalRacket/tools/ingest-decals.ps1";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function rep(a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(a).join(b);
}

rep(`  # pad the box out to the wanted aspect, centred, so nothing is ever cropped
  $w = [double]$box.w; $h = [double]$box.h
  if ($isCircle) { $w = [Math]::Max($w, $h); $h = $w }
  else {
    if (($w / $h) -lt $aspect) { $w = $h * $aspect } else { $h = $w / $aspect }
  }
  $outW = [int][Math]::Round($w); $outH = [int][Math]::Round($h)
  $offX = ($outW - $box.w) / 2.0; $offY = ($outH - $box.h) / 2.0`,
`  # STRETCH to the wanted aspect rather than pad to it. See the header.
  $srcAspect = $box.w / [double]$box.h
  $outW = $box.w; $outH = $box.h
  if ($srcAspect -lt $aspect) { $outW = [int][Math]::Round($box.h * $aspect) }
  else                        { $outH = [int][Math]::Round($box.w / $aspect) }
  if ($outW -lt 8) { $outW = 8 }
  if ($outH -lt 8) { $outH = 8 }
  $stretch = [Math]::Abs($srcAspect / $aspect - 1.0) * 100.0`, "stretch");

rep(`    $dst = New-Object System.Drawing.Rectangle([int]$offX, [int]$offY, $box.w, $box.h)`,
    `    $dst = New-Object System.Drawing.Rectangle(0, 0, $outW, $outH)`, "dst rect");

rep(`  "  {0,-20} {1,5} x {2,-5} aspect {3:N3}{4}" -f $name, $outW, $outH, ($outW / [double]$outH),
    $(if ($isCircle) { "  (square, circular part)" } else { "" })`,
`  $flag = ""
  if ($stretch -gt 25.0)    { $flag = "   <-- corrected {0:N0}%, WORTH A REDO" -f $stretch }
  elseif ($stretch -gt 6.0) { $flag = "   (corrected {0:N0}%)" -f $stretch }
  $mark = ""
  if ($isCircle) { $mark = "  circle" }
  "  {0,-20} {1,5} x {2,-5} aspect {3:N3}{4}{5}" -f $name, $outW, $outH, ($outW / [double]$outH), $mark, $flag`, "report");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("ingest now stretches to aspect and reports the correction");
