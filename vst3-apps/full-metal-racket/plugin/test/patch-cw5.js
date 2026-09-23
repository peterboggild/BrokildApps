/*  CLONE WARS BUGLIST 5 — the patina dirt reads as panel discoloration.

    Peter is right, and the cause is exactly where you would expect. The
    photographic dirt is the ONLY layer on the patina canvas drawn with no
    blending at all: the procedural grime uses `mix-blend-mode: overlay` and
    low-alpha blacks, and the damage decals are meant to look like exposed
    material. The dirt photos are grey concrete, and grey laid flat on a
    cobalt hull does not read as "dirt on the panel" — it reads as "the panel
    is a different colour there".

    The embed pipeline cannot fix it either: `process()` in embed-decals.py
    only desaturates BLUE-dominant pixels, so a neutral grey passes through
    untouched by construction.

    So the dirt now keeps its LUMINANCE — the texture, which is the whole
    value of a photograph — and takes the hull's HUE. Per decal, in an
    offscreen buffer: draw the photo, composite the hull colour over it in
    "color" mode (hue + saturation from the fill, lightness from beneath),
    then restore the photo's own alpha mask with "destination-in". Dirt that
    belongs to the metal it is sitting on.

    Not applied to the damage decals: a scratch showing bare material SHOULD
    break the hull colour, which is what makes it read as damage rather than
    as more dirt.

    House rule: this goes into plugin/Source/ui/ui.html AND mockup/index.html,
    which the audit confirmed are byte-identical through the whole patina
    section.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const BASE = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/clone-wars/";
const FILES = [BASE + "plugin/Source/ui/ui.html", BASE + "mockup/index.html"];

const A = [
"function drawImgDecal(img, x, y, size, rot, alpha) {",
"  if (!img.width) return;",
"  const s = size / Math.max(img.width, img.height);",
"  pctx.save();",
"  pctx.translate(x, y); pctx.rotate(rot);",
"  pctx.globalAlpha = alpha;",
"  pctx.scale(s, s);",
"  pctx.drawImage(img, -img.width / 2, -img.height / 2);",
"  pctx.restore();",
"  pctx.globalAlpha = 1;",
"}"].join(NLo);

const B = [
"function drawImgDecal(img, x, y, size, rot, alpha) {",
"  if (!img.width) return;",
"  const s = size / Math.max(img.width, img.height);",
"  pctx.save();",
"  pctx.translate(x, y); pctx.rotate(rot);",
"  pctx.globalAlpha = alpha;",
"  pctx.scale(s, s);",
"  pctx.drawImage(img, -img.width / 2, -img.height / 2);",
"  pctx.restore();",
"  pctx.globalAlpha = 1;",
"}",
"",
"/*  Dirt has to belong to the metal it is sitting on. The dirt photos are grey",
"    concrete, and grey laid flat on a cobalt hull reads as the panel being a",
"    different colour rather than as grime on it — which is exactly the report.",
"    (The embed pipeline cannot help: it only desaturates blue-dominant pixels,",
"    so a neutral grey passes through untouched by construction.)",
"",
"    So: keep the photograph's LUMINANCE, which is the texture and the whole",
"    reason to use a photograph, and take the hull's HUE. Done per decal in an",
"    offscreen buffer, because the patina canvas also carries the damage decals",
"    and the procedural grime, and those must not be tinted — a scratch showing",
"    bare material SHOULD break the hull colour. */",
"const TINTBUF = document.createElement(\"canvas\");",
"const tctx = TINTBUF.getContext(\"2d\");",
"function hullTint() {",
"  const v = getComputedStyle(document.documentElement).getPropertyValue(\"--hull\");",
"  return (v || \"\").trim() || \"#33506a\";",
"}",
"function drawDirtDecal(img, x, y, size, rot, alpha) {",
"  if (!img.width) return;",
"  TINTBUF.width = img.width; TINTBUF.height = img.height;",
"  tctx.clearRect(0, 0, img.width, img.height);",
"  tctx.globalCompositeOperation = \"source-over\";",
"  tctx.globalAlpha = 1;",
"  tctx.drawImage(img, 0, 0);",
"  //  hue and saturation from the hull, lightness from the photograph",
"  tctx.globalCompositeOperation = \"color\";",
"  tctx.globalAlpha = 0.82;",
"  tctx.fillStyle = hullTint();",
"  tctx.fillRect(0, 0, img.width, img.height);",
"  //  the fill covered the transparent margins too - put the mask back",
"  tctx.globalCompositeOperation = \"destination-in\";",
"  tctx.globalAlpha = 1;",
"  tctx.drawImage(img, 0, 0);",
"  tctx.globalCompositeOperation = \"source-over\";",
"  drawImgDecal(TINTBUF, x, y, size, rot, alpha);",
"}"].join(NLo);

const A2 = [
"      const d = dirt[Math.floor(rng() * dirt.length)];",
"      drawImgDecal(d.img, x, y, 60 + rng() * 140, rng() * 6.2832,",
"                   0.30 + rng() * 0.30);"].join(NLo);
const B2 = [
"      const d = dirt[Math.floor(rng() * dirt.length)];",
"      drawDirtDecal(d.img, x, y, 60 + rng() * 140, rng() * 6.2832,",
"                    0.30 + rng() * 0.30);"].join(NLo);

let bad = 0;
for (const F of FILES) {
  let s = fs.readFileSync(F, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
  for (const [a, b, tag] of [[A, B, "helper"], [A2, B2, "dirt call"]]) {
    const X = a.split(NLo).join(NL), Y = b.split(NLo).join(NL);
    const n = s.split(X).length - 1;
    if (n !== 1) { console.error("MISS in " + F.split("/").pop() + ": " + tag + " x" + n); bad++; continue; }
    s = s.split(X).join(Y);
  }
  if (!bad) fs.writeFileSync(F, s);
}
if (bad) process.exit(1);
console.log("CW5: dirt keeps its texture and takes the hull's hue (both files)");
