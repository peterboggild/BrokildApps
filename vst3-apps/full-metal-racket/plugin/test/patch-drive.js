// (a) The per-channel DRIVE curve was 1 + dr*11, so a "14%" setting already
//     sat well into the tanh knee: the channels with the most default drive
//     were exactly the ones with the least velocity range (SD1 2.6x against
//     CY2's 7.6x, in DRIVE order). A quadratic-plus-linear curve keeps the
//     top end and gives the bottom back.
// (b) Brightness measured as a spectral centroid is dominated by the
//     fundamental, so a kick reads DARKER when hit harder even though its
//     filter has opened an octave. Measure upper content RELATIVE to the body.
"use strict";
const fs = require("fs");
const miss = [];

function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  for (const [a, b, tag] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(path + ": " + tag + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(path, s);
}

const E = "C:/Users/peter/b/FullMetalRacket/Source/Engine.cpp";
const T = "C:/Users/peter/b/FullMetalRacket/test/test.cpp";

const writeE = edit(E, [
  [`                    const float g = 1.0f + dr * 11.0f;`,
   `                    /*  Quadratic plus a gentle linear term. The old
                        1 + dr*11 put a 14% setting already deep in the tanh
                        knee, which cost the channel most of its velocity
                        range for nothing anyone had asked for. */
                    const float g = 1.0f + dr * 2.0f + dr * dr * 14.0f;`,
   "drive curve"],
  // defaults: less drive baked into the boot kit
  [`{ "bd1", "BD 1",  FAM_KICK,  36,   30.f,  120.f,   40.f, 2200.f,  0.28f,0.46f,0.42f,0.34f,0.40f,0.16f,0.80f, 0 },`,
   `{ "bd1", "BD 1",  FAM_KICK,  36,   30.f,  120.f,   40.f, 2200.f,  0.28f,0.46f,0.42f,0.34f,0.40f,0.10f,0.80f, 0 },`,
   "bd1 default drive"],
  [`{ "bd2", "BD 2",  FAM_KICK,  35,   30.f,  120.f,   40.f, 2200.f,  0.16f,0.62f,0.30f,0.20f,0.55f,0.24f,0.72f, 2 },`,
   `{ "bd2", "BD 2",  FAM_KICK,  35,   30.f,  120.f,   40.f, 2200.f,  0.16f,0.62f,0.30f,0.20f,0.55f,0.16f,0.72f, 2 },`,
   "bd2 default drive"],
  [`{ "sd1", "SD 1",  FAM_SNARE, 38,  100.f,  420.f,   30.f,  900.f,  0.42f,0.34f,0.52f,0.52f,0.34f,0.14f,0.76f, 0 },`,
   `{ "sd1", "SD 1",  FAM_SNARE, 38,  100.f,  420.f,   30.f,  900.f,  0.42f,0.34f,0.52f,0.52f,0.34f,0.08f,0.76f, 0 },`,
   "sd1 default drive"]
]);

const writeT = edit(T, [
  [`        auto bright = [] (const Buf& b) -> double
        {
            double num = 0.0, den = 0.0;
            for (double f = 120.0; f < 14000.0; f *= 1.12)
            {
                const double m = goertzel (b, f, 48000.0, 0, 12000);
                num += m * f; den += m;
            }
            return num / std::max (1e-12, den);
        };
        const double bs = bright (soft), bh = bright (hard);
        std::printf ("    %-6s centroid: soft %.0f Hz, hard %.0f Hz\\n", channelName (c), bs, bh);`,
   `        /*  Upper content RELATIVE to the body, not a bare centroid: a
            kick's centroid is pinned by its own fundamental, so opening its
            filter by a full octave still reads as "darker". This asks the
            question that was meant — how much is going on above the drum's
            own note compared with the note itself. */
        const double f0 = (double) xmap (paramSpec (paramIndex ((std::string (channelId (c)) + "_tune").c_str())).def,
                                         paramSpec (paramIndex ((std::string (channelId (c)) + "_tune").c_str())).lo,
                                         paramSpec (paramIndex ((std::string (channelId (c)) + "_tune").c_str())).hi);
        auto bright = [f0] (const Buf& b) -> double
        {
            double hi = 0.0, lo = 0.0;
            for (double f = f0 * 0.7; f < f0 * 1.6; f *= 1.04) lo += goertzel (b, f, 48000.0, 0, 12000);
            for (double f = f0 * 2.5; f < 14000.0; f *= 1.12)  hi += goertzel (b, f, 48000.0, 0, 12000);
            return hi / std::max (1e-12, lo);
        };
        const double bs = bright (soft), bh = bright (hard);
        std::printf ("    %-6s upper/body at %.0f Hz: soft %.3f, hard %.3f\\n", channelName (c), f0, bs, bh);`,
   "brightness measure"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
writeE(); writeT();
console.log("patched OK");
