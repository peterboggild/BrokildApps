/*  Gain structure, round 3: the levels themselves.

    With the filter no longer saturating, the gain reduction it used to do by
    accident is gone - and the stage meters show where it went:

      STEEL CATHEDRAL     STRUCT bus 2.47   pre-limit 1.32   limiter -3.6 dB
      THE SEA IS MADE...  STRUCT bus 2.34   pre-limit 1.59   limiter -7.5 dB

    A stratum bus peaking at 2.5 BEFORE its own fader is not a gain structure;
    it means the fader spends its life undoing the voice, and the limiter
    finishes the job. -7.5 dB of limiting on a drone is the limiter becoming
    the sound, which is the same complaint one stage later.

    So the voices are trimmed to a defined level and the master makes it back:
      * STRUCTURE 0.75 -> 0.52. It was the hottest of the three and the one
        driving every over-level preset; a modal bank with many voices sums
        far more than a two-oscillator one.
      * MASS and SIGNAL 0.65 -> 0.58, a gentle trim so the four strata sit in
        the same window.
      * VOLUME's default 0.70 -> 0.82, which is +2.7 dB and puts the finished
        level back where it was. It is a per-instance control that no preset
        writes, so this moves every patch together and changes no balance.
*/
const fs = require("fs");
const S = "C:/Users/peter/b/ThirtyThousandYears/Source/Strata.h";
const M = "C:/Users/peter/b/ThirtyThousandYears/Source/Memory.h";
const PA = "C:/Users/peter/b/ThirtyThousandYears/Source/Params.h";
let s = fs.readFileSync(S, "utf8");
let m = fs.readFileSync(M, "utf8");
let pa = fs.readFileSync(PA, "utf8");

const se = [
  [`        const float uniNorm = uni == 1 ? 1.0f : 0.6f;
        const float aeGain = 0.65f * (0.35f + 0.65f * c.vel) * amd;`,
   `        const float uniNorm = uni == 1 ? 1.0f : 0.6f;
        /*  The voice's own level, chosen so a full chord lands near unity on the
            stratum bus and the channel fader works around unity rather than
            undoing the voice. */
        const float aeGain = 0.58f * (0.35f + 0.65f * c.vel) * amd;`],
  [`        const float aeGain = 0.65f * (0.35f + 0.65f * c.vel);
        for (int i = 0; i < n; ++i)
        {
            // PM operator`,
   `        const float aeGain = 0.58f * (0.35f + 0.65f * c.vel);
        for (int i = 0; i < n; ++i)
        {
            // PM operator`],
  [`        const float aeGain = 0.75f * (0.4f + 0.6f * c.vel);
        const float strikeLvl = p[P_st_strikelvl];`,
   `        /*  A modal bank sums many more contributors than a two-oscillator
            voice, so it needs the lowest per-voice level of the three: it was
            reaching 2.5 on its own bus and the limiter was taking 7.5 dB off
            the presets built on it. */
        const float aeGain = 0.52f * (0.4f + 0.6f * c.vel);
        const float strikeLvl = p[P_st_strikelvl];`],
];
let miss = [];
for (const [a] of se) { const n = s.split(a).length - 1; if (n !== 1) miss.push("STRATA " + n + "x: " + a.slice(0, 50).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of se) s = s.replace(a, b);
fs.writeFileSync(S, s);

const ma = `            const float norm = 2.0f / std::sqrt (std::max (1.0f, overlap));`;
const mb = `            const float norm = 1.8f / std::sqrt (std::max (1.0f, overlap));`;
if (m.split(ma).length !== 2) { console.log("MEMORY ANCHOR MISS"); process.exit(1); }
m = m.replace(ma, mb);
fs.writeFileSync(M, m);

const pb = `    X(volume,     "volume",     "VOLUME",          0.7f,  KP_VOL,  0, 0,     F_NS) \\`;
const pc = `    X(volume,     "volume",     "VOLUME",          0.82f, KP_VOL,  0, 0,     F_NS) \\`;
if (pa.split(pb).length !== 2) { console.log("PARAMS ANCHOR MISS"); process.exit(1); }
pa = pa.replace(pb, pc);
fs.writeFileSync(PA, pa);
console.log("gain structure round 3 patched");
