/*  THIN WALLS - render the real panel page to a PNG.
      node test/uishot.js <out.png> [--size=1400x900] [--setup="<js>"]

    Loads Source/ui/ui.html in headless Chrome with the native side stubbed,
    fires an initialState and a scene exactly as native would, runs the setup
    expression (which can drive window.__TW), renders synchronously and shoots.

    The PNG is the point: the probe can say that pixels changed, but only a
    picture can say whether the thing in the room looks like a loudspeaker.
*/
"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const cp = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const UI = path.join(ROOT, "Source", "ui", "ui.html");
const CHROME = "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";

const argv = process.argv.slice(2);
const OUT = path.resolve(argv.find(a => !a.startsWith("--")) || "ui-shot.png");
const SZ = (argv.find(a => a.startsWith("--size=")) || "--size=1400x900").split("=")[1];
const W = parseInt(SZ.split("x")[0], 10) || 1400;
const H = parseInt(SZ.split("x")[1], 10) || 900;
const SETUP = (argv.find(a => a.startsWith("--setup=")) || "--setup=").slice(8);

/*  The v2 defaults, protocol section 3. Kept in real units and converted
    here, so a number in this file can be checked against the document by
    eye rather than by trusting an already-normalised constant. */
const INPUTS = ["OFF","MAIN L","MAIN R","MAIN L+R","AUX L","AUX R","AUX L+R"];
const MATS = ["ABSORBING","FURNISHED","PLASTER","TILED","STUDIO"];
const SURF = ["AS WALLS"].concat(MATS);          // the floor and ceiling lists
const ROOMN = ["LARGE","SMALL","GIANT"];
const SRC = [
  { x:3.0,  y:2.5, z:1.2, yaw:0,   type:1, dir:0,   inp:3 },
  { x:1.5,  y:1.0, z:1.2, yaw:45,  type:1, dir:0,   inp:0 },
  { x:4.5,  y:7.0, z:1.2, yaw:270, type:0, dir:0.5, inp:0 },
  { x:12.0, y:4.5, z:1.6, yaw:180, type:0, dir:0,   inp:0 }
];
const PARAMS = [];
SRC.forEach((d, i) => {
  const n = i + 1;
  PARAMS.push({ id:"s"+n+"x", v:d.x/18 }, { id:"s"+n+"y", v:d.y/9 },
              { id:"s"+n+"z", v:(d.z-0.2)/2.2 }, { id:"s"+n+"yaw", v:d.yaw/360 },
              { id:"s"+n+"type", v:d.type, stepped:true, steps:2, choices:"PURE|LOUDSPEAKER" },
              { id:"s"+n+"dir", v:d.dir },
              { id:"s"+n+"in", v:d.inp/6, stepped:true, steps:7, choices:INPUTS.join("|") },
              { id:"s"+n+"lvl", v:0.8 });
});
[["lisx",0.25],["lisy",0.2778],["lisyaw",0.5],["door1",1],["door2",1],["door3",0]]
  .forEach(p => PARAMS.push({ id:p[0], v:p[1] }));
// per room: walls, floor, ceiling, then the two breakable walls
[1/(MATS.length-1), 1/(MATS.length-1), 2/(MATS.length-1)].forEach((wv, i) => {
  const n = i + 1, R = ROOMN[i] + " ";
  PARAMS.push({ id:"mat"+n, name:R+"ROOM WALLS",   v:wv, stepped:true, steps:MATS.length, choices:MATS.join("|") });
  PARAMS.push({ id:"flr"+n, name:R+"ROOM FLOOR",   v:0,  stepped:true, steps:SURF.length, choices:SURF.join("|") });
  PARAMS.push({ id:"cel"+n, name:R+"ROOM CEILING", v:0,  stepped:true, steps:SURF.length, choices:SURF.join("|") });
  ["a","b"].forEach((k, j) => {
    PARAMS.push({ id:"fold"+n+k,  name:R+"FOLD "+k.toUpperCase(),        v:0.5 });
    PARAMS.push({ id:"foldp"+n+k, name:R+"FOLD "+k.toUpperCase()+" POS", v:0.5 });
  });
});
[["direct",0.8],["early",0.8],["reverb",0.8],["mix",1],["output",0.8],["earspan",0.0294]]
  .forEach(p => PARAMS.push({ id:p[0], v:p[1] }));
PARAMS.forEach(p => { if (!p.name) p.name = p.id.toUpperCase(); });

