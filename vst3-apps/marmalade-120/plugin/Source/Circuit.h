/*  MARMALADE 120 - the circuit solver

    A netlist in, a sample-by-sample solver out. Nothing in here knows it is a
    guitar amp: it reads a small SPICE-like netlist, stamps it into a modified
    nodal analysis (MNA) system, discretises every capacitor and inductor with
    the trapezoidal rule and reduces the lot to the nonlinear state-space form
    of the DK method (Yeh; Holters & Zoelzer):

        p    = D x + E u                  (linear part seen by the tubes)
        v    = p + K i(v)                 (solved by Newton, M unknowns)
        x'   = A x + B u + C i            (capacitor and inductor states)
        y    = Do x + Eo u + Fo i         (whatever the caller asked to read)

    so the per-sample cost is a few small matrix products and a Newton solve
    only as wide as the device ports - 2 per triode, 3 per pentode, 1 per
    diode - rather than
    the whole circuit. Changing a resistor or capacitor (a pot, a switch)
    rebuilds the matrices; the states carry over.

    Plain C++17, double throughout, no JUCE: the bench drives it directly and
    exports the same netlist to ngspice to check it.

    Netlist lines (case-insensitive element letter, SPICE value suffixes):
        R<name> a b value
        C<name> a b value
        L<name> a b value
        V<name> + - DC value          a fixed source
        V<name> + - IN index          a source the caller sets per sample
        T<name> a b c d ratio         ideal transformer, V(a,b) = ratio*V(c,d)
        X<name> p g k MODEL           triode  (12AX7)
        X<name> p g2 g1 k MODEL       pentode (EL34, EL34X2 = two in parallel)
        D<name> a k MODEL             diode (1N4007)
        .out name node                read a node voltage
        .out name I(Vname)            read a source's current (+ to - inside)
    Node "0" or "gnd" is ground. A tube brings its own interelectrode
    capacitances with it, added as ordinary capacitors.
*/
#pragma once

#include <string>
#include <vector>
#include <cmath>

namespace mm
{

//==============================================================================
/*  The nonlinear devices, each with its published model and its source.

    TRIODE   Dempwolf & Zoelzer, "A physically-motivated triode model for
             circuit simulations" (DAFx 2011): cathode current
                 Ik = G (softplus(C (Vpk/mu + Vgk)) / C)^gamma
             grid current
                 Ig = Gg (softplus(Cg Vgk) / Cg)^xi + Ig0
             and the plate gets what the grid does not: Ip = Ik - Ig. Fitted
             by its authors to MEASURED curves of a real ECC83, grid current
             included - which is what an overdriven guitar stage lives on.
    PENTODE  Koren, "Improved vacuum-tube models for SPICE simulations"
             (Glass Audio 8(5), 1996), his published EL34 fit, plus a
             Dempwolf-form grid current.
    DIODE    Shockley, I = Is (exp(V / n Vt) - 1), with the published
             1N4007 SPICE parameters; its series resistance is an R in the
             netlist, where it can be seen.

    Capacitances are per tube, from the datasheets. */
struct DeviceModel
{
    enum Kind { TRIODE, PENTODE, DIODE } kind;
    const char* name;
    const char* source;                 // where the numbers come from

    // TRIODE: G, C, mu, gamma (cathode) and Gg, Cg, xi, Ig0 (grid)
    double G = 0, C = 0, mu = 0, gamma = 0;
    // PENTODE: Koren
    double ex = 0, kg1 = 0, kg2 = 0, kp = 0, kvb = 0;
    // grid current, both tube kinds
    double Gg = 0, Cg = 0, xi = 0, Ig0 = 0;
    // DIODE
    double Is = 0, n = 0;

    double mult = 1.0;                  // devices in parallel
    double cgk = 0, cgp = 0, cpk = 0;   // per device; pentode: g1-k, g1-p, p-k

    int ports() const { return kind == PENTODE ? 3 : kind == TRIODE ? 2 : 1; }
};

const DeviceModel* findDeviceModel (const std::string& name);

/*  Port currents and their Jacobian (row = current, column = voltage).
    Triode:  0 = grid-cathode, 1 = plate-cathode.
    Pentode: 0 = g1-cathode, 1 = g2-cathode, 2 = plate-cathode.
    Diode:   0 = anode-cathode. */
void deviceCurrents (const DeviceModel& m, const double* v, double* i, double* J);

//==============================================================================
struct Element
{
    enum Kind { R, C, L, V, T, X } kind = R;      // X: any nonlinear device, D lines included
    std::string name;
    int n[4] { -1, -1, -1, -1 };
    double value = 0.0;          // R, C, L: the part; V: the DC value; T: ratio
    int input = -1;              // V only: which caller input drives it, or -1
    const DeviceModel* model = nullptr;
};

struct OutputSpec
{
    std::string name;
    int node = -1;               // a node voltage, or
    int source = -1;             // a source's current (element index)
};

//==============================================================================
class Circuit
{
public:
    /*  Returns false and fills error() on anything it does not understand.
        Nothing is half-built: a failed parse leaves the circuit empty. */
    bool parse (const std::string& text);
    const std::string& error() const { return err; }

