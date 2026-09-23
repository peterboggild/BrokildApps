"use strict";
const fs = require("fs");
const path = require("path");
const root = "C:/Users/peter/b/Nineteen84";
const files = {};
function load(rel) { const p = path.join(root, rel); files[rel] = { p, s: fs.readFileSync(p, "utf8"), n: 0 }; }
function edit(rel, from, to, count = 1) {
  const f = files[rel];
  const parts = f.s.split(from);
  if (parts.length - 1 !== count) { console.error("ANCHOR MISS in " + rel + " (" + (parts.length - 1) + " of " + count + "):\n" + from.slice(0, 160)); process.exit(1); }
  f.s = parts.join(to); f.n++;
}
load("test/host/host.cpp"); load("Source/Patches.cpp");

// host: JUCE's VST3 wrapper emulates 16 x 130 MIDI CCs as hidden parameters plus a bypass; count ours
edit("test/host/host.cpp",
`    const int np = inst->getParameters().size();
    std::printf ("the host is offered %d parameters\\n", np);
    int automatable = 0; bool sawMacro5 = false, sawIL = false;
    for (auto* p : inst->getParameters())
    {
        const juce::String nm = p->getName (32);
        if (p->isAutomatable()) ++automatable;`,
`    /*  JUCE's VST3 wrapper emulates 16 x 130 MIDI controllers as hidden
        parameters (2080 of them) plus a Bypass, so a synth is "offered" 2212.
        A DAW hides those; so does this count. */
    int np = 0, automatable = 0; bool sawMacro5 = false, sawIL = false;
    for (auto* p : inst->getParameters())
    {
        const juce::String nm = p->getName (32);
        if (nm.startsWith ("MIDI CC") || nm == "Bypass") continue;
        ++np;
        if (p->isAutomatable()) ++automatable;`);
edit("test/host/host.cpp",
`    if (argc > 2) for (auto* p : inst->getParameters()) std::printf`,
`    std::printf ("the host is offered %d of ours (%d in all, with the emulated MIDI CCs)\\n", np, inst->getParameters().size());
    if (argc > 2) for (auto* p : inst->getParameters()) std::printf`);

// patches: basses at 8'/16' (a 32' rank at D2 is 18 Hz), brass brighter once the envelope settles
edit("Source/Patches.cpp",
`        c.rank ("a_", 100, 30, 55, 0, 60, 0, 1, 10, 0, 500, 35, 1, 0.2f, 0.9f, 8, 350, 200, 4, 300, 70, 220, 90);
        c.rank ("b_", 100, 0, 50, 0, 60, 0, 0, 10, 0, 400, 30, 1, 0.2f, 0.9f, 8, 350, 200, 4, 300, 70, 220, 65);`,
`        c.rank ("a_", 100, 30, 55, 0, 60, 0, 2, 10, 0, 500, 35, 1, 0.2f, 0.9f, 8, 350, 200, 4, 300, 70, 220, 90);
        c.rank ("b_", 100, 0, 50, 0, 60, 0, 1, 10, 0, 400, 30, 1, 0.2f, 0.9f, 8, 350, 200, 4, 300, 70, 220, 65);`);
edit("Source/Patches.cpp",
`        c.rank ("a_", 100, 0, 50, 0, 0, 0, 1, 10, 0, 350, 45, 1, 0.4f, 1.0f, 5, 260, 150, 3, 250, 60, 200, 100);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 0, 10, 0, 350, 45, 1, 0.4f, 1.0f, 5, 260, 150, 3, 250, 60, 200, 60);`,
`        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 350, 45, 1, 0.4f, 1.0f, 5, 260, 150, 3, 250, 60, 200, 100);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 1, 10, 0, 350, 45, 1, 0.4f, 1.0f, 5, 260, 150, 3, 250, 60, 200, 60);`);
edit("Source/Patches.cpp",
`        c.rank ("a_", 20, 100, 70, 0, 50, 0, 1, 10, 0, 700, 25, 0, -0.1f, 0.8f, 6, 300, 200, 3, 300, 65, 250, 90);
        c.rank ("b_", 0, 100, 60, 0, 0, 0, 0, 10, 0, 500, 20, 0, -0.1f, 0.6f, 6, 300, 200, 3, 300, 65, 250, 60);`,
`        c.rank ("a_", 20, 100, 70, 0, 50, 0, 2, 10, 0, 700, 25, 0, -0.1f, 0.8f, 6, 300, 200, 3, 300, 65, 250, 90);
        c.rank ("b_", 0, 100, 60, 0, 0, 0, 1, 10, 0, 500, 20, 0, -0.1f, 0.6f, 6, 300, 200, 3, 300, 65, 250, 60);`);
