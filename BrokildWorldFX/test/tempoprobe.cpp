/*  Reproduce Peter's report: "starting at 120 BPM everything is fine, but
    changing tempo in the DAW just creates a mess."

    The first version of this probe only called setBpm() and measured the
    SETTLED state, which told us almost nothing: a real host calls
    setTransport() every block with an advancing ppq, and the complaint is
    about the TRANSITION, not the destination. So this one behaves like a
    DAW — ppq advances at whatever the current tempo is — and measures what
    comes out DURING and AFTER the change.  */
#include "../src/bwfx.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

using namespace bwfx;

static int typeByName (const char* id)
{
    for (int t = 0; t < numModuleTypes(); ++t)
        if (std::string (moduleDescriptor (t).id) == id) return t;
    return -1;
}

/*  A host: ppq advances at the current tempo, reported every block, exactly
    as JUCE's AudioPlayHead does. `changeAt` seconds in, the tempo changes. */
struct Host
{
    double fs, bpm, ppq = 0.0;
    void run (Rack& r, float* L, float* R, int n, int block, double newBpm, int changeAt)
    {
        int done = 0;
        while (done < n)
        {
            const int m = std::min (block, n - done);
            if (changeAt >= 0 && done <= changeAt && done + m > changeAt) bpm = newBpm;
            r.setTransport (bpm, ppq, true);
            r.process (L + done, R + done, m);
            ppq += (double) m * bpm / (60.0 * fs);
            done += m;
        }
    }
};

static std::vector<int> peaks (const float* L, int from, int to, float rel)
{
    float mx = 0;
    for (int i = from; i < to; ++i) mx = std::max (mx, std::fabs (L[i]));
    const float th = mx * rel;
    std::vector<int> pk;
    for (int i = from + 1; i < to - 1; ++i)
    {
        const float a = std::fabs (L[i]);
        if (a < th) continue;
        if (a >= std::fabs (L[i - 1]) && a > std::fabs (L[i + 1]))
        {
            if (! pk.empty() && i - pk.back() < 2000)
            { if (a > std::fabs (L[pk.back()])) pk.back() = i; }
            else pk.push_back (i);
        }
    }
    return pk;
}

static void gapsFrom (const char* tag, const float* L, int from, int to,
                      double fs, double want)
{
    auto pk = peaks (L, from, to, 0.12f);
    std::printf ("   %-22s want %.4f s :", tag, want);
    for (size_t i = 1; i < pk.size() && i <= 8; ++i)
        std::printf (" %.4f", (double) (pk[i] - pk[i - 1]) / fs);
    if (pk.size() < 2) std::printf (" (no echo train found)");
    std::printf ("\n");
}