const SCENE = {
  sources: SRC.map((d, i) => ({ pos:[d.x,d.y,d.z], yaw:d.yaw, type:d.type,
                                room:0, active:i === 0 ? 1 : 0, dir:d.dir })),
  lis:[4.5,2.5,1.65], lisRoom:0, lisYaw:180,
  paths:[
    { t:"direct", s:0, pts:[[3.0,2.5,1.2],[4.5,2.5,1.65]], db:0.0, ms:4.4 },
    { t:"r1", s:0, pts:[[3.0,2.5,1.2],[1.2,0.0,1.4],[4.5,2.5,1.65]], db:-8.2, ms:9.9 },
    { t:"r1", s:0, pts:[[3.0,2.5,1.2],[5.4,5.0,1.4],[4.5,2.5,1.65]], db:-9.6, ms:11.2 },
    { t:"r1", s:0, pts:[[3.0,2.5,1.2],[0.0,3.4,1.5],[4.5,2.5,1.65]], db:-11.1, ms:13.0 },
    { t:"r2", s:0, pts:[[3.0,2.5,1.2],[0.0,1.1,1.5],[2.2,0.0,1.5],[4.5,2.5,1.65]], db:-15.4, ms:18.1 },
    { t:"portal", s:0, pts:[[3.0,2.5,1.2],[6.0,2.5,1.1],[9.5,4.2,1.6],[6.0,2.5,1.1],[4.5,2.5,1.65]], db:-13.1, ms:26.0 },
    { t:"leaf", s:0, pts:[[3.0,2.5,1.2],[4.5,5.0,1.0],[4.5,2.5,1.65]], db:-28.7, ms:14.5 },
    { t:"wall", s:0, pts:[[3.0,2.5,1.2],[6.0,4.2,1.2],[4.5,2.5,1.65]], db:-33.2, ms:12.0 },
    { t:"direct", s:1, pts:[[1.5,1.0,1.2],[4.5,2.5,1.65]], db:-3.4, ms:8.1 },
    { t:"portal", s:2, pts:[[4.5,7.0,1.2],[4.5,5.0,1.1],[4.5,2.5,1.65]], db:-17.2, ms:21.0 },
    { t:"portal", s:3, pts:[[12.0,4.5,1.6],[6.0,2.5,1.1],[4.5,2.5,1.65]], db:-22.9, ms:34.0 }
  ],
  rt:[[0.62,0.48,0.38,0.29],[0.41,0.33,0.26,0.20],[2.41,1.96,1.42,0.95]],
  plan:[ [[0,0],[6,0],[6,5],[0,5]], [[3,5],[6,5],[6,8.5],[3,8.5]], [[6,0],[18,0],[18,9],[6,9]] ],
  in:-14.0, out:-16.2, drr:3.8
};

const STUB = "\n<script>\n" +
  "window.__sent=[]; window.__handlers={};\n" +
  "window.__JUCE__={backend:{emitEvent:function(n,o){window.__sent.push({name:n,msg:o});}," +
  "addEventListener:function(n,f){(window.__handlers[n]=window.__handlers[n]||[]).push(f);}}};\n" +
  "window.__fire=function(n,d){(window.__handlers[n]||[]).forEach(function(f){" +
  "try{f(d);}catch(e){window.__err.push('handler '+n+': '+e.message);}});};\n" +
  "</" + "script>\n";

const DRIVER = "\n<script>\n(function(){\n" +
  " window.__fire('initialState', { build:'260921.1', showrays:1, selsrc:0, auxConnected:0," +
  " wav:{name:'',playing:0,seconds:0,gain:0.75}, params:" + JSON.stringify(PARAMS) + " });\n" +
  " window.__fire('scene', " + JSON.stringify(SCENE) + ");\n" +
  " window.__fire('notice', { text:'four sources, one apartment' });\n" +
  " try { " + SETUP + " } catch(e){ window.__err.push('setup: ' + e.message); }\n" +
  " window.__TW.render();\n" +
  " var pre=document.createElement('pre'); pre.id='probe'+'out';\n" +
  " pre.textContent='ERR:'+window.__err.length+(window.__err[0]?(' '+window.__err[0]):'')" +
  "   +'  renderer='+window.__TW.renderer+'  selsrc='+window.__TW.selsrc;\n" +
  " document.body.appendChild(pre);\n" +
  "})();\n</" + "script>\n";

let page = fs.readFileSync(UI, "utf8");
if (page.indexOf("<!-- probe-stub -->") < 0){
  console.error("the page has no probe-stub marker; refusing to guess where the stub goes");
  process.exit(2);
}
page = page.replace("<!-- probe-stub -->", STUB);
const end = page.lastIndexOf("</body>");
page = page.slice(0, end) + DRIVER + page.slice(end);

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "twshot-"));
const file = path.join(tmp, "ui-shot.html");
fs.writeFileSync(file, page, "utf8");
fs.mkdirSync(path.dirname(OUT), { recursive:true });
try { fs.unlinkSync(OUT); } catch(e){}

const r = cp.spawnSync(CHROME, [
  "--headless=new",
  "--screenshot=" + OUT,
  "--window-size=" + W + "," + H,
  "--hide-scrollbars",
  "--virtual-time-budget=6000",
  "--enable-unsafe-swiftshader",
  "--use-angle=swiftshader",
  "--allow-file-access-from-files",
  "--user-data-dir=" + path.join(tmp, "chrome"),
  "--no-first-run",
  "--no-default-browser-check",
  "file:///" + file.replace(/\\/g, "/")
], { encoding:"utf8", maxBuffer: 64 * 1024 * 1024 });

const ok = fs.existsSync(OUT);
console.log((ok ? "wrote " : "FAILED ") + OUT + "  " + W + "x" + H +
            (ok ? ("  " + fs.statSync(OUT).size + " bytes") : ""));
if (!ok && r.stderr) console.error(String(r.stderr).split("\n").slice(0, 8).join("\n"));
try { fs.rmSync(tmp, { recursive:true, force:true }); } catch(e){}
process.exit(ok ? 0 : 1);
