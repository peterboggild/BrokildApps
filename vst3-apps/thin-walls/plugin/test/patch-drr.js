/*  The DRR check had two faults of its own, both found by measurement after the
    interpolator fix raised the direct path to its true level:

      1. It integrated a 3.4 s decay over a 3.0 s take, so the reverberant side
         was short by whatever fell off the end. The vacuous-window family again.
      2. It computed the critical distance from S x alpha, while the engine's own
         absorption also counts the doors and the party walls - a larger A, so a
         larger true critical distance, so a positive bias at the bench's one.

    And the target was wrong: "0 dB at the critical distance" assumes an
    omnidirectional receiver. A real head hears a source straight ahead +1.04 dB
    louder than the same power arriving diffusely (measured: twprobe hrtfbal),
    and the engine's eight diffuse directions are themselves +0.16 dB above the
    sphere average, so the figure to expect is +0.9 dB.
*/
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "bench.cpp");
let s = fs.readFileSync(p, "utf8");
const misses = [];
const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(c + " matches for: " + a.slice(0, 80)); return; } s = s.split(a).join(b); };

rep(`            const int r = 0; const Room& R = ROOMS[r];
            float ey[NBAND]; Engine::eyringRt60 (r, p.material, p.door, ey);
            // Sabine area from the material (doors shut, walls transmit almost nothing)
            const float A = R.surface() * MATERIAL_ALPHA[p.material[r]][3];
            const double rc = std::sqrt (A / (16.0 * 3.14159265));`,
`            const int r = 0;
            float ey[NBAND]; Engine::eyringRt60 (r, p.material, p.door, ey);
            /*  The engine's OWN absorption area, not S x alpha: it also counts the
                doors and what the party walls pass, which makes A larger and the
                critical distance longer. Taking the smaller one put every reading
                on the wrong side of the target. */
            float A = 0;
            { Engine* q = fresh (fs); q->setParams (p); A = q->field (r).absorptionArea[3]; delete q; }
            const double rc = std::sqrt (A / (16.0 * 3.14159265));`);

rep(`                Take t = render (*e, p, fs, 3.0, impulseAt (2400));`,
`                // long enough to hold the whole decay: a 3 s take cannot measure
                // the reverberant energy of a 3.4 s tail
                Take t = render (*e, p, fs, std::max (3.0, 2.0 * (double) ey[3] + 1.0), impulseAt (2400));`);

rep(`            char buf[200]; std::snprintf (buf, sizeof buf, "large %s: DRR at the critical distance %.2f m is %.1f dB (0 expected), %.1f at half, %.1f at double", MATERIAL_NAMES[m], rc, drr[1], drr[0], drr[2]);
            // reflective rooms put the critical distance inside the near field (0.27 m) where the
            // head itself is a fraction of the distance; 3.5 dB there is the honest bound
            check (std::abs (drr[1]) < 3.5 && drr[0] > drr[1] + 3.5 && drr[2] < drr[1] - 3.5, buf);`,
`            /*  +0.9 dB, not 0: a head hears a frontal source that much louder than
                the same power arriving diffusely. Below about half a metre the
                critical distance is inside the near field - the head is a third of
                the way to the source and neither 1/r nor a far-field HRTF applies -
                so there only the 6 dB per doubling is asserted. */
            const bool farField = rc > 0.5;
            char buf[240];
            std::snprintf (buf, sizeof buf, "large %s: DRR at the critical distance %.2f m is %+.1f dB (%s), %+.1f at half, %+.1f at double",
                           MATERIAL_NAMES[m], rc, drr[1], farField ? "+0.9 expected, the head's own frontal gain" : "near field, value not asserted", drr[0], drr[2]);
            const bool slope = drr[0] > drr[1] + 3.5 && drr[2] < drr[1] - 3.5;
            check (slope && (! farField || std::abs (drr[1] - 0.9) < 2.5), buf);`);

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
fs.writeFileSync(p, s);
console.log("DRR check repaired");
