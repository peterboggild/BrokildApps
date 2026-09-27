/*  THE KIERANATOR ROUND — buglist 9, 10 and 11.

    9.  A/B PAGES AND A LAST STEP. Peter's model, which is better than the one
        I first proposed: BARS keeps meaning exactly what it means today — how
        long a PAGE is stretched over — and the extra length comes from a
        second page rather than from reinterpreting the division. So an
        existing pattern is LAST 16 with an empty page B, which is bit for bit
        what it does now. No migration table, nothing to get wrong. LAST is
        free to be 12 or 25, and then the loop stops agreeing with the bar,
        which is the polyrhythm he was after.

        32 steps x 4 bits will not fit the uint64 the pattern used to live in,
        and the pattern is read once per sample, so it moves to a two-slot
        buffer with an atomic index. getExtra still returns SIXTEEN characters
        whenever steps 17-32 are empty — not merely something that sounds the
        same, but the identical string — or every rack blob in every saved
        project would change for no musical reason.

    10. A RANDOM BRUSH. A ninth brush that resolves, per hit, to one of the
        seven real ones. Seeded from the loop cycle and the step, so a bar is
        reproducible, a re-render gives the same audio, and the bench can test
        it at all. NONE is excluded from the draw: a brush that sometimes does
        nothing reads as a dropout rather than a choice.

    11. A CHAOS SLIDER. Two behaviours, which are the same idea at two scales:
        each fired step has that effect's own parameters perturbed, and a step
        that is OFF fires anyway with a probability rising with the knob. Both
        seeded the same way. At 0 it is EXACTLY inert — every jitter is 1.0f,
        which is an IEEE-exact multiply, and no hash is consulted.
*/
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/BrokildWorldFX/modules/bwfx_modules.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const miss = [];
function rep(a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(a).join(b);
}
const J = (arr) => arr.join(NL);

// ── 1 · two more parameters, appended so every existing index is untouched ──
rep(J([
  '        { "duty",   "DUTY",   50,  0, 100, 0, "%", nullptr },',
  "    };"
]), J([
  '        { "duty",   "DUTY",   50,  0, 100, 0, "%", nullptr },',
  '        { "last",   "LAST",   16,  2, 32,  1, "",  nullptr },',
  '        { "chaos",  "CHAOS",  0,   0, 100, 0, "%", nullptr },',
  "    };"
]), "params");

rep('DISRUPTOR_PARAMS, 8, "steps"', 'DISRUPTOR_PARAMS, 10, "steps"', "numParams");

// ── 2 · the pattern store: 32 steps, nine values, double buffered ──────────
rep(J([
  "    // the pattern: 16 characters '0'..'7', one per step",
  "    void setExtra (const std::string& x) override",
  "    {",
  "        uint64_t p = 0;",
  "        for (int i = 0; i < 16; ++i)",
  "        {",
  "            const int c = i < (int) x.size() ? x[(size_t) i] - '0' : 0;",
  "            p |= (uint64_t) (c >= 0 && c <= 7 ? c : 0) << (3 * i);",
  "        }",
  "        patternIn.store (p, std::memory_order_relaxed);",
  "    }",
  "    std::string getExtra() const override",
  "    {",
  "        const uint64_t p = patternIn.load (std::memory_order_relaxed);",
  "        std::string s (16, '0');",
  "        for (int i = 0; i < 16; ++i) s[(size_t) i] = (char) ('0' + ((p >> (3 * i)) & 7));",
  '        return s == "0000000000000000" ? std::string() : s;   // default = empty blob',
  "    }"
]), J([
  "    /*  The pattern: up to 32 characters '0'..'8', one per step, where 8 is",
  "        the RANDOM brush. 32 x 4 bits does not fit a uint64 and the pattern",
  "        is read once per SAMPLE, so it lives in two slots with an atomic",
  "        index rather than in a packed word — the writer fills the spare slot",
  "        and publishes it, and the audio thread never sees a half-written",
  "        pattern. */",
  "    void setExtra (const std::string& x) override",
  "    {",
  "        const int idx = patIdx.load (std::memory_order_relaxed);",
  "        auto& dst = pat[(size_t) (idx ^ 1)];",
  "        for (int i = 0; i < kSteps; ++i)",
  "        {",
  "            const int c = i < (int) x.size() ? x[(size_t) i] - '0' : 0;",
  "            dst[(size_t) i] = (uint8_t) (c >= 0 && c <= 8 ? c : 0);",
  "        }",
  "        patIdx.store (idx ^ 1, std::memory_order_release);",
  "    }",
  "    /*  SIXTEEN characters whenever the second page is empty — the identical",
  "        string the module returned before page B existed, not merely one",
  "        that sounds the same. Otherwise every rack blob in every saved",
  "        project would change the day this shipped. */",
  "    std::string getExtra() const override",
  "    {",
  "        const auto& p = pat[(size_t) patIdx.load (std::memory_order_acquire)];",
  "        int len = 16;",
  "        for (int i = 16; i < kSteps; ++i) if (p[(size_t) i] != 0) { len = kSteps; break; }",
  "        std::string s ((size_t) len, '0');",
  "        bool any = false;",
  "        for (int i = 0; i < len; ++i)",
  "        {",
  "            s[(size_t) i] = (char) ('0' + p[(size_t) i]);",
  "            if (p[(size_t) i] != 0) any = true;",
  "        }",
  "        return any ? s : std::string();                       // default = empty blob",
  "    }"
]), "extra");

