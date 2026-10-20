#include "Circuit.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace mm
{

//==============================================================================
namespace
{
    /*  Koren's published fits. The pentode's (1 + sgn) factor of 2 is folded
        into the current equations below, as Koren writes them; grid current is
        Dempwolf's form with a conduction knee near 0 V. The EL34X2 entry is a
        matched pair in parallel - the four-valve output stage solved as two
        halves of a push-pull, which is exact for matched tubes.

        Of the interelectrode capacitances only grid-to-plate is kept, on the
        triodes: multiplied by the stage's gain (Miller) it is a real top-end
        roll-off against a grid stopper or a tone stack. The rest sit across
        low impedances and act far above anything audible, and every capacitor
        is a state the solver pays for each sample - they were measured at a
        third of its cost. The EL34s are driven through 1k1 and need none. */
    const TubeModel MODELS[] =
    {
        //  name     pent   mu     ex    kg1     kg2    kp     kvb    gg       cg    xi     mult  cgk  cgp      cpk
        { "12ax7",  false, 100.0, 1.40, 1060.0, 0.0,   600.0, 300.0, 6.0e-4, 13.9, 1.354, 1.0, 0.0, 1.7e-12, 0.0 },
        { "el34",   true,  11.0,  1.35, 650.0,  4200.0, 60.0, 24.0,  2.0e-3, 9.0,  1.35,  1.0, 0.0, 0.0,     0.0 },
        { "el34x2", true,  11.0,  1.35, 650.0,  4200.0, 60.0, 24.0,  2.0e-3, 9.0,  1.35,  2.0, 0.0, 0.0,     0.0 },
    };

    inline double softplus (double a)
    {
        if (a > 30.0)  return a;
        if (a < -30.0) return std::exp (a);
        return std::log1p (std::exp (a));
    }

    inline double sigmoid (double a)
    {
        if (a >= 0.0) { const double e = std::exp (-a); return 1.0 / (1.0 + e); }
        const double e = std::exp (a);
        return e / (1.0 + e);
    }

    std::string lower (std::string s)
    {
        for (auto& c : s) c = (char) std::tolower ((unsigned char) c);
        return s;
    }

    /*  SPICE values: 4.7k, 1meg, 22n, 100u. "m" is milli and "meg" is mega,
        exactly as SPICE has it, because a netlist that means something
        different here than in ngspice would make the cross-check worthless. */
    bool parseValue (const std::string& tok, double& out)
    {
        const std::string t = lower (tok);
        char* end = nullptr;
        const double v = std::strtod (t.c_str(), &end);
        if (end == t.c_str()) return false;
        std::string suf (end);
        double mul = 1.0;
        if      (suf.rfind ("meg", 0) == 0) mul = 1e6;
        else if (suf.empty())               mul = 1.0;
        else switch (suf[0])
        {
            case 't': mul = 1e12;  break;
            case 'g': mul = 1e9;   break;
            case 'k': mul = 1e3;   break;
            case 'm': mul = 1e-3;  break;
            case 'u': mul = 1e-6;  break;
            case 'n': mul = 1e-9;  break;
            case 'p': mul = 1e-12; break;
            case 'f': mul = 1e-15; break;
            default:  return false;
        }
        out = v * mul;
        return true;
    }

    inline double at (const std::vector<double>& m, int cols, int r, int c) { return m[(size_t) (r * cols + c)]; }

    constexpr double GMIN   = 1.0e-12;   // to ground at every node, as SPICE does at junctions
    constexpr double GSHORT = 1.0e6;     // an inductor at DC
    constexpr int    MAX_ITER = 40;      // per sample; the bench reports the worst it ever needed
}

const TubeModel* findTubeModel (const std::string& name)
{
    const std::string n = lower (name);
    for (const auto& m : MODELS)
        if (n == m.name) return &m;
    return nullptr;
}

//==============================================================================
namespace
{
    /*  softplus(a) = ln(1 + e^a) and its derivative, the logistic, from one
        exponential: e^-|a| never overflows, and the two share it. */
    inline void softplusSig (double a, double& sp, double& sg)
    {
        const double e = std::exp (-std::abs (a));
        sp = std::max (a, 0.0) + std::log1p (e);
        sg = (a >= 0.0) ? 1.0 / (1.0 + e) : e / (1.0 + e);
    }

    /*  gg * (softplus(cg v) / cg)^xi, and d/dv. Below about -2 V the grid
        draws less than a femtoamp, and most of the time that is where every
        grid is - so it returns zero there without spending three
        transcendentals on proving it. */
    inline void gridCurrent (const TubeModel& m, double vg, double& ig, double& dig)
    {
        const double a = m.cg * vg;
        if (a < -28.0) { ig = 0.0; dig = 0.0; return; }
        double sp, sg;
        softplusSig (a, sp, sg);
        const double base = sp / m.cg;
        const double pw = std::exp (m.xi * std::log (base));     // base^xi
        ig  = m.gg * pw;
        dig = m.gg * m.xi * pw / base * sg;
    }
}

/*  Every pow() here is written as exp(ex * ln E) with its derivative as
    ex * E^ex / E, because the per-sample cost of the whole amp is mostly
    these few lines and std::pow costs as much as both. */
void tubeCurrents (const TubeModel& m, const double* v, double* i, double* J)
{
    if (! m.pentode)
    {
        const double vg = v[0], vp = v[1];
        double ip = 0.0, dvg = 0.0, dvp = 0.0;
        if (vp > 0.0)
        {
            const double s = std::sqrt (m.kvb + vp * vp);
            double sp, sg;
            softplusSig (m.kp * (1.0 / m.mu + vg / s), sp, sg);
            const double E1 = vp / m.kp * sp;
            if (E1 > 1.0e-300)
            {
                const double dE_g = vp * sg / s;
                const double dE_p = sp / m.kp - vp * vp * vg * sg / (s * s * s);
                const double P = std::exp (m.ex * std::log (E1));
                ip = 2.0 / m.kg1 * P;
                const double dI = 2.0 * m.ex / m.kg1 * P / E1;
                dvg = dI * dE_g;
                dvp = dI * dE_p;
            }
        }
        double ig, dig;
        gridCurrent (m, vg, ig, dig);

        i[0] = m.mult * ig;
        i[1] = m.mult * ip;
        J[0] = m.mult * dig;  J[1] = 0.0;
        J[2] = m.mult * dvg;  J[3] = m.mult * dvp;
        return;
    }

    const double vg1 = v[0], vp = v[2];
    const double g2  = std::max (v[1], 1.0);
    const double dg2 = (v[1] > 1.0) ? 1.0 : 0.0;

    double sp, sg;
    softplusSig (m.kp * (1.0 / m.mu + vg1 / g2), sp, sg);
    const double E1 = std::max (g2 / m.kp * sp, 1.0e-300);
    const double dE_g1 = sg;
    const double dE_g2 = (sp / m.kp - sg * vg1 / g2) * dg2;

    const double P  = std::exp (m.ex * std::log (E1));
    const double dP = m.ex * P / E1;

    /*  Koren's atan of the plate voltage, which on its own would conduct
        backwards below 0 V - as hard as forwards. A plate does get driven
        negative (the transformer swings one half to twice the supply and the
        other below ground at full drive), and a valve then simply conducts
        nothing. So the plate voltage goes through a smooth floor at 0 first:
        identical above a few volts, zero below, and with no corner for
        Newton to trip on. */
    constexpr double VS = 2.0;
    double vpe, dvpe;
    {
        const double a = vp / VS;
        double sp, sg;
        softplusSig (a, sp, sg);
        vpe = VS * sp;
        dvpe = sg;
    }
    const double rr   = vpe / m.kvb;
    const double atn  = std::atan (rr);
    const double datn = dvpe / (m.kvb * (1.0 + rr * rr));

    const double ip  = 2.0 / m.kg1 * P * atn;
    const double ig2 = P / m.kg2;

    double ig1, dig1;
    gridCurrent (m, vg1, ig1, dig1);

    const double k = m.mult;
    i[0] = k * ig1;
    i[1] = k * ig2;
    i[2] = k * ip;

    // row = port current, column = port voltage
    J[0] = k * dig1;                         J[1] = 0.0;                              J[2] = 0.0;
    J[3] = k * dP * dE_g1 / m.kg2;           J[4] = k * dP * dE_g2 / m.kg2;           J[5] = 0.0;
    J[6] = k * 2.0 / m.kg1 * dP * dE_g1 * atn;
    J[7] = k * 2.0 / m.kg1 * dP * dE_g2 * atn;
    J[8] = k * 2.0 / m.kg1 * P * datn;
}

//==============================================================================
bool luFactor (double* a, int n, int* piv)
{
    for (int k = 0; k < n; ++k)
    {
        int p = k;
        double best = std::abs (a[k * n + k]);
        for (int r = k + 1; r < n; ++r)
            if (std::abs (a[r * n + k]) > best) { best = std::abs (a[r * n + k]); p = r; }
        piv[k] = p;
        if (best < 1.0e-300) return false;
        if (p != k)
            for (int c = 0; c < n; ++c) std::swap (a[k * n + c], a[p * n + c]);

        const double inv = 1.0 / a[k * n + k];
        for (int r = k + 1; r < n; ++r)
        {
            const double f = (a[r * n + k] *= inv);
            if (f == 0.0) continue;
            for (int c = k + 1; c < n; ++c) a[r * n + c] -= f * a[k * n + c];
        }
    }
    return true;
}

void luSolve (const double* lu, int n, const int* piv, double* b)
{
    for (int k = 0; k < n; ++k)
        if (piv[k] != k) std::swap (b[k], b[piv[k]]);
    for (int r = 1; r < n; ++r)
    {
        double s = b[r];
        for (int c = 0; c < r; ++c) s -= lu[r * n + c] * b[c];
        b[r] = s;
    }
    for (int r = n - 1; r >= 0; --r)
    {
        double s = b[r];
        for (int c = r + 1; c < n; ++c) s -= lu[r * n + c] * b[c];
        b[r] = s / lu[r * n + r];
    }
}

//==============================================================================
/*  The same LU at a size fixed at compile time. Newton factors a 4x4 (preamp)
    or 10x10 (power) every iteration of every sample, and with the size known
    the compiler unrolls and vectorises what the general loop cannot - measured
    at about twice as fast. */
namespace
{
    template <int n>
    bool luFactorN (double* a, int* piv)
    {
        for (int k = 0; k < n; ++k)
        {
            int p = k;
            double best = std::abs (a[k * n + k]);
            for (int r = k + 1; r < n; ++r)
            {
                const double v = std::abs (a[r * n + k]);
                if (v > best) { best = v; p = r; }
            }
            piv[k] = p;
            if (best < 1.0e-300) return false;
            if (p != k)
                for (int c = 0; c < n; ++c) std::swap (a[k * n + c], a[p * n + c]);
            const double inv = 1.0 / a[k * n + k];
            for (int r = k + 1; r < n; ++r)
            {
                const double f = (a[r * n + k] *= inv);
                for (int c = k + 1; c < n; ++c) a[r * n + c] -= f * a[k * n + c];
            }
        }
        return true;
    }

    template <int n>
    void luSolveN (const double* lu, const int* piv, double* b)
    {
        for (int k = 0; k < n; ++k)
            if (piv[k] != k) std::swap (b[k], b[piv[k]]);
        for (int r = 1; r < n; ++r)
        {
            double s = b[r];
            for (int c = 0; c < r; ++c) s -= lu[r * n + c] * b[c];
            b[r] = s;
        }
        for (int r = n - 1; r >= 0; --r)
        {
            double s = b[r];
            for (int c = r + 1; c < n; ++c) s -= lu[r * n + c] * b[c];
            b[r] = s / lu[r * n + r];
        }
    }
}

bool Circuit::factorPorts()
{
    switch (M)
    {
        case 2:  return luFactorN<2>  (Jac.data(), piv.data());
        case 4:  return luFactorN<4>  (Jac.data(), piv.data());
        case 6:  return luFactorN<6>  (Jac.data(), piv.data());
        case 8:  return luFactorN<8>  (Jac.data(), piv.data());
        case 10: return luFactorN<10> (Jac.data(), piv.data());
        case 12: return luFactorN<12> (Jac.data(), piv.data());
        default: return luFactor (Jac.data(), M, piv.data());
    }
}

void Circuit::solvePorts (double* b) const
{
    switch (M)
    {
        case 2:  luSolveN<2>  (Jac.data(), piv.data(), b); return;
        case 4:  luSolveN<4>  (Jac.data(), piv.data(), b); return;
        case 6:  luSolveN<6>  (Jac.data(), piv.data(), b); return;
        case 8:  luSolveN<8>  (Jac.data(), piv.data(), b); return;
        case 10: luSolveN<10> (Jac.data(), piv.data(), b); return;
        case 12: luSolveN<12> (Jac.data(), piv.data(), b); return;
        default: luSolve (Jac.data(), M, piv.data(), b); return;
    }
}

//==============================================================================
int Circuit::addNode (const std::string& raw)
{
    const std::string name = lower (raw);
    if (name == "0" || name == "gnd") return -1;
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i] == name) return (int) i;
    nodes.push_back (name);
    return (int) nodes.size() - 1;
}

