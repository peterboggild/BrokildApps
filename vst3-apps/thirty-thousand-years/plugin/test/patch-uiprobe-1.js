// Two probe faults, both mine:
//   1. frame() waited on requestAnimationFrame, which does not advance under
//      Chrome's --virtual-time-budget, so the driver never got past the first
//      await and produced no report at all. A timeout does advance.
//   2. "the page carries no copy of the parameter list" matched m_o1wave",
//      which is the LAYOUT table naming where an id is drawn - exactly what the
//      page is supposed to do. What it must not carry is a parameter's DEFAULT,
//      RANGE or LIST NAMES; those arrive in initialState.
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/test/uiprobe.js";
let s = fs.readFileSync(p, "utf8");
const edits = [
  ['  function frame() { return new Promise(function (r) { requestAnimationFrame(function () { requestAnimationFrame(r); }); }); }',
   '  /*  A frame under --virtual-time-budget: rAF does NOT advance there, so this\n' +
   '      waits on a timeout, which does. The page draws from its own rAF where it\n' +
   '      has one, so anything measured after this has had a chance to lay out. */\n' +
   '  function frame() { return tick(24); }'],
  ['  add("the page carries no copy of the parameter list",\n      !/TTY_PARAMS|KP_[A-Z]+/.test(page) && page.indexOf("m_o1wave\\"") < 0);',
   '  /*  The page NAMES ids (that is placement, its job); what it must not carry is\n' +
   '      a parameter\'s default, range or list names - those arrive in initialState. */\n' +
   '  add("the page carries no copy of the parameter table",\n' +
   '      !/TTY_PARAMS|KP_[A-Z]+|numParams/.test(page) &&\n' +
   '      page.indexOf("SAMPLE & HOLD") < 0 && page.indexOf("PENTATONIC") < 0 && page.indexOf("REVERSE EXPO") < 0,\n' +
   '      (/TTY_PARAMS|KP_[A-Z]+|numParams|SAMPLE & HOLD|PENTATONIC|REVERSE EXPO/.exec(page) || [""])[0]);'],
];
let miss = [];
for (const [a] of edits) if (s.split(a).length !== 2) miss.push(a.slice(0, 60));
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(p, s);
console.log("uiprobe patched");
