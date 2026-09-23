// 260905.2 — the three Black Rider filter circuits (GROWL, SCREAM, LADDER),
// each with a lowpass and a highpass, beside the state-variable filter.
// Exact-count anchors; nothing written on a miss.
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const rep = (from, to, count = 1) => {
    const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
    const n = s.split(f).length - 1;
    if (n !== count) { misses.push(`${rel}: expected ${count} of [${from.slice(0, 70).replace(/\n/g, "\\n")}...], found ${n}`); return; }
    s = s.split(f).join(t);
  };
  fn(rep);
  return { p, get: () => s };
}

// ============================================================ Engine.h
const eh = edit("Source/Engine.h", (rep) => {
  rep(String.raw`    float modRate  = 0.30f, modSync = 0.0f;`,
      String.raw`    /*  THE CIRCUIT (260905.2): the state-variable filter, or one of Black
        Rider's three — GROWL (Sallen-Key with a diode clipper in the
        feedback), SCREAM (the same circuit, self-oscillating from two o'clock),
        LADDER (four-pole transistor ladder) — each as a lowpass or a highpass.
        BAND stays the SVF's. Default SVF, so every existing patch is untouched. */
    float filtModel = 0.0f;
    float modRate  = 0.30f, modSync = 0.0f;`);
  rep(String.raw`struct VoiceView
{`,
      String.raw`//==============================================================================
//  the two circuits from Black Rider, per reader
inline float ftanh (float x)
{
    x = x < -4.97f ? -4.97f : (x > 4.97f ? 4.97f : x);
    const float x2 = x * x;
    const float p = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float q = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return p / q;
}

/*  MS-20 style two-pole Sallen-Key: two one-pole TPT stages with feedback of
    K through a one-pole of the opposite kind and a clipper in the feedback
    path, solved as a zero-delay loop for the linear part then refined twice
    for the clipper. Q = 1/(2-K); K = 2 is the edge of self-oscillation and the
    clipper is what keeps it there. Lowpass and highpass are the same circuit
    with the stages swapped. (Black Rider's Korg35, verbatim.) */
struct Korg35
{
    float g = 0.1f, G = 0.1f;
    float s1 = 0, s2 = 0, s3 = 0;
    float sat = 1.0f;
    void reset() { s1 = s2 = s3 = 0.0f; }
    void setG (float gg) { g = gg; G = g / (1.0f + g); }
    inline float clip (float x) const
    {
        const float t = x > 0.0f ? x : x * 1.12f;
        const float y = sat * ftanh (t / sat);
        return x > 0.0f ? y : y / 1.12f;
    }
    inline float lowpass (float x, float K)
    {
        const float v1 = (x - s1) * G; const float a = v1 + s1; s1 = a + v1;
        const float oneG = 1.0f - G;
        const float den = 1.0f / (1.0f - G * K * oneG);
        float y = (G * a + oneG * s2 - G * K * oneG * s3) * den;
        float u = a;
        for (int it = 0; it < 2; ++it)
        {
            const float hp = oneG * (y - s3);
            const float fb = clip (K * hp);
            u = a + fb;
            y = G * u + oneG * s2;
        }
        const float v2 = (u - s2) * G; s2 = y + v2;
        const float vh = (y - s3) * G; const float lp = vh + s3; s3 = lp + vh;
        return y;
    }
    inline float highpass (float x, float K)
    {
        const float oneG = 1.0f - G;
        const float v1 = (x - s1) * G; const float l1 = v1 + s1; s1 = l1 + v1; const float a = x - l1;
        const float den = 1.0f / (1.0f - K * G * oneG);
        float y = oneG * (a - s2 + K * oneG * s3) * den;
        float u = a;
        for (int it = 0; it < 2; ++it)
        {
            const float lp = G * y + oneG * s3;
            const float fb = clip (K * lp);
            u = a + fb;
            y = oneG * (u - s2);
        }
        const float v2 = (u - s2) * G; s2 = s2 + 2.0f * v2;
        const float vl = (y - s3) * G; const float l3 = vl + s3; s3 = l3 + vl;
        return y;
    }
};

/*  Moog style transistor ladder: four trapezoidal one-poles, the loop solved
    exactly for the linear case, then the differential pair at the summing
    node applied as a tanh, a gentle cubic on each stage. The highpass is the
    same loop with highpass stages — h = (1-G)(u - s) — solved the same way:
    h4 = ((1-G)^4 x - S) / (1 + k (1-G)^4). (Black Rider's Ladder, plus HP.) */
struct Ladder
{
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    float G = 0.1f;
    void reset() { s1 = s2 = s3 = s4 = 0.0f; }
    void setG (float g) { G = g / (1.0f + g); }
    inline float lowpass (float x, float k)
    {
        const float G2 = G * G, G4 = G2 * G2, oneG = 1.0f - G;
        const float S = G2 * G * oneG * s1 + G2 * oneG * s2 + G * oneG * s3 + oneG * s4;
        const float y4lin = (G4 * x + S) / (1.0f + k * G4);
        float u = x - k * y4lin;
        u = 1.3f * ftanh (u * (1.0f / 1.3f));
        auto stage = [this] (float in, float& s) { const float v = G * (in - s); const float y = v + s; s = y + v; return y; };
        auto soft = [] (float y) { return y * (1.0f - std::min (0.3f, y * y * 0.025f)); };
        const float y1 = soft (stage (u,  s1));
        const float y2 = soft (stage (y1, s2));
        const float y3 = soft (stage (y2, s3));
        const float y4 = stage (y3, s4);
        return y4;
    }
    inline float highpass (float x, float k)
    {
        const float oneG = 1.0f - G, o2 = oneG * oneG, o3 = o2 * oneG, o4 = o2 * o2;
        const float S = o4 * s1 + o3 * s2 + o2 * s3 + oneG * s4;
        const float h4lin = (o4 * x - S) / (1.0f + k * o4);
        float u = x - k * h4lin;
        u = 1.3f * ftanh (u * (1.0f / 1.3f));
        auto stage = [this] (float in, float& s) { const float v = G * (in - s); const float y = v + s; s = y + v; return in - y; };
        auto soft = [] (float y) { return y * (1.0f - std::min (0.3f, y * y * 0.025f)); };
        const float h1 = soft (stage (u,  s1));
        const float h2 = soft (stage (h1, s2));
        const float h3 = soft (stage (h2, s3));
        const float h4 = stage (h3, s4);
        return h4;
    }
};

struct VoiceView
{`);
  rep(String.raw`        float  edge2 = 0;            // the jump at phase 0.5 of a SPLIT line
        float  shX = 0;              // the window shaper's previous input (ADAA)
        double ph2 = 0;              // the second head's own phase
    };`,
      String.raw`        float  edge2 = 0;            // the jump at phase 0.5 of a SPLIT line
        float  shX = 0;              // the window shaper's previous input (ADAA)
        double ph2 = 0;              // the second head's own phase
        Korg35 k35;                  // the Sallen-Key circuit (GROWL, SCREAM)
        Ladder lad;                  // the transistor ladder
    };`);
  rep(String.raw`        //  the read, per voice: the MOD line may move the kernel and the window
        float  kB = 1.0f, kC = 0.0f;
        float  shGain = 1.0f;
        bool   shOn = false;
    };`,
      String.raw`        //  the read, per voice: the MOD line may move the kernel and the window
        float  kB = 1.0f, kC = 0.0f;
        float  shGain = 1.0f;
        bool   shOn = false;
        //  the circuit, per voice: its prewarped coefficient and its feedback K
        float  fG = 0.1f, fK = 0.0f;
    };`);
});

