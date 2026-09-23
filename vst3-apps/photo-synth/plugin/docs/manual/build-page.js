const BROKILD_ROOT = require("path").resolve(__dirname, "..").replace(/\\/g, "/");
/* Rebuild the manual's screenshot page from the current plugin UI:
   the native stub, plus a __M hook the job setups drive. */
const fs = require("fs");
const SRC = "" + BROKILD_ROOT + "/Source/ui/ui.html";
const STUB = process.argv[2];
const OUT = process.argv[3];

let s = fs.readFileSync(SRC, "utf8");
const stub = fs.readFileSync(STUB, "utf8");

const a = '<script>\r\n(function () {\r\n  "use strict";';
if (s.split(a).length - 1 !== 1) throw new Error("app script anchor");
s = s.replace(a, stub + a);

const b = '\r\n  document.addEventListener("visibilitychange"';
if (s.split(b).length - 1 !== 1) throw new Error("hook anchor");
s = s.replace(b,
  '\r\n  window.__M = { S: S, pads: pads, padTone: padTone, padFilter: padFilter, padEnv: padEnv,' +
  ' padFx: padFx, padOf: padOf, setMode: setMode, apply: apply, setLatch: setLatch,' +
  ' showBuildId: showBuildId, loadDemo: loadDemo, updateReadouts: updateReadouts,' +
  ' startAuto: startAuto, updateMotionUI: updateMotionUI, setPadPaused: setPadPaused,' +
  ' specToggle: specToggle, motionIn: motionIn, applyFactory: applyFactory };' + b);

fs.writeFileSync(OUT, s);
console.log("page.html rebuilt from ui.html (" + s.length + " bytes)");
