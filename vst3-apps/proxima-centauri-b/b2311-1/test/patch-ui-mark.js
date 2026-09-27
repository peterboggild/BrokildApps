/*  260902.3 — the panel shows the mark.

    Same discipline as patch-mark.js: exact-count anchors, collect every miss,
    abort before writing.
*/
const fs = require('fs');
const P = 'C:/Users/peter/b/ArtefactB2311_1/Source/ui/ui.html';

let s = fs.readFileSync(P, 'utf8');
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const miss = [];
function rep (find, into) {
  const n = s.split(find).length - 1;
  if (n !== 1) { miss.push('[' + n + 'x] ' + find.split(NL)[0].slice(0, 70)); return; }
  s = s.replace(find, into);
}
const J = a => a.join(NL);

//  1. the buffer
rep('let PH=null, HT=null, RT=null;',
    'let PH=null, HT=null, RT=null, MK=null;');

//  2. decode it
rep(J(['  if (p.rt){ const b = atob(p.rt);']),
    J(['  if (p.mk){ const b = atob(p.mk);',
       '             if(!MK || MK.length!==b.length) MK = new Uint8Array(b.length);',
       '             for(let i=0;i<b.length;i++) MK[i]=b.charCodeAt(i); }',
       '  if (p.rt){ const b = atob(p.rt);']));

//  3. draw it. A marked unit is drawn in the mark's own colour, brightening
//     with how deep the mark is, so a stroke stays visible and fades.
rep(J(['        const rq = RT ? RT[i]/255 : 0.5;',
       '        const rr = Math.round(120 + 135*rq), gg = Math.round(96 + 118*rq), bb = Math.round(190 - 150*rq);',
       '        cx.strokeStyle = warm > 0.015']),
    J(['        const rq = RT ? RT[i]/255 : 0.5;',
       '        let rr = Math.round(120 + 135*rq), gg = Math.round(96 + 118*rq), bb = Math.round(190 - 150*rq);',
       '        /*  THE MARK. A touched unit counts at a different rate, which is to say',
       '            it has been moved into another layer — so it is drawn as one: pulled',
       '            towards green where it was sped up and towards violet where it was',
       '            slowed, by how deep the mark still is. It fades as the mark fades,',
       '            which is what makes MEMORY legible without a readout. */',
       '        const mq = MK ? (MK[i]-128)/127 : 0;',
       '        const ma = Math.min(1, Math.abs(mq));',
       '        if (ma > 0.004){',
       '          const tr = mq > 0 ?  90 : 214, tg = mq > 0 ? 255 : 120, tb = mq > 0 ? 168 : 255;',
       '          rr = Math.round(rr + (tr-rr)*ma); gg = Math.round(gg + (tg-gg)*ma); bb = Math.round(bb + (tb-bb)*ma);',
       '          cx.beginPath(); cx.arc(px,py,r*(0.55+0.75*ma),0,TAU);',
       '          cx.fillStyle = "rgba("+tr+","+tg+","+tb+","+(0.10*ma).toFixed(3)+")"; cx.fill();',
       '        }',
       '        cx.strokeStyle = warm > 0.015']));

//  4. the mark's own opacity floor, so a faint trace on a dim unit still reads
rep('          : "rgba("+rr+","+gg+","+bb+","+(0.26 + 0.60*p).toFixed(3)+")";',
    '          : "rgba("+rr+","+gg+","+bb+","+(0.26 + 0.60*p + 0.40*ma).toFixed(3)+")";');

//  5. MEMORY belongs with the counting, since that is what it changes
rep('  ["THE COUNTING", ["ratelo","ratehi","couple","dead","leak"]],',
    '  ["THE COUNTING", ["ratelo","ratehi","couple","dead","leak","memory"]],');

if (miss.length) {
  console.error('ABORTED, nothing written. Missed anchors:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log('ui.html patched: 5 edits');
