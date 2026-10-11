#pragma once
// Magneto Transport - record and replay equalisation (stage 4). docs/physics/07-playback.md.
//
// The replay amplifier follows a standard (IEC or NAB time constants per speed)
// plus an HF trim for the playback head's own spacing and gap losses; the record
// amplifier's pre-emphasis makes the tape's short-circuit flux follow the same
// standard. Both are realised as cascades of first-order shelves - minimum-phase,
// like the analogue circuits they stand for - fitted to a magnitude target.
// No JUCE in here.

namespace mt
{

/*  Replay time constants of a standard at a speed (log-interpolated between
    4.76 / 9.53 / 19.05 / 38.1 / 76.2 cm/s). w1 = 1 / tau1, 0 when there is none. */
void eqTimeConstants (int standard, double vCms, double& tau2, double& w1);

struct ShelfCascade
{
    static constexpr int NS = 12;
    // analogue: overall gain (dB) and one first-order shelf per corner
    double g0Db = 0.0;
    double fc[NS] {}, gDb[NS] {};
    // digital, at the rate it was discretised for
    double g0 = 1.0;
    double b0[NS] {}, b1[NS] {}, a1[NS] {};

    /*  Least-squares fit (in dB) to a magnitude target, n points. Returns the
        worst error of the fit, dB. */
    double fit (const double* fHz, const double* targetDb, int n, bool warm = false, double fMaxErr = 16000.0);
    void discretise (double fs);
    double analogueDb (double fHz) const;
};

struct ShelfState
{
    double x1[ShelfCascade::NS] {}, y1[ShelfCascade::NS] {};
    void reset() { for (int i = 0; i < ShelfCascade::NS; ++i) x1[i] = y1[i] = 0.0; }
    inline double tick (const ShelfCascade& c, double x)
    {
        double v = x * c.g0;
        for (int i = 0; i < ShelfCascade::NS; ++i)
        {
            const double y = c.b0[i] * v + c.b1[i] * x1[i] - c.a1[i] * y1[i];
            x1[i] = v; y1[i] = y; v = y;
        }
        return v;
    }
};

} // namespace mt
