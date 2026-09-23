/*  Give more presets a journey.
 *
 *  Ten of the forty-eight shipped scenes. These sixteen are the ones where a
 *  journey is the point of the patch: the eight DRONES, the seven RESTRAINED
 *  BEDS and SWARM. Deliberately NOT touched:
 *
 *      RESONANT GESTURE  a gesture is over in seconds; a thirty-minute arc
 *                        through it would describe something nobody hears.
 *      PLAYABLE BASS     played, not left running.
 *      DRY               these exist to be bare. A journey is the opposite of
 *                        what they are for.
 *
 *  Scenes are SPARSE: each names only the handful of parameters that travel,
 *  and everything else stays wherever the preset put it. So a journey can only
 *  move what it names, which is what makes adding one safe.
 *
 *  The three arcs, one per family, chosen to suit what the family is:
 *      DRONE           a slab that erodes and widens as it goes.
 *      RESTRAINED BED  stays quiet throughout; the light on it changes.
 *      EVOLVING WORLD  the events grow denser, then the coupling takes over.
 */
"use strict";
const fs = require("fs");
const path = require("path");

const FILE = path.join(__dirname, "..", "Source", "Presets.cpp");

const ARCS = {
  drone: [
    "e_fb_send=0.0 mem_erosion=0.05 l_coupling=0.2",
    "e_fb_send=0.1 l_coupling=0.4 m_drift=0.35",
    "e_fb_send=0.2 mem_on=1 mem_gain=0.22 mem_erosion=0.4 e_distance=0.45",
    "e_fb_send=0.12 mem_erosion=0.7 e_scale=0.8 l_coupling=0.65",
  ],
  bed: [
    "l_coupling=0.15 e_rv_mod=0.1 mem_erosion=0.05",
    "l_coupling=0.4 e_rv_mod=0.3 m_drift=0.3",
    "mem_on=1 mem_gain=0.18 e_distance=0.5 l_coupling=0.55",
    "mem_erosion=0.5 e_scale=0.75 l_coupling=0.7 e_distance=0.35",
  ],
  world: [
    "v1_prob=0.15 l_coupling=0.3",
    "v1_prob=0.45 l_coupling=0.55 m_drift=0.4",
    "v1_prob=0.75 e_fb_send=0.2 st_couple=0.4",
    "v1_prob=0.4 l_coupling=0.8 mem_on=1 mem_gain=0.25",
  ],
};

const TARGETS = [
  ["BENEATH THIRTY KILOMETRES OF CONCRETE", "drone"], ["THE SEA IS MADE OF IRON", "drone"],
  ["ORBITAL DEBRIS CHOIR", "drone"], ["STEEL CATHEDRAL", "drone"],
  ["ABANDONED SUBSTATION", "drone"], ["HOLLOW EARTH", "drone"],
  ["COOLING TOWERS", "drone"], ["MONOLITH", "drone"],
  ["A CITY WITHOUT WITNESSES", "bed"], ["POPULATION: ZERO", "bed"],
  ["THE SUN BEHIND THE ASH", "bed"], ["GRANITE PAD", "bed"],
  ["VOICES UNDER THE ICE", "bed"], ["LAST LIGHT IN THE TOWER", "bed"],
  ["THE DEEP FIELD", "bed"],
  ["SWARM", "world"],
];

let src = fs.readFileSync(FILE, "utf8");
const misses = [], plans = [];

for (const [name, arc] of TARGETS) {
  const key = '{ "' + name + '"';
  const i = src.indexOf(key);
  if (i < 0) { misses.push(name + " -- not found"); continue; }
  let j = src.indexOf('\n{ "', i + 1);
  if (j < 0) j = src.length;
  const body = src.slice(i, j);
  if (/#SCENE/.test(body)) { misses.push(name + " -- already has scenes"); continue; }
  if (!ARCS[arc]) { misses.push(name + " -- no such arc: " + arc); continue; }

  /*  the entry ends with the last string literal then `" },` -- append the
      scene block as its own literal just before that close */
  const close = body.lastIndexOf('" },');
  if (close < 0) { misses.push(name + " -- cannot find the end of the entry"); continue; }

  const scenes = ARCS[arc].map((s, k) => "#SCENE " + (k + 1) + " " + s + "\\n").join("");
  /*  arm it, and give it a long traversal: these are pieces, not gestures */
  const armed = body.slice(0, close + 1) + '\n  "h_on=1 h_dur=0.8\\n"\n  "' + scenes + '" },'
                + body.slice(close + 4);
  plans.push({ i, j, name, arc, text: armed });
}

if (misses.length) {
  console.error("REFUSING -- nothing written:");
  for (const m of misses) console.error("  " + m);
  process.exit(1);
}

/*  apply from the back so the earlier offsets stay valid */
plans.sort((a, b) => b.i - a.i);
for (const p of plans) src = src.slice(0, p.i) + p.text + src.slice(p.j);
fs.writeFileSync(FILE, src);

for (const p of plans.slice().reverse()) console.log("  " + p.arc.padEnd(7) + p.name);
console.log("\n  " + plans.length + " presets given a journey");
