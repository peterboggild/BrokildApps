"use strict";
const fs = require("fs"), path = require("path");
const P = path.join(__dirname, "uiprobe.js");
let s = fs.readFileSync(P, "utf8");
const part = fs.readFileSync(path.join(__dirname, "probe-v5.part.js"), "utf8");
if (part.indexOf(String.fromCharCode(92)) >= 0 || part.indexOf("${") >= 0 || part.indexOf("`") >= 0){ console.error("part has a backslash, ${ or a backtick"); process.exit(1); }
const ed = [["      cineAndExportChecks(idleCheck);\n", "      cineAndExportChecks(function (){ v5Checks(idleCheck); });\n"],
            ["  /* --- 18 furniture ---", part + "  /* --- 18 furniture ---"],
            ['"--virtual-time-budget=14000",', '"--virtual-time-budget=24000",']];
for (const [a] of ed) if (s.split(a).length !== 2){ console.error("anchor: " + a.slice(0, 40)); process.exit(1); }
for (const [a, b] of ed) s = s.replace(a, () => b);
fs.writeFileSync(P, s); console.log("ok");
