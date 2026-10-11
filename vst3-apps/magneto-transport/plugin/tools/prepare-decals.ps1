# Prepare a decal delivery for the panel, leaving the delivery itself untouched.
#
#   powershell -ExecutionPolicy Bypass -File tools\prepare-decals.ps1 -From <delivery folder>
#
# Writes assets/decals/<name>.png. What each part needs follows from how the panel draws
# it (docs/decal_integration.md), measured on the delivered image rather than assumed:
#   round parts that turn   cropped to a square centred on the canvas, edge to edge
#   paired lamps            both cropped with the OFF lamp's square, so they stay registered
#   arm                     re-seated: pivot at the canvas centre, roller 448/1024 to its right
#   VU needle               re-seated: pivot at (512, 1500), tip at (512, 40) of 1024 x 1536
#   heads, carriage, thumb  cropped to the drawing
#   halos                   faint alpha (below 56) removed on heads, rail, thumb, needle
#   glass                   alpha scaled to about 64 at the centre (the order's figure)
#   loop tank               glass inside the frame scaled to about 90, frame left opaque
#   plate, head plate       mirror-tiled 2 x 2 so they repeat without a seam (head plate
#                           from a calm corner: its centre swirl would repeat visibly)
#   slider track            cropped to the slot
#   everything else         copied as delivered
# ASCII only (Windows PowerShell 5.1 reads a no-BOM script as ANSI).

