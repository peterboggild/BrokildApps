// LEGION host harness. The bench proves the ENGINE; this proves the WRAPPER —
// the part a DAW touches and an offline DSP test never reaches: the parameter
// table, the buses, the latency report, state save/restore, the dry/wet mix,
// and BWFX actually being in the harmony path and not the dry one.
//
// It builds against the plugin's own shared-code target, so it runs exactly
// the code the VST3 runs. No audio device, no window, no DAW.

#include <JuceHeader.h>

#include "../src/PluginProcessor.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
    constexpr double kFs = 48000.0;
    constexpr int    kBlock = 256;

    void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float plain)
    {
        auto* p = s.getParameter (id);
        jassert (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    //  a vowel, the same shape the engine bench uses
    void makeVowel (juce::AudioBuffer<float>& b, int n, double f0)
    {
        struct R { double a1 = 0, a2 = 0, y1 = 0, y2 = 0;
                   void set (double f, double bw) { const double r = std::exp (-juce::MathConstants<double>::pi * bw / kFs);
                       a1 = 2 * r * std::cos (2 * juce::MathConstants<double>::pi * f / kFs); a2 = -r * r; }
                   double t (double x) { const double y = x + a1 * y1 + a2 * y2; y2 = y1; y1 = y; return y; } };
        R r1, r2, r3; r1.set (700, 90); r2.set (1220, 110); r3.set (2600, 170);
        b.setSize (2, n); b.clear();
        double ph = 0, peak = 0;
        std::vector<double> t ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            ph += f0 / kFs;
            double x = 0; if (ph >= 1.0) { ph -= 1.0; x = 1.0; }
            t[(size_t) i] = r3.t (r2.t (r1.t (x)));
            peak = std::max (peak, std::abs (t[(size_t) i]));
        }
        for (int i = 0; i < n; ++i)
        {
            const float v = (float) (t[(size_t) i] * 0.5 / peak);
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
    }

    void run (LegionProcessor& p, const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out)
    {
        const int n = in.getNumSamples();
        out.setSize (2, n); out.clear();
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> blk (2, kBlock);
        for (int i = 0; i < n; i += kBlock)
        {
            const int m = std::min (kBlock, n - i);
            blk.setSize (2, m, false, false, true);
            for (int ch = 0; ch < 2; ++ch) blk.copyFrom (ch, 0, in, ch, i, m);
            p.processBlock (blk, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, i, blk, ch, 0, m);
        }
    }

    float peakOf (const juce::AudioBuffer<float>& b, int from = 0)
    {
        float m = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = from; i < b.getNumSamples(); ++i)
                m = std::max (m, std::abs (b.getSample (ch, i)));
        return m;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("LEGION host harness\n\n");

    LegionProcessor proc;
    proc.setPlayConfigDetails (2, 2, kFs, kBlock);
    proc.prepareToPlay (kFs, kBlock);

    const int n = (int) (kFs * 1.5);
    juce::AudioBuffer<float> in, out;
    makeVowel (in, n, 120.0);

    // -- the parameter table is complete and uniquely named -----------------
    {
        juce::StringArray ids;
        for (auto* p : proc.getParameters())
            if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                ids.add (wp->paramID);
        const int before = ids.size();
        ids.removeDuplicates (false);
        CHECK (ids.size() == before, "a parameter id is declared twice");
        CHECK (before == 5 + 6 + legion::kVoices * (1 + kNumVoiceSpecs) + bwfx::kMacros,
               "the parameter count is %d, not what Params.h describes", before);
        std::printf ("  %d host parameters, all uniquely named\n", before);
    }

    // -- THE CONTRACT: MIX 0 is the input, delayed and nothing else ---------
    {
        setParam (proc.apvts, legion_ids::mix, 0.0f);
        setParam (proc.apvts, legion_ids::output, 0.0f);
        for (int v = 0; v < legion::kVoices; ++v)
            setParam (proc.apvts, legion_ids::voice (v, "on"), 0.0f);
        proc.prepareToPlay (kFs, kBlock);

        run (proc, in, out);
        const int lat = proc.getLatencySamples();
        float worst = 0;
        for (int i = lat; i < n; ++i)
            worst = std::max (worst, std::abs (out.getSample (0, i) - in.getSample (0, i - lat)));
        std::printf ("  latency %d samples (%.1f ms); MIX 0 error %.3g\n",
                     lat, 1000.0 * lat / kFs, worst);
        CHECK (worst < 1.0e-6f, "MIX 0 is not the input (%.3g)", worst);
    }

    // -- VOICES OFF IS A WIRE: at ANY mix, the input delayed and nothing else ---
    //  (before this, MIX 50 with every voice off turned the singer down 3 dB)
    for (float mixPct : { 50.0f, 100.0f })
    {
        setParam (proc.apvts, legion_ids::mix, mixPct);
        proc.prepareToPlay (kFs, kBlock);
        run (proc, in, out);
        const int lat = proc.getLatencySamples();
        float worst = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = lat + 2400; i < n; ++i)      //  past the 20 ms fade-up
                worst = std::max (worst, std::abs (out.getSample (ch, i) - in.getSample (ch, i - lat)));
        std::printf ("  voices off, MIX %3.0f: error %.3g\n", mixPct, worst);
        CHECK (worst == 0.0f, "voices off at MIX %.0f is not the input (%.3g)", mixPct, worst);
    }

    // -- the LEVELLER through the real wrapper, voices off -------------------
    //  a phrase alternating 24 dB apart; it must come out much closer, on
    //  both sides, identically (stereo-linked), with nothing non-finite
    {
        const int seg = (int) kFs, nn = seg * 6;
        juce::AudioBuffer<float> ph (2, nn), lo;
        for (int k = 0; k < 6; ++k)
        {
            const float s = (k % 2) ? 1.0f : 0.063f;          //  -24 dB
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < seg; ++i)
                    ph.setSample (ch, k * seg + i, in.getSample (ch, (k * seg + i) % n) * s);
        }
        setParam (proc.apvts, legion_ids::mix, 50.0f);
        setParam (proc.apvts, legion_ids::levOn, 1.0f);
        setParam (proc.apvts, legion_ids::levTop, -24.0f);
        setParam (proc.apvts, legion_ids::levLift, 12.0f);
        proc.prepareToPlay (kFs, kBlock);
        run (proc, ph, lo);
        const int lat = proc.getLatencySamples();
        auto segDb = [&] (const juce::AudioBuffer<float>& b, int k, int off)
        {
            double e = 0; const int a = k * seg + off + seg / 2, z = (k + 1) * seg + off;
            for (int i = a; i < z; ++i) e += (double) b.getSample (0, i) * b.getSample (0, i);
            return 10.0 * std::log10 (e / (z - a) + 1e-30);
        };
        const double inSpread  = segDb (ph, 5, 0) - segDb (ph, 4, 0);
        const double outSpread = segDb (lo, 5, lat) - segDb (lo, 4, lat);
        float lr = 0;
        for (int i = 0; i < nn; ++i) lr = std::max (lr, std::abs (lo.getSample (0, i) - lo.getSample (1, i)));
        std::printf ("  LEVELLER: %.1f dB apart in, %.1f dB apart out, L/R differ by %.3g\n",
                     inSpread, outSpread, lr);
        CHECK (outSpread < inSpread - 10.0, "the LEVELLER did not level in the plugin (%.1f -> %.1f)", inSpread, outSpread);
        CHECK (lr == 0.0f, "the LEVELLER moved the stereo image (%.3g)", lr);
        CHECK (peakOf (lo) < 1.0f && std::isfinite (lo.getSample (0, nn - 1)), "the LEVELLER went wild");

        setParam (proc.apvts, legion_ids::levOn, 0.0f);
        setParam (proc.apvts, legion_ids::levTop, -20.0f);
        setParam (proc.apvts, legion_ids::levLift, 6.0f);
        proc.prepareToPlay (kFs, kBlock);
    }

    // -- a voice on, MIX up, and something actually happens -----------------
    {
        setParam (proc.apvts, legion_ids::mix, 100.0f);
        setParam (proc.apvts, legion_ids::humanize, 0.0f);
        setParam (proc.apvts, legion_ids::voice (0, "on"), 1.0f);
        setParam (proc.apvts, legion_ids::voice (0, "pitch"), 7.0f);
        setParam (proc.apvts, legion_ids::voice (0, "level"), 0.0f);
        proc.prepareToPlay (kFs, kBlock);
        run (proc, in, out);
        const float pk = peakOf (out, (int) (kFs * 0.5));
        std::printf ("  one voice at +7 st, MIX 100: peak %.3f\n", pk);
        CHECK (pk > 0.05f && pk < 2.0f, "the harmony bus is silent or wild (%.3f)", pk);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                jassert (std::isfinite (out.getSample (ch, i)));
    }

    // -- every DETAIL switch renegotiates latency and still makes sound ------
    {
        for (int d = 0; d < legion::kDetails; ++d)
        {
            setParam (proc.apvts, legion_ids::detail, (float) d);
            /*  In a DAW the switch travels an asynchronous route: an APVTS
                listener sets a flag and the 15 Hz timer renegotiates latency
                with the host. Headless there is no message loop, so this
                takes the other route into the same code — prepareToPlay,
                which a host also calls on every sample-rate change. */
            proc.prepareToPlay (kFs, kBlock);
            run (proc, in, out);
            std::printf ("  detail %d: latency %d samples, peak %.3f\n",
                         d, proc.getLatencySamples(), peakOf (out, (int) (kFs * 0.5)));
            CHECK (proc.getLatencySamples() > 0, "detail %d reports no latency", d);
            CHECK (peakOf (out, (int) (kFs * 0.5)) > 0.02f, "detail %d is silent", d);
        }
        setParam (proc.apvts, legion_ids::detail, 1.0f);
        proc.prepareToPlay (kFs, kBlock);
    }

    // -- BWFX: on the HARMONY bus, and NOT on the dry one --------------------
    {
        //  find a module that unmistakably changes a signal, and prove the
        //  rack reaches the harmony at MIX 100 but not the singer at MIX 0
        int drive = -1;
        for (int t = 0; t < bwfx::numModuleTypes(); ++t)
            if (juce::String (bwfx::moduleDescriptor (t).id) == "tube") drive = t;
        if (drive < 0) drive = 0;
        std::printf ("  BWFX module under test: %s\n", bwfx::moduleDescriptor (drive).name);

        setParam (proc.apvts, legion_ids::mix, 0.0f);
        proc.rack().clearState();
        proc.prepareToPlay (kFs, kBlock);
        juce::AudioBuffer<float> dryClean; run (proc, in, dryClean);

        proc.rack().setEnabled (drive, true);
        proc.rack().setMix (1.0f);
        for (int p = 0; p < bwfx::moduleDescriptor (drive).numParams; ++p)
            proc.rack().setParam (drive, p, bwfx::moduleDescriptor (drive).params[p].hi);
        proc.prepareToPlay (kFs, kBlock);
        juce::AudioBuffer<float> dryRacked; run (proc, in, dryRacked);

        float dd = 0;
        for (int i = (int) (kFs * 0.5); i < n; ++i)
            dd = std::max (dd, std::abs (dryClean.getSample (0, i) - dryRacked.getSample (0, i)));
        CHECK (dd < 1.0e-6f, "BWFX on HARMONY reached the dry bus (%.3g)", dd);

        setParam (proc.apvts, legion_ids::mix, 100.0f);
        proc.prepareToPlay (kFs, kBlock);
        juce::AudioBuffer<float> wetRacked; run (proc, in, wetRacked);
        proc.rack().setEnabled (drive, false);
        proc.prepareToPlay (kFs, kBlock);
        juce::AudioBuffer<float> wetClean; run (proc, in, wetClean);

        float wd = 0;
        for (int i = (int) (kFs * 0.5); i < n; ++i)
            wd = std::max (wd, std::abs (wetClean.getSample (0, i) - wetRacked.getSample (0, i)));
        std::printf ("  rack on HARMONY: dry changed by %.3g, harmony changed by %.3g\n", dd, wd);
        CHECK (wd > 1.0e-4f, "BWFX on HARMONY did not reach the harmony (%.3g)", wd);

        //  and MASTER puts it back on everything
        setParam (proc.apvts, legion_ids::mix, 0.0f);
        setParam (proc.apvts, legion_ids::rackPos, 1.0f);
        proc.rack().setEnabled (drive, true);
        proc.prepareToPlay (kFs, kBlock);
        juce::AudioBuffer<float> masterRacked; run (proc, in, masterRacked);
        float md = 0;
        for (int i = (int) (kFs * 0.5); i < n; ++i)
            md = std::max (md, std::abs (dryClean.getSample (0, i) - masterRacked.getSample (0, i)));
        CHECK (md > 1.0e-4f, "BWFX on MASTER did not reach the output (%.3g)", md);
        std::printf ("  rack on MASTER: dry changed by %.3g\n", md);
    }

    // -- state: parameters AND the rack blob survive a round trip ------------
    {
        setParam (proc.apvts, legion_ids::mix, 42.0f);
        setParam (proc.apvts, legion_ids::voice (2, "form"), -5.5f);
        setParam (proc.apvts, legion_ids::voice (2, "on"), 1.0f);
        const std::string blob = proc.rack().toJson();

        juce::MemoryBlock mb;
        proc.getStateInformation (mb);

        LegionProcessor fresh;
        fresh.setPlayConfigDetails (2, 2, kFs, kBlock);
        fresh.prepareToPlay (kFs, kBlock);
        fresh.setStateInformation (mb.getData(), (int) mb.getSize());

        CHECK (std::abs (fresh.apvts.getRawParameterValue (legion_ids::mix)->load() - 42.0f) < 0.01f,
               "MIX did not survive the state round trip");
        CHECK (std::abs (fresh.apvts.getRawParameterValue (legion_ids::voice (2, "form"))->load() + 5.5f) < 0.01f,
               "a voice's FORMANT did not survive the state round trip");
        CHECK (fresh.rack().toJson() == blob, "the BWFX blob did not survive the state round trip");
        CHECK (fresh.rack().getEnabled (0) == proc.rack().getEnabled (0),
               "a BWFX power switch did not survive the state round trip");
        std::printf ("  state round trip: %d bytes, rack blob %d chars\n",
                     (int) mb.getSize(), (int) blob.size());
    }

    // -- buses ---------------------------------------------------------------
    {
        LegionProcessor b;
        using Set = juce::AudioChannelSet;
        CHECK (b.checkBusesLayoutSupported ({ { Set::mono() },   { Set::stereo() } }), "mono in / stereo out refused");
        CHECK (b.checkBusesLayoutSupported ({ { Set::stereo() }, { Set::stereo() } }), "stereo in / stereo out refused");
        CHECK (b.checkBusesLayoutSupported ({ { Set::mono() },   { Set::mono() } }),   "mono in / mono out refused");
        CHECK (! b.checkBusesLayoutSupported ({ { Set::stereo() }, { Set::mono() } }), "stereo in / mono out accepted");

        //  and mono in / stereo out actually runs
        b.setPlayConfigDetails (1, 2, kFs, kBlock);
        b.prepareToPlay (kFs, kBlock);
        setParam (b.apvts, legion_ids::mix, 100.0f);
        setParam (b.apvts, legion_ids::voice (0, "on"), 1.0f);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> mb (2, kBlock);
        for (int i = 0; i < 200; ++i)
        {
            for (int j = 0; j < kBlock; ++j) mb.setSample (0, j, in.getSample (0, (i * kBlock + j) % n));
            b.processBlock (mb, midi);
        }
        CHECK (std::isfinite (mb.getSample (0, 0)) && std::isfinite (mb.getSample (1, 0)),
               "mono in / stereo out produced a non-finite sample");
        std::printf ("  buses: mono->stereo, stereo->stereo, mono->mono\n");
    }

    std::printf ("\n%d checks", checks);
    if (failures == 0) std::printf (" — ALL CLEAR\n");
    else               std::printf (" — %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