int Circuit::nodeIndex (const std::string& raw) const
{
    const std::string name = lower (raw);
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i] == name) return (int) i;
    return -2;
}

int Circuit::elementIndex (const std::string& raw) const
{
    const std::string name = lower (raw);
    for (size_t i = 0; i < elems.size(); ++i)
        if (elems[i].name == name) return (int) i;
    return -1;
}

int Circuit::outputIndex (const std::string& raw) const
{
    const std::string name = lower (raw);
    for (size_t i = 0; i < outs.size(); ++i)
        if (outs[i].name == name) return (int) i;
    return -1;
}

bool Circuit::parse (const std::string& text)
{
    elems.clear(); nodes.clear(); outs.clear(); err.clear();
    numIn = 0;

    struct PendingOut { std::string name, what; int line; };
    std::vector<PendingOut> pendingOuts;

    std::istringstream in (text);
    std::string line;
    int lineNo = 0;

    auto fail = [&] (const std::string& why)
    {
        err = "line " + std::to_string (lineNo) + ": " + why + "  [" + line + "]";
        elems.clear(); nodes.clear(); outs.clear();
        return false;
    };

    while (std::getline (in, line))
    {
        ++lineNo;
        const auto semi = line.find (';');
        std::string body = (semi == std::string::npos) ? line : line.substr (0, semi);
        std::istringstream ts (body);
        std::vector<std::string> tok;
        for (std::string t; ts >> t;) tok.push_back (t);
        if (tok.empty() || tok[0][0] == '*') continue;

        const std::string head = lower (tok[0]);
        if (head == ".end") break;
        if (head == ".out")
        {
            if (tok.size() != 3) return fail ("expected .out name node|I(Vname)");
            pendingOuts.push_back ({ lower (tok[1]), lower (tok[2]), lineNo });
            continue;
        }
        if (head[0] == '.') continue;    // other dot-lines are ngspice's business

        Element e;
        e.name = head;
        switch (head[0])
        {
            case 'r': case 'c': case 'l':
            {
                if (tok.size() != 4) return fail ("expected name a b value");
                e.kind = head[0] == 'r' ? Element::R : head[0] == 'c' ? Element::C : Element::L;
                e.n[0] = addNode (tok[1]); e.n[1] = addNode (tok[2]);
                if (! parseValue (tok[3], e.value) || e.value <= 0.0) return fail ("bad value");
                break;
            }
            case 'v':
            {
                if (tok.size() != 5) return fail ("expected V+ V- DC value | IN index");
                e.kind = Element::V;
                e.n[0] = addNode (tok[1]); e.n[1] = addNode (tok[2]);
                const std::string mode = lower (tok[3]);
                if (mode == "dc") { if (! parseValue (tok[4], e.value)) return fail ("bad value"); }
                else if (mode == "in")
                {
                    e.input = std::atoi (tok[4].c_str());
                    if (e.input < 0) return fail ("bad input index");
                    numIn = std::max (numIn, e.input + 1);
                }
                else return fail ("V source must be DC or IN");
                break;
            }
            case 't':
            {
                if (tok.size() != 6) return fail ("expected T a b c d ratio");
                e.kind = Element::T;
                for (int k = 0; k < 4; ++k) e.n[k] = addNode (tok[(size_t) k + 1]);
                if (! parseValue (tok[5], e.value) || e.value == 0.0) return fail ("bad ratio");
                break;
            }
            case 'x':
            {
                e.kind = Element::X;
                e.model = findTubeModel (tok.back());
                if (e.model == nullptr) return fail ("unknown tube model " + tok.back());
                const size_t want = e.model->pentode ? 6u : 5u;
                if (tok.size() != want) return fail (e.model->pentode ? "expected X p g2 g1 k MODEL"
                                                                      : "expected X p g k MODEL");
                for (size_t k = 1; k + 1 < tok.size(); ++k) e.n[k - 1] = addNode (tok[k]);
                break;
            }
            default:
                return fail ("unknown element");
        }
        for (const auto& o : elems)
            if (o.name == e.name) return fail ("duplicate name");
        elems.push_back (e);

        // a tube's own capacitances, as plain capacitors the solver treats like any other
        if (e.kind == Element::X)
        {
            const auto& m = *e.model;
            const int p = e.n[0];
            const int g = m.pentode ? e.n[2] : e.n[1];
            const int k = m.pentode ? e.n[3] : e.n[2];
            const struct { const char* s; int a, b; double c; } caps[] =
                { { ".cgk", g, k, m.cgk }, { ".cgp", g, p, m.cgp }, { ".cpk", p, k, m.cpk } };
            for (const auto& c : caps)
            {
                if (c.c <= 0.0) continue;
                Element ce;
                ce.kind = Element::C;
                ce.name = e.name + c.s;
                ce.n[0] = c.a; ce.n[1] = c.b;
                ce.value = c.c * m.mult;
                elems.push_back (ce);
            }
        }
    }

    for (const auto& po : pendingOuts)
    {
        lineNo = po.line;
        OutputSpec o;
        o.name = po.name;
        if (po.what.size() > 3 && po.what.rfind ("i(", 0) == 0 && po.what.back() == ')')
        {
            const int ei = elementIndex (po.what.substr (2, po.what.size() - 3));
            if (ei < 0 || elems[(size_t) ei].kind != Element::V) return fail ("I() must name a V source");
            o.source = ei;
        }
        else
        {
            o.node = nodeIndex (po.what);
            if (o.node < -1) return fail ("unknown node " + po.what);
        }
        outs.push_back (o);
    }

    dirty = true; built = false;
    return true;
}

