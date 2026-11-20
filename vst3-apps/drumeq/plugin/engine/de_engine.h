#pragma once

// DRUMEQ - the engine. One drum microphone in (mono or stereo), the same out,
// with no latency:
//
//   INPUT trim, POLARITY
//   GATE        keyed on its own band-pass, hysteresis + hold, no look-ahead
//   HIGH-PASS / LOW-PASS   off, 12 or 24 dB/oct
//   EQ          four bands, each a magnitude-matched second-order section
//   COMPRESSOR  feed-forward, soft knee, key high-pass, program-dependent
//               release, parallel MIX
//   OUTPUT trim
//
// Every switch (polarity, a slope, a band's shape, EQ in, gate, compressor)
// CROSSFADES over a few milliseconds, and every knob is smoothed, so nothing
// the player touches clicks. With everything at its neutral value the output
// is the input, bit for bit (the bench memcmps it).
//
// Plain C++17, no JUCE: the bench and the plug-in run this same code.

#include "de_params.h"
#include "de_filters.h"

#include <atomic>
#include <vector>

namespace de
{
    class Engine
    {
    public:
        static constexpr int kMaxCh = 2;

        void prepare (double sampleRate, int maxBlock);
        void reset();

        //  in place; numCh 1 or 2
        void process (const Params&, float* const* ch, int numCh, int n);

        //  ---- meters (written by the audio thread, read anywhere) ---------
        float meterIn() const     { return mIn.load(); }       // peak, linear
        float meterOut() const    { return mOut.load(); }
        float meterGrDb() const   { return mGr.load(); }       // <= 0
        float meterGateDb() const { return mGate.load(); }     // <= 0, 0 = open

        //  ---- the panel's spectrum taps: mono, before and after ------------
        static constexpr int kRing = 8192;
        const float* ringPre() const  { return pre.data(); }
        const float* ringPost() const { return post.data(); }
        int ringPos() const { return ringW.load (std::memory_order_acquire); }

        //  ---- FIND DRUM: a capture of the input (after trim and polarity) --
        void armFind (bool on);
        bool findArmed() const { return findOn.load(); }
        int  findLength() const { return findLen.load (std::memory_order_acquire); }
        const float* findData() const { return findBuf.data(); }
        double sampleRate() const { return fs; }

    private:
        struct Smooth
        {
            double cur = 0, tgt = 0;
            void snap (double v) { cur = tgt = v; }
            bool step (double a)       // true while moving
            {
                if (cur == tgt) return false;
                cur = tgt + (cur - tgt) * a;
                if (std::fabs (cur - tgt) < 1e-7 * (1.0 + std::fabs (tgt))) cur = tgt;
                return true;
            }
        };

        //  a crossfade among a few paths; each weight walks linearly to its
        //  one-hot target
        template <int N> struct Fade
        {
            double w[N] = {};
            int target = 0;
            void snap (int t) { target = t; for (int i = 0; i < N; ++i) w[i] = i == t ? 1.0 : 0.0; }
            void tick (double step)
            {
                for (int i = 0; i < N; ++i)
                {
                    const double t = i == target ? 1.0 : 0.0;
                    if (w[i] < t) w[i] = std::fmin (t, w[i] + step);
                    else if (w[i] > t) w[i] = std::fmax (t, w[i] - step);
                }
            }
            bool settled() const { for (int i = 0; i < N; ++i) if (w[i] != (i == target ? 1.0 : 0.0)) return false; return true; }
            //  the weights along an S (a line's corners are kinks), summing to 1
            void shaped (double* out) const
            {
                double s = 0.0;
                for (int i = 0; i < N; ++i) { out[i] = w[i] * w[i] * (3.0 - 2.0 * w[i]); s += out[i]; }
                if (s > 0.0 && s != 1.0) for (int i = 0; i < N; ++i) out[i] /= s;
            }
        };

        void updateCoefs (bool force);

        double fs = 48000.0;
        bool fresh = true;

        //  smoothed values (dB, log2 Hz, log2 Q, plain)
        Smooth sIn, sOut, sMix, sMake;
        Smooth sHpf, sLpf;
        Smooth sF[4], sG[4], sQ[4];
        Smooth sKeyLo, sKeyHi, sSc;
        double aSmooth = 0.99;          // per sub-block
        double fadeStep = 0.001;        // per sample, 10 ms
        double polStep = 0.001;

        //  structure
        Fade<3> hpFade, lpFade;         // off, 12, 24
        Fade<2> t1Fade, t4Fade;         // shelf, bell
        double eqW = 1.0, gateW = 0.0, compW = 0.0, polS = 1.0;
        double eqT = 1.0, gateT = 0.0, compT = 0.0, polT = 1.0;

        //  coefficients (shared by both channels)
        Coefs hp12, hp24a, hp24b, lp12, lp24a, lp24b;
        Coefs band[4], bandAlt[4];      // band[0]/[3] = shelf; bandAlt[0]/[3] = bell
        Coefs keyHp, keyLp, scHp;
        double lastDesigned[16] = {};

        //  filter states, per channel
        struct Ch
        {
            Biquad hp12, hp24a, hp24b, lp12, lp24a, lp24b;
            Biquad band[4], bandAlt[4];
            Biquad keyHp, keyLp, scHp;
        } chs[kMaxCh];

        //  gate
        double gEnv = 0.0, gGain = 1.0, gHold = 0.0;
        bool gOpen = true;

        //  compressor
        double cY = 0.0, cSlow = 0.0;

        //  meters
        std::atomic<float> mIn { 0 }, mOut { 0 }, mGr { 0 }, mGate { 0 };

        //  spectrum rings
        std::vector<float> pre, post;
        std::atomic<int> ringW { 0 };

        //  FIND DRUM
        std::vector<float> findBuf;
        std::atomic<bool> findOn { false };
        std::atomic<int> findLen { 0 };

        Params cur;                     // the targets this block
    };
}
