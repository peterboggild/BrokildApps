/*  Is every installed bundle in the folder its own installer names?
 *
 *      node tools/check-groups.js
 *
 *  This exists because of a fault that is invisible to every other check:
 *  install-fleet.ps1's table says where a plug-in is PUT, and never says where
 *  it should stop being. So moving one between groups leaves the old copy
 *  exactly where it was -- as a DUPLICATE once the new one is installed, which
 *  is the worst case because both bundles carry the same class ids and which
 *  one a host loads is scan order; or simply in the WRONG FOLDER if the
 *  installer has not run since, which no duplicate scan can see.
 *
 *  Both happened on 2026-09-23 when the three collections landed: Brain Scan
 *  and High Tide ended up installed twice, Photo Synth and Legion sat in the
 *  main collection while the table said Experimental.
 *
 *  A plug-in the table does not name at all is reported but not failed, as
 *  long as the installer says so -- write "# <Name> is deliberately absent"
 *  beside the reason and this reads it. Clone Wars is the standing case: its
 *  binary comes from a CI-built zip, never a local build.
 */
"use strict";
const fs = require("fs");
const path = require("path");

const SELF = path.join(__dirname, "install-fleet.ps1");
const ROOTS = [
  ["Program Files", "C:/Program Files/Common Files/VST3/Brokild"],
  ["AudioDev",      "C:/Users/peter/AudioDev/VST3"],
];

const ps = fs.readFileSync(SELF, "utf8");

/*  what the table says */
const want = {};
for (const m of ps.matchAll(/name\s*=\s*"([^"]+)";\s*group\s*=\s*"([^"]+)"/g)) want[m[1]] = m[2];
if (!Object.keys(want).length) { console.error("REFUSING: no plug-in table found in install-fleet.ps1"); process.exit(2); }

/*  and what it says it is leaving out on purpose */
const expected = new Set();
for (const m of ps.matchAll(/#\s*(.+?)\s+is deliberately absent/g)) expected.add(m[1].trim());

let bad = 0, checked = 0;
const notes = [];

for (const [tag, root] of ROOTS) {
  if (!fs.existsSync(root)) { notes.push(`${tag}: not present on this machine`); continue; }

  /*  a group is a plain folder; a bundle is a folder ending .vst3 */
  const at = {};
  for (const d of fs.readdirSync(root, { withFileTypes: true })) {
    if (!d.isDirectory() || d.name.endsWith(".vst3")) continue;
    for (const b of fs.readdirSync(path.join(root, d.name)))
      if (b.endsWith(".vst3")) (at[b.slice(0, -5)] ||= []).push(d.name);
  }

  /*  a bundle loose at the VST3 root shadows the organised one by scan order */
  for (const d of fs.readdirSync(root, { withFileTypes: true }))
    if (d.isDirectory() && d.name.endsWith(".vst3")) {
      console.log(`  ${tag.padEnd(13)} ${d.name.slice(0, -5).padEnd(22)} *** LOOSE AT THE VST3 ROOT`);
      bad++;
    }

  for (const [name, grp] of Object.entries(want)) {
    const here = at[name] || [];
    checked++;
    if (here.length === 0) notes.push(`${tag}: ${name} is not installed (its group would be "${grp}")`);
    else if (here.length > 1) { console.log(`  ${tag.padEnd(13)} ${name.padEnd(22)} *** INSTALLED ${here.length} TIMES: ${here.join(", ")}`); bad++; }
    else if (here[0] !== grp)  { console.log(`  ${tag.padEnd(13)} ${name.padEnd(22)} *** IN "${here[0]}", TABLE SAYS "${grp}"`); bad++; }
  }

  for (const name of Object.keys(at))
    if (!want[name] && !expected.has(name)) {
      console.log(`  ${tag.padEnd(13)} ${name.padEnd(22)} *** INSTALLED BUT NOT IN THE TABLE`);
      bad++;
    }
}

for (const n of notes) console.log("  note: " + n);
console.log("");
if (bad) { console.log(`  ${bad} MISPLACED -- see above`); process.exit(1); }
console.log(`  every installed bundle is in the group its table names (${checked} checked)`);
