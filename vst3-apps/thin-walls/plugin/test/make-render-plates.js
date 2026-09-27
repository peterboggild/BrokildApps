/*  Build 260925.4: re-shoot only the two handbook plates the render qualities
 *  changed - the export panel (now with SOUND and BOUNCE WAV) and the Levels
 *  and head group (now with HEAD). Same pipeline as make-manual-plates.js:
 *
 *    node test/make-render-plates.js <rawDir> [jobsOut]
 *    powershell -File tools/live.ps1 -Jobs <jobsOut> -Exe <standalone>
 *    powershell -File tools/crop-plates.ps1 -Raw <rawDir>
 */
const fs = require('fs');
const path = require('path');
const RAW = (process.argv[2] || path.join(process.env.TEMP, 'tw-plates-render')).replace(/\\/g, '/');
const OUT = process.argv[3] || path.join(process.env.TEMP, 'tw-render-plates.json');
fs.mkdirSync(RAW, { recursive: true });
const SOFA = 'C:/Users/peter/b/_build/ThinWalls/test/_deps/tw_libmysofa-src/share/MIT_KEMAR_normal_pinna.sofa';
const WAV = 'C:/Users/peter/b/ThinWalls/test/testsignal.wav';

const J = [];
const wait = (ms, name) => J.push({ name: name || 'wait', wait: ms });
const ev = (name, fn) => J.push({ name, eval: fn });
const shot = (name, sel, pad) => J.push({ name, shoot: RAW + '/' + name + '.png', sel, pad: pad || 0 });

wait(4500, 'settle');
J.push({ name: 'hi-dpi', cmd: 'Emulation.setDeviceMetricsOverride', params: { width: 0, height: 0, deviceScaleFactor: 2, mobile: false } });
wait(1200);
ev('name the groups', `(function(){ var n = 0; document.querySelectorAll('.grp').forEach(function(g){ var h = g.querySelector('h2'); if (!h) return; if (!g.id) g.id = 'g_' + h.textContent.trim().toLowerCase().replace(/[^a-z]+/g, '_'); n++; }); return n + ' groups'; })()`);
ev('defaults, a head from a file, the test signal', `(function(){ var B = window.__JUCE__.backend; B.emitEvent('tw', { k:'presetDefault' }); B.emitEvent('tw', { k:'hrtfPath', path:'${SOFA}' }); B.emitEvent('tw', { k:'wavPath', path:'${WAV}' }); return 'sent'; })()`);
wait(1800);
ev('head row reads', `document.getElementById('headName').textContent`);
shot('ctrl-levels', '#g_levels_and_head', 3);
ev('record', `(function(){ window.__JUCE__.backend.emitEvent('tw', { k:'recStart' }); return 'rec'; })()`);
wait(3200);
ev('stop', `(function(){ window.__JUCE__.backend.emitEvent('tw', { k:'recStop' }); return 'stop'; })()`);
wait(1200);
ev('open the export panel, ULTRA + BASS', `(function(){ document.getElementById('bExpOpen').click(); var s = document.getElementById('expSnd'); s.value = '3'; s.dispatchEvent(new Event('change')); return document.getElementById('expPop').hidden ? 'STILL HIDDEN' : 'open, ' + s.options[s.selectedIndex].textContent; })()`);
wait(700);
shot('exp-pop', '#expPop', 3);
ev('put it back: built-in head, SOUND LIVE, panel shut', `(function(){ var B = window.__JUCE__.backend; B.emitEvent('tw', { k:'hrtfDefault' }); var s = document.getElementById('expSnd'); s.value = '0'; s.dispatchEvent(new Event('change')); document.getElementById('bExpOpen').click(); return 'reset'; })()`);
wait(800);
fs.writeFileSync(OUT, JSON.stringify(J, null, 1));
console.log('wrote ' + OUT + ' (' + J.length + ' jobs) -> ' + RAW);
