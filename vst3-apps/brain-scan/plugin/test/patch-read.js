// 260904.3 — the READ update: patches ui.html, bench.cpp and PluginProcessor.cpp
// with exact-count anchors. Nothing is written unless every anchor matches once.
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
    if (n !== count) { misses.push(`${rel}: expected ${count} of [${from.slice(0, 60)}...], found ${n}`); return; }
    s = s.split(f).join(t);
  };
  fn(rep, () => s);
  return { p, get: () => s };
}

// ---------------------------------------------------------------- ui.html
const ui = edit("Source/ui/ui.html", (rep, cur) => {
  rep(String.raw`      L_SPEC  = ["SINUS","SPINE","PULSE","MARROW","NERVE","RETINA","LUNG","SKULL","CORTEX"];`,
      String.raw`      L_SPEC  = ["SINUS","SPINE","PULSE","MARROW","NERVE","RETINA","LUNG","SKULL","CORTEX","SUTURE","ENAMEL","TENDON"];`);
  rep(String.raw`  ["specimen","SPECIMEN",KP.LIST,1.00,"the volume the lines read",0,8,L_SPEC],`,
      String.raw`  ["specimen","SPECIMEN",KP.LIST,0.0909,"the volume the lines read",0,11,L_SPEC],
  ["grain","GRAIN",KP.PCT,0.35,"how sharply the tissue is read: smoothed, or texel by texel",0,0],
  ["contrast","CONTRAST",KP.PCT,0.00,"the audio window: narrow it and the read saturates, a sine toward a square",0,0],
  ["fold","FOLD",KP.PCT,0.00,"what the window does past its edges: clip, or fold back in",0,0],`);
  rep(String.raw`  const r1 = mkRow(m1);
  mkKnob(r1, "scanA"); mkKnob(r1, "scanD"); mkKnob(r1, "scanAmt");

  const m2 = mkMod(1.30, "FILTER");`,
      String.raw`  const r1 = mkRow(m1);
  mkKnob(r1, "scanA"); mkKnob(r1, "scanD"); mkKnob(r1, "scanAmt");

  /*  THE READ — how the tissue becomes a waveform. GRAIN is the kernel
      (smoothed or texel by texel), CONTRAST the CT window applied to the
      audio, FOLD what the window does past its edges. */
  const m1b = mkMod(1.00, "READ · WINDOW");
  const r1b = mkRow(m1b);
  mkKnob(r1b, "grain", { hint:"how sharply the tissue is read — 0 smooths across texels (the soft, rounded read), 1 passes every texel as it is, kinks and grain included" });
  mkKnob(r1b, "contrast", { hint:"the CT window applied to the sound: narrow it and the read saturates against the window's edges — a sine leans toward a square, and there is more for the filter to bite on" });
  mkKnob(r1b, "fold", { hint:"what happens to the waveform past the window's edges: 0 clips it flat, 1 folds it back into the window — the folds add harmonics the tissue never had" });

  const m2 = mkMod(1.30, "FILTER");`);
});

