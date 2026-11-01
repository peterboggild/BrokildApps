// Local server for the app. Binds to localhost only, serves files from this folder,
// returns 404 for anything missing (no silent fallback to index.html), never caches.
//
// It also runs movie renders: the page renders each frame on the GPU and posts the raw pixels
// here; they are piped straight into ffmpeg (H.264 MP4, the master muxed in), so no frames are kept
// in the browser or on disk. Render endpoints only accept requests from this app's own page.
const http = require('http');
const fs = require('fs');
const vrMeta = require('./vr-meta');
const os = require('os');
const path = require('path');
const { spawn, spawnSync } = require('child_process');

const root = __dirname;
const port = Number(process.env.PORT || 4173);
const host = process.env.HOST || '127.0.0.1';

const mimeTypes = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.wav': 'audio/wav',
  '.mp3': 'audio/mpeg',
  '.mp4': 'video/mp4',
  '.md': 'text/plain; charset=utf-8'
};

function send(res, status, body, type) {
  res.writeHead(status, { 'Content-Type': type || 'text/plain; charset=utf-8', 'Cache-Control': 'no-store' });
  res.end(body);
}
const sendJSON = (res, status, obj) => send(res, status, JSON.stringify(obj), 'application/json; charset=utf-8');

// ---------- ffmpeg ----------
function findFfmpeg() {
  const candidates = [process.env.FFMPEG, 'ffmpeg'].filter(Boolean);
  const local = process.env.LOCALAPPDATA;
  if (local) {
    candidates.push(path.join(local, 'Microsoft', 'WinGet', 'Links', 'ffmpeg.exe'));
    const pk = path.join(local, 'Microsoft', 'WinGet', 'Packages');
    try {
      fs.readdirSync(pk).filter((d) => /ffmpeg/i.test(d)).forEach((d) => {
        fs.readdirSync(path.join(pk, d)).forEach((sub) => candidates.push(path.join(pk, d, sub, 'bin', 'ffmpeg.exe')));
      });
    } catch (err) { /* no winget packages */ }
  }
  for (const c of candidates) {
    try {
      const r = spawnSync(c, ['-version'], { encoding: 'utf8', timeout: 8000, windowsHide: true });
      if (r.status === 0) return { bin: c, version: (r.stdout.split('\n')[0] || '').replace(/^ffmpeg version\s*/, '').split(/[\s-]/)[0] };
    } catch (err) { /* try next */ }
  }
  return null;
}
let ffmpeg;
const ffmpegInfo = () => (ffmpeg === undefined ? (ffmpeg = findFfmpeg()) : ffmpeg);

// ---------- render jobs ----------
const jobs = new Map();
const IDLE_LIMIT_MS = 30 * 60 * 1000; // a job with no frame for this long is abandoned (tab closed)

function stamp() {
  const d = new Date();
  const p = (n) => String(n).padStart(2, '0');
  return `${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`;
}

function resolveOutDir(dir) {
  const d = String(dir || './renders').trim() || './renders';
  return path.isAbsolute(d) ? d : path.join(root, d);
}

function killJob(job, removeFile) {
  if (job.proc && job.proc.exitCode === null) {
    try { job.proc.stdin.destroy(); } catch (err) { /* ignore */ }
    job.proc.kill('SIGKILL');
  }
  clearInterval(job.idleTimer);
  // Windows keeps the files locked until ffmpeg has really gone
  const cleanup = () => {
    if (job.audio) fs.rm(job.audio, { force: true }, () => {});
    if (job.aacFile) fs.rm(job.aacFile, { force: true }, () => {});
    if (removeFile) fs.rm(job.file, { force: true }, () => {});
  };
  if (job.proc && job.code === undefined) job.proc.once('close', cleanup); else cleanup();
  jobs.delete(job.id);
}

