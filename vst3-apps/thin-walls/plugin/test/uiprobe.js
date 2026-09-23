/*  THIN WALLS - headless gate for Source/ui/ui.html
    node test/uiprobe.js  [--keep]

    Loads the real page in headless Chrome with a stubbed JUCE backend, fires
    the messages native would send, drives the views with real pointer and key
    events and reads a checklist back out of the DOM dump.

    Two rules this file exists to honour:
      - a check that cannot fail is worse than no check, so every ink check
        counts OPAQUE pixels that differ from that canvas's own background
        (a fresh canvas is transparent, so "not white" passes on nothing);
      - the parameter count follows the params list, never a typed number.
*/
"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const cp = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const UI = path.join(ROOT, "Source", "ui", "ui.html");
const CHROME = "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";
const KEEP = process.argv.indexOf("--keep") >= 0;
/*  --size 900x600 runs the whole gate at the smallest window the panel has to
    survive; the layout checks are the ones that care. */
const SZARG = (process.argv.find(a => /^--size=/.test(a)) || "--size=1400x900").split("=")[1];
const WIN = { w: parseInt(SZARG.split("x")[0], 10) || 1400, h: parseInt(SZARG.split("x")[1], 10) || 900 };

let pass = 0, fail = 0;
const lines = [];
function chk(ok, name, detail){
  const n = pass + fail + 1;
  if (ok) pass++; else fail++;
  lines.push((ok ? "PASS " : "FAIL ") + String(n).padStart(2, "0") + "  " + name +
             (detail ? "   [" + detail + "]" : ""));
}

/* ------------------------------------------------------------ the page */
if (!fs.existsSync(UI)){ console.error("no ui.html at " + UI); process.exit(2); }
const src = fs.readFileSync(UI, "utf8");

