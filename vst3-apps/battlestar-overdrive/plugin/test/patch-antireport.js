/*  Add section 8 to quality.cpp: what ANTITHRUST is actually doing, measured.
 *  Exact-count anchors; nothing written if one misses. */
const fs = require("fs");
const p = "C:/Users/peter/b/BattlestarOverdrive/test/quality.cpp";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(find, sub) {
  const f = find.join(NL);
  const n = s.split(f).length - 1;
  if (n !== 1) { miss.push(n + " matches: " + find[0].slice(0, 50)); return; }
  s = s.replace(f, sub.join(NL));
}

const SEC = [
"//==============================================================================",
"/*  8. WHAT IS ANTITHRUST ACTUALLY DOING?",
"    Peter asked, and the honest answer is a measurement rather than a recital",
"    of what the code was meant to do. Four things move together with the knob:",
"    the comb's delay, its depth, its feedback, and the choke after the drive. */",
"static void antithrustReport()",
"{",
"    std::printf (\"\\n8. ANTITHRUST, measured\\n\");",
"    std::printf (\"   %5s %9s %9s %9s %9s %9s %8s\\n\",",
"                 \"knob\", \"comb ms\", \"1st null\", \"null dB\", \"peak dB\", \"comp dB\", \"level\");",
"",
"    for (float a : { 0.0f, 0.25f, 0.50f, 0.75f, 1.00f })",
"    {",
"        const double ms = 12.0 * std::pow (0.03, (double) a);",
"        const double fNull = 1000.0 / ms;",
"        const double fPeak = 500.0 / ms;",
"",
"        // Comb depth: null against peak, both tones present in ONE render, so",
"        // any broadband level change cancels out of the ratio.",
"        double nullDb = 0.0, peakDb = 0.0;",
"        {",
"            Params p; p.engine = E_ION; p.thrust = 0.0f; p.spectrum = 0.5f; p.antithrust = a;",
"            Take t ((int)(SR*2));",
"            for (int i = 0; i < t.size(); ++i){",
"                const double x = i/SR;",
"                t.L[(size_t)i] = 0.25f*(float)(std::sin(TWOPI*fNull*x)+std::sin(TWOPI*fPeak*x));",
"                t.R[(size_t)i] = t.L[(size_t)i];",
"            }",
"            render (t, p);",
"            nullDb = db (goertzel (t.L, (int)SR, t.size(), fNull)) - db (0.25);",
"            peakDb = db (goertzel (t.L, (int)SR, t.size(), fPeak)) - db (0.25);",
"        }",
"",
"        // The choke: how much less gain a loud input gets than a quiet one.",
"        double g[2]; int j = 0;",
"        for (float amp : { 0.1f, 0.8f })",
"        {",
"            Params p; p.engine = E_ION; p.thrust = 0.3f; p.spectrum = 0.5f; p.antithrust = a;",
"            Take t ((int)(SR*3));",
"            for (int i = 0; i < t.size(); ++i){",
"                t.L[(size_t)i] = amp*(float)std::sin(TWOPI*150.0*i/SR); t.R[(size_t)i]=t.L[(size_t)i];",
"            }",
"            render (t, p);",
"            double s2=0; int n=0;",
"            for (int i=(int)(SR*2); i<t.size(); ++i){ s2 += (double)t.L[(size_t)i]*t.L[(size_t)i]; ++n; }",
"            g[j++] = db (std::sqrt (s2/std::max(1,n))) - db (amp);",
"        }",
"",
"        // What the knob costs in overall level on a normal signal.",
"        double lvl;",
"        {",
"            Params p; p.engine = E_ION; p.thrust = 0.5f; p.spectrum = 0.5f; p.antithrust = a;",
"            Take t ((int)(SR*3));",
"            for (int i = 0; i < t.size(); ++i){",
"                const double x = i/SR;",
"                t.L[(size_t)i] = 0.3f*(float)(std::sin(TWOPI*220*x)+0.6*std::sin(TWOPI*440*x));",
"                t.R[(size_t)i] = t.L[(size_t)i];",
"            }",
"            render (t, p);",
"            double s2=0; int n=0;",
"            for (int i=(int)(SR*2); i<t.size(); ++i){ s2 += (double)t.L[(size_t)i]*t.L[(size_t)i]; ++n; }",
"            lvl = db (std::sqrt (s2/std::max(1,n)));",
"        }",
"",
"        if (a == 0.0f)",
"            std::printf (\"   %5.2f %9s %9s %9s %9s %9.2f %8.2f\\n\", a, \"off\", \"-\", \"-\", \"-\", g[1]-g[0], lvl);",
"        else",
"            std::printf (\"   %5.2f %9.2f %9.0f %9.2f %9.2f %9.2f %8.2f\\n\",",
"                         a, ms, fNull, nullDb, peakDb, g[1]-g[0], lvl);",
"    }",
"    std::printf (\"   comp dB = how much less gain a loud input gets than a quiet one.\\n\");",
"    std::printf (\"   The whole effect is IDENTICAL on both channels: it changes tone and\\n\");",
"    std::printf (\"   dynamics, and does nothing at all to the stereo image.\\n\");",
"}",
"",
"int main()"
];

rep(["int main()"], SEC);
rep([
"    std::printf (\"====================================\\n\");",
"    spaceStability();"
], [
"    std::printf (\"====================================\\n\");",
"    antithrustReport();",
"    spaceStability();"
]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(p, s);
console.log("added section 8 to quality.cpp");
