// Bench section 7: several sources, inputs, the pure source's directivity.
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
let s = fs.readFileSync(path.join(root, "test/bench.cpp"), "utf8");
const anchor = `    std::printf ("\\n%d checks, %d failed%s\\n", checks, failures, failures == 0 ? "  -  ALL CLEAR" : "");`;
if (s.split(anchor).length !== 2) { console.error("anchor miss"); process.exit(1); }
const section = `    // ---- 7. several sources ------------------------------------------------------
    std::printf ("\\n7. several sources\\n");
    {
        // a source switched OFF changes nothing: memcmp against one source alone
        {
            Engine* a = fresh (fs); Engine* b = fresh (fs);
            Params pa = base(); Params pb = base();
            pb.src[1].input = IN_OFF; pb.src[1].x = 1.0f; pb.src[1].y = 1.0f;   // moved, but off
            uint32_t s1 = 11, s2 = 11;
            auto nA = [&s1] (int) { s1 = s1 * 1664525u + 1013904223u; return ((s1 >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f; };
            auto nB = [&s2] (int) { s2 = s2 * 1664525u + 1013904223u; return ((s2 >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f; };
            Take ta = render (*a, pa, fs, 1.0, nA), tb = render (*b, pb, fs, 1.0, nB);
            check (std::memcmp (ta.L.data(), tb.L.data(), ta.L.size() * sizeof (float)) == 0, "a source with INPUT off is bit-identical to no source at all");
            delete a; delete b;
        }
        // two sources from the two channels: L fed to a speaker on the left, R to one on the right
        {
            Engine* e = fresh (fs);
            Params p = base();
            p.src[0].input = IN_MAIN_L; p.src[0].x = 3.0f; p.src[0].y = 3.5f; p.src[0].type = SRC_PURE;
            p.src[1].input = IN_MAIN_R; p.src[1].x = 3.0f; p.src[1].y = 1.5f; p.src[1].type = SRC_PURE;
            p.lisX = 4.5f; p.lisY = 2.5f; p.lisYaw = 180;      // facing west: src[0] is to the right, src[1] to the left
            p.earlyDb = -80; p.reverbDb = -80; p.door[0] = p.door[1] = 0;
            // channel L carries a tone, channel R silence, then the reverse
            const int n = (int) (0.6 * fs);
            Take t; t.fs = fs; t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
            e->setParams (p);
            for (int i = 0; i < n; i += 256)
            {
                for (int k = 0; k < 256 && i + k < n; ++k)
                {
                    const float tone = 0.3f * std::sin (2.0f * 3.14159265f * 500.0f * (float) (i + k) / (float) fs);
                    t.L[(size_t) (i + k)] = (i + k < n / 2) ? tone : 0.0f;
                    t.R[(size_t) (i + k)] = (i + k < n / 2) ? 0.0f : tone;
                }
                e->process (&t.L[(size_t) i], &t.R[(size_t) i], std::min (256, n - i));
            }
            const int a0 = (int) (0.1 * fs), a1 = (int) (0.28 * fs), b0 = (int) (0.4 * fs), b1 = (int) (0.58 * fs);
            const double firstHalfLR = db (energy (t.L, a0, a1)) - db (energy (t.R, a0, a1));
            const double secondHalfLR = db (energy (t.L, b0, b1)) - db (energy (t.R, b0, b1));
            char buf[160]; std::snprintf (buf, sizeof buf, "MAIN L to the right-hand source, MAIN R to the left-hand one: L/R %.1f dB then %.1f dB", firstHalfLR, secondHalfLR);
            check (firstHalfLR < -4.0 && secondHalfLR > 4.0, buf);
            delete e;
        }
        // the AUX bus reaches a source
        {
            Engine* e = fresh (fs);
            Params p = base(); p.src[0].input = IN_AUX_L; p.earlyDb = -80; p.reverbDb = -80;
            const int n = 4800;
            std::vector<float> mL ((size_t) n, 0.0f), mR ((size_t) n, 0.0f), aL ((size_t) n, 0.0f), aR ((size_t) n, 0.0f), oL ((size_t) n), oR ((size_t) n);
            aL[2400] = 1.0f;
            e->setParams (p);
            for (int i = 0; i < n; i += 256) e->process (mL.data() + i, mR.data() + i, aL.data() + i, aR.data() + i, oL.data() + i, oR.data() + i, std::min (256, n - i));
            check (energy (oL, 0, n) > 1e-3, "AUX L feeds a source whose INPUT is AUX L", energy (oL, 0, n));
            delete e;
        }
        // the pure source: omni at 0, cardioid at 1
        {
            SourceParams sp; sp.type = SRC_PURE;
            sp.directivity = 0; const double o = Engine::directivityDb (sp, -1.0f, 3);
            sp.directivity = 1; const double c = Engine::directivityDb (sp, -1.0f, 3), c90 = Engine::directivityDb (sp, 0.0f, 3);
            check (std::abs (o) < 0.01 && c < -25.0 && std::abs (c90 + 6.0) < 0.2, "PURE: omni at directivity 0, cardioid at 1 (-6 dB at 90, off behind)", c, c90);
            // and the loudspeaker: nearly omni at 125 Hz, beamed at 4 kHz, 90 degrees in between
            SourceParams ls; ls.type = SRC_LOUDSPEAKER;
            const double lo = Engine::directivityDb (ls, -1.0f, 0), hi = Engine::directivityDb (ls, -1.0f, 5), side = Engine::directivityDb (ls, 0.0f, 5);
            char buf[160]; std::snprintf (buf, sizeof buf, "LOUDSPEAKER: behind %.1f dB at 125 Hz, %.1f at 4 kHz, %.1f at 90 degrees / 4 kHz", lo, hi, side);
            check (lo > -6.0 && hi < -18.0 && side < hi + 18.0 && side > hi + 4.0, buf);
            // the radiated power of a cardioid is a third of an omni's: -4.8 dB
            sp.directivity = 1;
            check (std::abs (Engine::radiatedPowerDb (sp, 3) + 4.77) < 0.3, "a cardioid radiates 1/3 the power of an omni (-4.8 dB into the room)", Engine::radiatedPowerDb (sp, 3));
        }
        // sources in two rooms at once, doors shut: each is heard where it is
        {
            Engine* e = fresh (fs);
            Params p = base();
            p.src[0].input = IN_MAIN_L; p.src[0].x = 2.0f; p.src[0].y = 2.5f;
            p.src[1].input = IN_MAIN_R; p.src[1].x = 10.0f; p.src[1].y = 4.5f;     // in the hall
            p.lisX = 4.5f; p.lisY = 2.5f; p.lisYaw = 180; p.door[0] = p.door[1] = p.door[2] = 0;
            uint32_t s = 21;
            const int n = (int) (1.5 * fs);
            Take t; t.fs = fs; t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
            e->setParams (p);
            for (int i = 0; i < n; i += 256)
            {
                for (int k = 0; k < 256 && i + k < n; ++k) { s = s * 1664525u + 1013904223u; const float v = ((s >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f; t.L[(size_t) (i + k)] = (i + k < n / 2) ? v : 0.0f; t.R[(size_t) (i + k)] = (i + k < n / 2) ? 0.0f : v; }
                e->process (&t.L[(size_t) i], &t.R[(size_t) i], std::min (256, n - i));
            }
            const double here = db (energy (t.L, (int) (0.2 * fs), (int) (0.7 * fs)));
            const double nextDoor = db (energy (t.L, (int) (0.95 * fs), (int) (1.45 * fs)));
            char buf[160]; std::snprintf (buf, sizeof buf, "one source in the room, one behind a shut door into the hall: %.0f dB apart, both audible", here - nextDoor);
            check (here - nextDoor > 15.0 && here - nextDoor < 60.0, buf);
            delete e;
        }
        // cost with all four sources live, every door open
        {
            Engine* e = fresh (fs);
            Params p = base();
            for (int k = 0; k < MAX_SOURCES; ++k) p.src[k].input = IN_MAIN_LR;
            p.src[1].x = 1.0f; p.src[1].y = 1.0f; p.src[2].x = 4.5f; p.src[2].y = 7.0f; p.src[3].x = 12.0f; p.src[3].y = 4.5f;
            p.door[0] = p.door[1] = p.door[2] = 1;
            uint32_t s = 5;
            const int n = (int) (10.0 * fs);
            std::vector<float> L ((size_t) n), R ((size_t) n);
            for (int i = 0; i < n; ++i) { s = s * 1664525u + 1013904223u; L[(size_t) i] = R[(size_t) i] = ((s >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.2f; }
            e->setParams (p);
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < n; i += 256) e->process (&L[(size_t) i], &R[(size_t) i], std::min (256, n - i));
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  cost: four sources, %d paths active, %.1f %% of one core at 48 kHz\\n", e->numActivePaths(), 100.0 * secs / 10.0);
            check (secs < 6.0 && std::isfinite (peakAbs (L)) && peakAbs (L) < 8.0, "four sources: under 60 % of a core, bounded", 100.0 * secs / 10.0, peakAbs (L));
            delete e;
        }
    }

`;
s = s.split(anchor).join(section + anchor);
fs.writeFileSync(path.join(root, "test/bench.cpp"), s);
console.log("section 7 added");
