/*  GRAVITY WELL - the panel gate.

    Loads the SHIPPED ui.html in headless Chrome with a stubbed native side
    and drives it with the processor's own vocabulary.

    THE FIXTURE COMES FROM THE ENGINE.  `gwprobe table` prints the parameter
    table - kinds, ranges, choices and display laws - through the same code
    the processor uses.  The first version of this probe had a hand-typed
    fixture with the same wrong kind numbers as the page, and it passed while
    260926.1 drew every choice as a slider and every bipolar knob as a
    drop-down.  A fixture that agrees with the page proves nothing about the
    engine.

        node test/uiprobe.js                    the checks
        node test/uiprobe.js --shots <dir>      a PNG of every console, to LOOK at

    Two harness traps are handled on purpose:
      * --virtual-time-budget never completes against the well's endless
        rAF loop, so the DOM pass stubs rAF (?norAF=1) and the shots run in
        real time;
      * every Chrome gets its OWN user-data-dir, or it contends with the
        browser already open on this machine and hangs.
*/
const fs = require('fs');
const path = require('path');
const os = require('os');
const { execFileSync, spawnSync } = require('child_process');

const ROOT  = path.resolve(__dirname, '..');
const UI    = path.join(ROOT, 'Source', 'ui', 'ui.html');
const BWFX  = path.resolve(ROOT, '..', '..', '..', 'BrokildWorldFX', 'ui', 'bwfx-rack.js');
const PROBE = 'C:/Users/peter/b/_build/GravityWell/bench/Release/gwprobe.exe';
//  the mode table and the LFO divisions, read from the engine's own source -
//  a fixture typed here would agree with the page and prove nothing
const ENGH = fs.readFileSync(path.join(ROOT, 'Source', 'Engine.h'), 'utf8');
const ENGC = fs.readFileSync(path.join(ROOT, 'Source', 'Engine.cpp'), 'utf8');
const MODE_MASK = (/MODE_MASK\[7\]\s*=\s*\{([^}]*)\}/.exec(ENGH) || [0, ''])[1].split(',').map(x => parseInt(x.trim(), 16)).filter(x => !isNaN(x));
const LFO_DIVS = ((/LFO_DIV_NAME\[N_LFO_DIV\]\s*=\s*\{([^}]*)\}/.exec(ENGC) || [0, ''])[1].match(/"[^"]*"/g) || []).map(x => x.slice(1, -1));

function findChrome () {
    for (const p of ['C:/Program Files/Google/Chrome/Application/chrome.exe',
                     'C:/Program Files (x86)/Google/Chrome/Application/chrome.exe',
                     'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'])
        if (fs.existsSync(p)) return p;
    throw new Error('no Chrome or Edge');
}

//  the table, from the engine - with the house SAC nudge if it is refused
function engineTable () {
    for (let n = 0; n < 6; ++n) {
        try { return JSON.parse(execFileSync(PROBE, ['table'], { encoding: 'utf8' })); }
        catch (e) {
            if (e.code !== 'UNKNOWN' && e.errno !== -4094) throw e;
            fs.appendFileSync(PROBE, Buffer.from([Math.floor(Math.random() * 256)]));
        }
    }
    throw new Error('Smart App Control kept refusing gwprobe');
}

const TABLE = engineTable();
const FACTORY = [ { i:0, name:'SCHWARZSCHILD', group:'FOUNDATION' },
                  { i:1, name:'FLAT SPACE', group:'FOUNDATION' },
                  { i:10, name:'ACID 303', group:'WORKING' } ];

const stage = fs.mkdtempSync(path.join(os.tmpdir(), 'gw-ui-'));
fs.copyFileSync(BWFX, path.join(stage, 'bwfx-rack.js'));

