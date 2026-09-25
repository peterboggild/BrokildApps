/*  Builds the CDP jobs that shoot every handbook plate from the LIVE standalone
 *  (build 260925.3 onward).
 *
 *    node test/make-manual-plates.js <rawDir> [jobsOut]
 *    powershell -File tools/live.ps1 -Jobs <jobsOut> -Exe <standalone>
 *    powershell -File tools/crop-plates.ps1 -Raw <rawDir>
 *
 *  Every shot names an ELEMENT; tools/cdp.js measures it at the instant of the
 *  shot, at deviceScaleFactor 2, so no crop rectangle can outlive the layout
 *  it was measured in. The jobs file embeds three of Peter's pictures (approved
 *  for the handbook) as base64, so it is written to %TEMP%, never to the repo.
 *
 *  Side effects on the standalone's own profile are undone at the end:
 *  CINEMATIC goes back off, the arrangement goes back to its defaults.
 */
const fs = require('fs');
const path = require('path');

const RAW = (process.argv[2] || path.join(process.env.TEMP, 'tw-plates-raw')).replace(/\\/g, '/');
const OUT = process.argv[3] || path.join(process.env.TEMP, 'tw-manual-plates.json');
fs.mkdirSync(RAW, { recursive: true });

const UP = 'C:/Users/peter/.claude/uploads/4ed32785-f420-4c49-adc7-81af4c7d4139/';
const IMGS = ['a8af050c-image.jpg', 'b59c20db-image.jpg', '22a61087-image.jpg']
  .map(f => fs.readFileSync(UP + f).toString('base64'));

const J = [];
const wait = (ms, name) => J.push({ name: name || 'wait', wait: ms });
const ev = (name, fn) => J.push({ name, eval: fn });
const shot = (name, sel, pad) => J.push({ name, shoot: RAW + '/' + name + '.png', sel, pad: pad || 0 });
const whole = name => J.push({ name, shoot: RAW + '/' + name + '.png' });

/* A helper installed once in the page: P sets a parameter on both sides. */
const HELPERS = `(function(){
  var T = window.__TW, B = window.__JUCE__.backend;
  window.__P = function(id, v){ T.setParam(id, v); B.emitEvent('tw', { k:'p', id:id, v:v }); };
  window.__F = function(items){ T.setFurn(items); B.emitEvent('tw', { k:'furn', items:items }); };
  window.__D = function(){ B.emitEvent('tw', { k:'presetDefault' }); };
  window.__R = function(on){ B.emitEvent('tw', { k:'showrays', v:on ? 1 : 0 }); };
  /* the control groups carry no ids; name them by their headings */
  var n = 0;
  document.querySelectorAll('.grp').forEach(function(g){
    var h = g.querySelector('h2'); if (!h) return;
    var id = 'g_' + h.textContent.trim().toLowerCase().replace(/[^a-z]+/g, '_');
    if (!g.id) g.id = id; n++;
  });
  window.__BOTTOM = function(on){ var c = document.getElementById('ctrls'); c.scrollTop = on ? 1e5 : 0; return c.scrollTop; };
  window.__HIDE = function(on, keep){
    var wrap = document.getElementById('pov').parentElement;
    Array.prototype.forEach.call(wrap.children, function(e){
      if (e.tagName === 'CANVAS' || (keep && keep.indexOf(e.id) >= 0)) return;
      e.style.visibility = on ? 'hidden' : '';
    });
  };
  return 'helpers in, ' + n + ' groups named: ' + Array.prototype.map.call(document.querySelectorAll('.grp'), function(g){ return g.id; }).join(' ');
})()`;

wait(4500, 'settle');
J.push({ name: 'hi-dpi', cmd: 'Emulation.setDeviceMetricsOverride',
         params: { width: 0, height: 0, deviceScaleFactor: 2, mobile: false } });
wait(1200);
ev('helpers', HELPERS);
ev('window', `innerWidth + ' x ' + innerHeight + ' @ ' + devicePixelRatio + '   build ' + (document.body.innerText.match(/BUILD\\s+(\\d+\\.\\d+)/)||[])[1]`);
ev('cinematic off to start', `(function(){ window.__TW.setCine(false, 'high'); return JSON.stringify(window.__TW.cine()).slice(0, 80); })()`);

