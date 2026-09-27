/*  THE INTERLOCUTOR, phase B — the plugin.

    * OTHER is non-automatable, like SPECIMEN: dialling a body rebuilds it.
    * The message thread keeps the second body in step, and performs the
      remodelling the audio thread asked for (only ever at rest).
    * The panel is told about the second body, its living glow, the measured
      kinship, whether it is speaking, and how far both bodies have been
      remodelled by the conversation.
    * THE ACCENT IS SAVED. A body marked by a session must arrive marked in
      the next one, or "speech is surgery" is a lie the moment the project is
      closed. Stored as edge multipliers on the APVTS state, alongside the
      BWFX blob, and cleared by RESTORE.
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

const wH = edit("C:/Users/peter/b/ArtefactB2311/Source/PluginProcessor.h", [
[`    float lumPeak = 1e-6f;      // adaptive headroom for the glow stream`,
`    float lumPeak = 1e-6f;      // adaptive headroom for the glow stream
    float lumPeak2 = 1e-6f;     // ...and for the second body's
    int   otherLoadedUi = -1;
    int   accentSeen = -1;
    void  emitOther();
    juce::String accentToString (const float* a, int n) const;
    void  accentFromState (const juce::XmlElement& xml);`, "members"]
]);

const wC = edit("C:/Users/peter/b/ArtefactB2311/Source/PluginProcessor.cpp", [

//  OTHER rebuilds a body, so it is not an automation lane
[`        const bool automatable = juce::String (s.id) != "specimen";`,
`        //  SPECIMEN and OTHER each rebuild a whole being on the message
        //  thread; neither belongs on an automation lane (the mood-organ
        //  precedent from Blade Ruiner)
        const juce::String sid (s.id);
        const bool automatable = sid != "specimen" && sid != "other";`, "automatable"],

//  the accent rides in the session
[`        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));`,
`        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        /*  the accent: a body marked by a conversation must arrive marked.
            Edge multipliers, three decimals, and nothing at all when the
            bodies are still exactly their catalog selves. */
        {
            const auto a = accentToString (engine.accentSelf(), ab::kMaxEdges);
            const auto b = accentToString (engine.accentOther(), ab::kMaxEdges);
            if (a.isNotEmpty()) xml->setAttribute ("accA", a);
            if (b.isNotEmpty()) xml->setAttribute ("accB", b);
        }`, "save accent"],

[`            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            xml->removeAttribute ("bwfx");`,
`            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            xml->removeAttribute ("bwfx");
            accentFromState (*xml);
            xml->removeAttribute ("accA");
            xml->removeAttribute ("accB");`, "load accent"],

[`            engine.clearMemory();
            uiHasState = false;`,
`            engine.clearMemory();
            uiHasState = false;
            otherLoadedUi = -1;
            accentSeen = -1;`, "reset ui"],

//  the second body, its glow, and the state of the conversation
[`void ArtefactAudioProcessor::emitBwfx()`,
`juce::String ArtefactAudioProcessor::accentToString (const float* a, int n) const
{
    bool any = false;
    for (int e = 0; e < n; ++e)
        if (a[e] > 0.0f && std::fabs (a[e] - 1.0f) > 1.0e-4f) { any = true; break; }
    if (! any) return {};
    juce::String s;
    for (int e = 0; e < n; ++e)
    {
        if (e) s << ',';
        s << juce::String ((int) std::lround ((a[e] <= 0.0f ? 1.0f : a[e]) * 1000.0f));
    }
    return s;
}

void ArtefactAudioProcessor::accentFromState (const juce::XmlElement& xml)
{
    const auto sa = xml.getStringAttribute ("accA");
    const auto sb = xml.getStringAttribute ("accB");
    if (sa.isEmpty() && sb.isEmpty()) { engine.clearAccent(); return; }
    std::vector<float> A, B;
    auto parse = [] (const juce::String& s, std::vector<float>& out)
    {
        out.clear();
        if (s.isEmpty()) return;
        auto toks = juce::StringArray::fromTokens (s, ",", "");
        for (const auto& t : toks)
            out.push_back (juce::jlimit (0.05f, 6.0f, t.getIntValue() * 0.001f));
    };
    parse (sa, A);
    parse (sb, B);
    engine.setAccent (A.empty() ? nullptr : A.data(), (int) A.size(),
                      B.empty() ? nullptr : B.data(), (int) B.size());
}

