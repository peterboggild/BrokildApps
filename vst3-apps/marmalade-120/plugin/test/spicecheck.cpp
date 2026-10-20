/*  MARMALADE 120 - the solver against ngspice

    The same netlist the plugin runs is exported to ngspice, with the same
    tube equations written as behavioural sources, and both are driven with
    the same sine. ngspice is run far finer than the plugin (a 0.65 us step
    cap against the plugin's 5.2 us sample), so what this measures is the
    plugin's own error: its discretisation and its Newton, nothing else.

    Needs `ngspice` on the PATH. Prints, and exits 1 if any case is off by more
    than its tolerance.

        cmake -S test -B test/build && cmake --build test/build && ./test/build/spicecheck
*/
#include "Circuit.h"
#include "Or120Netlist.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

using namespace mm;

namespace
{
    std::string spiceName (const std::string& s)
    {
        std::string o = s;
        for (auto& c : o) if (c == '.') c = '_';
        return o;
    }

    std::string node (const Circuit& c, int n)
    {
        return n < 0 ? std::string ("0") : c.nodeNames()[(size_t) n];
    }

    std::string num (double v)
    {
        char b[64];
        std::snprintf (b, sizeof b, "%.10g", v);
        return b;
    }

    /*  One tube as ngspice behavioural current sources: the equations in
        Circuit.cpp's tubeCurrents(), term for term. */
    void tubeToSpice (std::ostream& o, const Circuit& c, const Element& e)
    {
        const auto& m = *e.model;
        const std::string n = spiceName (e.name);
        const std::string k = num (m.mult);
        if (! m.pentode)
        {
            const std::string P = node (c, e.n[0]), G = node (c, e.n[1]), K = node (c, e.n[2]);
            const std::string vp = "V(" + P + "," + K + ")", vg = "V(" + G + "," + K + ")";
            const std::string E1 = "(max(" + vp + ",0)/" + num (m.kp) + "*ln(1+exp(" + num (m.kp) + "*(1/" + num (m.mu)
                                 + "+" + vg + "/sqrt(" + num (m.kvb) + "+" + vp + "*" + vp + ")))))";
            o << "B" << n << "p " << P << " " << K << " I=" << k << "*2/" << num (m.kg1)
              << "*pwr(max(" << E1 << ",0)," << num (m.ex) << ")\n";
            o << "B" << n << "g " << G << " " << K << " I=" << k << "*" << num (m.gg) << "*pwr(ln(1+exp("
              << num (m.cg) << "*" << vg << "))/" << num (m.cg) << "," << num (m.xi) << ")\n";
            return;
        }
        const std::string P = node (c, e.n[0]), S = node (c, e.n[1]), G = node (c, e.n[2]), K = node (c, e.n[3]);
        const std::string vp = "V(" + P + "," + K + ")", vs = "max(V(" + S + "," + K + "),1)", vg = "V(" + G + "," + K + ")";
        const std::string E1 = "(" + vs + "/" + num (m.kp) + "*ln(1+exp(" + num (m.kp) + "*(1/" + num (m.mu) + "+" + vg + "/" + vs + "))))";
        o << "B" << n << "p " << P << " " << K << " I=" << k << "*2/" << num (m.kg1) << "*pwr(" << E1 << ","
          << num (m.ex) << ")*atan(2*ln(1+exp(" << vp << "/2))/" << num (m.kvb) << ")\n";
        o << "B" << n << "s " << S << " " << K << " I=" << k << "*pwr(" << E1 << "," << num (m.ex) << ")/" << num (m.kg2) << "\n";
        o << "B" << n << "g " << G << " " << K << " I=" << k << "*" << num (m.gg) << "*pwr(ln(1+exp("
          << num (m.cg) << "*" << vg << "))/" << num (m.cg) << "," << num (m.xi) << ")\n";
    }

    struct Case
    {
        const char* title;
        const char* net;
        std::vector<std::pair<std::string, double>> parts;   // element values to set first
        double inAmp, inHz, rail;                              // input sine; preamp rail (IN 1)
        const char* out;
        double tol;                                            // relative RMS error allowed
        double fs = 192000.0;
        double theta = 0.5;          // the solver's integrator; 0.5 = ngspice's trapezoidal
    };

