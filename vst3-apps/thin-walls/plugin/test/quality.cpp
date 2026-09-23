/*  THIN WALLS - sound-quality measurements.

    Peter's question: "is there a frequency roll off, that would not be there in
    the real room?"  This answers it by measuring, not by reasoning.

    The method: render the DIRECT PATH ALONE (absorbing hall, doors shut, early
    and reverb trimmed to -80 dB) and compare its magnitude spectrum with the
    response it is SUPPOSED to have - the same HRTF the engine looked up, times
    1/r, times ISO 9613-1 air absorption. The ratio, normalised at 1 kHz, is
    everything the processing added or lost. Anything but a flat line there is
    an artefact.

    Then the parts that could bend it, each isolated:
      - the fractional delay interpolation, swept across a whole sample
      - the four-anchor band filter above its top anchor
      - the head's own data (where the KEMAR measurement itself runs out)
      - the three host rates
      - the bottom end, where a stray DC blocker would show

    Prints numbers; asserts only the few bounds that would be faults.
*/
#include "Engine.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <complex>
#include <algorithm>
#include <cmath>
#include <memory>

using namespace tw;

static int failures = 0;
static void note (bool ok, const char* what, double a = 0)
{
    if (! ok) ++failures;
    std::printf ("  %s  %s", ok ? "ok  " : "FAULT", what);
    if (a != 0) std::printf ("   [%.3g]", a);
    std::printf ("\n");
}

//------------------------------------------------------------------------------
static void fft (std::vector<std::complex<double>>& a, bool inverse)
{
    const int n = (int) a.size();
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[(size_t) i], a[(size_t) j]);
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = 2 * 3.14159265358979 / len * (inverse ? 1 : -1);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (int i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (int k = 0; k < len / 2; ++k)
            {
                const auto u = a[(size_t) (i + k)], v = a[(size_t) (i + k + len / 2)] * w;
                a[(size_t) (i + k)] = u + v;
                a[(size_t) (i + k + len / 2)] = u - v;
                w *= wl;
            }
        }
    }
    if (inverse) for (auto& x : a) x /= n;
}

static const int NFFT = 8192;

// magnitude spectrum of a real signal, in dB, NFFT/2+1 bins
static std::vector<double> spectrum (const float* x, int n)
{
    std::vector<std::complex<double>> a ((size_t) NFFT, { 0.0, 0.0 });
    for (int i = 0; i < std::min (n, NFFT); ++i) a[(size_t) i] = { (double) x[i], 0.0 };
    fft (a, false);
    std::vector<double> m ((size_t) (NFFT / 2 + 1));
    for (int k = 0; k <= NFFT / 2; ++k) m[(size_t) k] = std::abs (a[(size_t) k]);
    return m;
}

static double binOf (double hz, double fs) { return hz / fs * NFFT; }

// dB at a frequency, averaged over a third octave so a single null cannot speak
static double dbAt (const std::vector<double>& mag, double hz, double fs)
{
    const double lo = binOf (hz / 1.122, fs), hi = binOf (hz * 1.122, fs);
    int a = std::max (1, (int) std::floor (lo)), b = std::min ((int) mag.size() - 1, (int) std::ceil (hi));
    if (b < a) { a = b = std::max (1, (int) std::lround (binOf (hz, fs))); }
    double s = 0; int n = 0;
    for (int k = a; k <= b; ++k) { s += mag[(size_t) k] * mag[(size_t) k]; ++n; }
    return 10.0 * std::log10 (std::max (s / std::max (1, n), 1e-30));
}

