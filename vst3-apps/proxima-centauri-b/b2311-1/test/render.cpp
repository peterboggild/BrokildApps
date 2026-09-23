/*  ARTEFACT B2311.1 — the demonstrations that ship with it.

    Five takes, each showing one thing the object does, rendered from the same
    engine the plug-in runs. A parameter may be RAMPED across a take, because
    two of these are about change over time — warming it, and the difference
    between leading it and shoving it — and a still setting cannot show either.

        ab1render <outdir>
*/
#include "../Source/Engine.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

static const int SR = 48000, BLK = 256;

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R)
{
    const int n = (int) L.size();
    FILE* f = fopen (path, "wb"); if (!f) { std::printf ("  ! %s\n", path); return; }
    auto u32 = [&](uint32_t v){ fwrite(&v,4,1,f); }; auto u16 = [&](uint16_t v){ fwrite(&v,2,1,f); };
    fwrite ("RIFF",1,4,f); u32 (36+n*4); fwrite ("WAVE",1,4,f);
    fwrite ("fmt ",1,4,f); u32 (16); u16 (1); u16 (2);
    u32 ((uint32_t)SR); u32 ((uint32_t)SR*4); u16 (4); u16 (16);
    fwrite ("data",1,4,f); u32 ((uint32_t)n*4);
    for (int i=0;i<n;++i){
        int a=(int)std::lround(32000.f*std::max(-1.f,std::min(1.f,L[(size_t)i])));
        int b=(int)std::lround(32000.f*std::max(-1.f,std::min(1.f,R[(size_t)i])));
        u16((uint16_t)(int16_t)a); u16((uint16_t)(int16_t)b); }
    fclose (f);
}

struct Demo
{
    const char* file;
    const char* caption;
    double secs;
    double bpm;
    bool   playing;
    int    specimen;          // -1 for the defaults
    const char* rampId;       // nullptr for none
    float  rampFrom, rampTo;
    //  a few overrides applied before the take
    const char* setId[4]; float setVal[4];
};

static float* find (ab1::Params& p, const char* id)
{
    for (int i = 0; i < ab1::numParams(); ++i)
        if (std::strcmp (ab1::paramSpec (i).id, id) == 0) return &ab1::paramSpec (i).get (p);
    return nullptr;
}

int main (int argc, char** argv)
{
    const char* dir = argc > 1 ? argv[1] : ".";

    const Demo demos[] =
    {
      { "B2311-01-01_the-two-clocks",
        "Nothing imposed on it. This is what the shelter's own clocks were "
        "standing next to for two years.",
        30.0, 120.0, false, -1, nullptr, 0, 0, {nullptr}, {0} },

      { "B2311-01-02_it-leans-towards-you",
        "The same object with a pulse imposed on it at 120. It does not lock; "
        "it leans, and the leaning builds across the take.",
        30.0, 120.0, true, -1, nullptr, 0, 0, {nullptr}, {0} },

      { "B2311-01-03_led-not-shoved",
        "The imposed pulse is pushed from a nudge to a shove across this take. "
        "It gathers, and then it scatters: the object can be led and cannot be "
        "forced.",
        30.0, 120.0, true, -1, "grip", 0.10f, 0.95f, {nullptr}, {0} },

      { "B2311-01-04_warming-it",
        "From liquid nitrogen to five hundred kelvin. Cold it does not count "
        "at all; warmed, it counts faster, and that is the whole of what "
        "heating it means.",
        34.0, 120.0, true, -1, "temp", 0.02f, 0.62f, {nullptr}, {0} },

      { "B2311-01-05_several-timings-at-once",
        "A wide spread of rates, so the body does not fall into one group. The "
        "regions visible on the face are these layers.",
        30.0, 120.0, true, -1, nullptr, 0, 0,
        {"ratelo","ratehi","couple","dead"}, {0.12f, 0.62f, 0.70f, 0.50f} },

      { "B2311-01-06_the-weight-of-it",
        "One of the sparse specimens. The counts are slow enough that a "
        "discharge is a single event rather than a texture, so what the body "
        "does with it can be heard: a large one reaches the low modes and is "
        "still sounding when the next arrives, a small one is not.",
        34.0, 120.0, true, 41, nullptr, 0, 0, {nullptr}, {0} },
    };

    std::printf ("%-38s %8s %9s %8s %7s\n", "file", "secs", "firings/s", "silent%", "peak");
    for (const Demo& d : demos)
    {
        ab1::Params p;
        if (d.specimen >= 0) ab1::applySpecimen (d.specimen, p);
        for (int i = 0; i < 4 && d.setId[i]; ++i)
            if (float* f = find (p, d.setId[i])) *f = d.setVal[i];

        ab1::Engine e; e.p = p;
        e.prepare (SR, BLK); e.service();
        e.setTransport (d.bpm, 0.0, d.playing);
        e.noteOn (48, 0.9f);

        float* ramp = d.rampId ? find (e.p, d.rampId) : nullptr;

        const int nb = (int)(d.secs * SR / BLK);
        std::vector<float> L, R; L.reserve ((size_t)nb*BLK); R.reserve ((size_t)nb*BLK);
        std::vector<float> bl (BLK), br (BLK);
        for (int b = 0; b < nb; ++b)
        {
            if (ramp) { const float t = (float) b / (float) std::max (1, nb-1);
                        *ramp = d.rampFrom + (d.rampTo - d.rampFrom) * t;
                        if (std::strcmp (d.rampId, "ratelo") == 0
                         || std::strcmp (d.rampId, "ratehi") == 0) e.service(); }
            e.process (bl.data(), br.data(), BLK);
            L.insert (L.end(), bl.begin(), bl.end());
            R.insert (R.end(), br.begin(), br.end());
        }

        double pk = 0; for (float v : L) pk = std::max (pk, (double) std::abs (v));
        for (float v : R) pk = std::max (pk, (double) std::abs (v));
        /*  Brought to a common peak. The object's dynamics live INSIDE a take —
            between takes they would only be telling you which demo happened to
            use a busier setting. */
        const float g = pk > 1e-9 ? (float)(0.72 / pk) : 1.f;
        for (size_t i = 0; i < L.size(); ++i) { L[i]*=g; R[i]*=g; }

        //  a short fade so nothing starts or stops on an edge
        const int fade = SR/12;
        for (int i = 0; i < fade && i < (int)L.size(); ++i){
            const float f = (float) i / fade;
            L[(size_t)i]*=f; R[(size_t)i]*=f;
            L[L.size()-1-(size_t)i]*=f; R[R.size()-1-(size_t)i]*=f; }

        char path[512]; std::snprintf (path, sizeof path, "%s/%s.wav", dir, d.file);
        writeWav (path, L, R);

        const double st = (double) e.stepsRun.load();
        std::printf ("%-38s %8.1f %9.0f %8.1f %7.3f\n", d.file, d.secs,
                     e.totalFired.load()/d.secs,
                     st>0 ? 100.0*e.silentSteps.load()/st : 0.0, pk*g);
    }
    return 0;
}
