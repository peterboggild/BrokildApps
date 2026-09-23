/*  app.json's "description" is the FRONT-PAGE CARD text - the only reader is
    BrokildApps/index.html, which builds the grid from these files at runtime.
    Budget is about a hundred words: what it is in a line, then the one thing
    worth knowing. Plain text only; the renderer escapes it.                 */
const fs = require("fs");
const P = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/thin-walls/app.json";
const j = JSON.parse(fs.readFileSync(P, "utf8"));

j.description =
  "An apartment of three rooms and three doors that you put the sound inside " +
  "and then walk around in. Place up to four sources anywhere, stand where you " +
  "like, turn your head, open a door or shut it. Every arrival is a journey the " +
  "engine traced: the straight line, the bounces, the path bent round a door " +
  "frame, the muffled part through a shut leaf or the party wall, and each " +
  "room's own tail. Five surface treatments, chosen separately for walls, floor " +
  "and ceiling; two walls of every room can be splayed outward. There is no " +
  "reverb amount, because the amount is decided by where you stood.";

j.note = "Windows VST3 \u00b7 build 260921.1 \u00b7 free download \u00b7 3 rooms, 3 doors, " +
         "4 sources \u00b7 5 materials per surface, breakable walls \u00b7 measured MIT KEMAR " +
         "head \u00b7 17-page handbook included.";

const words = j.description.split(/\s+/).length;
fs.writeFileSync(P, JSON.stringify(j, null, 2) + "\n");
console.log("app.json updated - description is " + words + " words");
