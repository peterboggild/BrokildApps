/*  Build Source/ui/ui.html: the cockpit.

    The well is AUTHORED in tools/well-demo.html, which still opens in a
    browser on its own - every visual fault in it was found that way.  This
    composes the shipped page from that one source plus the ship around it:

      * the VIEWSCREEN: the well, large, with telemetry inside the glass
      * two PILLARS carrying the six macros, on every page
      * the DESK: seven switchable consoles of knobs and HUD buttons

    Peter, 2026-09-27: "the current solution looks like an HTML app, not a
    professional VST3 ... How about making it all look like a spaceship? In
    that way the display makes more sense?"  It does: the well stops being a
    picture beside some sliders and becomes the window you fly by.

    THE LAYOUT MAP BELOW IS CHECKED HERE, AT BUILD TIME: every parameter in
    Engine.h must have exactly ONE home console (the HELM may also show
    shortcuts to parameters that live elsewhere), and every id the map names
    must exist.  A parameter added to the engine and forgotten here fails the
    build instead of silently having no knob.  The sequencer is off the panel
    by decision (Peter, 2026-09-27) and is the only exemption.

    The kind numbers are READ from Engine.h's #defines.  260926.1 shipped with
    the page assuming a different numbering, so bipolar controls were drawn as
    drop-downs and CHOICES AS SLIDERS - which is what made A ENGINE and B
    ENGINE flicker under the mouse: a slider sent 0.37, the plug-in stored a
    whole choice, the echo sent it back mid-drag, and the two fought.  The
    panel probe passed throughout, because its hand-typed fixture used the
    same wrong numbers as the page.

        node tools/build-ui.js
*/
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const SRC  = path.join(ROOT, 'tools', 'well-demo.html');
const ENGH = path.join(ROOT, 'Source', 'Engine.h');
const OUT  = path.join(ROOT, 'Source', 'ui', 'ui.html');

