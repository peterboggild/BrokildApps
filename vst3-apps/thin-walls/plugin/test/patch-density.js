/*  Section 7 of the quality probe: how dense is the late field, really?

    Two published numbers say whether a reverberator sounds like a room or like a
    pipe, and both can be measured from the output rather than argued about.

    1. SPECTRAL RIPPLE. A properly diffuse late field has a Gaussian impulse
       response, so its magnitude is Rayleigh distributed and the standard
       deviation of |H| in dB is 5.57 dB - a fixed number, independent of the
       room. Much more than that means the network's own modes are resolvable,
       which is heard as pitched ringing on the tail. Modes are resolvable when
       their spacing 1/L_total exceeds their bandwidth 2.2/RT60, i.e. when the
       total loop delay is shorter than RT60/2.2.

    2. NORMALISED ECHO DENSITY (Abel and Huang 2006). The fraction of samples in
       a window that exceed the window's own standard deviation, divided by
       erfc(1/sqrt2) = 0.3173. It converges to 1.0 for a Gaussian - that is, for
       a field so dense the individual echoes have merged. Below 1.0 the echoes
       are still countable, which is heard as grain or rattle.
*/
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "quality.cpp");
let s = fs.readFileSync(p, "utf8");
const anchor = `    std::printf ("\\n%s\\n", failures == 0 ? "no artefact found beyond the stated bounds" : "SOMETHING IS BENDING THE RESPONSE");`;
if (s.split(anchor).length !== 2) { console.error("anchor not found"); process.exit(1); }

const section = `    // ---- 7. how dense is the late field ------------------------------------------
    {
        const double fs = 48000.0;
        std::printf ("\\n7. the late field's density - is it a room or a pipe?\\n");
        std::printf ("     room  material     RT60   L_total  need   ripple dB   echo density at 50/100/200 ms\\n");
        std::printf ("                                               (5.57 = diffuse)   (1.00 = merged)\\n");

        for (int r = 0; r < NUM_ROOMS; ++r)
            for (int m : { 4, 1, 2 })                       // STUDIO, FURNISHED, PLASTER
            {
                auto ep = std::make_unique<Engine>(); Engine& e = *ep; e.prepare (fs, 256);
                Params pp;
                pp.material[0] = pp.material[1] = pp.material[2] = m;
                pp.door[0] = pp.door[1] = pp.door[2] = 0;
                pp.src[0].type = SRC_PURE;
                const Room& R = ROOMS[r];
                pp.src[0].x = R.x0 + 0.3f * (R.x1 - R.x0); pp.src[0].y = R.y0 + 0.35f * (R.y1 - R.y0); pp.src[0].z = EAR_HEIGHT;
                pp.lisX = R.x0 + 0.7f * (R.x1 - R.x0); pp.lisY = R.y0 + 0.6f * (R.y1 - R.y0);
                pp.directDb = -120; pp.earlyDb = -120;      // the late field alone
                e.setParams (pp);

                const int n = (int) (4.0 * fs);
                std::vector<float> L ((size_t) n, 0.0f), Rr ((size_t) n, 0.0f);
                for (int i = 0; i < n; i += 128)
                {
                    const int mm = std::min (128, n - i);
                    for (int k = 0; k < mm; ++k) { const float v = (i + k == 2400) ? 1.0f : 0.0f; L[(size_t) (i + k)] = v; Rr[(size_t) (i + k)] = v; }
                    e.process (&L[(size_t) i], &Rr[(size_t) i], mm);
                }

                // the total loop delay the network actually has
                double Ltot = 0;
                for (int k = 0; k < RoomField::N; ++k) Ltot += e.field (r).len[(size_t) k];
                Ltot /= fs;
                float rt[NBAND]; e.roomRt60 (r, rt);
                const double need = rt[3] / 2.2;

                // spectral ripple over 400 Hz - 2 kHz, measured on a settled second of tail
                const int from = 2400 + (int) (0.30 * fs);
                std::vector<std::complex<double>> a ((size_t) NFFT, { 0.0, 0.0 });
                for (int i = 0; i < NFFT && from + i < n; ++i) a[(size_t) i] = { (double) L[(size_t) (from + i)], 0.0 };
                fft (a, false);
                double mean = 0; int cnt = 0;
                const int k0 = (int) (400.0 / fs * NFFT), k1 = (int) (2000.0 / fs * NFFT);
                std::vector<double> bins;
                for (int k = k0; k <= k1; ++k)
                {
                    const double d = 20.0 * std::log10 (std::max (std::abs (a[(size_t) k]), 1e-30));
                    bins.push_back (d); mean += d; ++cnt;
                }
                mean /= std::max (1, cnt);
                double var = 0; for (double d : bins) var += (d - mean) * (d - mean);
                const double ripple = std::sqrt (var / std::max ((size_t) 1, bins.size()));

                // Abel-Huang normalised echo density
                auto echoDensity = [&] (double at)
                {
                    const int c = 2400 + (int) (at * fs);
                    const int half = (int) (0.010 * fs);        // a 20 ms window
                    if (c + half >= n) return 0.0;
                    double sd = 0; int nn = 0;
                    for (int i = c - half; i < c + half; ++i) { sd += (double) L[(size_t) i] * L[(size_t) i]; ++nn; }
                    sd = std::sqrt (sd / std::max (1, nn));
                    if (sd <= 0) return 0.0;
                    int over = 0;
                    for (int i = c - half; i < c + half; ++i) if (std::abs (L[(size_t) i]) > sd) ++over;
                    return (double) over / (double) nn / 0.3173;
                };

                std::printf ("     %-5s %-11s %5.2f  %5.0fms %5.0fms   %6.2f     %.2f  %.2f  %.2f\\n",
                             ROOMS[r].name, MATERIAL_NAMES[m], rt[3], Ltot * 1000, need * 1000,
                             ripple, echoDensity (0.05), echoDensity (0.10), echoDensity (0.20));
            }
        std::printf ("     a total loop delay below the \\"need\\" column means the network's own modes are\\n"
                     "     resolvable, which is heard as pitched ringing rather than a decay.\\n");
    }

`;
fs.writeFileSync(p, s.split(anchor).join(section + anchor));
console.log("density section added");
