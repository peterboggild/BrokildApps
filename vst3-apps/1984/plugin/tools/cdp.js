/*  Minimal Chrome DevTools Protocol driver - no dependencies.
 *
 *  Static checks cannot see an unclosed tag, a helper that does not exist, or a
 *  layer that paints nothing. The only thing that proves a WebView panel works
 *  is driving the live one, which is what this is for.
 *
 *    node tools/cdp.js <port> <jobs.json>
 *
 *  jobs.json is an array of { name, eval }, { name, wait } (ms),
 *  { name, shoot } (png path) or { name, cmd, params } (a raw CDP call).
 */
const http = require("http");
const net = require("net");
const crypto = require("crypto");

const PORT = parseInt(process.argv[2] || "9244", 10);
const JOBS = process.argv[3];

function getJson(path) {
  return new Promise((res, rej) => {
    http.get({ host: "127.0.0.1", port: PORT, path }, r => {
      let b = "";
      r.on("data", d => b += d);
      r.on("end", () => { try { res(JSON.parse(b)); } catch (e) { rej(e); } });
    }).on("error", rej);
  });
}

class WS {
  constructor(url) {
    const m = /^ws:\/\/([^:/]+):(\d+)(\/.*)$/.exec(url);
    if (!m) throw new Error("bad ws url " + url);
    this.host = m[1]; this.port = +m[2]; this.path = m[3];
    this.id = 0; this.pending = new Map(); this.buf = Buffer.alloc(0);
  }
  connect() {
    return new Promise((res, rej) => {
      const key = crypto.randomBytes(16).toString("base64");
      this.sock = net.connect(this.port, this.host, () => {
        this.sock.write(
          `GET ${this.path} HTTP/1.1\r\nHost: ${this.host}:${this.port}\r\n` +
          `Upgrade: websocket\r\nConnection: Upgrade\r\n` +
          `Sec-WebSocket-Key: ${key}\r\nSec-WebSocket-Version: 13\r\n\r\n`);
      });
      let handshook = false;
      this.sock.on("data", d => {
        if (!handshook) {
          const s = d.toString("latin1");
          const i = s.indexOf("\r\n\r\n");
          if (i < 0) return;
          if (!/101/.test(s.slice(0, 20))) return rej(new Error("no upgrade: " + s.slice(0, 80)));
          handshook = true;
          const rest = d.slice(Buffer.byteLength(s.slice(0, i + 4), "latin1"));
          if (rest.length) this.onData(rest);
          res();
          return;
        }
        this.onData(d);
      });
      this.sock.on("error", rej);
    });
  }
  onData(d) {
    this.buf = Buffer.concat([this.buf, d]);
    for (;;) {
      if (this.buf.length < 2) return;
      const len0 = this.buf[1] & 127;
      let off = 2, len = len0;
      if (len0 === 126) { if (this.buf.length < 4) return; len = this.buf.readUInt16BE(2); off = 4; }
      else if (len0 === 127) { if (this.buf.length < 10) return; len = Number(this.buf.readBigUInt64BE(2)); off = 10; }
      if (this.buf.length < off + len) return;
      const payload = this.buf.slice(off, off + len).toString("utf8");
      this.buf = this.buf.slice(off + len);
      try {
        const msg = JSON.parse(payload);
        if (msg.id && this.pending.has(msg.id)) {
          const { res } = this.pending.get(msg.id);
          this.pending.delete(msg.id);
          res(msg);
        }
      } catch (_) {}
    }
  }
  send(method, params) {
    const id = ++this.id;
    const body = Buffer.from(JSON.stringify({ id, method, params }), "utf8");
    const head = [];
    head.push(0x81);
    if (body.length < 126) head.push(0x80 | body.length);
    else if (body.length < 65536) { head.push(0x80 | 126, body.length >> 8 & 255, body.length & 255); }
    else { head.push(0x80 | 127, 0,0,0,0, body.length>>24&255, body.length>>16&255, body.length>>8&255, body.length&255); }
    const mask = crypto.randomBytes(4);
    const masked = Buffer.alloc(body.length);
    for (let i = 0; i < body.length; ++i) masked[i] = body[i] ^ mask[i & 3];
    this.sock.write(Buffer.concat([Buffer.from(head), mask, masked]));
    return new Promise(res => this.pending.set(id, { res }));
  }
}

const sleep = ms => new Promise(r => setTimeout(r, ms));

