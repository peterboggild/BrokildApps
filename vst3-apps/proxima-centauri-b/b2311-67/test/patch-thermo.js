/*  The thermometer strip follows an ECHOED temperature. Found by the four-
    standalone live check: the bench moved this finding's TEMPERATURE to 149 K
    (V.temp read 0.100) and the strip still said 293 K — the hostParam handler
    redrew the ring but never the thermometer. Exact-count anchor, nothing
    written on a miss.  Run:  node test/patch-thermo.js  */
const fs = require('fs');
const path = require('path');
const p = path.join(__dirname, '..', 'Source', 'ui', 'ui.html');
const raw = fs.readFileSync(p, 'utf8');
const crlf = raw.indexOf('\r\n') >= 0;
let s = crlf ? raw.split('\r\n').join('\n') : raw;
const oldS = `    onParamChanged(e.id);
    if (e.id === "habit") applyHabitVisual();
  }
});`;
const newS = `    onParamChanged(e.id);
    if (e.id === "habit") applyHabitVisual();
    if (e.id === "temp") thermoDraw();      // the bench moves this one; the strip must follow
  }
});`;
const n = s.split(oldS).length - 1;
if (n !== 1) { console.error('NOT WRITTEN: expected 1 match, found ' + n); process.exit(1); }
s = s.split(oldS).join(newS);
fs.writeFileSync(p, crlf ? s.split('\n').join('\r\n') : s, 'utf8');
console.log('patched ui.html');
