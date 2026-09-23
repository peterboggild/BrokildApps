// bench 9: the FOLD block also measures its aliasing at C5 — a folded wave has
// kinks, and the first-order ADAA is what keeps them clean.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "bench.cpp");
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const from = String.raw`            ok (pk[1] <= pk[0] * 1.05 + 1e-3, "a folded wave stays inside the window (no louder than the clipped one)", d);
            ok (thd[1] > -12.0, "and the fold is rich (a sine at gain 4 folds back on itself)", d);
        }`.split("\n").join(NL);
const to = String.raw`            ok (pk[1] <= pk[0] * 1.05 + 1e-3, "a folded wave stays inside the window (no louder than the clipped one)", d);
            ok (thd[1] > -12.0, "and the fold is rich (a sine at gain 4 folds back on itself)", d);
            //  and its kinks are anti-aliased: SPINE bright, gain 4, full fold, C5
            {
                Line sx[NLINES]; straightAll (sx, 1.0f, 0.0f);
                Engine* e = fresh (1, sx);
                e->p.grain = 1.0f; e->p.contrast = 0.5f; e->p.fold = 1.0f;
                const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
                Take t = render (*e, 72, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                const double fl = aliasFloorDb (m, fC5, N);
                char d2[128]; std::snprintf (d2, sizeof d2, "C5, SPINE y = 1, GRAIN 1, gain 4, FOLD 1: non-harmonic floor %.1f dB at level %d", fl, e->voiceLod (0));
                std::printf ("  %s\n", d2);
                ok (fl < -40.0, "a full fold at C5 keeps its aliasing 40 dB under the harmonics", d2);
                delete e;
            }
        }`.split("\n").join(NL);
if (s.split(from).length !== 2) { console.error("anchor miss"); process.exit(1); }
s = s.replace(from, to);
fs.writeFileSync(p, s, "utf8");
console.log("bench: fold alias check added");
