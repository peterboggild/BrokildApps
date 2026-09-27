// Handbook 260925.4, second pass: pages 21 and 22 ran into the folio (the gate said so)
const fs = require('fs');
const f = 'C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/thin-walls/plugin/docs/manual/manual.html';
let s = fs.readFileSync(f, 'utf8');
const NL = (s.split('\r\n').length - 1) > (s.split('\n').length - 1) / 2 ? '\r\n' : '\n';
s = s.split('\r\n').join('\n');
const miss = [];
function rep(a, b) { const n = s.split(a).length - 1; if (n !== 1) { miss.push(n + 'x: ' + a.slice(0, 90)); return; } s = s.replace(a, () => b); }

// page 21
rep(`<tr><td>PICTURE</td><td>AS SHOWN, CINEMATIC, PHOTO 16 or PHOTO 64 &mdash; the PHOTO settings trace that many samples into every frame, at seconds a frame</td></tr>`,
    `<tr><td>PICTURE</td><td>AS SHOWN, CINEMATIC, PHOTO 16 or 64 &mdash; PHOTO traces that many samples into every frame</td></tr>`);
rep(`<tr><td>RENDER MP4</td><td>renders the last take; CANCEL stops it and deletes the partial file</td></tr>`,
    `<tr><td>RENDER MP4</td><td>renders the last take; CANCEL deletes the partial file</td></tr>`);
rep(`          <p>The sound is re-rendered offline, at the SOUND chosen, through a fresh
            engine. Every frame is then redrawn from the same recording &mdash;
            the parameters at that frame's time, not the live controls &mdash;
            so what you see and what you hear come from one timeline. The video
            is H.264 with AAC sound, and it lands in
            <span class="mono">DOCUMENTS\\THIN WALLS VIDEOS</span>.</p>`,
    `          <p>The sound is re-rendered offline through a fresh engine, and
            every frame is redrawn from the same recording, so picture and
            sound share one timeline. H.264 with AAC sound, in
            <span class="mono">DOCUMENTS\\THIN WALLS VIDEOS</span>.</p>`);

// page 22
rep(`          <p>Only time, and only when you render. On a many-core desktop HIGH
            takes about one and a half times as long as the take, ULTRA a
            little more, and ULTRA + BASS about two and a half times &mdash; a
            four-second take in about ten seconds. A take that moves is traced again every 25 cm of
            walking.</p>`,
    `          <p>Only time, and only when you render: on a many-core desktop a
            four-second take takes about 6 s at HIGH or ULTRA and 10 s at ULTRA
            + BASS. A take that moves is traced again every 25 cm.</p>`);
rep(`          <p>Rays know nothing of room modes, which is what ULTRA + BASS is
            for; the wave has no walls to pass through. DIRECT, EARLY and REVERB do not reach
            below the crossover in ULTRA + BASS &mdash; the wave is all of them
            at once.</p>`,
    `          <p>Rays know nothing of room modes &mdash; that is what ULTRA + BASS
            is for. Below its crossover the wave is DIRECT, EARLY and REVERB at
            once, so those three do not reach there.</p>`);
rep(`        <div class="box cool" style="margin-top:4mm">
          <h4>What it costs</h4>`, `        <div class="box cool" style="margin-top:3mm">
          <h4>What it costs</h4>`);
rep(`        <div class="box" style="margin-top:4mm">
          <h4>Honestly</h4>`, `        <div class="box" style="margin-top:3mm">
          <h4>Honestly</h4>`);

if (miss.length) { console.log('MISSES:\n' + miss.join('\n')); process.exit(1); }
if (NL === '\r\n') s = s.split('\n').join('\r\n');
fs.writeFileSync(f, s);
console.log('ok');
