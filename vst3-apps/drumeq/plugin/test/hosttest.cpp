// DRUMEQ host harness. The bench proves the engine; this proves the WRAPPER and
// THE KIT: the parameter table, the buses, the latency, state, presets on disk,
// and several instances sharing one kit exactly as tracks in a DAW do (they run
// in one process here, as they do in the DAW, and meet in the same shared block).
// No DAW, no audio device, no window.

#include <JuceHeader.h>

#include "../src/PluginProcessor.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("  FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
    constexpr double kFs = 48000.0;
    constexpr int kBlock = 256;

    void prep (DrumEQProcessor& p, int ch)
    {
        juce::AudioProcessor::BusesLayout l;
        const auto set = ch == 2 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono();
        l.inputBuses.add (set); l.outputBuses.add (set);
        p.setBusesLayout (l);
        p.setPlayConfigDetails (ch, ch, kFs, kBlock);
        p.prepareToPlay (kFs, kBlock);
    }

    juce::AudioBuffer<float> run (DrumEQProcessor& p, const juce::AudioBuffer<float>& in)
    {
        juce::AudioBuffer<float> all (in), blk (in.getNumChannels(), kBlock);
        juce::MidiBuffer midi;
        for (int o = 0; o + kBlock <= in.getNumSamples(); o += kBlock)
        {
            for (int c = 0; c < in.getNumChannels(); ++c) blk.copyFrom (c, 0, in, c, o, kBlock);
            p.processBlock (blk, midi);
            for (int c = 0; c < in.getNumChannels(); ++c) all.copyFrom (c, o, blk, c, 0, kBlock);
        }
        return all;
    }

    bool same (const de::Params& a, const de::Params& b, float tol = 1e-3f, const char** which = nullptr)
    {
        for (int i = 0; i < de::kNumParams; ++i)
        {
            const auto& s = de::specs()[i];
            const float x = a.*(s.member), y = b.*(s.member);
            if (std::abs (x - y) > tol * (1.0f + std::abs (x))) { if (which) *which = s.id; return false; }
        }
        return true;
    }

    void tickAll (std::initializer_list<DrumEQProcessor*> ps) { for (auto* p : ps) p->kitTick(); }
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("DRUMEQ host harness\n\n");

    // -- the parameter table, the buses, the latency -------------------------
    {
        DrumEQProcessor p;
        juce::StringArray ids;
        for (auto* prm : p.getParameters())
            if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (prm)) ids.add (wp->paramID);
        const int before = ids.size();
        ids.removeDuplicates (false);
        CHECK (ids.size() == before && before == de::kNumParams, "%d host parameters (%d unique) for a table of %d", before, ids.size(), de::kNumParams);
        std::printf ("  %d host parameters, uniquely named\n", before);

        juce::AudioProcessor::BusesLayout mono, stereo, mixed;
        mono.inputBuses.add (juce::AudioChannelSet::mono());   mono.outputBuses.add (juce::AudioChannelSet::mono());
        stereo.inputBuses.add (juce::AudioChannelSet::stereo()); stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        mixed.inputBuses.add (juce::AudioChannelSet::mono());  mixed.outputBuses.add (juce::AudioChannelSet::stereo());
        CHECK (p.checkBusesLayoutSupported (mono) && p.checkBusesLayoutSupported (stereo), "mono and stereo tracks");
        CHECK (! p.checkBusesLayoutSupported (mixed), "mono in, stereo out should be refused");
        prep (p, 2);
        CHECK (p.getLatencySamples() == 0, "latency %d", p.getLatencySamples());
        std::printf ("  mono and stereo tracks, zero latency\n");

        //  neutral through the real wrapper is the input, bit for bit
        de::Params n; n.source = 0;
        p.applyParams (n);
        juce::AudioBuffer<float> x (2, 48 * kBlock);
        juce::Random r (3);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < x.getNumSamples(); ++i) x.setSample (c, i, r.nextFloat() * 0.6f - 0.3f);
        const auto y = run (p, x);
        bool exact = true;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < x.getNumSamples(); ++i) if (x.getSample (c, i) != y.getSample (c, i)) exact = false;
        CHECK (exact, "neutral through the wrapper is not bit-exact");
        std::printf ("  neutral through the wrapper: the input, bit for bit\n");
    }

    // -- state -------------------------------------------------------------------
    {
        DrumEQProcessor a; prep (a, 1);
        a.selectSource (de::S_TOM2);
        a.loadFactoryPreset (2);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        DrumEQProcessor b; prep (b, 1);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        b.kitTick();
        const char* w = nullptr;
        CHECK (same (a.currentParams(), b.currentParams(), 1e-4f, &w), "state does not round-trip (%s)", w ? w : "?");
        CHECK (b.currentSource() == de::S_TOM2 && b.currentName() == "METAL ATTACK", "restored as %d / %s", b.currentSource(), b.currentName().toRawUTF8());
        std::printf ("  state: TOM 2 / METAL ATTACK saved and restored, and a restore is not taken for a change of drum\n");
    }

    // -- a new drum on a track loads that drum -----------------------------------
    {
        DrumEQProcessor p; prep (p, 1);
        p.setLinked (false);
        p.selectSource (de::S_HAT);
        const char* w = nullptr;
        CHECK (same (p.currentParams(), de::presetParams (de::S_HAT, 0), 1e-3f, &w), "HI-HAT did not load its STANDARD (%s)", w ? w : "?");
        std::printf ("  choosing HI-HAT on a track loads the hi-hat's STANDARD\n");
    }

    // -- THE KIT ---------------------------------------------------------------------
    {
        DrumEQProcessor kin, snr, tom; prep (kin, 1); prep (snr, 1); prep (tom, 1);
        CHECK (kin.kitOpen(), "the shared kit block did not open");
        kin.selectSource (de::S_KICK_IN);
        snr.selectSource (de::S_SNARE_TOP);
        tom.selectSource (de::S_TOM1);
        tickAll ({ &kin, &snr, &tom });

        int counts[de::kNumSources];
        kin.kitPresence (counts);
        CHECK (counts[de::S_KICK_IN] >= 1 && counts[de::S_SNARE_TOP] >= 1 && counts[de::S_TOM1] >= 1, "the session does not see the three tracks");
        std::printf ("  three tracks on KICK IN, SNARE TOP and TOM 1 see each other\n");

        //  a factory kit loaded on the kick track reaches the other two
        juce::String rep;
        kin.loadFactoryKit (2, rep);                  // MODERN METAL
        tickAll ({ &snr, &tom });
        const char* w = nullptr;
        CHECK (same (kin.currentParams(), de::presetParams (de::S_KICK_IN, 2), 1e-3f, &w), "the kick did not take MODERN METAL (%s)", w ? w : "?");
        CHECK (same (snr.currentParams(), de::presetParams (de::S_SNARE_TOP, 2), 1e-3f, &w), "the snare did not take MODERN METAL (%s)", w ? w : "?");
        CHECK (same (tom.currentParams(), de::presetParams (de::S_TOM1, 2), 1e-3f, &w), "the tom did not take MODERN METAL (%s)", w ? w : "?");
        CHECK (snr.currentName() == "METAL CRACK" && snr.kitName() == "MODERN METAL", "snare says %s in %s", snr.currentName().toRawUTF8(), snr.kitName().toRawUTF8());
        std::printf ("  MODERN METAL loaded on the kick track: the snare is METAL CRACK, the tom METAL ATTACK\n");

        //  the drummer's kit: tweak the snare and the tom, save the whole kit from the KICK track
        if (auto* prm = snr.apvts.getParameter ("b3g")) prm->setValueNotifyingHost (prm->convertTo0to1 (9.0f));
        if (auto* prm = tom.apvts.getParameter ("b1f")) prm->setValueNotifyingHost (prm->convertTo0to1 (163.0f));
        tickAll ({ &snr, &tom });
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("drumeq-hosttest");
        dir.deleteRecursively(); dir.createDirectory();
        const auto kitFile = dir.getChildFile ("Our drummer.drumkit");
        CHECK (kin.saveKit (kitFile, "OUR DRUMMER", rep), "save kit: %s", rep.toRawUTF8());
        const auto v = juce::JSON::parse (kitFile.loadFileAsString());
        int drums = 0; float snareCrack = 0, tomBody = 0;
        if (auto* arr = v["drums"].getArray())
            for (const auto& d : *arr)
            {
                ++drums;
                if (d["source"].toString() == "SNARE TOP") snareCrack = (float) (double) d["params"]["b3g"];
                if (d["source"].toString() == "TOM 1") tomBody = (float) (double) d["params"]["b1f"];
            }
        std::printf ("  SAVE KIT from the kick track: %s\n", rep.toRawUTF8());
        CHECK (drums >= 3, "the kit holds %d drums", drums);
        CHECK (std::abs (snareCrack - 9.0f) < 0.05f && std::abs (tomBody - 163.0f) < 0.5f,
               "the kit lost the tweaks: snare crack %.2f, tom body %.1f", snareCrack, tomBody);

        //  another session: the factory kit is on, then OUR DRUMMER is loaded from the file
        kin.loadFactoryKit (0, rep);                  // STANDARD everywhere
        tickAll ({ &snr, &tom });
        CHECK (std::abs (snr.currentParams().b3g - de::presetParams (de::S_SNARE_TOP, 0).b3g) < 0.01f, "STANDARD did not reach the snare");
        CHECK (tom.loadKitFile (kitFile, rep), "load kit: %s", rep.toRawUTF8());
        tickAll ({ &kin, &snr });
        CHECK (std::abs (snr.currentParams().b3g - 9.0f) < 0.05f && std::abs (tom.currentParams().b1f - 163.0f) < 0.5f,
               "OUR DRUMMER came back as crack %.2f, body %.1f", snr.currentParams().b3g, tom.currentParams().b1f);
        CHECK (snr.kitName() == "OUR DRUMMER", "the kit is called %s", snr.kitName().toRawUTF8());
        std::printf ("  OUR DRUMMER loaded from the TOM track restores the snare's crack and the tom's body on their own tracks\n");

        //  UNLINKED keeps its own
        snr.setLinked (false);
        kin.loadFactoryKit (1, rep);                  // CLASSIC ROCK
        tickAll ({ &snr, &tom });
        CHECK (std::abs (snr.currentParams().b3g - 9.0f) < 0.05f, "an unlinked track followed the kit");
        CHECK (same (tom.currentParams(), de::presetParams (de::S_TOM1, 1), 1e-3f, &w), "a linked one did not (%s)", w ? w : "?");
        std::printf ("  an UNLINKED snare keeps its own settings while the rest of the kit changes\n");
        snr.setLinked (true);

        //  a new track joining the kit takes the kit's drum
        {
            DrumEQProcessor oh; prep (oh, 2);
            kin.loadFactoryKit (2, rep);              // MODERN METAL: OH = METAL CYMBALS ONLY
            oh.kitTick();
            oh.selectSource (de::S_OH);
            CHECK (same (oh.currentParams(), de::presetParams (de::S_OH, 2), 1e-3f, &w), "a new OH track did not join the kit (%s)", w ? w : "?");
            std::printf ("  a new track switched to OVERHEADS joins the kit as METAL CYMBALS ONLY\n");
        }

        //  a user preset for a drum, on disk
        {
            const auto pf = dir.getChildFile ("Tight crack.json");
            CHECK (snr.saveUserPreset (pf, "TIGHT CRACK"), "save preset");
            snr.loadFactoryPreset (0);
            CHECK (snr.loadUserPreset (pf) && snr.currentName() == "TIGHT CRACK", "load preset");
        }
        dir.deleteRecursively();
    }

    // -- FIND DRUM through the wrapper ------------------------------------------------------
    {
        DrumEQProcessor p; prep (p, 1);
        p.setLinked (false);
        p.selectSource (de::S_TOM3);
        juce::AudioBuffer<float> x (1, (int) (3.0 * kFs));
        x.clear();
        for (int k = 0; k < 5; ++k)
        {
            const int at = (int) ((0.1 + 0.55 * k) * kFs);
            double ph = 0;
            for (int i = 0; i < (int) (0.5 * kFs) && at + i < x.getNumSamples(); ++i)
            {
                const double t = i / kFs;
                ph += 2.0 * juce::MathConstants<double>::pi * 91.0 * (1.0 + 0.03 * std::exp (-t / 0.05)) / kFs;
                x.addSample (0, at + i, (float) (0.5 * std::exp (-t / 0.3) * (std::sin (ph) + 0.4 * std::sin (1.59 * ph))));
            }
        }
        p.startFind();
        run (p, x);
        const auto r = p.finishFind();
        std::printf ("  FIND DRUM on a floor tom at 91 Hz: %.1f Hz from %d strokes; BODY now %.1f Hz\n", r.hz, r.hits, p.currentParams().b1f);
        CHECK (std::abs (r.hz - 91.0) < 2.0 && std::abs (p.currentParams().b1f - r.hz) < 1.0, "FIND DRUM through the wrapper");
    }

    // -- FIND sets the levels: two recordings 34 dB apart come out the same ----------------
    {
        auto snareTake = [] (double scale)
        {
            juce::AudioBuffer<float> x (1, (int) (6.0 * kFs));
            x.clear();
            juce::Random r (3);
            for (int k = 0; k < 6; ++k)
            {
                const int at = (int) ((0.4 + 0.9 * k) * kFs);
                double ph = 0;
                for (int i = 0; i < (int) (0.6 * kFs) && at + i < x.getNumSamples(); ++i)
                {
                    const double t = i / kFs;
                    ph += 2.0 * juce::MathConstants<double>::pi * 196.0 / kFs;
                    const double n = i < (int) (0.01 * kFs) ? (r.nextFloat() * 2.0f - 1.0f) * std::exp (-t / 0.002) : 0.0;
                    x.addSample (0, at + i, (float) (scale * (0.5 * std::exp (-t / 0.12) * (std::sin (ph) + 0.5 * std::sin (2.3 * ph)) + 0.6 * n)));
                }
            }
            return x;
        };
        auto peakDb = [] (const juce::AudioBuffer<float>& b, int from)
        {
            float m = 0; for (int i = from; i < b.getNumSamples(); ++i) m = juce::jmax (m, std::abs (b.getSample (0, i)));
            return 20.0f * std::log10 (juce::jmax (1e-9f, m));
        };
        float outPk[2] {}, inSet[2] {};
        const double scales[2] = { 0.01, 0.5 };
        for (int pass = 0; pass < 2; ++pass)
        {
            DrumEQProcessor p; prep (p, 1);
            p.setLinked (false);
            p.selectSource (de::S_SNARE_TOP);
            const auto take = snareTake (scales[pass]);
            p.startFind();
            run (p, take);
            const auto r = p.finishFind();
            inSet[pass] = p.currentParams().in;
            const auto out = run (p, take);
            outPk[pass] = peakDb (out, (int) (0.3 * kFs));
            std::printf ("  FIND on a snare recorded %s: IN %+.1f dB, gate %.0f dB, %d hits -> output peak %.1f dBFS\n",
                         pass == 0 ? "very quiet" : "hot", inSet[pass], r.gthr, r.hits, outPk[pass]);
            CHECK (r.levelSet && r.hits == 6 && ! r.clamped, "the level was not set (%d hits)", r.hits);
        }
        CHECK (std::abs (inSet[0] - inSet[1] - 33.98f) < 0.5f, "the trims differ by %.2f dB for a 33.98 dB difference", inSet[0] - inSet[1]);
        CHECK (std::abs (outPk[0] - outPk[1]) < 0.5f, "the two takes come out at %.1f and %.1f dBFS", outPk[0], outPk[1]);

        //  KEEP TRACK LEVEL: OUT gives back what IN took
        DrumEQProcessor p; prep (p, 1);
        p.setLinked (false);
        p.selectSource (de::S_SNARE_TOP);
        p.setKeepLevel (true);
        p.startFind();
        run (p, snareTake (0.01));
        const auto r = p.finishFind();
        CHECK (std::abs (p.currentParams().in + p.currentParams().out) < 0.05f && r.outDelta < -20.0f,
               "KEEP LEVEL: IN %+.1f, OUT %+.1f", p.currentParams().in, p.currentParams().out);
        std::printf ("  KEEP TRACK LEVEL: IN %+.1f dB, OUT %+.1f dB\n", p.currentParams().in, p.currentParams().out);
    }

    // -- MASTER: one panel tunes the kit -------------------------------------------------------
    {
        auto kick = std::make_unique<DrumEQProcessor>();
        DrumEQProcessor snr, snr2, solo;
        for (auto* p : { kick.get(), &snr, &snr2, &solo }) prep (*p, 1);
        kick->selectSource (de::S_KICK_IN);
        snr.selectSource (de::S_SNARE_TOP);
        snr2.selectSource (de::S_SNARE_TOP);
        solo.selectSource (de::S_SNARE_TOP);
        solo.setLinked (false);
        auto tick = [&] { for (auto* p : { kick.get(), &snr, &snr2, &solo }) if (p != nullptr) p->kitTick(); };
        tick();

        CHECK (kick->setMaster (true) && kick->isMaster(), "the kick track could not become the master");
        CHECK (! solo.setMaster (true) && ! solo.isMaster(), "an UNLINKED track became the master");
        kick->setTarget (de::S_SNARE_TOP);
        CHECK (kick->editingRemote() && &kick->view() != kick.get(), "the master is not editing the snare");
        CHECK (kick->view().currentSource() == de::S_SNARE_TOP, "the master's view is on %d", kick->view().currentSource());

        //  the master turns the snare's CRACK up
        const float kickBefore = kick->currentParams().b3g, soloBefore = solo.currentParams().b3g;
        if (auto* prm = kick->view().apvts.getParameter ("b3g")) prm->setValueNotifyingHost (prm->convertTo0to1 (7.5f));
        tick(); tick();
        CHECK (std::abs (snr.currentParams().b3g - 7.5f) < 0.05f && std::abs (snr2.currentParams().b3g - 7.5f) < 0.05f,
               "the snare tracks did not follow: %.2f / %.2f", snr.currentParams().b3g, snr2.currentParams().b3g);
        CHECK (std::abs (solo.currentParams().b3g - soloBefore) < 0.01f, "an UNLINKED snare followed the master");
        CHECK (std::abs (kick->currentParams().b3g - kickBefore) < 0.01f && kick->currentSource() == de::S_KICK_IN,
               "the master's own track moved");
        std::printf ("  MASTER on the kick track: CRACK +7.5 dB reaches both linked snare tracks, not the unlinked one or the kick itself\n");

        //  a snare track changes itself from its own panel: the master sees it
        if (auto* prm = snr.apvts.getParameter ("b2g")) prm->setValueNotifyingHost (prm->convertTo0to1 (-6.0f));
        tick(); tick();
        CHECK (std::abs (kick->view().currentParams().b2g + 6.0f) < 0.05f, "the master did not see the snare's own change (%.2f)", kick->view().currentParams().b2g);

        //  the live view: the master sees what the snare track hears
        {
            juce::AudioBuffer<float> b (1, kBlock);
            juce::MidiBuffer m;
            for (int k = 0; k < 60; ++k)
            {
                for (int i = 0; i < kBlock; ++i) b.setSample (0, i, 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 200.0 * (k * kBlock + i) / kFs));
                snr.processBlock (b, m);
            }
            tick();
            float pre[drumkit::kSpec], post[drumkit::kSpec], met[4], rate = 0;
            const bool seen = kick->remoteView (pre, post, met, rate);
            int pk = 0; for (int i = 1; i < drumkit::kSpec; ++i) if (pre[i] > pre[pk]) pk = i;
            const double pkHz = 20.0 * std::pow (1000.0, (pk + 0.5) / drumkit::kSpec);
            CHECK (seen && met[0] > 0.1f && pkHz > 150.0 && pkHz < 260.0, "the master does not see the snare (%d, peak %.0f Hz)", (int) seen, pkHz);
            std::printf ("  the master's display shows the snare track: spectrum peak near %.0f Hz, input meter %.2f\n", pkHz, met[0]);
        }

        //  FIND from the master: the snare track listens and sets itself
        {
            const float inBefore = snr.currentParams().in;
            kick->requestRemoteFind (false);
            tick();
            CHECK (snr.remoteFindActive(), "the snare track did not start listening");
            juce::AudioBuffer<float> x (1, (int) (6.5 * kFs));
            x.clear();
            for (int k = 0; k < 6; ++k)
            {
                const int at = (int) ((0.3 + 1.0 * k) * kFs);
                double ph = 0;
                for (int i = 0; i < (int) (0.5 * kFs); ++i)
                {
                    ph += 2.0 * juce::MathConstants<double>::pi * 190.0 / kFs;
                    x.addSample (0, at + i, (float) (0.02 * std::exp (-i / kFs / 0.12) * std::sin (ph)));
                }
            }
            run (snr, x);
            tick();
            float sec = 0, hz = 0, din = 0; int hits = 0;
            const int st = kick->remoteFindState (sec, hz, hits, din);
            CHECK (st == 2 && hits >= 5 && std::abs (hz - 190.0f) < 4.0f && din > 20.0f,
                   "remote FIND: state %d, %d hits, %.1f Hz, IN %+.1f", st, hits, hz, din);
            CHECK (std::abs (snr.currentParams().in - (inBefore + din)) < 0.1f, "the snare's IN is %.1f", snr.currentParams().in);
            tick(); tick();
            CHECK (std::abs (kick->view().currentParams().in - snr.currentParams().in) < 0.1f, "the master's view did not take the new IN");
            std::printf ("  FIND from the master: the snare track heard %d hits at %.1f Hz and set IN %+.1f dB\n", hits, hz, din);
        }

        //  a drum with no track yet: edited, and kept in the saved kit
        kick->setTarget (de::S_TOM3);
        if (auto* prm = kick->view().apvts.getParameter ("b1g")) prm->setValueNotifyingHost (prm->convertTo0to1 (4.0f));
        tick();
        const auto kitFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("drumeq-master-test.drumkit");
        juce::String rep;
        CHECK (kick->saveKit (kitFile, "MASTER TEST", rep) && rep.contains ("TOM 3"), "the saved kit lacks the edited TOM 3: %s", rep.toRawUTF8());
        kitFile.deleteFile();

        //  back HOME: the master's knobs are its own track's again
        kick->setTarget (de::S_KICK_IN);
        CHECK (! kick->editingRemote() && &kick->view() == kick.get(), "HOME did not return to the kick");

        //  a second master takes the role; the first lets go
        CHECK (snr.setMaster (true), "the snare could not take the role");
        tick();
        CHECK (snr.isMaster() && ! kick->isMaster(), "two masters (%d %d)", (int) snr.isMaster(), (int) kick->isMaster());
        snr.setMaster (false);
        kick->setMaster (true);
        kick->setTarget (de::S_SNARE_TOP);
        tick();

        //  the master track removed: the followers keep what it set
        const float keep = snr.currentParams().b3g;
        kick.reset();
        tick();
        CHECK (std::abs (snr.currentParams().b3g - keep) < 0.01f && std::abs (keep - 7.5f) < 0.05f, "removing the master changed the snare");
        juce::MemoryBlock mb; snr.getStateInformation (mb);
        DrumEQProcessor back; prep (back, 1);
        back.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::abs (back.currentParams().b3g - 7.5f) < 0.05f, "the snare's project state does not hold the master's edit");
        std::printf ("  a second master takes the role; removing the master leaves every track with its settings, saved in its own state\n");
    }

    std::printf ("\n%d checks - %s\n", checks, failures == 0 ? "ALL CLEAR" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
