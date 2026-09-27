// Section 9: the broken walls. The decisive one is the first: a wall broken by
// two millimetres is geometrically the same room as a square one, so the general
// image search must reproduce what the shoebox arithmetic gives. If it misses
// paths, charges the wrong surface, or validates wrongly, that check fails.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "bench.cpp");
let s = fs.readFileSync(p, "utf8");
const anchor = `    std::printf ("\\n%d checks, %d failed%s\\n", checks, failures, failures == 0 ? "  -  ALL CLEAR" : "");`;
if (s.split(anchor).length !== 2) { console.error("anchor not found"); process.exit(1); }

const section = `    // ---- 9. walls that are no longer parallel --------------------------------------
    std::printf ("\\n9. broken walls\\n");
    {
        auto takeOf = [&] (float push0, float push1, float along0, double secs, int* paths)
        {
            Engine* e = fresh (fs);
            Params p = base(); p.src[0].type = SRC_PURE;
            p.material[0] = p.material[1] = p.material[2] = 2;          // PLASTER: hard, so reflections speak
            p.door[0] = p.door[1] = p.door[2] = 0;
            p.src[0].x = 3.0f; p.src[0].y = 2.5f; p.src[0].z = EAR_HEIGHT;
            p.lisX = 3.0f; p.lisY = 3.8f; p.lisYaw = 270.0f;
            p.breakPush[0][0] = push0; p.breakPush[0][1] = push1; p.breakAlong[0][0] = along0;
            Take t = render (*e, p, fs, secs, impulseAt (2400));
            if (paths) *paths = e->numActivePaths();
            delete e;
            return t;
        };

        // (a) a two-millimetre break is the same room: the general search must agree
        //     with the shoebox arithmetic it replaces
        {
            int nSquare = 0, nBroken = 0;
            Take sq = takeOf (0.0f, 0.0f, 0.5f, 1.0, &nSquare);
            Take br = takeOf (0.002f, 0.0f, 0.5f, 1.0, &nBroken);
            const double a = db (energy (sq.L, 0, sq.size())), b = db (energy (br.L, 0, br.size()));
            // and the early part, where the images are and where a missing path would show
            const int s0 = firstAbove (sq.L, 1e-5f);
            const double ae = db (energy (sq.L, s0, s0 + (int) (0.05 * fs)));
            const double be = db (energy (br.L, s0, s0 + (int) (0.05 * fs)));
            char buf[240];
            std::snprintf (buf, sizeof buf, "a wall broken by 2 mm is the same room: the general search lands within %.2f dB of the shoebox overall and %.2f dB over the first 50 ms (%d paths against %d)",
                           b - a, be - ae, nBroken, nSquare);
            check (std::abs (b - a) < 0.6 && std::abs (be - ae) < 0.6 && nBroken >= nSquare - 2, buf);
        }

        // (b) pushing a wall out really does make the room bigger and longer
        {
            RoomSurfaces surf; surf.wall = 2;
            float shut[NUM_DOORS] = { 0, 0, 0 };
            float flat7[NBAND], bent7[NBAND];
            Engine::eyringRt60 (0, surf, shut, flat7);
            Engine* e = fresh (fs);
            Params p = base(); p.material[0] = p.material[1] = p.material[2] = 2;
            p.door[0] = p.door[1] = p.door[2] = 0;
            p.breakPush[0][0] = 0.6f;
            Take t = render (*e, p, fs, 0.2, impulseAt (2400));
            e->roomRt60 (0, bent7);
            delete e;
            char buf[200];
            std::snprintf (buf, sizeof buf, "pushing a wall 0.6 m out lengthens the room's own decay: %.3f s becomes %.3f s at 1 kHz", flat7[3], bent7[3]);
            check (bent7[3] > flat7[3] * 1.002 && bent7[3] < flat7[3] * 1.15, buf);
        }

        // (c) the point of the whole thing: a source in the middle of a hard room
        //     combs, and breaking the walls stops it
        {
            auto ripple = [&] (float push0, float push1)
            {
                Engine* e = fresh (fs);
                Params p = base(); p.src[0].type = SRC_PURE;
                p.material[0] = p.material[1] = p.material[2] = 2;
                p.door[0] = p.door[1] = p.door[2] = 0;
                p.src[0].x = 3.0f; p.src[0].y = 2.5f; p.src[0].z = EAR_HEIGHT;
                p.lisX = 3.0f; p.lisY = 3.6f; p.lisYaw = 270.0f;
                p.reverbDb = -80;                       // the images alone
                p.breakPush[0][0] = push0; p.breakPush[0][1] = push1;
                p.breakAlong[0][0] = 0.42f; p.breakAlong[0][1] = 0.58f;
                Take t = render (*e, p, fs, 1.0, impulseAt (2400));
                const int a = firstAbove (t.L, 1e-5f);
                double m[24]; double mean = 0;
                for (int k = 0; k < 24; ++k)
                {
                    const double f = 300.0 * std::pow (4.0, k / 23.0);
                    std::vector<float> bnd = octaveBand (t.L, fs, f);
                    m[k] = db (energy (bnd, a, a + (int) (0.08 * fs)));
                    mean += m[k];
                }
                mean /= 24;
                double var = 0; for (int k = 0; k < 24; ++k) var += (m[k] - mean) * (m[k] - mean);
                delete e;
                return std::sqrt (var / 24);
            };
            const double flat = ripple (0.0f, 0.0f), bent = ripple (0.55f, 0.45f);
            char buf[220];
            std::snprintf (buf, sizeof buf, "a centred source in a square hard room is %.1f dB uneven across 300 Hz - 1.2 kHz; with both walls broken, %.1f dB", flat, bent);
            check (bent < flat, buf);
        }

        // (d) a wall pushed INTO the room is concave, and the geometry knows it:
        //     some paths are now blocked by the fold itself
        {
            int nOut = 0, nIn = 0;
            takeOf (0.55f, 0.0f, 0.5f, 0.3, &nOut);
            takeOf (-0.55f, 0.0f, 0.5f, 0.3, &nIn);
            char buf[200];
            std::snprintf (buf, sizeof buf, "pushed out the fold disperses (%d paths); pushed in it is concave and shadows itself (%d)", nOut, nIn);
            check (nIn <= nOut, buf);
        }

        // (e) bounded and finite across the whole range, in every room
        {
            bool ok = true; double worst = 0;
            for (int r = 0; r < NUM_ROOMS; ++r)
                for (int k = 0; k < 6; ++k)
                {
                    const float push = -0.6f + 0.24f * k;
                    Engine* e = fresh (fs);
                    Params p = base(); p.src[0].type = SRC_PURE;
                    p.material[0] = p.material[1] = p.material[2] = 3;
                    p.door[0] = p.door[1] = p.door[2] = 1;
                    const Room& R = ROOMS[r];
                    p.src[0].x = 0.5f * (R.x0 + R.x1) - 0.6f; p.src[0].y = 0.5f * (R.y0 + R.y1) + 0.5f;
                    p.lisX = 0.5f * (R.x0 + R.x1) + 0.8f; p.lisY = 0.5f * (R.y0 + R.y1) - 0.4f;
                    p.breakPush[r][0] = push; p.breakPush[r][1] = -push;
                    uint32_t sd = 9;
                    Take t = render (*e, p, fs, 2.0, [&sd] (int i) { sd = sd * 1664525u + 1013904223u; return i < 20000 ? ((sd >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f : 0.0f; });
                    const double pk = peakAbs (t.L);
                    if (! std::isfinite (pk) || pk > 8.0) ok = false;
                    worst = std::max (worst, pk);
                    delete e;
                }
            char buf[200];
            std::snprintf (buf, sizeof buf, "every room, every fold from -0.6 to +0.6 m: finite and bounded, worst peak %.3f", worst);
            check (ok, buf);
        }

        // (f) and the walls may be moved while a note is sounding without a click
        {
            Engine* e = fresh (fs);
            Params p = base(); p.src[0].type = SRC_PURE;
            p.material[0] = p.material[1] = p.material[2] = 2;
            const int n = (int) (3.0 * fs);
            Take t; t.fs = fs; t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
            e->setParams (p);
            for (int i = 0; i < n; i += 128)
            {
                const double tt = i / fs;
                if (tt > 0.5 && tt < 2.5) { p.breakPush[0][0] = (float) ((tt - 0.5) * 0.3); e->setParams (p); }
                for (int k = 0; k < 128; ++k) { const float v = 0.3f * std::sin (2.0f * 3.14159265f * 440.0f * (float) (i + k) / (float) fs); t.L[(size_t) (i + k)] = v; t.R[(size_t) (i + k)] = v; }
                e->process (&t.L[(size_t) i], &t.R[(size_t) i], 128);
            }
            std::vector<float> hf = octaveBand (t.L, fs, 8000);
            const double moving = db (energy (hf, (int) (0.6 * fs), (int) (2.4 * fs))) - db (energy (t.L, (int) (0.6 * fs), (int) (2.4 * fs)));
            char buf[200];
            std::snprintf (buf, sizeof buf, "a wall moved under a sounding note keeps everything above 6 kHz %.0f dB below it", -moving);
            check (moving < -45.0, buf);
            delete e;
        }
    }

`;
fs.writeFileSync(p, s.split(anchor).join(section + anchor));
console.log("section 9 added");
