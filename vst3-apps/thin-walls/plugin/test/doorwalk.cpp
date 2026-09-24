// doorwalk: walk the listener through an open doorway at a steady pace and
// measure what happens at the threshold. Not a gate - the probe for the
// "bump when crossing a doorway" report (2026-09-24).
//   doorwalk [part]   part = all | direct | early | reverb
// Input is noise low-passed at ~1.5 kHz, so energy above ~6 kHz in the output
// can only be an artefact (a discontinuity), never the signal.
#include "Engine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdlib>
using namespace tw;

int main (int argc, char** argv)
{
    const std::string part = argc > 1 ? argv[1] : "all";
    const double fs = 48000.0;
    auto e = std::make_unique<Engine>(); e->prepare (fs, 128);
    Params p;
    p.door[1] = 1.0f;                                   // LARGE-GIANT, x = 6, y 2.05..2.95
    p.src[0].x = 3.0f; p.src[0].y = 2.5f; p.src[0].yaw = 0;   // in LARGE, facing the door
    p.lisY = 2.5f; p.lisYaw = 0;
    // third argument "into": the source waits in GIANT, so the walk goes INTO its room
    if (argc > 3 && std::string (argv[3]) == "into") { p.src[0].x = 9.5f; p.src[0].yaw = 180; }
    if (part == "direct") { p.earlyDb = -120; p.reverbDb = -120; }
    if (part == "early")  { p.directDb = -120; p.reverbDb = -120; }
    if (part == "reverb") { p.directDb = -120; p.earlyDb = -120; }

    const double x0 = 4.5, x1 = 7.5, speed = 1.0;       // metres, m/s
    const int settle = (int) (2.0 * fs);
    const int walk = (int) ((x1 - x0) / speed * fs);
    const int n = settle + walk;
    std::vector<float> L ((size_t) n), R ((size_t) n);
    unsigned rng = argc > 2 ? (unsigned) atoi (argv[2]) : 12345u; float lp1 = 0, lp2 = 0, lp3 = 0, lp4 = 0;
    const float a = 1.0f - std::exp (-2.0f * 3.14159265f * 1500.0f / (float) fs);
    for (int i = 0; i < n; ++i)
    {
        rng = rng * 1664525u + 1013904223u;
        const float w = ((rng >> 8) / 8388608.0f - 1.0f) * 0.5f;
        lp1 += a * (w - lp1); lp2 += a * (lp1 - lp2); lp3 += a * (lp2 - lp3); lp4 += a * (lp3 - lp4);
        L[(size_t) i] = R[(size_t) i] = lp4 * 2.0f;
    }
    int crossAt = -1;
    for (int i = 0; i < n; i += 128)
    {
        const double t = (double) std::max (0, i - settle) / fs;
        p.lisX = (float) (x0 + speed * t);
        if (crossAt < 0 && p.lisX >= 6.0f) crossAt = i;
        e->setParams (p);
        e->process (&L[(size_t) i], &R[(size_t) i], std::min (128, n - i));
    }

    // per 10 ms window: level (dB) and HF artefact energy (second difference)
    const int W = 480;
    std::printf ("part %s, threshold crossed at t = %.3f s of the walk (x = 6.00 m)\n", part.c_str(), (crossAt - settle) / fs);
    std::printf ("   t(s)    x(m)   level dB   HF dB\n");
    double worstStep = 0, worstStepT = 0, baseStep = 0;
    double hfWorst = -200, hfWorstT = 0, hfBase = -200;
    double prevLev = 0; bool havePrev = false;
    for (int w0 = settle; w0 + W <= n; w0 += W)
    {
        double s = 0, h = 0;
        for (int i = w0; i < w0 + W; ++i)
        {
            s += 0.5 * (L[(size_t) i] * L[(size_t) i] + R[(size_t) i] * R[(size_t) i]);
            if (i >= 2)
            {
                const double dl = L[(size_t) i] - 2 * L[(size_t) i - 1] + L[(size_t) i - 2];
                const double dr = R[(size_t) i] - 2 * R[(size_t) i - 1] + R[(size_t) i - 2];
                h += 0.5 * (dl * dl + dr * dr);
            }
        }
        const double lev = 10 * std::log10 (s / W + 1e-20), hf = 10 * std::log10 (h / W + 1e-20);
        const double t = (w0 - settle) / fs;
        const bool nearDoor = std::abs (t - (crossAt - settle) / fs) < 0.15;
        if (havePrev)
        {
            const double st = std::abs (lev - prevLev);
            if (nearDoor) { if (st > worstStep) { worstStep = st; worstStepT = t; } }
            else baseStep = std::max (baseStep, st);
        }
        if (nearDoor) { if (hf > hfWorst) { hfWorst = hf; hfWorstT = t; } }
        else hfBase = std::max (hfBase, hf);
        prevLev = lev; havePrev = true;
        if (std::abs (t - (crossAt - settle) / fs) < 0.25)
            std::printf ("  %6.3f  %5.2f   %7.2f   %7.2f%s\n", t, x0 + speed * t, lev, hf, nearDoor ? "  *" : "");
    }
    std::printf ("level step per 10 ms: worst near the door %.2f dB (t %.3f), worst elsewhere %.2f dB\n", worstStep, worstStepT, baseStep);
    std::printf ("HF artefact: worst near the door %.1f dB (t %.3f), worst elsewhere %.1f dB\n", hfWorst, hfWorstT, hfBase);
    return 0;
}