// ── 3 · the clock and the step decision ────────────────────────────────────
rep(J([
  "        const float duty = clampf (getParam (7) / 100.0f, 0.05f, 1.0f);",
  "        const double beatsPerSample = useBpm / (60.0 * fs);"
]), J([
  "        const float duty = clampf (getParam (7) / 100.0f, 0.05f, 1.0f);",
  "        const int   last = (int) std::lround (getParam (8));",
  "        const int   nSteps = last < 2 ? 2 : (last > kSteps ? kSteps : last);",
  "        const float chaos = clampf (getParam (9) / 100.0f, 0.0f, 1.0f);",
  "        const auto& pgm = pat[(size_t) patIdx.load (std::memory_order_acquire)];",
  "        const double beatsPerSample = useBpm / (60.0 * fs);"
]), "clock params");

rep("        const uint64_t pattern = patternIn.load (std::memory_order_relaxed);" + NL, "");

rep(J([
  "            const double stepPos = b / stepBeats;",
  "            const int step = (int) ((int64_t) stepPos % 16);",
  "            const double phase = stepPos - std::floor (stepPos);"
]), J([
  "            const double stepPos = b / stepBeats;",
  "            /*  A host can hand out a negative ppq during a count-in, and a",
  "                negative index would read outside the pattern. */",
  "            const int64_t rawStep = (int64_t) std::floor (stepPos);",
  "            int64_t cycle = rawStep / nSteps;",
  "            int step = (int) (rawStep - cycle * nSteps);",
  "            if (step < 0) { step += nSteps; --cycle; }",
  "            const double phase = stepPos - std::floor (stepPos);"
]), "step index");

rep(J([
  "                cur.type = (int) ((pattern >> (3 * step)) & 7);",
  "                cur.start = w;",
  "                cur.t = 0;"
]), J([
  "                cur.type = pgm[(size_t) step];",
  "                cur.start = w;",
  "                cur.t = 0;",
  "                cur.jRep = 1.0; cur.jRatio = 1.0;",
  "                cur.jDecay = 1.0f; cur.jStop = 1.0f; cur.jCrush = 1.0f; cur.jDuty = 1.0f;",
  "",
  "                /*  RANDOM resolves to one of the SEVEN real effects, never",
  "                    to NONE — a brush that sometimes does nothing reads as a",
  "                    dropout rather than as a choice. Seeded from the loop",
  "                    cycle and the step, so the bar is reproducible and a",
  "                    re-render gives the same audio. */",
  "                if (cur.type == 8)",
  "                    cur.type = 1 + (int) (stepHash (cycle, step, 0x5EEDu) % 7u);",
  "",
  "                if (chaos > 0.0f)",
  "                {",
  "                    //  a step that is OFF fires anyway, more often as the",
  "                    //  knob comes up. This is the half that makes CHAOS feel",
  "                    //  alive rather than merely loose.",
  "                    if (cur.type == 0)",
  "                    {",
  "                        const uint32_t h = stepHash (cycle, step, 0xC0FFEEu);",
  "                        const uint32_t thresh = (uint32_t) (chaos * chaos * 0.45f * 65536.0f);",
  "                        if ((h & 0xFFFFu) < thresh) cur.type = 1 + (int) ((h >> 16) % 7u);",
  "                    }",
  "                    //  ...and whatever fires is knocked off its knob setting",
  "                    //  by up to +/- half the CHAOS reading. Applied AT the",
  "                    //  step, so it is a different glitch each time rather",
  "                    //  than a wobble across the bar.",
  "                    const float k = chaos * 0.5f;",
  "                    cur.jDecay = jitter (cycle, step, 0x11u, k);",
  "                    cur.jStop  = jitter (cycle, step, 0x22u, k);",
  "                    cur.jCrush = jitter (cycle, step, 0x33u, k);",
  "                    cur.jDuty  = jitter (cycle, step, 0x44u, k);",
  "                    cur.jRep   = (double) jitter (cycle, step, 0x55u, k);",
  "                    cur.jRatio = (double) jitter (cycle, step, 0x66u, k * 0.5f);",
  "                }"
]), "step fire");

// ── 4 · the jitter reaches the renderer ────────────────────────────────────
rep(J([
  "        double shuffleBack = 0;"
]), J([
  "        double shuffleBack = 0;",
  "        //  CHAOS: per-step multipliers on this effect's own settings. All",
  "        //  exactly 1 when the knob is shut, and x1.0f is IEEE-exact, so a",
  "        //  rack at CHAOS 0 renders the identical samples it always did.",
  "        double jRep = 1.0, jRatio = 1.0;",
  "        float jDecay = 1.0f, jStop = 1.0f, jCrush = 1.0f, jDuty = 1.0f;"
]), "voice jitter");

