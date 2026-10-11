// Magneto Transport - the equaliser fits per coating, and where they miss (prints).
#include "Engine.h"
#include <cstdio>
using namespace mt;
int main()
{
    for (int k = 0; k < NUM_COATINGS; ++k)
        std::printf ("coating %d (%s): worst fit %.3f dB at %.2f cm/s\n", k, coatingTables (k).name, Engine::eqFitWorstDb (k), Engine::eqFitWorstSpeed (k));
    for (int k = 0; k < NUM_COATINGS; ++k)
        for (int s = 0; s < 2; ++s)
            for (int rec = 1; rec >= 0; --rec)
            {
                std::printf ("coating %d %s %s at 4.6 cm/s, fit - target (dB):\n  ", k, s ? "NAB" : "IEC", rec ? "record" : "replay");
                for (double f : { 15.0, 25.0, 40.0, 63.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 4000.0, 6000.0 })
                    std::printf ("%5.0f:%6.2f ", f, Engine::eqFitErrorDb (k, s, 0, f, rec != 0));
                std::printf ("\n");
            }
}