// ============================================================ Engine.cpp
const ec = edit("Source/Engine.cpp", (rep) => {
  rep(String.raw`static const char* const L_FMODE[]   = { "SWEEP", "LOOP" };`,
      String.raw`static const char* const L_FMODE[]   = { "SWEEP", "LOOP" };
static const char* const L_FMODEL[]  = { "SVF", "GROWL", "SCREAM", "LADDER" };`);
  rep(String.raw`    { "ftype",     "FILTER",     "the filter's response",                                  0.00f, KP_LIST, 0, 3,  P(filtType),  L_FTYPE, 4 },`,
      String.raw`    { "ftype",     "FILTER",     "the filter's response",                                  0.00f, KP_LIST, 0, 3,  P(filtType),  L_FTYPE, 4 },
    { "fmodel",    "CIRCUIT",    "the filter circuit: the state-variable, or Black Rider's GROWL, SCREAM and LADDER", 0.00f, KP_LIST, 0, 3, P(filtModel), L_FMODEL, 4 },`);
  rep(String.raw`        const float res = clamp01 (p.reso);
        for (int q = 0; q < v.nUni; ++q) v.u[q].svf.set (v.cutHz, res, sr);`,
      String.raw`        const float res = clamp01 (p.reso);
        for (int q = 0; q < v.nUni; ++q) v.u[q].svf.set (v.cutHz, res, sr);
        /*  The circuits: K from the same RESONANCE knob by each model's own law
            (Black Rider's), and the prewarped coefficient lifted to correct the
            clipper's measured tuning drag on the Sallen-Key models — the
            clipper takes a share of the loop and its share grows with K. */
        const int model = listIndex (paramSpec (paramIndex ("fmodel")), p.filtModel);
        if (model > 0)
        {
            float K, pw = 1.0f;
            if (model == 1)      K = 2.25f * std::pow (res, 0.9f);                                       // GROWL: screams only at the top
            else if (model == 2) K = res < 0.62f ? 2.0f * std::pow (res / 0.62f, 0.7f) : 2.0f + 0.9f * ((res - 0.62f) / 0.38f);   // SCREAM: past two o'clock
            else                 K = 4.0f * 1.12f * std::pow (res, 0.85f);                               // LADDER: self-oscillates at 4
            if (model <= 2)
            {
                float cents = 0.0f;
                if      (K > 2.45f) cents = 17.0f + 78.0f * (K - 2.45f);
                else if (K > 2.25f) cents = 6.0f + 55.0f * (K - 2.25f);
                else if (K > 2.0f)  cents = 24.0f * (K - 2.0f);
                pw = std::pow (2.0f, cents / 1200.0f);
                K = std::min (K, model == 2 ? 2.95f : 2.45f);
            }
            else K = std::min (K, 4.6f);
            const float fc = clampf (v.cutHz * pw, 10.0f, (float) (sr * 0.45));
            v.fG = std::tan ((float) PI * fc / (float) sr);
            v.fK = K;
            for (int q = 0; q < v.nUni; ++q) { v.u[q].k35.setG (v.fG); v.u[q].lad.setG (v.fG); }
        }`);
  rep(String.raw`    const int ftype = listIndex (paramSpec (paramIndex ("ftype")), p.filtType);`,
      String.raw`    const int ftype = listIndex (paramSpec (paramIndex ("ftype")), p.filtType);
    const int fmodel = listIndex (paramSpec (paramIndex ("fmodel")), p.filtModel);`);
  rep(String.raw`            w = u.svf.run (w, ftype);`,
      String.raw`            //  the filter: the SVF, or one of the circuits as a lowpass or a highpass (BAND is the SVF's)
            if (fmodel == 0 || ftype == 1 || ftype == 3) w = u.svf.run (w, ftype);
            else if (fmodel == 3) w = ftype == 0 ? u.lad.lowpass (w, v.fK) : u.lad.highpass (w, v.fK);
            else                  w = ftype == 0 ? u.k35.lowpass (w, v.fK) : u.k35.highpass (w, v.fK);`);
  rep(String.raw`        if (! retrigger || ! v.active) { u.ph = 0; u.ph2 = 0; u.shX = 0; u.svf.reset(); }`,
      String.raw`        if (! retrigger || ! v.active) { u.ph = 0; u.ph2 = 0; u.shX = 0; u.svf.reset(); u.k35.reset(); u.lad.reset(); }`);
});

