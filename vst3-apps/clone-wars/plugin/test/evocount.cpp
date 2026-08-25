#include "cw_core.h"
#include <cstdio>
int main() {
    // count evolvers per band by their fingerprint (long attacks + slow lfos)
    int per[5] = {0,0,0,0,0}, tot = 0;
    for (int n = 0; n < 100; ++n) {
        cw::Patch p;
        cw::generatePatch ((uint32_t) n, p);
        int slow = 0;
        for (int v = 0; v < cw::kVoices; ++v)
            if (p.voice[v][cw::vfAAtk] > 0.45f && p.voice[v][cw::vfLfoRate] < 0.2f) ++slow;
        if (slow >= 12) { ++per[n / 20]; ++tot; }
    }
    printf ("evolving: %d/100  by band: %d %d %d %d %d\n", tot, per[0], per[1], per[2], per[3], per[4]);
    return 0;
}
