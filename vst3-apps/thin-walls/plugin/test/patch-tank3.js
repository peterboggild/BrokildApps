/*  My own algebra was wrong, and the measurement caught it: charging the loop
    over La/(1-g^2) made every live decay 27 % SHORT.

    The correct result, and it is a tidy one. For a Schroeder allpass of length La
    and gain g the impulse response is -g at 0 and (1-g^2)g^(k-1) at kLa, so the
    energy-weighted mean delay is

        sum_k k La (1-g^2)^2 g^(2k-2)  =  La (1-g^2)^2 / (1-g^2)^2  =  La

    exactly, and independent of g. The (1-g^2) cancels. An allpass holds the
    signal for precisely its own length on average, which is what makes it the
    right tool here: it buys modal density and echo density without touching the
    decay rate at all.

    So the loop is charged over the RAW length, as it was at first. The fault that
    really bit was the other one: an allpass rings at -20 log10(g) dB per La of
    its own, giving it a decay of 60 La / 4.15 = 14.5 La seconds at g = 0.62. With
    La near 7 ms that is 100 ms, longer than the 85 ms an absorbing room should
    take, so the diffusers were setting the tail rather than the room. The
    adaptive sizing already cures that by switching the chain off for a dead room;
    a hard cap on each allpass at RT60/43.5 makes it impossible rather than
    merely unlikely.
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
  rep(`    // 1 / (1 - g^2): how many times round its own loop an allpass sends the signal
    float apEff() const { return 1.0f / (1.0f - apG * apG); }`,
`    /*  A Schroeder allpass holds the signal for exactly its own length on
        average - the energy-weighted mean delay works out to La with the (1-g^2)
        cancelling, independent of g. So its raw length is what the loop grows by,
        and charging anything else makes the decay wrong (measured: 27 % short at
        1/(1-g^2)). Kept as a named function because it is the sort of factor one
        is tempted to put back. */
    static constexpr float apEff() { return 1.0f; }`);
});

edit("Source/Engine.cpp", rep => {
  rep(`    const float wantTotal = std::min (0.65f, 1.3f * rt60At1k / 2.2f) * (float) fs;
    const float needRaw = std::max (0.0f, (wantTotal - lineTotal) / apEff());`,
`    const float wantTotal = std::min (0.65f, 1.3f * rt60At1k / 2.2f) * (float) fs;
    const float needRaw = std::max (0.0f, (wantTotal - lineTotal) / apEff());
    /*  No single allpass may ring for longer than a third of the room's own
        decay. Its ringing is -20 log10(g) dB per La, so its own RT60 is
        60 La / 4.15 = 14.5 La at g = 0.62, and this is the length at which that
        reaches RT60/3. Without it a dead room's tail is set by its diffusers
        rather than by the room, which measured 0.22 s where Eyring says 0.09. */
    const int apCeil = std::max (0, (int) (rt60At1k * (float) fs / 43.5f));`);

  rep(`            if (al < 13) { apLen[idx] = 0; continue; }          // off rather than tiny
            al = std::min (al, AP_CAP - 1);`,
`            if (al < 13 || apCeil < 13) { apLen[idx] = 0; continue; }   // off rather than tiny
            al = std::min (al, std::min (AP_CAP - 1, apCeil));`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("the loop is charged over the raw length, and no diffuser may outring its room");
