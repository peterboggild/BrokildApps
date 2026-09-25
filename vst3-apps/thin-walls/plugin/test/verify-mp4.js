/*  verify-mp4.js - is an exported take a real, playable MP4 with sound?
 *
 *    node test/verify-mp4.js <file.mp4> [sampleSeconds,...]
 *
 *  "The file exists" proves nothing. This loads it in headless Chrome, which
 *  decodes H.264 and AAC itself, and reports: the video's size and duration,
 *  the mean colour at a few instants (a test export encodes its timeline in the
 *  colour), and the decoded audio's channels, rate, length and level.
 */
const fs = require("fs");
const os = require("os");
const path = require("path");
const cp = require("child_process");

const CHROME = "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";
const file = path.resolve(process.argv[2] || "");
if (!fs.existsSync(file)) { console.log("no such file: " + file); process.exit(1); }
const at = (process.argv[3] || "0.2,1.0,2.0,3.0,3.8").split(",").map(Number);

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "twmp4-"));
const mp4 = path.join(tmp, "v.mp4");
fs.copyFileSync(file, mp4);
const html = `<!doctype html><html><head><script>
window.__out = null;
async function go(){
  const res = {};
  try {
    const v = document.createElement('video');
    v.muted = true; v.src = 'v.mp4'; v.preload = 'auto';
    await new Promise((ok, bad) => { v.onloadeddata = ok; v.onerror = () => bad(new Error('video error ' + (v.error && v.error.code))); });
    res.w = v.videoWidth; res.h = v.videoHeight; res.duration = v.duration;
    const c = document.createElement('canvas'); c.width = v.videoWidth; c.height = v.videoHeight;
    const g = c.getContext('2d');
    res.frames = [];
    for (const t of ${JSON.stringify(at)}) {
      v.currentTime = t;
      // 'seeked' can fire before the new frame is on the element: a canvas drawn
      // then shows the PREVIOUS seek's frame (measured). Wait for the frame itself.
      await new Promise(ok => {
        let done = false; const fin = () => { if (!done) { done = true; ok(); } };
        if (v.requestVideoFrameCallback) v.requestVideoFrameCallback(() => fin());
        v.onseeked = () => setTimeout(fin, 400);
      });
      g.drawImage(v, 0, 0);
      const d = g.getImageData(0, c.height - 40, c.width, 30).data;
      let r = 0, gg = 0, b = 0, n = 0;
      for (let i = 0; i < d.length; i += 4) { r += d[i]; gg += d[i+1]; b += d[i+2]; n++; }
      res.frames.push({ t, r: Math.round(r/n), g: Math.round(gg/n), b: Math.round(b/n) });
    }
  } catch (e) { res.videoError = String(e); }
  try {
    const buf = await (await fetch('v.mp4')).arrayBuffer();
    const ac = new OfflineAudioContext(2, 48000, 48000);
    const ab = await ac.decodeAudioData(buf);
    res.audio = { channels: ab.numberOfChannels, rate: ab.sampleRate, seconds: ab.duration };
    let s = 0, pk = 0;
    for (let ch = 0; ch < ab.numberOfChannels; ch++) { const x = ab.getChannelData(ch); for (let i = 0; i < x.length; i++) { s += x[i]*x[i]; pk = Math.max(pk, Math.abs(x[i])); } }
    res.audio.rmsDb = 10 * Math.log10(s / (ab.length * ab.numberOfChannels) + 1e-20);
    res.audio.peak = pk;
  } catch (e) { res.audioError = String(e); }
  document.getElementById('out').textContent = JSON.stringify(res);
}
window.onload = go;
</script></head><body><pre id="out">pending</pre></body></html>`;
fs.writeFileSync(path.join(tmp, "v.html"), html);

// real time, over the debugging port: virtual time stalls the media pipeline
const port = 9300 + Math.floor(Math.random() * 300);
const chrome = cp.spawn(CHROME, [
  "--headless=new", "--allow-file-access-from-files", "--autoplay-policy=no-user-gesture-required",
  "--user-data-dir=" + path.join(tmp, "chrome"), "--remote-debugging-port=" + port, "--remote-allow-origins=*",
  "file:///" + path.join(tmp, "v.html").split(path.sep).join("/")
], { stdio: "ignore" });
const jobs = path.join(tmp, "jobs.json");
fs.writeFileSync(jobs, JSON.stringify([
  { name: "wait", wait: 2500 },
  { name: "result", eval: "(async function(){ for (let i = 0; i < 600; i++) { const t = document.getElementById('out').textContent; if (t !== 'pending') return t; await new Promise(r => setTimeout(r, 100)); } return 'timeout'; })()" }
]));
try {
  const out = cp.execFileSync("node", [path.join(__dirname, "..", "tools", "cdp.js"), String(port), jobs], { encoding: "utf8", timeout: 120000 });
  const NL = String.fromCharCode(10);
  console.log(out.split(NL).filter(l => l.trim().startsWith("{") || l.includes("timeout")).join(NL) || out);
} finally { chrome.kill(); }