//==============================================================================
void Circuit::prepare (double sampleRate)
{
    fs = sampleRate;
    dirty = true;
    build();
}

void Circuit::setValue (int ei, double value)
{
    if (ei < 0 || ei >= (int) elems.size() || value <= 0.0) return;
    auto& e = elems[(size_t) ei];
    if (e.value == value) return;

    // A capacitor's state is scaled with its conductance, so the switch keeps
    // the charge it had rather than inventing a new one.
    if (e.kind == Element::C && built)
        for (int r = 0; r < nx; ++r)
            if (reactive[(size_t) r] == ei) x[(size_t) r] *= value / e.value;

    e.value = value;
    dirty = true;
}

void Circuit::stamp (std::vector<double>& S, bool dc) const
{
    std::fill (S.begin(), S.end(), 0.0);
    auto add = [&] (int r, int c, double v) { if (r >= 0 && c >= 0) S[(size_t) (r * Ns + c)] += v; };
    auto cond = [&] (int a, int b, double g) { add (a, a, g); add (b, b, g); add (a, b, -g); add (b, a, -g); };

    for (int n = 0; n < N; ++n) add (n, n, GMIN);

    for (size_t ei = 0; ei < elems.size(); ++ei)
    {
        const auto& e = elems[ei];
        const int a = e.n[0], b = e.n[1];
        switch (e.kind)
        {
            case Element::R: cond (a, b, 1.0 / e.value); break;
            case Element::C: if (! dc) cond (a, b, e.value * fs / theta); break;
            case Element::L: cond (a, b, dc ? GSHORT : theta / (e.value * fs)); break;
            case Element::V:
            {
                const int br = branchOf[ei];
                add (a, br, 1.0);  add (b, br, -1.0);
                add (br, a, 1.0);  add (br, b, -1.0);
                break;
            }
            case Element::T:
            {
                const int br = branchOf[ei];
                const int c = e.n[2], d = e.n[3];
                const double n = e.value;
                add (a, br, 1.0);  add (b, br, -1.0);  add (c, br, -n);  add (d, br, n);
                add (br, a, 1.0);  add (br, b, -1.0);  add (br, c, -n);  add (br, d, n);
                break;
            }
            case Element::X: break;
        }
    }
}

