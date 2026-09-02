#!/usr/bin/env node
/*  recover-panel.js — get a Brokild plug-in's panel source back out of the
    binary it was compiled into.
    ==========================================================================

    Every Brokild synth's face is one HTML document — markup, CSS and the whole
    of its JavaScript — embedded in the module as a string literal. That means a
    shipped .vst3 contains a byte-exact copy of the panel source, and this
    recovers it: point the script at a built module and it writes the document
    back out as a file you can open in a browser and edit.

    It exists because of a real gap. B2311.1's source tree lives on one disk,
    at C:\Users\peter\b\ArtefactB2311_1, with no git remote — so unlike its
    nine siblings there is no second copy of it anywhere. Until that is fixed,
    the published archive is the only other place any of that work exists, and
    this pulls back the part of it that survives compilation intact.

    WHAT IT RECOVERS, AND WHAT IT CANNOT

      recovered exactly    the panel: markup, styling, layout, the drawing
                           code, the control behaviour, the message protocol
                           between the page and the processor, the comments
      NOT recovered        the C++ — the counting lattice, the voicing, the
                           parameter declarations, the bench. Machine code
                           does not come back, and nothing here pretends it
                           does.

    So this is a partial restore and should be read as one. It is worth having
    because the panel is a large, hand-written, hard-to-reproduce artefact, and
    because the message protocol it contains documents the processor's entire
    interface — every parameter id, every event the engine sends up — which is
    most of what you need to write the C++ side again.

    USAGE
      node recover-panel.js <file.vst3 | file.exe | archive.zip> [-o outdir]
      node recover-panel.js "Artefact B2311.1.vst3/Contents/x86_64-win/Artefact B2311.1.vst3"
      node recover-panel.js Artefact-B2311-1-win64.zip -o recovered/
*/

'use strict';
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const argv = process.argv.slice(2);
if (argv.length === 0 || argv.includes('-h') || argv.includes('--help')) {
  console.log('usage: node recover-panel.js <file.vst3|file.exe|archive.zip> [-o outdir]');
  process.exit(argv.length === 0 ? 1 : 0);
}
const oi = argv.indexOf('-o');
const outDir = oi >= 0 ? argv[oi + 1] : '.';
const target = argv.filter((a, i) => a !== '-o' && i !== oi + 1)[0];

/*  A document runs from its doctype to its closing tag. Two things make that
    less obvious than it sounds.

    "Printable" has to include everything above 0x7f, because the panels are
    UTF-8 and full of box-drawing rules and typographic quotes. And the run
    cannot simply be cut at the first padding: the compiler lays the resource
    NAME TABLE down a few NULs after the document, so a short terminator swallows
    "ui_html txplanet_png bwfxrack_js ..." onto the end of the file — which is
    what a first version of this did, silently, on two of the eleven panels.

    So: read generously, stop only at real padding, then trim back to the last
    closing tag. B2311.1's panel has no <html> element at all — it opens with a
    doctype and a <meta> and ends at its final </script> — so both endings have
    to be accepted. */
const PAD_RUN = 16;

function extractFrom (buf, start, nextDocAt) {
  const limit = nextDocAt > start ? nextDocAt : buf.length;
  let lastGood = start - 1, bad = 0;
  for (let i = start; i < limit; i++) {
    const c = buf[i];
    const printable = (c >= 32 && c < 127) || c >= 0x80 || c === 9 || c === 10 || c === 13;
    if (printable) { lastGood = i; bad = 0; }
    else if (++bad > PAD_RUN) break;
  }
  const raw = buf.slice(start, lastGood + 1);
  const text = raw.toString('latin1');

  for (const tag of ['</html>', '</script>']) {
    const at = text.lastIndexOf(tag);
    if (at >= 0) return { bytes: raw.slice(0, at + tag.length), closed: true };
  }
  return { bytes: raw, closed: false };
}

function findDocuments (buf) {
  const out = [];
  for (const needle of ['<!doctype html', '<!DOCTYPE html']) {
    let from = 0;
    for (;;) {
      const at = buf.indexOf(needle, from, 'latin1');
      if (at < 0) break;
      out.push(at);
      from = at + 1;
    }
  }
  return out.sort((a, b) => a - b);
}

function titleOf (text, fallback) {
  const m = /<title>([^<]{1,120})<\/title>/i.exec(text);
  const raw = m ? m[1] : fallback;
  return raw.trim().replace(/[^A-Za-z0-9.\- ]+/g, '').replace(/\s+/g, '-') || fallback;
}

