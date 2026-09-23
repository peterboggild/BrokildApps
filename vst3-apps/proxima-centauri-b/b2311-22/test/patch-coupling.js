/*  Peter: "no clear relation between the object and the sound … check that
    everything is working as it should and all couplings are live."

    Audit result — he was right, and the biggest coupling was a LIE:

    1. THE LUM STREAM WAS FAKE. The only sound->visual channel lit
       vec^2 * totalEnergy summed over modes — and eigenvectors are
       orthonormal, so that sum is the SAME at every node. The glow was one
       loudness scalar painted uniformly; migration, excitation location and
       section audibility were all invisible. Now the engine reports the
       truth: per-node energy = sum over voices and modes of (amp_k * vec_ki)^2
       — the sound's actual place on the body.

    2. SOUND NEVER SHAPED THE FORM. Node size was depth-only, the breathing
       rotation was pure clock, the fringes had no audio term. Now tissue
       SWELLS where the sound lives, edges become glowing VEINS when energy
       flows along them (migration made visible), the body's slow stirring
       accelerates while it speaks and stills when silent, the interference
       fringes ride the note's envelope, and lit tissue warms in hue.

    3. THE OBJECT NOW EXPLORES ITSELF. WAKE's wander was ~40x too weak to
       beat the winch pull (integrated sin * dt against an 0.85/s recall) —
       it moved w0 by ~0.01. The wander now rides the winch TARGET, so the
       section genuinely patrols (about +/-0.45 at full wake) — and WAKE
       defaults to 0.15, so a fresh artefact idles alive: the slab drifts,
       organs condense and evaporate, and a held note's audible mode-mix
       changes as the body wanders through the plane. Sustain floor raised
       so the bed the note rings down into is properly audible.
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

/* ── engine: honest node luminance + a real wander + audible bed ─────── */
const wH = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.h", [
[`    float debugEnergySum (int vi) const`,
`    /*  the sound's actual place on the body: per-node energy summed over
        active voices and all modes — (amp_k * vec_ki)^2. View-only (message
        thread reads amps relaxed; a torn float lights a pixel wrongly for
        one frame, nothing more). */
    void nodeLuminance (float* out) const
    {
        const Specimen& s = specimen();
        for (int i = 0; i < s.nNodes; ++i) out[i] = 0.0f;
        for (const auto& v : voices)
        {
            if (! v.active) continue;
            for (int k = 0; k < s.nModes; ++k)
            {
                const float a = v.amp[(size_t) k];
                if (a <= 1e-5f) continue;
                const float a2 = a * a;
                const float* col = &s.vec[(size_t) k * s.nNodes];
                for (int i = 0; i < s.nNodes; ++i)
                    out[i] += a2 * col[i] * col[i];
            }
        }
    }

    float debugEnergySum (int vi) const`, "node lum"]
]);

const wC = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.cpp", [

//  wake wander rides the winch target: the section genuinely patrols
[`    const float winchTarget = (clamp01 (p.winch) - 0.5f) * 2.0f;
    w0 += wVel * dt;
    wVel *= std::exp (-dt / 1.4f);
    w0 += (winchTarget - w0) * (1.0f - std::exp (-dt * 0.85f));
    if (p.wake > 0.0f)
    {
        subPhase += dt * (0.02f + 0.05f * p.wake);
        w0 += p.wake * 0.10f * std::sin (subPhase * 6.2831853f) * dt;
    }`,
`    /*  WAKE is the artefact exploring itself: the wander rides the winch
        TARGET (the old form added sin*dt against the 0.85/s recall — a
        ~40x-too-weak drive that moved w0 by ~0.01 and was invisible).
        The section now genuinely patrols, and everything downstream —
        slab membership, mode audibility, ghosts — moves with it. */
    float winchTarget = (clamp01 (p.winch) - 0.5f) * 2.0f;
    if (p.wake > 0.0f)
    {
        subPhase += dt * (0.015f + 0.05f * p.wake);
        winchTarget += p.wake * 0.45f * std::sin (subPhase * 6.2831853f);
    }
    w0 += wVel * dt;
    wVel *= std::exp (-dt / 1.4f);
    w0 += (winchTarget - w0) * (1.0f - std::exp (-dt * 0.85f));`, "wander"],

//  the bed the note rings down into must be properly audible
[`    const float sustain = 0.10f + 0.30f * ap;`,
 `    const float sustain = 0.16f + 0.34f * ap;`, "sustain"],

//  WAKE defaults on: a fresh artefact idles alive (still silent — wake only
//  feeds energy into ACTIVE voices; the no-note contract holds)
[`    { "wake",      "WAKE",       0.0f,  KP_PCT, 0, 1, &Params::wake },`,
 `    { "wake",      "WAKE",       0.15f, KP_PCT, 0, 1, &Params::wake },`, "wake default"]
]);