void Circuit::build()
{
    N = (int) nodes.size();
    branchOf.assign (elems.size(), -1);
    reactive.clear(); tubes.clear(); portOfTube.clear(); portCount.clear(); sources.clear();

    int nb = 0;
    M = 0;
    for (size_t ei = 0; ei < elems.size(); ++ei)
    {
        const auto& e = elems[ei];
        if (e.kind == Element::V || e.kind == Element::T) branchOf[ei] = N + nb++;
        if (e.kind == Element::V) sources.push_back ((int) ei);
        if (e.kind == Element::C || e.kind == Element::L) reactive.push_back ((int) ei);
        if (e.kind == Element::X)
        {
            tubes.push_back ((int) ei);
            portOfTube.push_back (M);
            portCount.push_back (e.model->pentode ? 3 : 2);
            M += e.model->pentode ? 3 : 2;
        }
    }
    Ns = N + nb;
    const int oldNx = nx;
    nx = (int) reactive.size();
    nu = (int) sources.size();
    no = (int) outs.size();

    if (! built || oldNx != nx)
    {
        x.assign ((size_t) nx, 0.0);
        vPort.assign ((size_t) M, 0.0);
    }
    xNew.assign ((size_t) nx, 0.0);
    iPort.assign ((size_t) M, 0.0);
    pBuf.assign ((size_t) M, 0.0);
    Jd.assign ((size_t) (M * M), 0.0);
    Jac.assign ((size_t) (M * M), 0.0);
    rhs.assign ((size_t) std::max (M, Ns), 0.0);
    rTrial.assign ((size_t) std::max (M, Ns), 0.0);
    step.assign ((size_t) M, 0.0);
    vTrial.assign ((size_t) M, 0.0);
    iTrial.assign ((size_t) M, 0.0);
    JdTrial.assign ((size_t) (M * M), 0.0);
    pPrev.assign ((size_t) M, 0.0);
    pMid.assign ((size_t) M, 0.0);
    vStart.assign ((size_t) M, 0.0);
    havePrev = false;
    jacValid = false;
    piv.assign ((size_t) std::max (M, Ns), 0);
    u.assign ((size_t) nu, 0.0);

    /*  The theta method: theta = 1/2 is the trapezoidal rule, 1 is backward
        Euler. Each element's current is i = G v + c x[n-1] and its state moves
        on as x[n] = (1 + beta) G v + d x[n-1], beta = (1 - theta) / theta;
        at theta = 1/2 that is the familiar 2 G v - x. */
    const double beta = (1.0 - theta) / theta;
    Gx.assign ((size_t) nx, 0.0);
    cx.assign ((size_t) nx, 0.0);
    dx.assign ((size_t) nx, 0.0);
    for (int r = 0; r < nx; ++r)
    {
        const auto& e = elems[(size_t) reactive[(size_t) r]];
        if (e.kind == Element::C) { Gx[(size_t) r] = e.value * fs / theta;   cx[(size_t) r] = -1.0; dx[(size_t) r] = -beta; }
        else                      { Gx[(size_t) r] = theta / (e.value * fs); cx[(size_t) r] =  1.0; dx[(size_t) r] =  1.0; }
    }

    // S, factorised once, then solved against each incidence column
    std::vector<double> S ((size_t) (Ns * Ns));
    stamp (S, false);
    std::vector<int> P ((size_t) Ns);
    if (! luFactor (S.data(), Ns, P.data())) { err = "singular circuit matrix"; return; }

    auto solveCols = [&] (int cols, auto fillCol, std::vector<double>& out)
    {
        out.assign ((size_t) (Ns * cols), 0.0);
        std::vector<double> col ((size_t) Ns);
        for (int c = 0; c < cols; ++c)
        {
            std::fill (col.begin(), col.end(), 0.0);
            fillCol (c, col);
            luSolve (S.data(), Ns, P.data(), col.data());
            for (int r = 0; r < Ns; ++r) out[(size_t) (r * cols + c)] = col[(size_t) r];
        }
    };
    auto incid = [] (std::vector<double>& col, int a, int b)
    {
        if (a >= 0) col[(size_t) a] += 1.0;
        if (b >= 0) col[(size_t) b] -= 1.0;
    };
    auto portNodes = [&] (int port, int& a, int& b)
    {
        for (size_t t = 0; t < tubes.size(); ++t)
        {
            const int first = portOfTube[t];
            const auto& e = elems[(size_t) tubes[t]];
            const int np = e.model->pentode ? 3 : 2;
            if (port < first || port >= first + np) continue;
            const int k = port - first;
            if (e.model->pentode)
            {
                const int pl = e.n[0], g2 = e.n[1], g1 = e.n[2], ca = e.n[3];
                a = (k == 0) ? g1 : (k == 1) ? g2 : pl;
                b = ca;
            }
            else
            {
                a = (k == 0) ? e.n[1] : e.n[0];
                b = e.n[2];
            }
            return;
        }
    };

    std::vector<double>& QX = Qfull_x;
    std::vector<double>& QU = Qfull_u;
    std::vector<double>& QN = Qfull_n;
    solveCols (nx, [&] (int c, std::vector<double>& col)
               { const auto& e = elems[(size_t) reactive[(size_t) c]]; incid (col, e.n[0], e.n[1]); }, QX);
    solveCols (nu, [&] (int c, std::vector<double>& col)
               { col[(size_t) branchOf[(size_t) sources[(size_t) c]]] = 1.0; }, QU);
    solveCols (M, [&] (int c, std::vector<double>& col)
               { int a = -1, b = -1; portNodes (c, a, b); incid (col, a, b); }, QN);

    // rows: an incidence row applied to a Q block
    auto rowSel = [] (const std::vector<double>& Qb, int cols, int a, int b, int c)
    {
        double s = 0.0;
        if (a >= 0) s += Qb[(size_t) (a * cols + c)];
        if (b >= 0) s -= Qb[(size_t) (b * cols + c)];
        return s;
    };

    A.assign ((size_t) (nx * nx), 0.0);
    B.assign ((size_t) (nx * nu), 0.0);
    Cm.assign ((size_t) (nx * M), 0.0);
    for (int r = 0; r < nx; ++r)
    {
        const auto& e = elems[(size_t) reactive[(size_t) r]];
        const double g2 = (1.0 + beta) * Gx[(size_t) r];
        for (int c = 0; c < nx; ++c)
            A[(size_t) (r * nx + c)] = (r == c ? dx[(size_t) r] : 0.0)
                                     - g2 * rowSel (QX, nx, e.n[0], e.n[1], c) * cx[(size_t) c];
        for (int c = 0; c < nu; ++c) B[(size_t) (r * nu + c)]  =  g2 * rowSel (QU, nu, e.n[0], e.n[1], c);
        for (int c = 0; c < M; ++c)  Cm[(size_t) (r * M + c)]  = -g2 * rowSel (QN, M,  e.n[0], e.n[1], c);
    }

    D.assign ((size_t) (M * nx), 0.0);
    E.assign ((size_t) (M * nu), 0.0);
    K.assign ((size_t) (M * M), 0.0);
    for (int r = 0; r < M; ++r)
    {
        int a = -1, b = -1; portNodes (r, a, b);
        for (int c = 0; c < nx; ++c) D[(size_t) (r * nx + c)] = -rowSel (QX, nx, a, b, c) * cx[(size_t) c];
        for (int c = 0; c < nu; ++c) E[(size_t) (r * nu + c)] =  rowSel (QU, nu, a, b, c);
        for (int c = 0; c < M; ++c)  K[(size_t) (r * M + c)]  = -rowSel (QN, M,  a, b, c);
    }

    Do.assign ((size_t) (no * nx), 0.0);
    Eo.assign ((size_t) (no * nu), 0.0);
    Fo.assign ((size_t) (no * M), 0.0);
    for (int r = 0; r < no; ++r)
    {
        const auto& o = outs[(size_t) r];
        const int row = (o.source >= 0) ? branchOf[(size_t) o.source] : o.node;
        if (row < 0) continue;                      // ground reads zero
        for (int c = 0; c < nx; ++c) Do[(size_t) (r * nx + c)] = -QX[(size_t) (row * nx + c)] * cx[(size_t) c];
        for (int c = 0; c < nu; ++c) Eo[(size_t) (r * nu + c)] =  QU[(size_t) (row * nu + c)];
        for (int c = 0; c < M; ++c)  Fo[(size_t) (r * M + c)]  = -QN[(size_t) (row * M + c)];
    }

    /*  The same matrices fused and stored column by column, for process():
        one pass of column updates (no reductions, so they vectorise without
        reassociating anything) produces p, y and the next x at once. Rows are
        [p (M) | y (no) | x' (nx)]. The fixed sources are folded into a
        constant, so only the caller's inputs are multiplied per sample. */
    R = M + no + nx;
    Wx.assign ((size_t) (R * nx), 0.0);
    Wu.assign ((size_t) (R * numIn), 0.0);
    Wi.assign ((size_t) ((no + nx) * M), 0.0);
    cst.assign ((size_t) R, 0.0);
    acc.assign ((size_t) R, 0.0);
    for (int c = 0; c < nx; ++c)
    {
        double* col = &Wx[(size_t) (c * R)];
        for (int r = 0; r < M; ++r)  col[r]          = D[(size_t) (r * nx + c)];
        for (int r = 0; r < no; ++r) col[M + r]      = Do[(size_t) (r * nx + c)];
        for (int r = 0; r < nx; ++r) col[M + no + r] = A[(size_t) (r * nx + c)];
    }
    for (int k = 0; k < nu; ++k)
    {
        const auto& e = elems[(size_t) sources[(size_t) k]];
        double* col = (e.input >= 0) ? &Wu[(size_t) (e.input * R)] : cst.data();
        const double scale = (e.input >= 0) ? 1.0 : e.value;
        for (int r = 0; r < M; ++r)  col[r]          += scale * E[(size_t) (r * nu + k)];
        for (int r = 0; r < no; ++r) col[M + r]      += scale * Eo[(size_t) (r * nu + k)];
        for (int r = 0; r < nx; ++r) col[M + no + r] += scale * B[(size_t) (r * nu + k)];
    }
    for (int c = 0; c < M; ++c)
    {
        double* col = &Wi[(size_t) (c * (no + nx))];
        for (int r = 0; r < no; ++r) col[r]      = Fo[(size_t) (r * M + c)];
        for (int r = 0; r < nx; ++r) col[no + r] = Cm[(size_t) (r * M + c)];
    }

    dirty = false;
    built = true;
}