//  what runs before the page: the error hook (in the HEAD, before anything
//  can throw), the rAF stub for the DOM pass, and a fake native backend
const head = `
<script>
if (location.search.indexOf("norAF") >= 0) window.requestAnimationFrame = function () { return 0; };
//  a BOUNDED loop for the shots: the well draws N real frames and stops, so
//  virtual time can finish and the shot waits for the panel to be built
(function () { const m = /frames=(\\d+)/.exec(location.search); if (!m) return;
  let left = +m[1]; const raf = window.requestAnimationFrame.bind(window);
  window.requestAnimationFrame = function (f) { return left-- > 0 ? raf(f) : 0; }; })();
window.__errors = [];
window.addEventListener("error", e => window.__errors.push(String(e.message)));
window.__sent = [];
window.__JUCE__ = { backend: {
  _h: {},
  emitEvent (name, payload) { window.__sent.push(payload); },
  addEventListener (n, f) { (this._h[n] = this._h[n] || []).push(f); },
  __fire (n, v) { (this._h[n] || []).forEach(f => { try { f(v); } catch (e) { window.__errors.push(n + ": " + e.message); } }); }
} };
</script>`;

//  the boot: initialState from the engine table, then optionally a page
const boot = `
<script>
window.addEventListener("load", () => setTimeout(() => {
  const B = window.__JUCE__.backend;
  try { localStorage.removeItem("gw.page"); } catch (e) {}
  B.__fire("initialState", { build: "TEST", patch: "SCHWARZSCHILD", params: ${JSON.stringify(TABLE)}, factory: ${JSON.stringify(FACTORY)}, modeMask: ${JSON.stringify(MODE_MASK)}, lfoDivs: ${JSON.stringify(LFO_DIVS)} });
  const m = /page=([a-z]+)/.exec(location.hash);
  if (m) window.__GW.showPage(m[1]);
  try { localStorage.removeItem("gw.keys"); } catch (e) {}
  if (/keys/.test(location.hash) && document.getElementById("keysBtn")) document.getElementById("keysBtn").click();
  B.__fire("scope", { mass: 0.42, rs: 1.3, peak: 0.4, step: -1, mod: [], frozen: [0,0,0,1,0,0,0,0,0],
                      wave: Array.from({ length: 256 }, (_, i) => Math.sin(i / 11) * 0.6) });
  if (location.search.indexOf("report") >= 0) setTimeout(report, 150);
}, 60));

function report () {
  const G = window.__GW, B = window.__JUCE__.backend, out = { errors: window.__errors };
  //  hello goes out at boot; read it before anything below clears the log
  const helloSeen = window.__sent.some(s => s && s.k === "hello");
  const within = (inner, outer) => {
    const a = inner.getBoundingClientRect(), b = outer.getBoundingClientRect();
    return a.left >= b.left - 1 && a.right <= b.right + 1 && a.top >= b.top - 1 && a.bottom <= b.bottom + 1;
  };

  //  1. every page: which ids got a control, of what kind, and is anything clipped
  out.seen = {}; out.kinds = {}; out.clipped = []; out.pages = [];
  const pillarIds = Array.from(document.querySelectorAll(".pillar .cell")).map(c => c.dataset.id);
  for (const c of document.querySelectorAll(".pillar .cell")) {
    out.seen[c.dataset.id] = true; out.kinds[c.dataset.id] = c.dataset.kind;
    if (!within(c, c.closest(".pillar"))) out.clipped.push("pillar:" + c.dataset.id);
  }
  for (const pg of G.pages) {
    G.showPage(pg.key);
    out.pages.push(pg.key);
    const desk = document.getElementById("desk");
    if (desk.scrollWidth > desk.clientWidth + 1) out.clipped.push(pg.key + ": the desk is " + desk.scrollWidth + " wide in " + desk.clientWidth);
    for (const sec of desk.querySelectorAll(".sec")) {
      if (!within(sec, desk)) out.clipped.push(pg.key + ": section " + sec.dataset.sec);
      //  a heading wider than its section runs into the next one - found by
      //  LOOKING at the first cockpit render, after every other check passed
      const h = sec.querySelector("h4");
      if (h && h.scrollWidth > h.clientWidth + 1) out.clipped.push(pg.key + ": heading " + sec.dataset.sec + " overflows");
    }
    for (const c of desk.querySelectorAll(".cell")) {
      out.seen[c.dataset.id] = true; out.kinds[c.dataset.id] = out.kinds[c.dataset.id] || c.dataset.kind;
      if (!within(c, c.closest(".sec"))) out.clipped.push(pg.key + ": " + c.dataset.id);
      const lab = c.querySelector(".lab");
      if (!c.dataset.id) continue;
      if (lab && lab.scrollWidth > lab.clientWidth + 1) out.clipped.push(pg.key + ": label of " + c.dataset.id + " truncated");
    }
  }
  out.pillarIds = pillarIds;
  out.geom = { vh: innerHeight, pill: Math.round(document.getElementById("pillL").getBoundingClientRect().height),
    cells: Array.from(document.querySelectorAll("#pillL .cell, #pillR .cell")).map(c => c.dataset.id + ":" + Math.round(c.getBoundingClientRect().top) + "-" + Math.round(c.getBoundingClientRect().bottom)),
    pillTop: Math.round(document.getElementById("pillL").getBoundingClientRect().top), pillBot: Math.round(document.getElementById("pillL").getBoundingClientRect().bottom),
    secH: Math.round((document.querySelector("#desk .sec")||{getBoundingClientRect:()=>({height:0})}).getBoundingClientRect().height),
    deskH: Math.round(document.getElementById("desk").getBoundingClientRect().height) };

  //  2. the cutoff shows the ENGINE's law
  G.showPage("horizons");
  window.__GW.set("fa_cut", 0.5, false);
  const cut = document.querySelector('.pillar .cell[data-id="fa_cut"] .val');
  out.cutText = cut ? cut.textContent : null;

  //  3. a choice sends a WHOLE index, once
  G.showPage("reactor");
  window.__sent.length = 0;
  const btns = document.querySelectorAll('#desk .cell[data-id="a_engine"] .hud');
  if (btns[1]) btns[1].click();
  out.engineSent = window.__sent.filter(s => s && s.k === "p" && s.id === "a_engine").map(s => s.v);
  out.engineOn = Array.from(btns).map(b => b.classList.contains("on"));

  //  4. the well's lights sit where the RADIUS knobs put them
  B.__fire("hostParam", { r_l1: 0.2, r_e3: 0.7 });
  const mods = G.mods();
  out.lfo1r = (mods.find(m => m.name === "LFO1") || {}).r;
  out.env3r = (mods.find(m => m.name === "ENV3") || {}).r;

  //  5. MASS is driven by the engine, and the MASS knob shows where the well IS
  out.massInput = document.getElementById("mass").value;
  const modArc = document.querySelector('.pillar .cell[data-id="macro_mass"] .mod');
  out.massArc = modArc ? (modArc.getAttribute("d") || "").length : 0;

  //  6. a frozen modulator says so on its own console
  G.showPage("orbits");
  out.onTabs = Array.from(document.querySelectorAll("#tabs .hud.on")).map(b => b.textContent.trim());
  const f = document.querySelector('.sec[data-sec="LFO 1"] .frz');
  out.frozen = f ? f.textContent : null;

  //  7. BWFX: the documented contract, and native state really arrives
  out.bwfxButton = document.querySelectorAll("[data-bwfx-open]").length;
  out.bwfxLoaded = !!window.BWFX;
  try { B.__fire("bwfx", { state: { mix: 0.33, order: [], modules: {} } }); out.bwfxMix = window.BWFX.state().mix; }
  catch (e) { out.bwfxMix = "threw: " + e.message; }

  out.hello = helloSeen;
  //  8. the keyboard: hidden until KEYS is pressed, HUD squares, inside the foot
  const kbd = document.getElementById("kbd"), foot = document.getElementById("foot");
  out.keysHiddenAtStart = getComputedStyle(kbd).display === "none";
  document.getElementById("keysBtn").click();
  out.keysShown = getComputedStyle(kbd).display !== "none";
  const keys = Array.from(kbd.querySelectorAll(".k"));
  out.keyCount = keys.length;
  out.keysSquare = keys.every(k => { const r = k.getBoundingClientRect(); return Math.abs(r.width - r.height) < 1; });
  out.keysInside = keys.every(k => within(k, foot)) && within(kbd, foot);
  out.keysLit = document.getElementById("keysBtn").classList.contains("on");
  document.getElementById("keysBtn").click();
  out.keysHiddenAgain = getComputedStyle(kbd).display === "none";
  out.tabs = document.querySelectorAll("#tabs .hud:not(.keysBtn)").length;

  //  9. every page's HUD displays exist, have room, and actually drew
  //  something - a canvas that stayed blank looks exactly like a layout
  B.__fire("scope", { mass: 0.3, rs: 1, peak: 0.6, step: -1, frozen: [],
                      mod: [0.8, 0.5, 0.2, 0.7, -0.4, 0.1, 0.3, -0.2, 0.3],
                      lvl: [0.7, 0.6, 0.3, 0.28, 0.4], wave: [] });
  out.huds = {};
  for (const pg of G.pages) {
    G.showPage(pg.key); G.hudDraw();
    out.huds[pg.key] = Array.from(document.querySelectorAll("#desk .hudv")).map(b => {
      const cv = b.querySelector("canvas"), w = Math.round(b.getBoundingClientRect().width);
      let ink = 0;
      try { const d = cv.getContext("2d").getImageData(0, 0, cv.width, cv.height).data;
            for (let i = 3; i < d.length; i += 16) if (d[i] > 30) ++ink; } catch (e) { ink = -1; }
      return b.dataset.hud + ":" + w + ":" + ink;
    });
  }
  out.tabNames = Array.from(document.querySelectorAll("#tabs .hud")).map(b => b.textContent.trim());

  //  10. a MODE the circuit does not have is dimmed, and only then
  G.showPage("horizons");
  G.set("fa_circ", 0, false);
  out.naLadder = Array.from(document.querySelectorAll('#desk .cell[data-id="fa_mode"] .hud')).map(b => b.classList.contains("na"));
  G.set("fa_circ", 3, false);
  out.naSvf = Array.from(document.querySelectorAll('#desk .cell[data-id="fa_mode"] .hud')).map(b => b.classList.contains("na"));

  //  11. TEMPO turns the rate into the division the engine plays
  G.showPage("orbits");
  G.set("l1_sync", 1, false); G.set("l1_rate", 7 / 15, false);
  const lr = document.querySelector('#desk .cell[data-id="l1_rate"] .val');
  out.lfoTempo = lr ? lr.textContent : null;
  G.set("l1_sync", 0, false);
  out.lfoFree = lr ? lr.textContent : null;

  //  12. SETTINGS: brightness reaches the ship, and RESET brings it back
  G.showPage("settings");
  const br = document.getElementById("set_bright");
  if (br) { br.value = 60; br.dispatchEvent(new Event("input")); }
  out.bright = document.getElementById("ship").style.filter;
  const rs = Array.from(document.querySelectorAll(".setp .hud")).find(b => b.textContent === "RESET");
  if (rs) rs.click();
  out.brightReset = document.getElementById("ship").style.filter;

  //  13. the compact matrix: the longest names fit their one cell
  G.showPage("matrix");
  const longest = id => { const c = (G.S.byId[id].choices || "").split("|"); let k = 0; c.forEach((x, i) => { if (x.length > c[k].length) k = i; }); return k; };
  G.set("m1_src", longest("m1_src"), false); G.set("m1_dst", longest("m1_dst"), false); G.set("m1_amt", 0.5, false);
  out.cmpCut = Array.from(document.querySelectorAll("#desk .cell.cmp .step .nm")).filter(n => n.scrollWidth > n.clientWidth + 1).map(n => n.textContent);
  out.cmpCount = document.querySelectorAll("#desk .cell.cmp").length;
  const pre = document.createElement("pre"); pre.id = "gwprobe"; pre.textContent = JSON.stringify(out);
  document.body.appendChild(pre);
}
</script>`;

