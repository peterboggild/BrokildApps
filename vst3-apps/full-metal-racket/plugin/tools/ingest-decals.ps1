# Turn delivered sprite sheets into individual, correctly-shaped panel parts.
#
# Reads assets/fmr-panel-decals/DELIVERED.md, pulls the JSON block out of it,
# crops each part from its sheet, trims the transparent margin, pads to the
# aspect ratio the panel needs, and writes art/<name>.png.
#
# Two details that matter more than they look:
#   * anything circular is padded to a SQUARE, because an oval knob wobbles
#     visibly as it turns;
#   * a lit/unlit PAIR is trimmed against the UNION of both bounding boxes, so
#     the two states stay registered with each other. Trimming them
#     independently shifts one against the other and the button appears to
#     twitch when it lights.
#
# Standalone files already named fmr-*.png are passed straight through.

param(
  [string]$SetDir = "c:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\assets\fmr-panel-decals",
  [string]$ArtDir = "C:\Users\peter\b\FullMetalRacket\art"
)

Add-Type -AssemblyName System.Drawing

# target aspect (w:h) per part; 1 means square
$ASPECT = @{
  "fmr-panel"        = 1.0
  "fmr-anodised"     = 1.0
  # fmr-cheek is deliberately absent: it is wood GRAIN, and correcting it to
  # the panel's very tall aspect smears the grain to seven times its scale.
  # It keeps its own proportions and the panel tiles it vertically instead.
  "fmr-knob"         = 1.0
  "fmr-fadercap-v"   = 1.0
  "fmr-fadercap-m"   = 368.0 / 272.0
  "fmr-slot-v"       = 104.0 / 400.0
  "fmr-slot-m"       = 184.0 / 1392.0
  "fmr-key"          = 848.0 / 168.0
  "fmr-key-lit"      = 848.0 / 168.0
  "fmr-keydark"      = 784.0 / 368.0
  "fmr-keydark-lit"  = 784.0 / 368.0
  "fmr-pad"          = 568.0 / 272.0
  "fmr-led"          = 1.0
  "fmr-led-lit"      = 1.0
  "fmr-nameplate"    = 1408.0 / 264.0
  "fmr-screw"        = 1.0
  "fmr-step"         = 144.0 / 132.0
  "fmr-step-on"      = 144.0 / 132.0
  "fmr-step-acc"     = 144.0 / 132.0
}
# The largest dimension each part is allowed to keep: roughly four times what
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
  "fmr-step"        = 144
  "fmr-step-on"     = 144
  "fmr-step-acc"    = 144
}
$CIRCLE = @("fmr-knob", "fmr-led", "fmr-led-lit", "fmr-screw")
# sets of states that must stay registered with one another. Not pairs — the
# step button has three, and trimming any of them independently shifts it
# against the others so the button appears to twitch as it lights.
$GROUPS = @(
  @("fmr-key", "fmr-key-lit"),
  @("fmr-keydark", "fmr-keydark-lit"),
  @("fmr-led", "fmr-led-lit"),
  @("fmr-step", "fmr-step-on", "fmr-step-acc")
)
# tileable textures are never trimmed: their edges ARE the content
$TILE   = @("fmr-panel", "fmr-anodised")

if (-not (Test-Path $ArtDir)) { New-Item -ItemType Directory -Force $ArtDir | Out-Null }

