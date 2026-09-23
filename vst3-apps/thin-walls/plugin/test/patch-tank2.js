/*  Two faults in the tank, both measured, both about the same thing: how long an
    allpass actually holds the signal.

    1. THE LOOP WAS UNDER-CHARGED. The decay per pass was computed over the line
       plus the RAW length of its allpasses. A Schroeder allpass of length La and
       gain g sends the signal round its own internal loop 1/(1-g^2) times on
       average, so its energy-weighted delay is La/(1-g^2) - for g = 0.62 that is
       1.625 La, not La. The loop was therefore 1.4x longer than the loss was
       charged for, and every decay came out long.

    2. A FIXED CHAIN CANNOT SERVE AN 80:1 RANGE OF DECAY. An allpass of length La
       and gain g rings at 20 log10(g) dB per La, so it has a decay of its own:
       60 La / (-20 log10 g) = 14.5 La seconds at g = 0.62. With La near 7 ms
       that is 100 ms, which is LONGER than the 85 ms an absorbing room should
       decay in - so the diffusers, not the room, were setting the tail. Measured:
       0.22 s where Eyring says 0.09.

    So the chain is sized to the decay, on the message side whenever the material
    or a door moves. A room that barely reverberates gets no diffusers at all and
    its lines alone already satisfy the modal criterion; a live room gets as much
    as it needs, capped at a total loop of 0.65 s - past that the mean loop gets
    so long that echo density suffers, and the modulation is what carries the
    remaining modes (measured: plaster reads 5.7 dB of ripple against the 5.57 of
    a perfectly diffuse field even where the criterion says it is short).
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

if (fs.readFileSync(path.join(root,"Source/Engine.h"),"utf8").indexOf("sizeDiffusers") < 0) edit("Source/Engine.h", rep => {
  rep(`    std::array<std::vector<float>, N * NAP> ap;
    std::array<int, N * NAP> apLen {}, apW {};
    std::array<int, N> apTotal {};
    float apG = 0.62f;`,
`    static constexpr int AP_CAP = 1536;   // allocated once; the active length is set per material
    std::array<std::vector<float>, N * NAP> ap;
    std::array<int, N * NAP> apLen {}, apW {};
    std::array<float, N> apTotal {};      // energy-weighted samples the allpasses add to line k
    float apG = 0.62f;
    // 1 / (1 - g^2): how many times round its own loop an allpass sends the signal
    float apEff() const { return 1.0f / (1.0f - apG * apG); }
    void sizeDiffusers (double fs, float rt60At1k);`);
});

edit("Source/Engine.cpp", rep => {
  // allocate for the cap, and give sizeDiffusers the job of choosing lengths
  rep(`            // three allpasses per line, each a prime so nothing lines up
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
            }`,
`            // the three allpasses inside this line: allocated for the cap here,
            // their working lengths chosen per material by sizeDiffusers
            for (int q = 0; q < RoomField::NAP; ++q)
            {
                const size_t idx = (size_t) (i * RoomField::NAP + q);
                F.ap[idx].assign ((size_t) RoomField::AP_CAP, 0.0f);
                F.apLen[idx] = 0;
                F.apW[idx] = 0;
            }
            F.apTotal[(size_t) i] = 0;`);

  // the sizing itself
  rep(`bool Engine::roomIsBroken (int room) const`,
`/*  Size the diffuser chain to the decay it has to serve.

    The target is a total loop delay of 1.3 x RT60/2.2 - the modal criterion with
    a little margin - capped at 0.65 s, because past that the mean loop gets long
    enough that echo density starts to suffer and the modulation is the better
    tool for what remains. Below the criterion the lines alone are enough and the
    chain is switched off entirely, which is what an absorbing room needs: its
    own decay is shorter than a diffuser's ringing.

    Every length is taken to a prime, and never to its own line's length, so
    nothing in the loop resonates with anything else in it. */
void RoomField::sizeDiffusers (double fs, float rt60At1k)
{
    auto isPrime = [] (int v) { if (v < 2) return false; for (int i = 2; i * i <= v; ++i) if (v % i == 0) return false; return true; };
    auto nextP = [&] (int n) { while (! isPrime (n)) ++n; return n; };

    float lineTotal = 0;
    for (int i = 0; i < N; ++i) lineTotal += (float) len[(size_t) i];

    const float wantTotal = std::min (0.65f, 1.3f * rt60At1k / 2.2f) * (float) fs;
    const float needRaw = std::max (0.0f, (wantTotal - lineTotal) / apEff());

    // shares of the three, summing to one, and of each line in proportion to itself
    const float share[NAP] = { 0.425f, 0.325f, 0.25f };
    for (int i = 0; i < N; ++i)
    {
        const float mine = needRaw * (float) len[(size_t) i] / std::max (1.0f, lineTotal);
        float raw = 0;
        for (int q = 0; q < NAP; ++q)
        {
            const size_t idx = (size_t) (i * NAP + q);
            int al = (int) std::lround (mine * share[q]);
            if (al < 13) { apLen[idx] = 0; continue; }          // off rather than tiny
            al = std::min (al, AP_CAP - 1);
            al = nextP (al);
            while (al == len[(size_t) i]) al = nextP (al + 1);
            if (al >= AP_CAP) al = nextP (AP_CAP / 2);
            apLen[idx] = al;
            if (apW[idx] >= al) apW[idx] = 0;
            raw += (float) al;
        }
        apTotal[(size_t) i] = raw * apEff();                    // what the LOOP grew by
    }
}