/* ---------------------------------------------------- defaults, rays on */
ev('defaults', `(function(){ __D(); return 'default'; })()`);
wait(900);
ev('the arrangement the older plates used', `(function(){
  [['s1x',3/18],['s1y',2.5/9],['s1z',(1.2-0.2)/2.2],['s1yaw',0],['s1type',1],['s1in',3/6],
   ['lisx',4.5/18],['lisy',2.5/9],['lisyaw',0.5],['door1',1],['door2',1],['door3',0]].forEach(function(p){ __P(p[0], p[1]); });
  __R(1); window.__TW.selectSource(0); __BOTTOM(false); return 'set'; })()`);
wait(1400, 'let both views redraw');
whole('panel');
shot('pov', '#povWrap');
shot('plan', '#planWrap');
shot('hud', '#hud', 2);
shot('status', '#status');
shot('ctrl-source', '#g_source', 3);

ev('scroll the control rows to the second row', `__BOTTOM(true)`);
wait(400);
shot('ctrl-levels', '#g_levels_and_head', 3);
shot('ctrl-view', '#g_view_and_files', 3);
shot('ctrl-wav', '#g_test_signal', 3);
ev('back up', `__BOTTOM(false)`);

ev('rays off for the labelled plan', `(function(){ __R(0); return 'off'; })()`);
wait(900);
shot('plan-labels', '#planWrap');

/* --------------------------------------------------- nine surfaces, folds */
ev('materials: hall STUDIO, carpet and cloud in the living room', `(function(){
  __P('mat3',4/4); __P('mat1',2/4); __P('flr1',1/5); __P('cel1',1/5); __P('mat2',1/4); return 'set'; })()`);
wait(1000);
ev('rt per room (250/1k/4k/8k)', `window.__TW.scene().rt.map(function(r){ return r.map(function(v){ return v.toFixed(2); }).join('/'); }).join('   ')`);
shot('ctrl-materials', '#g_materials', 3);
ev('break four walls', `(function(){
  [['fold1a',0.5+0.45/1.2],['foldp1a',0.3],['fold1b',0.5+0.35/1.2],['foldp1b',0.68],['fold3a',1.0],['foldp3a',0.34],['fold3b',0.5+0.5/1.2],['foldp3b',0.7]].forEach(function(p){ __P(p[0], p[1]); });
  return 'folded'; })()`);
wait(1200);
shot('ctrl-folds', '#g_broken_walls', 3);
shot('plan-folded', '#planWrap');
ev('flat walls, factory materials', `(function(){
  ['fold1a','foldp1a','fold1b','foldp1b','fold2a','foldp2a','fold2b','foldp2b','fold3a','foldp3a','fold3b','foldp3b'].forEach(function(id){ __P(id, 0.5); });
  [['mat1',1/4],['mat2',1/4],['mat3',2/4],['flr1',0],['cel1',0]].forEach(function(p){ __P(p[0], p[1]); }); return 'flat'; })()`);
wait(900);

/* ------------------------------------------------------- behind a door */
ev('source in the hall, door open', `(function(){ __R(1);
  __P('s1x',11/18); __P('s1y',3.2/9); __P('s1yaw',180/360); __P('door2',1); __P('lisx',3.2/18); __P('lisy',2.5/9); return 'set'; })()`);
wait(1300);
ev('paths, door open', `(function(){ var s=window.__TW.scene(); return s.paths.filter(function(p){return p.s===0;}).map(function(p){return p.t;}).join(',') + '   DRR ' + s.drr.toFixed(1); })()`);
shot('plan-open', '#planWrap');
ev('shut it', `(function(){ __P('door2',0); return 'shut'; })()`);
wait(1300);
ev('paths, door shut', `(function(){ var s=window.__TW.scene(); return s.paths.filter(function(p){return p.s===0;}).map(function(p){return p.t;}).join(',') + '   DRR ' + s.drr.toFixed(1); })()`);
shot('plan-shut', '#planWrap');

/* ------------------------------------------------------------- the two kinds */
ev('a loudspeaker close and facing you', `(function(){ __R(0);
  __P('door2',1); __P('s1type',1); __P('s1x',2.4/18); __P('s1y',2.9/9); __P('s1z',(1.1-0.2)/2.2); __P('s1yaw',345/360);
  __P('lisx',5.0/18); __P('lisy',2.3/9); __P('lisyaw',168/360); return 'set'; })()`);
