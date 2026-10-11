/*  Magneto Transport - the panel probe (stage 8). No dependencies.

      node test/uiprobe.js [--shots <dir>] [--keep]

    Serves Source/ui/ui.html with the card fixtures in test/fixtures/, loads it in
    headless Chrome with window.__JUCE__.backend stubbed BEFORE the page script runs,
    feeds it an initialState built from Source/Params.h's SPECS table (plus the HEAD
    PATTERN parameter), a presets event and 30 Hz telemetry, and checks what a static
    read cannot: that it draws, fits, opens its cards beside their controls, moves
    its live markers and needles, and sends what the contract says it sends.
    Prints a numbered PASS / FAIL list and exits non-zero on any FAIL.
    --shots <dir> writes panel.png, card.png, presets.png, gear.png (and a few more).
*/
"use strict";
const fs = require("fs");
const path = require("path");
const http = require("http");
const zlib = require("zlib");
const net = require("net");
const crypto = require("crypto");
const os = require("os");
const { spawn } = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const UI = path.join(ROOT, "Source", "ui", "ui.html");
const FIX = path.join(__dirname, "fixtures");
const args = process.argv.slice(2);
const SHOTS = args.includes("--shots") ? path.resolve(args[args.indexOf("--shots") + 1]) : null;
if (SHOTS) fs.mkdirSync(SHOTS, { recursive: true });

// ------------------------------------------------------------- the parameter table
function readSpecs(){
  const src = fs.readFileSync(path.join(ROOT, "Source", "Params.h"), "utf8");
  const defines = {}, consts = {};
  for (const f of ["Params.h", "HeadPattern.h"]){
    const p = path.join(ROOT, "Source", f); if (!fs.existsSync(p)) continue;
    const s = fs.readFileSync(p, "utf8");
    for (const m of s.matchAll(/#define\s+(\w+)\s+"([^"]*)"/g)) defines[m[1]] = m[2];
    for (const m of s.matchAll(/constexpr\s+int\s+(\w+)\s*=\s*(\d+)/g)) consts[m[1]] = +m[2];
  }
  const body = src.slice(src.indexOf("SPECS[]"));
  const re = /\{\s*"([^"]+)",\s*"([^"]+)",\s*Kind::(\w+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*"([^"]*)",\s*([^,]+),\s*&Params::\w+,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\}/g;
  const num = s => { let e = s.trim().replace(/(\d)f\b/g, "$1"); for (const k in consts) e = e.replace(new RegExp("\\b" + k + "\\b", "g"), consts[k]); return Function("return (" + e + ")")(); };
  const out = [];
  for (const m of body.matchAll(re)){
    const ch = m[8].trim();
    const choices = ch === "nullptr" ? "" : ch.startsWith('"') ? ch.slice(1, -1) : (defines[ch] || "");
    const hint = [...m[9].matchAll(/"((?:[^"\\]|\\.)*)"/g)].map(q => q[1]).join("");
    const kind = { Lin: "lin", Log: "log", Choice: "choice", Toggle: "toggle" }[m[3]];
    const def = num(m[6]);
    out.push({ id: m[1], name: m[2], kind, lo: num(m[4]), hi: num(m[5]), def, unit: m[7], choices, hint, v: def });
  }
  if (!out.find(p => p.id === "pattern"))
    out.push({ id: "pattern", name: "HEAD PATTERN", kind: "choice", lo: 0, hi: 6, def: 0, unit: "",
               choices: defines.MT_PATTERN_NAMES || "1:2:3|1:1.5:2|1:1.33:2|1:2:4|1:1.5:3|1:3:4|GOLDEN",
               hint: "The ratios SET snaps the heads to, around the tempo head.", v: 0 });
  return out;
}
const PARAMS = readSpecs();
const PART_IDS = ["part:supply", "part:takeup", "part:looptank", "part:erase", "part:record", "part:play", "part:capstan", "part:pinch",
                  "part:guides", "part:tape", "part:rail", "read:wf", "read:vu", "read:tapemeter", "read:loop", "set:tooltips",
                  "set:quality", "set:presets", "part:pattern"];
const UNIVERSE = new Set(PARAMS.map(p => p.id).concat(PART_IDS));

// ------------------------------------------------------------- the server
let serveFixtures = true, serveReal = false, serveDecals = null;
// synthetic decals for phase D: small PNGs, a disc on transparent or a flat fill, per docs/decal_parts.json
const DECAL_SPEC = JSON.parse(fs.readFileSync(path.join(ROOT, "docs", "decal_parts.json"), "utf8"));
function crc32(buf){ let c, crc = 0xffffffff; for (let n = 0; n < buf.length; n++){ c = (crc ^ buf[n]) & 0xff; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crc = (crc >>> 8) ^ c; } return (crc ^ 0xffffffff) >>> 0; }
function pngChunk(type, data){ const len = Buffer.alloc(4); len.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td)); return Buffer.concat([len, td, crc]); }
function makePng(w, h, px){
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++){ raw[y * (w * 4 + 1)] = 0; for (let x = 0; x < w; x++){ const c = px(x, y), o = y * (w * 4 + 1) + 1 + x * 4; raw[o] = c[0]; raw[o + 1] = c[1]; raw[o + 2] = c[2]; raw[o + 3] = c[3]; } }
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), pngChunk("IHDR", ihdr), pngChunk("IDAT", zlib.deflateSync(raw)), pngChunk("IEND", Buffer.alloc(0))]);
}
function syntheticDecals(){
  const m = {};
  DECAL_SPEC.parts.forEach((p, i) => {
    const sz = DECAL_SPEC.sizes[p.size], w = Math.round(sz[0] / 16), h = Math.round(sz[1] / 16);
    const col = [60 + (i * 37) % 160, 70 + (i * 53) % 150, 60 + (i * 29) % 140];
    m[p.file] = makePng(w, h, (x, y) => {
      if (p.kind === "O") return [col[0], col[1], col[2], 255];
      const dx = (x - w / 2) / (w / 2), dy = (y - h / 2) / (h / 2), inside = dx * dx + dy * dy < 0.8;
      return inside ? [col[0], col[1], col[2], p.kind === "S" ? 90 : 255] : [0, 0, 0, 0];
    });
  });
  return m;
}
function servedTips(){
  const fx = JSON.parse(fs.readFileSync(path.join(FIX, "tips.json"), "utf8"));
  const cards = Object.assign({}, fx.cards);
  PARAMS.forEach(p => { if (!cards[p.id]) cards[p.id] = { title: p.name, does: "<p>" + p.hint.replace(/&/g, "&amp;").replace(/</g, "&lt;") + "</p>", hood: "", notModelled: "" }; });
  PART_IDS.forEach(id => { if (!cards[id]) cards[id] = { title: id.split(":")[1].toUpperCase(), does: "<p>Fixture card for " + id + ".</p>", hood: "", notModelled: "" }; });
  return { version: 1, cards };
}
const server = http.createServer((req, res) => {
  const u = decodeURIComponent(req.url.split("?")[0]);
  const send = (code, type, body) => { res.writeHead(code, { "Content-Type": type, "Cache-Control": "no-store" }); res.end(body); };
  if (u === "/" || u === "/ui.html") return send(200, "text/html; charset=utf-8", fs.readFileSync(UI));
  if (u.startsWith("/decals/")){ const n = u.slice(8).replace(/\.png$/, ""); return serveDecals && serveDecals[n] ? send(200, "image/png", serveDecals[n]) : send(404, "text/plain", "no decal"); }
  if (serveReal && ["/tips.json", "/tips-numbers.json", "/figdata.json"].includes(u)){
    const p = path.join(ROOT, "Source", "ui", u.slice(1));
    return fs.existsSync(p) ? send(200, "application/json", fs.readFileSync(p)) : send(404, "text/plain", "none");
  }
  if (!serveFixtures) return send(404, "text/plain", "no fixtures");
  if (u === "/tips.json") return send(200, "application/json", JSON.stringify(servedTips()));
  if (u === "/tips-numbers.json" || u === "/figdata.json") return send(200, "application/json", fs.readFileSync(path.join(FIX, u.slice(1))));
  send(404, "text/plain", "not found");
});