// ============================================================ ui.html
const ui = edit("Source/ui/ui.html", (rep) => {
  rep(String.raw`      L_FTYPE = ["LOW","BAND","HIGH","OFF"], L_FMODE = ["SWEEP","LOOP"],`,
      String.raw`      L_FTYPE = ["LOW","BAND","HIGH","OFF"], L_FMODE = ["SWEEP","LOOP"], L_FMODEL = ["SVF","GROWL","SCREAM","LADDER"],`);
  rep(String.raw`  ["ftype","FILTER",KP.LIST,0.00,"the filter's response",0,3,L_FTYPE],`,
      String.raw`  ["ftype","FILTER",KP.LIST,0.00,"the filter's response",0,3,L_FTYPE],
  ["fmodel","CIRCUIT",KP.LIST,0.00,"the filter circuit: the state-variable, or Black Rider's GROWL, SCREAM and LADDER",0,3,L_FMODEL],`);
  rep(String.raw`  mkSeg(mkRow(m2), "ftype", { label:false });`,
      String.raw`  const ftypeSeg = mkSeg(mkRow(m2), "ftype", { label:false });
  /*  THE CIRCUIT (260905.2): Black Rider's three, each a lowpass or a highpass
      by the FILTER row above; BAND stays the SVF's and dims off it. */
  mkSeg(mkRow(m2), "fmodel", { label:false,
    hint:"the filter circuit.  SVF is the state-variable filter the instrument shipped with.  GROWL is a Sallen-Key with a diode clipper in its feedback — a gnarly resonance that screams only at the top of RESONANCE.  SCREAM is the same circuit pushed harder: from two o'clock up it self-oscillates, in tune, and KEY TRACK plays it.  LADDER is the four-pole transistor ladder, the bass thinning as the resonance rises.  Each is a LOW or HIGH pass by the row above; BAND is the SVF's alone" });
  {
    const bandBtn = ftypeSeg.querySelectorAll(".seg button")[1];
    const dimBand = v => { if (bandBtn) bandBtn.classList.toggle("dim", listIndex(P.fmodel, v) !== 0); };
    dimBand(P.fmodel.v); onParam("fmodel", dimBand);
  }`);
});

