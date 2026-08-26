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
    return juce::var (obj);
}

} // namespace bwfx_juce