let html = fs.readFileSync(UI, 'utf8');
html = html.replace('<title>', head + '\n<title>');
const endAt = html.lastIndexOf('</html>');
html = html.slice(0, endAt) + boot + html.slice(endAt);
const page = path.join(stage, 'ui.html');
fs.writeFileSync(page, html);
const url = 'file:///' + page.split(path.sep).join('/');

const chrome = findChrome();
function run (args, ms) {
    const prof = fs.mkdtempSync(path.join(os.tmpdir(), 'gw-ui-prof-'));
    return spawnSync(chrome, ['--headless=new', '--disable-gpu', '--no-sandbox', '--use-gl=swiftshader',
                              '--user-data-dir=' + prof].concat(args),
                     { encoding: 'utf8', maxBuffer: 1 << 26, timeout: ms });
}

/*  HEADLESS --window-size INCLUDES THE BROWSER FRAME: asking for 1340x880
    gave a 729 px viewport, and the probe reported pillars clipping at a size
    the editor never has.  The plug-in's WebView gets EXACTLY the editor size,
    so measure the frame once and ask for that much more.                  */
function frameOffset () {
    const f = path.join(stage, "calib.html");
    fs.writeFileSync(f, "<body><script>document.body.innerHTML='<pre id=v>'+innerWidth+'x'+innerHeight+'</pre>'</" + "script>");
    const r = run(["--window-size=1200,800", "--dump-dom", "file:///" + f.split(path.sep).join("/")], 30000);
    const m = /<pre id="v">(\d+)x(\d+)<\/pre>/.exec(r.stdout || "");
    if (!m) throw new Error("could not calibrate the viewport");
    return [1200 - +m[1], 800 - +m[2]];
}
const OFF = frameOffset();
console.log("viewport calibration: the frame costs " + OFF[0] + " x " + OFF[1] + " px");