// ------------------------------------------------------------- the engine
const engh = fs.readFileSync(ENGH, 'utf8');
const KIND = {};
for (const m of engh.matchAll(/#define\s+GW_KIND_([A-Z]+)\s+(\d+)/g)) KIND[m[1]] = +m[2];
for (const k of ['LIN', 'CHOICE', 'HZ', 'DB', 'PCT', 'SEC', 'INT', 'BIPOL'])
    if (!(k in KIND)) throw new Error('Engine.h has no GW_KIND_' + k);

const TABLE = [];
for (const m of engh.matchAll(/^\s*X\(\s*([a-z0-9_]+)\s*,\s*"([^"]*)"\s*,[^,]*,[^,]*,[^,]*,\s*GW_KIND_([A-Z]+)/gm))
    TABLE.push({ id: m[1], label: m[2], kind: m[3] });
if (TABLE.length < 100) throw new Error('read only ' + TABLE.length + ' parameters from Engine.h');

// ------------------------------------------------------------- the map
//  [id, short label]: a word the section heading already says is dropped
//  LEFT PILLAR: the filter station, on every page.  Peter, 2026-09-27: "cutoff
//  is the bread and butter of live manipulation of bass sounds" - so it is
//  the biggest knob on the ship and it never leaves the screen.  These are
//  SHORTCUTS: their home is HORIZONS, and the check below insists on it.
const PILLAR_L = { via: 'horizons', items: [['fa_cut','CUTOFF'],['fa_res','RESONANCE'],['fa_circ','CIRCUIT']] };
//  RIGHT PILLAR: the six macros, which live here and nowhere else
const PILLAR_R = ['macro_mass', 'macro_depth', 'macro_energy', 'macro_horiz', 'macro_time', 'macro_space'];

const PAGES = [
  //  hud: the line displays either side of the consoles, [left, right]
  { key: 'helm', name: 'HELM', hud: ['filter', 'meters'], sections: [
    { name: 'GRAVITY', items: [['mass_track','TRACK'],['mass_vel','VELOCITY'],['redshift','REDSHIFT'],
                               ['ring_amt','RINGDOWN'],['ring_decay','RING DECAY'],['ring_tone','RING TONE']] },
    { name: 'SOURCE', via: 'reactor', items: [['a_shape','SHAPE'],['a_level','LEVEL'],['sub_level','SUB']] },
    { name: 'HORIZON A', via: 'horizons', items: [['fa_mode','MODE'],['fa_env','ENV'],['fa_drive','DRIVE'],['fa_key','KEY']] },
    { name: 'DRIVE', via: 'output', items: [['drv_type','TYPE'],['drv_amt','AMOUNT'],['drv_pos','POSITION']] },
    { name: 'AMP', via: 'chrono', items: [['e1_a','ATTACK'],['e1_d','DECAY'],['e1_s','SUSTAIN'],['e1_r','RELEASE']] },
    { name: 'OUT', via: 'output', items: [['out_trim','TRIM']] },
  ]},
  { key: 'reactor', name: 'REACTOR', hud: ['osc', 'subnoise'], sections: [
    { name: 'OSCILLATOR A', items: [['a_engine','ENGINE'],['a_table','TABLE'],['a_shape','SHAPE'],['a_width','WIDTH'],
                                    ['a_oct','OCTAVE'],['a_semi','SEMI'],['a_fine','FINE'],['a_level','LEVEL']] },
    { name: 'OSCILLATOR B', items: [['b_engine','ENGINE'],['b_table','TABLE'],['b_shape','SHAPE'],['b_width','WIDTH'],
                                    ['b_oct','OCTAVE'],['b_semi','SEMI'],['b_fine','FINE'],['b_level','LEVEL']] },
    { name: 'CROSS', items: [['sync','SYNC'],['pm','PHASE MOD'],['ringmod','RING MOD']] },
    { name: 'SUB', items: [['sub_level','LEVEL'],['sub_oct','OCT DOWN'],['sub_shape','SHAPE']] },
    { name: 'NOISE', items: [['noise_level','LEVEL'],['noise_col','COLOUR']] },
    { name: 'ACCRETION DISK', items: [['disk_n','VOICES'],['disk_spread','SPREAD'],['disk_rot','ORBIT'],['disk_width','WIDTH']] },
  ]},
  { key: 'horizons', name: 'HORIZONS', hud: ['filter', 'routing'], sections: [
    { name: 'HORIZON A', items: [['fa_circ','CIRCUIT'],['fa_mode','MODE'],['fa_cut','CUTOFF'],['fa_res','RESONANCE'],
                                 ['fa_drive','DRIVE'],['fa_key','KEY'],['fa_env','ENV']] },
    { name: 'HORIZON B', items: [['fb_circ','CIRCUIT'],['fb_mode','MODE'],['fb_cut','CUTOFF'],['fb_res','RESONANCE'],
                                 ['fb_drive','DRIVE'],['fb_key','KEY'],['fb_env','ENV']] },
    { name: 'ROUTING', items: [['route','ROUTING'],['split','SPLIT'],['blend','A / B']] },
    { name: 'SINGULARITY', items: [['sing_src','SOURCE'],['sing_freq','FREQ'],['sing_level','LEVEL']] },
  ]},
  //  ENV 2 is the filter envelope: both horizons' ENV knobs read it.  ENV 3
  //  reaches nothing until the matrix sends it somewhere - the free one.
  { key: 'chrono', name: 'CHRONO', hud: ['env', 'envlive'], sections: [
    { name: 'AMP ENVELOPE', items: [['e1_a','ATTACK'],['e1_d','DECAY'],['e1_s','SUSTAIN'],['e1_r','RELEASE'],['e1_curve','CURVE']] },
    { name: 'FILTER ENVELOPE', mod: 1, items: [['e2_a','ATTACK'],['e2_d','DECAY'],['e2_s','SUSTAIN'],['e2_r','RELEASE'],['e2_curve','CURVE'],['r_e2','RADIUS']] },
    { name: 'DEPTH', via: 'horizons', items: [['fa_env','TO A CUT'],['fb_env','TO B CUT']] },
    { name: 'ENV 3 · FREE', mod: 2, items: [['e3_a','ATTACK'],['e3_d','DECAY'],['e3_s','SUSTAIN'],['e3_r','RELEASE'],['e3_curve','CURVE'],['r_e3','RADIUS']] },
  ]},
  { key: 'orbits', name: 'ORBITS', hud: ['lfo', 'lfolive'], sections: [
    { name: 'LFO 1', mod: 3, compact: 1, items: [['l1_rate','RATE'],['l1_shape','SHAPE'],['l1_sync','SYNC'],['r_l1','RADIUS'],['l1_amt','AMT'],['l1_dst','TARGET']] },
    { name: 'LFO 2', mod: 4, compact: 1, items: [['l2_rate','RATE'],['l2_shape','SHAPE'],['l2_sync','SYNC'],['r_l2','RADIUS'],['l2_amt','AMT'],['l2_dst','TARGET']] },
    { name: 'LFO 3', mod: 5, compact: 1, items: [['l3_rate','RATE'],['l3_shape','SHAPE'],['l3_sync','SYNC'],['r_l3','RADIUS'],['l3_amt','AMT'],['l3_dst','TARGET']] },
    { name: 'RANDOM', mod: 6, items: [['rnd_rate','RATE'],['rnd_smooth','SMOOTH'],['r_rnd','RADIUS']] },
    { name: 'CHIRP', mod: 7, items: [['chirp_time','TIME'],['r_chirp','RADIUS']] },
  ]},
  //  compact: the source and destination steppers take one cell, not two,
  //  so a slot is one row and the network diagram gets the room
  { key: 'matrix', name: 'MATRIX', hud: ['network', null], hudBig: 1, sections: [
    { name: 'MOD 1 – 3', compact: 1, cols: 3, items: [['m1_src','1 SOURCE'],['m1_dst','1 DEST'],['m1_amt','1 AMT'],
                                              ['m2_src','2 SOURCE'],['m2_dst','2 DEST'],['m2_amt','2 AMT'],
                                              ['m3_src','3 SOURCE'],['m3_dst','3 DEST'],['m3_amt','3 AMT']] },
    { name: 'MOD 4 – 6', compact: 1, cols: 3, items: [['m4_src','4 SOURCE'],['m4_dst','4 DEST'],['m4_amt','4 AMT'],
                                              ['m5_src','5 SOURCE'],['m5_dst','5 DEST'],['m5_amt','5 AMT'],
                                              ['m6_src','6 SOURCE'],['m6_dst','6 DEST'],['m6_amt','6 AMT']] },
    { name: 'MOD 7 – 8', compact: 1, cols: 3, items: [['m7_src','7 SOURCE'],['m7_dst','7 DEST'],['m7_amt','7 AMT'],
                                              ['m8_src','8 SOURCE'],['m8_dst','8 DEST'],['m8_amt','8 AMT']] },
  ]},
  { key: 'output', name: 'OUTPUT', hud: ['meters', 'loud'], sections: [
    { name: 'DRIVE', items: [['drv_type','TYPE'],['drv_amt','AMOUNT'],['drv_pos','POSITION']] },
    { name: 'VOICE', items: [['voicing','VOICING'],['glide','GLIDE'],['bend_range','BEND'],['transpose','TRANSPOSE'],['vel_amt','VELOCITY']] },
    { name: 'CHARACTER', items: [['vintage','VINTAGE'],['tilt','TILT']] },
    { name: 'LEVEL', items: [['out_trim','TRIM']] },
  ]},
  //  WORLD FX sits between OUTPUT and SETTINGS in the tab row, but it is the
  //  rack's own overlay rather than a page of ours
  { key: 'settings', name: 'SETTINGS', hud: ['filter', 'meters'], sections: [
    { name: 'DISPLAY', settings: true, items: [] },
  ]},
];

// ---------------------------------------------------- the build-time check
const byId = Object.fromEntries(TABLE.map(p => [p.id, p]));
const home = {};
const problems = [];
for (const id of PILLAR_R) {
    if (!byId[id]) problems.push('pillar names unknown id ' + id);
    home[id] = (home[id] || []).concat('pillar');
}
for (const [id] of PILLAR_L.items) {
    if (!byId[id]) { problems.push('left pillar names unknown id ' + id); continue; }
    const real = PAGES.find(p => p.key === PILLAR_L.via);
    if (!real || !real.sections.some(s => !s.via && s.items.some(x => x[0] === id)))
        problems.push('left pillar shortcut ' + id + ' is not at home on ' + PILLAR_L.via);
}
for (const pg of PAGES) for (const sec of pg.sections) for (const it of sec.items) {
    const id = Array.isArray(it) ? it[0] : it;
    if (!byId[id]) { problems.push(pg.key + '/' + sec.name + ' names unknown id ' + id); continue; }
    if (sec.via) {
        //  a shortcut must point at the console that really is the home
        const real = PAGES.find(p => p.key === sec.via);
        const there = real && real.sections.some(s => !s.via && s.items.some(x => (Array.isArray(x) ? x[0] : x) === id));
        if (!there) problems.push('shortcut ' + id + ' says its home is ' + sec.via + ' and it is not there');
        continue;
    }
    home[id] = (home[id] || []).concat(pg.key);
}
for (const p of TABLE) {
    if (p.id.startsWith('seq_')) {                      //  off the panel, by decision
        if (home[p.id]) problems.push(p.id + ' is on the panel but the sequencer was taken off it');
        continue;
    }
    const h = home[p.id] || [];
    if (h.length === 0) problems.push(p.id + ' (' + p.label + ') has NO home - it would be invisible');
    if (h.length > 1)  problems.push(p.id + ' has ' + h.length + ' homes: ' + h.join(', '));
}
if (problems.length) {
    console.log('THE LAYOUT MAP DOES NOT COVER THE ENGINE - nothing written:');
    for (const p of problems) console.log('  - ' + p);
    process.exit(1);
}

// ------------------------------------------------------------- the well
const demo = fs.readFileSync(SRC, 'utf8');
function block (tag) {
    const a = demo.indexOf('<' + tag + '>');
    const b = demo.indexOf('</' + tag + '>');
    if (a < 0 || b < 0) throw new Error('no <' + tag + '> in well-demo.html');
    return demo.slice(a + tag.length + 2, b);
}
const wellJs = block('script');
if (wellJs.indexOf('const MODS') < 0) throw new Error('the demo no longer declares MODS - the radii cannot be driven');

// ------------------------------------------------------------- the page
const html = `<!doctype html>
<html lang="en">
<meta charset="utf-8">
<title>Gravity Well</title>
<!--  GENERATED by tools/build-ui.js from tools/well-demo.html and Engine.h.
      Do not edit by hand: edit the demo (the well) or the generator (the
      ship) and run it again.  -->
<style>
:root{
  --void:#03050a; --hull:#070b12; --hull2:#0b111a; --seam:#18263a; --seam2:#223650;
  --ink:#d4e6f7; --dim:#5f7890; --dimmer:#34495f;
  --cyan:#4fc3f7; --cyan2:#8fdcff; --amber:#ffb74d; --red:#ff5a5a;
  --glow:0 0 6px rgba(79,195,247,.55), 0 0 14px rgba(79,195,247,.25);
  --mono:ui-monospace,"Cascadia Mono",Consolas,monospace;
}
*{box-sizing:border-box}
html,body{margin:0;height:100%;overflow:hidden;background:var(--void);color:var(--ink);
          font:11px/1.3 var(--mono);user-select:none;-webkit-user-select:none}
[hidden]{display:none!important}

/* --------------------------------------------------------------- the ship */
#ship{position:fixed;inset:0;display:grid;
      grid-template-rows:40px minmax(0,1fr) 30px 264px 62px;
      background:radial-gradient(ellipse at 50% 18%,#0b1422 0%,var(--void) 70%)}

#hdr{display:flex;align-items:center;gap:14px;padding:0 14px;
     background:linear-gradient(#0c131d,#070b12);border-bottom:1px solid var(--seam)}
#hdr .plate{letter-spacing:.36em;font-weight:700;color:#e6f3ff;font-size:13px;
            text-shadow:0 0 10px rgba(79,195,247,.35)}
#hdr .sub{letter-spacing:.2em;color:var(--dimmer);font-size:9px}
#hdr .spacer{flex:1}

/* the patch display, a HUD window */
#prog{display:flex;align-items:stretch;border:1px solid var(--seam2);background:#050910;
      box-shadow:inset 0 0 12px rgba(79,195,247,.08)}
#prog button{border:0;background:transparent;color:var(--cyan);padding:0 9px;font:13px var(--mono);cursor:pointer}
#prog button:hover{color:var(--cyan2);text-shadow:var(--glow)}
#progName{min-width:250px;padding:4px 10px;cursor:pointer;display:flex;flex-direction:column;justify-content:center}
#progName .g{font-size:8px;letter-spacing:.24em;color:var(--dim)}
#progName .n{font-size:12px;letter-spacing:.14em;color:var(--cyan2);text-shadow:0 0 8px rgba(79,195,247,.45)}

/* the HUD button, used everywhere */
.hud{background:rgba(8,16,26,.9);border:1px solid var(--seam2);color:var(--dim);
     font:9px/1 var(--mono);letter-spacing:.12em;padding:0 8px;cursor:pointer;
     transition:color .12s,border-color .12s,box-shadow .12s,background .12s}
.hud:hover{color:var(--ink);border-color:#31557a}
.hud.on{color:#e9f8ff;border-color:var(--cyan);background:rgba(79,195,247,.17);
        box-shadow:var(--glow);text-shadow:0 0 6px rgba(143,220,255,.9)}
#hdr .hud{height:24px}
.hud.warn:hover{border-color:#7a3131;color:#ffd0d0}

/* --------------------------------------------------------- the bridge row */
#bridge{display:grid;grid-template-columns:170px minmax(0,1fr) 170px;gap:10px;padding:10px 12px 8px;min-height:0}
.pillar{display:flex;flex-direction:column;justify-content:space-evenly;align-items:center;
        background:linear-gradient(90deg,#060a11,#0a1019 50%,#060a11);border:1px solid var(--seam);
        position:relative;min-height:0;padding-top:14px}
.pillar::before{content:"";position:absolute;inset:6px;border:1px solid rgba(79,195,247,.06);pointer-events:none}
.pillar .tag{position:absolute;top:6px;left:0;right:0;text-align:center;font-size:8px;letter-spacing:.3em;color:var(--dimmer)}
.pillar .cell .lab{font-size:9px;letter-spacing:.2em}
#pillR{display:grid;grid-template-columns:1fr 1fr;align-content:space-evenly;justify-items:center}
#pillL .cell.w2{width:146px}
#pillL .opts .hud{height:17px;font-size:8px}

/* the viewscreen */
#screen{position:relative;min-height:0;border:1px solid #20344c;background:#000;overflow:hidden;
        box-shadow:0 0 0 1px #05080d, 0 0 30px rgba(79,195,247,.10), inset 0 0 60px rgba(0,0,0,.9)}
#screen canvas#c{position:absolute;inset:0;width:100%;height:100%;display:block}
#screen .glass{position:absolute;inset:0;pointer-events:none;
  background:
    linear-gradient(160deg,rgba(143,220,255,.07) 0%,rgba(143,220,255,0) 32%),
    repeating-linear-gradient(0deg,rgba(0,0,0,.18) 0 1px,rgba(0,0,0,0) 1px 3px),
    radial-gradient(ellipse at 50% 50%,rgba(0,0,0,0) 55%,rgba(0,0,0,.55) 100%)}
#screen .br{position:absolute;width:22px;height:22px;border:2px solid rgba(79,195,247,.55);pointer-events:none}
#screen .br.tl{top:8px;left:8px;border-right:0;border-bottom:0}
#screen .br.tr{top:8px;right:8px;border-left:0;border-bottom:0}
#screen .br.bl{bottom:8px;left:8px;border-right:0;border-top:0}
#screen .br.bb{bottom:8px;right:8px;border-left:0;border-top:0}
#tele{position:absolute;left:16px;right:16px;bottom:12px;display:flex;gap:18px;flex-wrap:wrap;
      pointer-events:none;font-size:10px;letter-spacing:.08em;color:var(--dim);
      text-shadow:0 0 6px #000,0 0 2px #000}
#tele b{color:var(--amber);font-weight:600}
#tele .k{color:var(--dimmer);letter-spacing:.2em;font-size:8px;margin-right:6px}
#teleTop{position:absolute;left:18px;top:14px;pointer-events:none;font-size:8px;letter-spacing:.32em;color:rgba(143,220,255,.55)}
#rfps{display:none}

/* ------------------------------------------------------------- the tabs */
#tabs{display:flex;gap:4px;align-items:flex-end;padding:0 12px;border-bottom:1px solid var(--seam2)}
#tabs .hud{height:24px;min-width:92px;border-bottom:0;font-size:10px}
#tabs .wfx{color:#7fe0d0}
#tabs .wfx .bwfx-globe{height:12px;width:12px;vertical-align:-2px;margin-right:6px}

/* ------------------------------------------------------------- the desk */
#desk{display:flex;gap:10px;align-items:stretch;justify-content:center;padding:8px 12px;
      background:linear-gradient(#0a1019,#060a10);overflow:hidden;min-width:0}
.sec{position:relative;border:1px solid var(--seam);background:linear-gradient(#0b121c,#070c13);
     padding:3px 6px 4px;display:flex;flex-direction:column}
.sec::after{content:"";position:absolute;left:-1px;top:-1px;width:10px;height:10px;
            border-left:2px solid var(--cyan);border-top:2px solid var(--cyan);opacity:.55}
/*  the heading is IN the flow, so a section is never narrower than its own
    title - absolutely placed, 'SOURCE -> REACTOR' ran into the next console */
.sec>h4{margin:0 2px 0;height:13px;font-size:9px;letter-spacing:.24em;gap:10px;
        color:#7fa3c4;font-weight:600;display:flex;justify-content:space-between;white-space:nowrap}
.sec>h4 .via{color:var(--dimmer);letter-spacing:.14em;cursor:pointer}
.sec>h4 .via:hover{color:var(--cyan)}
.sec>h4 .frz{color:var(--red);text-shadow:0 0 6px rgba(255,90,90,.8)}
/*  74, not 78: three rows of 78 plus the heading made 254 px inside a 248 px
    console, and the bottom row of every section sat half outside its frame */
.grid{display:grid;grid-template-rows:repeat(3,74px);grid-auto-columns:66px;gap:0 2px}

/* a cell */
.cell{display:flex;flex-direction:column;align-items:center;justify-content:flex-start;min-width:0}
.cell.w2{grid-column:span 2}
.cell .lab{font-size:9px;letter-spacing:.12em;color:var(--dim);white-space:nowrap;overflow:hidden;
           text-overflow:ellipsis;max-width:100%;margin-top:1px}
.cell .val{font-size:10px;color:var(--dimmer);height:12px;white-space:nowrap}
.cell.hot .val{color:var(--amber);text-shadow:0 0 6px rgba(255,183,77,.6)}
.cell.hot .lab{color:var(--ink)}

/* the knob */
.knob{cursor:ns-resize;touch-action:none;display:block}
.knob .trk{fill:none;stroke:#132131;stroke-width:3.2;stroke-linecap:round}
.knob .arc{fill:none;stroke:var(--cyan);stroke-width:3.2;stroke-linecap:round;
           filter:drop-shadow(0 0 3px rgba(79,195,247,.9))}
.knob .mod{fill:none;stroke:var(--amber);stroke-width:2;stroke-linecap:round;opacity:.9;
           filter:drop-shadow(0 0 3px rgba(255,183,77,.9))}
.knob .cap{fill:url(#capg);stroke:#223449;stroke-width:1}
.knob .dot{fill:var(--cyan2)}
.cell:hover .knob .arc{stroke:var(--cyan2)}

/* a choice: glowing buttons */
.opts{display:grid;gap:2px;width:100%;align-content:start;margin-top:2px}
.opts .hud{height:13px;padding:0 3px;font-size:8px;letter-spacing:.08em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}

/* a stepper, for the long lists */
.step{display:flex;width:100%;margin-top:14px;border:1px solid var(--seam2);background:#050910}
.step button{border:0;background:transparent;color:var(--cyan);font:11px var(--mono);cursor:pointer;padding:0 5px}
.step .nm{flex:1;text-align:center;font-size:9px;letter-spacing:.08em;color:var(--cyan2);cursor:pointer;
          padding:5px 0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.step .nm:hover{text-shadow:var(--glow)}

/* the HUD displays either side of the consoles: line art, no controls */
#desk .hudv{flex:1 1 0;min-width:0;max-width:440px;position:relative;
            border:1px solid rgba(79,195,247,.09);
            background:radial-gradient(ellipse at 50% 55%,rgba(79,195,247,.045),rgba(0,0,0,0) 70%)}
#desk .hudv.big{flex:3 1 0;max-width:none}
#desk .hudv canvas{position:absolute;inset:0;width:100%;height:100%;display:block}
/* a stepper squeezed into one cell, for the matrix */
.cell.cmp .step{margin-top:14px}
.cell.cmp .step .nm{font-size:8px;letter-spacing:0}
.cell.cmp .step button{padding:0 2px;font-size:10px}
/* a MODE the chosen circuit does not have */
.opts .hud.na{opacity:.32;text-decoration:line-through}
.cell.inert{opacity:.42}
/* the settings page */
.setp{padding:8px 12px;width:520px}
.setrow{display:flex;align-items:center;gap:12px;margin:12px 0;font-size:10px;letter-spacing:.16em;color:var(--dim)}
.setrow .nm{width:160px}
.setrow input[type=range]{flex:1;accent-color:#4fc3f7}
.setrow .v{width:48px;color:var(--cyan2);text-align:right}
.setp .note{font-size:9px;color:var(--dimmer);letter-spacing:.08em;line-height:1.5;margin-top:14px}
.setp .hud{height:22px;margin-top:6px}
/* the world-fx plate on OUTPUT */
.bwfxplate{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;width:170px;height:234px}
.bwfxplate .hud{height:44px;width:150px;font-size:11px;letter-spacing:.24em}
.bwfxplate .note{font-size:8px;color:var(--dimmer);letter-spacing:.1em;text-align:center}

/* ------------------------------------------------------------- the foot */
/*  keys hidden (the default): the scope has the whole foot.  KEYS on: the
    keyboard takes its own width on the left and the scope the rest.   */
#foot{display:grid;grid-template-columns:minmax(0,1fr);gap:12px;padding:6px 12px 8px;
      border-top:1px solid var(--seam);background:#05080e;min-height:0}
#foot>*{min-height:0;min-width:0}
body.keys #foot{grid-template-columns:auto minmax(0,1fr)}
#kbd{display:none;position:relative;height:48px;align-self:center}
body.keys #kbd{display:block}
/*  hollow squares for the white keys, dark squares between them for the
    black - the same HUD language as every other control on the ship     */
#kbd .k{position:absolute;width:22px;height:22px;cursor:pointer;box-sizing:border-box;
        transition:background .08s,box-shadow .08s,border-color .08s}
#kbd .w{top:25px;border:1px solid rgba(143,220,255,.55);background:rgba(79,195,247,.04)}
#kbd .w:hover{border-color:var(--cyan2);background:rgba(79,195,247,.12)}
#kbd .b{top:0;border:1px solid #1f3348;background:#0a1320}
#kbd .b:hover{border-color:#31557a;background:#102033}
#kbd .k.down{background:var(--cyan);border-color:var(--cyan2);box-shadow:var(--glow)}
#kbd .k i{position:absolute;left:0;right:0;bottom:1px;text-align:center;font:7px/1 var(--mono);
          font-style:normal;letter-spacing:.04em;color:rgba(143,220,255,.55);pointer-events:none}
#kbd .k.down i{color:#021018}
#tabs .keysBtn{margin-left:auto}
#scope{width:100%;height:100%;display:block;background:#02040a;border:1px solid var(--seam)}

/* ------------------------------------------------------------- overlays */
#veil{position:fixed;inset:0;background:rgba(2,4,8,.72);display:flex;align-items:center;justify-content:center;z-index:50}
.chart{background:linear-gradient(#0b121c,#060a10);border:1px solid var(--cyan);box-shadow:var(--glow);
       padding:16px 18px;max-width:92vw;max-height:84vh;overflow:auto}
.chart h3{margin:0 0 12px;font-size:10px;letter-spacing:.34em;color:var(--cyan2)}
.chart .cols{display:flex;gap:18px}
.chart .col h5{margin:0 0 6px;font-size:8px;letter-spacing:.26em;color:var(--dim)}
.chart .col .hud{display:block;width:170px;height:20px;margin:0 0 3px;text-align:left}
.chart .row{display:flex;gap:6px;margin-top:12px;align-items:center}
.chart input{flex:1;background:#050910;border:1px solid var(--seam2);color:var(--ink);font:11px var(--mono);padding:4px 6px}
.chart .ctop{display:flex;align-items:center;gap:18px;margin:0 0 12px}
.chart .ctop h3{margin:0}
.ctabs{display:flex;gap:4px}
.ctabs .hud{height:20px;padding:0 10px}
.unav{display:flex;gap:8px;align-items:center;margin:0 0 8px;font-size:9px;letter-spacing:.2em;color:var(--dim)}
.unav .hud{width:24px;height:20px;padding:0}
.ugrid{display:grid;grid-template-columns:repeat(6,150px);grid-template-rows:repeat(7,20px);grid-auto-flow:column;gap:3px 6px}
.ugrid .hud{height:20px;text-align:left;overflow:hidden;white-space:nowrap;text-overflow:ellipsis}
.ugrid .hud.empty{opacity:.35}
.ugrid .hud.empty:hover{opacity:.8}
.uloose{margin-top:10px;font-size:8px;letter-spacing:.2em;color:var(--dim)}
.uloose .hud{height:20px;margin:4px 4px 0 0;padding:0 8px}
.pchart .hud{color:#a9c6df}
.pchart .hud:hover{color:#ffffff}
.pchart .hud:disabled{opacity:.3;cursor:default}
.pchart .col h5,.pchart .unav,.pchart .uloose{color:#9dbbd4}
.pchart .ugrid .hud.empty{opacity:1;color:#56728c;border-style:dashed}
.pchart .ugrid .hud.empty:hover{color:#d4e6f7}
.pchart .ugrid .hud.tgt{border-color:var(--amber);border-style:solid;color:#fff3dd;background:rgba(255,183,77,.14);box-shadow:0 0 8px rgba(255,183,77,.35)}
.pchart .unav .hud{width:30px}
.pchart .ulab{color:#d4e6f7;letter-spacing:.2em;min-width:230px;text-align:center}
.pchart .uhint{margin-left:auto;color:#7f9ab3;letter-spacing:.14em}
.psave{display:flex;gap:8px;align-items:center;margin:0 0 12px}
.psave input{width:280px;flex:none;background:#050910;border:1px solid var(--seam2);color:#e9f8ff;font:12px var(--mono);padding:5px 7px;letter-spacing:.06em}
.psave .hud{height:26px;padding:0 14px}
.psave .hud.rep{border-color:var(--amber);color:#fff3dd;background:rgba(255,183,77,.16)}
.psave .hud.danger{border-color:#7a3131;color:#ffb3b3}
.ptgt{font-size:10px;letter-spacing:.14em;color:#9dbbd4;min-width:300px}
.ptgt.rep{color:var(--amber)}
.pnote{font-size:9px;color:#8aa6bf;margin-top:12px;letter-spacing:.05em;line-height:1.5;max-width:900px}
::-webkit-scrollbar{width:6px;height:6px}
::-webkit-scrollbar-track{background:transparent}
::-webkit-scrollbar-thumb{background:rgba(79,195,247,.35);border-radius:3px}
::-webkit-scrollbar-thumb:hover{background:rgba(79,195,247,.6)}
::-webkit-scrollbar-corner{background:transparent}
.list{display:grid;grid-template-columns:repeat(4,150px);gap:3px}
.list .hud{height:20px;text-align:left}
</style>

<div id="ship">
  <header id="hdr">
    <span class="plate">GRAVITY WELL</span>
    <span class="sub" id="buildid">BROKILD</span>
    <span class="spacer"></span>
    <div id="prog">
      <button id="progPrev" title="previous patch">&#9666;</button>
      <div id="progName" title="open the patch chart"><span class="g" id="progGroup">-</span><span class="n" id="progNm">-</span></div>
      <button id="progNext" title="next patch">&#9656;</button>
    </div>
    <button class="hud" id="saveBtn">SAVE</button>
    <span class="spacer"></span>
    <button class="hud warn" id="panicBtn" title="all notes off">PANIC</button>
  </header>

  <div id="bridge">
    <div class="pillar" id="pillL"><span class="tag">HORIZON A</span></div>
    <div id="screen">
      <canvas id="c"></canvas>
      <div class="glass"></div>
      <div class="br tl"></div><div class="br tr"></div><div class="br bl"></div><div class="br bb"></div>
      <div id="teleTop">VIEWSCREEN &middot; SCHWARZSCHILD EMBEDDING</div>
      <div id="tele">
        <span><span class="k">RS</span><b id="rrs">0.00</b></span>
        <span><span class="k">MASS</span><b id="rmass">0.00</b></span>
        <span><span class="k">DILATION</span><b id="rdil">-</b></span>
        <span id="rfrozen"></span>
        <span id="rfps"></span>
      </div>
      <!--  the demo's own controls, kept so not one line of its render loop
            had to change.  The ENGINE drives them; nothing here is a knob.  -->
      <input id="mass"  type="range" min="0" max="100" value="0"  hidden>
      <input id="hor"   type="range" min="0" max="100" value="50" hidden>
      <input id="pulse" type="checkbox" hidden>
    </div>
    <div class="pillar" id="pillR"><span class="tag">GRAVITY &middot; SPACETIME</span></div>
  </div>

  <nav id="tabs"><button class="hud wfx" id="wfxTab" data-bwfx-open title="Brokild World FX - the rack">WORLD FX</button></nav>
  <div id="desk"></div>

  <div id="foot">
    <div id="kbd"></div>
    <canvas id="scope"></canvas>
  </div>
</div>
<div id="veil" hidden></div>

<svg width="0" height="0" style="position:absolute">
  <defs><radialGradient id="capg" cx="40%" cy="35%" r="70%">
    <stop offset="0%" stop-color="#1c2b3c"/><stop offset="100%" stop-color="#070c13"/></radialGradient></defs>
</svg>

<script>
${wellJs}
</script>

<script>
/*  THE BRIDGE.  The page is a pure view: it holds no value the engine does
    not also hold, and every control is built from the table the processor
    sends.  The layout map was checked against Engine.h when this file was
    generated, so every parameter has a knob and exactly one home.        */
(function(){
"use strict";
const el = id => document.getElementById(id);
const KIND = ${JSON.stringify(KIND)};
const PILLAR_L = ${JSON.stringify(PILLAR_L)};
const PILLAR_R = ${JSON.stringify(PILLAR_R)};
const PAGES = ${JSON.stringify(PAGES)};

const NB = (() => {
  const send = m => { try { window.__JUCE__.backend.emitEvent("gw", m); } catch (e) {} };
  return {
    send,
    on (n, f) { try { window.__JUCE__.backend.addEventListener(n, f); } catch (e) {} }
  };
})();

const S = { byId: {}, v: {}, cells: {}, page: "helm", factory: [], prog: 0, user: [], frozen: [], mass: 0 };

//  ---- the modulators on the well are the REAL ones.  The demo placed them
//  at hard-coded radii, so the picture ignored every RADIUS knob; now each
//  light sits where its parameter puts it.  ENV3 is added: the engine has it.
if (typeof MODS !== "undefined" && !MODS.some(m => m.name === "ENV3"))
  MODS.splice(1, 0, { name:"ENV3", r:1.0, rate:0.45, hue:[1.00,0.48,0.30] });
const RADIUS_OF = { ENV2:"r_e2", ENV3:"r_e3", LFO1:"r_l1", LFO2:"r_l2", LFO3:"r_l3", RND:"r_rnd", CHIRP:"r_chirp" };
const HUE_OF = {};
if (typeof MODS !== "undefined") for (const m of MODS) if (RADIUS_OF[m.name])
  HUE_OF[RADIUS_OF[m.name]] = "rgb(" + m.hue.map(c => Math.round(c * 255)).join(",") + ")";
function syncRadii () {
  if (typeof MODS === "undefined") return;
  for (const m of MODS) { const id = RADIUS_OF[m.name]; if (id && id in S.v) m.r = S.v[id]; }
}

// ------------------------------------------------------------ formatting
function lawOf (p) { return (p.lawBase != null && p.lawSpan != null) ? [p.lawBase, p.lawSpan] : null; }
function fmt (p, v) {
  const lr = /^l([123])_rate$/.exec(p.id);
  if (lr && S.lfoDivs && S.lfoDivs.length && S.v["l" + lr[1] + "_sync"] > 0.5) {
    //  the engine's own rounding: round(v * (N - 1))
    const n = S.lfoDivs.length;
    return S.lfoDivs[Math.max(0, Math.min(n - 1, Math.round(v * (n - 1))))];
  }
  if (p.kind === KIND.CHOICE) { const c = (p.choices || "").split("|"); return c[Math.max(0, Math.min(c.length - 1, Math.round(v)))] || ""; }
  const law = lawOf(p);
  if (law) {
    const x = law[0] * Math.pow(law[1], Math.max(0, Math.min(1, v)));
    if (p.kind === KIND.SEC) return x < 1 ? Math.round(x * 1000) + " ms" : x.toFixed(2) + " s";
    if (x >= 1000) return (x / 1000).toFixed(2) + " kHz";
    if (x < 10) return x.toFixed(2) + " Hz";
    return Math.round(x) + " Hz";
  }
  //  TRANSPOSE reads in semitones: the whole throw is an octave each way
  if (p.id === "transpose") return (v > 0 ? "+" : "") + (v * 12).toFixed(1) + " st";
  switch (p.kind) {
    case KIND.PCT:   return Math.round(v * 100) + "%";
    case KIND.BIPOL: return Math.abs(p.hi) > 1.5 ? (v > 0 ? "+" : "") + Math.round(v) + " c"
                                                  : (v > 0 ? "+" : "") + Math.round(v * 100) + "%";
    case KIND.DB:    return (v > 0 ? "+" : "") + v.toFixed(1) + " dB";
    case KIND.INT:   return (p.lo < 0 && v > 0 ? "+" : "") + Math.round(v);
    default:         return v.toFixed(2);
  }
}

// ------------------------------------------------------------ the knob
const NS = "http://www.w3.org/2000/svg";
const A0 = Math.PI * 0.75, SWEEP = Math.PI * 1.5;          //  270 degrees, gap at the bottom
function arcPath (cx, cy, r, t0, t1) {
  const a0 = A0 + SWEEP * t0, a1 = A0 + SWEEP * t1;
  const x0 = cx + r * Math.cos(a0), y0 = cy + r * Math.sin(a0);
  const x1 = cx + r * Math.cos(a1), y1 = cy + r * Math.sin(a1);
  const large = (a1 - a0) > Math.PI ? 1 : 0;
  return "M" + x0.toFixed(2) + " " + y0.toFixed(2) + " A" + r + " " + r + " 0 " + large + " 1 " + x1.toFixed(2) + " " + y1.toFixed(2);
}
function mk (tag, attrs) { const e = document.createElementNS(NS, tag); for (const k in attrs) e.setAttribute(k, attrs[k]); return e; }

function makeKnob (p, size, label, withMod) {
  const cell = document.createElement("div"); cell.className = "cell"; cell.dataset.id = p.id; cell.dataset.kind = "knob";
  const svg = mk("svg", { class:"knob", width:size, height:size, viewBox:"0 0 " + size + " " + size });
  const c = size / 2, r = size / 2 - 4;
  svg.appendChild(mk("path", { class:"trk", d: arcPath(c, c, r, 0, 1) }));
  const arc = mk("path", { class:"arc" }); svg.appendChild(arc);
  let mod = null;
  if (withMod) { mod = mk("path", { class:"mod" }); svg.appendChild(mod); }
  svg.appendChild(mk("circle", { class:"cap", cx:c, cy:c, r: r - 6 }));
  const dot = mk("circle", { class:"dot", r: Math.max(1.6, size / 28) }); svg.appendChild(dot);
  const lab = document.createElement("div"); lab.className = "lab"; lab.textContent = label;
  const val = document.createElement("div"); val.className = "val";
  cell.appendChild(svg); cell.appendChild(lab); cell.appendChild(val);
  if (HUE_OF[p.id]) arc.style.stroke = HUE_OF[p.id];
  cell.title = p.name;

  const bip = p.kind === KIND.BIPOL || (p.lo < 0 && p.hi > 0);
  const paint = () => {
    const v = S.v[p.id];
    const t = Math.max(0, Math.min(1, (v - p.lo) / (p.hi - p.lo)));
    const z = bip ? (0 - p.lo) / (p.hi - p.lo) : 0;
    const t0 = Math.min(t, z), t1 = Math.max(t, z);
    arc.setAttribute("d", t1 - t0 < 0.004 ? "" : arcPath(c, c, r, t0, t1));
    const a = A0 + SWEEP * t, rd = r - 11;
    dot.setAttribute("cx", (c + rd * Math.cos(a)).toFixed(2));
    dot.setAttribute("cy", (c + rd * Math.sin(a)).toFixed(2));
    val.textContent = fmt(p, v);
  };
  const paintMod = x => { if (mod) mod.setAttribute("d", x < 0.004 ? "" : arcPath(c, c, r - 5, 0, Math.min(1, x))); };

  //  drag, fine drag, wheel, double-click home.  Document-level listeners
  //  for the drag, because pointer capture is not something to depend on.
  let y0 = 0, v0 = 0, dragging = false;
  const setV = v => {
    v = Math.max(p.lo, Math.min(p.hi, v));
    if (p.kind === KIND.INT) v = Math.round(v);
    set(p.id, v, true);
  };
  const move = e => {
    if (!dragging) return;
    const span = (p.hi - p.lo) * (e.shiftKey ? 0.1 : 1);
    setV(v0 + (y0 - e.clientY) / 190 * span);
  };
  const up = () => { dragging = false; cell.classList.remove("hot");
                     document.removeEventListener("pointermove", move); document.removeEventListener("pointerup", up); };
  svg.addEventListener("pointerdown", e => {
    e.preventDefault(); dragging = true; y0 = e.clientY; v0 = S.v[p.id]; cell.classList.add("hot");
    document.addEventListener("pointermove", move); document.addEventListener("pointerup", up);
  });
  svg.addEventListener("wheel", e => {
    e.preventDefault();
    const step = p.kind === KIND.INT ? 1 : (p.hi - p.lo) / 100 * (e.shiftKey ? 0.1 : 1);
    setV(S.v[p.id] - Math.sign(e.deltaY) * step);
  }, { passive:false });
  svg.addEventListener("dblclick", () => setV(p.def));
  cell.addEventListener("pointerenter", () => cell.classList.add("hot"));
  cell.addEventListener("pointerleave", () => { if (!dragging) cell.classList.remove("hot"); });

  return { cell, paint, paintMod, setV };
}

// ------------------------------------------------------------ a choice
function makeChoice (p, label) {
  const opts = (p.choices || "").split("|");
  const cell = document.createElement("div"); cell.className = "cell"; cell.dataset.id = p.id;
  const lab = document.createElement("div"); lab.className = "lab"; lab.textContent = label;
  cell.appendChild(lab);
  cell.title = p.name;

  if (opts.length > 8) {
    //  a long list: a stepper, and the whole list one click away
    cell.classList.add("w2"); cell.dataset.kind = "stepper";
    const st = document.createElement("div"); st.className = "step";
    const prev = document.createElement("button"); prev.textContent = "\\u25C2";
    const nm   = document.createElement("div"); nm.className = "nm";
    const next = document.createElement("button"); next.textContent = "\\u25B8";
    st.appendChild(prev); st.appendChild(nm); st.appendChild(next); cell.appendChild(st);
    const go = d => set(p.id, (Math.round(S.v[p.id]) + d + opts.length) % opts.length, true);
    prev.onclick = () => go(-1); next.onclick = () => go(1);
    nm.onclick = () => openList(p, opts);
    return { cell, paint: () => { nm.textContent = opts[Math.round(S.v[p.id])] || ""; } };
  }

  cell.dataset.kind = "buttons";
  const box = document.createElement("div"); box.className = "opts";
  if (opts.length > 4) { cell.classList.add("w2"); box.style.gridTemplateColumns = "1fr 1fr"; }
  const bs = opts.map((o, i) => {
    const b = document.createElement("button"); b.className = "hud"; b.textContent = o; b.title = o;
    //  a choice is a WHOLE index, sent once - never a position the plug-in
    //  has to round, which is what made the old slider fight the echo
    b.onclick = () => set(p.id, i, true);
    box.appendChild(b); return b;
  });
  cell.appendChild(box);
  const circOf = p.id === "fa_mode" ? "fa_circ" : p.id === "fb_mode" ? "fb_circ" : null;
  return { cell, paint: () => {
    const k = Math.round(S.v[p.id]);
    bs.forEach((b, i) => b.classList.toggle("on", i === k));
    if (circOf && S.modeMask) {
      const m = S.modeMask[Math.round(S.v[circOf])];
      bs.forEach((b, i) => {
        const na = m != null && !((m >> i) & 1);
        b.classList.toggle("na", na);
        b.title = na ? opts[i] + " - not on this circuit: it plays LP" : opts[i];
      });
    }
  } };
}

function makeControl (p, label, size) {
  return p.kind === KIND.CHOICE ? makeChoice(p, label) : makeKnob(p, size || 46, label, false);
}

// ------------------------------------------------------------ building
function register (id, ctl) { (S.cells[id] = S.cells[id] || []).push(ctl); ctl.paint(); }

function buildPillars () {
  const L = el("pillL"), R = el("pillR");
  L.querySelectorAll(".cell").forEach(n => n.remove());
  R.querySelectorAll(".cell").forEach(n => n.remove());
  //  the filter station: cutoff is the biggest knob on the ship
  PILLAR_L.items.forEach(([id, label], k) => {
    const p = S.byId[id]; if (!p) return;
    const ctl = p.kind === KIND.CHOICE ? makeChoice(p, label) : makeKnob(p, k === 0 ? 92 : 60, label, false);
    L.appendChild(ctl.cell); register(id, ctl);
  });
  for (const id of PILLAR_R) {
    const p = S.byId[id]; if (!p) continue;
    //  MASS carries a second arc: amber is where the well actually IS,
    //  cyan is where you set it.  The follower adds signal energy and
    //  velocity, so the two differ, and that difference is the instrument.
    const ctl = makeKnob(p, 56, p.name, id === "macro_mass");
    if (id === "macro_mass") ctl.cell.title = "MASS - deepens the well; past noon it is heard directly: the top sinks and the sub gathers weight";
    R.appendChild(ctl.cell); register(id, ctl);
  }
}

function cellsFor (item) {
  const id = Array.isArray(item) ? item[0] : item;
  const p = S.byId[id];
  if (!p || p.kind !== KIND.CHOICE) return 1;
  return (p.choices || "").split("|").length > 4 ? 2 : 1;
}

function buildTabs () {
  const nav = el("tabs"), wfx = el("wfxTab"); nav.innerHTML = "";
  for (const pg of PAGES) {
    if (pg.key === "settings" && wfx) nav.appendChild(wfx);
    const b = document.createElement("button"); b.className = "hud"; b.textContent = pg.name; b.dataset.page = pg.key;
    b.onclick = () => showPage(pg.key);
    nav.appendChild(b);
  }
  const k = document.createElement("button"); k.className = "hud keysBtn"; k.id = "keysBtn";
  k.textContent = "KEYS"; k.title = "show the keyboard";
  k.onclick = () => showKeys(!document.body.classList.contains("keys"));
  nav.appendChild(k);
  let on = false; try { on = localStorage.getItem("gw.keys") === "1"; } catch (e) {}
  showKeys(on);
}

function showKeys (on) {
  document.body.classList.toggle("keys", on);
  const b = el("keysBtn"); if (b) b.classList.toggle("on", on);
  try { localStorage.setItem("gw.keys", on ? "1" : "0"); } catch (e) {}
  //  a key held while the keyboard vanishes must not hang
  if (!on) document.querySelectorAll("#kbd .k.down").forEach(d => d.dispatchEvent(new Event("pointerup")));
  requestAnimationFrame(drawScope);
}

function showPage (key) {
  S.page = key;
  try { localStorage.setItem("gw.page", key); } catch (e) {}
  for (const b of el("tabs").children) b.classList.toggle("on", b.dataset.page === key);
  const desk = el("desk"); desk.innerHTML = "";
  //  the knobs of the previous page are gone; forget them, keep the pillars
  for (const id in S.cells) S.cells[id] = S.cells[id].filter(c => c.cell.isConnected);
  const pg = PAGES.find(p => p.key === key);
  HUD.list = [];
  if (pg.hud && pg.hud[0]) desk.appendChild(hudBox(pg.hud[0], pg.hudBig));
  for (const sec of pg.sections) {
    const box = document.createElement("div"); box.className = "sec"; box.dataset.sec = sec.name;
    const h = document.createElement("h4");
    const t = document.createElement("span"); t.textContent = sec.name; h.appendChild(t);
    if (sec.via) {
      const v = document.createElement("span"); v.className = "via"; v.textContent = "\\u2192 " + PAGES.find(p => p.key === sec.via).name;
      v.onclick = () => showPage(sec.via); h.appendChild(v);
    }
    if (sec.mod != null) { const f = document.createElement("span"); f.className = "frz"; f.dataset.mod = sec.mod; h.appendChild(f); }
    box.appendChild(h);
    if (sec.settings) { box.appendChild(buildSettings()); desk.appendChild(box); continue; }
    const grid = document.createElement("div"); grid.className = "grid";
    //  columns: enough for the cells in three rows, and never narrower than
    //  the widest control, or a two-wide choice would have nowhere to go
    let cells = 0, widest = 1;
    const isStepper = it => { const p = S.byId[Array.isArray(it) ? it[0] : it]; return p && p.kind === KIND.CHOICE && (p.choices || "").split("|").length > 8; };
    for (const it of sec.items) { const n = (sec.compact && isStepper(it)) ? 1 : cellsFor(it); cells += n; widest = Math.max(widest, n); }
    const cols = sec.cols || Math.max(widest, Math.ceil(cells / 3));
    grid.style.gridTemplateColumns = "repeat(" + cols + ", 66px)";
    grid.style.gridAutoFlow = "row dense";
    for (const it of sec.items) {
      const id = Array.isArray(it) ? it[0] : it;
      const p = S.byId[id]; if (!p) continue;
      const ctl = makeControl(p, Array.isArray(it) ? it[1] : p.name);
      if (sec.compact && ctl.cell.dataset.kind === "stepper") { ctl.cell.classList.remove("w2"); ctl.cell.classList.add("cmp"); }
      grid.appendChild(ctl.cell);
      register(id, ctl);
    }
    box.appendChild(grid);
    desk.appendChild(box);
  }
  if (pg.hud && pg.hud[1]) desk.appendChild(hudBox(pg.hud[1], false));
  paintFrozen();
  setTimeout(hudDraw, 0);
}

function set (id, v, fromUi) {
  S.v[id] = v;
  for (const c of (S.cells[id] || [])) c.paint();
  if (id.startsWith("r_")) syncRadii();
  if (id === "macro_horiz") el("hor").value = String(Math.round(v * 100));
  //  a rate reads as a division once TEMPO is on; a MODE button dims when
  //  the circuit has no such mode - so both repaint when the other moves
  const DEP = { l1_sync:"l1_rate", l2_sync:"l2_rate", l3_sync:"l3_rate", fa_circ:"fa_mode", fb_circ:"fb_mode" };
  if (DEP[id]) for (const c of (S.cells[DEP[id]] || [])) c.paint();
  hudSoon();
  if (fromUi) NB.send({ k: "p", id: id, v: v });
}

function paintFrozen () {
  document.querySelectorAll(".frz").forEach(f => { f.textContent = S.frozen[+f.dataset.mod] ? "FROZEN" : ""; });
}

// ------------------------------------------------------------ overlays
function veil (build) {
  const v = el("veil"); v.innerHTML = ""; v.hidden = false;
  const box = document.createElement("div"); box.className = "chart";
  build(box); v.appendChild(box);
  v.onclick = e => { if (e.target === v) v.hidden = true; };
}
function openList (p, opts) {
  veil(box => {
    const h = document.createElement("h3"); h.textContent = p.name; box.appendChild(h);
    const l = document.createElement("div"); l.className = "list";
    opts.forEach((o, i) => {
      const b = document.createElement("button"); b.className = "hud" + (i === Math.round(S.v[p.id]) ? " on" : "");
      b.textContent = o; b.onclick = () => { set(p.id, i, true); el("veil").hidden = true; };
      l.appendChild(b);
    });
    box.appendChild(l);
  });
}
//  THE PATCH CHART - loads, saves and clears in one place.
//  FACTORY is the bank (00-31, read only).  YOURS and SAVE show the numbered
//  slots 032-199 as a window of 6 columns x 7 that slides a column or a page
//  at a time, so every slot can be reached AND seen before anything is written.
const U_FIRST = 32, U_LAST = 199, U_ROWS = 7, U_COLS = 6, U_PER = U_ROWS * U_COLS;
const U_MAXSTART = U_LAST - U_PER + 1;          //  158: the last window ends exactly on 199
const CH = { mode: "yours", start: U_FIRST, target: -1, name: "", confirmClear: false, focus: false };
function uClampStart (s) {
  s = U_FIRST + Math.round((s - U_FIRST) / U_ROWS) * U_ROWS;
  return Math.max(U_FIRST, Math.min(U_MAXSTART, s));
}
function uAt (s) { return S.user.find(x => x.slot === s); }
function uFirstFree () { for (let s = U_FIRST; s <= U_LAST; ++s) if (!uAt(s)) return s; return -1; }
function uShow (s) { if (s < CH.start || s >= CH.start + U_PER) CH.start = uClampStart(s - U_ROWS * 2); }
function stripLabel (s) {
  s = (s || "").trim(); const i = s.indexOf("  ");
  if (i > 0 && /^[0-9-]+$/.test(s.slice(0, i))) s = s.slice(i + 2);
  return s.trim();
}
function openChart (mode) {
  if (mode !== "factory" && mode !== "yours" && mode !== "save") mode = S.curSlot >= U_FIRST ? "yours" : "factory";
  CH.mode = mode; CH.confirmClear = false;
  if (mode === "save") {
    CH.name = S.curName || "MY PATCH";
    //  re-saving the patch you are on is the commonest save; otherwise the first free slot
    CH.target = S.curSlot >= U_FIRST ? S.curSlot : uFirstFree();
    if (CH.target < 0) CH.target = U_LAST;
    uShow(CH.target); CH.focus = true;
  } else if (mode === "yours" && S.curSlot >= U_FIRST) uShow(S.curSlot);
  renderChart();
}
function openSave (want) {
  openChart("save");
  if (typeof want === "number" && want >= U_FIRST && want <= U_LAST) { CH.target = want; uShow(want); renderChart(); }
}
function chartSave () {
  const n = (CH.name || "").trim();
  if (!n || CH.target < U_FIRST || CH.target > U_LAST) return;
  NB.send({ k: "patchSave", name: n, slot: CH.target });
  S.curSlot = CH.target; S.curUser = null;
  showProg("YOURS", slotLabel(CH.target) + "  " + n);
  el("veil").hidden = true;
}
function chartClear () {
  const s = CH.target;
  NB.send({ k: "patchDelete", slot: s });
  S.user = S.user.filter(x => x.slot !== s);
  if (S.curSlot === s) { S.curSlot = -1; S.curUser = null; showProg("PATCH", S.curName || "-"); }
  CH.confirmClear = false; renderChart();
}
function renderChart () {
  veil(box => {
    box.classList.add("pchart");
    const top = document.createElement("div"); top.className = "ctop";
    const h = document.createElement("h3"); h.textContent = "PATCH CHART"; top.appendChild(h);
    const tabs = document.createElement("div"); tabs.className = "ctabs";
    [["factory", "FACTORY 00-31"], ["yours", "YOURS 032-199"], ["save", "SAVE"]].forEach(t => {
      const b = document.createElement("button"); b.className = "hud" + (t[0] === CH.mode ? " on" : "");
      b.textContent = t[1]; b.dataset.tab = t[0];
      b.onclick = () => t[0] === "save" ? openChart("save") : (CH.mode = t[0], CH.confirmClear = false, renderChart());
      tabs.appendChild(b);
    });
    top.appendChild(tabs); box.appendChild(top);

    if (CH.mode === "factory") {
      const cols = document.createElement("div"); cols.className = "cols";
      const groups = [];
      for (const f of S.factory) { let g = groups.find(x => x.name === f.group); if (!g) groups.push(g = { name: f.group, items: [] }); g.items.push(f); }
      for (const g of groups) {
        const c = document.createElement("div"); c.className = "col";
        const t = document.createElement("h5"); t.textContent = g.name; c.appendChild(t);
        for (const f of g.items) {
          const b = document.createElement("button"); b.className = "hud" + (f.i === S.prog && S.curSlot < U_FIRST ? " on" : "");
          b.textContent = String(f.i).padStart(2, "0") + "  " + f.name;
          b.onclick = () => { loadFactory(f.i); el("veil").hidden = true; };
          c.appendChild(b);
        }
        cols.appendChild(c);
      }
      box.appendChild(cols);
      const note = document.createElement("div"); note.className = "pnote";
      note.textContent = "The factory bank stays as it is. To keep your own version of a sound, open SAVE and give it a slot of its own.";
      box.appendChild(note);
      return;
    }

    //  SAVE: the name, where it will go, and what is there now
    if (CH.mode === "save") {
      const bar = document.createElement("div"); bar.className = "psave";
      const inp = document.createElement("input"); inp.value = CH.name; inp.maxLength = 40; inp.placeholder = "NAME";
      inp.oninput = () => { CH.name = inp.value; go.disabled = !inp.value.trim(); };
      inp.onkeydown = e => { if (e.key === "Enter") chartSave(); if (e.key !== "Escape") e.stopPropagation(); };
      const t = uAt(CH.target);
      const info = document.createElement("span"); info.className = "ptgt" + (t ? " rep" : "");
      info.textContent = "SLOT " + slotLabel(CH.target) + "  " + (t ? "REPLACES " + t.name : "FREE");
      const go = document.createElement("button"); go.className = "hud on" + (t ? " rep" : "");
      go.textContent = t ? "REPLACE" : "SAVE"; go.dataset.act = "save"; go.onclick = chartSave;
      go.disabled = !CH.name.trim();
      bar.appendChild(inp); bar.appendChild(info); bar.appendChild(go);
      if (t) {
        const clr = document.createElement("button"); clr.className = "hud danger"; clr.dataset.act = "clear";
        clr.textContent = CH.confirmClear ? "CLICK AGAIN TO CLEAR " + slotLabel(CH.target) : "CLEAR SLOT";
        clr.onclick = () => { if (CH.confirmClear) chartClear(); else { CH.confirmClear = true; renderChart(); } };
        bar.appendChild(clr);
      }
      box.appendChild(bar);
      if (CH.focus) { CH.focus = false; setTimeout(() => { inp.focus(); inp.select(); }, 0); }
    }

    //  the window onto the slots, and the arrows that slide it
    const a = CH.start, z = Math.min(U_LAST, a + U_PER - 1);
    const nav = document.createElement("div"); nav.className = "unav";
    const mk = (txt, d, tip) => { const b = document.createElement("button"); b.className = "hud"; b.textContent = txt; b.title = tip;
      b.dataset.shift = d; b.disabled = (d < 0 && a <= U_FIRST) || (d > 0 && a >= U_MAXSTART);
      b.onclick = () => { CH.start = uClampStart(CH.start + d); CH.confirmClear = false; renderChart(); }; return b; };
    nav.appendChild(mk("◂◂", -U_PER, "a page lower (PageUp)"));
    nav.appendChild(mk("◂", -U_ROWS, "a column lower (Left)"));
    const lab = document.createElement("span"); lab.className = "ulab";
    const saved = S.user.filter(x => x.slot >= U_FIRST && x.slot <= U_LAST).length;
    lab.textContent = slotLabel(a) + " - " + slotLabel(z) + "      " + saved + " OF 168 SLOTS USED";
    nav.appendChild(lab);
    nav.appendChild(mk("▸", U_ROWS, "a column higher (Right)"));
    nav.appendChild(mk("▸▸", U_PER, "a page higher (PageDown)"));
    const hint = document.createElement("span"); hint.className = "uhint";
    hint.textContent = CH.mode === "save" ? "CLICK A SLOT TO AIM AT IT  ·  DOUBLE-CLICK AN EMPTY ONE TO SAVE THERE" : "CLICK TO LOAD  ·  CLICK AN EMPTY SLOT TO SAVE THERE";
    nav.appendChild(hint);
    box.appendChild(nav);

    const grid = document.createElement("div"); grid.className = "ugrid";
    for (let sl = a; sl <= z; ++sl) {
      const x = uAt(sl);
      const b = document.createElement("button");
      const cur = x && sl === S.curSlot, tgt = CH.mode === "save" && sl === CH.target;
      b.className = "hud" + (x ? "" : " empty") + (cur && !tgt ? " on" : "") + (tgt ? " tgt" : "");
      b.dataset.slot = sl;
      b.textContent = slotLabel(sl) + "  " + (x ? x.name : "—");
      b.title = x ? x.name : "empty";
      if (CH.mode === "save") {
        b.onclick = () => { CH.target = sl; CH.confirmClear = false; renderChart(); };
        if (!x) b.ondblclick = () => { CH.target = sl; chartSave(); };
      } else {
        b.onclick = x ? () => { loadUser(x); el("veil").hidden = true; } : () => openSave(sl);
      }
      grid.appendChild(b);
    }
    box.appendChild(grid);

    //  patches saved before slots existed: still loadable until re-saved into one
    const loose = userSorted().filter(x => !(x.slot >= U_FIRST && x.slot <= U_LAST));
    if (loose.length && CH.mode === "yours") {
      const l = document.createElement("div"); l.className = "uloose";
      l.textContent = "UNNUMBERED - LOAD ONE AND SAVE IT TO GIVE IT A SLOT";
      const row = document.createElement("div");
      for (const x of loose) {
        const b = document.createElement("button"); b.className = "hud"; b.textContent = x.name;
        b.onclick = () => { loadUser(x); el("veil").hidden = true; };
        row.appendChild(b);
      }
      l.appendChild(row); box.appendChild(l);
    }
    if (CH.mode === "save") {
      const note = document.createElement("div"); note.className = "pnote";
      note.textContent = "Each slot is a file in Documents" + String.fromCharCode(92) + "Brokild patches" + String.fromCharCode(92) + "Gravity Well, shared by every project. To rename, save to the same slot. To move, save to the new slot and clear the old one. A cleared patch is kept in the Deleted folder there.";
      box.appendChild(note);
    }
  });
}
//  the slot window follows the keys too, but never steals them from the name field
document.addEventListener("keydown", e => {
  if (el("veil").hidden || !el("veil").querySelector(".ugrid")) return;
  if (e.target && e.target.tagName === "INPUT") return;
  const d = e.key === "ArrowLeft" ? -U_ROWS : e.key === "ArrowRight" ? U_ROWS : e.key === "PageUp" ? -U_PER : e.key === "PageDown" ? U_PER : 0;
  if (!d) return;
  e.preventDefault(); CH.start = uClampStart(CH.start + d); CH.confirmClear = false; renderChart();
});
function showProg (group, name) { el("progGroup").textContent = group; el("progNm").textContent = name; S.curName = stripLabel(name); }
function slotLabel (s) { return s >= 0 ? String(s).padStart(3, "0") : "--"; }
function userSorted () { return S.user.slice().sort((a, b) => (a.slot < 0 ? 1e4 : a.slot) - (b.slot < 0 ? 1e4 : b.slot) || a.name.localeCompare(b.name)); }
function loadUser (x) { NB.send({ k:"patchLoad", path:x.path }); S.curSlot = x.slot; S.curUser = x.path; showProg("YOURS", slotLabel(x.slot) + "  " + x.name); }
//  the arrows walk 00..31 and then your slots in order, and wrap
function stepProg (d) {
  const list = S.factory.map(f => ({ f: f })).concat(userSorted().map(x => ({ u: x })));
  if (!list.length) return;
  let at = S.curUser ? list.findIndex(e => e.u && e.u.path === S.curUser) : S.prog;
  if (at < 0) at = 0;
  const e = list[(at + d + list.length) % list.length];
  if (e.u) loadUser(e.u); else loadFactory(e.f.i);
}
function loadFactory (i) {
  const n = S.factory.length; if (!n) return;
  i = (i + n) % n; S.prog = i; S.curSlot = -1; S.curUser = null;
  NB.send({ k:"factory", i:i });
  const f = S.factory[i]; showProg(f.group, String(i).padStart(2, "0") + "  " + f.name);
}
el("progPrev").onclick = () => stepProg(-1);
el("progNext").onclick = () => stepProg(+1);
el("progName").onclick = () => openChart();
el("saveBtn").onclick = () => openSave();
el("panicBtn").onclick = () => NB.send({ k:"panic" });
document.addEventListener("keydown", e => { if (e.key === "Escape") el("veil").hidden = true; });

// ------------------------------------------------------------ the HUD displays
//  Line art either side of the consoles.  Everything drawn is DERIVED: from
//  the parameters, through the same formulas the engine runs, or from the
//  live values the engine streams.  A display can never show a sound that is
//  not there - the rule every panel in the fleet has paid for once.
const HUD = { list: [], gain: 1, tr: {}, t: 0, seed: 12345, pk: [0, 0], timer: 0 };
const C1 = "143,220,255", C0 = "79,195,247", AMB = "255,183,77", NEG = "190,150,255";
function hudBox (kind, big) {
  const box = document.createElement("div"); box.className = "hudv" + (big ? " big" : ""); box.dataset.hud = kind;
  const cv = document.createElement("canvas"); box.appendChild(cv);
  HUD.list.push({ kind: kind, cv: cv });
  return box;
}
function hudSoon () { if (!HUD.timer) HUD.timer = setTimeout(() => { HUD.timer = 0; hudDraw(); }, 30); }
function hudPrep (cv) {
  const w = cv.clientWidth, h = cv.clientHeight;
  if (w < 90 || h < 60) return null;
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  const W = Math.round(w * dpr), H = Math.round(h * dpr);
  if (cv.width !== W || cv.height !== H) { cv.width = W; cv.height = H; }
  const g = cv.getContext("2d");
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, w, h);
  return { g: g, w: w, h: h };
}
function hstroke (g, rgb, a, lw, blur) {
  const k = HUD.gain;
  g.strokeStyle = "rgba(" + rgb + "," + Math.min(1, a * k).toFixed(3) + ")";
  g.lineWidth = lw * (0.6 + 0.4 * k);
  g.shadowColor = "rgba(" + rgb + "," + Math.min(1, 0.85 * k).toFixed(3) + ")";
  g.shadowBlur = (blur == null ? 6 : blur) * k;
}
function htext (g, str, x, y, rgb, a, align, size) {
  g.shadowBlur = 0;
  g.font = (size || 9) + "px ui-monospace, Consolas, monospace";
  g.textAlign = align || "left"; g.textBaseline = "alphabetic";
  g.fillStyle = "rgba(" + (rgb || C1) + "," + Math.min(1, (a == null ? 0.6 : a) * HUD.gain).toFixed(3) + ")";
  g.fillText(str, x, y);
}
function hline (g, x0, y0, x1, y1, rgb, a) { hstroke(g, rgb, a, 1, 0); g.beginPath(); g.moveTo(x0, y0); g.lineTo(x1, y1); g.stroke(); }
function hframe (d, title) {
  const g = d.g, L = 8;
  hstroke(g, C0, 0.45, 1, 0);
  g.beginPath();
  g.moveTo(1, L); g.lineTo(1, 1); g.lineTo(L, 1);
  g.moveTo(d.w - L, 1); g.lineTo(d.w - 1, 1); g.lineTo(d.w - 1, L);
  g.moveTo(1, d.h - L); g.lineTo(1, d.h - 1); g.lineTo(L, d.h - 1);
  g.moveTo(d.w - L, d.h - 1); g.lineTo(d.w - 1, d.h - 1); g.lineTo(d.w - 1, d.h - L);
  g.stroke();
  if (title) htext(g, title, 10, 15, C1, 0.5, "left", 8);
}
//  f(t) in -1..1 over t in 0..1, drawn about the midline y0
function plotFn (g, x0, y0, w, h, n, f, rgb, a, lw) {
  hstroke(g, rgb, a, lw || 1.3);
  g.beginPath();
  for (let i = 0; i <= n; ++i) {
    const t = i / n, y = y0 - Math.max(-1.25, Math.min(1.25, f(t))) * h;
    if (i) g.lineTo(x0 + t * w, y); else g.moveTo(x0 + t * w, y);
  }
  g.stroke();
}
function glowDot (g, x, y, r, rgb, a) {
  g.shadowBlur = 10 * HUD.gain; g.shadowColor = "rgba(" + rgb + ",0.9)";
  g.fillStyle = "rgba(" + rgb + "," + Math.min(1, a * HUD.gain).toFixed(3) + ")";
  g.beginPath(); g.arc(x, y, r, 0, Math.PI * 2); g.fill(); g.shadowBlur = 0;
}
const V = id => (S.v[id] != null ? S.v[id] : (S.byId[id] ? S.byId[id].def : 0));
function lawVal (id) { const p = S.byId[id], l = p && lawOf(p); return l ? l[0] * Math.pow(l[1], Math.max(0, Math.min(1, V(id)))) : V(id); }
function choiceName (id) { const p = S.byId[id]; if (!p) return ""; const c = (p.choices || "").split("|"); return c[Math.round(V(id))] || ""; }
function hueRgb (rid, fallback) { const h = HUE_OF[rid]; return h ? h.slice(4, -1) : fallback; }
function fmtId (id) { const p = S.byId[id]; return p ? fmt(p, V(id)) : ""; }

// ---- REACTOR: one cycle of each oscillator, the sub, the noise
//  the recipes WaveTables::init() builds, so the picture IS the table
function wtAmp (t, n) {
  switch (t) {
    case 0: return 1 / n;
    case 1: return (n % 2) ? 1 / n : 0;
    case 2: return Math.exp(-Math.pow((n - 5) / 3.2, 2)) + 0.6 * Math.exp(-Math.pow((n - 13) / 4, 2));
    case 3: return (n % 2) ? 1.2 / (n * n) : 0.9 / n;
    case 4: return Math.exp(-n * 0.09) * (1 + 0.5 * Math.sin(n * 1.7));
    case 5: return 1 / (n * (1 + 0.35 * Math.sin(n * 0.8)));
    case 6: return (n <= 3 ? 1 / n : 0.8 / Math.sqrt(n)) * ((n % 3) ? 1 : 0.4);
    default: return 1 / n * (1 + 0.7 * Math.sin(n * 2.399));
  }
}
function oscCycle (x) {
  const wt = V(x + "_engine") > 0.5, shape = Math.max(0, Math.min(1, V(x + "_shape")));
  const width = Math.max(0.03, Math.min(0.97, V(x + "_width")));
  const N = 180, out = [];
  for (let i = 0; i <= N; ++i) {
    const ph = i / N; let y = 0;
    if (wt) {
      //  Osc A reads the table at the note's own harmonic limit; 32 is a bass note's view
      const H = 32, tilt = 0.5 + 2.5 * shape, tab = Math.round(V(x + "_table"));
      for (let h = 1; h <= H; ++h) y += wtAmp(tab, h) * Math.exp(-(h - 1) / (tilt * H)) * Math.sin(2 * Math.PI * ph * h);
    } else {
      //  Osc::analogue - saw, square, triangle, sine, morphed in that order
      const tab = [2 * ph - 1, ph < width ? 1 : -1, 2 * Math.abs(2 * ph - 1) - 1, Math.sin(2 * Math.PI * ph)];
      const xx = Math.min(shape, 0.9999) * 3, k = Math.floor(xx), f = xx - k;
      y = tab[k] + (tab[Math.min(3, k + 1)] - tab[k]) * f;
    }
    out.push(y);
  }
  let pk = 1e-6; for (const y of out) pk = Math.max(pk, Math.abs(y));
  return out.map(y => y / pk);
}
function drawOsc (d) {
  const g = d.g; hframe(d, "REACTOR · ONE CYCLE");
  const top = 24, rh = (d.h - top - 6) / 2;
  [["a", C1], ["b", C0]].forEach((row, k) => {
    const x = row[0], rgb = row[1], y0 = top + rh * k + rh / 2 + 4, lvl = V(x + "_level"), live = lvl > 0.001;
    hline(g, 12, y0, d.w - 12, y0, C0, 0.13);
    const cyc = oscCycle(x);
    plotFn(g, 16, y0, d.w - 32, rh * 0.34, cyc.length - 1, t => cyc[Math.round(t * (cyc.length - 1))], rgb, live ? 0.95 : 0.2, 1.5);
    const eng = V(x + "_engine") > 0.5 ? choiceName(x + "_table") : "ANALOGUE";
    htext(g, x.toUpperCase() + "  " + eng + "  " + (live ? Math.round(lvl * 100) + "%" : "SILENT"), 14, y0 - rh / 2 + 6, rgb, live ? 0.75 : 0.35);
  });
}
function drawSubNoise (d) {
  const g = d.g; hframe(d, "SUB · NOISE");
  const top = 24, rh = (d.h - top - 6) / 2;
  //  one cycle of the sub, against oscillator A's fundamental for scale:
  //  SUB OCTAVE counts octaves below A, so A runs 2 or 4 times across it
  const oct = Math.max(1, Math.round(V("sub_oct"))), sq = V("sub_shape") > 0.5, lvl = V("sub_level");
  let y0 = top + rh / 2 + 4;
  hline(g, 12, y0, d.w - 12, y0, C0, 0.13);
  plotFn(g, 16, y0, d.w - 32, rh * 0.3, 240, t => Math.sin(2 * Math.PI * t * Math.pow(2, oct)), C0, 0.2, 1);
  plotFn(g, 16, y0, d.w - 32, rh * 0.36 * (0.35 + 0.65 * lvl), 240,
         t => sq ? (t < 0.5 ? 1 : -1) : Math.sin(2 * Math.PI * t), C1, lvl > 0.001 ? 0.95 : 0.2, 1.6);
  htext(g, "SUB  " + oct + " OCT DOWN  " + (sq ? "SQUARE" : "SINE") + "  " +
        (lvl > 0.001 ? Math.round(lvl * 100) + "%" : "OFF"), 14, y0 - rh / 2 + 6, C1, lvl > 0.001 ? 0.75 : 0.35);
  //  noise: the engine's own colouring, applied to a fresh stretch each frame
  const col = V("noise_col"), nl = V("noise_level");
  y0 = top + rh * 1.5 + 4;
  hline(g, 12, y0, d.w - 12, y0, C0, 0.13);
  let r = HUD.seed = (HUD.seed * 1103515245 + 12345) >>> 0;
  const rnd = () => { r = (Math.imul(r, 1664525) + 1013904223) >>> 0; return r / 4294967296 * 2 - 1; };
  const N = 260, pts = []; let lp = 0, hp = 0, pk = 1e-6;
  for (let i = 0; i < N + 40; ++i) {
    let z = rnd();
    if (col < 0.5) { lp += (0.01 + 1.98 * col) * (z - lp); z = lp * (1 + (0.5 - col) * 4); }
    else if (col > 0.5) { hp += (1 - (col - 0.5) * 1.9) * 0.5 * (z - hp); z = z - hp; }
    if (i >= 40) { pts.push(z); pk = Math.max(pk, Math.abs(z)); }
  }
  plotFn(g, 16, y0, d.w - 32, rh * 0.34 * (nl > 0.001 ? 0.35 + 0.65 * nl : 0.3), N - 1, t => pts[Math.round(t * (N - 1))] / pk,
         C1, nl > 0.001 ? 0.8 : 0.16, 1);
  htext(g, "NOISE  " + (col < 0.4 ? "RUMBLE" : col > 0.6 ? "HISS" : "WHITE") + "  " + (nl > 0.001 ? Math.round(nl * 100) + "%" : "OFF"),
        14, y0 - rh / 2 + 6, C1, nl > 0.001 ? 0.75 : 0.35);
}

// ---- HORIZONS: what each circuit does to the spectrum, and how they are wired
//  Characteristic curves, not a measurement: the ladder is four poles, the
//  diode three, GROWL/SCREAM/SVF two, COMB a comb.  A MODE the circuit does
//  not have is drawn as the LP it actually plays.
function fResp (circ, mode, fc, res, f) {
  const x = f / fc;
  const mask = S.modeMask && S.modeMask[circ] != null ? S.modeMask[circ] : 15;
  if (!((mask >> mode) & 1)) mode = 0;
  const mul = (a, b) => [a[0] * b[0] - a[1] * b[1], a[0] * b[1] + a[1] * b[0]];
  const mag = a => Math.hypot(a[0], a[1]);
  const onePlus = [1, x];
  const pw = n => { let q = [1, 0]; for (let i = 0; i < n; ++i) q = mul(q, onePlus); return q; };
  switch (circ) {
    case 0: { const k = res * 3.9, q = pw(4); return (1 + 0.5 * k) / mag([q[0] + k, q[1]]); }
    case 4: { const k = res * 7.5, q = pw(3); return (1 + 0.5 * k) / mag([q[0] + k, q[1]]); }
    case 5: {
      const gk = 0.35 + res * 0.6, w = 2 * Math.PI * x, sg = mode === 3 ? 1 : -1;
      return (1 - gk) / Math.hypot(1 + sg * gk * Math.cos(w), gk * Math.sin(w));
    }
    case 6: {
      let y = 0; const Q = 4 + res * 10;
      [[1, 1], [2.3, 0.6], [3.4, 0.4]].forEach(m => { const xx = x / m[0]; y += m[1] * (xx / Q) / Math.hypot(1 - xx * xx, xx / Q); });
      return y;
    }
    default: {
      const Q = 0.55 + res * (circ === 2 ? 14 : circ === 1 ? 6 : 10);
      const D = Math.hypot(1 - x * x, x / Q);
      if (mode === 2) return x * x / D;
      if (mode === 1) return (x / Q) / D;
      if (mode === 3) return Math.abs(1 - x * x) / D;
      return 1 / D;
    }
  }
}
function drawFilter (d) {
  const g = d.g; hframe(d, "HORIZONS · RESPONSE");
  //  a narrow box (HELM) gets the legend on a line of its own, under the title
  const narrow = d.w < 330;
  const L = 14, R = d.w - 10, T = narrow ? 36 : 26, B = d.h - 18, fmin = 20, fmax = 20000, dbT = 18, dbB = -36;
  const X = f => L + (Math.log(f / fmin) / Math.log(fmax / fmin)) * (R - L);
  const Y = db => T + (dbT - Math.max(dbB, Math.min(dbT, db))) / (dbT - dbB) * (B - T);
  [100, 1000, 10000].forEach((f, i) => { hline(g, X(f), T, X(f), B, C0, 0.1); htext(g, ["100", "1k", "10k"][i], X(f) + 2, B + 11, C0, 0.35, "left", 8); });
  [0, -24].forEach(db => hline(g, L, Y(db), R, Y(db), C0, db === 0 ? 0.16 : 0.08));
  const A = { circ: Math.round(V("fa_circ")), mode: Math.round(V("fa_mode")), fc: lawVal("fa_cut"), res: V("fa_res") };
  const Bf = { circ: Math.round(V("fb_circ")), mode: Math.round(V("fb_mode")), fc: lawVal("fb_cut"), res: V("fb_res") };
  const route = Math.round(V("route")), bl = V("blend"), xf = lawVal("split");
  const N = 220, fs = [], ha = [], hb = [], ht = [];
  for (let i = 0; i <= N; ++i) {
    const f = fmin * Math.pow(fmax / fmin, i / N);
    const a = fResp(A.circ, A.mode, A.fc, A.res, f), b = fResp(Bf.circ, Bf.mode, Bf.fc, Bf.res, f);
    let t;
    if (route === 1) t = (1 - bl) * a + bl * b;
    else if (route === 2) { const u = Math.pow(f / xf, 2); t = a / Math.sqrt(1 + u * u) + b * u / Math.sqrt(1 + u * u); }
    else t = a * b;
    fs.push(f); ha.push(a); hb.push(b); ht.push(t);
  }
  const curve = (arr, rgb, a, lw) => {
    hstroke(g, rgb, a, lw);
    g.beginPath();
    arr.forEach((v, i) => { const x = X(fs[i]), y = Y(20 * Math.log10(Math.max(v, 1e-5))); if (i) g.lineTo(x, y); else g.moveTo(x, y); });
    g.stroke();
  };
  curve(hb, C0, 0.3, 1);
  curve(ha, C0, 0.45, 1);
  curve(ht, C1, 0.95, 1.7);
  //  where each cutoff sits
  [[A, "A"], [Bf, "B"]].forEach(q => {
    const x = X(Math.max(fmin, Math.min(fmax, q[0].fc)));
    hline(g, x, B - 5, x, B, C1, 0.6);
    htext(g, q[1], x, B - 7, C1, 0.55, "center", 8);
  });
  const nm = q => ["LADDER", "GROWL", "SCREAM", "SVF", "DIODE", "COMB", "FORMANT"][q.circ];
  htext(g, "A " + nm(A) + "  B " + nm(Bf) + "  " + ["SERIES", "PARALLEL", "SPLIT"][route] + "  = HEARD", narrow ? 10 : d.w - 10, narrow ? 27 : 15, C1, 0.45, narrow ? "left" : "right", 8);
}
function drawRouting (d) {
  const g = d.g; hframe(d, "HORIZONS · SIGNAL PATH");
  const route = Math.round(V("route")), w = d.w, h = d.h;
  const midY = h * 0.44, upY = h * 0.26, loY = h * 0.62, singY = h * 0.86;
  const box = (x, y, bw, label, sub, rgb, a) => {
    hstroke(g, rgb, a, 1.2);
    g.strokeRect(x - bw / 2, y - 11, bw, 22);
    htext(g, label, x, y + 3, rgb, a, "center", 9);
    if (sub) htext(g, sub, x, y + 22, rgb, a * 0.6, "center", 8);
  };
  const path = (pts, rgb, a, lw) => {
    hstroke(g, rgb, a, lw || 1.2);
    g.beginPath(); pts.forEach((p, i) => { if (i) g.lineTo(p[0], p[1]); else g.moveTo(p[0], p[1]); }); g.stroke();
    //  the signal, moving: three dots a path
    let len = 0; const seg = [];
    for (let i = 1; i < pts.length; ++i) { const l = Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]); seg.push(l); len += l; }
    for (let k = 0; k < 3; ++k) {
      let u = ((HUD.t * 0.35 + k / 3) % 1) * len, i = 0;
      while (i < seg.length - 1 && u > seg[i]) { u -= seg[i]; ++i; }
      const t = seg[i] ? u / seg[i] : 0;
      glowDot(g, pts[i][0] + (pts[i + 1][0] - pts[i][0]) * t, pts[i][1] + (pts[i + 1][1] - pts[i][1]) * t, 1.8, rgb, a);
    }
  };
  const xIn = w * 0.08, xOut = w * 0.92, cA = choiceName("fa_circ"), cB = choiceName("fb_circ");
  const mA = choiceName("fa_mode"), mB = choiceName("fb_mode");
  if (route === 0) {
    const xa = w * 0.34, xb = w * 0.62;
    path([[xIn, midY], [xa - 30, midY]], C0, 0.6); path([[xa + 30, midY], [xb - 30, midY]], C0, 0.6); path([[xb + 30, midY], [xOut - 34, midY]], C0, 0.6);
    box(xa, midY, 60, "A", cA + " " + mA, C1, 0.9); box(xb, midY, 60, "B", cB + " " + mB, C1, 0.9);
  } else {
    const xs = w * 0.24, xa = w * 0.5, xm = w * 0.74;
    const lab = route === 1 ? null : "SPLIT";
    if (route === 2) box(xs, midY, 54, "SPLIT", fmtId("split"), C1, 0.8);
    const x0 = route === 2 ? xs + 27 : xIn;
    path([[xIn, midY], [x0, midY]], C0, 0.6);
    path([[x0, midY], [x0 + 12, upY], [xa - 30, upY]], C0, 0.6);
    path([[x0, midY], [x0 + 12, loY], [xa - 30, loY]], C0, 0.6);
    box(xa, upY, 60, "A", cA + " " + mA, C1, 0.9); box(xa, loY, 60, "B", cB + " " + mB, C1, 0.9);
    if (lab) { htext(g, "LOW", x0 + 16, upY - 5, C0, 0.5, "left", 8); htext(g, "HIGH", x0 + 16, loY + 13, C0, 0.5, "left", 8); }
    path([[xa + 30, upY], [xm - 10, midY]], C0, 0.6); path([[xa + 30, loY], [xm - 10, midY]], C0, 0.6);
    hstroke(g, C1, 0.9, 1.2); g.beginPath(); g.arc(xm, midY, 10, 0, Math.PI * 2); g.stroke();
    htext(g, "+", xm, midY + 4, C1, 0.9, "center", 11);
    if (route === 1) htext(g, "A/B " + fmtId("blend"), xm, midY + 26, C1, 0.5, "center", 8);
    path([[xm + 10, midY], [xOut - 34, midY]], C0, 0.6);
  }
  box(xOut - 16, midY, 32, "OUT", null, C1, 0.8);
  htext(g, "IN", xIn, midY - 8, C1, 0.6, "center", 8);
  //  the singularity leaves before the horizons and rejoins after them
  const sl = V("sing_level");
  path([[xIn, midY], [xIn, singY], [xOut - 16, singY], [xOut - 16, midY + 11]], AMB, sl > 0.001 ? 0.25 + 0.6 * sl : 0.1, 1);
  htext(g, "SINGULARITY  " + (sl > 0.001 ? Math.round(sl * 100) + "%" : "OFF") + "  bypasses the horizons", w * 0.5, singY - 5, AMB, sl > 0.001 ? 0.6 : 0.25, "center", 8);
}

// ---- CHRONO: the three envelopes, drawn with the engine's own segments
function envCurve (pre) {
  const ta = Math.max(1e-4, lawVal(pre + "_a")), td = Math.max(1e-4, lawVal(pre + "_d")), su = V(pre + "_s");
  const tr = Math.max(1e-4, lawVal(pre + "_r")), cv = Math.max(0, Math.min(1, V(pre + "_curve")));
  //  Env::tick: attack towards 1.05, decay towards su - 0.02, release towards -0.02
  const att = ta * Math.log(1.05 / 0.05), dec = td * Math.log(Math.max(1.0001, (1.02 - su) / 0.021));
  const rel = su > 0.0005 ? tr * Math.log((su + 0.02) / 0.0205) : 0;
  const hold = Math.max(0.15, 0.3 * (att + dec + rel)), T = att + dec + hold + rel;
  const f = t => {
    let y;
    if (t < att) y = Math.min(1, 1.05 - 1.05 * Math.exp(-t / ta));
    else if (t < att + dec) y = Math.max(su, (su - 0.02) + (1.02 - su) * Math.exp(-(t - att) / td));
    else if (t < att + dec + hold) y = su;
    else y = Math.max(0, -0.02 + (su + 0.02) * Math.exp(-(t - att - dec - hold) / tr));
    return y * y + (y - y * y) * cv;
  };
  return { f: f, T: T, hold0: att + dec, hold1: att + dec + hold };
}
function secs (t) { return t < 1 ? Math.round(t * 1000) + " ms" : t.toFixed(2) + " s"; }
function drawEnv (d) {
  const g = d.g; hframe(d, "CHRONO · SHAPES");
  const rows = [["e1", "AMP", C1, 0], ["e2", "FILTER", hueRgb("r_e2", C1), 1], ["e3", "ENV 3", hueRgb("r_e3", C1), 2]];
  const top = 22, rh = (d.h - top - 4) / 3;
  rows.forEach((r, k) => {
    const e = envCurve(r[0]), y1 = top + rh * (k + 1) - 6, hh = rh - 24, x0 = 14, w = d.w - 40;
    hline(g, x0, y1, x0 + w, y1, C0, 0.13);
    const X = t => x0 + t / e.T * w;
    hline(g, X(e.hold1), y1 - hh, X(e.hold1), y1, C0, 0.12);
    hstroke(g, r[2], 0.9, 1.5);
    g.beginPath();
    for (let i = 0; i <= 200; ++i) { const t = e.T * i / 200, y = y1 - e.f(t) * hh; if (i) g.lineTo(X(t), y); else g.moveTo(X(t), y); }
    g.stroke();
    //  the live value, where the voice's envelope is right now
    const live = S.mod && S.mod[r[3]] != null ? Math.max(0, Math.min(1, S.mod[r[3]])) : 0;
    glowDot(g, x0 + w + 12, y1 - live * hh, 2.6, r[2], 0.35 + 0.65 * live);
    htext(g, r[1] + "  " + secs(e.T - (e.hold1 - e.hold0)), x0 + 2, y1 - hh - 5, r[2], 0.65, "left", 8);
  });
}
function drawTraces (d, title, lanes) {
  const g = d.g; hframe(d, title);
  const top = 22, rh = (d.h - top - 4) / lanes.length;
  lanes.forEach((ln, k) => {
    const y0 = top + rh * k + rh / 2 + 2, amp = rh * 0.38, tr = HUD.tr[ln.i] || [];
    hline(g, 12, y0 + (ln.bip ? 0 : amp), d.w - 12, y0 + (ln.bip ? 0 : amp), C0, 0.12);
    if (tr.length > 1) {
      hstroke(g, ln.rgb, 0.9, 1.3);
      g.beginPath();
      const n = 180;
      tr.forEach((v, i) => {
        const x = 12 + (d.w - 24) * (1 - (tr.length - 1 - i) / (n - 1));
        const y = ln.bip ? y0 - Math.max(-1, Math.min(1, v)) * amp : y0 + amp - Math.max(0, Math.min(1, v)) * 2 * amp;
        if (i) g.lineTo(x, y); else g.moveTo(x, y);
      });
      g.stroke();
    }
    htext(g, ln.name + (S.frozen && S.frozen[ln.i] ? "  FROZEN" : ""), 14, y0 - amp + 6, ln.rgb, 0.6, "left", 8);
  });
}

// ---- ORBITS: the three LFO shapes, as Lfo::tick draws them
function lfoShape (shape, ph) {
  switch (shape) {
    case 1: return 4 * Math.abs(ph - 0.5) - 1;
    case 2: return 2 * ph - 1;
    case 3: return ph < 0.5 ? 1 : -1;
    case 4: { const u = Math.max(1e-3, Math.min(1, 1 - ph)), acc = Math.pow(u, -0.375) - 1;
              return Math.sin(2 * Math.PI * Math.max(0, Math.min(40, acc * 0.35))); }
    default: return Math.sin(2 * Math.PI * ph);
  }
}
function drawLfo (d) {
  const g = d.g; hframe(d, "ORBITS · TWO CYCLES");
  const top = 22, rh = (d.h - top - 4) / 3;
  [1, 2, 3].forEach((n, k) => {
    const rgb = hueRgb("r_l" + n, C1), y0 = top + rh * k + rh / 2 + 2, sh = Math.round(V("l" + n + "_shape"));
    hline(g, 14, y0, d.w - 26, y0, C0, 0.12);
    plotFn(g, 14, y0, d.w - 40, rh * 0.32, 220, t => lfoShape(sh, (t * 2) % 1), rgb, 0.9, 1.4);
    const live = S.mod && S.mod[3 + n - 1] != null ? S.mod[3 + n - 1] : 0;
    glowDot(g, d.w - 14, y0 - Math.max(-1, Math.min(1, live)) * rh * 0.32, 2.6, rgb, 0.9);
    const sync = V("l" + n + "_sync") > 0.5;
    htext(g, "LFO " + n + "  " + choiceName("l" + n + "_shape") + "  " + fmtId("l" + n + "_rate") + (sync ? "  TEMPO" : ""),
          16, y0 - rh * 0.32 - 6, rgb, 0.7, "left", 8);
  });
}

// ---- MATRIX: every live route, as a glowing network
//  source index -> which live value the scope carries for it
const SRC_LIVE = { 1: 0, 2: 1, 3: 2, 4: 3, 5: 4, 6: 5, 7: 6, 8: 7, 9: 8 };
function drawNetwork (d) {
  const g = d.g;
  const srcs = ((S.byId.m1_src || {}).choices || "").split("|"), dsts = ((S.byId.m1_dst || {}).choices || "").split("|");
  const routes = [];
  for (let k = 1; k <= 8; ++k) {
    const s0 = Math.round(V("m" + k + "_src")), d0 = Math.round(V("m" + k + "_dst")), a = V("m" + k + "_amt");
    if (s0 > 0 && d0 > 0 && Math.abs(a) > 0.001) routes.push({ s: s0, d: d0, a: a, k: k });
  }
  const slots = routes.length;
  //  each LFO's own target, set on ORBITS
  for (let l = 1; l <= 3; ++l) {
    const d0 = Math.round(V("l" + l + "_dst")), a = V("l" + l + "_amt");
    if (d0 > 0 && Math.abs(a) > 0.001) routes.push({ s: 3 + l, d: d0, a: a, k: 8 + l });
  }
  //  the built-in routes, dashed: the amp envelope to LEVEL, the filter
  //  envelope to each cutoff at its DEPTH
  const di = nm => dsts.indexOf(nm);
  routes.push({ s: 1, d: di("LEVEL"), a: 1, k: 12, fixed: 1 });
  if (Math.abs(V("fa_env")) > 0.001) routes.push({ s: 2, d: di("A CUTOFF"), a: V("fa_env"), k: 13, fixed: 1 });
  if (Math.abs(V("fb_env")) > 0.001) routes.push({ s: 2, d: di("B CUTOFF"), a: V("fb_env"), k: 14, fixed: 1 });
  hframe(d, "MATRIX · " + slots + " OF 8 SLOTS · DASHED = BUILT IN");
  const top = 30, bot = d.h - 12, xs = d.w * 0.26, xd = d.w * 0.74;
  const ys = i => top + (bot - top) * (i - 1) / Math.max(1, srcs.length - 2);
  const yd = i => top + (bot - top) * (i - 1) / Math.max(1, dsts.length - 2);
  const usedS = {}, usedD = {};
  routes.forEach(r => { usedS[r.s] = 1; usedD[r.d] = 1; });
  //  the links: curves that breathe with their source
  routes.forEach(r => {
    const li = SRC_LIVE[r.s], live = li != null && S.mod ? Math.min(1, Math.abs(S.mod[li] || 0)) : 0.6;
    const rgb = r.a >= 0 ? C1 : NEG, a = 0.25 + 0.6 * live;
    const y0 = ys(r.s), y1 = yd(r.d), cx = (xs + xd) / 2;
    hstroke(g, rgb, a, 0.8 + 2.4 * Math.abs(r.a));
    if (r.fixed) g.setLineDash([5, 4]);
    g.beginPath(); g.moveTo(xs, y0); g.bezierCurveTo(cx, y0, cx, y1, xd, y1); g.stroke();
    if (r.fixed) g.setLineDash([]);
    for (let j = 0; j < 3; ++j) {
      const t = (HUD.t * (0.25 + 0.5 * live) + j / 3 + r.k * 0.13) % 1, u = 1 - t;
      const x = u * u * u * xs + 3 * u * u * t * cx + 3 * u * t * t * cx + t * t * t * xd;
      const y = u * u * u * y0 + 3 * u * u * t * y0 + 3 * u * t * t * y1 + t * t * t * y1;
      glowDot(g, x, y, 1.6, rgb, 0.4 + 0.6 * live);
    }
  });
  //  the nodes: every source and destination a dot; only the used ones speak
  for (let i = 1; i < srcs.length; ++i) {
    const on = usedS[i], y = ys(i);
    glowDot(g, xs, y, on ? 3.2 : 1.3, C1, on ? 0.95 : 0.25);
    if (on) htext(g, srcs[i], xs - 9, y + 3, C1, 0.8, "right", 9);
  }
  for (let i = 1; i < dsts.length; ++i) {
    const on = usedD[i], y = yd(i);
    glowDot(g, xd, y, on ? 3.2 : 1.3, C1, on ? 0.95 : 0.25);
    if (on) htext(g, dsts[i], xd + 9, y + 3, C1, 0.8, "left", 9);
  }

}

// ---- OUTPUT: the meters, amber where the ceiling is working
function dbOf (x) { return 20 * Math.log10(Math.max(x, 1e-6)); }
function drawMeters (d) {
  const g = d.g; hframe(d, "OUTPUT · LEVEL");
  const L = S.lvl || [0, 0, 0, 0, 0], over = Math.max(0, Math.min(1, L[4] || 0));
  const top = 26, bot = d.h - 14, lo = -48;
  const Y = db => top + (0 - Math.max(lo, Math.min(0, db))) / (0 - lo) * (bot - top);
  const cx = d.w / 2, bw = Math.min(26, d.w * 0.12);
  [0, -6, -12, -24, -36, -48].forEach(db => { hline(g, cx - bw - 26, Y(db), cx + bw + 26, Y(db), C0, 0.1);
    htext(g, String(db), cx - bw - 30, Y(db) + 3, C0, 0.4, "right", 8); });
  [0, 1].forEach(ch => {
    //  the panel holds its own peak, falling 12 dB a second
    HUD.pk[ch] = Math.max(L[ch] || 0, HUD.pk[ch] * 0.955);
    const x = ch ? cx + 4 : cx - bw - 4, rms = dbOf(L[2 + ch] || 0), pk = dbOf(HUD.pk[ch]);
    hstroke(g, C0, 0.35, 1, 0); g.strokeRect(x, top, bw, bot - top);
    const rgb = over > 0.02 ? AMB : C0;
    g.shadowBlur = 8 * HUD.gain; g.shadowColor = "rgba(" + rgb + ",0.8)";
    g.fillStyle = "rgba(" + rgb + "," + (0.55 * HUD.gain).toFixed(3) + ")";
    g.fillRect(x + 2, Y(rms), bw - 4, bot - Y(rms)); g.shadowBlur = 0;
    hline(g, x, Y(pk), x + bw, Y(pk), over > 0.02 ? AMB : C1, 0.95);
    htext(g, ch ? "R" : "L", x + bw / 2, bot + 11, C1, 0.5, "center", 8);
  });
  const pkAll = Math.max(HUD.pk[0], HUD.pk[1]);
  htext(g, (pkAll > 1e-5 ? dbOf(pkAll).toFixed(1) : "-inf") + " dB", d.w - 10, 15, C1, 0.7, "right", 9);
  htext(g, "CEILING", cx, top - 6, over > 0.02 ? AMB : C0, over > 0.02 ? 0.4 + 0.6 * over : 0.3, "center", 8);
}
function drawLoud (d) {
  const g = d.g; hframe(d, "OUTPUT · HISTORY");
  const tr = HUD.tr.lv || [], ov = HUD.tr.ov || [], top = 26, bot = d.h - 14, lo = -48, n = 180;
  const Y = db => top + (0 - Math.max(lo, Math.min(0, db))) / (0 - lo) * (bot - top);
  [0, -12, -24, -36].forEach(db => { hline(g, 30, Y(db), d.w - 10, Y(db), C0, 0.1); htext(g, String(db), 26, Y(db) + 3, C0, 0.4, "right", 8); });
  for (let i = 1; i < tr.length; ++i) {
    const x0 = 30 + (d.w - 40) * (1 - (tr.length - i) / (n - 1)), x1 = 30 + (d.w - 40) * (1 - (tr.length - 1 - i) / (n - 1));
    const hot = (ov[i] || 0) > 0.02;
    hstroke(g, hot ? AMB : C1, hot ? 1 : 0.85, 1.5);
    g.beginPath(); g.moveTo(x0, Y(tr[i - 1])); g.lineTo(x1, Y(tr[i])); g.stroke();
  }
  htext(g, "AMBER: THE CEILING IS SHAPING THE PEAKS", d.w - 10, d.h - 4, AMB, 0.35, "right", 8);
}

function hudDraw () {
  for (const e of HUD.list) {
    if (!e.cv.isConnected) continue;
    const d = hudPrep(e.cv); if (!d) continue;
    switch (e.kind) {
      case "osc":      drawOsc(d); break;
      case "subnoise": drawSubNoise(d); break;
      case "filter":   drawFilter(d); break;
      case "routing":  drawRouting(d); break;
      case "env":      drawEnv(d); break;
      case "envlive":  drawTraces(d, "CHRONO · LIVE", [
                         { i: 0, name: "AMP", rgb: C1 }, { i: 1, name: "FILTER", rgb: hueRgb("r_e2", C1) },
                         { i: 2, name: "ENV 3", rgb: hueRgb("r_e3", C1) }]); break;
      case "lfo":      drawLfo(d); break;
      case "lfolive":  drawTraces(d, "ORBITS · LIVE", [
                         { i: 3, name: "LFO 1", rgb: hueRgb("r_l1", C1), bip: 1 }, { i: 4, name: "LFO 2", rgb: hueRgb("r_l2", C1), bip: 1 },
                         { i: 5, name: "LFO 3", rgb: hueRgb("r_l3", C1), bip: 1 }, { i: 6, name: "RANDOM", rgb: hueRgb("r_rnd", C1), bip: 1 },
                         { i: 7, name: "CHIRP", rgb: hueRgb("r_chirp", C1), bip: 1 }]); break;
      case "network":  drawNetwork(d); break;
      case "meters":   drawMeters(d); break;
      case "loud":     drawLoud(d); break;
    }
  }
}
function hudFeed (o) {
  S.mod = o.mod || S.mod;
  S.lvl = o.lvl || S.lvl;
  HUD.t += 1 / 30;
  const push = (k, v) => { const a = HUD.tr[k] = HUD.tr[k] || []; a.push(v); if (a.length > 180) a.shift(); };
  for (let i = 0; i < 9; ++i) push(i, S.mod && S.mod[i] != null ? S.mod[i] : 0);
  if (S.lvl) { push("lv", dbOf(Math.max(S.lvl[2] || 0, S.lvl[3] || 0))); push("ov", S.lvl[4] || 0); }
  hudDraw();
}

// ------------------------------------------------------------ settings
//  Per viewer, not per patch: they belong to the room the ship is in.
const SET = { bright: 100, hud: 150 };
function loadSettings () {
  try { const o = JSON.parse(localStorage.getItem("gw.settings.v2") || "{}"); for (const k in SET) if (typeof o[k] === "number") SET[k] = o[k]; } catch (e) {}
}
function saveSettings () { try { localStorage.setItem("gw.settings.v2", JSON.stringify(SET)); } catch (e) {} }
function applySettings () {
  el("ship").style.filter = SET.bright === 100 ? "" : "brightness(" + (SET.bright / 100).toFixed(2) + ")";
  HUD.gain = SET.hud / 100;
  hudDraw();
}
function buildSettings () {
  const wrap = document.createElement("div"); wrap.className = "setp";
  const rows = [["bright", "PANEL BRIGHTNESS", 40, 160], ["hud", "HUD LINES", 30, 300]];
  const inputs = {};
  for (const r of rows) {
    const row = document.createElement("div"); row.className = "setrow";
    const nm = document.createElement("span"); nm.className = "nm"; nm.textContent = r[1];
    const inp = document.createElement("input"); inp.type = "range"; inp.min = r[2]; inp.max = r[3]; inp.value = SET[r[0]]; inp.id = "set_" + r[0];
    const v = document.createElement("span"); v.className = "v"; v.textContent = SET[r[0]] + "%";
    inp.oninput = () => { SET[r[0]] = +inp.value; v.textContent = inp.value + "%"; applySettings(); saveSettings(); };
    row.appendChild(nm); row.appendChild(inp); row.appendChild(v); wrap.appendChild(row);
    inputs[r[0]] = [inp, v];
  }
  const reset = document.createElement("button"); reset.className = "hud"; reset.textContent = "RESET";
  reset.onclick = () => { SET.bright = 100; SET.hud = 150;
    for (const k in inputs) { inputs[k][0].value = SET[k]; inputs[k][1].textContent = SET[k] + "%"; }
    applySettings(); saveSettings(); };
  wrap.appendChild(reset);
  const note = document.createElement("div"); note.className = "note";
  note.textContent = "Brightness lifts or dims the whole ship, the viewscreen included. HUD LINES sets how bold and bright the line displays are - the two beside this panel show it as you move it. Both are remembered on this computer and belong to no patch.";
  wrap.appendChild(note);
  return wrap;
}
loadSettings();

// ------------------------------------------------------------ the scope
const sc = el("scope"), sctx = sc.getContext("2d");
let wave = [];
function drawScope () {
  const w = sc.clientWidth, h = sc.clientHeight;
  if (!w || !h) return;
  if (sc.width !== w || sc.height !== h) { sc.width = w; sc.height = h; }
  sctx.clearRect(0, 0, w, h);
  sctx.strokeStyle = "#0f1c2b"; sctx.lineWidth = 1;
  for (let i = 1; i < 4; ++i) { sctx.beginPath(); sctx.moveTo(w * i / 4, 0); sctx.lineTo(w * i / 4, h); sctx.stroke(); }
  sctx.beginPath(); sctx.moveTo(0, h / 2); sctx.lineTo(w, h / 2); sctx.stroke();
  if (!wave.length) return;
  sctx.strokeStyle = "#4fc3f7"; sctx.lineWidth = 1.3; sctx.shadowColor = "rgba(79,195,247,.8)"; sctx.shadowBlur = 5;
  sctx.beginPath();
  for (let i = 0; i < wave.length; ++i) {
    const x = i / (wave.length - 1) * w;
    const y = h / 2 - Math.max(-1, Math.min(1, wave[i])) * h * 0.45;
    i ? sctx.lineTo(x, y) : sctx.moveTo(x, y);
  }
  sctx.stroke(); sctx.shadowBlur = 0;
}

// ------------------------------------------------------------ keyboard
//  Four octaves, C1 to B4: where a bass lives.  White keys on the lower row,
//  black keys on the upper row centred between their neighbours.
(function () {
  const box = el("kbd");
  const PITCH = 25, SIZE = 22;
  const isBlack = n => [1, 3, 6, 8, 10].indexOf(n % 12) >= 0;
  let w = 0;
  for (let n = 24; n < 72; ++n) {
    const d = document.createElement("div");
    const black = isBlack(n);
    d.className = "k " + (black ? "b" : "w");
    d.dataset.n = n;
    d.style.left = (black ? (w - 1) * PITCH + PITCH / 2 : w * PITCH) + "px";
    if (!black) {
      if (n % 12 === 0) { const t = document.createElement("i"); t.textContent = "C" + (n / 12 - 1); d.appendChild(t); }
      ++w;
    }
    const on  = e => { if (e) e.preventDefault(); d.classList.add("down"); NB.send({ k:"n", n:n, on:true, v:0.9 }); };
    const off = () => { if (!d.classList.contains("down")) return; d.classList.remove("down"); NB.send({ k:"n", n:n, on:false }); };
    d.addEventListener("pointerdown", on);
    d.addEventListener("pointerup", off);
    d.addEventListener("pointerleave", off);
    box.appendChild(d);
  }
  box.style.width = (w * PITCH - (PITCH - SIZE)) + "px";
})();

// ------------------------------------------------------------ world fx
//  The documented contract (bwfx-rack.js, top of file): attach({send}), feed
//  state with onState(), and ONE host button carrying data-bwfx-open.
//  260926.1 fed state to BWFX.state(), which is a getter - the overlay then
//  showed its own default rack while the plug-in ran another.
(function () {
  const s = document.createElement("script");
  s.src = "bwfx-rack.js";
  s.onload = () => {
    try { BWFX.attach({ send: m => NB.send(Object.assign({ k: "bwfx" }, m)) }); } catch (e) {}
  };
  document.head.appendChild(s);
})();

// ------------------------------------------------------------ the native side
NB.on("initialState", st => {
  S.byId = {}; S.v = {};
  for (const p of (st.params || [])) { S.byId[p.id] = p; S.v[p.id] = p.v; }
  S.factory = (st.factory || []).slice();
  S.lfoDivs  = st.lfoDivs || [];
  S.modeMask = st.modeMask || null;
  el("buildid").textContent = "BROKILD \\u00B7 BUILD " + (st.build || "-");
  const fi = S.factory.findIndex(f => f.name === st.patch);
  S.curSlot = st.patchSlot >= 0 ? st.patchSlot : -1; S.curUser = null;
  if (S.curSlot >= 32) showProg("YOURS", slotLabel(S.curSlot) + "  " + st.patch);
  else if (fi >= 0) { S.prog = fi; showProg(S.factory[fi].group, String(fi).padStart(2, "0") + "  " + st.patch); }
  else showProg("PATCH", st.patch || "-");
  S.cells = {};
  buildPillars();
  buildTabs();
  let start = "helm"; try { start = localStorage.getItem("gw.page") || "helm"; } catch (e) {}
  if (!PAGES.some(p => p.key === start)) start = "helm";
  showPage(start);
  syncRadii();
  applySettings();
  el("hor").value = String(Math.round((S.v.macro_horiz != null ? S.v.macro_horiz : 0.5) * 100));
  NB.send({ k: "ready" });
});

NB.on("hostParam", o => { for (const id in o) if (id in S.byId) set(id, o[id], false); });

NB.on("scope", o => {
  //  the well is driven by the SOUND: MASS is the engine's own follower
  S.mass = o.mass || 0;
  el("mass").value = String(Math.round(S.mass * 100));
  el("rmass").textContent = S.mass.toFixed(2);
  for (const c of (S.cells.macro_mass || [])) if (c.paintMod) c.paintMod(S.mass);
  const fr = (o.frozen || []).map(x => !!x);
  let changed = fr.length !== S.frozen.length;
  for (let i = 0; !changed && i < fr.length; ++i) if (fr[i] !== S.frozen[i]) changed = true;
  S.frozen = fr;
  if (changed) paintFrozen();
  wave = o.wave || [];
  drawScope();
  hudFeed(o);
  const noWell = (o.rs || 0) < 0.02;
  for (const c of (S.cells.redshift || [])) {
    c.cell.classList.toggle("inert", noWell);
    c.cell.title = noWell ? "REDSHIFT needs mass in the well - raise MASS and HORIZON" : "REDSHIFT";
  }
});

NB.on("patchTree", t => {
  S.user = Array.isArray(t) ? t : [];
  //  a patch restored with the project: find its file so the arrows know where they are
  if (S.curSlot >= 32 && !S.curUser) { const x = S.user.find(u => u.slot === S.curSlot); if (x) S.curUser = x.path; }
});
//  notices are for the native side's own record; the header is set by the page
//  on every load and save, so a notice must never rename the patch on screen
NB.on("notice", m => { S.lastNotice = String(m); });
NB.on("bwfx", st => { try { BWFX.onState(st); } catch (e) {} });

//  for the probe, and for anyone diagnosing a panel from a debugger: the
//  state lives in this closure and is otherwise unreadable from outside
window.__GW = { S: S, pages: PAGES, showPage: showPage, set: set, hud: HUD, hudDraw: hudDraw, hudFeed: hudFeed, settings: SET, applySettings: applySettings, mods: () => (typeof MODS !== "undefined" ? MODS : []), ch: () => CH };

//  A reload the editor never saw would leave the processor believing an ack
//  that no longer holds - so every boot says hello first.
NB.send({ k: "hello" });
window.addEventListener("resize", () => { drawScope(); hudDraw(); });
})();
</script>
</html>
`;

fs.mkdirSync(path.dirname(OUT), { recursive: true });
fs.writeFileSync(OUT, html);
console.log('wrote ' + path.relative(ROOT, OUT) + '  (' + html.length + ' bytes)');
console.log('every parameter has one home: ' + Object.keys(home).length + ' placed, ' +
            TABLE.filter(p => p.id.startsWith('seq_')).length + ' sequencer parameters off the panel by decision');