wait(1300);
shot('pov-speaker', '#povWrap');
ev('the same source as a PURE point', `(function(){ __P('s1type',0); __P('s1dir',0.6); __P('s1z',(1.35-0.2)/2.2); return 'pure'; })()`);
wait(1300);
shot('pov-pure', '#povWrap');

/* ------------------------------------------------------------- furniture */
ev('defaults again, then furnish the living room (TILED walls, so it shows)', `(function(){
  __D(); return 'default'; })()`);
wait(900);
ev('furnish', `(function(){
  __P('mat1', 3/4);
  __P('s1x',1.6/18); __P('s1y',2.4/9); __P('s1z',(1.05-0.2)/2.2); __P('s1yaw',0); __P('s1type',1); __P('s1in',3/6);
  __P('lisx',5.2/18); __P('lisy',3.3/9); __P('lisyaw',200/360);
  __F([ {t:'rug',x:2.9,y:2.4,yaw:90}, {t:'sofa',x:3.0,y:0.55,yaw:0}, {t:'armchair',x:1.1,y:0.9,yaw:315},
        {t:'bookcase',x:3.2,y:4.78,yaw:180}, {t:'table',x:4.6,y:4.0,yaw:0}, {t:'person',x:2.4,y:3.7,yaw:300},
        {t:'curtain',x:0.12,y:2.4,yaw:270} ]);
  window.__TW.selectFurn(1); __R(0); return 'furnished: ' + window.__TW.furn().length + ' pieces'; })()`);
wait(1600, 'let the engine recompute the room');
ev('what the readout says', `document.getElementById('furnRead').textContent`);
ev('rt of the living room now (250/1k/4k/8k)', `window.__TW.scene().rt[0].map(function(v){ return v.toFixed(2); }).join('/')`);
shot('plan-furn', '#planWrap');
ev('scroll to the furniture group', `__BOTTOM(true)`);
wait(400);

ev('back up', `__BOTTOM(false)`);
ev('no selection for the 3D view', `(function(){ window.__TW.selectFurn(-1); return 'ok'; })()`);
wait(1200);
shot('pov-furn', '#povWrap');
ev('empty the room, and compare the readout', `(function(){ __F([]); return 'empty'; })()`);
wait(1400);
ev('readout with no furniture', `document.getElementById('furnRead').textContent`);
ev('rt of the living room, empty (250/1k/4k/8k)', `window.__TW.scene().rt[0].map(function(v){ return v.toFixed(2); }).join('/')`);

/* -------------------------------------------------------- the gallery */
const GALLERY = `(async function(){
  var T = window.__TW, B = window.__JUCE__.backend;
  __D(); await new Promise(function(r){ setTimeout(r, 900); });
  __P('mat1', 1/4); __P('lisx', 5.3/18); __P('lisy', 2.5/9); __P('lisyaw', 180/360);
  __P('s1x', 0.55/18); __P('s1y', 1.75/9); __P('s1z', (1.05-0.2)/2.2); __P('s1yaw', 0); __P('s1type', 1); __P('s1in', 1/6);
  __P('s2x', 0.55/18); __P('s2y', 3.25/9); __P('s2z', (1.05-0.2)/2.2); __P('s2yaw', 0); __P('s2type', 1); __P('s2in', 2/6);
  __P('s3in', 0); __P('s4in', 0);
  __F([ {t:'rug', x:2.7, y:2.5, yaw:90}, {t:'sofa', x:2.9, y:0.5, yaw:0},
        {t:'armchair', x:1.2, y:4.2, yaw:225}, {t:'bookcase', x:3.0, y:4.78, yaw:180} ]);
  var data = IMGS;
  function dims(b64){ return new Promise(function(ok){ var im = new Image(); im.onload = function(){ ok([im.naturalWidth, im.naturalHeight]); }; im.src = 'data:image/jpeg;base64,' + b64; }); }
  var along = [1.0, 2.5, 4.0], frames = [0, 1, 0], items = [], images = {};
  for (var i = 0; i < 3; i++){
    var d = await dims(data[i]);
    var id = 'gal' + i;
    images[id] = data[i];
    items.push({ id:id, room:0, wall:0, along:along[i], z:1.75, w:0.85, aspect:d[1]/d[0], frame:frames[i], kind:0 });
    B.emitEvent('tw', { k:'picAdd', id:id, jpg:data[i] });
  }
  T.setPics(items, images); B.emitEvent('tw', { k:'pics', items:items });
  __R(0);
  return 'hung ' + items.length + ' pictures';
})()`.replace('IMGS', JSON.stringify(IMGS));
ev('dress the living room as a gallery', GALLERY);
wait(2000);
ev('select the middle picture', `(function(){ window.__TW.selectPic(1); return JSON.stringify(window.__TW.picPlace(1)); })()`);
wait(900);
shot('plan-pics', '#planWrap');
ev('scroll to the furniture group', `__BOTTOM(true)`);
wait(400);
shot('ctrl-furn', '#grpFurn', 3);
ev('back up', `__BOTTOM(false)`);
ev('picture note', `document.getElementById('picNote').textContent`);
ev('deselect, overlays off for the comparison', `(function(){ window.__TW.selectPic(-1); window.__TW.selectFurn(-1); __HIDE(true, ['cineHud']); return 'ok'; })()`);
wait(1200);
shot('cine-off', '#povWrap');

