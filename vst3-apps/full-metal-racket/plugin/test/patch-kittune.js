// A bench check for KIT TUNE, because Peter reports it does nothing.
// Measures the actual pitch of a kick, a tom and a hat at three settings.
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/FullMetalRacket/test/test.cpp";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function rep(a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(a).join(b);
}

rep(`int main (int argc, char** argv)`,
`//==============================================================================
static void groupKitTune()
{
    std::printf ("\\n-- 23. KIT TUNE -------------------------------------------------\\n");

    //  measure the sounding pitch of one channel at three KIT TUNE settings
    auto pitchOf = [] (int c, float kt) -> double
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f; e.p.g[GP_SAG] = 0.0f;
        e.p.g[GP_KITTUNE] = kt;
        for (int k = 0; k < NCH; ++k) if (k != c) e.p.ch[k][CP_MUTE] = 1.0f;
        e.p.ch[c][CP_BEND] = 0.0f;
        e.p.ch[c][CP_DECAY] = 0.85f;
        e.p.ch[c][CP_DRIVE] = 0.0f;
        e.trigger (c, 0.35f);
        Buf b (48000 * 2); render (e, b);
        const int i = paramIndex ((std::string (channelId (c)) + "_tune").c_str());
        const double base = (double) xmap (paramSpec (i).def, paramSpec (i).lo, paramSpec (i).hi);
        return dominant (b, 48000.0, 9000, 60000, base * 0.45, base * 2.2);
    };

    for (int c : { 0, 4, 6, 8 })
    {
        const double lo = pitchOf (c, 0.0f), mid = pitchOf (c, 0.5f), hi = pitchOf (c, 1.0f);
        const double semis = 12.0 * std::log2 (std::max (1e-9, hi / std::max (1e-9, lo)));
        std::printf ("    %-6s  KIT TUNE 0%% = %7.2f Hz   50%% = %7.2f   100%% = %7.2f   span %+.1f semitones\\n",
                     channelName (c), lo, mid, hi, semis);
        ok (std::fabs (semis) > 1.0,
            (std::string ("KIT TUNE moves ") + channelName (c)).c_str(), semis, 1.0);
    }
}

int main (int argc, char** argv)`, "kittune group");

rep(`    groupSeeds();
    groupCpu();`,
`    groupSeeds();
    groupKitTune();
    groupCpu();`, "call");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("kit tune check added");
