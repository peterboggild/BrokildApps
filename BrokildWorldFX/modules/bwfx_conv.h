#pragma once

// Uniform partitioned FFT convolution for the BWFX convolution reverb —
// the JUCE-free replacement for juce::dsp::Convolution.
//
// Partition (hop) size 512, FFT 1024. The wet path arrives one partition
// late (~10.7 ms at 48 k) — imperceptible as reverb pre-delay; the dry path
// is untouched. CPU scales as irLength/hop per sample and is what makes a
// 6 s IR affordable.
//
// Threading contract (the irLock lesson, restated): the audio thread only
// ever reads the spectra set published in `live`. setImpulse() runs on the
// message thread, builds the INACTIVE set completely, then publishes it
// with one atomic store; process() crossfades from the old set to the new
// over ~85 ms, rendering both against the SAME input history during the
// fade. No locks anywhere, and the audio thread never sees a half-built IR.

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace bwfx
{

// In-place iterative radix-2 complex FFT, interleaved re/im.
void fftComplex (float* reim, int n, bool inverse);

class PartConv
{
public:
    static constexpr int kHop = 512;
    static constexpr int kFft = 2 * kHop;

    // message thread; maxSeconds bounds the partition pool
    void prepare (double fs, float maxSeconds);

    // message thread: build the inactive spectra set from a stereo IR and
    // publish it (audio thread crossfades over)
    void setImpulse (const float* irL, const float* irR, int frames);

    void reset();                                   // audio thread, no alloc

    // audio thread: ADDS the wet signal into outL/outR
    void process (const float* inL, const float* inR, float* outL, float* outR, int n);

    bool ready() const { return live.load (std::memory_order_acquire) >= 0; }

private:
    struct SpectraSet
    {
        std::vector<float> data;    // nPartAlloc * kFft * 2 channels (re/im pairs)
        int nPart = 0;              // valid partitions
    };

    int nPartAlloc = 0;
    std::array<SpectraSet, 2> sets;
    std::atomic<int> live { -1 };   // -1 = none, else 0/1; message thread writes

    // input spectra history ring (shared by both sets during a crossfade)
    std::vector<float> histL, histR;    // nPartAlloc * kFft each (re/im)
    int histPos = 0;
    int histValid = 0;   // hops written since reset — never read stale spectra

    // stream buffering: gather kHop input samples, emit kHop output samples
    std::array<float, kHop> gathL {}, gathR {};     // current input block
    std::array<float, kHop> outAL {}, outAR {};     // ready output block
    std::array<float, kHop> prevHopL {}, prevHopR {};  // overlap-save memory
    int fill = 0;

    int applied = -1;               // set index the audio thread is using
    float xfade = 1.0f;             // 1 = fully on `applied`
    int   xfadeFrom = -1;

    std::vector<float> workA, workB, accA, accB;    // FFT scratch (audio thread)

    void renderHop();               // one hop: FFT input, MAC, IFFT, overlap
    void macSet (const SpectraSet& s, float* accL, float* accR) const;
};

// Deterministic stereo impulse — exact port of Photo-Synth 2's
// makeReverbImpulse(), including the WebAudio ConvolverNode normalisation.
// type: 0 room, 1 hall, 2 plate, 3 spring, 4 reverse.
// Writes 2*frames floats into out (ch0 then ch1); returns frames.
int makeReverbImpulse (int type, float lengthSeconds, double fs, std::vector<float>& out);

} // namespace bwfx