/* ── processor: replace the fake glow with the truth ─────────────────── */
const wP = edit("C:/Users/peter/b/ArtefactB2311/Source/PluginProcessor.cpp", [
[`        //  per-node luminance: where the sound currently lives on the body
        const ab::Specimen& s = engine.specimen();
        juce::Array<juce::var> lum;
        {
            float nl[ab::kMaxNodes] = { 0 };
            for (int vi = 0; vi < ab::kVoices; ++vi)
            {
                const float es = engine.debugEnergySum (vi);
                if (es <= 0.0f) continue;
                //  approximate: light the excitation profile of the voice via
                //  the section-weighted mode energies is engine-internal; the
                //  panel only needs a plausible glow, so use mode energies
                //  projected through the eigenvectors' squares
                for (int k = 0; k < s.nModes; k += 2)
                    for (int i = 0; i < s.nNodes; i += 1)
                    {
                        const float vv = s.vec[(size_t) k * s.nNodes + i];
                        nl[i] += vv * vv * es * 0.5f;
                    }
                break;   // the loudest voice is plenty for a glow
            }
            for (int i = 0; i < s.nNodes; ++i)
                lum.add (juce::jlimit (0.0f, 1.0f, nl[i] * 1.2f));
        }`,
`        //  per-node luminance: where the sound ACTUALLY lives on the body.
        //  (The first build summed vec^2 over modes — orthonormal, so the
        //  same at every node: a uniform glow carrying zero information.
        //  This is the real thing, from the realised per-mode amplitudes.)
        const ab::Specimen& s = engine.specimen();
        juce::Array<juce::var> lum;
        {
            float nl[ab::kMaxNodes] = { 0 };
            engine.nodeLuminance (nl);
            float pk = 1e-6f;
            for (int i = 0; i < s.nNodes; ++i) pk = std::max (pk, nl[i]);
            //  slow adaptive headroom so both a strike and its quiet bed read
            lumPeak = std::max (pk, lumPeak * 0.985f);
            const float sc = 1.0f / (lumPeak * 1.1f + 1e-6f);
            for (int i = 0; i < s.nNodes; ++i)
                lum.add (juce::jlimit (0.0f, 1.0f, std::sqrt (nl[i] * sc)));
        }`, "true lum"]
]);

const wPh = edit("C:/Users/peter/b/ArtefactB2311/Source/PluginProcessor.h", [
[`    ab::Engine engine;`,
`    ab::Engine engine;
    float lumPeak = 1e-6f;      // adaptive headroom for the glow stream`, "lumPeak"]
]);

/* ── the panel: sound shapes the form ────────────────────────────────── */
const wU = edit("C:/Users/peter/b/ArtefactB2311/Source/ui/ui.html", [

//  a smoothed audio level, and the body stirs while it speaks
[`let theta = 0;`,
`let theta = 0;
let AUD = 0, stirAcc = 0;     // smoothed audio level; sound-driven stirring`, "aud state"],

[`  theta = 0.16 * Math.sin(t * 0.05) + 0.07 * Math.sin(t * 0.013);   // breathing parallax`,
`  /*  the body's slow stirring accelerates while it SPEAKS and stills when
      silent — shape motion is now caused by sound, not merely coincident */
  const tB = t + stirAcc;
  theta = 0.16 * Math.sin(tB * 0.05) + 0.07 * Math.sin(tB * 0.013);`, "stir"],

//  tissue swells where the sound lives; lit tissue warms in hue
[`    if (P.vis[i]) {
      items.push([P.sx[i], P.sy[i], P.size[i] * 1.35, 0.85, hue, 0]);
      /* the gripped organ flares: the body visibly answers the hand */
      const grip = gest && pin.set && pin.set.has(i) ? 0.5 : 0;
      const glo = Math.min(1, (LUM[i] || 0) * 0.8 + grip);
      if (glo > 0.01)
        items.push([P.sx[i], P.sy[i], P.size[i] * 1.7, glo, hue, 2]);`,
`    if (P.vis[i]) {
      /* tissue SWELLS where the sound lives — the form is the spectrum */
      const li = LUM[i] || 0;
      const sw = 1 + li * 0.85;
      items.push([P.sx[i], P.sy[i], P.size[i] * 1.35 * sw, 0.85, hue + li * 0.05, 0]);
      /* the gripped organ flares: the body visibly answers the hand */
      const grip = gest && pin.set && pin.set.has(i) ? 0.5 : 0;
      const glo = Math.min(1, li * 0.9 + grip);
      if (glo > 0.01)
        items.push([P.sx[i], P.sy[i], P.size[i] * (1.7 + li * 0.6), glo, hue + li * 0.05, 2]);`, "swell"],

//  edges become veins when energy flows along them (migration visible)
[`                  (P.size[a] + (P.size[b] - P.size[a]) * f) * 0.52, 0.42, hue, 0]);`,
`                  (P.size[a] + (P.size[b] - P.size[a]) * f) * 0.52 * (1 + vein * 0.7),
                  0.42 + vein * 0.5, hue + vein * 0.05, vein > 0.25 ? 2 : 0]);`, "vein amt"],

//  fringes ride the note's envelope, not only motion
[`  gl.uniform1f(uComp.motion, clamp(Math.abs(secV) * 7 + cursorSpeed * 0.004, 0, 1));`,
`  gl.uniform1f(uComp.motion, clamp(Math.abs(secV) * 7 + cursorSpeed * 0.004 + AUD * 0.55, 0, 1));`, "fringe aud"],

//  per-frame: smooth the audio level, advance the stirring
[`  cursorSpeed *= 0.94;               // motion-vision needs stillness to exist`,
`  cursorSpeed *= 0.94;               // motion-vision needs stillness to exist
  let __ls = 0;
  for (let i = 0; i < LUM.length; i++) __ls += LUM[i];
  AUD = AUD * 0.88 + Math.min(1, __ls / (LUM.length * 0.22 + 1)) * 0.12;
  stirAcc += AUD * 0.045;            // the body stirs while it speaks`, "aud tick"],

//  the vein needs computing where the strand loop knows its endpoints
[`    const hue = ((SPEC.tags[a] + SPEC.tags[b]) / 12);
    for (let s = 1; s <= 3; s++) {`,
`    const hue = ((SPEC.tags[a] + SPEC.tags[b]) / 12);
    const vein = Math.min(LUM[a] || 0, LUM[b] || 0) * 2.2;
    for (let s = 1; s <= 3; s++) {`, "vein def"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wH(); wC(); wP(); wPh(); wU();
console.log("couplings live: true lum, swell, veins, stir, fringes, patrol, wake on");
