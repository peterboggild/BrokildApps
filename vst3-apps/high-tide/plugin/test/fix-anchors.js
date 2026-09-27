"use strict";
const BROKILD_ROOT = require("path").resolve(__dirname, "..").replace(/\\/g, "/");
const fs = require("fs");
const p = "" + BROKILD_ROOT + "/test/patch-docs.js";
let s = fs.readFileSync(p, "utf8");
const A = "  [`    <p>HIGH TIDE is a Brokild instrument. Free. Windows VST3 and standalone,`,\n" +
          "   `    <p>Every control, button, tool and timeline lane carries a hint, and the panel probe refuses\n" +
          "    to pass if one is missing: 89 of 89.</p>\n" +
          "    <p>HIGH TIDE is a Brokild instrument. Free. Windows VST3 and standalone,`]";
const A2 = "  [`    <div class=\"foot-note\">HIGH TIDE is a Brokild instrument.`,\n" +
           "   `    <p>Every control, button, tool and timeline lane carries a hint, and the panel probe refuses\n" +
           "    to pass if one is missing: 89 of 89.</p>\n" +
           "    <div class=\"foot-note\">HIGH TIDE is a Brokild instrument.`]";
const B = "  [`      Rock the terrain at the note and the sound period-doubles into chaos, an\n" +
          "      octave at a time.</p>`,\n" +
          "   `      Rock the terrain at the note and the sound period-doubles into chaos, an\n" +
          "      octave at a time. Seventeen <b>starters</b> to play straight away, and every control on the\n" +
          "      panel tells you what it is for.</p>`],";
const B2 = "  [`      into chaos, an octave at a time.</p>`,\n" +
           "   `      into chaos, an octave at a time. Seventeen <b>starters</b> to play straight away, and every\n" +
           "      control on the panel tells you what it is for.</p>`],";
const miss = [];
if (s.split(A).length !== 2) miss.push("A"); else s = s.split(A).join(A2);
if (s.split(B).length !== 2) miss.push("B"); else s = s.split(B).join(B2);
if (miss.length) { console.log("MISS " + miss.join(",")); process.exit(1); }
fs.writeFileSync(p, s, "utf8");
console.log("anchors corrected");
