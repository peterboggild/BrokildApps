/*  MARMALADE 50 - where the spring model's one calibrated number comes from

    SpringTank::SENSITIVITY (output-coil volts per input-coil volt) is the one
    value in the amp set to a target rather than read off a sheet or a
    datasheet: the tank on the user's amp has not been measured. The target:
    with the REVERB pot at noon, the reverb reaching the phase splitter's
    mixing node carries about 8 dB less energy than the dry signal there, on
    a played phrase, preamp 2 at gain 3. This tool measures that ratio for the
    current constant and prints the constant that would hit the target
    exactly. The ratio scales as SENSITIVITY^2, because the recovery stage is
    small-signal and the mixing node is resistive.

        mm50springcal [reverb 0..1, default 0.5] [target dB, default -8]
*/
#include "Engine50.h"
#include "SpringTank.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

using namespace mm;

namespace
{
    // a plucked phrase: decaying partials, a few notes, gaps for the tail to show
    std::vector<float> phrase (double sr, double seconds)
    {
        std::vector<float> x ((size_t) (seconds * sr), 0.0f);
        const double notes[] = { 82.41, 110.0, 146.83, 196.0, 164.81, 123.47 };
        for (int k = 0; k < 6; ++k)
        {
            const size_t start = (size_t) ((0.1 + 0.55 * k) * sr);
            for (size_t i = 0; start + i < x.size() && i < (size_t) (1.2 * sr); ++i)
            {
                const double t = i / sr;
                double s = 0;
                for (int h = 1; h <= 8; ++h) s += std::sin (2 * 3.14159265358979 * notes[k] * h * t) / h;
                x[start + i] += (float) (0.12 * s * std::exp (-t * 3.0));
            }
        }
        return x;
    }

    std::vector<double> run (bool withTank, float reverb, double sr, const std::vector<float>& in)
    {
        Engine50 e;
        Params50 p;
        p.channel = 1; p.gain2 = 0.3f; p.volume2 = 0.5f; p.reverb = reverb; p.reverbOn = true;
        e.setParams (p);
        auto tank = std::make_shared<SpringTank>();
        tank->prepare (sr, Engine50::CHUNK);
        if (withTank) e.setTank ([tank] (float* io, int n) { tank->process (io, n); });
        else          e.setTank ([] (float* io, int n) { for (int i = 0; i < n; ++i) io[i] = 0.0f; });
        e.prepare (sr, 512);
        std::vector<double> v;
        std::vector<float> buf (1);
        for (float s : in)
        {
            buf[0] = s;
            e.process (buf.data(), nullptr, 1);
            v.push_back (e.circuit().nodeVoltage ("mx2"));
        }
        return v;
    }
}

int main (int argc, char** argv)
{
    const float reverb = argc > 1 ? (float) std::atof (argv[1]) : 0.5f;
    const double target = argc > 2 ? std::atof (argv[2]) : -8.0;
    const double sr = 48000.0;
    const auto in = phrase (sr, 5.0);

    const auto dry = run (false, reverb, sr, in);
    const auto wet = run (true, reverb, sr, in);

    double mD = 0; for (double v : dry) mD += v; mD /= dry.size();
    double eD = 0, eW = 0;
    for (size_t i = 0; i < dry.size(); ++i) { const double d = dry[i] - mD; eD += d * d; const double w = wet[i] - dry[i]; eW += w * w; }
    const double ratio = 10 * std::log10 (eW / eD);
    const double need = SpringTank::SENSITIVITY * std::pow (10.0, (target - ratio) / 20.0);
    std::printf ("REVERB %.2f: reverb at the splitter mix %.1f dB re dry (SENSITIVITY %.4f)\n", reverb, ratio, SpringTank::SENSITIVITY);
    std::printf ("for %.1f dB: SENSITIVITY %.4f\n", target, need);
    return 0;
}
