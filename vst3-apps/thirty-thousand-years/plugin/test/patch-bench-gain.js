/*  The bench check that would have caught the complaint.

    "Presets that seem to contain clipping effects although the total output is
    below 0 dB" is a gain structure fault, and every existing check passed
    while it was there, because they all measured the engine and none of them
    measured the BANK against the output stage. Three claims, per preset:

      * the limiter is essentially idle. Limiter::reduction is instantaneous,
        so a non-zero reading at the end of a settled render is CONTINUOUS gain
        reduction, not a transient being caught - a compressor nobody asked
        for, and audible on a drone as exactly the pumping that was reported.
      * nothing reaches the +-1.5 safety clamp, which is genuine hard clipping.
      * the bank's own spread is bounded, so no patch is lost beside its
        neighbours and none is three times louder than the rest.

    The window matters and has caught me twice already: a bank holding both
    infinite drones and one-shot struck gestures cannot be judged on a window
    that starts after note-on, or the gestures measure silence. This renders
    FROM note-on and keeps the loudest 2 s.
*/
const fs = require("fs");
const B = "C:/Users/peter/b/ThirtyThousandYears/test/bench.cpp";
let s = fs.readFileSync (B, "utf8");

const anchor = `static void listPresets()`;
const add = `static void testBankLevels()
{
    std::printf ("\\n[12] the bank against the output stage\\n");
    float worstRed = 0.0f, loudest = 0.0f, quietest = 1e9f, hotPeak = 0.0f;
    const char* worstRedName = ""; const char* hotName = "";
    int clipped = 0;
    for (int i = 0; i < numPresets(); ++i)
    {
        /*  From note-on, because a struck gesture is over before a window that
            begins later; 8 s is past every attack in the bank. */
        Engine* e = fresh (48000.0, i); e->p[P_drone] = 1; e->noteOn (45, 0.9f); e->noteOn (52, 0.85f);
        Take t = render (*e, 8.0, 512);
        const int n = t.n(), W = 96000, H = 24000;
        float loud = 0.0f, pk = 0.0f;
        for (int k = 0; k < n; ++k) pk = std::max (pk, std::max (std::abs (t.L[(size_t) k]), std::abs (t.R[(size_t) k])));
        for (int s0 = 0; s0 + W <= n; s0 += H)
        {
            double e2 = 0; for (int k = s0; k < s0 + W; ++k) { const float l = t.L[(size_t) k], r = t.R[(size_t) k]; e2 += l * l + r * r; }
            loud = std::max (loud, (float) std::sqrt (e2 / (2 * W)));
        }
        if (e->limReduction > worstRed) { worstRed = e->limReduction; worstRedName = preset (i).name; }
        if (pk > hotPeak) { hotPeak = pk; hotName = preset (i).name; }
        if (pk >= 1.4999f) ++clipped;
        loudest = std::max (loudest, loud); quietest = std::min (quietest, loud);
        delete e;
    }
    check (worstRed < 0.05f, "the limiter is idle on every factory preset",
           (std::string ("worst ") + std::to_string (worstRed) + " on " + worstRedName).c_str());
    check (clipped == 0, "no preset reaches the output clamp", (std::to_string (clipped) + " did").c_str());
    check (hotPeak < 1.0f, "no preset asks the output stage for more than full scale",
           (std::string ("worst ") + std::to_string (hotPeak) + " on " + hotName).c_str());
    const float spread = 20.0f * std::log10 ((loudest + 1e-9f) / (quietest + 1e-9f));
    check (spread < 26.0f, "the bank's loudness spread is bounded",
           (std::to_string (spread) + " dB").c_str());
    std::printf ("  info  loudest 2 s across the bank: %.4f .. %.4f (%.1f dB)\\n", quietest, loudest, spread);
}

static void listPresets()`;

if (s.split (anchor).length !== 2) { console.log ("ANCHOR MISS"); process.exit (1); }
s = s.replace (anchor, add);

const ca = `testClicks(); testCost();`;
const cb = `testClicks(); testBankLevels(); testCost();`;
if (s.split (ca).length !== 2) { console.log ("CALL ANCHOR MISS"); process.exit (1); }
s = s.replace (ca, cb);

if (s.indexOf ("#include <string>") < 0)
{
  const ia = `#include <chrono>`;
  if (s.split (ia).length !== 2) { console.log ("INCLUDE ANCHOR MISS"); process.exit (1); }
  s = s.replace (ia, `#include <chrono>\n#include <string>`);
}
fs.writeFileSync (B, s);
console.log ("bank-level check added");