void ArtefactAudioProcessor::emitOther()
{
    if (! emitToUi) return;
    const ab::Specimen& s = engine.otherSpecimen();
    auto* o = new juce::DynamicObject();
    o->setProperty ("cat", s.catalog);
    o->setProperty ("family", s.family);
    o->setProperty ("nNodes", s.nNodes);
    o->setProperty ("voiceHz", s.voiceHz);
    //  how much of each other the two bodies can physically hear
    o->setProperty ("kin", spectralKinship (s, engine.specimen()));

    juce::Array<juce::var> nodes;
    for (int i = 0; i < s.nNodes; ++i)
    {
        auto* nd = new juce::DynamicObject();
        nd->setProperty ("x", s.px[i]);
        nd->setProperty ("y", s.py[i]);
        nd->setProperty ("z", s.pz[i]);
        nd->setProperty ("w", s.pw[i]);
        nodes.add (juce::var (nd));
    }
    o->setProperty ("nodes", nodes);
    juce::Array<juce::var> edges;
    for (int e = 0; e < s.nEdges; ++e)
    {
        juce::Array<juce::var> pair;
        pair.add (s.edgeA[e]);
        pair.add (s.edgeB[e]);
        edges.add (juce::var (pair));
    }
    o->setProperty ("edges", edges);
    emitToUi ("other", juce::var (o));
}

void ArtefactAudioProcessor::emitBwfx()`, "emit other"],

//  keep the second body in step; do the remodelling; re-emit what changed
[`    if (! emitToUi) return;
    if (! uiHasState.exchange (true)) emitInitialState();`,
`    /*  the second body follows its parameter, and comes into existence the
        moment CONVERSE is raised above zero (generating a specimen is a
        message-thread job — the audio thread may never do it) */
    {
        const int wantO = (int) std::lround (raw[(size_t) ab::paramIndex ("other")]->load());
        const bool wantsConv = raw[(size_t) ab::paramIndex ("converse")]->load() > 0.0f;
        if (wantsConv && (wantO != otherLoadedUi || engine.otherLoadedCatalog() < 0))
        {
            engine.loadOther (wantO);
            otherLoadedUi = wantO;
            emitOther();
        }
    }

    /*  PLASTICITY: the audio thread has decided the bodies are at rest and
        the scars are deep enough. Re-solving eigenmodes is our work, not
        its. Both bodies may have changed, so both are re-sent. */
    if (engine.remodelPending())
    {
        engine.serviceRemodel();
        emitSpecimen();
        emitOther();
    }

    if (! emitToUi) return;
    if (! uiHasState.exchange (true)) emitInitialState();`, "timer other"],

//  the panel needs the conversation's state as well as the section's
[`        o->setProperty ("lum", lum);
        emitToUi ("sec", juce::var (o));`,
`        o->setProperty ("lum", lum);

        /*  the second body's own glow — lit by what it hears as much as by
            what it says — and the state of the conversation */
        if (engine.otherLoadedCatalog() >= 0
            && raw[(size_t) ab::paramIndex ("converse")]->load() > 0.0f)
        {
            const ab::Specimen& os = engine.otherSpecimen();
            float nl[ab::kMaxNodes] = { 0 };
            engine.nodeLuminanceOther (nl);
            float pk = 1e-6f;
            for (int i = 0; i < os.nNodes; ++i) pk = std::max (pk, nl[i]);
            lumPeak2 = std::max (pk, lumPeak2 * 0.985f);
            const float sc = 1.0f / (lumPeak2 * 1.1f + 1e-6f);
            juce::Array<juce::var> lum2;
            for (int i = 0; i < os.nNodes; ++i)
                lum2.add (juce::jlimit (0.0f, 1.0f, std::sqrt (nl[i] * sc)));
            o->setProperty ("lum2", lum2);
            o->setProperty ("kin", engine.debugKinship());
            o->setProperty ("say", engine.debugSpeaking() ? 1 : 0);
        }
        o->setProperty ("accent", engine.accentSteps());
        emitToUi ("sec", juce::var (o));`, "sec extras"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wH(); wC();
console.log("B: the plugin knows about the second body, and remembers its accent");
