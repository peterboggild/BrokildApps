/*  Thirty Thousand Years — offline renders for the demos.

        ttyrender <outdir>                    the four demonstrations
        ttyrender <outdir> preset <i> <sec>   one preset, drone on, a chord

    Writes 24-bit stereo WAVs at 48 kHz. Each demo carries its event
    timeline in the file name's companion .txt so it can be reproduced.
*/
#include "Engine.h"
#include "Presets.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>

using namespace tty;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    std::ofstream f (path, std::ios::binary);
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 2 * 3;
    auto w32 = [&] (uint32_t v) { f.put ((char) (v & 255)); f.put ((char) ((v >> 8) & 255)); f.put ((char) ((v >> 16) & 255)); f.put ((char) ((v >> 24) & 255)); };
    auto w16 = [&] (uint16_t v) { f.put ((char) (v & 255)); f.put ((char) ((v >> 8) & 255)); };
    f.write ("RIFF", 4); w32 (36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 6); w16 (6); w16 (24);
    f.write ("data", 4); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c)
        {
            const float v = clampf (c == 0 ? L[i] : R[i], -1.0f, 1.0f);
            const int32_t s = (int32_t) std::lround (v * 8388607.0f);
            f.put ((char) (s & 255)); f.put ((char) ((s >> 8) & 255)); f.put ((char) ((s >> 16) & 255));
        }
}

struct Session
{
    Engine e; std::vector<float> L, R; std::string log; double t = 0;
    void start (int pi, double sr = 48000.0)
    {
        applyPreset (pi, e.p, e.macro, e.scene, e.sceneSet, e.life.slots, e.life.mseg);
        e.p[P_determin] = 1; e.p[P_seed] = 7; e.prepare (sr, 512); e.setTransport (100.0, 0.0, true);
        log += "preset " + std::string (preset (pi).name) + "\n";
    }
    void run (double sec) { const int n = (int) (sec * e.sr); const size_t o = L.size(); L.resize (o + n); R.resize (o + n); for (int i = 0; i < n; i += 256) { const int m = std::min (256, n - i); e.process (&L[o + i], &R[o + i], m); } t += sec; }
    void set (const char* id, float v) { const int i = paramIndex (id); if (i >= 0) e.p[i] = v; log += std::to_string (t) + " set " + id + " " + std::to_string (v) + "\n"; }
    void on (int n, float v = 0.8f) { e.noteOn (n, v); log += std::to_string (t) + " on " + std::to_string (n) + "\n"; }
    void off (int n) { e.noteOff (n); log += std::to_string (t) + " off " + std::to_string (n) + "\n"; }
    void save (const std::string& base)
    {
        float pk = 0.0f; double e2 = 0.0;
        for (size_t i = 0; i < L.size(); ++i) { pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i]))); e2 += L[i] * L[i] + R[i] * R[i]; }
        writeWav (base + ".wav", L, R, (int) e.sr); std::ofstream (base + ".txt") << log;
        std::printf ("  %s (%.1f s)  peak %.3f  rms %.1f dBFS\n", base.c_str(), L.size() / e.sr, pk, 20.0 * std::log10 (std::sqrt (e2 / (2.0 * (double) L.size())) + 1e-9));
    }
};

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: ttyrender <outdir> [preset <i> <sec>]\n"); return 1; }
    const std::string out = std::string (argv[1]) + "/";
    if (argc >= 5 && ! std::strcmp (argv[2], "preset"))
    {
        Session s; s.start (std::atoi (argv[3])); s.set ("drone", 1.0f); s.run (std::atof (argv[4]));
        s.save (out + "preset-" + std::to_string (std::atoi (argv[3])));
        return 0;
    }
    // 1. the flagship journey, 90 s: HISTORY runs the four scenes
    {
        Session s; s.start (15); s.set ("drone", 1.0f); s.set ("h_on", 1.0f); s.set ("h_mode", 1.0f); s.set ("h_dur", xunmap (80.0f, 1.0f, 1800.0f)); s.set ("h_loop", 0.0f);
        s.run (30.0); s.on (57, 0.7f); s.run (8.0); s.off (57); s.run (52.0);
        s.save (out + "01-thirty-thousand-years");
    }
    // 2. a quiet scene, 60 s
    {
        Session s; s.start (8); s.set ("drone", 1.0f); s.run (20.0); s.set ("mac_humanity", 0.6f); s.run (20.0); s.set ("mac_distance", 0.7f); s.run (20.0);
        s.save (out + "02-population-zero");
    }
    // 3. a playable phrase on IRON BASS then CABLE BASS
    {
        Session s; s.start (17);
        const int ph[] = { 33, 33, 40, 36, 33, 31, 33, 45 }; const double du[] = { 0.5, 0.5, 0.25, 0.25, 0.5, 0.5, 1.0, 1.5 };
        for (int rep = 0; rep < 2; ++rep) for (int i = 0; i < 8; ++i) { s.on (ph[i], 0.6f + 0.35f * (i % 3 == 0)); s.run (du[i] * 0.8); s.off (ph[i]); s.run (du[i] * 0.2); }
        s.run (2.0);
        applyPreset (18, s.e.p, s.e.macro, s.e.scene, s.e.sceneSet, s.e.life.slots, s.e.life.mseg);
        s.on (33, 0.8f); s.run (3.0); s.on (40, 0.7f); s.run (3.0); s.off (33); s.run (2.0); s.off (40); s.run (4.0);
        s.save (out + "03-playable-phrase");
    }
    // 4. a dense example that leaves room: the flagship under a kick and a bass line
    {
        Session s; s.start (15); s.set ("drone", 1.0f); s.set ("m_hp", 0.45f); s.set ("bassmono_on", 1.0f); s.set ("e_rv_mix", 0.3f); s.set ("e_distance", 0.35f);
        s.run (24.0);
        // a synthetic kick and a sub bass line added on top of the render (not through the synth), so the arrangement can be heard
        const int n = (int) s.L.size(); const double sr = s.e.sr;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr; const double beat = std::fmod (t, 0.6);
            const float kick = beat < 0.25 ? (float) (std::sin (TAU * (55.0 + 120.0 * std::exp (-beat * 30.0)) * beat) * std::exp (-beat * 14.0)) * 0.7f : 0.0f;
            const int step = (int) (t / 0.6) % 4; const double bassHz = step == 0 ? 55.0 : (step == 2 ? 41.2 : 49.0);
            const float bass = (float) (std::sin (TAU * bassHz * t) * 0.35 * (beat < 0.5 ? 1.0 : std::exp (-(beat - 0.5) * 20.0)));
            s.L[(size_t) i] = s.L[(size_t) i] * 0.8f + kick + bass; s.R[(size_t) i] = s.R[(size_t) i] * 0.8f + kick + bass;
        }
        /*  The backing track alone is kick 0.7 plus bass 0.35, so the sum was
            over full scale before the instrument was added and a hard clamp
            was hiding it: this demo was clipping, and a demo that clips is a
            demonstration of the wrong thing. Mastered instead - the whole
            passage is measured and scaled once, which is what a mix engineer
            would do and what the instrument's own limiter must not be asked
            to do. */
        {
            float pk = 0.0f;
            for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (s.L[(size_t) i]), std::abs (s.R[(size_t) i])));
            const float g = pk > 0.0f ? 0.89f / pk : 1.0f;
            for (int i = 0; i < n; ++i) { s.L[(size_t) i] *= g; s.R[(size_t) i] *= g; }
        }
        s.save (out + "04-in-an-arrangement");
    }
    return 0;
}
