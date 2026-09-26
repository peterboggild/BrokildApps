/*  beettest - Beetmachine's bench. Drives the processor with no host.

    Every claim the design makes is measured here, and each check is built so
    it CAN fail: routing is proven by energy landing on one output pair and not
    the others, chokes against the same render without the choking hit,
    alignment against each drum rendered on its own.                         */

#include <JuceHeader.h>
#include "../src/BeetProcessor.h"
#include "../../../kickstart/plugin/src/PluginProcessor.h"
#include "../../../snare-tactics/plugin/src/PluginProcessor.h"
#include "../../../hats-off/plugin/src/PluginProcessor.h"

static int fails = 0, checks = 0;
static void check (bool ok, const juce::String& what, const juce::String& detail = {})
{
    ++checks;
    if (! ok) ++fails;
    std::printf ("  %s  %s%s\n", ok ? "ok  " : "FAIL", what.toRawUTF8(),
                 detail.isEmpty() ? "" : ("   [" + detail + "]").toRawUTF8());
}

constexpr double SR = 48000.0;
constexpr int BS = 256;

struct Note { double t; int note; float vel; };

//  Renders `seconds` of audio. Returns all channels: 0-1 main, 2+2s .. aux s.
static juce::AudioBuffer<float> render (BeetProcessor& p, const std::vector<Note>& notes, double seconds)
{
    const int total = (int) (seconds * SR);
    const int chans = p.getTotalNumOutputChannels();
    juce::AudioBuffer<float> outAll (chans, total);
    outAll.clear();
    juce::AudioBuffer<float> block (chans, BS);
    for (int pos = 0; pos < total; pos += BS)
    {
        const int n = juce::jmin (BS, total - pos);
        juce::AudioBuffer<float> b (block.getArrayOfWritePointers(), chans, n);
        juce::MidiBuffer midi;
        for (const auto& nt : notes)
        {
            const int at = (int) (nt.t * SR);
            if (at >= pos && at < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, nt.note, nt.vel), at - pos);
        }
        p.processBlock (b, midi);
        for (int c = 0; c < chans; ++c) outAll.copyFrom (c, pos, b, c, 0, n);
    }
    return outAll;
}

static std::unique_ptr<BeetProcessor> fresh (bool aux)
{
    auto p = std::make_unique<BeetProcessor>();
    if (aux) p->enableAllBuses();
    p->setRateAndBufferSizeDetails (SR, BS);
    p->prepareToPlay (SR, BS);
    return p;
}

static double energy (const juce::AudioBuffer<float>& b, int ch, int from, int to)
{
    double e = 0;
    to = juce::jmin (to, b.getNumSamples());
    for (int c = ch; c < ch + 2; ++c)
    {
        auto* x = b.getReadPointer (c);
        for (int i = juce::jmax (0, from); i < to; ++i) e += (double) x[i] * x[i];
    }
    return e;
}
static int onset (const juce::AudioBuffer<float>& b, int ch, float thr)
{
    for (int i = 0; i < b.getNumSamples(); ++i)
        if (std::abs (b.getSample (ch, i)) > thr || std::abs (b.getSample (ch + 1, i)) > thr) return i;
    return -1;
}
//  level 0 dB and pan centre on one slot, so a check about timing measures timing
static void neutral (BeetProcessor& p, int s)
{
    p.apvts.getParameter ("s" + juce::String (s + 1) + "_level")->setValueNotifyingHost (p.apvts.getParameter ("s" + juce::String (s + 1) + "_level")->convertTo0to1 (0.0f));
    p.apvts.getParameter ("s" + juce::String (s + 1) + "_pan")->setValueNotifyingHost (0.5f);
}

//  pick a drum preset by NAME (a kit's open hat may be short; the choke checks
//  need something that is still ringing when the choke lands)
static void presetByName (BeetProcessor& p, int s, const juce::String& name)
{
    if (auto* d = p.slotProcessor (s))
        for (int i = 0; i < d->getNumPrograms(); ++i)
            if (d->getProgramName (i) == name) { p.setSlotPreset (s, i); return; }
    std::printf ("  (no preset named %s)\n", name.toRawUTF8());
}

