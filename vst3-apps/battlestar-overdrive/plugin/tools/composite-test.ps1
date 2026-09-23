# Composite the knobs, fuel tube and button onto the panel at the geometry in
# assets/panel-geometry.json, so the numbers can be CHECKED by looking rather
# than trusted. Renders both knob sets for comparison.
param(
  [string]$Root = "C:\Users\peter\b\BattlestarOverdrive",
  [ValidateSet("knobChrome","knobOrange")][string]$Set = "knobChrome",
  [double]$Value = 0.5,          # 0..1, drives every knob's pointer
  [int]$FuelIndex = 0,           # 0 = full .. 14 = empty
  [switch]$ButtonLit,
  [string]$Out = ""
)
Add-Type -AssemblyName System.Drawing
$geo = Get-Content (Join-Path $Root "assets\panel-geometry.json") -Raw | ConvertFrom-Json
$dec = Join-Path $Root "assets\decals"
if (-not $Out) { $Out = Join-Path $Root "docs\composite-$Set.png" }

$panel = [System.Drawing.Bitmap]::FromFile((Join-Path $dec "panel.png"))
$canvas = New-Object System.Drawing.Bitmap $geo.panel.w, $geo.panel.h
$g = [System.Drawing.Graphics]::FromImage($canvas)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.DrawImage($panel, 0, 0, $geo.panel.w, $geo.panel.h)

$sheetDef = $geo.sheets.$Set
$sheet = [System.Drawing.Bitmap]::FromFile((Join-Path $dec $sheetDef.file))

# --- fuel tube ---------------------------------------------------------------
$f = $geo.sheets.fuel
$fsheet = [System.Drawing.Bitmap]::FromFile((Join-Path $dec $f.file))
$fi = [Math]::Max(0, [Math]::Min(14, $FuelIndex))
$fc = $f.cols[$fi % 3]; $fr = $f.rows[[Math]::Floor($fi / 3)]
$srcF = New-Object System.Drawing.Rectangle $fc, $fr, $f.w, $f.h
$dstF = New-Object System.Drawing.Rectangle $geo.fuelGlass.x, $geo.fuelGlass.y, $geo.fuelGlass.w, $geo.fuelGlass.h
$g.DrawImage($fsheet, $dstF, $srcF, [System.Drawing.GraphicsUnit]::Pixel)

# --- knobs -------------------------------------------------------------------
# The decal's pointer points straight up, so the rotation IS the value angle.
$sweep = [double]$geo.knobSweepDeg
$d = [double]$geo.knobDiameter
foreach ($p in $geo.pots) {
  $part = $sheetDef.parts[$p.sheetIndex]
  $ang = ($Value - 0.5) * $sweep
  $state = $g.Save()
  $g.TranslateTransform([float]$p.x, [float]$p.y)
  $g.RotateTransform([float]$ang)
  # Draw centred on the (now rotated) origin, keeping the part's own aspect.
  $h = $d * $part.h / [double]$part.w
  $dstK = New-Object System.Drawing.RectangleF ([float](-$d/2)), ([float](-$h/2)), ([float]$d), ([float]$h)
  $srcK = New-Object System.Drawing.Rectangle $part.x, $part.y, $part.w, $part.h
  $g.DrawImage($sheet, $dstK, $srcK, [System.Drawing.GraphicsUnit]::Pixel)
  $g.Restore($state)
}

# --- Autorefill button -------------------------------------------------------
$b = $sheetDef.button
$bd = [double]$geo.button.d
$bh = $bd * $b.h / [double]$b.w
if ($ButtonLit) {
  # A lit button glows into the bezel around it.
  $glow = New-Object System.Drawing.Drawing2D.GraphicsPath
  $glow.AddEllipse([float]($geo.button.cx - $bd*1.15), [float]($geo.button.cy - $bd*1.15), [float]($bd*2.3), [float]($bd*2.3))
  $br = New-Object System.Drawing.Drawing2D.PathGradientBrush $glow
  $br.CenterColor = [System.Drawing.Color]::FromArgb(150, 255, 40, 20)
  $br.SurroundColors = @([System.Drawing.Color]::FromArgb(0, 255, 40, 20))
  $g.FillPath($br, $glow)
  $br.Dispose(); $glow.Dispose()
}
$dstB = New-Object System.Drawing.RectangleF ([float]($geo.button.cx - $bd/2)), ([float]($geo.button.cy - $bh/2)), ([float]$bd), ([float]$bh)
$srcB = New-Object System.Drawing.Rectangle $b.x, $b.y, $b.w, $b.h
$g.DrawImage($sheet, $dstB, $srcB, [System.Drawing.GraphicsUnit]::Pixel)

$g.Dispose()
$canvas.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$canvas.Dispose(); $panel.Dispose(); $sheet.Dispose(); $fsheet.Dispose()
Write-Output "$Out   set=$Set value=$Value fuel=$FuelIndex lit=$ButtonLit"