rep(J([
  "            case 1:                                                // RETRIG",
  "            {",
  "                const int64_t k = v.t / (int64_t) repLen;",
  "                const double pos = (double) (v.t % (int64_t) repLen);",
  "                const float g = std::pow (decay, (float) k);"
]), J([
  "            case 1:                                                // RETRIG",
  "            {",
  "                const double rl = std::max (64.0, repLen * v.jRep);",
  "                const int64_t k = v.t / (int64_t) rl;",
  "                const double pos = (double) (v.t % (int64_t) rl);",
  "                const float g = std::pow (clampf (decay * v.jDecay, 0.0f, 1.0f), (float) k);"
]), "retrig jitter");

rep(J([
  "                const double u = std::min (1.0, (double) v.t / std::max (1.0, stepLen));",
  "                const double rate = std::max (0.0, 1.0 - stopDepth * std::pow (u, 1.4));",
  "                // integrated read position (closed form of the power ramp)",
  "                const double travel = (double) v.t - stopDepth * stepLen * std::pow (u, 2.4) / 2.4;"
]), J([
  "                const double u = std::min (1.0, (double) v.t / std::max (1.0, stepLen));",
  "                const double sd = (double) clampf (stopDepth * v.jStop, 0.0f, 1.0f);",
  "                const double rate = std::max (0.0, 1.0 - sd * std::pow (u, 1.4));",
  "                // integrated read position (closed form of the power ramp)",
  "                const double travel = (double) v.t - sd * stepLen * std::pow (u, 2.4) / 2.4;"
]), "tapestop jitter");

rep("                const double pos = std::fmod ((double) v.t * ratio, span);",
    "                const double pos = std::fmod ((double) v.t * ratio * v.jRatio, span);", "pitch jitter");

rep(J([
  "                const double hold = 1.0 + crushAmt * crushAmt * 60.0;"
]), J([
  "                const float ca = clampf (crushAmt * v.jCrush, 0.0f, 1.0f);",
  "                const double hold = 1.0 + ca * ca * 60.0;"
]), "crush jitter a");
rep("                const double q = std::pow (2.0, 11.0 - crushAmt * 8.0);",
    "                const double q = std::pow (2.0, 11.0 - ca * 8.0);", "crush jitter b");

rep(J([
  "                const double edge = 0.01;",
  "                float g = 0.0f;",
  "                if (phase < duty)",
  "                {",
  "                    g = 1.0f;",
  "                    if (phase < edge)               g = (float) (phase / edge);",
  "                    else if (phase > duty - edge)   g = (float) ((duty - phase) / edge);",
  "                }"
]), J([
  "                const double edge = 0.01;",
  "                const double dy = (double) clampf (duty * v.jDuty, 0.05f, 1.0f);",
  "                float g = 0.0f;",
  "                if (phase < dy)",
  "                {",
  "                    g = 1.0f;",
  "                    if (phase < edge)             g = (float) (phase / edge);",
  "                    else if (phase > dy - edge)   g = (float) ((dy - phase) / edge);",
  "                }"
]), "gate jitter");

// ── 5 · members and the seeded hash ────────────────────────────────────────
rep(J([
  "    int lastStep = -1;",
  "    uint32_t rng = 0xD15Bu;",
  "    std::atomic<uint64_t> patternIn { 0 };"
]), J([
  "    int lastStep = -1;",
  "    uint32_t rng = 0xD15Bu;",
  "",
  "    /*  Deterministic per (loop cycle, step, purpose). Everything random in",
  "        this module is drawn from here, so a bar always sounds the same way",
  "        twice, a bounce matches the playback, and the bench can assert on",
  "        it. A free-running rand() would give none of those. */",
  "    static uint32_t stepHash (int64_t cycle, int step, uint32_t salt)",
  "    {",
  "        uint64_t h = (uint64_t) cycle * 0x9E3779B97F4A7C15ull;",
  "        h ^= (uint64_t) (uint32_t) step * 0xBF58476D1CE4E5B9ull;",
  "        h ^= (uint64_t) salt * 0x94D049BB133111EBull;",
  "        h ^= h >> 30; h *= 0xBF58476D1CE4E5B9ull;",
  "        h ^= h >> 27; h *= 0x94D049BB133111EBull;",
  "        h ^= h >> 31;",
  "        return (uint32_t) h;",
  "    }",
  "    static float jitter (int64_t cycle, int step, uint32_t salt, float amount)",
  "    {",
  "        const float u = (float) (stepHash (cycle, step, salt) & 0xFFFFFFu) / 16777215.0f;",
  "        return 1.0f + amount * (2.0f * u - 1.0f);",
  "    }",
  "",
  "    static constexpr int kSteps = 32;",
  "    std::array<uint8_t, kSteps> pat[2] { };",
  "    std::atomic<int> patIdx { 0 };"
]), "members");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("KIERANATOR: 32 steps, LAST, RANDOM brush, CHAOS");
