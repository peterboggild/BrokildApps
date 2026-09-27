/*  The late field, rebuilt as a proper tank.

    MEASURED FAULTS (test/quality.cpp section 7):
      - total loop delay 95-242 ms against the RT60/2.2 the decay needs, so the
        network's own modes are resolvable: spectral ripple 6.1-10.7 dB where a
        diffuse field is exactly 5.57
      - normalised echo density 0.51 at 50 ms in the hall, where 1.0 is "merged"
      - and the delay modulation that used to smear the modes was removed when
        the lines went integer, which left them stationary and therefore audible
        as pitch

    THE FIX, all three at once:

    1. THREE SERIES ALLPASSES INSIDE EVERY LINE. An allpass is unity gain at
       every frequency, so it cannot change the decay, but its length counts
       towards the loop's total - which IS the modal density - and it splits
       every echo passing through it into a train, which is the echo density.
       Lengths at 0.85, 0.65 and 0.5 of the line's own, so the loop total is
       about three times what it was, and each one taken to a prime so nothing
       lines up with anything.

    2. THE MODULATION BACK, through the sinc reader. That is what made it
       lossless: the old linear read inside the loop cost 3-5 dB of the field's
       energy per pass. 1.5 samples at a third of a hertz is 0.006 % of pitch
       deviation - inaudible as wobble, and over seconds it smears every
       remaining mode across its neighbours.

    3. A LONGER INPUT DIFFUSER, four allpasses rather than two, which is what
       the sparse first 50 ms in the hall was asking for.

    The decay per pass now has to be charged over the line PLUS its allpasses,
    and the level calibration's total delay likewise. Both are one term each, and
    the efficiency the engine measures at prepare catches whatever is left.
*/
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(rel + ": " + c + " for " + a.slice(0, 70)); return; } s = s.split(a).join(b); };
  fn(rep);
  if (misses.length === 0) fs.writeFileSync(p, s);
}

edit("Source/Engine.h", rep => {
  rep(`struct RoomField
{
    static constexpr int N = 16;
    std::array<DelayLine, N> line;
    std::array<int, N> len {};
    std::array<BandFilter, N> loss;
    std::array<float, N> out {};          // this sample's line outputs
    float y = 0;                          // the observed sum, this sample
    // input side
    int preDelay = 0;
    float apBuf1[512] = {}; int ap1Len = 0, ap1W = 0;
    float apBuf2[1024] = {}; int ap2Len = 0, ap2W = 0;`,
`struct RoomField
{
    static constexpr int N = 16;
    static constexpr int NAP = 3;         // allpasses inside each line
    static constexpr int NIN = 4;         // allpasses on the way in
    std::array<DelayLine, N> line;
    std::array<int, N> len {};
    std::array<BandFilter, N> loss;
    std::array<float, N> out {};          // this sample's line outputs
    float y = 0;                          // the observed sum, this sample

    /*  The tank. Three allpasses in series inside every line: unity gain, so the
        decay is untouched, but they carry the loop's modal density and multiply
        its echo density. apTotal is what each line's loop grew by, which the
        decay per pass and the level calibration both have to know. */
    std::array<std::vector<float>, N * NAP> ap;
    std::array<int, N * NAP> apLen {}, apW {};
    std::array<int, N> apTotal {};
    float apG = 0.62f;
    // and the modulation that keeps the remaining modes from standing still
    float modPhase[N] = {}, modRate[N] = {}, modDepth = 1.5f;

    // input side
    int preDelay = 0;
    std::array<std::vector<float>, NIN> inAp;
    std::array<int, NIN> inApLen {}, inApW {};`);
});