    std::string exportSpice (const Circuit& c, const Case& k, double tstep, double tstop, const std::string& dataFile,
                            const char* method)
    {
        std::ostringstream o;
        o << "* exported by spicecheck: " << k.title << "\n";
        for (const auto& e : c.elements())
        {
            const std::string n = spiceName (e.name);
            const std::string a = node (c, e.n[0]), b = node (c, e.n[1]);
            switch (e.kind)
            {
                case Element::R: o << (n[0] == 'r' ? "" : "R") << n << " " << a << " " << b << " " << num (e.value) << "\n"; break;
                case Element::C: o << (n[0] == 'c' ? "" : "C") << n << " " << a << " " << b << " " << num (e.value) << "\n"; break;
                case Element::L: o << n << " " << a << " " << b << " " << num (e.value) << "\n"; break;
                case Element::V:
                    if (e.input == 0)      o << n << " " << a << " " << b << " DC 0 SIN(0 " << num (k.inAmp) << " " << num (k.inHz) << ")\n";
                    else if (e.input == 1) o << n << " " << a << " " << b << " DC " << num (k.rail) << "\n";
                    else                   o << n << " " << a << " " << b << " DC " << num (e.value) << "\n";
                    break;
                case Element::T:
                {
                    const std::string cc = node (c, e.n[2]), d = node (c, e.n[3]), mid = n + "_m";
                    o << "E" << n << " " << a << " " << mid << " " << cc << " " << d << " " << num (e.value) << "\n";
                    o << "V" << n << "s " << mid << " " << b << " DC 0\n";
                    o << "F" << n << " " << d << " " << cc << " V" << n << "s " << num (e.value) << "\n";
                    break;
                }
                case Element::X: tubeToSpice (o, c, e); break;
            }
        }
        // the same tiny leak to ground the solver puts on every node
        for (size_t i = 0; i < c.nodeNames().size(); ++i)
            o << "Rgmin_" << i << " " << c.nodeNames()[i] << " 0 1e12\n";

        o << ".options method=" << method << " reltol=1e-4 abstol=1e-13 vntol=1e-8 interp\n";
        o << ".tran " << num (tstep) << " " << num (tstop) << " 0 " << num (tstep / 8.0) << "\n";
        const auto& spec = c.outputs()[(size_t) c.outputIndex (k.out)];
        o << ".control\nset wr_singlescale\nrun\nwrdata " << dataFile << " v(" << node (c, spec.node) << ")\nquit\n.endc\n.end\n";
        return o.str();
    }

    bool runCase (const Case& k, const std::string& dir)
    {
        Circuit c;
        if (! c.parse (k.net)) { std::printf ("parse: %s\n", c.error().c_str()); return false; }
        for (const auto& pv : k.parts)
        {
            const int ei = c.elementIndex (pv.first);
            if (ei < 0) { std::printf ("no part %s\n", pv.first.c_str()); return false; }
            c.setValue (ei, pv.second);
        }
        const double fs = k.fs, tstop = 0.06;
        c.setTheta (k.theta);
        c.prepare (fs);

        static int caseNo = 0; ++caseNo;
        const std::string tag = "/case" + std::to_string (caseNo);
        const std::string cir = dir + tag + ".cir", dat = dir + tag + ".dat";
        /*  Trapezoidal first, the solver's own method. ngspice's trapezoidal
            can refuse a fully saturated output stage ("timestep too small"),
            and then Gear is the fallback - a slightly different integrator,
            so a looser match is expected there, and it says which it used. */
        std::vector<double> ref;
        const char* used = nullptr;
        for (const char* method : { "trap", "gear" })
        {
            { std::ofstream f (cir); f << exportSpice (c, k, 1.0 / fs, tstop, dat, method); }
            std::remove (dat.c_str());
            const std::string cmd = "ngspice -b " + cir + " > " + dir + tag + ".log 2>&1";
            if (std::system (cmd.c_str()) != 0) continue;
            ref.clear();
            std::ifstream f (dat);
            double t, v;
            while (f >> t >> v) ref.push_back (v);
            if (ref.size() > (size_t) (0.9 * tstop * fs)) { used = method; break; }
        }
        if (used == nullptr) { std::printf ("ngspice failed both ways, see %s%s.log\n", dir.c_str(), tag.c_str()); return false; }

        double in[2] = { 0.0, k.rail };
        if (! c.solveDC (in)) { std::printf ("DC: %s\n", c.error().c_str()); return false; }
        c.resetStats();
        const int oi = c.outputIndex (k.out);
        std::vector<double> out ((size_t) c.numOutputs());
        std::vector<double> sim;
        const int n = (int) std::lround (tstop * fs);
        for (int i = 0; i <= n; ++i)
        {
            in[0] = k.inAmp * std::sin (2.0 * M_PI * k.inHz * i / fs);
            c.process (in, out.data());
            sim.push_back (out[(size_t) oi]);
        }

        // compare over the last 40 ms, AC-coupled both, so it is the waveform
        const size_t len = std::min (ref.size(), sim.size());
        const size_t from = (size_t) (0.02 * fs);
        double mr = 0, ms = 0;
        for (size_t i = from; i < len; ++i) { mr += ref[i]; ms += sim[i]; }
        mr /= (double) (len - from); ms /= (double) (len - from);
        double e2 = 0, r2 = 0, peak = 0;
        for (size_t i = from; i < len; ++i)
        {
            const double a = ref[i] - mr, b = sim[i] - ms;
            e2 += (a - b) * (a - b); r2 += a * a; peak = std::max (peak, std::abs (a));
        }
        const double rel = std::sqrt (e2 / std::max (r2, 1e-30));
        const bool ok = rel < k.tol && len > from + 1000;
        std::printf ("%-52s %-4s pk %8.3f V  dc %8.3f/%8.3f  err %7.4f%%  newton avg %.2f max %d fails %d  %s\n",
                     k.title, used, peak, mr, ms, 100.0 * rel, c.meanIterations(), c.maxIterations(), c.failures(), ok ? "ok" : "FAIL");
        return ok;
    }
}

