/*  The network calibrates itself now, for decay as well as level.

    Why it needs to. An allpass holds the signal for its own length ON AVERAGE,
    which is what the loss is charged over, but it does not hold all of it for
    that long: it spreads the energy over a train, and the latest arrivals have
    had the loss applied fewer times per second than the earliest. So the tail
    decays slightly slower than the mean-delay model says. Measured across the
    twelve room-and-material cases the bias ran from 0 to 20 %, worst where the
    chain is long against the band's own decay.

    Rather than model that, measure it. At prepare each room runs its own network
    at four design decays spanning the range, with the chain sized for each as it
    would be in use, and records two numbers: how far the measured decay misses
    the design (the trim) and how much rendered energy the formula's steady-state
    gain really delivers (the efficiency, which was already measured, now at four
    points rather than one). Both are then interpolated in log decay at runtime.

    The Martian Gain principle, applied to a reverberator: the gain is measured,
    not modelled.
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
  rep(`    float inGain = 0;                     // for a source in this room, full field
    float efficiency = 1;                 // measured at prepare: rendered power / formula`,
`    float inGain = 0;                     // for a source in this room, full field
    /*  Measured at prepare, at four design decays, with the diffuser chain sized
        for each as it would be in use. calEff is the rendered energy against the
        steady-state formula; calTrim is how far the network's own decay misses
        the one it was designed for. Both interpolated in log decay at runtime. */
    static constexpr int NCAL = 4;
    float calRt[NCAL]   = { 0.12f, 0.45f, 1.5f, 5.0f };
    float calEff[NCAL]  = { 1, 1, 1, 1 };
    float calTrim[NCAL] = { 1, 1, 1, 1 };
    float calAt (const float* tbl, float rt) const
    {
        if (rt <= calRt[0]) return tbl[0];
        if (rt >= calRt[NCAL - 1]) return tbl[NCAL - 1];
        for (int i = 1; i < NCAL; ++i)
            if (rt <= calRt[i])
            {
                const float u = (std::log (rt) - std::log (calRt[i - 1])) / (std::log (calRt[i]) - std::log (calRt[i - 1]));
                return tbl[i - 1] + u * (tbl[i] - tbl[i - 1]);
            }
        return tbl[NCAL - 1];
    }
    float efficiency = 1;                 // the value in use, for the probes to read`);
});

