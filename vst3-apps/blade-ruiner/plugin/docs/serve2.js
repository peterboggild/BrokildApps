const http=require('http'),fs=require('fs'),path=require('path');
const ROOT=process.argv[2], PORT=+process.argv[3];
const TYPES={'.html':'text/html','.js':'text/javascript','.json':'application/json',
             '.svg':'image/svg+xml','.webmanifest':'application/manifest+json',
             '.jpg':'image/jpeg','.png':'image/png','.css':'text/css'};
http.createServer((q,r)=>{
  const f=path.join(ROOT, decodeURIComponent(q.url.split('?')[0]));
  fs.readFile(f,(e,d)=>{
    if(e){ r.writeHead(404); r.end('no'); return; }
    r.writeHead(200,{'Content-Type':TYPES[path.extname(f).toLowerCase()]||'application/octet-stream',
                     'Cache-Control':'no-store'});
    r.end(d);
  });
}).listen(PORT,'127.0.0.1',()=>console.log('serving '+ROOT+' on '+PORT));