edit("Source/Patches.cpp",
`        c.rank ("a_", 100, 50, 52, 0, 80, 0, 0, 10, 0, 180, 20, 1, 0.0f, 0.3f, 3000, 8000, 5000, 2500, 1000, 100, 4000, 90);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 1, 10, 0, 300, 50, 1, 0.0f, 0.2f, 3000, 8000, 5000, 2500, 1000, 100, 4000, 55);`,
`        c.rank ("a_", 100, 50, 52, 0, 80, 0, 1, 10, 0, 180, 20, 1, 0.0f, 0.3f, 3000, 8000, 5000, 2500, 1000, 100, 4000, 90);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 300, 50, 1, 0.0f, 0.2f, 3000, 8000, 5000, 2500, 1000, 100, 4000, 55);`);
// brass: the envelope decays to the BASE cutoff, so the base must be bright enough to sustain a brass
edit("Source/Patches.cpp",
`        c.rank ("a_", 90, 30, 60, 0, 30, 0, 2, 10, 0, 1100, 20, 0, -0.3f, 0.65f, 130, 900, 400, 55, 400, 85, 500, 85);
        c.rank ("b_", 80, 0, 50, 0, 20, 0, 2, 10, 0, 900, 15, 0, -0.2f, 0.55f, 180, 1100, 400, 70, 400, 85, 500, 70);`,
`        c.rank ("a_", 90, 30, 60, 0, 30, 0, 2, 10, 0, 2600, 20, 0, -0.4f, 0.45f, 130, 900, 400, 55, 400, 85, 500, 85);
        c.rank ("b_", 80, 0, 50, 0, 20, 0, 2, 10, 0, 2200, 15, 0, -0.3f, 0.4f, 180, 1100, 400, 70, 400, 85, 500, 70);`);
edit("Source/Patches.cpp",
`        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 1500, 35, 0, -0.5f, 0.7f, 220, 1400, 600, 120, 600, 80, 700, 85);
        c.rank ("b_", 100, 45, 55, 0, 0, 0, 1, 10, 0, 1200, 30, 1, -0.4f, 0.6f, 260, 1400, 600, 160, 600, 80, 700, 75);`,
`        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 10, 0, 3000, 35, 0, -0.6f, 0.5f, 220, 1400, 600, 120, 600, 80, 700, 85);
        c.rank ("b_", 100, 45, 55, 0, 0, 0, 1, 10, 0, 2400, 30, 1, -0.5f, 0.4f, 260, 1400, 600, 160, 600, 80, 700, 75);`);
edit("Source/Patches.cpp",
`        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 40, 0, 700, 30, 1, -0.2f, 0.9f, 70, 500, 300, 25, 300, 75, 350, 85);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 2, 40, 0, 700, 30, 1, -0.2f, 0.9f, 70, 500, 300, 25, 300, 75, 350, 85);`,
`        c.rank ("a_", 100, 0, 50, 0, 0, 0, 2, 40, 0, 1800, 30, 1, -0.4f, 0.6f, 70, 500, 300, 25, 300, 75, 350, 85);
        c.rank ("b_", 100, 0, 50, 0, 0, 0, 2, 40, 0, 1800, 30, 1, -0.4f, 0.6f, 70, 500, 300, 25, 300, 75, 350, 85);`);
edit("Source/Patches.cpp",
`        c.rank ("a_", 100, 20, 58, 0, 20, 0, 2, 10, 0, 1300, 25, 0, -0.4f, 0.7f, 150, 1000, 500, 80, 500, 85, 550, 85);
        c.rank ("b_", 100, 20, 55, 0, 20, 0, 1, 10, 0, 1000, 25, 0, -0.4f, 0.6f, 200, 1000, 500, 100, 500, 85, 550, 70);`,
`        c.rank ("a_", 100, 20, 58, 0, 20, 0, 2, 10, 0, 2800, 25, 0, -0.5f, 0.45f, 150, 1000, 500, 80, 500, 85, 550, 85);
        c.rank ("b_", 100, 20, 55, 0, 20, 0, 1, 10, 0, 2200, 25, 0, -0.5f, 0.4f, 200, 1000, 500, 100, 500, 85, 550, 70);`);
edit("Source/Patches.cpp",
`        c.rank ("a_", 70, 40, 75, 0, 40, 0, 2, 10, 0, 800, 10, 0, -0.35f, 0.4f, 300, 1500, 700, 220, 500, 90, 900, 85);
        c.rank ("b_", 70, 40, 72, 0, 40, 0, 1, 10, 0, 600, 10, 0, -0.35f, 0.35f, 350, 1500, 700, 260, 500, 90, 900, 60);`,
`        c.rank ("a_", 70, 40, 75, 0, 40, 0, 2, 10, 0, 1400, 10, 0, -0.5f, 0.3f, 300, 1500, 700, 220, 500, 90, 900, 85);
        c.rank ("b_", 70, 40, 72, 0, 40, 0, 1, 10, 0, 1100, 10, 0, -0.5f, 0.25f, 350, 1500, 700, 260, 500, 90, 900, 60);`);

for (const rel in files) { fs.writeFileSync(files[rel].p, files[rel].s); console.log(rel + ": " + files[rel].n + " edits"); }
