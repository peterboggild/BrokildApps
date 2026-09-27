// A probe mode that answers one question: how much louder is a source straight
// ahead than the same acoustic power arriving diffusely?  That number IS the
// direct-to-reverberant ratio at the critical distance for a real head, and the
// textbook "0 dB" assumes an omnidirectional receiver.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "probe.cpp");
let s = fs.readFileSync(p, "utf8");
const anchor = '    if (what == "dist")';
if (s.split(anchor).length !== 2) { console.error("anchor not found"); process.exit(1); }

const mode = `    if (what == "hrtfbal")
    {
        Hrtf h; h.prepare (fs);
        const int n = h.numTaps();
        std::vector<float> l ((size_t) n), r ((size_t) n);
        float itd = 0;
        auto energy = [&] (float az, float el)
        {
            h.lookup (az, el, l.data(), r.data(), itd);
            double e = 0;
            for (int i = 0; i < n; ++i) e += (double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i];
            return 0.5 * e;                      // per ear
        };
        // the sphere, area weighted
        double sum = 0, w = 0;
        for (int ei = -8; ei <= 8; ++ei)
        {
            const float el = ei * 10.0f;
            const float cw = std::cos (el * 3.14159265f / 180.0f);
            for (int ai = 0; ai < 36; ++ai) { sum += cw * energy (ai * 10.0f, el); w += cw; }
        }
        const double diffuse = sum / w;
        // the eight directions the engine's own diffuse render uses
        double d8 = 0;
        for (int j = 0; j < 8; ++j) d8 += energy (22.5f + 45.0f * j, (j & 1) ? 20.0f : -10.0f);
        d8 /= 8.0;
        std::printf ("per-ear HRTF energy, %.0f Hz, %d taps\\n", fs, n);
        std::printf ("  sphere average (area weighted, 612 directions): %.4f\\n", diffuse);
        std::printf ("  the engine's eight diffuse directions:          %.4f  (%+.2f dB vs the sphere)\\n", d8, 10 * std::log10 (d8 / diffuse));
        for (float az : { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f })
            std::printf ("  az %5.0f: %.4f  (%+.2f dB vs the sphere)\\n", az, energy (az, 0.0f), 10 * std::log10 (energy (az, 0.0f) / diffuse));
        std::printf ("\\n  So a source straight ahead is %+.2f dB louder than the same power arriving\\n"
                     "  diffusely. That is the direct-to-reverberant ratio a real head measures at\\n"
                     "  the critical distance; the textbook 0 dB assumes an omnidirectional ear.\\n",
                     10 * std::log10 (energy (0.0f, 0.0f) / diffuse));
        return 0;
    }

`;
fs.writeFileSync(p, s.split(anchor).join(mode + anchor));
console.log("hrtfbal mode added");