// ------------------------------------------------------------- CDP
function getJson(port, p){
  return new Promise((res, rej) => {
    http.get({ host: "127.0.0.1", port, path: p }, r => { let b = ""; r.on("data", d => b += d); r.on("end", () => { try { res(JSON.parse(b)); } catch (e) { rej(e); } }); }).on("error", rej);
  });
}
class WS {
  constructor(url){ const m = /^ws:\/\/([^:/]+):(\d+)(\/.*)$/.exec(url); this.host = m[1]; this.port = +m[2]; this.path = m[3]; this.id = 0; this.pending = new Map(); this.buf = Buffer.alloc(0); }
  connect(){
    return new Promise((res, rej) => {
      const key = crypto.randomBytes(16).toString("base64");
      this.sock = net.connect(this.port, this.host, () => this.sock.write(`GET ${this.path} HTTP/1.1\r\nHost: ${this.host}:${this.port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: ${key}\r\nSec-WebSocket-Version: 13\r\n\r\n`));
      let hs = false;
      this.sock.on("data", d => {
        if (!hs){ const s = d.toString("latin1"), i = s.indexOf("\r\n\r\n"); if (i < 0) return; if (!/101/.test(s.slice(0, 20))) return rej(new Error("no upgrade")); hs = true;
                  const rest = d.slice(i + 4); if (rest.length) this.onData(rest); return res(); }
        this.onData(d);
      });
      this.sock.on("error", rej);
    });
  }
  onData(d){
    this.buf = Buffer.concat([this.buf, d]);
    for (;;){
      if (this.buf.length < 2) return;
      const l0 = this.buf[1] & 127; let off = 2, len = l0;
      if (l0 === 126){ if (this.buf.length < 4) return; len = this.buf.readUInt16BE(2); off = 4; }
      else if (l0 === 127){ if (this.buf.length < 10) return; len = Number(this.buf.readBigUInt64BE(2)); off = 10; }
      if (this.buf.length < off + len) return;
      const payload = this.buf.slice(off, off + len).toString("utf8"); this.buf = this.buf.slice(off + len);
      try { const msg = JSON.parse(payload); if (msg.id && this.pending.has(msg.id)){ const p = this.pending.get(msg.id); this.pending.delete(msg.id); p(msg); } } catch (_) {}
    }
  }
  send(method, params){
    const id = ++this.id, data = Buffer.from(JSON.stringify({ id, method, params: params || {} }), "utf8"), mask = crypto.randomBytes(4);
    let hdr;
    if (data.length < 126) hdr = Buffer.from([0x81, 0x80 | data.length]);
    else if (data.length < 65536){ hdr = Buffer.alloc(4); hdr[0] = 0x81; hdr[1] = 0x80 | 126; hdr.writeUInt16BE(data.length, 2); }
    else { hdr = Buffer.alloc(10); hdr[0] = 0x81; hdr[1] = 0x80 | 127; hdr.writeBigUInt64BE(BigInt(data.length), 2); }
    const body = Buffer.alloc(data.length); for (let i = 0; i < data.length; i++) body[i] = data[i] ^ mask[i & 3];
    this.sock.write(Buffer.concat([hdr, mask, body]));
    return new Promise((res, rej) => { this.pending.set(id, m => m.error ? rej(new Error(method + ": " + JSON.stringify(m.error))) : res(m.result)); setTimeout(() => rej(new Error(method + " timed out")), 20000); });
  }
}
const sleep = ms => new Promise(r => setTimeout(r, ms));
function findChrome(){
  const c = [process.env.CHROME, "C:/Program Files/Google/Chrome/Application/chrome.exe", "C:/Program Files (x86)/Google/Chrome/Application/chrome.exe",
             path.join(process.env.LOCALAPPDATA || "", "Google/Chrome/Application/chrome.exe"), "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"];
  return c.find(p => p && fs.existsSync(p));
}

// ------------------------------------------------------------- the data the native side would send
const INIT = { build: "261011.probe", rate: 96000, params: PARAMS, quality: 1, settings: { tooltips: true, delay: 0.8 }, preset: { name: "SPACE ECHO 201", dirty: false } };
const PRESETS = {
  factory: [
    { index: 0, name: "STUDIO CLEAN", group: "CLEAN" }, { index: 1, name: "SLAPBACK 38", group: "CLEAN" }, { index: 2, name: "TRANSPARENT DOUBLE", group: "CLEAN" },
    { index: 3, name: "SPACE ECHO 201", group: "CLASSIC ECHO" }, { index: 4, name: "DUB SIREN TAIL", group: "CLASSIC ECHO" }, { index: 5, name: "ROCKABILLY", group: "CLASSIC ECHO" },
    { index: 6, name: "THREE HEADS 1:2:3", group: "MULTI-HEAD" }, { index: 7, name: "DOTTED GALLOP", group: "MULTI-HEAD" }, { index: 8, name: "GOLDEN SPIRAL", group: "MULTI-HEAD" },
    { index: 9, name: "WORN BUDGET MACHINE", group: "CHARACTER" }, { index: 10, name: "SHEDDING TAPE", group: "CHARACTER" }, { index: 11, name: "WARM SATURATION", group: "CHARACTER" },
    { index: 12, name: "FOUR BAR LOOP", group: "LOOPS" }, { index: 13, name: "FLUX MEMORY GHOSTS", group: "LOOPS" },
    { index: 14, name: "BROKEN RADIO", group: "STRANGE" }, { index: 15, name: "TAPE STOP DIVE", group: "STRANGE" } ],
  user: [{ name: "My Dub Loop" }, { name: "Late Night" }],
  current: { name: "SPACE ECHO 201", factory: 3, dirty: false } };