//------------------------------------------------------------------------------
// the engine rendering ONE impulse through the direct path only
static std::vector<float> directImpulse (double fs, float distance, int* firstOut = nullptr)
{
    // tw::Engine is far larger than Windows 1 MB default stack (96 path slots of
    // 384-tap arrays): a stack local crashes with 0xC00000FD before a line prints,
    // which git-bash reports as a bare exit 127. The bench heap-allocates for the
    // same reason.
    auto ep = std::make_unique<Engine>(); Engine& e = *ep; e.prepare (fs, 256);
    Params p;
    p.material[0] = p.material[1] = p.material[2] = 0;            // absorbing
    p.door[0] = p.door[1] = p.door[2] = 0;
    p.src[0].type = SRC_PURE; p.src[0].directivity = 0;
    p.lisX = 9.0f; p.lisY = 4.5f; p.lisYaw = 0.0f;                // facing +x
    p.src[0].x = 9.0f + distance; p.src[0].y = 4.5f; p.src[0].z = EAR_HEIGHT;
    p.earlyDb = -80; p.reverbDb = -80;
    e.setParams (p);

    const int n = (int) (0.4 * fs);
    std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
    const int at = 2400;
    for (int i = 0; i < n; i += 128)
    {
        const int m = std::min (128, n - i);
        for (int k = 0; k < m; ++k) { const float v = (i + k == at) ? 1.0f : 0.0f; L[(size_t) (i + k)] = v; R[(size_t) (i + k)] = v; }
        e.process (&L[(size_t) i], &R[(size_t) i], m);
    }
    int first = 0;
    for (int i = 0; i < n; ++i) if (std::abs (L[(size_t) i]) > 1e-5f) { first = i; break; }
    if (firstOut) *firstOut = first;
    // the window from the arrival, long enough for the whole head response
    std::vector<float> w ((size_t) NFFT, 0.0f);
    for (int i = 0; i < NFFT && first - 8 + i < n; ++i) w[(size_t) i] = L[(size_t) (first - 8 + i)];
    return w;
}

// the reference: the very taps the engine looked up, with the air term
static std::vector<float> referenceImpulse (double fs, float distance)
{
    Hrtf h; h.prepare (fs);
    std::vector<float> l ((size_t) h.numTaps()), r ((size_t) h.numTaps());
    float itd = 0;
    h.lookup (0.0f, 0.0f, l.data(), r.data(), itd);
    // air absorption over the distance, as the engine's own band filter would fit it
    float db[NBAND];
    for (int b = 0; b < NBAND; ++b) db[b] = -Engine::airDbPerMetre (b) * distance;
    BandFilter f; f.setCoeffs ((float) fs); f.setBandsDb (db); f.reset();
    std::vector<float> w ((size_t) NFFT, 0.0f);
    for (int i = 0; i < h.numTaps(); ++i) w[(size_t) i] = f.process (l[(size_t) i]);
    return w;
}

