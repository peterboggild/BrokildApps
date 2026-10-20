/*  MARMALADE 50 - the solver against ngspice (the Marmalade 120's check, on this netlist)

    The whole amplifier, the same netlist the plugin runs, exported to ngspice
    with every device written as the same equations (behavioural sources for
    the valves, ngspice's own diode with the same parameters).

    A cold amp takes seconds of circuit time to come up - the rails' slowest
    filter is about a second - which ngspice cannot afford at the step it
    needs. So the solver warms the amp up exactly as the plugin does, and
    then hands ngspice its state as initial conditions (every node voltage,
    every inductor's current, the mains at the same phase). The whole
    netlist, as one Circuit: how the blocks of the plugin's split agree with
    it is test/splitcheck.cpp's to show. From that instant
    both run the same 60 ms side by side, ngspice with its step capped at an
    eighth of the solver's sample, and the waveforms are compared: so what
    this measures is the solver's own error, discretisation and Newton,
    nothing else.

    Needs `ngspice` on the PATH. Exits 1 if any case is off by more than its
    tolerance.
*/
#include "Circuit.h"
#include "Tv50Netlist.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace mm;

namespace
{
    constexpr double MAINS_PEAK = 240.0 * 1.41421356237, MAINS = 50.0;

    std::string spiceName (const std::string& s)
    {
        std::string o = s;
        for (auto& c : o) if (c == '.') c = '_';
        return o;
    }
    std::string node (const Circuit& c, int n) { return n < 0 ? std::string ("0") : c.nodeNames()[(size_t) n]; }
    std::string num (double v) { char b[64]; std::snprintf (b, sizeof b, "%.12g", v); return b; }

    // ln(1 + e^x), written so that it cannot overflow, as Circuit.cpp has it
    std::string softplus (const std::string& x)
    {
        return "(max(" + x + ",0)+ln(1+exp(-abs(" + x + "))))";
    }

    /*  One device as ngspice sees it: the equations of deviceCurrents(),
        term for term. */
    void deviceToSpice (std::ostream& o, const Circuit& c, const Element& e)
    {
        const auto& m = *e.model;
        const std::string n = spiceName (e.name), k = num (m.mult);
        if (m.kind == DeviceModel::DIODE)
        {
            o << "D" << n << " " << node (c, e.n[0]) << " " << node (c, e.n[1]) << " d_" << m.name << "\n";
            o << ".model d_" << m.name << " D(IS=" << num (m.Is) << " N=" << num (m.n) << ")\n";
            return;
        }
        const auto grid = [&] (const std::string& G, const std::string& K)
        {
            const std::string vg = "V(" + G + "," + K + ")";
            return k + "*(" + num (m.Gg) + "*pwr(" + softplus (num (m.Cg) + "*" + vg) + "/" + num (m.Cg) + ","
                 + num (m.xi) + ")+" + num (m.Ig0) + ")";
        };
        if (m.kind == DeviceModel::TRIODE)
        {
            const std::string P = node (c, e.n[0]), G = node (c, e.n[1]), K = node (c, e.n[2]);
            const std::string ik = k + "*" + num (m.G) + "*pwr(" + softplus (num (m.C) + "*(V(" + P + "," + K + ")/"
                                 + num (m.mu) + "+V(" + G + "," + K + "))") + "/" + num (m.C) + "," + num (m.gamma) + ")";
            o << "B" << n << "g " << G << " " << K << " I=" << grid (G, K) << "\n";
            o << "B" << n << "p " << P << " " << K << " I=" << ik << "-(" << grid (G, K) << ")\n";
            return;
        }
        // the pentode of deviceCurrents(), term for term
        const std::string P = node (c, e.n[0]), S = node (c, e.n[1]), G = node (c, e.n[2]), K = node (c, e.n[3]);
        const std::string vs = "max(V(" + S + "," + K + "),1)", vg = "V(" + G + "," + K + ")";
        const std::string vpe = "(2*" + softplus ("V(" + P + "," + K + ")/2") + ")";
        const std::string Ce = "(" + num (m.C) + "*pwr(250/" + vs + "," + num (m.s) + "))";
        const std::string ik = "(" + num (m.G) + "*pwr(" + softplus (Ce + "*(" + vg + "+" + vs + "/" + num (m.mu) + ")") + "/" + Ce + "," + num (m.gamma) + "))";
        const std::string vkn = "(" + num (m.vk) + "*(" + vs + "/250)*pwr(max(" + ik + ",1e-12)/0.1," + num (m.q) + "))";
        const std::string f = "((1-exp(-" + vpe + "/" + vkn + "))*(1-" + num (m.beta) + "/(1+" + vpe + "/" + num (m.lam) + ")))";
        o << "B" << n << "p " << P << " " << K << " I=" << k << "*" << ik << "*" << f << "\n";
        o << "B" << n << "s " << S << " " << K << " I=" << k << "*" << ik << "*(1-" << f << ")\n";
        o << "B" << n << "g " << G << " " << K << " I=" << grid (G, K) << "\n";
    }

