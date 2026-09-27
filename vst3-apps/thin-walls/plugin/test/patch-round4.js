// Round 4: snap the room weights on the first parameter set; self-measured
// network efficiency at prepare; the late field carries what the images do not.
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
  rep(`    float inGain = 0;
    float rt60[NBAND] = {};`, `    float inGain = 0;                     // for a source in this room, full field
    float inGainSrc = 0;                  // the same minus what the images already render
    float efficiency = 1;                 // measured at prepare: rendered power / formula
    float rt60[NBAND] = {};`);
  rep(`    void rebuildCouplings();`, `    void rebuildCouplings();
    void measureEfficiency (int r);`);
});

edit("Source/Engine.cpp", rep => {
  // (a) snap on fresh
  rep(`        mixNow = p.mix; trimOut = dbToLin (p.outputDb);
        paramsFresh = false;`,
      `        mixNow = p.mix; trimOut = dbToLin (p.outputDb);
        const int rl = std::max (0, roomOf (lisPos.x, lisPos.y)), rs = std::max (0, roomOf (srcPos.x, srcPos.y));
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            rooms[(size_t) r].weight = rooms[(size_t) r].weightTarget = (r == rl) ? 1.0f : 0.0f;
            srcW[r] = (r == rs) ? 1.0f : 0.0f;
        }
        paramsFresh = false;`);

  // (b) measured efficiency: run the network alone with flat losses at RT 1 s
  rep(`    couplings.clear();
    for (int a = 0; a < NUM_ROOMS; ++a)
        for (int b = 0; b < NUM_ROOMS; ++b)
        {
            if (a == b) continue;
            int axis; float pos, s0, s1, h;
            if (! sharedWall (a, b, axis, pos, s0, s1, h)) continue;
            Coupling c; c.from = a; c.to = b; c.filt.setCoeffs ((float) fs);
            couplings.push_back (c);
        }

    reset();`,
      `    for (int r = 0; r < NUM_ROOMS; ++r) measureEfficiency (r);

    reset();`);
  rep(`void Engine::rebuildCouplings()
{`, `/*  The steady-state formula G = fs RT / (13.8 L) assumes the rendered signal
    sees the stored energy uniformly. It does not quite: the sign-flipped
    Hadamard leaves some correlation between the lines a pair readout sums, and
    a first build measured 0.6 of the formula. So the efficiency is MEASURED
    here, once per room at prepare, with flat losses at a 1 s decay: an impulse
    in, the eight pair signals' energy out, against the formula's prediction. */
void Engine::measureEfficiency (int r)
{
    RoomField& F = rooms[(size_t) r];
    const float RT = 1.0f;
    for (int i = 0; i < RoomField::N; ++i)
    {
        F.line[(size_t) i].clear(); F.loss[(size_t) i].reset();
        float db[NBAND]; for (int b = 0; b < NBAND; ++b) db[b] = -60.0f * (float) F.len[(size_t) i] / ((float) fs * RT);
        F.loss[(size_t) i].setBandsDb (db);
    }
    static const float SGN_SRC0[16] = { 1,-1, 1, 1,-1, 1,-1,-1, 1,-1,-1, 1, 1,-1, 1,-1 };
    static const float SGN_MIX0[16] = { 1, 1,-1, 1,-1,-1, 1, 1,-1, 1, 1,-1, 1,-1,-1,-1 };
    double rendered = 0;
    const int total = (int) (2.2 * RT * fs);
    float Ltot = 0; for (int i = 0; i < RoomField::N; ++i) Ltot += (float) F.len[(size_t) i];
    for (int n = 0; n < total; ++n)
    {
        float out[16];
        for (int k = 0; k < 16; ++k) out[k] = F.loss[(size_t) k].process (F.line[(size_t) k].readInt (F.len[(size_t) k] - 1));
        float v[16]; for (int k = 0; k < 16; ++k) v[k] = out[k];
        for (int len = 1; len < 16; len <<= 1)
            for (int a = 0; a < 16; a += len << 1)
                for (int b = a; b < a + len; ++b) { const float p = v[b], q = v[b + len]; v[b] = p + q; v[b + len] = p - q; }
        const float inj = n == 0 ? 1.0f : 0.0f;
        float w[16];
        for (int k = 0; k < 16; ++k) { w[k] = 0.25f * SGN_MIX0[k] * v[k] + 0.25f * SGN_SRC0[k] * inj; F.line[(size_t) k].write (w[k]); }
        // what the diffuse render would sum: eight pairs at 1/4, i.e. sum of pair^2 / 16
        for (int j = 0; j < 8; ++j) { const float s = (w[2 * j] + w[2 * j + 1]) * 0.25f; rendered += (double) s * s; }
    }
    const double formula = (double) fs * RT / (13.8 * Ltot);
    F.efficiency = (float) std::max (0.05, std::min (4.0, rendered / formula));
    for (int i = 0; i < RoomField::N; ++i) { F.line[(size_t) i].clear(); F.loss[(size_t) i].reset(); }
}

void Engine::rebuildCouplings()
{`);
  rep(`        const float tail = std::exp (-13.8f * F.tEarly / F.rt60[3]);
        F.inGain = std::sqrt (16.0f * PI * tail / (A[3] * G));`,
      `        F.inGain = std::sqrt (16.0f * PI / (A[3] * G * F.efficiency));
        F.inGainSrc = F.inGain;`);

  // (c) the late field carries what the images do not, per geometry
  rep(`    // the room weights for the diffuse render
    for (int r = 0; r < NUM_ROOMS; ++r) rooms[(size_t) r].weightTarget = (r == rl) ? 1.0f : 0.0f;`,
      `    // the room weights for the diffuse render
    for (int r = 0; r < NUM_ROOMS; ++r) rooms[(size_t) r].weightTarget = (r == rl) ? 1.0f : 0.0f;

    // the source room's late field: 16 pi / A of reverberant energy in all,
    // minus what the rendered images already carry (at 1 kHz)
    if (rs >= 0)
    {
        RoomField& F = rooms[(size_t) rs];
        double early = 0;
        for (int i = 0; i < nspecs; ++i)
        {
            const PathSpec& s = specs[(size_t) i];
            if (s.kind == PathKind::Refl1 || s.kind == PathKind::Refl2)
            { const double g = s.gain * std::pow (10.0, s.bandDb[3] / 20.0); early += g * g; }
        }
        const double all = 16.0 * PI / F.absorptionArea[3];
        const double fraction = std::max (0.10, std::min (1.0, 1.0 - early / all));
        F.inGainSrc = F.inGain * (float) std::sqrt (fraction);
    }`);
  rep(`                float s = F.inFilt.process (F.pre.readInt (F.preDelay)) * F.inGain * srcW[r];`,
      `                float s = F.inFilt.process (F.pre.readInt (F.preDelay)) * F.inGainSrc * srcW[r];`);
});

