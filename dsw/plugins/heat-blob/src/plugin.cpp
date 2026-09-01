// heat-blob — the DSW tutorial plugin.
//
// The smallest complete DEX that still shows every part of the ABI:
// a simulated state (a 256x256 temperature field), a control message
// ({t:"set"}), an interaction message ({t:"poke"}), telemetry back to the
// UI, and an RGBA render. docs/CREATING-A-PLUGIN.md walks through this
// file line by line.

#include "dex_plugin.h"
#include "dex_msg.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr int N = 256;                      // the plate is N x N cells

struct Heat {
    std::vector<float> t, t2;               // temperature field + scratch
    std::vector<uint8_t> rgba;              // the frame render() hands out
    double diff = 0.20;                     // diffusivity 0..0.25 (stability)
    double cool = 0.9985;                   // per-step cooling toward 0
    bool paused = false;
    double statClock = 0;                   // seconds until next telemetry
    char msg[128];                          // one queued plugin->UI message
    bool msgPending = false;

    Heat() : t(N * N, 0.0f), t2(N * N, 0.0f), rgba(N * N * 4, 0) {
        poke(0.5, 0.5, 60.0f);              // greet the user with one blob
    }

    void poke(double x, double y, float amount) {
        const int cx = (int)(x * N), cy = (int)(y * N), r = N / 16;
        for (int j = -r; j <= r; ++j)
            for (int i = -r; i <= r; ++i) {
                const int px = cx + i, py = cy + j;
                if (px < 0 || px >= N || py < 0 || py >= N) continue;
                const float d2 = (float)(i * i + j * j) / (float)(r * r);
                if (d2 <= 1.0f) t[py * N + px] += amount * (1.0f - d2);
            }
    }

    void step() {
        const float a = (float)diff, keep = (float)cool;
        // one explicit diffusion step; the borders stay cold (clamped)
        #pragma omp parallel for
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                const int i = y * N + x;
                const float c = t[i];
                const float l = t[y * N + (x > 0 ? x - 1 : x)];
                const float rr = t[y * N + (x < N - 1 ? x + 1 : x)];
                const float u = t[(y > 0 ? y - 1 : y) * N + x];
                const float d = t[(y < N - 1 ? y + 1 : y) * N + x];
                t2[i] = (c + a * (l + rr + u + d - 4.0f * c)) * keep;
            }
        t.swap(t2);
    }
};

// ---- the seven ABI functions ------------------------------------------------

void *create(void) { return new Heat(); }
void destroy(void *inst) { delete (Heat *)inst; }

int advance(void *inst, double dt) {
    Heat &h = *(Heat *)inst;
    if (h.paused) return 0;                  // 0 = idle, host may sleep
    // a few sim steps per call keeps the message loop responsive
    for (int i = 0; i < 4; ++i) h.step();
    h.statClock -= dt;
    if (h.statClock <= 0) {                  // ~2 Hz telemetry to the UI
        h.statClock = 0.5;
        double total = 0, peak = 0;
        for (float v : h.t) { total += v; if (v > peak) peak = v; }
        snprintf(h.msg, sizeof h.msg,
                 "{\"t\":\"stats\",\"total\":%.1f,\"peak\":%.2f}", total, peak);
        h.msgPending = true;
    }
    return 1;                                // 1 = did work, call again
}

void on_message(void *inst, const char *json, size_t len) {
    Heat &h = *(Heat *)inst;
    const std::string m(json, len);
    const std::string t = dexmsg::type_of(m);
    if (t == "set") {
        const std::string k = dexmsg::get_str(m, "k");
        if (k == "diff")  h.diff = dexmsg::get_num(m, "v", h.diff);
        if (k == "pause") h.paused = dexmsg::get_num(m, "v", 0) > 0.5;
    } else if (t == "poke") {
        h.poke(dexmsg::get_num(m, "x", 0.5), dexmsg::get_num(m, "y", 0.5), 40.0f);
    } else if (t == "clear") {
        std::fill(h.t.begin(), h.t.end(), 0.0f);
    }
}

const char *poll_message(void *inst) {
    Heat &h = *(Heat *)inst;
    if (!h.msgPending) return nullptr;
    h.msgPending = false;
    return h.msg;                            // valid until the next ABI call
}

int render(void *inst, dex_frame *out) {
    Heat &h = *(Heat *)inst;
    // black -> ember -> orange -> white, the classic heat look
    #pragma omp parallel for
    for (int i = 0; i < N * N; ++i) {
        const float v = h.t[i];
        uint8_t *p = &h.rgba[(size_t)i * 4];
        p[0] = (uint8_t)std::fmin(255.0f, v * 8.0f);
        p[1] = (uint8_t)std::fmin(255.0f, v * 3.0f);
        p[2] = (uint8_t)std::fmin(255.0f, v * v * 0.05f);
        p[3] = 255;
    }
    out->width = N;
    out->height = N;
    out->rgba = h.rgba.data();
    return 1;
}

const dex_plugin_api api = {
    DEX_ABI_VERSION,
    "heat-blob", "Heat Blob (tutorial)", "1.0",
    create, destroy, advance, on_message, poll_message, render,
};

} // namespace

extern "C" DEX_EXPORT const dex_plugin_api *dex_plugin_entry(void) {
    return &api;
}
