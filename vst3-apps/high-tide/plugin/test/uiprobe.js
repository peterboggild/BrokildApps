/*  HIGH TIDE — the panel probe.

    Static checks cannot see a page that executes nothing (a missing
    </script> once passed every one of them). This loads Source/ui/ui.html in
    headless Chrome with a probe appended to a COPY, drives it through the
    __HT hooks and real pointer events, and prints PASS/FAIL per check.

        node test/uiprobe.js [--shot out.png]
*/
"use strict";
const fs = require("fs"), path = require("path"), cp = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const UI = path.join(ROOT, "Source", "ui", "ui.html");
const BWFX = "C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js";
const SCR = process.env.SCRATCH || path.join(process.env.TEMP || ".", "hightide-uiprobe");
const CHROME = "C:/Program Files/Google/Chrome/Application/chrome.exe";
fs.mkdirSync(SCR, { recursive: true });

const PROBE = `
<script>
setTimeout(function(){
  var r = {};
  function step(name, fn){ try { r[name] = fn(); } catch (e) { r[name] = "EXC " + (e && e.message); } }
  step("draw", function(){ for (var i=0;i<3;i++) __HT.draw(); return true; });
  step("err", function(){ return window.__err; });
  step("state", function(){ var s = __HT.state(); return {gl:s.gl, glErr:s.glErr, glLastErr:s.glLastErr, live:s.live, voices:s.voices.length, build:s.build}; });
  step("specs", function(){ var P = __HT.specs(); var ids = Object.keys(P); var missing = ids.filter(function(id){ return !document.querySelector('.ctl[data-id="'+id+'"]'); }); return {n:ids.length, missing:missing}; });
  step("rows", function(){ return __HT.tl.layout().rows.map(function(x){ return x.id; }); });
  step("pins", function(){
    var c = document.getElementById("tlc");
    function ev(type, x, y, btn){ var e = new PointerEvent(type, {bubbles:true, cancelable:true, clientX:x, clientY:y, button:btn||0, buttons: btn===2?2:(type==="pointerup"?0:1), pointerId:1, pointerType:"mouse", isPrimary:true}); c.dispatchEvent(e); document.dispatchEvent(e); }
    var before = __HT.lanes().pin.on.length;
    var p = __HT.tl.pt("pin", 5.3, 0.15, "on");     // an empty spot on the mock's lane
    ev("pointerdown", p.x, p.y, 0); ev("pointerup", p.x, p.y, 0);
    var afterAdd = __HT.lanes().pin.on.length;
    var pt = __HT.lanes().pin.on.slice().sort(function(a,b){return Math.abs(a.t-5.3)-Math.abs(b.t-5.3);})[0];
    var t0 = pt ? pt.t : null;
    var q = __HT.tl.pt("pin", t0||5.3, pt?pt.v:0.15, "on");
    ev("pointerdown", q.x, q.y, 0); ev("pointermove", q.x+60, q.y-20, 0); ev("pointerup", q.x+60, q.y-20, 0);
    var pt2 = __HT.lanes().pin.on.slice().sort(function(a,b){return Math.abs(a.t-(t0||1.2))-Math.abs(b.t-(t0||1.2));});
    var moved = pt2.some(function(a){ return Math.abs(a.t-(t0||0)) > 0.05; });
    var n2 = __HT.lanes().pin.on.length;
    var last = __HT.lanes().pin.on[__HT.lanes().pin.on.length-1];
    var w = __HT.tl.pt("pin", last.t, last.v, "on");
    ev("pointerdown", w.x, w.y, 2); ev("pointerup", w.x, w.y, 2);
    var afterDel = __HT.lanes().pin.on.length;
    return {before:before, afterAdd:afterAdd, moved:moved, afterDel:afterDel};
  });
  step("sculpt", function(){
    __HT.generate("plain", true);
    var U = __HT.terrain(); var NX = __HT.NX, NZ = __HT.NZ;
    var j = Math.round(0.5*(NZ-1));
    var before = 0; for (var i=0;i<NX;i++) before += U[j*NX+i];
    __HT.lock(true);
    __HT.sculpt("push", -0.75, 0.5, 1.0);
    var after = 0; for (var i2=0;i2<NX;i2++) after += U[j*NX+i2];
    var worst = 0, ws = [];
    for (var k=1;k<=5;k++){ var h = 0.08*k; var w = __HT.colWidth(j, h); var ref = 2*Math.sqrt(2*h); ws.push(+(w/ref).toFixed(4)); worst = Math.max(worst, Math.abs(w/ref-1)); }
    return {changed: Math.abs(after-before) > 1e-3, widthRatios: ws, worstDeparture: +worst.toFixed(4)};
  });
  step("water", function(){
    //  while voices sound, the water follows THEIR tide; with none, the knob
    if (__HT.mock) __HT.mock.stop();
    __HT.balls({t:0, lvl:0, v:[]});
    __HT.setParam("tide", 0.9, true); __HT.draw(); var s = __HT.state(); return {waterT:s.waterT, Tabs:s.Tabs, Rmin:s.Rmin, Rmax:s.Rmax}; });
  step("mock", function(){ var s = __HT.state(); return {voicesSeen:s.wakeN > 0, wakeN:s.wakeN, ledger:__HT.ledger()}; });
  step("tips", function(){
    var T = __HT.tips(), missing = [], n = 0;
    function need(sel, key){ Array.prototype.forEach.call(document.querySelectorAll(sel), function(e){
      var k = key(e); n++; if (!T[k]) missing.push(k); }); }
    need(".ctl[data-id]", function(e){ return e.dataset.id; });
    need("[data-tool]", function(e){ return "tool:" + e.dataset.tool; });
    need("[data-stamp]", function(e){ return "stamp:" + e.dataset.stamp; });
    need("#hdr .hb", function(e){ return e.id === "b-generate" ? "ui:generate" : "btn:" + e.id.slice(2); });
    ["ui:radius","ui:strength","ui:lock","ui:generate","ui:undo","ui:redo","btn:globe","btn:home","ui:view","ui:patchname","ui:kbd"]
      .forEach(function(k){ n++; if (!T[k]) missing.push(k); });
    __HT.tl.layout();
    ["amp","mod","lfo1","lfo2","tide","rock","pin"].forEach(function(id){ n++; if (!T["lane:"+id]) missing.push("lane:"+id); });
    return {checked:n, missing:missing, total:Object.keys(T).length};
  });
  step("tipPlace", function(){
    var out = {};
    [["tone","#rail-r"],["tool:push","#rail-l"],["btn:factory","#hdr"]].forEach(function(pair){
      var key = pair[0];
      var node = key.indexOf("tool:") === 0 ? document.querySelector('[data-tool="push"]')
               : key.indexOf("btn:") === 0 ? document.getElementById("b-factory")
               : document.querySelector('.ctl[data-id="' + key + '"]');
      var r = node.getBoundingClientRect();
      var t = __HT.tipAt(key, node);
      var overlap = !(t.x + t.w <= r.left || t.x >= r.right || t.y + t.h <= r.top || t.y >= r.bottom);
      var onScreen = t.x >= 0 && t.y >= 0 && t.x + t.w <= innerWidth + 1 && t.y + t.h <= innerHeight + 1;
      out[key] = {on:t.on, overlap:overlap, onScreen:onScreen, hasHow: t.html.indexOf("th") > 0};
    });
    __HT.tipHide();
    return out;
  });
  step("glAfter", function(){ __HT.draw(); var gl = __HT.gl(); return gl ? gl.getError() : "no gl"; });
  var pre = document.createElement("pre"); pre.id = "probe"; pre.textContent = JSON.stringify(r); document.body.appendChild(pre);
}, 1800);
</script>`;