// ---------------------------------------------------------------- bench.cpp
const bench = edit("test/bench.cpp", (rep) => {
  rep(String.raw`            int ext[3]; cubeExtent (cube, VN, 0.5f, ext);
            std::snprintf (d, sizeof d, "sphere reads %d x %d x %d voxels (want ~48 each)", ext[0], ext[1], ext[2]);
            std::printf ("  %s\n", d);
            /*  one source voxel on the coarse axis is 2 mm = 3.2 output voxels,
                and a 0.5 threshold on a partial-volume edge costs about that. */
            ok (std::abs (ext[0] - ext[2]) <= 5 && std::abs (ext[1] - ext[2]) <= 5,
                "and a sphere in it is still a sphere - THE anisotropy check", d);
            ok (ext[2] > 40, "resampling by INDEX would read about 12 voxels there; this reads the millimetres", d);`,
      String.raw`            int ext[3]; cubeExtent (cube, VN, 0.5f, ext);
            const int wantExt = (int) std::lround (0.75 * VN);          // a 30 mm sphere in a 40 mm cube
            std::snprintf (d, sizeof d, "sphere reads %d x %d x %d voxels (want ~%d each)", ext[0], ext[1], ext[2], wantExt);
            std::printf ("  %s\n", d);
            /*  one source voxel on the coarse axis is 2 mm = VN/20 output
                voxels, and a 0.5 threshold on a partial-volume edge costs
                about that. */
            const int tol = VN / 20 + 2;
            ok (std::abs (ext[0] - ext[2]) <= tol && std::abs (ext[1] - ext[2]) <= tol,
                "and a sphere in it is still a sphere - THE anisotropy check", d);
            ok (ext[2] > wantExt * 5 / 6, "resampling by INDEX would read about a quarter of that there; this reads the millimetres", d);`);

  rep(String.raw`    e->p.scanAmt = 0.5f;                     // no scan envelope`,
      String.raw`    e->p.grain = 0.0f;                       // the smoothing kernel: the formula checks were written against it; GRAIN has its own section
    e->p.scanAmt = 0.5f;                     // no scan envelope`);

  rep(String.raw`    std::printf ("\n%d checks, %d failed  -  %s\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");`,
      String.raw`    //==========================================================================
    head ("9 - the read: GRAIN, the window, the gritty three (260904.3)");
    {
        //  ---- GRAIN passes the top octave the B-spline muffled --------------
        {
            /*  SPINE at y = 1 carries 48 harmonics. A full-span line reads
                128 texels, so harmonic 40 sits at 0.62 of the texel Nyquist,
                where the B-spline's sinc^4 costs about 6 dB and Catmull-Rom
                about 1. */
            Line six[NLINES]; straightAll (six, 1.0f, 0.0f);
            const double fA2 = 110.0;                                   // A2: the 40th at 4.4 kHz
            double h40[2], h20[2];
            for (int g = 0; g < 2; ++g)
            {
                Engine* e = fresh (1, six);
                e->p.grain = (float) g;
                Take t = render (*e, 45, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                const double h1 = std::max (1e-12, harmMag (m, fA2, 1, N));
                h40[g] = dB (harmMag (m, fA2, 40, N) / h1);
                h20[g] = dB (harmMag (m, fA2, 20, N) / h1);
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "vs the fundamental: H20 %.1f -> %.1f dB, H40 %.1f -> %.1f dB (grain 0 -> 1)", h20[0], h20[1], h40[0], h40[1]);
            std::printf ("  %s\n", d);
            ok (h40[1] - h40[0] > 3.0, "GRAIN 1 lifts harmonic 40 by more than 3 dB", d);
            ok (std::abs (h20[1] - h20[0]) < 3.0, "and leaves the middle of the band nearly alone (it is a kernel, not a tilt)", d);
        }
        //  ---- CONTRAST: a sine leans toward a square, monotonically ----------
        {
            Line six[NLINES]; straightAll (six, 0.5f, 0.0f);
            double thd[5]; bool mono = true;
            for (int q = 0; q < 5; ++q)
            {
                Engine* e = fresh (0, six);
                e->p.contrast = 0.25f * (float) q;
                Take t = render (*e, 57, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                const double h1 = harmMag (m, f0, 1, N);
                double hs = 0; for (int k = 2; k <= 24; ++k) { const double h = harmMag (m, f0, k, N); hs += h * h; }
                thd[q] = dB (std::sqrt (hs) / std::max (1e-12, h1));
                if (q > 0 && thd[q] <= thd[q - 1]) mono = false;
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "SINUS THD vs contrast 0 .. 1: %.1f %.1f %.1f %.1f %.1f dB", thd[0], thd[1], thd[2], thd[3], thd[4]);
            std::printf ("  %s\n", d);
            ok (thd[0] < -60.0, "at CONTRAST 0 the window is not there: SINUS still reads a sine", d);
            ok (mono && thd[4] > -12.0, "and narrowing it raises the harmonics monotonically toward a square's", d);
        }
        //  ---- the window is anti-aliased: SPINE bright, contrast 1, C5 -------
        {
            /*  The worst case: the brightest field, the hardest window, a
                note high enough that the clip's harmonics pass Nyquist within
                a few. The ADAA and the coarser level the budget chooses must
                keep the non-harmonic floor down. */
            Line six[NLINES]; straightAll (six, 1.0f, 0.0f);
            const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
            double fl[2]; int lv[2];
            for (int q = 0; q < 2; ++q)
            {
                Engine* e = fresh (1, six);
                e->p.grain = 1.0f; e->p.contrast = (float) q;
                Take t = render (*e, 72, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                fl[q] = aliasFloorDb (m, fC5, N);
                lv[q] = e->voiceLod (0);
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "C5, SPINE y = 1, GRAIN 1: floor %.1f dB at level %d plain, %.1f dB at level %d with CONTRAST 1", fl[0], lv[0], fl[1], lv[1]);
            std::printf ("  %s\n", d);
            ok (fl[0] < -40.0, "the interpolating kernel alone keeps the alias floor under -40 dB at C5", d);
            ok (fl[1] < -30.0, "the hardest window at C5 keeps its aliasing 30 dB under the harmonics", d);
        }
        //  ---- FOLD: past the window the wave comes back in ------------------
        {
            Line six[NLINES]; straightAll (six, 0.5f, 0.0f);
            double pk[2], thd[2];
            for (int f = 0; f < 2; ++f)
            {
                Engine* e = fresh (0, six);
                e->p.contrast = 0.5f; e->p.fold = (float) f;          // gain 4
                Take t = render (*e, 57, 0.8);
                pk[f] = 0; for (size_t i = (size_t) (0.3 * SR); i < t.L.size(); ++i) pk[f] = std::max (pk[f], (double) std::abs (t.L[i]));
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                const double h1 = harmMag (m, f0, 1, N);
                double hs = 0; for (int k = 2; k <= 24; ++k) { const double h = harmMag (m, f0, k, N); hs += h * h; }
                thd[f] = dB (std::sqrt (hs) / std::max (1e-12, h1));
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "gain 4: clip THD %.1f dB peak %.3f; fold THD %.1f dB peak %.3f", thd[0], pk[0], thd[1], pk[1]);
            std::printf ("  %s\n", d);
            ok (pk[1] <= pk[0] * 1.05 + 1e-3, "a folded wave stays inside the window (no louder than the clipped one)", d);
            ok (thd[1] > -12.0, "and the fold is rich (a sine at gain 4 folds back on itself)", d);
        }
        //  ---- the gritty three: made of edges ------------------------------
        {
            /*  Energy above the 8th harmonic against the fundamental — the
                part of the spectrum a filter has something to do with. The
                old bread (SPINE at its middle) sits deep; the new tissue does
                not. TENDON's bell ratios are cut to one cycle, so its content
                is HARMONIC like every single-cycle read (a wavetable cannot
                be otherwise); what it carries is the strike at the wrap. */
            auto topDb = [&] (int spec, float y, float z, float grain)
            {
                Line six[NLINES]; straightAll (six, y, z);
                Engine* e = fresh (spec, six);
                e->p.grain = grain;
                Take t = render (*e, 57, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                const double h1 = harmMag (m, f0, 1, N);
                double hs = 0; for (int k = 9; k <= 48; ++k) { const double h = harmMag (m, f0, k, N); hs += h * h; }
                delete e;
                return dB (std::sqrt (hs) / std::max (1e-12, h1));
            };
            const double spine = topDb (1, 0.5f, 0.0f, 0.35f);
            const double sut   = topDb (9, 0.5f, 0.5f, 1.0f);
            const double ena   = topDb (10, 0.0f, 0.0f, 1.0f);
            const double ten   = topDb (11, 1.0f, 1.0f, 0.5f);
            char d[160]; std::snprintf (d, sizeof d, "above the 8th harmonic: SPINE mid %.1f, SUTURE %.1f, ENAMEL %.1f, TENDON %.1f dB", spine, sut, ena, ten);
            std::printf ("  %s\n", d);
            ok (sut > spine + 12.0 && ena > spine + 12.0, "SUTURE and ENAMEL carry 12 dB more top than SPINE's middle", d);
            ok (ten > spine + 6.0, "TENDON's strike is brighter than SPINE's middle too", d);
        }
        //  ---- the build: two million texels, published from a worker --------
        {
            Engine e; e.p = Params(); e.prepare (SR, BLK);
            e.p.specimen = 8.0f / 11.0f;                               // CORTEX: in the cache from section 7
            e.serviceAsync();
            int polls = 0;
            while (e.specimenLoaded() != 8 && polls < 3000) { std::this_thread::sleep_for (std::chrono::milliseconds (2)); e.serviceAsync(); ++polls; }
            char d[128]; std::snprintf (d, sizeof d, "serviceAsync published CORTEX after %d polls", polls);
            std::printf ("  %s\n", d);
            ok (e.specimenLoaded() == 8, "serviceAsync builds on a worker and publishes on a later call", d);
            e.p.specimen = 3.0f / 11.0f;                               // MARROW: dropped from the cache by the last four
            const auto t0 = std::chrono::steady_clock::now();
            e.service();
            const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::snprintf (d, sizeof d, "a full synchronous build of MARROW: %.0f ms (%d^3 texels, %d threads)", wall * 1000.0, VN, (int) std::thread::hardware_concurrency());
            std::printf ("  %s\n", d);
            ok (e.specimenLoaded() == 3 && wall < 3.0, "a full build stays under three seconds", d);
        }
        //  ---- cost with the window and the interpolating kernel on -----------
        {
            Line six[NLINES]; Params p; factory (2).build (six, p);
            p.unison = 1.0f; p.grain = 1.0f; p.contrast = 0.6f; p.fold = 0.5f;
            Engine eng; eng.p = p; eng.prepare (SR, BLK); eng.setLines (six); eng.service();
            for (int v = 0; v < 8; ++v) eng.noteOn (36 + v * 5, 0.9f);
            std::vector<float> l (BLK), r (BLK);
            const auto t0 = std::chrono::steady_clock::now();
            const int nb = (int) (5.0 * SR / BLK);
            for (int b = 0; b < nb; ++b) eng.process (l.data(), r.data(), BLK);
            const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            char d[96]; std::snprintf (d, sizeof d, "%.2f %% of one core, 32 readers, GRAIN 1 + window + fold", wall / 5.0 * 100.0);
            std::printf ("  %s\n", d);
            ok (wall / 5.0 < 0.18, "the window costs little: 32 readers with everything on under 18 % of a core", d);
        }
    }

    std::printf ("\n%d checks, %d failed  -  %s\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");`);
});