/* ---------------------------------------------------- static checks (node) */
(function staticChecks(){
  /* an external URL anywhere outside an HTML comment */
  const noComments = src.replace(/<!--[\s\S]*?-->/g, "");
  const urls = noComments.match(/https?:\/\//g) || [];
  chk(urls.length === 0, "no external URL in the page", urls.length + " found");
  chk(!/<link\b/i.test(src), "no link element");
  chk(!/<img\b/i.test(src), "no image element");

  /* every script block closes - an unclosed one makes the browser execute
     nothing at all and no static check downstream can see it */
  const opens = (src.match(/<script\b/gi) || []).length;
  const closes = (src.match(/<\/script>/gi) || []).length;
  chk(opens > 0 && opens === closes, "every script tag is closed", opens + " open / " + closes + " close");

  chk(/\[hidden\]\s*\{\s*display\s*:\s*none\s*!important/.test(src),
      "hidden is forced with !important");

  /* the error hook is the FIRST script in the document */
  const firstScript = src.indexOf("<script");
  const errHook = src.indexOf("window.__err");
  chk(errHook > firstScript && errHook < src.indexOf("</script>"),
      "window.__err listener is in the first script block");

  /* node --check on each extracted script block */
  const blocks = [];
  const re = /<script\b[^>]*>([\s\S]*?)<\/script>/gi;
  let m;
  while ((m = re.exec(src))) blocks.push(m[1]);
  let bad = "";
  blocks.forEach((b, i) => {
    const f = path.join(os.tmpdir(), "tw-block-" + i + ".js");
    fs.writeFileSync(f, b, "utf8");
    try { cp.execSync("node --check \"" + f + "\"", { stdio:"pipe" }); }
    catch (e){ bad += "block " + i + ": " + String(e.stderr || e.message).split("\n")[0] + " "; }
    try { fs.unlinkSync(f); } catch (_){}
  });
  chk(bad === "", "node --check on every script block", bad || (blocks.length + " blocks"));
})();

/* ------------------------------------------------------- the driven page */
const STUB = `
<script>
window.__sent = [];
window.__handlers = {};
window.__JUCE__ = { backend: {
  emitEvent: function (name, obj){ window.__sent.push({ name:name, msg:obj }); },
  addEventListener: function (name, fn){
    (window.__handlers[name] = window.__handlers[name] || []).push(fn);
  }
}};
window.__fire = function (name, data){
  (window.__handlers[name] || []).forEach(function (f){ try { f(data); } catch(e){ window.__err.push("handler " + name + ": " + e.message); } });
};
</script>
`;

/*  The parameter list is READ OUT OF PROTOCOL.md, never typed here. A typed
    list is a second source of truth and goes stale the moment the contract
    grows - which it has done twice now, 20 ids to 47 to 65. If the document
    and the page disagree, that is the finding. */
const INPUTS = ["OFF","MAIN L","MAIN R","MAIN L+R","AUX L","AUX R","AUX L+R"];
const PROTO = fs.readFileSync(path.join(ROOT, "PROTOCOL.md"), "utf8");
/*  The material lists, READ OUT OF THE DOCUMENT rather than typed: they grow
    in the engine and a copy here would go stale the day they do. The names
    live in a code span inside the row's choice cell, and the pipes that
    separate them are the same character the table uses for its own columns -
    which is why this is done line by line and not with the table regex below.

    The per-room rows are written once with {n} in the id, so that is the id
    to look for; the row is expanded for n = 1..3 further down. */
function protocolRow(idTpl){
  for (const line of PROTO.split("\n")){
    if (line.indexOf("| `" + idTpl + "`") !== 0) continue;
    const m = /choice\s*`([^`]+)`/.exec(line);
    return { line:line, list: m ? m[1].split("|").map(t => t.trim()) : null };
  }
  throw new Error("PROTOCOL.md: cannot find the " + idTpl + " row");
}
const MATS = protocolRow("mat{n}").list;
const SURF_MATS = protocolRow("flr{n}").list;
/*  The ceiling row names no list of its own - it says it is the floor's. That
    sentence is what is checked, rather than the two being assumed equal: the
    day one of them gains an entry the other does not, this is the line that
    notices. */
const CEL_ROW = protocolRow("cel{n}");
const CEL_MATS = CEL_ROW.list || (/same as the floor/i.test(CEL_ROW.line) ? SURF_MATS : null);
if (!MATS || !SURF_MATS || !CEL_MATS)
  throw new Error("PROTOCOL.md: cannot read one of the three material lists");
chk(SURF_MATS.length === MATS.length + 1 && SURF_MATS[0] === "AS WALLS" &&
    SURF_MATS.slice(1).join("|") === MATS.join("|"),
    "the floor list is the wall list with AS WALLS in front of it",
    "walls " + MATS.length + " [" + MATS.join(",") + "], surfaces " + SURF_MATS.length +
    " [" + SURF_MATS.join(",") + "]");

function protocolParams(){
  const out = [];
  if (!/`s\{n\}x`/.test(PROTO))
    throw new Error("PROTOCOL.md has no s{n}x row - the contract changed shape");
  /*  The per-source defaults are prose in section 3, so they are parsed from
      that sentence rather than copied: "Source 2 at (1.5, 1.0, 1.2) facing
      45 deg, LOUDSPEAKER, OFF." */
  const defs = [];
  const first = /Defaults:\s*source 1 at \(([\d.]+),\s*([\d.]+),\s*([\d.]+)\s*m\)\s*facing\s*([\d.]+)[^,]*,\s*(\w+),\s*([A-Z+ ]+?),\s*0 dB/i.exec(PROTO);
  if (!first) throw new Error("PROTOCOL.md: cannot read the source 1 defaults");
  defs.push({ x:+first[1], y:+first[2], z:+first[3], yaw:+first[4],
              type:first[5].toUpperCase() === "LOUDSPEAKER" ? 1 : 0, dir:0,
              inp:INPUTS.indexOf(first[6].trim()), lvl:0.8 });
  const re2 = /Source ([234]) at \(([\d.]+),\s*([\d.]+),\s*([\d.]+)\)\s*facing\s*\n?\s*([\d.]+)[^,]*,\s*(\w+)(?: with directivity ([\d.]+))?,\s*(OFF)/gi;
  let m2;
  while ((m2 = re2.exec(PROTO))){
    defs.push({ x:+m2[2], y:+m2[3], z:+m2[4], yaw:+m2[5],
                type:m2[6].toUpperCase() === "LOUDSPEAKER" ? 1 : 0,
                dir:m2[7] ? +m2[7] : 0, inp:0, lvl:0.8 });
  }
  if (defs.length !== 4) throw new Error("PROTOCOL.md: read " + defs.length + " source defaults, wanted 4");
  for (let n = 1; n <= 4; n++){
    const d = defs[n-1];
    out.push(["s"+n+"x", "SOURCE "+n+" X", d.x/18, 0, 0, null]);
    out.push(["s"+n+"y", "SOURCE "+n+" Y", d.y/9, 0, 0, null]);
    out.push(["s"+n+"z", "SOURCE "+n+" HEIGHT", (d.z-0.2)/2.2, 0, 0, null]);
    out.push(["s"+n+"yaw", "SOURCE "+n+" FACING", d.yaw/360, 0, 0, null]);
    out.push(["s"+n+"type", "SOURCE "+n+" TYPE", d.type, 1, 2, "PURE|LOUDSPEAKER"]);
    out.push(["s"+n+"dir", "SOURCE "+n+" DIRECTIVITY", d.dir, 0, 0, null]);
    out.push(["s"+n+"in", "SOURCE "+n+" INPUT", d.inp/(INPUTS.length-1), 1, INPUTS.length, INPUTS.join("|")]);
    out.push(["s"+n+"lvl", "SOURCE "+n+" LEVEL", d.lvl, 0, 0, null]);
  }
  /*  The rest are ordinary table rows: | `id` | NAME | default | kind | ... */
  const plain = [];
  const re = /^\|\s*`([a-z0-9]+)`\s*\|\s*([^|]+?)\s*\|\s*([0-9.]+)\s*\|\s*(float|choice[^|]*)\s*\|/gmi;
  let m;
  while ((m = re.exec(PROTO))){
    const id = m[1];
    if (/^s[1-4]/.test(id)) continue;
    /*  Every choice parameter now lives in the per-room block, which this
        regex cannot match because its ids carry braces. One out here would be
        a row whose list nothing knows, so it is a finding rather than a
        guess. */
    if (/^choice/i.test(m[4]))
      throw new Error("PROTOCOL.md: choice row '" + id + "' outside the per-room block");
    plain.push([id, m[2].toUpperCase(), parseFloat(m[3]), 0, 0, null]);
  }
  /*  The per-room rows are written ONCE with {n} in the id and expanded here
      for n = 1..3, exactly as the document says to. Their defaults are a
      per-room list where the rooms differ (the walls) and one number where
      they do not. */
  const room = [];
  const tpl = [];
  const rre = /^\|\s*`([a-z]+\{n\}[a-z]*)`\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|\s*(float|choice[^|]*)\s*\|/gmi;
  let rm;
  while ((rm = rre.exec(PROTO))) tpl.push({ id:rm[1], name:rm[2], def:rm[3], kind:rm[4] });
  if (tpl.length !== 7)
    throw new Error("PROTOCOL.md: read " + tpl.length + " per-room rows, wanted 7");
  for (let n = 1; n <= 3; n++){
    for (const t of tpl){
      const id = t.id.replace("{n}", String(n));
      const defs = t.def.split(",").map(x => parseFloat(x.trim()));
      const raw = defs.length > 1 ? defs[n-1] : defs[0];
      let steps = 0, list = null;
      if (/^choice/i.test(t.kind)){
        list = /^mat/.test(id) ? MATS : (/^flr/.test(id) ? SURF_MATS : CEL_MATS);
        steps = list.length;
      }
      if (!isFinite(raw)) throw new Error("PROTOCOL.md: no default for " + id);
      /*  A choice default is written in the table as its INDEX; the wire
          carries index/(steps-1), which the document says one line below the
          table. Reading the number without applying that rule is how mat3
          would have arrived as 2.0 and clamped to the last entry. */
      room.push([id, t.name.replace(/\bn\b/g, String(n)).toUpperCase(),
                 steps ? raw/(steps-1) : raw, steps ? 1 : 0, steps,
                 list ? list.join("|") : null]);
    }
  }
  /*  Order: the per-room block sits between the doors and the level knobs,
      which is where the document puts it. */
  const at = plain.map(p => p[0]).indexOf("door3");
  if (at < 0) throw new Error("PROTOCOL.md: no door3 row to place the per-room block after");
  return out.concat(plain.slice(0, at + 1), room, plain.slice(at + 1));
}
const PARAMS = protocolParams();
const DECLARED = ((PROTO.match(/(\d+)\s+parameters\./) || [0, "0"])[1]) | 0;
/*  The document states its own total one line under the table; the list
    parsed out of the table must agree with it, or one of the two is stale. */
chk(DECLARED > 0 && PARAMS.length === DECLARED,
    "the parameter table matches the count the protocol declares",
    PARAMS.length + " parsed, " + DECLARED + " declared");

const DRIVER = `
<script>
(function(){
  var R = [];
  function chk(ok, name, detail){
    R.push((ok ? "PASS " : "FAIL ") + name + (detail ? "   [" + detail + "]" : ""));
  }
  function out(){
    var pre = document.createElement("pre");
    pre.id = "probe" + "out";      /* built from two pieces so the dump holds one literal */
    pre.textContent = R.join("\\n");
    document.body.appendChild(pre);
  }
  function sentOf(kind){
    return (window.__sent || []).filter(function (s){ return s.msg && s.msg.k === kind; });
  }
  function ps(id){ return sentOf("p").filter(function (s){ return s.msg.id === id; }); }

  var PARAMS = ${JSON.stringify(PARAMS)};
  var MATS = ${JSON.stringify(MATS)};
  var SURF = ${JSON.stringify(SURF_MATS)};

  /* --- fire what native would send ------------------------------------ */
  window.__fire("initialState", {
    build: "260921.1",
    showrays: 1,
    selsrc: 0,
    auxConnected: 0,
    wav: { name:"", playing:0, seconds:0, gain:0.75 },
    params: PARAMS.map(function (p){
      return { id:p[0], name:p[1], v:p[2], stepped:!!p[3], steps:p[4], choices:p[5] };
    })
  });
  var SCENE = {
    sources:[
      { pos:[3.0,2.5,1.2],  yaw:0,   type:1, room:0, active:1, dir:0 },
      { pos:[1.5,1.0,1.2],  yaw:45,  type:1, room:0, active:0, dir:0 },
      { pos:[4.5,7.0,1.2],  yaw:270, type:0, room:1, active:0, dir:0.5 },
      { pos:[12.0,4.5,1.6], yaw:180, type:0, room:2, active:0, dir:0 }
    ],
    lis:[4.5,2.5,1.65], lisRoom:0, lisYaw:180,
    paths:[
      { t:"direct", s:0, pts:[[3.0,2.5,1.2],[4.5,2.5,1.65]], db:0.0, ms:4.4 },
      { t:"r1", s:0, pts:[[3.0,2.5,1.2],[1.2,0.0,1.4],[4.5,2.5,1.65]], db:-8.2, ms:9.9 },
      { t:"r1", s:0, pts:[[3.0,2.5,1.2],[5.4,5.0,1.4],[4.5,2.5,1.65]], db:-9.6, ms:11.2 },
      { t:"r2", s:0, pts:[[3.0,2.5,1.2],[0.0,1.1,1.5],[2.2,0.0,1.5],[4.5,2.5,1.65]], db:-15.4, ms:18.1 },
      { t:"portal", s:0, pts:[[3.0,2.5,1.2],[6.0,2.5,1.1],[8.6,3.4,1.65],[6.0,2.5,1.1],[4.5,2.5,1.65]], db:-13.1, ms:26.0 },
      { t:"leaf", s:0, pts:[[3.0,2.5,1.2],[4.5,5.0,1.0],[4.5,2.5,1.65]], db:-28.7, ms:14.5 },
      { t:"wall", s:0, pts:[[3.0,2.5,1.2],[6.0,4.2,1.2],[4.5,2.5,1.65]], db:-33.2, ms:12.0 },
      { t:"direct", s:1, pts:[[1.5,1.0,1.2],[4.5,2.5,1.65]], db:-3.4, ms:8.1 },
      { t:"r1", s:1, pts:[[1.5,1.0,1.2],[3.0,0.0,1.3],[4.5,2.5,1.65]], db:-11.0, ms:12.6 },
      { t:"portal", s:2, pts:[[4.5,7.0,1.2],[4.5,5.0,1.1],[4.5,2.5,1.65]], db:-17.2, ms:21.0 },
      { t:"portal", s:3, pts:[[12.0,4.5,1.6],[6.0,2.5,1.1],[4.5,2.5,1.65]], db:-22.9, ms:34.0 },
      { t:"doorfield", s:0, db:-21.0, ms:20.0 }
    ],
    rt:[[0.62,0.48,0.38,0.29],[0.41,0.33,0.26,0.20],[2.41,1.96,1.42,0.95]],
    in:-14.0, out:-16.2, drr:3.8
  };
  window.__fire("scene", SCENE);
  window.__TW.render();

  /* --- 1 no JS errors -------------------------------------------------- */
  chk(window.__err.length === 0, "no JS errors on the page",
      window.__err.length ? window.__err.slice(0,3).join(" | ") : "0");

  /* --- 2,3 both canvases carry ink ------------------------------------- */
  /*  A fresh canvas is TRANSPARENT, so "not white" is true of every pixel on
      one and passes on nothing. Count pixels that are opaque and differ from
      that canvas's own background - and for the 3D view also demand that the
      luminance actually varies, because a flat fill of the wrong colour would
      otherwise score 100 percent. */
  function ink(cv, bg){
    var off = document.createElement("canvas");
    off.width = Math.min(cv.width, 900); off.height = Math.min(cv.height, 900);
    var c = off.getContext("2d");
    c.drawImage(cv, 0, 0, off.width, off.height);
    var d;
    try { d = c.getImageData(0, 0, off.width, off.height).data; }
    catch(e){ return { pct:-1, dark:-1, sd:-1, err:e.message }; }
    var n = 0, dark = 0, col = 0, tot = off.width * off.height, s = 0, s2 = 0;
    for (var i = 0; i < d.length; i += 4){
      if (d[i+3] < 200) continue;                       /* transparent is not ink */
      var r = d[i], g = d[i+1], b = d[i+2];
      var L = 0.2126*r + 0.7152*g + 0.0722*b;
      s += L; s2 += L*L;
      var dr = Math.abs(r-bg[0]), dg = Math.abs(g-bg[1]), db2 = Math.abs(b-bg[2]);
      if (dr + dg + db2 > 26) n++;
      /*  the plan's paper carries a painted vignette, so "differs from the
          paper colour" is true nearly everywhere - real drawing is DARK */
      if (L < 168) dark++;
      /*  The paper, the washes, the walls and every glyph on the plan are
          WARM - r is above b everywhere on it. A blue-dominant pixel can only
          be a drawn path. ("Saturated" does not work: the paper's own vignette
          makes 8 percent of the sheet saturated before a single ray lands.) */
      if (b > r + 18) col++;
    }
    var mean = s/tot;
    return { pct:n/tot*100, dark:dark/tot*100, col:col/tot*100,
             sd:Math.sqrt(Math.max(0, s2/tot - mean*mean)), tot:tot };
  }
  var bg = window.__TW.bg;
  var i3 = ink(document.getElementById("pov"), bg.pov);
  var ip = ink(document.getElementById("plan"), bg.plan);
  chk(i3.pct > 20 && i3.sd > 12, "3D view has a picture (" + window.__TW.renderer + ")",
      i3.pct.toFixed(1) + "% non-background, luminance sd " + i3.sd.toFixed(1));
  chk(ip.dark > 0.4 && ip.sd > 6, "plan view has ink on the paper",
      ip.dark.toFixed(2) + "% dark pixels, luminance sd " + ip.sd.toFixed(1));

  /* --- 4 every parameter in the list has a control or a readout --------- */
  var missing = [];
  PARAMS.forEach(function (p){
    if (!document.querySelector('[data-id="' + p[0] + '"]')) missing.push(p[0]);
  });
  chk(missing.length === 0, "all " + PARAMS.length + " parameters have a control or readout",
      missing.length ? missing.join(",") : PARAMS.length + " of " + PARAMS.length);

  /* the page must not have invented parameters of its own either */
  var ids = {};
  Array.prototype.forEach.call(document.querySelectorAll("[data-id]"), function (e){ ids[e.getAttribute("data-id")] = 1; });
  var known = {}; PARAMS.forEach(function (p){ known[p[0]] = 1; });
  var extra = Object.keys(ids).filter(function (k){ return !known[k]; });
  chk(extra.length === 0, "no control bound to an unknown parameter", extra.join(",") || "none");

  /* --- 5 every control carries a hint ---------------------------------- */
  var tipped = document.querySelectorAll("[data-tip]");
  var blank = 0;
  Array.prototype.forEach.call(tipped, function (e){
    if (!(e.getAttribute("data-tip") || "").trim()) blank++;
  });
  var untipped = Array.prototype.filter.call(document.querySelectorAll("[data-id]"), function (e){
    return !e.hasAttribute("data-tip");
  }).map(function (e){ return e.getAttribute("data-id"); });
  chk(tipped.length > 0 && blank === 0 && untipped.length === 0,
      "every control has a non-empty hint",
      tipped.length + " hints, " + blank + " blank, " + untipped.length + " bare");

  /* --- 6 a hint never covers the control it explains -------------------- */
  var overlap = [], zero = [];
  var tip = document.getElementById("tip");
  Array.prototype.forEach.call(tipped, function (e){
    var r = e.getBoundingClientRect();
    if (r.width === 0 && r.height === 0) return;
    e.dispatchEvent(new PointerEvent("pointerover", { bubbles:true, clientX:r.left+2, clientY:r.top+2 }));
    var t = tip.getBoundingClientRect();
    if (t.width < 8 || t.height < 8){ zero.push(e.getAttribute("data-id") || e.id || e.tagName); return; }
    var hit = !(t.right <= r.left || t.left >= r.right || t.bottom <= r.top || t.top >= r.bottom);
    if (hit) overlap.push(e.getAttribute("data-id") || e.id || e.tagName);
  });
  chk(overlap.length === 0 && zero.length === 0, "no hint overlaps its own control",
      "overlap " + overlap.length + ", empty " + zero.length +
      (overlap.length ? " (" + overlap.slice(0,3).join(",") + ")" : ""));
  tip.hidden = true;

  /* --- 6b the layout holds ---------------------------------------------- */
  var de = document.documentElement;
  var strip = document.getElementById("ctrls");
  var povR = document.getElementById("pov").getBoundingClientRect();
  var planR = document.getElementById("plan").getBoundingClientRect();
  chk(de.scrollWidth <= de.clientWidth + 1 && strip.scrollWidth <= strip.clientWidth + 1,
      "nothing scrolls sideways",
      "page " + de.scrollWidth + "/" + de.clientWidth + ", strip " + strip.scrollWidth + "/" + strip.clientWidth);
  chk(povR.width > 100 && povR.height > 100 && planR.width > 100 && planR.height > 100 &&
      povR.right <= planR.left + 1 && Math.abs(povR.height - planR.height) < 2,
      "the two views sit side by side and are both real",
      "pov " + Math.round(povR.width) + "x" + Math.round(povR.height) +
      ", plan " + Math.round(planR.width) + "x" + Math.round(planR.height));

  /* --- 7,8 the handshake ------------------------------------------------ */
  chk(sentOf("hello").length === 1, "hello sent once", String(sentOf("hello").length));
  var order = window.__sent.map(function (s){ return s.msg.k; });
  chk(order.indexOf("stateack") >= 0, "stateack sent after initialState");

  /* --- 9 a native value moves the page and sends nothing back ----------- */
  var beforeP = sentOf("p").length;
  var was = window.__TW.state().lisyaw;
  window.__TW.setParam("lisyaw", 0.25);
  var now = window.__TW.state().lisyaw;
  chk(Math.abs(now - 0.25) < 1e-6 && Math.abs(now - was) > 1e-6 && sentOf("p").length === beforeP,
      "setParam applies without echoing a p back",
      was.toFixed(3) + " -> " + now.toFixed(3) + ", p sent " + (sentOf("p").length - beforeP));

  /* --- 9b a hostParam echo moves the control itself --------------------- */
  var slider = document.querySelector('input[data-id="direct"]');
  var bp = sentOf("p").length;
  window.__fire("hostParam", { params:[{ id:"direct", v:0.2 }] });
  chk(Math.abs(parseFloat(slider.value) - 0.2) < 1e-3 && sentOf("p").length === bp,
      "hostParam moves the slider and sends no p",
      "slider " + slider.value + ", p sent " + (sentOf("p").length - bp));
  window.__fire("hostParam", { params:[{ id:"direct", v:0.8 }] });

  /* --- 10 dragging the source in the plan ------------------------------- */
  var plan = document.getElementById("plan");
  var pr = plan.getBoundingClientRect();
  /*  The source sits where the parameters put it; where that lands on the
      canvas is the PAGE's arithmetic, so ask the page rather than keeping a
      second copy of the layout here. */
  function w2s(x, y){
    var f = window.__TW.planFit();
    return [pr.left + f.ox + x * f.s, pr.top + f.oy + (9 - y) * f.s];
  }
  function planDrag(fromX, fromY, toX, toY){
    var a = w2s(fromX, fromY), b = w2s(toX, toY);
    plan.dispatchEvent(new PointerEvent("pointerdown", { bubbles:true, pointerId:7, button:0, buttons:1, clientX:a[0], clientY:a[1] }));
    window.dispatchEvent(new PointerEvent("pointermove", { bubbles:true, pointerId:7, buttons:1, clientX:a[0]+9, clientY:a[1]-9 }));
    window.dispatchEvent(new PointerEvent("pointermove", { bubbles:true, pointerId:7, buttons:1, clientX:b[0], clientY:b[1] }));
    window.dispatchEvent(new PointerEvent("pointerup", { bubbles:true, pointerId:7, buttons:0, clientX:b[0], clientY:b[1] }));
  }
  planDrag(3.0, 2.5, 3.9, 3.2);
  var gotDown = sentOf("touch").some(function (s){ return s.msg.id === "s1x" && s.msg.down === true; });
  var gotUp = sentOf("touch").some(function (s){ return s.msg.id === "s1x" && s.msg.down === false; });
  chk(gotDown && gotUp && ps("s1x").length > 0 && ps("s1y").length > 0,
      "plan drag of source 1 sends s1x and s1y inside a gesture",
      "touch " + gotDown + "/" + gotUp + ", p s1x " + ps("s1x").length + ", p s1y " + ps("s1y").length +
      ", now " + window.__TW.state().s1x.toFixed(4) + "," + window.__TW.state().s1y.toFixed(4));

  /*  A source whose INPUT is OFF is ghosted, not gone: it still has to take a
      drag, which is the whole reason it is drawn at all. Source 3 is PURE and
      OFF by default, in the box room. */
  var b3 = sentOf("p").length;
  planDrag(4.5, 7.0, 4.0, 6.4);
  var moved3 = Math.abs(window.__TW.state().s3x - 0.25) > 1e-4;
  chk(ps("s3x").length > 0 && ps("s3y").length > 0 && moved3,
      "a ghosted source still drags and sends its own ids",
      "p s3x " + ps("s3x").length + ", s3y " + ps("s3y").length +
      ", now " + window.__TW.state().s3x.toFixed(4) + "," + window.__TW.state().s3y.toFixed(4));
  chk(window.__TW.selsrc === 2 && sentOf("selsrc").some(function (s){ return s.msg.v === 2; }),
      "touching a source selects it and reports the selection",
      "selsrc " + window.__TW.selsrc);

  /* --- the SOURCE panel follows the selection -------------------------- */
  var vis = Array.prototype.filter.call(document.querySelectorAll("[data-srcset]"), function (e){ return !e.hidden; });
  var zEl = document.querySelector('input[data-id="s3z"]');
  var zVis = zEl && zEl.getBoundingClientRect().height > 0;
  var z1 = document.querySelector('input[data-id="s1z"]');
  chk(vis.length === 1 && vis[0].getAttribute("data-srcset") === "2" && zVis &&
      z1.getBoundingClientRect().height === 0,
      "selecting source 3 shows source 3's own controls and hides the others",
      "visible sets " + vis.length + " (" + (vis[0] ? vis[0].getAttribute("data-srcset") : "-") + ")");
  window.__TW.setParam("s3z", 0.9);
  chk(Math.abs(parseFloat(zEl.value) - 0.9) < 1e-3,
      "and those controls carry source 3's values", "s3z slider " + zEl.value);
  window.__TW.setParam("s3z", 0.4545);

  /* --- DIRECTIVITY belongs to PURE only -------------------------------- */
  var d3 = document.querySelector('input[data-id="s3dir"]');
  var livePure = !d3.disabled;
  window.__TW.setParam("s3type", 1);                 /* make it a LOUDSPEAKER */
  var deadLs = d3.disabled;
  window.__TW.setParam("s3type", 0);
  chk(livePure && deadLs && !d3.disabled,
      "DIRECTIVITY is live for PURE and greyed for LOUDSPEAKER",
      "pure enabled " + livePure + ", loudspeaker disabled " + deadLs);

  /* --- the AUX inputs are marked when the host has not connected them --- */
  var in3 = document.querySelector('select[data-id="s3in"]');
  var auxTxt = in3.options[4].textContent;
  chk(/not connected/i.test(auxTxt) && !in3.options[4].disabled &&
      !/not connected/i.test(in3.options[1].textContent),
      "an unconnected AUX input says so but stays selectable",
      JSON.stringify(auxTxt));

  /* --- both source models are really drawn ------------------------------ */
  /*  Stand 1.5 m in front of source 1 looking at it, then flip its type. If
      only one model were implemented - or if the type were read but never
      used - the two renders would be identical. */
  function povSig(){
    var cv = document.getElementById("pov");
    var off = document.createElement("canvas");
    off.width = 96; off.height = 72;
    var c = off.getContext("2d");
    c.drawImage(cv, 0, 0, off.width, off.height);
    return c.getImageData(0, 0, off.width, off.height).data;
  }
  function sigDiff(a, b){
    var s = 0, n = 0;
    for (var i = 0; i < a.length; i += 4){
      s += Math.abs(a[i]-b[i]) + Math.abs(a[i+1]-b[i+1]) + Math.abs(a[i+2]-b[i+2]);
      n++;
    }
    return s / n / 3;
  }
  window.__TW.setParam("s1x", 3.0/18); window.__TW.setParam("s1y", 2.5/9);
  window.__TW.setParam("s1z", (1.2-0.2)/2.2); window.__TW.setParam("s1yaw", 0.5);
  window.__TW.setParam("lisx", 4.5/18); window.__TW.setParam("lisy", 2.5/9);
  window.__TW.setParam("lisyaw", 0.5);
  window.__TW.setParam("s1type", 1); window.__TW.render();
  var sigLs = povSig();
  window.__TW.setParam("s1type", 0); window.__TW.render();
  var sigPure = povSig();
  window.__TW.setParam("s1type", 1); window.__TW.render();
  var sigBack = povSig();
  /*  Measured as the AREA that changed, not as the mean over the frame. A
      loudspeaker a metre and a half away covers a few per cent of the view,
      so averaging its disappearance over the whole picture divides the one
      real difference by the wall behind it and reports a number that says
      more about the window size than about the model. */
  function sigMoved(a, b){
    var n = 0, tot = 0;
    for (var i = 0; i < a.length; i += 4){
      tot++;
      if (Math.abs(a[i]-b[i]) + Math.abs(a[i+1]-b[i+1]) + Math.abs(a[i+2]-b[i+2]) > 40) n++;
    }
    return n / tot * 100;
  }
  var dFlip = sigMoved(sigLs, sigPure), dSame = sigMoved(sigLs, sigBack);
  chk(dFlip > 1.0 && dSame === 0,
      "the 3D view draws a different object for LOUDSPEAKER and for PURE",
      "flip changed " + dFlip.toFixed(2) + "% of the view, same type back " + dSame.toFixed(2) + "%");

  /*  and a source that is OFF is drawn FAINTER than the same source on -
      ghosting is the claim, not hiding. */
  window.__TW.setParam("s1in", 3/6); window.__TW.render();
  var sigOn = povSig();
  window.__TW.setParam("s1in", 0); window.__TW.render();
  var sigOff = povSig();
  window.__TW.setParam("s1in", 3/6); window.__TW.render();
  chk(sigDiff(sigOn, sigOff) > 1.5,
      "an inactive source is ghosted rather than hidden or unchanged",
      "on vs off " + sigDiff(sigOn, sigOff).toFixed(2) + " levels mean");

  function planSig(){
    var cv = document.getElementById("plan");
    var off = document.createElement("canvas");
    off.width = 132; off.height = 88;
    var c = off.getContext("2d");
    c.drawImage(cv, 0, 0, off.width, off.height);
    return c.getImageData(0, 0, off.width, off.height).data;
  }
  /*  Strongly red ink, which on this warm sheet can only be something drawn
      as a warning: the paper, the washes, the walls and every glyph are warm
      but nowhere near this saturated. */
  function redInk(cv){
    var off = document.createElement("canvas");
    off.width = Math.min(cv.width, 900); off.height = Math.min(cv.height, 900);
    var c = off.getContext("2d");
    c.drawImage(cv, 0, 0, off.width, off.height);
    var d = c.getImageData(0, 0, off.width, off.height).data;
    var n = 0, tot = off.width * off.height;
    for (var i = 0; i < d.length; i += 4){
      if (d[i+3] < 200) continue;
      if (d[i] > 120 && d[i] - d[i+2] > 70 && d[i+1] < d[i] * 0.80) n++;
    }
    return n / tot * 100;
  }

  /* --- 11 walking --------------------------------------------------------*/
  function press(k, n){
    for (var i = 0; i < (n || 1); i++){
      window.dispatchEvent(new KeyboardEvent("keydown", { key:k, bubbles:true }));
      window.dispatchEvent(new KeyboardEvent("keyup", { key:k, bubbles:true }));
    }
  }
  var bx = window.__TW.state().lisx, by = window.__TW.state().lisy;
  var beforeWalk = ps("lisx").length + ps("lisy").length;
  press("w", 1);
  var afterWalk = ps("lisx").length + ps("lisy").length;
  var ax = window.__TW.state().lisx, ay = window.__TW.state().lisy;
  chk(afterWalk > beforeWalk && (Math.abs(ax-bx) > 1e-6 || Math.abs(ay-by) > 1e-6),
      "pressing W walks and sends lisx or lisy",
      "p " + (afterWalk - beforeWalk) + ", moved " +
      Math.hypot((ax-bx)*18, (ay-by)*9).toFixed(3) + " m");

  /*  A closed door is a wall - the protocol says walking is blocked below an
      aperture of 0.5, and that is the whole point of the doors. Ten presses
      carry you 0.6 m, which is more than enough to cross x = 6 if it is open
      and nowhere near enough to get past the 0.30 m wall margin if it is not.
      Frame independent on purpose: one press is one step. */
  function walkAt(doorV){
    window.__TW.setParam("door2", doorV);
    window.__TW.setParam("lisx", 5.50/18);
    window.__TW.setParam("lisy", 2.50/9);
    window.__TW.setParam("lisyaw", 0);          /* facing east, at the doorway */
    press("w", 10);
    return window.__TW.state().lisx * 18;
  }
  var xShut = walkAt(0), xOpen = walkAt(1);
  chk(xShut < 5.75 && xOpen > 6.0, "a shut door blocks the walk, an open one does not",
      "shut stopped at x " + xShut.toFixed(2) + " m, open reached " + xOpen.toFixed(2) + " m");
  window.__TW.setParam("lisx", 0.25); window.__TW.setParam("lisy", 0.2778);
  window.__TW.setParam("lisyaw", 0.5);

  /* --- 12 the rays are really drawn -------------------------------------- */
  var bRays = document.getElementById("bRays");
  if (!window.__TW.rays) bRays.click();
  bRays.click();                       /* off */
  window.__TW.render();
  var offInk = ink(plan, bg.plan).col;
  bRays.click();                       /* on again */
  window.__TW.render();
  var onInk = ink(plan, bg.plan).col;
  chk(onInk > offInk * 1.6 && onInk - offInk > 0.05, "showrays draws the paths on the plan",
      "blue pixels off " + offInk.toFixed(3) + "% -> on " + onInk.toFixed(3) + "%");
  chk(sentOf("showrays").length >= 2, "the RAYS button reports the preference",
      String(sentOf("showrays").length));

  /* --- 12b the SAVE hint counts the parameters rather than naming them ---- */
  var saveBtn = Array.prototype.filter.call(document.querySelectorAll("button"), function (b){
    return b.textContent.trim() === "Save";
  })[0];
  var saveTip = saveBtn ? (saveBtn.getAttribute("data-tip") || "") : "";
  chk(!!saveBtn && saveTip.indexOf(String(PARAMS.length)) >= 0 && !/[^0-9]19[^0-9]/.test(saveTip),
      "the SAVE hint counts the parameters instead of naming a stale number",
      JSON.stringify(saveTip));

  /* --- 12c the nine surfaces -------------------------------------------- */
  var TWx = window.__TW;
  var surfIds = [];
  for (var sr = 1; sr <= 3; sr++) surfIds.push("mat"+sr, "flr"+sr, "cel"+sr);
  var surfGone = surfIds.filter(function (id){
    return !document.querySelector('select[data-id="' + id + '"]');
  });
  chk(surfGone.length === 0, "all nine surface selectors exist",
      surfGone.length ? "missing " + surfGone.join(",") : "9 of 9");

  /*  The floor and the ceiling carry one entry the walls do not, at the front
      and as the default. A list that merely has the right LENGTH would pass a
      check that only counted, so the names are compared in order. */
  var surfBad = [];
  ["flr1","flr2","flr3","cel1","cel2","cel3"].forEach(function (id){
    var e = document.querySelector('select[data-id="' + id + '"]');
    var nm = e ? Array.prototype.map.call(e.options, function (o){ return o.textContent; }) : [];
    if (nm.length !== SURF.length || nm.join("|") !== SURF.join("|")) surfBad.push(id + "=[" + nm.join(",") + "]");
  });
  chk(surfBad.length === 0 && SURF.length === MATS.length + 1 && SURF[0] === "AS WALLS",
      "floor and ceiling offer " + SURF.length + " entries with AS WALLS first",
      surfBad.length ? surfBad.join("  ") : SURF.join(","));
  var w1 = document.querySelector('select[data-id="mat1"]');
  var wNames = Array.prototype.map.call(w1.options, function (o){ return o.textContent; });
  chk(wNames.join("|") === MATS.join("|"),
      "and the wall selector offers the materials only, with no AS WALLS entry",
      wNames.length + " [" + wNames.join(",") + "]");

  /*  Setting the LARGE floor to ABSORBING: the right index on the wire, the
      right value in the page, and the right entry showing. */
  var f1 = document.querySelector('select[data-id="flr1"]');
  var absIdx = SURF.indexOf("ABSORBING");
  var fBefore = ps("flr1").length;
  TWx.render();
  var floorWas = povSig();
  /*  A selector that is not there has to FAIL this check, not throw: a driver
      that dies reports as no failures at all, which is the worst of the three
      outcomes. */
  if (f1){
    f1.value = String(absIdx);
    f1.dispatchEvent(new Event("change", { bubbles:true }));
  }
  var wantFlr = absIdx / (SURF.length - 1);
  var fSent = ps("flr1");
  chk(!!f1 && absIdx > 0 && fSent.length === fBefore + 1 &&
      Math.abs(fSent[fSent.length-1].msg.v - wantFlr) < 1e-6 &&
      Math.abs(TWx.state().flr1 - wantFlr) < 1e-6 &&
      f1.options[parseInt(f1.value,10)].textContent === "ABSORBING",
      "setting the LARGE floor to ABSORBING sends index/(steps-1) and reads back",
      "index " + absIdx + " of " + SURF.length + " -> v " +
      (fSent.length > fBefore ? fSent[fSent.length-1].msg.v.toFixed(4) : "none") +
      ", page " + TWx.state().flr1.toFixed(4));
  /*  and the carpet has to reach the FLOOR, not only the selector - a page
      that stored the value and went on drawing the walls' material would pass
      everything above. */
  TWx.render();
  var floorNow = povSig();
  chk(sigMoved(floorWas, floorNow) > 1.0,
      "and a carpet on the floor reaches the 3D view",
      "changed " + sigMoved(floorWas, floorNow).toFixed(2) + "% of the picture");
  TWx.setParam("flr1", 0);
  TWx.render();

  /* --- 12d the broken walls --------------------------------------------- */
  var foldIds = [];
  for (var fr = 1; fr <= 3; fr++) foldIds.push("fold"+fr+"a", "foldp"+fr+"a", "fold"+fr+"b", "foldp"+fr+"b");
  var foldGone = foldIds.filter(function (id){
    return !document.querySelector('input[data-id="' + id + '"]');
  });
  chk(foldGone.length === 0, "all twelve broken-wall faders exist",
      foldGone.length ? "missing " + foldGone.join(",") : "12 of 12");

  ["fold1a","fold1b","fold2a","fold2b","fold3a","fold3b",
   "foldp1a","foldp1b","foldp2a","foldp2b","foldp3a","foldp3b"].forEach(function (id){
    TWx.setParam(id, 0.5);
  });
  TWx.render();
  var flatSig = planSig();
  var flatPoly = TWx.roomPoly(2);

  function driveFader(el, v){
    el.dispatchEvent(new PointerEvent("pointerdown", { bubbles:true, pointerId:21, button:0, buttons:1 }));
    el.value = String(v);
    el.dispatchEvent(new Event("input", { bubbles:true }));
    el.dispatchEvent(new PointerEvent("pointerup", { bubbles:true, pointerId:21, buttons:0 }));
  }
  var fsl = document.querySelector('input[data-id="fold3a"]');
  var slBefore = ps("fold3a").length;
  driveFader(fsl, 0.90);
  var slDown = sentOf("touch").some(function (t){ return t.msg.id === "fold3a" && t.msg.down === true; });
  var slUp = sentOf("touch").some(function (t){ return t.msg.id === "fold3a" && t.msg.down === false; });
  TWx.render();
  var slSig = planSig();
  chk(slDown && slUp && ps("fold3a").length > slBefore && Math.abs(TWx.state().fold3a - 0.90) < 1e-6 &&
      TWx.roomPoly(2).length === flatPoly.length + 1 && sigMoved(flatSig, slSig) > 0.3,
      "a broken-wall fader sends its parameter inside a gesture and breaks the wall on the paper",
      "touch " + slDown + "/" + slUp + ", p " + (ps("fold3a").length - slBefore) + ", polygon " + flatPoly.length +
      " -> " + TWx.roomPoly(2).length + " points, plan changed " +
      sigMoved(flatSig, slSig).toFixed(2) + "%");
  TWx.setParam("fold3a", 0.5);
  var lisWas = [TWx.state().lisx, TWx.state().lisy, TWx.state().lisyaw];
  TWx.setParam("lisx", 13.5/18); TWx.setParam("lisy", 4.5/9); TWx.setParam("lisyaw", 0);
  TWx.render();
  var wallFlat = povSig(), boxFlat = TWx.meshBox();
  TWx.setParam("fold3a", 0.98);                  /* the hall's east wall, pushed out */
  TWx.render();
  var wallBent = povSig(), boxBent = TWx.meshBox();
  TWx.setParam("fold3a", 0.5);
  TWx.render();
  var wallBack = povSig();
  /*  Two halves, and the first is the one that matters: the VERTICES the room
      is built from have to reach further east, by the 0.576 m the fader asked
      for. Measuring only the picture passes on a mesh still built from the
      rectangle - the corner darkening follows the polygon on its own, and
      that alone repaints 11 per cent of the view. */
  var reach = (boxFlat && boxBent) ? (boxBent[2] - boxFlat[2]) : 0;
  chk(Math.abs(reach - 0.576) < 0.02 && sigMoved(wallFlat, wallBent) > 1.0 &&
      sigMoved(wallFlat, wallBack) === 0,
      "and the wall really moves in the room you are standing in",
      "the built geometry reaches " + reach.toFixed(3) + " m further east (wanted 0.576), " +
      "the view changed " + sigMoved(wallFlat, wallBent).toFixed(2) +
      "%, putting it back " + sigMoved(wallFlat, wallBack).toFixed(2) + "%");
  TWx.setParam("lisx", lisWas[0]); TWx.setParam("lisy", lisWas[1]); TWx.setParam("lisyaw", lisWas[2]);
  TWx.render();

  /*  The marker in the plan carries BOTH of that wall's numbers: out of the
      room is the push, along the wall is the split. Each move sets an
      absolute value from where the pointer IS, so a drag that ends on the
      wall line ends with the split it started with - which is what makes the
      two halves of the gesture separable at all. */
  var fp = TWx.foldPoint(0, 0);                 /* LARGE, west wall */
  var t0 = TWx.state().foldp1a;
  var pPerp = ps("fold1a").length, qPerp = ps("foldp1a").length;
  planDrag(fp[0], fp[1], fp[0] - 0.45, fp[1]);  /* straight out of the room */
  var gotFoldDown = sentOf("touch").some(function (t){ return t.msg.id === "fold1a" && t.msg.down === true; });
  var gotFoldUp = sentOf("touch").some(function (t){ return t.msg.id === "fold1a" && t.msg.down === false; });
  chk(gotFoldDown && gotFoldUp && ps("fold1a").length > pPerp &&
      TWx.state().fold1a > 0.80 && Math.abs(TWx.state().foldp1a - t0) < 0.02,
      "dragging the marker out of the room sends fold1a inside a gesture and leaves the split alone",
      "touch " + gotFoldDown + "/" + gotFoldUp + ", p " + (ps("fold1a").length - pPerp) +
      ", push " + TWx.state().fold1a.toFixed(4) + ", split " + t0.toFixed(4) +
      " -> " + TWx.state().foldp1a.toFixed(4));

  var v0 = TWx.state().fold1a;
  var fp2 = TWx.foldPoint(0, 0);
  var qAlong = ps("foldp1a").length;
  planDrag(fp2[0], fp2[1], fp2[0], fp2[1] + 1.2);   /* along the wall instead */
  var alongDown = sentOf("touch").some(function (t){ return t.msg.id === "foldp1a" && t.msg.down === true; });
  var alongUp = sentOf("touch").some(function (t){ return t.msg.id === "foldp1a" && t.msg.down === false; });
  chk(alongDown && alongUp && ps("foldp1a").length > qAlong &&
      Math.abs(TWx.state().foldp1a - t0) > 0.05 && Math.abs(TWx.state().fold1a - v0) < 0.02,
      "and dragging it ALONG the wall sends foldp1a and leaves the push alone",
      "touch " + alongDown + "/" + alongUp + ", p " + (ps("foldp1a").length - qAlong) +
      ", split " + t0.toFixed(4) + " -> " + TWx.state().foldp1a.toFixed(4) +
      ", push " + v0.toFixed(4) + " -> " + TWx.state().fold1a.toFixed(4));

  /*  The ENGINE's polygon, when it sends one, is what the plan is drawn from.
      The scene fired at the top of this run carries none, so the page has been
      drawing its own all along; one with a polygon in it must move the wall. */
  TWx.setParam("fold1a", 0.5); TWx.setParam("foldp1a", 0.5);
  TWx.render();
  var noPlanSig = planSig();
  var withPlan = JSON.parse(JSON.stringify(SCENE));
  function rectPoly(x0, y0, x1, y1){ return [[x0,y0],[x1,y0],[x1,y1],[x0,y1]]; }
  withPlan.plan = [rectPoly(0,0,6,5), rectPoly(3,5,6,8.5),
                   [[6,0],[18,0],[18,9],[12,7.5],[6,9]]];
  window.__fire("scene", withPlan);
  TWx.render();
  var planSigA = planSig();
  var gp = TWx.roomPoly(2);
  chk(gp.length === 5 && Math.abs(gp[3][0] - 12) < 1e-6 && Math.abs(gp[3][1] - 7.5) < 1e-6 &&
      sigMoved(noPlanSig, planSigA) > 1.0,
      "a polygon in the scene is what the plan draws",
      "GIANT has " + gp.length + " points, the fourth at " + gp[3] +
      ", plan changed " + sigMoved(noPlanSig, planSigA).toFixed(2) + "%");
  window.__fire("scene", SCENE);                 /* and back to no polygon */
  TWx.render();

  /*  Concave: bent INTO the room, which focuses. The plan has to SAY so, and
      an equal push the other way must not - a page that warned about every
      fold would pass a check that only looked at the concave one. planNotes
      is pushed where the text is drawn, so a note in it is a note on paper. */
  TWx.setParam("fold1a", 0.85);                  /* +0.42 m, out of the room */
  TWx.render();
  var outNotes = TWx.planNotes().join(" | ");
  var outRed = redInk(plan);
  TWx.setParam("fold1a", 0.15);                  /* -0.42 m, into it */
  TWx.render();
  var inNotes = TWx.planNotes().join(" | ");
  var inRed = redInk(plan);
  chk(/concave/i.test(inNotes) && !/concave/i.test(outNotes) && inRed > outRed + 0.02,
      "a wall bent INTO the room is marked concave on the plan and one bent out is not",
      "bent in: " + JSON.stringify(inNotes.slice(0, 80)) + " red " + inRed.toFixed(3) +
      "% / bent out: " + JSON.stringify(outNotes.slice(0, 40)) + " red " + outRed.toFixed(3) + "%");

  /*  Put every wall back flat, so the checks after this one measure the page
      rather than this block. */
  ["fold1a","fold1b","fold2a","fold2b","fold3a","fold3b",
   "foldp1a","foldp1b","foldp2a","foldp2b","foldp3a","foldp3b"].forEach(function (id){
    TWx.setParam(id, 0.5);
  });
  TWx.render();

  /* --- 13 the test signal group ------------------------------------------ */
  var grpOk = !!(document.getElementById("bWavOpen") && document.getElementById("bWavPlay") &&
                 document.getElementById("wavGain") && document.getElementById("wavFile"));
  chk(grpOk, "TEST SIGNAL group exists (load, play, level, file)");
  var bo = sentOf("wavOpen").length, bpl = sentOf("wavPlay").length, bg2 = sentOf("wavGain").length;
  if (grpOk){
    document.getElementById("bWavOpen").click();
    document.getElementById("bWavPlay").click();
    var wg = document.getElementById("wavGain");
    wg.value = "0.4";
    wg.dispatchEvent(new Event("input", { bubbles:true }));
  }
  chk(sentOf("wavOpen").length === bo + 1 && sentOf("wavPlay").length === bpl + 1 &&
      sentOf("wavGain").length === bg2 + 1,
      "test signal buttons send wavOpen, wavPlay and wavGain",
      "open " + (sentOf("wavOpen").length - bo) + ", play " + (sentOf("wavPlay").length - bpl) +
      ", gain " + (sentOf("wavGain").length - bg2));
  window.__fire("wav", { name:"hallway-clap.wav", playing:1, seconds:12.34, gain:0.6 });
  var ft = (document.getElementById("wavFile") || {}).textContent || "";
  chk(ft.indexOf("hallway-clap.wav") >= 0 && ft.indexOf("12.3") >= 0,
      "the wav event names the file and its length", JSON.stringify(ft));

  /* --- 14 the scene stream must not cost draws by itself ------------------ */
  function fireScenes(n, inDb){
    for (var i = 0; i < n; i++){
      SCENE.in = inDb;                 /* only the level moves: same geometry */
      window.__fire("scene", SCENE);
    }
  }
  /*  Measured on the pending flag as well as on the draw counter: headless
      Chrome produces no animation frames here at all, so a counter alone
      cannot tell a page that booked no redraw from one whose frame has simply
      not arrived. (No back-ticks in this block - it is a template literal.) */
  var TW = window.__TW;
  TW.render();
  var lit = TW.glow;                   /* read BEFORE, or the message lies */
  fireScenes(12, -80);                 /* silence: the live glow must go out */
  chk(lit > 0 && TW.glow === 0 && TW.pending === true,
      "silence puts the speaker glow out, and books the one frame that clears it",
      "glow " + lit.toFixed(3) + " -> " + TW.glow.toFixed(3) + ", frame booked " + TW.pending);

  TW.render();
  var s0 = TW.draws;
  fireScenes(12, -80);                 /* settled silence: nothing at all */
  chk(TW.draws === s0 && TW.pending === false,
      "a settled silent scene stream costs no redraw at all",
      "draws " + s0 + " -> " + TW.draws + ", frame booked " + TW.pending);

  fireScenes(12, -10);                 /* signal: one redraw, not twelve */
  var lit2 = TW.glow, booked = TW.pending, dAfter = TW.draws;
  TW.render();
  fireScenes(12, -10);                 /* still inside the same 100 ms window */
  chk(lit2 > 0 && booked === true && dAfter === s0 && TW.pending === false,
      "twelve scene messages in one tick book exactly one glow redraw, then the rate limit holds",
      "glow " + lit2.toFixed(3) + ", booked " + booked + ", immediate draws " + (dAfter - s0) +
      ", re-booked inside 100 ms " + TW.pending);

  /* --- 15 the movement keys always win --------------------------------- */
  /*  This is the thing Peter kept getting caught by: once a select, a slider
      or a button had the focus the browser handed it the arrows and W A S D,
      and the listener stopped walking. Every check here has TWO halves and
      both matter - that the step happened, AND that the control did not move.
      Either half alone passes on a page that is still broken the other way. */
  /*  Returns what the page DID to the key. A synthetic event runs no default
      action, so "the select still says 1" is true even on a page that never
      touched the event - the two things worth measuring are that the default
      was cancelled and that the control never saw the key at all. */
  function keyOn(el, key){
    var reached = 0;
    var spy = function (){ reached++; };
    el.addEventListener("keydown", spy);
    var ev = new KeyboardEvent("keydown", { key:key, bubbles:true, cancelable:true });
    el.dispatchEvent(ev);
    el.removeEventListener("keydown", spy);
    el.dispatchEvent(new KeyboardEvent("keyup", { key:key, bubbles:true, cancelable:true }));
    return { stopped: ev.defaultPrevented, reached: reached };
  }
  function walkedBy(fn){
    var x0 = TW.state().lisx, y0 = TW.state().lisy, a0 = TW.state().lisyaw;
    var p0 = ps("lisx").length + ps("lisy").length, q0 = ps("lisyaw").length;
    fn();
    return { moved: Math.hypot((TW.state().lisx - x0)*18, (TW.state().lisy - y0)*9),
             turned: Math.abs(TW.state().lisyaw - a0) * 360,
             sent: ps("lisx").length + ps("lisy").length - p0,
             sentYaw: ps("lisyaw").length - q0 };
  }
  TW.setParam("door1", 0); TW.setParam("door2", 1); TW.setParam("door3", 0);
  TW.setParam("lisx", 3.0/18); TW.setParam("lisy", 2.5/9); TW.setParam("lisyaw", 0);
  var matSel = document.querySelector('select[data-id="mat1"]');
  matSel.focus();
  var selWas = matSel.value;
  var wSeen = null;
  var wKey = walkedBy(function (){ wSeen = keyOn(matSel, "w"); });
  chk(wKey.sent > 0 && wKey.moved > 1e-4 && matSel.value === selWas &&
      wSeen.stopped && wSeen.reached === 0,
      "W walks even with a menu focused, and the menu never sees the key",
      "moved " + wKey.moved.toFixed(3) + " m, p " + wKey.sent +
      ", select " + selWas + " -> " + matSel.value +
      ", cancelled " + wSeen.stopped + ", reached the menu " + wSeen.reached);

  matSel.focus();
  selWas = matSel.value;
  var aSeen = null;
  var aKey = walkedBy(function (){ aSeen = keyOn(matSel, "ArrowUp"); });
  chk(aKey.sent > 0 && aKey.moved > 1e-4 && matSel.value === selWas &&
      aSeen.stopped && aSeen.reached === 0,
      "ArrowUp walks even with a menu focused, and the menu never sees the key",
      "moved " + aKey.moved.toFixed(3) + " m, p " + aKey.sent +
      ", select " + selWas + " -> " + matSel.value +
      ", cancelled " + aSeen.stopped + ", reached the menu " + aSeen.reached);

  /*  A focused FADER is the other trap: the arrows are its own default
      action, so the slider crept while the listener stood still. */
  var dirSl = document.querySelector('input[data-id="direct"]');
  dirSl.focus();
  var slWas = parseFloat(dirSl.value);
  /*  LEFT and RIGHT TURN the head rather than stepping sideways, so this one
      is measured against the heading - the first version of this check asked
      whether the listener had MOVED and failed on a page that was working. */
  var rSeen = null;
  var rKey = walkedBy(function (){ rSeen = keyOn(dirSl, "ArrowRight"); });
  chk(rKey.sentYaw > 0 && rKey.turned > 1e-3 && Math.abs(parseFloat(dirSl.value) - slWas) < 1e-9 &&
      rSeen.stopped && rSeen.reached === 0,
      "an arrow with a fader focused turns the head and the fader never sees it",
      "turned " + rKey.turned.toFixed(2) + " deg, p " + rKey.sentYaw +
      ", fader " + slWas + " -> " + dirSl.value +
      ", cancelled " + rSeen.stopped + ", reached the fader " + rSeen.reached);
  var uKey = walkedBy(function (){ keyOn(dirSl, "ArrowUp"); });
  chk(uKey.sent > 0 && uKey.moved > 1e-4,
      "and a forward arrow with a fader focused still walks",
      "moved " + uKey.moved.toFixed(3) + " m, p " + uKey.sent);

  /*  and the one exception: somewhere text is genuinely being typed. There is
      no text field on this panel, so the probe makes one - the guard exists so
      that adding one later does not break walking, and a guard nothing tests
      is a guard nobody can trust. */
  var txt = document.createElement("input");
  txt.type = "text"; txt.style.position = "fixed"; txt.style.left = "-400px";
  document.body.appendChild(txt);
  txt.focus();
  var tSeen = [];
  var tKey = walkedBy(function (){ tSeen.push(keyOn(txt, "w"), keyOn(txt, "ArrowUp")); });
  var letThrough = tSeen.every(function (t){ return !t.stopped && t.reached === 1; });
  chk(tKey.sent === 0 && tKey.moved === 0 && document.activeElement === txt && letThrough,
      "a key typed into a text field does not walk, and the field gets it",
      "moved " + tKey.moved.toFixed(3) + " m, p " + tKey.sent +
      ", untouched and delivered " + letThrough);
  txt.remove();

  /* --- 16 the door handles in the 3D view ------------------------------- */
  /*  The picking lives inside the page closure; __TW.project is its inverse,
      so the probe aims at a handle without keeping a second copy of the
      camera - which would drift the first time the field of view changed. */
  var pov = document.getElementById("pov");
  function aimAt(id){ return TW.project(TW.handlePos(id)); }
  function hoverAt(p){
    pov.dispatchEvent(new PointerEvent("pointermove", { bubbles:true, pointerId:11, clientX:p[0], clientY:p[1] }));
  }
  /*  Stand in the living room a metre and a half from the shut door to the
      box room, looking straight at it. */
  TW.setParam("door1", 0);
  TW.setParam("lisx", 4.50/18); TW.setParam("lisy", 3.60/9); TW.setParam("lisyaw", 90/360);
  TW.render();
  var near = aimAt("door1");
  hoverAt(near);
  var hoverNear = TW.hoverDoor, cursorNear = pov.classList.contains("handle");
  var pickNear = TW.pick(near[0], near[1]);
  chk(hoverNear === "door1" && cursorNear && pickNear && pickNear.id === "door1",
      "hovering the handle of a door within reach reports it and changes the cursor",
      "hover " + hoverNear + ", cursor class " + cursorNear +
      ", " + (pickNear ? pickNear.dist.toFixed(2) + " m away" : "no pick"));

  /*  All three doors, each from its own doorway. They differ in the axis the
      wall runs along AND in which end the hinge is at - door 3 hangs the
      other way round from door 2 on the same wall - so one of them working
      says nothing about the other two. */
  var stands = [
    { id:"door1", x:4.50, y:3.60, yaw:90 },
    { id:"door2", x:4.80, y:2.80, yaw:0 },
    { id:"door3", x:5.00, y:6.20, yaw:0 }
  ];
  var picked = [];
  stands.forEach(function (st){
    TW.setParam(st.id, 0);
    TW.setParam("lisx", st.x/18); TW.setParam("lisy", st.y/9); TW.setParam("lisyaw", st.yaw/360);
    var p = aimAt(st.id);
    var h = p ? TW.pick(p[0], p[1]) : null;
    picked.push(st.id + ":" + (h ? h.id + "@" + h.dist.toFixed(2) + "m" : "nothing"));
  });
  chk(picked.every(function (t, i){ return t.indexOf(stands[i].id + ":" + stands[i].id) === 0; }),
      "every door offers its handle from its own doorway", picked.join(", "));
  TW.setParam("door1", 0);
  TW.setParam("lisx", 4.50/18); TW.setParam("lisy", 3.60/9); TW.setParam("lisyaw", 90/360);
  TW.render();
  near = aimAt("door1");
  hoverAt(near);

  /*  Hovering must also be worth something to look at: the handle under the
      pointer is drawn lit, so a picture with the pointer on it and one
      without cannot be the same picture. */
  TW.render();
  var sigHot = povSig();
  hoverAt([near[0] + 260, near[1]]);
  TW.render();
  var sigCold = povSig();
  chk(TW.hoverDoor === null && sigDiff(sigHot, sigCold) > 0.05,
      "the handle under the pointer is drawn differently from one that is not",
      "hot vs cold " + sigDiff(sigHot, sigCold).toFixed(3) + " levels mean");

  /*  The same handle from the far corner of the living room: in view, in
      front, nothing in the way - and still not yours to turn. */
  TW.setParam("lisx", 1.00/18); TW.setParam("lisy", 1.00/9);
  TW.setParam("lisyaw", (Math.atan2(5.00-1.00, 4.80-1.00)*180/Math.PI)/360);
  TW.render();
  var far = aimAt("door1");
  hoverAt(far);
  var farDist = Math.hypot(4.80-1.00, 5.00-1.00);
  chk(!!far && TW.hoverDoor === null && !pov.classList.contains("handle"),
      "the same handle out of reach is not hovered and the cursor stays put",
      farDist.toFixed(2) + " m away, hover " + TW.hoverDoor +
      ", on screen at " + (far ? Math.round(far[0]) + "," + Math.round(far[1]) : "-"));

  /* --- a drag on the handle swings the door ----------------------------- */
  TW.setParam("door1", 0.5);
  TW.setParam("lisx", 4.30/18); TW.setParam("lisy", 3.95/9); TW.setParam("lisyaw", 80/360);
  TW.render();
  var grab = aimAt("door1");
  var pBefore = ps("door1").length, vBefore = TW.state().door1;
  var yawBefore = ps("lisyaw").length;
  hoverAt(grab);
  pov.dispatchEvent(new PointerEvent("pointerdown", { bubbles:true, pointerId:12, button:0, buttons:1, clientX:grab[0], clientY:grab[1] }));
  for (var g = 1; g <= 4; g++)
    window.dispatchEvent(new PointerEvent("pointermove", { bubbles:true, pointerId:12, buttons:1, clientX:grab[0] + g*16, clientY:grab[1] }));
  window.dispatchEvent(new PointerEvent("pointerup", { bubbles:true, pointerId:12, buttons:0, clientX:grab[0] + 64, clientY:grab[1] }));
  var tDown = sentOf("touch").filter(function (t){ return t.msg.id === "door1" && t.msg.down === true; }).length;
  var tUp = sentOf("touch").filter(function (t){ return t.msg.id === "door1" && t.msg.down === false; }).length;
  chk(tDown > 0 && tUp > 0 && ps("door1").length > pBefore &&
      Math.abs(TW.state().door1 - vBefore) > 0.01 && !TW.doorAnim,
      "dragging the handle swings the door and sends door1 inside a gesture",
      "touch " + tDown + "/" + tUp + ", p " + (ps("door1").length - pBefore) +
      ", aperture " + vBefore.toFixed(3) + " -> " + TW.state().door1.toFixed(3));

  /*  A drag on the handle must not ALSO be a look, or the room spins while
      you open a door. */
  chk(ps("lisyaw").length === yawBefore,
      "grabbing a handle is not a look - the head does not turn",
      "lisyaw messages during the drag: " + (ps("lisyaw").length - yawBefore));

  /* --- a click with no movement throws it, or slams it ------------------ */
  TW.setParam("door1", 1);
  TW.render();
  var clickAt = aimAt("door1");
  hoverAt(clickAt);
  pov.dispatchEvent(new PointerEvent("pointerdown", { bubbles:true, pointerId:13, button:0, buttons:1, clientX:clickAt[0], clientY:clickAt[1] }));
  window.dispatchEvent(new PointerEvent("pointerup", { bubbles:true, pointerId:13, buttons:0, clientX:clickAt[0], clientY:clickAt[1] }));
  var animArmed = TW.doorAnim, atClick = TW.state().door1;
  var pAtClick = ps("door1").length;
  setTimeout(function (){
    setTimeout(function (){
      var shut = TW.state().door1;
      /*  A door that TELEPORTED from 1 to 0 would satisfy the end points and
          nothing else, so the SWING is what is measured - and it is measured
          off the values the page actually sent, not off a wall clock this
          harness does not keep. A jump sends one value; a swing sends a run
          of them, none of which is either end. */
      var swing = ps("door1").slice(pAtClick).map(function (m){ return m.msg.v; });
      var between = swing.filter(function (v){ return v > 0.02 && v < 0.98; }).length;
      chk(animArmed && atClick > 0.98 && between >= 2 && shut < 0.02 && !TW.doorAnim,
          "a click with no movement slams the door, and swings rather than jumps",
          "armed " + animArmed + ", value at the click " + atClick.toFixed(3) +
          ", " + swing.length + " values sent of which " + between + " part way" +
          ", settled " + shut.toFixed(3));
      var upsBefore = sentOf("touch").filter(function (t){ return t.msg.id === "door1" && t.msg.down === false; }).length;
      TW.render();
      var p2 = aimAt("door1");
      hoverAt(p2);
      pov.dispatchEvent(new PointerEvent("pointerdown", { bubbles:true, pointerId:14, button:0, buttons:1, clientX:p2[0], clientY:p2[1] }));
      window.dispatchEvent(new PointerEvent("pointerup", { bubbles:true, pointerId:14, buttons:0, clientX:p2[0], clientY:p2[1] }));
      setTimeout(function (){
        var open = TW.state().door1;
        var ups = sentOf("touch").filter(function (t){ return t.msg.id === "door1" && t.msg.down === false; }).length;
        chk(open > 0.98 && !TW.doorAnim && ups > upsBefore,
            "clicking it again throws it wide, and the gesture is closed behind it",
            "aperture " + open.toFixed(3) + ", touch ups " + upsBefore + " -> " + ups);
        materialChecks();
      }, 520);
    }, 420);
  }, 120);

  /* --- 17 a material list longer than the page ships with ---------------- */
  function materialChecks(){
    /*  One entry longer than whatever the protocol currently lists, with a
        name that is certainly not already in it - the engine is about to gain
        STUDIO, and a probe that appended that name would start testing a list
        with the same entry twice the day the document catches up. */
    var extra = MATS.indexOf("STUDIO") < 0 ? "STUDIO" : "ANECHOIC";
    var grown = MATS.concat([extra]);
    function fireMats(list){
      window.__fire("initialState", {
        build:"260921.1", showrays:1, selsrc:0, auxConnected:0,
        wav:{ name:"", playing:0, seconds:0, gain:0.75 },
        params: PARAMS.map(function (p){
          var o = { id:p[0], name:p[1], v:p[2], stepped:!!p[3], steps:p[4], choices:p[5] };
          if (/^mat[123]$/.test(o.id)){
            o.stepped = true; o.steps = list.length; o.choices = list.join("|");
            o.v = 1 / (list.length - 1);
          }
          return o;
        })
      });
    }
    fireMats(grown);
    var sel = document.querySelector('select[data-id="mat1"]');
    var names = Array.prototype.map.call(sel.options, function (o){ return o.textContent; });
    /*  Every entry reachable, and the value each one stands for is
        index/(steps-1) - the one piece of arithmetic the protocol states about
        a choice parameter. */
    var wrong = [], sigs = [];
    for (var i = 0; i < grown.length; i++){
      var v = i / (grown.length - 1);
      TW.setParam("mat1", v); TW.setParam("mat2", v); TW.setParam("mat3", v);
      if (sel.value !== String(i)) wrong.push(grown[i] + " -> option " + sel.value);
      TW.render();
      sigs.push(planSig());
    }
    chk(names.length === grown.length && names.join("|") === grown.join("|") && wrong.length === 0,
        "a " + grown.length + "-entry material list renders " + grown.length + " and reads back the right one",
        names.length + " options [" + names.join(",") + "]" +
        (wrong.length ? ", wrong: " + wrong.join(", ") : ", every value correct"));

    /*  and each of them has to reach the PICTURE. A page that still divided by
        three would land two of these five values on the same material and draw
        the identical plan twice, which is exactly what this measures. */
    var worst = 1e9, pair = "";
    for (var a = 0; a < sigs.length; a++) for (var b = a+1; b < sigs.length; b++){
      var dd = sigDiff(sigs[a], sigs[b]);
      if (dd < worst){ worst = dd; pair = grown[a] + "/" + grown[b]; }
    }
    chk(worst > 0.08, "all " + grown.length + " materials draw a different plan",
        "closest pair " + pair + " differs by " + worst.toFixed(3) + " levels mean");

    fireMats(MATS);                    /* put the page back as native has it */
    var back = Array.prototype.map.call(sel.options, function (o){ return o.textContent; });
    chk(back.length === MATS.length && back.join("|") === MATS.join("|"),
        "and a list that shrinks again takes the selector back with it",
        back.length + " options [" + back.join(",") + "]");

    /*  Back to where the rest of the gate expects to find things, and drawn
        once so nothing is owed - otherwise the idle checks below would be
        measuring this block instead of the page. */
    TW.setParam("lisx", 0.25); TW.setParam("lisy", 0.2778); TW.setParam("lisyaw", 0.5);
    TW.render();
    setTimeout(function (){
      fireScenes(1, -10);              /* past the 100 ms window: allowed again */
      chk(TW.pending === true, "past 100 ms the glow may redraw again", "booked " + TW.pending);
      TW.render();
      idleCheck();
    }, 160);
  }

  /* --- 15 idle costs nothing ---------------------------------------------- */
  function idleCheck(){
    var d0 = window.__TW.draws;
    setTimeout(function (){
      var d1 = window.__TW.draws;
      chk(d1 - d0 === 0, "zero draws over a second of idle", d0 + " -> " + d1);

      /* one last look for anything that threw along the way */
      chk(window.__err.length === 0, "still no JS errors after driving the page",
          window.__err.length ? window.__err.slice(0,3).join(" | ") : "0");
      R.push("renderer=" + window.__TW.renderer);
      R.push("draws=" + window.__TW.draws);
      R.push("messages=" + window.__sent.length);
      out();
    }, 1000);
  }
})();
</script>
`;

let page = src;
if (page.indexOf("<!-- probe-stub -->") < 0){
  console.error("the page has no probe-stub marker; refusing to guess where the stub goes");
  process.exit(2);
}
page = page.replace("<!-- probe-stub -->", STUB);
const endBody = page.lastIndexOf("</body>");
page = page.slice(0, endBody) + DRIVER + page.slice(endBody);

const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), "twprobe-"));
const tmpFile = path.join(tmpDir, "ui-probe.html");
fs.writeFileSync(tmpFile, page, "utf8");

/* ------------------------------------------------------------ run chrome */
if (!fs.existsSync(CHROME)){
  console.error("chrome not found at " + CHROME);
  process.exit(2);
}
const args = [
  "--headless=new",
  "--dump-dom",
  "--virtual-time-budget=14000",
  "--window-size=" + WIN.w + "," + WIN.h,
  "--hide-scrollbars",
  "--allow-file-access-from-files",
  "--enable-unsafe-swiftshader",
  "--use-angle=swiftshader",
  "--user-data-dir=" + path.join(tmpDir, "chrome"),
  "--no-first-run",
  "--no-default-browser-check",
  "file:///" + tmpFile.replace(/\\/g, "/")
];
const run = cp.spawnSync(CHROME, args, { encoding:"utf8", maxBuffer: 64 * 1024 * 1024 });
const dom = run.stdout || "";

const m = dom.match(/<pre id="probeout">([\s\S]*?)<\/pre>/);
if (!m){
  console.log(lines.join("\n"));
  console.error("\nthe driver produced no output - the page probably threw before it ran.");
  const errs = dom.match(/__err[^<]{0,400}/);
  if (run.stderr) console.error(String(run.stderr).split("\n").slice(0, 12).join("\n"));
  process.exit(1);
}
const decoded = m[1]
  .replace(/&lt;/g, "<").replace(/&gt;/g, ">")
  .replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&amp;/g, "&");

const info = [];
for (const raw of decoded.split("\n")){
  const line = raw.trim();
  if (!line) continue;
  if (/^(PASS|FAIL) /.test(line)) chk(line.startsWith("PASS"), line.slice(5));
  else info.push(line);
}

console.log("THIN WALLS ui probe");
console.log("file    " + UI);
console.log("bytes   " + fs.statSync(UI).size);
console.log("window  " + WIN.w + "x" + WIN.h);
console.log("");
console.log(lines.join("\n"));
console.log("");
for (const i of info) console.log("        " + i);
console.log("");
console.log(fail === 0 ? ("ALL CLEAR - " + pass + " checks") : (fail + " CHECK(S) FAILED of " + (pass + fail)));

if (KEEP) console.log("kept: " + tmpFile);
else { try { fs.rmSync(tmpDir, { recursive:true, force:true }); } catch (e){} }
process.exit(fail === 0 ? 0 : 1);
