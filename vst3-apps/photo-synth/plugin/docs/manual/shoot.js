/* Screenshot harness for the Photo-Synth 2 manual.
 *
 * Drives a headless Chrome over the DevTools protocol: for each job it runs a
 * setup expression in the page, waits, then captures either the whole viewport
 * or the exact bounds of one element at 2x, so the plates come out crisp and
 * tightly cropped like the existing plates in the manual.
 *
 * node shoot.js <page-url> <out-dir> <jobs.json>
 */
const net = require("net");
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");

const PORT = Number(process.env.SHOOT_PORT || 9333);
const pageUrl = process.argv[2];
const outDir = process.argv[3];
const jobs = JSON.parse(fs.readFileSync(process.argv[4], "utf8"));

function httpJson(pathname) {
  return new Promise(function (res, rej) {
    require("http").get({ host: "127.0.0.1", port: PORT, path: pathname }, function (r) {
      let d = "";
      r.on("data", function (c) { d += c; });
      r.on("end", function () { try { res(JSON.parse(d)); } catch (e) { rej(e); } });
    }).on("error", rej);
  });
}

function connect(wsUrl) {
  return new Promise(function (resolve, reject) {
    const m = wsUrl.match(/ws:\/\/([^:]+):(\d+)(\/.*)/);
    const key = crypto.randomBytes(16).toString("base64");
    const sock = net.connect(Number(m[2]), m[1], function () {
      sock.write("GET " + m[3] + " HTTP/1.1\r\nHost: " + m[1] +
                 "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key +
                 "\r\nSec-WebSocket-Version: 13\r\n\r\n");
    });
    let up = false, buf = Buffer.alloc(0), id = 0;
    const pending = new Map();
    function send(method, params) {
      return new Promise(function (res) {
        const msg = { id: ++id, method: method, params: params || {} };
        pending.set(msg.id, res);
        const p = Buffer.from(JSON.stringify(msg));
        const mask = crypto.randomBytes(4);
        let h;
        if (p.length < 126) h = Buffer.from([0x81, 0x80 | p.length]);
        else if (p.length < 65536) { h = Buffer.alloc(4); h[0] = 0x81; h[1] = 0xFE; h.writeUInt16BE(p.length, 2); }
        else { h = Buffer.alloc(10); h[0] = 0x81; h[1] = 0xFF; h.writeBigUInt64BE(BigInt(p.length), 2); }
        for (let i = 0; i < p.length; i++) p[i] ^= mask[i & 3];
        sock.write(Buffer.concat([h, mask, p]));
      });
    }
    sock.on("data", function (d) {
      if (!up) {
        const s = d.toString("latin1");
        const i = s.indexOf("\r\n\r\n");
        if (i < 0) return;
        up = true;
        resolve({ send: send });
        d = d.slice(i + 4);
        if (!d.length) return;
      }
      buf = Buffer.concat([buf, d]);
      for (;;) {
        if (buf.length < 2) return;
        const op = buf[0] & 0x0f;
        let len = buf[1] & 0x7f, off = 2;
        if (len === 126) { if (buf.length < 4) return; len = buf.readUInt16BE(2); off = 4; }
        else if (len === 127) { if (buf.length < 10) return; len = Number(buf.readBigUInt64BE(2)); off = 10; }
        if (buf.length < off + len) return;
        const payload = buf.slice(off, off + len).toString("utf8");
        buf = buf.slice(off + len);
        if (op !== 1) continue;
        try {
          const j = JSON.parse(payload);
          if (j.id && pending.has(j.id)) { pending.get(j.id)(j); pending.delete(j.id); }
        } catch (e) { /* event we do not care about */ }
      }
    });
    sock.on("error", reject);
  });
}

const sleep = function (ms) { return new Promise(function (r) { setTimeout(r, ms); }); };

(async function () {
  const list = await httpJson("/json");
  const page = list.find(function (t) { return t.type === "page"; });
  if (!page) throw new Error("no page target");
  const cdp = await connect(page.webSocketDebuggerUrl);

  await cdp.send("Page.enable");
  await cdp.send("Runtime.enable");
  await cdp.send("Emulation.setDeviceMetricsOverride",
    { width: 1400, height: 1000, deviceScaleFactor: 2, mobile: false });
  await cdp.send("Page.navigate", { url: pageUrl });
  await sleep(2600);

  for (const job of jobs) {
    if (job.setup) {
      const r = await cdp.send("Runtime.evaluate",
        { expression: "(function(){" + job.setup + "})()", returnByValue: true, awaitPromise: false });
      if (r.result && r.result.exceptionDetails)
        console.log("  setup error in " + job.name + ": " + JSON.stringify(r.result.exceptionDetails).slice(0, 200));
    }
    await sleep(job.wait || 650);

    let clip;
    if (job.selector) {
      const r = await cdp.send("Runtime.evaluate", {
        returnByValue: true,
        expression: "(function(){var e=document.querySelector(" + JSON.stringify(job.selector) + ");" +
                    "if(!e) return null; var b=e.getBoundingClientRect();" +
                    "var p=" + (job.pad || 0) + ";" +
                    "return JSON.stringify({x:Math.max(0,b.left-p),y:Math.max(0,b.top-p)," +
                    "w:b.width+2*p,h:b.height+2*p});})()"
      });
      const v = r.result && r.result.result && r.result.result.value;
      if (!v) { console.log("  MISSING element for " + job.name + " (" + job.selector + ")"); continue; }
      const b = JSON.parse(v);
      clip = { x: b.x, y: b.y, width: b.w, height: b.h, scale: 2 };
    }

    const shot = await cdp.send("Page.captureScreenshot",
      clip ? { format: "png", clip: clip, captureBeyondViewport: true } : { format: "png" });
    const data = shot.result && shot.result.data;
    if (!data) { console.log("  capture failed for " + job.name); continue; }
    const file = path.join(outDir, job.name + ".png");
    fs.writeFileSync(file, Buffer.from(data, "base64"));
    console.log("  " + job.name + ".png  " + (fs.statSync(file).size / 1024).toFixed(0) + " kB" +
                (clip ? "  " + Math.round(clip.width) + "x" + Math.round(clip.height) : "  viewport"));
  }
  process.exit(0);
})().catch(function (e) { console.error("shoot failed:", e.message); process.exit(1); });