// True peak (dBTP) of an audio file, from ffmpeg's EBU R128 meter. null if it cannot be measured.
function measureTruePeak(file) {
  return new Promise((resolve) => {
    const p = spawn(ffmpegInfo().bin, ['-hide_banner', '-nostats', '-i', file, '-map', '0:a:0', '-af', 'ebur128=peak=true', '-f', 'null', '-'], { stdio: ['ignore', 'ignore', 'pipe'], windowsHide: true });
    let err = '';
    p.stderr.on('data', (d) => { err = (err + d).slice(-6000); });
    p.on('close', () => {
      const m = err.split('Summary:').pop().match(/True peak:\s*Peak:\s*(-?[\d.]+|-inf)/);
      resolve(m && m[1] !== '-inf' ? Number(m[1]) : null);
    });
    p.on('error', () => resolve(null));
  });
}

// Lossy AAC overshoots the source peaks (often 0.5–2 dB on loud masters). To keep the movie from
// clipping, only a static gain is applied (no limiting, no compression): the used part of the song is
// encoded, its decoded peak measured, and the gain lowered further until that peak is at most
// AAC_MAX_PEAK. The measured file is then muxed as is (-c:a copy). Lossless PCM copies the soundtrack untouched.
const AAC_CEILING = -1.0;   // first guess: source true peak to this
const AAC_MAX_PEAK = -0.3;  // accepted decoded sample peak, dBFS

function runFfmpeg(args) {
  return new Promise((resolve) => {
    const p = spawn(ffmpegInfo().bin, ['-hide_banner', '-nostats', ...args], { stdio: ['ignore', 'ignore', 'pipe'], windowsHide: true });
    let err = '';
    p.stderr.on('data', (d) => { err = (err + d).slice(-8000); });
    p.on('close', (code) => resolve({ code, err }));
    p.on('error', (e) => resolve({ code: -1, err: e.message }));
  });
}

async function prepareAac(job) {
  const s = job.spec;
  const out = path.join(os.tmpdir(), `fv-aac-${job.id}.m4a`);
  let gain = job.audioPeak !== null ? Math.min(0, AAC_CEILING - job.audioPeak) : 0;
  let peak = null;
  for (let pass = 0; pass < 4; pass += 1) {
    const af = [];
    if (gain < 0) af.push(`volume=${gain.toFixed(2)}dB`);
    // a range that starts mid-song cuts into loud audio; that step makes AAC overshoot (a click over
    // 0 dB in the first ~15 ms). A 10 ms fade-in removes it and is inaudible.
    if (s.startTime > 0) af.push('afade=t=in:d=0.01');
    const enc = await runFfmpeg(['-y', '-ss', String(s.startTime), '-t', (s.frames / s.fps).toFixed(4), '-i', job.audio, '-vn', '-map', '0:a:0',
      ...(af.length ? ['-af', af.join(',')] : []), '-c:a', 'aac', '-b:a', '320k', out]);
    if (enc.code !== 0) throw new Error(`soundtrack encode failed: ${enc.err.trim().split('\n').pop()}`);
    const det = await runFfmpeg(['-i', out, '-af', 'volumedetect', '-f', 'null', '-']);
    const m = det.err.match(/max_volume:\s*(-?[\d.]+) dB/);
    peak = m ? Number(m[1]) : null;
    if (peak === null || peak <= AAC_MAX_PEAK) break;
    gain -= peak - AAC_MAX_PEAK + 0.1;
  }
  job.aacFile = out;
  job.audioGain = gain;
  job.aacPeak = peak;
}

