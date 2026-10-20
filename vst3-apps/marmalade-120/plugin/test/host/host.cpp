/*  What does a DAW actually get?

    Loads the VST3 bundle, prints its parameters in a host's own terms, then
    renders a plucked low E and a chord's worth of high notes through it at
    48 kHz and 44.1 kHz, both QUALITY settings, and reports level, latency and
    whether anything non-finite came out. Exits 1 on anything wrong.

      mmhost <path.vst3>
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf ("usage: mmhost <path.vst3>\n"); return 2; }
    const juce::String path (argv[1]);

    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);
    juce::KnownPluginList list;
    juce::OwnedArray<juce::PluginDescription> found;
    for (int i = 0; i < fm.getNumFormats(); ++i)
        if (auto* f = fm.getFormat (i))
            if (f->getName().containsIgnoreCase ("VST3"))
                list.scanAndAddFile (path, true, found, *f);
    if (found.isEmpty()) { std::printf ("FAIL: no plugin found in %s\n", path.toRawUTF8()); return 1; }

    std::printf ("found: %s by %s, %s\n", found[0]->name.toRawUTF8(),
                 found[0]->manufacturerName.toRawUTF8(), found[0]->category.toRawUTF8());

    int bad = 0;
    for (double sr : { 48000.0, 44100.0 })
        for (int quality : { 0, 1 })
        {
            juce::String err;
            auto inst = fm.createPluginInstance (*found[0], sr, 256, err);
            if (inst == nullptr) { std::printf ("FAIL: instantiate: %s\n", err.toRawUTF8()); return 1; }

            auto& params = inst->getParameters();
            if (sr == 48000.0 && quality == 0)
            {
                std::printf ("%d parameters:\n", params.size());
                for (auto* p : params)
                    std::printf ("  %-10s automatable=%d  %s\n", p->getName (32).toRawUTF8(),
                                 (int) p->isAutomatable(), p->getCurrentValueAsText().toRawUTF8());
            }
            for (auto* p : params)
                if (p->getName (32) == "QUALITY") p->setValueNotifyingHost ((float) quality);

            inst->setPlayConfigDetails (2, 2, sr, 256);
            inst->prepareToPlay (sr, 256);

            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            double peak = 0.0, sum2 = 0.0;
            long n = 0;
            bool finite = true;
            const int blocks = (int) (2.0 * sr / 256);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const double t = (b * 256 + i) / sr;
                    const double tt = std::fmod (t, 1.0);
                    const float x = (float) (0.5 * std::sin (2.0 * M_PI * 82.41 * t) * std::exp (-tt * 3.0)
                                           + 0.2 * std::sin (2.0 * M_PI * 659.3 * t) * std::exp (-tt * 6.0)
                                           + 0.2 * std::sin (2.0 * M_PI * 987.8 * t) * std::exp (-tt * 6.0));
                    buf.setSample (0, i, x);
                    buf.setSample (1, i, x);
                }
                inst->processBlock (buf, midi);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 256; ++i)
                    {
                        const float s = buf.getSample (c, i);
                        if (! std::isfinite (s)) finite = false;
                        peak = std::max (peak, (double) std::abs (s));
                        if (b > blocks / 4) { sum2 += (double) s * s; ++n; }
                    }
            }
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            const double rms = std::sqrt (sum2 / std::max (1L, n));
            const bool ok = finite && peak > 0.01 && peak < 4.0;
            if (! ok) ++bad;
            std::printf ("%s  %.1f kHz %-6s latency %3d  peak %6.1f dBFS  rms %6.1f dBFS  %.0f%% of real time\n",
                         ok ? " ok " : "FAIL", sr / 1000.0, quality ? "HIGH" : "NORMAL", inst->getLatencySamples(),
                         20.0 * std::log10 (std::max (peak, 1e-9)), 20.0 * std::log10 (std::max (rms, 1e-9)),
                         100.0 * ms / 2000.0);
            inst->releaseResources();
        }
    std::printf (bad == 0 ? "mmhost: all renders good\n" : "mmhost: FAILED\n");
    return bad == 0 ? 0 : 1;
}