//==============================================================================
void Circuit::deviceEval (const double* v, double* i, double* J) const
{
    // off-block entries are zero from build() and nothing ever writes them
    double Jl[9];
    for (size_t t = 0; t < tubes.size(); ++t)
    {
        const auto& m = *elems[(size_t) tubes[t]].model;
        const int f = portOfTube[t];
        const int np = m.pentode ? 3 : 2;
        tubeCurrents (m, v + f, i + f, Jl);
        for (int r = 0; r < np; ++r)
            for (int c = 0; c < np; ++c)
                J[(f + r) * M + (f + c)] = Jl[r * np + c];
    }
}

/*  v = p + K i(v), by Newton from the predicted solution.

    Damped: a step that makes the residual worse is halved until it does not,
    and is capped in size besides - SPICE's junction limiting by another name.
    Without it, an output valve driven down into its knee while the transformer
    swings the other plate to twice the supply was measured flinging Newton to
    a kilovolt and never back. If the iteration budget runs out, the best
    answer seen is kept, not the last. */
bool Circuit::newton (const double* p, const double* Km, int maxIter, double tol)
{
    double* v = vPort.data();
    double* i = iPort.data();

    auto residual = [&] (const double* vv, double* ii, double* JJ, double* rr)
    {
        deviceEval (vv, ii, JJ);
        double worst = 0.0;
        for (int r = 0; r < M; ++r)
        {
            double s = p[r] - vv[r];
            for (int c = 0; c < M; ++c) s += Km[r * M + c] * ii[c];
            rr[r] = s;
            // written out, because std::max (worst, NaN) is worst: a NaN
            // residual would read as converged
            const double a = std::abs (s);
            if (! (a <= worst)) worst = a;
        }
        return worst;
    };

    double norm = residual (v, i, Jd.data(), rhs.data());
    double bestNorm = norm;
    vBest.assign (v, v + M);

    int it = 0;
    bool ok = norm < tol;
    while (! ok && it < maxIter)
    {
        ++it;
        // K * Jd - I, with Jd block diagonal: only each tube's own ports
        for (size_t t = 0; t < tubes.size(); ++t)
        {
            const int f = portOfTube[t], np = portCount[t];
            for (int r = 0; r < M; ++r)
                for (int c = f; c < f + np; ++c)
                {
                    double s = (r == c) ? -1.0 : 0.0;
                    for (int k = f; k < f + np; ++k) s += Km[r * M + k] * Jd[(size_t) (k * M + c)];
                    Jac[(size_t) (r * M + c)] = s;
                }
        }
        jacValid = factorPorts();
        if (! jacValid) break;
        for (int r = 0; r < M; ++r) step[(size_t) r] = -rhs[(size_t) r];
        solvePorts (step.data());

        double big = 0.0;
        for (int r = 0; r < M; ++r) big = std::max (big, std::abs (step[(size_t) r]));
        double lambda = (big > 200.0) ? 200.0 / big : 1.0;

        double trialNorm = norm;
        for (int h = 0; h < 8; ++h)
        {
            for (int r = 0; r < M; ++r) vTrial[(size_t) r] = v[r] + lambda * step[(size_t) r];
            trialNorm = residual (vTrial.data(), iTrial.data(), JdTrial.data(), rTrial.data());
            if (trialNorm < norm || h == 7) break;
            lambda *= 0.5;
        }
        std::copy (vTrial.begin(), vTrial.end(), v);
        std::copy (iTrial.begin(), iTrial.end(), i);
        std::swap (Jd, JdTrial);
        std::swap (rhs, rTrial);
        norm = trialNorm;

        if (norm < bestNorm) { bestNorm = norm; vBest.assign (v, v + M); }
        if (norm < tol || lambda * big < tol) ok = true;
    }

    worstIter = std::max (worstIter, it);
    totalIter += it; ++solves;
    if (! ok)
    {
        ++newtonFailures;
        if (bestNorm < norm)
        {
            std::copy (vBest.begin(), vBest.end(), v);
            residual (v, i, Jd.data(), rhs.data());
        }
    }
    return ok;
}

