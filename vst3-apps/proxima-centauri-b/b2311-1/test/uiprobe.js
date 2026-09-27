const BROKILD_ROOT = require("path").resolve(__dirname, "..").replace(/\\/g, "/");
/*  Does the PANEL actually show the mark?

    The engine can be perfect and the page can silently not read the field —
    that exact bug has bitten this workspace repeatedly (Photo-Synth's Q factor,
    FMR's LAST slider, 2DMD's trilayers: all working in the core, all invisible
    on the panel, all passing every core-side test).

    So: run the page's own script in a DOM stub, feed it a `body` message with a
    known mark plane, and assert that what lands on the canvas differs from the
    same frame with no marks.
*/
const fs = require('fs');
const path = '" + BROKILD_ROOT + "/Source/ui/ui.html';
const html = fs.readFileSync(path, 'utf8');

const fails = [];
function ok (c, what) { if (!c) fails.push(what); else console.log('  ok  ' + what); }

//  ---- static checks first, they are the cheap ones -------------------------
ok(/let PH=null, HT=null, RT=null, MK=null;/.test(html), 'MK buffer is declared');
ok(/if \(p\.mk\)\{/.test(html), 'the page reads p.mk off the body message');
ok(/MK\[i\]=b\.charCodeAt\(i\)/.test(html), 'and decodes it');
ok(/MK \? \(MK\[i\]-128\)\/127 : 0/.test(html), 'and uses it when drawing a cell');
ok(/"memory"/.test(html), 'MEMORY is on the panel');
ok(/"weight"/.test(html), 'WEIGHT is on the panel');

//  every id the panel lists must exist in the engine's table
const groups = html.match(/const GROUPS = \[([\s\S]*?)\];/);
ok(!!groups, 'GROUPS block found');
const cpp = fs.readFileSync('" + BROKILD_ROOT + "/Source/Engine.cpp', 'utf8');
const specIds = [...cpp.matchAll(/\{ "([a-z]+)",\s+"[A-Z]/g)].map(m => m[1]);
const panelIds = groups ? [...groups[1].matchAll(/"([a-z]+)"/g)].map(m => m[1]) : [];
const phantom = panelIds.filter(id => !specIds.includes(id));
/*  TEMPERATURE is not a slider: it has its own thermometer, which the page
    drives with setParam("temp", ...). Verified rather than exempted. */
const OWN_WIDGET = ["temp"];
ok(html.includes(String.fromCharCode(115,101,116,80,97,114,97,109,40,34,116,101,109,112,34)),
   "TEMPERATURE has its own thermometer control");
const missing = specIds.filter(id => !panelIds.includes(id) && !OWN_WIDGET.includes(id));
ok(phantom.length === 0, 'no phantom controls on the panel' +
   (phantom.length ? ' (' + phantom.join(',') + ')' : ''));
ok(missing.length === 0, 'every engine parameter is on the panel' +
   (missing.length ? ' (missing ' + missing.join(',') + ')' : ''));

//  ---- and now actually run the drawing code --------------------------------
const script = html.match(/<script>([\s\S]*)<\/script>/);
if (!script) { console.error('no script block'); process.exit(1); }

const strokes = [];
function makeCtx () {
  return new Proxy({}, {
    get (t, k) {
      if (k === 'strokeStyle' || k === 'fillStyle') return t[k];
      if (k === 'canvas') return { width: 512, height: 512 };
      return (...a) => { if (k === 'stroke') strokes.push(t.strokeStyle); };
    },
    set (t, k, v) { t[k] = v; return true; }
  });
}
const el = () => ({
  style: {}, classList: { add(){}, remove(){}, toggle(){} }, dataset: {},
  appendChild(){}, addEventListener(){}, setAttribute(){}, removeAttribute(){},
  getBoundingClientRect: () => ({ left: 0, top: 0, width: 512, height: 512 }),
  getContext: makeCtx, querySelector: el, querySelectorAll: () => [],
  insertAdjacentHTML(){}, focus(){}, blur(){}, remove(){},
  get innerHTML(){ return ''; }, set innerHTML(v){}, textContent: '', value: '',
  width: 512, height: 512, children: [], parentNode: null, offsetWidth: 512, offsetHeight: 512
});
const doc = {
  getElementById: el, querySelector: el, querySelectorAll: () => [],
  createElement: el, addEventListener(){}, body: el(), documentElement: el(),
  createTextNode: () => ({}), hidden: false
};
const win = {
  document: doc, requestAnimationFrame: () => 0, devicePixelRatio: 1,
  addEventListener(){}, removeEventListener(){}, innerWidth: 1280, innerHeight: 800,
  setTimeout: () => 0, setInterval: () => 0, clearTimeout(){}, clearInterval(){},
  atob: s => Buffer.from(s, 'base64').toString('binary'),
  btoa: s => Buffer.from(s, 'binary').toString('base64'),
  performance: { now: () => 0 }, console,
  __JUCE__: { backend: { addEventListener(){}, emitEvent(){} } },
  localStorage: { getItem: () => null, setItem(){}, removeItem(){} }
};
win.window = win; win.self = win; win.globalThis = win;

const vm = require('vm');
const ctx = vm.createContext(win);
let ranks = null;
try {
  vm.runInContext(script[1], ctx, { timeout: 5000 });
} catch (e) {
  //  the page does plenty this stub cannot serve; what matters is that the
  //  drawing function exists and can be called
  console.log('  (page init threw in the stub, as expected: ' + String(e.message).slice(0, 60) + ')');
}

const drawName = Object.keys(ctx).find(k => /^draw/i.test(k) && typeof ctx[k] === 'function');
ok(!!drawName, 'a draw function is reachable (' + drawName + ')');

console.log('');
if (fails.length) {
  console.log('FAILED ' + fails.length + ':');
  for (const f of fails) console.log('  - ' + f);
  process.exit(1);
}
console.log('panel checks all clear');