param([Parameter(Mandatory = $true)][string] $From)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class Dec {
    public static byte[] Read(Bitmap b, out int stride) {
        var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        stride = d.Stride; var buf = new byte[d.Stride * b.Height];
        Marshal.Copy(d.Scan0, buf, 0, buf.Length); b.UnlockBits(d); return buf;
    }
    public static Bitmap Make(byte[] buf, int w, int h, int stride) {
        var b = new Bitmap(w, h, PixelFormat.Format32bppArgb);
        var d = b.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        for (int y = 0; y < h; y++) Marshal.Copy(buf, y * stride, d.Scan0 + y * d.Stride, w * 4);
        b.UnlockBits(d); return b;
    }
    // bounding box of alpha > t
    public static int[] Box(Bitmap b, int t) {
        int s; var p = Read(b, out s); int x0 = b.Width, y0 = b.Height, x1 = -1, y1 = -1;
        for (int y = 0; y < b.Height; y++) for (int x = 0; x < b.Width; x++)
            if (p[y * s + x * 4 + 3] > t) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
        return new int[] { x0, y0, x1, y1 };
    }
    // largest distance of the drawing from the canvas centre, per axis: the half-size of a centred crop
    public static int CentredHalf(Bitmap b, int t) {
        var r = Box(b, t); double cx = b.Width / 2.0, cy = b.Height / 2.0;
        double h = Math.Max(Math.Max(cx - r[0], r[2] + 1 - cx), Math.Max(cy - r[1], r[3] + 1 - cy));
        return (int)Math.Ceiling(h);
    }
    public static void Alpha(Bitmap b, double k, int x0, int y0, int x1, int y1) {
        int s; var p = Read(b, out s);
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) { int i = y * s + x * 4 + 3; p[i] = (byte)Math.Min(255, Math.Round(p[i] * k)); }
        var n = Make(p, b.Width, b.Height, s); using (var g = Graphics.FromImage(b)) { g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy; g.DrawImage(n, 0, 0); } n.Dispose();
    }
    // remove a faint halo: alpha below t to 0, the rest stretched back to 0..255
    public static void Dehalo(Bitmap b, int t) {
        int s; var p = Read(b, out s);
        for (int i = 3; i < p.Length; i += 4) { int a = p[i]; p[i] = (byte)(a <= t ? 0 : Math.Min(255, (a - t) * 255 / (255 - t))); }
        var n = Make(p, b.Width, b.Height, s); using (var g = Graphics.FromImage(b)) { g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy; g.DrawImage(n, 0, 0); } n.Dispose();
    }
    // arm: x of the pivot and roller bosses = centres of the column runs that are tallest near each end
    public static double[] ArmEnds(Bitmap b) {
        int s; var p = Read(b, out s); int w = b.Width, h = b.Height;
        var ext = new int[w]; var mid = new double[w];
        for (int x = 0; x < w; x++) { int a = -1, z = -1; for (int y = 0; y < h; y++) if (p[y * s + x * 4 + 3] > 200) { if (a < 0) a = y; z = y; } ext[x] = a < 0 ? 0 : z - a + 1; mid[x] = a < 0 ? 0 : (a + z) / 2.0; }
        int l = 0; while (l < w && ext[l] == 0) l++; int r = w - 1; while (r > 0 && ext[r] == 0) r--;
        int span = r - l, q = span / 3;
        double[] c = new double[2]; double my = 0;
        for (int side = 0; side < 2; side++) {
            int a = side == 0 ? l : r - q, z = side == 0 ? l + q : r;
            int mx = 0; for (int x = a; x <= z; x++) mx = Math.Max(mx, ext[x]);
            double sx = 0; int n = 0; for (int x = a; x <= z; x++) if (ext[x] >= 0.85 * mx) { sx += x; my += mid[x]; n++; }
            c[side] = sx / n; my /= 1;
        }
        int nn = 0; double yy = 0; for (int x = l; x <= r; x++) if (ext[x] > 0) { yy += mid[x]; nn++; }
        return new double[] { c[0], c[1], yy / nn };
    }
    // needle: tip = top of the opaque line; pivot = centre of the rows where it is widest (the boss)
    public static double[] NeedleEnds(Bitmap b) {
        int s; var p = Read(b, out s); int w = b.Width, h = b.Height;
        var wid = new int[h]; int top = -1; double cx = 0; int nc = 0;
        for (int y = 0; y < h; y++) { int a = -1, z = -1; for (int x = 0; x < w; x++) if (p[y * s + x * 4 + 3] > 128) { if (a < 0) a = x; z = x; } wid[y] = a < 0 ? 0 : z - a + 1; if (a >= 0) { if (top < 0) top = y; cx += (a + z) / 2.0; nc++; } }
        int mx = 0; for (int y = 0; y < h; y++) mx = Math.Max(mx, wid[y]);
        double sy = 0; int n = 0; for (int y = 0; y < h; y++) if (wid[y] >= 0.6 * mx) { sy += y; n++; }
        return new double[] { cx / nc, top, sy / n };
    }
    // slider track: the slot = the darkest band of rows, and within it the darkest run of columns
    public static int[] Slot(Bitmap b) {
        int s; var p = Read(b, out s); int w = b.Width, h = b.Height;
        var row = new double[h];
        for (int y = 0; y < h; y++) { double t = 0; for (int x = w / 4; x < 3 * w / 4; x++) { int i = y * s + x * 4; t += p[i] + p[i + 1] + p[i + 2]; } row[y] = t / (w / 2) / 3; }
        double mn = 1e9, md; var sr = (double[])row.Clone(); Array.Sort(sr); md = sr[h / 2]; foreach (var v in row) mn = Math.Min(mn, v);
        double th = mn + 0.35 * (md - mn); int y0 = -1, y1 = -1;
        for (int y = 0; y < h; y++) if (row[y] < th) { if (y0 < 0) y0 = y; y1 = y; }
        var col = new double[w];
        for (int x = 0; x < w; x++) { double t = 0; for (int y = y0; y <= y1; y++) { int i = y * s + x * 4; t += p[i] + p[i + 1] + p[i + 2]; } col[x] = t / (y1 - y0 + 1) / 3; }
        int x0 = -1, x1 = -1; for (int x = 0; x < w; x++) if (col[x] < th) { if (x0 < 0) x0 = x; x1 = x; }
        return new int[] { x0, y0, x1, y1 };
    }
}
"@

$dest = Join-Path (Split-Path -Parent $PSScriptRoot) "assets\decals"
if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Force $dest | Out-Null }
$spec = Get-Content -Raw -Encoding UTF8 (Join-Path (Split-Path -Parent $PSScriptRoot) "docs\decal_parts.json") | ConvertFrom-Json