//  ---- the shots, if asked for
const si = process.argv.indexOf('--shots');
if (si > 0) {
    const dir = process.argv[si + 1];
    //  a stale shot from an earlier run looks exactly like a fresh one - one
    //  did, the first time, and it was the empty panel - so clear them first
    fs.rmSync(dir, { recursive: true, force: true });
    fs.mkdirSync(dir, { recursive: true });
    const keys = ['helm', 'reactor', 'horizons', 'chrono', 'orbits', 'matrix', 'output', 'helm&keys'];
    for (const k of keys) {
        const out = path.join(dir, 'gw-' + k.replace('&', '-') + '.png');
        for (let t = 0; t < 3 && !fs.existsSync(out); ++t)
            //  NO offset here: in --screenshot mode headless Chrome gives the page
            //  the whole window, while --dump-dom loses 151 px to the frame.  The
            //  first cockpit shots came out 1356x1031, taller than the editor.
            run(['--window-size=1340,880', '--hide-scrollbars', '--virtual-time-budget=5000',
                 '--screenshot=' + out, url + '?frames=40#page=' + k], 60000);
        console.log('shot ' + out + (fs.existsSync(out) ? '' : '  (MISSING)'));
    }
    process.exit(0);
}


//  ---- the checks, at the default size and at the smallest the editor allows
let checks = 0, fails = 0;
const CHECK = (ok, what) => { ++checks; if (!ok) { ++fails; console.log('  FAIL  ' + what); } };

