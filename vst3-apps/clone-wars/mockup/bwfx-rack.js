/* ===========================================================================
   BROKILD WORLD FX — the shared rack overlay fragment.

   Ships inside the BWFX repo and is compiled into every Brokild synth
   (BinaryData + resource provider, or a file beside the mockup). The host
   page adds ONE button carrying data-bwfx-open (the globe + accent) and,
   in the plugin bridge only, wires the message pipe:

       BWFX.attach({ send: obj => backend.emitEvent("cw", {k:"bwfx", ...obj}) });
       backend.addEventListener("bwfx", p => BWFX.onState(p));

   Without attach() the fragment runs standalone on its own state — the
   mockup stays a fully explorable instrument.

   Everything below is generated from the module descriptors: the pedals,
   their controls, the state. The C++ registry is the source of truth; the
   DEFAULT_DESC snapshot only serves the standalone mockup and is overridden
   by the descriptors the native side sends.
   =========================================================================== */
(function () {
  "use strict";

  /* Snapshot of bwfx::descriptorJson() for standalone use. */
  var DEFAULT_DESC = [
    { id: "saturation", name: "TUBE", sub: "asymmetric valve saturation", ver: 1, params: [
      { id: "drive", name: "DRIVE", def: 8, lo: 0, hi: 24, step: 0, unit: "dB" },
      { id: "tone", name: "TONE", def: 72, lo: 0, hi: 100, step: 0, unit: "%" } ] },
    { id: "phaser", name: "SWEEP", sub: "vintage 4-stage phaser", ver: 1, params: [
      { id: "mix", name: "MIX", def: 35, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "rate", name: "RATE", def: 40, lo: 5, hi: 200, step: 0, unit: "cHz" },
      { id: "depth", name: "DEPTH", def: 55, lo: 0, hi: 100, step: 0, unit: "%" } ] },
    { id: "chorus", name: "ENSEMBLE", sub: "dual-line chorus", ver: 1, params: [
      { id: "mix", name: "MIX", def: 40, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "rate", name: "RATE", def: 45, lo: 5, hi: 300, step: 0, unit: "cHz" },
      { id: "depth", name: "DEPTH", def: 55, lo: 0, hi: 100, step: 0, unit: "%" } ] },
    { id: "stutter", name: "GATE", sub: "rhythmic stutter gate", ver: 1, params: [
      { id: "amount", name: "AMOUNT", def: 50, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "rate", name: "RATE", def: 80, lo: 10, hi: 160, step: 0, unit: "dHz" } ] },
    { id: "delay", name: "ECHO", sub: "stereo tape echo", ver: 1, params: [
      { id: "mix", name: "MIX", def: 25, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "time", name: "TIME", def: 260, lo: 40, hi: 900, step: 0, unit: "ms" },
      { id: "feedback", name: "FEEDBACK", def: 34, lo: 0, hi: 112, step: 0, unit: "%" },
      { id: "offset", name: "OFFSET", def: 0, lo: -250, hi: 250, step: 5, unit: "ms" },
      { id: "character", name: "CHARACTER", def: 0, lo: 0, hi: 1, step: 1, unit: "", choices: "CLEAN|TAPE" } ] },
    { id: "reverb", name: "SPACE", sub: "stereo convolution space", ver: 1, params: [
      { id: "mix", name: "MIX", def: 25, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "character", name: "CHARACTER", def: 0, lo: 0, hi: 4, step: 1, unit: "", choices: "ROOM|HALL|PLATE|SPRING|REVERSE" },
      { id: "length", name: "LENGTH", def: 180, lo: 20, hi: 600, step: 0, unit: "cs" } ] }
  ];

  var VERSION = "1.0.0";
  var desc = DEFAULT_DESC;
  var send = null;                 // native pipe; null = standalone
  var state = defaultState();
  var open = false;
  var built = false;

  function defaultState() {
    var s = { mix: 1, order: [], modules: {} };
    desc.forEach(function (d) {
      s.order.push(d.id);
      var p = {};
      d.params.forEach(function (pd) { p[pd.id] = pd.def; });
      s.modules[d.id] = { on: 0, p: p };
    });
    return s;
  }

  function descOf(id) { for (var i = 0; i < desc.length; i++) if (desc[i].id === id) return desc[i]; return null; }

  function fmt(pd, v) {
    switch (pd.unit) {
      case "%":   return Math.round(v) + " %";
      case "dB":  return (Math.round(v * 10) / 10) + " dB";
      case "ms":  return Math.round(v) + " ms";
      case "cHz": return (v / 100).toFixed(2) + " Hz";
      case "dHz": return (v / 10).toFixed(1) + " Hz";
      case "cs":  return (v / 100).toFixed(2) + " s";
      default:
        if (pd.choices) { var c = pd.choices.split("|"); return c[Math.round(v)] || c[0]; }
        return String(Math.round(v * 100) / 100);
    }
  }

  /* ------------------------------------------------------------------ CSS */
  var CSS = [
    ".bwfx-veil{position:fixed;inset:0;z-index:9600;display:none;align-items:flex-start;justify-content:center;",
    " background:rgba(4,6,8,.72);backdrop-filter:blur(3px);overflow:auto;padding:4vh 16px 6vh;}",
    ".bwfx-veil.open{display:flex}",
    ".bwfx-win{--bwfx-acc:var(--bwfx-accent,#3fe0d8);position:relative;width:min(1060px,96vw);",
    " background:linear-gradient(180deg,#101418,#0a0d10 70%);border:1px solid #23303a;border-radius:14px;",
    " box-shadow:0 24px 80px rgba(0,0,0,.7),0 1px 0 rgba(255,255,255,.06) inset;",
    " font-family:system-ui,Segoe UI,Roboto,sans-serif;color:#cfd8dc;padding:0 0 10px;}",
    ".bwfx-head{display:flex;align-items:center;gap:12px;padding:12px 16px 10px;border-bottom:1px solid #1d2830;}",
    ".bwfx-head .bwfx-globe{width:22px;height:22px;color:var(--bwfx-acc);filter:drop-shadow(0 0 6px var(--bwfx-acc));flex:none}",
    ".bwfx-title{font-size:14px;font-weight:700;letter-spacing:.34em;color:#e8f4f2;white-space:nowrap}",
    ".bwfx-title small{display:block;font-size:9px;font-weight:400;letter-spacing:.22em;color:#5d7a76;margin-top:2px}",
    ".bwfx-mixrow{display:flex;align-items:center;gap:8px;margin-left:auto;font-size:10px;letter-spacing:.14em;color:#7c948f;}",
    ".bwfx-mixrow input{width:120px;accent-color:var(--bwfx-acc)}",
    ".bwfx-mixrow output{font:600 11px ui-monospace,Menlo,monospace;color:var(--bwfx-acc);min-width:38px;text-align:right}",
    ".bwfx-close{appearance:none;border:1px solid #2a3a44;background:#131b21;color:#cfd8dc;border-radius:8px;",
    " padding:7px 14px;font:600 11px system-ui,sans-serif;letter-spacing:.16em;cursor:pointer}",
    ".bwfx-close:hover{border-color:var(--bwfx-acc);color:var(--bwfx-acc)}",
    ".bwfx-cols{display:grid;grid-template-columns:1fr 1fr;gap:14px;padding:14px 16px 6px;}",
    "@media (max-width:820px){.bwfx-cols{grid-template-columns:1fr}}",
    ".bwfx-rack-t{font-size:10px;font-weight:700;letter-spacing:.3em;color:#6d8a86;padding:0 2px 8px}",
    ".bwfx-list{display:grid;gap:9px;align-content:start}",
    /* --- pedal shell (Photo-Synth 2 fx-module, BWFX-scoped) --- */
    ".bwfx-mod{--fxc:#f0a848;--fxglow:rgba(240,168,72,.5);",
    " background:linear-gradient(180deg,var(--fxbg1,#201a12),var(--fxbg2,#151109));",
    " border:1px solid #26303a;border-left:5px solid var(--fxc);border-radius:11px;overflow:hidden;",
    " transition:border-color .15s,opacity .15s,transform .12s;",
    " box-shadow:0 1px 0 rgba(255,255,255,.05) inset,0 3px 10px rgba(0,0,0,.35);}",
    ".bwfx-mod.bwfx-dragging{opacity:.65;border-color:var(--bwfx-acc);transform:scale(.99)}",
    ".bwfx-mod.bwfx-off{opacity:.62}",
    ".bwfx-mod.bwfx-off .bwfx-name{color:#77828a}",
    ".bwfx-mhead{display:flex;align-items:center;gap:7px;min-height:46px;padding:5px 7px}",
    ".bwfx-grip{appearance:none;border:0;background:transparent;color:#5d6a72;cursor:grab;",
    " width:30px;align-self:stretch;border-radius:7px;font-size:18px;line-height:1;touch-action:none;}",
    ".bwfx-grip:active{cursor:grabbing;color:var(--bwfx-acc)}",
    ".bwfx-mtoggle{appearance:none;border:0;background:transparent;color:#dde5e9;font:inherit;text-align:left;",
    " padding:6px 3px;cursor:pointer;display:flex;align-items:baseline;gap:8px;flex:1 1 auto;min-width:0;}",
    ".bwfx-index{color:var(--fxc);font:600 11px/1.5 ui-monospace,Menlo,monospace;",
    " background:#0f0d0a;border:1px solid rgba(255,255,255,.14);border-radius:4px;padding:0 4px;",
    " text-shadow:0 0 6px var(--fxglow);}",
    ".bwfx-name{font-size:12.5px;font-weight:700;letter-spacing:.12em;text-transform:uppercase}",
    ".bwfx-sub{font-size:11px;color:#7f8b93;font-weight:400;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}",
    ".bwfx-mtoggle::after{content:'+';color:#7f8b93;font-size:15px;margin-left:auto}",
    ".bwfx-mtoggle[aria-expanded=true]::after{content:'\\2013'}",
    ".bwfx-move{display:flex;gap:3px;flex:none;align-items:center}",
    ".bwfx-move .bwfx-ud{appearance:none;border:1px solid #2a3a44;background:#131b21;color:#aebec6;",
    " border-radius:7px;padding:5px 8px;font-size:12px;cursor:pointer}",
    ".bwfx-move .bwfx-ud:hover{border-color:var(--bwfx-acc);color:var(--bwfx-acc)}",
    ".bwfx-ctl{display:grid;grid-template-columns:1fr 1fr;gap:2px 12px;padding:8px 11px 10px;",
    " border-top:1px solid rgba(255,255,255,.07);}",
    /* display:grid would beat the hidden attribute's UA display:none (the
       Black Rider about-box lesson, attribute edition) — restate it. */
    ".bwfx-ctl[hidden]{display:none}",
    ".bwfx-row{display:grid;grid-template-columns:1fr auto;grid-template-areas:'lab out' 'ctl ctl';",
    " align-items:center;gap:2px 8px;padding:3px 0;min-width:0;}",
    ".bwfx-row label{grid-area:lab;font-size:10.5px;letter-spacing:.1em;color:#93a1a8}",
    ".bwfx-row output{grid-area:out;font:600 11px ui-monospace,Menlo,monospace;color:var(--fxc);",
    " text-shadow:0 0 6px var(--fxglow);}",
    ".bwfx-row .bwfx-c{grid-area:ctl;width:100%}",
    ".bwfx-row input[type=range]{width:100%;accent-color:var(--fxc);height:18px}",
    ".bwfx-row select{width:100%;background:#10161b;color:#cfd8dc;border:1px solid #2a3a44;border-radius:6px;",
    " font:11px system-ui,sans-serif;padding:4px 6px}",
    ".bwfx-row.bwfx-wide{grid-column:1/-1}",
    /* --- the rocker (Photo-Synth 2 spec-rocker at pedal size) --- */
    ".bwfx-rocker{appearance:none;flex:none;width:26px;height:30px;padding:3px;cursor:pointer;",
    " background:linear-gradient(160deg,#2e2e30,#0b0b0c 55%,#1b1b1c);",
    " border:1px solid #000;border-radius:6px;",
    " box-shadow:0 1px 0 rgba(255,255,255,.10) inset,0 -2px 3px rgba(0,0,0,.85) inset,0 2px 7px rgba(0,0,0,.6);}",
    ".bwfx-rocker .bwfx-lens{display:block;width:100%;height:100%;border-radius:3px;position:relative;overflow:hidden;",
    " border:1px solid rgba(0,0,0,.9);",
    " background:linear-gradient(180deg,rgba(255,255,255,.16),transparent 22%),",
    "  repeating-linear-gradient(90deg,rgba(0,0,0,.38) 0 1px,transparent 1px 4px),",
    "  linear-gradient(180deg,#232628 0%,#121415 55%,#232628 100%);",
    " box-shadow:0 2px 4px rgba(0,0,0,.85) inset;transition:box-shadow .2s;}",
    ".bwfx-rocker .bwfx-lens::after{content:'';position:absolute;left:8%;right:8%;bottom:8%;height:24%;border-radius:3px;",
    " background:linear-gradient(180deg,rgba(255,255,255,.13),rgba(255,255,255,.02));border:1px solid rgba(0,0,0,.5);}",
    ".bwfx-rocker .bwfx-lens i{position:absolute;inset:0;opacity:0;transition:opacity .22s;",
    " background:radial-gradient(circle at 50% 44%,#fff6d9 0 7%,transparent 26%),",
    "  radial-gradient(circle at 50% 44%,var(--fxc) 0 30%,transparent 72%),",
    "  repeating-linear-gradient(90deg,rgba(0,0,0,.25) 0 1px,transparent 1px 4px),",
    "  radial-gradient(circle at 50% 50%,var(--fxc) 0 62%,#1a1c1d 100%);}",
    ".bwfx-rocker[aria-pressed=true] .bwfx-lens i{opacity:1}",
    ".bwfx-rocker[aria-pressed=true] .bwfx-lens{box-shadow:0 0 14px var(--fxglow),0 2px 4px rgba(0,0,0,.6) inset}",
    /* --- per-pedal boutique faceplates (Photo-Synth 2's enclosures) --- */
    ".bwfx-mod[data-fx=saturation]{--fxc:#ff5533;--fxglow:rgba(255,85,51,.55);",
    " background:repeating-linear-gradient(-45deg,rgba(255,85,51,.08) 0 12px,transparent 12px 24px),",
    "  linear-gradient(180deg,#2c1008,#160605);border-color:#6b2417;border-left-color:#ff5533;border-radius:6px;}",
    ".bwfx-mod[data-fx=saturation] .bwfx-name{color:#ff8f70;letter-spacing:.24em;text-shadow:0 0 9px rgba(255,85,51,.55)}",
    ".bwfx-mod[data-fx=phaser]{--fxc:#ffa14d;--fxglow:rgba(255,161,77,.5);",
    " background:radial-gradient(circle at 88% 18%,rgba(255,150,60,.20) 0 16%,transparent 17%),",
    "  radial-gradient(circle at 88% 18%,rgba(255,150,60,.11) 0 30%,transparent 31%),",
    "  radial-gradient(circle at 88% 18%,rgba(255,150,60,.06) 0 45%,transparent 46%),",
    "  linear-gradient(180deg,#2c1508,#180b04);border-color:#6e3d18;border-left-color:#ffa14d;border-radius:14px;}",
    ".bwfx-mod[data-fx=phaser] .bwfx-name{font-family:Georgia,'Times New Roman',serif;font-style:italic;",
    " text-transform:none;font-size:14.5px;color:#ffb877;letter-spacing:.04em;}",
    ".bwfx-mod[data-fx=chorus]{--fxc:#7fd6ff;--fxglow:rgba(127,214,255,.5);",
    " background:linear-gradient(160deg,#132b40,#0a1622 70%);border-color:#2c4a63;border-left-color:#7fd6ff;border-radius:20px;",
    " box-shadow:0 0 20px rgba(90,170,230,.14),0 1px 0 rgba(255,255,255,.09) inset;}",
    ".bwfx-mod[data-fx=chorus] .bwfx-name{color:#b3e2ff;font-weight:400;letter-spacing:.32em;",
    " text-shadow:0 0 14px rgba(127,214,255,.65);}",
    ".bwfx-mod[data-fx=stutter]{--fxc:#e05cff;--fxglow:rgba(224,92,255,.5);",
    " background:repeating-linear-gradient(0deg,rgba(224,92,255,.06) 0 2px,transparent 2px 7px),",
    "  linear-gradient(180deg,#1d1028,#130a19);border-style:dashed;border-color:#7a4a99;border-left-color:#e05cff;",
    " border-left-style:solid;border-radius:2px;}",
    ".bwfx-mod[data-fx=stutter] .bwfx-name{font-family:ui-monospace,Menlo,monospace;color:#eda9ff;",
    " text-shadow:1px 0 0 rgba(0,255,255,.45),-1px 0 0 rgba(255,0,90,.45);}",
    ".bwfx-mod[data-fx=delay]{--fxc:#59e389;--fxglow:rgba(89,227,137,.45);",
    " background:repeating-linear-gradient(0deg,rgba(0,0,0,.28) 0 1px,transparent 1px 4px),",
    "  linear-gradient(180deg,#0f2818,#08130a);border-color:#2c5c3c;border-left-color:#59e389;border-radius:8px;}",
    ".bwfx-mod[data-fx=delay] .bwfx-name{font-family:ui-monospace,Menlo,monospace;color:#8af0ac;letter-spacing:.18em;}",
    ".bwfx-mod[data-fx=reverb]{--fxc:#9fc4ff;--fxglow:rgba(159,196,255,.5);",
    " background:radial-gradient(1px 1px at 15% 30%,rgba(255,255,255,.75) 50%,transparent 51%),",
    "  radial-gradient(1px 1px at 45% 72%,rgba(255,255,255,.5) 50%,transparent 51%),",
    "  radial-gradient(1.5px 1.5px at 76% 24%,rgba(255,255,255,.85) 50%,transparent 51%),",
    "  radial-gradient(1px 1px at 91% 62%,rgba(255,255,255,.45) 50%,transparent 51%),",
    "  radial-gradient(1px 1px at 31% 86%,rgba(255,255,255,.4) 50%,transparent 51%),",
    "  radial-gradient(ellipse at 50% 130%,#16283f 0%,#070c16 72%);border-color:#2b3f5e;border-left-color:#9fc4ff;",
    " border-radius:16px;}",
    ".bwfx-mod[data-fx=reverb] .bwfx-name{color:#c9dfff;letter-spacing:.36em;font-weight:400;}",
    /* --- SPECTRA side (characters arrive with a later BWFX update) --- */
    ".bwfx-spectra{border:1px dashed #2a3a44;border-radius:11px;min-height:180px;display:flex;",
    " flex-direction:column;align-items:center;justify-content:center;gap:10px;color:#5d7a76;",
    " background:repeating-linear-gradient(-45deg,rgba(63,224,216,.02) 0 14px,transparent 14px 28px);}",
    ".bwfx-spectra .bwfx-globe{width:30px;height:30px;color:#2c4a46;}",
    ".bwfx-spectra b{font-size:11px;letter-spacing:.3em;color:#6d8a86}",
    ".bwfx-spectra span{font-size:10.5px;letter-spacing:.08em;max-width:300px;text-align:center;line-height:1.6}",
    ".bwfx-foot{padding:8px 18px 2px;font:10px ui-monospace,Menlo,monospace;letter-spacing:.14em;color:#4d625e;",
    " display:flex;justify-content:space-between;gap:10px;flex-wrap:wrap;}"
  ].join("\n");

  var GLOBE = '<svg class="bwfx-globe" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.2">' +
    '<circle cx="8" cy="8" r="6.2"/><ellipse cx="8" cy="8" rx="6.2" ry="2.5"/>' +
    '<ellipse cx="8" cy="8" rx="2.5" ry="6.2"/></svg>';

  /* ------------------------------------------------------------- DOM build */
  var veil = null, listEl = null, mixIn = null, mixOut = null;

  function build() {
    if (built) return;
    built = true;
    var style = document.createElement("style");
    style.id = "bwfx-style";
    style.textContent = CSS;
    document.head.appendChild(style);

    veil = document.createElement("div");
    veil.className = "bwfx-veil";
    veil.innerHTML =
      '<div class="bwfx-win" role="dialog" aria-label="Brokild World FX">' +
      '  <div class="bwfx-head">' + GLOBE +
      '    <div class="bwfx-title">BROKILD WORLD FX<small>THE WORLD RACK &middot; EVERY BROKILD SYNTH, ONE RACK</small></div>' +
      '    <div class="bwfx-mixrow"><span>RACK MIX</span>' +
      '      <input id="bwfxMix" type="range" min="0" max="100" value="100">' +
      '      <output id="bwfxMixOut">100 %</output></div>' +
      '    <button class="bwfx-close" id="bwfxClose">CLOSE</button>' +
      '  </div>' +
      '  <div class="bwfx-cols">' +
      '    <div><div class="bwfx-rack-t">FX RACK</div><div class="bwfx-list" id="bwfxList"></div></div>' +
      '    <div><div class="bwfx-rack-t">SPECTRA RACK</div>' +
      '      <div class="bwfx-spectra">' + GLOBE +
      '        <b>SPECTRA</b>' +
      '        <span>The character modules join the world with a coming BWFX update &mdash; they will appear here, in every Brokild synth, on its next rebuild. The FX rack is live.</span>' +
      '      </div></div>' +
      '  </div>' +
      '  <div class="bwfx-foot"><span>BWFX ' + VERSION + '</span>' +
      '  <span>A PATCH STORES ITS OWN RACK &middot; EMPTY RACK = BIT-TRANSPARENT</span></div>' +
      '</div>';
    document.body.appendChild(veil);

    listEl = veil.querySelector("#bwfxList");
    mixIn = veil.querySelector("#bwfxMix");
    mixOut = veil.querySelector("#bwfxMixOut");

    veil.querySelector("#bwfxClose").addEventListener("click", close);
    veil.addEventListener("pointerdown", function (e) { if (e.target === veil) close(); });
    document.addEventListener("keydown", function (e) { if (e.key === "Escape" && open) close(); });

    mixIn.addEventListener("input", function () {
      state.mix = parseInt(mixIn.value, 10) / 100;
      mixOut.textContent = mixIn.value + " %";
      if (send) send({ op: "mix", v: state.mix });
    });

    renderList();
  }

  function renderList() {
    if (!listEl) return;
    listEl.innerHTML = "";
    state.order.forEach(function (id, idx) {
      var d = descOf(id);
      if (!d) return;
      var ms = state.modules[id] || { on: 0, p: {} };
      var mod = document.createElement("section");
      mod.className = "bwfx-mod" + (ms.on ? "" : " bwfx-off");
      mod.setAttribute("data-fx", id);

      var head = document.createElement("div");
      head.className = "bwfx-mhead";
      head.innerHTML =
        '<button class="bwfx-grip" type="button" aria-label="Drag ' + d.name + '">&#8942;&#8942;</button>' +
        '<button class="bwfx-mtoggle" type="button" aria-expanded="' + (ms.on ? "true" : "false") + '">' +
        '  <span class="bwfx-index">' + String(idx + 1).padStart(2, "0") + '</span>' +
        '  <span class="bwfx-name">' + d.name + '</span><span class="bwfx-sub">' + d.sub + '</span></button>' +
        '<span class="bwfx-move">' +
        '  <button class="bwfx-rocker" type="button" role="switch" aria-pressed="' + (ms.on ? "true" : "false") + '"' +
        '   aria-label="' + d.name + ' on or off" title="' + d.name + ' on or off"><span class="bwfx-lens"><i></i></span></button>' +
        '  <button class="bwfx-ud" type="button" data-up aria-label="Move ' + d.name + ' up">&#8593;</button>' +
        '  <button class="bwfx-ud" type="button" data-down aria-label="Move ' + d.name + ' down">&#8595;</button></span>';
      mod.appendChild(head);

      var ctl = document.createElement("div");
      ctl.className = "bwfx-ctl";
      if (!ms.on) ctl.hidden = true;
      d.params.forEach(function (pd) {
        var row = document.createElement("div");
        row.className = "bwfx-row" + (pd.choices ? " bwfx-wide" : "");
        var v = ms.p[pd.id] !== undefined ? ms.p[pd.id] : pd.def;
        if (pd.choices) {
          var opts = pd.choices.split("|").map(function (c, i) {
            return '<option value="' + i + '"' + (Math.round(v) === i ? " selected" : "") + ">" + c + "</option>";
          }).join("");
          row.innerHTML = "<label>" + pd.name + '</label><output></output><div class="bwfx-c"><select>' + opts + "</select></div>";
          row.querySelector("select").addEventListener("change", function (e) {
            setParamLocal(id, pd, parseInt(e.target.value, 10));
          });
        } else {
          var st = pd.step > 0 ? pd.step : ((pd.hi - pd.lo) <= 30 ? 0.1 : 1);
          row.innerHTML = "<label>" + pd.name + "</label><output>" + fmt(pd, v) + "</output>" +
            '<div class="bwfx-c"><input type="range" min="' + pd.lo + '" max="' + pd.hi + '" step="' + st + '" value="' + v + '"></div>';
          var inp = row.querySelector("input");
          var out = row.querySelector("output");
          inp.addEventListener("input", function () {
            var nv = parseFloat(inp.value);
            out.textContent = fmt(pd, nv);
            setParamLocal(id, pd, nv);
          });
        }
        ctl.appendChild(row);
      });
      mod.appendChild(ctl);

      /* head interactions */
      head.querySelector(".bwfx-mtoggle").addEventListener("click", function (e) {
        var ex = e.currentTarget.getAttribute("aria-expanded") === "true";
        e.currentTarget.setAttribute("aria-expanded", ex ? "false" : "true");
        ctl.hidden = ex;
      });
      head.querySelector(".bwfx-rocker").addEventListener("click", function (e) {
        e.stopPropagation();
        ms.on = ms.on ? 0 : 1;
        if (send) send({ op: "enable", m: id, on: ms.on });
        renderList();
      });
      head.querySelector("[data-up]").addEventListener("click", function () { move(id, -1); });
      head.querySelector("[data-down]").addEventListener("click", function () { move(id, 1); });
      bindDrag(head.querySelector(".bwfx-grip"), mod, id);

      listEl.appendChild(mod);
    });
    if (mixIn) {
      mixIn.value = Math.round(state.mix * 100);
      mixOut.textContent = Math.round(state.mix * 100) + " %";
    }
  }

  function setParamLocal(id, pd, v) {
    var ms = state.modules[id];
    if (!ms) return;
    ms.p[pd.id] = v;
    if (send) send({ op: "set", m: id, p: pd.id, v: v });
  }

  function move(id, dir) {
    var i = state.order.indexOf(id);
    var j = i + dir;
    if (i < 0 || j < 0 || j >= state.order.length) return;
    state.order[i] = state.order[j];
    state.order[j] = id;
    if (send) send({ op: "order", order: state.order.slice() });
    renderList();
  }

  /* pointer drag-to-reorder on the grip */
  function bindDrag(grip, mod, id) {
    grip.addEventListener("pointerdown", function (e) {
      e.preventDefault();
      grip.setPointerCapture(e.pointerId);
      mod.classList.add("bwfx-dragging");
      var moved = false;
      function onMove(ev) {
        moved = true;
        var els = Array.prototype.slice.call(listEl.children);
        var target = null;
        els.forEach(function (el) {
          var r = el.getBoundingClientRect();
          if (ev.clientY > r.top && ev.clientY < r.bottom && el !== mod) target = el;
        });
        if (target) {
          var r = target.getBoundingClientRect();
          if (ev.clientY < r.top + r.height / 2) listEl.insertBefore(mod, target);
          else listEl.insertBefore(mod, target.nextSibling);
        }
      }
      function onUp() {
        grip.removeEventListener("pointermove", onMove);
        grip.removeEventListener("pointerup", onUp);
        mod.classList.remove("bwfx-dragging");
        if (moved) {
          var order = Array.prototype.map.call(listEl.children, function (el) {
            return el.getAttribute("data-fx");
          });
          state.order = order;
          if (send) send({ op: "order", order: order.slice() });
        }
        renderList();
      }
      grip.addEventListener("pointermove", onMove);
      grip.addEventListener("pointerup", onUp);
    });
  }

  /* --------------------------------------------------------------- public */
  function openRack() {
    build();
    open = true;
    veil.classList.add("open");
    if (send) send({ op: "init" });     // refresh from the native truth
  }
  function close() {
    open = false;
    if (veil) veil.classList.remove("open");
  }

  window.BWFX = {
    version: VERSION,
    globeSvg: GLOBE,
    attach: function (cfg) {
      send = (cfg && cfg.send) || null;
      if (send) send({ op: "init" });
    },
    /* host -> UI: { desc:[...], state:{mix,order,modules} } (either optional) */
    onState: function (p) {
      if (!p) return;
      if (p.desc && p.desc.length) desc = p.desc;
      if (p.state) {
        var s = p.state;
        state = defaultState();
        if (typeof s.mix === "number") state.mix = s.mix;
        if (s.order && s.order.length) {
          var seen = {};
          var order = [];
          s.order.forEach(function (id) { if (descOf(id) && !seen[id]) { order.push(id); seen[id] = 1; } });
          desc.forEach(function (d) { if (!seen[d.id]) order.push(d.id); });
          state.order = order;
        }
        if (s.modules) desc.forEach(function (d) {
          var m = s.modules[d.id];
          if (!m) return;
          state.modules[d.id].on = m.on ? 1 : 0;
          d.params.forEach(function (pd) {
            if (m.p && typeof m.p[pd.id] === "number") state.modules[d.id].p[pd.id] = m.p[pd.id];
          });
        });
      }
      if (built) renderList();
    },
    open: openRack,
    close: close,
    toggle: function () { open ? close() : openRack(); },
    isOpen: function () { return open; },
    state: function () { return state; }
  };

  /* the one host button: any element carrying data-bwfx-open */
  function bindButtons() {
    document.querySelectorAll("[data-bwfx-open]").forEach(function (btn) {
      if (btn.__bwfx) return;
      btn.__bwfx = true;
      btn.addEventListener("click", function () { window.BWFX.toggle(); });
      if (!btn.querySelector(".bwfx-globe")) btn.insertAdjacentHTML("afterbegin", GLOBE);
    });
  }
  bindButtons();                       // whatever precedes us in the DOM
  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", bindButtons);   // the rest
})();
