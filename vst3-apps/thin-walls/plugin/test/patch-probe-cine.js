"use strict";
const fs = require("fs"), path = require("path");
const P = path.join(__dirname, "uiprobe.js");
let s = fs.readFileSync(P, "utf8");
const part = fs.readFileSync(path.join(__dirname, "probe-cine-export.part.js"), "utf8");
if (part.indexOf(String.fromCharCode(92)) >= 0 || part.indexOf("${") >= 0) { console.error("part has a backslash or ${"); process.exit(1); }
const a = "      TW.render();\n      idleCheck();\n", b = "  /* --- 18 furniture ---";
if (s.split(a).length !== 2 || s.split(b).length !== 2) { console.error("anchors"); process.exit(1); }
s = s.replace(a, "      TW.render();\n      cineAndExportChecks(idleCheck);\n").replace(b, part + b);
fs.writeFileSync(P, s); console.log("ok");