//==============================================================================
bool Circuit::solveDC (const double* in)
{
    if (dirty) build();

    std::vector<double> S ((size_t) (Ns * Ns));
    stamp (S, true);
    std::vector<int> P ((size_t) Ns);
    if (! luFactor (S.data(), Ns, P.data())) { err = "singular DC matrix"; return false; }

    auto solveCol = [&] (std::vector<double>& col) { luSolve (S.data(), Ns, P.data(), col.data()); };

    // port-space DC matrices
    std::vector<double> QUd ((size_t) (Ns * nu)), QNd ((size_t) (Ns * M));
    std::vector<double> col ((size_t) Ns);
    for (int c = 0; c < nu; ++c)
    {
        std::fill (col.begin(), col.end(), 0.0);
        col[(size_t) branchOf[(size_t) sources[(size_t) c]]] = 1.0;
        solveCol (col);
        for (int r = 0; r < Ns; ++r) QUd[(size_t) (r * nu + c)] = col[(size_t) r];
    }
    // the same port incidence build() uses
    std::vector<std::pair<int, int>> pn ((size_t) M);
    for (size_t t = 0; t < tubes.size(); ++t)
    {
        const auto& e = elems[(size_t) tubes[t]];
        const int f = portOfTube[t];
        if (e.model->pentode)
        {
            pn[(size_t) f]     = { e.n[2], e.n[3] };
            pn[(size_t) f + 1] = { e.n[1], e.n[3] };
            pn[(size_t) f + 2] = { e.n[0], e.n[3] };
        }
        else
        {
            pn[(size_t) f]     = { e.n[1], e.n[2] };
            pn[(size_t) f + 1] = { e.n[0], e.n[2] };
        }
    }
    for (int c = 0; c < M; ++c)
    {
        std::fill (col.begin(), col.end(), 0.0);
        if (pn[(size_t) c].first  >= 0) col[(size_t) pn[(size_t) c].first]  += 1.0;
        if (pn[(size_t) c].second >= 0) col[(size_t) pn[(size_t) c].second] -= 1.0;
        solveCol (col);
        for (int r = 0; r < Ns; ++r) QNd[(size_t) (r * M + c)] = col[(size_t) r];
    }
    auto sel = [&] (const std::vector<double>& Qb, int cols, int port, int c)
    {
        double s = 0.0;
        if (pn[(size_t) port].first  >= 0) s += Qb[(size_t) (pn[(size_t) port].first  * cols + c)];
        if (pn[(size_t) port].second >= 0) s -= Qb[(size_t) (pn[(size_t) port].second * cols + c)];
        return s;
    };
    std::vector<double> Ed ((size_t) (M * nu)), Kd ((size_t) (M * M));
    for (int r = 0; r < M; ++r)
    {
        for (int c = 0; c < nu; ++c) Ed[(size_t) (r * nu + c)] =  sel (QUd, nu, r, c);
        for (int c = 0; c < M; ++c)  Kd[(size_t) (r * M + c)]  = -sel (QNd, M, r, c);
    }

    // every source ramped up from nothing to its value, so Newton walks there
    std::vector<double> ud ((size_t) nu);
    std::fill (vPort.begin(), vPort.end(), 0.0);
    bool ok = true;
    constexpr int STEPS = 40;
    for (int s = 1; s <= STEPS && ok; ++s)
    {
        const double f = (double) s / STEPS;
        for (int c = 0; c < nu; ++c)
        {
            const auto& e = elems[(size_t) sources[(size_t) c]];
            ud[(size_t) c] = ((e.input >= 0) ? (in != nullptr ? in[e.input] : 0.0) : e.value) * f;
        }
        for (int r = 0; r < M; ++r)
        {
            double sum = 0.0;
            for (int c = 0; c < nu; ++c) sum += Ed[(size_t) (r * nu + c)] * ud[(size_t) c];
            pBuf[(size_t) r] = sum;
        }
        ok = newton (pBuf.data(), Kd.data(), 200, 1.0e-9);
    }
    if (! ok) { err = "DC operating point did not converge"; return false; }
    u = ud;
    havePrev = false;
    jacValid = false;       // that factorisation was the DC one

    // full node voltages, then each state set to rest at them
    std::vector<double> vf ((size_t) Ns, 0.0);
    for (int r = 0; r < Ns; ++r)
    {
        double s = 0.0;
        for (int c = 0; c < nu; ++c) s += QUd[(size_t) (r * nu + c)] * ud[(size_t) c];
        for (int c = 0; c < M; ++c)  s -= QNd[(size_t) (r * M + c)] * iPort[(size_t) c];
        vf[(size_t) r] = s;
    }
    auto vn = [&] (int n) { return n >= 0 ? vf[(size_t) n] : 0.0; };
    for (int r = 0; r < nx; ++r)
    {
        const auto& e = elems[(size_t) reactive[(size_t) r]];
        const double vr = vn (e.n[0]) - vn (e.n[1]);
        if (e.kind == Element::C) x[(size_t) r] = Gx[(size_t) r] * vr;
        else                      x[(size_t) r] = GSHORT * vr - Gx[(size_t) r] * vr;
    }
    return true;
}

