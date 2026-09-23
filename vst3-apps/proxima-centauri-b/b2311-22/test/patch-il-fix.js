/*  Three measured faults in the new code, all mine:

    1. KINSHIP SATURATED at its clamp for every pair. heardTotal/sentPower
       sums 67 resonators each with unity peak gain, so several landing on
       one strong partial makes the ratio exceed 1 whatever the pair — the
       number carried no information. Kinship is now what the name says and
       what spectralKinship() measures: the MEAN FRACTION OF ITS MODES that
       actually rang, in [0,1]. The reply's loudness mapping is rescaled to
       match.

    2. PLASTICITY WAS TWELVE TIMES TOO SLOW to be an experience. One accent
       step in twelve seconds of hard playing means Peter would never hear
       his own accent arrive. At full PLASTICITY a step now lands every few
       seconds of playing; at the 0.15 default it is the work of minutes.

    3. The reply's loudness followed sqrt(sentPow) with no floor, so a body
       that heard little answered with almost nothing. Kin still answer far
       louder than strangers — that is the point — but a stranger's reply is
       audible, as specced.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
  for (const [a, b, tag] of subs) {
    const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(tag + " x" + n); continue; }
    s = s.split(A).join(B);
  }
  return () => fs.writeFileSync(path, s);
}

const wC = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.cpp", [

[`    il.sentPow = il.sentPow * hd + a1 * (il.sentAcc * inv);
    il.sentAcc = 0.0f;
    il.lastKin = il.sentPow > 1e-10f
               ? clampf (heardTot / il.sentPow, 0.0f, 4.0f) : 0.0f;`,
`    il.sentPow = il.sentPow * hd + a1 * (il.sentAcc * inv);
    il.sentAcc = 0.0f;
    /*  MUTUAL INTELLIGIBILITY, and it must be the quantity the name claims:
        the mean fraction of its own modes that actually rang. Summing 67
        unity-gain resonators against the total power instead (the first
        version) saturates for every pair and says nothing. */
    if (il.sentPow > 1e-10f)
    {
        float kinAcc = 0.0f;
        for (int k = 0; k < os.nModes; ++k)
            kinAcc += clampf (il.heardPow[(size_t) k] / il.sentPow, 0.0f, 1.0f);
        il.lastKin = kinAcc / (float) os.nModes;
    }
    else il.lastKin = 0.0f;`, "kinship"],

[`        //  loudness: kin answer strongly, strangers faintly but never mutely
        const float kin = clampf (il.lastKin * 1.6f, 0.0f, 1.0f);
        const float A = clamp01 (p.converse) * 9.0f * std::sqrt (il.sentPow)
                      * (0.22f + 0.78f * kin);`,
`        //  loudness: kin answer strongly, strangers faintly but never mutely
        const float kin = clampf (il.lastKin * 5.0f, 0.0f, 1.0f);
        const float A = clamp01 (p.converse) * 16.0f * std::sqrt (il.sentPow)
                      * (0.30f + 0.70f * kin);`, "reply amp"],

[`            const float rate = p.plastic * p.plastic * 0.0016f;`,
`            /*  fast enough to be an EXPERIENCE: at full plasticity an accent
                step lands every few seconds of playing, at the default it is
                the work of minutes. A change nobody lives long enough to
                hear is not a feature. */
            const float rate = p.plastic * p.plastic * 0.02f;`, "plastic rate"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wC();
console.log("kinship is a fraction; the reply carries; the accent arrives in a lifetime");