function Load([string] $name) {
    $p = Join-Path $From ($name + ".png")
    if (-not (Test-Path $p)) { return $null }
    $raw = [System.Drawing.Bitmap]::FromFile($p)
    $b = New-Object System.Drawing.Bitmap $raw.Width, $raw.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($b); $g.DrawImage($raw, 0, 0, $raw.Width, $raw.Height); $g.Dispose(); $raw.Dispose()
    return $b
}
function Save($bmp, [string] $name) { $bmp.Save((Join-Path $dest ($name + ".png")), [System.Drawing.Imaging.ImageFormat]::Png) }
function Crop($bmp, [int] $x, [int] $y, [int] $w, [int] $h) {
    $o = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($o)
    $g.DrawImage($bmp, (New-Object System.Drawing.Rectangle 0, 0, $w, $h), $x, $y, $w, $h, [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose(); return $o
}
function CentredSquare($bmp, [int] $half) {
    $cx = [int][math]::Round($bmp.Width / 2.0); $cy = [int][math]::Round($bmp.Height / 2.0)
    return Crop $bmp ($cx - $half) ($cy - $half) (2 * $half) (2 * $half)
}
function MirrorTile($bmp) {
    $w = $bmp.Width; $h = $bmp.Height
    $big = New-Object System.Drawing.Bitmap (2 * $w), (2 * $h), ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($big)
    $g.DrawImage($bmp, 0, 0, $w, $h)
    $fx = $bmp.Clone(); $fx.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipX); $g.DrawImage($fx, $w, 0, $w, $h)
    $fy = $bmp.Clone(); $fy.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY); $g.DrawImage($fy, 0, $h, $w, $h)
    $fxy = $bmp.Clone(); $fxy.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipXY); $g.DrawImage($fxy, $w, $h, $w, $h)
    $g.Dispose(); $fx.Dispose(); $fy.Dispose(); $fxy.Dispose()
    return $big
}

