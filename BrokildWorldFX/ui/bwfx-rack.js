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
      { id: "tone", name: "TONE", def: 72, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "phaser", name: "SWEEP", sub: "vintage 4-stage phaser", ver: 1, params: [
      { id: "mix", name: "MIX", def: 35, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "rate", name: "RATE", def: 40, lo: 5, hi: 200, step: 0, unit: "cHz" },
      { id: "depth", name: "DEPTH", def: 55, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "chorus", name: "ENSEMBLE", sub: "dual-line chorus", ver: 1, params: [
      { id: "mix", name: "MIX", def: 40, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "rate", name: "RATE", def: 45, lo: 5, hi: 300, step: 0, unit: "cHz" },
      { id: "depth", name: "DEPTH", def: 55, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "trem", name: "HARMONIC", sub: "harmonic tremolo & vibrato", ver: 1, params: [
      { id: "mode", name: "MODE", def: 0, lo: 0, hi: 2, step: 1, unit: "", choices: "HARMONIC|TREM|VIBRATO" },
      { id: "rate", name: "RATE", def: 400, lo: 5, hi: 2000, step: 0, unit: "cHz" },
      { id: "depth", name: "DEPTH", def: 55, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "sync", name: "SYNC", def: 0, lo: 0, hi: 7, step: 1, unit: "", choices: "FREE|2/1|1/1|1/2|1/4|1/8|1/16|1/32" },
      { id: "feel", name: "FEEL", def: 0, lo: 0, hi: 2, step: 1, unit: "", choices: "STRAIGHT|TRIPLET|DOTTED" },
      { id: "mix", name: "MIX", def: 100, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "stutter", name: "GATE", sub: "rhythmic stutter gate", ver: 1, params: [
      { id: "amount", name: "AMOUNT", def: 50, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "rate", name: "RATE", def: 80, lo: 10, hi: 160, step: 0, unit: "dHz" },
      { id: "sync", name: "SYNC", def: 0, lo: 0, hi: 7, step: 1, unit: "", choices: "FREE|2/1|1/1|1/2|1/4|1/8|1/16|1/32" },
      { id: "feel", name: "FEEL", def: 0, lo: 0, hi: 2, step: 1, unit: "", choices: "STRAIGHT|TRIPLET|DOTTED" }
    ] },
    { id: "lofi", name: "GRIT", sub: "sample crusher and dirt", ver: 1, params: [
      { id: "crush", name: "CRUSH", def: 25, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "noise", name: "NOISE", def: 0, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "dirt", name: "DIRT", def: 30, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "strip", name: "STRIP", sub: "compressor and 5-band EQ", ver: 1, params: [
      { id: "amount", name: "COMP", def: 35, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "attack", name: "ATTACK", def: 12, lo: 1, hi: 100, step: 0, unit: "ms" },
      { id: "release", name: "RELEASE", def: 180, lo: 20, hi: 800, step: 0, unit: "ms" },
      { id: "low", name: "80 HZ", def: 0, lo: -12, hi: 12, step: 0, unit: "dB" },
      { id: "lomid", name: "250 HZ", def: 0, lo: -12, hi: 12, step: 0, unit: "dB" },
      { id: "mid", name: "1 KHZ", def: 0, lo: -12, hi: 12, step: 0, unit: "dB" },
      { id: "himid", name: "3.5 KHZ", def: 0, lo: -12, hi: 12, step: 0, unit: "dB" },
      { id: "high", name: "10 KHZ", def: 0, lo: -12, hi: 12, step: 0, unit: "dB" },
      { id: "output", name: "OUTPUT", def: 0, lo: -12, hi: 12, step: 0, unit: "dB" }
    ] },
    { id: "delay", name: "ECHO", sub: "stereo tape echo", ver: 1, params: [
      { id: "mix", name: "MIX", def: 25, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "time", name: "TIME", def: 260, lo: 40, hi: 900, step: 0, unit: "ms" },
      { id: "feedback", name: "FEEDBACK", def: 34, lo: 0, hi: 112, step: 0, unit: "%" },
      { id: "offset", name: "OFFSET", def: 0, lo: -250, hi: 250, step: 5, unit: "ms" },
      { id: "character", name: "CHARACTER", def: 0, lo: 0, hi: 1, step: 1, unit: "", choices: "CLEAN|TAPE" },
      { id: "sync", name: "SYNC", def: 0, lo: 0, hi: 7, step: 1, unit: "", choices: "FREE|2/1|1/1|1/2|1/4|1/8|1/16|1/32" },
      { id: "feel", name: "FEEL", def: 0, lo: 0, hi: 2, step: 1, unit: "", choices: "STRAIGHT|TRIPLET|DOTTED" }
    ] },
    { id: "reverb", name: "SPACE", sub: "stereo convolution space", ver: 1, params: [
      { id: "mix", name: "MIX", def: 25, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "character", name: "CHARACTER", def: 0, lo: 0, hi: 4, step: 1, unit: "", choices: "ROOM|HALL|PLATE|SPRING|REVERSE" },
      { id: "length", name: "LENGTH", def: 180, lo: 20, hi: 600, step: 0, unit: "cs" }
    ] },
    { id: "shimmer", name: "SHIMMER", sub: "octave-up cathedral", ver: 1, params: [
      { id: "mix", name: "MIX", def: 30, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "size", name: "SIZE", def: 55, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "decay", name: "DECAY", def: 60, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "shimmer", name: "SHIMMER", def: 45, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "tone", name: "TONE", def: 55, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "rotary", name: "ROTARY", sub: "leslie cabinet, real inertia", ver: 1, params: [
      { id: "speed", name: "SPEED", def: 0, lo: 0, hi: 2, step: 1, unit: "", choices: "SLOW|FAST|BRAKE" },
      { id: "mix", name: "MIX", def: 100, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "balance", name: "BALANCE", def: 0, lo: -50, hi: 50, step: 0, unit: "" },
      { id: "growl", name: "GROWL", def: 20, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "kieranator", name: "KIERANATOR", sub: "step-sequenced havoc", ver: 1, custom: "steps", params: [
      { id: "length", name: "LENGTH", def: 0, lo: 0, hi: 1, step: 1, unit: "", choices: "1 BAR|2 BARS" },
      { id: "mix", name: "MIX", def: 100, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "repeat", name: "REPEAT", def: 1, lo: 0, hi: 2, step: 1, unit: "", choices: "1/2 STEP|1/4 STEP|1/8 STEP" },
      { id: "decay", name: "DECAY", def: 70, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "stop", name: "STOP", def: 100, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "pitch", name: "PITCH", def: 0, lo: 0, hi: 3, step: 1, unit: "", choices: "OCT DOWN|5TH DOWN|5TH UP|OCT UP" },
      { id: "crush", name: "CRUSH", def: 60, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "duty", name: "DUTY", def: 50, lo: 0, hi: 100, step: 0, unit: "%" }
    ] }
  ];

  /* Snapshot of bwfx::characterJson() for standalone use — GENERATED by
     `bwfxtest --cdesc` + scratchpad/regen-cdesc.js; never hand-typed. */
  var DEFAULT_CDESC = [
    { id: "darkdrone", name: "DARK DRONE", sub: "a cluster gone dark, sagging as it dies", ver: 1, audio: 0, params: [
      { id: "cluster", name: "CLUSTER", def: 24, lo: 0, hi: 60, step: 0, unit: "¢" },
      { id: "sag", name: "SAG", def: 25, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "drift", name: "DRIFT", def: 40, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "dtime", name: "DRIFT TIME", def: 40, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "pink", name: "PSYCHEDELIC PINK", sub: "a swirl of smeared bloom", ver: 1, audio: 1, params: [
      { id: "swirl", name: "SWIRL", def: 50, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "smear", name: "SMEAR", def: 55, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "bloom", name: "BLOOM", def: 45, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "black", name: "INDUSTRIAL BLACK", sub: "grind, chop, clang", ver: 1, audio: 1, params: [
      { id: "grind", name: "GRIND", def: 60, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "chop", name: "CHOP", def: 55, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "clang", name: "CLANG", def: 40, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "glass", name: "GLASS CATHEDRAL", sub: "the room the note prays in", ver: 1, audio: 1, params: [
      { id: "halo", name: "HALO", def: 55, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "shine", name: "SHINE", def: 60, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "tape", name: "TAPE SEANCE", sub: "the wow of a dying machine", ver: 1, audio: 0, params: [
      { id: "wobble", name: "WOBBLE", def: 45, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "sag", name: "SAG", def: 35, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "dull", name: "DULL", def: 40, lo: 0, hi: 100, step: 0, unit: "%" }
    ] },
    { id: "insect", name: "INSECT SWARM", sub: "sixteen wings, none agreeing", ver: 1, audio: 0, params: [
      { id: "swarm", name: "SWARM", def: 55, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "flutter", name: "FLUTTER", def: 60, lo: 0, hi: 100, step: 0, unit: "%" },
      { id: "skitter", name: "SKITTER", def: 40, lo: 0, hi: 100, step: 0, unit: "%" }
    ] }
  ];

  /* Built-in rack presets — GENERATED by `bwfxtest --presets`; the C++
     PRESETS table in bwfx_spectra.cpp is the source of truth. */
  var DEFAULT_PRESETS = [
    {
      "name": "VELVET STAGE",
      "blob": {
        "modules": {
          "chorus": {
            "on": 1,
            "p": {
              "mix": 30,
              "depth": 45
            }
          },
          "reverb": {
            "on": 1,
            "p": {
              "mix": 20,
              "character": 1,
              "length": 220
            }
          }
        }
      }
    },
    {
      "name": "DUB TELEGRAPH",
      "blob": {
        "modules": {
          "delay": {
            "on": 1,
            "p": {
              "mix": 30,
              "time": 340,
              "feedback": 55,
              "character": 1
            }
          },
          "lofi": {
            "on": 1,
            "p": {
              "crush": 20,
              "dirt": 40
            }
          }
        }
      }
    },
    {
      "name": "MOTOR CITY",
      "blob": {
        "modules": {
          "saturation": {
            "on": 1,
            "p": {
              "drive": 10,
              "tone": 60
            }
          },
          "rotary": {
            "on": 1,
            "p": {
              "speed": 0,
              "mix": 100,
              "growl": 30
            }
          }
        }
      }
    },
    {
      "name": "CATHEDRAL BLOOM",
      "blob": {
        "modules": {
          "shimmer": {
            "on": 1,
            "p": {
              "mix": 35,
              "size": 70,
              "decay": 70,
              "shimmer": 55
            }
          }
        },
        "spectra": {
          "glass": {
            "on": 1,
            "p": {
              "halo": 70,
              "shine": 50
            }
          }
        }
      }
    },
    {
      "name": "PINK HAZE",
      "blob": {
        "spectra": {
          "pink": {
            "on": 1,
            "p": {
              "swirl": 60,
              "smear": 60,
              "bloom": 55
            }
          }
        }
      }
    },
    {
      "name": "IRON WORKS",
      "blob": {
        "modules": {
          "strip": {
            "on": 1,
            "p": {
              "amount": 45
            }
          }
        },
        "spectra": {
          "black": {
            "on": 1,
            "p": {
              "grind": 70,
              "chop": 60,
              "clang": 45
            }
          }
        }
      }
    },
    {
      "name": "SEANCE",
      "blob": {
        "modules": {
          "reverb": {
            "on": 1,
            "p": {
              "mix": 22,
              "character": 3,
              "length": 300
            }
          }
        },
        "spectra": {
          "tape": {
            "on": 1,
            "p": {
              "wobble": 60,
              "sag": 45,
              "dull": 55
            }
          }
        }
      }
    },
    {
      "name": "THE SWARM",
      "blob": {
        "spectra": {
          "insect": {
            "on": 1,
            "p": {
              "swarm": 70,
              "flutter": 65,
              "skitter": 50
            }
          },
          "darkdrone": {
            "on": 1,
            "p": {
              "cluster": 30,
              "sag": 20,
              "drift": 50,
              "dtime": 60
            }
          }
        }
      }
    },
    {
      "name": "BROKEN TRANSMISSION",
      "blob": {
        "modules": {
          "kieranator": {
            "on": 1,
            "x": "1000300010002060",
            "p": {
              "mix": 100,
              "crush": 55
            }
          },
          "lofi": {
            "on": 1,
            "p": {
              "crush": 45,
              "dirt": 35
            }
          }
        }
      }
    },
    {
      "name": "POSSESSED CHOIR",
      "blob": {
        "modules": {
          "chorus": {
            "on": 1,
            "p": {
              "mix": 35
            }
          }
        },
        "spectra": {
          "darkdrone": {
            "on": 1,
            "p": {
              "cluster": 28
            }
          },
          "glass": {
            "on": 1,
            "p": {
              "halo": 60,
              "shine": 65
            }
          }
        }
      }
    }
  ];

  var VERSION = "1.4.0";
  var desc = DEFAULT_DESC;
  var charDesc = DEFAULT_CDESC;
  var presets = DEFAULT_PRESETS;
  var busLive = false;             // does THIS host map the world-mod bus?
  var send = null;                 // native pipe; null = standalone
  var state = defaultState();
  var open = false;
  var built = false;

  function defaultState() {
    var s = { mix: 1, order: [], modules: {}, spectra: {} };
    desc.forEach(function (d) {
      s.order.push(d.id);
      var p = {};
      d.params.forEach(function (pd) { p[pd.id] = pd.def; });
      s.modules[d.id] = { on: 0, pr: 1, p: p, x: "" };
    });
    charDesc.forEach(function (d) {
      var p = {};
      d.params.forEach(function (pd) { p[pd.id] = pd.def; });
      s.spectra[d.id] = { on: 0, pr: 1, p: p };
    });
    return s;
  }

  /* The step-grid editor (custom: "steps") — 16 cells, 8 effect brushes.
     The pattern is the module's opaque extra state: 16 chars '0'..'7'. */
  var STEPFX = [
    { c: "0", n: "·", t: "none",      col: "#2c3a42" },
    { c: "1", n: "RT",     t: "retrigger", col: "#ff9d4d" },
    { c: "2", n: "TS",     t: "tape stop", col: "#ff5533" },
    { c: "3", n: "RV",     t: "reverse",   col: "#5fa8ff" },
    { c: "4", n: "SH",     t: "shuffle",   col: "#c39bff" },
    { c: "5", n: "PT",     t: "pitch",     col: "#59e389" },
    { c: "6", n: "CR",     t: "crush",     col: "#e8d44d" },
    { c: "7", n: "GT",     t: "gate",      col: "#3fe0d8" }
  ];

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
    ".bwfx-presetrow{display:flex;align-items:center;gap:8px;margin-left:auto;font-size:10px;letter-spacing:.14em;color:#7c948f;}",
    ".bwfx-presetrow select{background:#10161b;color:#cfd8dc;border:1px solid #2a3a44;border-radius:6px;",
    " font:11px system-ui,sans-serif;padding:4px 6px;max-width:190px;}",
    ".bwfx-mixrow{display:flex;align-items:center;gap:8px;font-size:10px;letter-spacing:.14em;color:#7c948f;}",
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
    /* PRESENCE is the rack's own control: teal, whatever the pedal wears */
    ".bwfx-presrow label{color:var(--bwfx-accent,#3fe0d8);letter-spacing:.16em}",
    ".bwfx-presrow output{color:var(--bwfx-accent,#3fe0d8);text-shadow:0 0 6px rgba(63,224,216,.5)}",
    ".bwfx-presrow input[type=range]{accent-color:var(--bwfx-accent,#3fe0d8)}",
    /* --- the step grid (custom pedal editor, DISRUPTOR) --- */
    ".bwfx-steps{grid-column:1/-1;padding:4px 0 2px}",
    ".bwfx-steprow{display:grid;grid-template-columns:repeat(16,1fr);gap:3px;touch-action:none}",
    ".bwfx-step{aspect-ratio:1;border-radius:4px;border:1px solid rgba(255,255,255,.10);cursor:pointer;",
    " background:#141b20;display:flex;align-items:center;justify-content:center;",
    " font:600 8px ui-monospace,Menlo,monospace;color:#0a0d10;user-select:none;}",
    ".bwfx-step[data-q='1']{border-color:rgba(255,255,255,.22)}",
    ".bwfx-brushes{display:flex;gap:4px;flex-wrap:wrap;margin-top:6px}",
    ".bwfx-brush{border:1px solid rgba(255,255,255,.14);border-radius:5px;padding:3px 7px;cursor:pointer;",
    " font:600 9px ui-monospace,Menlo,monospace;letter-spacing:.06em;background:#10161b;color:#8fa0a8;}",
    ".bwfx-brush.on{color:#0a0d10;border-color:transparent}",
    ".bwfx-stephint{font-size:9px;letter-spacing:.1em;color:#5d7a76;margin-top:5px}",
    ".bwfx-tribute{font-size:8.5px;letter-spacing:.08em;color:#4d625e;font-style:italic;margin-top:4px}",
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
    /* --- live SPECTRA characters (Photo-Synth 2's possession units) --- */
    ".bwfx-rocker-lg{width:42px;height:52px;padding:4px;border-radius:8px}",
    ".bwfx-char .bwfx-mhead{min-height:62px}",
    ".bwfx-char .bwfx-mtoggle{text-align:left}",
    ".bwfx-char .bwfx-sub{text-transform:none;font-style:italic;letter-spacing:.06em}",
    ".bwfx-mod[data-spec=tape]{--fxc:#e0b46a;--fxglow:rgba(224,180,106,.5);",
    " background:radial-gradient(circle at 14% 30%,rgba(224,180,106,.13) 0 9px,transparent 10px),",
    "  radial-gradient(circle at 14% 30%,rgba(224,180,106,.07) 0 16px,transparent 17px),",
    "  radial-gradient(circle at 86% 30%,rgba(224,180,106,.13) 0 9px,transparent 10px),",
    "  radial-gradient(circle at 86% 30%,rgba(224,180,106,.07) 0 16px,transparent 17px),",
    "  linear-gradient(180deg,#231708,#140d04);border-color:#5e4520;border-left-color:#e0b46a;border-radius:5px;}",
    ".bwfx-mod[data-spec=tape] .bwfx-name{font-family:Georgia,'Times New Roman',serif;text-transform:none;",
    " font-size:15px;letter-spacing:.14em;color:#f0cd8e;text-shadow:0 0 10px rgba(224,180,106,.5);}",
    ".bwfx-mod[data-spec=insect]{--fxc:#b6ff45;--fxglow:rgba(182,255,69,.45);",
    " background:repeating-linear-gradient(64deg,rgba(182,255,69,.05) 0 2px,transparent 2px 9px),",
    "  repeating-linear-gradient(-64deg,rgba(182,255,69,.04) 0 2px,transparent 2px 9px),",
    "  linear-gradient(180deg,#131c07,#0a1004);border-color:#3f5c1c;border-left-color:#b6ff45;border-radius:12px;}",
    ".bwfx-mod[data-spec=insect] .bwfx-name{font-family:ui-monospace,Menlo,monospace;color:#d2ff8e;",
    " letter-spacing:.22em;text-shadow:0 0 8px rgba(182,255,69,.55);}",
    ".bwfx-mod[data-spec=darkdrone]{--fxc:#ff2440;--fxglow:rgba(255,36,64,.5);",
    " background:radial-gradient(ellipse at 50% -30%,rgba(255,36,64,.10) 0 55%,transparent 70%),",
    "  linear-gradient(180deg,#170608,#0b0304);border-color:#4a1119;border-left-color:#ff2440;border-radius:4px;}",
    ".bwfx-mod[data-spec=darkdrone] .bwfx-name{font-family:Georgia,'Times New Roman',serif;text-transform:none;",
    " font-size:15px;letter-spacing:.1em;color:#ff5c70;text-shadow:0 0 12px rgba(255,36,64,.6);}",
    ".bwfx-mod[data-spec=darkdrone] .bwfx-sub{color:#8e6b73}",
    ".bwfx-mod[data-spec=pink]{--fxc:#e07ce8;--fxglow:rgba(224,124,232,.5);",
    " background:radial-gradient(circle at 20% 25%,rgba(224,124,232,.16) 0 22%,transparent 45%),",
    "  radial-gradient(circle at 82% 70%,rgba(140,90,220,.18) 0 26%,transparent 52%),",
    "  linear-gradient(160deg,#2a1030,#160a1c);border-color:#6b3272;border-left-color:#e07ce8;border-radius:18px;}",
    ".bwfx-mod[data-spec=pink] .bwfx-name{font-family:Georgia,'Times New Roman',serif;font-style:italic;",
    " text-transform:none;font-size:15px;color:#f0aef5;letter-spacing:.05em;text-shadow:0 0 14px rgba(224,124,232,.65);}",
    ".bwfx-mod[data-spec=pink] .bwfx-sub{color:#c99ad0}",
    ".bwfx-mod[data-spec=black]{--fxc:#e8cc3c;--fxglow:rgba(232,204,60,.45);",
    " background:repeating-linear-gradient(-45deg,rgba(232,204,60,.05) 0 10px,transparent 10px 26px),",
    "  linear-gradient(180deg,#191708,#0d0c05);border-color:#8f7c1e;border-left-color:#e8cc3c;border-radius:2px;}",
    ".bwfx-mod[data-spec=black] .bwfx-name{font-family:ui-monospace,Menlo,monospace;color:#f2dd6a;",
    " letter-spacing:.3em;text-shadow:0 0 8px rgba(232,204,60,.5);}",
    ".bwfx-mod[data-spec=black] .bwfx-sub{color:#7d7a6d;letter-spacing:.12em}",
    ".bwfx-mod[data-spec=glass]{--fxc:#aee6ff;--fxglow:rgba(174,230,255,.5);",
    " background:linear-gradient(115deg,transparent 0 42%,rgba(174,230,255,.10) 46%,transparent 52%),",
    "  linear-gradient(180deg,#0e1a24,#081018);border-color:#2c4a63;border-left-color:#aee6ff;border-radius:14px;",
    " box-shadow:0 0 18px rgba(120,190,240,.10),0 1px 0 rgba(255,255,255,.08) inset;}",
    ".bwfx-mod[data-spec=glass] .bwfx-name{color:#d4f0ff;font-weight:400;letter-spacing:.34em;",
    " text-shadow:0 0 14px rgba(174,230,255,.6);}",
    ".bwfx-mod[data-spec=glass] .bwfx-sub{color:#8fb6cc}",
    ".bwfx-foot{padding:8px 18px 2px;font:10px ui-monospace,Menlo,monospace;letter-spacing:.14em;color:#4d625e;",
    " display:flex;justify-content:space-between;gap:10px;flex-wrap:wrap;}"
  ].join("\n");

  /* The canonical globe (BWFX-DESIGN.md) — the width/height attributes are
     load-bearing: without them an SVG defaults to 300x150 and blows up any
     host button it is injected into (seen live in Clone Wars 260826.1).
     Overlay CSS still overrides the size where it wants a bigger globe. */
  var GLOBE = '<svg class="bwfx-globe" viewBox="0 0 16 16" width="13" height="13" fill="none" stroke="currentColor" stroke-width="1.2">' +
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
      '    <div class="bwfx-presetrow"><span>PRESETS</span>' +
      '      <select id="bwfxPresetSel" aria-label="Built-in rack presets"></select></div>' +
      '    <div class="bwfx-mixrow"><span>RACK MIX</span>' +
      '      <input id="bwfxMix" type="range" min="0" max="100" value="100">' +
      '      <output id="bwfxMixOut">100 %</output></div>' +
      '    <button class="bwfx-close" id="bwfxClose">CLOSE</button>' +
      '  </div>' +
      '  <div class="bwfx-cols">' +
      '    <div><div class="bwfx-rack-t">FX RACK</div><div class="bwfx-list" id="bwfxList"></div></div>' +
      '    <div><div class="bwfx-rack-t">SPECTRA RACK</div>' +
      '      <div id="bwfxSpectraCol"></div></div>' +
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

    var presetSel = veil.querySelector("#bwfxPresetSel");
    populatePresets(presetSel);
    presetSel.addEventListener("change", function () {
      var i = parseInt(presetSel.value, 10);
      presetSel.value = "";                       // an action, not a state
      if (isNaN(i) || !presets[i]) return;
      applyPreset(presets[i]);
    });

    mixIn.addEventListener("input", function () {
      state.mix = parseInt(mixIn.value, 10) / 100;
      mixOut.textContent = mixIn.value + " %";
      if (send) send({ op: "mix", v: state.mix });
    });

    renderList();
    renderSpectra();
  }

  function populatePresets(sel) {
    if (!sel) return;
    sel.innerHTML = '<option value="" selected>&mdash;</option>' +
      presets.map(function (p, i) { return '<option value="' + i + '">' + p.name + "</option>"; }).join("");
  }

  /* Apply a preset. Native: hand the blob to the rack (op "blob") — the state
     echo then updates the whole overlay. Standalone: merge it locally. */
  function applyPreset(p) {
    if (send) { send({ op: "blob", j: JSON.stringify(p.blob) }); return; }
    applyBlobLocal(p.blob);
    renderList();
    renderSpectra();
  }

  function applyBlobLocal(b) {
    state = defaultState();
    if (!b) return;
    if (typeof b.mix === "number") state.mix = b.mix;
    if (b.order && b.order.length) {
      var seen = {}, order = [];
      b.order.forEach(function (id) { if (descOf(id) && !seen[id]) { order.push(id); seen[id] = 1; } });
      desc.forEach(function (d) { if (!seen[d.id]) order.push(d.id); });
      state.order = order;
    }
    if (b.modules) desc.forEach(function (d) {
      var src = b.modules[d.id], ms = state.modules[d.id];
      if (!src || !ms) return;
      ms.on = src.on ? 1 : 0;
      if (typeof src.pr === "number") ms.pr = src.pr;
      if (typeof src.x === "string") ms.x = src.x;
      if (src.p) d.params.forEach(function (pd) {
        if (typeof src.p[pd.id] === "number") ms.p[pd.id] = src.p[pd.id];
      });
    });
    if (b.spectra) charDesc.forEach(function (d) {
      var src = b.spectra[d.id], cs = state.spectra[d.id];
      if (!src || !cs) return;
      cs.on = src.on ? 1 : 0;
      if (typeof src.pr === "number") cs.pr = src.pr;
      if (src.p) d.params.forEach(function (pd) {
        if (typeof src.p[pd.id] === "number") cs.p[pd.id] = src.p[pd.id];
      });
    });
  }

  /* The SPECTRA rack. Audio characters (pink, black, glass) carry their own
     DSP inside the rack, so they are live in EVERY host; the pure modulators
     need the host's engine on the world-mod bus and appear only there. */
  function renderSpectra() {
    var col = veil && veil.querySelector("#bwfxSpectraCol");
    if (!col) return;
    col.innerHTML = "";
    var visible = charDesc.filter(function (d) { return busLive || d.audio; });
    if (!visible.length) {
      var plate = document.createElement("div");
      plate.className = "bwfx-spectra";
      plate.innerHTML = GLOBE + "<b>SPECTRA</b>" +
        "<span>The character modules arrive for this synth with a coming BWFX update &mdash; " +
        "its engine does not yet listen to the world-modulation bus. The FX rack is live.</span>";
      col.appendChild(plate);
      return;
    }
    var list = document.createElement("div");
    list.className = "bwfx-list";
    visible.forEach(function (d) {
      if (!state.spectra[d.id]) {
        var pdef = {};
        d.params.forEach(function (pd) { pdef[pd.id] = pd.def; });
        state.spectra[d.id] = { on: 0, pr: 1, p: pdef };
      }
      var cs = state.spectra[d.id];
      var unit = document.createElement("section");
      unit.className = "bwfx-mod bwfx-char" + (cs.on ? "" : " bwfx-off");
      unit.setAttribute("data-spec", d.id);

      var head = document.createElement("div");
      head.className = "bwfx-mhead";
      head.innerHTML =
        '<button class="bwfx-rocker bwfx-rocker-lg" type="button" role="switch" aria-pressed="' + (cs.on ? "true" : "false") + '"' +
        ' aria-label="' + d.name + ' armed" title="Arm ' + d.name + '"><span class="bwfx-lens"><i></i></span></button>' +
        '<button class="bwfx-mtoggle" type="button" aria-expanded="' + (cs.on ? "true" : "false") + '">' +
        '  <span class="bwfx-name">' + d.name + '</span><span class="bwfx-sub">' + d.sub + '</span></button>';
      unit.appendChild(head);

      var ctl = document.createElement("div");
      ctl.className = "bwfx-ctl";
      if (!cs.on) ctl.hidden = true;

      (function () {   // PRESENCE = arm strength, the morph lever
        var row = document.createElement("div");
        row.className = "bwfx-row bwfx-presrow";
        var v = Math.round((typeof cs.pr === "number" ? cs.pr : 1) * 100);
        row.innerHTML = "<label>PRESENCE</label><output>" + v + " %</output>" +
          '<div class="bwfx-c"><input type="range" min="0" max="100" step="1" value="' + v + '"></div>';
        var inp = row.querySelector("input"), out = row.querySelector("output");
        inp.addEventListener("input", function () {
          out.textContent = inp.value + " %";
          cs.pr = parseInt(inp.value, 10) / 100;
          if (send) send({ op: "cpresence", m: d.id, v: cs.pr });
        });
        ctl.appendChild(row);
      })();

      d.params.forEach(function (pd) {
        var row = document.createElement("div");
        row.className = "bwfx-row";
        var v = cs.p[pd.id] !== undefined ? cs.p[pd.id] : pd.def;
        row.innerHTML = "<label>" + pd.name + "</label><output>" + fmt(pd, v) + "</output>" +
          '<div class="bwfx-c"><input type="range" min="' + pd.lo + '" max="' + pd.hi + '" step="1" value="' + v + '"></div>';
        var inp = row.querySelector("input"), out = row.querySelector("output");
        inp.addEventListener("input", function () {
          var nv = parseFloat(inp.value);
          out.textContent = fmt(pd, nv);
          cs.p[pd.id] = nv;
          if (send) send({ op: "cset", m: d.id, p: pd.id, v: nv });
        });
        ctl.appendChild(row);
      });
      unit.appendChild(ctl);

      head.querySelector(".bwfx-rocker").addEventListener("click", function (e) {
        e.stopPropagation();
        cs.on = cs.on ? 0 : 1;
        if (send) send({ op: "cenable", m: d.id, on: cs.on });
        renderSpectra();
      });
      head.querySelector(".bwfx-mtoggle").addEventListener("click", function (e) {
        var ex = e.currentTarget.getAttribute("aria-expanded") === "true";
        e.currentTarget.setAttribute("aria-expanded", ex ? "false" : "true");
        ctl.hidden = ex;
      });

      list.appendChild(unit);
    });
    col.appendChild(list);

    var note = document.createElement("div");
    note.className = "bwfx-stephint";
    note.textContent = "CHARACTERS POSSESS THE SYNTH ITSELF · ARM ORDER LAYERS · PRESENCE IS THE GRIP";
    col.appendChild(note);
    if (!busLive) {
      var more = document.createElement("div");
      more.className = "bwfx-stephint";
      more.textContent = "DARK DRONE · TAPE SEANCE · INSECT SWARM JOIN WHEN THIS ENGINE TAKES THE BUS";
      col.appendChild(more);
    }
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

      // PRESENCE — the rack's own per-module dry/wet (teal: a world control,
      // not one of the pedal's knobs). It is also what a patch morph rides.
      (function () {
        var row = document.createElement("div");
        row.className = "bwfx-row bwfx-presrow";
        var v = Math.round((typeof ms.pr === "number" ? ms.pr : 1) * 100);
        row.innerHTML = "<label>PRESENCE</label><output>" + v + " %</output>" +
          '<div class="bwfx-c"><input type="range" min="0" max="100" step="1" value="' + v + '"></div>';
        var inp = row.querySelector("input");
        var out = row.querySelector("output");
        inp.addEventListener("input", function () {
          var nv = parseInt(inp.value, 10);
          out.textContent = nv + " %";
          ms.pr = nv / 100;
          if (send) send({ op: "presence", m: id, v: ms.pr });
        });
        ctl.appendChild(row);
      })();

      // custom pedal editor: the drawable step grid (DISRUPTOR)
      if (d.custom === "steps") (function () {
        var wrap = document.createElement("div");
        wrap.className = "bwfx-steps";
        var pat = typeof ms.x === "string" ? ms.x : "";
        while (pat.length < 16) pat += "0";
        pat = pat.slice(0, 16);
        var brush = "1";

        var grid = document.createElement("div");
        grid.className = "bwfx-steprow";
        var cells = [];
        function paintCell(k) {
          var c = pat[k];
          var fx = STEPFX[parseInt(c, 10)] || STEPFX[0];
          cells[k].style.background = c === "0" ? "#141b20" : fx.col;
          cells[k].textContent = c === "0" ? "" : fx.n;
          cells[k].setAttribute("data-q", (k % 4 === 0) ? "1" : "0");
        }
        function setCell(k, c) {
          if (pat[k] === c) return;
          pat = pat.slice(0, k) + c + pat.slice(k + 1);
          ms.x = pat === "0000000000000000" ? "" : pat;
          paintCell(k);
          if (send) send({ op: "extra", m: id, x: pat });
        }
        for (var k = 0; k < 16; ++k) (function (k) {
          var cell = document.createElement("div");
          cell.className = "bwfx-step";
          cells.push(cell);
          grid.appendChild(cell);
          cell.addEventListener("pointerdown", function (e) {
            e.preventDefault();
            setCell(k, pat[k] === brush ? "0" : brush);   // same brush = clear
          });
          cell.addEventListener("pointerenter", function (e) {
            if (e.buttons) setCell(k, brush);             // drag paints
          });
          paintCell(k);
        })(k);
        for (var k2 = 0; k2 < 16; ++k2) paintCell(k2);
        wrap.appendChild(grid);

        var brushes = document.createElement("div");
        brushes.className = "bwfx-brushes";
        STEPFX.slice(1).forEach(function (fx) {
          var b = document.createElement("div");
          b.className = "bwfx-brush" + (fx.c === brush ? " on" : "");
          b.textContent = fx.n + " " + fx.t.toUpperCase();
          if (fx.c === brush) b.style.background = fx.col;
          b.addEventListener("click", function () {
            brush = fx.c;
            brushes.querySelectorAll(".bwfx-brush").forEach(function (x, i) {
              var f = STEPFX[i + 1];
              x.classList.toggle("on", f.c === brush);
              x.style.background = f.c === brush ? f.col : "";
            });
          });
          brushes.appendChild(b);
        });
        wrap.appendChild(brushes);

        var hint = document.createElement("div");
        hint.className = "bwfx-stephint";
        hint.textContent = "PICK A BRUSH · PAINT THE BAR · SAME BRUSH AGAIN CLEARS A STEP";
        wrap.appendChild(hint);

        if (id === "kieranator") {           // Peter may retire this line later
          var trib = document.createElement("div");
          trib.className = "bwfx-tribute";
          trib.textContent = "Inspired by the late Kieran Foster" + String.fromCharCode(8217) + "s legendary VST, Glitch 2.";
          wrap.appendChild(trib);
        }
        ctl.appendChild(wrap);
      })();

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
      if (p.cdesc && p.cdesc.length) charDesc = p.cdesc;
      if (p.presets && p.presets.length) {
        presets = p.presets;
        if (built) populatePresets(veil.querySelector("#bwfxPresetSel"));
      }
      if (typeof p.busLive === "boolean") busLive = p.busLive;
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
          if (typeof m.pr === "number") state.modules[d.id].pr = m.pr;
          if (typeof m.x === "string") state.modules[d.id].x = m.x;
          d.params.forEach(function (pd) {
            if (m.p && typeof m.p[pd.id] === "number") state.modules[d.id].p[pd.id] = m.p[pd.id];
          });
        });
        if (s.spectra) charDesc.forEach(function (d) {
          var c = s.spectra[d.id];
          if (!c || !state.spectra[d.id]) return;
          state.spectra[d.id].on = c.on ? 1 : 0;
          if (typeof c.pr === "number") state.spectra[d.id].pr = c.pr;
          d.params.forEach(function (pd) {
            if (c.p && typeof c.p[pd.id] === "number") state.spectra[d.id].p[pd.id] = c.p[pd.id];
          });
        });
      }
      if (built) { renderList(); renderSpectra(); }
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