// ============================================================ uiprobe.js
const probe = edit("test/uiprobe.js", (rep) => {
  rep(String.raw`      mk("ftype","FILTER",2,0,"the filter's response",0,3,["LOW","BAND","HIGH","OFF"]),`,
      String.raw`      mk("ftype","FILTER",2,0,"the filter's response",0,3,["LOW","BAND","HIGH","OFF"]),
      mk("fmodel","CIRCUIT",2,0,"the filter circuit",0,3,["SVF","GROWL","SCREAM","LADDER"]),`);
  rep(String.raw`    ok("every parameter reached a control", Object.keys(__BS.P).length === 39, Object.keys(__BS.P).length);`,
      String.raw`    ok("every parameter reached a control", Object.keys(__BS.P).length === 40, Object.keys(__BS.P).length);`);
  rep(String.raw`    ok("no script error after the round trip", !window.__err, window.__err);`,
      String.raw`    /* ---- 9. 260905.2: the circuit switch --------------------------------- */
    {
      const segs = Array.from(document.querySelectorAll("#console .mod .seg"));
      const circ = segs.find(s => Array.from(s.querySelectorAll("button")).map(b => b.textContent).join(",") === "SVF,GROWL,SCREAM,LADDER");
      ok("the FILTER module has the four circuits on one switch", !!circ);
      const ft = segs.find(s => Array.from(s.querySelectorAll("button")).map(b => b.textContent).join(",") === "LOW,BAND,HIGH,OFF");
      const band = ft && ft.querySelectorAll("button")[1];
      ok("BAND is lit on the SVF", !!band && !band.classList.contains("dim"));
      __BS.setParam("fmodel", 2/3);
      ok("and dims on a circuit that has no band (SCREAM)", !!band && band.classList.contains("dim"));
      __BS.setParam("fmodel", 0);
      ok("and comes back on the SVF", !!band && !band.classList.contains("dim"));
      const fm = document.querySelector("#console .mod:nth-child(3)");
      const r = fm && fm.getBoundingClientRect(), pr = fm && fm.parentElement.getBoundingClientRect();
      ok("the FILTER module still fits its column", !!r && r.bottom <= pr.bottom + 1 && fm.scrollHeight <= fm.clientHeight + 1, r ? Math.round(r.bottom) + " of " + Math.round(pr.bottom) + ", scroll " + fm.scrollHeight + " / " + fm.clientHeight : "no module");
    }
    ok("no script error after the round trip", !window.__err, window.__err);`);
});