function startFfmpeg(job) {
  const s = job.spec;
  const args = ['-hide_banner', '-loglevel', 'error', '-y',
    '-f', 'rawvideo', '-pix_fmt', 'rgba', '-s', `${s.width}x${s.height}`, '-framerate', String(s.fps), '-i', 'pipe:0'];
  if (job.aacFile) args.push('-i', job.aacFile);
  else if (job.audio) args.push('-ss', String(s.startTime || 0), '-i', job.audio);
  // WebGL rows come bottom-up
  args.push('-vf', 'vflip', '-map', '0:v');
  if (job.aacFile) args.push('-map', '1:a', '-c:a', 'copy');
  else if (job.audio) args.push('-map', '1:a', '-c:a', 'pcm_s24le');
  // VR movies are 4K-8K wide: H.265 (headsets decode it in hardware at these sizes; H.264 tops out near 4096x2304)
  if (s.vr) args.push('-c:v', 'libx265', '-preset', 'medium', '-crf', String(Math.min(51, s.crf + 2)), '-tag:v', 'hvc1', '-x265-params', 'log-level=error', '-pix_fmt', 'yuv420p');
  else args.push('-c:v', 'libx264', '-preset', 'medium', '-crf', String(s.crf), '-pix_fmt', 'yuv420p');
  if (s.audio !== 'pcm') args.push('-movflags', '+faststart');
  args.push(
    '-t', (s.frames / s.fps).toFixed(4), job.file);
  job.proc = spawn(ffmpegInfo().bin, args, { stdio: ['pipe', 'ignore', 'pipe'], windowsHide: true });
  job.stderr = '';
  job.proc.stderr.on('data', (d) => { job.stderr = (job.stderr + d).slice(-4000); });
  job.proc.stdin.on('error', () => { /* reported via exit */ });
  job.exited = new Promise((resolve) => job.proc.on('close', (code) => { job.code = code; resolve(code); }));
}

function readBody(req, limit) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let n = 0;
    req.on('data', (c) => {
      n += c.length;
      if (n > limit) { reject(new Error('request too large')); req.destroy(); return; }
      chunks.push(c);
    });
    req.on('end', () => resolve(Buffer.concat(chunks)));
    req.on('error', reject);
  });
}

// Only the app page itself (same origin, custom header => no cross-site form posts) may render.
function renderAllowed(req) {
  if (req.headers['x-fv-render'] !== '1') return false;
  const origin = req.headers.origin;
  if (!origin) return true;
  try {
    const u = new URL(origin);
    return (u.hostname === 'localhost' || u.hostname === '127.0.0.1') && Number(u.port) === port;
  } catch (err) { return false; }
}

