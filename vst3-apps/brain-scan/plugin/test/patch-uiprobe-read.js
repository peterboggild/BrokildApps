// the panel probe's synthetic initialState grows with the processor's vocabulary:
// three READ parameters, twelve specimen slots, six console modules.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "uiprobe.js");
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const misses = [];
const rep = (from, to, count = 1) => {
  const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
  const n = s.split(f).length - 1;
  if (n !== count) { misses.push(`expected ${count} of [${from.slice(0, 60)}...], found ${n}`); return; }
  s = s.split(f).join(t);
};
rep(String.raw`      mk("specimen","SPECIMEN",2,1,"the volume the lines read",0,8,["SINUS","SPINE","PULSE","MARROW","NERVE","RETINA","LUNG","SKULL","CORTEX"]),`,
    String.raw`      mk("specimen","SPECIMEN",2,1/11,"the volume the lines read",0,11,["SINUS","SPINE","PULSE","MARROW","NERVE","RETINA","LUNG","SKULL","CORTEX","SUTURE","ENAMEL","TENDON"]),
      mk("grain","GRAIN",0,0.35,"how sharply the tissue is read: smoothed, or texel by texel"),
      mk("contrast","CONTRAST",0,0,"the audio window: narrow it and the read saturates, a sine toward a square"),
      mk("fold","FOLD",0,0,"what the window does past its edges: clip, or fold back in"),`);
rep(String.raw`                 {name:"LUNG",gloss:"noise"},{name:"SKULL",gloss:"a shell"},{name:"CORTEX",gloss:"the brain"}],`,
    String.raw`                 {name:"LUNG",gloss:"noise"},{name:"SKULL",gloss:"a shell"},{name:"CORTEX",gloss:"the brain"},
                 {name:"SUTURE",gloss:"a staircase"},{name:"ENAMEL",gloss:"spikes"},{name:"TENDON",gloss:"partials"}],`);
rep(String.raw`    ok("the console was built from initialState", document.querySelectorAll("#console .mod").length === 5,`,
    String.raw`    ok("the console was built from initialState", document.querySelectorAll("#console .mod").length === 6,`);
rep(String.raw`    ok("every parameter reached a control", Object.keys(__BS.P).length === 30, Object.keys(__BS.P).length);`,
    String.raw`    ok("every parameter reached a control", Object.keys(__BS.P).length === 33, Object.keys(__BS.P).length);`);
if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(p, s, "utf8");
console.log("uiprobe patched for 260904.3");
