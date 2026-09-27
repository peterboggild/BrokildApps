// Thread the optional per-channel outputs through Engine::process. The mono
// pre-pan signal of each channel goes through its own halfband cascade, the
// same one the main mix uses, so an aux out and the main bus agree.
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/FullMetalRacket/Source/Engine.cpp";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function rep(a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(a).join(b);
}

rep(`void Engine::process (float* L, float* R, int n)
{
    updateOs();`,
`void Engine::process (float* L, float* R, int n, float* const* aux)
{
    updateOs();

    if (aux != nullptr && auxA.size() != (size_t) NCH)
    {
        auxA.resize ((size_t) NCH); auxB.resize ((size_t) NCH);
        for (int c = 0; c < NCH; ++c) { auxA[(size_t) c].design(); auxB[(size_t) c].design(); }
    }`, "process signature");

rep(`    for (int i = 0; i < n; ++i)
    {
        float outL = 0.0f, outR = 0.0f;`,
`    for (int i = 0; i < n; ++i)
    {
        float outL = 0.0f, outR = 0.0f;
        float outA[NCH] = { 0.0f };`, "per-sample aux accumulator");

rep(`                const float m = std::fabs (s);
                if (m > chMeter[(size_t) c]) chMeter[(size_t) c] = m;`,
`                const float m = std::fabs (s);
                if (m > chMeter[(size_t) c]) chMeter[(size_t) c] = m;

                if (aux != nullptr)
                {
                    if (osFactor == 1) outA[c] = s;
                    else if (osFactor == 2)
                    {
                        auxB[(size_t) c].push (s);
                        if (os == osFactor - 1) outA[c] = auxB[(size_t) c].read();
                    }
                    else
                    {
                        auxA[(size_t) c].push (s);
                        if ((os & 1) == 1)
                        {
                            auxB[(size_t) c].push (auxA[(size_t) c].read());
                            if (os == osFactor - 1) outA[c] = auxB[(size_t) c].read();
                        }
                    }
                }`, "aux decimation");

rep(`        L[i] = ceilSoft (yL * vol);
        R[i] = ceilSoft (yR * vol);`,
`        L[i] = ceilSoft (yL * vol);
        R[i] = ceilSoft (yR * vol);
        if (aux != nullptr)
            for (int c = 0; c < NCH; ++c)
                if (aux[c] != nullptr)
                    aux[c][i] = std::isfinite (outA[c]) ? ceilSoft (outA[c] * vol) : 0.0f;`, "aux write");

rep(`    dec1L.reset(); dec1R.reset(); dec2L.reset(); dec2R.reset();
    rr.fill (0);`,
`    dec1L.reset(); dec1R.reset(); dec2L.reset(); dec2R.reset();
    for (auto& h : auxA) h.reset();
    for (auto& h : auxB) h.reset();
    rr.fill (0);`, "aux reset");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("aux outs patched OK");