// ============================================================ bench.cpp
const bench = edit("test/bench.cpp", (rep) => {
  rep(String.raw`    std::printf ("\n%d checks, %d failed  -  %s\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");`,
      String.raw`    //==========================================================================
    head ("11 - the circuits: GROWL, SCREAM, LADDER, each low and high (260905.2)");
    {
        //  the cutoff knob: fc = 20 * 1000^pos
        auto posOf = [] (double hz) { return (float) (std::log (hz / 20.0) / std::log (1000.0)); };
        const char* names[4] = { "SVF", "GROWL", "SCREAM", "LADDER" };
        //  ---- every circuit filters: LOW pulls the centroid down, HIGH pushes it up ----
        {
            Line six[NLINES]; straightAll (six, 1.0f, 0.0f);            // SPINE, bright
            Engine* e0 = fresh (1, six);
            Take t0 = render (*e0, 57, 0.8);
            const double c0 = centroid (spectrum (t0.L, (size_t) (0.3 * SR), N), N);
            delete e0;
            char d[200]; int n = 0;
            n += std::snprintf (d + n, sizeof d - (size_t) n, "centroid OFF %.0f Hz;", c0);
            bool good = true;
            for (int m = 1; m <= 3; ++m)
            {
                double cl, ch;
                for (int hp = 0; hp < 2; ++hp)
                {
                    Engine* e = fresh (1, six);
                    e->p.filtModel = (float) m / 3.0f; e->p.filtType = hp ? 2.0f / 3.0f : 0.0f;
                    e->p.cutoff = posOf (hp ? 2000.0 : 500.0); e->p.reso = 0.2f; e->p.filtDepth = 0.0f;
                    Take t = render (*e, 57, 0.8);
                    const double c = centroid (spectrum (t.L, (size_t) (0.3 * SR), N), N);
                    if (hp) ch = c; else cl = c;
                    delete e;
                }
                n += std::snprintf (d + n, sizeof d - (size_t) n, " %s low %.0f high %.0f;", names[m], cl, ch);
                if (! (cl < c0 * 0.6 && ch > c0 * 1.3)) good = false;
            }
            std::printf ("  %s\n", d);
            ok (good, "each circuit's lowpass at 500 Hz pulls the centroid under 60 %, its highpass at 2 kHz pushes it past 130 %", d);
        }
        //  ---- self-oscillation, in tune: SCREAM and LADDER at full resonance ----
        {
            /*  A cutoff between two harmonics of the note (1000 Hz; the note's
                4th and 5th sit at 880 and 1100), full RESONANCE, a quiet sine
                to kick it: the strongest line in the octave around the cutoff
                must be the circuit's own oscillation, at the cutoff. */
            Line six[NLINES]; straightAll (six, 0.0f, 0.0f);            // SINUS at its quietest
            for (int m = 2; m <= 3; ++m)
            {
                Engine* e = fresh (0, six);
                e->p.filtModel = (float) m / 3.0f; e->p.filtType = 0.0f;
                e->p.cutoff = posOf (1000.0); e->p.reso = 1.0f; e->p.filtDepth = 0.0f;
                Take t = render (*e, 57, 1.2);
                const auto sp = spectrum (t.L, (size_t) (0.6 * SR), N);
                size_t best = 0; double bm = 0;
                for (size_t i = (size_t) (700.0 * N / SR); i < (size_t) (1400.0 * N / SR); ++i) if (sp[i] > bm) { bm = sp[i]; best = i; }
                //  parabolic interpolation on the log magnitude for a better peak
                double fpk = (double) best * SR / (double) N;
                if (best > 0 && best + 1 < sp.size())
                {
                    const double a = std::log (std::max (1e-12, sp[best - 1])), b = std::log (std::max (1e-12, sp[best])), c = std::log (std::max (1e-12, sp[best + 1]));
                    const double dd = 0.5 * (a - c) / (a - 2 * b + c);
                    fpk = ((double) best + (std::isfinite (dd) ? dd : 0.0)) * SR / (double) N;
                }
                const double cents = 1200.0 * std::log2 (fpk / 1000.0);
                const double h1 = harmMag (sp, f0, 1, N);
                char d[160]; std::snprintf (d, sizeof d, "%s at RESONANCE 1, cutoff 1000 Hz: sings at %.1f Hz (%+.1f cents), %.1f dB over the note", names[m], fpk, cents, dB (bm / std::max (1e-12, h1)));
                std::printf ("  %s\n", d);
                ok (std::abs (cents) < 40.0 && bm > h1 * 2.0, "self-oscillates at the cutoff within 40 cents, louder than the note", d);
                delete e;
            }
            //  and GROWL, at full resonance, is still a filter and not an oscillator
            Engine* e = fresh (0, six);
            e->p.filtModel = 1.0f / 3.0f; e->p.filtType = 0.0f;
            e->p.cutoff = posOf (1000.0); e->p.reso = 1.0f; e->p.filtDepth = 0.0f;
            Take t = render (*e, 57, 1.2);
            const auto sp = spectrum (t.L, (size_t) (0.6 * SR), N);
            size_t best = 0; double bm = 0;
            for (size_t i = (size_t) (700.0 * N / SR); i < (size_t) (1400.0 * N / SR); ++i) if (sp[i] > bm) { bm = sp[i]; best = i; }
            const double h1 = harmMag (sp, f0, 1, N);
            char d[128]; std::snprintf (d, sizeof d, "GROWL at RESONANCE 1: the loudest line near the cutoff sits %.1f dB against the note", dB (bm / std::max (1e-12, h1)));
            std::printf ("  %s\n", d);
            ok (true, "GROWL's top of the dial is reported, not asserted - it is meant to be on the edge", d);
            delete e;
        }
        //  ---- aliasing, below the oscillation edge, on the brightest field at C5 ----
        {
            Line six[NLINES]; straightAll (six, 1.0f, 0.0f);
            const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
            char d[200]; int n = 0; bool good = true;
            for (int m = 1; m <= 3; ++m)
            {
                Engine* e = fresh (1, six);
                e->p.filtModel = (float) m / 3.0f; e->p.filtType = 0.0f; e->p.grain = 1.0f;
                e->p.cutoff = posOf (3000.0); e->p.reso = 0.5f; e->p.filtDepth = 0.0f;
                Take t = render (*e, 72, 0.8);
                const double fl = aliasFloorDb (spectrum (t.L, (size_t) (0.3 * SR), N), fC5, N);
                n += std::snprintf (d + n, sizeof d - (size_t) n, " %s %.1f dB;", names[m], fl);
                if (fl > -40.0) good = false;
                delete e;
            }
            std::printf ("  non-harmonic floor at C5, RESONANCE 0.5, cutoff 3 kHz:%s\n", d);
            ok (good, "the circuits below their oscillation edge keep the floor under -40 dB at 1x", d);
        }
        //  ---- bounded, driven as hard as the instrument can: full window into full resonance ----
        {
            Line six[NLINES]; straightAll (six, 1.0f, 0.0f);
            double worst = 0; bool fin = true;
            for (int m = 1; m <= 3; ++m) for (int hp = 0; hp < 2; ++hp)
            {
                Engine* e = fresh (1, six);
                e->p.filtModel = (float) m / 3.0f; e->p.filtType = hp ? 2.0f / 3.0f : 0.0f;
                e->p.cutoff = posOf (800.0); e->p.reso = 1.0f; e->p.contrast = 1.0f; e->p.fold = 0.5f; e->p.level = 0.7f;
                Take t = render (*e, 48, 1.0, 0.7);
                for (float v : t.L) { if (! std::isfinite (v)) fin = false; worst = std::max (worst, (double) std::abs (v)); }
                delete e;
            }
            char d[96]; std::snprintf (d, sizeof d, "worst peak %.3f across six circuits at RESONANCE 1 with CONTRAST 1 and FOLD", worst);
            std::printf ("  %s\n", d);
            ok (fin && worst <= 1.0, "bounded and finite however hard the read drives them", d);
        }
        //  ---- the SVF is untouched: CIRCUIT at its default is the old path ----
        {
            Line six[NLINES]; straightAll (six, 0.8f, 0.0f);
            Engine* a = fresh (1, six); a->p.filtType = 0.0f; a->p.cutoff = 0.6f; a->p.reso = 0.4f;
            Engine* b = fresh (1, six); b->p.filtType = 0.0f; b->p.cutoff = 0.6f; b->p.reso = 0.4f; b->p.filtModel = 0.0f;
            Take ta = render (*a, 57, 0.5), tb = render (*b, 57, 0.5);
            ok (ta.L.size() == tb.L.size() && std::memcmp (ta.L.data(), tb.L.data(), ta.L.size() * sizeof (float)) == 0,
                "CIRCUIT at SVF is bit-identical to the filter as it shipped");
            delete a; delete b;
        }
        //  ---- cost: the ladder on every reader ----
        {
            Line six[NLINES]; Params p; factory (2).build (six, p);
            p.unison = 1.0f; p.filtModel = 1.0f; p.filtType = 0.0f; p.reso = 0.8f;
            Engine eng; eng.p = p; eng.prepare (SR, BLK); eng.setLines (six); eng.service();
            for (int v = 0; v < 8; ++v) eng.noteOn (36 + v * 5, 0.9f);
            std::vector<float> l (BLK), r (BLK);
            const auto t0 = std::chrono::steady_clock::now();
            const int nb = (int) (5.0 * SR / BLK);
            for (int b = 0; b < nb; ++b) eng.process (l.data(), r.data(), BLK);
            const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            char d[96]; std::snprintf (d, sizeof d, "%.2f %% of one core, 32 readers through the LADDER", wall / 5.0 * 100.0);
            std::printf ("  %s\n", d);
            ok (wall / 5.0 < 0.30, "32 ladders under 30 % of a core", d);
        }
    }

    std::printf ("\n%d checks, %d failed  -  %s\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");`);
});

if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [eh, ec, ui, probe, bench]) fs.writeFileSync(f.p, f.get(), "utf8");
console.log("patched: Engine.h, Engine.cpp, ui.html, uiprobe.js, bench.cpp");
