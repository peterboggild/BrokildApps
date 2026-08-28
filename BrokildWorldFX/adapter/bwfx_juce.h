#pragma once

// The JUCE side of the BWFX adapter, shared by every host synth so the
// message pipe is written ONCE. Header-only; include from the host's
// PluginProcessor. (The bwfx core stays JUCE-free — this file is the only
// place the two meet.)
//
// Host recipe (the four calls, see BWFX-DESIGN.md):
//   1. bwfxRack.prepare(fs, maxBlock) in prepareToPlay;
//      bwfxRack.process(L, R, n) after the engine in processBlock;
//      bwfxRack.service() from a ~15 Hz timer that runs with the editor
//      CLOSED too (reverb IR builds — a project restore can enable the
//      reverb long before the window opens).
//   2. Store bwfxRack.toJson() in project state AND user patch files;
//      restore with fromJson() (empty string = default empty rack, so old
//      projects are untouched). A patch stores its own rack.
//   3. Route any UI message with k=="bwfx" into bwfx_juce::handleMessage;
//      when it returns true (or after a patch load), emit
//      bwfx_juce::stateVar() to the page as event "bwfx". Serve
//      ui/bwfx-rack.js beside the panel page and add the ONE
//      [data-bwfx-open] button.
//   4. Map bwfxRack.worldMod() onto the voice engine (neutral until the
//      SPECTRA characters land — safe to defer).

#include <JuceHeader.h>
#include "../src/bwfx.h"

namespace bwfx_juce
{

inline int typeByName (const juce::String& id)
{
    for (int t = 0; t < bwfx::numModuleTypes(); ++t)
        if (id == bwfx::moduleDescriptor (t).id) return t;
    return -1;
}

// Handle one {k:"bwfx", op:...} message from the shared rack UI.
// Returns true when the caller should answer with the full state
// (op "init" — the overlay asking for the native truth).
inline bool handleMessage (bwfx::Rack& rack, const juce::var& m)
{
    const juce::String op = m.getProperty ("op", juce::var()).toString();

    if (op == "set")
    {
        const int t = typeByName (m.getProperty ("m", juce::var()).toString());
        if (t < 0) return false;
        const auto& d = bwfx::moduleDescriptor (t);
        const juce::String pid = m.getProperty ("p", juce::var()).toString();
        for (int p = 0; p < d.numParams; ++p)
            if (pid == d.params[p].id)
            {
                rack.setParam (t, p, (float) (double) m.getProperty ("v", 0.0));
                return false;
            }
    }
    else if (op == "enable")
    {
        const int t = typeByName (m.getProperty ("m", juce::var()).toString());
        if (t >= 0) rack.setEnabled (t, (int) m.getProperty ("on", 0) != 0);
    }
    else if (op == "presence")
    {
        const int t = typeByName (m.getProperty ("m", juce::var()).toString());
        if (t >= 0) rack.setPresence (t, (float) (double) m.getProperty ("v", 1.0));
    }
    else if (op == "order")
    {
        if (auto* arr = m.getProperty ("order", juce::var()).getArray())
        {
            int order[bwfx::kMaxModules];
            int count = 0;
            for (const auto& e : *arr)
            {
                const int t = typeByName (e.toString());
                if (t >= 0 && count < bwfx::kMaxModules) order[count++] = t;
            }
            rack.setOrder (order, count);
        }
    }
    else if (op == "mix")
    {
        rack.setMix ((float) (double) m.getProperty ("v", 1.0));
    }
    else if (op == "extra")     // opaque module state (the KIERANATOR pattern)
    {
        const int t = typeByName (m.getProperty ("m", juce::var()).toString());
        if (t >= 0) rack.setExtra (t, m.getProperty ("x", juce::var ("")).toString().toStdString());
    }
    else if (op == "cenable" || op == "cset" || op == "cpresence")   // SPECTRA
    {
        const juce::String id = m.getProperty ("m", juce::var()).toString();
        int c = -1;
        for (int i = 0; i < bwfx::numCharacters(); ++i)
            if (id == bwfx::characterDescriptor (i).id) { c = i; break; }
        if (c < 0) return false;
        if (op == "cenable") rack.setCharArmed (c, (int) m.getProperty ("on", 0) != 0);
        else if (op == "cpresence") rack.setCharPresence (c, (float) (double) m.getProperty ("v", 1.0));
        else
        {
            const auto& d = bwfx::characterDescriptor (c);
            const juce::String pid = m.getProperty ("p", juce::var()).toString();
            for (int p = 0; p < d.numParams; ++p)
                if (pid == d.params[p].id)
                { rack.setCharParam (c, p, (float) (double) m.getProperty ("v", 0.0)); break; }
        }
    }
    else if (op == "macro")
    {
        /*  Handled by the APVTS-aware overload below, never here. A macro's
            value belongs to the HOST PARAMETER, and pushMacros writes that
            parameter into the rack once per block — so anything this function
            stored would be overwritten by the next audio block, which is
            exactly what happened: the rail moved and nothing changed. */
    }
    else if (op == "assign")    // arm-and-click: wire a macro to a control
    {
        /*  The overlay speaks PERCENT, the way macroAssignJson() writes it;
            the core works in -1..+1. Without this divide every depth clamped
            to full, so only +-100 appeared to work — and it worked by
            accident rather than by agreement. */
        rack.setMacroAssign ((int) m.getProperty ("i", 0),
                             m.getProperty ("d", juce::var ("")).toString().toStdString(),
                             (float) ((double) m.getProperty ("a", 0.0) / 100.0));
        return true;
    }
    else if (op == "unassign")  // clear one macro entirely
    {
        rack.clearMacroAssigns ((int) m.getProperty ("i", 0));
        return true;
    }
    else if (op == "blob")      // apply a whole rack blob (the preset path)
    {
        rack.fromJson (m.getProperty ("j", juce::var ("")).toString().toStdString());
        return true;            // caller re-emits state so the UI adopts it
    }
    else if (op == "init")
    {
        return true;
    }
    return false;
}

// The payload for the "bwfx" event: descriptors + full rack state, plus
// whether THIS host's engine consumes the world-mod bus (the overlay shows
// the SPECTRA rack live only where it actually does something).
inline juce::var stateVar (bwfx::Rack& rack)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("desc",  juce::JSON::parse (juce::String (bwfx::descriptorJson())));
    obj->setProperty ("cdesc", juce::JSON::parse (juce::String (bwfx::characterJson())));
    obj->setProperty ("state", juce::JSON::parse (juce::String (rack.toJson())));
    obj->setProperty ("busLive", rack.isWorldModConsumed());
    obj->setProperty ("presets", juce::JSON::parse (juce::String (bwfx::presetsJson())));
    /*  The wiring (structure, from the blob) and the live values (host
        parameters, which the rail shows but does not own). */
    obj->setProperty ("macros", juce::JSON::parse (juce::String (rack.macroAssignJson())));
    juce::Array<juce::var> mv;
    for (int i = 0; i < bwfx::kMacros; ++i) mv.add (rack.getMacro (i));
    obj->setProperty ("macroVals", mv);
    obj->setProperty ("macroDbg", juce::JSON::parse (juce::String (rack.macroDebugJson())));
    return juce::var (obj);
}

