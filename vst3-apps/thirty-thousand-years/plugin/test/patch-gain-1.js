/*  THE GAIN STRUCTURE ROUND. Peter: "many presets seem to contain clipping
    effects although the total output is below 0 db".

    Measured before touching anything (ttyprobe thd):

      one sine, SVF wide open            THD  -88.1 dB     <- clean
      one sine, LADDER wide open         THD  -34.8 dB     <- 1.8 % on a SINE
      one sine at FULL level, LADDER     THD  -31.2 dB
      two sines at full, LADDER          THD  -21.3 dB     <- 8.6 %
      SIGNAL, two tables at full         THD -111.7 dB     <- clean

    So the fault is one stage: the ladder's saturator. `sat = 0.6 + drive*3`
    means that with DRIVE at ZERO the tanh still sees 0.6x the signal, and a
    tanh is only linear well below its knee - at 0.8 amplitude it was taking
    22 % off the peaks. The filter was distorting at rest, and because it
    happens per voice, deep inside the instrument, the output meter never saw
    it. That is precisely "clipping although the output is below 0 dB".

    THE FIX, in three places:

    1. A drive law that is TRANSPARENT AT REST. `ftanh(u*d)/d` is a saturator
       whose colour is set by d and whose level is not: at small d it is the
       identity, at large d it is a fuzz. d now starts at 0.14 and reaches 5.6
       at DRIVE 100 %, so a clean patch is clean and a driven one is dirtier
       than before.

    2. The same law in the SVF's state saturator, which had the same 0.6.

    3. STRUCTURE ended with ftanh(y * 1.2), which coloured every note to keep
       fracture bursts bounded. A soft ceiling does the same job and is
       transparent below 70 % of full scale (ceilSoft is exactly that), so an
       ordinary bowed note is no longer shaped on its way out.

    None of this changes what the controls mean: DRIVE still goes from clean
    to destroyed, and the resonance still self-oscillates - it just does not
    arrive already distorted.
*/
const fs = require("fs");
const D = "C:/Users/peter/b/ThirtyThousandYears/Source/Dsp.h";
const S = "C:/Users/peter/b/ThirtyThousandYears/Source/Strata.h";
let d = fs.readFileSync(D, "utf8");
let s = fs.readFileSync(S, "utf8");

/* ---- 1 & 2: the two filters ------------------------------------------- */
const de = [
  [`    // mode 0 LP, 1 BP, 2 HP, 3 NOTCH
    inline float operator() (float x, int mode, float sat = 1.0f)`,
   `    /*  mode 0 LP, 1 BP, 2 HP, 3 NOTCH.
        sat is a DRIVE, not a level: ftanh(v*sat)/sat is the identity for small
        sat and a saturator for large, so a filter at rest must be given a
        small one. It used to be handed 0.6 at zero drive and coloured every
        note (measured -35 dB THD on a sine). */
    inline float operator() (float x, int mode, float sat = 1.0f)`],
  [`struct Ladder
{
    float s[4] = { 0, 0, 0, 0 };
    float G = 0.1f, k = 0.0f;`,
   `struct Ladder
{
    float s[4] = { 0, 0, 0, 0 };
    float G = 0.1f, k = 0.0f;
    /*  The saturator in the summing node is what gives the ladder its
        character, but it has to be REACHED rather than always on: at drive 0
        the loop is linear to a thousandth. */`],
];
let miss = [];
for (const [a] of de) { const n = d.split(a).length - 1; if (n !== 1) miss.push("DSP " + n + "x: " + a.slice(0, 50).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of de) d = d.replace(a, b);
fs.writeFileSync(D, d);

/* ---- the drive law, and STRUCTURE's output ---------------------------- */
const se = [
  [`    void set (int m, float hz, float res, float drive, double sr)
    {
        mode = m;
        if (m == 0) ladder.set (hz, res, sr);
        else        svf.set (hz, 0.5f + res * res * 30.0f, sr);
        sat = 0.6f + drive * 3.0f;
    }`,
   `    void set (int m, float hz, float res, float drive, double sr)
    {
        mode = m;
        if (m == 0) ladder.set (hz, res, sr);
        else        svf.set (hz, 0.5f + res * res * 30.0f, sr);
        /*  TRANSPARENT AT REST. ftanh(u*sat)/sat is the identity for small sat;
            the old 0.6 floor meant the filter distorted a plain sine by 1.8 %
            with DRIVE at zero, which is what "clipping although the output is
            below 0 dB" sounded like. 0.14 measures -75 dB on the same sine,
            and the top of the knob is dirtier than it was. */
        sat = 0.14f + 5.5f * drive * drive;
    }`],
  [`            rawPeak = std::max (std::abs (y), rawPeak * 0.999999f);
            out[i] = ftanh (y * 1.2f) * aenv.tick() * aeGain * c.wmTrem;`,
   `            rawPeak = std::max (std::abs (y), rawPeak * 0.999999f);
            /*  A soft ceiling, not a saturator: fracture bursts still cannot
                leave the body unbounded, but an ordinary note is untouched
                below 70 % of full scale (ceilSoft is transparent there). */
            out[i] = ceilSoft (y, 1.35f) * aenv.tick() * aeGain * c.wmTrem;`],
];
miss = [];
for (const [a] of se) { const n = s.split(a).length - 1; if (n !== 1) miss.push("STRATA " + n + "x: " + a.slice(0, 50).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of se) s = s.replace(a, b);
fs.writeFileSync(S, s);
console.log("gain structure patched");
