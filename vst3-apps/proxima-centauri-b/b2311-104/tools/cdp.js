/*  A small Chrome DevTools Protocol driver — no dependencies, hand-rolled
    WebSocket. Used to drive the live plugin panel: evaluate expressions in it,
    read state back, and screenshot elements.

      node cdp.js <port> <jobs.json>

    A job is  {"eval": "<expression>"}  |  {"wait": ms}
              {"shoot": {"sel": "#x"|null, "out": "a.png", "pad": 0, "scale": 2}}

    Screenshot clips are in PAGE coordinates, not viewport, so scrollX/scrollY
    have to be added and captureBeyondViewport set — a clip taken straight from
    getBoundingClientRect shoots blank the moment the document is scrolled. */
const http = require('http');
const net = require('net');
const crypto = require('crypto');
const fs = require('fs');

const PORT = parseInt(process.argv[2] || '9241', 10);
const JOBS = process.argv[3];

function get(path) {
  return new Promise((res, rej) => {
    http.get({ host: '127.0.0.1', port: PORT, path }, r => {
      let d = ''; r.on('data', c => d += c); r.on('end', () => res(d));
    }).on('error', rej);
  });
}

class WS {
  constructor(url) {
    const m = url.match(/^ws:\/\/([^:/]+):(\d+)(\/.*)$/);
    this.host = m[1]; this.port = +m[2]; this.path = m[3];
    this.id = 0; this.pending = new Map(); this.buf = Buffer.alloc(0);
  }
  connect() {
    return new Promise((res, rej) => {
      const key = crypto.randomBytes(16).toString('base64');
      this.sock = net.connect(this.port, this.host, () => {
        this.sock.write(
          'GET ' + this.path + ' HTTP/1.1\r\n' +
          'Host: ' + this.host + ':' + this.port + '\r\n' +
          'Upgrade: websocket\r\nConnection: Upgrade\r\n' +
          'Sec-WebSocket-Key: ' + key + '\r\nSec-WebSocket-Version: 13\r\n\r\n');
      });
      let head = true;
      this.sock.on('data', d => {
        if (head) {
          const s = d.indexOf('\r\n\r\n');
          if (s < 0) return;
          head = false;
          this.buf = Buffer.concat([this.buf, d.slice(s + 4)]);
          res();
        } else this.buf = Buffer.concat([this.buf, d]);
        this.drain();
      });
      this.sock.on('error', rej);
    });
  }
  drain() {
    for (;;) {
      if (this.buf.length < 2) return;
      const b1 = this.buf[1] & 0x7f;
      let off = 2, len = b1;
      if (b1 === 126) { if (this.buf.length < 4) return; len = this.buf.readUInt16BE(2); off = 4; }
      else if (b1 === 127) { if (this.buf.length < 10) return; len = Number(this.buf.readBigUInt64BE(2)); off = 10; }
      if (this.buf.length < off + len) return;
      const payload = this.buf.slice(off, off + len).toString('utf8');
      this.buf = this.buf.slice(off + len);
      try {
        const m = JSON.parse(payload);
        if (m.id && this.pending.has(m.id)) { this.pending.get(m.id)(m); this.pending.delete(m.id); }
      } catch (e) {}
    }
  }
  send(method, params) {
    const id = ++this.id;
    const msg = JSON.stringify({ id, method, params: params || {} });
    const p = Buffer.from(msg, 'utf8');
    const mask = crypto.randomBytes(4);
    let head;
    if (p.length < 126) head = Buffer.from([0x81, 0x80 | p.length]);
    else if (p.length < 65536) { head = Buffer.alloc(4); head[0] = 0x81; head[1] = 0xfe; head.writeUInt16BE(p.length, 2); }
    else { head = Buffer.alloc(10); head[0] = 0x81; head[1] = 0xff; head.writeBigUInt64BE(BigInt(p.length), 2); }
    const out = Buffer.concat([head, mask, p]);
    for (let i = 0; i < p.length; i++) out[head.length + 4 + i] = p[i] ^ mask[i % 4];
    return new Promise(res => { this.pending.set(id, res); this.sock.write(out); });
  }
}

(async () => {
  let list;
  for (let t = 0; t < 40; t++) {
    try { list = JSON.parse(await get('/json/list')); if (list.length) break; } catch (e) {}
    await new Promise(r => setTimeout(r, 500));
  }
  if (!list || !list.length) { console.error('no CDP target on ' + PORT); process.exit(1); }
  const page = list.find(t => t.type === 'page') || list[0];
  const ws = new WS(page.webSocketDebuggerUrl);
  await ws.connect();
  await ws.send('Runtime.enable');
  await ws.send('Page.enable');

  const jobs = JSON.parse(fs.readFileSync(JOBS, 'utf8'));
  for (const job of jobs) {
    if (job.wait) { await new Promise(r => setTimeout(r, job.wait)); continue; }
    if (job.eval !== undefined) {
      const r = await ws.send('Runtime.evaluate', { expression: job.eval, returnByValue: true, awaitPromise: true });
      const v = r.result && r.result.result;
      const ex = r.result && r.result.exceptionDetails;
      if (ex) console.log('EXCEPTION: ' + (ex.exception ? ex.exception.description : ex.text));
      else console.log(typeof v.value === 'object' ? JSON.stringify(v.value) : String(v.value));
      continue;
    }
    if (job.shoot) {
      const s = job.shoot;
      let clip = null;
      if (s.sel) {
        const r = await ws.send('Runtime.evaluate', {
          expression: '(()=>{const e=document.querySelector(' + JSON.stringify(s.sel) + ');if(!e)return null;'
            + 'const b=e.getBoundingClientRect();const p=' + (s.pad || 0) + ';'
            + 'return {x:b.left+scrollX-p,y:b.top+scrollY-p,width:b.width+2*p,height:b.height+2*p};})()',
          returnByValue: true });
        clip = r.result.result.value;
        if (!clip) { console.log('no element ' + s.sel); continue; }
        clip.scale = s.scale || 2;
      } else {
        const r = await ws.send('Runtime.evaluate', {
          expression: '({x:0,y:0,width:innerWidth,height:innerHeight})', returnByValue: true });
        clip = r.result.result.value; clip.scale = s.scale || 2;
      }
      const shot = await ws.send('Page.captureScreenshot', { format: 'png', clip, captureBeyondViewport: true });
      if (shot.result && shot.result.data) {
        fs.writeFileSync(s.out, Buffer.from(shot.result.data, 'base64'));
        console.log('wrote ' + s.out);
      } else console.log('shoot failed: ' + JSON.stringify(shot).slice(0, 200));
    }
  }
  process.exit(0);
})();
