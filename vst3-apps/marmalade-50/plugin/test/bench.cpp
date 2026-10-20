/*  MARMALADE 50 - bench

    Measures the simulated amp the way a technician would measure the real
    one, and prints the numbers next to what the sheets or the maker say:
      - the operating point: the rails against the test points, the EL34s
      - rated power: a 1 kHz sine, power into the 8 ohm tap at 10% THD
        (Orange rates the amp at 50 W)
      - the controls: each one's effect on the response, end to end
      - the channel relays: how far down the idle channel is
      - the reverb drive: the volts across the tank's input coil
      - Newton: failures anywhere (must be 0)
    [KNOWN] lines are measurements with no target to meet.

        mm50test
*/
#include "Partition.h"
#include "Tv50Netlist.h"
#include "Tv50Split.h"
#include "Engine50.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace mm;

namespace
{
    constexpr double PI = 3.14159265358979323846;
    int fails = 0, checks = 0;

    void check (bool ok, const char* what)
    {
        ++checks;
        if (! ok) ++fails;
        std::printf ("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    }

    /*  The engine at 48 kHz (the circuit at 96 kHz), fed a sine, and its
        output read as the 8 ohm tap's volts. The THD is the residual after
        the fundamental is fitted out, over whole cycles. */
    struct Tone { double vrms, thd, fund; };       // total rms, THD (ratio), the fundamental's rms

    Tone measure (Engine50& e, double freq, double amp, double seconds = 0.4)
    {
        const double sr = 48000.0;
        const int settle = (int) (0.25 * sr), n = (int) (seconds * sr);
        std::vector<float> buf (512);
        std::vector<double> y;
        long t = 0;
        for (int done = 0; done < settle + n;)
        {
            const int m = std::min (512, settle + n - done);
            for (int i = 0; i < m; ++i, ++t) buf[(size_t) i] = (float) (amp * std::sin (2.0 * PI * freq * t / sr));
            e.process (buf.data(), nullptr, m);
            for (int i = 0; i < m; ++i) if (done + i >= settle) y.push_back (buf[(size_t) i] * 40.0 / std::pow (10.0, Engine50::outputDb (0.5f) / 20.0));
            done += m;
        }
        // whole cycles only
        const int per = (int) std::lround (sr / freq);
        const size_t N = (y.size() / (size_t) per) * (size_t) per;
        double c = 0, s = 0, ss = 0;
        for (size_t i = 0; i < N; ++i)
        {
            const double ph = 2.0 * PI * freq * (double) (i + (size_t) settle) / sr;
            c += y[i] * std::cos (ph); s += y[i] * std::sin (ph); ss += y[i] * y[i];
        }
        c *= 2.0 / N; s *= 2.0 / N;
        const double f2 = 0.5 * (c * c + s * s), tot = ss / N;
        return { std::sqrt (tot), std::sqrt (std::max (0.0, tot - f2) / std::max (1e-30, f2)), std::sqrt (f2) };
    }

    Params50 base()
    {
        Params50 p;
        p.output = 0.5f;            // 0 dB... measure() undoes the trim anyway
        p.reverbOn = false;
        return p;
    }
}

int main()
{
    std::printf ("MARMALADE 50 bench\n\n");

    //--------------------------------------------------------------------------
    std::printf ("operating point (warm, P1 as set in the netlist)\n");
    {
        Engine50 e;
        Params50 p = base();
        e.setParams (p);
        e.prepare (48000.0, 512);
        check (e.ok(), "the engine builds and finds its operating point");
        auto& c = e.circuit();
        std::vector<float> z (512, 0.0f);
        double rail[6] {}, bias = 0, ia = 0, ib = 0;
        const char* names[] = { "ap", "bp", "cp", "dp", "ep", "fp" };
        const int blocks = 40;
        for (int k = 0; k < blocks; ++k)
        {
            std::fill (z.begin(), z.end(), 0.0f);      // process() writes its output here: silence in, every block
            e.process (z.data(), nullptr, 512);
            for (int r = 0; r < 6; ++r) rail[r] += c.nodeVoltage (names[r]) / blocks;
            bias += c.nodeVoltage ("nb") / blocks;
            ia += (c.nodeVoltage ("ota") - c.nodeVoltage ("pa")) / c.getValue ("rwa") / blocks;
            ib += (c.nodeVoltage ("otb") - c.nodeVoltage ("pb")) / c.getValue ("rwb") / blocks;
        }
        const double sheet[6] = { 0, 390, 317, 270, 245, 218 };
        for (int r = 0; r < 6; ++r)
            std::printf ("    %c  %6.1f V%s", "ABCDEF"[r], rail[r],
                         r == 0 ? "   (not on the sheet)\n" : "\n");
        (void) sheet;
        std::printf ("    bias %.2f V, EL34 %.1f / %.1f mA (biased; the sheet's test points are with the preset fully anti-CW, see mm50bias)\n",
                     bias, ia * 1e3, ib * 1e3);
        check (std::abs (0.5 * (ia + ib) - 0.035) < 0.003, "EL34s idle at the 35 mA they were biased to");
    }

    //--------------------------------------------------------------------------
    std::printf ("\nrated power (a generator in the loop's RETURN, 1 kHz, into the 8 ohm tap)\n");
    {
        /*  A class AB pentode pair with as little feedback as this one (1.5%
            of the 8 ohm tap, into the splitter's tail) is well past 10% THD
            long before it clips - the pair's transconductance expands (the
            EL34s' power law), measured 5% from the valves alone at the grid
            swing that makes 7 W. So 10% THD is not where this amp is rated;
            the most power it delivers at its fundamental, driven into
            saturation, is. */
        Engine50 e;
        Params50 p = base(); p.intoReturn = true;
        e.setParams (p);
        e.prepare (48000.0, 512);
        double best = 0.0, at10 = 0.0;
        for (double v : { 0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.6, 0.8, 1.2, 2.0, 3.0, 4.0 })
        {
            const Tone t = measure (e, 1000.0, v, 0.2);
            const double wf = t.fund * t.fund / 8.0;          // fund is the fundamental's rms
            std::printf ("    [KNOWN] %.2f V peak in: fundamental %.1f W, THD %.1f%%\n", v, wf, 100.0 * t.thd);
            best = std::max (best, wf);
            if (t.thd < 0.10) at10 = wf;
        }
        std::printf ("    most power at the fundamental: %.1f W into a nominal 8 ohms (Orange: 50 W); [KNOWN] under 10%% THD: %.1f W\n",
                     best, at10);
        check (best > 40.0 && best < 60.0, "saturated power within 20% of the 50 W the amp is sold as");
        check (e.circuit().failures() == 0, "no Newton failures driving the power amp into saturation");
    }

    //--------------------------------------------------------------------------
    std::printf ("\ncontrols, channel 2 (small signal, gain 2): response at 100 Hz / 1 kHz / 5 kHz, dB\n");
    {
        struct Case { const char* name; float Params50::* knob; };
        const Case cases[] = { { "treble", &Params50::treble }, { "middle", &Params50::middle }, { "bass", &Params50::bass } };
        for (const auto& cs : cases)
        {
            double r[2][3];
            for (int end = 0; end < 2; ++end)
            {
                Engine50 e;
                Params50 p = base(); p.channel = 1; p.gain2 = 0.2f; p.volume2 = 0.3f;
                p.*(cs.knob) = end ? 1.0f : 0.0f;
                e.setParams (p);
                e.prepare (48000.0, 512);
                const double fr[3] = { 100.0, 1000.0, 5000.0 };
                for (int f = 0; f < 3; ++f) r[end][f] = 20.0 * std::log10 (measure (e, fr[f], 0.01, 0.2).fund);
            }
            std::printf ("    [KNOWN] %-7s 0: %6.1f %6.1f %6.1f   10: %6.1f %6.1f %6.1f\n", cs.name,
                         r[0][0], r[0][1], r[0][2], r[1][0], r[1][1], r[1][2]);
            if (cs.knob == &Params50::treble) check (r[1][2] - r[0][2] > 6.0, "treble lifts 5 kHz");
            if (cs.knob == &Params50::bass)   check (r[1][0] - r[0][0] > 6.0, "bass lifts 100 Hz");
            if (cs.knob == &Params50::middle) check (r[1][1] - r[0][1] > 3.0, "middle lifts 1 kHz");
        }
    }

    //--------------------------------------------------------------------------
    std::printf ("\nchannel relays\n");
    {
        /*  What reaches the speaker from the jack with the selected
            channel's gain and volume right down, with the idle channel's
            knobs up and then down. The relays work if the idle channel's
            knobs make no difference: what is left is the jack's signal
            getting through on the rails (both first stages run whichever
            channel is selected, and rail F feeds the reverb recovery). */
        double sel[2], up[2], down[2];
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < 3; ++k)
            {
                Engine50 e;
                Params50 p = base();
                const bool selectThis = k == 0;
                p.channel = selectThis ? ch : 1 - ch;
                const float g = k == 2 ? 0.0f : 0.2f, v = k == 2 ? 0.0f : 0.3f;
                (ch == 0 ? p.gain1 : p.gain2) = g;  (ch == 0 ? p.volume1 : p.volume2) = v;
                (ch == 0 ? p.gain2 : p.gain1) = 0.0f; (ch == 0 ? p.volume2 : p.volume1) = 0.0f;
                e.setParams (p);
                e.prepare (48000.0, 512);
                (k == 0 ? sel : k == 1 ? up : down)[ch] = measure (e, 1000.0, 0.01, 0.2).fund;
            }
        for (int ch = 0; ch < 2; ++ch)
            std::printf ("    [KNOWN] preamp %d: %.3g V selected; idle %.2g V with its knobs up, %.2g V down\n",
                         ch + 1, sel[ch], up[ch], down[ch]);
        check (sel[0] > 1e-3 && sel[1] > 1e-3, "both channels pass signal when selected");
        check (std::abs (20.0 * std::log10 (up[0] / down[0])) < 1.0 && std::abs (20.0 * std::log10 (up[1] / down[1])) < 1.0,
               "an idle channel's knobs change nothing at the speaker (within 1 dB)");
    }

    //--------------------------------------------------------------------------
    std::printf ("\nreverb drive (preamp 2, gain 2, volume 3: the tank input coil's volts)\n");
    {
        Engine50 e;
        Params50 p = base(); p.channel = 1; p.gain2 = 0.2f; p.volume2 = 0.3f; p.reverbOn = true;
        e.setParams (p);
        e.prepare (48000.0, 512);
        auto& c = e.circuit();
        std::vector<float> buf (512);
        double pk = 0;
        long t = 0;
        for (int k = 0; k < 40; ++k)
        {
            for (int i = 0; i < 512; ++i, ++t) buf[(size_t) i] = (float) (0.1 * std::sin (2.0 * PI * 500.0 * t / 48000.0));
            e.process (buf.data(), nullptr, 512);
            if (k > 10) pk = std::max (pk, std::abs (c.nodeVoltage ("tk")));
        }
        std::printf ("    [KNOWN] 100 mV at 500 Hz in: %.2f V peak across the tank's input (the transformer and tank are estimates)\n", pk);
        check (pk > 1e-3, "the reverb driver drives the tank");
    }

    std::printf ("\n%d of %d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
