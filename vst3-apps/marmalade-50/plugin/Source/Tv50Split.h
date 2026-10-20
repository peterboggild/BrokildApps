/*  MARMALADE 50 - where the circuit is cut into blocks (see Partition.h)

    Three blocks, solved in signal order every sample:
      preamp      both channels (V10, V9, V8), the loop's send valve
                  (V7-A, its plate on rail D) and the D, E, F rails  14 ports
      loop        the loop's return (V7-B) and the reverb driver (V6-A)  4 ports
      power       the reverb recovery (V6-B), the phase splitter (V5),
                  the EL34s, the output transformer and speaker, the
                  supply                                              15 ports
    against 33 ports as one block.

    A cut costs the loading back across it one sample of lag, which matters
    in proportion to the impedance of the node on the near side against the
    cut resistor. So every cut has a stiff node on the near side:
      R5     the send valve's cathode follower (a few hundred ohms) into 82k
      R17    rail D, behind 47u, into V7-B's plate
      R24    rail F, behind 22u, into V6-B's plate
      R34    rail C into rail D, 4k7 between 100u and 47u
      R1     rail C, behind 100u, into the reverb transformer
      R8     the loop's output (tens of kilohms) into the splitter's 1M mix
    The volume wipers (R3, R4) were tried as cuts and are NOT: a wiper is
    as high an impedance as the 220k behind it, and the lag showed at
    -36 dB. test/splitcheck.cpp measures the split against the whole circuit.
*/
#pragma once

#include <string>
#include <vector>

namespace mm
{

static const std::vector<std::string> TV50_CUTS =
{
    "r5", "r17", "r24", "r34", "r1", "r8",
};

// one node in each block, in the order they are solved
static const std::vector<std::string> TV50_ORDER = { "g10a", "g7b", "g5a" };

} // namespace mm
