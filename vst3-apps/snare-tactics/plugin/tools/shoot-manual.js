// Snare Tactics manual gate: render manual.html in headless Chrome, measure every
// page, and shoot each page to a PNG so it can be LOOKED at.
//
// A .page is a fixed sheet with overflow:hidden. Anything too tall is not
// pushed onto a new page - it is silently cut off, and the DOM still looks
// right. So every element that holds a text node or an image is measured
// against its OWN page's box, and against that page's folio strip.
// The page count is read from the document, never typed.
//
// usage: node shoot-manual.js [manual.html] [outDir]
// Needs Node 22+ (global WebSocket). Exit code 1 if anything overflows.
'use strict';
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const os = require('os');

const html = path.resolve(process.argv[2] || path.join(__dirname, '..', 'docs', 'manual', 'manual.html'));
const out = path.resolve(process.argv[3] || path.join(__dirname, '..', 'docs', 'manual', 'pages'));
const port = 9311;
const chromes = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'];
const chrome = chromes.find(p => fs.existsSync(p));
if (!chrome) { console.error('no Chrome/Edge'); process.exit(2); }

const PW = 1123, PH = 794;   // 297 x 210 mm at 96 dpi
const udd = fs.mkdtempSync(path.join(os.tmpdir(), 'st-manual-'));
const proc = spawn(chrome, ['--headless=new', '--disable-gpu', '--no-first-run', '--hide-scrollbars',
  '--remote-debugging-port=' + port, '--remote-allow-origins=*', '--user-data-dir=' + udd,
  '--window-size=' + PW + ',' + PH, 'about:blank'], { stdio: 'ignore' });

const sleep = ms => new Promise(r => setTimeout(r, ms));

async function connect() {
  for (let i = 0; i < 60; i++) {
    try {
      const list = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
      const pg = list.find(t => t.type === 'page');
      if (pg) return pg.webSocketDebuggerUrl;
    } catch (e) {}
    await sleep(250);
  }
  throw new Error('no CDP');
}

(async () => {
  let ws, id = 0;
  const pending = new Map();
  try {
    ws = new WebSocket(await connect());
    await new Promise(r => ws.onopen = r);
    ws.onmessage = m => { const d = JSON.parse(m.data); if (d.id && pending.has(d.id)) { pending.get(d.id)(d); pending.delete(d.id); } };
    const send = (method, params = {}) => new Promise(r => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
    const ev = async expr => { const r = await send('Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise: true }); return r.result && r.result.result ? r.result.result.value : r; };

    await send('Emulation.setDeviceMetricsOverride', { width: PW, height: PH, deviceScaleFactor: 1, mobile: false });
    await send('Page.enable');
    await send('Page.navigate', { url: 'file:///' + html.replace(/\\/g, '/') });
    await sleep(2500);
    await ev('document.fonts.ready.then(()=>1)');

    const report = await ev(`(function(){
      var pages=[...document.querySelectorAll('.page')], out={pages:pages.length, imgs:0, broken:[], over:[], folio:[]};
      document.querySelectorAll('img').forEach(function(i){ out.imgs++; if(!(i.complete&&i.naturalWidth>0)) out.broken.push(i.getAttribute('src')); });
      pages.forEach(function(pg,pi){
        var pr=pg.getBoundingClientRect(), f=pg.querySelector('.folio'), fr=f?f.getBoundingClientRect():null;
        pg.querySelectorAll('*').forEach(function(el){
          var own=el.tagName==='IMG'||[...el.childNodes].some(function(n){return n.nodeType===3&&n.textContent.trim().length;});
          if(!own) return;
          if(f && (el===f || f.contains(el))) return;
          var r=el.getBoundingClientRect(); if(r.width===0||r.height===0) return;
          // a banded cover image is clipped by its own box on purpose; measure the box it sits in
          if(el.tagName==='IMG' && el.parentElement.classList.contains('cover-art')) r=el.parentElement.getBoundingClientRect();
          var ob=r.bottom-pr.bottom, orr=r.right-pr.right, ol=pr.left-r.left, ot=pr.top-r.top;
          var what=(el.tagName.toLowerCase()+' ['+(el.tagName==='IMG'?el.getAttribute('src'):el.textContent.trim().slice(0,30))+']');
          if(ob>1||orr>1||ol>1||ot>1) out.over.push('page '+(pi+1)+' '+what+' b'+ob.toFixed(1)+' r'+orr.toFixed(1));
          if(fr && r.bottom>fr.top-1 && r.top<fr.bottom) out.folio.push('page '+(pi+1)+' '+what+' bottom '+r.bottom.toFixed(1)+' folio top '+fr.top.toFixed(1));
        });
      });
      return out;})()`);

    console.log('pages in document: ' + report.pages);
    console.log('images: ' + (report.imgs - report.broken.length) + ' of ' + report.imgs + ' loaded' + (report.broken.length ? '  BROKEN: ' + report.broken.join(', ') : ''));
    console.log('OVERFLOW: ' + (report.over.length ? report.over.join('\n  ') : 'clean - every text/image element fits its page'));
    console.log('FOLIO:    ' + (report.folio.length ? report.folio.join('\n  ') : 'clean - nothing touches a page-number strip'));

    fs.mkdirSync(out, { recursive: true });
    for (const f of fs.readdirSync(out)) if (f.endsWith('.png')) fs.unlinkSync(path.join(out, f));
    const tops = await ev(`[...document.querySelectorAll('.page')].map(p=>p.getBoundingClientRect().top+scrollY)`);
    for (let i = 0; i < tops.length; i++) {
      const r = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true,
        clip: { x: 0, y: tops[i], width: PW, height: PH, scale: 1 } });
      fs.writeFileSync(path.join(out, 'p' + String(i + 1).padStart(2, '0') + '.png'), Buffer.from(r.result.data, 'base64'));
    }
    console.log('shots: ' + tops.length + ' -> ' + out);
    process.exitCode = (report.over.length || report.folio.length || report.broken.length) ? 1 : 0;
  } catch (e) { console.error(e); process.exitCode = 2; }
  finally { try { ws && ws.close(); } catch (e) {} proc.kill(); }
})();