    using Parts = std::vector<std::pair<std::string, double>>;

    struct Case
    {
        const char* title;
        Parts parts;                 // pot halves etc., set before the warm-up
        double inAmp, inHz;          // the signal's sine
        int inPort;                  // 0: the guitar's jack, 3: the loop's RETURN jack
        double fs;
        double tol;                  // relative RMS error allowed
    };

    double theta = 0.55;

    bool runCase (const Case& k, const std::string& dir, int caseNo)
    {
        Circuit c;
        if (! c.parse (TV50_AMP)) { std::printf ("parse: %s\n", c.error().c_str()); return false; }
        c.setTheta (theta);
        for (const auto& pv : k.parts)
        {
            const int ei = c.elementIndex (pv.first);
            if (ei < 0) { std::printf ("no part %s\n", pv.first.c_str()); return false; }
            c.setValue (ei, pv.second);
        }

        // the warm-up the plugin does
        double in[4] = { 0.0, MAINS_PEAK, 0.0, 0.0 };
        c.prepare (16000.0);
        if (! c.solveDC (in)) { std::printf ("DC: %s\n", c.error().c_str()); return false; }
        std::vector<double> out ((size_t) c.numOutputs());
        double phase = 0.0;
        auto step = [&] (double rate, double gtr)
        {
            const double m = std::sin (2.0 * M_PI * phase);
            phase += MAINS / rate; if (phase >= 1.0) phase -= 1.0;
            in[k.inPort] = gtr; in[1] = MAINS_PEAK * m;
            c.process (in, out.data());
        };
        // 10 s: the bias supply's 220k into 10u is the slowest filter here
        for (int i = 0; i < (int) (10.0 * 16000.0); ++i) step (16000.0, 0.0);
        c.changeRate (k.fs);
        for (int i = 0; i < (int) (0.1 * k.fs); ++i) step (k.fs, 0.0);

        /*  The hand-off. ngspice's t = 0 is the solver's last sample; its
            t = 1/fs is the solver's next, which will use mains phase `phase`. */
        const double tstep = 1.0 / k.fs, tstop = 0.06;
        const double phDeg = 360.0 * (phase - MAINS / k.fs);
        const std::string tag = dir + "/spice" + std::to_string (caseNo);
        {
            std::ofstream o (tag + ".cir");
            o << "* spicecheck: " << k.title << "\n";
            for (size_t ei = 0; ei < c.elements().size(); ++ei)
            {
                const auto& e = c.elements()[ei];
                const std::string n = spiceName (e.name), a = node (c, e.n[0]), b = node (c, e.n[1]);
                switch (e.kind)
                {
                    case Element::R: o << (n[0] == 'r' ? "" : "R") << n << " " << a << " " << b << " " << num (e.value) << "\n"; break;
                    case Element::C: o << (n[0] == 'c' ? "" : "C") << n << " " << a << " " << b << " " << num (e.value) << "\n"; break;
                    case Element::L: o << n << " " << a << " " << b << " " << num (e.value)
                                       << " ic=" << num (c.reactiveCurrent ((int) ei)) << "\n"; break;
                    case Element::V:
                        // the guitar starts at the next sample; its phase puts that at t = 1/fs
                        if (e.input == k.inPort) o << n << " " << a << " " << b << " DC 0 SIN(0 " << num (k.inAmp) << " " << num (k.inHz) << " 0 0 " << num (-360.0 * k.inHz / k.fs) << ")\n";
                        else if (e.input == 1)   o << n << " " << a << " " << b << " DC 0 SIN(0 " << num (MAINS_PEAK) << " " << num (MAINS) << " 0 0 " << num (phDeg) << ")\n";
                        else if (e.input >= 0)   o << n << " " << a << " " << b << " DC 0\n";      // the tank's return, a plug not used
                        else                   o << n << " " << a << " " << b << " DC " << num (e.value) << "\n";
                        break;
                    case Element::T:
                    {
                        const std::string cc = node (c, e.n[2]), d = node (c, e.n[3]), mid = n + "_m";
                        o << "E" << n << " " << a << " " << mid << " " << cc << " " << d << " " << num (e.value) << "\n";
                        o << "V" << n << "s " << mid << " " << b << " DC 0\n";
                        o << "F" << n << " " << d << " " << cc << " V" << n << "s " << num (e.value) << "\n";
                        o << ".ic V(" << mid << ")=" << num (e.n[1] >= 0 ? c.nodeVoltage (e.n[1]) : 0.0) << "\n";
                        break;
                    }
                    case Element::X: deviceToSpice (o, c, e); break;
                }
            }
            for (size_t i = 0; i < c.nodeNames().size(); ++i)
            {
                o << "Rgmin_" << i << " " << c.nodeNames()[i] << " 0 1e12\n";
                o << ".ic V(" << c.nodeNames()[i] << ")=" << num (c.nodeVoltage ((int) i)) << "\n";
            }
            o << ".options method=trap reltol=1e-5 abstol=1e-12 vntol=1e-7 interp\n";
            o << ".tran " << num (tstep) << " " << num (tstop) << " 0 " << num (tstep / 8.0) << " uic\n";
            const auto nodeOf = [&] (const char* out) { return node (c, c.outputs()[(size_t) c.outputIndex (out)].node); };
            o << ".control\nset wr_singlescale\nrun\nwrdata " << tag << ".dat v(" << nodeOf ("spk") << ") v(" << nodeOf ("b")
              << ")\nquit\n.endc\n.end\n";
        }
        std::remove ((tag + ".dat").c_str());
        if (std::system (("ngspice -b " + tag + ".cir > " + tag + ".log 2>&1").c_str()) != 0)
        { std::printf ("ngspice failed, see %s.log\n", tag.c_str()); return false; }

        /*  Each row placed by its own time stamp: with uic ngspice's first row
            is t = 1/fs, not 0, and indexing rows would compare everything one
            sample apart (which reads as a percent-level error that halves
            when the rate doubles - the tell). */
        const int n = (int) std::lround (tstop * k.fs);
        std::vector<double> refSpk ((size_t) n + 1, NAN), refB1 ((size_t) n + 1, NAN);
        {
            std::ifstream f (tag + ".dat");
            double t, a, b;
            while (f >> t >> a >> b)
            {
                const long idx = std::lround (t * k.fs);
                if (idx >= 0 && idx <= n) { refSpk[(size_t) idx] = a; refB1[(size_t) idx] = b; }
            }
        }

        // the solver, from the same instant
        const int iS = c.outputIndex ("spk"), iB = c.outputIndex ("b");
        std::vector<double> simSpk { out[(size_t) iS] }, simB1 { out[(size_t) iB] };
        c.resetStats();
        for (int i = 1; i <= n; ++i)
        {
            step (k.fs, k.inAmp * std::sin (2.0 * M_PI * k.inHz * (i - 1) / k.fs));
            simSpk.push_back (out[(size_t) iS]);
            simB1.push_back (out[(size_t) iB]);
        }

        // relative RMS error, both AC (the speaker has no DC; the ripple is what matters on B+)
        auto compare = [&] (const std::vector<double>& ref, const std::vector<double>& sim, double& peak)
        {
            const size_t len = std::min (ref.size(), sim.size());
            double mr = 0, ms = 0; size_t cnt = 0;
            for (size_t i = 0; i < len; ++i) if (std::isfinite (ref[i])) { mr += ref[i]; ms += sim[i]; ++cnt; }
            mr /= (double) std::max<size_t> (cnt, 1); ms /= (double) std::max<size_t> (cnt, 1);
            double e2 = 0, r2 = 0; peak = 0;
            for (size_t i = 0; i < len; ++i)
            {
                if (! std::isfinite (ref[i])) continue;
                const double a = ref[i] - mr, b = sim[i] - ms;
                e2 += (a - b) * (a - b); r2 += a * a; peak = std::max (peak, std::abs (a));
            }
            return std::sqrt (e2 / std::max (r2, 1e-30));
        };
        if (std::getenv ("DUMP"))
        {
            std::ofstream d (tag + ".cmp");
            for (size_t i = 0; i < simB1.size() && i < refB1.size(); ++i)
                d << i << " " << refSpk[i] << " " << simSpk[i] << " " << refB1[i] << " " << simB1[i] << "\n";
        }
        double pkS, pkB;
        const double eS = compare (refSpk, simSpk, pkS);
        const double eB = compare (refB1, simB1, pkB);
        size_t got = 0;
        for (double v : refSpk) if (std::isfinite (v)) ++got;
        const bool enough = got > (size_t) (0.9 * tstop * k.fs);
        const bool ok = enough && eS < k.tol && eB < k.tol;
        std::printf ("%-44s %3.0fk  spk pk %8.4f V err %7.3f%%  B+ ripple pk %6.2f V err %6.3f%%  newton avg %.2f max %d fail %d  %s\n",
                     k.title, k.fs / 1000.0, pkS, 100.0 * eS, pkB, 100.0 * eB,
                     c.meanIterations(), c.maxIterations(), c.failures(), ok ? "ok" : "FAIL");
        return ok;
    }
}

