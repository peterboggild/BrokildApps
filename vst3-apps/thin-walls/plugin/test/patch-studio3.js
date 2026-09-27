/*  The first STUDIO table was guessed and measured 1.37-1.45x of spread across
    250 Hz - 4 kHz, RISING with frequency: over-tapered, bright-decaying.

    Solved instead. For a flat RT60 the whole Eyring denominator must be constant:
        -S ln(1 - alpha_b) + 4 m_b V = D,   D = 0.161 V / RT
    so  alpha_b = 1 - exp (-(D - 4 m_b V) / S).
    Taking the hall (V 540, S 426) at RT 0.70 s gives 0.2525 0.2519 0.2508 0.2489
    0.2452 0.2331 0.1884 - which is to say CONSTANT absorption with a little less
    at the very top, exactly what broadband absorbers plus bass traps are, the
    top-end relief paying for air absorption. The same table then lands the living
    room at 0.385 s and the box room at 0.274 s, both within a twentieth of flat.

    Also: the diffusion check measured the early window with the late field left
    in, which buried the difference it was looking for (20.2 % against 22.0 %).
    Trim the reverb and it measures the images alone.
*/
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(rel + ": " + c + " for " + a.slice(0, 70)); return; } s = s.split(a).join(b); };
  fn(rep);
  if (misses.length === 0) fs.writeFileSync(p, s);
}

edit("Source/Engine.cpp", rep => {
  rep(`    /*  STUDIO: bass traps at the bottom, restraint at the top. The gentle fall
        with frequency is what pays for air absorption, so the decay comes out
        flat across the band instead of bass-heavy. About 0.7 s in the hall and
        0.4 s in the living room - treated, still live. */
    { 0.32f, 0.30f, 0.28f, 0.26f, 0.24f, 0.21f, 0.18f },`,
`    /*  STUDIO. Not guessed: solved from Eyring for a decay that does not move
        across the band, -S ln(1-alpha) + 4mV held constant at 0.161 V / 0.70 s
        for the hall. It comes out as CONSTANT absorption near a quarter, with a
        little relief at the very top to pay for air absorption - which is what
        broadband absorbers and bass traps are. 0.70 s in the hall, 0.39 in the
        living room, 0.27 in the box room: treated, and still live. */
    { 0.252f, 0.252f, 0.251f, 0.249f, 0.245f, 0.233f, 0.188f },`);
});

edit("test/bench.cpp", rep => {
  rep(`            check (spreadS < 1.35 && spreadS < spreadT * 0.6, buf);`,
      `            check (spreadS < 1.15 && spreadS < spreadT * 0.6, buf);`);
  rep(`                p.src[0].x = 2.0f; p.src[0].y = 1.5f; p.src[0].z = EAR_HEIGHT;
                p.lisX = 4.0f; p.lisY = 3.0f; p.lisYaw = 200.0f;
                Take t = render (*e, p, fs, 4.0, impulseAt (2400));`,
      `                p.src[0].x = 2.0f; p.src[0].y = 1.5f; p.src[0].z = EAR_HEIGHT;
                p.lisX = 4.0f; p.lisY = 3.0f; p.lisYaw = 200.0f;
                p.reverbDb = -80;                  // the images alone, or the field buries the difference
                Take t = render (*e, p, fs, 4.0, impulseAt (2400));`);
  rep(`            std::snprintf (buf, sizeof buf, "diffusion: the early specular window holds %.1f %% of the energy in STUDIO against %.1f %% in PLASTER, and the room is still live",
                           100.0 * eS / tS, 100.0 * eP / tP);
            check (eS / tS < eP / tP, buf);`,
      `            std::snprintf (buf, sizeof buf, "diffusion: the specular reflections carry %.1f %% of what PLASTER's carry, the rest scattered into the room rather than absorbed",
                           100.0 * eS / eP);
            check (eS < 0.75 * eP, buf);`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("studio round 3 applied");