edit("Source/Engine.cpp", rep => {
  // ---- allocate the tank
  rep(`        for (int i = 0; i < RoomField::N; ++i)
        {
            const float spread = 0.55f + 0.95f * (float) i / (RoomField::N - 1);
            int len = (int) std::lround (tau * fs * spread * (1.0f + 0.04f * (rnd() - 0.5f)));
            len = nextPrime (std::max (len, 32));
            F.len[(size_t) i] = len;
            F.line[(size_t) i].prepare (len + 8);
            F.loss[(size_t) i].setCoeffs ((float) fs);
        }
        F.ap1Len = std::min (511, std::max (16, (int) (0.35f * tau * fs)));
        F.ap2Len = std::min (1023, std::max (24, (int) (0.83f * tau * fs)));`,
`        for (int i = 0; i < RoomField::N; ++i)
        {
            const float spread = 0.55f + 0.95f * (float) i / (RoomField::N - 1);
            int len = (int) std::lround (tau * fs * spread * (1.0f + 0.04f * (rnd() - 0.5f)));
            len = nextPrime (std::max (len, 32));
            F.len[(size_t) i] = len;
            F.line[(size_t) i].prepare (len + 4 * FracDelay::TAPS);
            F.loss[(size_t) i].setCoeffs ((float) fs);
            F.modRate[i] = 0.17f + 0.31f * rnd();
            F.modPhase[i] = 2.0f * PI * rnd();

            // three allpasses per line, each a prime so nothing lines up
            const float frac[RoomField::NAP] = { 0.85f, 0.65f, 0.5f };
            F.apTotal[(size_t) i] = 0;
            for (int q = 0; q < RoomField::NAP; ++q)
            {
                int al = nextPrime (std::max (13, (int) std::lround (frac[q] * (float) len * (1.0f + 0.05f * (rnd() - 0.5f)))));
                // never the same length as its own line, or the two resonate together
                while (al == len) al = nextPrime (al + 1);
                const size_t idx = (size_t) (i * RoomField::NAP + q);
                F.apLen[idx] = al;
                F.apW[idx] = 0;
                F.ap[idx].assign ((size_t) al, 0.0f);
                F.apTotal[(size_t) i] += al;
            }
        }
        // the way in: four allpasses, spread over the mean free path
        {
            const float inFrac[RoomField::NIN] = { 0.21f, 0.37f, 0.58f, 0.83f };
            for (int q = 0; q < RoomField::NIN; ++q)
            {
                const int al = nextPrime (std::max (11, (int) (inFrac[q] * tau * fs)));
                F.inApLen[(size_t) q] = al;
                F.inApW[(size_t) q] = 0;
                F.inAp[(size_t) q].assign ((size_t) al, 0.0f);
            }
        }`);

  // ---- reset
  rep(`        F.feed.clear(); F.y = 0;
        std::fill (std::begin (F.apBuf1), std::end (F.apBuf1), 0.0f); F.ap1W = 0;
        std::fill (std::begin (F.apBuf2), std::end (F.apBuf2), 0.0f); F.ap2W = 0;`,
`        F.feed.clear(); F.y = 0;
        for (size_t q = 0; q < F.ap.size(); ++q) { std::fill (F.ap[q].begin(), F.ap[q].end(), 0.0f); F.apW[q] = 0; }
        for (size_t q = 0; q < F.inAp.size(); ++q) { std::fill (F.inAp[q].begin(), F.inAp[q].end(), 0.0f); F.inApW[q] = 0; }`);

  // ---- the decay is charged over the line AND its allpasses
  rep(`                for (int b = 0; b < NBAND; ++b) lossDb[b] = -60.0f * (float) F.len[(size_t) i] / ((float) fs * F.rt60[dbgFlatLoss ? 3 : b]);`,
      `                // the loop is the line plus the three allpasses inside it
                const float loopLen = (float) (F.len[(size_t) i] + F.apTotal[(size_t) i]);
                for (int b = 0; b < NBAND; ++b) lossDb[b] = -60.0f * loopLen / ((float) fs * F.rt60[dbgFlatLoss ? 3 : b]);`);

  // ---- the level calibration's total delay likewise
  rep(`            float Ltot = 0; for (int i = 0; i < RoomField::N; ++i) Ltot += (float) F.len[(size_t) i];
            const float G = (float) fs * F.rt60[3] / (13.8f * Ltot);`,
      `            float Ltot = 0;
            for (int i = 0; i < RoomField::N; ++i) Ltot += (float) (F.len[(size_t) i] + F.apTotal[(size_t) i]);
            const float G = (float) fs * F.rt60[3] / (13.8f * Ltot);`);

  // ---- the efficiency measurement must run the same loop the audio does
  rep(`    for (int i = 0; i < RoomField::N; ++i)
    {
        F.line[(size_t) i].clear(); F.loss[(size_t) i].reset();
        float db[NBAND]; for (int b = 0; b < NBAND; ++b) db[b] = -60.0f * (float) F.len[(size_t) i] / ((float) fs * RT);
        F.loss[(size_t) i].setBandsDb (db);
    }
    double rendered = 0;
    const int total = (int) (2.2 * RT * fs);
    float Ltot = 0; for (int i = 0; i < RoomField::N; ++i) Ltot += (float) F.len[(size_t) i];`,
`    for (int i = 0; i < RoomField::N; ++i)
    {
        F.line[(size_t) i].clear(); F.loss[(size_t) i].reset();
        const float loopLen = (float) (F.len[(size_t) i] + F.apTotal[(size_t) i]);
        float db[NBAND]; for (int b = 0; b < NBAND; ++b) db[b] = -60.0f * loopLen / ((float) fs * RT);
        F.loss[(size_t) i].setBandsDb (db);
    }
    for (size_t q = 0; q < F.ap.size(); ++q) { std::fill (F.ap[q].begin(), F.ap[q].end(), 0.0f); F.apW[q] = 0; }
    double rendered = 0;
    const int total = (int) (2.2 * RT * fs);
    float Ltot = 0; for (int i = 0; i < RoomField::N; ++i) Ltot += (float) (F.len[(size_t) i] + F.apTotal[(size_t) i]);`);

  rep(`        float out[16];
        for (int k = 0; k < 16; ++k) out[k] = F.loss[(size_t) k].process (F.line[(size_t) k].readInt (F.len[(size_t) k] - 1));`,
`        float out[16];
        for (int k = 0; k < 16; ++k)
        {
            float v = F.loss[(size_t) k].process (F.line[(size_t) k].readInt (F.len[(size_t) k] - 1));
            for (int q = 0; q < RoomField::NAP; ++q)
            {
                const size_t idx = (size_t) (k * RoomField::NAP + q);
                const float b = F.ap[idx][(size_t) F.apW[idx]];
                const float y = -F.apG * v + b;
                F.ap[idx][(size_t) F.apW[idx]] = v + F.apG * y;
                if (++F.apW[idx] >= F.apLen[idx]) F.apW[idx] = 0;
                v = y;
            }
            out[k] = v;
        }`);

  // ---- the audio loop: modulation, then the allpasses
  rep(`            for (int k = 0; k < RoomField::N; ++k)
            {
                // integer delay on purpose: any interpolation here is a lowpass
                // applied once per pass, hundreds of times a second - it took
                // 3-5x of the field's energy before it was measured
                float v = F.line[(size_t) k].readInt (F.len[(size_t) k] - 1);
                v = F.loss[(size_t) k].process (v);
                F.out[(size_t) k] = v; y += SGN_OUT[k] * v;
            }`,
`            for (int k = 0; k < RoomField::N; ++k)
            {
                /*  Modulated, and read with the sinc. The old LINEAR read here
                    cost 3-5 dB of the field's energy per pass, which is why the
                    modulation was taken out; with a lossless reader it is free,
                    and it is what keeps the network's own modes from standing
                    still and being heard as pitch. */
                F.modPhase[k] += phaseInc * F.modRate[k];
                if (F.modPhase[k] > 2.0f * PI) F.modPhase[k] -= 2.0f * PI;
                const float d = (float) (F.len[(size_t) k] - 1) + F.modDepth * std::sin (F.modPhase[k]);
                float v = F.line[(size_t) k].read (d);
                v = F.loss[(size_t) k].process (v);
                // the three allpasses inside this line
                for (int q = 0; q < RoomField::NAP; ++q)
                {
                    const size_t idx = (size_t) (k * RoomField::NAP + q);
                    const float b = F.ap[idx][(size_t) F.apW[idx]];
                    const float yy = -F.apG * v + b;
                    F.ap[idx][(size_t) F.apW[idx]] = v + F.apG * yy;
                    if (++F.apW[idx] >= F.apLen[idx]) F.apW[idx] = 0;
                    v = yy;
                }
                F.out[(size_t) k] = v; y += SGN_OUT[k] * v;
            }`);

  rep(`    const float kw = 1.0f - std::exp (-1.0f / (0.03f * (float) fs));`,
      `    const float phaseInc = 2.0f * PI / (float) fs;
    const float kw = 1.0f - std::exp (-1.0f / (0.03f * (float) fs));`);

  // ---- the input diffuser, four deep
  rep(`            float sIn = inj[r];
            // two Schroeder allpasses spread the injection in time
            { float b = F.apBuf1[F.ap1W]; float y = -0.6f * sIn + b; F.apBuf1[F.ap1W] = sIn + 0.6f * y; if (++F.ap1W >= F.ap1Len) F.ap1W = 0; sIn = y; }
            { float b = F.apBuf2[F.ap2W]; float y = -0.55f * sIn + b; F.apBuf2[F.ap2W] = sIn + 0.55f * y; if (++F.ap2W >= F.ap2Len) F.ap2W = 0; sIn = y; }
            inj[r] = sIn;`,
`            float sIn = inj[r];
            // four Schroeder allpasses smear the injection before it enters the
            // tank: the hall's first 50 ms measured at half the echo density it
            // needed, which is a sparse injection, not a sparse loop
            static const float inG[RoomField::NIN] = { 0.62f, 0.58f, 0.55f, 0.52f };
            for (int q = 0; q < RoomField::NIN; ++q)
            {
                const float b = F.inAp[(size_t) q][(size_t) F.inApW[(size_t) q]];
                const float y = -inG[q] * sIn + b;
                F.inAp[(size_t) q][(size_t) F.inApW[(size_t) q]] = sIn + inG[q] * y;
                if (++F.inApW[(size_t) q] >= F.inApLen[(size_t) q]) F.inApW[(size_t) q] = 0;
                sIn = y;
            }
            inj[r] = sIn;`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("the tank: allpasses inside every line, modulation back, a four-deep input diffuser");