int main()
{
    const char* d = std::getenv ("TMPDIR");
    const std::string dir = d ? d : "/tmp";

    // a dual gain pot's two gangs, the top part of each
    auto gain1 = [] (double top) { return Parts { { "rg1a", top }, { "rg1b", 1e6 - top }, { "rg2a", top }, { "rg2b", 1e6 - top } }; };
    auto gain2 = [] (double top) { return Parts { { "rg3a", top }, { "rg3b", 1e6 - top }, { "rg4a", top }, { "rg4b", 1e6 - top } }; };
    auto join = [] (Parts a, const Parts& b) { a.insert (a.end(), b.begin(), b.end()); return a; };
    // the relays as each channel selects them (the netlist rests on preamp 1)
    const Parts pre1 = { { "rrl4x", 1e9 }, { "rrl4y", 0.1 }, { "rrl1x", 1e9 }, { "rrl1y", 0.1 } };
    const Parts pre2 = { { "rrl4x", 0.1 }, { "rrl4y", 1e9 }, { "rrl1x", 0.1 }, { "rrl1y", 1e9 } };
    const Parts intoReturn = { { "rjn", 1e9 }, { "rjr", 0.1 } };
    const Parts revOn = { { "rrl2", 1e9 } };

    /*  Tolerances say what each case can honestly show. At idle B carries
        about 10 mV of ripple behind the choke and 100u, and ngspice's own
        precision on a 390 V node (reltol 1e-5) is 4 mV of that, so 10% is
        as tight as the reference itself is (measured 6%). A hard-clipped
        preamp's harmonics reach the solver's discretisation at 96k: 5% there,
        1.6% at 192k - the error converging with rate, as on the Marmalade
        120 (6% at 96k, 2% at 192k). */
    const std::vector<Case> cases =
    {
        { "idle: the hum and the ripple alone",            {},                                  0.0, 440.0, 0,  96000.0, 0.10 },
        { "preamp 2 clean: gain low, 100 mV",              join (pre2, gain2 (900e3)),          0.1, 440.0, 0,  96000.0, 0.01 },
        { "preamp 1 crunch: gain half, 100 mV",            join (pre1, gain1 (500e3)),          0.1, 220.0, 0,  96000.0, 0.03 },
        { "preamp 1 flat out, 500 mV",                     join (pre1, gain1 (1e3)),            0.5, 220.0, 0,  96000.0, 0.06 },
        { "preamp 1 flat out, at 192k",                    join (pre1, gain1 (1e3)),            0.5, 220.0, 0, 192000.0, 0.02 },
        { "power amp from RETURN, into saturation",        join (intoReturn, revOn),            1.0, 220.0, 3,  96000.0, 0.03 },
    };

    // THETA=0.5 in the environment checks the bare trapezoidal rule instead
    if (const char* t = std::getenv ("THETA")) theta = std::atof (t);
    std::printf ("solver theta %.2f (the plugin runs 0.55)\n", theta);
    int bad = 0, no = 0;
    for (const auto& k : cases) if (! runCase (k, dir, ++no)) ++bad;
    std::printf ("%s\n", bad == 0 ? "spicecheck: all cases agree with ngspice" : "spicecheck: FAILED");
    return bad == 0 ? 0 : 1;
}
