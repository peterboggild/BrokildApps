// BUG HUNT — three real ones found by reading rather than by listening, plus
// the one optimisation that was actually costing something.
//
// 1. KEY MODE STOLE OTHER CHANNELS' NOTES. A channel switched to chromatic
//    play answers a two-octave range around its own note — and those ranges
//    overlap the GM map. Turn KEY on for BD1 (note 36) and it claimed 12..60,
//    which swallows the snare at 38 and the closed hat at 42: the rest of the
//    kit went silent. The fix is order. The exact GM map is consulted FIRST,
//    so every channel keeps its own note, and KEY MODE gets everything else.
//
// 2. RANDOM RANDOMISED THE TRANSPORT. It skipped the master and the
//    oversampling but not SEQ, TEMPO, MORPH, SWING, FEEL or GRIP — so pressing
//    RANDOM could start the sequencer, change the tempo, and throw a morph in.
//    Those belong to the performance, not to the sound.
//
// 3. PANNING COST TWO TRIG CALLS PER CHANNEL PER OVERSAMPLED SAMPLE — twenty
//    four of them, 2.3 million a second at 2x. Pan is constant across a block;
//    it is computed once per block now.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(p.split("/").pop() + ": " + t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

const wc = edit(R + "Source/Engine.cpp", [

// ---- 1 · KEY MODE must not steal mapped notes ----------------------------
[`    for (int c = 0; c < NCH; ++c)
        if (p.ch[c][CP_KEY] >= 0.5f)
        {
            const int root = CHANS[c].note;
            if (note >= root - 24 && note <= root + 24)
            {
                trigger (c, velocity, (float) (note - root));
                return;
            }
        }

    for (int c = 0; c < NCH; ++c)
        if (CHANS[c].note == note) { trigger (c, velocity); return; }`,
`    /*  The exact map wins. A KEY MODE channel answers a two-octave range, and
        those ranges overlap the GM map — with KEY on for BD1 (note 36) its
        range covered 12..60, which swallowed the snare at 38 and the closed
        hat at 42 and silenced most of the kit. So: a note that belongs to a
        channel goes to that channel, and KEY MODE gets everything else. */
    for (int c = 0; c < NCH; ++c)
        if (CHANS[c].note == note) { trigger (c, velocity); return; }

    for (int c = 0; c < NCH; ++c)
        if (p.ch[c][CP_KEY] >= 0.5f)
        {
            const int root = CHANS[c].note;
            if (note >= root - 24 && note <= root + 24)
            {
                trigger (c, velocity, (float) (note - root));
                return;
            }
        }`, "key mode order"],

// ---- 3 · the pan trig moves out of the inner loop ------------------------
[`    std::array<float, NCH> chMeter { };

    for (int i = 0; i < n; ++i)`,
`    std::array<float, NCH> chMeter { };

    /*  Pan is constant across a block, so its two trig calls belong here and
        not in the inner loop — where they were running twelve times per
        oversampled sample, 2.3 million times a second at 2x, to produce the
        same two numbers over and over. */
    std::array<float, NCH> panL { }, panR { };
    for (int c = 0; c < NCH; ++c)
    {
        float pan = clampf (p.ch[c][CP_PAN], 0.0f, 1.0f) * 2.0f - 1.0f;
        if (wmActive && wmPan != 0.0f)
            pan = clampf (pan + wmPan * (std::fmod ((float) c * 0.6180339887f, 1.0f) * 2.0f - 1.0f), -1.0f, 1.0f);
        const float th = (pan * 0.5f + 0.5f) * 1.5707963f;
        panL[(size_t) c] = std::cos (th);
        panR[(size_t) c] = std::sin (th);
    }

    for (int i = 0; i < n; ++i)`, "pan precompute"],

[`                float pan = clampf (P[CP_PAN], 0.0f, 1.0f) * 2.0f - 1.0f;
                if (wmActive && wmPan != 0.0f)
                    pan = clampf (pan + wmPan * (std::fmod ((float) c * 0.6180339887f, 1.0f) * 2.0f - 1.0f), -1.0f, 1.0f);

                const float th = (pan * 0.5f + 0.5f) * 1.5707963f;
                sumL += s * std::cos (th);
                sumR += s * std::sin (th);
                mono += s;`,
`                sumL += s * panL[(size_t) c];
                sumR += s * panR[(size_t) c];
                mono += s;`, "pan use"]
]);

// ---- 2 · RANDOM leaves the performance alone -----------------------------
const wp = edit(R + "Source/PluginProcessor.cpp", [
[`        const juce::String sid (s.id);
        if (sid == "os" || sid == "volume") continue;
        if (s.chan >= 0 && s.slot == fmr::CP_MUTE) continue;      // never mute a channel at random`,
`        const juce::String sid (s.id);
        /*  The transport and the performance controls are not part of a
            sound. RANDOM used to be able to start the sequencer, change the
            tempo and throw in a morph, which is not what anyone presses it
            for — the same list applyKitIndex leaves alone. */
        if (sid == "os" || sid == "volume" || sid == "seq" || sid == "tempo"
            || sid == "morph" || sid == "swing" || sid == "feel" || sid == "grip") continue;
        if (s.chan >= 0 && s.slot == fmr::CP_MUTE) continue;      // never mute a channel at random`, "random skips"]
]);

// ---- bench cover for both bugs -------------------------------------------
const wt = edit(R + "test/test.cpp", [
[`    //  and a low note must ring LONGER, as a bigger drum does`,
`    /*  KEY MODE must not steal notes that belong to other channels. With BD1
        chromatic, its range covers the snare and the hat — and they have to go
        on sounding, or turning KEY on silences most of the kit. */
    {
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        e.p.ch[0][CP_KEY] = 1.0f;                       // BD1 chromatic, root 36
        for (int c = 0; c < NCH; ++c) if (c != 2) e.p.ch[c][CP_MUTE] = 1.0f;   // hear SD1 only
        e.noteOn (38, 1.0f);                            // the snare's own note
        Buf b (24000); render (e, b);
        std::printf ("    with BD1 chromatic, note 38 still reaches SD 1: peak %.3f\\n", peak (b));
        ok (peak (b) > 0.05f, "KEY MODE does not steal another channel's note", peak (b), 0.05);
    }
    {
        //  and a note NOT on the map still reaches the chromatic channel
        Engine e; fresh (e);
        e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f;
        e.p.ch[0][CP_KEY] = 1.0f;
        for (int c = 0; c < NCH; ++c) if (c != 0) e.p.ch[c][CP_MUTE] = 1.0f;
        e.noteOn (31, 1.0f);                            // unmapped
        Buf b (24000); render (e, b);
        ok (peak (b) > 0.05f, "an unmapped note still plays the chromatic channel", peak (b), 0.05);
    }

    //  and a low note must ring LONGER, as a bigger drum does`, "keymode bench"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wc(); wp(); wt();
console.log("bug hunt: key-mode note stealing, random touching the transport, pan trig in the inner loop");