/*  THE FIVE HOST PARAMETERS.

    This is the entire automatable surface of the rack, and the count is
    FROZEN — a parameter list that grows is the one thing the blob design
    exists to avoid. Ids and names are fixed forever for the same reason:
    hosts bind automation to the id and cache the name.

    A synth needs two lines: addMacroParameters() in its layout, and
    pushMacros() once per processBlock. */
inline const char* macroParamId (int i)
{
    static const char* ids[bwfx::kMacros] =
        { "bwfx_macro1", "bwfx_macro2", "bwfx_macro3", "bwfx_macro4", "bwfx_macro5" };
    return ids[i < 0 ? 0 : (i >= bwfx::kMacros ? bwfx::kMacros - 1 : i)];
}

inline const char* macroParamName (int i)
{
    static const char* names[bwfx::kMacros] =
        { "BWFX MACRO 1", "BWFX MACRO 2", "BWFX MACRO 3", "BWFX MACRO 4", "BWFX MACRO 5" };
    return names[i < 0 ? 0 : (i >= bwfx::kMacros ? bwfx::kMacros - 1 : i)];
}

/*  Every macro defaults to 0, which is the contract: zero means the patch
    exactly as saved. A fresh instance therefore sounds as it always did, and
    a host that never touches these lanes cannot change the sound. */
template <typename Layout>
inline void addMacroParameters (Layout& layout)
{
    for (int i = 0; i < bwfx::kMacros; ++i)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { macroParamId (i), 1 }, macroParamName (i),
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return juce::String ((int) std::lround (v * 100.0f)) + " %"; })));
}

/*  Call once per processBlock, next to setBpm. Cheap: five atomic loads and
    five stores, and nothing downstream happens unless something is assigned. */
inline void pushMacros (bwfx::Rack& rack, juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < bwfx::kMacros; ++i)
        if (auto* p = apvts.getRawParameterValue (macroParamId (i)))
            rack.setMacro (i, p->load());
}

/*  THE MESSAGE ENTRY POINT EVERY SYNTH SHOULD USE.

    A macro's value lives in a HOST PARAMETER, so the rail has to move the
    parameter — not the rack. Storing it in the rack looks like it works and
    then loses the race with the next audio block, because pushMacros writes
    the parameter over it every time. One owner per value, and here the owner
    is the host.

    The change is wrapped in a GESTURE, and that part is not optional: a bare
    setValueNotifyingHost tells the host the value moved, but Ableton's
    Configure listens for begin/endChangeGesture to decide that the user
    TOUCHED a control — without them the macro never appears in the wrapper
    alongside the plugin's other parameters. It is also what lets a host
    record the move as automation. */
inline bool handleMessage (bwfx::Rack& rack,
                           juce::AudioProcessorValueTreeState& apvts,
                           const juce::var& m)
{
    if (m.getProperty ("op", juce::var()).toString() == "macro")
    {
        const int i = (int) m.getProperty ("i", 0);
        if (i >= 0 && i < bwfx::kMacros)
            if (auto* p = apvts.getParameter (macroParamId (i)))
            {
                //  the range is 0..1, so the normalised value IS the value
                const float v = juce::jlimit (0.0f, 1.0f, (float) (double) m.getProperty ("v", 0.0));
                p->beginChangeGesture();
                p->setValueNotifyingHost (v);
                p->endChangeGesture();
            }
        return false;                 // no state echo: the rail already shows it
    }
    return handleMessage (rack, m);
}

} // namespace bwfx_juce