const tape = Array.from({ length: 160 }, (_, i) => { const x = -6 + 33 * i / 160; return x < 0 ? 0 : Math.round(1000 * (0.6 + 0.5 * Math.sin(i / 4)) * Math.exp(-Math.max(0, x - 20) / 4)); });
const TELE = { speed: 19, target: 19, cap: 0, supA: 0, takA: 0, supR: 0.122, takR: 0.07, loopA: 0, mode: 0, loopLen: 2, syncOn: false, syncClamp: false,
  splice: false, in: 0.3, out: 0.25, rec: 0.42, wf: 0.036, wear: 0, stock: 0, looper: -1, recording: true, eraseHc: 2.25, barsFit: true, barsNow: 0,
  brokenEff: [15, 100, 100, 100, 0, 0], clogUm: 0, huntPct: 0, crease: 0, repro: false, reproMs: 0, heads: [4, 8, 12], headVel: [0, 0, 0],
  headPk: [0.3, 0, 0], tape, tapeFrom: -6, tapeTo: 27, vuIn: -40, vuOut: -40, quality: 1, latencyMs: 0, patternFit: { mask: 0 } };
const STUB = `
window.__sent = []; window.__ls = {};
window.__JUCE__ = { backend: {
  emitEvent(n, o){ window.__sent.push(JSON.parse(JSON.stringify(o))); },
  addEventListener(n, f){ (window.__ls[n] = window.__ls[n] || []).push(f); } } };
window.__fire = (n, o) => (window.__ls[n] || []).forEach(f => f(o));
`;

// ------------------------------------------------------------- the run
const results = [];
let n = 0;
function check(name, ok, detail){ results.push({ n: ++n, name, ok: !!ok, detail: detail === undefined ? "" : String(detail) }); console.log(`${String(n).padStart(2)}. ${ok ? "PASS" : "FAIL"}  ${name}${detail !== undefined && detail !== "" ? "  -  " + detail : ""}`); }
const warns = [];

