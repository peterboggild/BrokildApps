/*  THREE SPECIMENS WERE MUTE, and eleven more nearly so — found while
    building the interlocutor bench, which happened to pick specimen 3.

    Cause: the 4D body used eigenvectors 1-4 as its spatial axes, normalised
    by min/max. A LOCALISED eigenvector (one node large, every other node
    flat — perfectly ordinary in tree and community topologies) then maps 67
    of 68 nodes to the same coordinate. Specimen 3's w-histogram was
    literally "1 node at -1, 67 nodes at +1": a body with no extent in the
    fourth dimension has no cross-section, so the slab caught nothing, so
    every mode weighed zero, so the artefact was silent.

    Two fixes, cause and symptom:

    1. THE AXES ARE THE FOUR MOST DELOCALISED LOW MODES, not simply the first
       four. A spatial axis must actually spread the body along itself;
       ranking the lowest dozen modes by inverse participation ratio and
       taking the four flattest is the standard spectral-embedding remedy and
       costs nothing at generation time. They are then re-sorted into
       eigenvalue order, so axis 4 remains the "slowest" of the chosen four.

    2. NORMALISATION BECOMES ROBUST: median-centred, scaled by the 10-90
       percentile spread, soft-bounded with tanh. Min/max normalisation lets
       a single outlier node squash the entire rest of the body into a point.

    3. And the vessel is never empty-handed: sliceWeights widens the slab
       until it contains tissue. Whatever the anatomy, the artefact speaks.

    This changes every specimen's BODY (positions, section behaviour, the
    panel) but not its VOICE: ratios come from eigenvalues, which are
    untouched. The alienness floor and the whole spectrum catalog stay
    bit-identical — the bench prints both.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
  for (const [a, b, tag] of subs) {
    const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(tag + " x" + n); continue; }
    s = s.split(A).join(B);
  }
  return () => fs.writeFileSync(path, s);
}

const wS = edit("C:/Users/peter/b/ArtefactB2311/Source/Specimen.cpp", [
[`    //  the body: eigenvectors 1..4 are the spatial axes (mode = dimension)
    auto axis = [&] (int which, float* dst)
    {
        const int m = order[std::min (which, n - 1)];
        double lo = 1e9, hi = -1e9;
        for (int i = 0; i < n; ++i)
        {
            const double v = V[(size_t) i * n + m];
            lo = std::min (lo, v); hi = std::max (hi, v);
        }
        const double span = std::max (1e-9, hi - lo);
        for (int i = 0; i < n; ++i)
            dst[i] = (float) (2.0 * (V[(size_t) i * n + m] - lo) / span - 1.0);
    };
    axis (1, out.px.data());
    axis (2, out.py.data());
    axis (3, out.pz.data());
    axis (4, out.pw.data());`,
`    /*  The body: four eigenvectors ARE its spatial axes (mode = dimension).
        But an axis has to spread the body along itself, and a LOCALISED
        eigenvector — one large node, the rest flat, entirely ordinary in
        tree and community topologies — does the opposite: it puts the whole
        creature at a single coordinate. Three catalog entries were mute
        because their w axis did exactly that: no extent in the fourth
        dimension means no cross-section, so nothing was ever in the slab.
        So: rank the lowest dozen modes by inverse participation ratio and
        take the four most delocalised, then restore eigenvalue order. */
    auto axis = [&] (int which, float* dst)
    {
        const int m = order[std::min (which, n - 1)];
        //  robust normalisation: median-centred, 10-90 percentile scale,
        //  soft-bounded. Min/max lets one outlier squash everything else.
        double raw[kMaxNodes], srt[kMaxNodes];
        for (int i = 0; i < n; ++i) raw[i] = V[(size_t) i * n + m];
        std::copy (raw, raw + n, srt);
        std::sort (srt, srt + n);
        const double med = srt[n / 2];
        const double p10 = srt[(int) (n * 0.10)];
        const double p90 = srt[(int) (n * 0.90)];
        const double sc = std::max (1e-12, std::max (p90 - med, med - p10));
        for (int i = 0; i < n; ++i)
            dst[i] = (float) std::tanh ((raw[i] - med) / sc);
    };
    {
        auto ipr = [&] (int idx) -> double
        {
            const int m = order[idx];
            double s2 = 0, s4 = 0;
            for (int i = 0; i < n; ++i)
            {
                const double v = V[(size_t) i * n + m];
                s2 += v * v; s4 += v * v * v * v;
            }
            return s2 > 1e-18 ? s4 / (s2 * s2) : 1.0;    // 1 = one node only
        };
        int cand[16], nc = 0;
        for (int c = 1; c <= std::min (n - 1, 13) && nc < 16; ++c) cand[nc++] = c;
        std::sort (cand, cand + nc, [&] (int a, int b) { return ipr (a) < ipr (b); });
        int ax[4];
        for (int t = 0; t < 4; ++t) ax[t] = cand[nc > t ? t : nc - 1];
        std::sort (ax, ax + 4);            // keep them in eigenvalue order
        axis (ax[0], out.px.data());
        axis (ax[1], out.py.data());
        axis (ax[2], out.pz.data());
        axis (ax[3], out.pw.data());
    }`, "axes"]
]);

const wE = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.cpp", [
[`static void sliceWeights (const Specimen& s, float w0, float eps,
                          std::array<float, kMaxModes>& out)
{
    for (int k = 0; k < s.nModes; ++k)`,
`static void sliceWeights (const Specimen& s, float w0, float eps0,
                          std::array<float, kMaxModes>& out)
{
    /*  The vessel is never empty-handed: if the slab happens to fall between
        the tissue, it widens until it holds some. Whatever the anatomy and
        wherever the winch is parked, the artefact speaks. */
    float eps = eps0;
    for (int pass = 0; pass < 6; ++pass)
    {
        int inside = 0;
        for (int i = 0; i < s.nNodes; ++i)
            if (std::fabs (s.pw[i] - w0) < eps) { ++inside; break; }
        if (inside > 0) break;
        eps *= 1.8f;
    }
    for (int k = 0; k < s.nModes; ++k)`, "slab floor"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wS(); wE();
console.log("axes: delocalised + robust; slab: never empty");