function Get-Alpha-Bounds($bmp) {
  $r = New-Object System.Drawing.Rectangle(0, 0, $bmp.Width, $bmp.Height)
  $d = $bmp.LockBits($r, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                     [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  try {
    $bytes = New-Object byte[] ($d.Stride * $bmp.Height)
    [System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $bytes, 0, $bytes.Length)
    $x0 = $bmp.Width; $y0 = $bmp.Height; $x1 = -1; $y1 = -1
    for ($y = 0; $y -lt $bmp.Height; $y++) {
      $row = $y * $d.Stride
      for ($x = 0; $x -lt $bmp.Width; $x++) {
        if ($bytes[$row + $x * 4 + 3] -gt 8) {
          if ($x -lt $x0) { $x0 = $x }
          if ($x -gt $x1) { $x1 = $x }
          if ($y -lt $y0) { $y0 = $y }
          if ($y -gt $y1) { $y1 = $y }
        }
      }
    }
  } finally { $bmp.UnlockBits($d) }
  if ($x1 -lt 0) { return $null }          # nothing but transparency
  return @{ x = $x0; y = $y0; w = ($x1 - $x0 + 1); h = ($y1 - $y0 + 1) }
}

function Save-Part($src, $box, $name) {
  $aspect = if ($ASPECT.ContainsKey($name)) { $ASPECT[$name] } else { $box.w / $box.h }
  $isCircle = $CIRCLE -contains $name

  # STRETCH to the wanted aspect rather than pad to it. See the header.
  $srcAspect = $box.w / [double]$box.h
  $outW = $box.w; $outH = $box.h
  if ($srcAspect -lt $aspect) { $outW = [int][Math]::Round($box.h * $aspect) }
  else                        { $outH = [int][Math]::Round($box.w / $aspect) }
  if ($outW -lt 8) { $outW = 8 }
  if ($outH -lt 8) { $outH = 8 }
  $stretch = [Math]::Abs($srcAspect / $aspect - 1.0) * 100.0

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
  }

  $out = New-Object System.Drawing.Bitmap($outW, $outH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($out)
  try {
    $g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $dst = New-Object System.Drawing.Rectangle(0, 0, $outW, $outH)
    $srcR = New-Object System.Drawing.Rectangle($box.x, $box.y, $box.w, $box.h)
    $g.DrawImage($src, $dst, $srcR, [System.Drawing.GraphicsUnit]::Pixel)
  } finally { $g.Dispose() }

  # ---- the cheek is tiled vertically, so it has to BE tileable -------------
  if ($name -eq "fmr-cheek") {
    # lose the rounded ends: they are what repeats
    $trim = [int]($out.Height * 0.15)
    $keepH = $out.Height - 2 * $trim
    if ($keepH -gt 16) {
      $slice = $out.Clone((New-Object System.Drawing.Rectangle(0, $trim, $out.Width, $keepH)),
                          [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
      # slice + its own vertical mirror: seamless at the middle AND at the wrap
      # the arithmetic has to happen BEFORE the argument list: PowerShell
      # parses an inline expression there as an array and New-Object refuses it
      $tileW = [int]$out.Width
      $tileH = [int]($keepH * 2)
      $tile = New-Object System.Drawing.Bitmap($tileW, $tileH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
      $tg = [System.Drawing.Graphics]::FromImage($tile)
      try {
        $tg.DrawImage($slice, 0, 0, $tileW, $keepH)
        $flip = $slice.Clone()
        $flip.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY)
        $tg.DrawImage($flip, 0, $keepH, $tileW, $keepH)
        $flip.Dispose()
      } finally { $tg.Dispose() }
      $slice.Dispose(); $out.Dispose(); $out = $tile
      $outW = $out.Width; $outH = $out.Height
      $shrunk = "$shrunk   ends cropped, mirror-tiled"
    }
  }

  $path = Join-Path $ArtDir "$name.png"
  $out.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $out.Dispose()
  $flag = ""
  if ($stretch -gt 25.0)    { $flag = "   <-- corrected {0:N0}%, WORTH A REDO" -f $stretch }
  elseif ($stretch -gt 6.0) { $flag = "   (corrected {0:N0}%)" -f $stretch }
  $mark = ""
  if ($isCircle) { $mark = "  circle" }
  "  {0,-20} {1,5} x {2,-5} aspect {3:N3}{4}{5}{6}" -f $name, $outW, $outH, ($outW / [double]$outH), $mark, $flag, $shrunk
}

# ---- 1 · anything already delivered as its own correctly named file --------
$standalone = @()
Get-ChildItem $SetDir -Filter "fmr-*.png" -ErrorAction SilentlyContinue | ForEach-Object {
  $name = $_.BaseName
  $bmp = [System.Drawing.Bitmap]::new($_.FullName)
  try {
    $box = if ($TILE -contains $name) { @{ x = 0; y = 0; w = $bmp.Width; h = $bmp.Height } }
           else { Get-Alpha-Bounds $bmp }
    if ($null -eq $box) { Write-Output "  $name : entirely transparent, skipped"; return }
    Write-Output (Save-Part $bmp $box $name)
    $standalone += $name
  } finally { $bmp.Dispose() }
}

# ---- 2 · the sheets, via DELIVERED.md --------------------------------------
# EVERY manifest, not just the first: each delivery round writes its own, and
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
  $m = [regex]::Match($text, '(?s)```json\s*(.*?)```')
  if (-not $m.Success) { Write-Output ("  {0}: no json block" -f $f.Name); continue }
  try { $sheets += (($m.Groups[1].Value | ConvertFrom-Json).sheets) }
  catch { Write-Output ("  {0}: json would not parse - {1}" -f $f.Name, $_.Exception.Message) }
}

$done = 0
foreach ($sheet in $sheets) {
  $sp = Join-Path $SetDir $sheet.file
  if (-not (Test-Path $sp)) { Write-Output ("  MISSING SHEET {0}" -f $sheet.file); continue }
  Write-Output ("sheet {0}" -f $sheet.file)
  $bmp = [System.Drawing.Bitmap]::new($sp)
  try {
    # a lit/unlit pair is trimmed to the UNION of both boxes, so the states
    # stay registered; trimming them apart makes the button twitch when lit
    $boxes = @{}
    foreach ($part in $sheet.parts) {
      $r = $part.rect
      $cx = [Math]::Max(0, [int]$r[0]); $cy = [Math]::Max(0, [int]$r[1])
      $cw = [Math]::Min([int]$r[2], $bmp.Width - $cx); $ch = [Math]::Min([int]$r[3], $bmp.Height - $cy)
      if ($cw -le 0 -or $ch -le 0) { Write-Output ("  {0}: rect outside the sheet" -f $part.name); continue }
      $sub = $bmp.Clone((New-Object System.Drawing.Rectangle($cx, $cy, $cw, $ch)),
                        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
      try {
        $b = if ($TILE -contains $part.name) { @{ x = 0; y = 0; w = $cw; h = $ch } } else { Get-Alpha-Bounds $sub }
        if ($null -eq $b) { Write-Output ("  {0}: crop is entirely transparent" -f $part.name); continue }
        # keep the crop origin AND the offset within it, so a pair can be
        # registered against each other later without dragging their
        # positions on the sheet into it
        $boxes[$part.name] = @{ x = ($cx + $b.x); y = ($cy + $b.y); w = $b.w; h = $b.h
                                ox = $cx; oy = $cy; rx = $b.x; ry = $b.y }
      } finally { $sub.Dispose() }
    }
    foreach ($grp in $GROUPS) {
      $have = @($grp | Where-Object { $boxes.ContainsKey($_) })
      if ($have.Count -lt 2) { continue }
      # union in CROP-RELATIVE coordinates: the same object seen in several
      # frames, wherever those frames happen to sit on the sheet
      $rx = ($have | ForEach-Object { $boxes[$_].rx } | Measure-Object -Minimum).Minimum
      $ry = ($have | ForEach-Object { $boxes[$_].ry } | Measure-Object -Minimum).Minimum
      $rw = ($have | ForEach-Object { $boxes[$_].rx + $boxes[$_].w } | Measure-Object -Maximum).Maximum - $rx
      $rh = ($have | ForEach-Object { $boxes[$_].ry + $boxes[$_].h } | Measure-Object -Maximum).Maximum - $ry
      foreach ($k in $have) {
        $boxes[$k] = @{ x = ($boxes[$k].ox + $rx); y = ($boxes[$k].oy + $ry); w = $rw; h = $rh
                        ox = $boxes[$k].ox; oy = $boxes[$k].oy; rx = $rx; ry = $ry }
      }
      Write-Output ("  {0} registered against each other ({1} x {2})" -f ($have -join " / "), $rw, $rh)
    }
    foreach ($part in $sheet.parts) {
      if (-not $boxes.ContainsKey($part.name)) { continue }
      if ($standalone -contains $part.name) { Write-Output ("  {0}: standalone file wins, sheet copy skipped" -f $part.name); continue }
      Write-Output (Save-Part $bmp $boxes[$part.name] $part.name)
      $done++
    }
  } finally { $bmp.Dispose() }
}

Write-Output ("ingested {0} standalone + {1} from sheets -> {2}" -f $standalone.Count, $done, $ArtDir)
Write-Output "now run: bash tools/refresh-decals.sh   (the CMake glob is configure-time)"
