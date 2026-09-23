// The KIT TUNE check measures a hat wrongly.
//
// A hat has no fundamental worth finding: its pitch is carried by a bandpassed
// cluster sitting between about two and ten kilohertz. The check was scanning
// 154 Hz to 2.2 kHz — derived from the channel's TUNE parameter, which is the
// cluster's base, not where the energy is — so it locked onto a low partial
// that barely moves and reported five semitones where the engine transposes
// twenty-four.
//
// Fourth time in this project the probe has been wrong rather than the engine.
// For anything metal, measure the spectral CENTROID: it is where the sound
// actually sits, and a transpose moves it by exactly the transpose.
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/FullMetalRacket/test/test.cpp";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function rep(a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(a).join(b);
}

rep(`        const int i = paramIndex ((std::string (channelId (c)) + "_tune").c_str());
        const double base = (double) xmap (paramSpec (i).def, paramSpec (i).lo, paramSpec (i).hi);
        //  wide enough to hold a two-octave transpose either side, and for the
        //  metal channels the dominant partial is well above the fundamental
        return dominant (b, 48000.0, 4000, 40000, base * 0.35, base * 5.0);`,
`        const int i = paramIndex ((std::string (channelId (c)) + "_tune").c_str());
        const double base = (double) xmap (paramSpec (i).def, paramSpec (i).lo, paramSpec (i).hi);

        /*  A hat has no fundamental worth finding — its pitch is a bandpassed
            cluster spread over several kilohertz — so for the metal channels
            the honest measure is where the weight of the sound sits. */
        if (channelFamily (c) == FAM_METAL)
        {
            double num = 0.0, den = 0.0;
            for (double f = 200.0; f < 16000.0; f *= 1.06)
            {
                const double m = goertzel (b, f, 48000.0, 2000, 20000);
                num += m * f; den += m;
            }
            return num / std::max (1e-12, den);
        }
        return dominant (b, 48000.0, 4000, 40000, base * 0.35, base * 5.0);`, "metal centroid");

rep(`        std::printf ("    %-6s  KIT TUNE 0%% = %7.2f Hz   50%% = %7.2f   100%% = %7.2f   span %+.1f semitones\\n",
                     channelName (c), lo, mid, hi, semis);`,
`        std::printf ("    %-6s  KIT TUNE 0%% = %7.2f Hz   50%% = %7.2f   100%% = %7.2f   span %+.1f semitones%s\\n",
                     channelName (c), lo, mid, hi, semis,
                     channelFamily (c) == FAM_METAL ? "   (spectral centroid)" : "");`, "print");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("metal channels measured by centroid");