int main()
{
    const char* d = std::getenv ("TMPDIR");
    const std::string dir = d ? d : "/tmp";

    const std::vector<Case> cases =
    {
        { "preamp, gain 3 o'clock, 100 mV",       OR120_PREAMP, { { "rgaina", 150e3 }, { "rgainb", 850e3 } },  0.1, 440.0, 330.0, "drive", 0.005 },
        { "preamp, gain full, 1 V (hard clip)",   OR120_PREAMP, { { "rgaina", 1e3 },   { "rgainb", 999e3 } },  1.0, 440.0, 330.0, "drive", 0.01 },
        { "preamp, bass 0 treble 10, FAC 1n",     OR120_PREAMP, { { "rbassa", 999e3 }, { "rbassb", 1e3 }, { "rtreba", 1e3 }, { "rtrebb", 999e3 }, { "cfac", 1e-9 } }, 0.3, 1000.0, 330.0, "drive", 0.01 },
        { "power, master half, 2 V drive",        OR120_POWER,  {}, 2.0, 440.0, 0.0, "spk", 0.005 },
        { "power, master full, 20 V (saturated)", OR120_POWER,  { { "rmasaa", 1e3 }, { "rmasab", 219e3 }, { "rmasba", 1e3 }, { "rmasbb", 219e3 } }, 20.0, 220.0, 0.0, "spk", 0.02 },

        // and at 96 kHz, the rate the plugin runs at behind a 44.1/48 kHz host
        { "96k: preamp, gain full, 1 V",          OR120_PREAMP, { { "rgaina", 1e3 },   { "rgainb", 999e3 } },  1.0, 440.0, 330.0, "drive", 0.02, 96000.0 },
        { "96k: power, master full, 20 V",        OR120_POWER,  { { "rmasaa", 1e3 }, { "rmasab", 219e3 }, { "rmasba", 1e3 }, { "rmasbb", 219e3 } }, 20.0, 220.0, 0.0, "spk", 0.03, 96000.0 },

        /*  The plugin's own integrator (theta 0.55, see Circuit::setTheta)
            against ngspice's trapezoidal: what the damping costs. */
        { "96k, theta .55: preamp, gain full, 1 V", OR120_PREAMP, { { "rgaina", 1e3 }, { "rgainb", 999e3 } },  1.0, 440.0, 330.0, "drive", 0.03, 96000.0, 0.55 },
        { "96k, theta .55: power, master half, 2 V", OR120_POWER, {}, 2.0, 440.0, 0.0, "spk", 0.03, 96000.0, 0.55 },
        { "96k, theta .55: power, full, 20 V @1k",  OR120_POWER,  { { "rmasaa", 1e3 }, { "rmasab", 219e3 }, { "rmasba", 1e3 }, { "rmasbb", 219e3 } }, 20.0, 1000.0, 0.0, "spk", 0.10, 96000.0, 0.55 },
    };

    int bad = 0;
    for (const auto& k : cases) if (! runCase (k, dir)) ++bad;
    std::printf ("%s\n", bad == 0 ? "spicecheck: all cases agree with ngspice" : "spicecheck: FAILED");
    return bad == 0 ? 0 : 1;
}
