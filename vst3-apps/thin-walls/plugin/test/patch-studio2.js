/*  Two things:

    1. The splay guard `max (0.2f, len + dLen)` was meant to stop a perturbation
       driving a path length negative, but it also floored GENUINE short paths -
       a source 0.135 m from the head read as 0.2 m, which is -3.4 dB. Measured
       as TILED's half-critical-distance reading falling from +9.1 to +5.6 dB.
       Now the floor applies only where there IS a perturbation, so every
       material with splay 0 is arithmetically untouched.

    2. Section 8 of the bench: what makes STUDIO a studio, each claim measured.
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
  rep(`                    finishSpec (s, arriveFrom, departTo, std::max (0.2f, len + dLen));`,
      `                    // the floor guards the PERTURBATION only: a genuinely short path
                    // must keep its own length, or a source held close to the head
                    // quietly loses level
                    finishSpec (s, arriveFrom, departTo, dLen != 0.0f ? std::max (0.2f, len + dLen) : len);`);
});

edit("test/bench.cpp", rep => {
  rep(`    std::printf ("\\n%d checks, %d failed%s\\n", checks, failures, failures == 0 ? "  -  ALL CLEAR" : "");`,
`    // ---- 8. what makes STUDIO a studio -------------------------------------------
    std::printf ("\\n8. the studio\\n");
    {
        const int STUDIO = NUM_MATERIALS - 1;
        // (a) the decay is FLAT across the band - the property treatment buys
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            int mats[NUM_ROOMS] = { STUDIO, STUDIO, STUDIO };
            float shut[NUM_DOORS] = { 0, 0, 0 };
            float st[NBAND], ti[NBAND];
            Engine::eyringRt60 (r, mats, shut, st);
            int tiled[NUM_ROOMS] = { 3, 3, 3 };
            Engine::eyringRt60 (r, tiled, shut, ti);
            const double spreadS = std::max ({ st[1], st[3], st[5] }) / std::min ({ st[1], st[3], st[5] });
            const double spreadT = std::max ({ ti[1], ti[3], ti[5] }) / std::min ({ ti[1], ti[3], ti[5] });
            char buf[220];
            std::snprintf (buf, sizeof buf, "%s STUDIO: RT60 %.2f / %.2f / %.2f s at 250 / 1k / 4k - a spread of %.2fx, where TILED spreads %.2fx",
                           ROOMS[r].name, st[1], st[3], st[5], spreadS, spreadT);
            check (spreadS < 1.35 && spreadS < spreadT * 0.6, buf);
        }
        // (b) live, not dead: the hall keeps a real tail
        {
            int mats[NUM_ROOMS] = { STUDIO, STUDIO, STUDIO };
            float shut[NUM_DOORS] = { 0, 0, 0 };
            float st[NBAND]; Engine::eyringRt60 (2, mats, shut, st);
            char buf[160];
            std::snprintf (buf, sizeof buf, "the hall as a studio is live, not dead: %.2f s at 1 kHz", st[3]);
            check (st[3] > 0.45 && st[3] < 1.0, buf);
        }
        // (c) diffusion: the specular reflections give way, the room does not go quiet
        {
            auto earlyLate = [&] (int mat, double& early, double& total)
            {
                Engine* e = fresh (fs);
                Params p = base(); p.src[0].type = SRC_PURE;
                p.material[0] = p.material[1] = p.material[2] = mat;
                p.door[0] = p.door[1] = p.door[2] = 0;
                p.src[0].x = 2.0f; p.src[0].y = 1.5f; p.src[0].z = EAR_HEIGHT;
                p.lisX = 4.0f; p.lisY = 3.0f; p.lisYaw = 200.0f;
                Take t = render (*e, p, fs, 4.0, impulseAt (2400));
                const int a = firstAbove (t.L, 1e-5f);
                // the window that holds the images and almost nothing else
                early = energy (t.L, a + (int) (0.004 * fs), a + (int) (0.030 * fs));
                total = energy (t.L, a, t.size());
                delete e;
            };
            double eS, tS, eP, tP;
            earlyLate (NUM_MATERIALS - 1, eS, tS);
            earlyLate (2, eP, tP);                     // PLASTER, a hard room with no diffusion
            char buf[240];
            std::snprintf (buf, sizeof buf, "diffusion: the early specular window holds %.1f %% of the energy in STUDIO against %.1f %% in PLASTER, and the room is still live",
                           100.0 * eS / tS, 100.0 * eP / tP);
            check (eS / tS < eP / tP, buf);
        }
        // (d) splayed walls: the reflections stop landing at exact multiples, so a
        //     source in the middle of the room does not comb. Measured as the spread
        //     of the early response across a fine frequency comb.
        {
            auto ripple = [&] (int mat)
            {
                Engine* e = fresh (fs);
                Params p = base(); p.src[0].type = SRC_PURE;
                p.material[0] = p.material[1] = p.material[2] = mat;
                p.door[0] = p.door[1] = p.door[2] = 0;
                p.src[0].x = 3.0f; p.src[0].y = 2.5f; p.src[0].z = EAR_HEIGHT;   // dead centre of LARGE
                p.lisX = 3.0f; p.lisY = 3.6f; p.lisYaw = 270.0f;
                p.reverbDb = -80;                      // the images alone
                Take t = render (*e, p, fs, 1.0, impulseAt (2400));
                const int a = firstAbove (t.L, 1e-5f);
                // energy in 24 narrow bands across 300 Hz - 1.2 kHz, how uneven
                double m[24]; double mean = 0;
                for (int k = 0; k < 24; ++k)
                {
                    const double f = 300.0 * std::pow (4.0, k / 23.0);
                    std::vector<float> b = octaveBand (t.L, fs, f);
                    m[k] = db (energy (b, a, a + (int) (0.08 * fs)));
                    mean += m[k];
                }
                mean /= 24;
                double var = 0; for (int k = 0; k < 24; ++k) var += (m[k] - mean) * (m[k] - mean);
                delete e;
                return std::sqrt (var / 24);
            };
            const double rs = ripple (NUM_MATERIALS - 1), rp = ripple (2);
            char buf[220];
            std::snprintf (buf, sizeof buf, "splay and diffusion: the early response of a centred source is %.1f dB uneven in STUDIO against %.1f dB in PLASTER", rs, rp);
            check (rs < rp, buf);
        }
        // (e) the four original materials are untouched by any of it
        {
            for (int m = 0; m < NUM_MATERIALS - 1; ++m)
            {
                bool zero = MATERIAL_SPLAY[m] == 0.0f;
                for (int b = 0; b < NBAND; ++b) if (MATERIAL_SCATTER[m][b] != 0.0f) zero = false;
                char buf[160];
                std::snprintf (buf, sizeof buf, "%s scatters nothing and splays nothing, so it sounds exactly as it did", MATERIAL_NAMES[m]);
                check (zero, buf);
            }
        }
    }

    std::printf ("\\n%d checks, %d failed%s\\n", checks, failures, failures == 0 ? "  -  ALL CLEAR" : "");`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("studio round 2 applied");