const src = fs.readFileSync(UI, "utf8");
if (!/<\/script>\s*<\/body>/.test(src)) console.log("WARN: the page does not end with </script></body>");
const out = src.replace(/<\/body>/, PROBE + "\n</body>");
const copy = path.join(SCR, "ui-probe.html");
fs.writeFileSync(copy, out, "utf8");
fs.copyFileSync(BWFX, path.join(SCR, "bwfx-rack.js"));
//  the panel loads its decals from decals/ beside the page
const DEC = path.join(ROOT, "Source", "ui", "decals");
if (fs.existsSync(DEC)) {
    const to = path.join(SCR, "decals");
    fs.mkdirSync(to, { recursive: true });
    for (const n of fs.readdirSync(DEC)) fs.copyFileSync(path.join(DEC, n), path.join(to, n));
}

const shotIdx = process.argv.indexOf("--shot");
const shot = shotIdx > 0 ? path.resolve(process.argv[shotIdx + 1]) : null;
const url = "file:///" + copy.replace(/\\/g, "/");
const common = ["--headless=new", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--no-sandbox",
                "--disable-gpu-sandbox", "--window-size=1240,820", "--hide-scrollbars", "--virtual-time-budget=6000",
                "--user-data-dir=" + path.join(SCR, "profile")];