// ---------------------------------------------------------------- PluginProcessor.cpp
const proc = edit("Source/PluginProcessor.cpp", (rep) => {
  rep(String.raw`        if (std::strcmp (paramSpec (i).id, "specimen") == 0) engine.p.specimen = raw[(size_t) i]->load();
    engine.service();
    if (engine.specimenLoaded() != specimenSent) emitVolume();`,
      String.raw`        if (std::strcmp (paramSpec (i).id, "specimen") == 0) engine.p.specimen = raw[(size_t) i]->load();
    engine.serviceAsync();                 // built on a worker; published on a later tick
    if (engine.specimenLoaded() != specimenSent) emitVolume();`);
  rep(String.raw`    if (v.specimen == -1 || v.lod[0].empty()) return;
    std::vector<uint8_t> bytes (v.lod[0].size());
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = (uint8_t) juce::jlimit (0, 255, (int) std::lround (v.lod[0][i] * 255.0f));`,
      String.raw`    /*  The page renders 64^3 (a 262 144-texel stream and an R8 texture it
        was built around); the engine reads 128^3. It gets the level whose
        side is 64 — the same bytes as before, binomially averaged. */
    const int lv = v.levelWithSide (64);
    if (v.specimen == -1 || v.lod[lv].empty()) return;
    std::vector<uint8_t> bytes (v.lod[lv].size());
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = (uint8_t) juce::jlimit (0, 255, (int) std::lround (v.lod[lv][i] * 255.0f));`);
  rep(String.raw`    o->setProperty ("n", VN);`, String.raw`    o->setProperty ("n", v.side[lv]);`);
});

if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [ui, bench, proc]) fs.writeFileSync(f.p, f.get(), "utf8");
console.log("patched: ui.html, bench.cpp, PluginProcessor.cpp");