async function handleRender(req, res, pathname, query) {
  if (!renderAllowed(req)) { send(res, 403, 'Forbidden'); return; }
  const op = pathname.slice('/render/'.length);
  const job = jobs.get(query.get('id'));

  if (op === 'check' && req.method === 'GET') {
    const f = ffmpegInfo();
    sendJSON(res, 200, { ffmpeg: f ? f.version : null, renders: resolveOutDir('./renders') });
    return;
  }
  if (req.method !== 'POST') { send(res, 405, 'Method not allowed'); return; }

  if (op === 'start') {
    if (!ffmpegInfo()) { sendJSON(res, 500, { error: 'ffmpeg was not found. Install it (winget install Gyan.FFmpeg) or set FFMPEG to its path, then restart the server.' }); return; }
    let spec;
    try { spec = JSON.parse((await readBody(req, 65536)).toString('utf8')); } catch (err) { sendJSON(res, 400, { error: 'bad request' }); return; }
    const width = Math.round(Number(spec.width));
    const height = Math.round(Number(spec.height));
    const fps = Number(spec.fps);
    const frames = Math.round(Number(spec.frames));
    if (!(width >= 16 && height >= 16 && width % 2 === 0 && height % 2 === 0 && width <= 8192 && height <= 8192 && width * height <= 7680 * 4320)) { sendJSON(res, 400, { error: 'Width and height must be even, 16–8192, at most 7680 × 4320 pixels in all.' }); return; }
    if (!(fps > 0 && fps <= 120 && frames > 0)) { sendJSON(res, 400, { error: 'bad frame rate or frame count' }); return; }
    const dir = resolveOutDir(spec.outDir);
    try { fs.mkdirSync(dir, { recursive: true }); } catch (err) { sendJSON(res, 500, { error: `Cannot create the output folder ${dir}: ${err.message}` }); return; }
    const base = String(spec.name || 'fractal-video').replace(/[^a-z0-9._-]+/gi, '-').replace(/^-+|-+$/g, '').slice(0, 80) || 'fractal-video';
    const id = Math.random().toString(36).slice(2) + Date.now().toString(36);
    const audioMode = spec.audio === 'pcm' ? 'pcm' : 'aac';
    // VR: stereo layout + crop for the spherical metadata, and the file-name tag VR players look for
    let vr = null;
    if (spec.vr && typeof spec.vr === 'object') {
      const st = Math.round(Number(spec.vr.stereo));
      const b = Array.isArray(spec.vr.bounds) ? spec.vr.bounds.slice(0, 4).map((x) => Math.min(0.49, Math.max(0, Number(x) || 0))) : [0, 0, 0, 0];
      const tag = String(spec.vr.tag || '').replace(/[^A-Za-z0-9_]+/g, '').slice(0, 16);
      if (st >= 0 && st <= 2) vr = { stereo: st, bounds: b.length === 4 ? b : [0, 0, 0, 0], tag };
    }
    const j = {
      id, file: path.join(dir, `${base}-${stamp()}${vr && vr.tag ? vr.tag : ''}.${audioMode === 'pcm' ? 'mov' : 'mp4'}`), audio: null, audioPeak: null, audioGain: 0,
      proc: null, next: 0, last: Date.now(),
      spec: { width, height, fps, frames, audio: audioMode, startTime: Math.max(0, Number(spec.startTime) || 0), crf: Math.min(35, Math.max(8, Math.round(Number(spec.crf) || 16))), vr }
    };
    j.idleTimer = setInterval(() => { if (Date.now() - j.last > IDLE_LIMIT_MS) killJob(j, true); }, 60000);
    jobs.set(id, j);
    sendJSON(res, 200, { id, file: j.file });
    return;
  }
  if (!job) { sendJSON(res, 404, { error: 'No such render (the server may have been restarted).' }); return; }
  job.last = Date.now();

  if (op === 'audio') {
    if (job.proc) { sendJSON(res, 409, { error: 'audio must come before the first frame' }); return; }
    const ext = (String(query.get('ext') || 'wav').match(/^[a-z0-9]{1,5}$/i) || ['wav'])[0];
    job.audio = path.join(os.tmpdir(), `fv-audio-${job.id}.${ext}`);
    const out = fs.createWriteStream(job.audio);
    req.pipe(out);
    out.on('finish', async () => {
      try {
        job.audioPeak = await measureTruePeak(job.audio);
        if (job.spec.audio === 'aac') await prepareAac(job);
        job.last = Date.now();
        sendJSON(res, 200, { ok: true, truePeak: job.audioPeak, gain: job.audioGain, aacPeak: job.aacPeak });
      } catch (err) {
        sendJSON(res, 500, { error: err.message });
      }
    });
    out.on('error', (err) => sendJSON(res, 500, { error: err.message }));
    return;
  }

  if (op === 'frame') {
    const n = Number(query.get('n'));
    if (n !== job.next) { sendJSON(res, 409, { error: `expected frame ${job.next}, got ${n}` }); return; }
    if (!job.proc) startFfmpeg(job);
    if (job.code !== undefined) { sendJSON(res, 500, { error: `ffmpeg stopped: ${job.stderr.trim() || `exit ${job.code}`}` }); return; }
    const expect = job.spec.width * job.spec.height * 4;
    let got = 0;
    const stdin = job.proc.stdin;
    req.on('data', (c) => {
      got += c.length;
      if (got > expect) { killJob(job, true); req.destroy(); return; }
      if (!stdin.write(c)) { req.pause(); stdin.once('drain', () => req.resume()); }
    });
    req.on('end', () => {
      if (got !== expect) { killJob(job, true); sendJSON(res, 400, { error: `frame ${n} has ${got} bytes, expected ${expect}` }); return; }
      job.next += 1;
      sendJSON(res, 200, { ok: true });
    });
    return;
  }

  if (op === 'finish') {
    if (!job.proc) { killJob(job, true); sendJSON(res, 400, { error: 'no frames were rendered' }); return; }
    job.proc.stdin.end();
    const code = await job.exited;
    clearInterval(job.idleTimer);
    if (job.audio) fs.rm(job.audio, { force: true }, () => {});
    if (job.aacFile) fs.rm(job.aacFile, { force: true }, () => {});
    jobs.delete(job.id);
    if (code !== 0) { sendJSON(res, 500, { error: `ffmpeg failed: ${job.stderr.trim() || `exit ${code}`}` }); return; }
    let vrInfo = null;
    if (job.spec.vr) {
      try {
        vrMeta.injectSpherical(job.file, job.spec.vr);
        vrInfo = vrMeta.readSpherical(job.file);
      } catch (err) { sendJSON(res, 500, { error: `The movie was written (${job.file}) but the VR metadata could not be added: ${err.message}` }); return; }
    }
    let size = 0;
    try { size = fs.statSync(job.file).size; } catch (err) { /* ignore */ }
    sendJSON(res, 200, { file: job.file, size, frames: job.next, vr: vrInfo, audio: job.audio ? job.spec.audio : null, truePeak: job.audioPeak, gain: job.audioGain, aacPeak: job.aacPeak });
    return;
  }

  if (op === 'cancel') {
    killJob(job, true);
    sendJSON(res, 200, { ok: true });
    return;
  }
  send(res, 404, 'Not found');
}

