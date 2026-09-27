/*  What is actually drawn across the header. Measure, do not guess. */
"use strict";
const fs = require("fs"), path = require("path"), cp = require("child_process");
const ROOT = path.resolve(__dirname, "..");
const SCR = process.env.SCRATCH || path.join(process.env.TEMP || ".", "hightide-uiprobe");
const CHROME = "C:/Program Files/Google/Chrome/Application/chrome.exe";
fs.mkdirSync(SCR, { recursive: true });

const PROBE = `
<script>
setTimeout(function(){
  var r = {};
  function box(el){ if(!el) return null; var b = el.getBoundingClientRect();
    return {x:Math.round(b.left), y:Math.round(b.top), w:Math.round(b.width), h:Math.round(b.height)}; }
  function pseudo(sel, which){ var el = document.querySelector(sel); if(!el) return null;
    var c = getComputedStyle(el, which);
    return {content:c.content, h:c.height, w:c.width, top:c.top, bottom:c.bottom,
            bg:(c.backgroundImage||"").slice(0,60), size:c.backgroundSize, repeat:c.backgroundRepeat, pos:c.position}; }
  r.hdr = box(document.getElementById("hdr"));
  r.hdrAfter = pseudo("#hdr", "::after");
  r.tlBefore = pseudo("#tl", "::before");
  r.plate = box(document.getElementById("plate"));
  var np = document.querySelector("#plate .np");
  r.np = box(np);
  if (np) { var c = getComputedStyle(np); r.npStyle = {bg:(c.backgroundImage||"").slice(0,60), size:c.backgroundSize, color:c.color, w:c.width, h:c.height}; }
  r.classes = document.body.className;
  r.imgs = {};
  var D = __HT.decals();
  r.decals = D;
  ["pearl","tack","flag","bezel","nameplate","glass","wood"].forEach(function(n){
    var i = new Image(); i.src = "decals/ht-"+n+".png";
    r.imgs[n] = "pending";
  });
  var pre = document.createElement("pre"); pre.id = "probe"; pre.textContent = JSON.stringify(r, null, 1); document.body.appendChild(pre);
}, 1800);
</script>`;

const src = fs.readFileSync(path.join(ROOT, "Source", "ui", "ui.html"), "utf8");
const copy = path.join(SCR, "ui-hdr.html");
fs.writeFileSync(copy, src.replace(/<\/body>/, PROBE + "\n</body>"), "utf8");
const url = "file:///" + copy.replace(/\\/g, "/");
let dom = "";
try {
  dom = cp.execFileSync(CHROME, ["--headless=new", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
    "--no-sandbox", "--window-size=1240,820", "--hide-scrollbars", "--virtual-time-budget=6000",
    "--user-data-dir=" + path.join(SCR, "profile"), "--dump-dom", url],
    { encoding: "utf8", maxBuffer: 64 << 20, timeout: 120000 });
} catch (e) { dom = (e.stdout || "") + ""; }
const m = dom.match(/<pre id="probe">([\s\S]*?)<\/pre>/);
if (!m) { console.log("no result"); process.exit(2); }
console.log(m[1].replace(/&quot;/g, '"').replace(/&amp;/g, "&").replace(/&lt;/g, "<").replace(/&gt;/g, ">"));
