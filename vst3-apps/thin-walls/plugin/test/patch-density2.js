/*  Two faults in my own measurement, both of the misplaced-window family:

    1. L_total counted the delay lines only, not the allpasses inside them - so
       it reported the number from BEFORE the tank was built and could never show
       the fix working.

    2. The ripple was measured over a fixed window 300 ms after the impulse. For
       a 0.30 s decay that is past the end of the tail, so it measured the
       numerical floor rather than the modes. And there is no window length that
       works untreated: short enough not to span the decay is too short to
       RESOLVE modes a few hertz apart. The standard way out is to divide the
       decay back out - multiply the tail by exp(+13.8 t / RT60) so it becomes
       stationary - and then transform. That measures the modal structure and
       nothing else.
*/
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "quality.cpp");
let s = fs.readFileSync(p, "utf8");
const misses = [];
const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(c + " for " + a.slice(0, 70)); return; } s = s.split(a).join(b); };

rep(`                // the total loop delay the network actually has
                double Ltot = 0;
                for (int k = 0; k < RoomField::N; ++k) Ltot += e.field (r).len[(size_t) k];
                Ltot /= fs;`,
`                // the total loop delay the network actually has: every line AND
                // the allpasses inside it, because an allpass's length counts
                // towards the modal density exactly as a delay's does
                double Ltot = 0;
                for (int k = 0; k < RoomField::N; ++k)
                    Ltot += e.field (r).len[(size_t) k] + e.field (r).apTotal[(size_t) k];
                Ltot /= fs;`);

rep(`                // spectral ripple over 400 Hz - 2 kHz, measured on a settled second of tail
                const int from = 2400 + (int) (0.30 * fs);
                std::vector<std::complex<double>> a ((size_t) NFFT, { 0.0, 0.0 });
                for (int i = 0; i < NFFT && from + i < n; ++i) a[(size_t) i] = { (double) L[(size_t) (from + i)], 0.0 };
                fft (a, false);`,
`                /*  Spectral ripple over 400 Hz - 2 kHz. The decay is divided back
                    out first (multiply by exp(+13.8 t / RT60)) so the window holds
                    a stationary signal: without that, a window long enough to
                    resolve modes a few hertz apart is also long enough to span the
                    whole decay, and what comes back is the envelope, not the modes.
                    Hann windowed, starting once the field is established. */
                const int from = 2400 + (int) (0.04 * fs);
                std::vector<std::complex<double>> a ((size_t) NFFT, { 0.0, 0.0 });
                const double tau60 = 13.8 / std::max (0.05f, rt[3]);
                for (int i = 0; i < NFFT && from + i < n; ++i)
                {
                    const double t = (double) i / fs;
                    const double comp = std::exp (tau60 * t * 0.5);        // amplitude, not power
                    const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / (NFFT - 1));
                    a[(size_t) i] = { (double) L[(size_t) (from + i)] * comp * win, 0.0 };
                }
                fft (a, false);`);

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
fs.writeFileSync(p, s);
console.log("the density measurement repaired");