// ---------- static files ----------
const server = http.createServer((req, res) => {
  let url;
  let pathname;
  try {
    url = new URL(req.url, 'http://localhost');
    pathname = decodeURIComponent(url.pathname);
  } catch (err) {
    send(res, 400, 'Bad request');
    return;
  }
  if (pathname.startsWith('/render/')) {
    handleRender(req, res, pathname, url.searchParams).catch((err) => { console.error(err); if (!res.headersSent) sendJSON(res, 500, { error: err.message }); });
    return;
  }
  if (req.method !== 'GET' && req.method !== 'HEAD') { send(res, 405, 'Method not allowed'); return; }
  if (pathname === '/') pathname = '/index.html';
  const file = path.normalize(path.join(root, pathname));
  if (file !== root && !file.startsWith(root + path.sep)) { send(res, 403, 'Forbidden'); return; }
  if (path.basename(file).startsWith('.')) { send(res, 404, 'Not found'); return; }
  fs.stat(file, (err, stats) => {
    if (err || !stats.isFile()) { send(res, 404, 'Not found'); return; }
    res.writeHead(200, {
      'Content-Type': mimeTypes[path.extname(file).toLowerCase()] || 'application/octet-stream',
      'Content-Length': stats.size,
      'Cache-Control': 'no-store'
    });
    if (req.method === 'HEAD') { res.end(); return; }
    const stream = fs.createReadStream(file);
    stream.on('error', () => res.destroy());
    stream.pipe(res);
  });
});

server.on('error', (err) => {
  if (err.code === 'EADDRINUSE') {
    console.error(`Port ${port} is already in use. Another copy of the server is probably running: open http://localhost:${port}/ or start with PORT=4174.`);
  } else {
    console.error(err);
  }
  process.exit(1);
});

const shutdown = () => { jobs.forEach((j) => killJob(j, true)); process.exit(0); };
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);

server.listen(port, host, () => {
  console.log(`Fractal Video on http://localhost:${port}/  (Ctrl+C to stop)`);
  const f = ffmpegInfo();
  console.log(f ? `Movie render: ffmpeg ${f.version}` : 'Movie render: ffmpeg not found (winget install Gyan.FFmpeg, or set FFMPEG=path)');
});