edit("test/probe.cpp", rep => {
  rep(`    if (what == "rtflat")`, `    if (what == "drr")
    {
        // direct, images and late field at the critical distance, each alone
        for (int m = 0; m < NUM_MATERIALS; ++m)
        {
            const float A = ROOMS[0].surface() * MATERIAL_ALPHA[m][3];
            const double rc = std::sqrt (A / (16.0 * 3.14159265));
            double en[3];
            for (int part = 0; part < 3; ++part)
            {
                Engine e; e.prepare (fs, 256);
                Params p; p.material[0] = p.material[1] = p.material[2] = m; p.srcType = 0;
                p.door[0] = p.door[1] = p.door[2] = 0;
                p.lisX = 3.0f; p.lisY = 2.5f; p.lisYaw = 0; p.srcX = 3.0f + (float) rc; p.srcY = 2.5f; p.srcZ = EAR_HEIGHT;
                p.directDb = part == 0 ? 0.0f : -120.0f; p.earlyDb = part == 1 ? 0.0f : -120.0f; p.reverbDb = part == 2 ? 0.0f : -120.0f;
                std::vector<float> L, R; run (e, p, L, R, (int) (6 * fs), 10);
                double s = 0; for (size_t i = 0; i < L.size(); ++i) s += (double) L[i] * L[i] + (double) R[i] * R[i];
                en[part] = 0.5 * s;
                if (part == 2) std::printf ("   [efficiency %.3f, inGain %.3f, inGainSrc %.3f]\\n", e.field (0).efficiency, e.field (0).inGain, e.field (0).inGainSrc);
            }
            std::printf ("%s at rc = %.2f m: direct %.4f, images %.4f, late %.4f  -> DRR %.1f dB (0 wanted), late/(dir) %.1f dB, images/dir %.1f dB\\n",
                         MATERIAL_NAMES[m], rc, en[0], en[1], en[2], 10 * std::log10 (en[0] / (en[1] + en[2])), 10 * std::log10 (en[2] / en[0]), 10 * std::log10 (en[1] / en[0]));
        }
        return 0;
    }

    if (what == "rtflat")`);
});

edit("test/bench.cpp", rep => {
  rep(`            const double sEarly = slopeDbPerS (0.03, 0.12), sLate = slopeDbPerS (2.0, 4.0);
            const double tLate = -60.0 / sLate;
            char buf[200]; std::snprintf (buf, sizeof buf, "double slope: early %.0f dB/s (small room Eyring %.2f s), late %.0f dB/s = %.1f s (hall Eyring %.1f s)", sEarly, eySmall[3], sLate, tLate, eyGiant[3]);
            check (sEarly < sLate - 40.0 && tLate > 0.6 * eyGiant[3] && tLate < 1.6 * eyGiant[3], buf);`,
      `            const double drop40 = db (edc[(size_t) (0.040 * fs)]) - db (edc[(size_t) (0.003 * fs)]);
            const double sLate = slopeDbPerS (2.0, 4.0);
            const double tLate = -60.0 / sLate;
            char buf[220]; std::snprintf (buf, sizeof buf, "double slope: the dead room's own field is %.0f dB down by 40 ms (Eyring %.2f s), then the hall's returns at %.0f dB/s = %.1f s (hall Eyring %.1f s)", -drop40, eySmall[3], sLate, tLate, eyGiant[3]);
            check (drop40 < -12.0 && tLate > 0.6 * eyGiant[3] && tLate < 1.6 * eyGiant[3], buf);`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("round 4 applied");
