/*  A static server for checking the published pages.
 *
 *  The front page fetches manifest.json and every app.json at RUNTIME, so a
 *  file:// load builds no cards at all and proves nothing. Bound to 127.0.0.1
 *  rather than 0.0.0.0 on purpose: the default pops a Windows Firewall prompt.
 *
 *    node tools/serve.js <root> [port]
 */
const http = require("http");
const fs = require("fs");
const path = require("path");

const ROOT = process.argv[2];
const PORT = parseInt(process.argv[3] || "8777", 10);

const TYPES = {
  ".html": "text/html; charset=utf-8", ".js": "text/javascript",
  ".json": "application/json", ".css": "text/css",
  ".jpg": "image/jpeg", ".jpeg": "image/jpeg", ".png": "image/png",
  ".svg": "image/svg+xml", ".pdf": "application/pdf", ".zip": "application/zip"
};

http.createServer((req, res) => {
  let p = decodeURIComponent(req.url.split("?")[0]);
  if (p.endsWith("/")) p += "index.html";
  const file = path.join(ROOT, p);
  // never serve outside the root
  if (!path.resolve(file).startsWith(path.resolve(ROOT))) { res.writeHead(403); return res.end(); }
  fs.readFile(file, (err, buf) => {
    if (err) { res.writeHead(404, { "content-type": "text/plain" }); return res.end("404 " + p); }
    res.writeHead(200, { "content-type": TYPES[path.extname(file).toLowerCase()] || "application/octet-stream" });
    res.end(buf);
  });
}).listen(PORT, "127.0.0.1", () => console.log("serving " + ROOT + " on http://127.0.0.1:" + PORT));
