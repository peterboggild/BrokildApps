'use strict';
// Spherical Video V2 metadata (st3d + sv3d/equi) for MP4/MOV files, the form YouTube, the Meta Quest
// player and most VR players read. ffmpeg cannot write it from the command line, so it is injected
// after encoding: the two boxes go at the end of the video sample entry (avc1/hvc1), every parent box
// grows by their size, and if moov sits before mdat (faststart) all chunk offsets move by the same amount.
// Spec: https://github.com/google/spatial-media/blob/master/docs/spherical-video-v2-rfc.md

const fs = require('fs');

const CONTAINERS = new Set(['moov', 'trak', 'mdia', 'minf', 'stbl']);
const SAMPLE_ENTRIES = new Set(['avc1', 'avc3', 'hvc1', 'hev1']);

function box(type, payload) {
  const b = Buffer.alloc(8 + payload.length);
  b.writeUInt32BE(b.length, 0);
  b.write(type, 4, 'latin1');
  payload.copy(b, 8);
  return b;
}
const full = (type, payload) => box(type, Buffer.concat([Buffer.alloc(4), payload]));

// opts: { stereo: 0 mono | 1 top/bottom | 2 left/right, bounds: [top, bottom, left, right] fractions cropped }
function sphericalBoxes(opts) {
  const st3d = full('st3d', Buffer.from([opts.stereo | 0]));
  const svhd = full('svhd', Buffer.from('Fractal Video\0', 'latin1'));
  const prhd = full('prhd', Buffer.alloc(12)); // yaw, pitch, roll = 0
  const eq = Buffer.alloc(16);
  (opts.bounds || [0, 0, 0, 0]).forEach((f, i) => eq.writeUInt32BE(Math.min(0xffffffff, Math.round(f * 4294967296)), i * 4));
  const proj = box('proj', Buffer.concat([prhd, full('equi', eq)]));
  return Buffer.concat([st3d, box('sv3d', Buffer.concat([svhd, proj]))]);
}

function readBoxes(buf, start, end) {
  const out = [];
  let p = start;
  while (p + 8 <= end) {
    let size = buf.readUInt32BE(p);
    const type = buf.toString('latin1', p + 4, p + 8);
    let head = 8;
    if (size === 1) { size = Number(buf.readBigUInt64BE(p + 8)); head = 16; }
    else if (size === 0) size = end - p;
    if (size < head || p + size > end) throw new Error(`bad box ${type} at ${p}`);
    out.push({ type, start: p, size, head });
    p += size;
  }
  return out;
}

// Rebuild a box with `extra` appended to every video sample entry inside it. Returns [buffer, added, found].
function rebuild(buf, b, extra) {
  const body = buf.subarray(b.start + b.head, b.start + b.size);
  if (CONTAINERS.has(b.type)) {
    let added = 0;
    let found = 0;
    const parts = readBoxes(buf, b.start + b.head, b.start + b.size).map((c) => {
      const r = rebuild(buf, c, extra);
      added += r[1];
      found += r[2];
      return r[0];
    });
    return [box(b.type, Buffer.concat(parts)), added, found];
  }
  if (b.type === 'stsd') {
    // FullBox (4) + entry count (4), then sample entries
    const s0 = b.start + b.head + 8;
    let added = 0;
    let found = 0;
    const parts = readBoxes(buf, s0, b.start + b.size).map((c) => {
      const raw = buf.subarray(c.start + c.head, c.start + c.size);
      if (!SAMPLE_ENTRIES.has(c.type)) return box(c.type, raw);
      // drop any older st3d/sv3d so a second injection replaces rather than stacks
      const kids = readBoxes(buf, c.start + c.head + 78, c.start + c.size).filter((k) => k.type !== 'st3d' && k.type !== 'sv3d');
      const keep = Buffer.concat([raw.subarray(0, 78), ...kids.map((k) => buf.subarray(k.start, k.start + k.size))]);
      const nb = box(c.type, Buffer.concat([keep, extra]));
      added += nb.length - c.size;
      found += 1;
      return nb;
    });
    return [box('stsd', Buffer.concat([buf.subarray(b.start + b.head, s0), ...parts])), added, found];
  }
  return [box(b.type, body), 0, 0];
}

function shiftOffsets(moov, delta) {
  const walk = (start, end) => {
    readBoxes(moov, start, end).forEach((c) => {
      if (CONTAINERS.has(c.type)) walk(c.start + c.head, c.start + c.size);
      else if (c.type === 'stco' || c.type === 'co64') {
        const n = moov.readUInt32BE(c.start + c.head + 4);
        let p = c.start + c.head + 8;
        for (let i = 0; i < n; i += 1) {
          if (c.type === 'stco') { moov.writeUInt32BE(moov.readUInt32BE(p) + delta, p); p += 4; }
          else { moov.writeBigUInt64BE(moov.readBigUInt64BE(p) + BigInt(delta), p); p += 8; }
        }
      }
    });
  };
  walk(8, moov.length);
}

function injectSpherical(file, opts) {
  const buf = fs.readFileSync(file);
  const top = readBoxes(buf, 0, buf.length);
  const mi = top.findIndex((b) => b.type === 'moov');
  if (mi < 0) throw new Error('no moov box');
  const moovBox = top[mi];
  const [moov, added, found] = rebuild(buf, moovBox, sphericalBoxes(opts));
  if (!found) throw new Error('no H.264/H.265 video track found');
  const delta = moov.length - moovBox.size;
  const mdatAfter = top.some((b, i) => i > mi && b.type === 'mdat');
  if (mdatAfter && delta) shiftOffsets(moov, delta);
  const parts = top.map((b) => (b === moovBox ? moov : buf.subarray(b.start, b.start + b.size)));
  const tmp = `${file}.vrtmp`;
  fs.writeFileSync(tmp, Buffer.concat(parts));
  fs.renameSync(tmp, file);
  return { added, tracks: found };
}

// Read back what a file says about itself (for tests and the server's report).
function readSpherical(file) {
  const buf = fs.readFileSync(file);
  const res = { stereo: null, bounds: null };
  const walk = (start, end) => {
    readBoxes(buf, start, end).forEach((c) => {
      if (CONTAINERS.has(c.type) || c.type === 'sv3d' || c.type === 'proj') walk(c.start + c.head, c.start + c.size);
      else if (c.type === 'stsd') walk(c.start + c.head + 8, c.start + c.size);
      else if (SAMPLE_ENTRIES.has(c.type)) walk(c.start + c.head + 78, c.start + c.size);
      else if (c.type === 'st3d') res.stereo = buf.readUInt8(c.start + c.head + 4);
      else if (c.type === 'equi') res.bounds = [0, 1, 2, 3].map((i) => buf.readUInt32BE(c.start + c.head + 4 + i * 4) / 4294967296);
    });
  };
  const top = readBoxes(buf, 0, buf.length);
  const m = top.find((b) => b.type === 'moov');
  if (m) walk(m.start + m.head, m.start + m.size);
  return res;
}

module.exports = { injectSpherical, readSpherical, sphericalBoxes };
