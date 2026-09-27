// Two fixes from looking at the decalled panel.
//
// 1. MOIRE. The knob is a beautiful 1024px part with fine concentric brushing,
//    drawn at 52px on the panel. The browser's own downscale samples that
//    brushing rather than averaging it, and the result is a rotating star
//    pattern that looks like a fan blade — the part is right, the scaling is
//    wrong. Every part is now downsampled at INGEST to about four times its
//    on-panel size with a proper bicubic filter, which averages the detail
//    away instead of aliasing it. It also takes the compiled size from ~19 MB
//    to a fraction of that.
//
// 2. The panel went muddy: the cream texture was MULTIPLIED with the cream
//    gradient underneath it, and cream times cream is tan. The texture is the
//    ground now, with the lighting laid over it.
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

const w1 = edit(R + "tools/ingest-decals.ps1", [
[`$CIRCLE = @("fmr-knob", "fmr-led", "fmr-led-lit", "fmr-screw")`,
`# The largest dimension each part is allowed to keep: roughly four times what
# it occupies on the panel. Beyond that the browser is downscaling fine detail
# at render time, which aliases — the knob's concentric brushing turned into a
# rotating star. Downsampling here, once, with a real filter, fixes it.
$MAXDIM = @{
  "fmr-panel"       = 1024   # tiled at 512, so 1024 is already generous
  "fmr-anodised"    = 640
  "fmr-cheek"       = 720
  "fmr-knob"        = 208
  "fmr-fadercap-v"  = 112
  "fmr-fadercap-m"  = 192
  "fmr-slot-v"      = 400
  "fmr-slot-m"      = 1392
  "fmr-key"         = 424
  "fmr-key-lit"     = 424
  "fmr-keydark"     = 392
  "fmr-keydark-lit" = 392
  "fmr-pad"         = 284
  "fmr-led"         = 96
  "fmr-led-lit"     = 96
  "fmr-nameplate"   = 1408
  "fmr-screw"       = 96
}
$CIRCLE = @("fmr-knob", "fmr-led", "fmr-led-lit", "fmr-screw")`, "maxdim table"],

[`  $stretch = [Math]::Abs($srcAspect / $aspect - 1.0) * 100.0`,
`  $stretch = [Math]::Abs($srcAspect / $aspect - 1.0) * 100.0

  # downsample to the size the panel actually needs
  $shrunk = ""
  if ($MAXDIM.ContainsKey($name)) {
    $cap = $MAXDIM[$name]
    $big = [Math]::Max($outW, $outH)
    if ($big -gt $cap) {
      $k = $cap / [double]$big
      $was = "{0}x{1}" -f $outW, $outH
      $outW = [Math]::Max(8, [int][Math]::Round($outW * $k))
      $outH = [Math]::Max(8, [int][Math]::Round($outH * $k))
      $shrunk = "   from $was"
    }
  }`, "downsample"],

[`    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic`,
`    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality`, "pixel offset"],

[`  "  {0,-20} {1,5} x {2,-5} aspect {3:N3}{4}{5}" -f $name, $outW, $outH, ($outW / [double]$outH), $mark, $flag`,
 `  "  {0,-20} {1,5} x {2,-5} aspect {3:N3}{4}{5}{6}" -f $name, $outW, $outH, ($outW / [double]$outH), $mark, $flag, $shrunk`, "report"]
]);

const w2 = edit(R + "Source/ui/ui.html", [
[`      add('#face{background-image:url("' + u + '"),radial-gradient(130% 70% at 50% -10%,#ffffff30,#0000 55%),linear-gradient(180deg,#f6efe0,#e7dcc3 18%,#d3c6a6 92%,#bfb08c)!important;background-size:512px 512px,auto,auto;background-blend-mode:multiply,normal,normal}');
      add('.strip,#webk{background-image:url("' + u + '")!important;background-size:512px 512px;background-blend-mode:multiply}');`,
`      /*  The texture is the GROUND, with the lighting over it. Multiplying it
          into the cream gradient underneath made cream times cream, which is
          tan, and the whole machine went muddy. */
      add('#face{background:radial-gradient(130% 70% at 50% -10%,#ffffff44,#0000 52%),' +
          'linear-gradient(180deg,#ffffff22,#0000 22%,#00000018),url("' + u + '") 0 0/512px 512px!important}');
      add('.strip,#webk{background:linear-gradient(180deg,#00000018,#0000 26%,#0000001c),' +
          'url("' + u + '") 0 0/512px 512px!important}');`, "face blend"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
w1(); w2();
console.log("parts downsample at ingest; the panel texture is the ground, not a multiply");