static juce::String db (double ratio) { return juce::String (10.0 * std::log10 (juce::jmax (1e-30, ratio)), 1) + " dB"; }

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);    // a crash must not eat the output
    juce::ScopedJuceInitialiser_GUI gui;
    std::printf ("\nBEETMACHINE bench\n\n");

    //  --- kits ---------------------------------------------------------------
    {
        auto p = fresh (false);
        std::printf ("kits\n");
        check (p->slotType (0) == beet::KICK && p->slotType (2) == beet::SNARE && p->slotType (4) == beet::HATS
               && p->slotType (7) == beet::HATS, "a fresh instance is a whole kit in the convention (kicks 1-2, snares 3-4, metal 5-8)");
        juce::StringArray missing;
        for (int k = 0; k < beet::numKits(); ++k) { p->loadKit (k); missing.addArray (p->missingPresets); }
        check (missing.isEmpty(), "every kit's presets exist by name in their drum (" + juce::String (beet::numKits()) + " kits)", missing.joinIntoString ("; "));
        p->loadKit (0);
        check ((p->slotChokedBy (5) & (1u << 4)) != 0, "the open hat (slot 6) is choked by the closed hat (slot 5) in the kits");
    }

    //  --- latency / alignment ------------------------------------------------
    std::printf ("\nalignment\n");
    int maxLat = 0;
    {
        auto p = fresh (true);
        maxLat = p->alignedLatency();
        std::printf ("  latency: kick %d  snare %d  hats %d  -> reported %d samples\n",
                     p->typeLatency (beet::KICK), p->typeLatency (beet::SNARE), p->typeLatency (beet::HATS), maxLat);
        check (p->getLatencySamples() == maxLat, "the host is told the aligned latency");

        //  each drum type's onset inside Beetmachine must be its onset alone
        //  plus exactly what alignment adds
        for (int t = beet::KICK; t < beet::NUM_TYPES; ++t)
        {
            std::unique_ptr<juce::AudioProcessor> solo;
            if (t == beet::KICK)  solo = std::make_unique<KickstartProcessor>();
            if (t == beet::SNARE) solo = std::make_unique<SnareTacticsProcessor>();
            if (t == beet::HATS)  solo = std::make_unique<HatsOffProcessor>();
            solo->setCurrentProgram (1);
            solo->setPlayConfigDetails (0, 2, SR, BS);
            solo->prepareToPlay (SR, BS);
            juce::AudioBuffer<float> a (2, (int) SR / 2); a.clear();
            for (int pos = 0; pos < a.getNumSamples(); pos += BS)
            {
                juce::AudioBuffer<float> v (a.getArrayOfWritePointers(), 2, pos, juce::jmin (BS, a.getNumSamples() - pos));
                juce::MidiBuffer m;
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, beet::neutralNote (t, t == beet::KICK ? 2 : 1), 0.85f), 0);   // the drums default to GM KIT
                solo->processBlock (v, m);
            }
            const int alone = onset (a, 0, 1.0e-4f);

            const int slot = t == beet::KICK ? 0 : t == beet::SNARE ? 2 : 4;
            auto q = fresh (true);
            for (int s = 0; s < 8; ++s) q->setSlotOut (s, beet::OUT_OWN);
            q->setSlotPreset (slot, 1);
            neutral (*q, slot);                // level 0 dB, centre: the check is about TIME, not level
            auto out = render (*q, { { 0.0, beet::C3_ROW[slot], 0.85f } }, 0.5);
            const int inBeet = onset (out, 2 + 2 * slot, 1.0e-4f);
            const int expect = alone + (maxLat - q->typeLatency (t));
            check (inBeet == expect, juce::String (beet::typeShort (t)) + ": onset lands where alignment puts it",
                   "alone " + juce::String (alone) + ", in Beetmachine " + juce::String (inBeet) + ", expected " + juce::String (expect));
        }
    }

    //  --- note maps and routing ----------------------------------------------
    std::printf ("\nnotes and routing\n");
    for (int map = 0; map < 2; ++map)
    {
        const int* row = map == 0 ? beet::C3_ROW : beet::GM_ROW;
        bool allRight = true; juce::String bad;
        for (int s = 0; s < beet::NUM_SLOTS; ++s)
        {
            auto p = fresh (true);
            p->setNoteMap (map == 0 ? BeetProcessor::MAP_C3 : BeetProcessor::MAP_GM);
            for (int i = 0; i < 8; ++i) p->setSlotOut (i, beet::OUT_OWN);
            auto out = render (*p, { { 0.01, row[s], 0.9f } }, 0.4);
            const double mine = energy (out, 2 + 2 * s, 0, out.getNumSamples());
            double others = energy (out, 0, 0, out.getNumSamples());
            for (int i = 0; i < 8; ++i) if (i != s) others += energy (out, 2 + 2 * i, 0, out.getNumSamples());
            if (! (mine > 1e-3 && others == 0.0)) { allRight = false; bad << "slot " << (s + 1) << " (note " << row[s] << ") "; }
        }
        check (allRight, juce::String (map == 0 ? "C3 row" : "GM row") + ": each note plays its slot, on that slot's own output only", bad);
    }
    {
        auto p = fresh (true);
        for (int i = 0; i < 8; ++i) p->setSlotOut (i, beet::OUT_OWN);
        p->setSlotNote (2, beet::C3_ROW[0]);                                   // slot 3 now shares slot 1's note
        auto out = render (*p, { { 0.01, beet::C3_ROW[0], 0.9f } }, 0.3);
        check (energy (out, 2, 0, out.getNumSamples()) > 1e-3 && energy (out, 6, 0, out.getNumSamples()) > 1e-3,
               "two slots on one note layer (both sound)");
        check (p->noteMap() == BeetProcessor::MAP_CUSTOM, "an edited note makes the map read CUSTOM");
    }
    {
        //  OWN with the host having left the pair switched off falls back to the mix
        auto p = fresh (false);
        for (int i = 0; i < 8; ++i) p->setSlotOut (i, beet::OUT_OWN);
        auto out = render (*p, { { 0.01, beet::C3_ROW[0], 0.9f } }, 0.3);
        check (energy (out, 0, 0, out.getNumSamples()) > 1e-3, "OWN with the output pair off in the host falls back to the mix (never silent)");
    }
    {
        //  the mix is exactly the sum of what OWN sends to the pairs
        std::vector<Note> groove;
        for (int b = 0; b < 8; ++b) { groove.push_back ({ b * 0.25, beet::C3_ROW[0], 0.9f }); groove.push_back ({ b * 0.25 + 0.125, beet::C3_ROW[4], 0.6f }); }
        groove.push_back ({ 0.5, beet::C3_ROW[2], 0.9f }); groove.push_back ({ 1.5, beet::C3_ROW[2], 0.9f });
        auto a = fresh (true);
        auto mixOut = render (*a, groove, 2.2);
        auto b = fresh (true);
        for (int i = 0; i < 8; ++i) b->setSlotOut (i, beet::OUT_OWN);
        auto ownOut = render (*b, groove, 2.2);
        double worst = 0, peak = 0;
        for (int i = 0; i < mixOut.getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c)
            {
                double sum = 0;
                for (int s = 0; s < 8; ++s) sum += ownOut.getSample (2 + 2 * s + c, i);
                worst = juce::jmax (worst, std::abs (sum - (double) mixOut.getSample (c, i)));
                peak = juce::jmax (peak, std::abs ((double) mixOut.getSample (c, i)));
            }
        check (peak > 0.01 && worst < 1.0e-5, "the stereo mix equals the sum of the eight own outputs",
               "peak " + juce::String (peak, 3) + ", worst difference " + juce::String (worst, 8));
    }
    {
        auto p = fresh (true);
        for (int i = 0; i < 8; ++i) p->setSlotOut (i, beet::OUT_BOTH);
        auto out = render (*p, { { 0.01, beet::C3_ROW[0], 0.9f } }, 0.3);
        check (energy (out, 0, 0, out.getNumSamples()) > 1e-3 && energy (out, 2, 0, out.getNumSamples()) > 1e-3, "BOTH sends to the mix and the slot's pair");
    }
    {
        auto p = fresh (true);
        p->setSlotSolo (2, true);
        auto out = render (*p, { { 0.01, beet::C3_ROW[0], 0.9f }, { 0.01, beet::C3_ROW[2], 0.9f } }, 0.3);
        auto q = fresh (true);
        auto only = render (*q, { { 0.01, beet::C3_ROW[2], 0.9f } }, 0.3);
        double d = 0; for (int i = 0; i < out.getNumSamples(); ++i) d = juce::jmax (d, (double) std::abs (out.getSample (0, i) - only.getSample (0, i)));
        check (d < 1.0e-6, "solo: with slot 3 soloed, the mix is exactly slot 3 alone", "worst " + juce::String (d, 8));
    }

    //  --- chokes -------------------------------------------------------------
    std::printf ("\nchokes\n");
    {
        //  open hat (slot 6), then the closed hat (slot 5) at 0.3 s
        const double tc = 0.3;
        auto a = fresh (true);
        for (int i = 0; i < 8; ++i) a->setSlotOut (i, beet::OUT_OWN);
        a->setSlotChokedBy (5, 1u << 4);
        presetByName (*a, 5, "Crash 16");
        auto with = render (*a, { { 0.0, beet::C3_ROW[5], 0.9f }, { tc, beet::C3_ROW[4], 0.9f } }, 0.8);
        auto b = fresh (true);
        for (int i = 0; i < 8; ++i) b->setSlotOut (i, beet::OUT_OWN);
        b->setSlotChokedBy (5, 1u << 4);
        presetByName (*b, 5, "Crash 16");
        auto without = render (*b, { { 0.0, beet::C3_ROW[5], 0.9f } }, 0.8);

        const int at = (int) (tc * SR) + maxLat;
        const int w0 = at + (int) (0.015 * SR), w1 = at + (int) (0.200 * SR);
        const double eWith = energy (with, 2 + 2 * 5, w0, w1), eWithout = energy (without, 2 + 2 * 5, w0, w1);
        check (eWithout > 1e-4 && eWith / eWithout < 1e-4, "a closed-hat hit chokes the ringing open hat (15-200 ms after)",
               "choked " + db (eWith / eWithout) + " below the unchoked tail");
        const double before = energy (with, 2 + 2 * 5, 0, at - 16), beforeRef = energy (without, 2 + 2 * 5, 0, at - 16);
        check (std::abs (before - beforeRef) <= 1e-9 * (1.0 + beforeRef), "and nothing changes before the choking hit sounds");

        //  the fade is a fade, not a click: the largest sample step in the
        //  choke window is no larger than the open hat's own
        double stepChoke = 0, stepSelf = 0;
        for (int i = at - 32; i < at + 400; ++i) stepChoke = juce::jmax (stepChoke, (double) std::abs (with.getSample (12, i) - with.getSample (12, i - 1)));
        for (int i = at - 32; i < at + 400; ++i) stepSelf  = juce::jmax (stepSelf, (double) std::abs (without.getSample (12, i) - without.getSample (12, i - 1)));
        check (stepSelf > 1.0e-4 && stepChoke <= stepSelf * 1.05 + 1e-6, "the choke fades (no step larger than the crash's own, which is ringing)",
               "largest step " + juce::String (stepChoke, 5) + " vs " + juce::String (stepSelf, 5));

        auto c = fresh (true);
        for (int i = 0; i < 8; ++i) c->setSlotOut (i, beet::OUT_OWN);
        c->setSlotChokedBy (5, 1u << 4);
        presetByName (*c, 5, "Crash 16");
        auto again = render (*c, { { 0.0, beet::C3_ROW[5], 0.9f }, { tc, beet::C3_ROW[4], 0.9f }, { 0.5, beet::C3_ROW[5], 0.9f } }, 0.8);
        const int a2 = (int) (0.5 * SR) + maxLat;
        check (energy (again, 12, a2, a2 + 4800) > 1e-3, "a choked slot plays again on its next hit");

        auto d = fresh (true);
        for (int i = 0; i < 8; ++i) d->setSlotOut (i, beet::OUT_OWN);
        d->setSlotChokedBy (5, 0);                  // the kit sets it; this check needs it OFF
        presetByName (*d, 5, "Crash 16");
        auto noChoke = render (*d, { { 0.0, beet::C3_ROW[5], 0.9f }, { tc, beet::C3_ROW[4], 0.9f } }, 0.8);
        check (energy (noChoke, 12, w0, w1) > 0.5 * eWithout, "with the choke switched off, the open hat rings on");
    }

    //  --- empty slots cost nothing -------------------------------------------
    std::printf ("\ncost\n");
    {
        std::vector<Note> busy;
        for (int b = 0; b < 32; ++b)
            for (int s = 0; s < 8; ++s) busy.push_back ({ b * 0.125 + s * 0.011, beet::C3_ROW[s], 0.8f });
        auto full = fresh (false);
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto o1 = render (*full, busy, 4.0);
        const double msFull = juce::Time::getMillisecondCounterHiRes() - t0;

        auto empty = fresh (false);
        for (int s = 0; s < 8; ++s) empty->setSlotType (s, beet::EMPTY);
        t0 = juce::Time::getMillisecondCounterHiRes();
        auto o2 = render (*empty, busy, 4.0);
        const double msEmpty = juce::Time::getMillisecondCounterHiRes() - t0;
        std::printf ("  full kit, a hit every 16th on all 8 slots: %.1f ms for 4 s = %.1f %% of one core\n", msFull, msFull / 40.0);
        std::printf ("  eight empty slots, same MIDI:              %.2f ms for 4 s = %.3f %% of one core\n", msEmpty, msEmpty / 40.0);
        check (energy (o2, 0, 0, o2.getNumSamples()) == 0.0, "empty slots are silent");
        check (msEmpty < msFull * 0.02, "empty slots cost next to nothing (under 2 % of a full kit)");
        auto quiet = fresh (false);
        auto o3 = render (*quiet, {}, 1.0);
        check (energy (o3, 0, 0, o3.getNumSamples()) == 0.0, "a full kit with no MIDI is exactly silent");
    }

    //  --- state -------------------------------------------------------------
    std::printf ("\nstate\n");
    {
        auto a = fresh (false);
        a->loadKit (2);
        a->setSlotType (7, beet::SNARE);
        a->setSlotPreset (7, 5);
        a->setSlotNote (3, 71);
        a->setSlotOut (4, beet::OUT_BOTH);
        a->setSlotChokedBy (6, 0b00000011u);
        a->setSlotSolo (1, true);
        if (auto* prm = a->apvts.getParameter ("s2_level")) prm->setValueNotifyingHost (0.3f);
        //  a change made in a drum's OWN panel must survive too
        if (auto* d = a->slotProcessor (0)) if (auto* prm = d->getParameters()[2]) prm->setValueNotifyingHost (0.123f);

        juce::MemoryBlock mb;
        a->getStateInformation (mb);
        auto b = fresh (false);
        b->setStateInformation (mb.getData(), (int) mb.getSize());

        bool same = true; juce::String diff;
        for (int s = 0; s < 8; ++s)
        {
            if (a->slotType (s) != b->slotType (s)) { same = false; diff << "type" << s << " "; }
            if (a->slotNote (s) != b->slotNote (s)) { same = false; diff << "note" << s << " "; }
            if (a->slotOut (s) != b->slotOut (s))   { same = false; diff << "out" << s << " "; }
            if (a->slotChokedBy (s) != b->slotChokedBy (s)) { same = false; diff << "choke" << s << " "; }
            if (a->slotSolo (s) != b->slotSolo (s)) { same = false; diff << "solo" << s << " "; }
            if (a->slotPreset (s) != b->slotPreset (s)) { same = false; diff << "preset" << s << " "; }
            if (a->slotProcessor (s) != nullptr)
            {
                juce::MemoryBlock x, y;
                a->slotProcessor (s)->getStateInformation (x);
                b->slotProcessor (s)->getStateInformation (y);
                if (x != y) { same = false; diff << "drum" << s << " "; }
            }
        }
        check (same, "save and reload restores every slot, including each drum's own settings", diff);
        check (std::abs (a->apvts.getParameter ("s2_level")->getValue() - b->apvts.getParameter ("s2_level")->getValue()) < 1e-6f,
               "and the slot strip parameters");
    }

    //  --- BWFX on the main mix ----------------------------------------------
    std::printf ("\nBWFX\n");
    {
        int echo = -1;
        for (int t = 0; t < bwfx::numModuleTypes(); ++t)
            if (juce::String (bwfx::moduleDescriptor (t).id) == "delay" || juce::String (bwfx::moduleDescriptor (t).name) == "ECHO") echo = t;
        check (echo >= 0, "the rack knows ECHO");

        const std::string emptyRack = bwfx::Rack().toJson();
        auto plain = fresh (true);
        check (plain->rack().toJson() == emptyRack, "a fresh instance has the EMPTY rack (nothing changes until you arm something)");

        //  slot 1 goes to its OWN output as well, so the rack's reach can be seen
        std::vector<Note> groove;
        for (int b = 0; b < 8; ++b) { groove.push_back ({ b * 0.25, beet::C3_ROW[0], 0.9f }); groove.push_back ({ b * 0.25 + 0.125, beet::C3_ROW[4], 0.6f }); }
        auto wet = fresh (true);
        for (auto* q : { plain.get(), wet.get() }) q->setSlotOut (0, beet::OUT_BOTH);
        if (echo >= 0) wet->rack().setEnabled (echo, true);
        auto a = render (*plain, groove, 2.5), b = render (*wet, groove, 2.5);
        double diffMain = 0, diffOwn = 0;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < a.getNumSamples(); ++i)
            {
                diffMain += std::pow ((double) a.getSample (c, i) - b.getSample (c, i), 2.0);
                diffOwn  += std::pow ((double) a.getSample (2 + c, i) - b.getSample (2 + c, i), 2.0);
            }
        check (diffMain > 1e-3 * energy (a, 0, 0, a.getNumSamples()), "ECHO on changes the main mix", db (diffMain / juce::jmax (1e-30, energy (a, 0, 0, a.getNumSamples()))));
        check (diffOwn == 0.0, "and leaves a slot's OWN output bit for bit alone: the rack is on the mix bus only");

        juce::MemoryBlock mb;
        wet->getStateInformation (mb);
        auto back = fresh (false);
        back->setStateInformation (mb.getData(), (int) mb.getSize());
        check (back->rack().toJson() == wet->rack().toJson() && echo >= 0 && back->rack().getEnabled (echo),
               "the rack is saved with the project and comes back");

        //  a project saved before BWFX existed carries no rack at all
        auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        xml->removeAttribute ("bwfx");
        juce::MemoryBlock old;
        juce::AudioProcessor::copyXmlToBinary (*xml, old);
        auto older = fresh (false);
        if (echo >= 0) older->rack().setEnabled (echo, true);
        older->setStateInformation (old.getData(), (int) old.getSize());
        check (older->rack().toJson() == emptyRack, "a project from before BWFX loads with the empty rack");
        check (! older->apvts.state.hasProperty ("bwfx"), "and the blob never lingers in the parameter state");

        const int before = wet->rack().getEnabled (echo) ? 1 : 0;
        wet->loadKit (3);
        check (before == 1 && wet->rack().getEnabled (echo), "loading a kit leaves the rack alone (a kit is the drums, the rack is the bus)");
    }

    std::printf ("\n%d checks, %d failed - %s\n\n", checks, fails, fails == 0 ? "ALL CLEAR" : "FAILURES");
    return fails == 0 ? 0 : 1;
}