bool Engine::roomIsBroken (int room) const`);

  // size the chain once the decay times are known, before the losses are charged
  rep(`            roomAbsorption (r, surfNow[r], doorNow, abar, A, F.rt60, gg[r].area, gg[r].perimeter);
            for (int b = 0; b < NBAND; ++b) F.absorptionArea[b] = A[b];`,
`            roomAbsorption (r, surfNow[r], doorNow, abar, A, F.rt60, gg[r].area, gg[r].perimeter);
            for (int b = 0; b < NBAND; ++b) F.absorptionArea[b] = A[b];
            // the diffuser chain has to be sized before the losses are charged over it
            F.sizeDiffusers (fs, F.rt60[3]);`);

  rep(`                // the loop is the line plus the three allpasses inside it
                const float loopLen = (float) (F.len[(size_t) i] + F.apTotal[(size_t) i]);`,
      `                // the loop is the line plus what its allpasses really hold
                const float loopLen = (float) F.len[(size_t) i] + F.apTotal[(size_t) i];`);

  rep(`            for (int i = 0; i < RoomField::N; ++i) Ltot += (float) (F.len[(size_t) i] + F.apTotal[(size_t) i]);`,
      `            for (int i = 0; i < RoomField::N; ++i) Ltot += (float) F.len[(size_t) i] + F.apTotal[(size_t) i];`);

  // the efficiency probe: size the chain for its own RT of 1 s first
  rep(`    RoomField& F = rooms[(size_t) r];
    const float RT = 1.0f;
    for (int i = 0; i < RoomField::N; ++i)
    {
        F.line[(size_t) i].clear(); F.loss[(size_t) i].reset();
        const float loopLen = (float) (F.len[(size_t) i] + F.apTotal[(size_t) i]);`,
`    RoomField& F = rooms[(size_t) r];
    const float RT = 1.0f;
    F.sizeDiffusers (fs, RT);
    for (int i = 0; i < RoomField::N; ++i)
    {
        F.line[(size_t) i].clear(); F.loss[(size_t) i].reset();
        const float loopLen = (float) F.len[(size_t) i] + F.apTotal[(size_t) i];`);
  rep(`    float Ltot = 0; for (int i = 0; i < RoomField::N; ++i) Ltot += (float) (F.len[(size_t) i] + F.apTotal[(size_t) i]);`,
      `    float Ltot = 0; for (int i = 0; i < RoomField::N; ++i) Ltot += (float) F.len[(size_t) i] + F.apTotal[(size_t) i];`);

  // an allpass whose length is 0 is not in the chain
  rep(`            for (int q = 0; q < RoomField::NAP; ++q)
            {
                const size_t idx = (size_t) (k * RoomField::NAP + q);
                const float b = F.ap[idx][(size_t) F.apW[idx]];
                const float y = -F.apG * v + b;
                F.ap[idx][(size_t) F.apW[idx]] = v + F.apG * y;
                if (++F.apW[idx] >= F.apLen[idx]) F.apW[idx] = 0;
                v = y;
            }
            out[k] = v;`,
`            for (int q = 0; q < RoomField::NAP; ++q)
            {
                const size_t idx = (size_t) (k * RoomField::NAP + q);
                if (F.apLen[idx] <= 0) continue;
                const float b = F.ap[idx][(size_t) F.apW[idx]];
                const float y = -F.apG * v + b;
                F.ap[idx][(size_t) F.apW[idx]] = v + F.apG * y;
                if (++F.apW[idx] >= F.apLen[idx]) F.apW[idx] = 0;
                v = y;
            }
            out[k] = v;`);
  rep(`                for (int q = 0; q < RoomField::NAP; ++q)
                {
                    const size_t idx = (size_t) (k * RoomField::NAP + q);
                    const float b = F.ap[idx][(size_t) F.apW[idx]];
                    const float yy = -F.apG * v + b;
                    F.ap[idx][(size_t) F.apW[idx]] = v + F.apG * yy;
                    if (++F.apW[idx] >= F.apLen[idx]) F.apW[idx] = 0;
                    v = yy;
                }`,
`                for (int q = 0; q < RoomField::NAP; ++q)
                {
                    const size_t idx = (size_t) (k * RoomField::NAP + q);
                    if (F.apLen[idx] <= 0) continue;
                    const float b = F.ap[idx][(size_t) F.apW[idx]];
                    const float yy = -F.apG * v + b;
                    F.ap[idx][(size_t) F.apW[idx]] = v + F.apG * yy;
                    if (++F.apW[idx] >= F.apLen[idx]) F.apW[idx] = 0;
                    v = yy;
                }`);
});

edit("test/quality.cpp", rep => {
  rep(`                    Ltot += e.field (r).len[(size_t) k] + e.field (r).apTotal[(size_t) k];`,
      `                    Ltot += (double) e.field (r).len[(size_t) k] + e.field (r).apTotal[(size_t) k];`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("the diffuser chain is now sized to the decay");