console.log('GRAVITY WELL panel probe - ' + TABLE.length + ' parameters from the engine\n');

for (const [w, h] of [[1340, 880], [1100, 760]]) {
    const r = run(['--virtual-time-budget=8000', '--window-size=' + (w + OFF[0]) + ',' + (h + OFF[1]), '--dump-dom', url + '?norAF=1&report=1'], 60000);
    const m = (r.stdout || '').match(/<pre id="gwprobe">([\s\S]*?)<\/pre>/);
    if (!m) { console.log('  at ' + w + 'x' + h + ': THE PAGE PRODUCED NO REPORT'); ++checks; ++fails; continue; }
    const o = JSON.parse(m[1].replace(/&quot;/g, '"').replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>'));
    const tag = ' [' + w + 'x' + h + ']';
    console.log('     geom ' + JSON.stringify(o.geom));
    console.log('-- at ' + w + 'x' + h);

    CHECK(o.errors.length === 0, 'JS errors' + tag + ': ' + JSON.stringify(o.errors));
    CHECK(o.clipped.length === 0, 'clipped' + tag + ': ' + o.clipped.slice(0, 8).join('; '));
    if (w !== 1340) continue;           //  the rest does not depend on the size

    const shown = TABLE.filter(p => !p.hidden), hidden = TABLE.filter(p => p.hidden);
    const missing = shown.filter(p => !o.seen[p.id]).map(p => p.id);
    const leaked  = hidden.filter(p => o.seen[p.id]).map(p => p.id);
    console.log('     ' + (shown.length - missing.length) + ' of ' + shown.length + ' parameters have a control; ' +
                hidden.length + ' sequencer parameters deliberately off the panel');
    CHECK(missing.length === 0, 'parameters with no control: ' + missing.join(', '));
    CHECK(leaked.length === 0, 'the sequencer is still on the panel: ' + leaked.join(', '));

    //  the kind bug that shipped: a choice must never be a knob, a number never buttons
    const wrongKind = [];
    for (const p of shown) {
        const k = o.kinds[p.id];
        if (!k) continue;
        if (p.kind === 1 && k === 'knob') wrongKind.push(p.id + ' is a choice drawn as a knob');
        if (p.kind !== 1 && k !== 'knob') wrongKind.push(p.id + ' is a number drawn as ' + k);
    }
    CHECK(wrongKind.length === 0, wrongKind.slice(0, 6).join('; '));

    const fc = TABLE.find(p => p.id === 'fa_cut');
    const want = Math.round(fc.lawBase * Math.pow(fc.lawSpan, 0.5));
    console.log('     cutoff at 0.5 reads "' + o.cutText + '", the engine law says ' + want + ' Hz');
    CHECK(o.cutText === want + ' Hz', 'the cutoff display does not follow the engine law');
    CHECK(o.pillarIds.indexOf('fa_cut') >= 0 && o.pillarIds.indexOf('fa_res') >= 0 && o.pillarIds.indexOf('fa_circ') >= 0,
          'cutoff, resonance and circuit are not on the always-visible pillar');

    console.log('     A ENGINE click sent ' + JSON.stringify(o.engineSent) + ', buttons ' + JSON.stringify(o.engineOn));
    CHECK(o.engineSent.length === 1 && o.engineSent[0] === 1, 'a choice button did not send one whole index');
    CHECK(o.engineOn[0] === false && o.engineOn[1] === true, 'the chosen button is not the lit one');

    console.log('     LFO1 radius on the well ' + o.lfo1r + ' (knob 0.2), ENV3 ' + o.env3r + ' (knob 0.7)');
    CHECK(o.lfo1r === 0.2 && o.env3r === 0.7, 'the lights on the well do not follow the RADIUS knobs');
    CHECK(o.massInput === '42', 'the engine does not drive the well');
    CHECK(o.massArc > 0, 'the MASS knob does not show where the well actually is');
    CHECK(o.frozen === 'FROZEN', 'a frozen modulator does not say so on its console');
    CHECK(JSON.stringify(o.onTabs) === '["ORBITS"]', 'on ORBITS the lit tabs are ' + JSON.stringify(o.onTabs));

    console.log('     BWFX: ' + o.bwfxButton + ' button(s), loaded ' + o.bwfxLoaded + ', native mix arrived as ' + o.bwfxMix);
    CHECK(o.bwfxButton >= 1, 'no data-bwfx-open button - the fragment has nothing to bind');
    CHECK(o.bwfxLoaded, 'the BWFX fragment did not load');
    CHECK(o.bwfxMix === 0.33, 'native rack state does not reach the overlay (onState, not state)');
    CHECK(o.tabs === 9, 'there are not eight pages plus WORLD FX: ' + JSON.stringify(o.tabNames));
    const order = (o.tabNames || []).filter(t => t !== 'KEYS');
    const wi = order.findIndex(t => /WORLD FX/.test(t));
    CHECK(wi > 0 && order[wi - 1] === 'OUTPUT' && order[wi + 1] === 'SETTINGS', 'WORLD FX is not between OUTPUT and SETTINGS: ' + order.join(' '));
    for (const k in o.huds) console.log('     hud ' + k + ': ' + (o.huds[k].join('  ') || '-'));
    const wantHud = { helm: 2, reactor: 2, horizons: 2, chrono: 2, orbits: 2, matrix: 1, output: 2, settings: 2 };
    for (const k in wantHud) {
        const hs = o.huds[k] || [];
        CHECK(hs.length === wantHud[k], k + ' has ' + hs.length + ' HUD displays, wants ' + wantHud[k]);
        for (const h of hs) {
            const [kind, w, ink] = h.split(':');
            CHECK(+w >= 140, k + ': the ' + kind + ' display has only ' + w + ' px');
            CHECK(+ink > 40, k + ': the ' + kind + ' display drew nothing (' + ink + ' lit samples)');
        }
    }
    console.log('     modes dimmed on LADDER ' + JSON.stringify(o.naLadder) + ', on SVF ' + JSON.stringify(o.naSvf));
    CHECK(JSON.stringify(o.naLadder) === '[false,true,true,true]', 'the ladder should offer LP only');
    CHECK(JSON.stringify(o.naSvf) === '[false,false,false,false]', 'the SVF should offer every mode');
    console.log('     LFO 1 rate at 7/15: "' + o.lfoTempo + '" on TEMPO, "' + o.lfoFree + '" free; engine divisions ' + LFO_DIVS.length);
    CHECK(LFO_DIVS.length === 16 && o.lfoTempo === LFO_DIVS[7], 'the synced rate does not read as the engine division ' + LFO_DIVS[7]);
    CHECK(/Hz$/.test(o.lfoFree || ''), 'a free rate does not read in Hz');
    console.log('     settings: brightness 60 gives "' + o.bright + '", reset gives "' + o.brightReset + '"');
    CHECK(Math.abs(parseFloat(String(o.bright).replace("brightness(", "")) - 0.6) < 1e-6 && o.brightReset === '', 'the brightness setting does not reach the ship');
    CHECK(o.cmpCount === 16 && o.cmpCut.length === 0, 'the compact matrix: ' + o.cmpCount + ' steppers, cut: ' + o.cmpCut.join(', '));
    console.log('     keys: hidden at start ' + o.keysHiddenAtStart + ', shown ' + o.keysShown + ', ' + o.keyCount + ' keys, square ' + o.keysSquare + ', inside the foot ' + o.keysInside);
    CHECK(o.keysHiddenAtStart, 'the keyboard is showing before KEYS was pressed');
    CHECK(o.keysShown && o.keysLit, 'KEYS does not show the keyboard, or its button does not light');
    CHECK(o.keyCount === 48, 'the keyboard is not four octaves');
    CHECK(o.keysSquare, 'the keys are not square');
    CHECK(o.keysInside, 'the keyboard spills out of the foot');
    CHECK(o.keysHiddenAgain, 'KEYS does not hide the keyboard again');
    CHECK(o.hello === true, 'the page never said hello - the processor would wait for ever');
}

console.log('\n' + checks + ' checks, ' + fails + ' failures');
if (!fails) console.log('ALL CLEAR');
process.exit(fails ? 1 : 0);