(async () => {
  await new Promise(r => server.listen(0, "127.0.0.1", r));
  const httpPort = server.address().port;
  const chrome = findChrome();
  if (!chrome){ console.log("FAIL: no chrome.exe found"); process.exit(2); }
  const port = 9400 + Math.floor(Math.random() * 400);
  const udd = fs.mkdtempSync(path.join(os.tmpdir(), "mt-uiprobe-"));
  const proc = spawn(chrome, ["--headless=new", "--disable-gpu", "--no-first-run", "--no-default-browser-check", "--hide-scrollbars", "--mute-audio",
    `--remote-debugging-port=${port}`, `--user-data-dir=${udd}`, "--window-size=1600,900", "about:blank"], { stdio: "ignore" });
  let target = null;
  for (let i = 0; i < 80 && !target; i++){ await sleep(150); try { const l = await getJson(port, "/json"); target = l.find(t => t.type === "page"); } catch (_) {} }
  if (!target){ console.log("FAIL: chrome did not start"); proc.kill(); process.exit(2); }
  const ws = new WS(target.webSocketDebuggerUrl); await ws.connect();
  await ws.send("Page.enable"); await ws.send("Runtime.enable");
  await ws.send("Emulation.setDeviceMetricsOverride", { width: 1600, height: 900, deviceScaleFactor: 1, mobile: false });
  await ws.send("Page.addScriptToEvaluateOnNewDocument", { source: STUB });
  const ev = async (expr) => {
    const r = await ws.send("Runtime.evaluate", { expression: expr, returnByValue: true, awaitPromise: true });
    if (r.exceptionDetails) throw new Error("eval: " + (r.exceptionDetails.exception && r.exceptionDetails.exception.description || r.exceptionDetails.text) + "\n  in: " + expr.slice(0, 200));
    return r.result.value;
  };
  const mouse = async (type, x, y) => ws.send("Input.dispatchMouseEvent", { type, x, y, button: type === "mouseMoved" ? "none" : "left", clickCount: type === "mouseMoved" ? 0 : 1 });
  const click = async (x, y) => { await mouse("mouseMoved", x, y); await mouse("mousePressed", x, y); await mouse("mouseReleased", x, y); };
  const clickEl = async sel => { const c = await ev(`(()=>{const e=document.querySelector(${JSON.stringify(sel)}); if(!e) return null; const r=e.getBoundingClientRect(); return [r.left+r.width/2, r.top+r.height/2];})()`); if (!c) throw new Error("no element " + sel); await click(c[0], c[1]); };
  const key = async (k, code, vk) => { await ws.send("Input.dispatchKeyEvent", { type: "keyDown", key: k, code, windowsVirtualKeyCode: vk }); await ws.send("Input.dispatchKeyEvent", { type: "keyUp", key: k, code, windowsVirtualKeyCode: vk }); };
  const shot = async name => { if (!SHOTS) return; const r = await ws.send("Page.captureScreenshot", { format: "png" }); fs.writeFileSync(path.join(SHOTS, name), Buffer.from(r.data, "base64")); };
  const load = async () => { await ws.send("Page.navigate", { url: `http://127.0.0.1:${httpPort}/ui.html` }); for (let i = 0; i < 100; i++){ await sleep(100); try { if (await ev("document.readyState === 'complete' && !!window.__MT")) break; } catch (_) {} } };
  const sent = async () => ev("window.__sent");
  const clearSent = async () => ev("window.__sent.length = 0, true");
  const overlap = (a, b) => !(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);
  const rectOf = async sel => ev(`(()=>{const e=document.querySelector(${JSON.stringify(sel)}); const d=document.getElementById('deck').getBoundingClientRect(), r=e.getBoundingClientRect(), s=+document.getElementById('deck').dataset.scale; return {x:(r.left-d.left)/s,y:(r.top-d.top)/s,w:r.width/s,h:r.height/s};})()`);

  try {
    // ---------------------------------------------------------------- phase A: no card files at all
    serveFixtures = false;
    await load();
    await ev(`window.__fire('initialState', ${JSON.stringify(INIT)}), true`);
    await sleep(400);
    const fbCard = await ev(`(()=>{__MT.openCard('bias'); const t=document.querySelector('#card .ct').textContent, b=document.querySelector('#card .cb').textContent; __MT.closeCard(); return t+' | '+b.slice(0,60);})()`);
    const errA = await ev("window.__errors.slice()");
    check("without tips.json / tips-numbers.json / figdata.json the page degrades: no errors, the card falls back to the hint", errA.length === 0 && /BIAS/.test(fbCard) && /bias/i.test(fbCard), errA.length ? errA.join(" | ") : fbCard);

    // ---------------------------------------------------------------- phase B: the fixtures
    serveFixtures = true;
    await load();
    const hello = await sent();
    check("hello sent on boot", hello.some(m => m.k === "hello"));
    await ev(`window.__fire('initialState', ${JSON.stringify(INIT)}), true`);
    await ev(`window.__fire('presets', ${JSON.stringify(PRESETS)}), true`);
    await ev(`window.__teleBase = ${JSON.stringify(TELE)}; window.__teleOv = {}; window.__teleTimer = setInterval(() => window.__fire('tele', Object.assign({}, window.__teleBase, window.__teleOv)), 33); true`);
    await sleep(700);
    const s1 = await sent();
    check("stateack and presetList sent after initialState", s1.some(m => m.k === "stateack") && s1.some(m => m.k === "presetList"));
    check("tips, numbers and figures loaded", await ev("__MT.loaded.tips && __MT.loaded.nums && __MT.loaded.figs"));

    const missingCtl = await ev(`${JSON.stringify(PARAMS.map(p => p.id))}.filter(id => !__MT.ctls[id] || !__MT.ctls[id].el.isConnected)`);
    check("every parameter in initialState has a control", missingCtl.length === 0, missingCtl.length ? "missing: " + missingCtl.join(" ") : PARAMS.length + " parameters");

    const grp = await ev(`[...document.querySelectorAll('.grp')].map(g => ({n:g.querySelector('h3').textContent, over: g.scrollHeight > g.clientHeight + 1 || g.scrollWidth > g.clientWidth + 1, s: g.scrollWidth+'x'+g.scrollHeight+' in '+g.clientWidth+'x'+g.clientHeight}))`);
    check("every group fits its box", grp.every(g => !g.over), grp.filter(g => g.over).map(g => g.n + " " + g.s).join(" | ") || grp.length + " groups");

    // nothing inside a group spills outside it either (e.g. a control pushed past the edge)
    const spill = await ev(`(()=>{const out=[];document.querySelectorAll('.grp').forEach(g=>{const r=g.getBoundingClientRect();g.querySelectorAll('.ctl,.btn,.seg,.knob,.hsl,.window,#loopRead,#loopWhy,#patMsg,#patFit').forEach(e=>{const q=e.getBoundingClientRect();if(!q.width)return;if(q.left<r.left-0.5||q.right>r.right+0.5||q.top<r.top-0.5||q.bottom>r.bottom+0.5)out.push(g.querySelector('h3').textContent+':'+(e.dataset.id||e.id||e.className));});});return out;})()`);
    check("no control pokes out of its plate", spill.length === 0, spill.join(" | "));
    // nor sits on top of another (a fixed width can be beaten by a later rule and a neighbour slides under it)
    const lap = await ev(`(()=>{const out=[];const els=[...document.querySelectorAll('#deck .ctl, #wfRead, #patSet, #patFit, #patMsg, #loopRead, #loopWhy, .delay, .vubox, #tapebox, #presetBox, #gear, #build, #speedbox, #lamps, #brand')].filter(e=>e.getClientRects().length);
      const inside=(a,b)=>a!==b&&b.contains(a);
      for(let i=0;i<els.length;i++)for(let j=i+1;j<els.length;j++){const a=els[i],b=els[j];if(inside(a,b)||inside(b,a))continue;
        const p=a.getBoundingClientRect(),q=b.getBoundingClientRect();
        if(p.left<q.right-1&&q.left<p.right-1&&p.top<q.bottom-1&&q.top<p.bottom-1)out.push((a.dataset.id||a.id||a.className)+' / '+(b.dataset.id||b.id||b.className));}
      // and inside each control, the children do not overlap each other
      document.querySelectorAll('#deck .ctl').forEach(c=>{const k=[...c.children].filter(e=>e.getClientRects().length);for(let i=0;i<k.length;i++)for(let j=i+1;j<k.length;j++){const p=k[i].getBoundingClientRect(),q=k[j].getBoundingClientRect();if(p.left<q.right-1&&q.left<p.right-1&&p.top<q.bottom-1&&q.top<p.bottom-1)out.push(c.dataset.id+': '+k[i].className+' / '+k[j].className);}});
      return out;})()`);
    check("no two controls (or readouts) overlap", lap.length === 0, lap.slice(0, 10).join(" | "));

    const head = await ev(`(()=>{var h=document.getElementById('head'),b=document.getElementById('build').getBoundingClientRect(),r=h.getBoundingClientRect();return {over:h.scrollWidth>h.clientWidth+1||b.right>r.right-4, txt:'content '+h.scrollWidth+' in '+h.clientWidth+', build ends '+Math.round(b.right)}})()`);
    check("header fits (build id inside it)", !head.over, head.txt);
    check("wordmark reads exactly 'Magneto Transport' and the model line 'MODEL MT-1'", await ev("document.getElementById('wordmark').textContent==='Magneto Transport' && document.getElementById('model').textContent==='MODEL MT-1'"));

    // text that is clipped by its own box (labels, values, keys)
    const clipped = await ev(`[...document.querySelectorAll('#deck label, #deck .val, .seg b, .btn, .lampbox, #readout, #readout2, #pNameText')].filter(e => e.getClientRects().length && e.scrollWidth > e.clientWidth + 1 && getComputedStyle(e).textOverflow !== 'ellipsis').map(e => (e.closest('[data-id]') ? e.closest('[data-id]').dataset.id : e.id || e.className) + ':' + e.textContent.trim()).slice(0, 12)`);
    check("no label, value or key is clipped", clipped.length === 0, clipped.join(" | "));

    // ---- the card map
    const ids = await ev("Object.keys(__MT.cards())");
    const need = PARAMS.map(p => p.id).concat(PART_IDS.filter(i => i !== "part:looptank"));
    const lack = need.filter(i => !ids.includes(i));
    check("every control and machine part is registered for a card (REEL)", lack.length === 0, lack.length ? "unregistered: " + lack.join(" ") : ids.length + " ids");
    await ev("window.__teleOv.mode = 1, true"); await sleep(300);
    const idsLoop = await ev("Object.keys(__MT.cards())");
    check("the loop tank is registered in LOOP mode", idsLoop.includes("part:looptank") && !idsLoop.includes("part:supply"));
    await ev("window.__teleOv.mode = 0, true"); await sleep(200);
    const tipsServed = servedTips();
    const allIds = [...new Set(ids.concat(idsLoop))];
    const noCard = allIds.filter(i => !tipsServed.cards[i]);
    const strange = allIds.filter(i => !UNIVERSE.has(i));
    check("every registered id has a card in the (fixture) tips.json, and names one the protocol knows", noCard.length === 0 && strange.length === 0,
      (noCard.length ? "no card: " + noCard.join(" ") + " " : "") + (strange.length ? "unknown ids: " + strange.join(" ") : ""));
    const real = path.join(ROOT, "Source", "ui", "tips.json");
    if (fs.existsSync(real)){
      try { const rt = JSON.parse(fs.readFileSync(real, "utf8")); const miss = allIds.filter(i => !(rt.cards || {})[i]); if (miss.length) warns.push("the real Source/ui/tips.json has no card for: " + miss.join(" ")); }
      catch (e) { warns.push("Source/ui/tips.json does not parse: " + e.message); }
    } else warns.push("Source/ui/tips.json does not exist yet (cards fall back to the parameter hints)");

    // ---- open a card: beside its control, inside the deck
    const placeOk = async (id, nn, sel) => {
      await ev(`__MT.openCard(${JSON.stringify(id)}, ${nn || 0}), true`); await sleep(60);
      const c = await ev("__MT.card");
      const el = sel ? await rectOf(sel) : await ev(`(()=>{const m=__MT.cards()[${JSON.stringify(id)}]; const e=Array.isArray(m)?m[0]:m; if (e && e.region) { const r = (${nn || 0} && e.all.length) ? e.all.find(a=>a.n===${nn || 0}).region : e.region; return {x:r[0],y:r[1],w:r[2],h:r[3]}; } const d=document.getElementById('deck').getBoundingClientRect(), r=e.getBoundingClientRect(), s=+document.getElementById('deck').dataset.scale; return {x:(r.left-d.left)/s,y:(r.top-d.top)/s,w:r.width/s,h:r.height/s};})()`);
      const p = c.placed;
      const inside = p && p.x >= 0 && p.y >= 0 && p.x + p.w <= 1600 && p.y + p.h <= 900;
      return { ok: c.open && p && inside && !overlap(p, el), txt: JSON.stringify(p) + " vs " + JSON.stringify({ x: Math.round(el.x), y: Math.round(el.y), w: Math.round(el.w), h: Math.round(el.h) }) };
    };
    for (const [id, nn] of [["bias"], ["drive"], ["looper"], ["output"], ["h3pan"], ["part:capstan"], ["part:supply"], ["part:play", 3], ["read:vu", 2], ["set:presets"], ["part:tape"]]){
      const r = await placeOk(id, nn);
      check(`card '${id}${nn ? " " + nn : ""}' opens beside its element, inside the deck`, r.ok, r.txt);
    }
    check("the playback head card names its head", (await ev("__MT.openCard('part:play', 2), document.querySelector('#card .ct').textContent")) === "PLAYBACK HEAD 2");

    // ---- numbers and figures
    await ev("__MT.openCard('bias'), true"); await sleep(100);
    const nums = await ev("[...document.querySelectorAll('#card span[data-num]')].map(s => s.textContent)");
    check("numbers resolved from tips-numbers.json", nums.length === 2 && nums.includes("−34.5 dB") && nums.includes("3.26 Hc"), nums.join(", "));
    const mathOk = await ev("!!document.querySelector('#card math') && document.querySelector('#card math').getBoundingClientRect().width > 50");
    check("MathML renders in the card", mathOk);
    const fig = await ev("(()=>{const s=document.querySelector('#card figure svg'); return s ? {paths: s.querySelectorAll('path').length, texts: [...s.querySelectorAll('text')].map(t=>t.textContent), live: !!s.querySelector('.live')} : null})()");
    check("the bias figure draws: two series, axes with units, legend, marks, live marker", fig && fig.paths >= 2 && fig.texts.some(t => /bias \(Hc\)/.test(t)) && fig.texts.includes("1 kHz") && fig.texts.includes("operating") && fig.live,
      fig ? fig.texts.slice(0, 14).join(" / ") : "no svg");
    const px0 = +(await ev("document.querySelector('#card .live').dataset.px"));
    await ev("__MT.set('bias', 6), true"); await sleep(250);
    const px1 = +(await ev("document.querySelector('#card .live').dataset.px"));
    const lab1 = await ev("document.querySelector('#card .live text').textContent");
    check("the live marker moves with the parameter (db_to_ratio: BIAS 0 -> +6 dB)", px1 > px0 + 20 && /now 6\.5/.test(lab1), px0 + " -> " + px1 + ", " + lab1);
    await ev("__MT.set('bias', 0), true");
    await ev("__MT.openCard('erase'), true"); await sleep(100);
    await ev("__MT.set('erase', 10), true"); await sleep(200);
    const e0 = +(await ev("document.querySelector('#card .live').dataset.x"));
    await ev("__MT.set('erase', 90), true"); await sleep(200);
    const e1 = +(await ev("document.querySelector('#card .live').dataset.x"));
    check("a table-mapped marker follows ERASE (10 % -> 0.92 Hc, 90 % -> 1.57 Hc)", Math.abs(e0 - 0.92) < 0.01 && Math.abs(e1 - 1.57) < 0.01, e0 + " / " + e1);
    await ev("__MT.openCard('speed', 0, true), true"); await sleep(150);
    const sp0 = +(await ev("document.querySelector('#card .live').dataset.px"));
    await ev("window.__teleOv.speed = 38, true"); await sleep(300);
    const sp1 = +(await ev("document.querySelector('#card .live').dataset.px"));
    check("a pinned card's telemetry marker keeps updating (speed 19 -> 38 cm/s, log axis)", sp1 > sp0 + 20 && (await ev("__MT.card.pinned")), sp0 + " -> " + sp1);
    await ev("window.__teleOv.speed = 19, true");
    await ev("__MT.openCard('part:capstan', 0, true), true"); await sleep(120);
    await shot("card.png");
    await ev("__MT.closeCard(), true");

    // ---- hover with a real pointer (after a pause: a card closed a moment ago makes the next one open warm)
    await sleep(800);
    const dr = await rectOf('[data-id="drive"]');
    await mouse("mouseMoved", dr.x + dr.w / 2, dr.y + dr.h / 2);
    await sleep(350);
    const early = await ev("__MT.card.open");
    const hintNow = await ev("document.getElementById('hintText').textContent");
    await sleep(800);
    const hc = await ev("__MT.card");
    const ptr = { x: dr.x + dr.w / 2 - 1, y: dr.y + dr.h / 2 - 1, w: 2, h: 2 };
    check("hover: the hint bar answers at once, the card after the 0.8 s delay, beside the knob and clear of the pointer",
      !early && /input amplifier/i.test(hintNow) && hc.open && hc.id === "drive" && !overlap(hc.placed, dr) && !overlap(hc.placed, ptr), JSON.stringify(hc.placed) + " / hint: " + hintNow);
    await mouse("mouseMoved", 800, 885); await sleep(600);
    check("moving away closes an unpinned card", !(await ev("__MT.card.open")));
    // hover a machine part
    const capR = await ev("__MT.cards()['part:capstan'].region");
    await mouse("mouseMoved", capR[0] + capR[2] / 2, capR[1] + capR[3] / 2);
    await sleep(1200);
    const mc = await ev("__MT.card");
    check("hover on the machine drawing opens the part's card (capstan)", mc.open && mc.id === "part:capstan" && !overlap(mc.placed, { x: capR[0], y: capR[1], w: capR[2], h: capR[3] }), JSON.stringify(mc));
    // click on the card pins it; Esc closes
    await click(mc.placed.x + 40, mc.placed.y + 60); await sleep(100);
    const pinned = await ev("__MT.card.pinned");
    await mouse("mouseMoved", 800, 885); await sleep(600);
    const stillOpen = await ev("__MT.card.open");
    await key("Escape", "Escape", 27); await sleep(100);
    check("clicking the card pins it (it survives the pointer leaving); Esc closes it", pinned && stillOpen && !(await ev("__MT.card.open")));

    // ---- VU needles with ballistics
    await ev("window.__teleOv.vuIn = -38, window.__teleOv.vuOut = -38, true"); await sleep(900);
    const v0 = await ev("__MT.vu()");
    await ev("window.__teleOv.vuIn = -18, true"); await sleep(90);
    const vMid = await ev("__MT.vu()");
    await sleep(900);
    const v1 = await ev("__MT.vu()");
    check("the IN needle moves with vuIn, with VU ballistics (0 VU at -18 dBFS; not there yet after 90 ms)",
      v1.inDeg > v0.inDeg + 30 && Math.abs(v1.in - Math.pow(10, -3 / 20)) < 0.02 && vMid.in < v1.in * 0.85 && Math.abs(v1.outDeg - v0.outDeg) < 1,
      `${v0.inDeg.toFixed(1)} deg -> ${vMid.inDeg.toFixed(1)} (90 ms) -> ${v1.inDeg.toFixed(1)}; OUT ${v1.outDeg.toFixed(1)}`);
    await ev("window.__teleOv.vuOut = -20, window.__teleOv.in = 0.95, true"); await sleep(300);
    check("the peak lamp lights on a peak above -1 dBFS", (await ev("document.querySelector('#vuIn .pk').getAttribute('fill')")) === "#ff4a2c");
    await ev("window.__teleOv.in = 0.3, true");

    // ---- HEAD PATTERN
    await ev("window.__teleOv.patternFit = {mask: 4}, true"); await sleep(200);
    check("patternFit says which head is out of reach", (await ev("document.getElementById('patFit').textContent")) === "HEAD 3 OUT OF REACH");
    await ev("window.__teleOv.patternFit = {mask: 6}, true"); await sleep(200);
    check("... and two of them", (await ev("document.getElementById('patFit').textContent")) === "HEADS 2 + 3 OUT OF REACH");
    await ev("window.__teleOv.patternFit = {mask: 0}, true");
    await clearSent();
    const st0 = await ev("__MT.P.pattern.v");
    await clickEl('[data-id="pattern"] .sk:last-child'); await sleep(80);
    const sp = await sent();
    check("the pattern stepper sets parameter 'pattern' (with a gesture)", sp.some(m => m.k === "p" && m.id === "pattern" && m.v === st0 + 1) && sp.some(m => m.k === "touch" && m.id === "pattern"),
      await ev("document.querySelector('[data-id=pattern] .sw').textContent"));
    await clearSent();
    await clickEl("#patSet"); await sleep(80);
    check("SET sends patternSet", (await sent()).some(m => m.k === "patternSet"));
    await ev(`window.__fire('patternResult', {ok:false, outMask:4, pos:[4,8,24]}), true`); await sleep(80);
    const pm = await ev("({t: document.getElementById('patMsg').textContent, c: document.getElementById('patMsg').className})");
    check("patternResult flashes what happened", /HEAD 3 CLAMPED/.test(pm.t) && pm.c === "bad", pm.t);
    await ev(`window.__fire('patternResult', {ok:true, outMask:0, pos:[4,6,8]}), true`); await sleep(80);
    check("... and a clean snap", /SET: 4\.0 · 6\.0 · 8\.0 cm/.test(await ev("document.getElementById('patMsg').textContent")));

    // ---- presets
    await clearSent();
    await clickEl("#pName"); await sleep(250);
    const pmn = await ev("({open: document.getElementById('presetMenu').classList.contains('open'), heads: [...document.querySelectorAll('#presetMenu .pmh')].map(h=>h.textContent), cur: (document.querySelector('#presetMenu .pmi.cur')||{}).textContent})");
    check("the preset menu lists the groups in order, USER last, the current one marked",
      pmn.open && JSON.stringify(pmn.heads) === JSON.stringify(["CLEAN", "CLASSIC ECHO", "MULTI-HEAD", "CHARACTER", "LOOPS", "STRANGE", "USER"]) && pmn.cur === "SPACE ECHO 201", JSON.stringify(pmn));
    const pmRect = await rectOf("#presetMenu");
    check("the preset menu sits inside the deck", pmRect.x >= 0 && pmRect.y >= 0 && pmRect.x + pmRect.w <= 1600 && pmRect.y + pmRect.h <= 900, JSON.stringify(pmRect));
    await shot("presets.png");
    await clickEl('#presetMenu .pmi[data-factory="7"]'); await sleep(120);
    const pl = await sent();
    check("choosing a factory preset sends presetLoad {factory} and asks for the list again", pl.some(m => m.k === "presetLoad" && m.factory === 7) && pl.some(m => m.k === "presetList"));
    await ev("__MT.set('drive', 3), true"); await sleep(50);
    check("a changed control marks the preset name with *", /\*$/.test(await ev("document.getElementById('pNameText').textContent")), await ev("document.getElementById('pNameText').textContent"));
    await clearSent();
    await clickEl("#pName"); await sleep(150);
    await clickEl('#presetMenu .pmi[data-user="Late Night"] .del'); await sleep(80);
    const conf = await ev("document.querySelector('#presetMenu .pmi[data-user=\"Late Night\"] .del').textContent");
    const before = (await sent()).filter(m => m.k === "presetDelete").length;
    await clickEl('#presetMenu .pmi[data-user="Late Night"] .del'); await sleep(80);
    check("deleting a user preset asks once, then sends presetDelete", conf === "DELETE?" && before === 0 && (await sent()).some(m => m.k === "presetDelete" && m.user === "Late Night"));
    await key("Escape", "Escape", 27); await sleep(80);
    await clearSent();
    await clickEl("#pSave"); await sleep(150);
    const dlgOpen = await ev("document.getElementById('dlg').classList.contains('open') && document.activeElement && document.activeElement.id === 'dlgName'");
    await ws.send("Input.insertText", { text: "Bad/Name" }); await clickEl("#dlgOk"); await sleep(80);
    const refused = (await sent()).filter(m => m.k === "presetSave").length === 0 && /cannot hold/.test(await ev("document.getElementById('dlgNote').textContent"));
    await ev("document.getElementById('dlgName').value = '', document.getElementById('dlgName').focus(), true");
    await ws.send("Input.insertText", { text: "My Tape Echo" });
    await shot("save.png");
    await clickEl("#dlgOk"); await sleep(120);
    const sv = await sent();
    check("SAVE opens the name dialog; a path character is refused; presetSave carries the typed name", dlgOpen && refused && sv.some(m => m.k === "presetSave" && m.name === "My Tape Echo") && !(await ev("document.getElementById('dlg').classList.contains('open')")),
      JSON.stringify(sv.filter(m => m.k === "presetSave")));
    await ev(`window.__fire('presets', ${JSON.stringify(Object.assign({}, PRESETS, { user: PRESETS.user.concat([{ name: "My Tape Echo" }]), current: { name: "My Tape Echo", factory: -1, dirty: false } }))}), true`);
    check("the presets event updates the name (USER)", (await ev("document.getElementById('pNameText').textContent")) === "My Tape Echo");

    // ---- the gear
    await clearSent();
    await clickEl("#gear"); await sleep(200);
    const gOpen = await ev("document.getElementById('gearMenu').classList.contains('open')");
    const gRect = await rectOf("#gearMenu");
    await shot("gear.png");
    await clickEl("#gTips"); await sleep(80);
    const gs = await sent();
    check("the gear menu opens inside the deck; TOOLTIPS off sends settings", gOpen && gRect.x + gRect.w <= 1600 && gs.some(m => m.k === "settings" && m.tooltips === false && m.delay === 0.8), JSON.stringify(gs));
    const dRect = await rectOf("#gDelay");
    await clearSent();
    await mouse("mouseMoved", dRect.x + dRect.w - 1, dRect.y + dRect.h / 2); await mouse("mousePressed", dRect.x + dRect.w - 1, dRect.y + dRect.h / 2); await mouse("mouseReleased", dRect.x + dRect.w - 1, dRect.y + dRect.h / 2); await sleep(80);
    check("the HOVER DELAY slider sends settings with the new delay (3.0 s at the right end)", (await sent()).some(m => m.k === "settings" && m.delay === 3), await ev("document.getElementById('gDelayVal').textContent"));
    await clearSent();
    await clickEl("#gQual b:nth-child(3)"); await sleep(80);
    check("QUALITY HIGH sends {k:'quality', v:2}", (await sent()).some(m => m.k === "quality" && m.v === 2));
    await clickEl("#gTips"); await sleep(50);
    await key("Escape", "Escape", 27); await sleep(80);
    check("Esc closes the gear menu", !(await ev("document.getElementById('gearMenu').classList.contains('open')")));
    // tooltips off: no card on hover
    await ev("__MT.settings.tooltips = false, true");
    await mouse("mouseMoved", dr.x + dr.w / 2, dr.y + dr.h / 2); await sleep(1100);
    check("with TOOLTIPS off, hovering opens no card", !(await ev("__MT.card.open")));
    await ev("__MT.settings.tooltips = true, true");
    await mouse("mouseMoved", 800, 885); await sleep(400);

    // ---- the existing behaviour: LOOP readout and dimming, REPRO, BROKEN
    await ev("__MT.set('mode', 1); __MT.set('looper', 1); window.__teleOv.mode = 1; window.__teleOv.looper = 1; window.__teleOv.eraseHc = 2.25; window.__teleOv.reRecDb = -3.1; true"); await sleep(300);
    const lp = await ev("({idle: document.getElementById('loopGrp').classList.contains('idle'), read: document.getElementById('loopRead').textContent, why: document.getElementById('loopWhy').textContent, eraseIdle: __MT.ctls.erase.el.classList.contains('idle')})");
    check("LOOP: the panel lights, the readout and the reason read as before", !lp.idle && /LOOP DUB \| recording \| re-recorded in place, -3\.1 dB a pass/.test(lp.read) && /^DUB:/.test(lp.why) && !lp.eraseIdle, lp.read + " / " + lp.why);
    await shot("loop.png");
    await ev("__MT.set('mode', 0); window.__teleOv.mode = 0; window.__teleOv.looper = -1; window.__teleOv.repro = true; window.__teleOv.reproMs = 210.4; true"); await sleep(300);
    const rp = await ev("({btn: document.querySelector('[data-id=repro] .btn').textContent, h: document.querySelector('[data-id=repro] .btn').getBoundingClientRect().height, inert: [...document.querySelectorAll('[data-head=\"2\"],[data-head=\"3\"]')].every(e=>e.classList.contains('inert')), fb: document.querySelector('[data-id=feedback]').closest('.grp').classList.contains('inert'), loopIdle: document.getElementById('loopGrp').classList.contains('idle')})");
    check("REPRO: the button shows the delay on one line; heads 2, 3 and FEEDBACK dim; REEL dims the loop panel", /REPRO 210 ms/.test(rp.btn) && rp.h < 30 && rp.inert && rp.fb && rp.loopIdle, JSON.stringify(rp));
    await ev("window.__teleOv.repro = false; __MT.set('broken', 75); window.__teleOv.brokenEff = [70, 160, 150, 150, 55, -3]; window.__teleOv.clogUm = 0.4; window.__teleOv.huntPct = 2; true"); await sleep(300);
    const bk = await ev("({dots: ['condition','capstan','reels','scrape','wear','bias'].map(k => __MT.ctls[k].el.querySelector('.knob').classList.contains('broken')), val: document.querySelector('[data-id=broken] .val').textContent, title: __MT.ctls.condition.el.querySelector('.val').title})");
    check("BROKEN: red dots on the knobs it moves, the faults named, 'acts at' titles", bk.dots.every(Boolean) && /CLOG HUNT/.test(bk.val) && /act at 70 %/.test(bk.title), JSON.stringify(bk));
    const grp2 = await ev(`[...document.querySelectorAll('.grp')].filter(g => g.scrollHeight > g.clientHeight + 1 || g.scrollWidth > g.clientWidth + 1).map(g => g.querySelector('h3').textContent)`);
    check("every group still fits with BROKEN faults named", grp2.length === 0, grp2.join(" "));
    await shot("broken.png");
    await ev("__MT.set('broken', 0); window.__teleOv.brokenEff = [15,100,100,100,0,0]; window.__teleOv.clogUm = 0; window.__teleOv.huntPct = 0; true"); await sleep(250);
    check("OK again: the dots are gone", (await ev("['condition','capstan','reels','scrape','wear','bias'].every(k => !__MT.ctls[k].el.querySelector('.knob').classList.contains('broken'))")));

    // ---- head drag on the rail still works
    await clearSent();
    const h1 = await ev("__MT.cards()['part:play'].all.find(a => a.n === 1).region");
    const hx = h1[0] + h1[2] / 2, hy = h1[1] + h1[3] / 2;
    await mouse("mouseMoved", hx, hy); await mouse("mousePressed", hx, hy); await mouse("mouseMoved", hx + 60, hy); await mouse("mouseReleased", hx + 60, hy); await sleep(80);
    const dg = await sent();
    check("dragging a playback head on the drawing sets its position (with a gesture)", dg.some(m => m.k === "p" && m.id === "h1pos" && m.v > 4.5) && dg.some(m => m.k === "touch" && m.id === "h1pos" && m.down === false));

    // ---- geom()
    const gm = await ev("__MT.geom()");
    const kinds = new Set(gm.map(g => g.kind));
    const need2 = ["canvas", "plate", "knob", "button", "seg", "key", "slider", "stepper", "meter-vu", "meter-bar", "lamp", "window", "screw", "gear"];
    const outOf = gm.filter(g => g.x < -0.5 || g.y < -0.5 || g.x + g.w > 1600.5 || g.y + g.h > 900.5);
    check("geom() lists every visual part type, all inside the deck", need2.every(k => kinds.has(k)) && outOf.length === 0 && gm.length > 100,
      gm.length + " parts; missing kinds: " + need2.filter(k => !kinds.has(k)).join(" ") + (outOf.length ? "; outside: " + outOf.map(g => g.id).join(" ") : ""));

    await shot("panel.png");
    // ---- the MECH slot: a knob when the table carries 'mech' (it does since stage 10), a blank plug when not
    const slot1 = await ev("!!(__MT.ctls.mech && __MT.ctls.mech.el.querySelector('.knob')) && !document.querySelector('[data-id=mech] .plug')");
    const grp3 = await ev(`[...document.querySelectorAll('.grp')].filter(g => g.scrollHeight > g.clientHeight + 1 || g.scrollWidth > g.clientWidth + 1).map(g => g.querySelector('h3').textContent)`);
    const INIT0 = Object.assign({}, INIT, { params: PARAMS.filter(p => p.id !== "mech") });
    await ev(`window.__fire('initialState', ${JSON.stringify(INIT0)}), true`); await sleep(200);
    const slot0 = await ev("!!document.querySelector('[data-id=mech] .plug') && !__MT.ctls.mech");
    check("MECH SOUNDS is a knob beside OUTPUT (and the plate still fits); without 'mech' the slot is a blank plug", slot0 && slot1 && grp3.length === 0,
      "knob " + slot1 + ", plug without it " + slot0 + (grp3.length ? ", overflowing: " + grp3.join(" ") : ""));
    await ev(`window.__fire('initialState', ${JSON.stringify(INIT)}), true`); await sleep(150);
    check("the real table again: the knob comes back", await ev("!!(__MT.ctls.mech && __MT.ctls.mech.el.querySelector('.knob'))"));
    const errs = await ev("window.__errors.slice()");
    check("no entries in window.__errors", errs.length === 0, errs.join(" | "));

    // ---- phase D: the decals. The loader asks for every part the order lists; a full
    // synthetic set must be taken up whole, move nothing, and log nothing.
    const names = await ev("__MT.decalNames.slice()");
    const want = DECAL_SPEC.parts.map(p => p.file);
    check("the panel's decal list is the order's list (docs/decal_parts.json)", JSON.stringify(names) === JSON.stringify(want),
      "panel " + names.length + ", order " + want.length);
    check("with no decals served the panel uses none", (await ev("__MT.decals().length")) === 0);
    const geomOf = () => ev("__MT.geom().map(g => g.kind + ':' + g.id + ':' + g.x + ',' + g.y + ',' + g.w + ',' + g.h)");
    const gA = await geomOf();
    serveDecals = syntheticDecals();
    await load();
    await ev(`window.__fire('initialState', ${JSON.stringify(INIT)}), true`);
    await ev(`window.__teleBase = ${JSON.stringify(TELE)}; setInterval(() => window.__fire('tele', window.__teleBase), 33); true`);
    await sleep(900);
    const dRep = await ev("({ n: __MT.decals().length, vud: document.querySelectorAll('svg.vu .vud').length, wm: getComputedStyle(document.documentElement).getPropertyValue('--wm').length, kd: [...document.querySelectorAll('.knob .kd')].filter(e => getComputedStyle(e).display !== 'none').length, knobs: document.querySelectorAll('.knob').length, errs: window.__errors.slice() })");
    check("a full set of decals is taken up whole: every part, the VU faces, the wordmark, every knob", dRep.n === want.length && dRep.vud === 2 && dRep.wm > 20 && dRep.kd === dRep.knobs && dRep.knobs > 20,
      dRep.n + "/" + want.length + " parts, " + dRep.vud + " VU faces, wordmark " + (dRep.wm > 20) + ", knobs " + dRep.kd + "/" + dRep.knobs);
    await shot("panel-decals.png");
    const gB = await geomOf();
    const moved = gA.filter((g, i) => g !== gB[i]);
    check("the decals move nothing: every measured part keeps its place and size", gA.length === gB.length && moved.length === 0,
      gA.length + " parts" + (moved.length ? "; moved: " + moved.slice(0, 4).join(" | ") : ""));
    check("no errors with the decals in", dRep.errs.length === 0 && (await ev("window.__errors.length")) === 0, dRep.errs.join(" | "));
    serveDecals = null;

    // ---- phase C (report only): the REAL generated card files, every card opened once
    serveReal = true;
    await load();
    await ev(`window.__fire('initialState', ${JSON.stringify(INIT)}), true`);
    await ev(`window.__teleBase = ${JSON.stringify(TELE)}; window.__teleOv = {}; setInterval(() => window.__fire('tele', Object.assign({}, window.__teleBase, window.__teleOv)), 33); true`);
    await sleep(600);
    const realRep = await ev(`(()=>{const ids=Object.keys(__MT.cards());let figs=0;ids.forEach(id=>{__MT.openCard(id); figs+=document.querySelectorAll('#card figure svg').length;});__MT.closeCard();return {n:ids.length, figs, errs: window.__errors.slice(), loaded: __MT.loaded};})()`);
    if (realRep.errs.length) warns.push("with the REAL card files, opening all " + realRep.n + " cards logged: " + [...new Set(realRep.errs)].join(" | "));
    else warns.push("with the REAL card files: " + realRep.n + " cards open cleanly, " + realRep.figs + " figures drawn (loaded " + JSON.stringify(realRep.loaded) + ")");
  } catch (e) {
    check("probe ran to the end", false, e.message);
  } finally {
    try { await ws.send("Browser.close"); } catch (_) {}
    setTimeout(() => { try { proc.kill(); } catch (_) {} }, 300);
    server.close();
  }
  warns.forEach(w => console.log("WARN  " + w));
  const bad = results.filter(r => !r.ok).length;
  console.log(bad ? `${bad} of ${results.length} FAILED` : `ALL ${results.length} PASS`);
  if (SHOTS) console.log("screenshots in " + SHOTS);
  setTimeout(() => process.exit(bad ? 1 : 0), 500);
})();
