/*  THE SITE, tested THROUGH THE VST3 WRAPPER.

    The standalone check and the in-process state-machine check both pass, so
    if the climate still fails in a DAW the fault can only be in the one link
    neither of them covers: the plug-in wrapper inside a host. This is that
    link — a real host that scans the installed bundles, instantiates two
    findings in ONE process (as a DAW does), pumps the message thread so their
    timers run, then moves one finding's temperature parameter and reads the
    other's.

    It reports what a DAW would see, in the DAW's own terms: parameter names,
    parameter values, and whether the second plug-in followed the first.

      ab104host                 use the installed bundles
      ab104host <a.vst3> <b.vst3>
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace
{
constexpr double kRate = 48000.0;
constexpr int    kBlock = 512;

int checks = 0, fails = 0;
void ok (bool c, const juce::String& what, const juce::String& detail = {})
{
    ++checks;
    if (! c) { ++fails; std::printf ("  FAIL  %s   %s\n", what.toRawUTF8(), detail.toRawUTF8()); }
}

//  pump the message thread (the plug-ins' site timers live there) while
//  pretending to be an audio thread, because a DAW does both
void spin (juce::AudioPluginInstance* a, juce::AudioPluginInstance* b, double seconds)
{
    juce::AudioBuffer<float> buf (2, kBlock);
    juce::MidiBuffer midi;
    const int blocks = (int) (seconds * kRate / kBlock);
    for (int i = 0; i < blocks; ++i)
    {
        for (auto* p : { a, b })
            if (p != nullptr) { buf.clear(); p->processBlock (buf, midi); }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (
            (int) (1000.0 * kBlock / kRate));
    }
}

juce::AudioProcessorParameter* paramNamed (juce::AudioPluginInstance& p, const juce::String& want)
{
    for (auto* q : p.getParameters())
        if (q->getName (64).equalsIgnoreCase (want)) return q;
    return nullptr;
}

//  every finding names its temperature in kelvin in its own text, so the
//  reading is taken the way a user reads it: off the parameter's own label
double kelvinOf (juce::AudioProcessorParameter* p)
{
    if (p == nullptr) return 0.0;
    return p->getText (p->getValue(), 32).retainCharacters ("0123456789.").getDoubleValue();
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File house ("C:\\Program Files\\Common Files\\VST3\\Brokild\\Proxima Centauri B findings");
    juce::String pathA = argc > 2 ? argv[1] : house.getChildFile ("Artefact B2311.104.vst3").getFullPathName();
    juce::String pathB = argc > 2 ? argv[2] : house.getChildFile ("Artefact B2311.22.vst3").getFullPathName();

    //  JUCE 8 retired AudioPluginFormatManager::addDefaultFormats() in favour
    //  of this helper; the old call is a deleted function, not a missing flag
    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);

    auto load = [&] (const juce::String& path) -> std::unique_ptr<juce::AudioPluginInstance>
    {
        juce::OwnedArray<juce::PluginDescription> found;
        juce::KnownPluginList list;
        for (auto* f : fm.getFormats())
            if (f->getName().contains ("VST3"))
                list.scanAndAddFile (path, true, found, *f);
        if (found.isEmpty()) { std::printf ("  could not scan %s\n", path.toRawUTF8()); return {}; }
        juce::String err;
        auto inst = fm.createPluginInstance (*found[0], kRate, kBlock, err);
        if (inst == nullptr) std::printf ("  could not instantiate: %s\n", err.toRawUTF8());
        return inst;
    };

    /*  THE MATERIAL (2026-10-12), through the wrapper: the CHARACTER parameter
        is what a DAW sees, and a played note must actually change with it —
        a parameter that exists and does nothing would pass every other check. */
    {
        std::printf ("=== THE MATERIAL, through the VST3 wrapper ===\n");
        auto m = load (pathA);
        if (m == nullptr) { std::printf ("no plug-in, nothing proved\n"); return 1; }
        m->prepareToPlay (kRate, kBlock);
        auto* chp = paramNamed (*m, "CHARACTER");
        ok (chp != nullptr, "the host sees a CHARACTER parameter");
        ok (chp != nullptr && chp->isAutomatable(), "CHARACTER is automatable");
        auto take = [&] (float v) -> std::vector<float>
        {
            if (chp != nullptr) { chp->beginChangeGesture(); chp->setValueNotifyingHost (v); chp->endChangeGesture(); }
            juce::AudioBuffer<float> buf (2, kBlock);
            std::vector<float> out;
            for (int i = 0; i < (int) (2.0 * kRate / kBlock); ++i)     //  silence first: let any old tail go
            { juce::MidiBuffer none; buf.clear(); m->processBlock (buf, none); }
            for (int i = 0; i < (int) (1.6 * kRate / kBlock); ++i)
            {
                juce::MidiBuffer midi;
                if (i == 0)   midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 100), 0);
                if (i == (int) (1.4 * kRate / kBlock)) midi.addEvent (juce::MidiMessage::noteOff (1, 40), 0);
                buf.clear(); m->processBlock (buf, midi);
                for (int k = 0; k < kBlock; ++k) out.push_back (buf.getSample (0, k));
            }
            return out;
        };
        const auto oldT = take (0.0f), newT = take (0.8f);
        double eo = 0, en = 0, ed = 0;
        const size_t a0 = (size_t) (0.6 * kRate), a1 = (size_t) (1.3 * kRate);
        for (size_t i = a0; i < a1 && i < oldT.size() && i < newT.size(); ++i)
        { eo += oldT[i] * oldT[i]; en += newT[i] * newT[i]; }
        //  the difference in SHAPE, level taken out: each take scaled to unit rms
        const double so = std::sqrt (eo / (double) (a1 - a0)), sn = std::sqrt (en / (double) (a1 - a0));
        //  brightness with the level taken out: the energy of the sample-to-
        //  sample difference against the signal's own energy (a sine at f
        //  scores (2 sin(pi f/fs))^2; every overtone raises it). A level
        //  change cannot move it.
        double dO = 0, dN = 0;
        for (size_t i = a0 + 1; i < a1; ++i)
        { const double xo = oldT[i] - oldT[i - 1], xn = newT[i] - newT[i - 1]; dO += xo * xo; dN += xn * xn; }
        const double bO = dO / std::max (1e-20, eo), bN = dN / std::max (1e-20, en);
        //  ...and the STRIKE, the first 300 ms: specimen 0 is a BELL, whose
        //  metal rings at the onset and whose sustain is meant to be mellow
        double sO = 0, sN = 0, qO = 0, qN = 0;
        for (size_t i = 1; i < (size_t) (0.3 * kRate); ++i)
        {
            const double xo = oldT[i] - oldT[i - 1], xn = newT[i] - newT[i - 1];
            sO += xo * xo; sN += xn * xn; qO += oldT[i] * oldT[i]; qN += newT[i] * newT[i];
        }
        const double kO = sO / std::max (1e-20, qO), kN = sN / std::max (1e-20, qN);
        juce::ignoreUnused (ed);
        std::printf ("  E2 held: CHARACTER 0 rms %.4f brightness %.2e;  CHARACTER 0.8 rms %.4f brightness %.2e (x%.1f)\n",
                     so, bO, sn, bN, bN / std::max (1e-20, bO));
        std::printf ("  E2 strike: brightness %.2e vs %.2e (x%.1f)\n", kO, kN, kN / std::max (1e-20, kO));
        //  the harmonic PROFILE of the held tone, as the bench's ab104timbre
        //  reads it: harmonics 2..10 against the fundamental, floored at -50 dB
        auto prof = [&] (const std::vector<float>& x, int k) -> double
        {
            const double f0 = 440.0 * std::pow (2.0, (40 - 69) / 12.0);
            auto gz = [&] (double f) { const double w = 2 * juce::MathConstants<double>::pi * f / kRate, c = 2 * std::cos (w);
                double s1 = 0, s2 = 0; const size_t n = a1 - a0;
                for (size_t i = 0; i < n; ++i) { const double h = 0.5 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * (double) i / (double) (n - 1));
                    const double s0 = x[a0 + i] * h + c * s1 - s2; s2 = s1; s1 = s0; }
                return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)); };
            return std::max (-50.0, 20.0 * std::log10 (gz (f0 * k) / std::max (1e-15, gz (f0))));
        };
        double pd = 0;
        std::printf ("  harmonics 2..10, CHARACTER 0 -> 0.8 (dB):");
        for (int k = 2; k <= 10; ++k) { const double po = prof (oldT, k), pn = prof (newT, k); pd += (pn - po) * (pn - po); std::printf (" %.0f/%.0f", po, pn); }
        pd = std::sqrt (pd / 9.0);
        std::printf ("  -> %.1f dB rms\n", pd);
        ok (so > 1e-3 && sn > 1e-3, "a played note sounds at both settings");
        ok (pd > 4.0, "CHARACTER changes the timbre of a played note (its harmonic profile, not its level)",
            juce::String (pd, 1) + " dB rms profile change");
        m->releaseResources();
    }

    std::printf ("=== THE SITE, through the VST3 wrapper (two findings, one process) ===\n");
    auto a = load (pathA);
    auto b = load (pathB);
    if (a == nullptr || b == nullptr) { std::printf ("no plug-ins, nothing proved\n"); return 1; }
    std::printf ("  loaded %s and %s\n", a->getName().toRawUTF8(), b->getName().toRawUTF8());

    a->prepareToPlay (kRate, kBlock);
    b->prepareToPlay (kRate, kBlock);

    auto* ta = paramNamed (*a, "AMBIENT");
    auto* tb = paramNamed (*b, "TEMPERATURE");
    if (ta == nullptr) ta = paramNamed (*a, "TEMPERATURE");
    if (tb == nullptr) tb = paramNamed (*b, "AMBIENT");
    ok (ta != nullptr && tb != nullptr, "both findings expose a temperature parameter");
    if (ta == nullptr || tb == nullptr) return 1;

    //  let them find each other and settle
    spin (a.get(), b.get(), 2.0);
    std::printf ("  settled: %s %.0f K, %s %.0f K\n",
                 a->getName().toRawUTF8(), kelvinOf (ta), b->getName().toRawUTF8(), kelvinOf (tb));

    /*  The bench must be shared FROM INSIDE the plug-ins, so this host does not
        touch the settings file — it drives the panels' own message, exactly as
        a click on the SITE panel does. Both findings accept {k:"site"}. */
    if (auto* ed = a->createEditorIfNeeded()) { juce::ignoreUnused (ed); }
    spin (a.get(), b.get(), 0.5);

    const double aBefore = kelvinOf (ta), bBefore = kelvinOf (tb);

    //  move the first finding's temperature, as dragging its slider does
    ta->beginChangeGesture();
    ta->setValueNotifyingHost (0.80f);
    ta->endChangeGesture();
    spin (a.get(), b.get(), 2.5);

    const double aAfter = kelvinOf (ta), bAfter = kelvinOf (tb);
    std::printf ("  moved %s: %.0f -> %.0f K;  %s: %.0f -> %.0f K\n",
                 a->getName().toRawUTF8(), aBefore, aAfter,
                 b->getName().toRawUTF8(), bBefore, bAfter);

    ok (std::abs (aAfter - aBefore) > 5.0, "the moved finding's own temperature changed",
        juce::String (aBefore) + " -> " + juce::String (aAfter));

    /*  Whether the SECOND finding follows depends on the bench's CLIMATE
        setting, which is the user's and which this host deliberately does not
        change. So the check is conditional and says which case it saw — a test
        that silently passes either way would prove nothing. */
    const juce::File cfg (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                              .getChildFile ("Brokild").getChildFile ("ProximaSite.json"));
    const juce::String txt = cfg.existsAsFile() ? cfg.loadFileAsString() : juce::String();
    const bool climateOn = txt.contains ("\"climate\": 1");
    std::printf ("  the bench's climate setting is %s\n", climateOn ? "SHARED" : "own (not shared)");
    if (climateOn)
        ok (std::abs (bAfter - aAfter) < 3.0, "with CLIMATE shared, the second finding followed",
            juce::String (bAfter) + " vs " + juce::String (aAfter));
    else
        ok (std::abs (bAfter - bBefore) < 1.0, "with CLIMATE off, the second finding did not move",
            juce::String (bBefore) + " -> " + juce::String (bAfter));

    a->releaseResources(); b->releaseResources();
    a.reset(); b.reset();

    std::printf ("\n%d checks, %d failed  -  %s\n", checks, fails, fails ? "SEE ABOVE" : "ALL CLEAR");
    return fails ? 1 : 0;
}