(async () => {
  let targets = [];
  for (let i = 0; i < 40; ++i) {
    try { targets = await getJson("/json"); if (targets.length) break; } catch (_) {}
    await sleep(500);
  }
  const page = targets.find(t => t.type === "page") || targets[0];
  if (!page) { console.error("no CDP target on port " + PORT); process.exit(1); }
  console.log("target: " + (page.title || page.url));

  const ws = new WS(page.webSocketDebuggerUrl);
  await ws.connect();
  await ws.send("Runtime.enable", {});
  await ws.send("Page.enable", {});

  const jobs = JSON.parse(require("fs").readFileSync(JOBS, "utf8"));
  let failures = 0;
  for (const j of jobs) {
    if (j.wait) { await sleep(j.wait); continue; }
    /* A raw CDP command. Media queries key off the VIEWPORT, so an element
       resized from JS proves nothing about a responsive layout - the only
       honest narrow test is Emulation.setDeviceMetricsOverride, and Chrome
       will not open a window narrower than ~500px to do it for you. */
    if (j.cmd) {
      const r = await ws.send(j.cmd, j.params || {});
      console.log(`  ${j.name || j.cmd}
     ${r.error ? "ERROR " + JSON.stringify(r.error) : "ok"}`);
      continue;
    }
    if (j.shoot) {
      /* an element or rect plate: clip in PAGE coordinates (the scroll added), rendered at scale */
      let params = { format: "png" };
      let b = null;
      if (j.rect) b = { x: j.rect[0], y: j.rect[1], w: j.rect[2], h: j.rect[3] };
      else if (j.sel) {
        const expr = "(function(){var e=document.querySelector(" + JSON.stringify(j.sel) + ");if(!e)return null;var r=e.getBoundingClientRect();return {x:r.left+window.scrollX,y:r.top+window.scrollY,w:r.width,h:r.height};})()";
        const q = await ws.send("Runtime.evaluate", { expression: expr, returnByValue: true });
        b = q.result && q.result.result && q.result.result.value;
        if (!b) { failures++; console.log("  " + (j.name || "shot") + "\n     NO ELEMENT " + j.sel); continue; }
        if (j.sel2) {
          const expr2 = "(function(){var e=document.querySelector(" + JSON.stringify(j.sel2) + ");if(!e)return null;var r=e.getBoundingClientRect();return {x:r.left+window.scrollX,y:r.top+window.scrollY,w:r.width,h:r.height};})()";
          const q2 = await ws.send("Runtime.evaluate", { expression: expr2, returnByValue: true });
          const c = q2.result && q2.result.result && q2.result.result.value;
          if (c) { const x0 = Math.min(b.x, c.x), y0 = Math.min(b.y, c.y), x1 = Math.max(b.x + b.w, c.x + c.w), y1 = Math.max(b.y + b.h, c.y + c.h); b = { x: x0, y: y0, w: x1 - x0, h: y1 - y0 }; }
        }
      }
      if (b) {
        const pd = typeof j.pad === "object" && j.pad ? j.pad : { l: j.pad || 0, t: j.pad || 0, r: j.pad || 0, b: j.pad || 0 };
        params = { format: "png", captureBeyondViewport: true, clip: { x: b.x - (pd.l || 0), y: b.y - (pd.t || 0), width: b.w + (pd.l || 0) + (pd.r || 0), height: b.h + (pd.t || 0) + (pd.b || 0), scale: j.scale || 2 } };
      }
      const r = await ws.send("Page.captureScreenshot", params);
      const b64 = r.result && r.result.data;
      if (b64) {
        require("fs").writeFileSync(j.shoot, Buffer.from(b64, "base64"));
        console.log(`  ${j.name || "shot"}\n     ${j.shoot}`);
      } else {
        failures++;
        console.log(`  ${j.name || "shot"}\n     FAILED to capture`);
      }
      continue;
    }
    const r = await ws.send("Runtime.evaluate", {
      expression: j.eval, returnByValue: true, awaitPromise: true
    });
    const res = r.result && r.result.result;
    const exc = r.result && r.result.exceptionDetails;
    if (exc) {
      failures++;
      console.log(`  ${j.name}\n     THREW: ${exc.exception ? exc.exception.description : exc.text}`);
    } else {
      const v = res ? (res.value !== undefined ? res.value : res.description) : "(none)";
      const text = typeof v === "object" ? JSON.stringify(v) : String(v);
      console.log(`  ${j.name}\n     ${text}`);
    }
  }
  process.exit(failures ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