let dom = "";
try {
    dom = cp.execFileSync(CHROME, common.concat(["--dump-dom", url]), { encoding: "utf8", maxBuffer: 64 << 20, timeout: 120000 });
} catch (e) { dom = (e.stdout || "") + ""; if (!dom) { console.log("chrome failed: " + e.message); process.exit(2); } }
const m = dom.match(/<pre id="probe">([\s\S]*?)<\/pre>/);
if (!m) { console.log("no probe result in the DOM (" + dom.length + " bytes)"); console.log(dom.slice(0, 400)); process.exit(2); }
const r = JSON.parse(m[1].replace(/&quot;/g, '"').replace(/&amp;/g, "&").replace(/&lt;/g, "<").replace(/&gt;/g, ">"));

let fails = 0;
function check(c, what, detail) { console.log((c ? "  PASS  " : "  FAIL  ") + what + (detail !== undefined ? "   " + JSON.stringify(detail) : "")); if (!c) fails++; }
check(r.err === null, "no JS error", r.err);
check(r.state && r.state.gl === true, "WebGL2 context created", r.state);
check(r.glAfter === 0, "gl.getError() == 0 after a draw", r.glAfter);
check(r.specs && r.specs.n === 46 && r.specs.missing.length === 0, "all 46 parameter controls present", r.specs);
check(r.tips && Array.isArray(r.tips.missing) && r.tips.missing.length === 0, "every control, button, tool and lane has a hint", r.tips);
check(r.tipPlace && typeof r.tipPlace === "object" && Object.keys(r.tipPlace).length === 3 && Object.keys(r.tipPlace).every(function (k) { return r.tipPlace[k].on && !r.tipPlace[k].overlap && r.tipPlace[k].onScreen && r.tipPlace[k].hasHow; }),
      "the hint appears beside its control, on screen, and says how to use it", r.tipPlace);
check(Array.isArray(r.rows) && r.rows.join(",") === "amp,mod,lfo1,lfo2,tide,rock,pin", "7 timeline lanes in order", r.rows);
check(r.pins && r.pins.afterAdd === r.pins.before + 1, "click adds a pin", r.pins);
check(r.pins && r.pins.moved === true, "drag moves a pin", r.pins);
check(r.pins && r.pins.afterDel === r.pins.afterAdd - 1, "right-click removes a pin", r.pins);
check(r.sculpt && r.sculpt.changed === true, "a sculpt stroke changes the terrain", r.sculpt);
check(r.sculpt && r.sculpt.worstDeparture < 0.03, "TUNE LOCK keeps the width function within 3 %", r.sculpt);
check(r.water && Math.abs(r.water.waterT - 0.9) < 0.01, "TIDE moves the water", r.water);
check(r.mock && r.mock.voicesSeen === true, "the mock backend is alive (a wake was recorded)", r.mock);
console.log(fails ? "\n" + fails + " FAILED" : "\nALL PASS");

if (shot) {
    try { cp.execFileSync("taskkill", ["/F", "/IM", "chrome.exe"], { stdio: "ignore" }); } catch (e) {}
    const plain = path.join(SCR, "ui-shot.html");
    fs.writeFileSync(plain, src, "utf8");
    try {
        cp.execFileSync(CHROME, common.concat(["--screenshot=" + shot, "file:///" + plain.replace(/\\/g, "/")]), { stdio: "ignore", timeout: 120000 });
        console.log("screenshot: " + shot);
    } catch (e) { console.log("screenshot failed: " + e.message); }
}
process.exit(fails ? 1 : 0);
