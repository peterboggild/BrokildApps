/*  MARMALADE - one netlist, solved as several smaller circuits

    Newton's cost grows with the cube of the number of device ports, and the
    matrix products with the square of the states: a whole amp as one block
    costs more than its parts. This splits ONE netlist into blocks at named
    resistors and solves the blocks one after another, every sample, in
    order. Nothing is rewritten by hand: the blocks are generated from the
    same parsed netlist the whole-circuit solver (and ngspice) checks.

    A cut resistor sits in both blocks. In each, its far end is a voltage
    source held at the far node's voltage: the voltage solved this sample if
    the far block has already run, the last sample's if not. So the signal's
    forward path through a cut carries no delay when the blocks run in
    signal order; only the loading back across it is a sample late. That is
    exact in the limit and very nearly so where it is used - a resistor of
    tens of kilohms into a node it barely moves within one sample (a valve's
    grid, a rail behind tens of microfarads). The bench measures how nearly,
    against the whole circuit solved as one.

    Cuts must be resistors; the blocks are what is left connected (ground
    aside) once they are taken out.
*/
#pragma once

#include "Circuit.h"

#include <string>
#include <vector>

namespace mm
{

class Partition
{
public:
    /*  `cuts`: the resistors to cut at. `order`: one node per block, the
        blocks in the order they are solved (each block named by any node
        in it); blocks not named run last. False and error() on failure. */
    bool build (const std::string& netlist, const std::vector<std::string>& cuts,
                const std::vector<std::string>& order);
    const std::string& error() const { return err; }

    int numBlocks() const { return (int) blocks.size(); }
    Circuit& block (int b) { return blocks[(size_t) b].c; }
    // each block's netlist, as generated
    const std::string& blockNetlist (int b) const { return blocks[(size_t) b].text; }

    int numInputs() const { return numIn; }
    int numOutputs() const { return (int) outNames.size(); }
    int outputIndex (const std::string& name) const;

    void setTheta (double t) { for (auto& b : blocks) b.c.setTheta (t); }
    void prepare (double sampleRate);
    void changeRate (double newRate);
    double sampleRate() const { return blocks.empty() ? 0.0 : blocks[0].c.sampleRate(); }

    /*  The operating point: each block's own DC solve, around the blocks in
        order until the voltages across every cut agree. */
    bool solveDC (const double* in = nullptr);

    /*  A part by name, in whichever block(s) hold it (a cut resistor, both). */
    bool setValue (const std::string& element, double value);
    double getValue (const std::string& element) const;
    void update() { for (auto& b : blocks) b.c.update(); }

    void process (const double* in, double* out);

    double nodeVoltage (const std::string& node);

    int failures() const;
    int maxIterations() const;
    void resetStats() { for (auto& b : blocks) b.c.resetStats(); }

private:
    struct Coupling { int localInput; int net; };          // a far node's voltage, in
    struct Block
    {
        Circuit c;
        std::string text;
        std::vector<int> globalIn;          // local input -> global input, or -1 for a coupling
        std::vector<Coupling> couplings;
        std::vector<std::pair<int, int>> publish;    // local output -> net it carries
        std::vector<std::pair<int, int>> outputs;    // local output -> global output
        std::vector<double> in, out;
    };

    std::vector<Block> blocks;
    std::vector<std::string> outNames;
    std::vector<double> net;               // the coupled nodes' latest voltages
    std::vector<std::string> nodeOwnerName;
    std::vector<int> nodeOwner;            // full-netlist node -> block
    int numIn = 0;
    std::string err;
};

} // namespace mm