/*  A shipped archive holds the module inside a bundle inside a zip. Unpacking
    it here saves the caller a step, and the caller is usually someone who has
    just downloaded the archive to find out what is in it.

    The zip is read directly rather than shelled out to. Windows — where this
    script is most likely to be run — has no `unzip`, and these archives are
    built with backslash path separators, which several unpackers report as a
    warning and a non-zero exit even when they succeed. Parsing the central
    directory ourselves sidesteps both. */
function unzipEntries (file) {
  const buf = fs.readFileSync(file);
  //  End of central directory: scan back from the end for its signature. The
  //  trailing comment is at most 64 KB, so that is as far as we ever look.
  let eocd = -1;
  for (let i = buf.length - 22; i >= Math.max(0, buf.length - 22 - 65535); i--) {
    if (buf.readUInt32LE(i) === 0x06054b50) { eocd = i; break; }
  }
  if (eocd < 0) throw new Error('not a zip file (no end-of-central-directory record)');
  const count = buf.readUInt16LE(eocd + 10);
  let p = buf.readUInt32LE(eocd + 16);

  const out = [];
  for (let n = 0; n < count; n++) {
    if (buf.readUInt32LE(p) !== 0x02014b50) throw new Error('corrupt central directory at entry ' + n);
    const method  = buf.readUInt16LE(p + 10);
    const compSz  = buf.readUInt32LE(p + 20);
    const nameLen = buf.readUInt16LE(p + 28);
    const extraLen= buf.readUInt16LE(p + 30);
    const cmtLen  = buf.readUInt16LE(p + 32);
    const local   = buf.readUInt32LE(p + 42);
    const name    = buf.toString('utf8', p + 46, p + 46 + nameLen).replace(/\\/g, '/');
    p += 46 + nameLen + extraLen + cmtLen;

    if (name.endsWith('/')) continue;
    if (buf.readUInt32LE(local) !== 0x04034b50) throw new Error('corrupt local header for ' + name);
    const lNameLen  = buf.readUInt16LE(local + 26);
    const lExtraLen = buf.readUInt16LE(local + 28);
    const from = local + 30 + lNameLen + lExtraLen;
    const raw  = buf.slice(from, from + compSz);
    let data;
    if (method === 0) data = raw;
    else if (method === 8) data = zlib.inflateRawSync(raw);
    else continue;                       //  nothing here is ever stored otherwise
    out.push({ name, data });
  }
  return out;
}

/*  Yield {label, buffer} for every module worth searching. */
function modulesIn (file) {
  if (!/\.zip$/i.test(file)) return [{ label: file, data: fs.readFileSync(file) }];
  let entries;
  try {
    entries = unzipEntries(file);
  } catch (e) {
    console.error('could not read ' + file + ': ' + e.message);
    console.error('Unpack it by hand and point this script at the module inside');
    console.error('Contents/x86_64-win/.');
    process.exit(1);
  }
  //  the .vst3 module and the standalone carry the same page; one is enough
  const mod = entries.find(e => /\.vst3$/i.test(e.name) && !e.name.endsWith('/'))
           || entries.find(e => /\.(exe|dll)$/i.test(e.name));
  if (!mod) { console.error('no module found inside ' + file); process.exit(1); }
  return [{ label: mod.name, data: mod.data }];
}

fs.mkdirSync(outDir, { recursive: true });
let written = 0;

for (const mod of modulesIn(target)) {
  const buf = mod.data;
  const label = path.basename(mod.label);
  const starts = findDocuments(buf);
  if (starts.length === 0) {
    console.log(label + ': no embedded HTML document found');
    continue;
  }
  starts.forEach((at, n) => {
    const { bytes, closed } = extractFrom(buf, at, starts[n + 1]);
    const text = bytes.toString('utf8');
    //  A real panel is thousands of bytes and closes its script. A short run is
    //  a fragment of some other string that happens to start the same way, and
    //  writing it out as if it were source would be a lie.
    if (bytes.length < 1024) {
      console.log(label + ': skipped a ' + bytes.length + '-byte fragment at ' + at);
      return;
    }
    const name = titleOf(text, path.basename(mod.label, path.extname(mod.label))) + (starts.length > 1 ? '-' + n : '') + '.html';
    const dst = path.join(outDir, name);
    fs.writeFileSync(dst, bytes);
    written++;
    console.log(
      dst + '  ' + bytes.length + ' bytes  (offset ' + at + ')' +
      (closed ? '' : '  — WARNING: does not end cleanly, may be truncated')
    );
  });
}

if (written === 0) process.exit(2);
console.log('');
console.log('Recovered ' + written + ' document(s). This is the panel only — the C++ engine');
console.log('is not in here and cannot be. Treat it as a partial restore.');