//==============================================================================
void Circuit::process (const double* in, double* out)
{
    if (dirty) build();

    for (int c = 0; c < nu; ++c)
    {
        const auto& e = elems[(size_t) sources[(size_t) c]];
        u[(size_t) c] = (e.input >= 0) ? in[e.input] : e.value;
    }

    double* a = acc.data();
    std::copy (cst.begin(), cst.end(), a);
    for (int c = 0; c < nx; ++c)
    {
        const double xc = x[(size_t) c];
        const double* col = &Wx[(size_t) (c * R)];
        for (int r = 0; r < R; ++r) a[r] += col[r] * xc;
    }
    for (int k = 0; k < numIn; ++k)
    {
        const double uk = in[k];
        const double* col = &Wu[(size_t) (k * R)];
        for (int r = 0; r < R; ++r) a[r] += col[r] * uk;
    }
    const double* p = a;

    /*  Predict before solving. Linearised about the last answer,
        v = p + K i(v) moves by (I - K J)^-1 dp - and the factored (K J - I)
        from the last Newton step is still sitting in Jac, so the prediction
        is one back-substitution rather than a factorisation. It starts
        Newton most of the way there; without a usable factorisation (the
        first sample, or after a rebuild) the plain dp predictor stands in. */
    std::copy (vPort.begin(), vPort.end(), vStart.begin());
    if (havePrev)
    {
        for (int r = 0; r < M; ++r) step[(size_t) r] = p[r] - pPrev[(size_t) r];
        if (jacValid)
        {
            solvePorts (step.data());
            for (int r = 0; r < M; ++r) vPort[(size_t) r] -= step[(size_t) r];
        }
        else
            for (int r = 0; r < M; ++r) vPort[(size_t) r] += step[(size_t) r];
    }
    const bool hadPrev = havePrev;
    havePrev = true;

    if (! newton (p, K.data(), MAX_ITER, 1.0e-5) && hadPrev)
    {
        /*  Continuation. With the states fixed, the tubes' port voltages are
            a continuous function of p alone, so walking p from last sample's
            value to this one in small steps, each solved from the one before,
            follows the same solution branch to the same answer - where one
            leap from the prediction can land Newton nowhere. That is what an
            output stage switching over at full drive asks of it: ngspice gets
            through the same edge by cutting its timestep to nanoseconds.
            Rare, so its cost does not matter. */
        for (int S : { 4, 32, 256 })
        {
            std::copy (vStart.begin(), vStart.end(), vPort.begin());
            bool ok = true;
            for (int k = 1; k <= S && ok; ++k)
            {
                const double f = (double) k / S;
                for (int r = 0; r < M; ++r) pMid[(size_t) r] = pPrev[(size_t) r] + f * (p[r] - pPrev[(size_t) r]);
                ok = newton (pMid.data(), K.data(), MAX_ITER, 1.0e-5);
            }
            ++continuations;
            if (ok) { --newtonFailures; break; }    // the leap's failure is forgiven
        }
    }
    std::copy (p, p + M, pPrev.begin());

    double* rest = a + M;              // y, then x'
    const int nr = no + nx;
    for (int c = 0; c < M; ++c)
    {
        const double ic = iPort[(size_t) c];
        const double* col = &Wi[(size_t) (c * nr)];
        for (int r = 0; r < nr; ++r) rest[r] += col[r] * ic;
    }
    std::copy (rest, rest + no, out);
    std::copy (rest + no, rest + nr, x.begin());
}

double Circuit::nodeVoltage (int node)
{
    // From the states as they now stand: exact at rest, which is what the
    // bench asks it for (the operating point).
    if (node < 0 || dirty) return 0.0;
    double s = 0.0;
    for (int c = 0; c < nx; ++c) s -= Qfull_x[(size_t) (node * nx + c)] * cx[(size_t) c] * x[(size_t) c];
    for (int c = 0; c < nu; ++c) s += Qfull_u[(size_t) (node * nu + c)] * u[(size_t) c];
    for (int c = 0; c < M; ++c)  s -= Qfull_n[(size_t) (node * M + c)] * iPort[(size_t) c];
    return s;
}

} // namespace mm
