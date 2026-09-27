/*  Gain structure, round 2: give the filter a defined operating level.

    Round 1 made the saturator transparent at rest (-35 -> -59 dB on a sine),
    but two oscillators at full still measured -44 dB, because their sum is
    1.6 and the saturator's clean range is not that wide. A filter needs an
    operating level, the way a mixing desk does: the oscillators feed it at a
    nominal level and the makeup comes back after it. Since the filter is
    linear at rest, scaling in and out is exactly transparent.

      * IN_SCALE 0.55 before the filter, 1/0.55 after it. Nothing about the
        level knobs changes: they still behave as a mixer, and the voice comes
        out where it did.
      * The ladder's own bound becomes a SOFT CEILING at 2.0 rather than a
        drive-dependent tanh. It is transparent below 1.4, so it never touches
        an ordinary note, and it is what keeps a self-oscillating resonance at
        a musical amplitude instead of letting it run.
      * The drive law from round 1 stays: sat = 0.02 + 5.5 * DRIVE^2, the
        identity at rest and a fuzz at the top.

    The probe grew a self-oscillation measurement at the same time, because
    bounding the resonance differently is exactly the kind of change that
    trades one fault for another quietly.
*/
const fs = require("fs");
const D = "C:/Users/peter/b/ThirtyThousandYears/Source/Dsp.h";
const S = "C:/Users/peter/b/ThirtyThousandYears/Source/Strata.h";
const P = "C:/Users/peter/b/ThirtyThousandYears/test/probe.cpp";
let d = fs.readFileSync(D, "utf8");
let s = fs.readFileSync(S, "utf8");
let pr = fs.readFileSync(P, "utf8");

/* ---- the ladder's bound ------------------------------------------------ */
const da = `        const float u = (x - k * S) / (1.0f + k * G4);
        float v = ftanh (u * drive) / drive;`;
const db = `        const float u = (x - k * S) / (1.0f + k * G4);
        /*  Two jobs, kept apart. DRIVE colours the loop: ftanh(u*d)/d is the
            identity for small d, so at rest this is a clean filter. The soft
            ceiling bounds it: transparent below 1.4, asymptotic at 2.0, which
            is what stops a self-oscillating resonance from running away
            without colouring every note on the way. */
        float v = ceilSoft (ftanh (u * drive) / drive, 2.0f);`;
if (d.split(da).length !== 2) { console.log("DSP ANCHOR MISS"); process.exit(1); }
d = d.replace(da, db);
fs.writeFileSync(D, d);

/* ---- the drive law, and the filter's operating level -------------------- */
const se = [
  [`        sat = 0.14f + 5.5f * drive * drive;`,
   `        sat = 0.02f + 5.5f * drive * drive;`],
  [`struct FilterPair   // ladder or SVF, one selector
{
    Ladder ladder; Svf svf; int mode = 0; float sat = 1.0f;`,
   `struct FilterPair   // ladder or SVF, one selector
{
    /*  THE FILTER'S OPERATING LEVEL. The oscillators feed it at IN_SCALE and
        the makeup comes back after, the way a desk stages gain; since the
        filter is linear at rest this is exactly transparent. Without it, two
        oscillators at full summed to 1.6 and drove the saturator at DRIVE
        ZERO - measured 0.6 % distortion on what should be a clean patch. */
    static constexpr float IN_SCALE = 0.55f;
    static constexpr float MAKEUP = 1.0f / IN_SCALE;
    Ladder ladder; Svf svf; int mode = 0; float sat = 1.0f;`],
  [`    inline float operator() (float x)
    {
        if (mode == 0) return ladder (x, sat);
        return svf (x, mode - 1, sat);
    }`,
   `    inline float operator() (float x)
    {
        x *= IN_SCALE;
        return (mode == 0 ? ladder (x, sat) : svf (x, mode - 1, sat)) * MAKEUP;
    }`],
];
let miss = [];
for (const [a] of se) { const n = s.split(a).length - 1; if (n !== 1) miss.push("STRATA " + n + "x: " + a.slice(0, 50).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of se) s = s.replace(a, b);
fs.writeFileSync(S, s);

/* ---- the probe: self-oscillation must stay musical --------------------- */
const pa = `        /*  SIGNAL: two wavetables at full, which should not distort at all. */`;
const pb = `        /*  Bounding the resonance differently is exactly the kind of change that
            trades one fault for another, so it is measured in the same run. */
        std::printf ("\\nMASS: self-oscillation, no oscillators at all\\n");
        for (int fm : { 0, 1 })
        {
            Engine e; bareSine (e); e.prepare (48000.0, 512); bareSine (e);
            e.p[P_m_o1lvl] = 0; e.p[P_m_o2lvl] = 0; e.p[P_m_sub] = 0;
            e.p[P_m_fmode] = (float) fm; e.p[P_m_res] = 1.0f; e.p[P_m_cut] = 0.55f; e.p[P_m_fdrive] = 0;
            e.noteOn (45, 1.0f); render (e, 2.0); Take t = render (e, 1.0);
            float pk = 0; for (int i = 0; i < t.n(); ++i) pk = std::max (pk, std::abs (t.L[(size_t) i]));
            std::printf ("  %-8s RESONANCE 100 %%: bus peak %5.2f   out peak %5.2f   rms %.3f\\n",
                         fm == 0 ? "LADDER" : "SVF", e.stagePeak[Engine::ST_MASS], pk, rms (t.L));
        }

        /*  SIGNAL: two wavetables at full, which should not distort at all. */`;
if (pr.split(pa).length !== 2) { console.log("PROBE ANCHOR MISS"); process.exit(1); }
pr = pr.replace(pa, pb);
fs.writeFileSync(P, pr);
console.log("gain structure round 2 patched");
