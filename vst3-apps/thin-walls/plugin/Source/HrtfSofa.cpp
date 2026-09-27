/*  THIN WALLS - a personal head from a SOFA file.

    libmysofa reads the file (AES69 SimpleFreeFieldHRIR), resamples it to the
    engine's rate and interpolates any direction. This file samples it onto the
    engine's own elevation rings (-40 .. 90 degrees, 10 apart) around the WHOLE
    circle - a real head is not symmetric - and diffuse-field equalises the set
    the way the built-in MIT KEMAR set was: every response is divided by the
    RMS magnitude over all directions (per ear), a zero-phase correction, so a
    personal set keeps its own timing and its own ITD and differs from the
    built-in one only by the head it measured.

    Coordinates: SOFA x front, y LEFT, z up; the engine's azimuth is clockwise
    (90 = right), so y = -cos(el) sin(az).
*/
#include "Engine.h"
#include "HrtfData.h"
#include <mysofa.h>
#include <complex>
#include <cmath>
#include <vector>
#include <algorithm>

namespace tw
{

namespace
{
    // an in-place radix-2 FFT, enough for a 1024-point spectrum of each response
    void fft (std::vector<std::complex<double>>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = 2.0 * 3.14159265358979323846 / (double) len * (inverse ? 1.0 : -1.0);
            const std::complex<double> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0, 0.0);
                for (size_t j = 0; j < len / 2; ++j)
                {
                    const auto u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v; a[i + j + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
        if (inverse) for (auto& x : a) x /= (double) n;
    }

    const char* sofaError (int e)
    {
        switch (e)
        {
            case MYSOFA_INVALID_FORMAT:        return "not a SOFA file this reader understands";
            case MYSOFA_UNSUPPORTED_FORMAT:    return "a SOFA convention other than SimpleFreeFieldHRIR";
            case MYSOFA_NO_MEMORY:             return "out of memory";
            case MYSOFA_READ_ERROR:            return "the file could not be read";
            case MYSOFA_INVALID_ATTRIBUTES:    return "the file's attributes are invalid";
            case MYSOFA_INVALID_DIMENSIONS:    return "the file's dimensions are invalid";
            case MYSOFA_INVALID_DIMENSION_LIST:return "the file's dimension list is invalid";
            case MYSOFA_INVALID_COORDINATE_TYPE: return "an unsupported coordinate type";
            case MYSOFA_ONLY_EMITTER_WITH_ECI_SUPPORTED: return "only one emitter is supported";
            case MYSOFA_ONLY_DELAYS_WITH_IR_OR_MR_SUPPORTED: return "unsupported delay layout";
            case MYSOFA_ONLY_THE_SAME_SAMPLING_RATE_SUPPORTED: return "mixed sampling rates";
            case MYSOFA_RECEIVERS_WITH_RCI_SUPPORTED: return "unsupported receiver layout";
            case MYSOFA_RECEIVERS_WITH_CARTESIAN_SUPPORTED: return "unsupported receiver coordinates";
            case MYSOFA_INVALID_RECEIVER_POSITIONS: return "the two ears are not where they should be";
            case MYSOFA_ONLY_SOURCES_WITH_MC_SUPPORTED: return "unsupported source layout";
            default: return "the file could not be opened";
        }
    }
}

bool Hrtf::loadSofa (const std::string& file, double sampleRate, std::string& error)
{
    int filterLength = 0, err = 0;
    MYSOFA_EASY* e = mysofa_open (file.c_str(), (float) sampleRate, &filterLength, &err);
    if (e == nullptr || err != MYSOFA_OK || filterLength <= 0)
    {
        error = sofaError (err);
        if (e != nullptr) mysofa_close (e);
        return false;
    }

    const int nt = std::max (32, std::min (MAX_TAPS - 96, filterLength));
    // the rings: the built-in elevations, whole circles at the built-in spacing
    std::vector<int> rn ((size_t) hrtfdata::NRING), rf ((size_t) hrtfdata::NRING);
    int ndir = 0;
    for (int r = 0; r < hrtfdata::NRING; ++r)
    {
        rn[(size_t) r] = hrtfdata::RING_N[r] == 1 ? 1 : 2 * (hrtfdata::RING_N[r] - 1);
        rf[(size_t) r] = ndir;
        ndir += rn[(size_t) r];
    }

    std::vector<float> t ((size_t) ndir * 2 * (size_t) nt, 0.0f), id ((size_t) ndir, 0.0f);
    std::vector<float> L ((size_t) filterLength), R ((size_t) filterLength);
    const double d2r = 3.14159265358979323846 / 180.0;
    for (int r = 0; r < hrtfdata::NRING; ++r)
    {
        const double el = hrtfdata::RING_ELEV[r] * d2r;
        for (int a = 0; a < rn[(size_t) r]; ++a)
        {
            const double az = (360.0 / rn[(size_t) r]) * a * d2r;
            const float x = (float) (std::cos (el) * std::cos (az));
            const float y = (float) (-std::cos (el) * std::sin (az));
            const float z = (float) std::sin (el);
            float dl = 0, dr = 0;
            mysofa_getfilter_float (e, x, y, z, L.data(), R.data(), &dl, &dr);
            const int d = rf[(size_t) r] + a;
            float* tl = &t[((size_t) d * 2) * (size_t) nt];
            float* tr = tl + nt;
            for (int i = 0; i < nt; ++i) { tl[i] = L[(size_t) i]; tr[i] = R[(size_t) i]; }
            if (filterLength > nt)      // a short raised-cosine fade where the response is cut
                for (int i = 0; i < 24; ++i)
                {
                    const float w = 0.5f * (1.0f + std::cos (3.14159265f * (float) (i + 1) / 25.0f));
                    tl[nt - 24 + i] *= w; tr[nt - 24 + i] *= w;
                }
            // delays the file states separately (seconds), positive = right ear later
            id[(size_t) d] = (float) ((dr - dl) * sampleRate);
        }
    }
    mysofa_close (e);

    // diffuse-field equalisation, per ear: |H| / RMS over directions, zero phase
    const size_t N = 1024;
    for (int ear = 0; ear < 2; ++ear)
    {
        std::vector<double> power (N / 2 + 1, 0.0);
        std::vector<std::complex<double>> buf (N);
        for (int d = 0; d < ndir; ++d)
        {
            const float* h = &t[((size_t) d * 2 + (size_t) ear) * (size_t) nt];
            std::fill (buf.begin(), buf.end(), std::complex<double> (0.0, 0.0));
            for (int i = 0; i < nt; ++i) buf[(size_t) i] = h[i];
            fft (buf, false);
            for (size_t k = 0; k <= N / 2; ++k) power[k] += std::norm (buf[k]);
        }
        std::vector<double> gain (N / 2 + 1);
        for (size_t k = 0; k <= N / 2; ++k)
        {
            const double rms = std::sqrt (power[k] / ndir);
            gain[k] = 1.0 / std::max (rms, 1.0e-4);
        }
        // tame the extremes: no more than +/- 20 dB of correction anywhere
        for (auto& g : gain) g = std::max (0.1, std::min (10.0, g));
        for (int d = 0; d < ndir; ++d)
        {
            float* h = &t[((size_t) d * 2 + (size_t) ear) * (size_t) nt];
            std::fill (buf.begin(), buf.end(), std::complex<double> (0.0, 0.0));
            for (int i = 0; i < nt; ++i) buf[(size_t) i] = h[i];
            fft (buf, false);
            for (size_t k = 0; k <= N / 2; ++k) { buf[k] *= gain[k]; if (k > 0 && k < N / 2) buf[N - k] = std::conj (buf[k]); }
            fft (buf, true);
            for (int i = 0; i < nt; ++i) h[i] = (float) buf[(size_t) i].real();
        }
    }

    fs = sampleRate; ntap = nt; taps = std::move (t); itd = std::move (id);
    ringN = std::move (rn); ringFirst = std::move (rf);
    full = true; path = file;
    const size_t slash = file.find_last_of ("/\\");
    label = slash == std::string::npos ? file : file.substr (slash + 1);
    return true;
}

} // namespace tw