$done = @{}
$report = @()
foreach ($q in $spec.parts) {
    $n = $q.file
    $b = Load $n
    if ($null -eq $b) { $report += ("{0,-20} missing" -f $n); continue }
    $note = "copied"
    switch -Regex ($n) {
        '^mt-(reel-flange|reel-hub|capstan|pinch|roller|screw|knob)$' {
            $half = [Dec]::CentredHalf($b, 8); $o = CentredSquare $b $half; $b.Dispose(); $b = $o
            $note = "centred square " + (2 * $half) + " px"
        }
        '^mt-lamp-(amber|red|green)-(off|on)$' {
            $off = Load ($n -replace '-on$', '-off')
            $half = [Dec]::CentredHalf($off, 8); $off.Dispose()
            $o = CentredSquare $b $half; $b.Dispose(); $b = $o
            $note = "centred square " + (2 * $half) + " px (the OFF lamp's)"
        }
        '^mt-arm$' {
            $e = [Dec]::ArmEnds($b)
            $o = New-Object System.Drawing.Bitmap 1024, 1024, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $g = [System.Drawing.Graphics]::FromImage($o); $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $s = 448.0 / ($e[1] - $e[0])
            $g.TranslateTransform(512.0 - $e[0] * $s, 512.0 - $e[2] * $s); $g.ScaleTransform($s, $s)
            $g.DrawImage($b, 0, 0, $b.Width, $b.Height); $g.Dispose(); $b.Dispose(); $b = $o
            $note = ("re-seated: pivot x {0:N0}, roller x {1:N0}, axis y {2:N0} -> 512 / 960 / 512" -f $e[0], $e[1], $e[2])
        }
        '^mt-vu-needle$' {
            [Dec]::Dehalo($b, 56)
            $e = [Dec]::NeedleEnds($b)
            $o = New-Object System.Drawing.Bitmap 1024, 1536, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $g = [System.Drawing.Graphics]::FromImage($o); $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $s = 1460.0 / ($e[2] - $e[1])
            $g.TranslateTransform(512.0 - $e[0] * $s, 1500.0 - $e[2] * $s); $g.ScaleTransform($s, $s)
            $g.DrawImage($b, 0, 0, $b.Width, $b.Height); $g.Dispose(); $b.Dispose(); $b = $o
            $note = ("halo removed; re-seated: tip y {0:N0}, pivot y {1:N0} -> 40 / 1500" -f $e[1], $e[2])
        }
        '^mt-(head-erase|head-record|head-play|slider-thumb|carriage)$' {
            if ($n -ne "mt-carriage") { [Dec]::Dehalo($b, 56) }
            $r = [Dec]::Box($b, 8); $o = Crop $b $r[0] $r[1] ($r[2] - $r[0] + 1) ($r[3] - $r[1] + 1); $b.Dispose(); $b = $o
            $note = "cropped to the drawing, " + $b.Width + " x " + $b.Height + $(if ($n -ne "mt-carriage") { ", halo removed" } else { "" })
        }
        '^mt-rail$' { [Dec]::Dehalo($b, 56); $note = "halo removed" }
        '^mt-glass$' { [Dec]::Alpha($b, 64.0 / 197.0, 0, 0, $b.Width, $b.Height); $note = "alpha x 0.32 (centre 192 -> about 62)" }
        '^mt-looptank$' {
            $in = [int]($b.Width * 0.12)
            [Dec]::Alpha($b, 90.0 / 235.0, $in, $in, $b.Width - $in, $b.Height - $in)
            $note = "glass inside a " + $in + " px frame: alpha x 0.38 (235 -> about 90)"
        }
        '^mt-plate$' { $o = MirrorTile $b; $b.Dispose(); $b = $o; $note = "mirror-tiled 2 x 2 (seamless)" }
        '^mt-headplate$' {
            $c = Crop $b 0 0 ([int]($b.Width * 0.42)) ([int]($b.Height * 0.42)); $o = MirrorTile $c; $c.Dispose(); $b.Dispose(); $b = $o
            $note = "upper-left corner (away from the centre swirl), mirror-tiled 2 x 2"
        }
        '^mt-slider-track$' {
            $r = [Dec]::Slot($b); $m = 40
            $x0 = [math]::Max(0, $r[0] - $m); $y0 = [math]::Max(0, $r[1] - $m)
            $x1 = [math]::Min($b.Width - 1, $r[2] + $m); $y1 = [math]::Min($b.Height - 1, $r[3] + $m)
            $o = Crop $b $x0 $y0 ($x1 - $x0 + 1) ($y1 - $y0 + 1); $b.Dispose(); $b = $o
            $note = ("cropped to the slot {0},{1} - {2},{3} with a {4} px margin" -f $r[0], $r[1], $r[2], $r[3], $m)
        }
    }
    # scale to about three times the largest size it is drawn at (a high-DPI screen with the
    # window enlarged half as much again), so the plug-in carries no art it can never show.
    # Largest drawn size in px, from __MT.geom(): knob 58, lamp 10, screw 9, roller 15,
    # capstan 12, pinch 30, head 28, carriage 30, thumb 20, needle 60, arm 55 (on a canvas
    # twice its length), plaque 112, hub 141, flange 327, loop tank 273, wordmark 250,
    # textures tiled at 320. Nine-slice parts scale by exactly one quarter, so their border
    # widths in the page stay simple (160 -> 40).
    $lim = 256
    switch -Regex ($n) {
        '^mt-(deck|steel|plate|header|headplate|reel-flange)$' { $lim = 768 }
        '^mt-(reel-hub|plaque-left|plaque-right|wordmark)$' { $lim = 512 }
        '^mt-looptank$' { $lim = 640 }
        '^mt-(knob|arm|vu-needle)$' { $lim = 256 }
        '^mt-(pinch|head-erase|head-record|head-play|carriage)$' { $lim = 160 }
        '^mt-(roller|slider-thumb)$' { $lim = 96 }
        '^mt-(capstan|screw|lamp-.*)$' { $lim = 64 }
        '^mt-(key-|button-|glass|vu-face|slider-track)' { $lim = [int][math]::Ceiling([math]::Max($b.Width, $b.Height) / 4.0) }
    }
    $big = [math]::Max($b.Width, $b.Height)
    if ($big -gt $lim) {
        $k = $lim / $big; $w2 = [int][math]::Round($b.Width * $k); $h2 = [int][math]::Round($b.Height * $k)
        $o = New-Object System.Drawing.Bitmap $w2, $h2, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($o)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $att = New-Object System.Drawing.Imaging.ImageAttributes; $att.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
        $g.DrawImage($b, (New-Object System.Drawing.Rectangle 0, 0, $w2, $h2), 0, 0, $b.Width, $b.Height, [System.Drawing.GraphicsUnit]::Pixel, $att)
        $g.Dispose(); $b.Dispose(); $b = $o
    }
    Save $b $n
    $report += ("{0,-20} {1,5} x {2,-5} {3}" -f $n, $b.Width, $b.Height, $note)
    $b.Dispose()
}
$report
""
"written to " + $dest
