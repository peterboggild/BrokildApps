// Add section 2b to the quality probe: the ITD read on a source hard to one side.
// The far ear carries the largest fractional delay, so it is where a two-point
// read would cost the most, and a per-ear loss is the one that smears the image.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "quality.cpp");
let s = fs.readFileSync(p, "utf8");
const anchor = "    // ---- 3. the band filter above its top anchor";
if (s.split(anchor).length !== 2) { console.error("anchor not found"); process.exit(1); }

const section = `    // ---- 2b. the ITD read, on a source hard to one side --------------------------
    {
        const double fs = 48000.0;
        std::printf ("\\n2b. the ITD interpolation: a source at 90 degrees, swept across one sample\\n");
        std::printf ("     frac     8k      12k     16k     20k    (the FAR ear, which carries the ITD)\\n");
        double worst = 0;
        const float step = (float) (SPEED_OF_SOUND / fs);
        for (int i = 0; i < 4; ++i)
        {
            const float d = 2.0f + step * (float) i / 4.0f;
            auto ep = std::make_unique<Engine>(); Engine& e = *ep; e.prepare (fs, 256);
            Params p;
            p.material[0] = p.material[1] = p.material[2] = 0;
            p.door[0] = p.door[1] = p.door[2] = 0;
            p.src[0].type = SRC_PURE; p.src[0].directivity = 0;
            p.lisX = 9.0f; p.lisY = 4.5f; p.lisYaw = 90.0f;                    // facing north
            p.src[0].x = 9.0f + d; p.src[0].y = 4.5f; p.src[0].z = EAR_HEIGHT; // due east: 90 deg to the right
            p.earlyDb = -80; p.reverbDb = -80;
            e.setParams (p);
            const int n = (int) (0.4 * fs);
            std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
            for (int k = 0; k < n; k += 128)
            {
                const int m = std::min (128, n - k);
                for (int q = 0; q < m; ++q) { const float v = (k + q == 2400) ? 1.0f : 0.0f; L[(size_t) (k + q)] = v; R[(size_t) (k + q)] = v; }
                e.process (&L[(size_t) k], &R[(size_t) k], m);
            }
            int first = 0;
            for (int k = 0; k < n; ++k) if (std::abs (L[(size_t) k]) > 1e-6f) { first = k; break; }
            std::vector<float> w ((size_t) NFFT, 0.0f);
            for (int k = 0; k < NFFT && first - 8 + k < n; ++k) w[(size_t) k] = L[(size_t) (first - 8 + k)];
            Hrtf h; h.prepare (fs);
            std::vector<float> hl ((size_t) h.numTaps()), hr ((size_t) h.numTaps());
            float itd = 0; h.lookup (90.0f, 0.0f, hl.data(), hr.data(), itd);
            std::vector<float> ref ((size_t) NFFT, 0.0f);
            for (int k = 0; k < h.numTaps(); ++k) ref[(size_t) k] = hl[(size_t) k];
            auto mg = spectrum (w.data(), NFFT), mw = spectrum (ref.data(), NFFT);
            const double r1k = dbAt (mg, 1000, fs) - dbAt (mw, 1000, fs);
            std::printf ("     %.2f ", (double) i / 4.0);
            for (double f : { 8000.0, 12000.0, 16000.0, 20000.0 })
            {
                const double dev = (dbAt (mg, f, fs) - dbAt (mw, f, fs)) - r1k;
                std::printf ("%8.2f", dev);
                if (std::abs (dev) > std::abs (worst)) worst = dev;
            }
            std::printf ("\\n");
        }
        char buf[180];
        std::snprintf (buf, sizeof buf, "the far ear of a 90-degree source stays within %+.2f dB of its own HRTF up to 20 kHz", worst);
        note (std::abs (worst) < 1.5, buf);
    }

`;
fs.writeFileSync(p, s.split(anchor).join(section + anchor));
console.log("section 2b added");
