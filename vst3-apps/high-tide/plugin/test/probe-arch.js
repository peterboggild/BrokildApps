/*  What element is at the arch, and where do the canvases actually sit. */
"use strict";
const fs = require("fs"), path = require("path"), cp = require("child_process");
const ROOT = path.resolve(__dirname, "..");
const SCR = process.env.SCRATCH || path.join(process.env.TEMP || ".", "hightide-uiprobe");
const CHROME = "C:/Program Files/Google/Chrome/Application/chrome.exe";

const PROBE = `
<script>
setTimeout(function(){
  var r = {pts:{}, boxes:{}};
  function box(id){ var e=document.getElementById(id); if(!e) return null; var b=e.getBoundingClientRect();
    var c=getComputedStyle(e);
    return {x:Math.round(b.left),y:Math.round(b.top),w:Math.round(b.width),h:Math.round(b.height),
            pos:c.position, z:c.zIndex, ov:c.overflow}; }
  ["hdr","view","gl","ov","tl","tlc","plate","deck","main"].forEach(function(id){ r.boxes[id]=box(id); });
  [[460,20],[560,14],[300,44],[700,25],[100,20]].forEach(function(p){
    var e = document.elementFromPoint(p[0],p[1]);
    r.pts[p[0]+","+p[1]] = e ? (e.tagName+"#"+(e.id||"")+"."+(e.className||"").toString().slice(0,40)) : "none";
  });
  var ov=document.getElementById("ov");
  if(ov){ r.ovAttr={width:ov.width,height:ov.height}; }
  var gl=document.getElementById("gl");
  if(gl){ r.glAttr={width:gl.width,height:gl.height}; }
  var pre=document.createElement("pre"); pre.id="probe"; pre.textContent=JSON.stringify(r,null,1); document.body.appendChild(pre);
}, 1800);
</script>`;

const src = fs.readFileSync(path.join(ROOT, "Source", "ui", "ui.html"), "utf8");
const copy = path.join(SCR, "ui-arch.html");
fs.writeFileSync(copy, src.replace(/<\/body>/, PROBE + "\n</body>"), "utf8");
let dom = "";
try {
  dom = cp.execFileSync(CHROME, ["--headless=new", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
    "--no-sandbox", "--window-size=1240,820", "--hide-scrollbars", "--virtual-time-budget=6000",
    "--user-data-dir=" + path.join(SCR, "profile"), "--dump-dom", "file:///" + copy.replace(/\\/g, "/")],
    { encoding: "utf8", maxBuffer: 64 << 20, timeout: 120000 });
} catch (e) { dom = (e.stdout || "") + ""; }
const m = dom.match(/<pre id="probe">([\s\S]*?)<\/pre>/);
console.log(m ? m[1].replace(/&quot;/g, '"').replace(/&amp;/g, "&").replace(/&lt;/g, "<").replace(/&gt;/g, ">") : "no result");