edit("Source/Engine.cpp", rep => {
  // the calibration itself, replacing the single-point efficiency measurement
  rep(`void Engine::measureEfficiency (int r)
{`,
`void Engine::measureEfficiency (int r)
{
    RoomField& FF = rooms[(size_t) r];
    // four design decays, each with the chain it would really have
    for (int c = 0; c < RoomField::NCAL; ++c)
    {
        const float RTc = FF.calRt[c];
        FF.sizeDiffusers (fs, RTc);
        for (int i = 0; i < RoomField::N; ++i)
        {
            FF.line[(size_t) i].clear(); FF.loss[(size_t) i].reset();
            const float loopLen = (float) FF.len[(size_t) i] + FF.apTotal[(size_t) i];
            float db[NBAND]; for (int b = 0; b < NBAND; ++b) db[b] = -60.0f * loopLen / ((float) fs * RTc);
            FF.loss[(size_t) i].setBandsDb (db);
        }
        for (size_t q = 0; q < FF.ap.size(); ++q) { std::fill (FF.ap[q].begin(), FF.ap[q].end(), 0.0f); FF.apW[q] = 0; }

        float Lt = 0; for (int i = 0; i < RoomField::N; ++i) Lt += (float) FF.len[(size_t) i] + FF.apTotal[(size_t) i];
        const int total = (int) ((2.6 * RTc + 0.15) * fs);
        std::vector<float> trace ((size_t) total, 0.0f);
        double rendered = 0;
        for (int n = 0; n < total; ++n)
        {
            float out[16];
            for (int k = 0; k < 16; ++k)
            {
                float v = FF.loss[(size_t) k].process (FF.line[(size_t) k].readInt (FF.len[(size_t) k] - 1));
                for (int q = 0; q < RoomField::NAP; ++q)
                {
                    const size_t idx = (size_t) (k * RoomField::NAP + q);
                    if (FF.apLen[idx] <= 0) continue;
                    const float b = FF.ap[idx][(size_t) FF.apW[idx]];
                    const float y = -FF.apG * v + b;
                    FF.ap[idx][(size_t) FF.apW[idx]] = v + FF.apG * y;
                    if (++FF.apW[idx] >= FF.apLen[idx]) FF.apW[idx] = 0;
                    v = y;
                }
                out[k] = v;
            }
            float v16[16]; for (int k = 0; k < 16; ++k) v16[k] = out[k];
            for (int len = 1; len < 16; len <<= 1)
                for (int a = 0; a < 16; a += len << 1)
                    for (int b = a; b < a + len; ++b) { const float p = v16[b], q2 = v16[b + len]; v16[b] = p + q2; v16[b + len] = p - q2; }
            const float inj = n == 0 ? 1.0f : 0.0f;
            float w[16];
            for (int k = 0; k < 16; ++k) { w[k] = 0.25f * SGN_MIX[k] * v16[k] + 0.25f * SGN_SRC[r][k] * inj; FF.line[(size_t) k].write (w[k]); }
            float s = 0;
            for (int j = 0; j < 8; ++j) { const float pr = (w[2 * j] + w[2 * j + 1]) * 0.25f; rendered += (double) pr * pr; s += pr; }
            trace[(size_t) n] = s;
        }
        FF.calEff[c] = (float) std::max (0.05, std::min (4.0, rendered / ((double) fs * RTc / (13.8 * Lt))));

        // the decay the network really has, by backward integration
        std::vector<double> edc ((size_t) total, 0.0);
        double acc = 0;
        for (int i = total - 1; i >= 0; --i) { acc += (double) trace[(size_t) i] * trace[(size_t) i]; edc[(size_t) i] = acc; }
        const int from = (int) (0.02 * fs);
        float trim = 1.0f;
        if (from < total && edc[(size_t) from] > 0)
        {
            const double top = 10.0 * std::log10 (edc[(size_t) from]);
            int i5 = -1, i25 = -1;
            for (int i = from; i < total; ++i)
            {
                const double d = 10.0 * std::log10 (std::max (edc[(size_t) i], 1e-300)) - top;
                if (i5 < 0 && d <= -5.0) i5 = i;
                if (d <= -25.0) { i25 = i; break; }
            }
            if (i5 > 0 && i25 > i5)
            {
                const double measured = 3.0 * (i25 - i5) / fs;
                trim = (float) std::max (0.5, std::min (2.0, measured / RTc));
            }
        }
        FF.calTrim[c] = trim;
        for (int i = 0; i < RoomField::N; ++i) { FF.line[(size_t) i].clear(); FF.loss[(size_t) i].reset(); }
        for (size_t q = 0; q < FF.ap.size(); ++q) { std::fill (FF.ap[q].begin(), FF.ap[q].end(), 0.0f); FF.apW[q] = 0; }
    }
    FF.efficiency = FF.calEff[1];
    return;
}

void Engine::measureEfficiencyOld (int r)
{`);

  // the runtime side: trim the design, and take the efficiency from the curve
  rep(`                // the loop is the line plus what its allpasses really hold
                const float loopLen = (float) F.len[(size_t) i] + F.apTotal[(size_t) i];
                for (int b = 0; b < NBAND; ++b) lossDb[b] = -60.0f * loopLen / ((float) fs * F.rt60[dbgFlatLoss ? 3 : b]);`,
`                // the loop is the line plus what its allpasses really hold, and
                // the design is trimmed by what this network was measured to do
                const float loopLen = (float) F.len[(size_t) i] + F.apTotal[(size_t) i];
                for (int b = 0; b < NBAND; ++b)
                {
                    const float want = F.rt60[dbgFlatLoss ? 3 : b];
                    lossDb[b] = -60.0f * loopLen * F.calAt (F.calTrim, want) / ((float) fs * want);
                }`);

  rep(`            F.inGain = std::sqrt (16.0f * PI / (A[3] * G * F.efficiency));`,
      `            F.efficiency = F.calAt (F.calEff, F.rt60[3]);
            F.inGain = std::sqrt (16.0f * PI / (A[3] * G * F.efficiency));`);
});

edit("Source/Engine.h", rep => {
  rep(`    void measureEfficiency (int r);`, `    void measureEfficiency (int r);
    void measureEfficiencyOld (int r);`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("the network measures its own decay and level at four design points");