int main()
{
    const double fs = 48000.0;
    const int tDelay = typeByName ("delay");
    const int tKier  = typeByName ("kieranator");

    std::printf ("=== 1. SYNCED ECHO, with a host clock, across a tempo change ===\n");
    std::printf ("1/4 note: 120 -> 0.5000 s | 90 -> 0.6667 | 150 -> 0.4000 | 121 -> 0.4959\n\n");

    struct Case { double to; const char* name; };
    const Case cases[] = { { 90.0, "120 -> 90" }, { 150.0, "120 -> 150" },
                           { 121.0, "120 -> 121" } };
    for (const auto& c : cases)
    {
        Rack r;
        r.prepare (fs, 512);
        r.setEnabled (tDelay, true);
        r.setParam (tDelay, 0, 100.0f);      // full wet
        r.setParam (tDelay, 2, 86.0f);       // long train so it survives the window
        r.setParam (tDelay, 5, 4.0f);        // 1/4
        r.setParam (tDelay, 6, 0.0f);        // straight

        const int N = (int) (fs * 12);
        std::vector<float> L (N, 0.0f), R (N, 0.0f);
        L[2400] = 1.0f; R[2400] = 1.0f;      // ONE click; everything after is echo

        Host h { fs, 120.0 };
        const int changeAt = (int) (fs * 3);
        h.run (r, L.data(), R.data(), N, 512, c.to, changeAt);

        std::printf ("%s\n", c.name);
        gapsFrom ("before the change", L.data(), 2400, changeAt, fs, 0.5);
        gapsFrom ("across the change", L.data(), changeAt, changeAt + (int) (fs * 3), fs, 60.0 / c.to);
        gapsFrom ("well after", L.data(), changeAt + (int) (fs * 3), N, fs, 60.0 / c.to);
        std::printf ("\n");
    }

    /*  2. Is the transition CLEAN or a mess? A delay whose time glides
        pitch-shifts everything in the line; a delay that snaps splices. The
        complaint is about what the change SOUNDS like, so measure the worst
        sample-to-sample step around it against the settled chain's own. */
    std::printf ("=== 2. WHAT THE CHANGE SOUNDS LIKE (worst sample step) ===\n");
    {
        auto worstStep = [&] (double to, int at, const char* tag)
        {
            Rack r;
            r.prepare (fs, 512);
            r.setEnabled (tDelay, true);
            r.setParam (tDelay, 0, 60.0f);
            r.setParam (tDelay, 2, 70.0f);
            r.setParam (tDelay, 5, 4.0f);
            const int N = (int) (fs * 8);
            std::vector<float> L (N), R (N);
            for (int i = 0; i < N; ++i)      // a steady tone, so a splice shows
            { const float s = 0.3f * (float) std::sin (2.0 * 3.14159265 * 220.0 * i / fs);
              L[(size_t) i] = s; R[(size_t) i] = s; }
            Host h { fs, 120.0 };
            h.run (r, L.data(), R.data(), N, 512, to, at);
            auto step = [&] (int a, int b) {
                float mx = 0;
                for (int i = a + 1; i < b; ++i) mx = std::max (mx, std::fabs (L[i] - L[i - 1]));
                return mx;
            };
            const float settled = step ((int) (fs * 1.5), (int) (fs * 2.5));
            const float around  = step (at - 2400, at + (int) (fs * 1.0));
            std::printf ("   %-14s settled step %.4f, around the change %.4f  -> %s\n",
                         tag, (double) settled, (double) around,
                         around > settled * 3.0f ? "*** DISCONTINUITY ***" : "smooth");
        };
        worstStep (90.0,  (int) (fs * 4), "120 -> 90");
        worstStep (150.0, (int) (fs * 4), "120 -> 150");
        worstStep (121.0, (int) (fs * 4), "120 -> 121");
    }

    /*  ...and the other synced modules, which set a RATE rather than moving a
        read pointer through a buffer full of audio. If those are clean, the
        fault is the delay alone and not the sync machinery. */
    std::printf ("\n=== 2b. THE OTHER SYNCED MODULES, same change ===\n");
    {
        const char* ids[] = { "stutter", "harmonic" };
        for (const char* id : ids)
        {
            const int t = typeByName (id);
            if (t < 0) { std::printf ("   %s: not found\n", id); continue; }
            Rack r;
            r.prepare (fs, 512);
            r.setEnabled (t, true);
            r.setParam (t, 0, 100.0f);
            const int N = (int) (fs * 8);
            std::vector<float> L (N), R (N);
            for (int i = 0; i < N; ++i)
            { const float v = 0.3f * (float) std::sin (2.0 * 3.14159265 * 220.0 * i / fs);
              L[(size_t) i] = v; R[(size_t) i] = v; }
            Host h { fs, 120.0 };
            const int at = (int) (fs * 4);
            h.run (r, L.data(), R.data(), N, 512, 90.0, at);
            auto step = [&] (int a, int b) {
                float mx = 0;
                for (int i = a + 1; i < b; ++i) mx = std::max (mx, std::fabs (L[i] - L[i - 1]));
                return mx; };
            const float settled = step ((int) (fs * 1.5), (int) (fs * 3.5));
            const float around  = step (at - 2400, at + (int) (fs * 1.0));
            std::printf ("   %-14s settled step %.4f, around the change %.4f  -> %s\n",
                         id, (double) settled, (double) around,
                         around > settled * 3.0f ? "*** DISCONTINUITY ***" : "smooth");
        }
    }

    /*  3. THE KIERANATOR runs off the bar, not off a delay time: its step
        grid comes from ppq. Ask whether its steps still land on the beat
        after the tempo moves. A stutter that has lost the grid is exactly
        what "a mess" sounds like. */
    std::printf ("\n=== 3. KIERANATOR STEP GRID ACROSS A TEMPO CHANGE ===\n");
    if (tKier < 0) std::printf ("   (kieranator not found)\n");
    else
    {
        auto gridRun = [&] (double to, const char* tag)
        {
            Rack r;
            r.prepare (fs, 512);
            r.setEnabled (tKier, true);
            r.setParam (tKier, 0, 100.0f);              // full wet
            r.setExtra (tKier, "7777777777777777");     // GATE on every step
            r.setParam (tKier, 7, 50.0f);               // 50% duty, so it chops
            const int N = (int) (fs * 16);
            std::vector<float> L (N), R (N);
            for (int i = 0; i < N; ++i)
            { const float s = 0.3f * (float) std::sin (2.0 * 3.14159265 * 330.0 * i / fs);
              L[(size_t) i] = s; R[(size_t) i] = s; }
            Host h { fs, 120.0 };
            h.run (r, L.data(), R.data(), N, 512, to, (int) (fs * 6));
            //  where does the output change character? envelope edges
            std::vector<int> edges;
            float prev = 0;
            for (int i = 1000; i < N; i += 64)
            {
                float e = 0;
                for (int j = i - 1000; j < i; ++j) e = std::max (e, std::fabs (L[j]));
                if (prev > 1e-4f && (e > prev * 2.5f || e < prev * 0.4f)) edges.push_back (i);
                prev = e;
            }
            std::printf ("   %-12s %d envelope edges; first six after the change:",
                         tag, (int) edges.size());
            int shown = 0;
            for (int e : edges) if (e > (int) (fs * 6) && shown < 6)
            { std::printf (" %.2fs", (double) e / fs); ++shown; }
            std::printf ("\n");
        };
        gridRun (120.0, "no change");
        gridRun (90.0,  "120 -> 90");
        gridRun (150.0, "120 -> 150");
    }
    return 0;
}