    /*  The sample rate fixes the trapezoidal conductances, so it is part of
        building the matrices. */
    void prepare (double sampleRate);

    /*  The integrator: 0.5 is the trapezoidal rule, higher damps. The
        trapezoidal rule never loses energy, so a fast transient in a stiff
        part of the circuit (an output stage switching over across its
        transformer) rings at Nyquist forever - measured flinging the EL34
        plates past a kilovolt and Newton off the map. ngspice's own
        trapezoidal refused the same circuit; its Gear did not. A little above
        0.5 kills the ringing (it decays by (1-theta)/theta per sample) and
        stays within a fraction of a dB of the trapezoidal rule through the
        audio band. Set before prepare(). */
    void setTheta (double t) { theta = t; dirty = true; }

    /*  Rebuild for another sample rate, carrying every capacitor's voltage and
        current and every inductor's current across: the circuit does not
        notice. For a cheap warm-up at a low rate. */
    void changeRate (double newRate);
    double sampleRate() const { return fs; }
    double getTheta() const { return theta; }

    /*  Solve the DC operating point (capacitors open, inductors shorted, the
        supplies ramped up from zero) and set every state to it, so the first
        sample is a warm amp rather than one being switched on. `in` holds the
        inputs' values to rest at (the signal at zero, a rail at its voltage);
        null means all zero. */
    bool solveDC (const double* in = nullptr);

    int numInputs() const { return numIn; }
    int numOutputs() const { return (int) outs.size(); }
    int outputIndex (const std::string& name) const;
    int elementIndex (const std::string& name) const;
    int nodeIndex (const std::string& name) const;

    /*  Change a part's value. The matrices are rebuilt on the next update()
        (or process()), so several changes cost one rebuild. */
    void setValue (int element, double value);
    double getValue (int element) const { return elems[(size_t) element].value; }
    void update() { if (dirty) build(); }

    /*  One sample. `in` holds numInputs() values, `out` numOutputs(). */
    void process (const double* in, double* out);

    /*  Newton's worst case over the run so far, for the bench. */
    int maxIterations() const { return worstIter; }
    int failures() const { return newtonFailures; }
    double meanIterations() const { return solves > 0 ? (double) totalIter / (double) solves : 0.0; }
    long long continuationCount() const { return continuations; }
    void resetStats() { worstIter = 0; newtonFailures = 0; totalIter = 0; solves = 0; continuations = 0; }

    // the parsed netlist, read by the ngspice exporter
    const std::vector<Element>& elements() const { return elems; }
    const std::vector<std::string>& nodeNames() const { return nodes; }
    const std::vector<OutputSpec>& outputs() const { return outs; }

    int numStates() const { return nx; }
    int numPorts() const { return M; }
    int systemSize() const { return Ns; }

    // the most recent full node solution, for the bench's operating point
    double nodeVoltage (int node);
    double portVoltage (int p) const { return vPort[(size_t) p]; }
    // a capacitor's or inductor's current (a to b) at the last solved sample
    double reactiveCurrent (int element);

private:
    int addNode (const std::string& name);
    void build();
    void stamp (std::vector<double>& S, bool dc) const;
    bool newton (const double* p, const double* Kmat, int maxIter, double tol);
    void deviceEval (const double* v, double* i, double* Jd) const;
    bool factorPorts();
    void solvePorts (double* b) const;

    std::vector<Element> elems;
    std::vector<std::string> nodes;
    std::vector<OutputSpec> outs;
    std::string err;

    // index maps
    std::vector<int> branchOf;           // element -> extra MNA row, or -1
    std::vector<int> reactive;           // element indices of C and L
    std::vector<int> tubes;              // element indices of X (every nonlinear device)
    std::vector<int> portA, portB;       // each port's two nodes
    std::vector<int> portOfTube;         // first port of each tube
    std::vector<int> portCount;          // and how many it has
    std::vector<int> sources;            // element indices of V, in u order

    int N = 0, Ns = 0, nx = 0, M = 0, nu = 0, numIn = 0, no = 0;
    double fs = 48000.0;
    double theta = 0.55;
    bool dirty = true, built = false;

    // DK matrices, row-major
    std::vector<double> A, B, Cm, D, E, K, Do, Eo, Fo;
    std::vector<double> Gx, cx, dx;      // per state: conductance, and the two signs
    std::vector<double> Wx, Wu, Wi, cst, acc;   // the fused, column-major form process() runs
    int R = 0;
    std::vector<double> x, xPrev, xNew, u, vPort, iPort, pBuf, Jd, Jac, rhs;
    std::vector<double> step, vTrial, iTrial, JdTrial, rTrial, vBest;
    std::vector<int> piv;

    // the full node solution is only rebuilt when asked for
    std::vector<double> Qfull_x, Qfull_u, Qfull_n;

    int worstIter = 0, newtonFailures = 0;
    long long totalIter = 0, solves = 0;
    std::vector<double> pPrev;           // last sample's p, for the predictor
    std::vector<double> pMid, vStart;    // for continuation
    bool havePrev = false, jacValid = false;
    long long continuations = 0;
};

//==============================================================================
// small dense helpers, shared with the bench
bool luFactor (double* a, int n, int* piv);
void luSolve (const double* lu, int n, const int* piv, double* b);

} // namespace mm
