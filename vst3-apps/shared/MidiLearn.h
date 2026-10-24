#pragma once

#include <JuceHeader.h>

/*  Host-independent MIDI CC -> parameter map with MIDI learn.

    Hosts such as Ableton do not always route a controller to a plugin whose
    panel is a web view, so the plugin carries its own map: right-click a knob
    to arm it, the next CC binds. onCc() runs on the audio thread; everything
    else on the message thread. One CC per parameter; the CCs the synths
    already use (bank, mod wheel, sustain, RPN/NRPN, channel mode) are never
    bound. Copy of this header lives in vst3-apps/shared; each plugin includes
    it by relative path. */
struct MidiLearn
{
    std::array<std::atomic<int>, 128> map;
    std::atomic<int>  learnIdx { -1 };
    std::atomic<bool> dirty    { true };

    MidiLearn() { clear(); }

    static bool reserved (int cc)
    {
        return cc == 0 || cc == 1 || cc == 6 || cc == 32 || cc == 38 || cc == 64 || (cc >= 98 && cc <= 101) || cc >= 120;
    }

    void clear()            { for (auto& m : map) m = -1; dirty = true; }
    void unbind (int idx)   { for (auto& m : map) if (m.load() == idx) m = -1; dirty = true; }
    void bind (int cc, int idx)
    {
        if (cc < 0 || cc > 127 || idx < 0) return;
        unbind (idx);
        map[(size_t) cc] = idx;
    }

    /*  Audio thread. Completes an armed learn, then returns the parameter
        index bound to this CC, or -1. */
    int onCc (int cc, int numParams)
    {
        if (cc < 0 || cc > 127 || reserved (cc)) return -1;
        const int armed = learnIdx.exchange (-1);
        if (armed >= 0 && armed < numParams) { bind (cc, armed); }
        const int pi = map[(size_t) cc].load();
        return (pi >= 0 && pi < numParams) ? pi : -1;
    }

    juce::String toString (const juce::StringArray& ids) const
    {
        juce::StringArray parts;
        for (int cc = 0; cc < 128; ++cc)
        {
            const int pi = map[(size_t) cc].load();
            if (pi >= 0 && pi < ids.size()) parts.add (juce::String (cc) + "=" + ids[pi]);
        }
        return parts.joinIntoString (",");
    }

    void fromString (const juce::String& s, const juce::StringArray& ids)
    {
        clear();
        for (const auto& part : juce::StringArray::fromTokens (s, ",", ""))
        {
            const int cc = part.upToFirstOccurrenceOf ("=", false, false).getIntValue();
            const int pi = ids.indexOf (part.fromFirstOccurrenceOf ("=", false, false));
            if (cc >= 0 && cc < 128 && pi >= 0 && ! reserved (cc)) map[(size_t) cc] = pi;
        }
    }

    /*  Shipped defaults: bind a General-MIDI controller to a parameter id, if
        the plugin has it. */
    void setDefault (int cc, const juce::String& id, const juce::StringArray& ids)
    {
        const int pi = ids.indexOf (id);
        if (pi >= 0 && ! reserved (cc)) { map[(size_t) cc] = pi; dirty = true; }
    }

    /*  Message thread: the page's "learn" / "unlearn" messages. True if handled. */
    bool handleMessage (const juce::String& k, const juce::DynamicObject& o, const juce::StringArray& ids)
    {
        if (k == "learn")
        {
            const int idx = ids.indexOf (o.getProperty ("id").toString());
            learnIdx = (idx >= 0 && (bool) o.getProperty ("on")) ? idx : -1;
            dirty = true;
            return true;
        }
        if (k == "unlearn")
        {
            const int idx = ids.indexOf (o.getProperty ("id").toString());
            if (idx >= 0) unbind (idx);
            learnIdx = -1;
            return true;
        }
        return false;
    }

    /*  Message thread, from the UI timer: the "ccmap" event payload, or an
        invalid var when nothing changed. */
    juce::var takeUpdate (const juce::StringArray& ids)
    {
        if (! dirty.exchange (false)) return {};
        auto* m = new juce::DynamicObject();
        for (int cc = 0; cc < 128; ++cc)
        {
            const int pi = map[(size_t) cc].load();
            if (pi >= 0 && pi < ids.size()) m->setProperty (ids[pi], cc);
        }
        auto* o = new juce::DynamicObject();
        o->setProperty ("map", juce::var (m));
        const int li = learnIdx.load();
        o->setProperty ("learn", (li >= 0 && li < ids.size()) ? ids[li] : juce::String());
        return juce::var (o);
    }
};