/* ---------------------------------------------------------- CINEMATIC */
ev('cinematic HIGH, held out of PHOTO', `(function(){ window.__TW.setCine(true, 'high'); window.__TW.cineHold(true); return 'on'; })()`);
ev('wait for the shaders', `(async function(){ for (var i = 0; i < 120; i++){ var c = window.__TW.cine(); if (c.ok && c.ms > 0) return 'ready after ' + (i/4) + ' s: ' + JSON.stringify(c).slice(0, 200); await new Promise(function(r){ setTimeout(r, 250); }); } return 'NOT READY ' + JSON.stringify(window.__TW.cine()); })()`);
wait(2500, 'let the frame time settle');
ev('cinematic readout', `document.getElementById('cineHud').textContent`);
shot('cine-on', '#povWrap');
shot('hdr', '#cineBox, #recBox', 8);
ev('let PHOTO take over, at ULTRA', `(function(){ window.__TW.setCine(true, 'ultra'); window.__TW.cineHold(false); return 'photo allowed'; })()`);
ev('wait for PHOTO to converge', `(async function(){ var t0 = Date.now(); for (var i = 0; i < 480; i++){ var c = window.__TW.cine(); if (c.converged) return 'converged after ' + ((Date.now()-t0)/1000).toFixed(1) + ' s: ' + JSON.stringify(c).slice(0, 200); await new Promise(function(r){ setTimeout(r, 250); }); } return 'NOT CONVERGED ' + JSON.stringify(window.__TW.cine()); })()`);
wait(600);
ev('photo readout', `document.getElementById('cineHud').textContent`);
shot('photo', '#povWrap');
ev('overlays back, cinematic off', `(function(){ __HIDE(false); window.__TW.setCine(false, 'high'); return JSON.stringify(window.__TW.cine()).slice(0, 60); })()`);
wait(900);
shot('pov-pics', '#povWrap');

/* ---------------------------------------------------------- REC, EXPORT */
ev('record a short take', `(function(){ window.__JUCE__.backend.emitEvent('tw', { k:'recStart' }); return 'rec'; })()`);
wait(3200);
ev('while recording', `JSON.stringify(window.__TW.rec())`);
shot('hdr-rec', '#cineBox, #recBox', 8);
ev('stop', `(function(){ window.__JUCE__.backend.emitEvent('tw', { k:'recStop' }); return 'stop'; })()`);
wait(1200);
ev('take held', `JSON.stringify(window.__TW.rec())`);
ev('open the export panel', `(function(){ document.getElementById('bExpOpen').click(); return document.getElementById('expPop').hidden ? 'STILL HIDDEN' : 'open'; })()`);
wait(700);
ev('export options', `JSON.stringify(window.__TW.expOpt())`);
shot('exp-pop', '#expPop', 3);
ev('close it', `(function(){ document.getElementById('bExpOpen').click(); return 'closed'; })()`);

/* ---------------------------------------------------------- tidy up */
ev('factory arrangement again, cinematic off', `(function(){ __D(); window.__TW.setCine(false, 'high'); return 'defaults'; })()`);
wait(900);
ev('errors at the end (must be [])', `JSON.stringify(window.__TW.errors || [])`);

fs.writeFileSync(OUT, JSON.stringify(J, null, 1));
console.log('jobs: ' + OUT + '  (' + J.length + ' steps, raw PNGs to ' + RAW + ')');