//------------------------------------------------------------------------------
int main()
{
    std::printf ("THIN WALLS quality - is anything rolling off that the room would not?\n");

    const double RATES[3] = { 44100.0, 48000.0, 96000.0 };
    const double F[11] = { 20, 31.5, 63, 125, 250, 1000, 4000, 8000, 12000, 16000, 20000 };

    // ---- 1. the direct path against the response it should have ------------------
    for (double fs : RATES)
    {
        std::printf ("\n1. direct path vs its own HRTF, %.0f Hz (deviation, dB, 0 = exact)\n     ", fs);
        for (double f : F) if (f < fs * 0.47) std::printf ("%8.0f", f);
        std::printf ("\n");

        const float dist = 2.0f;
        auto got = directImpulse (fs, dist);
        auto want = referenceImpulse (fs, dist);
        auto mg = spectrum (got.data(), NFFT), mw = spectrum (want.data(), NFFT);
        const double ref = dbAt (mg, 1000, fs) - dbAt (mw, 1000, fs);

        double worst = 0; double worstHz = 0;
        std::printf ("     ");
        for (double f : F)
        {
            if (f >= fs * 0.47) continue;
            const double d = (dbAt (mg, f, fs) - dbAt (mw, f, fs)) - ref;
            std::printf ("%8.2f", d);
            if (f <= 20000 && std::abs (d) > std::abs (worst)) { worst = d; worstHz = f; }
        }
        std::printf ("\n");
        char buf[160];
        std::snprintf (buf, sizeof buf, "%.0f Hz: worst deviation from the physics %+.2f dB at %.0f Hz", fs, worst, worstHz);
        note (std::abs (worst) < 1.0, buf);
    }

    // ---- 2. the fractional delay, swept across one whole sample ------------------
    {
        const double fs = 48000.0;
        std::printf ("\n2. fractional delay: HF loss as the source moves across one sample (%.0f Hz)\n", fs);
        std::printf ("     frac     8k      12k     16k     20k\n");
        double worst = 0;
        const float base = 2.0f;
        const float step = (float) (SPEED_OF_SOUND / fs);       // one sample of travel
        for (int i = 0; i < 4; ++i)
        {
            const float d = base + step * (float) i / 4.0f;
            auto got = directImpulse (fs, d);
            auto want = referenceImpulse (fs, d);
            auto mg = spectrum (got.data(), NFFT), mw = spectrum (want.data(), NFFT);
            const double ref = dbAt (mg, 1000, fs) - dbAt (mw, 1000, fs);
            std::printf ("     %.2f ", (double) i / 4.0);
            for (double f : { 8000.0, 12000.0, 16000.0, 20000.0 })
            {
                const double dev = (dbAt (mg, f, fs) - dbAt (mw, f, fs)) - ref;
                std::printf ("%8.2f", dev);
                if (std::abs (dev) > std::abs (worst)) worst = dev;
            }
            std::printf ("\n");
        }
        char buf[160];
        std::snprintf (buf, sizeof buf, "interpolation costs at most %+.2f dB at 20 kHz over a whole sample of delay", worst);
        note (std::abs (worst) < 1.5, buf);
    }

    // ---- 2b. the ITD read, on a source hard to one side --------------------------
    {
        const double fs = 48000.0;
        std::printf ("\n2b. the ITD interpolation: a source at 90 degrees, swept across one sample\n");
        std::printf ("     frac     8k      12k     16k     20k    (the FAR ear, which carries the ITD)\n");
        double worst = 0;
        const float step = (float) (SPEED_OF_SOUND / fs);
        for (int i = 0; i < 4; ++i)
        {
            const float d = 2.0f + step * (float) i / 4.0f;
            auto ep = std::make_unique<Engine>(); Engine& e = *ep; e.prepare (fs, 256);
            Params p;
            p.material[0] = p.material[1] = p.material[2] = 0;
            p.door[0] = p.door[1] = p.door[2] = 0;
            p.src[0].type = SRC_PURE; p.src[0].directivity = 0;
            p.lisX = 9.0f; p.lisY = 4.5f; p.lisYaw = 90.0f;                    // facing north
            p.src[0].x = 9.0f + d; p.src[0].y = 4.5f; p.src[0].z = EAR_HEIGHT; // due east: 90 deg to the right
            p.earlyDb = -80; p.reverbDb = -80;
            e.setParams (p);
            const int n = (int) (0.4 * fs);
            std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
            for (int k = 0; k < n; k += 128)
            {
                const int m = std::min (128, n - k);
                for (int q = 0; q < m; ++q) { const float v = (k + q == 2400) ? 1.0f : 0.0f; L[(size_t) (k + q)] = v; R[(size_t) (k + q)] = v; }
                e.process (&L[(size_t) k], &R[(size_t) k], m);
            }
            int first = 0;
            for (int k = 0; k < n; ++k) if (std::abs (L[(size_t) k]) > 1e-6f) { first = k; break; }
            std::vector<float> w ((size_t) NFFT, 0.0f);
            for (int k = 0; k < NFFT && first - 8 + k < n; ++k) w[(size_t) k] = L[(size_t) (first - 8 + k)];
            Hrtf h; h.prepare (fs);
            std::vector<float> hl ((size_t) h.numTaps()), hr ((size_t) h.numTaps());
            float itd = 0; h.lookup (90.0f, 0.0f, hl.data(), hr.data(), itd);
            std::vector<float> ref ((size_t) NFFT, 0.0f);
            for (int k = 0; k < h.numTaps(); ++k) ref[(size_t) k] = hl[(size_t) k];
            auto mg = spectrum (w.data(), NFFT), mw = spectrum (ref.data(), NFFT);
            const double r1k = dbAt (mg, 1000, fs) - dbAt (mw, 1000, fs);
            std::printf ("     %.2f ", (double) i / 4.0);
            for (double f : { 8000.0, 12000.0, 16000.0, 20000.0 })
            {
                const double dev = (dbAt (mg, f, fs) - dbAt (mw, f, fs)) - r1k;
                std::printf ("%8.2f", dev);
                if (std::abs (dev) > std::abs (worst)) worst = dev;
            }
            std::printf ("\n");
        }
        char buf[180];
        std::snprintf (buf, sizeof buf, "the far ear of a 90-degree source stays within %+.2f dB of its own HRTF up to 20 kHz", worst);
        note (std::abs (worst) < 1.5, buf);
    }

    // ---- 3. the band filter above its top anchor ---------------------------------
    {
        const double fs = 48000.0;
        std::printf ("\n3. the four-anchor loss filter: asked vs delivered (dB)\n");
        // a hard case: a tiled wall's absorption, six bounces
        float target[NBAND];
        for (int b = 0; b < NBAND; ++b) target[b] = 6.0f * 10.0f * std::log10 (1.0f - MATERIAL_ALPHA[3][b]);
        BandFilter f; f.setCoeffs ((float) fs); f.setBandsDb (target); f.reset();
        std::vector<float> imp ((size_t) NFFT, 0.0f); imp[0] = 1.0f;
        for (int i = 0; i < NFFT; ++i) imp[(size_t) i] = f.process (imp[(size_t) i]);
        auto m = spectrum (imp.data(), NFFT);
        std::printf ("     freq     250     1k      4k      8k      12k     16k     20k\n     asked ");
        for (int b : { 1, 3, 5, 6 }) std::printf ("%8.2f", target[b]);
        std::printf ("      -       -       -\n     got   ");
        double worstAnchor = 0;
        for (int i = 0; i < 4; ++i)
        {
            static const double HZ[4] = { 250, 1000, 4000, 8000 };
            static const int BI[4] = { 1, 3, 5, 6 };
            const double hz = HZ[i];
            const int b = BI[i];
            const double got = dbAt (m, hz, fs);
            std::printf ("%8.2f", got);
            worstAnchor = std::max (worstAnchor, std::abs (got - target[b]));
        }
        for (double hz : { 12000.0, 16000.0, 20000.0 }) std::printf ("%8.2f", dbAt (m, hz, fs));
        std::printf ("\n");
        std::printf ("     (above 8 kHz the top shelf carries on; the number above 8k is an extrapolation, not a measurement)\n");
        char buf[160];
        std::snprintf (buf, sizeof buf, "the filter hits its four anchors to %.2f dB", worstAnchor);
        note (worstAnchor < 0.3, buf);
    }

    // ---- 4. the head's own data ---------------------------------------------------
    {
        std::printf ("\n4. the KEMAR data itself (front, 0 deg) - where the measurement runs out\n");
        for (double fs : RATES)
        {
            Hrtf h; h.prepare (fs);
            std::vector<float> l ((size_t) h.numTaps()), r ((size_t) h.numTaps());
            float itd = 0; h.lookup (0.0f, 0.0f, l.data(), r.data(), itd);
            std::vector<float> w ((size_t) NFFT, 0.0f);
            for (int i = 0; i < h.numTaps(); ++i) w[(size_t) i] = l[(size_t) i];
            auto m = spectrum (w.data(), NFFT);
            const double at1k = dbAt (m, 1000, fs);
            std::printf ("     %.0f Hz, %3d taps: ", fs, h.numTaps());
            for (double f : { 8000.0, 12000.0, 16000.0, 20000.0, 22000.0 })
                if (f < fs * 0.47) std::printf (" %.0fk %+.1f ", f / 1000, dbAt (m, f, fs) - at1k);
            std::printf ("\n");
        }
        std::printf ("     (relative to 1 kHz; this is the head and the 1994 measurement, not the processing)\n");
    }

    // ---- 5. the bottom end ---------------------------------------------------------
    {
        const double fs = 48000.0;
        std::printf ("\n5. the bottom end (a stray high-pass would show here)\n");
        auto got = directImpulse (fs, 2.0f);
        auto want = referenceImpulse (fs, 2.0f);
        auto mg = spectrum (got.data(), NFFT), mw = spectrum (want.data(), NFFT);
        const double ref = dbAt (mg, 1000, fs) - dbAt (mw, 1000, fs);
        double worst = 0;
        std::printf ("     ");
        for (double f : { 10.0, 20.0, 31.5, 63.0, 125.0 })
        {
            const double d = (dbAt (mg, f, fs) - dbAt (mw, f, fs)) - ref;
            std::printf (" %.0f Hz %+.2f ", f, d);
            if (std::abs (d) > std::abs (worst)) worst = d;
        }
        std::printf ("\n");
        char buf[160];
        std::snprintf (buf, sizeof buf, "nothing touches the bottom: worst %+.2f dB down to 10 Hz", worst);
        note (std::abs (worst) < 0.5, buf);
    }

    // ---- 6. the whole thing, room and all -------------------------------------------
    {
        const double fs = 48000.0;
        std::printf ("\n6. the full render (furnished room, everything on) against a flat input\n");
        // tw::Engine is far larger than Windows 1 MB default stack (96 path slots of
    // 384-tap arrays): a stack local crashes with 0xC00000FD before a line prints,
    // which git-bash reports as a bare exit 127. The bench heap-allocates for the
    // same reason.
    auto ep = std::make_unique<Engine>(); Engine& e = *ep; e.prepare (fs, 256);
        Params p;                       // factory defaults
        e.setParams (p);
        const int n = (int) (3.0 * fs);
        std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
        uint32_t s = 1;
        for (int i = 0; i < n; ++i) { s = s * 1664525u + 1013904223u; const float v = ((s >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.25f; L[(size_t) i] = R[(size_t) i] = v; }
        std::vector<float> in = L;
        for (int i = 0; i < n; i += 256) e.process (&L[(size_t) i], &R[(size_t) i], std::min (256, n - i));
        // average spectra over the settled second
        std::vector<double> mi ((size_t) (NFFT / 2 + 1), 0.0), mo (mi.size(), 0.0);
        int blocks = 0;
        for (int off = (int) (1.0 * fs); off + NFFT < n; off += NFFT)
        {
            auto a = spectrum (in.data() + off, NFFT), b = spectrum (L.data() + off, NFFT);
            for (size_t k = 0; k < mi.size(); ++k) { mi[k] += a[k] * a[k]; mo[k] += b[k] * b[k]; }
            ++blocks;
        }
        for (size_t k = 0; k < mi.size(); ++k) { mi[k] = std::sqrt (mi[k] / blocks); mo[k] = std::sqrt (mo[k] / blocks); }
        const double ref = dbAt (mo, 1000, fs) - dbAt (mi, 1000, fs);
        std::printf ("     transfer, relative to 1 kHz (this one SHOULD tilt: it is a room and a head)\n     ");
        for (double f : { 31.5, 63.0, 125.0, 250.0, 1000.0, 4000.0, 8000.0, 12000.0, 16000.0, 20000.0 })
            std::printf (" %.0f %+.1f ", f, (dbAt (mo, f, fs) - dbAt (mi, f, fs)) - ref);
        std::printf ("\n");
    }

    // ---- 7. how dense is the late field ------------------------------------------
    {
        const double fs = 48000.0;
        std::printf ("\n7. the late field's density - is it a room or a pipe?\n");
        std::printf ("     room  material     RT60   L_total  need   ripple dB   echo density at 50/100/200 ms\n");
        std::printf ("                                               (5.57 = diffuse)   (1.00 = merged)\n");

        for (int r = 0; r < NUM_ROOMS; ++r)
            for (int m : { 4, 1, 2 })                       // STUDIO, FURNISHED, PLASTER
            {
                auto ep = std::make_unique<Engine>(); Engine& e = *ep; e.prepare (fs, 256);
                Params pp;
                pp.material[0] = pp.material[1] = pp.material[2] = m;
                pp.door[0] = pp.door[1] = pp.door[2] = 0;
                pp.src[0].type = SRC_PURE;
                const Room& R = ROOMS[r];
                pp.src[0].x = R.x0 + 0.3f * (R.x1 - R.x0); pp.src[0].y = R.y0 + 0.35f * (R.y1 - R.y0); pp.src[0].z = EAR_HEIGHT;
                pp.lisX = R.x0 + 0.7f * (R.x1 - R.x0); pp.lisY = R.y0 + 0.6f * (R.y1 - R.y0);
                pp.directDb = -120; pp.earlyDb = -120;      // the late field alone
                e.setParams (pp);

                const int n = (int) (4.0 * fs);
                std::vector<float> L ((size_t) n, 0.0f), Rr ((size_t) n, 0.0f);
                for (int i = 0; i < n; i += 128)
                {
                    const int mm = std::min (128, n - i);
                    for (int k = 0; k < mm; ++k) { const float v = (i + k == 2400) ? 1.0f : 0.0f; L[(size_t) (i + k)] = v; Rr[(size_t) (i + k)] = v; }
                    e.process (&L[(size_t) i], &Rr[(size_t) i], mm);
                }

                // the total loop delay the network actually has: every line AND
                // the allpasses inside it, because an allpass's length counts
                // towards the modal density exactly as a delay's does
                double Ltot = 0;
                for (int k = 0; k < RoomField::N; ++k)
                    Ltot += (double) e.field (r).len[(size_t) k] + e.field (r).apTotal[(size_t) k];
                Ltot /= fs;
                float rt[NBAND]; e.roomRt60 (r, rt);
                const double need = rt[3] / 2.2;

                /*  Spectral ripple over 400 Hz - 2 kHz. The decay is divided back
                    out first (multiply by exp(+13.8 t / RT60)) so the window holds
                    a stationary signal: without that, a window long enough to
                    resolve modes a few hertz apart is also long enough to span the
                    whole decay, and what comes back is the envelope, not the modes.
                    Hann windowed, starting once the field is established. */
                const int from = 2400 + (int) (0.04 * fs);
                std::vector<std::complex<double>> a ((size_t) NFFT, { 0.0, 0.0 });
                const double tau60 = 13.8 / std::max (0.05f, rt[3]);
                for (int i = 0; i < NFFT && from + i < n; ++i)
                {
                    const double t = (double) i / fs;
                    const double comp = std::exp (tau60 * t * 0.5);        // amplitude, not power
                    const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * i / (NFFT - 1));
                    a[(size_t) i] = { (double) L[(size_t) (from + i)] * comp * win, 0.0 };
                }
                fft (a, false);
                double mean = 0; int cnt = 0;
                const int k0 = (int) (400.0 / fs * NFFT), k1 = (int) (2000.0 / fs * NFFT);
                std::vector<double> bins;
                for (int k = k0; k <= k1; ++k)
                {
                    const double d = 20.0 * std::log10 (std::max (std::abs (a[(size_t) k]), 1e-30));
                    bins.push_back (d); mean += d; ++cnt;
                }
                mean /= std::max (1, cnt);
                double var = 0; for (double d : bins) var += (d - mean) * (d - mean);
                const double ripple = std::sqrt (var / std::max ((size_t) 1, bins.size()));

                // Abel-Huang normalised echo density
                auto echoDensity = [&] (double at)
                {
                    const int c = 2400 + (int) (at * fs);
                    const int half = (int) (0.010 * fs);        // a 20 ms window
                    if (c + half >= n) return 0.0;
                    double sd = 0; int nn = 0;
                    for (int i = c - half; i < c + half; ++i) { sd += (double) L[(size_t) i] * L[(size_t) i]; ++nn; }
                    sd = std::sqrt (sd / std::max (1, nn));
                    if (sd <= 0) return 0.0;
                    int over = 0;
                    for (int i = c - half; i < c + half; ++i) if (std::abs (L[(size_t) i]) > sd) ++over;
                    return (double) over / (double) nn / 0.3173;
                };

                std::printf ("     %-5s %-11s %5.2f  %5.0fms %5.0fms   %6.2f     %.2f  %.2f  %.2f\n",
                             ROOMS[r].name, MATERIAL_NAMES[m], rt[3], Ltot * 1000, need * 1000,
                             ripple, echoDensity (0.05), echoDensity (0.10), echoDensity (0.20));
            }
        std::printf ("     a total loop delay below the \"need\" column means the network's own modes are\n"
                     "     resolvable, which is heard as pitched ringing rather than a decay.\n");
    }

    std::printf ("\n%s\n", failures == 0 ? "no artefact found beyond the stated bounds" : "SOMETHING IS BENDING THE RESPONSE");
    return failures == 0 ? 0 : 1;
}
