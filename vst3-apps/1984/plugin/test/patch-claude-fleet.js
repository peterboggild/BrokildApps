"use strict";
const fs = require("fs");
const p = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const BT = "`";
const anchor = "| **Thin Walls** | " + BT + "b\\ThinWalls" + BT + " | " + BT + "brokild-thin-walls" + BT + " |" + NL;
const parts = s.split(anchor);
if (parts.length !== 2) { console.error("anchor miss: " + (parts.length - 1)); process.exit(1); }
const row = "| **1984** | " + BT + "b\\Nineteen84" + BT + " (PRODUCT_NAME 1984; a target cannot start with a digit) | " + BT + "brokild-1984" + BT + " |" + NL;
s = parts.join(anchor + row);
fs.writeFileSync(p, s);
console.log("fleet row added");
